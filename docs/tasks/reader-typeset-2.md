# reader-typeset-2：排版补齐和阅读主题（功能清单 `typeset`、`antialias`、`bookstyle`、`themes`，移植队列 R04、R11）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7、9～12 条）。

现状：`reader-settings-1` 照原厂做了字号、行距、段距、边距四档和页脚项。用户勾的还缺：字距、首行缩进、下划线位置；抗锯齿强度；忽略书内样式；一键切换整套排版的阅读主题。

## 要做的

1. 照原厂移植字距、首行缩进、下划线位置：`$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/main/reader/text_spacing.h`、`page_text_raster.cc`、`book_reader_prefs.cc`（先拷进 `apps/reader/vendor/metalio_prefs/` 已有的那份旁边，写清来源）。
2. 抗锯齿：照原厂的开关（字形灰度边缘开或关）；强度分几档原厂公开代码里没有的，只写差距清单，不凭空定曲线。
3. 忽略书内样式：照 CrossPoint 的 CSS 解析（`$INK_DEPS/../ink-hw/upstream/oss/crosspoint-reader/lib/Epub/Epub/css/CssParser.cpp`）先接能解析的样式（对齐、缩进、粗斜体），开关打开时统一用我们的排版。
4. 阅读主题：一组命名的整套排版（例如“紧凑”“舒适”“大字”），一键切换，可在此基础上微调；照 CrossInk 的阅读配置（`$INK_DEPS/../ink-hw/upstream/oss/CrossInk/src/util/ReaderStatusBarConfig.h` 等）组织。
5. 新设置都按家人保存，改了以后重排保留当前位置（沿用现有重排保锚点）；能力沿用 `reader.set_layout`，页面快照给当前值。

## 范围和预算

- 只改：`apps/reader/**`（含 vendor 目录）、阅读测试和截图、`assets/i18n/en.tsv`（只加新文案）、生成的 `docs/system/capabilities.md`、`docs/tasks/reader-typeset-2/`。不碰别的任务在改的文件。
- 胶水不超过 300 行（拷来的原文不算）；不新增常驻任务、文件格式。

## 验收

1. 测试：每个新设置改动后的像素和锚点回归（同一段文字）；主题切换后各项值正确、微调保留；忽略书内样式开和关的对比；两个家人设置不串；反向对照。
2. 主工程普通全量 ctest 过（含 `reader-*`）；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小。
3. 截图：设置页、三种主题的正文页，常规和 120%。
