# StoryToVideo Renew 跨平台适配计划审核报告

**审核日期**: 2026-02-17
**审核人**: Claude (Sonnet 4.5)
**计划版本**: Initial Draft
**审核结论**: **有条件通过** - 需解决以下关键问题后方可执行

---

## 一、总体评价

### ✅ 优点
1. **范围界定清晰**: In/Out of Scope明确，客户端三平台优先、服务端Linux生产的策略合理
2. **平台差异分析专业**: 9维度对比表（事件循环/路径/证书/崩溃/打包等）准确且实用
3. **接口设计合理**: `platform_types.h`、`PathService`、`CredentialStore` 等抽象层设计符合架构原则
4. **文档规划完善**: 7个新增文档+3个更新文档覆盖全面
5. **风险意识强**: 预设了Qt版本漂移、公证失败、DLL缺失等常见问题

### ⚠️ 主要问题
1. **时间计划过于激进**: 4周完成三平台完整闭环（含MSI/DMG/签名/公证/诊断）不现实
2. **依赖管理策略模糊**: vcpkg、Qt版本、第三方库跨平台统一方案不明确
3. **MVP范围定义不清**: CredentialStore、CrashHandler 等在初期可能是过度设计
4. **技术决策缺失**: Qt版本、spdlog策略、WebEngine需求等关键选型未明确
5. **测试标准不匹配**: 性能/稳定性指标缺乏基线数据，20并发、95%成功率在MVP阶段过高

---

## 二、现状与计划差距分析

### 已实现（基于代码库审查）
| 维度 | 现状 | 对齐度 |
|---|---|:---:|
| 架构分层 | ✅ core/infra/app 三层清晰，core 零 Qt 依赖 | 100% |
| 服务端基础 | ✅ Go/Gin后端、JWT认证、SSE、Job编排已实现 | 90% |
| CI基础 | ✅ GitHub Actions 已配置（仅Ubuntu） | 30% |
| 客户端骨架 | ✅ Presenter/QML页面骨架、DTO定义已完成 | 60% |

### 缺失项（Critical Path）
| 维度 | 计划要求 | 当前缺失 | 风险等级 |
|---|---|---|:---:|
| 平台抽象层 | `app/platform` + `infra/platform` | **完全缺失** | 🔴 |
| 工具链配置 | `cmake/toolchains/*.cmake` | 无 | 🔴 |
| CI矩阵 | Win/Mac/Linux 三平台 | 仅Linux | 🔴 |
| 依赖管理 | vcpkg/conan统一方案 | 依赖系统包 | 🟡 |
| 打包脚本 | CPack + MSI/DMG配置 | 无 | 🔴 |
| 崩溃诊断 | PDB/dSYM保留策略 | 无 | 🟡 |
| 签名公证 | codesign/notarytool流程 | 无 | 🔴 |

---

## 三、关键技术决策缺失

### 1. Qt版本锁定策略
**当前状态**: `find_package(Qt6 REQUIRED)` 无版本限制
**问题**: Qt 6.5/6.6/6.7 API差异可能导致跨平台构建失败
**建议**:
```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2)
# 并在文档明确：Win/macOS 6.5+，Linux 6.2+（Ubuntu 22.04 repo版本）
```

### 2. spdlog 策略
**当前状态**: `option(STV_USE_SYSTEM_SPDLOG)` 可选，默认禁用
**问题**: 跨平台日志行为不一致，生产环境诊断困难
**建议**: 锁定为 **FetchContent 方式统一使用 v1.12.0**，移除 `SPDLOG_DISABLED` 分支

### 3. libcurl 后端选择
**当前状态**: `find_package(CURL REQUIRED)`
**问题**:
- Windows 默认 Schannel vs WinSSL
- macOS 默认 SecureTransport vs OpenSSL
- 证书验证行为差异

**建议**:
- **短期**: 统一使用 OpenSSL 后端（vcpkg 安装）
- **长期**: 计划第2阶段切换到原生后端（Schannel/SecureTransport）

### 4. 第三方库管理
**当前状态**: Linux 使用系统包（apt-get libcurl4-openssl-dev）
**问题**: Windows/macOS 无统一方案，版本漂移风险高
**建议**: 采用 **vcpkg manifest mode**
```cmake
# vcpkg.json
{
  "dependencies": [
    "curl[ssl]",
    "spdlog",
    "nlohmann-json"  // 建议引入，用于 JSON 解析
  ]
}
```

---

## 四、里程碑调整建议

### 原计划（4周，评估为 **不可行** ⚠️）
- Week 1: 跨平台抽象层
- Week 2: Windows 完整闭环
- Week 3: macOS 完整闭环
- Week 4: Linux对齐+文档

### 建议修订（8周，分两阶段）

