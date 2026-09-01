#include "../catalog_private.hpp"
#include "network_private.hpp"
#include "../../codecs/json/json_value_codec.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>

namespace kyna::detail {
namespace {

std::string safeEndpoint(const std::string &url) {
  const auto query = url.find('?');
  return query == std::string::npos ? url : url.substr(0, query) + "?<redacted>";
}

KynaError networkError(const NetworkRequest &request, const NetworkFailure &failure) {
  Diagnostic diagnostic{request.method + " " + safeEndpoint(request.url) + ": " +
                            networkFailurePhaseName(failure.phase) + " error: " + failure.message,
                        {}, false, "KNET2001"};
  diagnostic.category = "network";
  diagnostic.causes.push_back(
      {"libcurl", std::to_string(failure.nativeCode), failure.message});
  diagnostic.notes.push_back(failure.retryable ? "the transport classified this failure as retryable"
                                               : "the transport classified this failure as non-retryable");
  diagnostic.help =
      "check the endpoint, DNS, proxy, certificate trust, and network access; use --trace for native details";
  return KynaError(diagnostic);
}

} // namespace

void installNetworkLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();
  auto capabilities = interpreter.runtimeCapabilities();

  auto get = std::make_shared<Function>();
  get->native = true;
  get->nativeCall = [capabilities](const std::vector<Value> &a) {
    if (a.size() != 1 || !std::holds_alternative<std::string>(a[0].data))
      throw KynaError({"httpGet expects a URL string", {1, 1}, false});
    NetworkRequest request{"GET", std::get<std::string>(a[0].data), std::nullopt, {},
                           std::chrono::milliseconds(30000)};
    NetworkFailure failure;
    auto response = capabilities.network->send(request, failure);
    if (!response)
      throw networkError(request, failure);
    return Value(std::move(response->body));
  };
  global->define("httpGet", Value(get), false);

