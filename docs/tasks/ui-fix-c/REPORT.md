# ui-fix-c 执行报告

基线：`69cc9868a5e6b95c8376293b385308431fef92a0`。范围：UX13、UX14、UX19。状态：页面修改、专用正反向验证已完成；全量构建正在处理词表范围冲突，尚未验收。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| UX13 / A01～A07 | 【实测】专用检查通过；来源细化未做 | 首末页截断说明、首次听解释/再听一遍、停止朗读、点按说完了；大字页码与关闭可见；书页原文逐字保持 | `python -B tests/ui_fix_c/run.py --build build --case panel` |
| UX14 / A08～A10、A14、A15 | 【实测】专用检查通过 | 设置真实对象名；其他对象明确名称未记录；结果/时间；过期、不支持撤销的原因；额度不承诺恢复日期 | `python -B tests/ui_fix_c/run.py --build build --case records` |
| UX19 / G01～G05 | 【实测】专用检查通过；日期/前后图见接口提案 | 现有缩略图像素及按压轮廓；空态无无效箭头；读错不冒充空；触摸和 AI 手动重读、拔回合成卡后恢复；文件名/序号；删除保留确认 | `python -B tests/ui_fix_c/run.py --build build --case gallery` |
| 常规及 120% 前后截图 | 【实测】已生成，正在复核 | 本目录 `before/after-*-100/120.png`；来自同一驱动的 LVGL clean 帧，无设计稿 | `python -B tests/ui_fix_c/run.py --build build --screens`，前图加 `--before <基线>` |
| 针对性测试及反向对照 | 【实测】8 组正向通过；6 组撤回失败 | `build/ui-fix-c/{before,after,reverse}/summary.json`；不改工作树，单独编译基线页面替换同一测试目标 | `python -B tests/ui_fix_c/run.py --build build --reverse <基线>` |
| 普通全量 ctest（含 ui-scale） | 待运行 | 待补原始汇总 | 待补 |
| 内存检查 | 待运行 | 待补范围与结果 | 待补 |
| S31 / gcccheck | 【实测】GCC 三文件无诊断；S31 待构建 | `gcccheck: 3 compiled, 0 with findings` | `python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/ui/shell/ai_panel.c system/ui/shell/ai_pages.c system/apps/gallery/gallery.c` |
| 范围、行数及隐私检查 | 进行中 | 当前产品代码新增 122 行、删除 49 行，共 171 行，小于 220；未新增服务/线程/文件格式 | `git diff --numstat` |

## 没做到的

- A04：服务没有向面板提供书名/页码或具体引用来源的接口。本任务保留单页既有来源文案，不把它算作来源细化完成。
- G01：索引没有拍摄日期；未编造日期分组。G04：照片前后切换没有现成原子导航，未伪装实现；见 [接口提案](INTERFACES.md)。
- 普通全量 ctest、ASan/UBSan 全量和 S31 暂未完成。

## 和设计不一致的

- 无新增服务或存储；上述来源、日期与翻图缺口如实保留。

## 改过的已有测试

- 暂无。

## 要用户定的

- 新文案需要追加英文词表，但 `assets/i18n/en.tsv` 不在任务书白名单内。已提出异步范围确认；逐条待追加内容在 [TRANSLATIONS.tsv](TRANSLATIONS.tsv)。未获答复前不改白名单外文件。

## 顺手发现

- 构建在 i18n 严格检查处拦住新增文案；没有跳过或放宽检查。UI 专用驱动用已有构建选项单独链接，中文验证通过不能替代全量验收。

## 新增的机制

- 无新任务、状态机、持久格式、缓存、队列或配置项。
- `gallery.reload` 是任务明确要求的手动重读动作，复用既有媒体执行器与读取流程；其接口先记于 [INTERFACES.md](INTERFACES.md)，不自动重试。
- AI 面板增加本轮是否已播放的界面标记，用于任务要求的首听/再听文案。

## 要改的文档

- 合并时由主会话刷新生成的能力文档（`gallery.reload`），不在本分支修改共享文档。
- 若范围获准，只向英文词表追加本任务必要译文。
