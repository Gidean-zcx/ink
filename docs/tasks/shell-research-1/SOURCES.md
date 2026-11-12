# 来源与观察范围

查阅日期均为 **2026-11-12**。官方帮助、宣传图和代码各自只能证明其所描述或呈现的内容；本次没有操作这些设备。以下“观察”都是静态资料观察，不是产品实测。像素建议见 [DESIGN.md](DESIGN.md)，源码固定版本见 [REUSE.md](REUSE.md)。

## S01 Kindle

- [Amazon：进入 Kindle 书库](https://www.aboutamazon.com/news/devices/how-to-find-your-kindle-library)：最近读过的内容与底部书库入口，阅读中可回主页。
- [Scribe 2024 指南的顶部栏原图](https://d1ergij2b6wmg5.cloudfront.net/kug/kindle_scribe_2024/v1/en-US/image/chrome_en.png)：已下载、实看 `work/refs/kindle-scribe-home.png`。**这张图只有状态栏和搜索框，不是完整主页**；不能拿它证明书架的具体几何。
- 可取：内容入口、固定工具栏、熟悉的搜索形状。商店推荐和销售内容不作为本项目默认内容。

## S02 BOOX

- [官方 Home Screen and Dock](https://booxsupport.zendesk.com/hc/en-us/articles/10701108549140-Home-Screen-and-Dock)，明确适用 **Tab 系列**，不是所有 BOOX 型号的同一桌面。
- [官方配图](https://booxsupport.zendesk.com/hc/article_attachments/10725493990676)，已下载、实看 `work/refs/boox-tab-home.jpg`：上方书库与笔记组件，下方应用网格、分页点及常用入口。
- 可取：内容小组件、网格和 Dock 共存。图中也有边框；不能据此宣称“成熟墨水屏都不用框”。大平板的并排组件不能原比例缩进 4.7 寸。

## S03 掌阅 iReader

- [Ocean 4 长续航版 2025 官方认证店](https://detail.youzan.com/show/goods?alias=1y43umgr1e67n0i&from_source=gbox_seo)，仅用商品和界面图，不采用价格、健康或性能宣传。
- [手势图](https://img01.yzcdn.cn/upload_files/2025/04/24/FsF-7MceezhW-9HxW8dzHaKROQEl.jpg)：`work/refs/ireader-ocean-gestures.jpg`，已实看；图示七个手势区域，不能证明每个区域的默认功能。
- [实体键与阅读页图](https://img01.yzcdn.cn/upload_files/2025/04/23/FqUYk6TYFG4-Ss7jT8HZQCbrvT0l.jpg)：`work/refs/ireader-ocean-keys.jpg`，已实看。可取中文排版、正文优先、实体键捷径；没有取得这款系统外壳的完整尺寸规范，不反推字号或确认框像素。

## S04 Kobo

- [官方 Home screen 帮助及配图](https://help.kobo.com/hc/en-us/articles/360017640133-Kobo-eReader-Home-screen)：最近四本书、封面直达、书库及搜索；移出主页不等于删书。
- 黑白主页原图链接为 [article attachment 25996600897815](https://help.kobo.com/hc/article_attachments/25996600897815)。本次浏览工具可定位原图，独立下载超时，**未把原图下载或看图完成写成事实**。方案 C 的近期内容结构依据官方正文。
- [官方 SleepCover 帮助](https://help.kobo.com/hc/en-us/articles/360017706434-Adjust-SleepCover-settings)：只证明 Kobo 的盖合休眠行为，不能给 Plus 增加没有核实的霍尔开关能力。

## S05 reMarkable

- [官方 Navigating on your reMarkable](https://support.remarkable.com/articles/Knowledge/Navigating-on-your-reMarkable)：My files、列表/网格、搜索和最近/收藏抽屉。
- [官方 3.12 My files 改版](https://remarkable.com/blog/release-3-12-a-more-organized-my-files)：文件类型缩略图和回根目录入口；这是特定历史版本说明。
- 可取：文档主导的排版、清楚的层级和回原文件。大屏侧栏、双指手势不直接套到本机。未取得可直接使用的完整外壳源码。

## S06 Mudita Kompakt

- [官方产品页](https://mudita.com/products/phones/mudita-kompakt/) 的两张图已下载、实看：`work/refs/mudita-kompakt-home.jpg`（时钟、电话/短信等少量入口；[下载图](https://images.ctfassets.net/isxmxtc67n72/1kcxpZnMR8Zy35ShI8CPnL/0fdb1d3e42d4f2242024cc605c8e1bb6/Column.jpg?w=670&h=671&fl=progressive&q=90&fm=jpg)）与 `work/refs/mudita-kompakt-overview.jpg`（静态待机画面；[下载图](https://images.ctfassets.net/isxmxtc67n72/1PfonTBArwxrGY4jPNTzQh/5c897ff9a891e7b19003eee71806e70c/Frame_198.jpg?w=1200&h=680&fl=progressive&q=90&fm=jpg)）。这是产品照片，固件构建号未知。
- [官方第三方应用说明](https://support.mudita.com/en/support/solutions/articles/77000577356-does-mudita-kompakt-support-third-party-apps-)：**MuditaOS K 基于 AOSP**。下一个条目的 MuditaOS 不能冒充 Kompakt 的现成外壳代码。
- [官方首次设置](https://support.mudita.com/en/support/solutions/articles/77000579071)：分步设置、可跳过教程。借流程秩序，不套用指纹硬件。
- [官方 1.4.0 说明](https://support.mudita.com/en/support/solutions/articles/77000589204)：锁屏音乐控件与充电信息按场景出现。不能推出 Plus 的锁屏音频权限相同。

## S07 MuditaOS

- [官方仓库](https://github.com/mudita/MuditaOS)，FreeRTOS，面向 **Pure 与 Harmony**；本轮读取桌面、对话框、状态栏、锁定窗口，版本见 REUSE。
- 可取：黑白层次、时钟页状态栏不重复时间、统一导航语义。Pure 的实体键选中态和非触控布局不能等同于 Kompakt，也不照搬到 9 mm 触控标准。

## S08 Light Phone III

- [官方屏幕说明](https://support.thelightphone.com/hc/en-us/articles/360030784132-Display-Screen-Light)：III 是 **AMOLED**，黑白界面；II 才是 E Ink。本轮不把 III 的滚动、速度或耗电用作墨水屏证据。
- [官方 Toolbox Menu](https://support.thelightphone.com/hc/en-us/articles/360039578512-Toolbox-Menu)：工具列表可排列，电话和设置不能删除。[工具状态说明](https://support.thelightphone.com/hc/en-us/articles/360031128671-Tool-Availability-Status)说明其排版主导的工具设计。
- [产品图出处](https://www.thelightphone.com/lightiii)：本次独立抓取只有 JavaScript 壳，没有拿到可核实的 III 工具列表截图。方案 B 的截图参考用 S06 的少量入口照片，列表结构依据本条正文与 S07 源码，二者不混称为同一画面。

## S09 PebbleOS

- [官方 Recommended Guidelines and Patterns](https://developer.repebble.com/guides/design-and-interaction/recommended/)：黑白 aplite 的列表、动作栏、状态栏示意，以及保持导航位置、直接反馈。[One Click Actions](https://developer.repebble.com/guides/design-and-interaction/one-click-actions/)补常用动作捷径。
- 官方同页也建议动画和完成提示窗；这些**不采用**，分别受本项目 D-127 和“就地反馈”约束。表盘/手表屏技术和操作方式不等同于本机电子纸触屏。

## S10 海信墨水屏手机

- [海信官方 A9 页面](https://mall.hisense.com/items/4769)可定位机型；独立下载超时，未取得完整官方交互手册。
- [作者一手开箱](https://sblt.deali.cn/blog/eink-hisense-hi-reader)明确区分 Hi Reader 的标签桌面与 A9 的传统手机桌面、小组件。照片证据复核沿用仓库已存的 Hi Reader 引导图，只复制到 `work/refs/hisense-hireader-guide.webp`；实看后仅用来理解控件分组，**不把它称为 A9 截图**。
- [天极网样机评测](https://mobile.yesky.com/283/725998783.shtml)是 2022 年一手评测，当前固件未知。仅用于历史界面线索，不采用其护眼、性能、刷新速度结论。
- 可取：手机式应用组织和固定通信入口。刷新模式和通知/控制中心细节交给并行调研，本任务不另选一套。

## S11 iOS HIG

- [Typography](https://developer.apple.com/design/human-interface-guidelines/typography.md)、[Buttons](https://developer.apple.com/design/human-interface-guidelines/buttons.md)、[Text fields](https://developer.apple.com/design/human-interface-guidelines/text-fields.md)：字号/字重层次、主要和次要动作、字段标签与长短文本区分。
- [Alerts](https://developer.apple.com/design/human-interface-guidelines/alerts.md)、[Action sheets](https://developer.apple.com/design/human-interface-guidelines/action-sheets.md)、[Sheets](https://developer.apple.com/design/human-interface-guidelines/sheets.md)：重要决定、主动选择、上下文短任务各有容器；返回与关闭含义不同。
- [Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility.md)：参考语义和可读性；本机的实际像素按 297 dpi 估算，不把 iOS pt 当物理印刷点或面板 px。玻璃效果、模糊、阴影、彩色语义和动画不移入。

## S12 Material 3

- [Dialogs 规范入口](https://m3.material.io/components/dialogs/guidelines)、[Bottom sheets 规范入口](https://m3.material.io/components/bottom-sheets/guidelines)仅返回 JavaScript 壳；补读官方实现文档 [Dialog.md](https://github.com/material-components/material-components-android/blob/master/docs/components/Dialog.md)、[BottomSheet.md](https://github.com/material-components/material-components-android/blob/master/docs/components/BottomSheet.md)，核对默认主题和 modal/standard 区别。
- 官方 [Compose dialog](https://developer.android.com/develop/ui/compose/components/dialog)、[bottom sheets](https://developer.android.com/develop/ui/compose/components/bottom-sheets)、[snackbar](https://developer.android.com/develop/ui/compose/components/snackbar)、[触控目标](https://developer.android.com/develop/ui/compose/accessibility/api-defaults)：M3 组件语义、短暂反馈和最小 48 dp 命中范围。DESIGN 中的 px 是本项目建议，不冒称 M3 原始像素规格。

## S13 AI：豆包、Gemini、Siri

- [豆包手机助手官网](https://o.doubao.com/)：搜索索引可读其 AI 键、当前屏幕与经授权的数据调取说明；独立 HTML 是脚本入口，**没有完成底部面板的逐帧视觉核查**。因此只借随处唤起、就地操作的方向，不宣称复刻其面板高度或权限实现。
- [Gemini 官方 screen actions](https://support.google.com/gemini/answer/15850607?hl=en-EN)：从当前应用唤起、屏幕/文件上下文入口；采用“让上下文可辨识”的交互，仍按我们自己的披露和确认规则。
- [Apple 官方 Siri HIG](https://developer.apple.com/design/human-interface-guidelines/siri.md)：应用暴露动作和实体、标注页面内容的语义路线。采用同路能力与上下文引用，不依赖发光边缘提示，也不假定本设备可运行 Apple/Gemini/豆包的客户端。

## S14 全局导航、成员与代码

- [Android 官方多用户](https://support.google.com/android/answer/2865483?hl=en)：快速选人、各人的主页与数据空间分开；本项目身份、孩子权限仍以既有要求为准。
- AOSP、MuditaOS、PebbleOS、InfiniTime、ESP-Brookesia 的准确文件、行号、版本和可复用边界集中在 [REUSE.md](REUSE.md)。
- 查询 `repo:Gidean-zcx/ink is:open (shell OR 外壳)` Issue、`repo:Gidean-zcx/ink is:open (shell OR research)` PR，GitHub 连接器均返回空列表；只说明这两个查询未命中，不说明所有并行工作都不存在。任务书已明确 frameworks-research-1 的边界。
- 交互决策板原链接本次无法访问，未读到集合 `decisions`，没有假称同步。以基线决策记录为本次对照；本任务禁止改共享决策。

## 本地参考图清单

下面都是不跟踪的 `work/refs/` 文件，**没有第三方图片进入本次提交**。本项目既有截图只在只读的 `$INK_DESIGN/shots-20261111/` 查看，也没有复制进提交；它们是 2026-11-11 留存图，不是本次对给定基线重跑的结果。

| 文件名 | SHA-256 | 出处 |
| --- | --- | --- |
| boox-tab-home.jpg | `239d7b8fc1560807591d481f7c02c56dc07e212766e003a56514d45a858b5e51` | S02 原图 |
| mudita-kompakt-home.jpg | `77e03f73500cd9a43c1332781f330c398a715f4b8bcf98ff45b7841a9a9a1484` | S06 产品页 Column.jpg |
| mudita-kompakt-overview.jpg | `d5977e981dfa4751d0c5dde87bf000ecd3ff04140dfcc6008dd1d9cf9a6963dc` | S06 产品页 Frame_198.jpg |
| ireader-ocean-gestures.jpg | `6f5b793938b892e955086db8ff40f672c7d3b35323333cd3685c939df0bcba6b` | S03 手势图 |
| ireader-ocean-keys.jpg | `466a48a32f28f6f12752ff2faac32704d81344468876278544ed1c5767abd63c` | S03 实体键图 |
| kindle-scribe-home.png | `98af5417ded8f1f71cf96ebe2b7b601bdb27b9bdfab724e4f6626c097e3255d3` | S01 顶部栏原图 |

Hi Reader 图的原有证据链见 [既有来源摘记](../../research/interaction-library/artifacts/hisense-controls-deepening/source-notes.md)。下载中遇到的 403、超时和脚本壳均保留为证据缺口；不绕过访问限制、不登录、不下载固件。
