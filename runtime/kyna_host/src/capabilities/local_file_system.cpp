#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "kyna/execution/runtime_capabilities.hpp"
#include "../host_private.hpp"
#include "file_stream.hpp"

namespace kyna::detail {

class LocalFileSystem final : public FileSystemPort {
public:
  std::optional<std::string> read(const std::filesystem::path &path, std::string &error) override {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      error = "read file '" + path.string() + "': path is missing or permission was denied";
      return std::nullopt;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
  }

  bool write(const std::filesystem::path &path, const std::string &contents,
             std::string &error) override {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
      error = "write file '" + path.string() + "': parent is missing or permission was denied";
      return false;
    }
    file << contents;
    if (!file) {
      error = "write file '" + path.string() + "': not all bytes reached storage";
      return false;
    }
    return true;
  }

  bool createDirectories(const std::filesystem::path &path, std::string &error) override {
    std::error_code failure;
    std::filesystem::create_directories(path, failure);
    if (failure) {
      error = "create directory '" + path.string() + "': " + failure.message();
      return false;
    }
    return std::filesystem::is_directory(path, failure) && !failure;
  }

  bool exists(const std::filesystem::path &path, std::string &error) override {
    std::error_code failure;
    const bool found = std::filesystem::exists(path, failure);
    if (failure)
      error = "inspect path '" + path.string() + "': " + failure.message();
    return found;
  }

  bool remove(const std::filesystem::path &path, std::string &error) override {
    std::error_code failure;
    const bool removed = std::filesystem::remove(path, failure);
    if (failure)
      error = "remove path '" + path.string() + "': " + failure.message();
    return removed;
  }

  std::optional<std::vector<std::string>> list(const std::filesystem::path &path,
                                               std::string &error) override {
    std::error_code failure;
    std::filesystem::directory_iterator entries(path, failure);
    if (failure) {
      error = "list directory '" + path.string() + "': " + failure.message();
      return std::nullopt;
    }
    std::vector<std::string> names;
    for (const auto &entry : entries)
      names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
  }

  ReadCloser openRead(const std::filesystem::path &path, std::string &error) override {
    auto stream = std::make_shared<FileStreamReader>(path);
    if (!stream->isOpen()) {
      error = "read file '" + path.string() + "': path is missing or permission was denied";
      return nullptr;
    }
    return stream;
  }

  WriteCloser openWrite(const std::filesystem::path &path, std::string &error) override {
    auto stream = std::make_shared<FileStreamWriter>(path);
    if (!stream->isOpen()) {
      error = "write file '" + path.string() + "': parent is missing or permission was denied";
      return nullptr;
    }
    return stream;
  }
};

std::shared_ptr<FileSystemPort> makeLocalFileSystem() {
  return std::make_shared<LocalFileSystem>();
}

} // namespace kyna::detail
