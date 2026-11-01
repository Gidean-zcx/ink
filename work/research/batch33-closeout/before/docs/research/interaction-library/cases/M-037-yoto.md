# M-037 Yoto Player 第三代 / Mini 2024：儿童控制、家庭内容与离线数据

查阅：2026-10-26；本轮补证2026-10-27。固定 **Yoto Player（3rd Generation）及 Yoto Mini（2024 Edition）**，伴随 iOS/Android app；不混入第四代绿键、电源短按语义或旧 Mini 充电方案。按官方帮助适用型号引用。无实机固件/app 构建：OS 记录列 2.23.2，另说 2.23.3 仅限特定新生产批次；app 更新页列稳定版 3.34（2026-04-14）和 beta 3.35。这些历史列项不能证明查阅日所有用户运行该版（E-YOD-19、20）。

地区/语言为英文官方帮助，不宣称中国大陆服务可用性。家庭账户、普通持有卡、MYO、自选数字内容和 Club Collection 分开；会员只在相应内容路径构成条件。**D2 文档重建；无设备操作、登录、购买、连续视觉观察或单帧图像观察。** 2026-10-26主要为官方搜索索引；本轮6个正常公开HTML已归档，分类页内嵌正文可直接解析，未调用API。两个闹钟配图返回403后停止，均未观察。所有时间为文档描述，未测响应耗时。

## 功能目录与覆盖

下表保留2026-10-26基线，旧“未查”状态以本轮更新及新台账为准。仍未取得固定版本完整单册。下表以官方 app 总览全部一级页签及硬件/设置/家庭/MYO/离线帮助目录重建；**帮助站文章全集未核定**。目录项只计覆盖范围，不计新流程。

| 功能域 | 官方目录/章节 | 已关联 F / E | 缺口/未覆盖 |
| --- | --- | --- | --- |
| 首次与联网 | Setup；Wi-Fi troubleshooting | F-M037-04；E-YOD-03、04 | 恢复出厂迁移、全部校园/公共网络分支未查。 |
| 儿童实体控制 | Controls → Earlier players | F-M037-05；E-YOD-01 | 取卡/重插、换卡、重启的精确断点未查明。 |
| app News | Using the app → News | E-YOD-02；目录 | 推送、通知权限和回退未深入。 |
| app Library / 播放 | Library、Device Picker、Now Playing、Maxi Player | F-M037-06、09；E-YOD-02、08 | 搜索/筛选/Groups、双端并发、手机后台/锁屏/来电恢复未覆盖；变速设备适用范围未统一确认。 |
| app Create / MYO | Record、My recordings、Playlist；MP3/MYO 帮助 | F-M037-07、08、13；E-YOD-06、07、10 | 文件限制、上传失败/重复、原稿导出、删除撤销未查。 |
| app Discover / 内容 | Radio、Podcasts、Sleep、Stories；Club Collection | E-YOD-02、18；F-M037-01 | 已区分流与可下载音频；RSS、电台 URL、交互卡完整流程未覆盖。 |
| Settings：状态 | 名称、电量、Manage Wi-Fi、Audio Download Status | F-M037-01、04、14；E-YOD-02、04、16 | 下载优先级、完整缓存管理入口未核定。 |
| Settings：日夜与闹钟 | Alarms；Day/Night、亮度、音量、Right Button Shortcuts | F-M037-02、06；E-YOD-02、05 | 闹钟编辑/取消和计时冲突未查；Player 夜灯与 Mini 分开。 |
| Settings：Advanced | 节能/自动关机、Bluetooth pairing、耳机限音、时钟/时区、版本/注册信息、Beta、Unpair | F-M037-12、15；E-YOD-02、14、15 | 节能与下载、解绑删除范围、beta 退出/降级未查。 |
| 家庭用户与设备 | Pair phone；Add users；Add players | F-M037-11；E-YOD-11、12、13 | 同书库不证明跨设备断点、同步队列、多房间同时播放。 |
| 内容归属与恢复 | Remove cards；Share/gift；Replace card；Club | F-M037-09、10、13；E-YOD-08、09、10、18 | 外借后缓存、同时播放、授权撤回延迟未说明。 |
| 离线/错误/更新 | Offline；Download/playback；Mini card reading；OS/App updates | F-M037-01、03、14；E-YOT-01、03；E-YOD-16、17、19、20 | 手机离线缓存、固件更新失败、配额与错误日志导出未覆盖。 |

