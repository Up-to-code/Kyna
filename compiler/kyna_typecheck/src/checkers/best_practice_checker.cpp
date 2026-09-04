#include "best_practice_checker.hpp"
#include <algorithm>
#include <optional>
#include <set>
#include <string>

namespace kyna {
namespace {

class BestPracticeChecker {
public:
  std::vector<Diagnostic> check(const std::vector<StmtPtr> &declarations) {
    for (const auto &declaration : declarations)
      statement(declaration, false);
    return std::move(diagnostics);
  }

private:
  std::vector<Diagnostic> diagnostics;

  static std::optional<std::string> calleeName(const ExprPtr &expression) {
    if (!expression)
      return std::nullopt;
    if (const auto *variable = std::get_if<Variable>(&expression->node))
      return variable->name;
    if (const auto *member = std::get_if<Member>(&expression->node)) {
      const auto owner = calleeName(member->object);
      return owner ? *owner + "." + member->name : member->name;
    }
    return std::nullopt;
  }

  void warning(std::string code, std::string message, SourceSpan span, std::string help) {
    Diagnostic diagnostic{std::move(message), span, true, std::move(code)};
    diagnostic.help = std::move(help);
    diagnostics.push_back(std::move(diagnostic));
  }

  void inspectCall(const Call &call, bool protectedByTry, bool resultUsed) {
    const auto name = calleeName(call.callee);
    if (!name)
      return;

    static const std::set<std::string> fallibleOperations{
        "fetch",        "httpGet",       "readFile",        "writeFile",
        "readJsonFile", "writeJsonFile", "createDirectory", "listDirectory",
        "removePath",   "jsonParse",     "process.json",    "fs.read",
        "fs.write",     "fs.readJson",   "fs.writeJson",    "fs.createDirectory",
        "fs.list",      "fs.remove",     "db.query",        "db.execute"};
    static const std::set<std::string> returnedValues{
        "fetch",     "httpGet",      "readFile",    "readJsonFile", "listDirectory",
        "jsonParse", "process.json", "filter",      "sort",         "bubbleSort",
        "call",      "fs.read",      "fs.readJson", "fs.list", "db.query", "db.execute",
        "map",       "reduce",       "find",        "any",     "all",      "unique",
        "collections.map", "collections.reduce", "collections.find", "collections.any",
        "collections.all", "collections.unique"};

    const auto location = call.callee ? call.callee->location : SourceSpan{};
    if (fallibleOperations.contains(*name) && !protectedByTry)
      warning("K2601", "fallible operation '" + *name + "' is not protected by try/catch", location,
              "move this call into the try block and handle its failure in catch");
    if (returnedValues.contains(*name) && !resultUsed)
      warning("K2605", "result of '" + *name + "' is ignored", location,
              "assign the result to a binding or use it directly");
    if ((*name == "processRun" || *name == "build" || *name == "process.run") && !call.args.empty())
      warning("K2604", "shell command execution requires trusted input", location,
              "never concatenate untrusted data into a shell command");

    if ((*name == "db.query" || *name == "db.execute") && call.args.size() > 1 &&
        !std::holds_alternative<Literal>(call.args[1]->node))
      warning("K2610", "dynamic SQL text can allow injection", call.args[1]->location,
              "keep SQL text constant and pass data through $1, $2, ... placeholders");

    if ((*name == "fetch" || *name == "httpGet") && !call.args.empty()) {
      if (const auto *literal = std::get_if<Literal>(&call.args.front()->node);
          literal && literal->kind == Literal::Kind::String &&
          (literal->value.starts_with("\"http://") || literal->value.starts_with("'http://")))
        warning("K2603", "plain HTTP does not protect request data", call.args.front()->location,
                "use an https:// URL for network requests");
      if (*name == "fetch") {
        bool hasTimeout = false;
        if (call.args.size() > 1)
          if (const auto *options = std::get_if<ObjectExpr>(&call.args[1]->node))
            hasTimeout = std::any_of(options->fields.begin(), options->fields.end(),
                                     [](const ObjectField &field) { return field.name == "timeout"; });
        if (!hasTimeout)
          warning("K2606", "fetch uses the default network timeout", location,
                  "set a timeout option in milliseconds for predictable backend behavior");
      }
    }
  }

