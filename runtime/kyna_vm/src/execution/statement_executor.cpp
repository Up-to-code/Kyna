#include "kyna/semantics/modifier_query.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include <exception>

namespace kyna {
Interpreter::Interpreter(RuntimeCapabilities runtimeCapabilities, RuntimeInitializer initialize)
    : global(std::make_shared<Environment>()), environment(global),
      capabilities(std::move(runtimeCapabilities)) {
  if (initialize)
    initialize(*this);
}
Value Interpreter::execute(const std::vector<StmtPtr> &program) {
  Value last;
  for (auto &s : program) {
    exec(s);
    objectHeap.maybeCollect(rootEnvironments());
    if (flow.kind != Flow::None)
      break;
  }
  return last;
}
std::vector<Environment *> Interpreter::rootEnvironments() const {
  std::vector<Environment *> roots{global.get(), environment.get()};
  for (const auto &module : moduleRoots)
    if (module)
      roots.push_back(module.get());
  return roots;
}
Value Interpreter::executeIn(const std::vector<StmtPtr> &program,
                             std::shared_ptr<Environment> module) {
  const auto previous = environment;
  environment = std::move(module);
  moduleRoots.push_back(environment);
  auto result = execute(program);
  environment = previous;
  return result;
}
void Interpreter::execute(const StmtPtr &s) { exec(s); }
Value Interpreter::evaluate(const ExprPtr &e) { return eval(e); }
void Interpreter::execBlock(const BlockStmt &b, std::shared_ptr<Environment> env) {
  auto old = environment;
  environment = std::move(env);
  try {
    for (auto &s : b.statements) {
      exec(s);
      if (flow.kind != Flow::None)
        break;
    }
  } catch (...) {
    environment = old;
    throw;
  }
  environment = old;
}
void Interpreter::exec(const StmtPtr &s) {
  std::visit(
      [this](const auto &n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, VarDecl>) {
          Value v = n.initializer ? eval(n.initializer) : Value();
          environment->define(n.name, v, n.mutableBinding);
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
          eval(n.expression);
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
          execBlock(n, std::make_shared<Environment>(environment));
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          if (eval(n.condition).isTruthy())
            exec(n.thenBranch);
          else if (n.elseBranch)
            exec(n.elseBranch);
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
          ++loopDepth;
          while (eval(n.condition).isTruthy()) {
            exec(n.body);
            if (flow.kind == Flow::Return)
              break;
            if (flow.kind == Flow::Break) {
              if (flow.label.empty() || flow.label == n.label)
                flow = {};
              else
                break;
              break;
            }
            if (flow.kind == Flow::Continue) {
              if (flow.label.empty() || flow.label == n.label)
                flow = {};
              else
                break;
            }
          }
          --loopDepth;
        } else if constexpr (std::is_same_v<T, LoopStmt>) {
          if (n.initializer)
            exec(n.initializer);
          ++loopDepth;
          while (!n.condition || eval(n.condition).isTruthy()) {
            exec(n.body);
            if (flow.kind == Flow::Return)
              break;
            if (flow.kind == Flow::Break) {
              if (flow.label.empty() || flow.label == n.label)
                flow = {};
              else
                break;
              break;
            }
            if (flow.kind == Flow::Continue) {
              if (flow.label.empty() || flow.label == n.label)
                flow = {};
              else
                break;
            }
            if (n.increment)
              eval(n.increment);
          }
          --loopDepth;
        } else if constexpr (std::is_same_v<T, TryStmt>) {
          std::exception_ptr pendingFailure;
          try {
            exec(n.tryBranch);
          } catch (const RuntimeThrownError &error) {
            if (!n.catchBranch) {
              pendingFailure = std::current_exception();
            } else {
              auto old = environment;
              environment = std::make_shared<Environment>(old);
              environment->define(n.catchName, Value(error.value), false);
              try {
                exec(n.catchBranch);
              } catch (...) {
                environment = old;
                pendingFailure = std::current_exception();
              }
              environment = old;
            }
          } catch (const KynaError &error) {
            if (!n.catchBranch) {
              pendingFailure = std::current_exception();
            } else {
              auto old = environment;
              environment = std::make_shared<Environment>(old);
              const auto code = error.diagnostic.code == "K0000" ? "KRT2300"
                                                                  : error.diagnostic.code;
              environment->define(
                  n.catchName,
                  Value(objectHeap.allocateError(error.what(), code, Value())), false);
              try {
                exec(n.catchBranch);
              } catch (...) {
                environment = old;
                pendingFailure = std::current_exception();
              }
              environment = old;
            }
          } catch (...) {
            pendingFailure = std::current_exception();
          }
          const auto pendingFlow = flow;
          if (n.finallyBranch) {
            flow = {};
            exec(n.finallyBranch);
            if (flow.kind == Flow::None)
              flow = pendingFlow;
          }
          if (pendingFailure && flow.kind == Flow::None)
            std::rethrow_exception(pendingFailure);
        } else if constexpr (std::is_same_v<T, SwitchStmt>) {
          const auto subject = eval(n.subject);
          ++switchDepth;
          for (const auto &arm : n.cases) {
            const bool selected = arm.isDefault || subject.equals(eval(arm.value));
            if (!selected)
              continue;
            exec(arm.body);
            if (flow.kind == Flow::Break && flow.label.empty())
              flow = {};
            break;
          }
          --switchDepth;
        } else if constexpr (std::is_same_v<T, ThrowStmt>) {
          const auto value = eval(n.value);
          if (const auto *error = std::get_if<ErrorPtr>(&value.data); error && *error)
            throw RuntimeThrownError(*error);
          throw RuntimeThrownError(
              objectHeap.allocateError(value.display(), "KRT2301", value));
        } else if constexpr (std::is_same_v<T, BreakStmt>) {
          if (loopDepth == 0 && switchDepth == 0)
            throw KynaError({"break must be inside a switch or loop", {1, 1}, false});
          flow = {Flow::Break, {}, n.label};
        } else if constexpr (std::is_same_v<T, ContinueStmt>) {
          if (loopDepth == 0)
            throw KynaError({"continue must be inside a loop", {1, 1}, false});
          flow = {Flow::Continue, {}, n.label};
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
          flow = {Flow::Return, n.value ? eval(n.value) : Value(), ""};
        } else if constexpr (std::is_same_v<T, FunctionDecl>) {
          auto f = std::make_shared<Function>();
          f->declaration = n;
          f->closure = environment;
          environment->define(n.name, Value(f), false);
        } else if constexpr (std::is_same_v<T, ClassDecl>) {
          auto c = std::make_shared<Class>();
          c->declaration = n;
          if (!n.parent.empty()) {
            auto v = environment->get(n.parent).value;
            if (!std::holds_alternative<ClassPtr>(v.data))
              throw KynaError({"'" + n.parent + "' is not a class", {1, 1}, false});
            c->parent = std::get<ClassPtr>(v.data);
          }
          for (auto &field : n.fields)
            if (hasModifier(field.modifiers, "static"))
              c->staticFields[field.name] = field.initializer ? eval(field.initializer) : Value();
          for (const auto &m : n.methods) {
            auto f = std::make_shared<Function>();
            f->declaration = m;
            f->closure = environment;
            c->methods[m.name] = f;
          }
          environment->define(n.name, Value(c), false);
        } else if constexpr (std::is_same_v<T, InterfaceDecl>) { /* interfaces are compile-time
                                                                    contracts in v0.1 */
        }
      },
      s->node);
}

