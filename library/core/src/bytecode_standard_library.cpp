#include "kyna/stdlib/bytecode_standard_library.hpp"
#include "kyna/formats/document_formats.hpp"
#include "kyna/text/unicode_text.hpp"
#include "format_value_codec.hpp"
#include "json_value_codec.hpp"
#include <algorithm>
#include <cctype>
#include <ostream>

namespace kyna {
namespace {

NativeCallResult failure(std::string code, std::string message, RuntimeValue cause = {}) {
  return {{}, NativeCallFailure{std::move(code), std::move(message), std::move(cause)}};
}

NativeCallResult textFailure(const UnicodeTextError &error, const RuntimeValue &cause) {
  return failure(error.code, error.message, cause);
}

class StandardLibraryBytecodeAdapter final : public BytecodeNativeAdapter {
public:
  StandardLibraryBytecodeAdapter(RuntimeCapabilities hostCapabilities, std::ostream &output)
      : capabilities(std::move(hostCapabilities)), standardOutput(output) {}

  NativeCallResult invoke(std::string_view name, std::span<const RuntimeValue> arguments,
                          Heap &heap) override {
    if (name == "print" || name == "log") {
      for (std::size_t index = 0; index < arguments.size(); ++index) {
        if (index)
          standardOutput << ' ';
        standardOutput << arguments[index].display();
      }
      standardOutput << '\n';
      return {};
    }
    if (name == "typeOf") {
      if (arguments.size() != 1)
        return failure("KSTD2001", "typeOf expects exactly one argument");
      return {RuntimeValue(arguments.front().typeName()), std::nullopt};
    }
    if (name == "len") {
      if (arguments.size() != 1)
        return failure("KSTD2002", "len expects exactly one argument");
      const auto &value = arguments.front();
      if (const auto text = std::get_if<std::string>(&value.data)) {
        auto length = unicodeLength(*text);
        return length ? NativeCallResult{RuntimeValue(*length), std::nullopt}
                      : textFailure(length.error(), value);
      }
      if (const auto array = std::get_if<ArrayPtr>(&value.data); array && *array)
        return {RuntimeValue(static_cast<std::int64_t>((*array)->elements.size())), std::nullopt};
      if (const auto object = std::get_if<ObjectPtr>(&value.data); object && *object)
        return {RuntimeValue(static_cast<std::int64_t>((*object)->fields.size())), std::nullopt};
      return failure("KSTD2003", "len requires a string, array, or object", value);
    }
    if (name == "textContains") {
      if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
          !std::holds_alternative<std::string>(arguments[1].data))
        return failure("KTEXT2010", "textContains expects a string and a string needle");
      auto found = unicodeFind(std::get<std::string>(arguments[0].data),
                               std::get<std::string>(arguments[1].data));
      return found ? NativeCallResult{RuntimeValue(found->has_value()), std::nullopt}
                   : textFailure(found.error(), arguments[0]);
    }
    if (name == "textFind") {
      if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
          !std::holds_alternative<std::string>(arguments[1].data))
        return failure("KTEXT2011", "textFind expects a string and a string needle");
      auto found = unicodeFind(std::get<std::string>(arguments[0].data),
                               std::get<std::string>(arguments[1].data));
      if (!found)
        return textFailure(found.error(), arguments[0]);
      return {found->has_value() ? RuntimeValue(**found) : RuntimeValue(), std::nullopt};
    }
    if (name == "textSlice") {
      if ((arguments.size() != 2 && arguments.size() != 3) ||
          !std::holds_alternative<std::string>(arguments[0].data) ||
          !std::holds_alternative<std::int64_t>(arguments[1].data) ||
          (arguments.size() == 3 && !std::holds_alternative<std::int64_t>(arguments[2].data)))
        return failure("KTEXT2012", "textSlice expects text, start, and optional end integers");
      const auto end = arguments.size() == 3
                           ? std::optional{std::get<std::int64_t>(arguments[2].data)}
                           : std::nullopt;
      auto sliced = unicodeSlice(std::get<std::string>(arguments[0].data),
                                 std::get<std::int64_t>(arguments[1].data), end);
      return sliced ? NativeCallResult{RuntimeValue(std::move(*sliced)), std::nullopt}
                    : textFailure(sliced.error(), arguments[0]);
    }
    if (name == "textReplace") {
      if (arguments.size() != 3 || !std::holds_alternative<std::string>(arguments[0].data) ||
          !std::holds_alternative<std::string>(arguments[1].data) ||
          !std::holds_alternative<std::string>(arguments[2].data))
        return failure("KTEXT2013", "textReplace expects text, needle, and replacement strings");
      auto replaced = unicodeReplace(std::get<std::string>(arguments[0].data),
                                     std::get<std::string>(arguments[1].data),
                                     std::get<std::string>(arguments[2].data));
      return replaced ? NativeCallResult{RuntimeValue(std::move(*replaced)), std::nullopt}
                      : textFailure(replaced.error(), arguments[0]);
    }
    if (name == "textSplit") {
      if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
          !std::holds_alternative<std::string>(arguments[1].data))
        return failure("KTEXT2014", "textSplit expects text and separator strings");
      auto pieces = unicodeSplit(std::get<std::string>(arguments[0].data),
                                 std::get<std::string>(arguments[1].data));
      if (!pieces)
        return textFailure(pieces.error(), arguments[0]);
      auto *result = heap.allocateArray();
      for (auto &piece : *pieces)
        result->elements.emplace_back(std::move(piece));
      return {RuntimeValue(result), std::nullopt};
    }
    if (name == "textTrim" || name == "textLower" || name == "textUpper") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KTEXT2015", std::string(name) + " expects exactly one string");
      UnicodeTextResult<std::string> transformed =
          name == "textTrim"    ? unicodeTrim(std::get<std::string>(arguments[0].data))
          : name == "textLower" ? unicodeLower(std::get<std::string>(arguments[0].data))
                                : unicodeUpper(std::get<std::string>(arguments[0].data));
      return transformed ? NativeCallResult{RuntimeValue(std::move(*transformed)), std::nullopt}
                         : textFailure(transformed.error(), arguments[0]);
    }
    if (name == "error") {
      if (arguments.size() != 1)
        return failure("KSTD2004", "error expects exactly one argument");
      return failure("KRT2300", arguments.front().display(), arguments.front());
    }
    if (name == "readFile") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2001", "readFile expects exactly one string path");
      std::string message;
      auto contents = capabilities.files->read(std::get<std::string>(arguments[0].data), message);
      return contents ? NativeCallResult{RuntimeValue(std::move(*contents)), std::nullopt}
                      : failure("KFS2001", std::move(message), arguments[0]);
    }
    if (name == "writeFile") {
      if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
          !std::holds_alternative<std::string>(arguments[1].data))
        return failure("KFS2002", "writeFile expects a string path and string content");
      std::string message;
      if (!capabilities.files->write(std::get<std::string>(arguments[0].data),
                                     std::get<std::string>(arguments[1].data), message))
        return failure("KFS2002", std::move(message), arguments[0]);
      return {RuntimeValue(true), std::nullopt};
    }
    if (name == "fileExists") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2003", "fileExists expects exactly one string path");
      std::string message;
      const auto exists =
          capabilities.files->exists(std::get<std::string>(arguments[0].data), message);
      if (!message.empty())
        return failure("KFS2003", std::move(message), arguments[0]);
      return {RuntimeValue(exists), std::nullopt};
    }
    if (name == "createDirectory") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2004", "createDirectory expects exactly one string path");
      std::string message;
      if (!capabilities.files->createDirectories(std::get<std::string>(arguments[0].data),
                                                  message))
        return failure("KFS2004", std::move(message), arguments[0]);
      return {RuntimeValue(true), std::nullopt};
    }
    if (name == "removePath") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2005", "removePath expects exactly one string path");
      std::string message;
      const auto removed =
          capabilities.files->remove(std::get<std::string>(arguments[0].data), message);
      if (!message.empty())
        return failure("KFS2005", std::move(message), arguments[0]);
      return {RuntimeValue(removed), std::nullopt};
    }
    if (name == "listDirectory") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2006", "listDirectory expects exactly one string path");
      std::string message;
      auto entries =
          capabilities.files->list(std::get<std::string>(arguments[0].data), message);
      if (!entries)
        return failure("KFS2006", std::move(message), arguments[0]);
      auto *result = heap.allocateArray();
      for (auto &entry : *entries)
        result->elements.emplace_back(std::move(entry));
      return {RuntimeValue(result), std::nullopt};
    }
    if (name == "readJsonFile") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2007", "readJsonFile expects exactly one string path");
      std::string message;
      auto contents = capabilities.files->read(std::get<std::string>(arguments[0].data), message);
      if (!contents)
        return failure("KFS2007", std::move(message), arguments[0]);
      try {
        return {parseJsonValue(*contents, heap), std::nullopt};
      } catch (const KynaError &error) {
        return failure(error.diagnostic.code.empty() ? "K5100" : error.diagnostic.code,
                       error.diagnostic.message, arguments[0]);
      }
    }
    if (name == "writeJsonFile") {
      if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2008", "writeJsonFile expects a string path and value");
      std::string encoded;
      try {
        encoded = stringifyJsonValue(arguments[1]);
      } catch (const KynaError &error) {
        return failure(error.diagnostic.code.empty() ? "K5101" : error.diagnostic.code,
                       error.diagnostic.message, arguments[1]);
      }
      std::string message;
      if (!capabilities.files->write(std::get<std::string>(arguments[0].data), encoded, message))
        return failure("KFS2008", std::move(message), arguments[0]);
      return {RuntimeValue(true), std::nullopt};
    }
    if (name == "processEnv") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KPROC2001", "processEnv expects exactly one variable name");
      auto value = capabilities.processes->environment(std::get<std::string>(arguments[0].data));
      return {value ? RuntimeValue(std::move(*value)) : RuntimeValue(), std::nullopt};
    }
    if (name == "osName" || name == "osArchitecture" || name == "osWorkingDirectory" ||
        name == "terminalIsInteractive" || name == "terminalSupportsColor") {
      if (!arguments.empty())
        return failure("KHOST2000", std::string(name) + " expects no arguments");
      if (!capabilities.host)
        return failure("KHOST2000", "host information capability is unavailable");
      if (name == "osName")
        return {RuntimeValue(capabilities.host->operatingSystem()), std::nullopt};
      if (name == "osArchitecture")
        return {RuntimeValue(capabilities.host->architecture()), std::nullopt};
      if (name == "terminalIsInteractive")
        return {RuntimeValue(capabilities.host->standardOutputIsTerminal()), std::nullopt};
      if (name == "terminalSupportsColor")
        return {RuntimeValue(capabilities.host->supportsColor()), std::nullopt};
      std::string message;
      auto directory = capabilities.host->workingDirectory(message);
      return directory ? NativeCallResult{RuntimeValue(std::move(*directory)), std::nullopt}
                       : failure("KHOST2001", std::move(message));
    }
    if (name == "processRun" || name == "build") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KPROC2002", std::string(name) + " expects one command string");
      return {RuntimeValue(static_cast<std::int64_t>(
                  capabilities.processes->run(std::get<std::string>(arguments[0].data)))),
              std::nullopt};
    }
    if (name == "sleep" || name == "wait") {
      if (arguments.size() != 1 || !std::holds_alternative<std::int64_t>(arguments[0].data))
        return failure("KTIME2001", std::string(name) + " expects integer milliseconds");
      const auto duration = std::get<std::int64_t>(arguments[0].data);
      if (duration < 0)
        return failure("KTIME2002", std::string(name) + " duration cannot be negative",
                       arguments[0]);
      capabilities.clock->sleep(std::chrono::milliseconds(duration));
      return {};
    }
    if (name == "httpGet") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KNET2000", "httpGet expects exactly one URL string");
      NetworkRequest request{"GET", std::get<std::string>(arguments[0].data), std::nullopt, {},
                             std::chrono::milliseconds(30000)};
      NetworkFailure networkFailure;
      auto response = capabilities.network->send(request, networkFailure);
      if (!response)
        return failure("KNET2001", "GET request failed during " +
                                       std::string(networkFailurePhaseName(networkFailure.phase)) +
                                       ": " + networkFailure.message,
                       arguments[0]);
      return {RuntimeValue(std::move(response->body)), std::nullopt};
    }
    if (name == "fetchResult") {
      auto fetched = invoke("fetch", arguments, heap);
      auto roots = heap.rootScope();
      auto *result = static_cast<Object *>(nullptr);
      if (fetched.failure) {
        RuntimeValue error(heap.allocateError(fetched.failure->message, fetched.failure->code,
                                               fetched.failure->cause));
        roots.protect(error);
        result = heap.allocate();
        result->fields["ok"] = RuntimeValue(false);
        result->fields["response"] = RuntimeValue();
        result->fields["error"] = error;
      } else {
        roots.protect(fetched.value);
        result = heap.allocate();
        bool responseOk = true;
        if (const auto response = std::get_if<ObjectPtr>(&fetched.value.data);
            response && *response) {
          if (const auto found = (*response)->fields.find("ok");
              found != (*response)->fields.end() &&
              std::holds_alternative<bool>(found->second.data))
            responseOk = std::get<bool>(found->second.data);
        }
        result->fields["ok"] = RuntimeValue(responseOk);
        result->fields["response"] = fetched.value;
        result->fields["error"] = RuntimeValue();
      }
      return {RuntimeValue(result), std::nullopt};
    }
    if (name == "fetch") {
      if (arguments.empty() || arguments.size() > 2 ||
          !std::holds_alternative<std::string>(arguments[0].data) ||
          (arguments.size() == 2 && !std::holds_alternative<ObjectPtr>(arguments[1].data)))
        return failure("KNET1001", "fetch expects a URL and optional request-options object");
      NetworkRequest request;
      request.url = std::get<std::string>(arguments[0].data);
      if (arguments.size() == 2) {
        const auto *options = std::get<ObjectPtr>(arguments[1].data);
        if (const auto found = options->fields.find("method"); found != options->fields.end()) {
          if (!std::holds_alternative<std::string>(found->second.data))
            return failure("KNET1002", "fetch option 'method' must be a string", found->second);
          request.method = std::get<std::string>(found->second.data);
          std::transform(request.method.begin(), request.method.end(), request.method.begin(),
                         [](unsigned char character) { return std::toupper(character); });
        }
        if (const auto found = options->fields.find("body"); found != options->fields.end()) {
          if (!std::holds_alternative<std::string>(found->second.data))
            return failure("KNET1002", "fetch option 'body' must be a string", found->second);
          request.body = std::get<std::string>(found->second.data);
        }
        if (const auto found = options->fields.find("timeout"); found != options->fields.end()) {
          if (!std::holds_alternative<std::int64_t>(found->second.data) ||
              std::get<std::int64_t>(found->second.data) <= 0)
            return failure("KNET1002", "fetch option 'timeout' must be a positive integer",
                           found->second);
          request.timeout =
              std::chrono::milliseconds(std::get<std::int64_t>(found->second.data));
        }
        if (const auto found = options->fields.find("headers"); found != options->fields.end()) {
          if (!std::holds_alternative<ObjectPtr>(found->second.data))
            return failure("KNET1003", "fetch option 'headers' must be an object", found->second);
          for (const auto &[header, value] : std::get<ObjectPtr>(found->second.data)->fields) {
            if (!std::holds_alternative<std::string>(value.data))
              return failure("KNET1003", "fetch header '" + header + "' must be a string",
                             value);
            request.headers.insert_or_assign(header, std::get<std::string>(value.data));
          }
        }
      }
      NetworkFailure networkFailure;
      auto response = capabilities.network->send(request, networkFailure);
      if (!response)
        return failure("KNET2001", request.method + " request failed during " +
                                       std::string(networkFailurePhaseName(networkFailure.phase)) +
                                       ": " + networkFailure.message,
                       arguments[0]);
      auto *result = heap.allocate();
      result->fields["ok"] = RuntimeValue(response->ok());
      result->fields["status"] = RuntimeValue(static_cast<std::int64_t>(response->status));
      result->fields["url"] = RuntimeValue(response->effectiveUrl.empty()
                                                ? request.url
                                                : std::move(response->effectiveUrl));
      result->fields["method"] = RuntimeValue(request.method);
      result->fields["__kynaResponse"] = RuntimeValue(true);
      result->fields["__kynaResponseBody"] = RuntimeValue(std::move(response->body));
      auto *headers = heap.allocate();
      for (auto &[header, value] : response->headers)
        headers->fields.insert_or_assign(std::move(header), RuntimeValue(std::move(value)));
      result->fields["headers"] = RuntimeValue(headers);
      return {RuntimeValue(result), std::nullopt};
    }
    if (name == "responseText" || name == "responseJson") {
      if (arguments.size() != 1 || !std::holds_alternative<ObjectPtr>(arguments[0].data))
        return failure("KNET2100", std::string(name) + " requires a fetch response");
      const auto *response = std::get<ObjectPtr>(arguments[0].data);
      const auto marker = response->fields.find("__kynaResponse");
      const auto body = response->fields.find("__kynaResponseBody");
      if (marker == response->fields.end() || !std::holds_alternative<bool>(marker->second.data) ||
          !std::get<bool>(marker->second.data) || body == response->fields.end() ||
          !std::holds_alternative<std::string>(body->second.data))
        return failure("KNET2100", std::string(name) + " requires a fetch response",
                       arguments[0]);
      if (name == "responseText")
        return {body->second, std::nullopt};
      try {
        return {parseJsonValue(std::get<std::string>(body->second.data), heap), std::nullopt};
      } catch (const KynaError &error) {
        return failure(error.diagnostic.code.empty() ? "K5100" : error.diagnostic.code,
                       "response JSON is invalid: " + error.diagnostic.message, arguments[0]);
      }
    }
    if (name == "jsonParse") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("K5100", "jsonParse expects exactly one JSON string");
      try {
        return {parseJsonValue(std::get<std::string>(arguments[0].data), heap), std::nullopt};
      } catch (const KynaError &error) {
        return failure(error.diagnostic.code.empty() ? "K5100" : error.diagnostic.code,
                       error.diagnostic.message, arguments[0]);
      }
    }
    if (name == "jsonStringify") {
      if (arguments.size() != 1)
        return failure("K5101", "jsonStringify expects exactly one value");
      try {
        return {RuntimeValue(stringifyJsonValue(arguments[0])), std::nullopt};
      } catch (const KynaError &error) {
        return failure(error.diagnostic.code.empty() ? "K5101" : error.diagnostic.code,
                       error.diagnostic.message, arguments[0]);
      }
    }
    if (name == "tomlParse" || name == "xmlParse") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure(name == "tomlParse" ? "KFORMAT1001" : "KFORMAT1101",
                       std::string(name) + " expects exactly one string");
      const auto parsed = name == "tomlParse"
                              ? parseTomlDocument(std::get<std::string>(arguments[0].data))
                              : parseXmlDocument(std::get<std::string>(arguments[0].data));
      if (!parsed.valid)
        return failure(parsed.failure.code, parsed.failure.message, arguments[0]);
      return {formatValueToRuntime(parsed.value, heap), std::nullopt};
    }
    if (name == "tomlStringify" || name == "xmlStringify") {
      if (arguments.size() != 1)
        return failure(name == "tomlStringify" ? "KFORMAT1002" : "KFORMAT1102",
                       std::string(name) + " expects exactly one value");
      std::string conversionError;
      auto value = runtimeValueToFormat(arguments[0], conversionError);
      if (!value)
        return failure(name == "tomlStringify" ? "KFORMAT1002" : "KFORMAT1102",
                       std::move(conversionError), arguments[0]);
      const auto encoded = name == "tomlStringify" ? stringifyTomlDocument(*value)
                                                    : stringifyXmlDocument(*value);
      if (!encoded.valid)
        return failure(encoded.failure.code, encoded.failure.message, arguments[0]);
      return {RuntimeValue(std::get<std::string>(encoded.value.data)), std::nullopt};
    }
    if (name == "push") {
      if (arguments.size() != 2 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
        return failure("KCOL1004", "push expects an array and a value");
      std::get<ArrayPtr>(arguments[0].data)->elements.push_back(arguments[1]);
      return {};
    }
    if (name == "pop") {
      if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
        return failure("KCOL1005", "pop expects exactly one array");
      auto *array = std::get<ArrayPtr>(arguments[0].data);
      if (array->elements.empty())
        return {};
      auto value = array->elements.back();
      array->elements.pop_back();
      return {std::move(value), std::nullopt};
    }
    if (name == "keys") {
      if (arguments.size() != 1 || !std::holds_alternative<ObjectPtr>(arguments[0].data))
        return failure("KCOL1006", "keys expects exactly one object");
      auto *result = heap.allocateArray();
      for (const auto &[key, value] : std::get<ObjectPtr>(arguments[0].data)->fields)
        result->elements.emplace_back(key);
      return {RuntimeValue(result), std::nullopt};
    }
    if (name == "unique") {
      if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
        return failure("KCOL1003", "unique expects exactly one array");
      auto *result = heap.allocateArray();
      for (const auto &candidate : std::get<ArrayPtr>(arguments[0].data)->elements) {
        const auto duplicate =
            std::any_of(result->elements.begin(), result->elements.end(),
                        [&](const RuntimeValue &accepted) { return accepted.equals(candidate); });
        if (!duplicate)
          result->elements.push_back(candidate);
      }
      return {RuntimeValue(result), std::nullopt};
    }
    if (name == "sort" || name == "bubbleSort") {
      if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
        return failure("KCOL1007", std::string(name) +
                                       " currently expects one array in bytecode execution");
      auto *result = heap.allocateArray();
      result->elements = std::get<ArrayPtr>(arguments[0].data)->elements;
      const auto less = [](const RuntimeValue &left, const RuntimeValue &right) {
        if (const auto leftInteger = std::get_if<std::int64_t>(&left.data)) {
          if (const auto rightInteger = std::get_if<std::int64_t>(&right.data))
            return *leftInteger < *rightInteger;
          if (const auto rightFloat = std::get_if<double>(&right.data))
            return static_cast<double>(*leftInteger) < *rightFloat;
        }
        if (const auto leftFloat = std::get_if<double>(&left.data)) {
          if (const auto rightInteger = std::get_if<std::int64_t>(&right.data))
            return *leftFloat < static_cast<double>(*rightInteger);
          if (const auto rightFloat = std::get_if<double>(&right.data))
            return *leftFloat < *rightFloat;
        }
        if (const auto leftString = std::get_if<std::string>(&left.data))
          if (const auto rightString = std::get_if<std::string>(&right.data))
            return *leftString < *rightString;
        return left.typeName() < right.typeName();
      };
      std::stable_sort(result->elements.begin(), result->elements.end(), less);
      return {RuntimeValue(result), std::nullopt};
    }
    return failure("KSTD2099", "unknown standard-library native '" + std::string(name) + "'");
  }

