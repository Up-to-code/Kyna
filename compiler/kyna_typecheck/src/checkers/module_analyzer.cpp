#include "kyna/semantics/module_analyzer.hpp"
#include "best_practice_checker.hpp"
#include "kyna/semantics/export_cache.hpp"
#include "kyna/semantics/program_analyzer.hpp"
#include <algorithm>
#include <optional>

namespace kyna {
namespace {

std::map<std::string, TypeRef> exportedTypes(const ModuleRecord &module) {
  std::map<std::string, TypeRef> exports;
  for (const auto &statement : module.syntax.module.declarations) {
    std::visit(
        [&](const auto &declaration) {
          using T = std::decay_t<decltype(declaration)>;
          if constexpr (std::is_same_v<T, VarDecl>) {
            if (declaration.exported)
              exports[declaration.name] =
                  declaration.hasType ? declaration.type : TypeRef{"any", false, {}};
          } else if constexpr (std::is_same_v<T, FunctionDecl>) {
            if (declaration.exported)
              exports[declaration.name] =
                  declaration.hasReturnType ? declaration.returnType : TypeRef{"any", false, {}};
          } else if constexpr (std::is_same_v<T, ClassDecl>) {
            if (declaration.exported)
              exports[declaration.name] = TypeRef{"class:" + declaration.name, false, {}};
          } else if constexpr (std::is_same_v<T, InterfaceDecl>) {
            if (declaration.exported)
              exports[declaration.name] = TypeRef{declaration.name, false, {}};
          }
        },
        statement->node);
  }
  return exports;
}

} // namespace

// Gather ambient interface declarations from a type-definition module. All
// top-level interfaces in an ambient file are globally visible to importers,
// mirroring how `.d.ts`/`.d.ky` declaration files provide ambient contracts.
std::vector<InterfaceDecl> ambientInterfaces(const ModuleRecord &module) {
  std::vector<InterfaceDecl> result;
  if (!module.isDeclaration)
    return result;
  result.reserve(module.syntax.module.declarations.size());
  for (const auto &statement : module.syntax.module.declarations)
    if (const auto *iface = std::get_if<InterfaceDecl>(&statement->node))
      result.push_back(*iface);
  return result;
}

// Gather exported classes from an imported module so implementations can
// construct and type-check imported classes (`new User`, `item: User`).
std::vector<ClassDecl> exportedClasses(const ModuleRecord &module) {
  std::vector<ClassDecl> result;
  for (const auto &statement : module.syntax.module.declarations)
    if (const auto *klass = std::get_if<ClassDecl>(&statement->node); klass && klass->exported)
      result.push_back(*klass);
  return result;
}

// The single default-exported class of a module, if any. JavaScript-style
// `import Name from "..."` of a class must resolve `Name` as a type so `new
// Name(...)` and `Name.member` work across modules, exactly like named-export
// classes. The local alias may differ from the canonical class name, so the
// importer registers a renamed copy under the alias.
std::optional<ClassDecl> defaultClass(const ModuleRecord &module) {
  for (const auto &statement : module.syntax.module.declarations)
    if (const auto *klass = std::get_if<ClassDecl>(&statement->node);
        klass && klass->isDefault)
      return *klass;
  return std::nullopt;
}

bool AnalysisResult::ok() const {
  return program.has_value() &&
         std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const Diagnostic &diagnostic) { return !diagnostic.warning; });
}

