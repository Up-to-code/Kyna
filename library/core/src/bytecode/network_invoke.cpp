#include "bytecode_private.hpp"
#include "../codecs/json/json_value_codec.hpp"
#include "../catalog/network/parse_ipv4.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>

namespace kyna::detail {

std::optional<NativeCallResult> networkBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  if (name == "httpGet") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KNET2000", "httpGet expects exactly one URL string");
    NetworkRequest request{"GET", std::get<std::string>(arguments[0].data), std::nullopt, {},
                           std::chrono::milliseconds(30000)};
    NetworkFailure networkFailure;
    auto response = ctx.capabilities.network->send(request, networkFailure);
    if (!response)
      return bytecodeFailure("KNET2001", "GET request failed during " +
                                             std::string(networkFailurePhaseName(networkFailure.phase)) +
                                             ": " + networkFailure.message,
                             arguments[0]);
    return NativeCallResult{RuntimeValue(std::move(response->body)), std::nullopt};
  }
  if (name == "parseIP") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KNET2010", "parseIP expects an address string");
    const auto parsed = parseIPv4(std::get<std::string>(arguments[0].data));
    if (!parsed)
      return NativeCallResult{};
    return NativeCallResult{RuntimeValue(*parsed), std::nullopt};
  }
  if (name == "fetchResult") {
    auto fetched = networkBytecodeInvoke("fetch", arguments, ctx);
    auto roots = ctx.heap.rootScope();
    auto *result = static_cast<Object *>(nullptr);
    if (fetched->failure) {
      RuntimeValue error(ctx.heap.allocateError(fetched->failure->message, fetched->failure->code,
                                                 fetched->failure->cause));
      roots.protect(error);
      result = ctx.heap.allocate();
      result->fields["ok"] = RuntimeValue(false);
      result->fields["response"] = RuntimeValue();
      result->fields["error"] = error;
    } else {
      roots.protect(fetched->value);
      result = ctx.heap.allocate();
      bool responseOk = true;
      if (const auto response = std::get_if<ObjectPtr>(&fetched->value.data);
          response && *response) {
        if (const auto found = (*response)->fields.find("ok");
            found != (*response)->fields.end() &&
            std::holds_alternative<bool>(found->second.data))
          responseOk = std::get<bool>(found->second.data);
      }
      result->fields["ok"] = RuntimeValue(responseOk);
      result->fields["response"] = fetched->value;
      result->fields["error"] = RuntimeValue();
    }
    return NativeCallResult{RuntimeValue(result), std::nullopt};
  }
  if (name == "fetch") {
    if (arguments.empty() || arguments.size() > 2 ||
        !std::holds_alternative<std::string>(arguments[0].data) ||
        (arguments.size() == 2 && !std::holds_alternative<ObjectPtr>(arguments[1].data)))
      return bytecodeFailure("KNET1001", "fetch expects a URL and optional request-options object");
    NetworkRequest request;
    request.url = std::get<std::string>(arguments[0].data);
    if (arguments.size() == 2) {
      const auto *options = std::get<ObjectPtr>(arguments[1].data);
      if (const auto found = options->fields.find("method"); found != options->fields.end()) {
        if (!std::holds_alternative<std::string>(found->second.data))
          return bytecodeFailure("KNET1002", "fetch option 'method' must be a string", found->second);
        request.method = std::get<std::string>(found->second.data);
        std::transform(request.method.begin(), request.method.end(), request.method.begin(),
                       [](unsigned char character) { return std::toupper(character); });
      }
      if (const auto found = options->fields.find("body"); found != options->fields.end()) {
        if (!std::holds_alternative<std::string>(found->second.data))
          return bytecodeFailure("KNET1002", "fetch option 'body' must be a string", found->second);
        request.body = std::get<std::string>(found->second.data);
      }
      if (const auto found = options->fields.find("timeout"); found != options->fields.end()) {
        if (!std::holds_alternative<std::int64_t>(found->second.data) ||
            std::get<std::int64_t>(found->second.data) <= 0)
          return bytecodeFailure("KNET1002", "fetch option 'timeout' must be a positive integer",
                                 found->second);
        request.timeout = std::chrono::milliseconds(std::get<std::int64_t>(found->second.data));
      }
      if (const auto found = options->fields.find("headers"); found != options->fields.end()) {
        if (!std::holds_alternative<ObjectPtr>(found->second.data))
          return bytecodeFailure("KNET1003", "fetch option 'headers' must be an object", found->second);
        for (const auto &[header, value] : std::get<ObjectPtr>(found->second.data)->fields) {
          if (!std::holds_alternative<std::string>(value.data))
            return bytecodeFailure("KNET1003", "fetch header '" + header + "' must be a string",
                                   value);
          request.headers.insert_or_assign(header, std::get<std::string>(value.data));
        }
      }
    }
    NetworkFailure networkFailure;
    auto response = ctx.capabilities.network->send(request, networkFailure);
    if (!response)
      return bytecodeFailure("KNET2001", request.method + " request failed during " +
                                             std::string(networkFailurePhaseName(networkFailure.phase)) +
                                             ": " + networkFailure.message,
                             arguments[0]);
    auto *result = ctx.heap.allocate();
    result->fields["ok"] = RuntimeValue(response->ok());
    result->fields["status"] = RuntimeValue(static_cast<std::int64_t>(response->status));
    result->fields["url"] = RuntimeValue(response->effectiveUrl.empty()
                                                  ? request.url
                                                  : std::move(response->effectiveUrl));
    result->fields["method"] = RuntimeValue(request.method);
    result->fields["__kynaResponse"] = RuntimeValue(true);
    result->fields["__kynaResponseBody"] = RuntimeValue(std::move(response->body));
    auto *headers = ctx.heap.allocate();
    for (auto &[header, value] : response->headers)
      headers->fields.insert_or_assign(std::move(header), RuntimeValue(std::move(value)));
    result->fields["headers"] = RuntimeValue(headers);
    return NativeCallResult{RuntimeValue(result), std::nullopt};
  }
  if (name == "responseText" || name == "responseJson") {
    if (arguments.size() != 1 || !std::holds_alternative<ObjectPtr>(arguments[0].data))
      return bytecodeFailure("KNET2100", std::string(name) + " requires a fetch response");
    const auto *response = std::get<ObjectPtr>(arguments[0].data);
    const auto marker = response->fields.find("__kynaResponse");
    const auto body = response->fields.find("__kynaResponseBody");
    if (marker == response->fields.end() || !std::holds_alternative<bool>(marker->second.data) ||
        !std::get<bool>(marker->second.data) || body == response->fields.end() ||
        !std::holds_alternative<std::string>(body->second.data))
      return bytecodeFailure("KNET2100", std::string(name) + " requires a fetch response",
                             arguments[0]);
    if (name == "responseText")
      return NativeCallResult{body->second, std::nullopt};
    try {
      return NativeCallResult{parseJsonValue(std::get<std::string>(body->second.data), ctx.heap),
                              std::nullopt};
    } catch (const KynaError &error) {
      return bytecodeFailure(error.diagnostic.code.empty() ? "K5100" : error.diagnostic.code,
                             "response JSON is invalid: " + error.diagnostic.message, arguments[0]);
    }
  }
  return std::nullopt;
}

} // namespace kyna::detail
