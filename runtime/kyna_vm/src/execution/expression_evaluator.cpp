#include "kyna/semantics/modifier_query.hpp"
#include "expression_operations.hpp"
#include "kyna/execution/tree_walk_engine.hpp"

namespace kyna {

Value Interpreter::eval(const ExprPtr &e) {
  return std::visit(
      [this, &e](const auto &n) -> Value {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, Literal>) {
          switch (n.kind) {
          case Literal::Kind::Null:
            return Value();
          case Literal::Kind::Bool:
            return Value(n.value == "true");
          case Literal::Kind::Int:
            return Value(static_cast<int64_t>(std::stoll(n.value)));
          case Literal::Kind::Float:
            return Value(std::stod(n.value));
          case Literal::Kind::String: {
            return Value(decodeExpressionLiteral(n.value));
          }
          case Literal::Kind::Char: {
            const auto value = decodeExpressionLiteral(n.value);
            return Value(value.empty() ? '\0' : value.front());
          }
          }
        } else if constexpr (std::is_same_v<T, Variable>)
          return environment->get(n.name).value;
        else if constexpr (std::is_same_v<T, SelfExpr>)
          return environment->get("self").value;
        else if constexpr (std::is_same_v<T, SuperExpr>)
          return environment->get("self").value;
        else if constexpr (std::is_same_v<T, Unary>) {
          auto v = eval(n.right);
          if (n.op == TokenKind::Bang)
            return Value(!v.isTruthy());
          if (auto i = std::get_if<int64_t>(&v.data))
            return Value(-*i);
          if (auto d = std::get_if<double>(&v.data))
            return Value(-*d);
          throw KynaError({"unary '-' requires a number", {1, 1}, false});
        } else if constexpr (std::is_same_v<T, Binary>) {
          if (n.op == TokenKind::AndAnd) {
            auto l = eval(n.left);
            return l.isTruthy() ? Value(eval(n.right).isTruthy()) : Value(false);
          }
          if (n.op == TokenKind::OrOr) {
            auto l = eval(n.left);
            return l.isTruthy() ? Value(true) : Value(eval(n.right).isTruthy());
          }
          return evaluateExpressionBinary(n.op, eval(n.left), eval(n.right), e->location);
        } else if constexpr (std::is_same_v<T, Assign>) {
          Value v = eval(n.value);
          if (auto x = std::get_if<Variable>(&n.target->node))
            environment->assign(x->name, v);
          else if (auto m = std::get_if<Member>(&n.target->node))
            setMember(m->object, m->name, v);
          else if (auto i = std::get_if<Index>(&n.target->node))
            setIndex(i->object, i->index, v);
          else
            throw KynaError({"invalid assignment target", {1, 1}, false});
          return v;
        } else if constexpr (std::is_same_v<T, Call>)
          return call(n, n.callee);
        else if constexpr (std::is_same_v<T, Member>)
          return getMember(n);
        else if constexpr (std::is_same_v<T, Index>) {
          auto object = eval(n.object);
          auto index = eval(n.index);
          if (std::holds_alternative<std::nullptr_t>(object.data))
            throw KynaError({"cannot index null", n.object->location, false, "KRT2101"});
          if (std::holds_alternative<ArrayPtr>(object.data)) {
            if (!std::holds_alternative<int64_t>(index.data))
              throw KynaError({"array index must be an integer, got '" + index.typeName() + "'",
                               n.index->location, false, "KRT2103"});
            auto array = std::get<ArrayPtr>(object.data);
            auto i = std::get<int64_t>(index.data);
            if (i < 0 || static_cast<size_t>(i) >= array->elements.size())
              throw KynaError({"array index " + std::to_string(i) +
                                   " is out of bounds for length " +
                                   std::to_string(array->elements.size()),
                               n.index->location, false, "KRT2104"});
            return array->elements[static_cast<size_t>(i)];
          }
          if (std::holds_alternative<ObjectPtr>(object.data)) {
            if (!std::holds_alternative<std::string>(index.data))
              throw KynaError({"object key must be a string, got '" + index.typeName() + "'",
                               n.index->location, false, "KRT2103"});
            const auto objectValue = std::get<ObjectPtr>(object.data);
            const auto &key = std::get<std::string>(index.data);
            const auto found = objectValue->fields.find(key);
            if (found == objectValue->fields.end())
              throw KynaError({"object has no field '" + key + "'", n.index->location, false,
                               "KRT2105"});
            return found->second;
          }
          throw KynaError({"cannot index value of type '" + object.typeName() + "'",
                           n.object->location, false, "KRT2102"});
        } else if constexpr (std::is_same_v<T, ArrayExpr>) {
          auto array = objectHeap.allocateArray();
          for (auto &element : n.elements)
            array->elements.push_back(eval(element));
          return Value(array);
        } else if constexpr (std::is_same_v<T, NewExpr>) {
          auto v = environment->get(n.className).value;
          if (!std::holds_alternative<ClassPtr>(v.data))
            throw KynaError({"'" + n.className + "' is not a class", {1, 1}, false});
          auto c = std::get<ClassPtr>(v.data);
          if (hasModifier(c->declaration.modifiers, "abstract"))
            throw KynaError(
                {"cannot instantiate abstract class '" + n.className + "'", {1, 1}, false});
          auto o = objectHeap.allocate();
          o->klass = c;
          std::function<void(const ClassPtr &)> addFields = [&](const ClassPtr &k) {
            if (k->parent)
              addFields(k->parent);
            for (auto &f : k->declaration.fields)
              if (!hasModifier(f.modifiers, "static"))
                o->fields[f.name] = f.initializer ? eval(f.initializer) : Value();
          };
          addFields(c);
          auto init = c->findMethod("init");
          if (init)
            invoke(
                init,
                [&] {
                  std::vector<Value> a;
                  for (auto &x : n.args)
                    a.push_back(eval(x));
                  return a;
                }(),
                o);
          else if (!n.args.empty())
            throw KynaError({"constructor takes no arguments", {1, 1}, false});
          return Value(o);
        } else if constexpr (std::is_same_v<T, ObjectExpr>) {
          auto o = objectHeap.allocate();
          for (auto &f : n.fields)
            o->fields[f.name] = eval(f.value);
          return Value(o);
        } else if constexpr (std::is_same_v<T, IfExpr>) {
          auto b = eval(n.condition).isTruthy() ? n.thenBranch : n.elseBranch;
          auto *p = std::get_if<BlockStmt>(&b->node);
          if (!p) {
            exec(b);
            return Value();
          }
          auto old = environment;
          environment = std::make_shared<Environment>(environment);
          for (auto &s : p->statements)
            exec(s);
          Value r = p->tail ? eval(p->tail) : Value();
          environment = old;
          return r;
        } else if constexpr (std::is_same_v<T, MatchExpr>) {
          auto subject = eval(n.subject);
          for (auto &a : n.arms)
            if (a.wildcard || subject.equals(eval(a.pattern)))
              return eval(a.value);
          throw KynaError({"non-exhaustive match at runtime", {1, 1}, false});
        } else
          return Value();
      },
      e->node);
}
Value Interpreter::call(const Call &c, const ExprPtr &callee) {
  Value v = eval(callee);
  std::vector<Value> a;
  for (auto &e : c.args)
    a.push_back(eval(e));
  if (auto module = std::get_if<ModulePtr>(&v.data)) {
    if (!*module)
      throw KynaError({"cannot call a null module", callee->location, false});
    // `add(...)` where `add` is a JavaScript-style import bound to a module
    // namespace: resolve the exported function of the same name.
    if (const auto *name = std::get_if<Variable>(&callee->node)) {
      const auto member = (*module)->environment->get(name->name);
      if (std::holds_alternative<FunctionPtr>(member.value.data)) {
        const auto function = std::get<FunctionPtr>(member.value.data);
        return invoke(function, a, function->boundThis);
      }
    }
    throw KynaError(
        {"value of type '" + v.typeName() + "' is not callable as a function", callee->location,
         false});
  }
  if (!std::holds_alternative<FunctionPtr>(v.data))
    throw KynaError(
        {"value of type '" + v.typeName() + "' is not callable", callee->location, false});
  const auto function = std::get<FunctionPtr>(v.data);
  try {
    return invoke(function, a, function->boundThis);
  } catch (RuntimeThrownError &error) {
    error.frames.push_back(
        {function->native || function->declaration.name.empty() ? "<native>"
                                                                : function->declaration.name,
         callee->location});
    throw;
  } catch (const KynaError &error) {
    auto diagnostic = error.diagnostic;
    if (!diagnostic.location.known())
      diagnostic.location = callee->location;
    diagnostic.callFrames.push_back(
        {function->native || function->declaration.name.empty() ? "<native>"
                                                                : function->declaration.name,
         callee->location});
    throw KynaError(diagnostic);
  }
}
Value Interpreter::getMember(const Member &m) {
  if (std::holds_alternative<SuperExpr>(m.object->node)) {
    auto self = environment->get("self").value;
    auto o = std::get<ObjectPtr>(self.data);
    auto pc = environment->get("__parent_class").value;
    auto c = std::get<ClassPtr>(pc.data);
    auto f = c->findMethod(m.name);
    if (!f)
      throw KynaError({"parent has no member '" + m.name + "'", {1, 1}, false});
    auto bound = std::make_shared<Function>(*f);
    bound->boundThis = o;
    return Value(bound);
  }
  Value obj = eval(m.object);
  if (std::holds_alternative<std::nullptr_t>(obj.data)) {
    Diagnostic diagnostic{"cannot read member '" + m.name + "' from null", m.object->location,
                          false, "KRT2001"};
    diagnostic.help = "check the value against null before accessing the member";
    throw KynaError(diagnostic);
  }
  if (auto o = std::get_if<ObjectPtr>(&obj.data)) {
    if (auto f = (*o)->fields.find(m.name); f != (*o)->fields.end())
      return f->second;
    auto f = (*o)->klass ? (*o)->klass->findMethod(m.name) : nullptr;
    if (f) {
      auto bound = std::make_shared<Function>(*f);
      bound->boundThis = *o;
      return Value(bound);
    }
  }
  if (auto c = std::get_if<ClassPtr>(&obj.data)) {
    if (auto x = (*c)->staticFields.find(m.name); x != (*c)->staticFields.end())
      return x->second;
    if (auto f = (*c)->findMethod(m.name); f && hasModifier(f->declaration.modifiers, "static"))
      return Value(f);
  }
  if (auto module = std::get_if<ModulePtr>(&obj.data)) {
    if (!*module || !(*module)->exports.contains(m.name))
      throw KynaError(
          {"module has no exported member '" + m.name + "'", m.object->location, false, "K5008"});
    return (*module)->environment->get(m.name).value;
  }
  if (auto error = std::get_if<ErrorPtr>(&obj.data); error && *error) {
    if (m.name == "message")
      return Value((*error)->message);
    if (m.name == "code")
      return Value((*error)->code);
    if (m.name == "cause")
      return (*error)->cause;
    throw KynaError({"Error has no member '" + m.name + "'", m.object->location, false,
                     "KRT2302"});
  }
  throw KynaError({"value of type '" + obj.typeName() + "' has no member '" + m.name + "'",
                   m.object->location, false, "KRT2002"});
}
void Interpreter::setIndex(const ExprPtr &o, const ExprPtr &i, Value v) {
  auto object = eval(o);
  auto index = eval(i);
  if (std::holds_alternative<std::nullptr_t>(object.data))
    throw KynaError({"cannot assign through null", o->location, false, "KRT2101"});
  if (std::holds_alternative<ArrayPtr>(object.data)) {
    if (!std::holds_alternative<int64_t>(index.data))
      throw KynaError({"array index must be an integer, got '" + index.typeName() + "'",
                       i->location, false, "KRT2103"});
    auto array = std::get<ArrayPtr>(object.data);
    auto position = std::get<int64_t>(index.data);
    if (position < 0 || static_cast<size_t>(position) >= array->elements.size())
      throw KynaError({"array index " + std::to_string(position) +
                           " is out of bounds for length " +
                           std::to_string(array->elements.size()),
                       i->location, false, "KRT2104"});
    array->elements[static_cast<size_t>(position)] = std::move(v);
    return;
  }
  if (std::holds_alternative<ObjectPtr>(object.data)) {
    if (!std::holds_alternative<std::string>(index.data))
      throw KynaError({"object key must be a string, got '" + index.typeName() + "'",
                       i->location, false, "KRT2103"});
    auto objectValue = std::get<ObjectPtr>(object.data);
    const auto &key = std::get<std::string>(index.data);
    if (!objectValue->fields.contains(key))
      throw KynaError({"unknown field '" + key + "' on closed object", i->location, false,
                       "KRT2105"});
    objectValue->fields.insert_or_assign(key, std::move(v));
    return;
  }
  throw KynaError({"cannot index value of type '" + object.typeName() + "'", o->location,
                   false, "KRT2102"});
}
void Interpreter::setMember(const ExprPtr &o, const std::string &n, Value v) {
  auto obj = eval(o);
  if (std::holds_alternative<std::nullptr_t>(obj.data))
    throw KynaError({"cannot assign member '" + n + "' through null", o->location, false,
                     "KRT2001"});
  if (!std::holds_alternative<ObjectPtr>(obj.data))
    throw KynaError({"member assignment requires an object, got '" + obj.typeName() + "'",
                     o->location, false, "KRT2003"});
  auto x = std::get<ObjectPtr>(obj.data);
  if (!x->fields.contains(n))
    throw KynaError({"unknown field '" + n + "' on closed object", o->location, false,
                     "KRT2004"});
  x->fields[n] = std::move(v);
}
} // namespace kyna
