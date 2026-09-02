# Object model

Kyna uses one nominal class parent. `intf` is structural: a class explicitly names contracts with `implements`, while closed object values are checked structurally at interface assignment and argument boundaries. No interface state or implementation is copied. A class may implement many interfaces but may extend only one class.

Generic declarations will be represented as type parameters with optional upper bounds, for example `class Box<T>` and `fn first<T>(items: List<T>): T`. Instantiation specializes types in the analyzer; runtime class/function objects remain monomorphic and carry no source-language type erasure surprises. Generic constraints are checked before interpretation.

Unmodified members are private. The analyzer enforces protected/public access, static versus instance access, explicit and compatible overrides, final restrictions, abstract obligations, constructor arguments, and return paths. Traits and generics remain reserved syntax/design work and are not silently treated as `any`.
