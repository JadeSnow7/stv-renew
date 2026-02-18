# 腾讯 PC 客户端 AI 面试深度调研执行计划 v2.1

> 版本：v2.1  
> 时间窗：2024-02-18 ~ 2026-02-18（近24个月）  
> 范围：腾讯 PC 客户端（校招/实习）优先，允许客户端泛化补齐  
> 原则：高可信优先、分层交付、可降级保底

---

## Summary
目标不变：深入调查腾讯 PC 客户端（校招/实习）面试**内容 + 形式**，覆盖半结构化、客户端知识、C++、网络、多线程、Qt、AI 面。  
执行策略升级为“**可执行方法 + 分层目标 + 降级保底**”，确保样本不足时仍可交付可用版本。

---

## 1. 已锁定范围与策略
1. 时间窗：`2024-02-18 ~ 2026-02-18`（近24个月）。
2. 样本策略：`高可信优先`，L1/L2 为核心，L3 只作旁证。
3. 岗位策略：`腾讯PC客户端校招/实习优先`，允许客户端泛化补齐。
4. 交付策略：`MVP -> Standard -> Premium` 三层，不因样本不足导致计划失败。
5. 下一步执行：按 `Week1 实战样本池` 先跑通。

---

## 2. 关键风险修复（对应三项致命风险）
1. 执行方法缺失  
- 修复：新增“日任务 SOP + 评分 + 去重 + 冲突处理 + 验收门槛”。
2. 样本量不现实  
- 修复：将 `PC+Qt>=25` 作为 Standard 目标，MVP 调整为 `PC+Qt>=10`。
3. 无降级策略  
- 修复：新增降级链路 `MVP(30) -> M0(20)`，并给出触发条件与止损动作。

---

## 3. 公共接口/类型（固定数据契约）

### 3.1 InterviewSample
```json
{
  "sample_id": "string",
  "dedupe_fingerprint": "string",
  "source_url": "string",
  "source_tier": "L1|L2|L3",
  "published_at": "YYYY-MM-DD",
  "time_decay_weight": 0.3,
  "cohort": "intern|campus",
  "bg_team": "WXG|PCG|CSIG|IEG|unknown",
  "role_tag": "pc_client_cpp_qt|client_general|cpp_general|unclear",
  "rounds": "assessment|ai_round|tech_1|tech_2|manager|hr",
  "mentions_ai_round": true,
  "detail_richness": "high|medium|low",
  "engagement_score": 0,
  "confidence_score": 0,
  "verified_by_real_interview": false,
  "notes": "string"
}
```

### 3.2 QuestionItem
```json
{
  "question_id": "string",
  "domain": "cpp|network|multithreading|qt|client_arch|semi_structured|algo",
  "subdomain": "string",
  "question_text": "string",
  "round_type": "ai_round|tech|manager|hr",
  "frequency_level": "high|medium|low",
  "frequency_count": 0,
  "difficulty": "L1|L2|L3",
  "expected_answer_points": ["string"],
  "follow_up_questions": ["string"],
  "common_pitfalls": ["string"],
  "last_seen_at": "YYYY-MM-DD",
  "source_ids": ["S001", "S002"]
}
```

### 3.3 ConflictRecord
```json
{
  "conflict_id": "string",
  "topic": "string",
  "conflicting_claims": [
    {"claim": "string", "source_ids": ["S001"], "confidence": 80},
    {"claim": "string", "source_ids": ["S010"], "confidence": 62}
  ],
  "resolution": "adopt_claim_1|list_both|needs_more_data",
  "resolution_reason": "string"
}
```

### 3.4 文件级阻塞项
- 阻塞：`search_keywords.md` 缺失将影响 Week1 执行效率。  
- 处理：本计划已补齐 `search_keywords.md`；若再次缺失，使用“备用关键词包”。

---

## 4. Week1 可执行 SOP（每天1-2小时）

### Day1：牛客精准检索（PC/Qt/客户端）
- 目标：收集 `10-15` 条候选链接，初筛后保留 `>=8`。  
- 判定：24个月内，且至少含岗位/轮次/问题之一。

### Day2：牛客泛化检索（客户端通用/C++）
- 目标：新增 `10-12` 条合格链接。  
- 规则：标记 `role_tag=client_general/cpp_general`，不得冒充 PC+Qt。

### Day3：知乎/GitHub 补充与旁证
- 目标：补 `5-8` 条；仅 L2 及以上进入核心池。

### Day4：官方侧证据收集
- 目标：补流程与招聘趋势证据（非题库来源）。

### Day5：去重与分级
- 输出：干净链接池 `>=20`，统计 L1/L2 占比。

### Day6：录入首批结构化样本
- 输出：`>=5` 条完整样本（含 confidence_score）。

### Day7：周复盘与策略切换
- 输出：`week1_summary.md`（数量、质量、缺口、下周修正）。

