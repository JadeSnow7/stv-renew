#pragma once
#include "core/result.h"
#include "core/task_error.h"
#include "core/cancel_token.h"
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <functional>

namespace stv::infra {

// HTTP 方法
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE
};

// HTTP 请求
struct HttpRequest {
    HttpMethod method;
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string trace_id;  // 可观测性：贯穿请求链路
    std::chrono::milliseconds timeout{30000};  // 默认 30s
};

// HTTP 响应
struct HttpResponse {
    int status_code;  // 200, 404, 500 etc.
    std::map<std::string, std::string> headers;
    std::string body;
    std::string request_id;  // 服务端返回的请求 ID（用于排查）
    std::chrono::milliseconds elapsed_ms;  // 实际耗时
};

// HTTP 错误分类（用于 TaskError 的 details）
enum class HttpErrorCode {
    NETWORK_ERROR = 1001,    // 网络不可达、DNS 失败、连接拒绝
    TIMEOUT = 1002,          // 超时（连接/请求/读取）
    CANCELED = 1003,         // 用户主动取消
    SERVER_ERROR = 1004,     // 5xx 服务端错误
    CLIENT_ERROR = 1005,     // 4xx 客户端错误（除 429）
    RATE_LIMIT = 1006,       // 429 限流
    PARSE_ERROR = 1007,      // 响应解析失败（如 JSON 格式错误）
    UNKNOWN = 1999
};

// 将 HTTP 错误码转为 TaskError
stv::core::TaskError make_http_error(
    HttpErrorCode code,
    const std::string& user_message,
    const std::string& internal_message,
    bool retryable = false
);

// IHttpClient 接口（纯虚类）
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    // M2 练习入口：统一 HTTP 基础方法
    // 注意：req.url/req.trace_id/req.timeout 的校验建议放到实现层完成。
    virtual stv::core::Result<HttpResponse, stv::core::TaskError> get(
        const HttpRequest& request,
        std::shared_ptr<stv::core::CancelToken> cancel_token = nullptr
    ) {
        HttpRequest req = request;
        req.method = HttpMethod::GET;
        return execute(req, std::move(cancel_token));
    }

    virtual stv::core::Result<HttpResponse, stv::core::TaskError> post(
        const HttpRequest& request,
        std::shared_ptr<stv::core::CancelToken> cancel_token = nullptr
    ) {
        HttpRequest req = request;
        req.method = HttpMethod::POST;
        return execute(req, std::move(cancel_token));
    }

    // 按 request_id 取消 in-flight 请求。
    // 返回 true 表示成功命中并触发取消；false 表示未命中或已完成。
    virtual bool cancel(const std::string& request_id) = 0;

    // 同步接口（简化版，M2 先用这个）
    // 返回 Result<HttpResponse, TaskError>
    virtual stv::core::Result<HttpResponse, stv::core::TaskError> execute(
        const HttpRequest& request,
        std::shared_ptr<stv::core::CancelToken> cancel_token = nullptr
    ) = 0;

    // TODO（可选，M3 异步调度时扩展）：
    // using ResponseCallback = std::function<void(stv::core::Result<HttpResponse, TaskError>)>;
    // virtual void execute_async(const HttpRequest& req, ResponseCallback callback) = 0;
};

// 重试策略配置
struct RetryPolicy {
    int max_retries = 3;  // 最多重试 3 次
    std::chrono::milliseconds initial_backoff{1000};  // 首次退避 1s
    double backoff_multiplier = 2.0;  // 指数退避因子
    std::chrono::milliseconds max_backoff{30000};  // 最大退避 30s

    // 判断是否应该重试（根据错误码）
    bool should_retry(HttpErrorCode code) const {
        return code == HttpErrorCode::NETWORK_ERROR ||
               code == HttpErrorCode::TIMEOUT ||
               code == HttpErrorCode::SERVER_ERROR ||
               code == HttpErrorCode::RATE_LIMIT;
    }
};

// 带重试的 HttpClient 装饰器
class RetryableHttpClient : public IHttpClient {
public:
    RetryableHttpClient(
        std::shared_ptr<IHttpClient> inner,
        RetryPolicy policy = {}
    );

    stv::core::Result<HttpResponse, stv::core::TaskError> execute(
        const HttpRequest& request,
        std::shared_ptr<stv::core::CancelToken> cancel_token = nullptr
    ) override;

    bool cancel(const std::string& request_id) override;

private:
    std::shared_ptr<IHttpClient> inner_;
    RetryPolicy policy_;
};

} // namespace stv::infra
