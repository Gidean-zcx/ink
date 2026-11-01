# interaction-library 复用入口证据层级审计（2026-10-27）

## 判断

总体为 **有条件易用**：README 对 D1–D4、材料取得方式和“公开网页操作不等于目标硬件实测”解释清楚；design-reference-guide 也反复要求区分宣传、文档、静态图与真实操作。主要风险出现在从 `case-index.md` 或 `reference-points.csv` 直接检索时：关键区分没有作为可筛字段随行出现，设计者必须再跳 F/E 才能可靠判断。

## 最多五个具体问题

### 1. `reference-points.csv` 无法直接按 D2/D4 或 hands_on 筛选

- 证据：`reference-points.csv:1` 的15个字段只有 `evidence_basis`，没有 `depth`、`hands_on`、`operation_method`、`source_type`。
- 对照：`README.md:58–60` 明确说覆盖深度与取得方式是“两个维度”，并要求公开网页操作写 `hands_on=true`、`operation_method=automated_public_browser`。
- 影响：横向表是设计者的主筛选入口（`design-reference-guide.md:5`），但无法用结构化条件区分“D2文档/代码重建”和“D4操作”；必须逐个反查 coverage/E，容易把 `visible_result` 当观察结果。
- 可考虑措辞/检索修正：在指南第5行直接提醒“RP表不含D级/hands_on，须回F/E核对”，或以后给RP增加这几个派生字段。本次不修改。

### 2. `evidence_basis` 是自由文本枚举，缺少就地词典，宣传类也不能从RP直接识别

- 证据：`reference-points.csv:2` 用 `manual_diagram_static`，`:18` 用 `official_help_documented`，`:40` 用 `automated_public_browser`，`:90` 用 `official_document_text_reconstruction_no_ui_operation`，`:108` 用 `pinned_source_static_read_not_executed`；本次统计114行共有多种英文及中文表述。表头也没有 `source_type`。
- 对照：`README.md:60` 列出官方宣传、操作文档、索引文本、截图、源码、亲自操作、第三方报告等材料类型；`design-reference-guide.md:11` 称这些会“分别标记”。
- 影响：设计者不能稳定筛出“宣传功能”或统一合并“文档/代码重建”；同义值需要人工理解，且RP未收录不代表没有宣传依据。
- 可考虑措辞/检索修正：在RP旁给 `evidence_basis` 固定词典并明确它不是 `source_type`；宣传性质仍以E记录为准。

### 3. `case-index.md` 的“当前证据深度”容易被误读为证据可信度

- 证据：`case-index.md:13` 列名为“当前证据深度”；例如 `:74` QUADERNO 显示 `D2: 16`。本页顶部 `:7–9` 给数量及D4操作范围，但没有就地定义D2是任务重建而非实操。
- 对照：真正定义在 `README.md:58`：“D2 任务、步骤与分支重建”，“D4 有可复现的实际操作记录”；`:60` 又说明源码、演示、帮助页各有局限。
- 影响：只从案例索引进入的设计者可能把“D2:16”读成16项已有较强证据，尤其列名含“证据深度”。
- 可考虑措辞修正：索引列名改成“任务研究深度”，或在第7行加D2/D4简短定义和README锚点。本次不修改。

### 4. 部分D4型RP行的 `artifact_path` 只直达单帧，而非完整操作审计

- 证据：`reference-points.csv:40–47` 的 `evidence_basis=automated_public_browser`，但 `artifact_path` 分别指向单张PNG；完整动作只在 `source_locator` 以“操作审计文件名前缀…”描述。相对地，`:15–17` 的 Week Junior 行直接指向 `browser-operation-audit.md`。
- 对照：`design-reference-guide.md:5` 称每行可直达“原页/审计”；`:11` 要求从任务直达时间点/操作审计；`README.md:16` 强调环境、动作日志、截图和失败均应看审计。
- 影响：从Math类RP点击时最先看到的是结果单帧，容易漏掉动作顺序、失败和环境限定；单帧不能单独证明D4的可复现操作链。
- 可考虑检索修正：D4行增加独立 `operation_audit_path`，或让 `artifact_path` 优先指向审计并另设静态帧字段。本次不修改。

### 5. design-reference-guide 的表本身不显示层级，依赖自然语言警告和人工反查

- 证据：`design-reference-guide.md:15–16` 的表列仅“问题/入口/证据/必须核对”，没有D级、取得方式或hands_on列；`:21`、`:32`、`:35` 用自然语言说明静态/文档未实操，`:38`、`:53` 才在具体行写实际网页操作范围。
- 对照：`:11` 要求宣传、帮助、静态图、真实操作分别标记；`:70–74` 又要求引用最小单元走F/E/RP并保留限定条件。
- 影响：当前示例行大多写得谨慎，但区分依赖每格作者措辞，无法保证后续新增行一致，也不能一眼比较D2与D4。
- 可考虑措辞修正：在表前加一句“本表不展示D级/取得方式，必须经RP→F/E核对”，成本最低；若扩表再增加“证据层级/取得方式”列。

## 已经清楚、无需视为问题的部分

- `README.md:16` 明确当前没有手机/电子纸实机或登录原生App操作，D4可含未完成分支且不是通过率。
- `README.md:58–60` 对D1–D4和证据取得方式分维度定义充分。
- `design-reference-guide.md:3、9、11、70–76` 清楚限制“支持某功能”、静态图、文档步骤和真实保存之间的推断。
- `case-index.md:7–9` 对20条D4任务、21条浏览器证据、静态图和合成样本的计数边界写得准确。

## 命令、退出码与状态

- `pwd`；`rg --files docs/research/interaction-library | rg '(design-reference-guide\\.md|README\\.md|case-index|reference-points\\.csv)$'`；`find ...`：exit 0，定位四个目标文件。
- `nl -ba` 读取三份Markdown；`python3`逐行编号读取CSV：exit 0。合并输出过长被工具截断，随后用限定行段与关键词重新读取，未用截断部分单独下结论。
- `rg -n 'D1|D2|D3|D4|宣传|实操|实际操作|文档|代码|静态|证据|...'`：exit 0，定位层级定义和警告。
- `python3`以`csv.DictReader`读取 `reference-points.csv`：exit 0；确认15字段、114行，并统计 `evidence_basis` 等值。
- `nl -ba ... | sed ...` 与 `python3`打印指定CSV行：exit 0，复核所有引用行号和样例。
- 状态变化：仅新建 `work/research/batch50-integration/reuse-audit.md`；未修改正式资料，未联网，未创建子代理。

## 未知

本次只审四个入口，没有逐条验证114个RP对应的全部F/E内容，也未核对生成脚本是否已有预留字段；因此问题是“复用入口的可辨识性”，不是对底层证据正确性的否定。
