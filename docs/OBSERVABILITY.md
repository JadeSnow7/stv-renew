# 可观测性与错误处理指南

## 一、统一错误处理（TaskError）

### 错误字段规范

所有错误必须包含以下字段（定义在 `core/include/core/task_error.h`）：

```cpp
struct TaskError {
    ErrorCategory category;   // 错误分类：Network/Pipeline/Timeout/Canceled/Unknown
    int code;                 // 错误代码（业务定义）
    bool retryable;          // 是否可重试
    std::string user_message; // 面向用户的错误信息（简短、友好）
    std::string internal_message; // 内部调试信息（详细、技术性）
    std::map<std::string, std::string> details; // 额外详情（trace_id, request_id, retry_count 等）
};
```

### 错误分类（ErrorCategory）

- **Network**: 网络相关错误（HTTP 4xx/5xx、超时、连接失败）
- **Pipeline**: 管道/业务逻辑错误（数据验证失败、参数错误）
- **Timeout**: 超时错误（专门分类，便于监控）
- **Canceled**: 取消操作（用户主动取消或系统取消）
- **Unknown**: 未知错误（应尽量避免）

### HTTP 错误映射

HTTP 状态码 → TaskError 映射（实现在 `infra/src/http_client.cpp:make_http_error`）：

| HTTP 状态码范围 | ErrorCategory | retryable |
|----------------|---------------|-----------|
| 网络错误（无法连接） | Network | true |
| 超时 | Timeout | true |
| 4xx（客户端错误）| Pipeline | false |
| 429（限流）| Network | true |
| 5xx（服务端错误）| Network | true |

## 二、重试策略

### RetryPolicy 配置

```cpp
struct RetryPolicy {
    int max_retries = 2;                                    // 最大重试次数
    std::chrono::milliseconds initial_backoff = 500ms;     // 初始退避时间
    std::chrono::milliseconds max_backoff = 5000ms;        // 最大退避时间
    double backoff_multiplier = 2.0;                       // 退避倍数（指数退避）
    std::chrono::milliseconds sleep_slice = 50ms;          // 退避时检查取消的间隔
};
```

### 可重试的错误类型

- 网络超时（Timeout）
- 服务端错误（5xx）
- 限流错误（429 Too Many Requests）

### 不可重试的错误类型

- 客户端错误（4xx，如 400 Bad Request）
- 取消操作（Canceled）
- 业务逻辑错误（参数错误、数据验证失败）

## 三、日志规范

### 日志字段

所有日志必须包含：

```
[timestamp] [level] [trace_id] [component] event: message
```

示例：
```
[2026-02-20T14:30:15.123] [INFO] [trace-12345] [http_client] request_sent: url=/v1/storyboard request_id=req-001
[2026-02-20T14:30:15.456] [WARN] [trace-12345] [http_client] retry_scheduled: retry_count=1 backoff_ms=500
[2026-02-20T14:30:16.789] [ERROR] [trace-12345] [storyboard_stage] execution_failed: HTTP 500 Internal Server Error
```

### 日志级别

- **INFO**: 正常操作（请求开始、任务完成、状态变更）
- **WARN**: 警告（重试、降级、资源接近限制）
- **ERROR**: 错误（任务失败、无法恢复的错误）

### 关键事件

#### HTTP 客户端（infra/http_client）

- `request_sent`: HTTP 请求开始
- `request_completed`: HTTP 请求成功
- `request_failed`: HTTP 请求失败
- `retry_scheduled`: 计划重试
- `cancel_requested`: 收到取消请求

#### Stage 执行（infra/stages）

- `stage_started`: Stage 开始执行
- `stage_progress`: Stage 进度更新
- `stage_completed`: Stage 执行成功
- `stage_failed`: Stage 执行失败

#### 工作流编排（core/orchestrator）

- `workflow_started`: 工作流开始
- `task_state_changed`: 任务状态变更
- `workflow_completed`: 工作流完成
- `workflow_failed`: 工作流失败

## 四、配置管理（XDG 规范）

### 目录结构

遵循 XDG Base Directory Specification：

```
~/.config/stv-renew/           # 配置文件
  └── config.json              # 应用配置

~/.cache/stv-renew/            # 临时缓存
  ├── models/                  # 模型缓存（如 SD 1.5）
  └── http_cache/              # HTTP 响应缓存

~/.local/share/stv-renew/      # 应用数据
  └── outputs/                 # 输出视频（默认）
      ├── trace-12345.mp4
      └── ...
```

