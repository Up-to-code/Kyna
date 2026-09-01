#include "../database_private.hpp"

namespace kyna::detail {

KynaError databaseError(const DatabaseFailure &failure) {
  Diagnostic diagnostic{"database " + std::string(databaseFailurePhaseName(failure.phase)) +
                            " error: " + failure.message,
                        {}, false, "KDB2001"};
  diagnostic.category = "database";
  diagnostic.causes.push_back({"postgresql", failure.nativeCode, failure.message});
  diagnostic.notes.push_back(failure.retryable
                                 ? "the database adapter classified this failure as retryable"
                                 : "the database adapter classified this failure as non-retryable");
  diagnostic.help =
      "verify the connection settings and SQL; use parameter placeholders such as $1 instead of concatenating values";
  return KynaError(diagnostic);
}

} // namespace kyna::detail
