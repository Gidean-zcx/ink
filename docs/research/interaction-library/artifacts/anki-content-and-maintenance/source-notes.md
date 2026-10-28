# Anki：内容对象、分发与维护来源摘记

2026-10-27。官方Desktop手册公开Markdown正文；无Anki安装、登录、执行手册命令、图片观察或软件实操。当前实际构建未知。页内最低版本和历史排错保留，读取全部目录页面不等于所有分支已验证。

| E编号 | 来源 | 章节与短摘记 |
| --- | --- | --- |
| E-ANCM-01 | [Introduction](https://docs.ankiweb.net/manual/intro.md) | Mobile Clients; Legacy Documentation；桌面手册与移动手册分开，翻译可能滞后；仅范围目录。 |
| E-ANCM-02 | [Getting Started](https://docs.ankiweb.net/manual/getting-started.md) | Key Concepts; Shared Decks；笔记可派生多卡并分别调度；牌组与笔记类型独立。Get Shared下载后在客户端导入，再同步；不能直接把共享包加入AnkiWeb。 |
| E-ANCM-03 | [Background](https://docs.ankiweb.net/manual/background.md) | Active Recall Testing; Spaced Repetition; Why Anki；介绍主动回忆和间隔复习；效果、遗忘比例与对其他产品评价为上游叙述，本批未据此证明学习效果或比较优势。 |
| E-ANCM-04 | [Getting help](https://docs.ankiweb.net/manual/getting-help.md) | Asking good questions; platform support routes；先查排错与已有问题；求助应提供操作、实际错误和已尝试步骤。论坛身份与AnkiWeb不同；没有提交求助或创建账户。 |
| E-ANCM-19 | [Math and Symbols](https://docs.ankiweb.net/manual/math.md) | MathJax; LaTeX / Web-Mobile; Cloze Conflicts；选TeX文本→MathJax inline→Cards预览；换行方式有要求。LaTeX图片须电脑生成，可经Check Media批量生成后同步；移动端显示已有图，不能生成。 |
| E-ANCM-23 | [Self-Hosted Sync Server](https://docs.ankiweb.net/manual/sync-server.md) | Multiple Users; Storage Location; Client Setup; Reverse Proxies；独立同步服务存集合/媒体副本；客户端Preferences配URL与AnkiWeb account凭据。协议版本、子路径末尾斜杠与媒体上传限制影响连接；自托管不同于离线备份。 |
| E-ANCM-25 | [Contributing](https://docs.ankiweb.net/manual/contrib.md) | Sharing Decks Publicly; Sharing Decks Privately；公开分享先同步、网页Share；改牌组路径可能使更新无法匹配。接收者须重新下载导入，非自动推送。私享导出单牌组且去调度；新版包删除项不自动删接收者卡。 |
| E-ANCM-26 | [Resources](https://docs.ankiweb.net/manual/resources.md) | Resources；只有外部资源清单链接，本页不证明具体操作；未声称读完所链论坛。 |
| E-ANCM-27 | [Platform Notes](https://docs.ankiweb.net/manual/platform/intro.md) | Platform Notes；Windows/macOS/Linux目录父页；不作为独立功能。 |
| E-ANCM-28 | [Windows](https://docs.ankiweb.net/manual/platform/windows/intro.md) | Installing; Problems；Windows安装与故障目录父页。 |
| E-ANCM-29 | [Installing & Upgrading Anki on Windows](https://docs.ankiweb.net/manual/platform/windows/installing.md) | Requirements; Installing; Upgrading; Add-on Compatibility；按x64/ARM64与Windows版本选包；关闭程序再升级，文档称保留卡数据。旧版本/不同发行包另有卸载说明，插件兼容和降级不能由升级承诺推出。 |
| E-ANCM-30 | [Windows installation issues](https://docs.ankiweb.net/manual/platform/windows/installation-issues.md) | Error opening file for writing; Antivirus issues；安装写文件失败：关闭程序/浏览器，仍失败则重启后重试。仅问题分流，未测修复结果。 |
| E-ANCM-31 | [Windows startup issues](https://docs.ankiweb.net/manual/platform/windows/startup-issues.md) | No error; Multiple installations; Debugging；启动无窗、缺依赖、多安装副本各有分支；控制台输出用于定位，25.07–25.09.4另有入口路径。历史Windows排错不冒充当前支持范围。 |
| E-ANCM-32 | [Windows Display Issues](https://docs.ankiweb.net/manual/platform/windows/display-issues.md) | Changing Driver; Full Screen；23.10+可在Preferences改显示驱动；旧版配置文件可切换/恢复。正文早段默认software与末段默认Direct3D表述不齐，保留版本疑点。 |
| E-ANCM-33 | [Copy and paste issues](https://docs.ankiweb.net/manual/platform/windows/copy-and-paste.md) | Copy and paste issues；剪贴板监控工具可能干扰复制粘贴；文档没有稳定复现、具体错误UI或必然修复承诺。 |
| E-ANCM-34 | [Text Size](https://docs.ankiweb.net/manual/platform/windows/text-size.md) | Text Size；网页区域缩放与菜单等原生界面缩放分开；环境变量是桌面排错方法，不是移动手势或电子纸刷新算法。 |
| E-ANCM-35 | [Windows permission problems](https://docs.ankiweb.net/manual/platform/windows/permission-problems.md) | Permission Problems; Debugging Permission Problems；正文讨论本地文件只读/权限错误，含access denied字样；这不是网站访问挑战。记录权限排错与日志提供路径，未执行权限或安全软件变更。 |
| E-ANCM-36 | [macOS](https://docs.ankiweb.net/manual/platform/mac/intro.md) | Installing; Problems；macOS目录父页。 |
| E-ANCM-37 | [Installing & Upgrading Anki on macOS](https://docs.ankiweb.net/manual/platform/mac/installing.md) | Installing; Upgrading; Add-on Compatibility；下载并拖放应用；升级关闭后覆盖，文档称保留卡数据；旧macOS对应旧构建，插件兼容另核。 |
| E-ANCM-38 | [Display Issues on macOS](https://docs.ankiweb.net/manual/platform/mac/display-issues.md) | Change Video Driver; eGPUs；23.10+ Preferences改驱动后需重启；旧版配置文件可恢复auto；外置显卡空白另有选项，未实测。 |
| E-ANCM-39 | [Linux](https://docs.ankiweb.net/manual/platform/linux/intro.md) | Installing; Problems；Linux目录父页。 |
| E-ANCM-40 | [Installing & Upgrading Anki on Linux](https://docs.ankiweb.net/manual/platform/linux/installing.md) | Requirements; Installing; Upgrading; System Qt versions；官方包有架构/glibc/依赖条件，升级保留用户数据；发行版包与官方包、Qt/输入法适配不同，不能推断Plus可直接运行。 |
| E-ANCM-41 | [Missing Libraries](https://docs.ankiweb.net/manual/platform/linux/missing-libraries.md) | Missing Libraries；从终端启动并依错误补依赖；诊断可能多轮，不把执行命令等同恢复完成。 |
| E-ANCM-42 | [Display Issues on Linux](https://docs.ankiweb.net/manual/platform/linux/display-issues.md) | Changing Driver From Preferences / Terminal；Linux默认硬件加速；空白可试软件渲染，23.10+设置入口与旧配置方式分开。 |
| E-ANCM-43 | [Blank Main Window](https://docs.ankiweb.net/manual/platform/linux/blank-window.md) | Blank Main Window；文档将部分空窗与glibc/Qt兼容关联，建议新版Qt6或所链历史讨论；链接方案未逐个执行或验证。 |
| E-ANCM-44 | [Packages Distributed by Linux Distributions](https://docs.ankiweb.net/manual/platform/linux/distro-packages.md) | Linux Distro Packages；发行版可能替换依赖或关闭更新检查，问题支持对象随包来源不同；不概括所有发行版包都不可用。 |
| E-ANCM-45 | [Anki not picking up GTK theme on Gnome/Linux](https://docs.ankiweb.net/manual/platform/linux/gtk-theme.md) | Incorrect GTK Theme；指定GTK主题后需退出登录再登录；是环境排错，不是App内已观察设置过程。 |
| E-ANCM-46 | [Wayland](https://docs.ankiweb.net/manual/platform/linux/wayland.md) | Wayland；2.1.48起可显式启用；文档列无边框、窗口无法前置等条件，实际发行版/当前构建未知。 |
| E-ANCM-47 | [Input Methods on Linux](https://docs.ankiweb.net/manual/platform/linux/input-methods.md) | Fcitx；标准构建声称含fcitx支持但非所有发行版可用；与安装页关于PyQt特性受限一起保留，未测试中文输入。 |
| E-ANCM-51 | [Styling & HTML](https://docs.ankiweb.net/manual/templates/styling.md) | Card/Field Styling; Browser Appearance; Fonts; Fading; Javascript；Styling默认影响同笔记类型所有卡，可限定卡/字段；编辑字体与复习样式、浏览摘要模板分开。字体作为媒体且名称匹配；JS跨客户端/升级兼容不保证。 |

## 同一来源的补读

| 旧E | 本次复读章节 | 增补范围 |
| --- | --- | --- |
| E-AND-05 | Customizing Fields; Using Tags/Flags | 编辑字体/方向仅编辑域，排序字段只选一个；标签作用笔记，旗标作用单卡。 |
| E-AND-09 | Rows; Notes/Cards; Find and Replace; Finding Duplicates | 卡/笔记选区换算；多选隐藏编辑器。批量Toggle按当前项状态决定目标；替换默认仅选中笔记，可去限制。查重跨含同名字段的笔记类型，标签标记不等于合并删除。 |
| E-AND-10 | Simple searches; Limiting to a field | 普通词为子串匹配，限定字段默认全字段精确；富文本分段可影响命中。查询规则仍非完整操作符实测。 |

## 取证记录与冲突

worker获取16页后，将权限帮助中的access denied字样误判为挑战并停止。本路线日志如实保留。root正常打开对应官方HTML，确认是本地权限排错正文，再以普通公开GET取得剩余14页；两份日志分别记录，没有变更身份、绕过访问限制或执行手册中的系统命令。原始16页未重抓。

目录初筛把Contributing当概念页；读正文后发现有公开/私下分发流程，改为F-M020-25。Background中的学习优势、遗忘比例及对竞品评价只记为上游说法，不当独立研究结论。Windows默认渲染器表述与Linux输入法条件不强行拼成单一当前版本事实。

本次30个新URL对应目录页，不是30个新功能。仅三项新任务；既有制卡、模板、浏览、导入及排错补回旧F。公式生成、同步与分发均没有实际成功/错误/恢复记录。
