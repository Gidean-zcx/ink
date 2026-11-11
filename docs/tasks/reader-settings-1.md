# reader-settings-1：阅读设置和阅读菜单（照原厂的阅读偏好和设置面板移植）

先读 `docs/tasks/vendor-common.md`（第 1～3、6、7、9～12 条；第 8 条不碰设备），总规则 `docs/tasks/codex-rules.md` 第 9、10 节优先。

`reader-epub-1` 已合入（EPUB、目录、按成员续读）。UI 审计 `docs/tasks/ui-audit-2/PAGES-APPS.md` 的 B03、B04：阅读页点中间也是翻下一页，没有阅读工具入口；正文字号、行距、边距固定，没有阅读设置。用户要求“所有开发的非底层功能优先移植成熟项目”。

原厂 874538e 的成熟实现（MIT，同一块屏）：

- `main/display/screen/book_screen/reader/book_reader_prefs.{h,cc}`（660 行）：阅读偏好——字号、行距 4 档、段距 4 档（小 32 / 标准 56 / 大 80 / 很大 104 px）、页边距 4 档、底栏显示项（章节名、百分比和页码、电量、时间，可整条隐藏）、抗锯齿等；
- `main/display/screen/book_screen/reader/book_reader_overlay.cc`（395 行）：阅读页上点中间出的菜单；
- `main/display/screen/book_screen/settings/book_settings_sheet.cc`（1673 行）、`book_settings_opts_ui.cc`、`book_settings_font_ui.cc`：设置面板。

## 要做的

1. **偏好模型**：从 `book_reader_prefs` 整段拷各档位的数值和默认值（行距、段距、边距、底栏位），放 `apps/reader/vendor/metalio_prefs/`，旁边 `SOURCE.md`。存储换成我们的按成员设置（现有设置服务，每个家人各自一套），不用原厂的 NVS。字号用系统现有字体的几档（不做原厂的 SD 卡自定义字体，报告写缺口）；不改系统字号设置，阅读字号独立。
2. **阅读菜单**：照原厂 overlay 的做法，阅读页点中间出菜单（左右两侧照旧翻页）：目录、阅读设置、返回书架，以及跳到第几页/第几章的入口；再点中间或空白处收起。原厂 LVGL 页面能拷的整段拷，和 BookSession 绑死的部分照布局用我们的组件重写。
3. **设置面板**：照 `book_settings_sheet` 的布局：字号（加减和当前值）、行距、段距、边距（各 4 档，选中有勾）、底栏显示项开关。改动后正文重新分页，**位置保持在同一章同一处**（按章内偏移找回），只刷新必要区域。
4. **AI 接入**（人和 AI 共用）：`reader.get_layout`（当前各项）、`reader.set_layout`（改一项或几项，参数有范围检查），页面快照给出当前值和可选档位；`reader.menu`（打开或收起阅读菜单）。按成员隔离；孩子能改自己的阅读设置。
5. 底栏：按设置显示章节名、页码和百分比、电量、时间；时间和电量只在内容变化时局部刷新（D-127），不要每分钟整页刷。

## 范围和预算

- 只改：`apps/reader/**`（含新增 vendor 目录）、阅读相关测试和截图基线、生成的 `docs/system/capabilities.md`、`assets/i18n/en.tsv`（只加本任务新文案的英文）、`docs/tasks/reader-settings-1/`。不碰别的任务在改的文件（锁屏、设置页、传书、音乐、五子棋、页面整改）。
- 胶水和界面新增不超过 500 行（拷来的原文不算）；不新增常驻任务、定时循环、文件格式。

## 验收

1. 测试：每一项设置改了以后正文排版变化、位置不丢（改字号前后同一句话仍在当前页）；两个家人各自的设置不串；重启后保持；点中间出菜单、左右照旧翻页；AI 用能力改设置和页面一致；底栏开关生效；反向对照。现有阅读测试（含 `reader-epub*`、`reader_basic`、`reader_continuous`）不放宽照样过。
2. 主工程普通全量 ctest 过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件增大多少。
3. 截图（阅读菜单、设置面板、几种设置下的正文页，常规和 120%，`docs/tasks/reader-settings-1/`，单张不超过 300 KB）；上机核对清单（主会话在真机改字号和行距，看重排耗时和残影）。
