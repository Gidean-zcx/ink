# settings-1：设置框架和首页照 AOSP/Lineage 重做——首页分组、每行写当前值、6 种可用状态、能搜索；注册表照 PebbleOS 的 C 写法，一份声明同时生成页面、搜索和能力

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。

## 为什么

用户 2026-11-12（D-129）：“移植完整应用嫁接和封装…到我们的系统中 这里不止阅读器 还包括各类系统应用和部分基础框架（例如输入法 通知栏 设置等等）”；“外壳有很多我其实并不满意 希望你参考成熟设计完善一下”；D-131：设置是先做到成熟可用的外壳部件。

两份调研一致（`docs/tasks/frameworks-research-1/SETTINGS.md`；主会话另一份调研第 3 节）：结构、每行当前值、搜索照 AOSP/Lineage 设置——只有安卓设置能搜索，而且每行当前值和能不能用由统一的控制器算出来；在 C 里用 PebbleOS 那种“模块注册表加回调”实现；项目清单参考 MuditaOS（墨水屏手机）。现在的设置是 `system/ui/shell/` 里的几份文件，每个页面自己排行（`settings_pages.c:17-40` 的行表），没有搜索，首页没有当前值，不同页面的表单组织不一样（`docs/tasks/replan-sysapps/CURRENT.md` A11）。

现状（`shell-split-1` 拆完以后）：设置首页和设置定义在 `system/ui/shell/settings_body.c`（原 `navigation.c` 约 1155-1806、2040-2085 行），声音、存储、关于等子页的行表在 `settings_pages.c`；设置的读写都走 `system/policy/settings.c` 的 `settings.get`、`settings.set`（414-434 行）。

## 成熟来源（结构照着重写，注册表搬写法）

- Lineage 设置，`github.com/LineageOS/android_packages_apps_Settings` 提交 `f82184028165ea108335a9745428c26fea3cec47`（只读克隆到 `work/`）：
  - `res/xml/top_level_settings.xml:30-94`：首页分组、每项一个控制器、摘要运行时替换；
  - `src/com/android/settings/core/BasePreferenceController.java:75-121`：6 种可用状态（可用、可用但不进搜索、条件不满足暂不可用、本机不支持、对这个用户禁用、依赖项关了变灰），以及搜索索引的钩子（221、318、379 行附近）；
  - `src/com/android/settings/search/BaseSearchIndexProvider.java:52-110`：每页声明自己的可搜索项，和页面是同一份描述；
  - `src/com/android/settings/dashboard/DashboardFragment.java:462-481`：控制器刷新当前状态。
- PebbleOS，`github.com/coredevices/PebbleOS` 提交 `aa0c27e8330d010e9202f9a706a56002db5861ee`：`pebble/apps/system/settings/menu.c:23-50`、`menu.h:40-68`（设置模块注册表和入栈）、`option_menu.c:26-80`（选项行和选择回调）。搬它的“注册表加回调”写法，绘制换成我们的 LVGL 共用控件。
- MuditaOS，`github.com/mudita/MuditaOS` 提交 `2cd0e4721405c0bf263f311f27d38aadae6ac89f`：`module-apps/application-settings/windows/SettingsMainWindow.cpp:14-39`（墨水屏手机的设置分类）。

## 要做的

1. **注册表**（新文件，例如 `system/ui/shell/settings_registry.c` 和头文件）：每个设置项声明一次——编号、名称、别名（搜索用，例如暖光的别名“色温”“夜间”）、所属分组和页面路径、读写的设置或要调的能力、摘要函数（当前值的那句话）、可用状态函数（上面 6 种，其中“对这个家人不可用”给孩子、“依赖项关了变灰”要带原因）、是否只给大人。页面行、搜索索引、AI 能力都从这一份声明生成，不另写第二份会漂移的数据。
2. **首页**：分组（参照 AOSP 和 MuditaOS，用我们已有的项目，不新增功能）——网络和连接、声音和通知、显示和阅读、家人和孩子、AI、存储和备份、系统；每行右边写当前值（例如“Wi-Fi · 家里的网络名”“勿扰 · 关”“文字大小 · 标准”），值变了只刷这一行。
3. **搜索**：首页顶上一个搜索框（共用输入框）。输入“暖光”出结果：名称、路径（“显示和阅读 › 暖光”）、当前值；点结果直接进那一页并定位到那一行。孩子搜不到只给大人的项的值（可以显示“家长管理”，不显示值）。搜索也做成能力（例如 `settings.search`、按路径打开的 `settings.open`），AI 和人得到同一份结果。
4. **已有的二级页**照旧能进（家人、日期时间、蓝牙、移动网络这些页面以后由 `settings-2` 重做，这次只把它们在首页的入口行换成注册表生成的）；声音、存储、关于等 `settings_pages.c` 里的行表改成从注册表来。
5. 每个设置项照旧走 `settings.get`、`settings.set` 或它原来的能力，按钮和 AI 调同一个；页面快照列出分组、每行的名称、当前值、可用状态和能做的动作。

