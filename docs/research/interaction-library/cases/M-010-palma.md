# M-010 BOOX Palma 2：单手入口、按应用显示与读听返回

## 范围

初查2026-10-26，本轮补读/静态观察2026-10-27。对象仅为 **Palma 2 黑白 6.13 英寸、Android 13、NeoReader**；不含 Palma 一代、Palma 2 Pro、Palma 3。实际固件小版本/NeoReader 构建未知；当前通用帮助中的 V4.1+ 迁移能力单列为条件路径，官方通用更新文章不能单独证明本机已安装或收到该版本。[E-PALMA-01～03]

**D1+D2 文档研究，无实机、无连续视觉观察。D2只表示下表明确条件下的通用文档路径，不表示Palma 2当前构建已经验证。** 官方产品页仍写 HD/Balanced/Fast/Ultrafast 四模式；当前帮助则已采用 EinkWise 等新界面，不能混成同一固件截图。本轮取得原零售商URL的103页PDF，文件名含Palma 2 Pro，封面实际为Mobile ePaper Series通用V4.0；只作历史通用路径与静态对照，不能作为Palma 2专属或已装固件手册。[E-PALMA-15]

## 功能目录

| 域 | 已查 | 待查/适用边界 |
| --- | --- | --- |
| 单手控制 | 指纹电源键、翻页键、智能键短按/长按/双击；触控翻页区域 | 指纹录入/失败、按 app 实体键映射、锁屏生效、误双击；智能键流程仅 D1 |
| 系统/显示 | 双下拉面板、导航球、EinkWise、app 冻结 | Palma 2 当前首次向导、精确 Settings 层级、完整模式/刷新默认值 |
| 阅读 | 选段批注/查词、目录/书签、历史视图、TTS 入口 | 扫描件 OCR、断网朗读、音频焦点与逐句恢复 |
| 内容/数据 | BOOXDrop、云推送、源书与状态、V4.1备份 | 同名覆盖、下载删除、进度冲突、卡移除；未套用大屏 Notes/分屏能力 |
| 其他 | 官方列相机扫描、录音、第三方 app、小组件、屏保 | 相机 OCR 导出、录音编辑、所有屏保/小组件完整操作目录未核定 |

手机外形不能证明蜂窝通话。官方规格为 Wi-Fi/蓝牙，研究中的“来电打断”仅能作为第三方通信 app 的待测场景；不借 Palma 2 Pro 的 SIM/笔功能补本机。也不把安装 Android app 当成原生统一读听进度。

## 本轮来源目录与观察边界

[逐项目录](../batches/palma-controls-deepening-inventory.csv)列出V4.0 PDF全部73个书签目录项、旧官方PC类帮助59篇/14节及5项关键缺口。它分别标注目录、选定正文与静态观察；不把这些条目全部算作Palma 2功能。本批采用PDF书签目录，不称印刷总目录；页号使用PDF页序，所看4页无印刷页号，不能冒称印刷页码。

Palma 2官网Smart Button宣传图显示短按、双击、长按三项且例值均None；通用手册p86示例分别是Full refresh、AI Assistant、None。两图都没有执行动作；不能据此指定默认值或推断某次固件改变。[E-PLCD-03～04]

本轮实际看图仅有这张本机宣传图和通用PDF p1/86/95/97。具体观察、图片与来源身份见[静态审计](../artifacts/palma-controls-deepening/source-notes.md)。历史“Palma 2手册”论坛帖及链接附件GET404已停止；搜索索引的答复不能补成当前可得的专用手册。

