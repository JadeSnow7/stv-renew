# StoryToVideo Renew Server (Go/Gin)

This is the service-side implementation scaffold for the unified StoryToVideo plan.

## Features in this baseline

- JWT auth (`login`, `refresh`, `logout`, `me`)
- Project + storyboard CRUD
- Job orchestration with DAG stages:
  - `storyboard_generate`
  - `image_generate_*` (parallel)
  - `tts_generate_*` (parallel)
  - `compose_video`
- Cooperative cancel + retry endpoint
- SSE event stream with backlog replay (`Last-Event-ID`)
- Asset APIs and export APIs (signed URL placeholder)
- Unified response shape:
  - `{ "data": ..., "trace_id": "..." }`
  - `{ "error": { "code", "message", "retryable", "details" }, "trace_id": "..." }`

## Quick start

```bash
cd server
go mod tidy
go run ./cmd/stv-server
```

Windows (PowerShell):

```powershell
cd server
go mod tidy
go run .\cmd\stv-server
```

macOS/Linux:

```bash
cd server
go mod tidy
go run ./cmd/stv-server
```

Server listens on `:8080` by default.

Demo account:

- email: `demo@stv.local`
- password: `demo123456`

## Environment variables

- `STV_SERVER_ADDR` (default `:8080`)
- `STV_JWT_SECRET` (default `dev-change-me`)
- `STV_ACCESS_TTL` (default `15m`)
- `STV_REFRESH_TTL` (default `336h`)
- `STV_MAX_CONCURRENT_TASKS` (default `20`)
- `STV_MAX_USER_JOBS` (default `2`)
- `STV_MAX_SCENE_WORKERS` (default `6`)

## Notes

- Current persistence is in-memory for fast local integration. SQL migrations are provided under `migrations/`.
- Provider layer currently uses a mock adapter. Replace `internal/provider` with real LLM/Image/TTS/FFmpeg adapters.
- Signed download URL is placeholder logic for S3-compatible storage.
