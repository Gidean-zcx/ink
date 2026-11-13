# 交付计划 v4：外壳和现有应用做到成熟可用

项目经理，2026-11-12 13:40。范围按 D-131（用户 13:1x）：第一优先是外壳 9 个部件、17 个现有应用，以及它们在真机上能用所需的平台缺口；新增功能的移植排后（第 10 节只列顺序）；要被替换的模块不再修。

依据：backlog“规划 v3”；D-127～D-131；`frameworks-research-1` 报告；第二份独立调研 `scratchpad/frameworks-agent.md`（两份结论一致）；`shell-research-1` 的半成品（DESIGN.md 已写到第 3 节，推荐主屏方案 A）；`replan-sysapps/APPS.md`、`replan-media`、`replan-reader`、`replan-arch`；在跑任务的 run.log、dev.log 末尾。**磁盘 13:32 实测只剩 29 GiB**（不是 41）：fw-size-2 已涨到 6.2 GB、graft-mogo-1 7.4 GB，15 分钟里少了 6 GB。

## 0. 结论

- 拆成 **35 个新任务**（另有 1 个热点任务看情况再开），加上在跑的 4 个开发任务。**用 3 天：11-14 晚上的试用版 22 带齐外壳和全部现有应用的第一版；11-15 留一天缓冲**，用来修用户反馈、删旧版本、开始第二优先。用户说不用排到 15 号，所以 22 是目标，23 是保底。
- 瓶颈是磁盘，不是名额。一个开发任务占 6～8 GB，留 10 GB 余量以后，**同时只能跑 5 个开发任务**，另外可以跑 1～2 个只调研或只改中枢的轻任务。所以每个任务审完就清掉编译产物，夜里不停地派。
- **今天开工、不用等用户也不会白做的**：U 盘、拆 `navigation.c`、设计语言、谷歌拼音内核、PebbleOS 通知服务、声音按原采样率输出。**今晚接着派**：键盘、状态栏和控制中心、设置框架、老黄历数据、传书、中枢语音，以及 7 个应用的重写。
- **用户只需要定 5 件事**：主屏方案、设计语言、系统应用怎么换、锁屏遮不遮、Mac 当中枢。每件都有推荐和默认做法，不答也不会停工。
- **最大的风险是应用分区**。估计新增 400～1,150 KB，可是 fw-size-2 做完以后只空出约 810 KB。要等用户说“换”，删掉旧阅读等旧版本，才放得下。

## 1. 今晚这版（试用版 20，20:00 装）

| 带什么 | 用户能试到 | 截止 | 风险 |
| --- | --- | --- | --- |
| `fixes-1112` | 切换中英文以后第一个键不再丢；计时器能按秒设；边打字边存备忘，不再提示“没存上” | 18:30 合并 | 第 4 项（4G 测试替身）卡住超过 1 小时就跳过，不影响用户 |
| `fw-size-2` | 看不见（固件小约 400 KB，给后面腾地方） | 做完就单独先合（约 15:30），不等整批 | — |
| `hub-link-1` | 设备连上 Mac 上的中枢，AI 用 DeepSeek 回答（先打字问） | 17:30 前做完才带 | 真机联调要设备和 Mac 在同一个网络，设备还要在电脑上多留 30～60 分钟；内部内存紧（空闲 114 KB，最低 78 KB），连不上就默认不连，不影响别的功能 |
| `usb-disk-1`（13:45 派） | 插上电脑出现 U 盘，可以直接拷书 | 17:30 前做完才带，否则放进 21 | 打开 U 盘可能断开电脑和设备之间的串口（不确定），上机核对要用户在场 |
| `shell-split-1`（14:30 派） | 看不见（只拆文件，行为不变；明天外壳各部件能并行改全靠它） | 18:00 前做完就带，否则只合进分支、不进镜像 | — |
| 主会话：测电话短信 | 先装自动测试版，给 10000 打一个电话、发一条短信（用户已同意），看通话有没有声音、短信有没有回执，再装正式版 20 | 20:00 | 多花约 15 分钟 |

不带：新阅读（`graft-mogo-1` 先在模拟器跑，明天带）、设计语言（今晚出预览，等用户看）。退回办法：装回试用版 19（`ink-hw/trial-20261112/delivery-19/`）。

## 2. 接下来每天的版本

| 版本 | 日期 | 用户能试到（用户的话） | 靠哪些任务 |
| --- | --- | --- | --- |
| 21 | 11-13 晚 | 整个系统换新样式（框和圆圈少了，按钮和输入框统一，没内容的地方不再占位置）；新键盘（按键一样大，候选能展开，符号分类，九宫格），能整句出字；控制中心有了开关格；设置首页分组，每行写着当前值，能搜索；电话、联系人、短信、备忘、时钟、文件、计算器换成照成熟应用重写的版本；新阅读（和旧阅读并排）；新传书网页；插电脑当 U 盘；按住 AI 键说话就能问，回答能念出来（Mac 开着时）；音乐音质变好；主屏换成你选的方案（今晚选了才赶得上） | design-lang-1、keyboard-2、ime-engine-1、control-1、settings-1、phone-3、contacts-3、sms-3、memo-3、clock-3、files-3、small-apps-3、graft-mogo-1、transfer-2、usb-disk-1、hub-real-1、audio-out-1、home-1 |
| 22 | 11-14 晚 | 新主屏（如果 21 没赶上）；通知能分组、能直接回复，勿扰可以设例外；弹窗换新；锁屏新布局，加上 Mogo 锁屏样式（票根、拼贴、月历和老黄历）；新 AI 面板；开机、换家人、搜索、电源菜单统一新样式；设置各页（声音和铃声、前光、SIM 卡和接入点、手机模式）；新版日历（重复和例外、老黄历）、天气、相册、相机、远程网页；新阅读加书架封面、统计打卡和书摘；新音乐播放页；英文打字有单词补全。**外壳和现有应用的第一版到这里全部做完** | home-1、notify-ui-1、dialogs-1、lock-1、lock-styles-1、ai-panel-1、shell-sweep-1、settings-2、calendar-3、weather-3、gallery-3、camera-3、remote-2、reader-2、music-3、ime-engine-2 |
| 23 | 11-15 晚（缓冲） | 前几天你提的问题；你说“换”的旧版本删掉（旧阅读、旧音乐），腾出空间；22 没赶上的（最可能是设置各页、真机上的远程网页、相机和相册）；第二优先开始，先做录音机 | 修正任务、remove-old-*、recorder-1 |

