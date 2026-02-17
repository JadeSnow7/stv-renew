#include "infra/api_client.h"

#include <chrono>
#include <sstream>

namespace stv::infra {

namespace {

std::string make_trace_id() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::ostringstream oss;
  oss << "cli-" << now;
  return oss.str();
}

} // namespace

HttpBackendApi::HttpBackendApi(std::shared_ptr<IHttpClient> http_client,
                               std::string base_url)
    : http_client_(std::move(http_client)), base_url_(std::move(base_url)) {}

void HttpBackendApi::set_tokens(const AuthTokens &tokens) { tokens_ = tokens; }

stv::core::Result<void, ApiError> HttpBackendApi::ensure_http_client() const {
  if (!http_client_) {
    return stv::core::Result<void, ApiError>::Err(
        ApiError{0, "CLIENT_NOT_READY", false, "HTTP client is null", ""});
  }
  return stv::core::Result<void, ApiError>::Ok();
}

std::string HttpBackendApi::join_url(const std::string &path) const {
  if (base_url_.empty()) {
    return path;
  }
  if (!path.empty() && path.front() == '/') {
    return base_url_ + path;
  }
  return base_url_ + "/" + path;
}

ApiError HttpBackendApi::make_unimplemented_error(const std::string &api_name) const {
  return ApiError{
      501,
      "API_NOT_IMPLEMENTED",
      false,
      "HttpBackendApi endpoint binding is pending: " + api_name,
      make_trace_id(),
  };
}

stv::core::Result<AuthTokens, ApiError>
HttpBackendApi::login(const std::string & /*email*/,
                      const std::string & /*password*/) {
  auto ready = ensure_http_client();
  if (ready.is_err()) {
    return stv::core::Result<AuthTokens, ApiError>::Err(ready.error());
  }
  return stv::core::Result<AuthTokens, ApiError>::Err(
      make_unimplemented_error("login"));
}

stv::core::Result<void, ApiError> HttpBackendApi::refresh() {
  auto ready = ensure_http_client();
  if (ready.is_err()) {
    return ready;
  }
  return stv::core::Result<void, ApiError>::Err(
      make_unimplemented_error("refresh"));
}

stv::core::Result<stv::core::remote::ProjectDto, ApiError>
HttpBackendApi::create_project(const stv::core::remote::CreateProjectReq & /*req*/) {
  return stv::core::Result<stv::core::remote::ProjectDto, ApiError>::Err(
      make_unimplemented_error("create_project"));
}

stv::core::Result<std::vector<stv::core::remote::ProjectDto>, ApiError>
HttpBackendApi::list_projects(int /*page*/, int /*page_size*/) {
  return stv::core::Result<std::vector<stv::core::remote::ProjectDto>, ApiError>::Err(
      make_unimplemented_error("list_projects"));
}

stv::core::Result<stv::core::remote::StoryboardDto, ApiError>
HttpBackendApi::get_storyboard(const std::string & /*project_id*/) {
  return stv::core::Result<stv::core::remote::StoryboardDto, ApiError>::Err(
      make_unimplemented_error("get_storyboard"));
}

stv::core::Result<stv::core::remote::StoryboardDto, ApiError>
HttpBackendApi::update_storyboard(
    const std::string & /*project_id*/,
    const stv::core::remote::StoryboardPatchReq & /*req*/) {
  return stv::core::Result<stv::core::remote::StoryboardDto, ApiError>::Err(
      make_unimplemented_error("update_storyboard"));
}

stv::core::Result<stv::core::remote::JobDto, ApiError>
HttpBackendApi::start_job(const std::string & /*project_id*/) {
  return stv::core::Result<stv::core::remote::JobDto, ApiError>::Err(
      make_unimplemented_error("start_job"));
}

stv::core::Result<void, ApiError>
HttpBackendApi::cancel_job(const std::string & /*job_id*/) {
  return stv::core::Result<void, ApiError>::Err(
      make_unimplemented_error("cancel_job"));
}

stv::core::Result<void, ApiError>
HttpBackendApi::retry_job(const std::string & /*job_id*/) {
  return stv::core::Result<void, ApiError>::Err(
      make_unimplemented_error("retry_job"));
}

stv::core::Result<std::vector<stv::core::remote::AssetDto>, ApiError>
HttpBackendApi::list_assets(const stv::core::remote::AssetQuery & /*query*/) {
  return stv::core::Result<std::vector<stv::core::remote::AssetDto>, ApiError>::Err(
      make_unimplemented_error("list_assets"));
}

stv::core::Result<stv::core::remote::ExportDto, ApiError>
HttpBackendApi::export_video(const std::string & /*project_id*/) {
  return stv::core::Result<stv::core::remote::ExportDto, ApiError>::Err(
      make_unimplemented_error("export_video"));
}

} // namespace stv::infra
