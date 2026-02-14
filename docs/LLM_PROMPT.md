# StoryToVideo Renew — LLM Context Prompt

> **用法**：将本文件内容粘贴到任何大模型（ChatGPT / Claude / Gemini / DeepSeek 等）的 System Prompt 或首轮对话中，即可恢复完整项目上下文继续开发。

---

## 一、项目概述

**StoryToVideo Renew** 是一个桌面端 AI 视频生成客户端的**重构项目**，目标是用更好的架构和工程实践重写旧版 StoryToVideo 客户端。

- **输入**：用户输入故事文本 + 选择画风
- **输出**：AI 生成分镜图 → 合成视频（MP4）
- **核心价值**：面试导向的工程实战项目，重视架构设计、可测试性、可观测性

---

## 二、旧项目问题（重构原因）

旧项目存在 10 大技术债，重点包括：

1. **ViewModel 上帝对象** (650 LOC)：混合网络/状态/持久化/下载，不可测试
2. **无任务状态机**：用 `QHash<taskId, QVariantMap>` + 字符串类型 ("text_task"/"shot"/"video") 松散追踪
3. **轮询式任务追踪**：1 秒 QTimer 遍历所有 active tasks 发 HTTP
4. **无取消/暂停/恢复**：一旦触发不可中止
5. **无结构化错误处理**：全部 `QString errorMsg`
6. **Gateway 单文件 1582 LOC**
7. **无日志/Trace**：散落 `qDebug()`，无 trace_id
8. **核心逻辑依赖 Qt**：不可独立单测

---

## 三、新架构

### 3.1 分层架构

```
┌─────────────────────────────────────────────────┐
│  app (Qt6 依赖层)                                │
│    app/ui — QML Pages (CreatePage, ProgressPage) │
│    app/presenter — Presenter (QObject 桥接)       │
├───────────── 依赖方向: 仅向下 ─────────────────────┤
│  core (纯 C++17，零 Qt 依赖)                      │
│    core/orchestrator — WorkflowEngine            │
│    core/scheduler — SimpleScheduler (M1)         │
│    core/pipeline — IStage + StageContext          │
│    core/task — TaskDescriptor + State Machine     │
├──────────────────────────────────────────────────┤
│  infra (基础设施)                                 │
│    infra/telemetry — ILogger (spdlog)            │
│    infra/media — IMediaComposer (mock)           │
│    infra/io — FileSystem / Config                │
└──────────────────────────────────────────────────┘
```

**关键规则**：`core/*` 不依赖 Qt；`app/presenter` 是唯一桥接层；依赖方向只向下。

### 3.2 任务统一状态机 (7 态)

```
[*] → Queued
Queued → Ready: dependencies_met
Ready → Running: scheduler_dispatch
Running → Paused: user_pause
Paused → Running: user_resume
Running → Succeeded: stage_complete
Running → Failed: error
Running → Canceled: user_cancel / timeout
Queued → Canceled: user_cancel
Ready → Canceled: user_cancel
Paused → Canceled: user_cancel
Failed → Queued: retry
```

- **终态**：Succeeded / Failed / Canceled
- **唯一回退**：Failed → Queued（retry，重置 progress/error/timestamps）

### 3.3 核心接口

```cpp
// Result<T,E> — 类 Rust 错误处理，替代 bool 返回值和异常
template<typename T, typename E>
class Result {
    static Result Ok(T value);
    static Result Err(E error);
    bool is_ok() const;
    const T& value() const;
    const E& error() const;
};

// TaskDescriptor — 每个任务的核心数据结构
struct TaskDescriptor {
    std::string task_id;    // UUID
    std::string trace_id;   // 同一 workflow 共享
    TaskType    type;       // Storyboard / ImageGen / VideoClip / TTS / Compose
    TaskState   state;      // 7 种状态
    int         priority;
    float       progress;   // [0.0, 1.0]
    std::vector<std::string> deps;  // 前驱依赖
    std::optional<TaskError>  error;
    std::shared_ptr<CancelToken> cancel_token;
    Result<void, TaskError> transition_to(TaskState new_state);  // 合法性校验
};

// IStage — 流水线阶段接口
class IStage {
    virtual std::string name() const = 0;
    virtual Result<void, TaskError> execute(StageContext& ctx) = 0;
};

// IScheduler — 调度器接口
class IScheduler {
    virtual void submit(TaskDescriptor task, std::shared_ptr<IStage> stage) = 0;
    virtual void tick() = 0;                     // 事件循环驱动
    virtual Result<void, TaskError> cancel(const std::string& task_id) = 0;
    virtual void on_state_change(StateCallback cb) = 0;
};

// WorkflowEngine — 编排器
class WorkflowEngine {
    std::string start_workflow(const std::string& story_text,
                               const std::string& style, int scene_count);
    // 创建: Storyboard → ImageGen×N → Compose 任务链
    void set_stage_factory(StageFactory factory);  // mock/real 可替换
};
```

### 3.4 CancelToken 设计

```cpp
class CancelToken {
    std::atomic<bool> canceled_{false};
    void request_cancel() noexcept;     // compare_exchange_strong + release
    bool is_canceled() const noexcept;  // acquire 读取
    void on_cancel(Callback cb);        // 注册回调
};
```

- 单写者多读者模式，无锁设计
- `compare_exchange_strong` 保证回调只触发一次

---

## 四、当前项目结构（M1 已实现）

