# StoryToVideo Renew

> AI 驱动的视频生成桌面应用 - 从文本故事到最终视频的全自动工作流

[![Project Status](https://img.shields.io/badge/Status-M2%20Complete-brightgreen)]()
[![License](https://img.shields.io/badge/License-MIT-blue)]()

---

## 🎯 项目概述

StoryToVideo Renew 是一个现代化的 AI 视频生成工具，通过清晰的分层架构将 Qt 客户端、C++ 核心调度器和 Python AI 服务端解耦。

**核心特性**:
- 📝 文本故事 → 分镜脚本（Storyboard）
- 🎨 分镜 → AI 图像生成（Stable Diffusion）
- 🔊 分镜 → 语音合成（TTS）
- 🎬 图像 + 音频 → 最终视频（FFmpeg）
- 🚀 支持本地 GPU 推理 + Mock 保底模式
- 🔄 可靠的重试机制和取消支持

---

## 🏗️ 架构设计

### 分层说明

```
┌─────────────────────────────────────────────────┐
│               app (Qt GUI)                      │  Presenter: 纯桥接层
├─────────────────────────────────────────────────┤
│          core (Orchestration Logic)             │  WorkflowEngine, Scheduler, Task
├────────────────────┬────────────────────────────┤
│   infra_base       │      infra_net             │  Logger / HTTP + Stages
│   (Logger)         │   (HTTP + Stages)          │
└────────────────────┴────────────────────────────┘
                         ↓ HTTP
         ┌─────────────────────────────────┐
         │   server (Python FastAPI)       │
         │   ├─ MockProvider               │
         │   ├─ LocalGpuProvider (SD1.5)   │
         │   └─ services/ffmpeg_compose    │
         └─────────────────────────────────┘
```

**依赖方向**:
- `app → core + infra`
- `infra_net → core + infra_base`
- `infra_base` 独立（无依赖）
- `core` 独立（零 UI、零服务端耦合）
- `server` 独立进程，通过 HTTP 通信

---

## 🚀 快速开始

### 1. 依赖检查

```bash
./scripts/check_dependencies.sh
```

**必需**:
- CMake >= 3.21
- C++17 编译器（g++/clang++）
- Python 3.8+
- libcurl-dev

**可选**:
- Qt6（用于 GUI）
- NVIDIA GPU + PyTorch（用于 GPU 推理）
- FFmpeg with NVENC（用于硬件加速编码）

### 2. 构建客户端（无 UI 模式）

```bash
mkdir build && cd build
cmake .. -DSTV_BUILD_APP=OFF -DSTV_BUILD_TESTS=ON
cmake --build . -j$(nproc)
ctest --output-on-failure
```

### 3. 启动服务端（Mock 模式）

```bash
cd server
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

export STV_PROVIDER=mock
export STV_OUTPUT_DIR=/tmp/stv-output
python3 -m app.main
```

服务端启动后访问 `http://127.0.0.1:8765/docs` 查看 API 文档。

### 4. 端到端测试

```bash
./scripts/test_e2e_mock.sh
```

### 5. 性能与稳定性基准（M2）

```bash
# Mock 基线 + 30% 故障注入
./scripts/bench_m2.py --provider mock --runs 30 --out bench.json --report docs/reports/M2_perf.md

# GPU 基线 + 30% 故障注入
./scripts/bench_m2.py --provider local_gpu --runs 30 --out bench_gpu.json --report docs/reports/M2_perf.md
```

---

## 🌐 跨平台构建

客户端支持 Windows / macOS / Linux，使用 CMake Presets：

```bash
# Linux (GCC)
cmake --preset linux-gcc && cmake --build --preset linux-gcc --parallel

# Windows (MSVC)
cmake --preset windows-msvc && cmake --build --preset windows-msvc --parallel

# macOS (Universal Binary)
cmake --preset macos-universal && cmake --build --preset macos-universal --parallel
```

详见 [`docs/CROSS_PLATFORM_COMPAT_PLAN.md`](docs/CROSS_PLATFORM_COMPAT_PLAN.md)

---

## 📦 目录结构

```
stv-renew/
├── app/                  # Qt 客户端（Presenter + QML）
│   ├── include/app/
│   └── src/
├── core/                 # 核心调度与编排（零依赖业务逻辑）
│   ├── include/core/
│   └── src/
├── infra/                # 基础设施层
│   ├── include/infra/
│   │   ├── logger.h      # 日志接口
│   │   ├── http_client.h # HTTP 客户端 + 重试
│   │   ├── stages.h      # 真实 Stage 实现
│   │   └── config.h      # XDG 路径管理
│   └── src/
├── server/               # Python AI 服务端（M2）
│   ├── app/
│   │   ├── main.py       # FastAPI 入口
│   │   ├── schemas.py    # API 数据模型
│   │   ├── providers/    # AI Provider（mock/local_gpu）
│   │   └── services/     # 业务服务（task_registry, ffmpeg_compose）
│   └── requirements.txt
├── tests/                # 单元测试 + 集成测试
├── scripts/              # 构建和测试脚本
└── docs/                 # 设计文档
    ├── M2_PROGRESS.md    # M2 进度总结
    ├── M2_ACCEPTANCE.md  # M2 验收文档
    └── OBSERVABILITY.md  # 可观测性指南
```

---

## 🎯 M2 阶段完成情况

> **状态**: ✅ **代码实现完成**（2026-02-20）

### 已完成

- [x] **Phase 0**: 修复循环依赖 + 依赖探测脚本
- [x] **Phase 1**: 服务端骨架（FastAPI + API v1 + Providers）
- [x] **Phase 2**: 客户端 Stage 实现（HTTP 调用服务端）
- [x] **Phase 3**: GPU Provider（SD 1.5 + NVENC）
- [x] **Phase 4**: 可靠性收口（XDG 路径 + 错误处理）
- [x] **Phase 5**: 测试脚本 + 验收文档

详见 [`docs/M2_ACCEPTANCE.md`](docs/M2_ACCEPTANCE.md)

---

## 📖 核心概念

### TaskError（统一错误处理）

```cpp
struct TaskError {
    ErrorCategory category;   // Network/Pipeline/Timeout/Canceled/Unknown
    int code;
    bool retryable;
    std::string user_message;
    std::string internal_message;
    std::map<std::string, std::string> details; // trace_id, request_id, retry_count
};
```

### RetryPolicy（重试策略）

```cpp
struct RetryPolicy {
    int max_retries = 2;
    std::chrono::milliseconds initial_backoff = 500ms;
    std::chrono::milliseconds max_backoff = 5000ms;
    double backoff_multiplier = 2.0;
};
```

### XDG 路径规范

- **配置**: `~/.config/stv-renew/`
- **缓存**: `~/.cache/stv-renew/`
- **数据**: `~/.local/share/stv-renew/`
- **输出**: `~/.local/share/stv-renew/outputs/`（可通过 `$STV_OUTPUT_DIR` 覆盖）

---

## 🛠️ 开发指南

### 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `STV_PROVIDER` | `mock` | Provider 模式：mock/local_gpu/cloud |
| `STV_API_BASE_URL` | `http://127.0.0.1:8765` | 服务端 API 地址 |
| `STV_OUTPUT_DIR` | `~/.local/share/stv-renew/outputs/` | 输出目录 |
| `STV_GPU_SLOTS` | `1` | GPU 并发槽位 |
| `STV_VRAM_LIMIT_GB` | `7.5` | VRAM 软限制（GB）|
| `STV_MAX_RETRIES` | `2` | HTTP 最大重试次数 |

### 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `STV_BUILD_APP` | `ON` | 是否构建 Qt 应用（需要 Qt6）|
| `STV_BUILD_TESTS` | `ON` | 是否构建测试 |
| `STV_ENABLE_NETWORK_TESTS` | `OFF` | 是否启用外网测试 |

### 运行测试

```bash
# 构建测试
./scripts/test_build.sh

# E2E 测试（Mock 模式）
./scripts/test_e2e_mock.sh

# 单元测试
cd build && ctest --output-on-failure
```

---

## 📊 性能指标（M2 目标）

| 指标 | 目标 | 备注 |
|------|------|------|
| 4 scene 端到端（Mock）| < 5s | 无 GPU |
| 4 scene 端到端（GPU）| P50 < 35s, P95 < 55s | SD 1.5 |
| 取消响应延迟 | P95 < 300ms | - |
| 连续运行成功率 | >= 97% (30 次) | - |

---

## 🗺️ 路线图

### M2（当前）: 全链路闭环 ✅

- [x] 服务端 API 与 Provider
- [x] 客户端 Stage 集成
- [x] GPU 推理与 NVENC
- [x] 可靠性与可观测性

### M3（计划）: 调度升级

- [ ] ThreadPoolScheduler（多线程）
- [ ] DAG 依赖解析
- [ ] ResourceBudget（CPU/RAM/VRAM）

### M4（计划）: 工程加固

- [ ] CI/CD（GitHub Actions）
- [ ] AppImage / 跨平台打包
- [ ] ASan/TSan 可选编译
- [ ] 性能回归门禁

---

## 📄 许可证

MIT License

---

## 📞 联系方式

- **Issues**: [GitHub Issues](https://github.com/JadeSnow7/stv-renew/issues)
- **文档**: [`docs/`](docs/)
