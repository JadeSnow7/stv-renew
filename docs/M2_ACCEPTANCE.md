# M2 阶段验收文档

## 验收标准概述

M2 阶段目标：实现"Qt 客户端 + C++ 核心 + 本地服务端 + FFmpeg 输出"全链路通畅、可测、可量化。

## 当前实测记录（2026-02-20）

| 项目 | 结果 | 备注 |
|------|------|------|
| `./scripts/check_dependencies.sh` | ✅ 通过 | Qt6、nvidia-smi 为可选警告 |
| `./scripts/test_build.sh` | ✅ 通过 | 50 tests: 43 passed + 7 skipped（沙箱禁用 loopback socket） |
| `./scripts/test_e2e_mock.sh` | ✅ 通过 | 生成有效 mp4 输出 |
| `./scripts/bench_m2.py --provider mock` | ✅ Gate PASS | `bench.json` + `docs/reports/M2_perf.md` |
| `./scripts/bench_m2.py --provider local_gpu` | ✅ Gate PASS* | `bench_gpu.json` + `docs/reports/M2_perf_gpu.md` |

\* 当前环境未安装 `torch/diffusers`，`local_gpu` provider 实际走降级路径（mock 图像生成 + NVENC/ffmpeg 验证）。真实 GPU 推理指标需在安装 `requirements-gpu.txt` 后复测。

## 一、功能完整性检查

### 1.1 服务端 API

- [ ] `GET /healthz` 返回正常状态
- [ ] `POST /v1/storyboard` 生成分镜脚本
- [ ] `POST /v1/imagegen` 生成图像（mock/GPU）
- [ ] `POST /v1/tts` 生成语音
- [ ] `POST /v1/compose` 合成视频
- [ ] `POST /v1/cancel/{request_id}` 取消任务

### 1.2 Provider 模式

- [ ] Mock Provider 可用（无外部依赖）
- [ ] Local GPU Provider 代码完整（需 PyTorch）
- [ ] GPU 不可用时自动降级到 mock

### 1.3 客户端集成

- [ ] StoryboardStage 调用服务端 API
- [ ] ImageGenStage 调用服务端 API
- [ ] TtsStage 调用服务端 API
- [ ] ComposeStage 调用服务端 API
- [ ] Stage Factory 正确注入到 WorkflowEngine

### 1.4 端到端流程

- [ ] 输入故事文本 → 分镜脚本
- [ ] 分镜脚本 → N 张图像
- [ ] 分镜脚本 → N 段音频
- [ ] 图像 + 音频 → 最终视频（.mp4）
- [ ] 输出路径遵循 XDG 规范

## 二、架构正确性检查

### 2.1 依赖关系

- [ ] `infra_base` 不依赖 `core`（独立）
- [ ] `infra_net` 依赖 `core` 和 `infra_base`
- [ ] `core` 不依赖任何 `infra`
- [ ] `app` 依赖 `core` 和 `infra`
- [ ] 无循环依赖

### 2.2 分层职责

- [ ] `app/Presenter` 仅做 Qt ↔ core 桥接，无业务逻辑
- [ ] `core/WorkflowEngine` 负责任务编排，不涉及具体实现
- [ ] `infra/stages` 实现具体的 AI 调用逻辑
- [ ] `server` 独立进程，通过 HTTP 通信

## 三、可靠性检查

### 3.1 错误处理

- [ ] 所有错误都有 `TaskError` 封装
- [ ] 错误包含 `category`, `code`, `retryable`, `user_message`, `internal_message`
- [ ] HTTP 错误正确映射到 `ErrorCategory`
- [ ] 4xx 错误标记为不可重试
- [ ] 5xx 错误标记为可重试

### 3.2 重试机制

- [ ] `RetryableHttpClient` 实现指数退避
- [ ] 最大重试次数可配置（默认 2 次）
- [ ] 重试期间检查 `cancel_token`
- [ ] 重试日志包含 `retry_count`, `backoff_ms`

### 3.3 取消机制

- [ ] UI 取消请求能传递到 WorkflowEngine
- [ ] WorkflowEngine 取消所有相关任务
- [ ] Stage 执行期间检查 `cancel_token`
- [ ] HTTP 请求可通过 `request_id` 取消
- [ ] 取消后无僵尸任务或资源泄漏

## 四、可观测性检查

### 4.1 日志规范

- [ ] 日志格式：`[timestamp] [level] [trace_id] [component] event: message`
- [ ] 所有请求包含 `trace_id`
- [ ] HTTP 请求包含 `request_id`
- [ ] 关键事件有日志（request_sent, stage_started, workflow_completed 等）

### 4.2 XDG 路径

- [ ] 配置目录：`~/.config/stv-renew/`
- [ ] 缓存目录：`~/.cache/stv-renew/`
- [ ] 数据目录：`~/.local/share/stv-renew/`
- [ ] 输出目录：`~/.local/share/stv-renew/outputs/`（或 `$STV_OUTPUT_DIR`）
- [ ] 目录权限正确，可读写

## 五、性能指标验收

### 5.0 自动化基准脚本

统一使用 `scripts/bench_m2.py` 采集性能、资源与稳定性指标：

