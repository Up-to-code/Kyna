#pragma once

#include "kyna/diagnostics/diagnostic.hpp"
#include "kyna/syntax/declaration_nodes.hpp"
#include <vector>

namespace kyna {

std::vector<Diagnostic> checkBestPractices(const std::vector<StmtPtr> &declarations);

} // namespace kyna
