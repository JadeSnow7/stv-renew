#include "infra/stages.h"
#include "core/task_error.h"
#include <sstream>
#include <regex>
#include <cstdlib>

namespace stv::infra {

namespace {

/// 简单的 JSON 字段提取（临时实现，M3 应替换为正式 JSON 库）
std::string extract_json_string(const std::string &json, const std::string &key) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1) {
        return match[1].str();
    }
    return "";
}

int extract_json_int(const std::string &json, const std::string &key) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*(\\d+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1) {
        return std::stoi(match[1].str());
    }
    return 0;
}

float extract_json_float(const std::string &json, const std::string &key) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*([0-9.]+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1) {
        return std::stof(match[1].str());
    }
    return 0.0f;
}

/// 生成 UUID（简化版）
std::string generate_request_id() {
    static int counter = 0;
    std::ostringstream oss;
    oss << "req-" << std::chrono::steady_clock::now().time_since_epoch().count() 
        << "-" << (++counter);
    return oss.str();
}

} // namespace

// ========== StoryboardStage ==========

StoryboardStage::StoryboardStage(
    std::shared_ptr<IHttpClient> http_client,
    const std::string &api_base_url)
    : http_client_(std::move(http_client)), api_base_url_(api_base_url) {}

core::Result<void, core::TaskError> StoryboardStage::execute(core::StageContext &ctx) {
    // 输入：story_text, target_duration, scene_count
    auto story_text = ctx.get_input<std::string>("story_text", "");
    auto target_duration = ctx.get_input<float>("target_duration", 30.0f);
    auto scene_count = ctx.get_input<int>("scene_count", 4);

    if (story_text.empty()) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Pipeline, 1, false,
                          "Missing story_text", "StoryboardStage: story_text is empty", {}));
    }

    // 构造请求
    HttpRequest request;
    request.method = HttpMethod::POST;
    request.url = api_base_url_ + "/v1/storyboard";
    request.trace_id = ctx.trace_id;
    request.request_id = generate_request_id();
    request.headers["Content-Type"] = "application/json";
    request.timeout = std::chrono::milliseconds(30000);

    // 构造 JSON body（手动拼接，M3 应使用 JSON 库）
    std::ostringstream body;
    body << "{"
         << "\"trace_id\":\"" << ctx.trace_id << "\","
         << "\"request_id\":\"" << request.request_id << "\","
         << "\"story_text\":\"" << story_text << "\","
         << "\"target_duration\":" << target_duration << ","
         << "\"scene_count\":" << scene_count
         << "}";
    request.body = body.str();

    // 执行请求
    ctx.on_progress(0.3f);
    auto result = http_client_->execute(request, ctx.cancel_token);

    if (result.is_err()) {
        return core::Result<void, core::TaskError>::Err(result.error());
    }

    auto response = result.value();
    if (response.status_code != 200) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Network, response.status_code, true,
                          "Server error", "StoryboardStage: HTTP " + std::to_string(response.status_code), {}));
    }

    // 解析响应（简化）
    ctx.on_progress(0.8f);
    
    // 提取 scenes 数组（这里简化处理，只提取总时长）
    auto total_duration = extract_json_float(response.body, "total_duration");
    
    // 输出：storyboard_json（原始 JSON）
    ctx.set_output<std::string>("storyboard_json", response.body);
    ctx.set_output<float>("total_duration", total_duration);
    ctx.set_output<int>("scene_count", scene_count);

    ctx.on_progress(1.0f);
    return core::Result<void, core::TaskError>::Ok();
}

// ========== ImageGenStage ==========

ImageGenStage::ImageGenStage(
    std::shared_ptr<IHttpClient> http_client,
    const std::string &api_base_url)
    : http_client_(std::move(http_client)), api_base_url_(api_base_url) {}

core::Result<void, core::TaskError> ImageGenStage::execute(core::StageContext &ctx) {
    // 输入：prompt, width, height, num_inference_steps
    auto prompt = ctx.get_input<std::string>("prompt", "");
    auto width = ctx.get_input<int>("width", 512);
    auto height = ctx.get_input<int>("height", 512);
    auto steps = ctx.get_input<int>("num_inference_steps", 20);

    if (prompt.empty()) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Pipeline, 1, false,
                          "Missing prompt", "ImageGenStage: prompt is empty", {}));
    }

    HttpRequest request;
    request.method = HttpMethod::POST;
    request.url = api_base_url_ + "/v1/imagegen";
    request.trace_id = ctx.trace_id;
    request.request_id = generate_request_id();
    request.headers["Content-Type"] = "application/json";
    request.timeout = std::chrono::milliseconds(120000); // 图像生成可能较慢

    std::ostringstream body;
    body << "{"
         << "\"trace_id\":\"" << ctx.trace_id << "\","
         << "\"request_id\":\"" << request.request_id << "\","
         << "\"prompt\":\"" << prompt << "\","
         << "\"width\":" << width << ","
         << "\"height\":" << height << ","
         << "\"num_inference_steps\":" << steps
         << "}";
    request.body = body.str();

    ctx.on_progress(0.2f);
    auto result = http_client_->execute(request, ctx.cancel_token);

    if (result.is_err()) {
        return core::Result<void, core::TaskError>::Err(result.error());
    }

    auto response = result.value();
    if (response.status_code != 200) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Network, response.status_code, true,
                          "Server error", "ImageGenStage: HTTP " + std::to_string(response.status_code), {}));
    }

    ctx.on_progress(0.9f);
    
    // 提取 image_path
    auto image_path = extract_json_string(response.body, "image_path");
    if (image_path.empty()) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Pipeline, 2, false,
                          "Invalid response", "ImageGenStage: missing image_path in response", {}));
    }

    ctx.set_output<std::string>("image_path", image_path);
    ctx.on_progress(1.0f);
    return core::Result<void, core::TaskError>::Ok();
}

