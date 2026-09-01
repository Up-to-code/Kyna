#include "catalog_private.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include "kyna/stdlib/collections_library.hpp"
#include "kyna/stdlib/database_library.hpp"
#include "kyna/stdlib/standard_library_catalog.hpp"

namespace kyna {

void installStandardLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();
  installCollectionsLibrary(interpreter);
  installDatabaseLibrary(interpreter);

  detail::installConsoleLibrary(interpreter);
  detail::installTextLibrary(interpreter);
  detail::installCollectionLibrary(interpreter);
  detail::installFilesystemLibrary(interpreter);
  const auto processHost = detail::installProcessHostLibrary(interpreter);
  const auto formats = detail::installFormatsLibrary(interpreter);
  detail::installNetworkLibrary(interpreter);
  detail::installApiStoreLibrary(interpreter);

  auto process = interpreter.heap().allocate();
  process->fields["json"] = Value(formats.jsonParse);
  process->fields["stringify"] = Value(formats.jsonStringify);
  process->fields["run"] = Value(processHost.processRun);
  process->fields["env"] = Value(processHost.processEnv);
  global->define("process", Value(process), false);
}

} // namespace kyna
