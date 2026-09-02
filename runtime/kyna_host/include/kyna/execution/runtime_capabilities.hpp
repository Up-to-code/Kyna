#pragma once

#include "kyna/execution/database_port.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kyna {

// ---- Streaming byte I/O ---------------------------------------------------
// Go-style io.Reader / io.Writer adapters so large payloads can be processed in
// bounded chunks instead of being slurped into a single contiguous buffer.

class IReader {
public:
  virtual ~IReader() = default;
  // Reads up to `size` bytes into `buffer`; returns the number of bytes read,
  // or 0 at end-of-stream.
  virtual std::size_t read(std::uint8_t *buffer, std::size_t size) = 0;
};

class IWriter {
public:
  virtual ~IWriter() = default;
  // Writes up to `size` bytes from `buffer`; returns the number of bytes
  // written (may be less than `size` for a partial write).
  virtual std::size_t write(const std::uint8_t *buffer, std::size_t size) = 0;
};

class ICloser {
public:
  virtual ~ICloser() = default;
  virtual void close() = 0;
};

// Combined stream interfaces, mirroring Go's io.ReadCloser / io.WriteCloser.
class IReadCloser : public IReader {
public:
  virtual void close() = 0;
};

class IWriteCloser : public IWriter {
public:
  virtual void close() = 0;
};

using ReadCloser = std::shared_ptr<IReadCloser>;
using WriteCloser = std::shared_ptr<IWriteCloser>;

// A cancellation/deadline context, modelled on Go's context.Context. Long
// operations poll `isCancelled()` and bail out cooperatively once the token is
// signalled.
class Context {
public:
  virtual ~Context() = default;
  virtual bool isCancelled() const = 0;
  virtual std::optional<std::chrono::system_clock::time_point> deadline() const = 0;
  virtual void cancel() = 0;

  // An inert root context that is never cancelled and has no deadline.
  static std::shared_ptr<Context> Background();
  // Returns a derived context that is cancelled when `timeout` elapses or when
  // the returned cancellation callback is invoked.
  static std::pair<std::shared_ptr<Context>, std::function<void()>>
  WithTimeout(const std::shared_ptr<Context> &parent, std::chrono::milliseconds timeout);
};

class FileSystemPort {
public:
  virtual ~FileSystemPort() = default;
  virtual std::optional<std::string> read(const std::filesystem::path &, std::string &error) = 0;
  virtual bool write(const std::filesystem::path &, const std::string &, std::string &error) = 0;
  virtual bool createDirectories(const std::filesystem::path &, std::string &error) {
    error = "filesystem adapter does not support creating directories";
    return false;
  }
  virtual bool exists(const std::filesystem::path &, std::string &error) {
    error = "filesystem adapter does not support existence checks";
    return false;
  }
  virtual bool remove(const std::filesystem::path &, std::string &error) {
    error = "filesystem adapter does not support removing paths";
    return false;
  }
  virtual std::optional<std::vector<std::string>> list(const std::filesystem::path &,
                                                       std::string &error) {
    error = "filesystem adapter does not support listing directories";
    return std::nullopt;
  }
  // Opens a stream for reading. Returns nullptr (and sets `error`) on failure.
  virtual ReadCloser openRead(const std::filesystem::path &, std::string &error) {
    error = "filesystem adapter does not support streaming reads";
    return nullptr;
  }
  // Opens a stream for writing (truncating an existing file). Returns nullptr
  // (and sets `error`) on failure.
  virtual WriteCloser openWrite(const std::filesystem::path &, std::string &error) {
    error = "filesystem adapter does not support streaming writes";
    return nullptr;
  }
};

// Argument-vector process configuration for the secure `spawn` path. No shell
// interpolation is ever performed: the program is located and its arguments
// are passed verbatim, so metacharacters in arguments cannot inject commands.
struct ProcessConfig {
  std::string program;
  std::vector<std::string> args;
  std::filesystem::path workingDir;
  std::map<std::string, std::string> env;
  bool captureOutput{false};
};

struct ProcessResult {
  int exitCode{0};
  std::string stdoutText;
  std::string stderrText;
  // True when the process could not be started at all (e.g. program not found).
  bool failedToStart{false};
  std::string startError;
};

class ProcessPort {
public:
  virtual ~ProcessPort() = default;
  virtual int run(const std::string &command) = 0;
  virtual std::optional<std::string> environment(const std::string &name) = 0;
  // Securely executes a program with an explicit argument vector. The default
  // implementation reports it as unsupported; concrete ports override it with
  // posix_spawn / CreateProcessW.
  virtual ProcessResult spawn(const ProcessConfig &config);
};

// Read-only host and terminal facts. Keeping these behind an injected port makes
// language programs deterministic in tests and embedders.
class HostInfoPort {
public:
  virtual ~HostInfoPort() = default;
  virtual std::string operatingSystem() const = 0;
  virtual std::string architecture() const = 0;
  virtual std::optional<std::string> workingDirectory(std::string &error) const = 0;
  virtual bool standardOutputIsTerminal() const = 0;
  virtual bool supportsColor() const = 0;
};

enum class NetworkFailurePhase { Dns, Connect, Tls, Send, Receive, Timeout, Http, Transfer };

struct NetworkRequest {
  std::string method{"GET"};
  std::string url;
  std::optional<std::string> body;
  std::map<std::string, std::string> headers;
  std::chrono::milliseconds timeout{30000};
};

struct NetworkResponse {
  long status{0};
  std::string body;
  std::string effectiveUrl;
  std::map<std::string, std::string> headers;
  [[nodiscard]] bool ok() const { return status >= 200 && status < 300; }
};

struct NetworkFailure {
  NetworkFailurePhase phase{NetworkFailurePhase::Transfer};
  int nativeCode{0};
  std::string message;
  bool retryable{false};
};

[[nodiscard]] const char *networkFailurePhaseName(NetworkFailurePhase phase);

class NetworkPort {
public:
  virtual ~NetworkPort() = default;
  virtual std::optional<NetworkResponse> send(const NetworkRequest &request,
                                              NetworkFailure &failure) = 0;
};

class ClockPort {
public:
  virtual ~ClockPort() = default;
  virtual void sleep(std::chrono::milliseconds duration) = 0;
};

struct HttpServerOptions {
  std::string host{"127.0.0.1"};
  std::uint16_t port{3000};
  std::size_t maximumBodyBytes{1024 * 1024};
  std::chrono::milliseconds timeout{30000};
};

struct HttpIncomingRequest {
  std::string method;
  std::string target;
  std::string body;
  std::map<std::string, std::string> headers;
};

struct HttpOutgoingResponse {
  int status{200};
  std::string body;
  std::map<std::string, std::string> headers;
};

using HttpRequestHandler = std::function<HttpOutgoingResponse(const HttpIncomingRequest &)>;

class HttpServerPort {
public:
  virtual ~HttpServerPort() = default;
  virtual bool listen(const HttpServerOptions &, HttpRequestHandler, std::string &error) = 0;
};

struct RuntimeCapabilities {
  std::shared_ptr<FileSystemPort> files;
  std::shared_ptr<ProcessPort> processes;
  std::shared_ptr<NetworkPort> network;
  std::shared_ptr<ClockPort> clock;
  std::shared_ptr<DatabasePort> database;
  std::shared_ptr<HostInfoPort> host;
  std::shared_ptr<HttpServerPort> server;
};

RuntimeCapabilities productionRuntimeCapabilities();

} // namespace kyna
