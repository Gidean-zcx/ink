# 第47批 Shortcuts 整合结构审计

日期：2026-10-27。范围：本地只读核查正式资料；本文件是唯一新增状态。

## 1. M-043 既有 12 个 F 的唯一持有批次

- `F-M043-01`～`F-M043-06`：`docs/research/interaction-library/batches/ai-coverage.csv`
- `F-M043-07`～`F-M043-12`：`docs/research/interaction-library/batches/shortcuts-controls-deepening-coverage.csv`
- 每个 F 当前恰好出现于一个批次文件。补证必须更新上述原持有行；不能把同 ID 再写进新 coverage 批次，否则 `merge_batches.py` 在内容不同时报 `Conflicting`，内容相同虽可合并但违反批次约定的“同一任务补证更新所属批次行”。

## 2. RP 与字段

- 当前 `reference-points.csv` 共 88 行，没有 `RP-M043-*`，也没有 `product_id=M-043` 的行。
- 精确 15 列：`reference_id,product_id,question_tags,flow_ids,evidence_ids,source_locator,artifact_path,precondition,action,visible_result,evidence_basis,persistence_result,unknown_branches,version_scope,checked_on`。
- 批次 41～46 的专用校验做法要求：表头精确一致、RP ID 唯一、15 列均非空、分号拆出的 F/E 都存在、`artifact_path` 相对资料库且文件存在。通用 `check_docs.py` 当前不检查 RP。
- 新 ID 可从 `RP-M043-01` 起连续编号；当前没有碰撞。

## 3. 新批次文件命名与格式

- 合并器只读取 `docs/research/interaction-library/batches/*-evidence.jsonl` 和 `*-coverage.csv`，按稳定 ID 合并；同 ID 不同内容直接失败。
- 为避免和既有 `shortcuts-controls-deepening-{evidence.jsonl,coverage.csv}` 重名，第47批新增 E/F 若确有需要，建议成对命名为 `shortcuts-remaining-controls-evidence.jsonl` 与 `shortcuts-remaining-controls-coverage.csv`（名称可换，但两个后缀必须精确）。目录台账不进主合并，可命名 `shortcuts-remaining-controls-inventory.csv`。
- coverage 必须保持 11 列及顺序：`flow_id,product_id,task,depth,evidence_ids,known_branches,gaps,visual_status,hands_on,case_path,checked_on`。非操作文档任务应为 D1/D2、`hands_on=false`，视觉状态只能使用当前允许的四类之一；每个 F 必须写入对应案例，E 引用必须存在。
- evidence JSONL 每行一个 JSON 对象。现行基础 17 字段为：`evidence_id,product_id,title,url,checked_on,source_type,access_mode,version_scope,app_build,source_updated_on,locator,note,hands_on,visual_observed,archived_artifact,sha256,supersedes`。实际记录还常含 `firmware_version` 等条件字段；JSON 布尔值不能写成字符串。URL 必须为合法 HTTP(S)，产品必须存在，`supersedes`/`derived_from` 必须回指已有 E。归档附件需存在且 SHA-256 一致；`visual_observed=true` 还需正式附件或视觉审计及其散列。
- `merge_batches.py` 还会重写根 `evidence.jsonl`、`coverage.csv`、`catalog.csv`、`case-index.md`。`check_docs.py` 核本地链接、ID 唯一、M/F/E引用、案例与覆盖一致、附件散列、主索引与批次完全一致及 D4 浏览器审计组合。

## 4. 整合前基线

- 65 产品、65 部分案例。
- 956 F：D1 77、D2 859、D4 20（D3 0）。
- 1667 E，1497 个不同 URL。
- 88 RP。
- M-043：12 F、32 E、0 RP。
- `work/research/batch46-integration/final-check.json` 与现场重算对主计数一致；该文件另报静态视觉 E 228、操作浏览器 E 21、errors 为空。

## 5. 未知与注意

- 本审计没有判断第47批应新增多少 E/F/RP，也没有核来源正文。
- `data-dictionary.md` 的 RP 段仍写“当前68条”，已落后于现场 88 行；整合时应以 CSV 实数和批次 46 后基线为准。
- 检查时 `work/research/batch47-integration/shortcuts-controls-deepening-evidence.jsonl` 与对应 coverage 文件曾在并行任务路径中出现，但随后的 `stat` 未输出，说明它们可能被其他执行者创建后移走；本审计没有读取或修改它们，不能据此判断第47批草稿内容。