  void expression(const ExprPtr &value, bool protectedByTry, bool resultUsed = true) {
    if (!value)
      return;
    std::visit(
        [&](const auto &node) {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, Unary>) {
            expression(node.right, protectedByTry);
          } else if constexpr (std::is_same_v<T, AwaitExpr>) {
            expression(node.operand, protectedByTry);
          } else if constexpr (std::is_same_v<T, Binary>) {
            expression(node.left, protectedByTry);
            expression(node.right, protectedByTry);
          } else if constexpr (std::is_same_v<T, Assign>) {
            expression(node.target, protectedByTry);
            expression(node.value, protectedByTry);
          } else if constexpr (std::is_same_v<T, Call>) {
            inspectCall(node, protectedByTry, resultUsed);
            expression(node.callee, protectedByTry);
            for (const auto &argument : node.args)
              expression(argument, protectedByTry);
          } else if constexpr (std::is_same_v<T, Member>) {
            expression(node.object, protectedByTry);
          } else if constexpr (std::is_same_v<T, Index>) {
            expression(node.object, protectedByTry);
            expression(node.index, protectedByTry);
          } else if constexpr (std::is_same_v<T, ArrayExpr>) {
            for (const auto &element : node.elements)
              expression(element, protectedByTry);
          } else if constexpr (std::is_same_v<T, NewExpr>) {
            for (const auto &argument : node.args)
              expression(argument, protectedByTry);
          } else if constexpr (std::is_same_v<T, ObjectExpr>) {
            for (const auto &field : node.fields)
              expression(field.value, protectedByTry);
          } else if constexpr (std::is_same_v<T, IfExpr>) {
            expression(node.condition, protectedByTry);
            statement(node.thenBranch, protectedByTry);
            statement(node.elseBranch, protectedByTry);
          } else if constexpr (std::is_same_v<T, MatchExpr>) {
            expression(node.subject, protectedByTry);
            for (const auto &arm : node.arms) {
              expression(arm.pattern, protectedByTry);
              expression(arm.value, protectedByTry);
            }
          }
        },
        value->node);
  }

  void statement(const StmtPtr &value, bool protectedByTry) {
    if (!value)
      return;
    std::visit(
        [&](const auto &node) {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, VarDecl>) {
            expression(node.initializer, protectedByTry);
          } else if constexpr (std::is_same_v<T, ExprStmt>) {
            expression(node.expression, protectedByTry, false);
          } else if constexpr (std::is_same_v<T, BlockStmt>) {
            for (const auto &child : node.statements)
              statement(child, protectedByTry);
            expression(node.tail, protectedByTry);
          } else if constexpr (std::is_same_v<T, IfStmt>) {
            expression(node.condition, protectedByTry);
            statement(node.thenBranch, protectedByTry);
            statement(node.elseBranch, protectedByTry);
          } else if constexpr (std::is_same_v<T, WhileStmt>) {
            expression(node.condition, protectedByTry);
            statement(node.body, protectedByTry);
          } else if constexpr (std::is_same_v<T, LoopStmt>) {
            statement(node.initializer, protectedByTry);
            expression(node.condition, protectedByTry);
            expression(node.increment, protectedByTry);
            statement(node.body, protectedByTry);
          } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            expression(node.value, protectedByTry);
          } else if constexpr (std::is_same_v<T, SwitchStmt>) {
            expression(node.subject, protectedByTry);
            for (const auto &arm : node.cases) {
              expression(arm.value, protectedByTry);
              statement(arm.body, protectedByTry);
            }
          } else if constexpr (std::is_same_v<T, ThrowStmt>) {
            expression(node.value, protectedByTry);
          } else if constexpr (std::is_same_v<T, TryStmt>) {
            statement(node.tryBranch, true);
            if (const auto *block =
                    node.catchBranch ? std::get_if<BlockStmt>(&node.catchBranch->node) : nullptr;
                block && block->statements.empty() && !block->tail)
              warning("K2602", "empty catch block hides a runtime failure",
                      node.catchBranch->location,
                      "log the failure, recover explicitly, or rethrow it with 'throw'");
            statement(node.catchBranch, protectedByTry);
            statement(node.finallyBranch, protectedByTry);
          } else if constexpr (std::is_same_v<T, FunctionDecl>) {
            statement(node.body, false);
          } else if constexpr (std::is_same_v<T, ClassDecl>) {
            for (const auto &field : node.fields)
              expression(field.initializer, false);
            for (const auto &method : node.methods)
              statement(method.body, false);
          }
        },
        value->node);
  }
};

} // namespace

std::vector<Diagnostic> checkBestPractices(const std::vector<StmtPtr> &declarations) {
  return BestPracticeChecker{}.check(declarations);
}

} // namespace kyna
