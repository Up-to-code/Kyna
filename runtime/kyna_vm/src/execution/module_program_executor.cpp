#include "kyna/execution/tree_walk_interpreter.hpp"
#include <algorithm>

namespace kyna {

ExecutionResult TreeWalkInterpreter::execute(const CheckedProgram &program) {
  try {
    Value last;
    for (const auto &path : program.modules.initializationOrder) {
      if (initializedModules.contains(path))
        continue;
      const auto found = program.modules.modules.find(path);
      if (found == program.modules.modules.end())
        continue;
      auto environment = interpreter.createModuleEnvironment();
      for (const auto &dependency : found->second.dependencies) {
        const auto imported = initializedModules.find(dependency.canonicalPath);
        if (imported == initializedModules.end()) {
          Diagnostic diagnostic{"module dependency was not initialized", dependency.location,
                                false};
          diagnostic.code = "K5001";
          return {{}, {std::move(diagnostic)}};
        }
        const auto namespaceValue = Value(imported->second);
        bool bound = false;
        // JavaScript-style imports bind each imported name to the module
        // namespace; the canonical alias is one of those names.
        for (const auto &statement : found->second.syntax.module.declarations) {
          const auto *import = std::get_if<ImportDecl>(&statement->node);
          if (!import || import->alias != dependency.alias)
            continue;
          for (const auto &specifier : import->named) {
            // Bind the specific exported value (class, function, or value) so
            // `new User(...)`, `add(...)`, and member access work directly.
            environment->define(
                specifier.local,
                imported->second->environment->get(specifier.imported).value, false);
            bound = true;
          }
          if (!import->defaultName.empty()) {
            // Bind the actual default-exported value (class, function, or value)
            // so `new Name(...)` and `Name.member` work directly across modules.
            // Fall back to the namespace when the default export is not yet known.
            const bool hasDefault = !imported->second->defaultExport.empty();
            environment->define(
                import->defaultName,
                hasDefault
                    ? imported->second->environment
                          ->get(imported->second->defaultExport)
                          .value
                    : namespaceValue,
                false);
            bound = true;
          }
          if (!import->namespaceAlias.empty()) {
            environment->define(import->namespaceAlias, namespaceValue, false);
            bound = true;
          }
        }
        if (!bound)
          environment->define(dependency.alias, namespaceValue, false);
      }
      last = interpreter.executeIn(found->second.syntax.module.declarations, environment);
      auto module = std::make_shared<ModuleNamespace>();
      module->environment = std::move(environment);
      module->exports = found->second.syntax.module.exports;
      module->displayName = path.filename().string();
      // Record the canonical name of the single default-exported entity so a
      // consumer's `import Alias from "..."` can bind to the real value.
      for (const auto &statement : found->second.syntax.module.declarations) {
        const std::string *name = nullptr;
        if (const auto *var = std::get_if<VarDecl>(&statement->node); var && var->isDefault)
          name = &var->name;
        else if (const auto *fn = std::get_if<FunctionDecl>(&statement->node);
                 fn && fn->isDefault)
          name = &fn->name;
        else if (const auto *klass = std::get_if<ClassDecl>(&statement->node);
                 klass && klass->isDefault)
          name = &klass->name;
        if (name) {
          module->defaultExport = *name;
          break;
        }
      }
      initializedModules.insert_or_assign(path, std::move(module));
    }
    return {std::move(last), {}};
  } catch (const RuntimeThrownError &error) {
    Diagnostic diagnostic{error.value ? error.value->message : std::string("uncaught Error"),
                          {}, false,
                          error.value && !error.value->code.empty() ? error.value->code
                                                                   : "KRT2301"};
    diagnostic.category = "runtime";
    diagnostic.callFrames = error.frames;
    if (error.value && !std::holds_alternative<std::nullptr_t>(error.value->cause.data))
      diagnostic.notes.push_back("cause: " + error.value->cause.display());
    diagnostic.help = "catch this Error or let the enclosing function propagate it";
    return {{}, {std::move(diagnostic)}};
  } catch (const KynaError &error) {
    auto diagnostic = error.diagnostic;
    if (diagnostic.code == "K0000")
      diagnostic.code = "K5000";
    return {{}, {std::move(diagnostic)}};
  } catch (const std::exception &error) {
    Diagnostic diagnostic{std::string("uncaught native runtime exception: ") + error.what(), {},
                          false};
    diagnostic.code = "KVM9001";
    diagnostic.notes.push_back(
        "this exception crossed a native adapter without a Kyna source diagnostic");
    diagnostic.help = "rerun with --trace and report this as a Kyna runtime defect";
    return {{}, {std::move(diagnostic)}};
  }
}

} // namespace kyna
