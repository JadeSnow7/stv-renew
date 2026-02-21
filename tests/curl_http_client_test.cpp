#include <gtest/gtest.h>

#include "core/cancel_token.h"
#include "infra/curl_http_client.h"
#include "infra/http_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace stv::infra;
using namespace stv::core;
using namespace std::chrono_literals;

#ifndef STV_HAS_CURL
#define STV_HAS_CURL 0
#endif

namespace {

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string reason_phrase(int status) {
    switch (status) {
        case 200:
            return "OK";
        case 404:
            return "Not Found";
        case 429:
            return "Too Many Requests";
        case 500:
            return "Internal Server Error";
        default:
            return "Status";
    }
}

class LocalHttpServer {
public:
    LocalHttpServer() { start(); }
    ~LocalHttpServer() { stop(); }

    LocalHttpServer(const LocalHttpServer&) = delete;
    LocalHttpServer& operator=(const LocalHttpServer&) = delete;

    std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }
    bool ready() const { return start_error_.empty(); }
    const std::string& start_error() const { return start_error_; }

private:
    int listen_fd_ = -1;
    uint16_t port_ = 0;
    std::atomic<bool> stop_{false};
    std::thread worker_;
    std::string start_error_;

    void start() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            start_error_ = std::string("socket() failed: ") + std::strerror(errno);
            return;
        }

        int enable = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(0);

        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            start_error_ = std::string("bind() failed: ") + std::strerror(errno);
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }

        socklen_t addr_len = sizeof(addr);
        if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
            start_error_ = std::string("getsockname() failed: ") + std::strerror(errno);
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }
        port_ = ntohs(addr.sin_port);

        if (::listen(listen_fd_, 16) < 0) {
            start_error_ = std::string("listen() failed: ") + std::strerror(errno);
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }

        worker_ = std::thread([this] { run_loop(); });
    }

    void stop() {
        stop_.store(true);
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    static bool send_all(int fd, const std::string& data) {
        size_t sent = 0;
        while (sent < data.size()) {
            int flags = 0;
#ifdef MSG_NOSIGNAL
            flags = MSG_NOSIGNAL;
#endif
            const ssize_t n =
                ::send(fd, data.data() + sent, data.size() - sent, flags);
            if (n <= 0) {
                return false;
            }
            sent += static_cast<size_t>(n);
        }
        return true;
    }

    void send_response(int client_fd,
                       int status,
                       const std::string& body,
                       const std::string& request_id = "local-req") {
        std::ostringstream response;
        response << "HTTP/1.1 " << status << ' ' << reason_phrase(status) << "\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << body.size() << "\r\n";
        response << "X-Request-ID: " << request_id << "\r\n";
        response << "Connection: close\r\n\r\n";
        response << body;
        (void)send_all(client_fd, response.str());
    }

    void send_slow_stream(int client_fd) {
        constexpr size_t kChunkSize = 1024;
        constexpr int kChunkCount = 120;
        const std::string chunk(kChunkSize, 'a');

        std::ostringstream headers;
        headers << "HTTP/1.1 200 OK\r\n";
        headers << "Content-Type: application/octet-stream\r\n";
        headers << "Content-Length: " << (kChunkSize * kChunkCount) << "\r\n";
        headers << "X-Request-ID: local-stream\r\n";
        headers << "Connection: close\r\n\r\n";

        if (!send_all(client_fd, headers.str())) {
            return;
        }

        for (int i = 0; i < kChunkCount; ++i) {
            if (!send_all(client_fd, chunk)) {
                return;
            }
            std::this_thread::sleep_for(50ms);
        }
    }

    void handle_client(int client_fd) {
        std::string raw_request;
        char buffer[4096];
        while (raw_request.find("\r\n\r\n") == std::string::npos) {
            const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                return;
            }
            raw_request.append(buffer, static_cast<size_t>(n));
        }

        const size_t header_end = raw_request.find("\r\n\r\n");
        std::string headers = raw_request.substr(0, header_end + 4);
        std::string body = raw_request.substr(header_end + 4);

        std::istringstream request_stream(headers);
        std::string request_line;
        std::getline(request_stream, request_line);
        if (!request_line.empty() && request_line.back() == '\r') {
            request_line.pop_back();
        }

        std::string method;
        std::string path;
        std::string version;
        {
            std::istringstream line_stream(request_line);
            line_stream >> method >> path >> version;
        }

        size_t content_length = 0;
        const std::string content_length_tag = "Content-Length:";
        const size_t content_length_pos = headers.find(content_length_tag);
        if (content_length_pos != std::string::npos) {
            size_t value_start = content_length_pos + content_length_tag.size();
            while (value_start < headers.size() && headers[value_start] == ' ') {
                ++value_start;
            }
            size_t value_end = headers.find("\r\n", value_start);
            const std::string value = headers.substr(value_start, value_end - value_start);
            content_length = static_cast<size_t>(std::stoul(value));
        }

        while (body.size() < content_length) {
            const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                break;
            }
            body.append(buffer, static_cast<size_t>(n));
        }

        if (method == "GET" && path == "/get") {
            send_response(client_fd, 200, R"({"ok":true})", "local-get");
            return;
        }

        if (method == "POST" && path == "/post") {
            send_response(client_fd, 200, body.empty() ? R"({"ok":true})" : body, "local-post");
            return;
        }

        if (starts_with(path, "/delay/")) {
            const std::string delay_text = path.substr(std::strlen("/delay/"));
            int delay_ms = 0;
            try {
                delay_ms = std::stoi(delay_text);
            } catch (...) {
                delay_ms = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            send_response(client_fd, 200, R"({"delayed":true})", "local-delay");
            return;
        }

        if (path == "/slow-stream") {
            send_slow_stream(client_fd);
            return;
        }

        if (starts_with(path, "/status/")) {
            const std::string status_text = path.substr(std::strlen("/status/"));
            int status_code = 500;
            try {
                status_code = std::stoi(status_text);
            } catch (...) {
                status_code = 500;
            }
            send_response(client_fd, status_code, R"({"status":"custom"})", "local-status");
            return;
        }

        send_response(client_fd, 404, R"({"error":"not found"})", "local-404");
    }

    void run_loop() {
        while (!stop_.load()) {
            sockaddr_in client_addr{};
            socklen_t client_addr_len = sizeof(client_addr);
            const int client_fd =
                ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
            if (client_fd < 0) {
                if (stop_.load()) {
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                std::this_thread::sleep_for(5ms);
                continue;
            }

            handle_client(client_fd);
            ::close(client_fd);
        }
    }
};

