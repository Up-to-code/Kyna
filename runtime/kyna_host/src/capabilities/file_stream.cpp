#include "file_stream.hpp"

namespace kyna::detail {

FileStreamReader::FileStreamReader(std::filesystem::path path)
    : stream_(path, std::ios::binary) {}

std::size_t FileStreamReader::read(std::uint8_t *buffer, std::size_t size) {
  if (!stream_.is_open() || size == 0)
    return 0;
  stream_.read(reinterpret_cast<char *>(buffer), static_cast<std::streamsize>(size));
  const auto readCount = stream_.gcount();
  if (readCount < 0)
    return 0;
  return static_cast<std::size_t>(readCount);
}

void FileStreamReader::close() {
  if (stream_.is_open())
    stream_.close();
}

FileStreamWriter::FileStreamWriter(std::filesystem::path path)
    : stream_(path, std::ios::binary | std::ios::trunc) {}

std::size_t FileStreamWriter::write(const std::uint8_t *buffer, std::size_t size) {
  if (!stream_.is_open() || size == 0)
    return 0;
  stream_.write(reinterpret_cast<const char *>(buffer), static_cast<std::streamsize>(size));
  if (!stream_)
    return 0;
  return size;
}

void FileStreamWriter::close() {
  if (stream_.is_open())
    stream_.close();
}

} // namespace kyna::detail
