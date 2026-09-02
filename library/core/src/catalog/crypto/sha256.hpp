#pragma once

#include <string>
#include <string_view>

namespace kyna::detail {

// Compute the SHA-256 digest of `data` and return it as a lowercase hex string.
std::string sha256Hex(std::string_view data);

} // namespace kyna::detail
