# Plan 01: Type System Polymorphic Hierarchy

> **Goal**: Replace the flat, string-based `TypeRef` struct with a polymorphic `Type` interface, eliminating string-based type checking, introducing static singletons for primitives, and supporting first-class function signatures and structural types.
> **Inspiration**: Go's `go/types/type.go`, `cmd/compile/internal/types2`, and `universe.go`.

---

## 1. Problem Statement & Root Cause

Currently, Kyna defines all types in `compiler/kyna_types/include/kyna/semantics/type_model.hpp` via:

```cpp
struct TypeRef {
  std::string name{"void"};
  bool nullable{false};
  std::vector<TypeRef> typeArgs;
  std::vector<TypeRef> unionTypes;
  std::string str() const;
};
```

### Critical Flaws
1. **Dynamic Heap String Allocations**: Every primitive type check (`t("int")`, `t("str")`, `t("bool")`) allocates a new `std::string` on the heap.
2. **Loss of Function Signatures**: Higher-order functions collapse to `TypeRef{"func"}`, making it impossible to check argument or return type compatibility of callbacks.
3. **Encoding Metadata in String Names**: Classes are prefixed with `"class:Circle"`, modules with `"module:math"`, and unions with `"union"`.
4. **Equality via Serialization**: Type checks rely on `left.type.str() == right.type.str()`, which is slow and prone to formatting bugs (e.g. `TypeRef::str()` formatting `"union | int | str"`).

---

## 2. Target Architecture: The Polymorphic `Type` Hierarchy

### 2.1 The `Type` Interface
```cpp
namespace kyna::types {

enum class TypeKind : uint8_t {
  Basic,
  Named,
  Signature,
  Interface,
  Struct,
  Array,
  Map,
  Union,
  Nullable,
  TypeParam
};

class Type {
public:
  virtual ~Type() = default;
  virtual TypeKind kind() const = 0;
  virtual const Type* underlying() const = 0;
  virtual std::string str() const = 0;
  virtual bool isAssignableTo(const Type* target) const = 0;
  virtual bool isIdenticalTo(const Type* other) const = 0;
};

using TypePtr = const Type*; // Types are interned and immutable

} // namespace kyna::types
```

### 2.2 Concrete Type Implementations

1. **`BasicType`**: Singleton instances for `int`, `float`, `bool`, `string`, `void`, `null`, `any`.
   ```cpp
   class BasicType : public Type {
   public:
     enum class BasicKind { Int, Float, Bool, String, Void, Null, Any };
     BasicKind basicKind() const;
     TypeKind kind() const override { return TypeKind::Basic; }
     const Type* underlying() const override { return this; }
     std::string str() const override;
     bool isAssignableTo(const Type* target) const override;
     bool isIdenticalTo(const Type* other) const override;
   };
   ```

2. **`SignatureType`**: First-class function signatures.
   ```cpp
   class SignatureType : public Type {
   private:
     std::vector<TypePtr> params_;
     TypePtr returnType_;
     TypePtr receiverType_{nullptr}; // for class methods
     bool isVariadic_{false};
   public:
     TypeKind kind() const override { return TypeKind::Signature; }
     const std::vector<TypePtr>& params() const { return params_; }
     TypePtr returnType() const { return returnType_; }
     TypePtr receiverType() const { return receiverType_; }
     // ...
   };
   ```

3. **`NamedType`**: Nominal classes and user-defined types.
   ```cpp
   class NamedType : public Type {
   private:
     std::string name_;
     TypePtr underlying_{nullptr};
     std::vector<MethodSymbol> methods_;
   public:
     TypeKind kind() const override { return TypeKind::Named; }
     const std::string& name() const { return name_; }
     const Type* underlying() const override { return underlying_; }
     // ...
   };
   ```

4. **`Universe`**: Zero-allocation static registry.
   ```cpp
   class Universe {
   public:
     static const BasicType* Int();
     static const BasicType* Float();
     static const BasicType* Bool();
     static const BasicType* String();
     static const BasicType* Void();
     static const BasicType* Null();
     static const BasicType* Any();
   };
   ```

---

## 3. Implementation Steps

- [ ] **Step 1: Create `compiler/kyna_types/include/kyna/types/type.hpp`**
  - Define `TypeKind`, `Type` abstract interface, and `Universe` factory.
- [ ] **Step 2: Implement Concrete Types**
  - Implement `BasicType`, `SignatureType`, `NamedType`, `UnionType`, `NullableType` under `compiler/kyna_types/src/types/`.
- [ ] **Step 3: Update `CMakeLists.txt`**
  - Add newly created source files explicitly to `compiler/kyna_types/CMakeLists.txt`.
- [ ] **Step 4: Provide Backward Compatibility Adapter**
  - Maintain `TypeRef` as a thin wrapper or transition layer so existing code compiles incrementally.
- [ ] **Step 5: Migrate Semantic Type Checker**
  - Update `type_checker.cpp` and `analyzer.cpp` to use `Universe::Int()`, `Universe::Bool()`, and pointer comparisons instead of string matching.
- [ ] **Step 6: Eliminate `TypeRef::str()` Serialization for Comparison**
  - Replace `sameParameters()` in `statement_checker.cpp` with `Type::isIdenticalTo()`.

---

## 4. Verification Plan

1. **Unit Tests**:
   - Create `tests/types/test_type_hierarchy.cpp` verifying pointer identity, assignability, and immutability.
2. **Regression Verification**:
   - Run `ctest --test-dir build-debug --output-on-failure`.
3. **Repository Architecture Verifier**:
   - Run `python3 build_tools/verify_repository_architecture.py`.
