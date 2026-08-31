#include "kyna/execution/runtime_capabilities.hpp"
#include <algorithm>
#include <curl/curl.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <string_view>
#include <atomic>
#include <csignal>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace kyna {
namespace {

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
};

class LocalProcess final : public ProcessPort {
public:
  int run(const std::string &command) override { return std::system(command.c_str()); }
  std::optional<std::string> environment(const std::string &name) override {
    const auto *value = std::getenv(name.c_str());
    return value ? std::optional<std::string>(value) : std::nullopt;
  }
};

class LocalHostInfo final : public HostInfoPort {
public:
  std::string operatingSystem() const override {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#else
    return "unknown";
#endif
  }

  std::string architecture() const override {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
  }

  std::optional<std::string> workingDirectory(std::string &error) const override {
    std::error_code failure;
    auto directory = std::filesystem::current_path(failure);
    if (failure) {
      error = "read current working directory: " + failure.message();
      return std::nullopt;
    }
    return directory.string();
  }

  bool standardOutputIsTerminal() const override {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
  }

  bool supportsColor() const override {
    if (!standardOutputIsTerminal())
      return false;
    const auto *noColor = std::getenv("NO_COLOR");
    if (noColor)
      return false;
    const auto *terminal = std::getenv("TERM");
    return !terminal || std::string_view(terminal) != "dumb";
  }
};

class SystemClock final : public ClockPort {
public:
  void sleep(std::chrono::milliseconds duration) override { std::this_thread::sleep_for(duration); }
};

std::atomic_bool serverInterrupted{false};
void interruptServer(int) { serverInterrupted = true; }

class BeastHttpServer final : public HttpServerPort {
public:
  bool listen(const HttpServerOptions &options, HttpRequestHandler handler,
              std::string &error) override {
    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    using tcp = asio::ip::tcp;
    boost::system::error_code failure;
    const auto address = asio::ip::make_address(options.host, failure);
    if (failure) { error = "invalid bind address '" + options.host + "': " + failure.message(); return false; }
    asio::io_context context;
    tcp::acceptor acceptor(context);
    acceptor.open(address.is_v6() ? tcp::v6() : tcp::v4(), failure);
    if (failure) { error = "open HTTP listener: " + failure.message(); return false; }
    acceptor.set_option(asio::socket_base::reuse_address(true), failure);
    acceptor.bind({address, options.port}, failure);
    if (failure) { error = "bind " + options.host + ":" + std::to_string(options.port) + ": " + failure.message(); return false; }
    acceptor.listen(asio::socket_base::max_listen_connections, failure);
    if (failure) { error = "listen: " + failure.message(); return false; }
    acceptor.non_blocking(true, failure);
    serverInterrupted = false;
    const auto previousInterrupt = std::signal(SIGINT, interruptServer);
    const auto previousTerminate = std::signal(SIGTERM, interruptServer);
    while (!serverInterrupted) {
      tcp::socket socket(context); acceptor.accept(socket, failure);
      if (failure == asio::error::would_block || failure == asio::error::try_again) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue;
      }
      if (failure) { error = "accept: " + failure.message(); break; }
      beast::tcp_stream stream(std::move(socket)); stream.expires_after(options.timeout);
      beast::flat_buffer buffer; http::request_parser<http::string_body> parser;
      parser.body_limit(options.maximumBodyBytes);
      http::read(stream, buffer, parser, failure);
      HttpOutgoingResponse outgoing;
      if (failure == http::error::body_limit) outgoing = {413, "request body is too large", {{"content-type", "text/plain; charset=utf-8"}}};
      else if (failure) outgoing = {400, "invalid HTTP request", {{"content-type", "text/plain; charset=utf-8"}}};
      else {
        const auto request = parser.release(); HttpIncomingRequest incoming;
        incoming.method = std::string(request.method_string()); incoming.target = std::string(request.target()); incoming.body = request.body();
        for (const auto &header : request) incoming.headers.emplace(std::string(header.name_string()), std::string(header.value()));
        try { outgoing = handler(incoming); }
        catch (const std::exception &) { outgoing = {500, "internal server error", {{"content-type", "text/plain; charset=utf-8"}}}; }
      }
      http::response<http::string_body> response{static_cast<http::status>(outgoing.status), 11};
      response.body() = std::move(outgoing.body); response.keep_alive(false);
      for (const auto &[name, value] : outgoing.headers) response.set(name, value);
      if (response.find(http::field::content_type) == response.end()) response.set(http::field::content_type, "text/plain; charset=utf-8");
      response.prepare_payload(); stream.expires_after(options.timeout); http::write(stream, response, failure);
      stream.socket().shutdown(tcp::socket::shutdown_send, failure);
    }
    std::signal(SIGINT, previousInterrupt); std::signal(SIGTERM, previousTerminate);
    if (serverInterrupted) { error = "interrupted"; return false; }
    return error.empty();
  }
};

std::once_flag curlInitialization;

std::size_t appendResponse(char *contents, std::size_t size, std::size_t count, void *target) {
  const auto bytes = size * count;
  static_cast<std::string *>(target)->append(contents, bytes);
  return bytes;
}

std::string trimHeader(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::size_t appendHeader(char *contents, std::size_t size, std::size_t count, void *target) {
  const auto bytes = size * count;
  std::string line(contents, bytes);
  const auto separator = line.find(':');
  if (separator == std::string::npos)
    return bytes;
  auto name = trimHeader(line.substr(0, separator));
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char character) { return std::tolower(character); });
  auto value = trimHeader(line.substr(separator + 1));
  auto &headers = *static_cast<std::map<std::string, std::string> *>(target);
  if (const auto existing = headers.find(name); existing != headers.end())
    existing->second += ", " + value;
  else
    headers.emplace(std::move(name), std::move(value));
  return bytes;
}

