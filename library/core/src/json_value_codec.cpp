#include "json_value_codec.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>

namespace kyna {
namespace {

class JsonParser {
public:
  JsonParser(std::string_view input, Heap &managedHeap) : source(input), heap(managedHeap) {}

  Value parse() {
    auto value = parseValue();
    whitespace();
    if (position != source.size())
      fail("unexpected trailing JSON content");
    return value;
  }

private:
  std::string_view source;
  Heap &heap;
  std::size_t position{0};

  [[noreturn]] void fail(const std::string &message) const {
    Diagnostic diagnostic{message + " at JSON byte " + std::to_string(position + 1), {}, false};
    diagnostic.code = "K5100";
    throw KynaError(diagnostic);
  }

  void whitespace() {
    while (position < source.size() && (source[position] == ' ' || source[position] == '\t' ||
                                        source[position] == '\r' || source[position] == '\n'))
      ++position;
  }

  bool take(char expected) {
    whitespace();
    if (position >= source.size() || source[position] != expected)
      return false;
    ++position;
    return true;
  }

  void expect(std::string_view expected) {
    if (!source.substr(position).starts_with(expected))
      fail("expected '" + std::string(expected) + "'");
    position += expected.size();
  }

  static void appendCodePoint(std::string &output, unsigned value) {
    if (value <= 0x7f) {
      output.push_back(static_cast<char>(value));
    } else if (value <= 0x7ff) {
      output.push_back(static_cast<char>(0xc0 | (value >> 6)));
      output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else {
      output.push_back(static_cast<char>(0xe0 | (value >> 12)));
      output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
      output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    }
  }

  std::string parseString() {
    if (!take('"'))
      fail("expected JSON string");
    std::string result;
    while (position < source.size()) {
      const char character = source[position++];
      if (character == '"')
        return result;
      if (static_cast<unsigned char>(character) < 0x20)
        fail("control character in JSON string");
      if (character != '\\') {
        result.push_back(character);
        continue;
      }
      if (position >= source.size())
        fail("unterminated JSON escape");
      switch (source[position++]) {
      case '"':
        result.push_back('"');
        break;
      case '\\':
        result.push_back('\\');
        break;
      case '/':
        result.push_back('/');
        break;
      case 'b':
        result.push_back('\b');
        break;
      case 'f':
        result.push_back('\f');
        break;
      case 'n':
        result.push_back('\n');
        break;
      case 'r':
        result.push_back('\r');
        break;
      case 't':
        result.push_back('\t');
        break;
      case 'u': {
        if (position + 4 > source.size())
          fail("incomplete JSON Unicode escape");
        unsigned codePoint = 0;
        for (int digit = 0; digit < 4; ++digit) {
          const char value = source[position++];
          codePoint <<= 4;
          if (value >= '0' && value <= '9')
            codePoint += static_cast<unsigned>(value - '0');
          else if (value >= 'a' && value <= 'f')
            codePoint += static_cast<unsigned>(value - 'a' + 10);
          else if (value >= 'A' && value <= 'F')
            codePoint += static_cast<unsigned>(value - 'A' + 10);
          else
            fail("invalid JSON Unicode escape");
        }
        appendCodePoint(result, codePoint);
        break;
      }
      default:
        fail("invalid JSON escape");
      }
    }
    fail("unterminated JSON string");
  }

  Value parseNumber() {
    whitespace();
    const auto start = position;
    if (position < source.size() && source[position] == '-')
      ++position;
    if (position >= source.size() || source[position] < '0' || source[position] > '9')
      fail("invalid JSON number");
    if (source[position] == '0')
      ++position;
    else
      while (position < source.size() && source[position] >= '0' && source[position] <= '9')
        ++position;
    bool floating = false;
    if (position < source.size() && source[position] == '.') {
      floating = true;
      ++position;
      if (position >= source.size() || source[position] < '0' || source[position] > '9')
        fail("invalid JSON number fraction");
      while (position < source.size() && source[position] >= '0' && source[position] <= '9')
        ++position;
    }
    if (position < source.size() && (source[position] == 'e' || source[position] == 'E')) {
      floating = true;
      ++position;
      if (position < source.size() && (source[position] == '+' || source[position] == '-'))
        ++position;
      if (position >= source.size() || source[position] < '0' || source[position] > '9')
        fail("invalid JSON number exponent");
      while (position < source.size() && source[position] >= '0' && source[position] <= '9')
        ++position;
    }
    const auto token = source.substr(start, position - start);
    if (!floating) {
      std::int64_t value = 0;
      const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
      if (result.ec == std::errc{})
        return Value(value);
    }
    std::string owned(token);
    char *end = nullptr;
    const double value = std::strtod(owned.c_str(), &end);
    if (!end || end != owned.c_str() + owned.size() || !std::isfinite(value))
      fail("invalid JSON number");
    return Value(value);
  }

  Value parseArray() {
    take('[');
    auto *array = heap.allocateArray();
    if (take(']'))
      return Value(array);
    do {
      array->elements.push_back(parseValue());
    } while (take(','));
    if (!take(']'))
      fail("expected ']' after JSON array");
    return Value(array);
  }

  Value parseObject() {
    take('{');
    auto *object = heap.allocate();
    if (take('}'))
      return Value(object);
    do {
      whitespace();
      const auto key = parseString();
      if (!take(':'))
        fail("expected ':' after JSON object key");
      object->fields.insert_or_assign(key, parseValue());
    } while (take(','));
    if (!take('}'))
      fail("expected '}' after JSON object");
    return Value(object);
  }

  Value parseValue() {
    whitespace();
    if (position >= source.size())
      fail("expected JSON value");
    switch (source[position]) {
    case '"':
      return Value(parseString());
    case '[':
      return parseArray();
    case '{':
      return parseObject();
    case 't':
      expect("true");
      return Value(true);
    case 'f':
      expect("false");
      return Value(false);
    case 'n':
      expect("null");
      return Value();
    default:
      if (source[position] == '-' || (source[position] >= '0' && source[position] <= '9'))
        return parseNumber();
      fail("expected JSON value");
    }
  }
};

std::string escapeJsonString(std::string_view value) {
  std::ostringstream output;
  output << '"';
  for (const unsigned char character : value) {
    switch (character) {
    case '"':
      output << "\\\"";
      break;
    case '\\':
      output << "\\\\";
      break;
    case '\b':
      output << "\\b";
      break;
    case '\f':
      output << "\\f";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      if (character < 0x20)
        output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
               << static_cast<int>(character) << std::dec;
      else
        output << static_cast<char>(character);
    }
  }
  output << '"';
  return output.str();
}

std::string stringify(const Value &value, std::set<const void *> &active) {
  return std::visit(
      [&](const auto &stored) -> std::string {
        using T = std::decay_t<decltype(stored)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>)
          return "null";
        else if constexpr (std::is_same_v<T, bool>)
          return stored ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::int64_t>)
          return std::to_string(stored);
        else if constexpr (std::is_same_v<T, double>) {
          if (!std::isfinite(stored))
            throw KynaError({"JSON cannot encode a non-finite number", {}, false, "K5101"});
          std::ostringstream output;
          output << std::setprecision(15) << stored;
          return output.str();
        } else if constexpr (std::is_same_v<T, std::string>)
          return escapeJsonString(stored);
        else if constexpr (std::is_same_v<T, char>)
          return escapeJsonString(std::string(1, stored));
        else if constexpr (std::is_same_v<T, ArrayPtr>) {
          if (!stored)
            return "null";
          if (!active.insert(stored).second)
            throw KynaError({"JSON cannot encode a cyclic array", {}, false, "K5101"});
          std::string output = "[";
          for (std::size_t index = 0; index < stored->elements.size(); ++index) {
            if (index)
              output += ',';
            output += stringify(stored->elements[index], active);
          }
          active.erase(stored);
          return output + ']';
        } else if constexpr (std::is_same_v<T, ObjectPtr>) {
          if (!stored)
            return "null";
          if (!active.insert(stored).second)
            throw KynaError({"JSON cannot encode a cyclic object", {}, false, "K5101"});
          std::string output = "{";
          bool first = true;
          for (const auto &[name, field] : stored->fields) {
            if (std::holds_alternative<FunctionPtr>(field.data))
              continue;
            if (!first)
              output += ',';
            first = false;
            output += escapeJsonString(name) + ':' + stringify(field, active);
          }
          active.erase(stored);
          return output + '}';
        } else {
          throw KynaError({"value of type '" + value.typeName() + "' cannot be converted to JSON",
                           {},
                           false,
                           "K5101"});
        }
      },
      value.data);
}

} // namespace

Value parseJsonValue(std::string_view source, Interpreter &interpreter) {
  return parseJsonValue(source, interpreter.heap());
}

Value parseJsonValue(std::string_view source, Heap &heap) {
  return JsonParser(source, heap).parse();
}

std::string stringifyJsonValue(const Value &value) {
  std::set<const void *> active;
  return stringify(value, active);
}

} // namespace kyna
