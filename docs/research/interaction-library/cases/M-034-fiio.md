# M-034 FiiO JM21 / FiiO Music：曲库、工作模式与音频出口

## 范围和证据边界

查阅日期：2026-10-26；本轮深化2026-10-27。研究装配固定为 **原版JM21 3GB/32GB、Android13及预装FiiO Music**（文档范围，不是实机样本）；手机端 iOS/Android FiiO Music 只在官方明确涉及的遥控、通用功能处引用。JM21 固件和预装 app 构建号未知；官网另列4GB/64GB，二者的续航/电池/可用存储不混用。英文 FAQ 多标注 2024-12-11，完整手册没有精确修订号，并要求以实际产品为准。没有安装 app、连接设备或测量操作延迟。

深度为 **D2 文档重建**。现有15F；本轮新增外部M3U转内部歌单、电脑USB-DAC、AirPlay、歌词、升级及蓝牙音频接收，旧任务补回原号。实际查看手册p29/30/31/33的静态页；无连续观察或实机操作。扫描过滤、完整临时队列、逐书断点和删除确认仍未闭合。

官方FIIO发布页于2026-09-11列出Music **V3.3.4的JM21适配分支**，并说明播放页和均衡器重做。它是可查发布版本，不能当作本案例已安装版本；旧手册图片也不能冒充3.3.4画面。V3.2.8日志列WiFi文件夹传输，但没有当前端到端操作。本轮不执行更新。[E-FIIO-12、E-FICD-13]

## 功能目录与覆盖

2026-10-26 按42页完整手册的目录及章标题复核；下表以功能域归并，页码为手册印刷页。**“目录定位”只表示找到了该章，不表示已重建任务；关联某条 F 也只覆盖该条明确写出的部分。** 下表保留2026-10-26历史基线；其中“未覆盖”不是本轮结论，以下更新表和逐项台账为准。当前固件全部菜单、第三方app及其账户服务仍未核定。

| 功能域 | 手册章节/定位 | 已关联 F / E 与覆盖边界 | 缺口或未覆盖 |
| --- | --- | --- | --- |
| 开关机、实体键、接口与卡槽 | 按键与接口，p5–6 | F-M034-03；E-FIIO-01：播放键与关屏锁键 | 首次开机、卡装卸、实体键长短按全表、同时插入不同出口的行为尚未建流程。 |
| 状态栏、系统截图 | 状态栏/显示页，p7；截图，p36 | 目录定位；E-FIIO-01；无对应 F | 状态图标、通知操作、截图文件管理与返回入口未覆盖。 |
| 五种工作模式 | 入门功能：五种工作模式，p8–9 | F-M034-04；E-FIIO-03：选择/退出模式 | AirPlay、接收模式各自的连接、断开、取消及原播放恢复未闭合。 |
| 电脑传输、存储管理 | 电脑连接，p10 | F-M034-01；E-FIIO-02：电脑访问存储 | 存储权限、满盘、重名、传输取消、文件管理器批量操作未覆盖。 |
| 固件更新 | 固件升级，p11、39–40 | 目录定位；E-FIIO-01；F-M034-09仅涵盖部分故障恢复 | 正常OTA/本地更新、包校验、设置保留与失败回退没有独立完整流程。 |
| 第三方应用与语言 | 应用安装/语言，p12–13；系统语言输入，p27 | 目录定位；E-FIIO-01；无对应 F | 安装/授权/卸载/更新、离线下载与账户退出；系统与Music语言分别设置的任务未覆盖。 |
| USB DAC输入 | USB DAC，p13 | F-M034-04；E-FIIO-03：仅模式入口 | 电脑端设备选择、驱动失败、播放端配置、结束后恢复未覆盖；不能用数字输出流程代替。 |
| Wi-Fi与蓝牙 | Wi-Fi，p16；蓝牙，p14、17–19 | F-M034-05、F-M034-07；E-FIIO-04、E-FIIO-08：配对/回连与Link前提 | 首次Wi-Fi连接、忘记网络、蓝牙接收全链及多出口冲突仍缺。 |
| 系统音频与外部输出 | 音频，p20–23；USB数字输出，p36；同轴，p38 | F-M034-06；E-FIIO-05、E-FIIO-10：USB/PO/LO/SPDIF | 增益、滤波、声道平衡、All To DSD、LO音量、无耳机暂停未逐项分析；滤波器型号冲突见下文。 |
| 充电、显示与锁屏内容 | 充电，p15；显示，p23–25 | 目录定位；E-FIIO-01；F-M034-03不等于覆盖显示设置 | 充电状态、亮度/主题/夜间模式、字号/显示尺寸、指示灯、屏保、锁屏内容未覆盖。 |
| 全局导航与使用限制 | 全局设置，p25–26 | F-M034-03、F-M034-08；E-FIIO-01、E-FIIO-06：锁键和计时；音量限制有说明 | 手势/两键/三键导航、旋转、双击唤醒及各限制的组合行为未建流程。 |
| Music曲库、歌词与视觉/音效页面 | FiiO Music应用，p29–35 | F-M034-02、F-M034-08；E-FIIO-01、E-FIIO-11：找曲/批量入口/播放页及通用记曲目 | 排序、歌词/时间轴、EQ、表头/频谱/封面样式仅目录定位；扫描、临时队列、持久歌单、逐书断点与删除确认仍缺闭环。 |
| 遥控与局域网曲源 | FiiO Link与DLNA，p36–38 | F-M034-07；E-FIIO-08、E-FIIO-09：角色/连接与远端曲源 | Wi-Fi传歌、跨端队列冲突、缓存、远端删除及失联恢复未覆盖。 |
| 重置、恢复与技术支持 | 系统重置，p28；恢复模式，p38；重启/出厂/支持，p40–42 | F-M034-09；E-FIIO-07：重启、反馈及清除路径 | 网络重置/app偏好重置/整机清除的逐项影响、备份和恢复验收未覆盖。 |


