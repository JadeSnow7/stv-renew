#include "infra/curl_http_client.h"

namespace stv::infra {

CurlHttpClient::CurlHttpClient() : curl_(nullptr) {}

CurlHttpClient::~CurlHttpClient() = default;

size_t CurlHttpClient::write_callback(char *, size_t, size_t, void *) {
  return 0;
}

int CurlHttpClient::progress_callback(void *, long long, long long, long long,
                                      long long) {
  return 0;
}

HttpErrorCode CurlHttpClient::classify_curl_error(int) const {
  return HttpErrorCode::UNKNOWN;
}

stv::core::Result<HttpResponse, stv::core::TaskError> CurlHttpClient::execute(
    const HttpRequest &, std::shared_ptr<stv::core::CancelToken>) {
  return stv::core::Result<HttpResponse, stv::core::TaskError>::Err(
      make_http_error(HttpErrorCode::UNKNOWN,
                      "HTTP client backend unavailable on this build.",
                      "libcurl development package not found at build time",
                      false));
}

bool CurlHttpClient::cancel(const std::string&) { return false; }

} // namespace stv::infra