  auto fetch = std::make_shared<Function>();
  fetch->native = true;
  fetch->nativeCall = [&interpreter, capabilities](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2 ||
        !std::holds_alternative<std::string>(arguments[0].data) ||
        (arguments.size() == 2 && !std::holds_alternative<ObjectPtr>(arguments[1].data)))
      throw KynaError({"fetch expects a URL and optional request-options object", {1, 1}, false});
    const auto url = std::get<std::string>(arguments[0].data);
    std::string method = "GET";
    std::optional<std::string> requestBody;
    std::chrono::milliseconds timeout{30000};
    if (arguments.size() == 2) {
      const auto options = std::get<ObjectPtr>(arguments[1].data);
      if (const auto found = options->fields.find("method"); found != options->fields.end()) {
        if (!std::holds_alternative<std::string>(found->second.data))
          throw KynaError({"fetch option 'method' must be a string", {1, 1}, false});
        method = std::get<std::string>(found->second.data);
        std::transform(method.begin(), method.end(), method.begin(),
                       [](unsigned char character) { return std::toupper(character); });
      }
      if (const auto found = options->fields.find("body"); found != options->fields.end()) {
        if (!std::holds_alternative<std::string>(found->second.data))
          throw KynaError({"fetch option 'body' must be a JSON string", {1, 1}, false});
        requestBody = std::get<std::string>(found->second.data);
      }
      if (const auto found = options->fields.find("timeout"); found != options->fields.end()) {
        if (!std::holds_alternative<int64_t>(found->second.data) ||
            std::get<int64_t>(found->second.data) <= 0)
          throw KynaError({"fetch option 'timeout' must be a positive integer in milliseconds",
                           {}, false, "KNET1002"});
        timeout = std::chrono::milliseconds(std::get<int64_t>(found->second.data));
      }
    }
    std::map<std::string, std::string> requestHeaders;
    if (arguments.size() == 2) {
      const auto options = std::get<ObjectPtr>(arguments[1].data);
      if (const auto found = options->fields.find("headers"); found != options->fields.end()) {
        if (!std::holds_alternative<ObjectPtr>(found->second.data))
          throw KynaError({"fetch option 'headers' must be an object", {}, false, "KNET1003"});
        for (const auto &[name, value] : std::get<ObjectPtr>(found->second.data)->fields) {
          if (!std::holds_alternative<std::string>(value.data))
            throw KynaError({"fetch header '" + name + "' must have a string value", {}, false,
                             "KNET1003"});
          requestHeaders.insert_or_assign(name, std::get<std::string>(value.data));
        }
      }
    }
    NetworkRequest request{method, url, requestBody, std::move(requestHeaders), timeout};
    NetworkFailure failure;
    auto networkResponse = capabilities.network->send(request, failure);
    if (!networkResponse)
      throw networkError(request, failure);
    auto response = interpreter.heap().allocate();
    response->fields["ok"] = Value(networkResponse->ok());
    response->fields["status"] = Value(static_cast<std::int64_t>(networkResponse->status));
    response->fields["url"] = Value(networkResponse->effectiveUrl.empty()
                                                ? url
                                                : networkResponse->effectiveUrl);
    response->fields["method"] = Value(method);
    auto responseHeaderObject = interpreter.heap().allocate();
    for (const auto &[name, value] : networkResponse->headers)
      responseHeaderObject->fields.insert_or_assign(name, Value(value));
    response->fields["headers"] = Value(responseHeaderObject);
    auto textMethod = std::make_shared<Function>();
    textMethod->native = true;
    textMethod->nativeCall =
        [contents = networkResponse->body](const std::vector<Value> &methodArguments) {
      if (!methodArguments.empty())
        throw KynaError({"response.text expects no arguments", {1, 1}, false});
      return Value(contents);
    };
    response->fields["text"] = Value(textMethod);
    auto jsonMethod = std::make_shared<Function>();
    jsonMethod->native = true;
    jsonMethod->nativeCall =
        [&interpreter, contents = std::move(networkResponse->body)](
            const std::vector<Value> &methodArguments) {
          if (!methodArguments.empty())
            throw KynaError({"response.json expects no arguments", {1, 1}, false});
          return parseJsonValue(contents, interpreter);
        };
    response->fields["json"] = Value(jsonMethod);
    return Value(response);
  };
  global->define("fetch", Value(fetch), false);

  auto fetchResult = std::make_shared<Function>();
  fetchResult->native = true;
  fetchResult->nativeCall = [&interpreter, fetch](const std::vector<Value> &arguments) {
    try {
      Value response = fetch->nativeCall(arguments);
      auto roots = interpreter.heap().rootScope();
      roots.protect(response);
      auto result = interpreter.heap().allocate();
      bool responseOk = true;
      if (const auto object = std::get_if<ObjectPtr>(&response.data); object && *object) {
        if (const auto found = (*object)->fields.find("ok");
            found != (*object)->fields.end() && std::holds_alternative<bool>(found->second.data))
          responseOk = std::get<bool>(found->second.data);
      }
      result->fields["ok"] = Value(responseOk);
      result->fields["response"] = response;
      result->fields["error"] = Value();
      return Value(result);
    } catch (const KynaError &failure) {
      Value error(interpreter.heap().allocateError(failure.diagnostic.message,
                                                    failure.diagnostic.code, Value()));
      auto roots = interpreter.heap().rootScope();
      roots.protect(error);
      auto result = interpreter.heap().allocate();
      result->fields["ok"] = Value(false);
      result->fields["response"] = Value();
      result->fields["error"] = error;
      return Value(result);
    }
  };
  global->define("fetchResult", Value(fetchResult), false);

  auto http = interpreter.heap().allocate();
  http->fields["fetch"] = Value(fetch);
  http->fields["tryFetch"] = Value(fetchResult);

  auto responseHelper = std::make_shared<Function>();
  responseHelper->native = true;
  responseHelper->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2)
      throw KynaError({"http.response expects a body and optional options object", {}, false, "KHTTP1001"});
    auto response = interpreter.heap().allocate();
    response->fields["body"] = arguments[0]; response->fields["status"] = Value(std::int64_t{200});
    response->fields["headers"] = Value(interpreter.heap().allocate());
    if (arguments.size() == 2) {
      if (!std::holds_alternative<ObjectPtr>(arguments[1].data))
        throw KynaError({"http.response options must be an object", {}, false, "KHTTP1001"});
      const auto options = std::get<ObjectPtr>(arguments[1].data);
      if (const auto status = options->fields.find("status"); status != options->fields.end()) response->fields["status"] = status->second;
      if (const auto headers = options->fields.find("headers"); headers != options->fields.end()) response->fields["headers"] = headers->second;
    }
    return Value(response);
  };
  auto jsonResponse = std::make_shared<Function>();
  jsonResponse->native = true;
  jsonResponse->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2)
      throw KynaError({"http.json expects a value and optional status", {}, false, "KHTTP1002"});
    auto response = interpreter.heap().allocate(); response->fields["body"] = Value(stringifyJsonValue(arguments[0]));
    response->fields["status"] = arguments.size() == 2 ? arguments[1] : Value(std::int64_t{200});
    auto headers = interpreter.heap().allocate(); headers->fields["content-type"] = Value(std::string("application/json; charset=utf-8"));
    response->fields["headers"] = Value(headers); return Value(response);
  };
  auto redirect = std::make_shared<Function>();
  redirect->native = true;
  redirect->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"http.redirect expects a URL and optional status", {}, false, "KHTTP1003"});
    auto response = interpreter.heap().allocate(); response->fields["body"] = Value(std::string{});
    response->fields["status"] = arguments.size() == 2 ? arguments[1] : Value(std::int64_t{302});
    auto headers = interpreter.heap().allocate(); headers->fields["location"] = arguments[0]; response->fields["headers"] = Value(headers);
    return Value(response);
  };
  http->fields["response"] = Value(responseHelper);
  http->fields["json"] = Value(jsonResponse);
  http->fields["redirect"] = Value(redirect);
  http->fields["server"] = Value(createHttpServerFunction(interpreter, capabilities));
  global->define("http", Value(http), false);
}

} // namespace kyna::detail