#### **Phase 1: MVP跨平台构建（4周）**
**目标**: 三平台能本地构建运行，核心业务流可用

| Week | Workstream W (Windows) | Workstream M (macOS) | Workstream L (Linux) |
|---|---|---|---|
| 1 | vcpkg集成 + MSVC工具链 | Homebrew依赖 + Xcode工具链 | apt依赖锁版本 + GCC工具链 |
| 2 | PathService实现（APPDATA） | PathService实现（~/Library） | PathService实现（XDG） |
| 3 | 本地构建+运行验证 | Universal构建+运行验证 | 保持现有流程 |
| 4 | CI集成（构建+单测） | CI集成（构建+单测） | CI增加缓存优化 |

**交付物**:
- ✅ 三平台本地可构建运行
- ✅ CI 矩阵全绿（构建+core tests）
- ✅ 基础文档（PLATFORM_*.md 构建部分）
- ❌ **暂不做**: MSI/DMG打包、签名公证、CredentialStore、CrashHandler

#### **Phase 2: 生产就绪（4周）**
**目标**: 打包、签名、诊断能力完善

| Week | 交付内容 |
|---|---|
| 5 | Windows MSI + windeployqt |
| 6 | macOS DMG + codesign/notarytool |
| 7 | CrashHandler集成（可考虑Sentry SDK） |
| 8 | 性能基线建立 + 文档补全 |

---

## 五、具体修正建议

### 5.1 接口设计优化

#### ❌ 问题：CredentialStore 在 MVP 阶段过度设计
```cpp
// 计划中的接口
class CredentialStore {
  virtual void save_token(const std::string& key, const std::string& token) = 0;
  virtual std::string load_token(const std::string& key) = 0;
};
```

#### ✅ 建议：Phase 1 使用文件存储，Phase 2 再做平台原生
```cpp
// Phase 1: 简单文件存储（已足够安全，token有过期时间）
// infra/include/infra/token_storage.h
class TokenStorage {
public:
  void save(const std::string& access_token, const std::string& refresh_token);
  std::pair<std::string, std::string> load();
  void clear();
private:
  std::string get_token_file_path(); // 使用 PathService
};

// Phase 2: 再抽象为平台原生
class SecureCredentialStore : public TokenStorage { /*Keychain/CredentialManager*/ };
```

### 5.2 崩溃诊断策略

#### ❌ 问题：自研CrashHandler工作量大且易出错
**原计划成本**:
- Windows: SetUnhandledExceptionFilter + MiniDumpWriteDump
- macOS: mach exception + crash reporter集成
- 符号化流程自行维护
- **预估**: 2-3周开发 + 持续维护

#### ✅ 建议：集成 Sentry Native SDK
```cmake
# CMakeLists.txt
include(FetchContent)
FetchContent_Declare(sentry
  URL https://github.com/getsentry/sentry-native/releases/download/0.7.0/sentry-native.zip
)
FetchContent_MakeAvailable(sentry)

target_link_libraries(stv_app PRIVATE sentry)
```

**优势**:
- 三平台开箱即用
- 自动上传符号
- 提供云端符号化
- **成本**: 2-3天集成

### 5.3 代理服务策略

#### ❌ 问题：ProxyService实现复杂
**跨平台差异**:
- Windows: WinHTTP/WinINet API
- macOS: CFNetworkCopySystemProxySettings
- Linux: 环境变量 + GNOME/KDE settings

#### ✅ 建议：libcurl自动处理
```cpp
// infra/src/curl_http_client.cpp
void CurlHttpClient::configure_proxy() {
  // libcurl 默认会读取环境变量 http_proxy/https_proxy
  // Windows/macOS 通过 CURLOPT_PROXY 自动检测系统代理
  curl_easy_setopt(curl_, CURLOPT_PROXY, "");  // 空字符串 = 自动检测
}
```
**Phase 2 再做**: 提供UI配置项覆盖系统代理

### 5.4 CI 配置模板

```yaml
# .github/workflows/ci.yml (修订版)
name: Cross-Platform CI

on: [push, pull_request]

jobs:
  # ============ Phase 1 实现 ============
  build-matrix:
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: windows-latest
            preset: windows-msvc
            vcpkg_triplet: x64-windows
          - os: macos-14  # M1
            preset: macos-universal
          - os: ubuntu-22.04
            preset: linux-gcc

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Setup vcpkg (Win/macOS)
        if: runner.os != 'Linux'
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'a42af01b72c28a8e1d7b48107b33e4f286a55ef6'  # 锁定版本

      - name: Install system deps (Linux)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y libcurl4-openssl-dev libspdlog-dev qt6-base-dev

      - name: Configure
        run: cmake --preset ${{ matrix.preset }}

      - name: Build
        run: cmake --build --preset ${{ matrix.preset }}

      - name: Test
        run: ctest --preset ${{ matrix.preset }}

  # ============ Phase 2 实现 ============
  package-windows:
    needs: build-matrix
    runs-on: windows-latest
    steps:
      # ... MSI 打包

  package-macos:
    needs: build-matrix
    runs-on: macos-14
    steps:
      # ... DMG 打包 + 公证
```