AnalysisResult analyzeModuleGraph(ParsedModuleGraph graph) {
  std::vector<Diagnostic> diagnostics;
  std::vector<std::filesystem::path> cachedModules;
  for (const auto &path : graph.initializationOrder) {
    auto found = graph.modules.find(path);
    if (found == graph.modules.end())
      continue;
    std::map<std::string, TypeRef> imports;
    std::map<std::string, std::map<std::string, TypeRef>> moduleExports;
    std::map<std::string, const ModuleRecord *> dependencyModules;
    std::vector<InterfaceDecl> externalInterfaces;
    std::vector<ClassDecl> externalClasses;
    for (const auto &dependency : found->second.dependencies) {
      imports[dependency.alias] = TypeRef{"module:" + dependency.alias, false, {}};
      if (const auto module = graph.modules.find(dependency.canonicalPath);
          module != graph.modules.end()) {
        dependencyModules[dependency.alias] = &module->second;
        moduleExports[dependency.alias] = exportedTypes(module->second);
        auto ambient = ambientInterfaces(module->second);
        externalInterfaces.insert(externalInterfaces.end(), ambient.begin(), ambient.end());
        auto classes = exportedClasses(module->second);
        externalClasses.insert(externalClasses.end(), classes.begin(), classes.end());
      }
    }
    // JavaScript-style imports bind each imported name to the module's
    // namespace so the checker can resolve calls and member access.
    for (const auto &statement : found->second.syntax.module.declarations) {
      const auto *importDecl = std::get_if<ImportDecl>(&statement->node);
      if (!importDecl)
        continue;
      for (const auto &specifier : importDecl->named)
        imports[specifier.local] = TypeRef{"module:" + importDecl->alias, false, {}};
      if (!importDecl->defaultName.empty())
        imports[importDecl->defaultName] = TypeRef{"module:" + importDecl->alias, false, {}};
      if (!importDecl->namespaceAlias.empty())
        imports[importDecl->namespaceAlias] =
            TypeRef{"module:" + importDecl->alias, false, {}};
      // A default import of a class resolves the alias as a type as well as a
      // value, so `new Alias(...)` and `Alias.member` work across modules. The
      // canonical class name may differ from the local alias, so register a
      // renamed copy under the alias.
      if (!importDecl->defaultName.empty()) {
        const auto dep = dependencyModules.find(importDecl->alias);
        if (dep != dependencyModules.end())
          if (auto klass = defaultClass(*dep->second)) {
            klass->name = importDecl->defaultName;
            externalClasses.push_back(std::move(*klass));
          }
      }
      // Validate JavaScript-style named imports: the imported symbol must be
      // exported by the target module. A missing or non-exported name is a
      // compile error, matching module semantics.
      const auto exportMap = moduleExports.find(importDecl->alias);
      for (const auto &specifier : importDecl->named) {
        if (exportMap == moduleExports.end() ||
            !exportMap->second.contains(specifier.imported)) {
          Diagnostic diagnostic{"module '" + importDecl->path +
                                    "' has no exported member '" + specifier.imported + "'",
                                statement->location, false};
          diagnostic.code = "K4004";
          diagnostics.push_back(std::move(diagnostic));
        }
      }
    }
    const bool isEntry = path == graph.entry;
    auto sources = found->second.sourceFiles;
    if (sources.empty())
      sources.push_back(path);
    const auto cacheFile = export_cache::cachePath(path);
    if (!isEntry && export_cache::stampMatches(sources, cacheFile)) {
      cachedModules.push_back(path);
      continue;
    }
    Analyzer analyzer;
    analyzer.setExternalBindings(std::move(imports));
    analyzer.setModuleExports(std::move(moduleExports));
    analyzer.setExternalInterfaces(std::move(externalInterfaces));
    analyzer.setExternalClasses(std::move(externalClasses));
    auto moduleDiagnostics = analyzer.analyze(found->second.syntax.module.declarations);
    diagnostics.insert(diagnostics.end(), moduleDiagnostics.begin(), moduleDiagnostics.end());
    auto practiceDiagnostics = checkBestPractices(found->second.syntax.module.declarations);
    diagnostics.insert(diagnostics.end(), practiceDiagnostics.begin(), practiceDiagnostics.end());
    const bool moduleFailed =
        std::any_of(moduleDiagnostics.begin(), moduleDiagnostics.end(),
                    [](const Diagnostic &diagnostic) { return !diagnostic.warning; }) ||
        std::any_of(practiceDiagnostics.begin(), practiceDiagnostics.end(),
                    [](const Diagnostic &diagnostic) { return !diagnostic.warning; });
    if (moduleFailed)
      export_cache::invalidate(cacheFile);
    else if (!isEntry)
      export_cache::writeStamp(sources, cacheFile);
  }
  const bool failed = std::any_of(diagnostics.begin(), diagnostics.end(),
                                  [](const Diagnostic &diagnostic) { return !diagnostic.warning; });
  if (failed)
    return {std::nullopt, std::move(diagnostics), std::move(cachedModules)};
  return {CheckedProgram{std::move(graph)}, std::move(diagnostics), std::move(cachedModules)};
}

} // namespace kyna
