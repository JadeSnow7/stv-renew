# Week1 执行清单（Day1-Day7）

> 目标：用 7 天建立可用样本池，形成“可继续滚动执行”的调研流水线。  
> 时间：每天 1-2 小时。

## Day1：牛客精准检索（PC/Qt/客户端）
### 上午（40-60分钟）
- [ ] 在牛客搜索以下关键词（每个关键词翻 2-3 页）
  - [ ] 腾讯 PC客户端 面经 C++
  - [ ] 腾讯 Qt 客户端 面经
  - [ ] 腾讯 Windows 客户端 校招 面经
  - [ ] 腾讯 PC 客户端 实习 面经
- [ ] 保存候选链接到 `week1_links_day1_raw.txt`（>=10）

### 下午（30-60分钟）
- [ ] 按条件初筛（24个月内 + 至少含岗位/轮次/问题之一）
- [ ] 结果写入 `week1_links_day1_qualified.txt`（>=8）
- [ ] 给每条加初始标签：`pc_client_cpp_qt|client_general|unclear`

---

## Day2：牛客泛化检索（客户端通用/C++）
- [ ] 搜索“客户端通用”关键词并补链（>=10）
- [ ] 搜索“C++通用”关键词并补链（>=5）
- [ ] 合并到 `week1_links_master_raw.txt`
- [ ] 强制标记 `role_tag`，禁止把泛化样本标成 PC+Qt

---

## Day3：知乎/GitHub 补充
- [ ] 知乎检索并补充 3-5 条高质量候选
- [ ] GitHub 面经仓库/issue 检索并补充 2-3 条
- [ ] 对新增样本标记来源层级（L2/L3）
- [ ] 仅 `L2及以上` 加入核心候选池

---

## Day4：官方侧证据收集
- [ ] 收集官方流程或招聘节奏证据（非题库来源）
- [ ] 记录到 `week1_official_evidence.md`
- [ ] 每条写明：来源、日期、可验证链接

---

## Day5：去重 + 分级
- [ ] 对 `week1_links_master_raw.txt` 去重（URL标准化 + 标题相似）
- [ ] 生成 `week1_links_master.csv`（目标 >=20 条）
- [ ] 补全字段：`source_tier`、`role_tag`、`published_at`
- [ ] 统计 `L1/L2` 占比（目标 >=50%）

---

## Day6：录入首批结构化样本
- [ ] 从 master 池选 5 条高价值样本
- [ ] 按 `sample_template.csv` 字段完整录入到 `week1_samples.csv`
- [ ] 对每条计算 `confidence_score`
- [ ] 标注覆盖域：`cpp/network/multithreading/qt/client_arch/semi_structured/algo`

---

## Day7：周复盘 + 下周策略
- [ ] 输出 `week1_summary.md`，包含：
  - [ ] 样本数量（总量、PC+Qt、泛化）
  - [ ] 来源质量（L1/L2/L3 占比）
  - [ ] 评分分布（>=70、60-69、40-59、<40）
  - [ ] 问题与风险
  - [ ] Week2 调整动作
- [ ] 判定是否进入正常节奏或触发降级（M0）

---

## Week1 Gate（通过标准）
- [ ] 合格链接池 >= 20
- [ ] 完整样本 >= 5
- [ ] `L1/L2` 占比 >= 50%
- [ ] `PC+Qt` 样本 >= 2
- [ ] `week1_summary.md` 完成

## 触发降级（M0）
满足任一条件触发：
- [ ] 第4周总样本 < 15
- [ ] 第4周 `PC+Qt` 样本 < 3

降级后目标：总样本 >= 20、题库 >= 50、脚本 1 套。