## 3. 各部件和应用：来源、搬法、预算、开工、进哪版（问题 1）

“搬代码”是指把原文整块拷进来、接上我们的服务（写 SOURCE.md，带着它的测试）；“照着重写”是指照它的流程和布局逐页翻成我们的 LVGL 页面。预算只算产品代码（胶水和新写的），原样拷进来的不算。所有任务都遵守：每个用户操作登记成能力，按钮和 AI 走同一个入口，有页面快照；跟系统“文字大小”一起缩放，默认和最大字号各出一套截图；任务书写明固件 KB 预算。

### 3a 外壳

| 部件 | 任务 | 成熟来源 | 搬代码 / 照着重写 | 预算（行） | 依赖 | 开工 | 进版 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| （准备）拆文件 | shell-split-1 | — | 只搬家：`navigation.c` 2,388 行拆成状态栏、主屏、控制中心、通知列表、设置正文、今日编辑六个文件，行为不变 | 新增 ≤80 | 无 | 今天 14:30 | 20（看不见） |
| 设计语言 | design-lang-1 | iOS HIG 和 Material 3 的控件规格；reMarkable、Light 的排版；BOOX、Kobo 的内容组织（shell-research-1 第 1 节，加另一路代理看的 Kindle、文石、Mudita、Light、Pebble、长辈模式） | 照着重写：只改令牌和共用控件，包括三种按钮、输入框、开关、滑杆、分段、卡片、分页、列表行和 2 px 分隔线、空状态，以及 108 px 的点按范围 | 450 | shell-research-1 | 今天 15:30 | 21 |
| 主屏、今日、全部、Dock | home-1 | 方案 A：BOOX 式网格，保留大时钟（D-106），Dock 固定；“全部”页照 KISS 做搜索；“今日”照 Pebble 简览 | 照着重写 | 500 | 拆文件；design-lang-1 分支；用户选方案（默认 A） | 今晚 22:00（20:00 前选了）或 11-13 09:00 | 21 或 22 |
| 状态栏、控制中心 | control-1 | AOSP SystemUI 快捷设置：下拉分两段，开关格写名称和当前值，有亮度条；状态图标照 MuditaOS，只显示真实状态；可搬 InfiniTime 的 2×2 快捷区（`QuickSettings.cpp:60–157`）和 Brookesia v0.4.2 的状态栏控件 | 照着重写，片段可搬 | 450 | 拆文件；design-lang-1 分支 | 今晚 20:00 | 21 |
| 通知服务 | notify-svc-1 | PebbleOS 通知服务（约 2.5k 行 C，约 15 KB）：勿扰（手动、多个时段、日历忙时）、例外（只放来电等）、提醒偏好、动作和快捷回复（默认 5 条） | **搬代码**。存储换成我们按家人分开的存储，定时换成我们的定时服务，不另建第二个通知库 | 胶水 400 | 无（两份调研一致） | 今天 17:30 | 21（看不见） |
| 通知页面 | notify-ui-1 | AOSP 的分组、展开、直接回复；MuditaOS 的三种手机模式（联网、勿扰、离线）和“离线时只收短信” | 照着重写 | 450 | notify-svc-1 合入；control-1 定好通知区接口 | 11-13 11:00 | 22 |
| 键盘界面 | keyboard-2 | 照 fcitx5-android 逐页翻：候选展开成分页网格、符号分类分页、数字键盘、长按或上滑出符号；符号页参考 Trime；九键交互和墨水屏刷新规则照 Mogo（只刷按下的那个键） | 照着重写 | 600 | fixes-1112 合入（它改过 `keyboard.c`） | 今晚 18:45 | 21 |
| 输入法引擎 | ime-engine-1 | 谷歌拼音内核（AOSP PinyinIME 的 C++ 部分，12 个文件 7,804 行，-Os 约 46 KB，两份调研一致）；九键照 lvgl_pinyin_ime 的接法（数字串转拼音串，交给同一个引擎），沿用现有的 kiikoread 九键表 | **搬代码**。词库 1,068,442 B 替换现在的 968,735 B，每位家人一个用户词库文件 | 胶水 500 | fw-size-2 合入（词库打包要改同一处） | 今天 16:15 | 21（首选命中不低于现在才换，见附录） |
| 英文补全 | ime-engine-2 | 照雾凇的做法，英文词当次候选 | 自己写 | 300 | ime-engine-1 | 11-13 18:00 | 22 |
| 对话框、确认卡、底部面板、提示条、电源菜单 | dialogs-1 | iOS HIG 的 Alerts 和 Action Sheets；M3 的 Dialogs、Bottom sheets、Snackbar（shell-research-1 第 5 节） | 照着重写 | 400 | design-lang-1 合入（之后由它接管 `theme.c`） | 11-13 10:00 | 22 |
| 锁屏布局和待机 | lock-1 | Kindle、Kobo 的锁屏；AOSP 锁屏上受限的紧急呼叫和相机入口；Mudita Kompakt 的安静锁屏（shell-research-1 第 3 节） | 照着重写 | 300 | design-lang-1；用户定“遮不遮” | 11-13 14:00 | 22 |
| 锁屏样式 | lock-styles-1 | Mogo：票根、拼贴、时钟、月历和老黄历、壁纸、迷你书库 | **搬代码**（经嫁接接口） | 胶水 500 | graft-mogo-1 合入；almanac-2 | 11-13 10:00 | 22 |
| AI 面板和 AI 页面 | ai-panel-1 | 豆包、Gemini 式的底部面板，带当前页面的上下文；打断照 Siri（shell-research-1 第 5 节） | 照着重写 | 500 | design-lang-1；dialogs-1 的底部面板 | 11-13 15:00 | 22（真机还要 hub-link-1、hub-real-1） |
| 设置框架和首页 | settings-1 | AOSP/Lineage 的结构：首页分组、每行写当前值、6 种可用状态、搜索结果带路径；注册表照 PebbleOS 的 settings 模块（约 700 行） | 结构照着重写，注册表搬代码。一份声明同时生成页面、搜索索引和能力 | 600 | 拆文件；design-lang-1 分支 | 今晚 20:00 | 21 |
| 设置各页 | settings-2 | 页面清单照 MuditaOS：手机模式、SIM 卡、PIN 和接入点、前光、铃声和振动、自动锁屏；Wi-Fi 列表照 Brookesia；家人和孩子页套新样式 | 照着重写 | 700 | settings-1 合入 | 11-13 18:00 | 22（可能 23） |
| 其余外壳页面 | shell-sweep-1 | 开机、换家人、密码键盘、全局搜索、“正在做的事”、备份更新恢复、电量页，照 iOS 和 AOSP 的对应流程 | 照着重写，主要是套新样式 | 400 | design-lang-1；dialogs-1 | 11-13 14:00 | 22 |

