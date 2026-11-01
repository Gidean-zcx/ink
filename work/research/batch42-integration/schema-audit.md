# Batch42 资料结构与校验约束审计

日期：2026-10-27。范围：只读检查正式资料说明、当前索引、`merge_batches.py` 与 `check_docs.py`；未联网，未修改 `docs/`，未重跑旧 integration 或 merge。

## 当前聚合状态

本轮直接运行现有结构检查，结果通过：

- 65 个产品、65 个部分案例。
- 929 条 F：D1 77、D2 834、D3 0、D4 18。
- 1631 条 E，1463 个不同 URL。
- 221 条已查看静态视觉 E；19 条满足当前自动化浏览器操作审计规则的 E。
- 245 个 Markdown、1958 个本地链接，`errors=[]`。
- `reference-points.csv` 当前38行，15字段，`reference_id`唯一，当前没有空单元格。

这些数字是当前正式 master/batches 的登记量，不是竞品完整率，也不包含尚未写成正式 batch 的 Batch42 草稿。

## Batch42 增量文件应遵循的结构

### 1. 批次配对与合并方式

每个正式主题批次使用同名前缀的一对文件：

- `docs/research/interaction-library/batches/<batch-name>-evidence.jsonl`
- `docs/research/interaction-library/batches/<batch-name>-coverage.csv`

`merge_batches.py` 会读取 `batches/*-evidence.jsonl` 和 `batches/*-coverage.csv`，分别按 `evidence_id`、`flow_id` 合并。相同 ID 若内容不完全相同会立即抛出 `ValueError("Conflicting ...")`，不能靠文件排序静默覆盖。合并结果按 `(product_id, ID)` 排序后重写 master `evidence.jsonl`、`coverage.csv`，并基于所有 F 更新 `catalog.csv` 和重新生成 `case-index.md`。

因此：

- 新 E 使用未占用稳定编号；同 URL 新快照通常应追加原 E 的 `additional_captures`，不应只因复读再造来源。
- 补旧任务沿用原 F，并修改原持有批次的该行；只有新的用户任务才新增 F。
- 案例正文必须同步包含该 F；增删 F 不能只改 coverage。
- 批次 inventory 可另存，但不参与 F/E 聚合，也不能把目录节点当任务数。
- `case_path`、正式 artifact 路径相对 `docs/research/interaction-library/`；`raw_work_path` 相对项目根目录。

### 2. coverage CSV 的11个正式字段

当前字段顺序与含义：

1. `flow_id`
2. `product_id`
3. `task`
4. `depth`
5. `evidence_ids`
6. `known_branches`
7. `gaps`
8. `visual_status`
9. `hands_on`
10. `case_path`
11. `checked_on`

约束：

- `flow_id` 全库唯一；`product_id` 必须存在于 `catalog.csv`。
- `evidence_ids` 用分号分隔，全部 E 必须存在。
- `flow_id` 必须实际出现在 `case_path` 指向的案例正文中；案例正文出现的全部 F 与指向该案例的 coverage F 必须集合完全相等。
- 非操作任务当前只允许 `depth=D1|D2`、`hands_on=false`，且 `visual_status` 只能是 `not_observed`、`static_reference_only`、`static_official_images_only`、`static_manual_pages_only`。
- 当前检查器没有接受普通 D3 的分支；若直接写 D3 会被判为 `Unsupported operation/depth claim`。连续视觉材料仍应在扩展校验规则前保持现行证据等级或由根明确更新脚本/约定。
- D4 的 coverage 组合见下节。

### 3. evidence JSONL 的基本字段

`data-dictionary.md`列出的当前基础字段是：

- `evidence_id`、`product_id`、`title`、`url`
- `source_type`、`access_mode`
- `checked_on`、`source_updated_on`
- `version_scope`、`app_build`
- `locator`、`note`
- `hands_on`、`visual_observed`
- `archived_artifact`、`sha256`
- `supersedes`

当前主记录还常见 `firmware_version`；获取与视觉扩展字段按事实加入，如 `raw_work_path`、`raw_sha256`、`parsed_text_path`、`additional_captures`、`derived_from`、`artifact_hashes`、`visual_kind`、`visual_audit_path`、`visual_audit_sha256`。脚本对部分字段直接用下标访问，因此新记录至少应保留现有基础字段，即使未知也写 JSON `null`，不要省略 `hands_on`、`visual_observed`、`archived_artifact`、`sha256`、`supersedes`。

通用检查：

- `url` 必须是无空白、具有 host 的 HTTP(S) URL。
- `product_id` 必须存在；`evidence_id` 全库唯一。
- `supersedes`、`derived_from` 若非 null，目标 E 必须存在。
- `archived_artifact` 非 null 时文件必须存在，`sha256` 必须与实算一致。
- `visual_observed=true` 且没有 `archived_artifact` 时，必须提供存在的 `visual_audit_path`、正确的 `visual_audit_sha256`，且审计正文包含本 E 编号。
- `artifact_hashes` 中每个资料库相对路径都必须存在并与散列一致。
- HTTP 200、下载文件或 DOM 中发现控件不自动证明正文、视觉观察或操作成功。

### 4. RP 文件组织

RP 不放入 batch F/E 对，也不计入流程或证据数；当前只有资料库根目录：

