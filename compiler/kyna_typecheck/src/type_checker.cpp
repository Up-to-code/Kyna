#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/semantics/modifier_query.hpp"
#include "kyna/symbols/standard_library_symbols.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace kyna {
namespace {
TypeRef t(const std::string &name) { return TypeRef{name, false, {}}; }
int visibility(const std::vector<std::string> &modifiers) {
  if (hasModifier(modifiers, "public"))
    return 2;
  if (hasModifier(modifiers, "protected"))
    return 1;
  return 0;
}
} // namespace
TypeRef Analyzer::expr(const ExprPtr &e) {
  return std::visit(
      [this, &e](const auto &n) -> TypeRef {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, Literal>) {
          switch (n.kind) {
          case Literal::Kind::Null:
            return t("null");
          case Literal::Kind::Bool:
            return t("bool");
          case Literal::Kind::Int:
            return t("int");
          case Literal::Kind::Float:
            return t("float");
          case Literal::Kind::String:
            return t("str");
          case Literal::Kind::Char:
            return t("char");
          }
        } else if constexpr (std::is_same_v<T, Variable>) {
          if (!defined(n.name) && !interactive)
            error("undefined name '" + n.name + "'", e->location);
          if (auto bs = bindingScope(n.name))
            return bs->types[n.name];
          if (functions.contains(n.name))
            return t("func");
          if (classes.contains(n.name))
            return t("class:" + n.name);
          return t("any");
        } else if constexpr (std::is_same_v<T, SelfExpr>)
          return currentClass.empty() ? t("object") : t(currentClass);
        else if constexpr (std::is_same_v<T, SuperExpr>) {
          if (!currentClass.empty() && classes.contains(currentClass) &&
              !classes[currentClass].parent.empty())
            return t(classes[currentClass].parent);
          return t("object");
        } else if constexpr (std::is_same_v<T, Unary>) {
          auto x = expr(n.right);
          if (n.op == TokenKind::Bang)
            return t("bool");
          if (x.name != "int" && x.name != "float" && x.name != "num" && x.name != "any")
            error("unary '-' requires a numeric operand", e->location);
          return x;
        } else if constexpr (std::is_same_v<T, Binary>) {
          auto a = expr(n.left), b = expr(n.right);
          if (n.op == TokenKind::EqualEqual || n.op == TokenKind::BangEqual ||
              n.op == TokenKind::AndAnd || n.op == TokenKind::OrOr || n.op == TokenKind::Less ||
              n.op == TokenKind::LessEqual || n.op == TokenKind::Greater ||
              n.op == TokenKind::GreaterEqual)
            return t("bool");
          if (n.op == TokenKind::Plus && (a.name == "str" || b.name == "str"))
            return t("str");
          if (a.name == "any" || b.name == "any")
            return t("any");
          if ((a.name == "int" || a.name == "float" || a.name == "num" || a.name == "any") &&
              (b.name == "int" || b.name == "float" || b.name == "num" || b.name == "any"))
            return (a.name == "float" || b.name == "float")
                       ? t("float")
                       : (a.name == "int" && b.name == "int" ? t("int") : t("num"));
          error("operator requires compatible operands", e->location);
          return t("any");
        } else if constexpr (std::is_same_v<T, Assign>) {
          auto a = expr(n.target), b = expr(n.value);
          if (auto v = std::get_if<Variable>(&n.target->node)) {
            if (auto bs = bindingScope(v->name)) {
              if (!bs->mutableBindings[v->name])
                error("cannot assign to immutable binding '" + v->name + "'", e->location);
              if (!compatible(bs->types[v->name], b))
                error("cannot assign " + b.str() + " to " + bs->types[v->name].str(), e->location);
            }
          } else if (!std::holds_alternative<Member>(n.target->node) &&
                     !std::holds_alternative<Index>(n.target->node))
            error("invalid assignment target", e->location);
          return a;
        } else if constexpr (std::is_same_v<T, Call>) {
          auto c = expr(n.callee);
          std::vector<TypeRef> argumentTypes;
          argumentTypes.reserve(n.args.size());
          for (auto &a : n.args)
            argumentTypes.push_back(expr(a));
          if (auto v = std::get_if<Variable>(&n.callee->node); v && functions.contains(v->name)) {
            auto &f = functions[v->name];
            if (f.params.size() != n.args.size())
              error("function '" + v->name + "' expects " +
                        std::to_string(f.params.size()) + " argument(s), but " +
                        std::to_string(n.args.size()) + " were provided",
                    e->location, "KSEM1201", "match the declared parameter list");
            for (size_t i = 0; i < std::min(f.params.size(), n.args.size()); ++i) {
              const auto &at = argumentTypes[i];
              if (!compatible(f.params[i].type, at))
                error("argument " + std::to_string(i + 1) + " to '" + v->name + "' has type " +
                          at.str() + ", expected " + f.params[i].type.str(),
                      n.args[i]->location, "KSEM1202",
                      "pass a value compatible with parameter '" + f.params[i].name + "'");
            }
            return f.hasReturnType ? f.returnType : t("any");
          }
          // JavaScript-style imported module function: `add(...)` where the
          // callee name resolves inside the bound module namespace. Checked
          // before standard-library symbols so an import shadows a builtin.
          if (auto imported = std::get_if<Variable>(&n.callee->node);
              imported && c.name.starts_with("module:")) {
            const auto alias = c.name.substr(std::string_view("module:").size());
            const auto module = moduleExports.find(alias);
            if (module != moduleExports.end() && module->second.contains(imported->name))
              return module->second.at(imported->name);
          }
          if (auto variable = std::get_if<Variable>(&n.callee->node)) {
            if (const auto *builtin = findStandardLibrarySymbol(variable->name);
                builtin && builtin->callable) {
              if (n.args.size() < builtin->minimumArguments ||
                  n.args.size() > builtin->maximumArguments) {
                const auto expected = builtin->minimumArguments == builtin->maximumArguments
                                          ? std::to_string(builtin->minimumArguments)
                                          : std::to_string(builtin->minimumArguments) + " to " +
                                                (builtin->maximumArguments ==
                                                         std::numeric_limits<std::size_t>::max()
                                                     ? std::string("any number of")
                                                     : std::to_string(builtin->maximumArguments));
                error("standard-library function '" + variable->name + "' expects " + expected +
                          " argument(s), but " + std::to_string(n.args.size()) + " were provided",
                      e->location, "KSEM1204", "use the documented function signature");
              }
              for (std::size_t index = 0;
                   index < std::min(argumentTypes.size(), builtin->argumentKinds.size()); ++index) {
                if (!acceptsBuiltinArgument(builtin->argumentKinds[index], argumentTypes[index]))
                  error("argument " + std::to_string(index + 1) + " to '" + variable->name +
                            "' has type " + argumentTypes[index].str() + ", expected " +
                            std::string(builtinArgumentKindName(builtin->argumentKinds[index])),
                        n.args[index]->location, "KSEM1205",
                        "pass a value matching the standard-library function signature");
              }
              return builtin->returnType;
            }
          }
          if (std::holds_alternative<Member>(n.callee->node))
            return c;
          if (c.name != "func" && c.name != "any")
            error("value of type '" + c.str() + "' is not callable", n.callee->location,
                  "KSEM1203", "call a function value or check the binding's type");
          return t("any");
        } else if constexpr (std::is_same_v<T, Member>) {
          auto objectType = expr(n.object);
          if (objectType.name == "null") {
            error("cannot read member '" + n.name + "' from null", n.object->location,
                  "KSEM2401", "check the value against null before accessing the member");
            return t("any");
          }
          constexpr std::string_view modulePrefix = "module:";
          if (objectType.name.starts_with(modulePrefix)) {
            const auto alias = objectType.name.substr(modulePrefix.size());
            const auto module = moduleExports.find(alias);
            if (module == moduleExports.end() || !module->second.contains(n.name)) {
              error("module '" + alias + "' has no exported member '" + n.name + "'", e->location);
              return t("any");
            }
            return module->second.at(n.name);
          }
          if (objectType.name == "any" || objectType.name == "object")
            return t("any");
          if (objectType.name == "Error") {
            if (n.name == "message" || n.name == "code")
              return t("str");
            if (n.name == "cause")
              return t("any");
            error("Error has no member '" + n.name + "'", e->location, "KSEM2404",
                  "use message, code, or cause");
            return t("any");
          }
          bool staticAccess = false;
          std::string className = objectType.name;
          constexpr std::string_view classPrefix = "class:";
          if (className.starts_with(classPrefix)) {
            staticAccess = true;
            className.erase(0, classPrefix.size());
          }
          if (classes.contains(className)) {
            const ClassDecl *owner = &classes[className];
            const FieldDecl *field = nullptr;
            const FunctionDecl *method = nullptr;
            while (owner) {
              auto fieldIt =
                  std::find_if(owner->fields.begin(), owner->fields.end(),
                               [&](const auto &candidate) { return candidate.name == n.name; });
              auto methodIt =
                  std::find_if(owner->methods.begin(), owner->methods.end(),
                               [&](const auto &candidate) { return candidate.name == n.name; });
              if (fieldIt != owner->fields.end())
                field = &*fieldIt;
              if (methodIt != owner->methods.end())
                method = &*methodIt;
              if (field || method)
                break;
              if (owner->parent.empty() || !classes.contains(owner->parent))
                owner = nullptr;
              else
                owner = &classes[owner->parent];
            }
            const auto *modifiers = field    ? &field->modifiers
                                    : method ? &method->modifiers
                                             : nullptr;
            if (!modifiers) {
              error("unknown member '" + n.name + "' on '" + className + "'", e->location);
              return t("any");
            }
            if (hasModifier(*modifiers, "static") != staticAccess)
              error("member '" + n.name + "' must be accessed through " +
                        std::string(staticAccess ? "an instance" : "the class"),
                    e->location);
            const int access = visibility(*modifiers);
            bool subclassAccess = false;
            for (auto cursor = currentClass; !cursor.empty() && classes.contains(cursor);
                 cursor = classes[cursor].parent)
              if (owner && cursor == owner->name)
                subclassAccess = true;
            if (access == 0 && (!owner || currentClass != owner->name))
              error("private member '" + n.name + "' is not accessible here", e->location);
            if (access == 1 && (!owner || (!subclassAccess && currentClass != owner->name)))
              error("protected member '" + n.name + "' is not accessible here", e->location);
            return field ? field->type : (method->hasReturnType ? method->returnType : t("any"));
          }
          if (const auto *contract = interfaces.find(className)) {
            std::vector<std::string> stack;
            const auto effective = effectiveContract(*contract, stack);
            // Substitute generic arguments when accessing through an
            // instantiated interface (`repo.findById` on `Repository<User>`
            // yields `User?`, not the raw `T?`).
            const TypeRef contractRef{className, objectType.nullable, objectType.typeArgs};
            const auto field =
                std::find_if(effective.fields.begin(), effective.fields.end(),
                             [&](const auto &candidate) { return candidate.name == n.name; });
            if (field != effective.fields.end())
              return substitute(field->type, *contract, contractRef);
            const auto method =
                std::find_if(effective.methods.begin(), effective.methods.end(),
                             [&](const auto &candidate) { return candidate.name == n.name; });
            if (method != effective.methods.end())
              return substitute(method->returnType, *contract, contractRef);
          }
          error("member access requires a class, interface, object, or module", e->location);
          return t("any");
        } else if constexpr (std::is_same_v<T, Index>) {
          auto object = expr(n.object);
          auto index = expr(n.index);
          if (object.name == "array" && index.name != "int" && index.name != "any")
            error("array index must be int, got '" + index.str() + "'", n.index->location,
                  "KSEM2403", "use an integer index between zero and the array length minus one");
          if (object.name == "object" && index.name != "str" && index.name != "any")
            error("object key must be str, got '" + index.str() + "'", n.index->location,
                  "KSEM2403", "use a string key such as object[\"field\"]");
          if (object.name != "array" && object.name != "object" && object.name != "any")
            error(object.name == "null" ? "cannot index null"
                                          : "indexing requires an array or object, got '" +
                                                object.str() + "'",
                  n.object->location, "KSEM2402",
                  "check the value and its type before indexing it");
          return t("any");
        } else if constexpr (std::is_same_v<T, ArrayExpr>) {
          for (auto &element : n.elements)
            expr(element);
          return t("array");
        } else if constexpr (std::is_same_v<T, NewExpr>) {
          std::vector<TypeRef> argumentTypes;
          for (auto &a : n.args)
            argumentTypes.push_back(expr(a));
          if (!classes.contains(n.className)) {
            error("unknown class '" + n.className + "'", e->location);
          } else {
            const auto *initializer = findMethod(classes[n.className], "init");
            if (!initializer && !n.args.empty())
              error("constructor for '" + n.className + "' takes no arguments", e->location);
            if (initializer) {
              if (initializer->params.size() != n.args.size())
                error("constructor for '" + n.className + "' expects " +
                          std::to_string(initializer->params.size()) + " argument(s)",
                      e->location);
              for (std::size_t index = 0;
                   index < std::min(initializer->params.size(), argumentTypes.size()); ++index)
                if (!compatible(initializer->params[index].type, argumentTypes[index]))
                  error("constructor argument " + std::to_string(index + 1) + " for '" +
                            n.className + "' has incompatible type",
                        n.args[index]->location);
            }
          }
          return t(n.className);
        } else if constexpr (std::is_same_v<T, ObjectExpr>) {
          for (auto &f : n.fields)
            expr(f.value);
          return t("object");
        } else if constexpr (std::is_same_v<T, IfExpr>) {
          auto c = expr(n.condition);
          if (c.name != "bool" && c.name != "any")
            error("if condition must be bool", e->location);
          const auto branchType = [&](const StmtPtr &branch) {
            const auto &block = std::get<BlockStmt>(branch->node);
            auto parentScope = scope;
            scope = std::make_shared<Scope>();
            scope->parent = parentScope;
            for (const auto &statement : block.statements)
              if (const auto *function = std::get_if<FunctionDecl>(&statement->node)) {
                scope->types[function->name] = t("func");
                scope->mutableBindings[function->name] = false;
              }
            for (const auto &statement : block.statements)
              stmt(statement);
            const auto result = block.tail ? expr(block.tail) : t("void");
            scope = parentScope;
            return result;
          };
          auto a = branchType(n.thenBranch);
          auto b = branchType(n.elseBranch);
          return merge(a, b);
        } else if constexpr (std::is_same_v<T, MatchExpr>) {
          const auto subject = expr(n.subject);
          TypeRef r = t("void");
          bool first = true;
          bool wildcardSeen = false;
          bool trueSeen = false;
          bool falseSeen = false;
          std::set<std::string> literalPatterns;
          for (std::size_t index = 0; index < n.arms.size(); ++index) {
            auto &a = n.arms[index];
            if (a.wildcard) {
              if (wildcardSeen || index + 1 != n.arms.size())
                error("match arm is unreachable after a wildcard arm", e->location, "KSEM1402",
                      "keep exactly one wildcard arm and place it last");
              wildcardSeen = true;
            } else {
              if (wildcardSeen)
                error("match arm is unreachable after a wildcard arm", a.pattern->location,
                      "KSEM1402", "move the wildcard arm to the end");
              const auto patternType = expr(a.pattern);
              if (!compatible(subject, patternType))
                error("match pattern has type " + patternType.str() + ", but subject has type " +
                          subject.str(),
                      a.pattern->location, "KSEM1404",
                      "use a pattern compatible with the matched value");
              if (const auto *literal = std::get_if<Literal>(&a.pattern->node)) {
                const auto key = std::to_string(static_cast<int>(literal->kind)) + ':' +
                                 literal->value;
                if (!literalPatterns.insert(key).second)
                  error("duplicate literal pattern in match", a.pattern->location, "KSEM1403",
                        "remove the duplicate arm or use a different pattern");
                if (literal->kind == Literal::Kind::Bool)
                  (literal->value == "true" ? trueSeen : falseSeen) = true;
              }
            }
            auto arm = expr(a.value);
            if (first) {
              r = arm;
              first = false;
            } else
              r = merge(r, arm);
          }
          if (!wildcardSeen && !(subject.name == "bool" && trueSeen && falseSeen))
            error("match expression is not exhaustive", e->location, "KSEM1401",
                  "add a final '_ => value;' arm");
          return r;
        } else
          return t("any");
      },
      e->node);
}
} // namespace kyna