2026-10-27逐项台账见 [本轮目录CSV](../batches/fiio-controls-deepening-inventory.csv)：45个手册印刷目录条目（含章标题）、29个正文补充小节、23项型号FAQ，共97条来源入口；同功能重叠不合并算成新任务。

| 功能域 | 本轮已补 | 仍待核 |
| --- | --- | --- |
| 曲库/队列/歌单 | F02排序、行菜单Play next及批量取消的静态入口；F10外部M3U转内部歌单 | Scan for music仅见空库按钮；过滤/增量/停止扫描、完整队列重排及删除确认未知。 |
| 歌词/设置 | F13同名LRC和歌词显示；F06/08补输出优先级文案、LO固定音量与无耳机暂停条件 | 偏移是否回写文件、播放点保存、3.3.4新UI/EQ完整路径未知。 |
| 网络/遥控/音频输入 | F07补Cast连接和重连排错；F11/12/15分别接收USB、AirPlay、蓝牙音频；F04保留总模式入口与退出 | WiFi传歌目标目录/取消/索引反馈、一般音频焦点、模式退出后续播未知。 |
| 恢复 | F14在线/本地更新及错误分流；F09补网络/偏好/内存清除范围 | 更新保留和回滚、备份恢复、误删撤销未知。 |

本表是所读手册的功能索引，不是“已核定整机全部能力”。时间点书签、章节、音频焦点、跨设备进度等研究问题尚不能仅凭目录确认支持或不支持。

## 功能清单与任务流程