Value Interpreter::invoke(const FunctionPtr &f, const std::vector<Value> &args,
                          ObjectPtr thisObject) {
  if (f->native)
    return f->nativeCall(args);
  if (args.size() != f->declaration.params.size())
    throw KynaError({"function '" + f->declaration.name + "' expects " +
                         std::to_string(f->declaration.params.size()) + " argument(s)",
                     {1, 1},
                     false});
  auto old = environment;
  auto oldFlow = flow;
  flow = {};
  environment = std::make_shared<Environment>(f->closure);
  if (thisObject) {
    environment->define("self", Value(thisObject), false);
    if (thisObject->klass && thisObject->klass->parent)
      environment->define("__parent_class", Value(thisObject->klass->parent), false);
  }
  for (size_t i = 0; i < args.size(); ++i)
    environment->define(f->declaration.params[i].name, args[i], false);
  try {
    exec(f->declaration.body);
    Value result = flow.kind == Flow::Return ? flow.value : Value();
    flow = oldFlow;
    environment = old;
    return result;
  } catch (KynaError &error) {
    const auto location = f->declaration.body ? f->declaration.body->location : SourceSpan{};
    error.diagnostic.callFrames.push_back({f->declaration.name, location});
    flow = oldFlow;
    environment = old;
    throw;
  } catch (...) {
    flow = oldFlow;
    environment = old;
    throw;
  }
}
} // namespace kyna