### 3b 现有应用

| 应用 | 任务 | 成熟来源 | 搬代码 / 照着重写 | 预算（行） | 依赖 | 开工 | 进版 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 电话 | phone-3 | Fossify Phone：输号码时就匹配联系人、最近通话、通话中的控制；匹配规则对照 Lineage Dialer；LilyPhone 的 LVGL 显示片段 | 照着重写，LilyPhone 片段可搬 | 500 | design-lang-1 分支 | 今晚 | 21 |
| 联系人 | contacts-3 | Fossify Contacts：多个号码带标签、详情页先给动作、收藏、导入 vCard | 照着重写 | 500 | 同上 | 今晚 | 21 |
| 短信 | sms-3 | Fossify Messages：在会话里直接写、草稿、失败的原消息重发、分段提示、“已发出”和“已送达”分开；状态区分对照 Lineage | 照着重写 | 500 | 同上 | 今晚 | 21 |
| 时钟 | clock-3 | Fossify Clock：多个闹钟按周重复、多个计时器按秒、秒表计次、世界时钟；分秒输入和计次照 InfiniTime | 照着重写 | 500 | 同上；fixes-1112 合入 | 今晚 | 21 |
| 计算器、小游戏 | small-apps-3 | 计算器照 Fossify Calculator（历史回填、编辑、百分比、出错后能改），保留我们的定点运算核心；五子棋本来就是 CrossMux 原版，贪吃蛇和翻牌已移植，三个游戏只套新样式 | 计算器照着重写，游戏只套样式 | 400 | 同上 | 今晚 | 21 |
| 备忘 | memo-3 | Fossify Notes：随手写、清单、自动保存、搜索；待办对象参考 Joplin；录音控件用 ZSWatch 的 | 照着重写，录音控件可搬 | 600 | 同上；fixes-1112 合入 | 今晚 | 21 |
| 文件 | files-3 | Amaze 和 Fossify File Manager：搜索、多选、选目的地、结果和失败列表；文件类型和上下文菜单用 Flipper 的 C 代码 | 照着重写，Flipper 片段可搬 | 600 | 同上 | 今晚 | 21（可能 22） |
| 日历 | calendar-3 | Fossify Calendar 和 Etar：议程、多个提醒、重复和例外、“只改这一次”；农历用我们自己的；宜忌用 almanac-2 | 照着重写 | 700 | almanac-2；design-lang-1 | 11-13 11:00 | 22 |
| 天气 | weather-3 | 层级照 Breezy（当前、逐小时、降雨、预警）；“数据是多久以前的”照 Bangle；预报布局照 ZSWatch；中枢已有 Open-Meteo 真实数据源 | 照着重写 | 400 | hub-real-1；design-lang-1 | 11-13 14:00 | 22 |
| 相册 | gallery-3 | Aves（找图、看单张、看信息）；Fossify Gallery（缩放、方向、回收站）；AgentUI 图库的 C++ 代码 | 照着重写，AgentUI 片段可搬 | 500 | design-lang-1 | 11-13 14:00 | 22 |
| 相机 | camera-3 | Fossify Camera（拍前设置、拍后查看、最近照片）；AgentUI 相机的 C++ 代码；保留锁屏相机和扫码 | 照着重写 | 400 | design-lang-1 | 11-13 14:00 | 22 |
| 音乐（声音输出） | audio-out-1 | 照 MyPaperS31 的 S31 声音输出，按原采样率直接送出（不再转成 16 kHz 单声道） | **搬代码** | 250 | 无 | 今天 18:00 | 21 |
| 音乐（播放器） | music-3 | kiikoread 改版播放器：曲库、收藏、四种循环、标签、歌词、续播 | **搬代码**（经嫁接接口），做好后直接换掉旧音乐（D-141） | 胶水 600 | 嫁接接口；audio-out-1 | 11-13 11:00 | 22 |
| 阅读 | graft-mogo-1（在跑）、reader-2 | Mogo（同一台 S31）整块搬：书库、阅读页、目录书签、设置、TXT 和 GBK、EPUB；补全书架封面分类、最近、统计打卡、手账、书摘、“继续读”卡片 | **搬代码**（嫁接），和旧阅读并存 | 第一版在做；补全胶水 800 | graft-mogo-1 合入 | 11-13 10:00 | 第一版 21，补全 22 |
| 传书 | transfer-2 | kiikoread 改版：中文网页管理、上传前生成 TXT 目录、批量操作，入口有 Wi-Fi、热点、U 盘；原厂版用户排在最后，原位替换 | **搬代码** | 胶水 500，网页压缩进资源包 | hub-link-1 合入（热点文件） | 今晚 21:30 | 21（热点入口看 hotspot-1） |
| 远程网页 | remote-2 | 保留我们的设备端（灰阶、区域刷新、家人和密码的边界）；地址、历史、下载的交互照 Browservice，连接、输入、断线恢复照 noVNC | 照着补 | 设备 400；中枢浏览器节点另算 | hub-link-1；中枢浏览器节点（`hub/browser_node` 只有 README，写着“第 6 步实现”，还没有） | 11-13 15:00 | 22（设备端）；真机要等中枢浏览器节点，可能 23 |
| AI 页面 | ai-panel-1 | 见 3a | | | | | |

### 3c 平台缺口

| 缺口 | 任务 | 来源 | 预算（行） | 依赖 | 开工 | 进版 |
| --- | --- | --- | --- | --- | --- | --- |
| 设备连中枢 | hub-link-1（在跑） | ESP-IDF 自带的 esp_websocket_client 和 esp-tls | 设备 500，中枢 600 | — | 在跑 | 20（17:30 前做完）或 21 |
| U 盘 | usb-disk-1（任务书已有） | 原厂 TinyUSB MSC | 250 | 无 | 13:45 | 20 或 21 |
| 配网热点 | hub-link-1 第 2 项；缺口大就单开 hotspot-1 | 原厂 v2.0.77 的热点 | ≤150 或 300 | 看 hub-link-1 报告 | 要单开就今晚第一个派 | 21 |
| 空间 | fw-size-2（在跑） | — | 150 | — | 在跑 | 20 |
| 屏幕一直在刷时存不上 | fixes-1112 第 3 项（在跑） | — | — | — | 在跑 | 20 |
| 声音按原采样率 | audio-out-1 | MyPaperS31 | 250 | 无 | 18:00 | 21 |
| 中枢语音和天气 | hub-real-1（只改中枢，轻任务） | 本机跑 Whisper（D-130）；朗读用 Mac 自带的中文语音；天气用 Open-Meteo（中枢已有） | 中枢 600 | hub-link-1 合入；下载识别模型要用户点头 | 今晚 18:45 | 21（中枢跑在 Mac 上，不进固件） |
| 老黄历数据 | almanac-2 | Mogo 用的 lunar-javascript 宜忌规则生成离线表，用 cnlunar 对照 | 250 加数据表 | 无 | 今晚 20:30 | 21（看不见，给锁屏和日历用） |

