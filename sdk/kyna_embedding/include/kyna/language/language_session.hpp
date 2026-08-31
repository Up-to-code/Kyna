#pragma once

#include "kyna/execution/tree_walk_interpreter.hpp"
#include "kyna/modules/module_loader.hpp"
#include "kyna/semantics/module_analyzer.hpp"
#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/source/source_manager.hpp"
#include <filesystem>
#include <string>

namespace kyna {

struct LanguageSessionOptions {
  std::vector<std::filesystem::path> modulePaths;
  RuntimeCapabilities capabilities{productionRuntimeCapabilities()};
};

struct LanguageResult {
  std::vector<Diagnostic> diagnostics;
  bool executed{false};
  HeapStats heapStats;
  [[nodiscard]] bool ok() const;
};

struct InspectionResult {
  std::string output;
  std::vector<Diagnostic> diagnostics;
  [[nodiscard]] bool ok() const;
};

class LanguageSession {
public:
  explicit LanguageSession(LanguageSessionOptions options = {});
  LanguageResult check(const std::filesystem::path &entry);
  LanguageResult run(const std::filesystem::path &entry);
  LanguageResult checkSource(std::string name, std::string source);
  LanguageResult checkSourceAtPath(const std::filesystem::path &entry, std::string source);
  LanguageResult runSource(std::string name, std::string source, bool interactive = false);
  InspectionResult inspectTokens(std::string name, std::string source, bool json = false);
  InspectionResult inspectSyntax(std::string name, std::string source, bool json = false);
  InspectionResult inspectHir(std::string name, std::string source, bool json = false);
  InspectionResult inspectMir(std::string name, std::string source, bool json = false);
  InspectionResult inspectBytecode(std::string name, std::string source, bool json = false);
  SourceManager &sourceManager() { return sources; }

private:
  LanguageSessionOptions options;
  SourceManager sources;
  TreeWalkInterpreter executor;
  Analyzer interactiveAnalyzer;

  AnalysisResult compile(const std::filesystem::path &entry, std::vector<Diagnostic> &frontEnd);
};

} // namespace kyna