private:
  RuntimeCapabilities capabilities;
  std::ostream &standardOutput;
};

} // namespace

const std::vector<std::string> &bytecodeStandardLibraryFunctionNames() {
  static const std::vector<std::string> names{
      "print", "log", "typeOf", "len", "error", "readFile", "writeFile", "fileExists",
      "createDirectory", "removePath", "listDirectory", "readJsonFile", "writeJsonFile",
      "processEnv", "processRun", "build", "osName", "osArchitecture",
      "osWorkingDirectory", "terminalIsInteractive", "terminalSupportsColor",
      "sleep", "wait", "httpGet", "fetch",
      "fetchResult",
      "responseJson", "responseText", "jsonParse",
      "jsonStringify", "tomlParse", "tomlStringify", "xmlParse", "xmlStringify",
      "push", "pop", "keys", "unique", "sort", "bubbleSort",
      "textContains", "textFind", "textSlice", "textReplace", "textSplit", "textTrim",
      "textLower", "textUpper"};
  return names;
}

std::unique_ptr<BytecodeNativeAdapter>
createBytecodeStandardLibrary(RuntimeCapabilities capabilities, std::ostream &standardOutput) {
  return std::make_unique<StandardLibraryBytecodeAdapter>(std::move(capabilities), standardOutput);
}

} // namespace kyna