## 4. 派发顺序：7 个名额和磁盘（问题 3）

**磁盘规矩**：每次派之前用 `df -h /System/Volumes/Data` 看一眼，始终留 10 GB；同时最多 5 个开发任务，加 1～2 个轻任务。任务一审完就跑 `clean-done.sh` 清编译产物（每个约腾 5 GB），不等合并。上午停掉的 7 个任务（txt-gbk、music-pcm、timer-seconds、cell-double-1、ime-switch-key、memo-save-busy、almanac-1）都核过了，只有计划提交，`clean-done.sh` 本来就支持“放弃的任务”，现在就跑，约腾 2～3 GB。要是把这 7 个克隆整个删掉，还能再腾约 6 GB；它们算不上历史资料，由主会话定，拿不准就问用户一句。删了以后可以同时跑 6 个开发任务。

### 今天下午（13:45–18:30）

| 时间 | 空出的名额 | 派什么 | 主会话同时做 |
| --- | --- | --- | --- |
| 13:45 | 2 个空名额，但磁盘只够 1 个 | usb-disk-1 | 对上午停掉的 7 个任务跑 clean-done.sh |
| 约 14:30 | fw-size-2 做完 | shell-split-1 | 审 fw-size-2，清掉编译产物，单独先合（门槛约 50 分钟） |
| 约 15:30 | shell-research-1 做完 | design-lang-1（派前确认磁盘 ≥17 GB） | 审 shell-research-1，合两份调研文档；发“主屏三方案 + 锁屏遮不遮”选择页 |
| 约 16:15 | fixes-1112 做完 | ime-engine-1（基于已合入的 fw-size-2） | 审完清掉编译产物；16:00–18:30 写今晚要用的 14 份任务书 |
| 约 17:30 | hub-link-1 做完 | notify-svc-1；热点缺口大的话，写 hotspot-1 的任务书 | 审完清掉编译产物 |
| 约 18:00 | usb-disk-1、shell-split-1 做完 | audio-out-1 | 审完清掉编译产物 |
| 18:30 | — | — | 合并截止，按顺序合：fw-size-2（已合）、fixes-1112、hub-link-1、usb-disk-1、shell-split-1；过门槛；19:30 出镜像和说明 |

### 今晚到明早（按先后顺序排队，名额一空就派；时间是估计）

还在跑的：graft-mogo-1（约 22:00，不确定）、design-lang-1（约 20:00，做完出预览）、ime-engine-1（约 21:30）、notify-svc-1（约 22:30）、audio-out-1（约 22:30）。

| 顺序 | 任务 | 最早 | 基于哪个分支 | 预计完成 |
| --- | --- | --- | --- | --- |
| 0 | hotspot-1（只有 hub-link-1 说缺口大才派） | 18:45 | 主线 | 23:00 |
| 1 | keyboard-2 | 18:45 | 主线（含 fixes-1112） | 00:30 |
| 2 | hub-real-1（轻任务） | 18:45 | 主线（含 hub-link-1） | 22:00 |
| 3 | control-1 | 20:00 | design-lang-1 分支 | 01:00 |
| 4 | settings-1 | 20:00 | 同上 | 02:00 |
| 5 | almanac-2 | 20:30 | 主线 | 23:30 |
| 6 | phone-3 | 21:00 | design-lang-1 分支 | 02:30 |
| 7 | sms-3 | 21:30 | 同上 | 03:00 |
| 8 | transfer-2 | 21:30 | 主线（含 hub-link-1） | 03:00 |
| 9 | home-1（用户 20:00 前选了方案才派，没选就 11-13 09:00 按 A 派） | 22:00 | design-lang-1 分支 | 03:30 |
| 10 | contacts-3 | 23:00 | 同上 | 04:00 |
| 11 | memo-3 | 00:30 | 同上 | 05:30 |
| 12 | clock-3 | 01:00 | 同上 | 06:00 |
| 13 | files-3 | 02:00 | 同上 | 07:30 |
| 14 | small-apps-3 | 02:30 | 同上 | 07:30 |

表里和第 3 节说的“design-lang-1 分支”，是指 18:30 合并后的主线（已含 shell-split-1、fixes-1112）再合上 design-lang-1 做完的分支，得到一个临时分支，只用来起步。在它上面开工的任务，要等 design-lang-1 正式合入以后才能合。

### 11-13 白天（截止 18:30，出试用版 21）

| 时间 | 做什么 |
| --- | --- |
| 07:30–10:00 | 审夜里做完的约 12 个；09:00 发预览页：设计语言、键盘、控制中心、设置首页、主屏、7 个应用、传书网页，每项都有“可以”“再改”两个按钮 |
| 10:00 | 合 design-lang-1（用户没答就按推荐暂定），再合 keyboard-2、ime-engine-1、notify-svc-1、audio-out-1、transfer-2、almanac-2、graft-mogo-1；派 dialogs-1、reader-2、lock-styles-1，home-1 夜里没派的话也在这时派 |
| 11:00 | 派 music-3、notify-ui-1、calendar-3 |
| 12:00–15:00 | 合夜里的应用重写（预览点了头的，或者按第 5 节第 3 项默认换的）；派 lock-1、shell-sweep-1、weather-3、gallery-3、camera-3；dialogs-1 合入后派 ai-panel-1、remote-2 |
| 18:00 | settings-1 合入后派 settings-2；派 ime-engine-2 |
| 18:30 | 合并截止，出试用版 21 |

### 11-13 夜到 11-14

把剩下的跑完，加上预览和真机反馈带来的修正。11-14 上午审、合；用户说“换”的旧版本由 remove-old-* 删掉；18:30 截止，出试用版 22。11-15 是缓冲：出试用版 23，开始第二优先。

