#pragma once

#include "kyna/execution/runtime_object_model.hpp"
#include "kyna/execution/tree_walk_engine.hpp"

namespace kyna::detail {

// Tree-walk standard-library native installation, split by concern.
void installConsoleLibrary(Interpreter &interpreter);
void installTextLibrary(Interpreter &interpreter);
void installCollectionLibrary(Interpreter &interpreter);
void installFilesystemLibrary(Interpreter &interpreter);

// Process/host natives plus the os/terminal objects built from them.
struct ProcessHostNatives {
  FunctionPtr processRun;
  FunctionPtr processEnv;
};
ProcessHostNatives installProcessHostLibrary(Interpreter &interpreter);

// json/toml/xml natives plus their namespaced objects.
struct FormatNatives {
  FunctionPtr jsonParse;
  FunctionPtr jsonStringify;
};
FormatNatives installFormatsLibrary(Interpreter &interpreter);

void installNetworkLibrary(Interpreter &interpreter);
void installApiStoreLibrary(Interpreter &interpreter);

} // namespace kyna::detail
