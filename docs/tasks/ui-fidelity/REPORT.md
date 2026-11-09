# UI 还原度报告

基线：`62b0e83da1d34f50629b5e576807db1381b26078`。仅本地提交，未推送；仅在模拟器验证。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】全量前后分数完成 | 177 张图中 145 个已有整屏页面，逐页见下表；9 张没有对应页面，23 张为组件或规范图 | T1、T2 |
| 2 | 【实测】已修两页的三个画板，均改善；仍超过 12，逐页说明例外 | Settings 81.175→76.744，大字设置 85.418→80.657，FactoryReset 51.608→51.346；前后图在忽略的证据目录 | T2 |
| 3 | 【实测】普通及内存全量通过 | 普通 276/276（296.89 秒）；ASan/UBSan 276/276（657.87 秒），均零失败、零跳过；未放宽预算 | T3 |

## 全量分数表

【实测，模拟器】单位 /255，指标完全沿用 `tools/ui_compare.py` 的 1/4 灰度 MAE，没有改阈值、裁图、校色或重采样规则。所有页采用同一 Plus 档案、现行字体和主题。按修前分数从高到低排列。数字链接打开设计／实图／绝对差拼图；每页原始截图、来源 SHA-256、完整来源指标及场景校验证据均保留在 `build/ui-fidelity/before/`，后测放在 `after/`。所有场景已在修后重跑。

【实测】除两处修复外，139 屏截图逐字节相同；`FilePicker`、`TransferUsbFail` 的小幅变化只在状态栏，`Transfer` 的变化位于临时传输地址与二维码区域。五套独立视觉场景均再次通过；未将动态数据变化算作修复成果。

四张压力画板按实际页面对应，不伪造场景计数：`StressFiles` → `FileList`，`StressLarge` → 最大字号 `Settings`，`StressNotes` → `Notifications`，`StressThread` → 长短信 `SmsThread`。工具仍验证实际页面的原始计数；只将相应原画板以实际页面名提供给工具，像素不改。长名和分组示例未完全复现，逐页注明；这不是压力行为验收。

规范 §1 已明确：画板仍是旧尺寸，现行规格统一放大 1.2 倍；旧浅灰改为 #888888、次要文字纯黑。任务要求规范优先，不能把主题缩回去争取分数。下列原因是逐页未达项，并非像素目标通过。