## 主要任务

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M010-01 | 单手翻页与快捷动作（D1） | Palma 2；已配置按键 | 型号页确认智能键及指纹电源键；官方静态图可见三种按法设置。通用FAQ提示某些阅读App另需启用音量翻页；未取得本机完整设置链。 | Palma 2宣传图有短按/双击/长按三项且例值均None；通用V4.0例值不同，均不能证明出厂默认。通用FAQ要求部分App另开音量翻页。 | E-PALMA-01；E-PALMA-02；E-PLCD-01；E-PLCD-03；E-PLCD-04 | 本机设置入口、动作列表、按App事件映射、默认值、锁屏/休眠触发及指纹失败；静态图不闭合执行链。 |
| F-M010-02 | 进入控制、处理通知后回阅读 | app 开启 | 通用帮助：右上控制中心、左上通知中心；Settings→System Navigation可配置底/侧导航，导航球双击回桌面。V4.0图仅确认相应入口存在。 | 通用帮助给双下拉/导航球/导航路径，V4.0图可见手势或导航栏与侧手势入口；回桌面、返回阅读与结束媒体不是同一动作。 | E-PALMA-04；E-PALMA-15；E-PLCD-04 | Palma 2已装版本的入口、默认手势、通知媒体键、退出选区和返回位置；通用图未操作。 |
| F-M010-03 | 为阅读和滚动分别调显示 | app 已开；版本辨明 | 当前帮助：控制中心 → EinkWise；按 app 调模式，Full Refresh 可清残影。产品页旧四模式作为历史对照，不能把最新帮助的 Smooth 指定给本机。 | “本次刷新”“app 配置”“全局默认”不同；不是把更快模式设好便永久无残影。 | E-PALMA-01；E-PALMA-05；E-PALMA-06 | 当前本机实际模式及保存范围；延迟、灰阶与残影不作测量结论。 |
| F-M010-04 | 修正单手触摸翻页区 | NeoReader 已开 | 点中央 → 更多 → Settings → Touch Settings，选预设或自定义触摸区域。 | 改的是 NeoReader 触控行为，不能据此推定第三方 app/实体键同步改变。 | E-PALMA-07 | 防误触、左右手切换、旋转后的映射；预设图未逐一观察。 |
| F-M010-05 | 看书时查词、标注并回原文 | 可选文本；有词典 | 通用帮助：长按选文→标注；Contents→Annotations可管理和导出。中央菜单→More→Settings→Annotation Export Settings调整出口；未执行导出。 | 通用帮助：Contents→Annotations管理/导出到本地、邮件或其他账户；中央菜单→More→Settings→Annotation Export Settings。不采用该页另列的手写出口。 | E-PALMA-08；E-PALMA-09 | Palma 2实际导出格式/路径、覆盖及取消、原书与副本关系、重装恢复；关闭浮层后选区、离线词库及重复词合并。 |
| F-M010-06 | 查章/跳页后恢复阅读位置 | 同一文档内 | 通用帮助：点页角书签，空心变实心；Contents定位书签/目录。Progress跳页/章，Prev View和Next View往返文内视图。 | 通用帮助用页角书签图标空心变实心表示添加，Contents定位；Prev/Next View为文内视图历史，不是跨App/重启恢复。 | E-PALMA-10；E-PALMA-11 | 本机当前构建、字号变化后的锚点、关闭书后的历史长度、书签删除与同步冲突。 |
| F-M010-07 | 从读切听并排查后台停止 | NeoReader/TTS 引擎已可用 | Progress→TTS及系统引擎设置仍是通用路径。App Optimize→Others的保活、App Management冻结与Power休眠控制分别检查；通用PDF p94–95仅给蓝牙在线播放时保留连接/音频的条件。 | TTS入口/引擎配置与第三方App保活、冻结分开；V4.0通用手册另要求休眠后蓝牙在线播放时保留Wi-Fi/蓝牙/音频，图中音频Pause仅示例。同名帮助Power/Display入口不同。 | E-PALMA-11；E-PALMA-12；E-PALMA-13；E-PALMA-15；E-BXCD-06；E-BXCD-14；E-PLCD-02；E-PLCD-04 | 本机固件/入口及开关选项、保活/冻结/自启动/休眠优先级；锁屏与返回后的TTS句子、来电/闹钟/蓝牙恢复、离线包和计时结果。 |
| F-M010-08 | 电脑/手机送书后找到内容 | 同 LAN 或远程已登录 | BOOXDrop同LAN上传后看Receive；远程登录推送看Push。通用USB排错另给通知中的Charging this device via USB→File Transfer；这不是自动入库证明。 | LAN Receive与远程Push不同；通用USB排错先检查线/电脑，并在Charging this device via USB中选择File Transfer；未证明传输后自动入库。 | E-PALMA-14；E-PLCD-01 | Palma 2当前USB界面、书架扫描/打开、重名/中断/取消、卡移除及空间不足后的残留。 |
| F-M010-09 | 保留原书并迁移读书状态 | V4.1+；同账号/服务器/路径 | 通用V4.1+：长按书→Sync→Cloud Sync→Upload source file；书库设置→Backup and Restore→Local Backup and Restore生成/恢复booxc。需满足开关、版本与文件条件。 | 通用V4.1+：源文件每份≤200MB；总同步关闭时单书及手动同步不可用；同账号/服务器/目录。booxc含本地书与关联数据，两端均需V4.1+。 | E-PALMA-16；E-PALMA-03；E-PALMA-15 | Palma 2是否已装适用版本、恢复过程/失败、覆盖与重复名、跨版冲突、删除传播；第三方App数据不在该书库备份承诺内。 |
| F-M010-10 | 区分清数据与删缓存（D1） | 仅有通用 V4.0 文档证据 | 通用V4.0列Clear settings各对象；清密码默认设置不清密码。整机Reset Device另清全部用户数据；这些菜单及确认仍未在Palma 2验证。 | 通用V4.0区分清密码默认设置、书阅读数据、缓存/封面与整机重置；清阅读数据不可恢复，重置清用户数据。阅读锁仅NeoReader有效；Android11锁屏密码说明不套Palma 2。 | E-PALMA-15 | Palma 2现固件菜单/确认、完整删除/回收站、账号切换、备份是否完整和恢复成功；尚无可操作本机恢复指南。 |

