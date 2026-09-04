#include <kyna/lexing/tokenizer.hpp>
#include <kyna/modules/module_loader.hpp>
#include <kyna/modules/package_loader.hpp>
#include <kyna/parsing/module_parser.hpp>
#include "module_path_resolver.hpp"
#include <algorithm>
#include <map>
#include <optional>
#include <set>

namespace kyna {
namespace {

class GraphLoader {
public:
  GraphLoader(
      SourceManager &sourceManager, const ModuleLoadOptions &loadOptions,
      std::optional<std::pair<std::filesystem::path, std::string>> entryOverlay = std::nullopt)
      : sources(sourceManager), options(loadOptions), overlay(std::move(entryOverlay)) {
    if (overlay)
      overlay->first = module_loading::canonicalize(overlay->first);
  }

  ModuleLoadResult load(const std::filesystem::path &entryPath) {
    auto entry = module_loading::canonicalize(entryPath);
    std::error_code error;
    if (std::filesystem::is_directory(entry, error))
      return loadDirectoryPackage(entry);
    result.graph.entry = entry;
    visit(entry, {});
    return std::move(result);
  }

private:
  SourceManager &sources;
  const ModuleLoadOptions &options;
  ModuleLoadResult result;
  std::map<std::filesystem::path, int> state;
  std::vector<std::filesystem::path> stack;
  std::optional<std::pair<std::filesystem::path, std::string>> overlay;

  void report(std::string message, SourceSpan span, std::string code) {
    Diagnostic diagnostic{std::move(message), span, false};
    diagnostic.code = std::move(code);
    result.diagnostics.push_back(std::move(diagnostic));
  }

  void visit(const std::filesystem::path &path, SourceSpan importSpan) {
    std::error_code directoryError;
    if (std::filesystem::is_directory(path, directoryError)) {
      ingestDirectoryPackage(path, importSpan, false);
      return;
    }
    const auto existing = state[path];
    if (existing == 2)
      return;
    if (existing == 1) {
      std::string chain;
      auto start = std::find(stack.begin(), stack.end(), path);
      for (auto item = start; item != stack.end(); ++item) {
        if (!chain.empty())
          chain += " -> ";
        chain += item->filename().string();
      }
      chain += " -> " + path.filename().string();
      report("module import cycle: " + chain, importSpan, "K4002");
      return;
    }

    std::string loadError;
    std::optional<SourceId> sourceId;
    if (overlay && path == overlay->first)
      sourceId = sources.add(path.string(), overlay->second);
    else
      sourceId = sources.load(path, loadError);
    if (!sourceId) {
      report(loadError, importSpan, "K4000");
      return;
    }
    const auto *source = sources.find(*sourceId);
    auto lexed = tokenize(*source);
    result.diagnostics.insert(result.diagnostics.end(), lexed.diagnostics.begin(),
                              lexed.diagnostics.end());
    auto parsed = parseModule(*source, std::move(lexed.tokens));
    result.diagnostics.insert(result.diagnostics.end(), parsed.diagnostics.begin(),
                              parsed.diagnostics.end());

    state[path] = 1;
    stack.push_back(path);
    ModuleRecord record{std::move(parsed.tree),
                        {},
                        module_loading::isDeclarationFile(path),
                        {path}};
    std::set<std::string> aliases;
    for (const auto &statement : record.syntax.module.declarations) {
      const auto *import = std::get_if<ImportDecl>(&statement->node);
      if (!import)
        continue;
      if (!aliases.insert(import->alias).second) {
        report("duplicate module alias '" + import->alias + "'", statement->location, "K4003");
        continue;
      }
      const auto dependency = module_loading::resolveModulePath(path, import->path, options.modulePaths);
      if (!std::filesystem::exists(dependency)) {
        report("cannot resolve module '" + import->path + "'", statement->location, "K4001");
        continue;
      }
      if (!package_loading::isInternalImportAllowed(path, dependency)) {
        report("use of internal package '" + import->path + "' is not allowed from this module",
               statement->location, "KSEM1042");
        continue;
      }
      record.dependencies.push_back({import->alias, dependency, statement->location});
      visit(dependency, statement->location);
    }
    stack.pop_back();
    state[path] = 2;
    result.graph.modules.insert_or_assign(path, std::move(record));
    result.graph.initializationOrder.push_back(path);
  }

  static bool isPackageTestFile(const std::filesystem::path &path) {
    const auto name = path.filename().string();
    return name.ends_with("_test.kyna") || name.ends_with("_test.ky");
  }