| 页面 | 修前 | 修后 | 差异及未达原因 |
| --- | ---: | ---: | --- |
| LockImage | [119.158](../../../build/ui-fidelity/before/LockImage.png) | [119.158](../../../build/ui-fidelity/after/LockImage.png) | 真实壁纸／合成相机和照片像素与示例图不同；保留现行尺寸、深灰。 |
| Camera | [110.143](../../../build/ui-fidelity/before/Camera.png) | [110.143](../../../build/ui-fidelity/after/Camera.png) | 真实壁纸／合成相机和照片像素与示例图不同；保留现行尺寸、深灰。 |
| SmsThread | [100.430](../../../build/ui-fidelity/before/SmsThread.png) | [100.430](../../../build/ui-fidelity/after/SmsThread.png) | 真实合成长短信、会话数和输入框内容与示例不同；现行行高、气泡及深灰。 |
| StressThread | [94.687](../../../build/ui-fidelity/before/StressThread.png) | [94.687](../../../build/ui-fidelity/after/StressThread.png) | 对应实际长短信 SmsThread；合成长消息、会话和气泡分页与画板示例不同，保留现行尺寸和深灰。 |
| StressLarge | [85.418](../../../build/ui-fidelity/before/StressLarge.png) | [80.657](../../../build/ui-fidelity/after/StressLarge.png) | 对应 Settings 最大字号；仍是单行分组并保留家人卡，未做画板的两行重排；现行深灰与旧图也不同。 |
| Photo | [81.205](../../../build/ui-fidelity/before/Photo.png) | [81.205](../../../build/ui-fidelity/after/Photo.png) | 真实壁纸／合成相机和照片像素与示例图不同；保留现行尺寸、深灰。 |
| Settings | [81.175](../../../build/ui-fidelity/before/Settings.png) | [76.744](../../../build/ui-fidelity/after/Settings.png) | 已补图标底圆和分隔线；现行放大后的分组面积及深灰仍与旧图不同，当前家人数和网络状态不同。 |
| Remote | [79.606](../../../build/ui-fidelity/before/Remote.png) | [79.606](../../../build/ui-fidelity/after/Remote.png) | 实际合成网页内容、双页区域与画板示例网页不同；保留网页能力和现行规格。 |
| Gallery | [73.001](../../../build/ui-fidelity/before/Gallery.png) | [73.001](../../../build/ui-fidelity/after/Gallery.png) | 真实壁纸／合成相机和照片像素与示例图不同；保留现行尺寸、深灰。 |
| MainInCall | [72.390](../../../build/ui-fidelity/before/MainInCall.png) | [72.390](../../../build/ui-fidelity/after/MainInCall.png) | 活动卡来自实际播放／通话／作业；保留状态、成员隔离及现行字号、图标、深灰。 |
| Wallpaper | [61.845](../../../build/ui-fidelity/before/Wallpaper.png) | [61.845](../../../build/ui-fidelity/after/Wallpaper.png) | 真实壁纸／合成相机和照片像素与示例图不同；保留现行尺寸、深灰。 |
| Files | [58.807](../../../build/ui-fidelity/before/Files.png) | [58.807](../../../build/ui-fidelity/after/Files.png) | 实际目录、文件名、缩略图、分页与示例不同；现行行高、图标、深灰。 |
| Records | [57.446](../../../build/ui-fidelity/before/Records.png) | [57.446](../../../build/ui-fidelity/after/Records.png) | 现行字号、行高、分组深灰及实际服务值；内容按原有分页显示，不能缩回旧图。 |
| SetDisplay | [55.491](../../../build/ui-fidelity/before/SetDisplay.png) | [55.491](../../../build/ui-fidelity/after/SetDisplay.png) | 现行字号、行高、分组深灰及实际服务值；内容按原有分页显示，不能缩回旧图。 |
| MemoUndo | [55.471](../../../build/ui-fidelity/before/MemoUndo.png) | [55.471](../../../build/ui-fidelity/after/MemoUndo.png) | 实际备忘、长文／清单和失败／撤销状态与示例不同；现行字号、分页和深灰。 |
| Main | [54.194](../../../build/ui-fidelity/before/Main.png) | [54.194](../../../build/ui-fidelity/after/Main.png) | 现行字号、图标、深灰与旧图不同；应用目录、阅读进度和服务卡片是实际成员数据。 |
| PhoneRecents | [52.382](../../../build/ui-fidelity/before/PhoneRecents.png) | [52.382](../../../build/ui-fidelity/after/PhoneRecents.png) | 并行任务边界，本任务未改；实际声音／闹钟／通话与示例不同，且现行尺寸已放大。 |
| PhoneContacts | [52.030](../../../build/ui-fidelity/before/PhoneContacts.png) | [52.030](../../../build/ui-fidelity/after/PhoneContacts.png) | 并行任务边界，本任务未改；实际声音／闹钟／通话与示例不同，且现行尺寸已放大。 |
| KidHome | [51.766](../../../build/ui-fidelity/before/KidHome.png) | [51.766](../../../build/ui-fidelity/after/KidHome.png) | 现行字号、图标、深灰与旧图不同；应用目录、阅读进度和服务卡片是实际成员数据。 |
| FactoryReset | [51.608](../../../build/ui-fidelity/before/FactoryReset.png) | [51.346](../../../build/ui-fidelity/after/FactoryReset.png) | 已去装饰图标描边、开关白色方底并补分隔线；旧浅灰、旧行距和示例备份时间仍不同。 |
| MainPlaying | [51.465](../../../build/ui-fidelity/before/MainPlaying.png) | [51.465](../../../build/ui-fidelity/after/MainPlaying.png) | 活动卡来自实际播放／通话／作业；保留状态、成员隔离及现行字号、图标、深灰。 |
| SetStorage | [51.174](../../../build/ui-fidelity/before/SetStorage.png) | [51.174](../../../build/ui-fidelity/after/SetStorage.png) | 现行字号、行高、分组深灰及实际服务值；内容按原有分页显示，不能缩回旧图。 |
| LockPlaying | [50.837](../../../build/ui-fidelity/before/LockPlaying.png) | [50.837](../../../build/ui-fidelity/after/LockPlaying.png) | 活动卡来自实际播放／通话／作业；保留状态、成员隔离及现行字号、图标、深灰。 |
| ControlPlaying | [50.215](../../../build/ui-fidelity/before/ControlPlaying.png) | [50.215](../../../build/ui-fidelity/after/ControlPlaying.png) | 活动卡来自实际播放／通话／作业；保留状态、成员隔离及现行字号、图标、深灰。 |
| SetKid | [49.974](../../../build/ui-fidelity/before/SetKid.png) | [49.974](../../../build/ui-fidelity/after/SetKid.png) | 现行字号、行高、分组深灰及实际服务值；内容按原有分页显示，不能缩回旧图。 |
| StressFiles | [49.413](../../../build/ui-fidelity/before/StressFiles.png) | [49.413](../../../build/ui-fidelity/after/StressFiles.png) | 对应 FileList；使用实际文件夹具，未替换成画板的示例长文件名；现行行高、图标和深灰。 |
| SetMembers | [49.042](../../../build/ui-fidelity/before/SetMembers.png) | [49.042](../../../build/ui-fidelity/after/SetMembers.png) | 现行字号、行高、分组深灰及实际服务值；内容按原有分页显示，不能缩回旧图。 |
| FileList | [48.865](../../../build/ui-fidelity/before/FileList.png) | [48.865](../../../build/ui-fidelity/after/FileList.png) | 实际目录、文件名、缩略图、分页与示例不同；现行行高、图标、深灰。 |
| Today | [48.401](../../../build/ui-fidelity/before/Today.png) | [48.401](../../../build/ui-fidelity/after/Today.png) | 现行字号、图标、深灰与旧图不同；应用目录、阅读进度和服务卡片是实际成员数据。 |
| BootClaim | [48.138](../../../build/ui-fidelity/before/BootClaim.png) | [48.138](../../../build/ui-fidelity/after/BootClaim.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| SetAbout | [47.930](../../../build/ui-fidelity/before/SetAbout.png) | [47.930](../../../build/ui-fidelity/after/SetAbout.png) | 现行字号、行高、分组深灰及实际服务值；内容按原有分页显示，不能缩回旧图。 |
| KidToday | [47.332](../../../build/ui-fidelity/before/KidToday.png) | [47.332](../../../build/ui-fidelity/after/KidToday.png) | 现行字号、图标、深灰与旧图不同；应用目录、阅读进度和服务卡片是实际成员数据。 |
| AiOffline | [46.807](../../../build/ui-fidelity/before/AiOffline.png) | [46.807](../../../build/ui-fidelity/after/AiOffline.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| IconMenu | [46.353](../../../build/ui-fidelity/before/IconMenu.png) | [46.353](../../../build/ui-fidelity/after/IconMenu.png) | 现行字号、图标、深灰与旧图不同；应用目录、阅读进度和服务卡片是实际成员数据。 |
| AiBudget | [46.222](../../../build/ui-fidelity/before/AiBudget.png) | [46.222](../../../build/ui-fidelity/after/AiBudget.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| Memo | [46.208](../../../build/ui-fidelity/before/Memo.png) | [46.208](../../../build/ui-fidelity/after/Memo.png) | 实际备忘、长文／清单和失败／撤销状态与示例不同；现行字号、分页和深灰。 |
| SetSound | [46.001](../../../build/ui-fidelity/before/SetSound.png) | [46.001](../../../build/ui-fidelity/after/SetSound.png) | 并行任务边界，本任务未改；实际声音／闹钟／通话与示例不同，且现行尺寸已放大。 |
| TodayEdit | [45.104](../../../build/ui-fidelity/before/TodayEdit.png) | [45.104](../../../build/ui-fidelity/after/TodayEdit.png) | 现行字号、图标、深灰与旧图不同；应用目录、阅读进度和服务卡片是实际成员数据。 |
| AiTimeout | [44.719](../../../build/ui-fidelity/before/AiTimeout.png) | [44.719](../../../build/ui-fidelity/after/AiTimeout.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| StressConfirm | [43.802](../../../build/ui-fidelity/before/StressConfirm.png) | [43.802](../../../build/ui-fidelity/after/StressConfirm.png) | 真实确认事务、参数、提示和底页与示例不同；保留现行不透明面板及字号、按钮尺寸。 |
| Transfer | [43.755](../../../build/ui-fidelity/before/Transfer.png) | [43.687](../../../build/ui-fidelity/after/Transfer.png) | 实际授权、网址、传输和 USB 状态与示例不同；保留现行字号、控件与二维码。 |
| KidControl | [43.693](../../../build/ui-fidelity/before/KidControl.png) | [43.693](../../../build/ui-fidelity/after/KidControl.png) | 实际连接、通知和儿童允许项与示例不同；保留现行字号、控件和深灰。 |
| Clock | [43.577](../../../build/ui-fidelity/before/Clock.png) | [43.577](../../../build/ui-fidelity/after/Clock.png) | 并行任务边界，本任务未改；实际声音／闹钟／通话与示例不同，且现行尺寸已放大。 |
| Apps | [43.119](../../../build/ui-fidelity/before/Apps.png) | [43.119](../../../build/ui-fidelity/after/Apps.png) | 现行字号、图标、深灰与旧图不同；应用目录、阅读进度和服务卡片是实际成员数据。 |
| ConfirmSecret | [42.954](../../../build/ui-fidelity/before/ConfirmSecret.png) | [42.954](../../../build/ui-fidelity/after/ConfirmSecret.png) | 真实确认事务、参数、提示和底页与示例不同；保留现行不透明面板及字号、按钮尺寸。 |
| ImeLong | [42.267](../../../build/ui-fidelity/before/ImeLong.png) | [42.267](../../../build/ui-fidelity/after/ImeLong.png) | 真实组合串、候选、选区或长文与示例不同；现行字号、键帽和输入区布局。 |
| Ai | [42.209](../../../build/ui-fidelity/before/Ai.png) | [42.209](../../../build/ui-fidelity/after/Ai.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| LockPage | [42.093](../../../build/ui-fidelity/before/LockPage.png) | [42.093](../../../build/ui-fidelity/after/LockPage.png) | 实际锁屏底页／时钟内容与示例不同；现行字形、锁标记和深灰。 |
| Control | [41.063](../../../build/ui-fidelity/before/Control.png) | [41.063](../../../build/ui-fidelity/after/Control.png) | 实际连接、通知和儿童允许项与示例不同；保留现行字号、控件和深灰。 |
| SetVoice | [41.051](../../../build/ui-fidelity/before/SetVoice.png) | [41.051](../../../build/ui-fidelity/after/SetVoice.png) | 现行字号、行高、分组深灰及实际服务值；内容按原有分页显示，不能缩回旧图。 |
| ConfirmDelete | [40.930](../../../build/ui-fidelity/before/ConfirmDelete.png) | [40.930](../../../build/ui-fidelity/after/ConfirmDelete.png) | 真实确认事务、参数、提示和底页与示例不同；保留现行不透明面板及字号、按钮尺寸。 |
| RemoteAddress | [40.928](../../../build/ui-fidelity/before/RemoteAddress.png) | [40.928](../../../build/ui-fidelity/after/RemoteAddress.png) | 地址、连接／断网状态和键盘内容取自模拟服务；现行字号与控件大于旧画板。 |
| Search | [40.653](../../../build/ui-fidelity/before/Search.png) | [40.653](../../../build/ui-fidelity/after/Search.png) | 实际成员可见的搜索结果／空状态与示例不同；现行字号、输入框和深灰。 |
| FirstBoot | [40.547](../../../build/ui-fidelity/before/FirstBoot.png) | [40.547](../../../build/ui-fidelity/after/FirstBoot.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| ConfirmWeb | [39.942](../../../build/ui-fidelity/before/ConfirmWeb.png) | [39.942](../../../build/ui-fidelity/after/ConfirmWeb.png) | 真实确认事务、参数、提示和底页与示例不同；保留现行不透明面板及字号、按钮尺寸。 |
| AiLong | [39.741](../../../build/ui-fidelity/before/AiLong.png) | [39.741](../../../build/ui-fidelity/after/AiLong.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| RemoteType | [39.386](../../../build/ui-fidelity/before/RemoteType.png) | [39.386](../../../build/ui-fidelity/after/RemoteType.png) | 地址、连接／断网状态和键盘内容取自模拟服务；现行字号与控件大于旧画板。 |
| KidApps | [39.221](../../../build/ui-fidelity/before/KidApps.png) | [39.221](../../../build/ui-fidelity/after/KidApps.png) | 现行字号、图标、深灰与旧图不同；应用目录、阅读进度和服务卡片是实际成员数据。 |
| ImePinyin | [39.031](../../../build/ui-fidelity/before/ImePinyin.png) | [39.031](../../../build/ui-fidelity/after/ImePinyin.png) | 真实组合串、候选、选区或长文与示例不同；现行字号、键帽和输入区布局。 |
| AiSpeak | [38.567](../../../build/ui-fidelity/before/AiSpeak.png) | [38.567](../../../build/ui-fidelity/after/AiSpeak.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| RestoreDone | [38.558](../../../build/ui-fidelity/before/RestoreDone.png) | [38.558](../../../build/ui-fidelity/after/RestoreDone.png) | 实际备份时间、预览和执行结果与示例不同；保留现行状态图标、字号及主按钮。 |
| AiListen | [38.008](../../../build/ui-fidelity/before/AiListen.png) | [38.008](../../../build/ui-fidelity/after/AiListen.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| AiThink | [37.972](../../../build/ui-fidelity/before/AiThink.png) | [37.972](../../../build/ui-fidelity/after/AiThink.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| Confirm | [37.809](../../../build/ui-fidelity/before/Confirm.png) | [37.809](../../../build/ui-fidelity/after/Confirm.png) | 真实确认事务、参数、提示和底页与示例不同；保留现行不透明面板及字号、按钮尺寸。 |
| Calendar | [37.069](../../../build/ui-fidelity/before/Calendar.png) | [37.069](../../../build/ui-fidelity/after/Calendar.png) | 实际日期、提醒和日程与示例不同；现行标题、网格字号和深灰。 |
| Restore | [37.023](../../../build/ui-fidelity/before/Restore.png) | [37.023](../../../build/ui-fidelity/after/Restore.png) | 实际备份时间、预览和执行结果与示例不同；保留现行状态图标、字号及主按钮。 |
| ImeEdit | [36.919](../../../build/ui-fidelity/before/ImeEdit.png) | [36.919](../../../build/ui-fidelity/after/ImeEdit.png) | 真实组合串、候选、选区或长文与示例不同；现行字号、键帽和输入区布局。 |
| SmsCompose | [36.807](../../../build/ui-fidelity/before/SmsCompose.png) | [36.807](../../../build/ui-fidelity/after/SmsCompose.png) | 真实合成长短信、会话数和输入框内容与示例不同；现行行高、气泡及深灰。 |
| ConfirmExpired | [36.402](../../../build/ui-fidelity/before/ConfirmExpired.png) | [36.402](../../../build/ui-fidelity/after/ConfirmExpired.png) | 真实确认事务、参数、提示和底页与示例不同；保留现行不透明面板及字号、按钮尺寸。 |
| SetNetwork | [36.303](../../../build/ui-fidelity/before/SetNetwork.png) | [36.303](../../../build/ui-fidelity/after/SetNetwork.png) | 实际连接、通知和儿童允许项与示例不同；保留现行字号、控件和深灰。 |
| SetPrivacy | [36.130](../../../build/ui-fidelity/before/SetPrivacy.png) | [36.130](../../../build/ui-fidelity/after/SetPrivacy.png) | 现行字号、行高、分组深灰及实际服务值；内容按原有分页显示，不能缩回旧图。 |
| MemoSaveFail | [35.804](../../../build/ui-fidelity/before/MemoSaveFail.png) | [35.804](../../../build/ui-fidelity/after/MemoSaveFail.png) | 实际备忘、长文／清单和失败／撤销状态与示例不同；现行字号、分页和深灰。 |
| ConfirmSetting | [35.631](../../../build/ui-fidelity/before/ConfirmSetting.png) | [35.631](../../../build/ui-fidelity/after/ConfirmSetting.png) | 真实确认事务、参数、提示和底页与示例不同；保留现行不透明面板及字号、按钮尺寸。 |
| ImeSymbols | [35.492](../../../build/ui-fidelity/before/ImeSymbols.png) | [35.492](../../../build/ui-fidelity/after/ImeSymbols.png) | 真实组合串、候选、选区或长文与示例不同；现行字号、键帽和输入区布局。 |
| ClockRing | [35.467](../../../build/ui-fidelity/before/ClockRing.png) | [35.467](../../../build/ui-fidelity/after/ClockRing.png) | 并行任务边界，本任务未改；实际声音／闹钟／通话与示例不同，且现行尺寸已放大。 |
| ConfirmCall | [35.202](../../../build/ui-fidelity/before/ConfirmCall.png) | [35.202](../../../build/ui-fidelity/after/ConfirmCall.png) | 真实确认事务、参数、提示和底页与示例不同；保留现行不透明面板及字号、按钮尺寸。 |
| AiListenTap | [34.951](../../../build/ui-fidelity/before/AiListenTap.png) | [34.951](../../../build/ui-fidelity/after/AiListenTap.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| AiAsk | [34.627](../../../build/ui-fidelity/before/AiAsk.png) | [34.627](../../../build/ui-fidelity/after/AiAsk.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| Update | [34.514](../../../build/ui-fidelity/before/Update.png) | [34.514](../../../build/ui-fidelity/after/Update.png) | 实际固件、进度、阻塞／回退状态与示例不同；保留现行状态图标、字号和按钮尺寸。 |
| AiLog | [34.283](../../../build/ui-fidelity/before/AiLog.png) | [34.283](../../../build/ui-fidelity/after/AiLog.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| BootHello | [34.201](../../../build/ui-fidelity/before/BootHello.png) | [34.201](../../../build/ui-fidelity/after/BootHello.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| RemoteOffline | [33.684](../../../build/ui-fidelity/before/RemoteOffline.png) | [33.684](../../../build/ui-fidelity/after/RemoteOffline.png) | 地址、连接／断网状态和键盘内容取自模拟服务；现行字号与控件大于旧画板。 |
| ImeVoice | [33.632](../../../build/ui-fidelity/before/ImeVoice.png) | [33.632](../../../build/ui-fidelity/after/ImeVoice.png) | 真实组合串、候选、选区或长文与示例不同；现行字号、键帽和输入区布局。 |
| ConfirmParent | [33.225](../../../build/ui-fidelity/before/ConfirmParent.png) | [33.225](../../../build/ui-fidelity/after/ConfirmParent.png) | 真实确认事务、参数、提示和底页与示例不同；保留现行不透明面板及字号、按钮尺寸。 |
| Phone | [32.474](../../../build/ui-fidelity/before/Phone.png) | [32.474](../../../build/ui-fidelity/after/Phone.png) | 并行任务边界，本任务未改；实际声音／闹钟／通话与示例不同，且现行尺寸已放大。 |
| Sms | [32.312](../../../build/ui-fidelity/before/Sms.png) | [32.312](../../../build/ui-fidelity/after/Sms.png) | 真实合成长短信、会话数和输入框内容与示例不同；现行行高、气泡及深灰。 |
| ImePassword | [32.303](../../../build/ui-fidelity/before/ImePassword.png) | [32.303](../../../build/ui-fidelity/after/ImePassword.png) | 真实组合串、候选、选区或长文与示例不同；现行字号、键帽和输入区布局。 |
| Tasks | [32.283](../../../build/ui-fidelity/before/Tasks.png) | [32.283](../../../build/ui-fidelity/after/Tasks.png) | 活动卡来自实际播放／通话／作业；保留状态、成员隔离及现行字号、图标、深灰。 |
| MemoEdit | [32.268](../../../build/ui-fidelity/before/MemoEdit.png) | [32.268](../../../build/ui-fidelity/after/MemoEdit.png) | 实际备忘、长文／清单和失败／撤销状态与示例不同；现行字号、分页和深灰。 |
| ImeCandidates | [32.238](../../../build/ui-fidelity/before/ImeCandidates.png) | [32.238](../../../build/ui-fidelity/after/ImeCandidates.png) | 真实组合串、候选、选区或长文与示例不同；现行字号、键帽和输入区布局。 |
| BootRevoked | [31.483](../../../build/ui-fidelity/before/BootRevoked.png) | [31.483](../../../build/ui-fidelity/after/BootRevoked.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| AiHome | [31.105](../../../build/ui-fidelity/before/AiHome.png) | [31.105](../../../build/ui-fidelity/after/AiHome.png) | 实际问题、答案、错误及执行记录与示例不同；保留现行字号、面板比例与深灰。 |
| Upcoming | [30.835](../../../build/ui-fidelity/before/Upcoming.png) | [30.835](../../../build/ui-fidelity/after/Upcoming.png) | 实际日期、提醒和日程与示例不同；现行标题、网格字号和深灰。 |
| PinForgot | [30.379](../../../build/ui-fidelity/before/PinForgot.png) | [30.379](../../../build/ui-fidelity/after/PinForgot.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| Backup | [30.135](../../../build/ui-fidelity/before/Backup.png) | [30.135](../../../build/ui-fidelity/after/Backup.png) | 实际备份时间、预览和执行结果与示例不同；保留现行状态图标、字号及主按钮。 |
| PinSet | [29.473](../../../build/ui-fidelity/before/PinSet.png) | [29.473](../../../build/ui-fidelity/after/PinSet.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| UpdateReady | [29.362](../../../build/ui-fidelity/before/UpdateReady.png) | [29.362](../../../build/ui-fidelity/after/UpdateReady.png) | 实际固件、进度、阻塞／回退状态与示例不同；保留现行状态图标、字号和按钮尺寸。 |
| BootWifiPass | [29.293](../../../build/ui-fidelity/before/BootWifiPass.png) | [29.293](../../../build/ui-fidelity/after/BootWifiPass.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| MemoDetail | [28.821](../../../build/ui-fidelity/before/MemoDetail.png) | [28.821](../../../build/ui-fidelity/after/MemoDetail.png) | 实际备忘、长文／清单和失败／撤销状态与示例不同；现行字号、分页和深灰。 |
| FilePicker | [28.415](../../../build/ui-fidelity/before/FilePicker.png) | [28.414](../../../build/ui-fidelity/after/FilePicker.png) | 实际目录、文件名、缩略图、分页与示例不同；现行行高、图标、深灰。 |
| AiMemory | [28.311](../../../build/ui-fidelity/before/AiMemory.png) | [28.311](../../../build/ui-fidelity/after/AiMemory.png) | 现行字号、行高、分组深灰及实际服务值；内容按原有分页显示，不能缩回旧图。 |
| MemberSwitch | [28.298](../../../build/ui-fidelity/before/MemberSwitch.png) | [28.298](../../../build/ui-fidelity/after/MemberSwitch.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| Pin | [28.178](../../../build/ui-fidelity/before/Pin.png) | [28.178](../../../build/ui-fidelity/after/Pin.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| TransferLive | [27.892](../../../build/ui-fidelity/before/TransferLive.png) | [27.892](../../../build/ui-fidelity/after/TransferLive.png) | 实际授权、网址、传输和 USB 状态与示例不同；保留现行字号、控件与二维码。 |
| PowerMenu | [27.871](../../../build/ui-fidelity/before/PowerMenu.png) | [27.871](../../../build/ui-fidelity/after/PowerMenu.png) | 现行字号、状态图标和按钮尺寸；实际电量、书签及关机保存状态与示例不同。 |
| RemoteBlocked | [27.683](../../../build/ui-fidelity/before/RemoteBlocked.png) | [27.683](../../../build/ui-fidelity/after/RemoteBlocked.png) | 地址、连接／断网状态和键盘内容取自模拟服务；现行字号与控件大于旧画板。 |
| FactoryDone | [27.647](../../../build/ui-fidelity/before/FactoryDone.png) | [27.647](../../../build/ui-fidelity/after/FactoryDone.png) | 现行状态图标、字号和按钮更大；不能照抄画板未经证实的服务器离线／移除结论。 |
| PinWait | [27.370](../../../build/ui-fidelity/before/PinWait.png) | [27.370](../../../build/ui-fidelity/after/PinWait.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| FileActions | [27.000](../../../build/ui-fidelity/before/FileActions.png) | [27.000](../../../build/ui-fidelity/after/FileActions.png) | 实际目录、文件名、缩略图、分页与示例不同；现行行高、图标、深灰。 |
| TransferUsbBusy | [26.949](../../../build/ui-fidelity/before/TransferUsbBusy.png) | [26.949](../../../build/ui-fidelity/after/TransferUsbBusy.png) | 实际授权、网址、传输和 USB 状态与示例不同；保留现行字号、控件与二维码。 |
| TransferUsbFail | [26.798](../../../build/ui-fidelity/before/TransferUsbFail.png) | [26.802](../../../build/ui-fidelity/after/TransferUsbFail.png) | 实际授权、网址、传输和 USB 状态与示例不同；保留现行字号、控件与二维码。 |
| TransferUsbOn | [26.663](../../../build/ui-fidelity/before/TransferUsbOn.png) | [26.663](../../../build/ui-fidelity/after/TransferUsbOn.png) | 实际授权、网址、传输和 USB 状态与示例不同；保留现行字号、控件与二维码。 |
| PinWrong | [26.593](../../../build/ui-fidelity/before/PinWrong.png) | [26.593](../../../build/ui-fidelity/after/PinWrong.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| Members | [26.504](../../../build/ui-fidelity/before/Members.png) | [26.504](../../../build/ui-fidelity/after/Members.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| BatteryLow | [26.484](../../../build/ui-fidelity/before/BatteryLow.png) | [26.484](../../../build/ui-fidelity/after/BatteryLow.png) | 现行字号、状态图标和按钮尺寸；实际电量、书签及关机保存状态与示例不同。 |
| FileMove | [26.450](../../../build/ui-fidelity/before/FileMove.png) | [26.450](../../../build/ui-fidelity/after/FileMove.png) | 实际目录、文件名、缩略图、分页与示例不同；现行行高、图标、深灰。 |
| StressNotes | [26.232](../../../build/ui-fidelity/before/StressNotes.png) | [26.232](../../../build/ui-fidelity/after/StressNotes.png) | 对应 Notifications；已有合成通知与画板的长标题和分组数不同，未宣称复现同一压力数据。 |
| PhoneCall | [25.851](../../../build/ui-fidelity/before/PhoneCall.png) | [25.851](../../../build/ui-fidelity/after/PhoneCall.png) | 并行任务边界，本任务未改；实际声音／闹钟／通话与示例不同，且现行尺寸已放大。 |
| BackupFail | [25.495](../../../build/ui-fidelity/before/BackupFail.png) | [25.495](../../../build/ui-fidelity/after/BackupFail.png) | 实际备份时间、预览和执行结果与示例不同；保留现行状态图标、字号及主按钮。 |
| Lock | [25.402](../../../build/ui-fidelity/before/Lock.png) | [25.402](../../../build/ui-fidelity/after/Lock.png) | 实际锁屏底页／时钟内容与示例不同；现行字形、锁标记和深灰。 |
| RestoreFail | [25.358](../../../build/ui-fidelity/before/RestoreFail.png) | [25.358](../../../build/ui-fidelity/after/RestoreFail.png) | 实际备份时间、预览和执行结果与示例不同；保留现行状态图标、字号及主按钮。 |
| MemoChecklist | [25.131](../../../build/ui-fidelity/before/MemoChecklist.png) | [25.131](../../../build/ui-fidelity/after/MemoChecklist.png) | 实际备忘、长文／清单和失败／撤销状态与示例不同；现行字号、分页和深灰。 |
| BootRecovery | [24.983](../../../build/ui-fidelity/before/BootRecovery.png) | [24.983](../../../build/ui-fidelity/after/BootRecovery.png) | 实际固件、进度、阻塞／回退状态与示例不同；保留现行状态图标、字号和按钮尺寸。 |
| UpdateFail | [24.804](../../../build/ui-fidelity/before/UpdateFail.png) | [24.804](../../../build/ui-fidelity/after/UpdateFail.png) | 实际固件、进度、阻塞／回退状态与示例不同；保留现行状态图标、字号和按钮尺寸。 |
| KidTimeUp | [24.608](../../../build/ui-fidelity/before/KidTimeUp.png) | [24.608](../../../build/ui-fidelity/after/KidTimeUp.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| BootNoHub | [24.524](../../../build/ui-fidelity/before/BootNoHub.png) | [24.524](../../../build/ui-fidelity/after/BootNoHub.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| UpdateDone | [24.476](../../../build/ui-fidelity/before/UpdateDone.png) | [24.476](../../../build/ui-fidelity/after/UpdateDone.png) | 实际固件、进度、阻塞／回退状态与示例不同；保留现行状态图标、字号和按钮尺寸。 |
| UpdateBlocked | [24.358](../../../build/ui-fidelity/before/UpdateBlocked.png) | [24.358](../../../build/ui-fidelity/after/UpdateBlocked.png) | 实际固件、进度、阻塞／回退状态与示例不同；保留现行状态图标、字号和按钮尺寸。 |
| Notifications | [24.323](../../../build/ui-fidelity/before/Notifications.png) | [24.323](../../../build/ui-fidelity/after/Notifications.png) | 实际连接、通知和儿童允许项与示例不同；保留现行字号、控件和深灰。 |
| BootConfirm | [24.132](../../../build/ui-fidelity/before/BootConfirm.png) | [24.132](../../../build/ui-fidelity/after/BootConfirm.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| KidLimit | [23.687](../../../build/ui-fidelity/before/KidLimit.png) | [23.687](../../../build/ui-fidelity/after/KidLimit.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| BootRestore | [22.492](../../../build/ui-fidelity/before/BootRestore.png) | [22.492](../../../build/ui-fidelity/after/BootRestore.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| RestoreRun | [22.200](../../../build/ui-fidelity/before/RestoreRun.png) | [22.200](../../../build/ui-fidelity/after/RestoreRun.png) | 实际备份时间、预览和执行结果与示例不同；保留现行状态图标、字号及主按钮。 |
| SearchEmpty | [22.172](../../../build/ui-fidelity/before/SearchEmpty.png) | [22.172](../../../build/ui-fidelity/after/SearchEmpty.png) | 实际成员可见的搜索结果／空状态与示例不同；现行字号、输入框和深灰。 |
| MembersBack | [21.235](../../../build/ui-fidelity/before/MembersBack.png) | [21.235](../../../build/ui-fidelity/after/MembersBack.png) | 实际家人、密码状态和儿童限时与示例不同；现行字距、圆形图标、键盘和按钮尺寸。 |
| BootWifi | [21.189](../../../build/ui-fidelity/before/BootWifi.png) | [21.189](../../../build/ui-fidelity/after/BootWifi.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| BootJoinFail | [20.874](../../../build/ui-fidelity/before/BootJoinFail.png) | [20.874](../../../build/ui-fidelity/after/BootJoinFail.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| PhoneIncoming | [20.422](../../../build/ui-fidelity/before/PhoneIncoming.png) | [20.422](../../../build/ui-fidelity/after/PhoneIncoming.png) | 并行任务边界，本任务未改；实际声音／闹钟／通话与示例不同，且现行尺寸已放大。 |
| UpdateInstall | [20.421](../../../build/ui-fidelity/before/UpdateInstall.png) | [20.421](../../../build/ui-fidelity/after/UpdateInstall.png) | 实际固件、进度、阻塞／回退状态与示例不同；保留现行状态图标、字号和按钮尺寸。 |
| Charging | [18.897](../../../build/ui-fidelity/before/Charging.png) | [18.897](../../../build/ui-fidelity/after/Charging.png) | 现行字号、状态图标和按钮尺寸；实际电量、书签及关机保存状态与示例不同。 |
| RemoteLoading | [18.754](../../../build/ui-fidelity/before/RemoteLoading.png) | [18.754](../../../build/ui-fidelity/after/RemoteLoading.png) | 地址、连接／断网状态和键盘内容取自模拟服务；现行字号与控件大于旧画板。 |
| PowerOffEmpty | [18.235](../../../build/ui-fidelity/before/PowerOffEmpty.png) | [18.235](../../../build/ui-fidelity/after/PowerOffEmpty.png) | 现行字号、状态图标和按钮尺寸；实际电量、书签及关机保存状态与示例不同。 |
| PowerOff | [18.177](../../../build/ui-fidelity/before/PowerOff.png) | [18.177](../../../build/ui-fidelity/after/PowerOff.png) | 现行字号、状态图标和按钮尺寸；实际电量、书签及关机保存状态与示例不同。 |
| BootJoining | [14.998](../../../build/ui-fidelity/before/BootJoining.png) | [14.998](../../../build/ui-fidelity/after/BootJoining.png) | 真实配网、加入码、恢复预览和离线示例状态；现行字号、图标、按钮与旧图不同。 |
| BatterySaving | [12.413](../../../build/ui-fidelity/before/BatterySaving.png) | [12.413](../../../build/ui-fidelity/after/BatterySaving.png) | 现行字号、状态图标和按钮尺寸；实际电量、书签及关机保存状态与示例不同。 |

## 没做到的

- 145 屏最终均未达到 12；按任务允许的例外逐页说明原因，不将像素目标写成通过。不通过修改真实数据、主题或行为迎合旧图。
- 无对应页面的 9 张整屏参考：`ImeAi`；`Import`、`ImportFind`、`ImportJob`、`ImportFail`；`MemoPending`、`MemoRecord`；`SetBluetooth`；`Stepper`。Stepper 是组件设计参考，没有独立页面；没有借这项任务实现新页面。
- 23 张组件／规范展示图不是完整页面，尺寸也不等于 684×1216：

| 画板 | 尺寸 |
| --- | --- |
| ActivityCard | 620×112 |
| AiReceipts | 1448×1360 |
| AppList | 2212×1700 |
| Components | 1448×1320 |
| Foundation | 1448×1400 |
| Group | 628×252 |
| Icon | 48×48 |
| IconSheet | 1448×1216 |
| ImeVoiceStates | 1448×2660 |
| Interaction | 1448×1216 |
| Keyboard | 684×518 |
| Notice | 620×340 |
| PageBar | 320×104 |
| PartsNew | 1448×1320 |
| Receipt | 620×112 |
| SaveMark | 320×34 |
| Seg | 620×88 |
| Slider | 620×96 |
| Stages | 620×384 |
| StateKit | 1448×3000 |
| StatusBar | 684×52 |
| Toggle | 120×84 |
| TopBar | 684×104 |

- 未做 Linux/gcc 和实机验证。窗口版未启动；全量里的 `gui-smoke` 使用既有 SDL dummy 无窗口模式。

## 和设计不一致的

- `StressLarge` 尚未改成画板的两行列表，仍保留当前家人卡、单行分组和既有分页；未宣称这项布局已完成。
- 逐页差异见分数表；规格只认 `docs/product/system-ui-spec.md` §1，不改规范和设计稿。实际服务数据、错误状态和分页不改。
- `FactoryDone` 保留真实表述：备份保留，服务器是否移除设备请在手机核对；不能宣称已验证的离线状态。

## 改过的已有测试

- Settings 增加图标白底圆和行分隔线断言；FactoryReset 增加装饰图标无描边、分隔线、开关无白色方底断言。既有期望未改，撤掉修复分别失败 13／6 条；证据 `build/ui-fidelity/reverse/results.json`。
- CMake 的现有 UI 场景列表新增 `SetVoice`、`SetAbout`、`SetPrivacy`、`ImeSymbols`，默认及最大字号各跑一次；符号页通过真实“符号”按钮进入并检查“字母”返回键。既有场景、断言和预算未删除或放宽。
- 四张压力图增加显式页面映射，原有计数和页面身份仍校验；大字设置修前用基线页面与修前测试入口重新链接、实测成功，没有拿修后截图冒充。
- `tests/ui_fidelity.py` 只收集成功且刷新完成、页面身份正确的已有场景；用原工具比较，保留原指标和截图哈希，不把没有截图的页面默认为通过。

## 要用户定的

- 暂无必须等待的产品决定；按任务继续保留规范 §1。

## 顺手发现

- 默认构建 Python 缺少中枢测试依赖，基线 3 项跳过；只读依赖目录的 `hub/.venv/bin/python` 已有锁定版本 pytest 8.4.2、pytest-asyncio 1.2.0、aiohttp 3.12.15、PyYAML 6.0.3。最终测试使用该解释器，不安装或升级依赖；普通和内存版均零跳过。
- 过程偏差：首次补采基线大字截图的单文件编译和场景运行曾与内存全量短暂重叠；随后在全量结束后串行复跑，最终采用复跑证据。

## 实现范围

- 【实现】产品代码增加 13 行、删除 2 行，共 **15/300**。只改 Settings 的图标底圆和分隔线、FactoryReset 的装饰描边、分隔线和开关方底；`navigation.c` 的改动以 `PAGE_SETTINGS` 限定。没有改 SetSound、时钟、通话页、共享主题、行为、能力或快照内容。
- 【实现】架构审视：仍由既有界面模块绘制，按钮和 AI 继续走同一原子能力；服务边界、成员策略和快照接口均未改变。

## 新增的机制

- 无产品新任务、线程、状态机、格式、配置项、缓存或队列。截图收集脚本仅为本任务验证工具。

## 要改的文档

- 合并主会话可在 `docs/tasks/backlog.md` 记录本表的真实像素差异和规范优先例外；未改任何共享文档。

## 复现命令与证据

环境变量使用任务提供的 `INK_DEPS` 和 `INK_DESIGN`；不在报告写本机绝对路径。

修前复现版本为 `88817c36`：只加测量脚本和截图场景，产品代码与给定基线完全相同（`git diff 62b0e83d..88817c36 -- system apps platform drivers ports` 为空）；修后实现为 `8879f33a`。截图入口使用这两个版本；测量脚本使用本任务最终版本。修前四张压力画板的补充命令见 T4。

T1：先按总规则配置构建，额外选现成中枢测试 Python：

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
for suite in files_app calendar memo camera_gallery transfer_pages; do
  INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" "tests/$suite/run.py" --build build --visual
done
```

T2：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fidelity.py --build build --out build/ui-fidelity/before`。修后将输出目录换为 `build/ui-fidelity/after`；二者的 `scores.json`、`sources.json`、`unmatched.json`、`components.json` 和逐页拼图全部保留，不提交二进制产物。

T3：普通及 ASan/UBSan 全量均通过，未见 sanitizer 错误诊断；内存目录只将 T1 的构建目录换成 `build-asan`，并增加 `-DINK_SANITIZE=ON`。始终 `INK_WERROR=ON`、utility 优先级，完整构建和测试最多 `-j2`，两套完整构建与全量测试按步骤串行。日志及每步退出状态、用时已复制到 `build/ui-fidelity/logs/` 和 `build/ui-fidelity/validation.json`；原日志在 `work/ui-fidelity/`。

T4：压力画板的基线与修后补测（只读取 Git，替换对象和输出均在忽略目录）：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fidelity_reverse.py --build build --capture-large
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fidelity.py --build build --out build/ui-fidelity/before --variants-only --large-source build/ui-fidelity/baseline-large
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fidelity.py --build build --out build/ui-fidelity/after
```

【实测】基线大字设置重新链接后 48 条断言通过；撤掉两页修复的反向结果仍为 13／6 条断言失败。修复页面普通及内存版均零告警，Settings 默认／最大字号内部 RAM 最低余量 51,328 字节，FactoryReset 两种流程 49,280 字节；预算未改。

【实测】作为差异解释，按原算法的 1/4 缩图，仅统计“实图灰值 136、画板灰值 200～245”的重叠区域，对总 MAE 的贡献：Settings 16.271，大字设置 15.781；这部分本身已超过 12。该分析不修改主分数，不把它当作调整布局后分数的数学下界。原数据：`build/ui-fidelity/color-contributions.json`。

结束检查：`git diff --check` 通过；基线至最终提交差异仅任务报告、两处页面装饰、测量与回归测试及 CMake 场景登记。按总规则 §6 的原隐私正则检查，0 命中（grep 退出 1）；证据 `build/ui-fidelity/privacy-hits.txt`。没有提交截图、设计稿、构建产物、实际设备数据或本机绝对路径。未推送。
