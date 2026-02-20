#pragma once

#include "core/pipeline.h"
#include "core/task.h"
#include "infra/http_client.h"
#include "infra/stages.h"
#include <memory>
#include <string>

namespace stv::infra {

/// 创建真实的 Stage Factory
/// 根据 TaskType 返回对应的 Stage 实现（通过 HTTP 调用服务端）
class StageFactory {
public:
    explicit StageFactory(
        std::shared_ptr<IHttpClient> http_client,
        const std::string &api_base_url = "http://127.0.0.1:8765")
        : http_client_(std::move(http_client)), api_base_url_(api_base_url) {}

    std::shared_ptr<core::IStage> create_stage(core::TaskType type) {
        switch (type) {
        case core::TaskType::Storyboard:
            return std::make_shared<StoryboardStage>(http_client_, api_base_url_);
        case core::TaskType::ImageGen:
            return std::make_shared<ImageGenStage>(http_client_, api_base_url_);
        case core::TaskType::TTS:
            return std::make_shared<TtsStage>(http_client_, api_base_url_);
        case core::TaskType::Compose:
            return std::make_shared<ComposeStage>(http_client_, api_base_url_);
        case core::TaskType::VideoClip:
            // VideoClip 暂不实现（M2 不需要）
            return nullptr;
        }
        return nullptr;
    }

private:
    std::shared_ptr<IHttpClient> http_client_;
    std::string api_base_url_;
};

} // namespace stv::infra