```
stv_renew/
├── CMakeLists.txt                    # 顶层: C++17, spdlog + GTest via FetchContent
├── core/                             # 纯 C++17
│   ├── include/core/
│   │   ├── result.h                  # Result<T,E> 模板 + void 特化
│   │   ├── task_error.h              # ErrorCategory (7种) + TaskError
│   │   ├── cancel_token.h            # atomic CancelToken
│   │   ├── task.h                    # TaskDescriptor + 状态机
│   │   ├── pipeline.h               # IStage + StageContext (typed I/O)
│   │   ├── scheduler.h              # IScheduler 接口
│   │   └── orchestrator.h           # WorkflowEngine
│   ├── src/
│   │   ├── task.cpp                  # 状态转移 (table-driven)
│   │   ├── cancel_token.cpp          # compare_exchange + 回调
│   │   ├── scheduler.cpp            # SimpleScheduler (tick+deps+priority)
│   │   ├── pipeline.cpp             # 3 个 mock stage + factory
│   │   └── orchestrator.cpp         # 任务链创建 + 完成追踪
│   └── CMakeLists.txt
├── infra/
│   ├── include/infra/logger.h        # ILogger 接口
│   ├── src/logger.cpp                # ConsoleLogger (spdlog structured)
│   └── CMakeLists.txt
├── app/                              # Qt6 依赖层
│   ├── include/app/presenter.h       # Presenter QObject
│   ├── src/
│   │   ├── presenter.cpp             # QTimer tick 驱动, signal 转发
│   │   └── main.cpp                  # DI 装配: logger→scheduler→presenter→QML
│   ├── qml/
│   │   ├── main.qml                  # StackView 导航
│   │   ├── CreatePage.qml            # 故事输入 + 风格 + 场景数
│   │   └── ProgressPage.qml          # 进度条 + 日志 + 取消
│   └── CMakeLists.txt
└── tests/
    ├── test_task_state_machine.cpp    # 19 个状态转移测试
    ├── test_scheduler.cpp            # 6 个调度测试
    ├── test_pipeline.cpp             # 6 个流水线测试
    └── CMakeLists.txt                # + 5 个 CancelToken 测试
```

---

## 五、构建与测试

```bash
# 仅构建 core + infra + tests（不需要 Qt）
cmake -B build -DSTV_BUILD_APP=OFF -DSTV_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure -j4

# 构建完整应用（需要 Qt6）
cmake -B build -DSTV_BUILD_APP=ON -DSTV_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH=/path/to/qt6
cmake --build build -j$(nproc)
./build/app/stv_app
```

**当前测试状态**：36/36 全部通过 ✅

---

## 六、里程碑规划

| 阶段 | 状态 | 内容 |
|------|------|------|
| **M1** | ✅ 已完成 | Mock 闭环: story→storyboard→image×N→compose, 36 tests pass |
| **M2** | 待开始 | 真实服务: 接入 LLM/SD/TTS API, 错误收敛 + 重试 |
| **M3** | 待开始 | 调度升级: 线程池 + 依赖图 + 优先级队列 + 资源预算 |
| **M4** | 待开始 | 工程加固: CI, 指标, 配置管理, 文档 |

---

## 七、关键设计决策（面试 Q&A）

| 问题 | 回答要点 |
|------|---------|
| 为什么不用线程池+future 阻塞等待？ | 回调驱动+状态机避免线程占用，支持 pause/cancel/resume |
| 可观测性如何保证？ | trace_id 贯穿全链路 + spdlog 结构化日志 (ts/level/trace/component/event) |
| 如何量化验证？ | 端到端耗时 ≈ mock sleep×N ±5%；状态转移事件精确计数 |
| 跨平台与工程化？ | core 零 Qt 依赖, CMake + GTest FetchContent, CI 独立编译测试 |
| CancelToken 线程安全？ | `atomic<bool>` + `memory_order_acquire/release`，compare_exchange 保证回调单次触发 |
| 为什么 core 不依赖 Qt？ | 可独立编译测试, CI 无需安装 Qt, 方便移植到其他 UI 框架 |

---

## 八、编码风格与约定

- **语言**: C++17 (core/infra), Qt 6.x (app), QML 2.15
- **命名**: snake_case (变量/函数), PascalCase (类/枚举), SCREAMING_CASE (常量)
- **命名空间**: `stv::core`, `stv::infra`, `stv::app`
- **错误处理**: 全部使用 `Result<T, TaskError>`，不使用异常
- **日志**: `ILogger::info(trace_id, component, event, msg)` 四字段结构化
- **编译警告**: `-Wall -Wextra -Wpedantic -Werror=return-type`
- **第三方依赖**: 全部通过 CMake FetchContent 管理 (spdlog, googletest)

---

## 九、当前待办 / 你可以帮我做的事

1. **在 Linux 上构建并运行完整 UI**（需要 Qt6）
2. **M2: 替换 mock stage 为真实服务调用**
   - `MockStoryboardStage` → 真实 LLM API
   - `MockImageGenStage` → 真实 txt2img API
   - `MockComposeStage` → 真实 FFmpeg 调用
3. **M3: 升级调度器**
   - `SimpleScheduler` → `ThreadPoolScheduler`
   - 依赖图解析 + 优先级队列
   - 并行执行 ImageGen tasks（当前是串行 tick）
4. **增加测试**
   - WorkflowEngine 集成测试
   - Presenter 层测试（需要 QTest）
5. **CI 流水线** (GitHub Actions: cmake build + ctest)
6. **写面试材料**: 根据项目细节准备深入 Q&A
