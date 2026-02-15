#include "infra/http_client.h"
#include "infra/logger.h"
#include <thread>
#include <algorithm>
#include <charconv>

namespace stv::infra {

stv::core::TaskError make_http_error(
    HttpErrorCode code,
    const std::string& user_message,
    const std::string& internal_message,
    bool retryable
) {
    // TODO 1: 实现错误转换
    // 需要填充：category, code (int), retryable, user_message, internal_message, details
    // 提示：根据 HttpErrorCode 映射到 ErrorCategory（Network/Service/Validation）
    // 提示：details 中应添加 "http_error_code" 键，值为 code 的字符串形式
    
    
    stv::core::ErrorCategory category = stv::core::ErrorCategory::Unknown;
    switch (code) {
        case HttpErrorCode::NETWORK_ERROR:
            category = stv::core::ErrorCategory::Network;
            break;
        case HttpErrorCode::TIMEOUT:
            category = stv::core::ErrorCategory::Timeout;
            break;
        case HttpErrorCode::CANCELED:
            category = stv::core::ErrorCategory::Canceled;
            break;
        case HttpErrorCode::SERVER_ERROR:
        case HttpErrorCode::RATE_LIMIT:
            category = stv::core::ErrorCategory::Network;  // 服务端问题归为 Network
            break;
        case HttpErrorCode::CLIENT_ERROR:
        case HttpErrorCode::PARSE_ERROR:
            category = stv::core::ErrorCategory::Pipeline;  // 客户端逻辑错误归为 Pipeline
            break;
        default:
            category = stv::core::ErrorCategory::Unknown;
    }

    std::map<std::string, std::string> details = { {"http_error_code", std::to_string(static_cast<int>(code))} };
    return stv::core::TaskError(category, static_cast<int>(code), retryable, user_message, internal_message, details);
}

RetryableHttpClient::RetryableHttpClient(
    std::shared_ptr<IHttpClient> inner,
    RetryPolicy policy
) : inner_(std::move(inner)), policy_(std::move(policy)) {}

stv::core::Result<HttpResponse, stv::core::TaskError> RetryableHttpClient::execute(
    const HttpRequest& request,
    std::shared_ptr<stv::core::CancelToken> cancel_token
) {
    int attempt = 0;
    auto backoff = policy_.initial_backoff;

    while (true) {
        // TODO 2: 检查 cancel_token 是否已取消
        // 如果已取消，返回 Err(CANCELED)
        // 提示：需处理 cancel_token 为 nullptr 的情况
        // YOUR CODE HERE

        // TODO 2: 检查 cancel_token 是否已取消
        if (cancel_token && cancel_token->is_canceled()) {
            return stv::core::Result<HttpResponse, stv::core::TaskError>::Err(
                stv::core::TaskError::Canceled("Operation canceled by cancel token")
            );
        }

        // TODO 3: 调用 inner_->execute(request, cancel_token)
        auto result = inner_->execute(request, cancel_token);

        // TODO 4: 如果成功，直接返回
        if (result.is_ok()) {
            return result;
        }

        // TODO 5: 提取错误，判断是否应该重试
        auto& error = result.error();

        HttpErrorCode http_code = HttpErrorCode::UNKNOWN;
        const auto it = error.details.find("http_error_code");
        if (it != error.details.end()) {
            int parsed_code = 0;
            const std::string& code_str = it->second;
            const char* begin = code_str.data();
            const char* end = begin + code_str.size();
            auto [ptr, ec] = std::from_chars(begin, end, parsed_code);
            if (ec == std::errc() && ptr == end) {
                http_code = static_cast<HttpErrorCode>(parsed_code);
            }
        }

        bool should_retry = policy_.should_retry(http_code);
        bool has_attempts_left = attempt < policy_.max_retries;

        // TODO 7: 如果不满足重试条件，返回错误
        if (!should_retry || !has_attempts_left) {
            return result;
        }

        // TODO 8: 记录重试日志（使用 ILogger，假设已注入为成员或全局）
        // 格式：[trace_id] Retry attempt {attempt+1}/{max_retries}, backoff={backoff}ms
        // 提示：可用 printf 或构造 string，暂不强制用 ILogger（M2 后期统一）
        printf("[%s] Retry attempt %d/%d, backoff=%ldms\n",
               request.trace_id.c_str(), attempt + 1, policy_.max_retries,
               backoff.count());

        // TODO 9: 指数退避 sleep（需检测 cancel_token）
        // 提示：不能用 std::this_thread::sleep_for(backoff)，否则取消不及时
        // 需要分片 sleep + 检测 cancel
        auto sleep_until = std::chrono::steady_clock::now() + backoff;
        while (std::chrono::steady_clock::now() < sleep_until) {
            // YOUR CODE: 检查 cancel_token，如已取消则提前返回
            if (cancel_token && cancel_token->is_canceled()) {
                return stv::core::Result<HttpResponse, stv::core::TaskError>::Err(
                    stv::core::TaskError::Canceled("Operation canceled during backoff sleep")
                );
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // TODO 10: 更新 backoff（指数增长 + 上限）
        // 提示：backoff = std::min(backoff * multiplier, max_backoff)
        auto new_backoff = std::chrono::duration_cast<std::chrono::milliseconds>(
            backoff * policy_.backoff_multiplier
        );
        backoff = std::min(new_backoff, policy_.max_backoff);
        attempt++;
    }
}

bool RetryableHttpClient::cancel(const std::string& request_id) {
    return inner_ ? inner_->cancel(request_id) : false;
}

} // namespace stv::infra
