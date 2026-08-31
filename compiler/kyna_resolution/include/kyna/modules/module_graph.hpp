#pragma once

#include "kyna/syntax/syntax_tree.hpp"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace kyna {

struct ModuleDependency {
  std::string alias;
  std::filesystem::path canonicalPath;
  SourceSpan location;
};

struct ModuleRecord {
  SyntaxTree syntax;
  std::vector<ModuleDependency> dependencies;
  // True for ambient type-definition files (.kyna.d, .d.ky, .ky.d): their
  // declarations contribute types only and never emit runtime code.
  bool isDeclaration{false};
};

struct ParsedModuleGraph {
  std::filesystem::path entry;
  std::map<std::filesystem::path, ModuleRecord> modules;
  std::vector<std::filesystem::path> initializationOrder;
};

} // namespace kyna
