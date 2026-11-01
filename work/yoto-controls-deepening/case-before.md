# M-037 Yoto Player 第三代 / Mini 2024：儿童控制、家庭内容与离线数据

查阅：2026-10-26。固定 **Yoto Player（3rd Generation）及 Yoto Mini（2024 Edition）**，伴随 iOS/Android app；不混入第四代绿键、电源短按语义或旧 Mini 充电方案。按官方帮助适用型号引用。无实机固件/app 构建：OS 记录列 2.23.2，另说 2.23.3 仅限特定新生产批次；app 更新页列稳定版 3.34（2026-04-14）和 beta 3.35。这些历史列项不能证明查阅日所有用户运行该版（E-YOD-19、20）。

地区/语言为英文官方帮助，不宣称中国大陆服务可用性。家庭账户、普通持有卡、MYO、自选数字内容和 Club Collection 分开；会员只在相应内容路径构成条件。**D2 文档重建；无设备操作、登录、购买、连续视觉观察或单帧图像观察。** 主要取得官方搜索索引正文；直接打开多为动态壳，索引不是已归档网页截图。所有时间为文档描述，未测响应耗时。

## 功能目录与覆盖

本轮未取得固定版本完整单册。下表以官方 app 总览全部一级页签及硬件/设置/家庭/MYO/离线帮助目录重建；**帮助站文章全集未核定**。目录项只计覆盖范围，不计新流程。

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

保留原有 F-M037-01–03；本轮从 04 追加。“反馈”均是文档陈述。

| 流程 | 操作与反馈 | 状态/失败 | 证据 |
| --- | --- | --- | --- |
| F-M037-01 为外出准备离线故事 | 联网、接电，插卡触发内容准备；取卡，保持开机空闲。app → Settings → 设备 → Audio Download Status 查完成。 | 新内容先联网；电台/播客流和 app 远程发起有联网条件。每台独立按容量/使用缓存；淘汰不移除 app 书库，在线重播可重新下载。 | E-YOT-01 |
| F-M037-02 睡前限时播放 | 播放条 → Maxi Player → 月亮/星星 → 时长，到时停播。 | 换卡/内容不重置；重启、断电、跨设备未知。 | E-YOT-02 |
| F-M037-03 卡顿时判断原因 | 检查弱网络及首次下载队列；接近路由器或等下载完成再播。 | 书库可见与本机下载完成不同；后台下载描述有冲突，见下。 | E-YOT-01、E-YOT-03 |