```bash
# Mock 基线 + 故障注入
./scripts/bench_m2.py --provider mock --runs 30 --out bench.json --report docs/reports/M2_perf.md

# Local GPU 基线 + 故障注入
./scripts/bench_m2.py --provider local_gpu --runs 30 --out bench_gpu.json --report docs/reports/M2_perf.md
```

### 5.1 端到端性能（Mock Provider）

测试命令：
```bash
time ./scripts/test_e2e_mock.sh
```

| 指标 | 目标 | 实际 | 通过 |
|------|------|------|------|
| 总耗时（4 scene） | < 5s | ____ | [ ] |
| Storyboard 生成 | < 1s | ____ | [ ] |
| 单张图像生成 | < 0.5s | ____ | [ ] |
| 单段音频生成 | < 0.5s | ____ | [ ] |
| 视频合成 | < 2s | ____ | [ ] |

### 5.2 端到端性能（Local GPU Provider）

> 注意：需要 PyTorch + GPU 环境

| 指标 | 目标 | 实际 | 通过 |
|------|------|------|------|
| 总耗时（4 scene） | P50 < 35s | ____ | [ ] |
| 总耗时（4 scene） | P95 < 55s | ____ | [ ] |
| 单张 SD 图像生成 | < 15s | ____ | [ ] |
| 峰值 VRAM | < 7.5GB | ____ | [ ] |

### 5.3 取消延迟

测试方法：启动任务后立即取消，测量延迟

| 指标 | 目标 | 实际 | 通过 |
|------|------|------|------|
| 取消响应延迟 | P95 < 300ms | ____ | [ ] |

### 5.4 资源使用

测试命令：
```bash
# 客户端
pidof stv_app | xargs ps -o rss= | awk '{print $1/1024 " MB"}'

# 服务端
pidof python3 | xargs ps -o rss= | awk '{print $1/1024 " MB"}'
```

| 指标 | 目标 | 实际 | 通过 |
|------|------|------|------|
| 客户端峰值 RSS | < 500MB (不含 Qt) | ____ | [ ] |
| 服务端峰值 RSS | < 1.5GB | ____ | [ ] |

## 六、稳定性验收

### 6.1 连续运行测试

测试命令：
```bash
for i in {1..30}; do
  echo "Run $i/30"
  ./scripts/test_e2e_mock.sh || echo "FAIL: Run $i"
done
```

| 指标 | 目标 | 实际 | 通过 |
|------|------|------|------|
| 成功次数（30 次） | >= 29 (97%) | ____ | [ ] |

### 6.2 故障注入测试

模拟服务端随机返回 5xx 错误（30% 概率）

| 指标 | 目标 | 实际 | 通过 |
|------|------|------|------|
| 恢复成功率 | >= 90% | ____ | [ ] |

## 七、编译与构建检查

### 7.1 无 Qt 构建

测试命令：
```bash
./scripts/test_build.sh
```

- [ ] `STV_BUILD_APP=OFF` 时可完整构建
- [ ] 所有单元测试通过
- [ ] 测试不依赖外网（`STV_ENABLE_NETWORK_TESTS=OFF`）

### 7.2 有 Qt 构建

> 需要 Qt6 环境

测试命令：
```bash
mkdir build-qt && cd build-qt
cmake .. -DSTV_BUILD_APP=ON -DSTV_BUILD_TESTS=ON
cmake --build . -j$(nproc)
```

- [ ] `STV_BUILD_APP=ON` 时可完整构建
- [ ] `stv_app` 可执行文件生成
- [ ] 运行 `stv_app` 可显示 UI

## 八、文档完整性检查

- [ ] `README.md` 更新（包含 M2 进度）
- [ ] `server/README.md` 包含启动说明
- [ ] `docs/OBSERVABILITY.md` 包含错误处理和日志规范
- [ ] `docs/M2_PROGRESS.md` 记录实施过程

## 九、遗留问题与后续计划

### 9.1 已知限制（M2）

- [ ] Storyboard 使用简单规则，未集成 LLM
- [ ] TTS 生成静音音频（未集成真实 TTS）
- [ ] 图像质量依赖 SD1.5（M3 可升级到 SDXL）
- [ ] VideoClip stage 未实现（M2 不需要）

### 9.2 M3 计划

- [ ] ThreadPoolScheduler（多线程调度）
- [ ] DAG 依赖解析和并行执行
- [ ] ResourceBudget（CPU/RAM/VRAM 管理）
- [ ] 优先级队列与防饥饿

### 9.3 M4 计划

- [ ] CI 集成（GitHub Actions）
- [ ] AppImage 打包
- [ ] ASan/TSan 可选编译
- [ ] 性能回归门禁

## 十、签署与批准

| 角色 | 姓名 | 签名 | 日期 |
|------|------|------|------|
| 开发负责人 | ____ | ____ | ____ |
| 测试负责人 | ____ | ____ | ____ |
| 项目经理 | ____ | ____ | ____ |

---

## 验收结论

- [ ] **通过**：所有关键指标达标，可进入 M3
- [ ] **条件通过**：部分指标未达标但不影响核心功能，需在 M3 补齐
- [ ] **不通过**：关键功能缺失或性能/稳定性严重不达标，需返工

**备注**：
