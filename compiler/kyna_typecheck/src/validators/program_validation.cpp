#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/semantics/program_validation.hpp"

namespace kyna {
std::vector<Diagnostic> validate(const std::vector<StmtPtr> &program) {
  return Analyzer().analyze(program);
}
} // namespace kyna
