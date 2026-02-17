#pragma once

#include "core/remote/dto.h"
#include "core/result.h"
#include "infra/http_client.h"

#include <memory>
#include <string>
#include <vector>

namespace stv::infra {

struct ApiError {
  int http_status = 0;
  std::string code;
  bool retryable = false;
  std::string message;
  std::string trace_id;
};

struct AuthTokens {
  std::string access_token;
  std::string refresh_token;
  int expires_in_sec = 0;
};

class IBackendApi {
public:
  virtual ~IBackendApi() = default;

  virtual stv::core::Result<AuthTokens, ApiError>
  login(const std::string &email, const std::string &password) = 0;

  virtual stv::core::Result<void, ApiError> refresh() = 0;

  virtual stv::core::Result<stv::core::remote::ProjectDto, ApiError>
  create_project(const stv::core::remote::CreateProjectReq &req) = 0;

  virtual stv::core::Result<std::vector<stv::core::remote::ProjectDto>, ApiError>
  list_projects(int page, int page_size) = 0;

  virtual stv::core::Result<stv::core::remote::StoryboardDto, ApiError>
  get_storyboard(const std::string &project_id) = 0;

  virtual stv::core::Result<stv::core::remote::StoryboardDto, ApiError>
  update_storyboard(const std::string &project_id,
                    const stv::core::remote::StoryboardPatchReq &req) = 0;

  virtual stv::core::Result<stv::core::remote::JobDto, ApiError>
  start_job(const std::string &project_id) = 0;

  virtual stv::core::Result<void, ApiError>
  cancel_job(const std::string &job_id) = 0;

  virtual stv::core::Result<void, ApiError>
  retry_job(const std::string &job_id) = 0;

  virtual stv::core::Result<std::vector<stv::core::remote::AssetDto>, ApiError>
  list_assets(const stv::core::remote::AssetQuery &query) = 0;

  virtual stv::core::Result<stv::core::remote::ExportDto, ApiError>
  export_video(const std::string &project_id) = 0;
};

/// HttpBackendApi is the default remote implementation.
/// Current baseline provides interface wiring and transport invocation skeleton.
/// JSON binding is intentionally incremental and can be extended per endpoint.
class HttpBackendApi final : public IBackendApi {
public:
  HttpBackendApi(std::shared_ptr<IHttpClient> http_client,
                 std::string base_url);

  void set_tokens(const AuthTokens &tokens);
  [[nodiscard]] const AuthTokens &tokens() const { return tokens_; }

  stv::core::Result<AuthTokens, ApiError>
  login(const std::string &email, const std::string &password) override;

  stv::core::Result<void, ApiError> refresh() override;

  stv::core::Result<stv::core::remote::ProjectDto, ApiError>
  create_project(const stv::core::remote::CreateProjectReq &req) override;

  stv::core::Result<std::vector<stv::core::remote::ProjectDto>, ApiError>
  list_projects(int page, int page_size) override;

  stv::core::Result<stv::core::remote::StoryboardDto, ApiError>
  get_storyboard(const std::string &project_id) override;

  stv::core::Result<stv::core::remote::StoryboardDto, ApiError>
  update_storyboard(const std::string &project_id,
                    const stv::core::remote::StoryboardPatchReq &req) override;

  stv::core::Result<stv::core::remote::JobDto, ApiError>
  start_job(const std::string &project_id) override;

  stv::core::Result<void, ApiError>
  cancel_job(const std::string &job_id) override;

  stv::core::Result<void, ApiError> retry_job(const std::string &job_id) override;

  stv::core::Result<std::vector<stv::core::remote::AssetDto>, ApiError>
  list_assets(const stv::core::remote::AssetQuery &query) override;

  stv::core::Result<stv::core::remote::ExportDto, ApiError>
  export_video(const std::string &project_id) override;

private:
  std::shared_ptr<IHttpClient> http_client_;
  std::string base_url_;
  AuthTokens tokens_;

  stv::core::Result<void, ApiError> ensure_http_client() const;
  std::string join_url(const std::string &path) const;
  ApiError make_unimplemented_error(const std::string &api_name) const;
};

} // namespace stv::infra
