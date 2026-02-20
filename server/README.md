# StoryToVideo Server

本目录包含两套服务端实现，可根据需求选择。

---

## Python FastAPI 服务端（M2 当前实现）

本地 AI 视频生成服务端，支持多种 provider 模式。

### Provider 模式

- **Mock**：快速返回模拟数据，不依赖 GPU。用于开发/测试/CI。
- **Local GPU**：使用本地 GPU 推理（torch + diffusers），推荐 8GB+ VRAM。

### 快速开始

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

export STV_PROVIDER=mock
export STV_OUTPUT_DIR=/tmp/stv-output
python -m app.main
```

服务默认监听 `http://127.0.0.1:8765`。API 文档：`http://127.0.0.1:8765/docs`

### API Endpoints

- `GET /healthz` - 健康检查
- `POST /v1/storyboard` - 生成分镜脚本
- `POST /v1/imagegen` - 生成图像
- `POST /v1/tts` - 生成语音
- `POST /v1/compose` - 合成视频
- `POST /v1/cancel/{request_id}` - 取消任务

### 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `STV_PROVIDER` | `mock` | Provider 模式：mock/local_gpu |
| `STV_HOST` | `127.0.0.1` | 监听地址 |
| `STV_PORT` | `8765` | 监听端口 |
| `STV_OUTPUT_DIR` | `/tmp/stv-output` | 输出目录 |
| `STV_GPU_SLOTS` | `1` | GPU 并发槽位 |
| `STV_VRAM_LIMIT_GB` | `7.5` | VRAM 软限制（GB）|

---

## Go/Gin 服务端（跨平台后端骨架）

统一跨平台服务端，包含完整 JWT 认证、项目管理、作业调度和 SSE 事件流。

### 功能

- JWT auth（login/refresh/logout/me）
- Project + storyboard CRUD
- Job 调度（DAG stages: storyboard → image × N → tts × N → compose）
- SSE 事件流（带 backlog replay）
- 合作取消 + 重试

### 快速开始

```bash
go mod tidy
go run ./cmd/stv-server
```

默认监听 `:8080`。Demo 账号：`demo@stv.local` / `demo123456`

### 环境变量

- `STV_SERVER_ADDR` (default `:8080`)
- `STV_JWT_SECRET` (default `dev-change-me`)
- `STV_MAX_CONCURRENT_TASKS` (default `20`)
