#pragma once

#include "kyna/execution/runtime_object_model.hpp"

namespace kyna {

std::string decodeExpressionLiteral(const std::string &literal);
Value evaluateExpressionBinary(TokenKind op, const Value &left, const Value &right,
                               SourceSpan span);

} // namespace kyna
