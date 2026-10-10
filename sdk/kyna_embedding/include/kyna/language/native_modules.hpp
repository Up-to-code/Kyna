#pragma once
#include <kyna/language/native_functions.hpp>
#include <filesystem>
namespace kyna {
// Loads trusted executable code. Returned functions own the module's lifetime.
// Throws std::runtime_error for load, ABI, or descriptor failures.
std::vector<NativeFunction> loadNativeModule(const std::filesystem::path &path);
}
