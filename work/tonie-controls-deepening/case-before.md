# M-038 Toniebox 1 / 2：实体内容入口、离线准备与家长配置

## 范围

查阅：2026-10-26。**Toniebox 1 与 Toniebox 2 分开记录**，不与 Toniebox Lite 混用。证据来自英文美国/欧洲/澳洲官方帮助；具体固件、tonies app 构建和账号地区未知。1 代实体暂停/续播细节来自 EN 1.0、2018-04 的官方旧手册，适用性单列；其余尽量采用当前带代际名称的帮助页。家长 app 是管理端，盒子是实际播放/缓存端，Tonie/Creative Tonie 是内容入口，Toniecloud 是自制内容的保存和分发环节。

**D2 文档重建；无实机、无连续视觉观察。**已查实体选内容/音量/跳转、下载与离线、家长配置同步、Creative Tonie 编辑/删除、2 代远程播放和睡眠。未核定全部 Tonieplay 游戏、用户/家庭权限和耳机出口，未把图像替代文字说明或当成完整流程。

## 两代任务路径

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M038-01 | 1 代选故事、暂停和继续 | 盒子开机、绿灯；内容可用 | 放上 Tonie 播放；取下暂停，再放同一个续播。大/小耳朵调整音量。旧手册明确：中间没有播过另一 Tonie 才从原处恢复。 | 公仔存在性参与播放控制；不是每个公仔都已证明有独立持久断点。 | E-TONI-01 | 当前固件、自动关机/耗尽电量后位置、相同公仔换盒；旧手册限制待当前机验证。 |
| F-M038-02 | 1 代先下载，再带出门 | 已完成设置、有 Wi-Fi | 首次放公仔需联网；蓝灯闪说明未下完，留在顶部直到提示音与常绿。出门前主动开离线模式，逐个试播可检查是否准备好。 | 音频完整缓存在盒子中，公仔不等于音频存储卡；新版 Creative 内容也需再次联网取得。 | E-TONI-02 | 下载取消/断线续传、容量淘汰规则、缓存精确列表、离线错误词。 |
| F-M038-03 | 1 代切章与段内快进 | 内容已完整下载、加速度传感功能开启 | 敲侧面跳曲；轻倾直到提示音并保持姿势，在曲内快进/后退。App：Collection → Toniebox → 选盒 → Advanced settings → Fast-forwarding & rewinding 可改侧向含义，然后同步。 | 切曲和改变时间点是不同动作；未下完或功能关闭时不能据手势无效推定硬件坏。 | E-TONI-03；E-TONI-04 | 敲击力度、误识别、单章/末章边界、暂停时手势；未测固定秒数。 |
| F-M038-04 | 1 代开/关离线并下发设置 | 顶部无公仔 | 双耳约 5 秒至提示音/蓝闪，再单耳约 3 秒至白灯与语音进入离线；退出为双耳至绿灯/在线语音。联网同步配置：无公仔时单耳约 3 秒，等常绿及提示音。 | 离线状态跨使用保持，需手动退出；离线不取新公仔/新内容。App 保存配置与盒子接收配置是两步。 | E-TONI-05；E-TONI-06 | 同步失败回滚、离线时改配置的积压与冲突、语音语言。 |
| F-M038-05 | 家长给 Creative Tonie 添加内容 | 家庭账号、公仔已加入家庭 | App 中选择 Creative Tonie，可录音或上传已有 MP3/WAV，保存；官方教程列 90 分钟总量。再按具体盒子代际同步。 | 云端内容清单与盒子本地内容版本分离；上传不等于盒子已能离线播。 | E-TONI-15；E-TONI-05；E-TONI-10 | 录音权限、取消上传、格式失败、容量超限实际提示、不同家庭权限。 |
| F-M038-06 | 重排/删去 Creative 章节 | 已有自制内容 | Collection → Creative Tonies → 目标 → Edit；拖动三线柄排序后 Done；删除则选圆圈 → Delete → Done。网页版为 Edit Content，删后 Save Content。1 代随后单耳同步。 | 改云端章节顺序/成员，盒子同步后才体现。官方不提供把上传内容重新下载备份的入口，应另保留原文件。 | E-TONI-16；E-TONI-17；E-TONI-18 | 删除确认/撤销、录音导出、旧缓存清理时机、正播放章节被删除后的进度映射。 |
| F-M038-07 | 2 代查看并远程控制当前播放 | 盒子有互联网、家长 app 可连接；顶部 Tonie | App 播放控制提供当前 Tonie/章节、播放/暂停、重启/跳过。官方明确这些控制仅限 2 代；Tonieplay 游戏只可用其中的音量控制。 | 家长可以观察实际盒子当前内容；这是联网控制，不代表 app 离线或 1 代同样有效。 | E-TONI-08 | 完整入口层级、指令送达/超时反馈、重启究竟回曲还是整本、多人同时控制。 |
| F-M038-08 | 2 代下载后进入离线 | 顶部无 Tonie/游戏；所需内容已全下完 | 倒置盒子并长按双耳，至白色灯环进入 Parent Access；捏大耳听到离线语音。退出使用相同入口再捏大耳。 | 仅可用完整已缓存 Tonie/Tonieplay；不能激活新内容，App 配置不同步，文档列日出闹钟等 app 功能停用。状态需手动解除。 | E-TONI-09 | 离线时实体睡眠键可用范围，不能由“app 功能停用”泛推所有本地计时。 |
| F-M038-09 | 2 代更新配置/内容并纠正“还播旧的” | 网络可用，暂不放公仔 | 配置联网自动同步；固件更新需空置等待自动关机完成，放公仔会让更新跳过/延后。旧内容排查先在 app 看 online；帮助页称开机后约 30 秒同步，太早放公仔会阻止它，建议重新唤醒后先等。 | 配置已保存、盒子在线、同步完成、媒体已缓存是不同状态；故事优先可使更新延后。 | E-TONI-10；E-TONI-14 | 30 秒是官方提示而非本项目测速；网络差时完成确认、同步中断后版本原子性。 |
| F-M038-10 | 2 代睡前计时、续设与停止 | App 操作要求盒子在线 | Mine → 盒子 → Sleep Timer with Light，选 5–60 分钟；按钮显示剩余时间、灯环橙色，可重启/停止计时或立即关机。实体双耳 5 秒也可切换，使用记忆的上次配置；出厂默认 30 分钟。 | 计时终止播放/关盒，睡眠灯与播放状态相关；不把这个功能赋给 1 代。 | E-TONI-11；E-TONI-07 | 计时期间换公仔、重启、离线转换、下一晚记忆范围。 |
| F-M038-11 | 2 代判断“不能播/没灯” | 需知道年龄模式、供电、灯环设置 | 1+ 模式充电时不播放且禁倾斜快进，3+ 模式可边充边播。无灯可能关机或亮度 0；蓝色全环闪可为联网/下载，常绿可放内容，彩虹表示 Tonie 正播放。 | 一个外观并不唯一映射一个状态；年龄模式影响可用动作。 | E-TONI-12；E-TONI-13 | 年龄模式切换入口与确认、实体取放/续播的 2 代精确保存规则。 |

