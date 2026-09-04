# Kyna language specification (1.0 development)

## Lexical rules

UTF-8 source is tracked with byte-accurate spans; whitespace and indentation have no semantic meaning. Python-style `#` line comments are preferred. `//` and `/* ... */` remain accepted. Blocks use `{}` and ordinary statements end with `;`. Identifiers begin with an ASCII letter or `_`. String literals use double quotes and character literals use single quotes.

## Bindings and types

`var` creates a mutable, type-locked binding. `const` creates an immutable binding. A type is inferred from an initializer unless written after `:`. `any` is the explicit dynamic escape hatch. The built-ins are `int`, `float`, `num`, `str`, `char`, `bool`, `null`, `void`, and `any`; `int` and `float` are distinct and both are compatible with `num`. Types are non-nullable by default; `T?` is sugar for `T | null`. The legacy spellings `let`, `set`, and `func` remain accepted as aliases of `var`, `const`, and `fn`.

```kyna
var count: int = 1;
const title = "Kyna";
var maybe: str? = null;
```

Objects are closed: an assignment to a field not present in the object/class shape is rejected by the runtime. Arrays use `[a, b]`, zero-based integer indexing, and are mutable reference values; `len`, `push`, and `pop` are standard-library operations. `null` is a value and `void` describes absence of a function result.

## Functions and control flow

Parameters always have types and calls always use parentheses. Return annotations are optional for safely inferred functions and mandatory contracts when present. `if` requires a parenthesized condition and braced branches and supports `else if` chains. `while` and `loop (init; condition; increment)` are statements. `break` and `continue` may name a loop label. `switch` selects among constant case values: each arm is `case VALUE: { ... }`, the optional `default` arm must come last, and control does not fall through. `await expr` is a unary prefix operator that binds a pending result (currently a synchronous pass-through that lets `await fetch(...)` style code read naturally; see `examples/language/await_and_network.kyna`). `match` currently accepts literal and `_` arms; arms use `=>` and terminate with `;`. `throw value;` raises a typed `Error`; non-Error values become an Error whose `cause` is the original value. `catch (failure)` receives that Error and may inspect `failure.code`, `failure.message`, and `failure.cause`. `finally` runs on normal completion, return, caught failure, and rethrow. VM-generated failures use the same exception path and are catchable. `if` is expression-capable; loops are not.

## Objects and classes

Class inheritance is single-parent. Constructors are named `init`, construction uses `new`, and instance access requires explicit `self`. `super.member` resolves a parent member. Unmodified members are private. `protected` is visible to subclasses and `public` is visible everywhere. Static and instance access are distinct. Overrides require `override`, cannot narrow visibility, and must preserve parameter and return contracts. Final classes/methods cannot be extended/overridden. Abstract signatures end in `;`; concrete classes must resolve all abstract methods.

`intf` declares a structural shape. Classes opt in with `implements A, B`; closed object values are checked structurally when used as interface values. Imports create immutable namespaces and expose only declarations explicitly marked `export`. Nested functions are lexical closures: captured bindings remain alive after their declaring call returns, mutable captures are shared by cell, and separate closure factories produce independent state. Traits, generics, richer patterns, and streaming/async networking remain future work.
