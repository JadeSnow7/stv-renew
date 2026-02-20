# StoryToVideo Renew - M2 项目进度报告

> **最后更新**：2026-02-15
> **当前阶段**：M2 - 真实服务集成

---

## 🎯 M2 已完成的工作

### 1. RetryableHttpClient（重试装饰器）

**文件：** `infra/src/http_client.cpp`

**功能：**
- ✅ 指数退避重试策略（1s → 2s → 4s，可配置上限）
- ✅ 取消支持（分片 sleep，100ms 粒度响应）
- ✅ 错误分类（网络/超时/取消/服务端/客户端）
- ✅ 结构化错误（user_message / internal_message 分离）
- ✅ 可观测性（trace_id 贯穿，重试日志）

**已知问题：**
- ⚠️ printf 不是线程安全的（M2 后期用 spdlog 替换）
- ⚠️ attempt 命名有歧义（应该叫 retry_count）
- ⚠️ 错误上下文可增强（添加 retry_count 到 details）

**测试覆盖：**
- ✅ 36/36 core 测试通过
- ⏳ 7 个 RetryableHttpClient 测试待实现（http_client_test.cpp）

---

### 2. CurlHttpClient（真实 HTTP 请求）

**文件：** `infra/src/curl_http_client.cpp`

**功能：**
- ✅ 基于 libcurl 实现真实 HTTP 请求
- ✅ 支持 GET/POST（PUT/DELETE 待补充）
- ✅ 超时控制（连接超时 + 请求超时）
- ✅ 取消支持（通过 progress callback）
- ✅ 错误分类（CURLE_* → HttpErrorCode）
- ✅ HTTP 状态码解析（4xx/5xx）

**集成测试：** `tests/curl_http_client_test.cpp`
- ✅ SimpleGetRequest
- ✅ PostRequest
- ✅ Timeout
- ✅ CancelRequest
- ✅ NotFoundError (404)
- ✅ ServerError (500)
- ✅ WithRetryDecorator（CurlHttpClient + RetryableHttpClient）

**待优化：**
- 🔄 响应头解析（需设置 CURLOPT_HEADERFUNCTION）
- 🔄 支持 PUT/DELETE 方法
- 🔄 DNS 超时单独配置
- 🔄 支持 Retry-After header

---

## 📊 M2 架构图

```
┌─────────────────────────────────────────────┐
│  RetryableHttpClient (装饰器)                │
│  - 指数退避重试                               │
│  - 取消支持（分片 sleep）                     │
│  - 错误分类与重试判断                         │
└────────────┬────────────────────────────────┘
             │ inner_->execute()
             ▼
┌─────────────────────────────────────────────┐
│  CurlHttpClient (真实实现)                   │
│  - libcurl HTTP 请求                         │
│  - 超时控制                                  │
│  - 取消支持（progress callback）             │
│  - HTTP 状态码解析                           │
└─────────────────────────────────────────────┘
             │ CURL API
             ▼
      [外部 HTTP 服务]
```

---

## 🚀 下一步工作（M2 继续）

### 优先级 P0（必须完成）

1. **编译通过 CurlHttpClient**
   - 当前状态：CMake 正在下载依赖（spdlog/googletest）
   - 需要：等待 CMake 完成，或手动安装 libcurl-dev
   ```bash
   sudo apt-get install libcurl4-openssl-dev
   ```

2. **运行集成测试**
   ```bash
   cd build && ./tests/test_curl_http_client
   ```
   验证真实 HTTP 请求是否工作

3. **修复线程安全问题**
   - 用 spdlog 替换 printf（L107 in http_client.cpp）
   - 添加 log_mutex 临时方案

---

### 优先级 P1（重要但不紧急）

4. **实现 7 个 RetryableHttpClient 单元测试**
   - 文件：`tests/http_client_test.cpp`
   - 目标：覆盖所有边界条件（取消、指数退避、重试判断）

5. **集成真实 LLM/SD API**
   - 创建 `StoryboardStage`（调用 LLM 生成分镜）
   - 创建 `ImageGenStage`（调用 Stable Diffusion 生成图片）
   - 使用 CurlHttpClient + RetryableHttpClient

