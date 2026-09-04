#pragma once

#include <filesystem>
#include <vector>

namespace kyna::export_cache {

// Sidecar next to a module path: `math.kyna` → `math.kyna.kyc`.
std::filesystem::path cachePath(const std::filesystem::path &modulePath);

// True when KYNA_DISABLE_EXPORT_CACHE is unset/empty.
bool enabled();

bool stampMatches(const std::vector<std::filesystem::path> &sources,
                  const std::filesystem::path &cacheFile);

void writeStamp(const std::vector<std::filesystem::path> &sources,
                const std::filesystem::path &cacheFile);

void invalidate(const std::filesystem::path &cacheFile);

} // namespace kyna::export_cache