| F 编号 | 任务 | 前置状态 | 操作与官方描述的反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M034-01 | 将电脑文件放入设备 | USB 数据线；内置存储或 microSD | Windows 接线后出现 JM21 存储入口；Mac FAQ 指向 Android File Transfer，再选内部存储或卡管理文件。 | 文件到设备不等于已进入App曲库。卡槽按JM21 FAQ触点向下、推入锁定、再推弹出；扫描完成步骤仍未知。 | E-FIIO-02；E-FICD-08；E-FICD-15 | 扫描过滤/增量/移动或重复文件策略、弹卡后的队列及歌单状态、当前macOS兼容性、传输取消与索引失败。 |
| F-M034-02 | 找曲、批量处理及进入播放页 | FiiO Music 已有曲目 | 通用 app 页列专辑/歌手/文件夹与收藏、歌单；JM21 手册列字母定位、行尾菜单和批量收藏/加歌单/删除；点左下封面进入播放页，拖进度定位。 排序菜单可按添加时间/文件名/首字母/年份；手册图行菜单出现Play next及Cancel，批量图出现Select all/Cancel。 | 收藏、歌单成员和音频文件是不同对象；批量删除是否删文件及确认界面待核。 静态菜单不证明已触发或返回后的数据结果。 | E-FIIO-01；E-FIIO-11；E-FICD-15 | 扫描过滤、完整队列插入/替换/重排/去重、删除确认/是否删文件、撤销；旧手册界面不当作3.3.4已验证界面。 |
| F-M034-03 | 实体控制与熄屏 | 已播放；锁键设置允许对应按键 | 上/下一曲短按切曲、长按前后寻位；播放键切换暂停。Global 的 Key-lock settings 控制熄屏可用按键。 | 关屏、锁键、暂停是三个状态，不能相互代指。 | E-FIIO-01 | 第三方 app 按键一致性、通知/锁屏控件、来电或其他音频打断后自动恢复。 |
| F-M034-04 | 切换工作模式并返回 | 默认 Android 模式 | 下拉菜单选 Android、Pure Music、蓝牙接收、USB DAC 或 AirPlay；非 Android 模式可用屏上模式入口，或 Settings → Exit 返回 Android。 | 改变整机工作角色；不能把“蓝牙接收”理解为给蓝牙耳机发声。 本行只分析角色总入口；实际接收目标见F11/F12/F15。 | E-FIIO-03；E-FIIO-03 | 切换时原曲位置、队列、连接和音量是否保留；退出是否立即出声。 |
| F-M034-05 | 接蓝牙耳机及恢复连接 | 耳机进入配对状态 | Settings → Bluetooth → Pair new device；首次失败先区分“配对”与耳机“回连”状态。Audio → Wireless playback quality 改 codec 后需断开再连。AirPods 音量过小时可改 Sync Bluetooth volume 后重连。 | 配对记录、实际连接、codec、音量同步设置分别变化。 | E-FIIO-04 | 耳机断电后是否暂停；重连是否自动播放；多个出口优先级。 |
| F-M034-06 | 改用外置 DAC 或模拟出口 | 对应线材及接收设备 | USB：FiiO Music 启用 USB output → OTG 接 DAC → 播放；不兼容可关 USB output，或退出 app 后重插。Audio 分别选择 3.5mm 的 PO/LO/SPDIF 与 4.4mm 的 BAL.PO/BAL.LO。 | app 输出选项与系统端口角色共同影响发声；SPDIF 接收端不认 DoP 时 FAQ 建议 D2P。 手册仅把蓝牙发射与模拟/同轴间写为后接入优先；两模拟口的“按插入顺序”未明确先/后。LO可调关闭时固定最大音量。 | E-FIIO-05；E-FIIO-10；E-FIIO-10；E-FICD-15 | 切出口音量继承、两个模拟口的明确优先顺序、热插拔瞬间声音、失败UI提示和自动回退；未实测手册优先级。 |
| F-M034-07 | 手机遥控与局域网取曲 | Link两端FiiO Music；Cast为JM21 Cast/手机Control；网络条件按分支 | Wi-Fi Link：JM21 设 Server，手机设 Client 并填设备显示 IP；蓝牙 Link 先配对再分别设角色。iPhone 官方仅列 Wi-Fi Link。DLNA 则同网，在 JM21 主菜单搜索服务器播放。 Cast分支：同局域网，JM21 FiiO Cast → Start；手机FiiO Control → Add New Device → Player，选IP或扫JM21显示的二维码。 | Link/Music与Cast/Control是不同控制入口，DLNA是媒体来源。Cast失联先查网络、后台服务和IP；仍失败可删手机旧设备、重启JM21 Cast后重加。型号旧FAQ“只接收”与通用Music实验DLNA发射日志冲突，当前JM21发射能力待核。 | E-FIIO-08；E-FIIO-09；E-FICD-07；E-FICD-13 | 当前JM21构建DLNA发射入口和支持范围、Cast退出/授权/多客户端/断点、Wi-Fi传歌目标目录与失败处理、远端缓存与队列冲突。 |
| F-M034-08 | 睡前停止、闲置关机、下次继续 | 区分系统 Global 与 app 设置 | Global 的 Sleep 到时关整机；Idle 在无操作/播放时计时，有活动则重置。app 产品页另列分钟级停止播放及记忆上次曲目、一键继续。 | 系统计时与 app 停止不是同一层；通用产品页只足以证明记曲目，不证明 JM21 当前构建精确逐书断点。 手册Audio另有Auto pause without headphones：Android/Pure Music无耳机2分钟后暂停，不能写成立即拔耳机暂停。 | E-FIIO-06；E-FIIO-11；E-FICD-15 | App timer当前入口、换曲是否延时、计时重启保留、逐曲时间点与断电保存；无耳机暂停的计时起点/多出口判定未核。 |
| F-M034-09 | 无响应或 app 故障 | 能否操作系统决定路径 | 无响应可长按电源约 10 秒至 FiiO 标识；软件问题可取 fiio_log 或 Technical support → Online feedback。FAQ 另列 app 存储清理和恢复出厂的路径。 | 强制重启、清 app 数据、整机清除是不同级别；不能把后两者当作“刷新曲库”。 手册System → Reset options另列网络、App偏好、全部内存数据三类重置，影响不同。 | E-FIIO-07；E-FICD-15 | 日志敏感信息、偏好重置完整影响、歌单/进度备份恢复、断电后索引损坏与清除失败未核。 |
| F-M034-10 | 将外部M3U歌单转成应用内歌单 | 电脑已有歌单编辑工具及同一组音频；JM21处于存储连接；不是安装或实操记录 | 官方示例用foobar保存M3U，再把电脑盘符改成机内/storage/emulated/0/或卡/storage/external_sd/，反斜线改正斜线；歌单放在对应介质根目录。断开电脑后从FiiO Music的Local music → Folder打开M3U，点右上勾选 → Select all → 歌单图标 → 选已有或新建歌单，返回Playlist检查。 | 外部文件先从Folder访问，和应用内Playlist分开；导入是把成员加入内部歌单，并未证明编辑会回写原M3U。原文另列M11双卡路径，不用于JM21。 | E-FICD-09 | 标题明确含JM21，但页面含旧图，具体App构建未给出；实际JM21挂载路径、错路径提示、重复导入、重名、移卡恢复及导出回写未知。 |
| F-M034-11 | 接收电脑USB音频作为DAC并退出 | Windows已具备官方DAC驱动；Mac按JM21 FAQ免驱；此处未下载或安装 | 下拉模式或Settings → Mode choose → USB DAC → USB接电脑 → 在电脑及播放软件选择JM21音频设备。完成后按F04的屏上模式选择或Settings → Exit回Android。 | 这是电脑向JM21送音频，不是F06的JM21向外置DAC输出。USB DAC时不支持蓝牙发射；官方提醒有延迟，不建议用于看视频。 | E-FICD-04；E-FIIO-03；E-FIIO-12 | 英文FAQ/手册把电脑端选择项写成audio input，方向措辞可疑，未冒充已确认的Windows输出菜单；失败提示、原曲/队列/音量恢复和退出后自动发声未核。 |
| F-M034-12 | 接收iOS端AirPlay音频并返回本地模式 | JM21与iOS设备同一Wi-Fi；以此型号FAQ支持范围为限 | JM21切AirPlay模式；在iOS下拉菜单选择JM21为播放设备。结束时按F04模式入口或Settings → Exit回Android。 | 音乐来源在iOS设备；JM21接收音频，未描述把文件存入本地曲库。FAQ明确此路径面向iOS而非Android。 | E-FICD-06；E-FIIO-03 | 找不到设备、密码/权限、路由器隔离、断网/锁屏中断、播放点和退出后的恢复未知；iOS版与具体菜单外观未固定。 |
| F-M034-13 | 给本地歌曲配LRC并调节歌词显示 | 音频及同名LRC在同一文件夹；曲目可播放 | FAQ要求LRC与音乐同名同目录；播放页点中央封面到歌词。手册说明可调歌词时间轴和字号；p33静态图可见Shift ±0.5s、Restore、Zoom in/out、Cancel。 | 歌词文件与播放进度是不同对象；图中还可见No lyrics found状态，仅证明提示/控件在示例出现。 | E-FICD-10；E-FICD-15 | 未操作偏移、取消、恢复；是否改写LRC、每曲保存偏移、编码错误、退出后保持及3.3.4新播放页对应入口未知。 |
| F-M034-14 | 更新JM21固件并区分包错误与启动恢复 | 已确认JM21专用包；在线路径需网络，手册要求电量高于30%；未下载或执行固件 | 在线从通知或Technical Support → Firmware Upgrade → Check for updates进入；本地则将未解压包置内存或卡，选Local Upgrade定位文件。 | FAQ列升级后自动重启；在线报invalid image时，建议删除内存下原文所称“android-data-com.android.fiioupdate-files”目录后重试，也可改本地更新。包损坏需重新取得包；升级后卡启动可按F09强制重启，Recovery先选reboot system now与清除数据分开。 | E-FICD-11；E-FICD-15；E-FIIO-07 | 未核包校验、签名/降级、成功验收、配置和歌单保留；官方耗时与恢复建议非实测，重试不保证成功。 |
| F-M034-15 | 将手机蓝牙音频送入JM21并尝试恢复连接 | 手机为发射端，JM21为接收端；区别F05连接蓝牙耳机 | JM21下拉或Settings切Bluetooth receiving；手机蓝牙设备列表选择JM21。若连接失败，FAQ建议双方蓝牙关后重开再配对连接；结束用屏上模式入口或Settings → Exit回Android。 | 改变的是音频输入角色；不同于FiiO Link遥控，也不表示把手机音频文件复制进曲库。 | E-FICD-05；E-FIIO-03 | 重配为官方建议，未验证成功率；手机音量映射、接收界面反馈、通话打断、断连暂停和回本地后的断点未知。 |

