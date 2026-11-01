# M-010 BOOX Palma 2：单手入口、按应用显示与读听返回

## 范围

查阅：2026-10-26。对象仅为 **Palma 2 黑白 6.13 英寸、Android 13、NeoReader**；不含 Palma 一代、Palma 2 Pro、Palma 3。实际固件小版本/NeoReader 构建未知；当前通用帮助中的 V4.1+ 迁移能力单列为条件路径，官方通用更新文章不能单独证明本机已安装或收到该版本。[E-PALMA-01～03]

**D1+D2 文档研究，无实机、无连续视觉观察。** 官方产品页仍写 HD/Balanced/Fast/Ultrafast 四模式；当前帮助则已采用 EinkWise 等新界面，不能混成同一固件截图。另读到零售商托管、文件名含 Palma 2 Pro 的厂商通用 V4.0 手册，封面实际为 Mobile ePaper Series；仅用于识别历史版本/数据清理语义，不能作为 Palma 2 专属手册。[E-PALMA-15]

## 功能目录

| 域 | 已查 | 待查/适用边界 |
| --- | --- | --- |
| 单手控制 | 指纹电源键、翻页键、智能键短按/长按/双击；触控翻页区域 | 指纹录入/失败、按 app 实体键映射、锁屏生效、误双击；智能键流程仅 D1 |
| 系统/显示 | 双下拉面板、导航球、EinkWise、app 冻结 | Palma 2 当前首次向导、精确 Settings 层级、完整模式/刷新默认值 |
| 阅读 | 选段批注/查词、目录/书签、历史视图、TTS 入口 | 扫描件 OCR、断网朗读、音频焦点与逐句恢复 |
| 内容/数据 | BOOXDrop、云推送、源书与状态、V4.1备份 | 同名覆盖、下载删除、进度冲突、卡移除；未套用大屏 Notes/分屏能力 |
| 其他 | 官方列相机扫描、录音、第三方 app、小组件、屏保 | 相机 OCR 导出、录音编辑、所有屏保/小组件完整操作目录未核定 |

手机外形不能证明蜂窝通话。官方规格为 Wi-Fi/蓝牙，研究中的“来电打断”仅能作为第三方通信 app 的待测场景；不借 Palma 2 Pro 的 SIM/笔功能补本机。也不把安装 Android app 当成原生统一读听进度。

