#include "kyna/language/language_session.hpp"
#include "../support_private.hpp"

namespace kyna {

bool LanguageResult::ok() const { return !detail::hasErrors(diagnostics); }
bool InspectionResult::ok() const { return !detail::hasErrors(diagnostics); }

} // namespace kyna
