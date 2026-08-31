#pragma once

#include "kyna/semantics/type_model.hpp"
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace kyna {

enum class BuiltinArgumentKind { Any, String, Integer, Array, Object, Callable };

struct StandardLibrarySymbol {
  std::string_view name;
  bool callable{};
  std::size_t minimumArguments{};
  std::size_t maximumArguments{};
  std::span<const BuiltinArgumentKind> argumentKinds;
  BuiltinArgumentKind variadicKind{BuiltinArgumentKind::Any};
  TypeRef returnType;
};

[[nodiscard]] const StandardLibrarySymbol *
findStandardLibrarySymbol(std::string_view name);
[[nodiscard]] bool acceptsBuiltinArgument(BuiltinArgumentKind expected, const TypeRef &actual);
[[nodiscard]] std::string_view builtinArgumentKindName(BuiltinArgumentKind kind);

} // namespace kyna
