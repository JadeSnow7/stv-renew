#include <gtest/gtest.h>
#include "infra/curl_http_client.h"
#include "infra/http_client.h"
#include "core/cancel_token.h"
#include <thread>

using namespace stv::infra;
using namespace stv::core;

// 集成测试：真实 HTTP 请求（需要网络）
// 注意：这些测试依赖外部服务，可能不稳定

TEST(CurlHttpClientTest, SimpleGetRequest) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = "https://httpbin.org/get";  // 公开测试 API
    request.trace_id = "test-001";
    request.timeout = std::chrono::milliseconds(10000);

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_ok()) << "GET request failed: " << result.error().internal_message;
    EXPECT_EQ(result.value().status_code, 200);
    EXPECT_FALSE(result.value().body.empty());
}

TEST(CurlHttpClientTest, PostRequest) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::POST;
    request.url = "https://httpbin.org/post";
    request.body = R"({"test": "data"})";
    request.headers["Content-Type"] = "application/json";
    request.trace_id = "test-002";
    request.timeout = std::chrono::milliseconds(10000);

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().status_code, 200);
    EXPECT_NE(result.value().body.find("test"), std::string::npos);
}

TEST(CurlHttpClientTest, Timeout) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = "https://httpbin.org/delay/5";  // 延迟 5 秒响应
    request.trace_id = "test-003";
    request.timeout = std::chrono::milliseconds(1000);  // 1 秒超时

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_err());
    auto error = result.error();
    EXPECT_EQ(error.category, ErrorCategory::Timeout);
    EXPECT_TRUE(error.retryable);
}

TEST(CurlHttpClientTest, CancelRequest) {
    CurlHttpClient client;
    auto cancel_token = std::make_shared<CancelToken>();

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = "https://httpbin.org/delay/10";  // 延迟 10 秒
    request.trace_id = "test-004";
    request.timeout = std::chrono::milliseconds(30000);

    // 在另一个线程中延迟取消
    std::thread canceler([cancel_token]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        cancel_token->cancel();
    });

    auto start = std::chrono::steady_clock::now();
    auto result = client.execute(request, cancel_token);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    );

    canceler.join();

    ASSERT_TRUE(result.is_err());
    auto error = result.error();
    EXPECT_EQ(error.category, ErrorCategory::Canceled);
    EXPECT_LT(elapsed.count(), 2000);  // 应该在 2 秒内取消
}

TEST(CurlHttpClientTest, NotFoundError) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = "https://httpbin.org/status/404";
    request.trace_id = "test-005";
    request.timeout = std::chrono::milliseconds(10000);

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_err());
    auto error = result.error();
    EXPECT_EQ(error.category, ErrorCategory::Pipeline);  // CLIENT_ERROR 映射到 Pipeline
    EXPECT_FALSE(error.retryable);  // 4xx 不可重试
}

TEST(CurlHttpClientTest, ServerError) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = "https://httpbin.org/status/500";
    request.trace_id = "test-006";
    request.timeout = std::chrono::milliseconds(10000);

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_err());
    auto error = result.error();
    EXPECT_EQ(error.category, ErrorCategory::Network);  // SERVER_ERROR 映射到 Network
    EXPECT_TRUE(error.retryable);  // 5xx 可重试
}

// 集成测试：CurlHttpClient + RetryableHttpClient
TEST(CurlHttpClientTest, WithRetryDecorator) {
    auto inner = std::make_shared<CurlHttpClient>();
    RetryPolicy policy;
    policy.max_retries = 2;
    policy.initial_backoff = std::chrono::milliseconds(500);

    RetryableHttpClient retryable_client(inner, policy);

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = "https://httpbin.org/status/500";  // 模拟服务端错误
    request.trace_id = "test-007";
    request.timeout = std::chrono::milliseconds(5000);

    auto start = std::chrono::steady_clock::now();
    auto result = retryable_client.execute(request);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    );

    // 应该重试 2 次后失败
    ASSERT_TRUE(result.is_err());
    // 预期耗时：第1次 + sleep(500ms) + 第2次 + sleep(1000ms) + 第3次
    // 总共约 1.5 秒（取决于网络延迟）
    EXPECT_GT(elapsed.count(), 1000);  // 至少 1 秒（两次 sleep）
}