## 主要异常、世代差异和材料冲突

1 代官方当前帮助明确不提供定时器，也不能用 app 逐标题禁用公仔内容；app 可改耳机/扬声器最大音量、LED 亮度和敲击/倾斜方向。[E-TONI-07] 2 代则有联网 app 播放控制和睡眠计时；泛称“tonies app 能遥控”会误导 1 代家长。[E-TONI-08、E-TONI-11]

1 代旧手册中的重启与恢复出厂动作相近，但有无充电底座改变结果：离开底座倒置长按双耳是重启，连底座做相应动作是重置并清网络、内容和设置。因此不能把“一直按双耳”当作通用排障。此处是旧版操作风险的研究记录，实际使用应按型号当前支持页核对。[E-TONI-01]

2 代灯环红色全环闪和部分闪分别指向连接错误/错误词与低电；需结合语音和触发时刻，而不是只认“红灯坏了”。[E-TONI-13] App 离线仍能改表单并不代表盒子收到；本案例没有观察待同步提示或冲突合并。

2026-08 的 Creative Tonies 通用教程仍给出“单耳同步”步骤；专门 2 代帮助明确自动同步，因此本案例只把通用教程用于录入内容，2 代同步以专页为准。[E-TONI-15、E-TONI-10、E-TONI-14] 睡眠专页与离线页对离线后的本地计时范围没有说清，保留未知。故事队列、书签、笔记和跨盒逐书进度未查到足够证据，不写“不支持”。

## 对本项目的分析（推断）

Tonie 的可借鉴处是“对象即入口”：孩子无需先读出书名就能选择内容；对已能独立阅读的 5–6 岁儿童，应比较实体入口与可读封面/标题列表的效率，不能直接认定实体公仔优于屏幕。对应 R-018，1 代的换公仔例外提醒本项目明确“谁、哪本、哪个版本、听到哪、是否仍想继续”，不能只保存最近一个播放槽。

对 G-002，AI 要得到同步和缓存的真实状态，才能回答“明天没网能听吗”“刚换的故事为什么没变”。若只有后台编辑权限而无设备确认，不能宣称任务完成。家长准备、儿童选择、设备播出是三段任务；其云依赖与备份不可逆性也应独立讨论。Plus 的输入、音频与内容识别仍需硬件核查；本案例不构成 NFC、公仔或云服务选型。

下一项最小观察：用同一家庭账号和一只 Creative Tonie，在两代分别做 A→暂停→A、A→B→A、离线前后改章节、睡眠中换公仔；保存 app 状态和盒子语音/灯光的同步记录。达到“每代每个状态转换有完整起止记录”才升级连续观察。

