# Type system

The analyzer uses nominal primitive names with explicit nullable and union components. It checks declarations before execution. Compatibility is directional: `int` and `float` fit `num`; `any` bypasses static compatibility only where explicitly written; a value fits a union if it fits one arm; `null` fits nullable types. Unrelated inferred types do not widen to `any`.

Binding mutability is independent of type. The analyzer records `var`/`const` mutability and rejects assignments to `const`, while runtime cells repeat the check as a safety boundary. Members are similarly separate from bindings, allowing a mutable field through an immutable object reference.

Function parameter types and explicit return contracts are checked. An omitted return annotation is currently inferred dynamically as a function result type at call sites; explicit annotations remain strict. `void` and `null` are distinct.

## Interfaces

Interfaces (`intf`) are structural contracts checked at `implements` conformance and object-literal assignment:

```kyna
intf Shape {
  area(): float;
}
intf Named<T> extends Shape {
  name: str;
  label?: str;
  (): int;                 // call signature
  [key: str]: int;         // index signature
}
class Circle implements Named<float> {
  public name: str;
  public fn area(): float { return 3.14; }
  public fn _(): int { return 1; }
}
```

- `extends` merges parent contracts (a base's members are inherited); a generic parent can be specialized, e.g. `extends Base<int>` binds the parent's type parameter.
- Generic interface parameters (`intf Named<T>`) are instantiated at `implements`/`extends` sites, with type substitution applied to inherited fields, methods, call signatures, and index signatures.
- Optional properties (`name?: T`) are not required of implementing classes or object literals.
- `implements` requires compatible public fields and methods; method parameter and return types are checked after generic substitution.

Type-definition files (`.kyna.d`, `.d.ky`, `.ky.d`) are ambient: they declare interfaces used only at compile time and never execute.