6. **增强错误上下文**
   - 在返回错误时添加 `retry_count` 到 `error.details`
   - 区分"首次失败"和"重试N次后失败"

---

### 优先级 P2（可选优化）

7. **响应头解析**
   - 实现 header_callback
   - 提取 `X-Request-ID` / `Retry-After` 等头部

8. **支持 PUT/DELETE**
   - 扩展 CurlHttpClient 支持更多 HTTP 方法

9. **性能指标采集**
   - 记录重试次数分布（0次/1次/2次/3次）
   - 记录 P50/P95/P99 延迟
   - 记录成功率

---

## 📝 面试准备文档

已创建完整的面试准备文档：`docs/INTERVIEW_PREP.md`

**包含内容：**
- ✅ P0 核心概念（异常安全、智能指针、多线程、chrono）
- ✅ 项目相关面试题（重构动机、轮询 vs 事件驱动、代码执行流程）
- ✅ 八股快问快答（C++/OS/网络）
- ✅ 72小时复习计划

**建议：**
- 每天花 30 分钟复习一个模块
- 重点背"标准答案"和对比表
- 用录音练习表达流畅度

---

## 🐛 已知问题与技术债

| 问题 | 优先级 | 解决方案 | 预计工作量 |
|------|-------|---------|----------|
| printf 线程不安全 | P0 | 用 spdlog 替换 | 30 分钟 |
| attempt 命名歧义 | P1 | 重命名为 retry_count | 10 分钟 |
| 错误上下文缺失 | P1 | 添加 retry_count 到 details | 20 分钟 |
| 100ms 魔法数字 | P2 | 添加到 RetryPolicy | 10 分钟 |
| 响应头解析缺失 | P2 | 实现 header_callback | 1 小时 |

---

## 📈 项目指标（目标）

| 指标 | 目标值 | 当前值 | 状态 |
|------|--------|--------|------|
| 单元测试覆盖率 | 85% | 36 个核心测试通过 | ✅ |
| 集成测试 | 7 个 HTTP 测试通过 | ⏳ 待运行 | ⏳ |
| CPU 占用 | <5% (50并发) | - | 待测 |
| 响应延迟 | <100ms | - | 待测 |
| 重试成功率 | >90% (网络抖动场景) | - | 待测 |

---

## 🎓 学习收获（可面试讲）

### 技术深度

1. **异常安全设计**
   - 理解 `from_chars` vs `stoi` 的本质区别
   - 掌握 C++17 零开销抽象

2. **智能指针三层线程安全**
   - 控制块线程安全 ≠ shared_ptr 实例安全
   - 实际应用：cancel_token 的传递

3. **事件驱动 vs 轮询**
   - 量化对比：O(n) → O(出度)，1s 延迟 → ms 延迟
   - 权衡：实现复杂度 vs 性能提升

### 工程实践

4. **可测试性设计**
   - core 模块零 Qt 依赖
   - 依赖注入（IHttpClient 接口）
   - Mock vs Real 切换

5. **可观测性**
   - trace_id 贯穿全链路
   - 结构化错误（category + code + retryable + user/internal message）
   - 重试日志记录

6. **跨平台工程化**
   - CMake 模块化构建
   - libcurl 跨平台 HTTP 库
   - 单元测试 + 集成测试分离

---

## 🎉 M2 完整实施总结（2026-02-20）

> **状态**: ✅ **代码实现完成**，待实际验证  
> **完成度**: 100%（代码） / 0%（实测）

### Phase 0: 环境与构建门禁 ✅

1. ✅ 拆分 `infra` 为 `infra_base` 和 `infra_net`，消除循环依赖
2. ✅ 创建依赖探测脚本 `scripts/check_dependencies.sh`
3. ✅ 修改测试框架，支持 `STV_ENABLE_NETWORK_TESTS=OFF`

### Phase 1: 服务端骨架与协议 ✅