**产能核算**：11-12 13:45 到 11-14 18:30 共约 53 小时。5 个开发名额，每个任务连同审查约 4.5 小时，能跑约 55 个任务。实际要跑 35 个新任务，加约 30% 的第二轮修正（最近一批里多数任务交付时全量都没全过），约 46 个，余量约 15%。余量偏紧，所以磁盘能腾出来就开到 6 个名额；超过半天的修正推到 23。主会话 11-11 夜里一晚合过约 25 个任务，审查跟得上。前提是 11-13 要用的任务书，今晚就写好。

## 5. 要用户定的（问题 2）

| # | 要定的 | 选项 | 推荐 | 最晚 | 不答会怎样 |
| --- | --- | --- | --- | --- | --- |
| 1 | 主屏方案（草图今天约 15:30 出） | A 三屏网格，保留大时钟（BOOX 式）；B 一屏常用列表（Light、Mudita 式）；C 阅读优先（Kindle、Kobo 式） | A：和你以前定的“三屏加大时钟”（D-60、D-106）一致，老人孩子靠位置就能找到 | 最好今晚 20:00 装机时选，夜里就能开做；最晚 11-13 09:00 | 按 A 做：结构不变，只换样子 |
| 2 | 新设计语言（改前改后对比页，今晚约 20:30 出） | 可以 / 指出哪里再改 | 可以 | 11-13 10:00 | 按推荐先合，后面的新页面都用它；以后要调，全系统一起变 |
| 3 | 重写的系统应用怎么换 | A 预览点了头就原地替换，要退回就装上一版固件；B 新旧两版并存 | A。电话、短信两版并存会抢来电和通知，应用分区也放不下两份。阅读、音乐、锁屏样式照旧并存，等你说换 | 11-13 12:00（第一批重写合入前） | 照原来的规矩并存。电话、短信这类没法并存的，要等你看完预览才换，就进不了 21 |
| 4 | 锁屏时要不要遮住大人的页面（D-38 现在是保留上一页） | 遮住，只显示时钟和通知数 / 保留上一页 | 遮住：别人拿起设备，看不到你的短信和书 | 11-13 14:00 | 保留现状（D-38） |
| 5 | 让 Mac 当家庭中枢 | ① 试 AI、天气、远程网页时 Mac 要开着、中枢在跑，设备和 Mac 连同一个网络；② 往这台 Mac 下载语音识别模型（Whisper，约 0.5～1.6 GB，看选哪一档，不确定）；③ 朗读用 Mac 自带的中文语音（免费），还是火山方舟（付费）；④ 远程网页用 Mac 上已装的 Chrome，还是下载约 150 MB 的无头浏览器 | 都同意；朗读先用 Mac 自带的；远程网页先用已装的 Chrome | ① 今晚 19:00（联调前）；② ③ 11-13 15:00；④ 11-13 18:00 | 今晚不联调，AI 只在模拟器里能用；说话问 AI 进不了 21；远程网页在真机上推到 23 |

**已经定了、不再问的**：今晚给 10000 打电话、发短信（用户已同意）；通知照 AOSP 做分组和直接回复，设置照 AOSP 做搜索（两份调研一致，在预览里一起看）；输入法引擎用谷歌拼音（两份调研一致，过不了对比就不换）；中枢模型用 DeepSeek（D-130，只在用户自己用 AI 时调用，自动测试不调用）。

**预览什么时候出**：主屏三方案和锁屏遮不遮，今天约 15:30；设计语言，今晚约 20:30；键盘、谷歌拼音的对比数字、控制中心、设置首页、主屏、7 个应用、传书网页，11-13 09:00；对话框、阅读补全、锁屏样式、新音乐、通知、日历，11-13 约 17:00；AI 面板、锁屏、其余外壳页面、天气、相册、相机、远程网页、设置各页，11-14 08:00。每批最晚在当天 15:00 前答复，才赶得上当晚的版本。

## 6. 文件冲突怎么避开（问题 4）

规矩：

1. 一组文件同一时间只归一个任务。按下表交接，交接点就是上一个任务合入的时候。
2. **构建登记文件可以例外**：根目录 `CMakeLists.txt`、`ports/esp32s31/main/CMakeLists.txt`、`sdkconfig.defaults` 只许在末尾追加自己那几行（五子棋和 EPUB 当初就是这样并行做的），按完成的先后合，冲突由主会话花几分钟解决。今天 fw-size-2、hub-link-1、usb-disk-1 三个任务都会动这几个文件。
3. **共用的生成文件，任务都不改**：英文词表 `assets/i18n/en.tsv`（任务把要加的词交到 `docs/tasks/<任务>/I18N-ADD.tsv`，由主会话合）；`docs/system/capabilities.md` 合并时重新生成；`ui-dependencies.md` 和 backlog 由主会话改。
4. **主题令牌会影响所有页面，这样排**：
   - design-lang-1 独占 `theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c`，**只改令牌和共用控件，一个页面文件都不碰**，所以不会和页面任务改到同一个文件。
   - 页面任务只用令牌和共用控件，不在页面里写死像素和颜色。这样设计语言以后再调数值，页面自动跟着变，不用返工。这也是夜里那批任务敢在用户看预览之前开工、不会白做的前提。
   - design-lang-1 最先合（11-13 10:00）。夜里开工的界面任务都从它的分支起步，合并排在它后面，只要重跑测试。
   - 改令牌不会牵动一大片测试文件：测试截图不入库（`tests/` 里只有扫码测试的 11 张图）；按坐标点按的脚本只有 4 个（`tests/` 下 3 个，加上设备自动测试的 `memo.script`）。布局断言变了，由 design-lang-1 自己改。`memo.script` 里按的是键盘按键，归 keyboard-2 改。
   - design-lang-1 合入以后，11-13 当天 `theme.c` 和 `ui_theme.h` 归 dialogs-1（加底部面板、提示条）。别的任务要新的共用控件，写进报告，排给下一个负责的任务；新控件尽量放新文件。
   - 键盘令牌 `INK_KB_*`：keyboard-2 不碰 `ui_theme.h`，新令牌放进键盘自己的头文件；旧的那块以后由设计语言的后续任务删掉。
   - 状态栏高度 62→108（shell-research-1 的建议）会让每一页少 46 px，design-lang-1 要逐页核对放不放得下，这一项放进预览给用户看。