## 流程与状态

历史追加04–15保留；2026-10-27仅新增16–19，相同任务补原号。“反馈”均是文档陈述。

| 流程 | 操作与反馈 | 状态/失败 | 证据 |
| --- | --- | --- | --- |
| F-M037-01 为外出准备离线故事 | 联网、接电，插卡触发内容准备；取卡，保持开机空闲。app → Settings → 设备 → Audio Download Status 查完成。 | 新内容先联网；电台/播客流和 app 远程发起有联网条件。每台独立缓存；淘汰不移除app库，在线重播可触发重下。Player3为32GB；当前帮助把Mini2024与2021–23合列16/32GB并给机底序列规则；这不证明2024本身有两种装配，实际容量需核本机/型号专属资料。准备路径不保证隔夜完成。 | E-YOT-01；E-YTCD-12 |
| F-M037-02 睡前限时播放 | 播放条 → Maxi Player → 月亮/星星 → 时长，到时停播。 | 换卡/内容不重置；返回月亮星星图标选off可提前取消。重启、断电、跨设备未知。 | E-YOT-02 |
| F-M037-03 卡顿时判断原因 | 检查弱网络及首次下载队列；接近路由器或等下载完成再播。 | 书库可见与本机完成不同；正文称红时钟在线、白时钟离线、绿脸为连Wi-Fi而无互联网。图标未实际观察；下载调度冲突见下。 | E-YOT-01、E-YOT-03、E-YTCD-14 |