`docs/research/interaction-library/reference-points.csv`

当前38行，15字段：

`reference_id, product_id, question_tags, flow_ids, evidence_ids, source_locator, artifact_path, precondition, action, visible_result, evidence_basis, persistence_result, unknown_branches, version_scope, checked_on`

用途是“设计问题→具体前置状态/动作/可见结果→F/E→原图或审计”的增量检索层，不是第二套功能表或完整状态机。现有约定要求：

- `reference_id` 稳定且唯一。
- `flow_ids`、`evidence_ids`用分号分隔并回指现有 F/E。
- `artifact_path`相对资料库，直达图、页面摘记或操作审计。
- `question_tags`用于筛选，不代表全库标签穷尽。
- `precondition`、`action`、`visible_result`、`persistence_result`、`unknown_branches`分开写，不能用“支持某功能”替代状态结果。
- `evidence_basis`现有值包括 `manual_diagram_static`、`official_help_documented`、`official_static_images`、`automated_public_browser`；前三者不能写成操作成功。
- 当前 `check_docs.py` **没有读取或校验 RP**；Batch41 对38条RP的唯一性、F/E、附件和字段完整性是另做的人工/Python检查。Batch42若增RP，应继续独立执行这组检查，并同步 guide/dictionary 中当前行数。

## 当前新增 D4 的硬性条件

按当前 `check_docs.py`，D4 暂时只支持“自动化公开网页”这一种可接受组合。若 Batch42 新增 D4，coverage 行必须同时为：

- `depth=D4`
- `hands_on=true`（CSV字符串）
- `visual_status=observed_browser_sequence`
- `evidence_ids` 至少关联一条满足以下全部条件的 E

该支持 E 必须有：

- `hands_on: true`（JSON布尔值）
- `operation_method: "automated_public_browser"`
- `visual_kind: "browser_sequence"`
- `visual_observed: true`
- `operation_audit_path`：资料库相对路径，文件存在
- `operation_audit_sha256`：与该审计文件实算一致
- 审计正文同时包含该 `evidence_id`，并包含所支持的 `flow_id`
- `artifact_hashes` 至少包含一个路径以 `observed-states.json` 结尾、一个以 `trace-actions.json` 结尾；二者及其他登记附件必须存在且散列一致

此外，操作审计按说明应记录环境、逐步动作、前后可见状态、失败和未测范围，并保留按序截图/动作日志。D4表示那一次具体操作分支可复查，不表示全部成功、实体设备试用或全产品完成。

虽然 `batches/README.md` 在概念上允许未来增加人工设备或远程操作方法，**当前检查器的 `audited_browser()` 只接受 `automated_public_browser`**。Batch42若取得其他类型操作，不能直接标 `hands_on=true/D4`；需先由根代理明确扩展字段规范和检查器，否则全库检查必然失败。

## 合并与检查脚本的实际边界

`merge_batches.py` 会写 master、catalog 和 case-index，因此本轮只读审计没有运行它。它不会验证来源内容是否足以支撑 D2/D4，也不会读取 RP。

`check_docs.py`检查：

- Markdown结尾换行、行尾空格和本地链接存在性；
- M/F/E唯一性与产品、案例、证据引用；
- coverage与案例F集合；
-静态附件、视觉审计、操作审计和散列；
- `docs/**/*.json`语法；
- master F/E 与全部 batch 合并结果完全一致。

它不自动判断：来源质量、正文是否真的支持任务、型号/版本错配、任务闭环是否完整、图片是否被正确解读、RP文字是否成立。正式合并仍需根代理阅读新增案例/摘记并做语义验收。

## 执行记录

1. 定位规范和脚本：

   ```bash
   rg --files docs/research/interaction-library work/research/interaction-depth | rg '(^|/)(README\\.md|case-index\\.md|design-reference-guide\\.md|data-dictionary\\.md|schema[^/]*|check_docs\\.py|merge_batches\\.py)$' | sort
   ```

   Exit 0。没有独立 `schema.*` 文件；实际 schema 分散在 `data-dictionary.md`、`batches/README.md` 和检查脚本。

2. `sed`/`cat`读取：

   - `docs/research/interaction-library/README.md`
   - `case-index.md`
   - `design-reference-guide.md`
   - `data-dictionary.md`
   - `batches/README.md`
   - `work/research/interaction-depth/check_docs.py`
   - `work/research/interaction-depth/merge_batches.py`

   Exit 0。

3. 当前全库检查：

   ```bash
   python3 work/research/interaction-depth/check_docs.py
   ```

   Exit 0；输出本报告开头的聚合数，`errors=[]`。这不是重跑 integration 或 merge。

4. 读取 `reference-points.csv` 表头并以 Python 统计行数、唯一ID和空单元格：exit 0；38行、15字段、ID唯一、0空单元格。

5. 写入本报告并计算散列：exit 0。

## 未知与状态变化

- Batch42 尚未生成正式 batch 对、F/E编号或RP，因此本报告不判断它最终应新增多少条记录。
- 当前脚本没有为新操作方法或D3建立通过路径；是否扩展必须由根代理决定，不能由本审计推断。
- 本轮状态变化仅新增 `work/research/batch42-integration/schema-audit.md`；未修改 `docs/`，未运行 merge，未重跑任何旧 integration。