## 证据

均于 2026-10-26 获取官方正文；未实机。除明确写出的日期，其余页面更新时间未显示。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-TONI-01 | [Toniebox Guide EN 1.0](https://assets.ctfassets.net/l6vo76cztcax/6PkEgkjjGmwpatuF0n3lTH/1e2878aad4e7f405e9b07fc1eab54fdb/Manual_Toniebox_EN.pdf) | 2018-04，PDF 第 2 页 Playing and pausing；第 1 页重启/重置。仅旧 1 代。 |
| E-TONI-02 | [1 代无 Wi-Fi 使用](https://support.tonies.com/hc/en-us/articles/29036566638994-Does-the-Toniebox-1-work-without-Wi-Fi) | 下载完成指示与出行前离线试播。 |
| E-TONI-03 | [1 代快进/后退](https://support.tonies.com/hc/en-us/articles/29036493650962-How-do-I-fast-forward-and-rewind-using-the-Toniebox-1) | 倾斜、提示音、完整下载条件。 |
| E-TONI-04 | [1 代切换曲目](https://support.tonies.com/hc/en-us/articles/29036580563602-How-do-I-skip-back-and-forth-between-tracks-using-the-Toniebox-1) | 敲击、方向配置、同步。 |
| E-TONI-05 | [1 代同步](https://support.tonies.com/hc/en-us/articles/29036498212754-How-do-I-update-my-Toniebox-1-or-sync-changes-I-ve-made) | 空顶部、联网、单耳、绿灯提示音。 |
| E-TONI-06 | [1 代离线模式](https://support.tonies.com/hc/en-us/articles/29036416576402-How-do-I-put-my-Toniebox-1-in-Offline-Mode) | 开关步骤与保留状态。 |
| E-TONI-07 | [1 代 app 控制范围](https://support.tonies.com/hc/en-us/articles/29036587884050-Can-I-control-the-Toniebox-1-remotely-via-the-tonies-app) | 配置项、无 timer/逐标题停用。 |
| E-TONI-08 | [2 代 Playback Features FAQ](https://support.tonies.com/hc/en-150/articles/37307520777874-Playback-Features-FAQ) | 当前章节、远程播放、联网要求、排除 1 代。 |
| E-TONI-09 | [2 代离线模式](https://support.tonies.com/hc/en-au/articles/29352008338578-How-do-I-put-my-Toniebox-2-in-Offline-Mode) | Parent Access、手动退出、app 功能停用。 |
| E-TONI-10 | [2 代同步与固件更新](https://support.tonies.com/hc/en-150/articles/31004929359250-How-do-I-update-my-Toniebox-2-or-sync-changes-I-ve-made) | 自动同步、空置更新、内容优先。 |
| E-TONI-11 | [2 代 Sleep Light / Sunrise Alarm](https://support.tonies.com/hc/en-150/articles/29255589102866-Sleep-Light-with-Timer-Sunrise-Alarm-guide) | app 与双耳入口、剩余时间、记忆配置；闹钟仅目录级。 |
| E-TONI-12 | [2 代 Features FAQ](https://support.tonies.com/hc/en-150/articles/29036389310738-Meet-the-Toniebox-2-Features-FAQ) | 1+/3+ 模式；Tonieplay 与普通 Tonie 区别。 |
| E-TONI-13 | [2 代灯环状态](https://support.tonies.com/hc/en-150/articles/33985836073874-Toniebox-2-Dynamic-Light-Ring-Guide) | 无灯、蓝、绿、红、彩虹；只读正文未观察动画。 |
| E-TONI-14 | [2 代仍播放旧内容](https://support.tonies.com/hc/en-150/articles/34276878021138-My-Tonie-Creative-Tonie-is-not-playing-the-updated-content-on-the-Toniebox-2) | online 检查、约 30 秒同步、过早放公仔。 |
| E-TONI-15 | [Creative Tonies 101](https://us.tonies.com/blogs/beyond-the-box/creative-tonies-101) | 2026-08-18，注册、录音/上传、90 分钟；通用同步段不套给 2 代。 |
| E-TONI-16 | [重排 Creative 内容](https://support.tonies.com/hc/en-us/articles/29036510512658-How-do-I-change-the-order-of-the-content-on-my-Creative-Tonie) | App Edit/拖柄/Done，网页版 Save Content。 |
| E-TONI-17 | [删除 Creative 内容](https://support.tonies.com/hc/en-us/articles/29036577008658-How-do-I-delete-content-from-my-Creative-Tonie) | 选中/删除/保存/同步。 |
| E-TONI-18 | [Creative 内容可否下载](https://support.tonies.com/hc/en-us/articles/29036568693522-Can-I-download-content-from-a-Creative-Tonie) | 上传文件不可重新下载，另留原文件。 |
