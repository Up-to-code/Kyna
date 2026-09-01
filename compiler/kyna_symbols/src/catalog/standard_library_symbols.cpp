#include "kyna/symbols/standard_library_symbols.hpp"
#include <array>
#include <limits>

namespace kyna {
namespace {

using Kind = BuiltinArgumentKind;
constexpr auto variadic = std::numeric_limits<std::size_t>::max();

constexpr std::array oneAny{Kind::Any};
constexpr std::array oneString{Kind::String};
constexpr std::array oneInteger{Kind::Integer};
constexpr std::array oneArray{Kind::Array};
constexpr std::array oneObject{Kind::Object};
constexpr std::array arrayValue{Kind::Array, Kind::Any};
constexpr std::array arrayCallable{Kind::Array, Kind::Callable};
constexpr std::array arrayCallableAny{Kind::Array, Kind::Callable, Kind::Any};
constexpr std::array callableArray{Kind::Callable, Kind::Array};
constexpr std::array twoStrings{Kind::String, Kind::String};
constexpr std::array stringValue{Kind::String, Kind::Any};
constexpr std::array textSliceArguments{Kind::String, Kind::Integer, Kind::Integer};
constexpr std::array threeStrings{Kind::String, Kind::String, Kind::String};
constexpr std::array oneCallable{Kind::Callable};

TypeRef type(std::string name) { return {std::move(name), false, {}}; }

const std::array symbols{
    StandardLibrarySymbol{"print", true, 0, variadic, {}, Kind::Any, type("void")},
    StandardLibrarySymbol{"log", true, 0, variadic, {}, Kind::Any, type("void")},
    StandardLibrarySymbol{"typeOf", true, 1, 1, oneAny, Kind::Any, type("str")},
    StandardLibrarySymbol{"len", true, 1, 1, oneAny, Kind::Any, type("int")},
    StandardLibrarySymbol{"error", true, 1, 1, oneAny, Kind::Any, type("void")},
    StandardLibrarySymbol{"push", true, 2, 2, arrayValue, Kind::Any, type("void")},
    StandardLibrarySymbol{"pop", true, 1, 1, oneArray, Kind::Any, type("any")},
    StandardLibrarySymbol{"keys", true, 1, 1, oneObject, Kind::Any, type("array")},
    StandardLibrarySymbol{"unique", true, 1, 1, oneArray, Kind::Any, type("array")},
    StandardLibrarySymbol{"sort", true, 1, 2, arrayCallable, Kind::Any, type("array")},
    StandardLibrarySymbol{"bubbleSort", true, 1, 2, arrayCallable, Kind::Any, type("array")},
    StandardLibrarySymbol{"filter", true, 2, 2, arrayCallable, Kind::Any, type("array")},
    StandardLibrarySymbol{"map", true, 2, 2, arrayCallable, Kind::Any, type("array")},
    StandardLibrarySymbol{"reduce", true, 3, 3, arrayCallableAny, Kind::Any, type("any")},
    StandardLibrarySymbol{"find", true, 2, 2, arrayCallable, Kind::Any, type("any")},
    StandardLibrarySymbol{"any", true, 2, 2, arrayCallable, Kind::Any, type("bool")},
    StandardLibrarySymbol{"all", true, 2, 2, arrayCallable, Kind::Any, type("bool")},
    StandardLibrarySymbol{"call", true, 2, 2, callableArray, Kind::Any, type("any")},
    StandardLibrarySymbol{"readFile", true, 1, 1, oneString, Kind::Any, type("str")},
    StandardLibrarySymbol{"writeFile", true, 2, 2, twoStrings, Kind::Any, type("void")},
    StandardLibrarySymbol{"readJsonFile", true, 1, 1, oneString, Kind::Any, type("any")},
    StandardLibrarySymbol{"writeJsonFile", true, 2, 2, stringValue, Kind::Any, type("void")},
    StandardLibrarySymbol{"createDirectory", true, 1, 1, oneString, Kind::Any, type("bool")},
    StandardLibrarySymbol{"fileExists", true, 1, 1, oneString, Kind::Any, type("bool")},
    StandardLibrarySymbol{"removePath", true, 1, 1, oneString, Kind::Any, type("bool")},
    StandardLibrarySymbol{"listDirectory", true, 1, 1, oneString, Kind::Any, type("array")},
    StandardLibrarySymbol{"processRun", true, 1, 1, oneString, Kind::Any, type("int")},
    StandardLibrarySymbol{"build", true, 1, 1, oneString, Kind::Any, type("int")},
    StandardLibrarySymbol{"processEnv", true, 1, 1, oneString, Kind::Any,
                          TypeRef{"str", true, {}}},
    StandardLibrarySymbol{"osName", true, 0, 0, {}, Kind::Any, type("str")},
    StandardLibrarySymbol{"osArchitecture", true, 0, 0, {}, Kind::Any, type("str")},
    StandardLibrarySymbol{"osWorkingDirectory", true, 0, 0, {}, Kind::Any, type("str")},
    StandardLibrarySymbol{"terminalIsInteractive", true, 0, 0, {}, Kind::Any, type("bool")},
    StandardLibrarySymbol{"terminalSupportsColor", true, 0, 0, {}, Kind::Any, type("bool")},
    StandardLibrarySymbol{"sleep", true, 1, 1, oneInteger, Kind::Any, type("void")},
    StandardLibrarySymbol{"wait", true, 1, 1, oneInteger, Kind::Any, type("void")},
    StandardLibrarySymbol{"clockMs", true, 0, 0, {}, Kind::Any, type("int")},
    StandardLibrarySymbol{"profileLog", true, 2, 2, stringValue, Kind::Any, type("void")},
    StandardLibrarySymbol{"measure", true, 1, 1, oneCallable, Kind::Any, type("float")},
    StandardLibrarySymbol{"httpGet", true, 1, 1, oneString, Kind::Any, type("str")},
    StandardLibrarySymbol{"fetch", true, 1, 2, stringValue, Kind::Any, type("any")},
    StandardLibrarySymbol{"fetchResult", true, 1, 2, stringValue, Kind::Any, type("object")},
    StandardLibrarySymbol{"jsonParse", true, 1, 1, oneString, Kind::Any, type("any")},
    StandardLibrarySymbol{"jsonStringify", true, 1, 1, oneAny, Kind::Any, type("str")},
    StandardLibrarySymbol{"tomlParse", true, 1, 1, oneString, Kind::Any, type("object")},
    StandardLibrarySymbol{"tomlStringify", true, 1, 1, oneObject, Kind::Any, type("str")},
    StandardLibrarySymbol{"xmlParse", true, 1, 1, oneString, Kind::Any, type("object")},
    StandardLibrarySymbol{"xmlStringify", true, 1, 1, oneObject, Kind::Any, type("str")},
    StandardLibrarySymbol{"textContains", true, 2, 2, twoStrings, Kind::Any, type("bool")},
    StandardLibrarySymbol{"textFind", true, 2, 2, twoStrings, Kind::Any,
                          TypeRef{"int", true, {}}},
    StandardLibrarySymbol{"textSlice", true, 2, 3, textSliceArguments, Kind::Any, type("str")},
    StandardLibrarySymbol{"textReplace", true, 3, 3, threeStrings, Kind::Any, type("str")},
    StandardLibrarySymbol{"textSplit", true, 2, 2, twoStrings, Kind::Any, type("array")},
    StandardLibrarySymbol{"textTrim", true, 1, 1, oneString, Kind::Any, type("str")},
    StandardLibrarySymbol{"textLower", true, 1, 1, oneString, Kind::Any, type("str")},
    StandardLibrarySymbol{"textUpper", true, 1, 1, oneString, Kind::Any, type("str")},
    StandardLibrarySymbol{"collectGarbage", true, 0, 0, {}, Kind::Any, type("void")},
    StandardLibrarySymbol{"gcStats", true, 0, 0, {}, Kind::Any, type("str")},
    StandardLibrarySymbol{"logColor", true, 2, 2, twoStrings, Kind::Any, type("void")},
    StandardLibrarySymbol{"createApiStore", true, 1, 1, oneArray, Kind::Any, type("object")},
    StandardLibrarySymbol{"console", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"process", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"os", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"terminal", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"fs", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"http", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"json", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"toml", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"xml", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"db", false, 0, 0, {}, Kind::Any, type("object")},
    StandardLibrarySymbol{"collections", false, 0, 0, {}, Kind::Any, type("object")},
};

} // namespace

const StandardLibrarySymbol *findStandardLibrarySymbol(std::string_view name) {
  for (const auto &symbol : symbols)
    if (symbol.name == name)
      return &symbol;
  return nullptr;
}

bool acceptsBuiltinArgument(BuiltinArgumentKind expected, const TypeRef &actual) {
  if (expected == Kind::Any || actual.name == "any")
    return true;
  switch (expected) {
  case Kind::String: return actual.name == "str";
  case Kind::Integer: return actual.name == "int";
  case Kind::Array: return actual.name == "array";
  case Kind::Object: return actual.name == "object";
  case Kind::Callable: return actual.name == "func";
  case Kind::Any: return true;
  }
  return false;
}

std::string_view builtinArgumentKindName(BuiltinArgumentKind kind) {
  switch (kind) {
  case Kind::Any: return "any value";
  case Kind::String: return "str";
  case Kind::Integer: return "int";
  case Kind::Array: return "array";
  case Kind::Object: return "object";
  case Kind::Callable: return "function";
  }
  return "value";
}

} // namespace kyna