## 共同要求（页面任务都一样）

- 来源：功能、页面、状态、出错处理照上面的来源一个不落，不自己发挥；`docs/tasks/settings-1/SOURCE.md` 写每一部分对应来源的哪个文件和行号，哪些没照搬、为什么。
- 新设计语言（`design-lang-1`）：只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/ui_list.c`、`system/ui/textfield.c` 里的按钮、输入框、列表行、开关、滑杆、分段、空状态）；能点的不小于 108 px；一屏最多一个主按钮；一种输入框；行间细分隔线、组间留白（分组标题行只有灰字，不画框）；图标不套圆圈；没内容的区块不画，整页空才用共用空状态。不写死像素和颜色（`ui-scale` 检查）。旧画板的对照分数（`tools/ui_compare.py`）不作验收。
- 不变的规矩：家人隔离、孩子模式、确认卡只给 D-105 的高风险动作（改密码、恢复出厂这类照旧），少刷新（D-127：只刷变了的那块），按下反馈 D-31（默认不反色，结果就是反馈），密码不给 AI 不进日志。
- 文字大小：跟系统“文字大小”一起缩放（`ink_ui_px`），默认和最大一档各出一套截图；放不下就分页或换行，不缩字、不裁按钮。
- 原地重写，旧代码删掉，不留两套。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由），新文案的英文加进 `assets/i18n/en.tsv`，这两个文件都只在末尾追加本任务的行；合并冲突由主会话处理。
- 截图放 `docs/tasks/settings-1/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。

## 范围和预算

- 只改：`system/ui/shell/settings_body.c`、`system/ui/shell/settings_pages.c`、新的 `settings_registry.c` 和头文件、设置相关测试、CMake 里新文件的那一行、`docs/tasks/settings-1/`。
- 不碰：`settings_family.c`、`time_settings.c`、`bluetooth_page.c`、`mobile_network.c`（以后 `settings-2`）；`theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c`（`design-lang-1`）；`status_bar.c`、`control_center.c`（`control-1`）；`system/ui/search.c`（全局搜索接设置的结果以后再接，写进报告）。设置服务（`system/policy/settings.c`）缺小接口时可以增量加，列进报告。
- 能力登记：新能力在本任务自己的文件里用 `ink_cap_register` 登记（它可以调用多次）；`system/ui/shell/navigation.c` 拆完以后不派任务，确实要在它的初始化里加一行调用就只加这一行，列进报告，别的地方不改。
- 产品代码不超过 600 行；固件增量不超过 20 KB。
- 起步：主会话给的分支（18:30 合并后的主线，已含 `shell-split-1`，再合上 `design-lang-1`）；合并排在 `design-lang-1` 之后。

## 验收

1. **照 AOSP 设置走一遍**（`SETTINGS.md` 末尾列的演示，模拟器脚本，每步截图并断言）：打开设置 → 首页每组每行都有当前值 → 搜“暖光”→ 结果写着“显示和阅读 › 暖光 · 当前值”→ 点进去 → 调一格 → 返回首页，暖光那行的值已经变了；搜别名“色温”也能找到；关掉 Wi-Fi → Wi-Fi 那行写“已关闭”；一个依赖别的开关的项在依赖关着时变灰并写原因。同一条路径用能力调用（来源是 AI）再走一遍，搜索结果和打开的页面一样。
2. **孩子**：换成孩子，首页只出现孩子能看的项；搜“密码”看不到大人专用项的值。
3. **一份声明**：加一个测试，从注册表随便取一项，确认它同时出现在页面行、搜索结果和能力的说明里；反向对照：故意让某一项只出现在页面里，测试必须失败。
4. **刷新**：改一项设置只刷首页那一行（模拟器显示统计断言）。
5. **改前改后截图**：设置首页、搜索结果、一个二级页、孩子的首页，默认和最大文字大小各一套。
6. 主工程普通全量 ctest 过（含 `ui-scale`）；ASan 跑设置相关测试和 `ui-scene-*` 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写固件增量、产品代码行数、`gcccheck` 结果、改过的已有测试（只改定位方式和页面结构，不改业务断言），以及给 `settings-2` 用的注册表写法说明（一项怎么加）。
