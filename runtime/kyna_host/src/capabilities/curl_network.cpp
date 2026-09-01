#include <algorithm>
#include <chrono>
#include <curl/curl.h>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "kyna/execution/runtime_capabilities.hpp"
#include "../host_private.hpp"

namespace kyna::detail {
namespace {

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

} // namespace

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

std::shared_ptr<NetworkPort> makeCurlNetwork() {
  return std::make_shared<CurlNetwork>();
}

} // namespace kyna::detail
