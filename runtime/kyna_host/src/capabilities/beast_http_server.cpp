#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>
#include <utility>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "kyna/execution/runtime_capabilities.hpp"
#include "../host_private.hpp"

namespace kyna::detail {
namespace {

std::atomic_bool serverInterrupted{false};
void interruptServer(int) { serverInterrupted = true; }

} // namespace

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

std::shared_ptr<HttpServerPort> makeBeastHttpServer() {
  return std::make_shared<BeastHttpServer>();
}

} // namespace kyna::detail
