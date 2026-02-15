#include "infra/curl_http_client.h"
#include "infra/logger.h"
#include <curl/curl.h>
#include <chrono>
#include <sstream>

namespace stv::infra {

namespace {
    // 一次性初始化 libcurl（全局）
    struct CurlGlobalInit {
        CurlGlobalInit() { curl_global_init(CURL_GLOBAL_ALL); }
        ~CurlGlobalInit() { curl_global_cleanup(); }
    };
    static CurlGlobalInit g_curl_init;
}

CurlHttpClient::CurlHttpClient() {
    curl_ = curl_easy_init();
    if (!curl_) {
        throw std::runtime_error("Failed to initialize CURL");
    }
}

CurlHttpClient::~CurlHttpClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

// Write callback：接收响应数据
size_t CurlHttpClient::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buffer = static_cast<std::string*>(userdata);
    size_t total_size = size * nmemb;
    buffer->append(ptr, total_size);
    return total_size;
}

// Progress callback：支持取消
int CurlHttpClient::progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                       curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal; (void)dlnow; (void)ultotal; (void)ulnow;  // 避免未使用警告

    auto* cancel_token = static_cast<stv::core::CancelToken*>(clientp);
    if (cancel_token && cancel_token->is_canceled()) {
        return 1;  // 非0返回值会让 curl 中止请求
    }
    return 0;
}

HttpErrorCode CurlHttpClient::classify_curl_error(int curl_code) const {
    switch (curl_code) {
        case CURLE_OK:
            return HttpErrorCode::UNKNOWN;  // 不应该走到这里

        // 网络错误
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
            return HttpErrorCode::NETWORK_ERROR;

        // 超时
        case CURLE_OPERATION_TIMEDOUT:
            return HttpErrorCode::TIMEOUT;

        // 取消
        case CURLE_ABORTED_BY_CALLBACK:
            return HttpErrorCode::CANCELED;

        default:
            return HttpErrorCode::UNKNOWN;
    }
}

stv::core::Result<HttpResponse, stv::core::TaskError> CurlHttpClient::execute(
    const HttpRequest& request,
    std::shared_ptr<stv::core::CancelToken> cancel_token
) {
    using Result = stv::core::Result<HttpResponse, stv::core::TaskError>;

    // 重置 CURL handle（清除上次请求的状态）
    curl_easy_reset(curl_);

    // 设置 URL
    curl_easy_setopt(curl_, CURLOPT_URL, request.url.c_str());

    // 设置超时（毫秒）
    long timeout_ms = static_cast<long>(request.timeout.count());
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms / 2);  // 连接超时为总超时的一半

    // 设置请求方法
    if (request.method == HttpMethod::POST) {
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, request.body.size());
    } else if (request.method == HttpMethod::GET) {
        curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    }
    // TODO: 支持 PUT/DELETE

    // 设置请求头
    struct curl_slist* headers = nullptr;
    for (const auto& [key, value] : request.headers) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    }

    // 设置响应数据接收回调
    std::string response_buffer;
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &CurlHttpClient::write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_buffer);

    // 设置取消支持（通过 progress callback）
    if (cancel_token) {
        curl_easy_setopt(curl_, CURLOPT_XFERINFOFUNCTION, &CurlHttpClient::progress_callback);
        curl_easy_setopt(curl_, CURLOPT_XFERINFODATA, cancel_token.get());
        curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 0L);  // 启用 progress callback
    }

    // 记录开始时间
    auto start_time = std::chrono::steady_clock::now();

    // 执行请求
    CURLcode res = curl_easy_perform(curl_);

    // 记录耗时
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // 清理请求头
    if (headers) {
        curl_slist_free_all(headers);
    }

    // 检查请求是否成功
    if (res != CURLE_OK) {
        HttpErrorCode error_code = classify_curl_error(res);
        std::string curl_error_msg = curl_easy_strerror(res);

        std::string user_message;
        switch (error_code) {
            case HttpErrorCode::NETWORK_ERROR:
                user_message = "Network error occurred. Please check your connection.";
                break;
            case HttpErrorCode::TIMEOUT:
                user_message = "Request timed out. Please try again.";
                break;
            case HttpErrorCode::CANCELED:
                user_message = "Request was canceled.";
                break;
            default:
                user_message = "Unknown error occurred.";
                break;
        }

        std::string internal_message = "CURL error: " + curl_error_msg + " (code: " + std::to_string(res) + ")";

        return Result::Err(
            make_http_error(error_code, user_message, internal_message, error_code != HttpErrorCode::CANCELED)
        );
    }

    // 获取 HTTP 状态码
    long http_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);

    // 检查 HTTP 状态码
    if (http_code >= 500) {
        // 5xx 服务端错误
        return Result::Err(
            make_http_error(
                HttpErrorCode::SERVER_ERROR,
                "Server error occurred. Please try again later.",
                "HTTP " + std::to_string(http_code) + " response",
                true  // 可重试
            )
        );
    } else if (http_code == 429) {
        // 429 限流
        return Result::Err(
            make_http_error(
                HttpErrorCode::RATE_LIMIT,
                "Too many requests. Please slow down.",
                "HTTP 429 Rate Limit",
                true  // 可重试
            )
        );
    } else if (http_code >= 400) {
        // 4xx 客户端错误（不可重试）
        return Result::Err(
            make_http_error(
                HttpErrorCode::CLIENT_ERROR,
                "Invalid request. Please check your parameters.",
                "HTTP " + std::to_string(http_code) + " response",
                false  // 不可重试
            )
        );
    }

    // 构造成功响应
    HttpResponse response;
    response.status_code = static_cast<int>(http_code);
    response.body = std::move(response_buffer);
    response.request_id = request.trace_id + "_resp";  // 简化版，实际应从响应头提取
    response.elapsed_ms = std::chrono::milliseconds(elapsed_ms);

    // TODO: 解析响应头（需要设置 CURLOPT_HEADERFUNCTION）

    return Result::Ok(std::move(response));
}

} // namespace stv::infra