5. **`navigation.c`**：今天只归 shell-split-1。graft-mogo-1 如果在里面加了“新阅读”入口，就排在拆分之后合，主会话把那一两行挪到新文件里。

文件归属表：

| 文件 | 今天下午 | 今晚到 11-13 上午 | 11-13 白天起 |
| --- | --- | --- | --- |
| `system/ui/shell/navigation.c` | shell-split-1 | 拆完只剩导航能力和初始化，不派任务 | 不派 |
| 拆出来的 `status_bar.c`、`control_center.c` | — | control-1 | control-1 |
| 拆出来的 `shell_home.c`、`today_edit.c` | — | home-1（选了方案才派） | home-1 |
| 拆出来的 `notification_list.c` | — | — | notify-ui-1 |
| 拆出来的 `settings_body.c`，加上 `settings_pages.c` 和新的 `settings_registry.c` | — | settings-1 | settings-1，之后 settings-2 |
| `settings_family.c`、`time_settings.c`、`bluetooth_page.c`、`mobile_network.c` | — | — | settings-2（11-13 18:00 起） |
| `theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c` | design-lang-1 | design-lang-1 | dialogs-1 |
| `confirm_card.c`、`card.c`、`interrupt_card.c`、`power_menu.c`、`airplane_card.c`、`update_card.c` | — | — | dialogs-1 |
| `ai_panel.c`、`ai_chat.*`、`ai_pages.*` | — | — | ai-panel-1 |
| `lock_mark.c`、`off_picture.c` | — | — | lock-1 |
| `system/ui/lock_faces/*`、`wallpaper.c`、新的 `third_party/mogo-lock/` | — | — | lock-styles-1 |
| `boot_flow.c`、`system/ui/search.c`、`member_*.c`、`pinpad.*`、`kid_pages.c`、`factory_pages.c`、`backup_pages.c`、`update_pages.c`、`power_pages.c`、`activity_card.*` | — | — | shell-sweep-1 |
| `keyboard.c`、`tools/device/autotest/memo.script` | fixes-1112 | keyboard-2 | keyboard-2 |
| `system/services/text/*`、`tools/imedict`、`tests/ime_lexicon`、`asset_pack.py` 里词库那几行 | fw-size-2，之后 ime-engine-1 | ime-engine-1 | ime-engine-2 |
| `system/services/notify/*` | notify-svc-1 | notify-svc-1 | notify-ui-1（只加接口） |
| `system/services/audio/*`、S31 声音 HAL | audio-out-1 | audio-out-1 | music-3 |
| `system/apps/phone`、`system/services/comm/comm_service.c` | — | phone-3（`comm_service.c` 只归它） | — |
| `system/apps/sms` | — | sms-3（不改 `comm_service.c`，要改就写进报告） | — |
| `system/apps/contacts` | — | contacts-3 | — |
| `system/apps/` 下的 memo、clock、files、calculator、gomoku、snake、memory | fixes-1112（clock） | memo-3、clock-3、files-3、small-apps-3 | — |
| `system/apps/` 下的 calendar、weather、gallery、camera、remote | — | — | calendar-3、weather-3、gallery-3、camera-3、remote-2 |
| `system/services/time/almanac*`（新） | — | almanac-2 | calendar-3、lock-styles-1 只读着用 |
| `system/graft/*`、`third_party/mogo/`、`apps/` 下的新阅读 | graft-mogo-1 | graft-mogo-1 | reader-2（嫁接接口只许它改；lock-styles-1、music-3 发现缺口就写进报告） |
| `transfer_pages.c`、传书服务、S31 上的传书代码 | — | transfer-2 | — |
| `ports/esp32s31/hal_ws_s31.c`、热点 | hub-link-1 | hotspot-1（要单开时） | — |
| `ports/esp32s31/hal_usb_s31.c` | usb-disk-1 | — | — |
| `hub/ink_hub/models` | hub-link-1 | — | — |
| `hub/ink_hub/speech`、`content/weather.py`、README 里语音那一节 | — | hub-real-1 | remote-2 只改 `hub/browser_node` |

## 7. 关键路径和风险

**关键路径**：design-lang-1 今晚出预览，用户在 11-13 10:00 前答（不答就按推荐暂定），合入后外壳各部件和应用重写跟着合，22 收齐。另一条是 graft-mogo-1，合入后接 reader-2、lock-styles-1、music-3。

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| 应用分区放不下。现在空 412 KB（试用版 19 是 4,830,560 / 5,242,880 B），fw-size-2 再腾 ≥400 KB，合起来约 810 KB。新增估计（都不确定）：TLS 40～120 KB；TinyUSB 20～40 KB；新阅读 150～350 KB；谷歌拼音代码 50～70 KB，加上词库多出约 100 KB（压缩后也许少一半）；PebbleOS 通知 15～30 KB；传书网页 50～150 KB；新音乐、锁屏样式、老黄历表 80～200 KB；合计约 400～1,150 KB | 很可能在 11-14 前装不下 | 每份任务书写 KB 预算，合并门槛看固件大小；旧传书原地替换；用户在 21 上试过新阅读后请他说“换”，删旧阅读和原厂 EPUB 内核；分区表不能改 |
| graft-mogo-1 是大任务，可能拖到 11-13 中午 | 阅读补全、锁屏样式、新音乐顺延半天 | 早上先审它；接口没问题、阅读还不完整的话，先合嫁接接口 |
| 磁盘：29 GiB，15 分钟就少了 6 GB | 同时只能跑 5 个开发任务；磁盘写满过两次 | 每次派之前看 df；审完马上清编译产物；处理上午那 7 个克隆 |
| 设备每晚只能验一次 | 真机才有的问题要晚一天才发现 | 盯紧几处：TLS 吃内部内存（空闲 114 KB，最低 78 KB，缓冲要放 PSRAM）；U 盘可能断开串口，上机要用户在场；设备自动测试脚本是按坐标点的，界面一改，脚本得跟着改 |
| 谷歌拼音只用单字词频，整句可能不如预期 | 打字可能比现在差 | 90 条测试首选命中不低于现在的 83 条、全拼 71/73、简拼 12/16 才换；达不到就保留旧引擎可以切换，22 再定 |
| AI、天气、远程网页都靠 Mac 上的中枢 | Mac 关着，这些就都不能用 | 写进每晚的说明；中枢浏览器节点还没有，真机上的远程网页可能到 23 |
| 主会话负荷：每天要审 15～20 个任务、过 2～3 次门槛，总共要写约 35 份任务书 | 审不过来，名额就空着 | 今晚写好 11-13 的任务书；任务书大量引用调研文档的章节（见附录），不重写背景 |
| 第二轮修正多：最近一批里多数任务交付时全量没全过 | 每个多 2～4 小时 | 计划只留了约 15% 余量；磁盘腾出来就开到 6 个名额；超过半天的就推到 23 |
| 用户来不及看预览 | 合不进去 | 一天一页，每项一个按钮；最晚时间前不答就按推荐（第 5 节第 3 项定了以后，默认是“换”） |

