# Batch43 本地结构与计数验收

日期：2026-10-27。范围：只读验证正式资料与 `before-manifest.json` 基线；未联网，未运行浏览器、merge或integration，未修改 `docs/`。

## 结果

通过。

- `check_docs.py`：252 Markdown、2060本地链接、65产品/65部分案例、931F（834 D2、77 D1、20 D4）、1636E、1468个不同URL、221静态视觉E、21浏览器操作E，`errors=[]`。
- 基线正式文件1303个；按根README、AGENTS及`docs/**`同一范围实算：新增30、修改15、删除0。完整路径见 `worker-validation.json.formal_diff`。
- `docs/requirements.md` SHA-256仍为 `e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087`。
- `reference-points.csv`恰56条、15字段；ID唯一、无空单元格，全部F/E引用和`artifact_path`存在。
- 新 `E-TKSM-01` 和 `F-M047-06` 均存在。F06为D4、`hands_on=true`、`visual_status=observed_browser_sequence`；E满足当前自动化公开网页操作审计字段，全部登记附件散列匹配。
- 旧 `E-TFK-04` 与基线相比已更新：仍保持`hands_on=false`，观察页扩为2/3/8/9，新增p8/p9静态页和source-notes散列；没有被升级为操作E。
- 旧 `F-M047-02` 与基线相比已更新：仍为D2、`hands_on=false`、`static_reference_only`，补同刊Turtle Time纸面任务，继续限定为成人组织、无自动评分和课堂实测。
- 新操作审计直接实算23条状态、25条命令、15张正式浏览器PNG；`raw-capture-manifest.json.selected_visual_captures`也为15，与E、案例和审计文字一致。
- 正式网络摘要75条：HTTP 200×32、204×43；与审计一致。浏览器错误文件只有旧G3文章的一条Load error，审计已限定其不能归因于样刊音频。
- 初次匿名化模式扫描将OneTrust公开cookie配置UUID误报为潜在会话标识，脚本exit1并停止判定；本地核对命中仅为 `cdn.cookielaw.org/consent/...` 两个公开配置URL，非用户/会话路径。将该已知静态配置类别排除后复跑exit0；没有发现其他异常UUID路径。
- 根代理最后对README历史措辞及审计命令19/22的0基索引说明修改后，已以当前磁盘再次运行check与验证；文件集合仍为30/15/0，全部通过。

## 执行命令与退出状态

1. `python3 work/research/interaction-depth/check_docs.py > work/research/batch43-integration/check-docs-output.json`：根最后修改前后均exit0，最终`errors=[]`。
2. `find`定位 `artifacts/time-sample-reader/` 与 `batches/time-sample-reader-*`：exit0。
3. `cat`/`rg`读取新E/F、integration-result、旧news批次更新、M047案例与操作审计：exit0。
4. Python以`before-manifest.json`计算正式文件SHA差异，并检查RP、requirements、新旧F/E、附件散列、states/actions/PNG/网络计数：第一次因保守UUID规则报告OneTrust配置URL并exit1；核清后调整规则，最终exit0，`status=PASS`。
5. Python读取 `raw-capture-manifest.json` 键和`selected_visual_captures`：exit0，数量15；校验脚本字段随之修正后复跑exit0。
6. `sha256sum`核 requirements 和最终JSON：exit0。

## 证据文件

- 基线：`work/research/batch43-integration/before-manifest.json`
- 新批：`docs/research/interaction-library/batches/time-sample-reader-evidence.jsonl`、`time-sample-reader-coverage.csv`、`time-sample-reader-inventory.csv`
- 旧批更新：`batches/news-evidence.jsonl`、`batches/news-coverage.csv`
- 审计：`artifacts/time-sample-reader/browser-operation-audit.md`
- 原始正式日志：同目录 `browser/observed-states.json`、`trace-actions.json`、`network-statuses.json`、`browser-errors.json`、`raw-capture-manifest.json`
- 机器结果：`work/research/batch43-integration/worker-validation.json`

## 未知与状态变化

- 本次不重做根代理已完成的图片语义判断，也未重放浏览器会话；只验证结构、计数、引用和散列。
- 网络日志监听明确不是全会话完整捕获，75条一致只证明正式记录与审计数字相符。
- 公开样刊URL中的`ipt`来自公开教育页，是否长期稳定不由本地结构检查证明。
- 状态变化仅新增/更新 `worker-validation.md`、`worker-validation.json`、`check-docs-output.json` 和只读验证脚本；未修改正式资料。