## 分支、限制与未闭合的状态

官方 Global 帮助说明系统最大音量和开机默认音量可独立设置。它们给“换耳机以后突然太响”的研究提供入口，但未证明所有输出角色都按同样方式继承音量。系统菜单里的计时、音频出口与 app 内曲库需要分别观察。[E-FIIO-06、E-FIIO-10]

材料存在复用痕迹：Audio FAQ 的滤波器段落写到 ES9038Q2M，与产品页的 CS43198 不一致。这里只用它支持菜单项/操作，不采信该芯片归属或音质形容词。官网规格表与 FAQ 对工作模式的罗列也不完全一致，操作路径按专门 FAQ 记录，并保留需要实机核对的标记。[E-FIIO-10、E-FIIO-12]

已补的空库按钮、错误升级分流和遥控重连只是特定分支。文件缺失、扫描完成反馈、误删、重复导入、换卡后的失效歌单、账号切换、跨设备冲突没有足够当前版本证据；不写成“不支持”。本地 Music、DLNA、Android 第三方流媒体下载各自的数据和权限不能合并。没有证据保证任意 Audible/流媒体 app 在 JM21 上的行为。

## 本轮静态证据及排除项

实际看过的 [p29](../artifacts/fiio-controls-deepening/manual-p029.png)、[p30](../artifacts/fiio-controls-deepening/manual-p030.png)、[p31](../artifacts/fiio-controls-deepening/manual-p031.png)、[p33](../artifacts/fiio-controls-deepening/manual-p033.png) 分别支持空库/引导、行菜单、排序/批量及歌词调节控件的布局。图的排列不是本项目连续操作；菜单取消、进度变化及文件写回未实测。[E-FICD-15]