// ========== TtsStage ==========

TtsStage::TtsStage(
    std::shared_ptr<IHttpClient> http_client,
    const std::string &api_base_url)
    : http_client_(std::move(http_client)), api_base_url_(api_base_url) {}

core::Result<void, core::TaskError> TtsStage::execute(core::StageContext &ctx) {
    // 输入：text, voice, speed
    auto text = ctx.get_input<std::string>("text", "");
    auto voice = ctx.get_input<std::string>("voice", "default");
    auto speed = ctx.get_input<float>("speed", 1.0f);

    if (text.empty()) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Pipeline, 1, false,
                          "Missing text", "TtsStage: text is empty", {}));
    }

    HttpRequest request;
    request.method = HttpMethod::POST;
    request.url = api_base_url_ + "/v1/tts";
    request.trace_id = ctx.trace_id;
    request.request_id = generate_request_id();
    request.headers["Content-Type"] = "application/json";
    request.timeout = std::chrono::milliseconds(60000);

    std::ostringstream body;
    body << "{"
         << "\"trace_id\":\"" << ctx.trace_id << "\","
         << "\"request_id\":\"" << request.request_id << "\","
         << "\"text\":\"" << text << "\","
         << "\"voice\":\"" << voice << "\","
         << "\"speed\":" << speed
         << "}";
    request.body = body.str();

    ctx.on_progress(0.3f);
    auto result = http_client_->execute(request, ctx.cancel_token);

    if (result.is_err()) {
        return core::Result<void, core::TaskError>::Err(result.error());
    }

    auto response = result.value();
    if (response.status_code != 200) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Network, response.status_code, true,
                          "Server error", "TtsStage: HTTP " + std::to_string(response.status_code), {}));
    }

    ctx.on_progress(0.9f);
    
    auto audio_path = extract_json_string(response.body, "audio_path");
    auto duration = extract_json_float(response.body, "duration_seconds");

    if (audio_path.empty()) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Pipeline, 2, false,
                          "Invalid response", "TtsStage: missing audio_path in response", {}));
    }

    ctx.set_output<std::string>("audio_path", audio_path);
    ctx.set_output<float>("duration_seconds", duration);
    ctx.on_progress(1.0f);
    return core::Result<void, core::TaskError>::Ok();
}

// ========== ComposeStage ==========

ComposeStage::ComposeStage(
    std::shared_ptr<IHttpClient> http_client,
    const std::string &api_base_url)
    : http_client_(std::move(http_client)), api_base_url_(api_base_url) {}

core::Result<void, core::TaskError> ComposeStage::execute(core::StageContext &ctx) {
    // 输入：scenes（JSON 数组字符串），output_path
    auto scenes_json = ctx.get_input<std::string>("scenes_json", "");
    auto output_path = ctx.get_input<std::string>("output_path", "/tmp/output.mp4");

    if (scenes_json.empty()) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Pipeline, 1, false,
                          "Missing scenes", "ComposeStage: scenes_json is empty", {}));
    }

    HttpRequest request;
    request.method = HttpMethod::POST;
    request.url = api_base_url_ + "/v1/compose";
    request.trace_id = ctx.trace_id;
    request.request_id = generate_request_id();
    request.headers["Content-Type"] = "application/json";
    request.timeout = std::chrono::milliseconds(300000); // 视频合成可能很慢

    std::ostringstream body;
    body << "{"
         << "\"trace_id\":\"" << ctx.trace_id << "\","
         << "\"request_id\":\"" << request.request_id << "\","
         << "\"scenes\":" << scenes_json << ","
         << "\"output_path\":\"" << output_path << "\","
         << "\"fps\":24"
         << "}";
    request.body = body.str();

    ctx.on_progress(0.2f);
    auto result = http_client_->execute(request, ctx.cancel_token);

    if (result.is_err()) {
        return core::Result<void, core::TaskError>::Err(result.error());
    }

    auto response = result.value();
    if (response.status_code != 200) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Network, response.status_code, true,
                          "Server error", "ComposeStage: HTTP " + std::to_string(response.status_code), {}));
    }

    ctx.on_progress(0.9f);
    
    auto video_path = extract_json_string(response.body, "video_path");
    auto duration = extract_json_float(response.body, "duration_seconds");

    if (video_path.empty()) {
        return core::Result<void, core::TaskError>::Err(
            core::TaskError(core::ErrorCategory::Pipeline, 2, false,
                          "Invalid response", "ComposeStage: missing video_path in response", {}));
    }

    ctx.set_output<std::string>("video_path", video_path);
    ctx.set_output<float>("duration_seconds", duration);
    ctx.on_progress(1.0f);
    return core::Result<void, core::TaskError>::Ok();
}

} // namespace stv::infra
