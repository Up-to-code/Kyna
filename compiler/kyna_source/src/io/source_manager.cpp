#include "kyna/source/source_manager.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace kyna {
SourceId SourceManager::add(std::filesystem::path path, std::string text) {
  ManagedSource source;
  source.file.id = sources.size() + 1;
  source.file.path = std::move(path);
  source.file.text = std::move(text);
  source.lineStarts.push_back(0);
  for (std::size_t offset = 0; offset < source.file.text.size(); ++offset)
    if (source.file.text[offset] == '\n')
      source.lineStarts.push_back(offset + 1);
  sources.push_back(std::move(source));
  return sources.back().file.id;
}

std::optional<SourceId> SourceManager::load(const std::filesystem::path &path, std::string &error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error = "cannot open '" + path.string() + "'";
    return std::nullopt;
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  return add(path, contents.str());
}


} // namespace kyna