| 流程 | 功能/深度 | 前置状态 | 操作与文档反馈 | 数据变化/结束 | 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M037-04 | 首次配网与失败恢复 D2 | 本轮两款接电、无卡；app；适用 2.4GHz 网络 | Set Up a Player → 双橙键按 5 秒得六字母码 → app 输码、Join 设备热点 → 码变绿/语音提示 → 选家庭 Wi-Fi 输密码 → 绿 Wi-Fi；Pair My Phone 后重启回时钟。绿 Wi-Fi 脉动表示更新。 | 建网络/家庭关联；更新未完保持连接。热点切换失败可在手机 Wi-Fi 手动加入 Yoto_xxx，码变绿后回 app 等网络列表。 | E-YOD-03、E-YOD-04 | iOS Local Network 拒绝后需在手机设置开启；Android 自动切移动数据可能打断配网。空书库、更新失败后状态未测。 |
| F-M037-05 | 儿童选章、暂停、停止 D2 | Earlier players；插内容卡 | 左旋钮调音量、按上一章；右按下一章、转动选章；播放时左右长按回退/快进，屏显箭头表示加速：文档写超过1秒、4秒、7秒后依次20/50/120音频秒每秒，不当实测或精确寻位。电源短按暂停/继续，长按停止；无卡右键可切 Daily、Radio、时钟。 | 当前播放/章节改变；约 3 秒电源开关、10 秒硬重启，后者保留 Wi-Fi/音频。Player 伏面可调夜灯，Mini 不套用。 | E-YOD-01、E-YOD-20 | 普通“长按停止”与“约 3 秒关机”分界未定义；取卡保存时机、选章起点、误触复原未知。 |
| F-M037-06 | 家长选播放目标与限音 D2 | 家庭设备已登记；改限音时设备开机联网 | Device Picker 先选手机/目标播放器 → Library 播放；Now Playing 进入 Maxi。Settings → 所选设备 → Day/Night Volume Limit 分别设置。 | 控制所选设备；限制 0 可无声、最大档 16；耳机限音另有设置。 | E-YOD-02、E-YOD-05 | 下发失败、儿童同时调音量的冲突未知。日夜限音与无线耳机自有限音不可等同；手机控件不能全推广到设备。 |
| F-M037-07 | MP3→MYO 列表→写卡 D2 | 自有可用音频、MYO；网页/app 同账户 | MYO → New Playlist（或网页 My Cards）→ 建列表、上传、Save；app 列表详情 → … → Link to Card → 插 MYO → Link。红录制图标后绿勾，取出再插播放。 | 卡关联云端列表；本机音频下载是另一状态，离线仍查F01。自建MYO限100轨，单轨100MB/60分钟、整卡500MB/5小时；直接关联已购卡不套此限。 | E-YOD-06、E-YTCD-13 | 上传取消/断网、重复命名、卡旧关联保留时点、误覆写撤销未知。 |
| F-M037-08 | 录音→列表→更新/删除 D2 | app Create；家庭录音/MYO | Record → 倒计时 → 录音（可暂停/继续）→ Save → My recordings；选录音 Add to a playlist。修改共享列表会更新 app/关联卡。列表 … → Delete → 确认；录音左划 → 红删除。 | 删除后让设备接电、在线、空闲更新卡；录音保存、上传到列表、卡关联是不同状态。 | E-YOD-07、E-YOD-06 | 无已查回收站、误删恢复或原稿导出。编辑时正播行为、删录音与列表副本持久关系未完整说明。 |
| F-M037-09 | 书库移除与重新加入 D2 | Library 已有普通卡 | 卡详情 → … → Remove from the app → Remove；可 Cancel。再在设备播实体卡或手机扫描，可重新加入。 | 书库条目移除/回归；不等于卡失效或立即擦除各机缓存。 | E-YOD-08 | 无实体卡时恢复、删除传播延迟及缓存回收时机未知。 |
| F-M037-10 | 跨家庭借卡与取回 D2 | 普通持有卡；另一家庭 | 对方设备播卡后，条目转到对方书库；自己在自家设备播放/手机扫描，可转回。 | 原家庭条目相应移除；关联已购内容的 MYO 也适用。 | E-YOD-09 | 不套用原创录音或 Club Collection；离线、同时播放、断点的结果未知。 |
| F-M037-11 | 加家庭成员及第二台 D2 | 各用自己凭据；已有家庭 | Settings → Join a Family Account → 设备六字母码 → Join This Family Account；第二台用 Set Up a Player → Pair My Phone；Settings 显示成员/设备。 | 成员可控全部家庭设备/设置；每位成员在Settings → 用户三点可移除他人或离开家庭，不推定管理员独占权限。加入新家庭会离开原家庭。 | E-YOD-11、E-YOD-12、E-YOD-13、E-YOD-19、E-YTCD-08、E-YTCD-09 | 3.22 另引入邀请码；旧帮助仍称 Menu/Players，路径采用较新 Settings 帮助，不合并两种代码机制。未证明播放秒点共享。 |
| F-M037-12 | 手机输入：蓝牙音箱 D2 | Player 3rd / Mini 2024；手机 | Settings → 设备 → Advanced → Bluetooth Pairing 打开；左键约 3 秒 → 灰蓝牙 → 右旋钮选音箱并按 → 手机配对 → 确认图标后右键确认。 | 手机音频输出到 Yoto，播放由手机控制；左键约 3 秒退普通插卡模式。 | E-YOD-14 | 开关只允许进模式，不等于已连接；来电/断连后自动退出和卡片断点未知。 |
| F-M037-13 | 丢卡后以 MYO 恢复入口 D2 | 原内容仍在 Library；可用 MYO | 原卡详情 → … → Link to Card → 插 MYO、Link → 红图标转绿勾 → 取出重插。 | 一张已购内容对应一次 MYO 关联；找到原卡后可将 MYO 改连别的内容。 | E-YOD-10 | 不是恢复音频原稿，也不是多本已购卡合成列表；两卡缓存和断点未知。 |
| F-M037-14 | Mini 读卡故障与下载状态分流 D2 | 内容不播放/卡不识别 | 下载排错沿用 F-M037-03；查 Audio Download Status，完成仍异常可按电源 10 秒重启。Mini 多卡均不识别，用手机 NFC/另一台读卡区分卡与设备；Mini 帮助另给关闭 Bluetooth Pairing 后重试分支。 | 新分析对象是读卡器/卡片识别，而非重复登记下载流程；下载完成、读卡成功是不同节点，仍失败转支持。 | E-YOD-16、E-YOD-17、E-YOD-01 | Mini 帮助未逐项列批次，不当成每台必现。商城预览不等于可离线内容；错误动画和修复耗时未观察。 |
| F-M037-15 | 蓝牙耳机输出、取消、重连 D2 | 本轮两款；耳机靠近并配对 | 左键约 3 秒 → 右旋钮选耳机并按；搜索蓝图标闪烁，成功为彩色图标及钟面左上蓝点。取消：左长按回菜单 → 右旋钮选耳机并按 → 左按退出。 | 输出转耳机；不自动重连时重走选择。误配时移开/关闭其它配对设备再试。 | E-YOD-15 | 断连是否自动扬声、忘记旧耳机、音量继承未查；不搬用第四代“忘记耳机”组合键。 |
| F-M037-16 | 设置、取消及停响闹钟 D2 | App至少2.46、OS至少2.10.7；设置时目标设备开机联网，非已验证构建 | Settings → 目标设备 → Alarms → Set/Add an alarm；选时间、重复日、声音和音量 → Save alarm，Cancel不保存。默认全周，去掉不用的日；手机/设备预听是不同目标。 | 保存后列在该机闹钟列表，每机最多10项；点已有项编辑后保存或取消，右侧开关启停，详情底部Delete删除。到时需开机；任意设备键或app停止响铃。音量受日夜上限约束。 | E-YTCD-06 | 离线响铃、自动关机后唤醒、正在播故事时打断/恢复、时区变更和多机同步未说明；预听耗时未实测。 |
| F-M037-17 | 设日夜作息与第三代夜灯提示 D2 | Player3或Mini2024；家庭设备已登记；夜灯仅Player3 | Settings → 目标设备，分别设Morning/Bedtime；钟面日/月标记按时段变化，可分设限音/亮度/右键快捷。Player3另设两段夜灯颜色；翻面开、竖起关，伏面转右旋钮调亮至0。 | 日夜作息、夜灯、F02停播和F16闹钟是不同状态；Mini2024仅用日夜设置，没有夜灯。Player3夜灯模式暂不读室温；不套第四代自动夜灯亮度。 | E-YOD-02、E-YTCD-07 | 更改作息时正播内容、时区/离线转换、设置下发失败及跨设备冲突未知；第三代屏幕自动亮度不等于夜灯自动亮度。 |
| F-M037-18 | 接受亲友MYO列表并保留作者关系 D2 | 收到共享MYO链接；使用app；已有可用MYO；家庭加入不是此页接收步骤 | 从收到的链接在app预听，检查创作者身份；接受后加入自己的Library，再按F07关联实体卡。正文未给发起分享的当前完整按钮路径，此行从收件开始。 | 收件人接受的是共享列表入口，所有权仍属原作者；后续作者更新会反映在关联卡。作者邮件地址可见，持链接者可继续转发。内容被暂停后，官方仅受理创作者申请复核。 | E-YTCD-10、E-YOD-06 | 未接受/拒绝的具体按钮、复制成独立副本、撤回链接、删除成员对共享列表的影响、离线缓存何时失效和正在播放时更新未知。 |
| F-M037-19 | 清空本机后重新设置并区分家庭解绑 D2 | 固定Player3/Mini2024；官方Player/Mini三键步骤，无实际固件；删除性动作仅文档研究 | 同时按住三键：Player夜灯红脉动，Mini屏显红点；在3秒内松开并按一次右橙键。Player短橙闪/Mini橙点后关机；然后重走F04设置。 | 下载内容清除，但家庭绑定保留。赠送前另在Settings → 设备删Player Name，底部Remove from family account；用户退出则Settings → 本用户三点 → Leave this family account。 | E-YTCD-11 | 三键等待时长、错过3秒窗口、清除失败、缓存/进度/设置完整恢复、解绑传播与他人书库归属未核；不把F05硬重启当出厂重置。 |

