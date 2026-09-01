#include "network_private.hpp"
#include "../../codecs/json/json_value_codec.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace kyna::detail {
namespace {

struct ServerRoute {
  std::string method;
  std::string pattern;
  FunctionPtr handler;
};

std::string decodeUrlComponent(std::string_view value, bool plusAsSpace = false) {
  const auto hex = [](char character) -> int {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
  };
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (plusAsSpace && value[index] == '+') {
      decoded.push_back(' ');
    } else if (value[index] == '%' && index + 2 < value.size()) {
      const auto high = hex(value[index + 1]);
      const auto low = hex(value[index + 2]);
      if (high < 0 || low < 0) {
        decoded.push_back(value[index]);
      } else {
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2;
      }
    } else {
      decoded.push_back(value[index]);
    }
  }
  return decoded;
}

std::optional<std::map<std::string, std::string>> matchRoute(std::string_view pattern,
                                                             std::string_view path) {
  std::map<std::string, std::string> parameters;
  std::istringstream patterns{std::string(pattern)};
  std::istringstream paths{std::string(path)};
  std::string expected, actual;
  while (true) {
    const bool hasExpected = static_cast<bool>(std::getline(patterns, expected, '/'));
    const bool hasActual = static_cast<bool>(std::getline(paths, actual, '/'));
    if (hasExpected != hasActual) return std::nullopt;
    if (!hasExpected) break;
    if (!expected.empty() && expected.front() == ':')
      parameters.emplace(expected.substr(1), decodeUrlComponent(actual));
    else if (expected != actual) return std::nullopt;
  }
  return parameters;
}

ObjectPtr mapObject(Interpreter &interpreter, const std::map<std::string, std::string> &values) {
  auto object = interpreter.heap().allocate();
  for (const auto &[name, value] : values) object->fields[name] = Value(value);
  return object;
}

HttpOutgoingResponse runtimeHttpResponse(const Value &value) {
  if (!std::holds_alternative<ObjectPtr>(value.data))
    return {200, value.display(), {{"content-type", "text/plain; charset=utf-8"}}};
  const auto object = std::get<ObjectPtr>(value.data);
  HttpOutgoingResponse response;
  if (const auto status = object->fields.find("status"); status != object->fields.end() &&
      std::holds_alternative<std::int64_t>(status->second.data))
    response.status = static_cast<int>(std::get<std::int64_t>(status->second.data));
  if (const auto body = object->fields.find("body"); body != object->fields.end())
    response.body = std::holds_alternative<std::string>(body->second.data)
                        ? std::get<std::string>(body->second.data) : body->second.display();
  if (const auto headers = object->fields.find("headers"); headers != object->fields.end() &&
      std::holds_alternative<ObjectPtr>(headers->second.data))
    for (const auto &[name, header] : std::get<ObjectPtr>(headers->second.data)->fields)
      if (std::holds_alternative<std::string>(header.data)) response.headers[name] = std::get<std::string>(header.data);
  return response;
}

} // namespace