## 分支与未闭环状态

| 场景 | 目前可成立的判断 | 下一项观察 |
| --- | --- | --- |
| 实体键在某阅读 app 不翻页 | 通用FAQ提示音量翻页前提；本机存在按键仍不能证明每个App事件相同，逐App映射未核。 | 指定一个第三方 app 和版本，记录 BOOX 与 app 各自设置、事件结果。 |
| 关屏后听书停止 | 通用资料把冻结/保活与睡眠后的Wi-Fi、蓝牙、音频分开；Power图的Audio Pause仅示例，不能说取消冻结必然修复。 | TTS 与音频 app 各跑锁屏、应用切换、断网、蓝牙四种中断。 |
| 换设备只有进度没有书 | 旧固件手册仅同步状态，V4.1 增加源书上传；必须先问双方版本和是否上传源文件。 | 两端文件路径、文件大小/版本、来源和同步开关。 |
| AI 快捷键已能打开聊天 | 只证明一个入口；没有任意页面语义读取或跨 app 动作成功证据。 | 打开书内选区后问 AI，它能否取得准确文本与位置、动作是否可撤销。 |

尚未核定全部功能数；账户可得的免费云空间是 ONYX 服务，流媒体/app 自己的订阅、离线授权和数据不在同一套餐内。官方下载帮助要求设备或账号获取对应手册；本轮未取得专用当前手册，没有为此注册/登录/安装应用。[E-PALMA-17]

## 观察与推断

初查只有文本，原PDF截图请求失败未计观察。本轮正常取得原PDF并实际查看4张摘页及1张型号宣传图，均是静态参考，不证明连续过程、默认值或本机操作。没有测刷新/解锁/音频耗时。

对 Plus 的推断：掌上场景可借鉴固定“继续读听”、物理返回/暂停和显式全刷入口，但需要先确定按键在当前 app、锁屏、录音与电话中的优先级。G-002 要暴露当前按键映射和前台/后台状态，否则 AI 说“继续”可能只打开 app。对 R-018 与 5–6 岁自主读听，应把书的位置与音频位置分别保存并可解释地恢复；对成人新闻，滚动/阅读模式的切换可用任务预设，但不能默认满足 R-011 的60fps/无残影目标。

## 第44批：移动通用手册的内容与后台条件

仍限E-PALMA-15的Mobile ePaper Series通用V4.0，不是Palma 2专用当前手册。新增内容只用文件页42–45、48–50、77–82、87–89；没有新增手册视觉或实机观察。逐页短摘与限制见[补读依据](../artifacts/boox-system-content-deepening/source-notes.md)。

