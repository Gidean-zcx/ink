# settings-1 的来源对照

行号照任务书和 `docs/tasks/frameworks-research-1/SETTINGS.md` 给的提交（本机没有这三个仓库的克隆，`work/` 里也没有，行号没有在本机重新打开核对，标【引用】）。我们的实现都在 `system/ui/shell/` 下，C 加 LVGL 共用控件，没有拷贝任何一行 Java 或 C++ 原文。

| 部分 | 来源（文件:行） | 我们怎么做的 | 没照搬的，为什么 |
| --- | --- | --- | --- |
| 一份声明加回调的注册表 | PebbleOS `aa0c27e8` `pebble/apps/system/settings/menu.c:23-50`、`menu.h:40-68`（设置模块表，每个模块一项、带回调）【引用】 | `settings_registry.c` 的 `ITEMS[]`：每项一行，编号、名称、别名、图标、分组、所在页、读写的设置、点按的能力、摘要回调 `summary`、可用状态回调 `state`、是否只给大人。首页行、列表页行、搜索、`settings.open` 的编号清单都只读这张表 | Pebble 的模块是整页（窗口入栈），我们的一项是一行；入栈用我们已有的页面和能力（`ink_shell_open_page`、`shell.*`）|
| 选项行和选择回调 | PebbleOS `option_menu.c:26-80`【引用】 | 列表页的行（`settings_pages.c`）照旧由共用列表 `ink_ui_list` 画，点按走行上声明的能力（`settings.set` 或 `cap`/`args`）| 绘制换成我们的 LVGL 共用控件，不搬 Pebble 的 MenuLayer |
| 首页分组、每行一个控制器、摘要运行时换 | Lineage `f8218402` `res/xml/top_level_settings.xml:30-94`【引用】 | 首页 7 组（网络和连接、声音和通知、显示和阅读、家人和孩子、AI、存储和备份、系统），每行右边是 `summary` 算出的当前值 | 组和行用我们已有的项目，不新增功能；AOSP 的动态注入入口（`DashboardFragment` 的 tiles）不做 |
| 6 种可用状态 | Lineage `core/BasePreferenceController.java:75-121`【引用】 | `ink_sr_state_t`：AVAILABLE、UNSEARCHABLE、NOT_NOW（CONDITIONALLY_UNAVAILABLE）、UNSUPPORTED（UNSUPPORTED_ON_DEVICE）、PARENT（DISABLED_FOR_USER，给孩子，写“家长管理”）、DEPENDENT（DISABLED_DEPENDENT_SETTING，变灰写原因）。一个函数 `ink_sr_value()` 同时给出状态和这一行写什么 | AOSP 的 DISABLED_FOR_USER 指工作资料等多用户限制，这里换成本项目的孩子；UNSUPPORTED 在列表页照旧变灰写原因（原来的“这台设备暂不支持叫名字”），首页和搜索不出现 |
| 搜索索引和页面同一份描述 | Lineage `search/BaseSearchIndexProvider.java:52-110`，`BasePreferenceController` 的搜索钩子（约 221、318、379 行）【引用】 | `ink_sr_search()` 直接扫同一张表：名称开头、名称包含、别名或分组包含三档；UNSEARCHABLE、NOT_NOW、UNSUPPORTED 不进结果；孩子能搜到只给大人的项，但只写“家长管理”、不能点 | AOSP 的索引是 SettingsIntelligence 里的数据库，这里 60 项直接线性扫，不建索引、不存历史和建议 |
| 结果写路径 | AOSP 搜索结果的面包屑用 “ > ” 连接（`search_breadcrumb_connector`）【估算，没在本机核对】 | 结果第二行写“显示和阅读 > 暖光 · 3” | 任务书写的是 “›”，字体里没有这个字（`tools/fontgen` 的 punct 集合不含 U+203A），改用 “ > ” |
| 点结果进页并定位那一行 | AOSP 打开设置页时高亮搜到的那一项（`HighlightablePreferenceGroupAdapter`）【估算，没在本机核对】 | `settings.open`：手画的页（显示、声音、网络）翻到那一块并画一圈 3 px 聚焦框；列表页（语音和 AI、存储、关于、隐私、设备详情）翻到那一页并用共用列表的选中框 | 墨水屏不做高亮动画，只画一次静止的框 |
| 控制器刷新当前状态 | Lineage `dashboard/DashboardFragment.java:462-481`【引用】 | 首页记住本页画出的行；设置或状态变了，重新算每行的值，只把变了的那个标签换掉（`home_update()`）；行数、灰不灰、要不要换行变了才整页重画 | — |
| 墨水屏手机的分类 | MuditaOS `2cd0e472` `module-apps/application-settings/windows/SettingsMainWindow.cpp:14-39`【引用】 | 分组顺序参照它（连接在前、显示、手机模式、安全、系统在后），换成任务书的 7 组 | MuditaOS 用按键焦点导航，这里是触摸加翻页 |

## 我们自己的部分（成熟来源里没有）

- 孩子：首页只出现不只给大人的项；孩子从首页点暖光、亮度、音量、勿扰进的显示和声音页里，字号、自动锁屏、声音去向、通知这些大人的块不出现（规则 8，孩子碰不到墙）。
- AI：`settings.search` 和 `settings.open` 是能力表里的原子能力，人点结果和 AI 调用走同一个；`settings.open` 的参数说明是从注册表生成的全部编号；首页快照列出分组、名称、当前值、可用状态和这一行的动作。
- 设置的读写照旧只走 `settings.get`、`settings.set` 和原来的能力，注册表不存值。
