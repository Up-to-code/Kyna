// Owns module checking and the transfer of checked export contracts.
#include <kyna/semantics/module_analyzer.hpp>
#include <kyna/semantics/program_analyzer.hpp>
#include <kyna/semantics/export_cache.hpp>
#include "best_practice_checker.hpp"
#include <algorithm>
#include <set>

namespace kyna {
namespace {
using Bindings = std::map<std::string, TypeRef>;

bool hasErrors(const std::vector<Diagnostic> &diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto &d) { return !d.warning; });
}

TypeRef qualify(TypeRef type, const std::map<std::string, std::string> &names) {
  if (const auto found = names.find(type.name); found != names.end()) type.name = found->second;
  for (auto &argument : type.typeArgs) argument = qualify(argument, names);
  for (auto &arm : type.unionTypes) arm = qualify(arm, names);
  return type;
}

InterfaceDecl qualify(InterfaceDecl type, const std::map<std::string, std::string> &names) {
  if (const auto found = names.find(type.name); found != names.end()) type.name = found->second;
  for (auto &parent : type.parents) parent = qualify(parent, names);
  for (auto &field : type.fields) field.type = qualify(field.type, names);
  for (auto &method : type.methods) {
    method.returnType = qualify(method.returnType, names);
    for (auto &param : method.params) param.type = qualify(param.type, names);
  }
  for (auto &signature : type.callSignatures) {
    signature.returnType = qualify(signature.returnType, names);
    for (auto &param : signature.params) param.type = qualify(param.type, names);
  }
  for (auto &signature : type.indexSignatures) {
    signature.keyType = qualify(signature.keyType, names);
    signature.valueType = qualify(signature.valueType, names);
  }
  return type;
}

std::vector<std::filesystem::path> dependencySources(const ParsedModuleGraph &graph,
                                                    const std::filesystem::path &entry) {
  std::set<std::filesystem::path> visited, sources;
  const auto visit = [&](const auto &self, const std::filesystem::path &path) -> void {
    if (!visited.insert(path).second) return;
    const auto found = graph.modules.find(path);
    if (found == graph.modules.end()) return;
    if (found->second.sourceFiles.empty()) sources.insert(path);
    else sources.insert(found->second.sourceFiles.begin(), found->second.sourceFiles.end());
    for (const auto &dependency : found->second.dependencies) self(self, dependency.canonicalPath);
  };
  visit(visit, entry);
  return {sources.begin(), sources.end()};
}
}

bool AnalysisResult::ok() const { return program.has_value() && !hasErrors(diagnostics); }