| 流程 | 功能/深度 | 前置状态 | 操作与文档反馈 | 数据变化/结束 | 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M037-04 | 首次配网与失败恢复 D2 | 本轮两款接电、无卡；app；适用 2.4GHz 网络 | Set Up a Player → 双橙键按 5 秒得六字母码 → app 输码、Join 设备热点 → 码变绿/语音提示 → 选家庭 Wi-Fi 输密码 → 绿 Wi-Fi；Pair My Phone 后重启回时钟。绿 Wi-Fi 脉动表示更新。 | 建网络/家庭关联；更新未完保持连接。热点切换失败可在手机 Wi-Fi 手动加入 Yoto_xxx，码变绿后回 app 等网络列表。 | E-YOD-03、E-YOD-04 | iOS Local Network 拒绝后需在手机设置开启；Android 自动切移动数据可能打断配网。空书库、更新失败后状态未测。 |
| F-M037-05 | 儿童选章、暂停、停止 D2 | Earlier players；插内容卡 | 左旋钮调音量、按上一章；右按下一章、转动选章；播放时左右长按回退/快进，屏显箭头表示加速。电源短按暂停/继续，长按停止；无卡右键可切 Daily、Radio、时钟。 | 当前播放/章节改变；约 3 秒电源开关、10 秒硬重启，后者保留 Wi-Fi/音频。Player 伏面可调夜灯，Mini 不套用。 | E-YOD-01 | 普通“长按停止”与“约 3 秒关机”分界未定义；取卡保存时机、选章起点、误触复原未知。 |
| F-M037-06 | 家长选播放目标与限音 D2 | 家庭设备已登记；改限音时设备开机联网 | Device Picker 先选手机/目标播放器 → Library 播放；Now Playing 进入 Maxi。Settings → 所选设备 → Day/Night Volume Limit 分别设置。 | 控制所选设备；限制 0 可无声、最大档 16；耳机限音另有设置。 | E-YOD-02、E-YOD-05 | 下发失败、儿童同时调音量的冲突未知。日夜限音与无线耳机自有限音不可等同；手机控件不能全推广到设备。 |
| F-M037-07 | MP3→MYO 列表→写卡 D2 | 自有可用音频、MYO；网页/app 同账户 | MYO → New Playlist（或网页 My Cards）→ 建列表、上传、Save；app 列表详情 → … → Link to Card → 插 MYO → Link。红录制图标后绿勾，取出再插播放。 | 卡关联云端列表；本机音频下载是另一状态，离线仍查 F-M037-01。 | E-YOD-06 | 上传取消/断网、重复命名、卡旧关联保留时点、误覆写撤销未知。 |
| F-M037-08 | 录音→列表→更新/删除 D2 | app Create；家庭录音/MYO | Record → 倒计时 → 录音（可暂停/继续）→ Save → My recordings；选录音 Add to a playlist。修改共享列表会更新 app/关联卡。列表 … → Delete → 确认；录音左划 → 红删除。 | 删除后让设备接电、在线、空闲更新卡；录音保存、上传到列表、卡关联是不同状态。 | E-YOD-07、E-YOD-06 | 无已查回收站、误删恢复或原稿导出。编辑时正播行为、删录音与列表副本持久关系未完整说明。 |
| F-M037-09 | 书库移除与重新加入 D2 | Library 已有普通卡 | 卡详情 → … → Remove from the app → Remove；可 Cancel。再在设备播实体卡或手机扫描，可重新加入。 | 书库条目移除/回归；不等于卡失效或立即擦除各机缓存。 | E-YOD-08 | 无实体卡时恢复、删除传播延迟及缓存回收时机未知。 |
| F-M037-10 | 跨家庭借卡与取回 D2 | 普通持有卡；另一家庭 | 对方设备播卡后，条目转到对方书库；自己在自家设备播放/手机扫描，可转回。 | 原家庭条目相应移除；关联已购内容的 MYO 也适用。 | E-YOD-09 | 不套用原创录音或 Club Collection；离线、同时播放、断点的结果未知。 |
| F-M037-11 | 加家庭成员及第二台 D2 | 各用自己凭据；已有家庭 | Settings → Join a Family Account → 设备六字母码 → Join This Family Account；第二台用 Set Up a Player → Pair My Phone；Settings 显示成员/设备。 | 可访问家庭书库、管理设备；一个用户加入新家庭会离开原家庭。 | E-YOD-11、E-YOD-12、E-YOD-13、E-YOD-19 | 3.22 另引入邀请码；旧帮助仍称 Menu/Players，路径采用较新 Settings 帮助，不合并两种代码机制。未证明播放秒点共享。 |
| F-M037-12 | 手机输入：蓝牙音箱 D2 | Player 3rd / Mini 2024；手机 | Settings → 设备 → Advanced → Bluetooth Pairing 打开；左键约 3 秒 → 灰蓝牙 → 右旋钮选音箱并按 → 手机配对 → 确认图标后右键确认。 | 手机音频输出到 Yoto，播放由手机控制；左键约 3 秒退普通插卡模式。 | E-YOD-14 | 开关只允许进模式，不等于已连接；来电/断连后自动退出和卡片断点未知。 |
| F-M037-13 | 丢卡后以 MYO 恢复入口 D2 | 原内容仍在 Library；可用 MYO | 原卡详情 → … → Link to Card → 插 MYO、Link → 红图标转绿勾 → 取出重插。 | 一张已购内容对应一次 MYO 关联；找到原卡后可将 MYO 改连别的内容。 | E-YOD-10 | 不是恢复音频原稿，也不是多本已购卡合成列表；两卡缓存和断点未知。 |
| F-M037-14 | Mini 读卡故障与下载状态分流 D2 | 内容不播放/卡不识别 | 下载排错沿用 F-M037-03；查 Audio Download Status，完成仍异常可按电源 10 秒重启。Mini 多卡均不识别，用手机 NFC/另一台读卡区分卡与设备；Mini 帮助另给关闭 Bluetooth Pairing 后重试分支。 | 新分析对象是读卡器/卡片识别，而非重复登记下载流程；下载完成、读卡成功是不同节点，仍失败转支持。 | E-YOD-16、E-YOD-17、E-YOD-01 | Mini 帮助未逐项列批次，不当成每台必现。商城预览不等于可离线内容；错误动画和修复耗时未观察。 |
| F-M037-15 | 蓝牙耳机输出、取消、重连 D2 | 本轮两款；耳机靠近并配对 | 左键约 3 秒 → 右旋钮选耳机并按；搜索蓝图标闪烁，成功为彩色图标及钟面左上蓝点。取消：左长按回菜单 → 右旋钮选耳机并按 → 左按退出。 | 输出转耳机；不自动重连时重走选择。误配时移开/关闭其它配对设备再试。 | E-YOD-15 | 断连是否自动扬声、忘记旧耳机、音量继承未查；不搬用第四代“忘记耳机”组合键。 |