## 8. 砍掉或推迟了什么

- **停掉，因为模块要被替换**（用户说“要换的就先别做了”）：`reader-images-1`、`reader-typeset-2`（都在旧阅读上，插图、排版、主题由 Mogo 带来）；`music-pcm`（旧播放器，平台那部分改成 audio-out-1）；`txt-gbk`（Mogo 自带 GBK）；`almanac-1`（换成 almanac-2，用 Mogo 的数据，同时供锁屏和日历）；旧键盘、旧计时器不再追加修补（fixes-1112 已做完的两项随今晚这版带上）；旧锁屏表盘的自动走时缺口（lock-faces-2 遗留）不再修，由 Mogo 锁屏样式取代。
- **旧模块上的已知问题，并进对应的替换任务当验收项**：拨号页提示文字压住键盘（phone-3）；计算器在 120% 字号下算式区挤（small-apps-3）；时钟、相机、相册在“全部”页没有自己的图标（home-1）；“继续读”卡片的百分比和插拔卡不刷新（reader-2）；点“关闭飞行模式”后面板不自动收起（control-1）；音乐每次刷新都重复绑定按钮（music-3）；只有一页的列表也画“1/1”（design-lang-1）；时间设置页整页重画，以及两个日期时间选择器没统一（settings-2 和 calendar-3 共用一个）。
- **推迟到第二优先**（D-131，11-15 起）：见第 10 节。
- **没有砍任何现有功能**。英文补全只是排在 22，没有取消。手写不是现有功能，只列为可选项，等用户开口。

## 9. 和上一版计划（v3，今天下午）比改了什么

| 项目 | v3 | 现在 |
| --- | --- | --- |
| 外壳 | 11-13 出预览，11-14 起每天改一块，要 6～7 天 | 今天拆文件，今晚出设计语言预览，11-12 夜里起各部件并行改，11-14 全部进版 |
| 键盘和输入法 | 11-15 | 键盘界面今晚开工，谷歌拼音今天开工，都进 21 |
| U 盘 | 11-15 | 今天 13:45 开工，争取今晚 20 |
| 设备上的 AI | 11-14 | 今晚联调打字问，11-13 能说话问 |
| 通知和控制中心、设置、音乐、电话短信联系人、时钟、日历、天气、相册相机、备忘文件 | 11-15 以后逐个做 | 11-12 夜到 11-13 全部开工，21、22 进版 |
| 锁屏样式 | 11-15 | 11-13 开工，22 进版 |
| 微信读书 | 在应用顺序里 | 排到第二优先（D-131） |
| 老黄历 | almanac-1（cnlunar） | almanac-2（Mogo 的 lunar-javascript 数据，cnlunar 对照） |
| 新旧并存 | 全部并存 | 建议阅读、音乐、锁屏样式并存，系统应用原地替换（要用户定） |
| 产能 | 没算磁盘 | 按 29 GiB 磁盘算：同时 5 个开发任务，夜里不停 |

## 10. 第二优先（外壳和现有应用做完以后，11-15 起，按这个顺序）

1. 录音机（`recorder-1` 任务书已有：AI 键一键录、锁屏也能录）
2. 微信读书（要用户扫码登录；照 CrossMux 的离线缓存，ThingOS 作体验标杆）
3. OPDS 书源、坚果云网盘（都经中枢）
4. 有声书章节（中枢把 M4B 转成 Opus 加章节）
5. 网络电台（AgentUI radio）
6. 蓝牙耳机自动重连、蓝牙键盘
7. 应用市场（参考墨集、BoBoOS）
8. 儿童内容（没有完整来源，先找）
9. Game Boy（retro-go/gnuboy；游戏卡带用户自己提供）
10. 语音遥控电脑上的 AI
11. 手写（可选：先拿 Zinnia 加简体模型在电脑上做样本对比，用户要了再做）

原厂对比里还在等用户定的几项（写 ota_0 的联网升级、当蓝牙音箱等），不在这份计划里。

## 附：每个新任务的任务书要点（给主会话）

每份任务书都照 codex-rules 的 G1～G5 写，另外都要有这几条：搬进来的代码写 SOURCE.md、带上它自己的测试；每个用户操作都登记成能力、有页面快照；用 `ink_ui_px` 缩放，默认和最大字号各出一套截图；写固件 KB 预算；不碰设备；不改上面归属表里不属于自己的文件。