AnalysisResult analyzeModuleGraph(ParsedModuleGraph graph, const Bindings &nativeBindings) {
  std::vector<Diagnostic> diagnostics;
  std::vector<std::filesystem::path> cachedModules;
  std::map<std::filesystem::path, Bindings> contracts;
  std::map<std::filesystem::path, std::vector<InterfaceDecl>> interfaceContracts;
  for (const auto &path : graph.initializationOrder) {
    auto &module = graph.modules.at(path);
    Bindings imports = nativeBindings, importedTypes;
    std::map<std::string, Bindings> namespaces;
    std::vector<InterfaceDecl> externalInterfaces;
    std::vector<ClassDecl> externalClasses;
    for (const auto &dependency : module.dependencies) {
      const auto &target = graph.modules.at(dependency.canonicalPath);
      const auto &exports = contracts[dependency.canonicalPath];
      std::map<std::string, std::string> qualifiedNames;
      for (const auto &[name, type] : exports)
        if (type.name.starts_with("type:")) qualifiedNames[name] = dependency.alias + "." + name;
      for (const auto &iface : interfaceContracts[dependency.canonicalPath]) {
        qualifiedNames.try_emplace(iface.name, dependency.alias + "." + iface.name);
        externalInterfaces.push_back(qualify(iface, qualifiedNames));
      }
      for (const auto &[name, type] : exports) {
        namespaces[dependency.alias][name] = qualify(type, qualifiedNames);
        if (type.name.starts_with("type:")) {
          auto ref = type; ref.name.erase(0, 5);
          const auto qualified = qualify(ref, qualifiedNames);
          if (qualified.name != dependency.alias + "." + name)
            importedTypes[dependency.alias + "." + name] = qualified;
        }
      }
      imports[dependency.alias] = TypeRef{dependency.typeOnly ? "type:module" : "module:" + dependency.alias, false, {}, {}};
      for (const auto &statement : module.syntax.module.declarations) {
        const auto *import = std::get_if<ImportDecl>(&statement->node);
        if (!import || import->alias != dependency.alias) continue;
        auto bind = [&](const std::string &exported, const std::string &local) {
          const auto found = exports.find(exported);
          if (found == exports.end()) {
            diagnostics.emplace_back("module '" + import->path + "' has no exported member '" + exported + "'", statement->location, false, "K4004");
            return;
          }
          const bool isType = found->second.name.starts_with("type:");
          if (isType) {
            auto ref = found->second; ref.name.erase(0, 5);
            importedTypes[local] = qualify(ref, qualifiedNames);
            imports[local] = TypeRef{"type:" + importedTypes[local].name, false, importedTypes[local].typeArgs, importedTypes[local].unionTypes};
          } else if (import->typeOnly) {
            diagnostics.emplace_back("'" + exported + "' is a value export, not a type", statement->location, false, "KSEM1504");
          } else imports[local] = qualify(found->second, qualifiedNames);
          for (const auto &declaration : target.syntax.module.declarations)
            if (const auto *klass = std::get_if<ClassDecl>(&declaration->node); klass && klass->name == exported) {
              auto copy = *klass; copy.name = local; externalClasses.push_back(std::move(copy));
              imports[local] = TypeRef{"class:" + local, false, {}, {}};
            }
        };
        for (const auto &specifier : import->named) bind(specifier.imported, specifier.local);
        if (!import->defaultName.empty()) {
          bool found = false;
          for (const auto &declaration : target.syntax.module.declarations)
            std::visit([&](const auto &node) {
              using T = std::decay_t<decltype(node)>;
              if constexpr (std::is_same_v<T, VarDecl> || std::is_same_v<T, FunctionDecl> || std::is_same_v<T, ClassDecl> || std::is_same_v<T, InterfaceDecl>)
                if (node.isDefault) { bind(node.name, import->defaultName); found = true; }
            }, declaration->node);
          if (!found) diagnostics.emplace_back("module '" + import->path + "' has no default export", statement->location, false, "K4004");
        }
        if (!import->namespaceAlias.empty()) imports[import->namespaceAlias] = imports[dependency.alias];
      }
    }
    for (const auto &statement : module.syntax.module.declarations) {
      if (const auto *iface = std::get_if<InterfaceDecl>(&statement->node);
          iface && (iface->exported || module.isDeclaration || module.syntax.module.exports.contains(iface->name)))
        interfaceContracts[path].push_back(*iface);
    }
    // Re-exported contracts retain their referenced interface definitions.
    interfaceContracts[path].insert(interfaceContracts[path].end(), externalInterfaces.begin(), externalInterfaces.end());
    const auto sources = dependencySources(graph, path);
    const auto cache = export_cache::cachePath(path);
    auto cached = !graph.hasSourceOverlays && nativeBindings.empty() && path != graph.entry && export_cache::stampMatches(sources, cache)
        ? export_cache::readBindings(cache) : std::nullopt;
    Bindings checked;
    if (cached) {
      checked = std::move(*cached);
      cachedModules.push_back(path);
    } else {
      Analyzer analyzer;
      analyzer.setExternalBindings(std::move(imports));
      analyzer.setExternalTypes(std::move(importedTypes));
      analyzer.setModuleExports(std::move(namespaces));
      analyzer.setExternalInterfaces(std::move(externalInterfaces));
      analyzer.setExternalClasses(std::move(externalClasses));
      auto errors = analyzer.analyze(module.syntax.module.declarations);
      auto practices = checkBestPractices(module.syntax.module.declarations);
      // Warnings must remain observable on every check, so warning-bearing modules aren't cached.
      checked = analyzer.checkedBindings();
      if (!graph.hasSourceOverlays) {
        if (errors.empty() && practices.empty() && nativeBindings.empty() && path != graph.entry)
          export_cache::writeBindings(sources, cache, checked);
        else export_cache::invalidate(cache);
      }
      diagnostics.insert(diagnostics.end(), errors.begin(), errors.end());
      diagnostics.insert(diagnostics.end(), practices.begin(), practices.end());
    }
    for (const auto &name : module.syntax.module.exports) {
      if (const auto found = checked.find(name); found != checked.end()) contracts[path][name] = found->second;
      else {
        bool type = false;
        for (const auto &statement : module.syntax.module.declarations) {
          if (const auto *iface = std::get_if<InterfaceDecl>(&statement->node); iface && iface->name == name) {
            contracts[path][name] = TypeRef{"type:" + name, false, {}, {}}; type = true;
          }
          if (const auto *alias = std::get_if<TypeAliasDecl>(&statement->node); alias && alias->name == name) {
            auto target = alias->target; target.name = "type:" + target.name;
            contracts[path][name] = std::move(target); type = true;
          }
          if (const auto *klass = std::get_if<ClassDecl>(&statement->node); klass && klass->name == name) {
            contracts[path][name] = TypeRef{"class:" + name, false, {}, {}}; type = true;
          }
        }
        if (!type) diagnostics.emplace_back("cannot export undefined name '" + name + "'", SourceSpan{}, false, "K4004");
      }
    }
    for (const auto &statement : module.syntax.module.declarations)
      if (const auto *exports = std::get_if<ExportDecl>(&statement->node); exports && exports->typeOnly)
        for (const auto &name : exports->names)
          if (const auto found = contracts[path].find(name);
              found != contracts[path].end() && !found->second.name.starts_with("type:"))
            diagnostics.emplace_back("'" + name + "' is a value, not a type export", statement->location,
                                     false, "KSEM1504");
    if (module.isDeclaration)
      for (const auto &iface : interfaceContracts[path]) contracts[path][iface.name] = TypeRef{"type:" + iface.name, false, {}, {}};
  }
  if (hasErrors(diagnostics)) return {std::nullopt, std::move(diagnostics), std::move(cachedModules)};
  return {CheckedProgram{std::move(graph)}, std::move(diagnostics), std::move(cachedModules)};
}
} // namespace kyna