## 主要任务

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M010-01 | 单手翻页与快捷动作（D1） | Palma 2；已配置按键 | 官方确认翻页键及智能键；短按/长按/双击可分别关联刷新、AI Assistant 或第三方 app。指纹电源键用于解锁。 | 物理事件可映射不同任务；功能存在有据，但设置页面序列未取到。 | E-PALMA-01；E-PALMA-02 | 精确入口、默认映射、音量/翻页互斥、锁屏/休眠生效；不补成已观察操作。 |
| F-M010-02 | 进入控制、处理通知后回阅读 | app 开启 | 右上打开 Control Center、左上 Notification Center；Edit 调整控制快捷项。导航球点开，双击返桌面；底/侧手势可自定义。 | 清通知不同于终止播放/下载；从系统控制回 app 不保证保留页面内部选区。 | E-PALMA-04 | 通用帮助在 Palma 2 的菜单位置、媒体通知实际按钮、返回精确位置。 |
| F-M010-03 | 为阅读和滚动分别调显示 | app 已开；版本辨明 | 当前帮助：控制中心 → EinkWise；按 app 调模式，Full Refresh 可清残影。产品页旧四模式作为历史对照，不能把最新帮助的 Smooth 指定给本机。 | “本次刷新”“app 配置”“全局默认”不同；不是把更快模式设好便永久无残影。 | E-PALMA-01；E-PALMA-05；E-PALMA-06 | 当前本机实际模式及保存范围；延迟、灰阶与残影不作测量结论。 |
| F-M010-04 | 修正单手触摸翻页区 | NeoReader 已开 | 点中央 → 更多 → Settings → Touch Settings，选预设或自定义触摸区域。 | 改的是 NeoReader 触控行为，不能据此推定第三方 app/实体键同步改变。 | E-PALMA-07 | 防误触、左右手切换、旋转后的映射；预设图未逐一观察。 |
| F-M010-05 | 看书时查词、标注并回原文 | 可选文本；有词典 | 长按词/拖选段，选择词典、翻译或批注；Contents → Annotations 找回标注。单词可加入 Vocabulary Builder。 | 选词、段落标注与整书进度分别保存；第三方弹出词典需其自身支持。 | E-PALMA-08；E-PALMA-09 | 关闭浮层/返回后选区位置、词库离线内容、重复词合并。 |
| F-M010-06 | 查章/跳页后恢复阅读位置 | 同一文档内 | 中央菜单 → Contents 选目录/书签；Progress 可拖进度、跳章/输入页码。Prev View 回此前视图、Next View 前进。 | 临时历史视图与持久书签不同；不能等同跨 app 或重启恢复。 | E-PALMA-10；E-PALMA-11 | EPUB 字号变更后的锚点、书关掉后的历史长度。 |
| F-M010-07 | 从读切听并排查后台停止 | NeoReader/TTS 引擎已可用 | Progress → TTS；系统 TTS Output 配语速/语言包。后台故障检查 App Management 的 Freeze：官方明确冻结会阻止后台操作及通知。 | 有朗读入口、有引擎配置；“暂停后继续哪一句”仍不是官方已证实的状态协议。 | E-PALMA-11；E-PALMA-12；E-PALMA-13 | 锁屏、闹钟、第三方语音来电、蓝牙断连、睡眠定时、离线语音包实际结果。 |
| F-M010-08 | 电脑/手机送书后找到内容 | 同 LAN 或远程已登录 | LAN：设备开 BOOXDrop，浏览器访问所示地址上传，设备 Receive 查看；云端 Online transfer 后设备 Push 查看。 | 本地接收和远程推送分开；传输完成后才进入阅读任务。 | E-PALMA-14 | 文件重名、错误类型、移 microSD、取消/断点下载与空间不足。 |
| F-M010-09 | 保留原书并迁移读书状态 | V4.1+；同账号/服务器/路径 | 单书长按 → Sync → Cloud Sync、Upload source file；或 Library Settings → Backup and Restore → Local Backup and Restore，转移 .booxc 后在另一台 V4.1+ 恢复。 | 源书、批注/进度、本地备份分开；通用 V4.0 手册“只同步阅读数据”是历史行为，不能覆盖4.1增量。 | E-PALMA-16；E-PALMA-03；E-PALMA-15 | 冲突合并、同名重复、两端不同固件；不包括第三方 app 数据。 |
| F-M010-10 | 区分清数据与删缓存（D1） | 仅有通用 V4.0 文档证据 | 该手册的长按书属性区列 Clear settings：阅读数据清除不可恢复；文件锁只在 NeoReader 打开时有效，导出/别的 app 不继承。 | 说明操作语义与导出边界；未将菜单名承诺为 Palma 2 当前可照做路径。 | E-PALMA-15 | Palma 2实机确认、完整删除文件/回收站路径、确认对话框、账号切换与撤销。 |

## 分支与未闭环状态

| 场景 | 目前可成立的判断 | 下一项观察 |
| --- | --- | --- |
| 实体键在某阅读 app 不翻页 | 本机有翻页键不代表每个 app 使用同一输入事件；目前无本机官方逐 app 映射路径。 | 指定一个第三方 app 和版本，记录 BOOX 与 app 各自设置、事件结果。 |
| 关屏后听书停止 | 通用帮助指出冻结阻止后台；但不能单因该开关存在，就说取消冻结必然修复。 | TTS 与音频 app 各跑锁屏、应用切换、断网、蓝牙四种中断。 |
| 换设备只有进度没有书 | 旧固件手册仅同步状态，V4.1 增加源书上传；必须先问双方版本和是否上传源文件。 | 两端文件路径、文件大小/版本、来源和同步开关。 |
| AI 快捷键已能打开聊天 | 只证明一个入口；没有任意页面语义读取或跨 app 动作成功证据。 | 打开书内选区后问 AI，它能否取得准确文本与位置、动作是否可撤销。 |

尚未核定全部功能数；账户可得的免费云空间是 ONYX 服务，流媒体/app 自己的订阅、离线授权和数据不在同一套餐内。完整新机手册仍需设备或账号获取；没有为此注册/登录/安装应用。[E-PALMA-17]

## 观察与推断

本轮阅读官方产品/帮助及厂商署名资料。没有可用的连续视觉记录，网页图不算真实操作；PDF截图请求未得到可观察画面，未计视觉证据。

对 Plus 的推断：掌上场景可借鉴固定“继续读听”、物理返回/暂停和显式全刷入口，但需要先确定按键在当前 app、锁屏、录音与电话中的优先级。G-002 要暴露当前按键映射和前台/后台状态，否则 AI 说“继续”可能只打开 app。对 R-018 与 5–6 岁自主读听，应把书的位置与音频位置分别保存并可解释地恢复；对成人新闻，滚动/阅读模式的切换可用任务预设，但不能默认满足 R-011 的60fps/无残影目标。

## 证据

全部查于2026-10-26；共享 BOOX 帮助通过官方正文/公共API读取，更新日期写入批次。15为厂商署名、零售商托管通用手册，绝非已固定机型截图。

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