搜索找到的60秒/文件大小过滤说明点名旧X5，未套给JM21。当前手册和FAQ仍不足以闭合扫描过滤及WiFi传歌任务。均衡器文本、图中的PEQ标签和3.3.4重做说明也未拼成一个虚构当前界面。Music语言入口：旧手册写Settings → Languages，当前型号FAQ写ME → Languages，版本未注明，保留差异。[E-FICD-10、E-FICD-13、E-FICD-15]

## 对本项目的分析（推断）

对 R-018，“继续故事”不能只返回最后曲名，应分别知道内容身份、时间点、播放意图和出口；JM21 资料正好暴露这些层之间尚不能互相推定的地方。对 G-002，AI 的动作应可查询“现在处于 DAC 模式”“已配对但未连接”“USB output 开关已改但设备未识别”等状态，不能只描述按钮已点。

物理播放键适合少看屏幕的成人/儿童纯听，Plus 是否能采用取决于实际输入和音频硬件；Android app、HiFi 解码和外设兼容性均不是 Plus 的能力证据。5–6 岁儿童界面不宜直接移植多模式音频设置；可以研究把播放动作与家长配置入口分开，此为建议而非需求。

## 下一项核查与完成条件

固定 JM21 装配、固件及 FiiO Music 构建，用包含长音频、同名文件、坏文件和已移动文件的测试库，完成“导入→扫描→播放→标记/歌单→退出→重启→删除”。同时录到锁键开关、蓝牙断开和 USB 出口切换；逐次检查队列与时间点，不把官方建议的秒数当作实测耗时。

