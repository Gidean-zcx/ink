# Batch42 本地结构与计数验收

日期：2026-10-27。范围：只读验证正式资料与 `work/research/batch42-integration/before-manifest.json` 基线；未联网，未运行 integration 或 merge，未修改 `docs/`。

## 结果

通过。

- 当前全库检查：65 产品/65 部分案例，930F（834 D2、77 D1、19 D4），1635E、1467 个不同 URL、221 条静态视觉 E、20 条浏览器操作 E；249 个 Markdown、2016 个本地链接，`errors=[]`。
- 基线清单含1303个正式文件。按同一正式范围（根 `README.md`、`AGENTS.md`、`docs/**`）实算：新增42、修改14、删除0。完整路径在 `worker-validation.json.formal_diff`；新增含 brief042、DUMA batch 三件、两组 source/audit，以及27 PNG和6个浏览器JSON；修改清单包含 M031、旧 coverage、master、RP和入口文档。
- `docs/requirements.md` SHA-256仍为 `e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087`。
- `reference-points.csv` 恰49条、15字段，ID唯一；所有单元格非空，全部 F/E 存在，全部 `artifact_path` 存在。
- `E-DUMA-01`～`04` 均存在；01～03为 `hands_on=false`，04为 `hands_on=true`，并通过现有D4审计规则及全部附件散列检查。
- 新增流程恰有 `F-M031-20`，为D4/`hands_on=true`/`observed_browser_sequence`，证据为 `E-DUMA-01;02;04`。旧 `F-M031-16`、`F-M031-18` 与 `old-flow-changes.json` 中记录的 after 对象逐字段完全一致。
- Math审计实算83条 `observed-states`、58条 `trace-actions`、正式目录27幅 PNG；与 E、案例和操作审计声明一致。E-DUMA-04 登记的全部 artifact 散列均匹配。
- 正式网络记录440条：HTTP 200×380、404×35、204×21、302×3、422×1。失败记录537条：CSP×520、`net::ERR_ABORTED`×17；与审计文字一致。
- 正式网络路径含 `<anonymous-user>` 和 `<opaque-id>`占位符；未检出 UUID 形式标识，也未检出 `/users/` 后仍保留真实值的路径。`raw-capture-manifest.json`明确说明正式副本替换用户/会话路径标识，原始 work 散列另留。
- 根代理最后补入 brief042 的 DOM边界行和 M031 的 G-002 核查问题后，重新执行检查与哈希比较，数量和schema保持一致。

## 关键证据

- 基线：`work/research/batch42-integration/before-manifest.json`
- 旧F预期：`work/research/batch42-integration/old-flow-changes.json`
- 新E/F：`docs/research/interaction-library/batches/duolingo-math-interactions-evidence.jsonl`、`duolingo-math-interactions-coverage.csv`
- 操作审计：`docs/research/interaction-library/artifacts/duolingo-math-interactions/browser-operation-audit.md`
- 状态/命令/网络：同目录 `browser/observed-states.json`、`trace-actions.json`、`network-statuses.json`、`browser-errors.json`、`raw-capture-manifest.json`
- 机器验收详情：`work/research/batch42-integration/worker-validation.json`

## 执行命令与退出状态

1. `python3 work/research/interaction-depth/check_docs.py`：exit 0。根最后补文后再次运行，输出保存为 `check-docs-output.json`，仍为 `errors=[]`。
2. Python/`find`/`rg`读取 before manifest、DUMA E/F、M031、正式附件和 integration 元数据：exit 0。
3. Python统计 `observed-states.json`、`trace-actions.json`、正式PNG、HTTP状态、requestfailed类型及脱敏路径：exit 0。
4. `python3 work/research/batch42-integration/validate_worker.py`：首次及根最后补文后复跑均exit 0；最终摘要为 `PASS`、diff `[42,14,0]`、RP49、操作 `[83,58,27]`，无 problems。
5. `sha256sum docs/requirements.md work/research/batch42-integration/worker-validation.json`：exit 0；requirements匹配，JSON SHA-256为 `71770b3b5fe005bd19cb57423fb9469519e22e98f64dcd07aaa1f5771fc442a0`。

## 未知与状态变化

- 本次只验证结构、引用、数量、散列和脱敏模式，不重新解释数学交互或判断产品设计取舍。
- 未逐帧重新观看27张PNG，也未重放浏览器会话；视觉语义仍以根代理审计为准。
- 脱敏检查覆盖正式 `network-statuses.json` 与 `browser-errors.json` 中的URL模式；不审计原始 work 网络内容是否含临时匿名服务状态，因为正式归档与原始散列已分开。
- 状态变化只新增/更新 `worker-validation.md`、`worker-validation.json`、`check-docs-output.json` 和只读验证脚本；未修改正式资料。