| 任务 | 依据 | 只改 | 验收要点 |
| --- | --- | --- | --- |
| shell-split-1 | `navigation.c` 的函数分布：状态栏约 312–438 行、主屏 439–975、控制中心 976–1154、设置正文 1155–1806、能力 1808–2039、设置定义 2040–2085、今日编辑 2086–2360 | `system/ui/shell/` 的新文件、CMake 那几行 | 全量 ctest、ASan、S31 构建都过；ui-scene 截图逐像素一致；能力表不变 |
| design-lang-1 | shell-research-1 的 DESIGN.md 第 1 节（令牌对照表），加另一路代理的结论 | `theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c`，以及受影响的布局断言 | 每一页默认和最大字号的改前改后图（先看干净的整页图）；点按范围 ≥108 px；全量过；不改页面文件；逐页核对状态栏加高 46 px 后放不放得下 |
| keyboard-2 | frameworks-research-1 IME.md 的键盘界面表；frameworks-agent 第 1.2 节；fcitx5-android 固定提交里的 `GridPagingCandidateViewAdapter`、`PickerTabsUi`、`NumberKeyboard`、`PopupKeyboardUi`；Mogo 的 `ui_keyboard.c`（刷新规则） | `keyboard.c` 和新的键盘文件、键盘测试、`memo.script` | 按键一样大；候选能展开分页；有符号分类页和九宫格；按键和选词照旧只走 `keyboard.key` 和 `keyboard.pick` |
| ime-engine-1 | frameworks-agent 第 1.3 节（12 个文件，-Os 45,811 B）；frameworks-research-1 的 EVIDENCE（64 位机上读词库失败的原因）；lvgl_pinyin_ime 的九键接法（只当接线参考） | `system/services/text/`、新的 `vendor/googlepinyin`、`tools/imedict`、词库打包那几行、输入法测试 | 90 条测试首选 ≥83、全拼 ≥71/73、简拼 ≥12/16；九键能出整句；每位家人学的词互相隔开；密码框不学；写清 S31 增量和 PSRAM 峰值；不达标就保留旧引擎，可以切换 |
| notify-svc-1 | frameworks-agent 第 2 节 [B1]：`do_not_disturb.c`、`alerts_preferences.c`、`notification_storage.c`（只取逻辑）、`timeline_actions.c`（快捷回复） | `system/services/notify/`、新的 `vendor/pebble` | 勿扰三种来源；跨日时段；例外（来电、闹钟、重复来电）；动作和快捷回复走能力；按家人隔开；不建第二个通知库 |
| audio-out-1 | replan-media 声音一节；`ink-hw/upstream/oss/MyPaperS31` 的 I2S 输出；music-1 报告里转成 16 kHz 的问题 | 声音服务、S31 声音 HAL | 44.1 kHz 立体声直接送出；切歌正常；蓝牙耳机和录音不受影响 |
| usb-disk-1 | 已有任务书 `docs/tasks/usb-disk-1.md` | 照任务书 | 加一条：上机核对要用户在场 |
| hub-real-1 | D-130；`hub/ink_hub/speech/__init__.py` 的接口；`content/weather.py`（Open-Meteo 已有） | `hub/` | 本机 Whisper 的模型路径可配，测试用假模型；朗读用 Mac 的 `say`，转成 Opus；pytest 过；写清真机联调步骤 |
| almanac-2 | replan-media 锁屏分项（Mogo 的生成器调用 lunar-javascript 的宜忌规则）；`ink-hw/upstream/oss/lunar-javascript`、`cnlunar` | 新的 `system/services/time/almanac*`、资源包里的表 | 抽 50 天和 lunar-javascript 的结果一致；写清表有多大；提供 `time.almanac` 能力 |
| control-1 | frameworks-research-1 SHELL.md；frameworks-agent 第 2.2 节（下拉分两段，开关只刷自己那一格）；InfiniTime `QuickSettings.cpp:60–157` | `status_bar.c`、`control_center.c` | 开关格写名称和当前值；“连接中”“失败”“受家人限制”各有不同反馈；关飞行模式后面板自己收起 |
| settings-1 | SETTINGS.md；frameworks-agent 第 3 节（`BasePreferenceController` 的 6 种状态和搜索索引；Pebble settings 的 `menu.c` 注册表，693 行） | `settings_body.c`、`settings_pages.c`、新的 `settings_registry.c` | 一份声明同时生成页面、搜索和能力；搜索结果带路径和当前值；孩子看不到大人专用的设置值 |
| phone-3、contacts-3、sms-3 | replan-sysapps APPS.md 的 A01、A03、A02 节；CURRENT.md；SOURCES.md 里的固定提交（只读克隆到 `work/`） | 各自的应用目录；`comm_service.c` 只归 phone-3 | 照 APPS.md 每节的“参照路径”走一遍；拨出受理以后，没接通前不能显示成已接通；“结果不确定”时不能自动重发 |
| clock-3、small-apps-3、memo-3、files-3 | APPS.md 的 A04、A05、A09、A10 节 | 各自的应用目录 | 照各节的“参照路径”，例如计时 1 分 05 秒、`200+10%`、保存失败时字还在、重名冲突 |
| transfer-2 | replan-reader 的传书表；`ink-hw/upstream/oss/kiikoread-xiugai` 的网页管理和目录生成 | 传书服务、`transfer_pages.c`、S31 上的传书代码（替换原厂的 `ap_transfer`） | 网页资源压缩进资源包；三次错误、过期、按家人分开、满盘这些已有测试照样过 |
| home-1 | shell-research-1 DESIGN.md 第 2 节（方案 A 的坐标表） | `shell_home.c`、`today_edit.c` | 没有内容的区块不画；Dock 固定；换成孩子后只出现孩子能用的东西 |
| dialogs-1 | shell-research-1 第 5 节 | 见归属表 | 每个任务区只有一个主要按钮；不可逆的动作写清动词（例如“删除这份备忘”）；遵守 D-105 |
| lock-1 | shell-research-1 第 3 节 | `lock_mark.c`、`off_picture.c` | 没有通知时整片留白；紧急呼叫不用解锁（D-119）；锁屏相机只能拍、只能看这次拍的（D-121） |
| lock-styles-1 | replan-media 锁屏一节；Mogo 源码（S01） | 见归属表 | 分钟没变不刷新，只刷那一块；跨日自己刷新；和旧表盘并存 |
| reader-2 | graft-mogo-1 报告里“缺什么”那一节；Mogo 的书架、统计、手账、书摘 | 见归属表 | 旧阅读的功能逐项都有对应，才请用户说“换” |
| music-3 | replan-media 声音一节（kiikoread 改版，S06） | 见归属表 | 换掉旧音乐（D-141），数据自动迁移；用 audio-out-1 的输出 |
| notify-ui-1 | SHELL.md；frameworks-agent 第 2.2 节；MuditaOS 的 `PhoneModesWindow` | 见归属表 | 新通知来时，手指下面那条不会被换成别的；清掉摘要不会删掉短信原文 |
| ai-panel-1 | shell-research-1 第 5 节 | 见归属表 | 带当前页面的上下文；能打断；凭据框 AI 不能代输 |
| shell-sweep-1 | shell-research-1 第 4 节 | 见归属表 | 每页都套上新样式；首次开机的“先用起来”保留 |
| settings-2 | SETTINGS.md 的手机功能表；MuditaOS 清单 [C2]；Brookesia 的 Wi-Fi 页 | 见归属表 | 演示一遍：搜“暖光”、勿扰还放什么进来、选铃声并试听、两张 SIM 卡各自的状态 |
| calendar-3、weather-3、gallery-3、camera-3、remote-2 | APPS.md 的 A06、A07、A08、A13、A14 节 | 各自的应用目录；remote-2 另外改 `hub/browser_node` | 照各节的“参照路径” |
| ime-engine-2 | frameworks-agent 第 1.3 节（英文） | 文本服务 | 中英混输时英文词作次候选 |
| hotspot-1 | hub-link-1 报告第 2 项 | 热点 HAL（不碰 `boot_flow.c`） | 首次设置里“用手机设置”这一步能走通 |
