#pragma once

#include "core/pipeline.h"
#include "infra/http_client.h"
#include <memory>
#include <string>

namespace stv::infra {

/// StoryboardStage - 调用服务端 /v1/storyboard 生成分镜脚本
class StoryboardStage : public core::IStage {
public:
  explicit StoryboardStage(
      std::shared_ptr<IHttpClient> http_client,
      const std::string &api_base_url = "http://127.0.0.1:8765");

  [[nodiscard]] std::string name() const override { return "StoryboardStage"; }

  core::Result<void, core::TaskError> execute(core::StageContext &ctx) override;

private:
  std::shared_ptr<IHttpClient> http_client_;
  std::string api_base_url_;
};

/// ImageGenStage - 调用服务端 /v1/imagegen 生成图像
class ImageGenStage : public core::IStage {
public:
  explicit ImageGenStage(
      std::shared_ptr<IHttpClient> http_client,
      const std::string &api_base_url = "http://127.0.0.1:8765");

  [[nodiscard]] std::string name() const override { return "ImageGenStage"; }

  core::Result<void, core::TaskError> execute(core::StageContext &ctx) override;

private:
  std::shared_ptr<IHttpClient> http_client_;
  std::string api_base_url_;
};

/// TtsStage - 调用服务端 /v1/tts 生成语音
class TtsStage : public core::IStage {
public:
  explicit TtsStage(
      std::shared_ptr<IHttpClient> http_client,
      const std::string &api_base_url = "http://127.0.0.1:8765");

  [[nodiscard]] std::string name() const override { return "TtsStage"; }

  core::Result<void, core::TaskError> execute(core::StageContext &ctx) override;

private:
  std::shared_ptr<IHttpClient> http_client_;
  std::string api_base_url_;
};

/// ComposeStage - 调用服务端 /v1/compose 合成最终视频
class ComposeStage : public core::IStage {
public:
  explicit ComposeStage(
      std::shared_ptr<IHttpClient> http_client,
      const std::string &api_base_url = "http://127.0.0.1:8765");

  [[nodiscard]] std::string name() const override { return "ComposeStage"; }

  core::Result<void, core::TaskError> execute(core::StageContext &ctx) override;

private:
  std::shared_ptr<IHttpClient> http_client_;
  std::string api_base_url_;
};

} // namespace stv::infra
