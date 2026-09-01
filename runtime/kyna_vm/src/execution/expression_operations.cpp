#include "expression_operations.hpp"
#include <limits>

namespace kyna {

std::string decodeExpressionLiteral(const std::string &literal) {
  std::string value;
  const auto end = literal.size() > 1 ? literal.size() - 1 : literal.size();
  for (std::size_t index = 1; index < end; ++index) {
    if (literal[index] != '\\' || index + 1 >= end) {
      value.push_back(literal[index]);
      continue;
    }
    switch (literal[++index]) {
    case 'n': value.push_back('\n'); break;
    case 'r': value.push_back('\r'); break;
    case 't': value.push_back('\t'); break;
    case 'b': value.push_back('\b'); break;
    case 'f': value.push_back('\f'); break;
    case '\\': value.push_back('\\'); break;
    case '"': value.push_back('"'); break;
    case '\'': value.push_back('\''); break;
    default: value.push_back(literal[index]); break;
    }
  }
  return value;
}

Value evaluateExpressionBinary(TokenKind op, const Value &a, const Value &b, SourceSpan span) {
  if (op == TokenKind::EqualEqual) return Value(a.equals(b));
  if (op == TokenKind::BangEqual) return Value(!a.equals(b));
  if (op == TokenKind::Plus) {
    if (std::holds_alternative<std::string>(a.data) || std::holds_alternative<std::string>(b.data))
      return Value(a.display() + b.display());
    if (std::holds_alternative<int64_t>(a.data) && std::holds_alternative<int64_t>(b.data))
      return Value(std::get<int64_t>(a.data) + std::get<int64_t>(b.data));
    if ((std::holds_alternative<int64_t>(a.data) || std::holds_alternative<double>(a.data)) &&
        (std::holds_alternative<int64_t>(b.data) || std::holds_alternative<double>(b.data)))
      return Value((std::holds_alternative<int64_t>(a.data) ? std::get<int64_t>(a.data) : std::get<double>(a.data)) +
                   (std::holds_alternative<int64_t>(b.data) ? std::get<int64_t>(b.data) : std::get<double>(b.data)));
    throw KynaError({"'+' requires numbers or strings", span, false, "KRT2200"});
  }
  if (op == TokenKind::Percent &&
      (!std::holds_alternative<int64_t>(a.data) || !std::holds_alternative<int64_t>(b.data)))
    throw KynaError({"'%' requires integer operands", span, false, "KRT2202"});
  const bool nums = (std::holds_alternative<int64_t>(a.data) || std::holds_alternative<double>(a.data)) &&
                    (std::holds_alternative<int64_t>(b.data) || std::holds_alternative<double>(b.data));
  if (!nums) throw KynaError({"numeric operator requires numbers", span, false, "KRT2200"});
  const double x = std::holds_alternative<int64_t>(a.data) ? std::get<int64_t>(a.data) : std::get<double>(a.data);
  const double y = std::holds_alternative<int64_t>(b.data) ? std::get<int64_t>(b.data) : std::get<double>(b.data);
  switch (op) {
  case TokenKind::Minus: return Value(x - y);
  case TokenKind::Star: return Value(x * y);
  case TokenKind::Slash:
    if (y == 0) throw KynaError({"division by zero", span, false, "KRT2201"});
    return Value(x / y);
  case TokenKind::Percent:
    if (std::get<int64_t>(b.data) == 0) throw KynaError({"remainder by zero", span, false, "KRT2201"});
    if (std::get<int64_t>(a.data) == std::numeric_limits<int64_t>::min() && std::get<int64_t>(b.data) == -1)
      return Value(std::int64_t{0});
    return Value(std::get<int64_t>(a.data) % std::get<int64_t>(b.data));
  case TokenKind::Less: return Value(x < y);
  case TokenKind::LessEqual: return Value(x <= y);
  case TokenKind::Greater: return Value(x > y);
  case TokenKind::GreaterEqual: return Value(x >= y);
  default: throw KynaError({"unsupported numeric operator", span, false, "KRT2203"});
  }
}

} // namespace kyna