class CurlHttpClientTest : public ::testing::Test {
protected:
    LocalHttpServer server_;

    void SetUp() override {
        if (!server_.ready()) {
            GTEST_SKIP() << "Loopback fixture unavailable: " << server_.start_error();
        }
    }
};

#if STV_HAS_CURL

TEST_F(CurlHttpClientTest, SimpleGetRequest) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = server_.base_url() + "/get";
    request.trace_id = "test-001";
    request.request_id = "req-001";
    request.timeout = 3s;

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_ok()) << result.error().internal_message;
    EXPECT_EQ(result.value().status_code, 200);
    EXPECT_NE(result.value().body.find("\"ok\""), std::string::npos);
}

TEST_F(CurlHttpClientTest, PostRequest) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::POST;
    request.url = server_.base_url() + "/post";
    request.body = R"({"test":"data"})";
    request.headers["Content-Type"] = "application/json";
    request.trace_id = "test-002";
    request.request_id = "req-002";
    request.timeout = 3s;

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_ok()) << result.error().internal_message;
    EXPECT_EQ(result.value().status_code, 200);
    EXPECT_NE(result.value().body.find("test"), std::string::npos);
}

TEST_F(CurlHttpClientTest, Timeout) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = server_.base_url() + "/delay/1200";
    request.trace_id = "test-003";
    request.request_id = "req-003";
    request.timeout = 200ms;

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().category, ErrorCategory::Timeout);
    EXPECT_TRUE(result.error().retryable);
}

TEST_F(CurlHttpClientTest, CancelRequest) {
    CurlHttpClient client;
    auto cancel_token = CancelToken::create();

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = server_.base_url() + "/slow-stream";
    request.trace_id = "test-004";
    request.request_id = "req-004";
    request.timeout = 30s;

    std::thread canceler([cancel_token]() {
        std::this_thread::sleep_for(200ms);
        cancel_token->request_cancel();
    });

    const auto start = std::chrono::steady_clock::now();
    auto result = client.execute(request, cancel_token);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    canceler.join();

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().category, ErrorCategory::Canceled);
    EXPECT_LT(elapsed.count(), 2000);
}

TEST_F(CurlHttpClientTest, NotFoundError) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = server_.base_url() + "/status/404";
    request.trace_id = "test-005";
    request.request_id = "req-005";
    request.timeout = 3s;

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().category, ErrorCategory::Pipeline);
    EXPECT_FALSE(result.error().retryable);
}

TEST_F(CurlHttpClientTest, ServerError) {
    CurlHttpClient client;

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = server_.base_url() + "/status/500";
    request.trace_id = "test-006";
    request.request_id = "req-006";
    request.timeout = 3s;

    auto result = client.execute(request);

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().category, ErrorCategory::Network);
    EXPECT_TRUE(result.error().retryable);
}

TEST_F(CurlHttpClientTest, WithRetryDecorator) {
    auto inner = std::make_shared<CurlHttpClient>();
    RetryPolicy policy;
    policy.max_retries = 2;
    policy.initial_backoff = 100ms;
    policy.max_backoff = 400ms;
    policy.sleep_slice = 10ms;

    RetryableHttpClient retry_client(inner, policy);

    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = server_.base_url() + "/status/500";
    request.trace_id = "test-007";
    request.request_id = "req-007";
    request.timeout = 2s;

    const auto start = std::chrono::steady_clock::now();
    auto result = retry_client.execute(request);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().category, ErrorCategory::Network);
    EXPECT_EQ(result.error().details.at("retry_count"), "2");
    EXPECT_GE(elapsed.count(), 250);
}

#else

TEST(CurlHttpClientTest, CurlDisabledAtBuildTime) {
    GTEST_SKIP() << "libcurl not available, skipping CurlHttpClient integration tests.";
}

#endif  // STV_HAS_CURL

}  // namespace
