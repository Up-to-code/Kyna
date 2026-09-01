#include "kyna/stdlib/database_library.hpp"

#include <memory>
#include <string>
#include <vector>

#include "kyna/execution/database_port.hpp"
#include "kyna/execution/runtime_capabilities.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include "../database_private.hpp"

namespace kyna {
namespace {

FunctionPtr databaseQueryNative(Interpreter &interpreter) {
  const auto capabilities = interpreter.runtimeCapabilities();
  auto databaseQuery = std::make_shared<Function>();
  databaseQuery->native = true;
  databaseQuery->nativeCall = [&interpreter, capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() < 2 || arguments.size() > 3 ||
        !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data) ||
        (arguments.size() == 3 && !std::holds_alternative<ArrayPtr>(arguments[2].data)))
      throw KynaError(
          {"db.query expects a connection string, SQL string, and optional parameter array",
           {}, false, "KDB1001"});
    if (!capabilities.database)
      throw KynaError({"database capability is not available in this runtime", {}, false,
                       "KDB1000"});

    DatabaseRequest request;
    request.connectionString = std::get<std::string>(arguments[0].data);
    request.statement = std::get<std::string>(arguments[1].data);
    if (arguments.size() == 3)
      for (const auto &parameter : std::get<ArrayPtr>(arguments[2].data)->elements)
        request.parameters.push_back(detail::databaseScalar(parameter));

    DatabaseFailure failure;
    const auto databaseResult = capabilities.database->execute(request, failure);
    if (!databaseResult)
      throw detail::databaseError(failure);

    auto rows = interpreter.heap().allocateArray();
    for (const auto &databaseRow : databaseResult->rows) {
      auto row = interpreter.heap().allocate();
      for (const auto &[column, value] : databaseRow)
        row->fields.insert_or_assign(column, detail::runtimeScalar(value));
      rows->elements.emplace_back(row);
    }
    auto result = interpreter.heap().allocate();
    result->fields["rows"] = Value(rows);
    result->fields["affectedRows"] =
        Value(static_cast<std::int64_t>(databaseResult->affectedRows));
    result->fields["command"] = Value(databaseResult->command);
    return Value(result);
  };
  return databaseQuery;
}

} // namespace

void installDatabaseLibrary(Interpreter &interpreter) {
  auto databaseQuery = databaseQueryNative(interpreter);

  auto database = interpreter.heap().allocate();
  database->fields["query"] = Value(databaseQuery);
  database->fields["execute"] = Value(databaseQuery);
  interpreter.globals()->define("db", Value(database), false);
}

} // namespace kyna