FunctionPtr createHttpServerFunction(Interpreter &interpreter, RuntimeCapabilities capabilities) {
  auto createServer = std::make_shared<Function>();
  createServer->native = true;
  createServer->nativeCall = [&interpreter, capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() > 1 || (!arguments.empty() && !std::holds_alternative<ObjectPtr>(arguments[0].data)))
      throw KynaError({"http.server expects an optional options object", {}, false, "KHTTP1100"});
    HttpServerOptions serverOptions;
    if (!arguments.empty()) {
      const auto options = std::get<ObjectPtr>(arguments[0].data);
      if (const auto host = options->fields.find("host"); host != options->fields.end() && std::holds_alternative<std::string>(host->second.data)) serverOptions.host = std::get<std::string>(host->second.data);
      if (const auto port = options->fields.find("port"); port != options->fields.end() && std::holds_alternative<std::int64_t>(port->second.data)) serverOptions.port = static_cast<std::uint16_t>(std::get<std::int64_t>(port->second.data));
    }
    if (capabilities.processes) {
      if (const auto host = capabilities.processes->environment("KYNA_SERVER_HOST")) serverOptions.host = *host;
      if (const auto port = capabilities.processes->environment("KYNA_SERVER_PORT")) {
        try { const auto parsed = std::stoul(*port); if (parsed > 0 && parsed <= 65535) serverOptions.port = static_cast<std::uint16_t>(parsed); }
        catch (...) { throw KynaError({"KYNA_SERVER_PORT must be an integer from 1 to 65535", {}, false, "KHTTP1104"}); }
      }
    }
    auto routes = std::make_shared<std::vector<ServerRoute>>(); auto server = interpreter.heap().allocate();
    const auto addRoute = [routes](std::string method) {
      auto function = std::make_shared<Function>(); function->native = true;
      function->nativeCall = [routes, method = std::move(method)](const std::vector<Value> &values) {
        if (values.size() != 2 || !std::holds_alternative<std::string>(values[0].data) || !std::holds_alternative<FunctionPtr>(values[1].data))
          throw KynaError({"route registration expects a path and handler", {}, false, "KHTTP1101"});
        routes->push_back({method, std::get<std::string>(values[0].data), std::get<FunctionPtr>(values[1].data)}); return Value();
      }; return function;
    };
    for (const auto &method : {"GET", "POST", "PUT", "PATCH", "DELETE"}) {
      std::string name(method); std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
      server->fields[name] = Value(addRoute(method));
    }
    auto use = std::make_shared<Function>(); use->native = true;
    use->nativeCall = [routes](const std::vector<Value> &values) {
      if (values.size() != 1 || !std::holds_alternative<FunctionPtr>(values[0].data))
        throw KynaError({"app.use expects a middleware function", {}, false, "KHTTP1102"});
      routes->push_back({"*", "*", std::get<FunctionPtr>(values[0].data)}); return Value();
    }; server->fields["use"] = Value(use);
    auto listen = std::make_shared<Function>(); listen->native = true;
    listen->nativeCall = [&interpreter, capabilities, routes, serverOptions](const std::vector<Value> &values) {
      if (!values.empty()) throw KynaError({"app.listen expects no arguments", {}, false, "KHTTP1103"});
      if (!capabilities.server) throw KynaError({"the host has not provided HTTP server capability", {}, false, "KHTTP2000"});
      std::string failure;
      const bool ok = capabilities.server->listen(serverOptions, [&interpreter, routes](const HttpIncomingRequest &incoming) {
        const auto queryStart = incoming.target.find('?'); const auto path = incoming.target.substr(0, queryStart);
        std::map<std::string, std::string> query;
        if (queryStart != std::string::npos) {
          std::istringstream pairs(incoming.target.substr(queryStart + 1)); std::string pair;
          while (std::getline(pairs, pair, '&')) {
            const auto split = pair.find('=');
            const auto name = decodeUrlComponent(pair.substr(0, split), true);
            const auto value = split == std::string::npos
                                   ? std::string{}
                                   : decodeUrlComponent(pair.substr(split + 1), true);
            query[name] = value;
          }
        }
        auto request = interpreter.heap().allocate(); request->fields["method"] = Value(incoming.method); request->fields["path"] = Value(path);
        request->fields["query"] = Value(mapObject(interpreter, query)); request->fields["headers"] = Value(mapObject(interpreter, incoming.headers)); request->fields["body"] = Value(incoming.body);
        auto text = std::make_shared<Function>(); text->native = true; text->nativeCall = [body = incoming.body](const std::vector<Value> &args) { if (!args.empty()) throw KynaError({"request.text expects no arguments", {}, false}); return Value(body); };
        auto jsonBody = std::make_shared<Function>(); jsonBody->native = true; jsonBody->nativeCall = [&interpreter, body = incoming.body](const std::vector<Value> &args) { if (!args.empty()) throw KynaError({"request.json expects no arguments", {}, false}); return parseJsonValue(body, interpreter); };
        request->fields["text"] = Value(text); request->fields["json"] = Value(jsonBody);
        for (const auto &route : *routes) {
          if (route.method == "*") { const auto value = interpreter.invoke(route.handler, {Value(request)}); if (!std::holds_alternative<std::nullptr_t>(value.data)) return runtimeHttpResponse(value); continue; }
          if (route.method != incoming.method) continue;
          const auto parameters = matchRoute(route.pattern, path); if (!parameters) continue;
          request->fields["params"] = Value(mapObject(interpreter, *parameters)); return runtimeHttpResponse(interpreter.invoke(route.handler, {Value(request)}));
        }
        return HttpOutgoingResponse{404, "not found", {{"content-type", "text/plain; charset=utf-8"}}};
      }, failure);
      if (!ok) {
        Diagnostic diagnostic{failure, {}, false,
                              failure == "interrupted" ? "KHTTP0130" : "KHTTP2001"};
        diagnostic.category = "http";
        if (failure.find("Address already in use") != std::string::npos ||
            failure.find("address already in use") != std::string::npos) {
          diagnostic.help =
              "stop the existing Kyna Run/Dev task, or change [server].port in kyna.toml";
        }
        throw KynaError(diagnostic);
      }
      return Value();
    }; server->fields["listen"] = Value(listen); return Value(server);
  };
  return createServer;
}

} // namespace kyna::detail
