#include "kyna/source/source_manager.hpp"
#include <algorithm>

namespace kyna {
const SourceFile *SourceManager::find(SourceId id) const {
  if (id == UnknownSource || id > sources.size())
    return nullptr;
  return &sources[id - 1].file;
}

std::string_view SourceManager::line(SourceId id, int oneBasedLine) const {
  if (id == UnknownSource || id > sources.size() || oneBasedLine < 1)
    return {};
  const auto &source = sources[id - 1];
  const auto index = static_cast<std::size_t>(oneBasedLine - 1);
  if (index >= source.lineStarts.size())
    return {};
  const auto start = source.lineStarts[index];
  auto end = source.file.text.find('\n', start);
  if (end == std::string::npos)
    end = source.file.text.size();
  if (end > start && source.file.text[end - 1] == '\r')
    --end;
  return std::string_view(source.file.text).substr(start, end - start);
}

SourceSpan SourceManager::span(SourceId id, std::size_t start, std::size_t end) const {
  if (id == UnknownSource || id > sources.size())
    return {};
  const auto &source = sources[id - 1];
  start = std::min(start, source.file.text.size());
  end = std::clamp(end, start, source.file.text.size());
  auto locate = [&](std::size_t offset) {
    auto found = std::upper_bound(source.lineStarts.begin(), source.lineStarts.end(), offset);
    const auto lineIndex = static_cast<std::size_t>(found - source.lineStarts.begin() - 1);
    return std::pair{static_cast<int>(lineIndex + 1),
                     static_cast<int>(offset - source.lineStarts[lineIndex] + 1)};
  };
  const auto [startLine, startColumn] = locate(start);
  const auto [endLine, endColumn] = locate(end);
  return {id, start, end, startLine, startColumn, endLine, endColumn};
}

} // namespace kyna