## 分支、数据状态与证据冲突

- **断点与多设备仍有缺口。** F-M037-05 确认当次暂停/继续，F-M037-11 确认家庭设备/书库可见；均不证明取卡再插、A→B→A、手机→Player、两台设备、重启或断网恢复后继续同一秒点。OS 旧日志的播客恢复有出版方条件，不能扩展为所有卡的当前规则（E-YOD-20）。不为未知能力虚造 D2。
- **下载调度冲突保留（2026-10-27正文仍如此）。** E-YOT-01 写流式播放与后台下载，E-YOT-03 又称新内容在无卡空闲时下载。型号/固件条件不足；共同路径是外出前接电、空闲并逐台查完成。缓存按设备算，另一台已完成不证明这一台可离线。新容量页称删除“最长时间未播放”的音频，离线页则写least-played；频次与最近使用不是同一指标，当前算法仍未知（E-YTCD-12）。小时容量是语音样本估算，不作为保证。
- **删除分层。** F-M037-09 是书库条目，08 是录音/列表，07 是卡关联，01 是设备缓存；不能把其中一个宣称为“从所有地方永久删除”。MYO 列表与录音没有已查撤销路径。
- **Club 下载不等于永久授权。** Collection 的 MYO 须会员有效且内容仍在集合；计划结束后手机、设备和 MYO 失去播放权限，可购买该内容或续订恢复；只限登记家庭设备（E-YOD-18）。不套用普通已购卡，纯离线时授权何时重查未知。
- **日夜设置不是单一场景。** 右键快捷、限音、夜灯、闹钟、睡眠计时分别存在，F16已补闹钟编辑/取消/停止，F17补日夜与夜灯；其打断/续播优先级仍未核定。Mini 不套用 Player 夜灯（E-YOD-02）。
- 旧 Menu/Players/Account 与新 Settings 名称并存；邀请码发布记录与现场六字母码路径并存。保留定位与日期，不把帮助页日期当构建版本。

