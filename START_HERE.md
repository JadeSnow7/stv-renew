# 腾讯 PC 客户端 AI 面试调研启动指南（v2.1）

最后更新：2026-02-18

## 1. 先做这三件事（今天就能开始）
1. 打开 `search_keywords.md`，按 Day1 的前 4 条精准关键词在牛客执行搜索。
2. 将候选链接保存到 `week1_links_day1_raw.txt`（每行 1 条 URL）。
3. 用 `week1_checklist.md` 完成 Day1 上午与下午任务，产出 `week1_links_day1_qualified.txt`。

## 2. 你现在拥有的文件（执行顺序）
1. `tencent_pc_ai_interview_research_plan_v2.md`
   - v2.1 主计划（范围、分层目标、降级策略、验收）
2. `execution_config.md`
   - 锁定配置与阈值（MVP/Standard/Premium + M0 降级）
3. `week1_checklist.md`
   - 第一周每日 SOP（1-2 小时/天）
4. `search_keywords.md`
   - 可直接复制的检索词（牛客/知乎/GitHub/官方）
5. `sample_template.csv`
   - 结构化样本录入模板（含新增字段）
6. `scoring_guide.md`
   - 证据可信度评分与分级门槛
7. `week1_seed_links.md`
   - 已验证的起始证据池（7 条）

## 3. Week1 产出物（最小要求）
1. `week1_links_day1_raw.txt`：Day1 原始候选链接（>=10）
2. `week1_links_day1_qualified.txt`：Day1 初筛后链接（>=8）
3. `week1_links_master.csv`：Week1 汇总去重池（>=20）
4. `week1_samples.csv`：首批完整结构化样本（>=5）
5. `week1_summary.md`：周复盘（数量/质量/缺口/下周策略）

## 4. 决策规则（务必遵守）
1. 时间窗严格为 `2024-02-18 ~ 2026-02-18`。
2. `L1/L2` 为核心，`L3` 仅旁证，不单独支撑结论。
3. `PC+Qt` 样本不足时允许客户端泛化，但必须标记 `role_tag`。
4. 不允许编造题目或“无来源结论”。

## 5. 本周是否通过（Gate）
满足以下条件即 Week1 通过：
1. 合格链接池 >= 20
2. 完整样本 >= 5
3. `L1/L2` 占比 >= 50%
4. `PC+Qt` 样本 >= 2
5. 完成 `week1_summary.md`

## 6. 如果未通过怎么处理
1. 若合格链接 < 15：扩展泛化关键词，增加知乎/GitHub搜索权重。
2. 若 `PC+Qt` < 2：加强“PC客户端/Qt/Windows客户端”精准词，并单独追踪。
3. 若 `L1/L2` 占比 < 50%：降低 L3 收录比例，优先高互动原帖。

## 7. 目标路线图（摘要）
1. MVP（6 周）：样本 >= 30，PC+Qt >= 10，题库 >= 70，脚本 1 套
2. Standard（12-19 周）：样本 >= 60，PC+Qt >= 25，题库 >= 120，脚本 3 套
3. Premium：样本 >= 100，强化 AI 面专题与 BG 差异分析

执行时出现冲突或不确定项，统一按 `execution_config.md` 处理。
