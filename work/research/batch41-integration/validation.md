# 第 41 批本地验证

日期：2026-10-27。范围：只读验证正式资料结构、Batch41 指定新增项及四份定点正文；未联网，未修改 `docs/`，未扩展竞品研究。

## 验证结果

通过。未发现需要停止合并的结构错误，也未在指定四份正文中发现明显相互矛盾或把文档/静态画面升级成实操结果的句子。

- `check_docs.py` 通过：245 个 Markdown、1958 个本地链接、65 个产品且 65 个均 partial、929F（834 D2 / 77 D1 / 18 D4）、1631E、1463 个不同 URL、221 条静态视觉 E、19 条浏览器操作 E，`errors=[]`。
- `reference-points.csv` 恰有 38 条、`reference_id` 唯一；表头恰为 15 字段，38×15 个单元均非空。每条所引 F、E 均存在，`artifact_path` 均存在。
- 15 字段为：`reference_id`、`product_id`、`question_tags`、`flow_ids`、`evidence_ids`、`source_locator`、`artifact_path`、`precondition`、`action`、`visible_result`、`evidence_basis`、`persistence_result`、`unknown_branches`、`version_scope`、`checked_on`。
- `docs/requirements.md` SHA-256 为 `e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087`，与指定基线一致。
- `E-INRS-01`～`E-INRS-42`、`E-GNRS-01`～`E-GNRS-04` 共 46 条全部存在，`hands_on` 均为 JSON `false`。
- 新来源中 `visual_observed=true` 的恰为 4 条：`E-INRS-39`、`E-INRS-40`、`E-INRS-42`、`E-GNRS-02`。其 `artifact_hashes` 共关联 13 个互异的 PNG/JPG 文件；13 文件均存在且 SHA-256 与登记值一致。
- 相对 Batch41 `before/` 基线，新增流程恰为 `F-M044-17`；其 `depth=D2`、`hands_on=false`、`visual_status=static_official_images_only`，没有其他新增 F。
- 根代理修订后的日期字段已核：`E-INRS-39/40/42` 的 `source_published_on` 分别为 `2024-10-15`、`2024-11-28`、`2026-08-14`，三者 `source_updated_on=null`，没有再把博客发表日登记成更新时间。

## 定点语义核查

读取：

- `docs/briefs/041-news-reading-and-retention.md`
- `docs/research/interaction-library/artifacts/inoreader-reading-state/source-notes.md`
- `docs/research/interaction-library/artifacts/ground-source-scope/source-notes.md`
- `docs/research/interaction-library/cases/M-044-inoreader.md` 开头至新增 F17

相互数量一致：简报的 38 篇帮助、3 篇博客、1 个功能页、12 张 Inoreader 图、4 篇 Ground 正文和 1 张 Ground 图，与两份 source-notes 及“13 张图关联 4E”的结构结果一致；M044 的“此前2张+本批12张=14张”和“16旧F+1新F=17F”也一致。

证据等级一致：四份文档都明确没有登录、订阅、保存、过滤执行或 App 操作；静态入口没有被写成动作成功。邮件源取消后的“删除/失去访问”与“保留90天可恢复”冲突在简报、source-notes 和 M044 中均保留为未统一口径，没有被强行选定一种结论。Ground 的联网、原语言和套餐范围也保留了“文档声明、非账号或版本实测”的限制。

未发现明显未经证实升级。以下只是仍需保留的既有未知，不是本次错误：

- Inoreader 静态图不能证明创建邮件地址、收信、归档、删除、撤销或跨端恢复成功。
- Ground 套餐图没有版本号，帮助正文中的移动版/套餐文案与其他帮助可能处于不同版本，当前资料已明确禁止无条件拼接。
- 46 条新增 E 均为公开正文或静态图依据，不能代表当前安装构建、E-Ink4 Plus 或账号套餐实测。

## 执行命令与退出状态

1. 正式结构检查：

   ```bash
   python3 work/research/interaction-depth/check_docs.py
   ```

   Exit 0；输出上述 929F / 1631E / 1463 URL / `errors=[]`。

2. 定位 Batch41 正式附件、证据与新增流程：

   ```bash
   rg --files docs/research/interaction-library/artifacts docs/briefs | rg '(041|inoreader|ground.*remaining|remaining.*ground|source-notes\\.md)$' | sort
   rg -n 'E-INRS-|E-GNRS-|F-M044-17' docs/research/interaction-library/cases/M-044-inoreader.md docs/research/interaction-library/batches docs/research/interaction-library/evidence.jsonl docs/research/interaction-library/coverage.csv
   ```

   Exit 0。确认文件位置与正式引用。

3. Python 只读验证脚本（临时文件 `/tmp/validate_batch41.py`）：读取 RP、coverage、evidence、Batch41 `before/` 基线及附件，检查 15 字段、ID、F/E、附件存在、requirements 散列、新 E 的 `hands_on`、4 个视觉 E 的13张图及散列、新增 F 集合。

   ```bash
   python3 /tmp/validate_batch41.py
   ```

   Exit 0；`rp_missing_refs=[]`、`rp_empty_fields=[]`、`new_e_absent=[]`、`new_e_nonfalse_hands_on=[]`、`missing_images=[]`、`bad_image_hash=[]`、`new_flows=["F-M044-17"]`。

4. 定点读取简报、两份 source-notes 与 M044 开头，并核对关键边界文字：

   ```bash
   cat docs/briefs/041-news-reading-and-retention.md
   python3 - <<'PY'
   # 读取两份 source-notes 与 M044 前115行
   PY
   rg -n '38篇|3篇|1个功能页|12张|13张|17条任务|16个旧任务|新增一个|没有新增实际产品操作|取消.*90天|删除.*失去访问|不支持离线|Vantage.*移动' ...
   ```

   Exit 0。数量、冲突保留和非实操边界一致。

5. 日期字段定点核查：

   ```bash
   python3 - <<'PY'
   # 从 evidence.jsonl 打印 E-INRS-39/40/42 的 source_published_on、source_updated_on、hands_on
   PY
   ```

   Exit 0。三条均为已填发表日、`source_updated_on=null`、`hands_on=false`。

## 未知与状态变化

- 本次没有重读46个来源正文或13张图的视觉内容，只验证正式记录、散列和指定汇总文档的相互一致性；不重新判断每条来源是否足够支持全部细节。
- 没有运行产品、登录账号或验证邮件源、过滤、离线、播放及跨端恢复。
- 状态变化仅新增本验证报告；未修改 `docs/` 或任何被验证文件。临时只读脚本位于 `/tmp/validate_batch41.py`，不属于正式资料。

根代理归档补记：helper已执行的临时脚本现迁至 `work/research/batch41-integration/worker-validation-script.py`，原 `/tmp/validate_batch41.py` 已移除。以上命令路径保留实际执行时的历史值。未改正式资料或重新运行产品操作。
