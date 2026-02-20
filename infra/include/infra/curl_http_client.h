#pragma once

#include "infra/http_client.h"
#include <memory>
#include <cstdint>
#include <mutex>
#include <unordered_map>

// 前向声明（隐藏 curl 实现细节）
typedef void CURL;

namespace stv::infra {

/// 基于 libcurl 的真实 HTTP Client 实现
/// 支持：超时、取消、错误分类、POST/GET
class CurlHttpClient : public IHttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient() override;

    // 禁止拷贝（CURL handle 不可拷贝）
    CurlHttpClient(const CurlHttpClient&) = delete;
    CurlHttpClient& operator=(const CurlHttpClient&) = delete;

    stv::core::Result<HttpResponse, stv::core::TaskError> execute(
        const HttpRequest& request,
        std::shared_ptr<stv::core::CancelToken> cancel_token = nullptr
    ) override;
    bool cancel(const std::string& request_id) override;

private:
    CURL* curl_;  // libcurl handle（单实例，非线程安全）
    std::mutex curl_mutex_;
    std::mutex in_flight_mutex_;
    std::unordered_map<std::string, std::weak_ptr<stv::core::CancelToken>> in_flight_requests_;

    // 辅助方法
    HttpErrorCode classify_curl_error(int curl_code) const;
    void setup_request(const HttpRequest& request);
    HttpResponse parse_response(const std::string& response_buffer, long http_code, int64_t elapsed_ms);

    // CURL 回调函数（静态）
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static int progress_callback(void* clientp, long long dltotal, long long dlnow,
                                  long long ultotal, long long ulnow);
};

} // namespace stv::infra