---

## 六、测试验收标准调整

### 原计划问题
| 指标 | 原计划 | 问题 |
|---|---|---|
| 冷启动 p95 < 2.5s | 无基线数据 | 当前值未知 |
| 空闲内存 < 300MB | Qt6 Minimal 已 ~200MB | 目标过严 |
| 导出成功率 > 95% | 依赖后端稳定性 | 客户端无法控制 |
| 20并发任务 | 服务端当前是mock | MVP不现实 |

### 建议分阶段标准

#### Phase 1 (MVP)
✅ **功能验收** (MUST)
- [ ] 三平台能启动并显示登录页
- [ ] 登录后能创建项目
- [ ] 能编辑分镜并发起生成任务
- [ ] SSE事件能正常接收并更新UI
- [ ] 能取消任务

✅ **单元测试** (MUST)
- [ ] `PathService` 三平台路径规范一致性
- [ ] `TokenStorage` 读写幂等性
- [ ] `ApiClient` 错误重试逻辑
- [ ] `SseClient` 断线重连

❌ **暂不要求**
- 性能指标（需先建立基线）
- 并发压力（等后端就绪）
- 崩溃恢复（Phase 2）

#### Phase 2 (生产就绪)
✅ **性能基线建立**
```bash
# 使用 hyperfine 建立基线
hyperfine --warmup 3 './stv_app --quit-after-startup'
# 示例输出: 平均 1.8s ± 0.2s

# 目标：p95 不超过基线 +50%
```

✅ **稳定性测试**
- [ ] 连续运行12小时无内存泄漏（Valgrind/Instruments）
- [ ] 模拟网络中断后能自动重连
- [ ] 人为崩溃后下次启动能恢复草稿

---

## 七、文档策略建议

### 问题：计划中"中文主文档+英文命令"维护成本高

### 建议：采用 **结构化双语方案**

```
docs/
├── zh/                        # 中文主文档（面向国内开发者）
│   ├── PLATFORM_WINDOWS.md
│   ├── PLATFORM_MACOS.md
│   └── PLATFORM_LINUX.md
│
├── en/                        # 英文翻译（面向国际化）
│   └── PLATFORM_WINDOWS.md
│
├── PLATFORM_WINDOWS.md        # 英文主文档（命令/代码为主，注释用英文）
├── PLATFORM_MACOS.md
└── README.md                  # 双语索引
```

**优先级**:
1. Phase 1: 只写英文文档（代码注释+命令为主，描述简洁）
2. Phase 2: 补充中文翻译（可用AI辅助）

---

## 八、风险预案补充

### 8.1 macOS 公证失败应对

**原计划不足**: "预设离线诊断脚本"不够具体

**补充方案**:
```bash
# 1. 提交公证
xcrun notarytool submit stv_app.dmg \
  --apple-id "dev@example.com" \
  --password "@keychain:NOTARY_PASS" \
  --team-id "TEAM_ID" \
  --wait

# 2. 失败时获取详细日志
xcrun notarytool log <submission-id> --apple-id "..." > notary.log

# 3. 常见问题自检清单
- [ ] 所有 dylib 已签名（codesign -dv --verbose=4 检查）
- [ ] Hardened Runtime 已启用（codesign -d --entitlements - 检查）
- [ ] 时间戳服务器可达（--timestamp 参数）
- [ ] 开发者ID证书在有效期内
```

### 8.2 Windows DLL 依赖检查

**原计划**: "MSI安装后自动运行依赖自检脚本"

**问题**: MSI CustomAction 易出错，用户可能禁用

**改进方案**:
```cmake
# 使用 windeployqt + 验证脚本
install(CODE [[
  execute_process(
    COMMAND ${WINDEPLOYQT} --qmldir ${QML_DIR} $<TARGET_FILE:stv_app>
  )
  # 验证关键DLL存在
  file(GLOB DLLS "${CMAKE_INSTALL_PREFIX}/*.dll")
  list(LENGTH DLLS DLL_COUNT)
  if(DLL_COUNT LESS 10)
    message(FATAL_ERROR "windeployqt failed: only ${DLL_COUNT} DLLs found")
  endif()
]])
```

**补充**: 提供 `stv_app.exe --diagnose` 命令行参数，输出依赖检查结果

