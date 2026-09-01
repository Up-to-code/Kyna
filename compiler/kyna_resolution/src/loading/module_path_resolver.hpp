#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace kyna::module_loading {

std::filesystem::path canonicalize(const std::filesystem::path &path);
bool isDeclarationFile(const std::filesystem::path &path);
std::filesystem::path resolveModulePath(
    const std::filesystem::path &importer,
    const std::string &requestedPath,
    const std::vector<std::filesystem::path> &modulePaths);

} // namespace kyna::module_loading
