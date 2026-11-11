# ui-fix-c 执行报告

基线：`69cc9868a5e6b95c8376293b385308431fef92a0`。范围：UX13、UX14、UX19。页面修改及专用验证完成，**未通过全量验收**：必要译文、生成能力文档受任务白名单限制，尚未改动。

下列 `python`、`ctest` 分别使用 `$INK_DEPS/build-deps/pyenv/bin/python`、`$INK_DEPS/build-deps/pyenv/bin/ctest`。构建和测试均加 `taskpolicy -c utility`，并行上限 `-j2`；没有推送、碰设备或启动窗口。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| UX13 / A01～A07 | 【实测】专用检查通过；来源细化未做 | 首末页截断说明、首次听解释/再听一遍、停止朗读、点按说完了；大字页码与关闭可见；书页原文逐字保持 | `python -B tests/ui_fix_c/run.py --build build --case panel` |
| UX14 / A08～A10、A14、A15 | 【实测】专用检查通过 | 设置真实对象名；其他对象明确名称未记录；结果/时间；过期、不支持撤销的原因；额度不承诺恢复日期 | `python -B tests/ui_fix_c/run.py --build build --case records` |
| UX19 / G01～G05 | 【实测】专用检查通过；日期/前后图见接口提案 | 现有缩略图像素及按压轮廓；空态无无效箭头；读错不冒充空；触摸和 AI 手动重读、拔回合成卡后恢复；文件名/序号；删除保留确认 | `python -B tests/ui_fix_c/run.py --build build --case gallery` |
| 常规及 120% 前后截图 | 【实测】24 张，最大 45,858 字节 | [截图索引](SCREENSHOTS.md)；来自同一驱动的 LVGL clean 帧，无设计稿；已目视检查大字号面板/记录/相册/照片 | `python -B tests/ui_fix_c/run.py --build build --screens`，前图加 `--before <基线>` |
| 针对性测试及反向对照 | 【实测】8 组正向通过；6 组撤回失败 | `build/ui-fix-c/{before,after,reverse}/summary.json`；不改工作树，单独编译基线页面替换同一测试目标 | `python -B tests/ui_fix_c/run.py --build build --reverse <基线>` |
| 未修改的现有回归 | 【实测】5 项通过 | AI 触摸停止 105 断言；两个档案的布局各 106 断言；五张语义快照；离线重播 25 断言。当前面板重链接进原测试，不改原断言 | `python -B tests/ui_fix_c/existing.py --build build` |
| 普通全量 ctest（含 ui-scale） | 【实测】未通过；退出 8 | 原话：`97% tests passed, 11 tests failed out of 401`；耗时 1242.78 秒，另有 3 项跳过。生成词表失败后这轮沿用先前执行文件，不能作为最终改动的全量通过证据；后续五项回归单独重链接验证 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| ui-scale | 【实测】通过 | `ui-scale: 52 files, 0 violations`；全量中的 ui-scale / ui-scale-regressions 也通过 | `python -B tools/ui_scale_check.py` |
| 内存检查 | 【实测】构建中止，运行测试 0 项 | `build-asan` 配置成功；`INK_SANITIZE=ON`、`INK_WERROR=ON`，构建在 i18n 缺译文处退出 2。**没有运行 ASan/UBSan 测试，不能称内存检查通过** | 依总规则同参数配置，加 `-DINK_SANITIZE=ON`；`taskpolicy -c utility cmake --build build-asan -j2` |
| S31 | 【实测】未编过；退出 2 | 配置在 `components/ink_system/CMakeLists.txt:20` 的译文检查中止；没有烧录或设备操作 | `INK_LVGL_SOURCE_DIR="$INK_DEPS/build/_deps/lvgl-src" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| gcccheck | 【实测】三个产品文件无诊断 | `gcccheck: 3 compiled, 0 with findings`；这是 GCC 编译检查，不能替代 S31 链接或 Linux 全量 | `python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/ui/shell/ai_panel.c system/ui/shell/ai_pages.c system/apps/gallery/gallery.c` |
| 范围、行数及隐私检查 | 【实测】通过 | 39 个文件均在白名单；产品代码新增 121 行、删除 49 行，共 170 行，小于 220；绝对路径/MAC 模式扫描 0 匹配（grep 退出 1）；30 个本地链接有效 | `git diff --stat <基线>`、`git diff --check`；提交前及提交后按总规则扫描差异 |

## 没做到的

- A04 低优先项：AI 答复没有向面板传回具体引用来源元数据。本任务保留普通回答既有来源文案，不把它算作来源细化完成；真实超长截断时用明确边界说明替换。
- A01、A03、A08、A09：保留原来的按住说话、等待、网络/飞行模式/超时页；没有扩展超时页的部分回答及原问题展示。A02、A04 朗读文字、A05～A07、A10、A14、A15 已按上表验证。
- G01：索引没有拍摄日期；未编造日期分组。G04：照片前后切换没有现成原子导航，未伪装实现；见 [接口提案](INTERFACES.md)。
- 最终普通全量通过、ASan/UBSan 运行检查、S31 编译没有做到；不绕过 i18n 检查，也不私自更新共享能力文档。

## 和设计不一致的

- 无新增服务或存储；上述来源、日期与翻图缺口如实保留。

## 改过的已有测试

- 暂无。

## 要用户定的

- 新文案需要追加英文词表，但 `assets/i18n/en.tsv` 不在任务书白名单内。已提出异步范围确认；逐条待追加内容在 [TRANSLATIONS.tsv](TRANSLATIONS.tsv)。未获答复前不改白名单外文件。
- `gallery.reload` 使生成能力文档过期；总规则明确由主会话合并时刷新，因此本分支保留此项全量失败，不把它掩盖成通过。

## 顺手发现

- 构建在 i18n 严格检查处拦住新增文案；没有跳过或放宽检查。UI 专用驱动用已有构建选项单独链接，中文验证通过不能替代全量验收。
- 原始全量的失败分两组：`ai-chat`、其快照、两档案 `ui-answer`、`ai-history-photo` 的五项问题已用当前面板重链接原测试验证通过；`i18n-lang`、`i18n-shell-remaining`、`i18n-literals`、`ai-answer-long-en` 及两个能力表检查仍须在补译文、刷新共享文档并重建后全量重跑。跳过项为 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`。
- 日志保留于 `build/{build,ctest,asan-build,s31-build}.log`；专用检查摘要见 [EVIDENCE.md](EVIDENCE.md)。

## 新增的机制

- 无新任务、状态机、持久格式、缓存、队列或配置项。
- `gallery.reload` 是任务明确要求的手动重读动作，复用既有媒体执行器与读取流程；其接口先记于 [INTERFACES.md](INTERFACES.md)，不自动重试。
- AI 面板增加本轮是否已播放的界面标记，用于任务要求的首听/再听文案。

## 为什么不能更小

- 合计 170 行涉及三项独立整改，单项均少于 100 行。AI 快照原辅助函数把旧文案写死，而 `ai_chat.c` 禁止修改，需在本文件同步真实按钮和截断说明；只改可见文字会让 AI 仍看到旧语义。相册列表最多容纳三项固定动作，已用于翻页和相机，需在本页快照追加手动重读；修改共用列表会越过白名单。未采用新增通用布局/翻译/重试框架的方案。

## 要改的文档

- 合并时由主会话刷新生成的能力文档（`gallery.reload`），不在本分支修改共享文档。
- 若范围获准，只向英文词表追加本任务必要译文。
- 待追加的 20 项译文已在构建目录临时合并校验：`i18n: en 1538 entries`，退出 0；这仅验证提案，正式词表没有修改。
