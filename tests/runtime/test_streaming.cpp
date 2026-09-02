#include "kyna/execution/runtime_capabilities.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace kyna;

std::filesystem::path makeTempPath() {
  static int counter = 0;
  return std::filesystem::temp_directory_path() /
         ("kyna_stream_test_" + std::to_string(++counter) + ".bin");
}

void test_streaming_round_trip_in_chunks() {
  const auto path = makeTempPath();
  auto capabilities = productionRuntimeCapabilities();
  std::string error;

  // Write ~1.5MB in bounded chunks through the streaming writer.
  const std::size_t totalBytes = 1500 * 1024 + 123;
  auto writer = capabilities.files->openWrite(path, error);
  assert(writer != nullptr);
  const std::uint8_t pattern = 0xAB;
  std::vector<std::uint8_t> chunk(64 * 1024, pattern);
  std::size_t written = 0;
  while (written < totalBytes) {
    const auto n = std::min(chunk.size(), totalBytes - written);
    assert(writer->write(chunk.data(), n) == n);
    written += n;
  }
  writer->close();

  // Read it back in bounded chunks through the streaming reader.
  auto reader = capabilities.files->openRead(path, error);
  assert(reader != nullptr);
  std::size_t totalRead = 0;
  std::vector<std::uint8_t> buffer(64 * 1024);
  while (true) {
    const auto n = reader->read(buffer.data(), buffer.size());
    if (n == 0)
      break;
    for (std::size_t i = 0; i < n; ++i)
      assert(buffer[i] == pattern);
    totalRead += n;
  }
  reader->close();

  assert(totalRead == totalBytes);
  std::filesystem::remove(path);
}

void test_context_background_never_cancelled() {
  auto ctx = Context::Background();
  assert(!ctx->isCancelled());
  assert(!ctx->deadline().has_value());
  ctx->cancel(); // no-op, must not trip
  assert(!ctx->isCancelled());
}

void test_context_with_timeout_fires() {
  auto [ctx, cancelCb] = Context::WithTimeout(Context::Background(), std::chrono::milliseconds(10));
  assert(!ctx->isCancelled());
  assert(ctx->deadline().has_value());
  // Wait past the deadline and observe the context becoming cancelled.
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  assert(ctx->isCancelled());
  (void)cancelCb;
}

void test_context_manual_cancel() {
  auto [ctx, cancelCb] = Context::WithTimeout(Context::Background(), std::chrono::minutes(1));
  assert(!ctx->isCancelled());
  cancelCb();
  assert(ctx->isCancelled());
}

} // namespace

int main() {
  test_streaming_round_trip_in_chunks();
  test_context_background_never_cancelled();
  test_context_with_timeout_fires();
  test_context_manual_cancel();
  return 0;
}