| F编号 | 前提与操作 | 资料所述结果 | 未闭环 |
| --- | --- | --- | --- |
| F-M010-11 | Mobile ePaper通用V4.0说明；同ONYX账号且网页首次加载时联网；网页/手机推送到PushRead；打开加载后，中央菜单切Reading Mode | 手册称加载后可离线；未实际断网验证，未证明所有资源已缓存。[E-PALMA-15；E-BXSD-16] | Palma 2现固件适用、网页完整缓存/附件/失效、断网实测、取消订阅与持久进度未知 |

F05：通用手册p43规定点空白退出查词；p49–50列Annotations导出TXT至Storage/note/同名文件。这补充原通用帮助，不代表本机现在所有导出方式相同。p44–45的OCR说明针对文档图像，不当相机扫描流程；识别页可从Contents/Bookmarks跳回，清OCR的确认及对原书影响未知。[E-PALMA-15]

F07：p88自启动要求app自身支持，并在安装APK后先手动运行一次；成功后后台运行而不出现在最近任务。不能用最近任务列表推断后台工作成功。F08手机传输同Receive/Push分开，手机扫码不当本机相机OCR。F09：p81–82账号列表只保留账户信息，切换需重新登录，服务器也可能切换；这不是已经形成家庭成员隔离与恢复方案。[E-PALMA-15；E-BXSD-18]

PushRead的订阅、网页缓存和阅读位置是不同对象。后续讨论个人报纸及AI收集资料时，应逐项确认哪些内容真正离线可用、由谁读取、已读如何定义；这些是研究问题，未定新闻app或账户设计。

## 证据

## 结构化竞分（2026-10-28）

- 对象/信息架构
  - 按键（指纹电源/翻页/智能键）、系统导航与手势、显示模式（EinkWise/按app刷新/全刷）、NeoReader 内容对象（书/书签/目录/批注/选区/词典）、TTS 引擎设置、内容传输（BOOXDrop/Push/USB）、阅读数据同步与源书上传（V4.1）、清缓存/清阅读数据/恢复出厂为不同对象（F‑M010‑01/02/03/04/05/06/07/08/09/10/11）。
- 关键任务流（已证与限制，D2）
  - 单手入口与系统控制：双下拉/导航球/手势；智能键三种按法为入口（F‑M010‑01/02）。
  - 显示与阅读：EinkWise/按app模式；NeoReader 触控区、查词/标注/导出、目录/书签/Prev/Next View（F‑M010‑03/04/05/06）。
  - 读听切换与后台条件：TTS 入口与保活/冻结/休眠条件分离（F‑M010‑07）。
  - 传输与迁移：Receive/Push 与 USB File Transfer；V4.1 的源书上传与 .booxc 备份（F‑M010‑08/09）。
  - 清理与恢复：清设置/阅读数据/缓存与整机重置区分（F‑M010‑10）。
- 失败与恢复边界
  - 智能键/音量翻页需分别在系统/应用内配置；默认值与生效范围未证（F‑M010‑01）。
  - 读听/后台：冻结/保活/自启动与睡眠后的Wifi/蓝牙/音频是不同层；不承诺锁屏续听（F‑M010‑07）。
  - 迁移：V4.1 前后的能力不同；未证跨版/覆盖/失败回滚（F‑M010‑09）。
- 对 E‑Ink4 Plus 与 5–6 岁+成人家庭的启发与风险
  - 启发：把“按键/手势/显示/阅读对象/TTS/传输/备份/清理”拆分；显式“返回阅读/全刷/续读”。
  - 风险：把通用手册路径当当前机型实测；把“记视图历史”当跨重启/跨端断点；把PushRead缓存当离线保证。

