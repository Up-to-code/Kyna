#include "../support_private.hpp"

#include <type_traits>

namespace kyna::detail {

std::string statementKind(const Stmt &statement) {
  return std::visit(
      [](const auto &node) -> std::string {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ImportDecl>)
          return "import";
        if constexpr (std::is_same_v<T, VarDecl>)
          return node.mutableBinding ? "var" : "const";
        if constexpr (std::is_same_v<T, FunctionDecl>)
          return "function";
        if constexpr (std::is_same_v<T, ClassDecl>)
          return "class";
        if constexpr (std::is_same_v<T, InterfaceDecl>)
          return "interface";
        if constexpr (std::is_same_v<T, BlockStmt>)
          return "block";
        if constexpr (std::is_same_v<T, IfStmt>)
          return "if";
        if constexpr (std::is_same_v<T, WhileStmt>)
          return "while";
        if constexpr (std::is_same_v<T, LoopStmt>)
          return "loop";
        if constexpr (std::is_same_v<T, SwitchStmt>)
          return "switch";
        if constexpr (std::is_same_v<T, ReturnStmt>)
          return "return";
        if constexpr (std::is_same_v<T, ThrowStmt>)
          return "throw";
        if constexpr (std::is_same_v<T, TryStmt>)
          return "try";
        if constexpr (std::is_same_v<T, ExprStmt>)
          return "expression";
        return "statement";
      },
      statement.node);
}

std::string statementName(const Stmt &statement) {
  return std::visit(
      [](const auto &node) -> std::string {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ImportDecl>)
          return node.alias;
        if constexpr (std::is_same_v<T, VarDecl> || std::is_same_v<T, FunctionDecl> ||
                      std::is_same_v<T, ClassDecl> || std::is_same_v<T, InterfaceDecl>)
          return node.name;
        return {};
      },
      statement.node);
}

bool statementExported(const Stmt &statement) {
  return std::visit(
      [](const auto &node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, VarDecl> || std::is_same_v<T, FunctionDecl> ||
                      std::is_same_v<T, ClassDecl> || std::is_same_v<T, InterfaceDecl>)
          return node.exported;
        return false;
      },
      statement.node);
}

} // namespace kyna::detail
