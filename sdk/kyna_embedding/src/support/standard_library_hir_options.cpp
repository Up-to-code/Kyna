#include "../support_private.hpp"

#include "kyna/stdlib/bytecode_standard_library.hpp"
#include "kyna/stdlib/standard_library_catalog.hpp"

namespace kyna::detail {

HirLoweringOptions standardLibraryHirOptions() {
  return {
      bytecodeStandardLibraryFunctionNames(),
      {{"console.log", "log"},
       {"process.json", "jsonParse"},
       {"process.stringify", "jsonStringify"},
       {"process.run", "processRun"},
       {"process.env", "processEnv"},
       {"os.name", "osName"},
       {"os.architecture", "osArchitecture"},
       {"os.cwd", "osWorkingDirectory"},
       {"terminal.interactive", "terminalIsInteractive"},
       {"terminal.supportsColor", "terminalSupportsColor"},
       {"http.fetch", "fetch"},
       {"http.tryFetch", "fetchResult"},
       {"json.parse", "jsonParse"},
       {"json.stringify", "jsonStringify"},
       {"toml.parse", "tomlParse"},
       {"toml.stringify", "tomlStringify"},
       {"xml.parse", "xmlParse"},
       {"xml.stringify", "xmlStringify"},
       {"fs.read", "readFile"},
       {"fs.write", "writeFile"},
       {"fs.readJson", "readJsonFile"},
       {"fs.writeJson", "writeJsonFile"},
       {"fs.createDirectory", "createDirectory"},
       {"fs.exists", "fileExists"},
       {"fs.remove", "removePath"},
       {"fs.list", "listDirectory"},
       {"collections.unique", "unique"},
       {"collections.sort", "sort"}}};
}

} // namespace kyna::detail