### 环境变量覆盖

支持通过环境变量覆盖默认路径：

- `XDG_CONFIG_HOME`: 覆盖配置目录
- `XDG_CACHE_HOME`: 覆盖缓存目录
- `XDG_DATA_HOME`: 覆盖数据目录
- `STV_OUTPUT_DIR`: 直接指定输出目录

## 五、取消操作

### 取消流程

1. **UI 发起取消**
   - 用户点击"取消"按钮
   - Presenter 调用 `WorkflowEngine::cancel_workflow(trace_id)`

2. **任务层取消**
   - WorkflowEngine 遍历该工作流的所有任务
   - 调用每个任务的 `cancel_token->cancel()`

3. **Stage 响应取消**
   - Stage 在执行过程中定期检查 `ctx.cancel_token->is_canceled()`
   - 如果已取消，立即返回 `TaskError(Canceled, ...)`

4. **HTTP 层取消**
   - RetryableHttpClient 在重试退避期间检查 `cancel_token`
   - CurlHttpClient 通过 request_id 取消正在进行的请求

5.  **服务端取消**
   - 客户端调用 `POST /v1/cancel/{request_id}`
   - 服务端标记任务为已取消，停止推理

### 取消延迟目标

- UI 点击到任务停止：**P95 < 300ms**
- HTTP 请求取消响应：**P95 < 100ms**

## 六、监控指标

### 性能指标

- **端到端延迟**: 从 `start_workflow` 到 `workflow_completed` 的时间
  - 目标：4 scene @ P50 < 35s, P95 < 55s
- **Stage 耗时**: 每个 stage 的执行时间
  - Storyboard: < 2s
  - ImageGen: < 15s（GPU）, < 1s（mock）
  - TTS: < 5s
  - Compose: < 10s
- **取消延迟**: 从取消请求到任务终止的时间
  - 目标：P95 < 300ms

### 资源指标

- **客户端内存**: 峰值 RSS
  - 目标：< 500MB（不含 Qt）
- **服务端显存**: 峰值 VRAM
  - 目标：< 7.5GB（8GB GPU）
- **服务端内存**: 峰值 RSS
  - 目标：< 1.5GB

### 可靠性指标

- **成功率**: 30 次连续运行成功次数
  - 目标：>= 97%（至少 29/30）
- **恢复成功率**: 注入 30% 瞬时 5xx 时的成功率
  - 目标：>= 90%（重试机制有效）

## 七、故障注入测试

### 测试场景

1. **网络超时**: 服务端延迟响应 > timeout
2. **服务端错误**: 随机返回 500/502/503
3. **限流**: 返回 429 Too Many Requests
4. **取消竞态**: 在各阶段取消任务
5. **GPU OOM**: 显存不足时的降级
6. **NVENC 不可用**: 回退到 libx264

### 验证要点

- 错误分类正确（category, code, retryable）
- 重试次数符合预期
- 取消响应及时且无泄漏
- 日志完整且可追溯（trace_id/request_id 一致）
- 降级路径正常工作

## 八、调试技巧

### 查找特定工作流的日志

```bash
# 按 trace_id 过滤
grep "trace-12345" app.log

# 按 request_id 过滤（跨客户端和服务端）
grep "req-001" client.log server.log
```

### 分析性能瓶颈

```bash
# 提取各 stage 的耗时
grep "stage_completed" app.log | awk '{print $NF}'

# 统计重试次数
grep "retry_scheduled" app.log | wc -l
```

### 查看错误分布

```bash
# 错误分类统计
grep "ERROR" app.log | awk -F'[' '{print $4}' | sort | uniq -c
```

## 九、生产环境检查清单

- [ ] 所有错误都有 user_message 和 internal_message
- [ ] 可重试错误正确标记 retryable=true
- [ ] 日志包含 trace_id 和 request_id
- [ ] 取消操作在 300ms 内响应
- [ ] XDG 路径权限正确
- [ ] HTTP 超时设置合理（storyboard:30s, imagegen:120s, compose:300s）
- [ ] GPU 降级路径测试通过
- [ ] 30 次连续运行成功率 >= 97%