## 证据

以下均为 2026-10-26 打开官方正文，除 E-FIIO-01 为 PDF 文本；未亲自操作。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-FIIO-01 | [JM21 Complete User Manual](https://fiio-user-manual.oss-cn-hangzhou.aliyuncs.com/EN/JM21%20complete%20user%20manual.pdf) | 印刷页 5–6 按键、26 锁键、29–33 Music；构建号未给出。 |
| E-FIIO-02 | [JM21 接电脑访问存储](https://www.fiio.com/newsinfo/984179.html) | Windows、Mac 操作分支；FAQ 日期 2024-12-11。 |
| E-FIIO-03 | [JM21 工作模式](https://www.fiio.com/newsinfo/984182.html) | 五模式、下拉入口及 Exit。 |
| E-FIIO-04 | [JM21 蓝牙发射](https://www.fiio.com/newsinfo/984185.html) | 配对、codec 重连、AirPods 音量。 |
| E-FIIO-05 | [JM21 USB 数字输出](https://www.fiio.com/newsinfo/984187.html) | USB output、OTG、异常恢复。 |
| E-FIIO-06 | [JM21 Global 菜单](https://www.fiio.com/newsinfo/984195.html) | Power-off timers、Volume setting。 |
| E-FIIO-07 | [JM21 卡死及忘记密码](https://www.fiio.com/newsinfo/984178.html) | 强制重启、日志、Clear storage、Recovery。 |
| E-FIIO-08 | [FiiO Link](https://www.fiio.com/newsinfo/984190.html) | Bluetooth/WiFi 的 Server/Client；iPhone 限制。 |
| E-FIIO-09 | [JM21 DLNA](https://www.fiio.com/newsinfo/984192.html) | 同网、搜索服务器；接收角色限制。 |
| E-FIIO-10 | [JM21 Audio 菜单](https://www.fiio.com/newsinfo/984193.html) | 输出选择、SPDIF；滤波器芯片表述疑似复用。 |
| E-FIIO-11 | [FiiO Music iOS/Android 产品页](https://www.fiio.com/app) | 分类、歌单、定时及记曲目；不是当前构建完整操作手册。 |
| E-FIIO-12 | [JM21 产品页](https://fiio.com/jm21) | Android、两种内存/存储装配与芯片规格；只作范围/冲突核对。 |

### 2026-10-27新增来源与原件

本轮14份官方HTML正常HTTP200，复用原42页PDF。短摘记见 [source-notes](../artifacts/fiio-controls-deepening/source-notes.md)，原件/摘页SHA及页码在新证据JSONL；不下载固件、驱动、APK或视频。

| E | 官方来源 | 定位 |
| --- | --- | --- |
| E-FICD-01 | [JM21官方FAQ目录（23项）](https://www.fiio.com/jm21_faq) | FAQ 1–23；目录日期2024-12-11至2025-07-29 |
| E-FICD-02 | [JM21官方资料入口](https://www.fiio.com/newsinfo/984177.html) | User manual / USB DAC driver / Firmware三个链接 |
| E-FIIO-03 | [JM21五种工作模式](https://www.fiio.com/newsinfo/984182.html) | 五角色与非Android退出；手册p8–9 |
| E-FICD-04 | [JM21 USB DAC](https://www.fiio.com/newsinfo/984183.html) | Mac/Windows步骤及蓝牙发射限制；手册p13 |
| E-FICD-05 | [JM21蓝牙接收](https://www.fiio.com/newsinfo/984184.html) | 三段：进入、手机连接、失败重配；手册p19 |
| E-FICD-06 | [JM21 AirPlay](https://www.fiio.com/newsinfo/984191.html) | iOS三步骤；手册p9只作功能简介 |
| E-FICD-07 | [JM21 FiiO Cast](https://www.fiio.com/newsinfo/1063630.html) | 连接四步骤及remote connection failures；页面链接M33视频未访问 |
| E-FICD-08 | [JM21 microSD插入/弹出](https://www.fiio.com/newsinfo/1070933.html) | 插入与弹出四条；连接指触点方向 |
| E-FICD-09 | [外部M3U创建和内部歌单导入（含JM21）](https://fiiosupport.freshdesk.com/support/solutions/articles/69000869676-how-to-create-external-m3u-playlists-for-fiio-m3k-m5-m6-m7-m9-m11-jm21-m21-using-foobar-how-to-import) | 2025-06-12修改；Detailed operation及How to import |
| E-FICD-10 | [JM21语言/歌词/截图](https://www.fiio.com/newsinfo/984196.html) | Lyrics display；语言ME路径；手册p12–13/32–33/36 |
| E-FICD-11 | [JM21固件升级与失败分支](https://www.fiio.com/newsinfo/984180.html) | Online/Local upgrade及问题1–4；手册p11–12/39–41 |
| E-FIIO-12 | [JM21产品装配范围](https://www.fiio.com/jm21) | RAM and Storage / Android / USB DAC备注 |
| E-FICD-13 | [FiiO Music官方发布说明与JM21适配分支](https://forum.fiio.com/note/showNoteContent.do?id=202105071626425774349) | FIIO署名；2026-09-11 V3.3.4、2025-06-12 V3.2.8、2022-12-30实验DLNA；未下载APK |
| E-FIIO-10 | [JM21 Audio菜单](https://www.fiio.com/newsinfo/984193.html) | 9.Adjustable volume at LO；芯片文案冲突；手册p20–23 |
| E-FICD-15 | [JM21完整手册复读与四页静态摘页](https://fiio-user-manual.oss-cn-hangzhou.aliyuncs.com/EN/JM21%20complete%20user%20manual.pdf) | 目录p3–4；正文p6/23/28/29–34/39–42；实际看图仅p29/30/31/33 |

本轮仅改变研究记录。全部hands_on=false；未改变设备、账户、配对、曲库或固件状态。

根复核：工作模式、装配范围、Audio菜单复抓沿用E-FIIO-03/12/10并追加本轮原件；新E保留12条，不把三次复查计为新来源。E-FICD-15是E-FIIO-01同一PDF的静态补证。