旧来源初查2026-10-26；本轮复读既存官方JSON并再取原产品页/发布稿/PDF，获取方式与定位记录在来源元数据中。15仍是厂商署名、零售商托管通用手册；4页静态观察以衍生E-PLCD-04关联原手册。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-PALMA-01 | [Palma 2 产品页](https://shop.boox.com/products/palma2) | Android13、四模式、智能键/指纹、NeoReader、音频；不采相关商品规格。 |
| E-PALMA-02 | [Palma 2 发布说明](https://shop.boox.com/blogs/news/palma2-mindful-living) | 2024-10-22；实体键、原生app、相机/音频。 |
| E-PALMA-03 | [V4.1 官方增量](https://shop.boox.com/blogs/news/boox-firmware-v4-1-overview-of-new-features-and-upgrades) | 2025-08-28；源文件同步、本地备份；本机安装状态未核验。 |
| E-PALMA-04 | [Navigate Your Device](https://booxsupport.zendesk.com/hc/en-us/articles/25213043150996-Navigate-Your-Device) | 双下拉、导航球/手势；通用界面路径待本机验证。 |
| E-PALMA-05 | [EinkWise](https://booxsupport.zendesk.com/hc/en-us/articles/10701257505044-EinkWise) | 当前通用入口与机型差异。 |
| E-PALMA-06 | [Refresh Modes](https://booxsupport.zendesk.com/hc/en-us/articles/10701257029780-Refresh-Modes) | 按app设置、手动全刷；Smooth范围。 |
| E-PALMA-07 | [Set Up Page-Turn Area](https://booxsupport.zendesk.com/hc/en-us/articles/8569366359700-Set-Up-Page-Turn-Area) | NeoReader Touch Settings。 |
| E-PALMA-08 | [Translation and Dictionary](https://booxsupport.zendesk.com/hc/en-us/articles/8569333726740-Translation-and-Dictionary) | 选词、选段、生词及第三方词典条件。 |
| E-PALMA-09 | [Take Notes on Books](https://booxsupport.zendesk.com/hc/en-us/articles/8569296110100-Take-Notes-on-Books) | 本例仅采文字批注，不采手写/分屏能力。 |
| E-PALMA-10 | [TOC & Bookmarks](https://booxsupport.zendesk.com/hc/en-us/articles/10701496119060-TOC-Bookmarks) | Contents 与书签。 |
| E-PALMA-11 | [Book Progress](https://booxsupport.zendesk.com/hc/en-us/articles/10701495414804-Book-Progress) | 视图历史、跳页/章、TTS。 |
| E-PALMA-12 | [TTS 引擎设置](https://booxsupport.zendesk.com/hc/en-us/articles/360031058312-How-could-I-download-new-TTS-engine-on-BOOX-device-and-use-it-as-the-default-TTS-on-our-device) | 语速/语言包。 |
| E-PALMA-13 | [App Management Settings](https://booxsupport.zendesk.com/hc/en-us/articles/10701262170644-App-Management-Settings) | Freeze 对后台操作/通知的影响。 |
| E-PALMA-14 | [Transfer with Your Computer](https://booxsupport.zendesk.com/hc/en-us/articles/10701399613076-Transfer-with-Your-Computer) | Receive / Push 两种内容流。 |
| E-PALMA-15 | [Mobile ePaper Series 通用 V4.0 手册](https://ebookreader.dk/images/upload/Onyx%20BOOX%20Palma%202%20Pro%20User%20Manual.pdf) | PDF封面、p13–18显示、p30–36清数据/锁/同步；文件名不是适用机型证明。 |
| E-PALMA-16 | [Reading Data Syncing and Backup](https://booxsupport.zendesk.com/hc/en-us/articles/25401838386196-Reading-Data-Syncing-and-Backup) | V4.1 源书与 .booxc，迁移前提。 |
| E-PALMA-17 | [User Manual Download](https://booxsupport.zendesk.com/hc/en-us/articles/8569203926804-User-Manual-Download) | 本机或登录网页获得机型手册。 |
| E-PLCD-01 | [通用故障帮助](https://booxsupport.zendesk.com/hc/en-us/articles/8569506159124-Troubleshooting) | 旧官方JSON本地复读；只采用音量翻页和USB问答，不套Notes恢复。 |
| E-PLCD-02 | [E Ink Tablets休眠帮助](https://booxsupport.zendesk.com/hc/en-us/articles/8569254233620-Sleep-and-Battery) | 与既有E-BXCD-14的PC类Power/Display入口差异；非本机当前路径。 |
| E-PLCD-03 | [Palma 2 Smart Button图](../artifacts/palma-controls-deepening/palma2-smart-button.jpg) | E-PALMA-01产品页衍生静态证据。 |
| E-PLCD-04 | [通用手册4页观察](../artifacts/palma-controls-deepening/source-notes.md) | derived_from E-PALMA-15，PDF p1/86/95/97；非当前机型执行链。 |
