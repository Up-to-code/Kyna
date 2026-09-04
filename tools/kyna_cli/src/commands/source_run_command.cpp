#include "../cli_commands.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <thread>

namespace kyna::cli {
namespace {

class ProgressAnimation {
public:
  ProgressAnimation(bool enabled, std::ostream &target) : output(target) {
    if (!enabled)
      return;
    worker = std::thread([this] {
      constexpr std::array frames{"◐", "◓", "◑", "◒"};
      std::size_t frame = 0;
      while (!stopRequested.load(std::memory_order_relaxed)) {
        output << "\r" << frames[frame++ % frames.size()]
               << " Kyna is waking the bytecode…" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
      }
    });
  }

  ~ProgressAnimation() {
    if (!worker.joinable())
      return;
    stopRequested.store(true, std::memory_order_relaxed);
    worker.join();
    output << "\r\x1b[2K" << std::flush;
  }

private:
  std::ostream &output;
  std::atomic<bool> stopRequested{false};
  std::thread worker;
};

} // namespace

int runSourceFile(const Options &options, LanguageSession &session, std::istream &input,
                  std::ostream &output, std::ostream &errors) {
  if (options.input != "-") {
    ProgressAnimation progress(options.progress && options.richTerminal, errors);
    const auto result = session.run(options.input);
    const auto exitCode = renderResult(result, options, session, errors);
    if (options.heapStats)
      output << "heap: live=" << result.heapStats.live
             << " allocated=" << result.heapStats.allocated
             << " reclaimed=" << result.heapStats.reclaimed
             << " collections=" << result.heapStats.collections
             << " peak=" << result.heapStats.peakLive
             << " next-threshold=" << result.heapStats.nextThreshold
             << " objects=" << result.heapStats.objects
             << " arrays=" << result.heapStats.arrays
             << " captures=" << result.heapStats.captureCells
             << " closures=" << result.heapStats.closures
             << " bound-methods=" << result.heapStats.boundMethods
             << " errors=" << result.heapStats.errors << '\n';
    return exitCode;
  }
  std::string readError;
  auto source = readInput(options.input, input, readError);
  if (!readError.empty()) {
    errors << "kyna: " << readError << '\n';
    return 2;
  }
  const auto result = session.runSource("<stdin>", std::move(source));
  const auto exitCode = renderResult(result, options, session, errors);
  if (options.heapStats)
    output << "heap: live=" << result.heapStats.live
           << " allocated=" << result.heapStats.allocated
           << " reclaimed=" << result.heapStats.reclaimed
           << " collections=" << result.heapStats.collections
           << " peak=" << result.heapStats.peakLive
           << " next-threshold=" << result.heapStats.nextThreshold
           << " objects=" << result.heapStats.objects
           << " arrays=" << result.heapStats.arrays
           << " captures=" << result.heapStats.captureCells
           << " closures=" << result.heapStats.closures
           << " bound-methods=" << result.heapStats.boundMethods
           << " errors=" << result.heapStats.errors << '\n';
  return exitCode;
}

} // namespace kyna::cli
