#pragma once

#include "kyna/execution/runtime_capabilities.hpp"

#include <filesystem>
#include <fstream>
#include <memory>

namespace kyna::detail {

// Streaming reader backed by an open binary file. Reads are served directly
// from the underlying stream so callers can process arbitrarily large files in
// bounded chunks.
class FileStreamReader final : public IReadCloser {
public:
  explicit FileStreamReader(std::filesystem::path path);
  std::size_t read(std::uint8_t *buffer, std::size_t size) override;
  void close() override;
  bool isOpen() const { return stream_.is_open(); }

private:
  std::ifstream stream_;
};

// Streaming writer backed by an open binary file (truncating on creation).
class FileStreamWriter final : public IWriteCloser {
public:
  explicit FileStreamWriter(std::filesystem::path path);
  std::size_t write(const std::uint8_t *buffer, std::size_t size) override;
  void close() override;
  bool isOpen() const { return stream_.is_open(); }

private:
  std::ofstream stream_;
};

} // namespace kyna::detail