---

## 5. 分层目标与降级触发

### MVP（6周）
1. 总样本 `>=30`
2. PC+Qt `>=10`
3. L1/L2 占比 `>=50%`
4. 题库 `>=70题`（7域各10）
5. 脚本：15分钟技术面 1 套

### Standard（12~19周）
1. 总样本 `>=60`
2. PC+Qt `>=25`
3. 题库 `>=120题`
4. 脚本：30/45/60分钟 3 套

### Premium
1. 总样本 `>=100`
2. AI 面专题样本 `>=15`
3. 引入真实面试反馈 `>=5`
4. 增加 BG 差异分析和季度更新机制

### M0 降级（若4周未达标）
- 触发：总样本 `<15` 或 PC+Qt `<3`  
- 目标：总样本 `>=20`、题库 `>=50`、脚本 1 套  
- 说明：先保住可训练交付，再迭代升层。

---

## 6. 评分与证据门槛（固定）
1. `confidence_score = source + time + detail + engagement + bonus/penalty`
2. 结论级别门槛：
- 核心结论：样本分数 `>=70` 且至少2条证据
- 题库主来源：`>=60`
- `<40` 不入库
3. 冲突处理：
- 不强行合并；保留争议并标注适用范围（BG/岗位/年份）。

---

## 7. 测试与验收场景
1. 去重测试：同一面经转载能否被 `dedupe_fingerprint` 合并。  
2. 时效测试：24个月外样本不得进入核心结论。  
3. 质量测试：L1/L2 占比是否达到阈值。  
4. 覆盖测试：7大域是否均达到最小题量。  
5. 可训练测试：每题是否有“要点+追问+易错点”。  
6. 复现实验：第三方按文档在30分钟内可跑通 Day1 流程。

---

## 8. 初始已验证证据池（Week1 起点）
1. 腾讯暑期实习PC客户端一二三面（牛客，2025-03）  
- https://www.nowcoder.com/feed/main/detail/3d5e041f215740a599b81fc28f213f7a
2. 腾讯PC客户端开发实习面经（牛客，2024-04）  
- https://www.nowcoder.com/feed/main/detail/73e26eead30d47b4bdfcdfc464e46647
3. 腾讯PC客户端实习一面（牛客，含Qt/FFmpeg项目追问，2024-10）  
- https://www.nowcoder.com/feed/main/detail/9a6dac0308a64122a1d337c4cee270f8
4. 腾讯AI面试面经补充（牛客）  
- https://www.nowcoder.com/feed/main/detail/7cee87ca1e24418a92b081a3162aebb0
5. 腾讯暑期实习OC（牛客，含面委会+HR，2025-04）  
- https://www.nowcoder.com/feed/main/detail/ae204b5154304886a4223d939cb822ca
6. 腾讯 RTC 官方 AI 面试场景文档（官方产品侧证据）  
- https://trtc.io/zh/document/75309
7. 腾讯2025实习扩招（央广网，官方信息引用）  
- https://tech.cnr.cn/techph/20250225/t20250225_527080990.shtml

---

## 9. 备用关键词包（`search_keywords.md` 不可用时）
1. `site:nowcoder.com/feed/main/detail 腾讯 PC客户端 面经 C++`
2. `site:nowcoder.com/feed/main/detail 腾讯 Qt 客户端 面经`
3. `site:nowcoder.com 腾讯 客户端 面委会 HR 面经`
4. `site:nowcoder.com 腾讯 AI面试 面经`
5. `site:nowcoder.com 腾讯 暑期实习 客户端 一面 二面`
6. `site:zhihu.com 腾讯 客户端 面经 C++ Qt`
7. `site:github.com 腾讯 面经 客户端 C++`

---

## 10. 明确假设与默认值
1. 默认“腾讯官方招聘流程页难以直接结构化抓取”，流程证据可用高校就业站/权威媒体旁证。  
2. 默认 `PC+Qt` 精准样本稀缺，MVP 按 `>=10` 执行。  
3. 默认 AI 面样本以“候选人回忆 + 官方产品能力文档”联合推断，并标注置信度。  
4. 默认先交付可训练版本，再迭代到标准版，不等待“完美数据”。

---

## 11. 落地文件清单（执行用）
1. `search_keywords.md`：检索词库  
2. `week1_checklist.md`：Week1 日任务  
3. `week2_to_week6_checklist.md`：Week2-Week6 周任务  
4. `sample_template.csv`：InterviewSample 录入  
5. `question_bank_template.csv`：QuestionItem 录入  
6. `conflict_records_template.csv`：ConflictRecord 记录  
7. `evidence_claim_map_template.csv`：结论-证据映射  
8. `interview_script_templates.md`：模拟面脚本模板  
9. `mvp_delivery_checklist.md`：MVP 验收