### 8.3 Qt 版本漂移应对

**补充锁版本策略**:
```cmake
# CMakeLists.txt
set(QT_MIN_VERSION "6.5.0")
set(QT_MAX_VERSION "6.7.99")  # 不支持 Qt 7

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Quick)

if(Qt6_VERSION VERSION_GREATER ${QT_MAX_VERSION})
  message(FATAL_ERROR "Qt ${Qt6_VERSION} not tested, max ${QT_MAX_VERSION}")
endif()
```

**CI策略**: 矩阵测试 Qt 6.5/6.6/6.7 三个版本

---

## 九、成本与优先级重估

### 原计划工作量（乐观估算）
| Workstream | 原计划 | 实际预估 | 差距 |
|---|---|---|---|
| Windows适配 | 1周 | 2-3周 | 🔴 |
| macOS适配 | 1周 | 2-3周 | 🔴 |
| 签名公证 | 隐含在W/M | 1-2周 | 🔴 |
| 文档编写 | 1周 | 1.5周 | 🟡 |
| **总计** | **4周** | **7-10周** | **+75%~150%** |

### 建议优先级排序

#### P0 (MVP 必须，Phase 1)
1. ✅ vcpkg 依赖统一
2. ✅ CMake 工具链配置
3. ✅ PathService 平台实现
4. ✅ CI 三平台矩阵（构建+测试）
5. ✅ 基础文档（构建指南）

#### P1 (生产就绪，Phase 2)
6. ✅ MSI/DMG 打包
7. ✅ 签名与公证
8. ✅ TokenStorage 文件实现
9. ✅ 崩溃诊断（Sentry集成）
10. ✅ 完整文档

#### P2 (可选增强，Phase 3)
11. ⏸️ CredentialStore 原生实现（Keychain/CredentialManager）
12. ⏸️ ProxyService UI配置
13. ⏸️ 性能监控面板
14. ⏸️ 自动更新机制

---

## 十、审核结论与行动建议

### 🔴 阻塞问题（必须解决）
1. **锁定Qt版本**: 建议 Qt 6.5.3（稳定且三平台官方支持）
2. **确定依赖管理方案**: 强烈推荐 vcpkg manifest mode
3. **调整里程碑**: 采用 Phase 1 (4周 MVP) + Phase 2 (4周生产) 方案
4. **降低测试标准**: Phase 1 仅功能验收+单元测试，Phase 2 再做性能/压力

### 🟡 建议改进（推荐采纳）
5. **简化CredentialStore**: Phase 1 用文件存储
6. **使用Sentry**: 代替自研CrashHandler
7. **libcurl代理自动检测**: 代替自研ProxyService
8. **双语文档策略**: 英文优先，中文按需翻译

### ✅ 保持不变（已足够好）
9. **平台差异9维度表**: 准确且实用
10. **接口抽象设计**: PathService等设计合理
11. **范围界定**: 客户端三平台、服务端Linux生产策略正确

### 📋 下一步行动
1. **技术选型会议**（1天）
   - 确定Qt版本、vcpkg策略、Sentry vs 自研
   - 输出 `docs/TECHNICAL_DECISIONS.md`

2. **创建 Phase 1 Task List**（半天）
   - 分解为 20-30 个具体任务
   - 分配到 Workstream W/M/L

3. **搭建CI骨架**（2天）
   - 先让三平台"Hello World"跑通
   - 验证 vcpkg 缓存策略

4. **更新文档**（1天）
   - 修订 `CROSS_PLATFORM_COMPAT_PLAN.md`
   - 新增 `PHASE1_MVP_PLAN.md`

---

## 附录：参考资料

### Qt 跨平台最佳实践
- [Qt 6.5 Supported Platforms](https://doc.qt.io/qt-6/supported-platforms.html)
- [Qt for Windows - Deployment](https://doc.qt.io/qt-6/windows-deployment.html)
- [Qt for macOS - Deployment](https://doc.qt.io/qt-6/macos-deployment.html)

### vcpkg 集成
- [vcpkg Manifest Mode](https://learn.microsoft.com/en-us/vcpkg/users/manifests)
- [CMake Integration](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration)

### macOS 签名公证
- [Notarizing macOS Software](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution)
- [Hardened Runtime](https://developer.apple.com/documentation/security/hardened_runtime)

### 崩溃诊断
- [Sentry Native SDK](https://docs.sentry.io/platforms/native/)
- [Google Breakpad](https://chromium.googlesource.com/breakpad/breakpad/)

---

**审核签名**: Claude (Sonnet 4.5)
**建议有效期**: 2026-02-17 至 2026-03-17（1个月内技术环境稳定）
**复审触发条件**: Qt 7发布、Sentry重大变更、团队规模翻倍
