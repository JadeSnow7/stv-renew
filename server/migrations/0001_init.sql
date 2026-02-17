-- +goose Up
CREATE TABLE IF NOT EXISTS users (
  id TEXT PRIMARY KEY,
  email TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,
  role TEXT NOT NULL,
  status TEXT NOT NULL,
  created_at TIMESTAMPTZ NOT NULL,
  updated_at TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS refresh_tokens (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id),
  token_hash TEXT NOT NULL,
  expires_at TIMESTAMPTZ NOT NULL,
  revoked_at TIMESTAMPTZ NULL,
  created_at TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS projects (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id),
  name TEXT NOT NULL,
  story_text TEXT NOT NULL,
  style TEXT NOT NULL,
  scene_count INT NOT NULL,
  status TEXT NOT NULL,
  current_job_id TEXT NULL,
  created_at TIMESTAMPTZ NOT NULL,
  updated_at TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS storyboard_scenes (
  id TEXT PRIMARY KEY,
  project_id TEXT NOT NULL REFERENCES projects(id),
  scene_index INT NOT NULL,
  narration TEXT NOT NULL DEFAULT '',
  prompt TEXT NOT NULL DEFAULT '',
  negative_prompt TEXT NOT NULL DEFAULT '',
  duration_ms INT NOT NULL DEFAULT 3000,
  transition TEXT NOT NULL DEFAULT 'cut',
  locked BOOLEAN NOT NULL DEFAULT FALSE,
  updated_at TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS assets (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id),
  project_id TEXT NOT NULL REFERENCES projects(id),
  type TEXT NOT NULL,
  status TEXT NOT NULL,
  storage_key TEXT NOT NULL,
  mime_type TEXT NOT NULL,
  size_bytes BIGINT NOT NULL,
  width INT NULL,
  height INT NULL,
  duration_ms INT NULL,
  sha256 TEXT NULL,
  metadata_json JSONB NOT NULL DEFAULT '{}'::jsonb,
  expires_at TIMESTAMPTZ NOT NULL,
  created_at TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS jobs (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id),
  project_id TEXT NOT NULL REFERENCES projects(id),
  status TEXT NOT NULL,
  progress DOUBLE PRECISION NOT NULL,
  cancel_requested BOOLEAN NOT NULL DEFAULT FALSE,
  error_code TEXT NULL,
  error_message TEXT NULL,
  retryable BOOLEAN NOT NULL DEFAULT FALSE,
  trace_id TEXT NOT NULL,
  created_at TIMESTAMPTZ NOT NULL,
  started_at TIMESTAMPTZ NULL,
  ended_at TIMESTAMPTZ NULL
);

CREATE TABLE IF NOT EXISTS job_tasks (
  id TEXT PRIMARY KEY,
  job_id TEXT NOT NULL REFERENCES jobs(id),
  task_key TEXT NOT NULL,
  task_type TEXT NOT NULL,
  status TEXT NOT NULL,
  attempt INT NOT NULL DEFAULT 0,
  max_attempt INT NOT NULL DEFAULT 3,
  depends_on_json JSONB NOT NULL DEFAULT '[]'::jsonb,
  input_json JSONB NOT NULL DEFAULT '{}'::jsonb,
  output_json JSONB NOT NULL DEFAULT '{}'::jsonb,
  error_code TEXT NULL,
  error_message TEXT NULL,
  retryable BOOLEAN NOT NULL DEFAULT FALSE,
  next_run_at TIMESTAMPTZ NULL,
  started_at TIMESTAMPTZ NULL,
  ended_at TIMESTAMPTZ NULL,
  worker_id TEXT NULL
);

CREATE TABLE IF NOT EXISTS job_events (
  id BIGSERIAL PRIMARY KEY,
  job_id TEXT NOT NULL REFERENCES jobs(id),
  seq BIGINT NOT NULL,
  event_type TEXT NOT NULL,
  payload_json JSONB NOT NULL DEFAULT '{}'::jsonb,
  created_at TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS exports (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id),
  project_id TEXT NOT NULL REFERENCES projects(id),
  status TEXT NOT NULL,
  asset_id TEXT NULL,
  download_url TEXT NULL,
  created_at TIMESTAMPTZ NOT NULL,
  updated_at TIMESTAMPTZ NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_jobs_user_status_created ON jobs(user_id, status, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_assets_project_type_created ON assets(project_id, type, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_job_tasks_job_status_next_run ON job_tasks(job_id, status, next_run_at);

-- +goose Down
DROP TABLE IF EXISTS exports;
DROP TABLE IF EXISTS job_events;
DROP TABLE IF EXISTS job_tasks;
DROP TABLE IF EXISTS jobs;
DROP TABLE IF EXISTS assets;
DROP TABLE IF EXISTS storyboard_scenes;
DROP TABLE IF EXISTS projects;
DROP TABLE IF EXISTS refresh_tokens;
DROP TABLE IF EXISTS users;