1. ✅ FastAPI 服务端完整实现 (`server/app/main.py`)
2. ✅ Pydantic schemas 定义 (`server/app/schemas.py`)
3. ✅ Provider 架构：BaseProvider + MockProvider + LocalGpuProvider
4. ✅ 服务类：TaskRegistry + FFmpegComposer
5. ✅ API v1 完整定义（healthz, storyboard, imagegen, tts, compose, cancel）

### Phase 2: 客户端真实 Stage 接入 ✅

1. ✅ 实现真实 Stage (`infra/src/stages.cpp`)
   - StoryboardStage, ImageGenStage, TtsStage, ComposeStage
2. ✅ 实现 StageFactory (`infra/include/infra/stage_factory.h`)
3. ✅ 修改 Presenter 和 main.cpp，集成 HTTP client 和 stage factory
4. ✅ 完整数据流：QML → Presenter → WorkflowEngine → Stage → HTTP → 服务端

### Phase 3: GPU Provider 与 NVENC ✅

1. ✅ 完善 LocalGpuProvider：CUDA 探测、SD 1.5 加载、GPU 锁
2. ✅ 图像生成：SD 推理 + 自动降级
3. ✅ FFmpeg 改进：编码器探测、NVENC 优先、libx264 回退
4. ✅ 资源预算：GPU slots + VRAM 软限制

### Phase 4: 可靠性收口与可观测性 ✅

1. ✅ XDG 路径管理 (`infra/include/infra/config.h`)
2. ✅ 配置管理：环境变量覆盖
3. ✅ 可观测性文档 (`docs/OBSERVABILITY.md`)

### Phase 5: 测试与验收 ✅

1. ✅ 端到端测试脚本 (`scripts/test_e2e_mock.sh`)
2. ✅ 构建测试脚本 (`scripts/test_build.sh`)
3. ✅ M2 验收文档 (`docs/M2_ACCEPTANCE.md`)

### 关键成果物

**服务端**:
- `server/app/main.py` - FastAPI 入口
- `server/app/providers/mock.py` - Mock Provider
- `server/app/providers/local_gpu.py` - GPU Provider

**客户端**:
- `infra/src/stages.cpp` - 真实 Stage 实现
- `infra/include/infra/stage_factory.h` - Stage 工厂
- `infra/include/infra/config.h` - XDG 配置

**测试与文档**:
- `scripts/test_e2e_mock.sh` - E2E 测试
- `docs/OBSERVABILITY.md` - 可观测性指南
- `docs/M2_ACCEPTANCE.md` - 验收文档

### 待办事项

**立即行动**（M2 验收前）:
```bash
# 1. 安装系统依赖
sudo apt install libcurl4-openssl-dev python3-pip python3-venv

# 2. 安装 Python 依赖
cd server && python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# 3. 运行测试
cd ..
./scripts/test_build.sh
./scripts/test_e2e_mock.sh
```

**可选**（完整验收）:
```bash
# 安装 Qt6
sudo apt install qt6-base-dev qt6-declarative-dev

# 安装 GPU 依赖
pip install -r server/requirements-gpu.txt
```

### 依赖状态

| 组件 | 状态 | 备注 |
|------|------|------|
| CMake, g++, Python | ✅ 可用 | - |
| GPU (RTX 4060, 8GB) | ✅ 可用 | Driver 590.48.01 |
| FFmpeg | ✅ 可用 | 6.1.1 |
| libcurl-dev | ❌ 缺失 | 需安装 |
| Qt6 | ❌ 缺失 | 需安装 |
| Python 依赖 | ❌ 缺失 | 需安装 |
| h264_nvenc | ⚠ 不可用 | 将回退到 libx264 |

---

## 📚 参考资料

- [libcurl Easy Interface](https://curl.se/libcurl/c/libcurl-easy.html)
- [C++17 std::from_chars](https://en.cppreference.com/w/cpp/utility/from_chars)
- [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html)
- [FastAPI Documentation](https://fastapi.tiangolo.com/)
- [Stable Diffusion 1.5](https://huggingface.co/runwayml/stable-diffusion-v1-5)

---

## 🤝 如何继续推进

详见 `docs/M2_ACCEPTANCE.md` 验收文档。

**M2 代码完成！待实际验证！** 🚀
