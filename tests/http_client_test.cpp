#include <gtest/gtest.h>
#include "infra/http_client.h"
#include "core/cancel_token.h"
#include <chrono>
#include <thread>

using namespace stv::infra;
using namespace stv::core;
using namespace std::chrono_literals;

// Mock HttpClient（用于测试 RetryableHttpClient）
class MockHttpClient : public IHttpClient {
public:
    // 可配置：第几次调用成功，或一直失败
    int succeed_on_attempt = -1;  // -1 表示一直失败
    int call_count = 0;
    HttpErrorCode error_to_return = HttpErrorCode::NETWORK_ERROR;

    Result<HttpResponse, TaskError> execute(
        const HttpRequest& request,
        std::shared_ptr<CancelToken> cancel_token
    ) override {
        (void)request;  // 避免未使用参数警告
        (void)cancel_token;
        call_count++;
        if (succeed_on_attempt > 0 && call_count == succeed_on_attempt) {
            return Result<HttpResponse, TaskError>::Ok(
                HttpResponse{200, {}, "success", "req-123", 100ms}
            );
        }
        return Result<HttpResponse, TaskError>::Err(
            make_http_error(error_to_return, "mock error", "internal", true)
        );
    }

    bool cancel(const std::string& request_id) override {
        (void)request_id;
        return true;
    }
};

// TEST 1: 第一次就成功，不重试
TEST(RetryableHttpClientTest, SuccessOnFirstAttempt) {
    // TODO: 你来实现
    // 期望：call_count == 1
    // 提示步骤：
    // 1. 创建 MockHttpClient，设置 succeed_on_attempt = 1
    // 2. 创建 RetryableHttpClient(mock_client)
    // 3. 构造 HttpRequest
    // 4. 调用 execute()
    // 5. EXPECT_TRUE(result.is_ok())
    // 6. EXPECT_EQ(mock_client->call_count, 1)
}

// TEST 2: 第 2 次成功，验证重试 1 次
TEST(RetryableHttpClientTest, SuccessOnSecondAttempt) {
    // TODO: 你来实现
    // 期望：call_count == 2，总耗时 >= initial_backoff (1000ms)
    // 提示：用 std::chrono::steady_clock 测量时间
}

// TEST 3: 达到最大重试次数后失败
TEST(RetryableHttpClientTest, ExhaustedRetries) {
    // TODO: 你来实现
    // 期望：call_count == 1 + max_retries（如 max_retries=3，则 call_count=4）
    // 提示：succeed_on_attempt = -1（一直失败）
}

// TEST 4: 非 retryable 错误（CLIENT_ERROR），不重试
TEST(RetryableHttpClientTest, NonRetryableError) {
    // TODO: 你来实现
    // Mock 返回 CLIENT_ERROR (400)
    // 期望：call_count == 1（不重试）
}

// TEST 5: 重试期间被取消
TEST(RetryableHttpClientTest, CanceledDuringBackoff) {
    // TODO: 你来实现
    // 提示：在另一个线程延迟 500ms 后调用 cancel_token->cancel()
    // 期望：返回 CANCELED，总耗时 < initial_backoff (1000ms)
    // 示例代码结构：
    // auto cancel_token = std::make_shared<CancelToken>();
    // std::thread canceler([cancel_token]() {
    //     std::this_thread::sleep_for(500ms);
    //     cancel_token->cancel();
    // });
    // ... execute ...
    // canceler.join();
}

// TEST 6: 指数退避验证
TEST(RetryableHttpClientTest, ExponentialBackoff) {
    // TODO: 你来实现
    // Mock 一直失败，max_retries=3
    // 期望：
    //   - 第 1 次重试前 sleep ~1s
    //   - 第 2 次重试前 sleep ~2s
    //   - 第 3 次重试前 sleep ~4s
    // 可用时间戳验证各次调用间隔
    // 提示：用 vector<steady_clock::time_point> 记录每次 call 的时间
}

// TEST 7（可选加分）: cancel_token 为 nullptr
TEST(RetryableHttpClientTest, NullCancelToken) {
    // TODO: 期望正常执行，不崩溃
}