  ModuleLoadResult loadDirectoryPackage(const std::filesystem::path &directory) {
    ingestDirectoryPackage(directory, {}, true);
    return std::move(result);
  }

  void ingestDirectoryPackage(const std::filesystem::path &directory, SourceSpan importSpan,
                              bool setEntry) {
    const auto existing = state[directory];
    if (existing == 2)
      return;
    if (existing == 1) {
      report("module import cycle involving package '" + directory.filename().string() + "'",
             importSpan, "K4002");
      return;
    }
    auto discovered = package_loading::discoverPackage(directory);
    std::vector<package_loading::DiscoveredFile> files;
    for (const auto &file : discovered.files)
      if (!isPackageTestFile(file.path))
        files.push_back(file);
    if (files.empty()) {
      report("package directory contains no Kyna source files", importSpan, "K4000");
      return;
    }

    if (setEntry)
      result.graph.entry = directory;

    state[directory] = 1;
    stack.push_back(directory);

    SyntaxTree merged;
    merged.module.path = directory;
    std::vector<ModuleDependency> dependencies;
    std::set<std::string> aliases;
    std::set<std::filesystem::path> packagePaths;
    packagePaths.insert(module_loading::canonicalize(directory));
    for (const auto &file : files)
      packagePaths.insert(module_loading::canonicalize(file.path));
    bool allDeclaration = true;

    for (const auto &file : files) {
      std::string loadError;
      auto sourceId = sources.load(file.path, loadError);
      if (!sourceId) {
        report(loadError, {}, "K4000");
        continue;
      }
      const auto *source = sources.find(*sourceId);
      auto lexed = tokenize(*source);
      result.diagnostics.insert(result.diagnostics.end(), lexed.diagnostics.begin(),
                                lexed.diagnostics.end());
      auto parsed = parseModule(*source, std::move(lexed.tokens));
      result.diagnostics.insert(result.diagnostics.end(), parsed.diagnostics.begin(),
                                parsed.diagnostics.end());
      if (!file.declarationFile)
        allDeclaration = false;
      if (merged.module.source == UnknownSource)
        merged.module.source = parsed.tree.module.source;
      for (const auto &name : parsed.tree.module.exports)
        merged.module.exports.insert(name);

      for (auto &statement : parsed.tree.module.declarations) {
        const auto *import = std::get_if<ImportDecl>(&statement->node);
        if (import) {
          if (!aliases.insert(import->alias).second) {
            report("duplicate module alias '" + import->alias + "'", statement->location, "K4003");
            continue;
          }
          const auto dependency =
              module_loading::resolveModulePath(file.path, import->path, options.modulePaths);
          if (!std::filesystem::exists(dependency)) {
            report("cannot resolve module '" + import->path + "'", statement->location, "K4001");
            continue;
          }
          const auto canonicalDependency = module_loading::canonicalize(dependency);
          if (packagePaths.contains(canonicalDependency))
            continue; // sibling file already in this package
          if (!package_loading::isInternalImportAllowed(file.path, canonicalDependency)) {
            report("use of internal package '" + import->path +
                       "' is not allowed from this package",
                   statement->location, "KSEM1042");
            continue;
          }
          dependencies.push_back({import->alias, canonicalDependency, statement->location});
          visit(canonicalDependency, statement->location);
        }
        merged.module.declarations.push_back(std::move(statement));
      }
    }

    std::vector<std::filesystem::path> sourceFiles;
    sourceFiles.reserve(files.size());
    for (const auto &file : files)
      sourceFiles.push_back(file.path);
    ModuleRecord record{std::move(merged), std::move(dependencies), allDeclaration,
                        std::move(sourceFiles)};
    stack.pop_back();
    state[directory] = 2;
    result.graph.modules.insert_or_assign(directory, std::move(record));
    result.graph.initializationOrder.push_back(directory);
  }
};

} // namespace

bool ModuleLoadResult::ok() const {
  return std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const Diagnostic &diagnostic) { return !diagnostic.warning; });
}

ModuleLoadResult loadModuleGraph(SourceManager &sources, const std::filesystem::path &entry,
                                 const ModuleLoadOptions &options) {
  return GraphLoader(sources, options).load(entry);
}

ModuleLoadResult loadModuleGraphWithEntrySource(SourceManager &sources,
                                                const std::filesystem::path &entry,
                                                std::string source,
                                                const ModuleLoadOptions &options) {
  return GraphLoader(sources, options,
                     std::pair<std::filesystem::path, std::string>{entry, std::move(source)})
      .load(entry);
}

} // namespace kyna
