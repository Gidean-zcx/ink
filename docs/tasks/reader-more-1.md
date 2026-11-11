# reader-more-1：书架显示传进来的书、书签（照原厂书签模型）

先读 `docs/tasks/vendor-common.md`（第 1～3、6、7、9～12 条；第 8 条不碰设备），总规则 `docs/tasks/codex-rules.md` 第 9、10 节优先。

本任务从分支 `reader-settings-1-work` 开始（= `reader-settings-1` 的成果 + 最新主分支，包括 `reader-epub-1` 和传书 `transfer-s31-1`）。用户在功能清单里勾了“书签：收藏书签、书签管理（官方）”。传书任务合入后，用户从电脑或手机传的书落在家人的收件箱 `/me/收件箱/`，阅读器书架现在只扫 `/content/books`，看不到。

## 要做的

1. **书架显示传进来的书**（`apps/reader/reader.c`）：书架同时列出当前家人收件箱里的 `.epub` 和 `.txt`（只列当前家人自己的，按成员隔离），新传进来还没打开过的标“新”；打开、续读、目录和阅读设置和书架原有的书一样。不复制或移动文件（不新增导入机制）；收件箱里不是书的文件不列。传书时的临时文件（`.part`）不列。
2. **书签**：照原厂 `main/reader/book_bookmarks.{h,cc}`（874538e，MIT，154 行）的位置模型（章、块、章内偏移，与分页无关）和上限（256 个、摘录 768 字节）——整段拷进 `apps/reader/vendor/metalio_bookmarks/`，存储换成我们的按成员数据（现有存储接口，不用原厂的文件格式）。阅读菜单里加“加书签/删书签”和“书签”列表页（照原厂书签页：时间、章名、一句摘录，点一条跳过去，可删除）；当前页有书签时页角有标记。改字号、行距后书签照样能找到位置。
3. **AI 接入**（人和 AI 共用）：`reader.bookmarks`（列出）、`reader.add_bookmark`、`reader.remove_bookmark`、`reader.goto_bookmark`；页面快照给出当前页是否有书签。

## 范围和预算

- 只改：`apps/reader/**`（含新增 vendor 目录）、阅读相关测试和截图基线、`tests/integration/ai_surface.c` 里登记新能力的那几行、`assets/i18n/en.tsv`（只加本任务新文案的英文）、生成的 `docs/system/capabilities.md`、`docs/tasks/reader-more-1/`。不碰别的任务在改的文件。
- 胶水和界面新增不超过 400 行（拷来的原文不算）；不新增常驻任务、文件格式。

## 验收

1. 测试：收件箱里的书出现在书架、能打开续读，另一个家人看不到；`.part` 和非书文件不列；加书签、列表、跳转、删除；改字号后书签位置仍对；两个家人书签不串；重启后保留；AI 调能力和页面一致；反向对照。现有阅读测试（含 `reader-settings*`、`reader-epub*`）不放宽照样过。
2. 主工程普通全量 ctest 过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小（应用分区只剩约 8%，尽量小）。
3. 截图（书架含“新”书、书签列表、带书签标记的正文页，常规和 120%，`docs/tasks/reader-more-1/`，单张不超过 300 KB）。