NetworkFailurePhase requestPhase(CURLcode code) {
  switch (code) {
  case CURLE_COULDNT_RESOLVE_HOST: return NetworkFailurePhase::Dns;
  case CURLE_COULDNT_CONNECT: return NetworkFailurePhase::Connect;
  case CURLE_OPERATION_TIMEDOUT: return NetworkFailurePhase::Timeout;
  case CURLE_SSL_CONNECT_ERROR:
  case CURLE_PEER_FAILED_VERIFICATION:
  case CURLE_SSL_CERTPROBLEM:
  case CURLE_SSL_CIPHER: return NetworkFailurePhase::Tls;
  case CURLE_RECV_ERROR: return NetworkFailurePhase::Receive;
  case CURLE_SEND_ERROR: return NetworkFailurePhase::Send;
  case CURLE_HTTP_RETURNED_ERROR: return NetworkFailurePhase::Http;
  default: return NetworkFailurePhase::Transfer;
  }
}

bool retryable(CURLcode code) {
  return code == CURLE_COULDNT_CONNECT || code == CURLE_OPERATION_TIMEDOUT ||
         code == CURLE_RECV_ERROR || code == CURLE_SEND_ERROR;
}

class CurlNetwork final : public NetworkPort {
public:
  CurlNetwork() { std::call_once(curlInitialization, [] { curl_global_init(CURL_GLOBAL_DEFAULT); }); }

  std::optional<NetworkResponse> send(const NetworkRequest &request,
                                      NetworkFailure &failure) override {
    if (request.url == "mock://kyna/users")
      return NetworkResponse{200,
                             R"([{"id":1,"name":"Ada","active":true},{"id":2,"name":"Linus","active":false},{"id":3,"name":"Grace","active":true}])",
                             request.url,
                             {{"content-type", "application/json"}}};
    if (!request.url.starts_with("http://") && !request.url.starts_with("https://")) {
      failure = {NetworkFailurePhase::Transfer, 0,
                 "unsupported URL scheme; expected http or https", false};
      return std::nullopt;
    }

    CURLcode result = CURLE_FAILED_INIT;
    long status = 0;
    std::string response;
    std::string effectiveUrl;
    std::map<std::string, std::string> responseHeaders;
    char nativeError[CURL_ERROR_SIZE]{};
    for (int attempt = 1; attempt <= 3; ++attempt) {
      response.clear();
      responseHeaders.clear();
      auto *handle = curl_easy_init();
      if (!handle) {
        failure = {NetworkFailurePhase::Transfer, static_cast<int>(CURLE_FAILED_INIT),
                   "HTTP client initialization failed", false};
        return std::nullopt;
      }
      curl_slist *headers = nullptr;
      if (request.body && !request.headers.contains("Content-Type"))
        headers = curl_slist_append(headers, "Content-Type: application/json");
      for (const auto &[name, value] : request.headers)
        headers = curl_slist_append(headers, (name + ": " + value).c_str());
      curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str());
      curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request.method.c_str());
      curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 8L);
      curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
      curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeout.count()));
      curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
      curl_easy_setopt(handle, CURLOPT_USERAGENT, "Kyna/1.0.0");
      curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, nativeError);
      curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, appendResponse);
      curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response);
      curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, appendHeader);
      curl_easy_setopt(handle, CURLOPT_HEADERDATA, &responseHeaders);
      if (headers)
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
      if (request.body) {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body->data());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE,
                         static_cast<curl_off_t>(request.body->size()));
      }
      result = curl_easy_perform(handle);
      curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
      if (char *effective = nullptr;
          curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &effective) == CURLE_OK && effective)
        effectiveUrl = effective;
      if (headers)
        curl_slist_free_all(headers);
      curl_easy_cleanup(handle);
      if (result == CURLE_OK)
        return NetworkResponse{status, std::move(response), std::move(effectiveUrl),
                               std::move(responseHeaders)};
      if (!retryable(result) || attempt == 3)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(150 * attempt));
    }

    const std::string nativeMessage = nativeError[0] ? nativeError : curl_easy_strerror(result);
    failure = {requestPhase(result), static_cast<int>(result), nativeMessage, retryable(result)};
    return std::nullopt;
  }
};

} // namespace

const char *networkFailurePhaseName(NetworkFailurePhase phase) {
  switch (phase) {
  case NetworkFailurePhase::Dns: return "DNS resolution";
  case NetworkFailurePhase::Connect: return "connection";
  case NetworkFailurePhase::Tls: return "TLS handshake";
  case NetworkFailurePhase::Send: return "request send";
  case NetworkFailurePhase::Receive: return "response receive";
  case NetworkFailurePhase::Timeout: return "timeout";
  case NetworkFailurePhase::Http: return "HTTP response";
  case NetworkFailurePhase::Transfer: return "transfer";
  }
  return "transfer";
}

RuntimeCapabilities productionRuntimeCapabilities() {
  return {std::make_shared<LocalFileSystem>(), std::make_shared<LocalProcess>(),
          std::make_shared<CurlNetwork>(), std::make_shared<SystemClock>(),
          productionDatabasePort(), std::make_shared<LocalHostInfo>(),
          std::make_shared<BeastHttpServer>()};
}

} // namespace kyna
