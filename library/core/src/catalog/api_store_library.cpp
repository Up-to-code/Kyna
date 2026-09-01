#include "catalog_private.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include <algorithm>

namespace kyna::detail {

void installApiStoreLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();

  auto createApiStore = std::make_shared<Function>();
  createApiStore->native = true;
  createApiStore->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      throw KynaError({"createApiStore expects an initial record array", {1, 1}, false});
    auto store = interpreter.heap().allocate();
    store->fields["records"] = arguments[0];
    const auto records = std::get<ArrayPtr>(arguments[0].data);

    auto list = std::make_shared<Function>();
    list->native = true;
    list->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (!methodArguments.empty())
        throw KynaError({"store.list expects no arguments", {1, 1}, false});
      return Value(records);
    };
    store->fields["list"] = Value(list);

    auto getRecord = std::make_shared<Function>();
    getRecord->native = true;
    getRecord->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (methodArguments.size() != 1)
        throw KynaError({"store.get expects an id", {1, 1}, false});
      for (const auto &record : records->elements)
        if (const auto object = std::get_if<ObjectPtr>(&record.data); object && *object) {
          const auto id = (*object)->fields.find("id");
          if (id != (*object)->fields.end() && id->second.equals(methodArguments[0]))
            return record;
        }
      return Value();
    };
    store->fields["get"] = Value(getRecord);

    auto createRecord = std::make_shared<Function>();
    createRecord->native = true;
    createRecord->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (methodArguments.size() != 1 ||
          !std::holds_alternative<ObjectPtr>(methodArguments[0].data))
        throw KynaError({"store.create expects an object record", {1, 1}, false});
      records->elements.push_back(methodArguments[0]);
      return methodArguments[0];
    };
    store->fields["create"] = Value(createRecord);

    auto updateRecord = std::make_shared<Function>();
    updateRecord->native = true;
    updateRecord->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (methodArguments.size() != 2 ||
          !std::holds_alternative<ObjectPtr>(methodArguments[1].data))
        throw KynaError({"store.update expects an id and patch object", {1, 1}, false});
      for (auto &record : records->elements)
        if (const auto object = std::get_if<ObjectPtr>(&record.data); object && *object) {
          const auto id = (*object)->fields.find("id");
          if (id == (*object)->fields.end() || !id->second.equals(methodArguments[0]))
            continue;
          for (const auto &[name, value] : std::get<ObjectPtr>(methodArguments[1].data)->fields)
            (*object)->fields.insert_or_assign(name, value);
          return record;
        }
      return Value();
    };
    store->fields["update"] = Value(updateRecord);

    auto removeRecord = std::make_shared<Function>();
    removeRecord->native = true;
    removeRecord->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (methodArguments.size() != 1)
        throw KynaError({"store.remove expects an id", {1, 1}, false});
      const auto before = records->elements.size();
      std::erase_if(records->elements, [&](const Value &record) {
        const auto object = std::get_if<ObjectPtr>(&record.data);
        if (!object || !*object)
          return false;
        const auto id = (*object)->fields.find("id");
        return id != (*object)->fields.end() && id->second.equals(methodArguments[0]);
      });
      return Value(records->elements.size() != before);
    };
    store->fields["remove"] = Value(removeRecord);
    return Value(store);
  };
  global->define("createApiStore", Value(createApiStore), false);
}

} // namespace kyna::detail