## 分支、数据状态与证据冲突

- **断点与多设备仍有缺口。** F-M037-05 确认当次暂停/继续，F-M037-11 确认家庭设备/书库可见；均不证明取卡再插、A→B→A、手机→Player、两台设备、重启或断网恢复后继续同一秒点。OS 旧日志的播客恢复有出版方条件，不能扩展为所有卡的当前规则（E-YOD-20）。不为未知能力虚造 D2。
- **下载调度冲突保留。** E-YOT-01 写流式播放与后台下载，E-YOT-03 又称新内容在无卡空闲时下载。型号/固件条件不足；共同路径是外出前接电、空闲并逐台查完成。缓存按设备算，另一台已完成不证明这一台可离线。
- **删除分层。** F-M037-09 是书库条目，08 是录音/列表，07 是卡关联，01 是设备缓存；不能把其中一个宣称为“从所有地方永久删除”。MYO 列表与录音没有已查撤销路径。
- **Club 下载不等于永久授权。** Collection 的 MYO 须会员有效且内容仍在集合；计划结束后手机、设备和 MYO 失去播放权限，可购买该内容或续订恢复；只限登记家庭设备（E-YOD-18）。不套用普通已购卡，纯离线时授权何时重查未知。
- **日夜设置不是单一场景。** 右键快捷、限音、夜灯、闹钟、睡眠计时分别存在，其打断/续播优先级未核定。Mini 不套用 Player 夜灯（E-YOD-02）。
- 旧 Menu/Players/Account 与新 Settings 名称并存；邀请码发布记录与现场六字母码路径并存。保留定位与日期，不把帮助页日期当构建版本。

## 对本项目的意义（推断）与下一步

关联 R-018、H-004、G-002：儿童用卡/旋钮控制当前播放，家长用 app 管内容、目标设备和限制。AI 回答“已准备好出门听”需知道目标设备、内容授权、缓存完成与网络；回答“从上次继续”还需明确用户、卡、设备与时间位置。Plus 可据此提出状态需求；本案例没有验证 Yoto 自然语言控制或 AI 续讲。

最小观察：固定 Player 第三代、Mini 2024 及关于页版本，用普通卡/MYO 记录换卡、取卡、暂停、重启、两机及手机切换、离线缓存淘汰、正播时编辑列表、蓝牙断连；另用可恢复测试内容区分书库移除/录音删除。记录章节与秒点，不只看封面。当前未执行。**本轮只更新研究文档，无账户、设备、订阅或产品实现变更。**

## 证据

旧 E-YOT-01–03 保留；新增 E-YOD 为本轮定位。查阅日均为 2026-10-26；均取得官方搜索索引正文，未形成页面图像/连续流程归档。

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