## 对本项目的意义（推断）与下一步

关联 R-018、H-004、G-002：儿童用卡/旋钮控制当前播放，家长用 app 管内容、目标设备和限制。AI 回答“已准备好出门听”需知道目标设备、内容授权、缓存完成与网络；回答“从上次继续”还需明确用户、卡、设备与时间位置。Plus 可据此提出状态需求；本案例没有验证 Yoto 自然语言控制或 AI 续讲。

最小观察：固定 Player 第三代、Mini 2024 及关于页版本，用普通卡/MYO 记录换卡、取卡、暂停、重启、两机及手机切换、离线缓存淘汰、正播时编辑列表、蓝牙断连；另用可恢复测试内容区分书库移除/录音删除。记录章节与秒点，不只看封面。当前未执行。**本轮只更新研究文档，无账户、设备、订阅或产品实现变更。**

## 证据

旧 E-YOT-01–03 保留；E-YOD 为2026-10-26新增定位。查阅日均为 2026-10-26；均取得官方搜索索引正文，未形成页面图像/连续流程归档。

| 证据 | 官方来源 | 章节/页面所示更新日 |
| --- | --- | --- |
| E-YOT-01 | [联网与离线条件](https://support.yotoplay.com/en_gb/does-my-yoto-need-to-be-online-to-play-content-rkmbFFj7fg) | Download before trip；More than one player；2026-09-10。 |
| E-YOT-02 | [睡眠计时](https://support.yotoplay.com/en_gb/how-to-use-the-sleep-timer-on-your-yoto-HJ4gYKj7Mg) | Maxi timer；2026-07-20。 |
| E-YOT-03 | [卡顿与播放故障](https://support.yotoplay.com/en_gb/sound-is-stuttering-or-playback-isn%27t-working-on-my-yoto-BJ1yKYsXMg) | 新下载/弱网；与 E-YOT-01 保留冲突。 |
| E-YOD-01 | [实体控制](https://support.yotoplay.com/en_gb/how-to-control-your-yoto-player-SyGKKj7zg) | Earlier players controls；2026-09-10；排除第四代段落。 |
| E-YOD-02 | [app 页签与设置总览](https://support.yotoplay.com/en_gb/using-the-yoto-app-on-ios-and-android-ryJtFi7Me) | News/Create/Library/Discover/Settings；2026-07-17。 |
| E-YOD-03 | [首次设置](https://support.yotoplay.com/en_gb/how-to-set-up-your-yoto-player-or-mini-ryCTOYjmMl) | 旧代步骤 1–4；2026-10-16。 |
| E-YOD-04 | [Wi-Fi/配网异常](https://support.yotoplay.com/en_gb/wi-fi-and-connection-issues-S1YCuKjmGg) | 3rd Gen and older；Lost connection；2026-10-02。 |
| E-YOD-05 | [音量限制](https://support.yotoplay.com/en_gb/how-to-use-the-volume-limiter-on-your-yoto-SkDlFFiQMg) | Day/Night Volume Limit、Headphone Volume Limiter；日期未取得。 |
| E-YOD-06 | [MP3 到 MYO 卡](https://support.yotoplay.com/en_gb/how-to-add-your-own-mp3-files-to-a-yoto-card-rJl2utiQfe) | Playlist upload、Link to Card；2026-07-16。 |
| E-YOD-07 | [录音、更新、删除](https://support.yotoplay.com/en_gb/record-your-own-stories-BkKi_Fo7fg) | Steps 1–4；Playlists；Delete；2026-07-20。 |
| E-YOD-08 | [书库移除/重新加入](https://support.yotoplay.com/en_gb/how-to-remove-cards-from-your-yoto-library-B1O2_FjQMl) | 步骤 1–4 / Re-add；2026-07-16。 |
| E-YOD-09 | [分享/赠予普通卡](https://support.yotoplay.com/en_gb/how-to-share-or-gift-yoto-cards-r1LiOFi7fl) | 跨家庭归属；2026-07-20。 |
| E-YOD-10 | [MYO 替代丢失卡](https://support.yotoplay.com/en_gb/how-to-replace-a-yoto-card-with-a-make-your-own-card-rk42utsQze) | Link card；找到原卡；2026-07-16。 |
| E-YOD-11 | [家庭添加播放器](https://support.yotoplay.com/en_gb/how-to-add-yoto-players-to-your-family-account-Hy22dYs7zl) | Second player / Pair My Phone；具体日期未取得。 |
| E-YOD-12 | [家庭添加用户](https://support.yotoplay.com/en_gb/how-to-add-users-to-your-family-account-HJo2dtsQGg) | Join existing family；One family；日期未取得，保留旧导航。 |
| E-YOD-13 | [手机/设备配对](https://support.yotoplay.com/en_gb/how-to-pair-your-phone-with-your-yoto-player-or-mini-rk1ROFj7Gg) | First person / Second user；2026-07-21；较新 Settings 名称。 |
| E-YOD-14 | [作为蓝牙音箱](https://support.yotoplay.com/en_gb/using-your-yoto-as-a-bluetooth-speaker-BJzxKYimzl) | 明列 Player 3rd / Mini 2024；进入/退出；2026-09-10。 |
| E-YOD-15 | [连接耳机](https://support.yotoplay.com/en_gb/using-headphones-with-your-yoto-player-H1XeYYomMe) | 3rd Generation and earlier Mini；取消/重连；2026-10-13。 |
| E-YOD-16 | [下载/播放排错](https://support.yotoplay.com/en_gb/download-and-playback-troubleshooting-SJfjOYimGg) | 状态、重启、预览；2026-08-12。 |
| E-YOD-17 | [Mini 读卡排错](https://support.yotoplay.com/en_us/card-reading-issues-on-yoto-mini-HJPliRiXzl) | 多卡、NFC、Bluetooth Pairing；2026-07-08；批次未逐项标明。 |
| E-YOD-18 | [Club Collection 条件](https://support.yotoplay.com/en_gb/browse-the-yoto-club-card-collection-S1mTOKomzx) | MYO、结束会员、家庭范围；2026-07-17。 |
| E-YOD-19 | [app 更新记录](https://support.yotoplay.com/en_gb/yoto-app-updates-BJZeKFsQGg) | 3.34、3.35 beta、3.22 邀请码；页示 2026-07-20。 |
| E-YOD-20 | [OS 更新记录](https://support.yotoplay.com/en_gb/yoto-player-software-%28os%29-updates-ByglYKimze) | 2.23.2/2.23.3 批次；2.22.4 快进；旧播客续播条件。 |

## 2026-10-27目录深化与原件

[逐项目录](../batches/yoto-controls-deepening-inventory.csv)记录7个一级分类、5个本轮相关分类的87条文章（23内容/MYO、30使用、7新用户、19排错、8Club），以及29项App页签/设置，共123条入口。文章标题清点与正文分析分别标记；订单和第四代分类不在本轮展开，不能称整站全部正文已分析。第三代/Mini2024的完整固定版本手册仍未取得。

本轮补6条旧任务（01/02/03/05/07/11）和4个新目标（16–19），现19F。目录中的Groups、交互卡、RSS、Beta、时区及第三方开发者授权等没有闭合任务。右键旧代快捷不扩展为第四代可离线浏览20标题的绿键列表。

当前容量帮助将Mini2024与2021–23合并一行，未给2024单独分行；不能据此断定2024本身存在两种装配。其早期Mini序列规则：序列以5或更小数字开头且不以-H/-H7/-U结尾时列16GB；其他情形列32GB。没有实机序列，故不锁定容量。硬重启、清本机下载、家庭解绑、退出家庭、删除账户是不同层；本轮仅补前四项，不据菜单名推导账户数据恢复。

共享MYO的拥有者与家庭成员身份分开。移除成员不等于已证明收回所有共享链接、清掉离线内容；内容原作者后续编辑会传播到关联卡，具体离线同步/正在播放时切换未说明。官方暂停分享后的复核也只是申请路径，不保证恢复。

来源短摘记见[source-notes](../artifacts/yoto-controls-deepening/source-notes.md)。原件在work/yoto-controls-deepening，逐个URL/采集路径/正文hash定位/SHA记于新证据及旧E追加采集建议；不是页面截图或实操。

| 新E | 官方来源 | 定位 |
| --- | --- | --- |
| E-YTCD-01 | [Cards, Content & Make Your Own：公开帮助目录](https://support.yotoplay.com/en_gb/categories/cards-content-make-your-own-r1CcBmvlMx) | 公开网页内嵌数据，无API请求；分类body虽随HTML取得，仅选定文章实际读正文。；原件定位见evidence |
| E-YTCD-02 | [Using Your Yoto：公开帮助目录](https://support.yotoplay.com/en_gb/categories/using-your-yoto-rkx2SXPlGl) | 公开网页内嵌数据，无API请求；分类body虽随HTML取得，仅选定文章实际读正文。；原件定位见evidence |
| E-YTCD-03 | [New to Yoto：公开帮助目录](https://support.yotoplay.com/en_gb/categories/new-to-yoto-Bk23H7vefl) | 公开网页内嵌数据，无API请求；分类body虽随HTML取得，仅选定文章实际读正文。；原件定位见evidence |
| E-YTCD-04 | [Fixing a Problem & Safety：公开帮助目录](https://support.yotoplay.com/en_gb/categories/fixing-a-problem-safety-HyL6rmvgGg) | 公开网页内嵌数据，无API请求；分类body虽随HTML取得，仅选定文章实际读正文。；原件定位见evidence |
| E-YTCD-05 | [Yoto Club：公开帮助目录](https://support.yotoplay.com/en_gb/categories/yoto-club-Bym2rmvlGx) | 公开网页内嵌数据，无API请求；分类body虽随HTML取得，仅选定文章实际读正文。；原件定位见evidence |
| E-YTCD-06 | [闹钟：条件、保存/取消、启停与删除](https://support.yotoplay.com/en_gb/how-to-set-an-alarm-on-your-yoto-BJIxYtimMl) | 步骤1–18；App>=2.46、OS>=2.10.7；未确认实际构建；原件定位见evidence |
| E-YTCD-07 | [Player夜灯和Mini日夜模式边界](https://support.yotoplay.com/en_gb/using-your-yoto-as-a-nightlight-r1BYtjXfg) | 通用翻面/亮度/颜色；3rd Gen传感器；排除4th Gen段；原件定位见evidence |
| E-YTCD-08 | [家庭成员可控制全部登记设备](https://support.yotoplay.com/en_gb/family-accounts-HJq3OKomzx) | Family Account lets anyone who is a member；原件定位见evidence |
| E-YTCD-09 | [离开家庭或移除其他成员](https://support.yotoplay.com/en_gb/how-to-remove-a-user-from-your-family-account-H162uFjmfl) | 每位成员及Settings用户三点菜单；原件定位见evidence |
| E-YTCD-10 | [接收共享MYO及原作者所有权](https://support.yotoplay.com/en_gb/safe-sharing-with-friends-and-family-S1vytKsmMg) | Receiving / Giving or receiving / Reactivating；未核发起分享菜单；原件定位见evidence |
| E-YTCD-11 | [恢复出厂与家庭解绑分开](https://support.yotoplay.com/en_gb/how-to-factory-reset-your-yoto-player-or-mini-ByE1FKsXzl) | Player / Mini重置步骤及Remove device / Leave family；原件定位见evidence |
| E-YTCD-12 | [本机容量、缓存淘汰与恢复下载](https://support.yotoplay.com/en_gb/how-much-content-can-my-yoto-store-for-offline-listening-SJNFKiQGe) | 型号表；Mini序列规则；When storage is full；不采用小时估算作保证；原件定位见evidence |
| E-YTCD-13 | [自建MYO列表的容量限制](https://support.yotoplay.com/en_gb/how-much-audio-fits-on-a-make-your-own-card-r1ssdFimfl) | 100 tracks、单条/整卡限制；不套用已购卡关联；原件定位见evidence |
| E-YTCD-14 | [网络状态图标与下载前提](https://support.yotoplay.com/en_gb/how-to-check-if-your-yoto-player-is-online-rJ_A_Ki7Mx) | Online/offline；green/white Yoto Face；图仅读alt/正文，未看图；原件定位见evidence |

E-YOD-01/02/19/20、E-YOT-01/02/03沿用编号；本轮直接读到公开HTML中的正文，追加采集已合回原E，原取得方式保留。附图403、无图像观察；全部hands_on=false。只改研究记录，无设备、账号、播放、家庭或订阅状态变化。
