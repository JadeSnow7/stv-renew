# 执行配置锁定（execution_config）

版本：v2.1  
状态：LOCKED

## A. 固定策略
1. 时间窗：2024-02-18 ~ 2026-02-18。
2. 核心来源：L1/L2；L3 仅旁证。
3. 岗位优先级：PC 客户端 C++/Qt > 客户端通用 > C++通用。
4. 交付节奏：MVP -> Standard -> Premium。

## B. 分层阈值
### MVP（6周）
- total_samples >= 30
- pc_qt_samples >= 10
- l1_l2_ratio >= 50%
- question_bank >= 70
- scripts >= 1 (15min)

### Standard（12~19周）
- total_samples >= 60
- pc_qt_samples >= 25
- question_bank >= 120
- scripts >= 3 (30/45/60min)

### Premium
- total_samples >= 100
- ai_round_samples >= 15
- verified_feedback >= 5

## C. 降级触发（M0）
触发条件（第4周评估）：
- total_samples < 15 OR pc_qt_samples < 3

触发后目标：
- total_samples >= 20
- question_bank >= 50
- scripts >= 1

## D. 质量红线
1. 不允许无来源“传闻题”进核心结论。
2. 超过24个月样本不得作为核心证据。
3. 单条 L3 样本不得单独支撑结论。
4. 冲突结论必须入 `ConflictRecord`。

## E. Week1 Gate
- qualified_links >= 20
- full_samples >= 5
- l1_l2_ratio >= 50%
- pc_qt_samples >= 2
- week1_summary done
