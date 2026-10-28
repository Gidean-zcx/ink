# Batch 40 来源本地核查

核查日期：2026-10-27。范围：只读检查既有本地文件；未联网，未尝试获取受阻来源，未重读图片内容。除本报告外未主动修改文件。

## 结论

- Khan Kids 两个栏目页 `home` 与 `class` 的本地响应均为 HTTP 403。两份响应头均含 `cf-mitigated: challenge`，HTML 标题均为 `Just a moment...`，正文提示为 `Enable JavaScript and cookies to continue`。
- `summary.json` 记录 `section_gets: 2`、`section_statuses: [403, 403]`、`article_gets: 0`；`results.json` 的 10 个目标全部为 `article_url: null`、`article_get_attempted: false`。因此没有目标 article GET。
- 墨墨 `fetch-results.json` 引用的 8 个 `.txt` 全部存在，逐文件 SHA-256 均与 JSON 记录一致；`images.json` 引用的 9 张图全部存在，逐文件 SHA-256 均一致。仅校验存在性与哈希，没有重新查看图像内容。
- Android 日志 URL `https://memodocs.maimemo.com/docs/product-log-memo-android` 在当前正式主 evidence 中对应 `E-MMDVC-03`，在当前正式 batches 下仅 `vocab-device-deepening-evidence.jsonl` 存在同一条 `E-MMDVC-03` 并列记录。batches 中另有 `maimemo-controls-deepening-inventory.csv` 用相同 URL 指向旧目录证据 `E-MMO-10`；这不是另一条 `E-MMDVC-03`。历史 `work/research/*/before` 快照内还有副本，不属于当前正式主 evidence / batches 唯一性判断。
- Batch 40 baseline 存在：`work/research/batch40-integration/before-manifest.json` 及 `before/` 快照均可见。baseline 记录的 `docs/requirements.md` SHA-256 为 `e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087`；当前文件实算值相同。

## 文件与哈希证据

### Khan Kids

- `work/research/batch40-khan/sections/home.headers`: HTTP/2 403，Cloudflare challenge。
- `work/research/batch40-khan/sections/class.headers`: HTTP/2 403，Cloudflare challenge。
- `home.html`、`class.html`: 标题 `Just a moment...`，提示 `Enable JavaScript and cookies to continue`。
- `summary.json`: 10 个目标、2 次栏目 GET、0 次文章 GET。
- `results.json`: 10/10 项 `article_get_attempted=false`。

### 墨墨文本（8/8 一致）

- controller: `95fca3773d4a1c3c3a0e3572e4a376e01a69ec3b0c2b7540ec5e3693d9af3a27`
- harmony-features: `73fc3f82086720da69e472ed6a6577655b84a568721ffc9b5dcef8ba13d472c2`
- reminders: `7c49c9b6e2db75093fd958847e229b3eb009f46b4fba7f8f9d01e78768983800`
- rating-guide: `9b52b510a830115612134f67bf27bbc67ffd544225244fed9e19e489e253d90d`
- web-study: `bc09a41d67351a17692d922f3141ad149960fce18ce88c477b94c5a57f433999`
- android-log: `674c41f6b9aa0a20a6bc51ebf0a0056c3469a5e7f3247bc373e4400aebab79cd`
- web-log: `3b7518c489101b3ba2b0018146ce84b5d982e1887e149fb3da20297e4a070220`
- open-terms: `458981c64c4df28ad7a616d044ddb81ab825068896071df60368d1a7f00a9c18`

### 墨墨图片（9/9 一致）

- harmony-01.jpg: `8e4889daf64481d8f47a5a93317472b93e1bc2551664a9db0668a236c8a64cf8`
- harmony-02.jpg: `97feed78242407e9178e2540898a7e2447d0ce597d56246dc50f39ffd0da4b72`
- harmony-03.jpg: `1b809a26b03c96c778fd3d4fa7cebc4a1f13ba598dbce20d3b1145385b7fe3d8`
- harmony-04.jpg: `20dec62e7fa4f095634367ceb656f16acb20e3d528e4b975c150d9bb4377b88e`
- harmony-05.jpg: `e128d9088ab688c011e80b591f7466fa90708d4eff7e0254f8c38066f913f336`
- harmony-06.png: `f307be9a03239cda48a8500935e7513bfe57de287c1d347b11fad031adaaf173`
- harmony-07.png: `f6e7205b4e0725d4a51138b5e3d1effe24ff3a845bb7e73a01b7020a32122c6e`
- harmony-08.png: `261b7fc320186a7cf8bfef6c99cda532900dcd2a26f910d32a60d96a4a7dc23f`
- harmony-09.jpg: `af5b00fbbf93c78e10cbb5e4bb91bee853bf0645e4feb8b32963c1cf4eb4248c`

## 执行命令与退出状态

1. `pwd; rg --files work/research/batch40-khan work/research/batch40-maimemo work/research/batch40-integration ...; find work/research ...` — exit 0。确认目标文件与 baseline 存在；输出因文件很多被终端截断，随后用更窄命令核验关键项。
2. Python 读取并打印 `summary.json`、`results.json`、`fetch-results.json`、`images.json`、`before-manifest.json`；`sed` 检查两个 headers；`rg` 从两个 HTML 提取标题和 challenge 文本 — exit 0。
3. Python 对 8 个文本和 9 张图片逐项执行 SHA-256 比对，并计算当前 `docs/requirements.md` SHA；`rg` 检索 Android 日志 URL 与 `E-MMDVC-03`；`find` 检查 integration 文件 — exit 0。
4. `cat > work/research/batch40-integration/source-audit.md` — exit 0，新建本报告。

## 不确定性与边界

- 本核查只能确认既有落盘响应与元数据；没有联网复验当前远端状态。
- Cloudflare challenge 标题和提示来自本地 HTML，不代表目标正文内容。
- “唯一”限定为当前正式 `docs/research/interaction-library/evidence.jsonl` 与 `docs/research/interaction-library/batches/`；历史 baseline 快照按预期保留重复副本。

## 状态变化

仅新建 `work/research/batch40-integration/source-audit.md`。未修改正式 docs，未改动任何被核查来源文件。
