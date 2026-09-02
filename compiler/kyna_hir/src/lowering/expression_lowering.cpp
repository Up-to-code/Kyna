#include "syntax_lowerer.hpp"
#include <algorithm>
#include <cstdlib>
#include <optional>
#include <type_traits>

namespace kyna {
namespace {
std::string decodeQuotedLiteral(const std::string &literal) {
  if (literal.size() < 2)
    return literal;
  std::string result;
  result.reserve(literal.size() - 2);
  for (std::size_t index = 1; index + 1 < literal.size(); ++index) {
    if (literal[index] != '\\' || index + 2 >= literal.size()) {
      result.push_back(literal[index]);
      continue;
    }
    switch (literal[++index]) {
    case 'n': result.push_back('\n'); break;
    case 'r': result.push_back('\r'); break;
    case 't': result.push_back('\t'); break;
    case '\\': result.push_back('\\'); break;
    case '\'': result.push_back('\''); break;
    case '"': result.push_back('"'); break;
    default: result.push_back(literal[index]); break;
    }
  }
  return result;
}
} // namespace

std::optional<HirBinaryOperator> SyntaxLowerer::lowerOperator(TokenKind operation) {
    switch (operation) {
    case TokenKind::Plus: return HirBinaryOperator::Add;
    case TokenKind::Minus: return HirBinaryOperator::Subtract;
    case TokenKind::Star: return HirBinaryOperator::Multiply;
    case TokenKind::Slash: return HirBinaryOperator::Divide;
    case TokenKind::Percent: return HirBinaryOperator::Remainder;
    case TokenKind::EqualEqual: return HirBinaryOperator::Equal;
    case TokenKind::BangEqual: return HirBinaryOperator::NotEqual;
    case TokenKind::Less: return HirBinaryOperator::Less;
    case TokenKind::LessEqual: return HirBinaryOperator::LessEqual;
    case TokenKind::Greater: return HirBinaryOperator::Greater;
    case TokenKind::GreaterEqual: return HirBinaryOperator::GreaterEqual;
    case TokenKind::AndAnd: return HirBinaryOperator::And;
    case TokenKind::OrOr: return HirBinaryOperator::Or;
    default: return std::nullopt;
    }
  }

std::optional<SyntaxLowerer::ValueBlock>
SyntaxLowerer::lowerValueBlock(const StmtPtr &statement) {
    const auto *block = statement ? std::get_if<BlockStmt>(&statement->node) : nullptr;
    if (!block) {
      unsupported("a non-block expression branch", statement ? statement->location : SourceSpan{});
      return std::nullopt;
    }
    scopes.emplace_back();
    predeclareNestedFunctions(*block);
    std::vector<HirStatementId> statements;
    for (const auto &child : block->statements)
      if (const auto lowered = lowerStatement(child))
        statements.push_back(*lowered);
    const auto value = block->tail
                           ? lowerExpression(block->tail)
                           : std::optional{addExpression(HirConstantExpression{nullptr},
                                                         statement->location)};
    scopes.pop_back();
    if (!value)
      return std::nullopt;
    return ValueBlock{addStatement(HirBlockStatement{std::move(statements)},
                                   statement->location),
                      *value};
  }

std::optional<HirExpressionId> SyntaxLowerer::lowerExpression(const ExprPtr &expression) {
    if (!expression)
      return addExpression(HirConstantExpression{nullptr}, {});
    return std::visit(
        [&](const auto &node) -> std::optional<HirExpressionId> {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, Literal>) {
            HirConstant value;
            switch (node.kind) {
            case Literal::Kind::Null: value = nullptr; break;
            case Literal::Kind::Bool: value = node.value == "true"; break;
            case Literal::Kind::Int: value = static_cast<std::int64_t>(std::stoll(node.value)); break;
            case Literal::Kind::Float: value = std::stod(node.value); break;
            case Literal::Kind::String: value = decodeQuotedLiteral(node.value); break;
            case Literal::Kind::Char: {
              const auto decoded = decodeQuotedLiteral(node.value);
              value = decoded.empty() ? '\0' : decoded.front();
              break;
            }
            }
            return addExpression(HirConstantExpression{std::move(value)}, expression->location);
          } else if constexpr (std::is_same_v<T, Variable>) {
            const auto local = findLocal(node.name);
            if (local) {
              captureIfNeeded(*local);
              return addExpression(HirLocalExpression{*local}, expression->location);
            }
            if (const auto function = functions.find(node.name); function != functions.end())
              return addExpression(HirFunctionReferenceExpression{function->second},
                                   expression->location);
            unsupported("an unresolved local '" + node.name + "'", expression->location);
            return std::nullopt;
          } else if constexpr (std::is_same_v<T, SelfExpr>) {
            if (!currentSelf) {
              unsupported("self outside a method", expression->location);
              return std::nullopt;
            }
            captureIfNeeded(*currentSelf);
            return addExpression(HirLocalExpression{*currentSelf}, expression->location);
          } else if constexpr (std::is_same_v<T, SuperExpr>) {
            unsupported("super dispatch", expression->location);
            return std::nullopt;
          } else if constexpr (std::is_same_v<T, Unary>) {
            if (node.op != TokenKind::Minus && node.op != TokenKind::Bang) {
              unsupported("unary operator '" + tokenName(node.op) + "'", expression->location);
              return std::nullopt;
            }
            const auto operand = lowerExpression(node.right);
            if (!operand)
              return std::nullopt;
return addExpression(
                HirUnaryExpression{node.op == TokenKind::Minus ? HirUnaryOperator::Negate
                                                                : HirUnaryOperator::Not,
                                   *operand},
                expression->location);
          } else if constexpr (std::is_same_v<T, AwaitExpr>) {
            const auto operand = lowerExpression(node.operand);
            if (!operand)
              return std::nullopt;
            return addExpression(HirAwaitExpression{*operand}, expression->location);
          } else if constexpr (std::is_same_v<T, Call>) {
            const auto *callee = node.callee ? std::get_if<Variable>(&node.callee->node) : nullptr;
            const auto *member = node.callee ? std::get_if<Member>(&node.callee->node) : nullptr;
            std::vector<HirExpressionId> arguments;
            arguments.reserve(node.args.size());
            for (const auto &argument : node.args) {
              const auto lowered = lowerExpression(argument);
              if (!lowered)
                return std::nullopt;
              arguments.push_back(*lowered);
            }
            if (member && (member->name == "json" || member->name == "text") &&
                isResponseExpression(member->object)) {
              const auto receiver = lowerExpression(member->object);
              if (!receiver)
                return std::nullopt;
              arguments.insert(arguments.begin(), *receiver);
              return addExpression(
                  HirNativeCallExpression{member->name == "json" ? "responseJson"
                                                                  : "responseText",
                                          std::move(arguments)},
                  expression->location);
            }
            if (member && member->object) {
              if (const auto *namespaceName = std::get_if<Variable>(&member->object->node)) {
                const auto qualifiedName = namespaceName->name + "." + member->name;
                if (const auto native = options.nativeMemberFunctions.find(qualifiedName);
                    native != options.nativeMemberFunctions.end())
                  return addExpression(
                      HirNativeCallExpression{native->second, std::move(arguments)},
                      expression->location);
              }
            }
            if (callee && !findLocal(callee->name) && !functions.contains(callee->name) &&
                std::find(options.nativeFunctions.begin(), options.nativeFunctions.end(),
                          callee->name) != options.nativeFunctions.end())
              return addExpression(HirNativeCallExpression{callee->name, std::move(arguments)},
                                   expression->location);
            if (callee && !findLocal(callee->name) && functions.contains(callee->name))
              return addExpression(HirCallExpression{functions.at(callee->name),
                                                     std::move(arguments)},
                                   expression->location);
            const auto loweredCallee = lowerExpression(node.callee);
            if (!loweredCallee)
              return std::nullopt;
            return addExpression(
                HirIndirectCallExpression{*loweredCallee, std::move(arguments)},
                expression->location);
          } else if constexpr (std::is_same_v<T, Binary>) {
            const auto operation = lowerOperator(node.op);
            if (!operation) {
              unsupported("binary operator '" + tokenName(node.op) + "'", expression->location);
              return std::nullopt;
            }
            const auto left = lowerExpression(node.left);
            const auto right = lowerExpression(node.right);
            if (!left || !right)
              return std::nullopt;
            return addExpression(HirBinaryExpression{*left, *operation, *right},
                                 expression->location);
          } else if constexpr (std::is_same_v<T, Member>) {
            if (node.object && std::holds_alternative<SuperExpr>(node.object->node)) {
              if (!currentSelf || !currentClass ||
                  !program.classes[currentClass->value].parent) {
                unsupported("super member access without a parent class",
                            expression->location);
                return std::nullopt;
              }
              auto cursor = program.classes[currentClass->value].parent;
              while (cursor) {
                const auto &parent = program.classes[cursor->value];
                for (const auto &method : parent.methods)
                  if (method.name == node.name) {
                    captureIfNeeded(*currentSelf);
                    const auto receiver =
                        addExpression(HirLocalExpression{*currentSelf}, node.object->location);
                    return addExpression(
                        HirBoundMethodExpression{receiver, method.function},
                        expression->location);
                  }
                cursor = parent.parent;
              }
              unsupported("parent method '" + node.name + "' does not exist",
                          expression->location);
              return std::nullopt;
            }
            const auto object = lowerExpression(node.object);
            return object ? std::optional{addExpression(
                                HirMemberExpression{*object, node.name}, expression->location)}
                          : std::nullopt;
          } else if constexpr (std::is_same_v<T, Index>) {
            const auto object = lowerExpression(node.object);
            const auto index = lowerExpression(node.index);
            return object && index
                       ? std::optional{addExpression(HirIndexExpression{*object, *index},
                                                     expression->location)}
                       : std::nullopt;
          } else if constexpr (std::is_same_v<T, ArrayExpr>) {
            std::vector<HirExpressionId> elements;
            elements.reserve(node.elements.size());
            for (const auto &element : node.elements) {
              const auto lowered = lowerExpression(element);
              if (!lowered)
                return std::nullopt;
              elements.push_back(*lowered);
            }
            return addExpression(HirArrayExpression{std::move(elements)}, expression->location);
          } else if constexpr (std::is_same_v<T, NewExpr>) {
            const auto klass = classes.find(node.className);
            if (klass == classes.end()) {
              unsupported("construction of unknown class '" + node.className + "'",
                          expression->location);
              return std::nullopt;
            }
            std::vector<HirExpressionId> arguments;
            arguments.reserve(node.args.size());
            for (const auto &argument : node.args) {
              const auto lowered = lowerExpression(argument);
              if (!lowered)
                return std::nullopt;
              arguments.push_back(*lowered);
            }
            return addExpression(HirNewExpression{klass->second, std::move(arguments)},
                                 expression->location);
          } else if constexpr (std::is_same_v<T, ObjectExpr>) {
            std::vector<HirObjectField> fields;
            fields.reserve(node.fields.size());
            for (const auto &field : node.fields) {
              const auto lowered = lowerExpression(field.value);
              if (!lowered)
                return std::nullopt;
              fields.push_back({field.name, *lowered});
            }
            return addExpression(HirObjectExpression{std::move(fields)}, expression->location);
          } else if constexpr (std::is_same_v<T, Assign>) {
            const auto *target = node.target ? std::get_if<Variable>(&node.target->node) : nullptr;
            const auto local = target ? findLocal(target->name) : std::nullopt;
            const auto value = lowerExpression(node.value);
            if (!value)
              return std::nullopt;
            if (const auto *index = node.target ? std::get_if<Index>(&node.target->node) : nullptr) {
              const auto object = lowerExpression(index->object);
              const auto key = lowerExpression(index->index);
              return object && key
                         ? std::optional{addExpression(
                               HirAssignIndexExpression{*object, *key, *value},
                               expression->location)}
                         : std::nullopt;
            }
            if (const auto *member =
                    node.target ? std::get_if<Member>(&node.target->node) : nullptr) {
              const auto object = lowerExpression(member->object);
              return object ? std::optional{addExpression(
                                  HirAssignMemberExpression{*object, member->name, *value},
                                  expression->location)}
                            : std::nullopt;
            }
            if (!local) {
              unsupported("non-local assignment", expression->location);
              return std::nullopt;
            }
            captureIfNeeded(*local);
            return addExpression(HirAssignLocalExpression{*local, *value}, expression->location);
          } else if constexpr (std::is_same_v<T, IfExpr>) {
            const auto condition = lowerExpression(node.condition);
            const auto thenBranch = lowerValueBlock(node.thenBranch);
            const auto elseBranch = lowerValueBlock(node.elseBranch);
            if (!condition || !thenBranch || !elseBranch)
              return std::nullopt;
            return addExpression(HirIfExpression{*condition, thenBranch->prelude,
                                                 thenBranch->value, elseBranch->prelude,
                                                 elseBranch->value},
                                 expression->location);
          } else if constexpr (std::is_same_v<T, MatchExpr>) {
            const auto subject = lowerExpression(node.subject);
            if (!subject)
              return std::nullopt;
            std::vector<HirMatchArm> arms;
            arms.reserve(node.arms.size());
            for (const auto &arm : node.arms) {
              const auto pattern = arm.wildcard ? std::optional<HirExpressionId>{}
                                                : lowerExpression(arm.pattern);
              const auto value = lowerExpression(arm.value);
              if ((!arm.wildcard && !pattern) || !value)
                return std::nullopt;
              arms.push_back({pattern, *value});
            }
            return addExpression(HirMatchExpression{*subject, std::move(arms)},
                                 expression->location);
          } else {
            unsupported("this expression", expression->location);
            return std::nullopt;
          }
        },
        expression->node);
  }

} // namespace kyna
