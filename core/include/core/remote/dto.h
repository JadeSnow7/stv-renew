#pragma once

#include <map>
#include <string>
#include <vector>

namespace stv::core::remote {

struct SceneDto {
  std::string id;
  int scene_index = 0;
  std::string narration;
  std::string prompt;
  std::string negative_prompt;
  int duration_ms = 3000;
  std::string transition = "cut";
  bool locked = false;
};

struct StoryboardDto {
  std::string project_id;
  std::vector<SceneDto> scenes;
};

struct CreateProjectReq {
  std::string name;
  std::string story_text;
  std::string style;
  int scene_count = 4;
};

struct ProjectDto {
  std::string id;
  std::string user_id;
  std::string name;
  std::string story_text;
  std::string style;
  int scene_count = 0;
  std::string status;
  std::string current_job_id;
  std::string created_at;
  std::string updated_at;
};

struct StoryboardPatchReq {
  std::vector<SceneDto> scenes;
};

struct JobDto {
  std::string id;
  std::string user_id;
  std::string project_id;
  std::string status;
  double progress = 0.0;
  bool cancel_requested = false;
  std::string error_code;
  std::string error_message;
  bool retryable = false;
  std::string trace_id;
  std::string created_at;
  std::string started_at;
  std::string ended_at;
};

struct AssetDto {
  std::string id;
  std::string user_id;
  std::string project_id;
  std::string type;
  std::string status;
  std::string storage_key;
  std::string mime_type;
  long long size_bytes = 0;
  int duration_ms = 0;
  std::map<std::string, std::string> metadata;
  std::string created_at;
};

struct AssetQuery {
  std::string project_id;
  std::string type;
  int page = 1;
  int page_size = 20;
};

struct ExportDto {
  std::string id;
  std::string user_id;
  std::string project_id;
  std::string status;
  std::string asset_id;
  std::string download_url;
  std::string created_at;
  std::string updated_at;
};

struct JobEvent {
  std::string event_id;
  long long seq = 0;
  std::string trace_id;
  std::string job_id;
  std::string project_id;
  std::string type;
  std::string ts;
  std::map<std::string, std::string> payload;
};

} // namespace stv::core::remote
