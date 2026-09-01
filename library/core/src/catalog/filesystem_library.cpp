#include "catalog_private.hpp"
#include "../codecs/json/json_value_codec.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include <string>

namespace kyna::detail {

void installFilesystemLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();
  auto capabilities = interpreter.runtimeCapabilities();

  auto read = std::make_shared<Function>();
  read->native = true;
  read->nativeCall = [capabilities](const std::vector<Value> &a) {
    if (a.size() != 1 || !std::holds_alternative<std::string>(a[0].data))
      throw KynaError({"readFile expects a path", {1, 1}, false});
    std::string error;
    auto contents = capabilities.files->read(std::get<std::string>(a[0].data), error);
    if (!contents)
      throw KynaError({std::move(error), {1, 1}, false});
    return Value(std::move(*contents));
  };
  global->define("readFile", Value(read), false);
  auto write = std::make_shared<Function>();
  write->native = true;
  write->nativeCall = [capabilities](const std::vector<Value> &a) {
    if (a.size() != 2 || !std::holds_alternative<std::string>(a[0].data) ||
        !std::holds_alternative<std::string>(a[1].data))
      throw KynaError({"writeFile expects path and string content", {1, 1}, false});
    std::string error;
    if (!capabilities.files->write(std::get<std::string>(a[0].data),
                                   std::get<std::string>(a[1].data), error))
      throw KynaError({std::move(error), {1, 1}, false});
    return Value();
  };
  global->define("writeFile", Value(write), false);

  auto createDirectory = std::make_shared<Function>();
  createDirectory->native = true;
  createDirectory->nativeCall = [capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"createDirectory expects one path", {1, 1}, false, "K5200"});
    std::string error;
    if (!capabilities.files->createDirectories(std::get<std::string>(arguments[0].data), error))
      throw KynaError({std::move(error), {1, 1}, false, "K5200"});
    return Value(true);
  };
  global->define("createDirectory", Value(createDirectory), false);

  auto fileExists = std::make_shared<Function>();
  fileExists->native = true;
  fileExists->nativeCall = [capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"fileExists expects one path", {1, 1}, false, "K5201"});
    std::string error;
    const bool found = capabilities.files->exists(std::get<std::string>(arguments[0].data), error);
    if (!error.empty())
      throw KynaError({std::move(error), {1, 1}, false, "K5201"});
    return Value(found);
  };
  global->define("fileExists", Value(fileExists), false);

  auto removePath = std::make_shared<Function>();
  removePath->native = true;
  removePath->nativeCall = [capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError(
          {"removePath expects one file or empty-directory path", {1, 1}, false, "K5202"});
    std::string error;
    const bool removed =
        capabilities.files->remove(std::get<std::string>(arguments[0].data), error);
    if (!error.empty())
      throw KynaError({std::move(error), {1, 1}, false, "K5202"});
    return Value(removed);
  };
  global->define("removePath", Value(removePath), false);

  auto listDirectory = std::make_shared<Function>();
  listDirectory->native = true;
  listDirectory->nativeCall = [&interpreter, capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"listDirectory expects one directory path", {1, 1}, false, "K5203"});
    std::string error;
    auto names = capabilities.files->list(std::get<std::string>(arguments[0].data), error);
    if (!names)
      throw KynaError({std::move(error), {1, 1}, false, "K5203"});
    auto result = interpreter.heap().allocateArray();
    for (auto &name : *names)
      result->elements.emplace_back(std::move(name));
    return Value(result);
  };
  global->define("listDirectory", Value(listDirectory), false);

  auto readJsonFile = std::make_shared<Function>();
  readJsonFile->native = true;
  readJsonFile->nativeCall = [&interpreter, capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"readJsonFile expects one path", {1, 1}, false, "K5204"});
    std::string error;
    auto contents = capabilities.files->read(std::get<std::string>(arguments[0].data), error);
    if (!contents)
      throw KynaError({std::move(error), {1, 1}, false, "K5204"});
    return parseJsonValue(*contents, interpreter);
  };
  global->define("readJsonFile", Value(readJsonFile), false);

  auto writeJsonFile = std::make_shared<Function>();
  writeJsonFile->native = true;
  writeJsonFile->nativeCall = [capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"writeJsonFile expects a path and value", {1, 1}, false, "K5205"});
    std::string error;
    if (!capabilities.files->write(std::get<std::string>(arguments[0].data),
                                   stringifyJsonValue(arguments[1]), error))
      throw KynaError({std::move(error), {1, 1}, false, "K5205"});
    return Value(true);
  };
  global->define("writeJsonFile", Value(writeJsonFile), false);

  auto fileSystem = interpreter.heap().allocate();
  fileSystem->fields["read"] = Value(read);
  fileSystem->fields["write"] = Value(write);
  fileSystem->fields["readJson"] = Value(readJsonFile);
  fileSystem->fields["writeJson"] = Value(writeJsonFile);
  fileSystem->fields["createDirectory"] = Value(createDirectory);
  fileSystem->fields["exists"] = Value(fileExists);
  fileSystem->fields["remove"] = Value(removePath);
  fileSystem->fields["list"] = Value(listDirectory);
  global->define("fs", Value(fileSystem), false);
}

} // namespace kyna::detail
