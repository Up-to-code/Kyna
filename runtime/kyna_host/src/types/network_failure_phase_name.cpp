#include "kyna/execution/runtime_capabilities.hpp"

namespace kyna {

const char *networkFailurePhaseName(NetworkFailurePhase phase) {
  switch (phase) {
  case NetworkFailurePhase::Dns: return "DNS resolution";
  case NetworkFailurePhase::Connect: return "connection";
  case NetworkFailurePhase::Tls: return "TLS handshake";
  case NetworkFailurePhase::Send: return "request send";
  case NetworkFailurePhase::Receive: return "response receive";
  case NetworkFailurePhase::Timeout: return "timeout";
  case NetworkFailurePhase::Http: return "HTTP response";
  case NetworkFailurePhase::Transfer: return "transfer";
  }
  return "transfer";
}

} // namespace kyna
