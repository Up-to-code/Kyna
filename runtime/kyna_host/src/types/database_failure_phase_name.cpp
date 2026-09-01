#include "kyna/execution/database_port.hpp"

namespace kyna {

const char *databaseFailurePhaseName(DatabaseFailurePhase phase) {
  switch (phase) {
  case DatabaseFailurePhase::Configuration: return "configuration";
  case DatabaseFailurePhase::Connect: return "connection";
  case DatabaseFailurePhase::Prepare: return "statement preparation";
  case DatabaseFailurePhase::Execute: return "statement execution";
  case DatabaseFailurePhase::Decode: return "result decoding";
  }
  return "database";
}

} // namespace kyna
