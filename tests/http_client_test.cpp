#include <gtest/gtest.h>

#include "core/cancel_token.h"
#include "infra/http_client.h"

#include <chrono>
#include <thread>
#include <vector>

using namespace stv::infra;
using namespace stv::core;
using namespace std::chrono_literals;

namespace {

HttpRequest make_request() {
    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = "http://127.0.0.1:8765/test";
    request.trace_id = "trace-test-http-client";
    request.request_id = "req-test-http-client";
    request.timeout = 5s;
    return request;
}

class MockHttpClient : public IHttpClient {
public:
    int succeed_on_attempt = -1;  // -1 means always fail
    int call_count = 0;
    HttpErrorCode error_to_return = HttpErrorCode::NETWORK_ERROR;
    bool error_retryable = true;
    std::vector<std::chrono::steady_clock::time_point> call_timestamps;

    Result<HttpResponse, TaskError> execute(
        const HttpRequest&,
        std::shared_ptr<CancelToken>) override {
        call_timestamps.push_back(std::chrono::steady_clock::now());
        ++call_count;

        if (succeed_on_attempt > 0 && call_count == succeed_on_attempt) {
            HttpResponse response{};
            response.status_code = 200;
            response.body = "success";
            response.request_id = "resp-123";
            response.elapsed_ms = 10ms;
            return Result<HttpResponse, TaskError>::Ok(response);
        }

        return Result<HttpResponse, TaskError>::Err(
            make_http_error(error_to_return, "mock error", "mock internal", error_retryable));
    }

    bool cancel(const std::string&) override { return true; }
};

}  // namespace

TEST(RetryableHttpClientTest, SuccessOnFirstAttempt) {
    auto mock_client = std::make_shared<MockHttpClient>();
    mock_client->succeed_on_attempt = 1;

    RetryPolicy policy;
    policy.max_retries = 3;
    policy.initial_backoff = 100ms;
    policy.sleep_slice = 10ms;

    RetryableHttpClient client(mock_client, policy);
    auto result = client.execute(make_request());

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(mock_client->call_count, 1);
}

TEST(RetryableHttpClientTest, SuccessOnSecondAttempt) {
    auto mock_client = std::make_shared<MockHttpClient>();
    mock_client->succeed_on_attempt = 2;

    RetryPolicy policy;
    policy.max_retries = 3;
    policy.initial_backoff = 120ms;
    policy.sleep_slice = 10ms;

    RetryableHttpClient client(mock_client, policy);

    const auto start = std::chrono::steady_clock::now();
    auto result = client.execute(make_request());
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(mock_client->call_count, 2);
    EXPECT_GE(elapsed.count(), 100);
}

TEST(RetryableHttpClientTest, ExhaustedRetries) {
    auto mock_client = std::make_shared<MockHttpClient>();
    mock_client->succeed_on_attempt = -1;

    RetryPolicy policy;
    policy.max_retries = 3;
    policy.initial_backoff = 20ms;
    policy.sleep_slice = 5ms;

    RetryableHttpClient client(mock_client, policy);
    auto result = client.execute(make_request());

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(mock_client->call_count, 4);

    const auto& error = result.error();
    EXPECT_EQ(error.category, ErrorCategory::Network);
    EXPECT_EQ(error.details.at("retry_count"), "3");
    EXPECT_EQ(error.details.at("request_id"), "req-test-http-client");
}

TEST(RetryableHttpClientTest, NonRetryableError) {
    auto mock_client = std::make_shared<MockHttpClient>();
    mock_client->error_to_return = HttpErrorCode::CLIENT_ERROR;
    mock_client->error_retryable = false;

    RetryPolicy policy;
    policy.max_retries = 3;
    policy.initial_backoff = 50ms;
    policy.sleep_slice = 5ms;

    RetryableHttpClient client(mock_client, policy);
    auto result = client.execute(make_request());

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(mock_client->call_count, 1);
    EXPECT_EQ(result.error().category, ErrorCategory::Pipeline);
    EXPECT_EQ(result.error().details.at("retry_count"), "0");
}

TEST(RetryableHttpClientTest, CanceledDuringBackoff) {
    auto mock_client = std::make_shared<MockHttpClient>();

    RetryPolicy policy;
    policy.max_retries = 3;
    policy.initial_backoff = 400ms;
    policy.sleep_slice = 10ms;

    RetryableHttpClient client(mock_client, policy);
    auto cancel_token = CancelToken::create();

    std::thread canceler([cancel_token]() {
        std::this_thread::sleep_for(100ms);
        cancel_token->request_cancel();
    });

    const auto start = std::chrono::steady_clock::now();
    auto result = client.execute(make_request(), cancel_token);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    canceler.join();

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().category, ErrorCategory::Canceled);
    EXPECT_EQ(mock_client->call_count, 1);
    EXPECT_LT(elapsed.count(), 350);
}

TEST(RetryableHttpClientTest, ExponentialBackoff) {
    auto mock_client = std::make_shared<MockHttpClient>();

    RetryPolicy policy;
    policy.max_retries = 3;
    policy.initial_backoff = 60ms;
    policy.backoff_multiplier = 2.0;
    policy.max_backoff = 400ms;
    policy.sleep_slice = 5ms;

    RetryableHttpClient client(mock_client, policy);
    auto result = client.execute(make_request());

    ASSERT_TRUE(result.is_err());
    ASSERT_EQ(mock_client->call_count, 4);
    ASSERT_EQ(mock_client->call_timestamps.size(), 4u);

    const auto gap1 = std::chrono::duration_cast<std::chrono::milliseconds>(
        mock_client->call_timestamps[1] - mock_client->call_timestamps[0]);
    const auto gap2 = std::chrono::duration_cast<std::chrono::milliseconds>(
        mock_client->call_timestamps[2] - mock_client->call_timestamps[1]);
    const auto gap3 = std::chrono::duration_cast<std::chrono::milliseconds>(
        mock_client->call_timestamps[3] - mock_client->call_timestamps[2]);

    EXPECT_GE(gap1.count(), 45);
    EXPECT_GE(gap2.count(), 95);
    EXPECT_GE(gap3.count(), 190);
}

TEST(RetryableHttpClientTest, NullCancelToken) {
    auto mock_client = std::make_shared<MockHttpClient>();
    mock_client->succeed_on_attempt = 2;

    RetryPolicy policy;
    policy.max_retries = 3;
    policy.initial_backoff = 50ms;
    policy.sleep_slice = 5ms;

    RetryableHttpClient client(mock_client, policy);
    auto result = client.execute(make_request(), nullptr);

    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(mock_client->call_count, 2);
}
