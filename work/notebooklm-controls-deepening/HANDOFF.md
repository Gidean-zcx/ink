# M041 本轮交接（2026-10-26）

完成有限文档批次：新增 **13E / 6F**，原 7F 保留。案例合计 **13F / 20E**；所有 F 都是限定范围的 D2 文档流程，全部 `hands_on=false`、`visual_status=not_observed`。不是实测、没有新视觉证据。完整目录登记不表示所有功能已完成研究。

## 文件和归属

- 修改：`docs/research/interaction-library/cases/M-041-notebooklm.md`
- 新增：`docs/research/interaction-library/batches/notebooklm-controls-deepening-evidence.jsonl`（E-NBCD-01–13）
- 新增：`docs/research/interaction-library/batches/notebooklm-controls-deepening-coverage.csv`（F-M041-08–13）
- 新增：`docs/research/interaction-library/batches/notebooklm-controls-deepening-inventory.csv`（19 功能域）
- 旧行建议：`work/notebooklm-controls-deepening/existing-coverage-updates.csv`，6 个旧 F 的 18 字段，逐字段 before/after；LF，无字面量反斜杠 r/n。
- 原件、提取正文、脚本、取得/检查结果：仅 `work/notebooklm-controls-deepening/`。
- 未改 master、索引、旧 batch、README、requirements、ideas 或其他 case；未调用 helper。旧 ai-evidence / ai-coverage 哈希与基线一致。共享工作区其他代理的变化不归本批。

新 F 是思维导图、报告、数据表、幻灯片、信息图、视频六类独立产物任务，没有为来源导入、练习、进度另造重复 F。F01/02/04/05/06/07 各建议修改 `evidence_ids`、`known_branches`、`gaps`；F03 不变，旧 D2 等级不变。

## 根代理抽核入口

1. **E-NBCD-04 / E-NBCD-03：复制冲突。**
   - <https://support.google.com/gemininotebook/answer/16206563?hl=en#nb_copy>
   - 原件 `04-create.html` 的 article 内 `id="nb_copy"` / Create a private copy：Copy → Create，来源和 Studio 内容复制，聊天和笔记不复制；正文 `04-create.txt` 第 76–84 行。`probe-create.html` 同样包含此段。两份 article 均 15706 字符，本轮没有证据证明这两次响应正文不同。
   - <https://support.google.com/gemininotebook/answer/16269187?hl=en> 的 Duplicate a notebook 仍称不支持；不自行判定哪项已经部署。根已回复定位核对该原件段落。
2. **E-NBCD-10 / E-NBCD-12：修订与局部撤销。**
   - <https://support.google.com/gemininotebook/answer/16757456?hl=en>，Edit / Delete slides：修订不参考 sources；待删除页可 Restore；Generate revised deck 形成新套。原件 `10-slides.html`，提取正文 `10-slides.txt`。
   - <https://support.google.com/gemininotebook/answer/16296687?hl=en&co=GENIE.Platform%3DAndroid>，Slide Decks in App 仍称修订不能增删页；Limited features 说明离线音频不是设备文件导出、同步延迟建议退出重开。原件 `12-mobile.html`。
3. **E-NBCD-04 / E-NBCD-13：表格对象与出口。**
   - Create 的 Data Tables 段：Export to Sheets 后数据/引用分两页，导出不回同步、不继承权限。
   - <https://workspaceupdates.googleblog.com/2025/12/transform-sources-structured-data-tables-notebooklm.html>，正文日期 2025-12-23，只用于历史发布，不为当前新版功能赋上线日期。

## 命令、退出状态与失败

均在 `/home/workspace/reader` 执行。读取既有 case、template、README、requirements、ideas 与批次 schema 的 `sed` / `rg` / `cat` / Python JSON/CSV 读取均成功；有一次组合读取命令末尾检查 `bs4` 返回 **exit 1: ModuleNotFoundError**，没有安装，改用标准库 HTMLParser。

- `timeout 65s python3 -u work/notebooklm-controls-deepening/fetch-pages.py` → **exit 0**；14 个公开请求中 **13×HTTP 200、1×HTTP 429**。HTTP 与进程退出码分别记录。
- 429 为旧 E-NBL-04 的 2026-03-20 公告 URL；停止该访问，不重试、不绕过。本轮不把它作为新 E，不假称重新核文。
- `python3 work/notebooklm-controls-deepening/write-batch.py` → **exit 0**。
- `python3 work/notebooklm-controls-deepening/compose-case.py` → **exit 0**。
- `python3 work/notebooklm-controls-deepening/check-batch.py` → **exit 0 / PASS**。
- `git diff --check -- docs/research/interaction-library/cases/M-041-notebooklm.md docs/research/interaction-library/batches/notebooklm-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/notebooklm-controls-deepening-coverage.csv docs/research/interaction-library/batches/notebooklm-controls-deepening-inventory.csv` → **exit 0**。这 4 项目前显示 untracked，因此此命令本身不足以验文；以读取实际文件的 check-batch.py 为准。
- 曾尝试以一个 apply_patch 同时 Delete/Add 同路径，工具校验拒绝“multiple operations target”，没有改动；后以 compose-case.py 写入并完成检查。
- web 官方域搜索/正文打开成功；个别帮助的 web open 返回 400 timeout，后以公开 requests 200 原件核文。未对 401/403/429 失败端点继续访问。没有浏览器登录、提交资料、调用账户接口、生成产物或下载视频。

`check-results.json` 包括正式文件 SHA-256。检查涵盖：13 E 唯一且与其他批次无碰撞；6 新 F 连号且证据存在；13 个 case 任务编号稳定；19 inventory 行；6 旧 F /18 before 字段匹配旧文件；相对文件链接存在；UTF-8/LF；原件哈希；`archived_artifact=null`、`raw_work_path/raw_sha256` 正确；原批哈希未变；复制段确在两个 HTML article 内。没有新产品代码或测试。

## 仍未知

帮助缺精确构建/更新时间；NotebookLM→Gemini Notebook 路由变化与账号可用性不能混同。复制冲突与跨端修订差异保留；来源重复去重、人工删除确认/恢复、旧产物随来源更新、逐题改写/撤销评分、多端进度冲突、生成取消/排程取消、已生成产物误删恢复仍无完整链。卡片自评、保存进度和打开产物不等于掌握。下一步需要带构建/账户范围的官方演示或获授权实际观察，不继续堆宣传页。
