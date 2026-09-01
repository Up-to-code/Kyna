#include <charconv>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <variant>
#include <vector>

#include "kyna/execution/database_port.hpp"
#include "../host_private.hpp"

#if defined(KYNA_HAS_POSTGRESQL)
#include <libpq-fe.h>
#endif

#if defined(KYNA_HAS_POSTGRESQL)
namespace kyna::detail {
namespace {

struct ConnectionDeleter {
  void operator()(PGconn *connection) const { PQfinish(connection); }
};

struct ResultDeleter {
  void operator()(PGresult *result) const { PQclear(result); }
};

using Connection = std::unique_ptr<PGconn, ConnectionDeleter>;
using Result = std::unique_ptr<PGresult, ResultDeleter>;

bool retryableSqlState(std::string_view code) {
  return code.starts_with("08") || code == "40001" || code == "40P01" || code == "53300" ||
         code == "57P01";
}

std::string resultError(PGconn *connection, PGresult *result) {
  if (result) {
    if (const auto *message = PQresultErrorMessage(result); message && *message)
      return message;
  }
  const auto *message = PQerrorMessage(connection);
  return message ? message : "PostgreSQL operation failed without a native message";
}

DatabaseScalar decodeValue(PGresult *result, int row, int column) {
  if (PQgetisnull(result, row, column))
    return nullptr;
  const std::string_view text(PQgetvalue(result, row, column),
                              static_cast<std::size_t>(PQgetlength(result, row, column)));
  switch (PQftype(result, column)) {
  case 16:
    return text == "t";
  case 20:
  case 21:
  case 23: {
    std::int64_t value{};
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), value);
    if (converted.ec == std::errc{} && converted.ptr == text.data() + text.size())
      return value;
    break;
  }
  case 700:
  case 701: {
    std::string owned(text);
    char *end = nullptr;
    const auto value = std::strtod(owned.c_str(), &end);
    if (end == owned.c_str() + owned.size())
      return value;
    break;
  }
  default:
    break;
  }
  return std::string(text);
}

} // namespace

class PostgresDatabase final : public DatabasePort {
public:
  std::optional<DatabaseResult> execute(const DatabaseRequest &request,
                                        DatabaseFailure &failure) override {
    if (request.connectionString.empty() || request.statement.empty()) {
      failure = {DatabaseFailurePhase::Configuration, "KDB-INVALID-REQUEST",
                 "database connection string and SQL statement must not be empty", false};
      return std::nullopt;
    }

    Connection connection(PQconnectdb(request.connectionString.c_str()));
    if (!connection || PQstatus(connection.get()) != CONNECTION_OK) {
      failure = {DatabaseFailurePhase::Connect, "08000",
                 connection ? PQerrorMessage(connection.get()) : "libpq could not allocate a connection",
                 true};
      return std::nullopt;
    }

    std::vector<std::string> encoded;
    encoded.reserve(request.parameters.size());
    std::vector<const char *> values;
    values.reserve(request.parameters.size());
    for (const auto &parameter : request.parameters) {
      if (std::holds_alternative<std::nullptr_t>(parameter)) {
        encoded.emplace_back();
        values.push_back(nullptr);
      } else {
        encoded.push_back(std::visit(
            [](const auto &value) -> std::string {
              using T = std::decay_t<decltype(value)>;
              if constexpr (std::is_same_v<T, std::nullptr_t>)
                return {};
              else if constexpr (std::is_same_v<T, bool>)
                return value ? "true" : "false";
              else if constexpr (std::is_same_v<T, std::string>)
                return value;
              else
                return std::to_string(value);
            },
            parameter));
        values.push_back(encoded.back().c_str());
      }
    }

    Result result(PQexecParams(connection.get(), request.statement.c_str(),
                               static_cast<int>(values.size()), nullptr, values.data(), nullptr,
                               nullptr, 0));
    const auto status = result ? PQresultStatus(result.get()) : PGRES_FATAL_ERROR;
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
      const auto *state = result ? PQresultErrorField(result.get(), PG_DIAG_SQLSTATE) : nullptr;
      const std::string code = state ? state : "KDB-NATIVE";
      failure = {DatabaseFailurePhase::Execute, code,
                 resultError(connection.get(), result.get()), retryableSqlState(code)};
      return std::nullopt;
    }

    DatabaseResult decoded;
    if (const auto *tag = PQcmdStatus(result.get()); tag)
      decoded.command = tag;
    if (const auto *affected = PQcmdTuples(result.get()); affected && *affected) {
      std::uint64_t count{};
      const std::string_view text(affected);
      const auto conversion = std::from_chars(text.data(), text.data() + text.size(), count);
      if (conversion.ec == std::errc{})
        decoded.affectedRows = count;
    }
    for (int row = 0; row < PQntuples(result.get()); ++row) {
      DatabaseRow decodedRow;
      for (int column = 0; column < PQnfields(result.get()); ++column)
        decodedRow.insert_or_assign(PQfname(result.get(), column),
                                    decodeValue(result.get(), row, column));
      decoded.rows.push_back(std::move(decodedRow));
    }
    return decoded;
  }
};

std::shared_ptr<DatabasePort> makePostgresDatabase() {
  return std::make_shared<PostgresDatabase>();
}

} // namespace kyna::detail
#endif
