#pragma once

#include "kyna/diagnostics/diagnostic.hpp"
#include "kyna/modules/module_graph.hpp"
#include "kyna/source/source_manager.hpp"
#include <filesystem>
#include <vector>
#include <map>

namespace kyna {

struct ModuleLoadOptions {
  std::vector<std::filesystem::path> modulePaths;
  std::map<std::filesystem::path, std::string> sourceOverlays{};
};

struct ModuleLoadResult {
  ParsedModuleGraph graph;
  std::vector<Diagnostic> diagnostics;
  [[nodiscard]] bool ok() const;
};

ModuleLoadResult loadModuleGraph(SourceManager &sources, const std::filesystem::path &entry,
                                 const ModuleLoadOptions &options = {});
ModuleLoadResult loadModuleGraphWithEntrySource(SourceManager &sources,
                                                const std::filesystem::path &entry,
                                                std::string source,
                                                const ModuleLoadOptions &options = {});

} // namespace kyna
