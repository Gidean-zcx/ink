# M-036 Sony NW-A306：音频、系统控制与内容状态

## 范围

查阅：2026-10-26；本轮深化2026-10-27。对象是 **NW-A306（YY1301B1）及预装 Music player**，英文 Help Guide `/dmp/1301/v1/en/`；PDF 标识 5-042-672-11(5)。固件、Music player 构建及销售地区未固定，不扩展到其他 Walkman 或第三方 Android app。电脑歌单传输例子以 Music Center for PC 2.5+ 为前提。

**24项局部D2任务，没有实机或连续视觉操作。** 此前已有播放页及同版22/111页静态图；第46批复读59页正文，实际另看27、29、32、53、56、57、113共7页，其中32是文字步骤、57主要为文字和图标。四篇官方HTML与对应PDF核对；实际固件/构建/地区仍未知。全部静态页均不当执行结果，未安装、登录、付费、传输或修改设备。

本批逐页和取证范围见[系统与音频摘记](../artifacts/sony-system-audio/source-notes.md)。定时停止、时间点书签、来电打断与逐书续播仍无完整证据；音乐播放能力不自动等于有声书恢复能力。

## 功能目录与覆盖

[82项原始主题目录](../batches/sony-controls-deepening-inventory.csv)保留标题、URL和PDF定位。第46批在同一表更新39项，其中36项此前仅有目录、3项为复读；剩余16项仍只有目录，包含首次说明、介质注意、支持、维护和规格，不将它们误计为已分析任务。当前覆盖如下；此前“目录未展开”的判断随本批正文证据更新，历史来源及ID保留。

| 功能域 | 当前文档任务 | 仍需核查 |
| --- | --- | --- |
| 初始向导/主主页 | F17语言、许可、选择主页；Wi-Fi和账户可稍后，迁移可选 | 向导结束画面措辞差异；迁移内容、取消和失败 |
| 电源/充电 | F23指示灯、Battery Care、暂停后自动关机及72小时条款 | 持续音频条件、计时重置、USB供电差异和实际续航 |
| 桌面/通知/快捷/App | F18两主页、主主页设置、上滑App、下拉快捷；F03小组件/mini player | 当前配置、后台存活、误触和返回位置 |
| 锁屏/实体控制/音量 | F08 HOLD；F19认证；F22应用/输出相关音量及AVLS | 接收端控制、恢复、错码找回及家庭权限不能混同 |
| 传入/传回/曲库 | F01电脑准备与USB；F02目录索引；F16曲目传回；F21服务下载 | 半文件、重名、索引失败、服务缓存与整机恢复 |
| 队列/歌单/收藏 | F04–07临时队列、本机/电脑歌单、收藏；F11播放范围 | 重复/误删、换卡、文件移动及重建副作用 |
| 寻位/歌词 | F08实体寻位；F12 LRC配套 | 精确定位、自动取词与长音频逐书断点 |
| 无线/账户/服务 | F20 Wi-Fi成功文字、Google账户与Play入口；F09配对及输出 | 连接不等于服务可用，多账户不等于成员隔离；360外部App完整链未展开 |
| USB-DAC与音效 | F09输出条件、F13 USB-DAC输入和退出 | 两种USB角色、音量/音效、后台恢复分别实测 |
| 卡/删除/数据库 | F10删除/更新，F14卡生命周期 | 重新插入、失败回滚、取消和误删恢复 |
| 重启/清除/更新 | F15重启与出厂；F24版本与更新入口/条件 | 更新回滚、完整备份、重装与播放状态恢复 |
| 附录/规格 | 有原目录和部分注意事项正文 | 未用规格代替刷新、声音、续航、兼容或可靠性测试 |

上述是此版 Help Guide 的目录范围，不是 Android 或第三方服务的全部功能清单。未在目录发现某研究问题，不能据此断言产品不支持；账号切换、云端同步及长音频中断仍需独立证据。

## 功能与流程

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M036-01 | USB 传入并发现本地音乐 | Windows；本机/卡可写 | 接 USB → Use USB for 选 File Transfer、OK；电脑进入 WALKMAN 的 Music 文件夹，拖入文件，等传输结束才断开。菜单不出现可在 Settings → Connected devices → USB 找到。 | 音频存到指定介质；传输完成不代表所有格式、标签都可识别。 | E-SONY-05 | 取消/断线后的半文件清理、重名覆盖、错误提示。 |
| F-M036-02 | 添加非默认下载目录到曲库 | 文件位于其他文件夹 | Music player 的 Options → Settings → Folders to be imported → 注册按钮 → 选介质/目录 → 注册并确认；返回时确认更新数据库，也可稍后 Update database。 | 注册的是扫描范围，随后才将文件索引进曲库；注册中可取消。最多 10 个目录、识别至目录下第八层；不能注册根目录。 | E-SONY-06 | 更新中断、坏文件、移卡后空引用；取消注册不据文档推定为删文件。 |
| F-M036-03 | 分类找曲与播放页导航 | 曲库已索引 | WALKMAN Home → Music player → 分类/子分类 → 曲目开始播放；各页底部 mini player 可返回播放页。播放页上下左右切到相邻功能页，Operation guide 可查导航。 曲库分类图标可长按拖动；Options可选择显示分类或重置默认。播放页点标题/作者/专辑或Search related可找相关内容。 | 当前曲目与所在分类构成后续浏览/播放上下文。 | E-SONY-01；E-SONY-02；E-SYCD-12；E-SYCD-17 | 返回时列表滚动位置、分类切换是否重建队列、空库引导。 |
| F-M036-04 | 临时改听歌顺序 | 已有播放队列 | 播放页右滑进队列。队列 Options → Edit song order，拖排序柄；单项菜单 Remove from this list。曲库/收藏项可 Play next、Add to the end of Play queue 或 Empty Play queue and add。 队列Options → Add to playlist保存整队列；单项同名菜单只加该曲，不能选电脑导入歌单作为目标。 | 分别表示插到当前曲之后、末尾追加或替换整个队列；移出列表与文件删除分开。 | E-SONY-02；E-SYCD-13 | 替换是否确认/可撤销；重复项、当前项被移除、重启保留；整队列保存后的重复处理未核。 |
| F-M036-05 | 建立可长期复用的本机歌单 | 曲库有曲目 | Playlists → Options → Create new playlist → 名称、OK；播放页或列表项菜单 Add to playlist。歌单内可编辑顺序/移出曲目，歌单菜单可改名、Delete 并 YES 确认。 | 歌单保存于设备；从 Playlists → 歌单 → 曲目重新播放。 | E-SONY-07 | 同名冲突、文件移动后的成员修复、删除歌单的撤销。 |
| F-M036-06 | 导入电脑歌单并改编 | Windows Music Center for PC 2.5+；m3u | File Transfer 后电脑选设备/卡、选歌单、传入。播放器可播放和删除导入歌单；要编辑则菜单 Add to another playlist → Create new playlist，命名后改新副本。 | 电脑来源歌单不可直接在本机改；复制后产生新的本机歌单，不是原文件双向同步。 | E-SONY-08 | 路径失配、重复导入、两边修改合并；不能推定自动同步。 |
| F-M036-07 | 收藏及找回偏好内容 | 曲目/专辑/歌单已有 | 播放页星标或列表菜单 Add to Favorites；收藏页按类别找回。Options → Edit Favorites order 可拖动；星标/Remove from this list 可取消。 | 收藏可存三类对象；官方明确收藏信息不能导入电脑。 移卡时卡上对象收藏会disappear；重插恢复未说明（F14）。 | E-SONY-09；E-SYCD-07 | 重插卡/重建数据库后的收藏、误取消恢复、不同成员收藏隔离。 |
| F-M036-08 | 口袋盲操、防误按与寻位 | 播放中；HOLD 状态明确 | 实体播放键暂停/继续，上一曲短按回当前/上一曲开头，播放时长按倒退；下一曲同理。HOLD 向上锁媒体/音量键并显示图标，电源与触屏仍可用；按电源关屏防触摸。拖时间条时封面上显示定位时间。 | 锁实体键、关屏、锁屏认证、暂停四者不同；松开 HOLD 恢复按键。 官方提示已播时间和时间条可能不准确，不能称精确寻位。 | E-SONY-02；E-SONY-03；E-SONY-04；E-SYCD-17 | 锁屏通知实际操作；误拖恢复、第三方音频焦点与重启断点。 |
| F-M036-09 | 蓝牙配对/回连、音质设置与出口相关音效 | 蓝牙音频设备可用；首次/已有配对两分支 | 首次：Settings → Connected devices → Pair new device → 目标/必要密钥；设备名出现表示已连接，超时重试。回连：Settings → Connected devices → Connection preferences → Bluetooth开启后自动连接；未连或换目标则在Connected devices → Previously connected devices选设备，齿轮 → FORGET清配对。Wireless playback quality须下次连接才生效，可能受接收端设置限制。 | Sound adjustment与Music player共享数值；USB音频输出限制不等于USB-DAC输入限制，后者可用音效（F13）。系统Settings → Sound → High-Res streaming开关作用于其他App，手册描述开192kHz/32bit升频、关48kHz/16bit降频，并非所有服务兼容。 | E-SONY-11；E-SONY-12；E-SYCD-14；E-SYCD-15；E-SYCD-16；E-SYCD-05 | 断连后是否暂停、接有线优先级、重新连是否自动出声；第三方App后台/音频焦点/离线授权未核。 |
| F-M036-10 | 删除音轨并维护数据库 | 已定位对象 | 曲库分类到单项 → Context menu → Delete；播放页 Options → Delete 作用于当前曲。电脑也可删文件，但不可删改默认 Music 目录名。Settings 另有 Update/Rebuild database。 曲库也有更新按钮；先核内存Music/Download、卡Music或注册目录，再Update。 | 删除曲目不同于移出队列/歌单/收藏。Update/Rebuild是两个菜单项，现有资料没有说明二者保留对象或恢复效果的差异。 | E-SONY-10；E-SONY-13；E-SYCD-12；E-SYCD-18 | 删除确认、文件回收/撤销、Update/Rebuild对收藏/歌单的具体影响未知；不可把两个名称视为已验证的数据行为。 |
| F-M036-11 | 按分类边界设置随机、重复及播放范围 | 已选中分类、列表或搜索曲目 | 播放页点随机/重复，或 Options → Settings 设 Shuffle / Repeat；Playback range 选 All range 或 Selected range。图标均暗回普通模式。列表上的随机按钮以当前列表为范围。 | All range 仍受入口影响：随机模式从专辑进入时按专辑推进；要全曲随机/循环需从 All songs 进入。歌单/收藏仅播所选项；搜索结果点曲只播该曲，均不会因 All range 扩成全库。 | E-SYCD-02 | 切换范围是否立即改写当前队列、重启保留、空列表反馈未核。 |
| F-M036-12 | 制作并显示随曲歌词 | 有本地音轨及可编辑文本；电脑传入条件同F01 | 电脑保存 UTF-8 的同名 .lrc，时间按顺序；歌词与音频同文件夹传入。播放页 Options → Display lyrics。 | LRC 上限512 KB；歌词依附音频文件，不是播放器内时间点书签。 | E-SYCD-03；E-SYCD-04 | 错误时间标记/编码的具体提示、点击歌词跳转、歌词滚动与关闭返回状态未说明；不据此宣称可逐句寻位。 |
| F-M036-13 | 使用USB DAC并尝试恢复后台无声 | 电脑已按手册准备兼容播放软件和Windows驱动；此处未安装 | USB连接 → Use USB for 选 USB DAC、OK → Music player曲库 USB DAC → 开始提示 YES → 电脑播放。退出用Back或Library top，再确认YES。 | 输入灯绿=PCM、蓝=DSD、熄灭=未检测信号；显示格式信息。启用期间不能本机播歌或传文件，也不能无线听；本机音效仍应用。其他音视频App令DAC屏退后台后可能无声，返回 WALKMAN Home → Music player 显示DAC屏可能恢复。 | E-SYCD-05；E-SYCD-06 | 恢复仅为官方条件性建议，未实测；切回本地后断点、音频焦点、驱动报错及视频音画同步未验证。 |
| F-M036-14 | 准备、弹出与更换microSD并辨认丢失的状态 | 先备份重要卡数据；本机格式化将清空卡 | 金触点朝上插至卡住；Settings → Storage → 下拉选卡 → Options → Format，按提示完成。取卡前同入口先Eject，再按卡弹出；未识别时官方建议取出重插。 | 移卡时官方把当前播放内容信息、位置、队列曲目/顺序列为deleted，把卡内曲目/专辑/歌单收藏及卡内曲目的歌单登记信息列为disappear。这里不能推定删除了音频文件或重插必然恢复关联。 | E-SYCD-07；E-SYCD-08 | 重新插卡后哪些关联恢复、当前播放来自内存时的影响范围、格式化确认文案、损坏卡错误未知；大量数据建库约10分钟以上只是官方估计。 |
| F-M036-15 | 操作不稳时选择重启，并辨清恢复出厂的另一结果 | 重启针对不稳；清除前先备份，必要时具备锁屏凭据 | 屏幕亮时电源长按2秒 → Restart；冻结时电源按住10秒强制重启。若另选出厂清除：Settings → System → Reset options → Erase all data → 确认/所需凭据 → 再确认。 | 重启不删数据和配对信息。恢复出厂删除内存音乐/照片、机上Google信息及下载App，结束自动重启并恢复预装内容；界面可能随系统版变化。 | E-SYCD-09；E-SYCD-10 | 不把“不删数据”推成播放断点必保留；清除中断、卡数据影响、账号重新绑定及完整恢复流程未核。 |
| F-M036-16 | 将设备曲目传回电脑留存 | Windows Music Center for PC 2.5+；USB File Transfer | 打开PC软件并连接；未出现USB菜单走Settings → Connected devices → USB。软件传输页选设备或microSD → Manage songs in device → 选曲 → 右键 Import to PC。 | 保存的是所选音乐内容；传输时不可拔线。清除前可从Detailed song information的File location确认原文件位置。此流程不等于收藏、队列、账号或App整机备份。 | E-SYCD-11；E-SYCD-10；E-SONY-09 | 导出覆盖冲突、半途取消、完整性校验、播放列表/元数据的恢复范围未说明。 |

## 具体分支与异常

| 情况 | 资料支持的判断和恢复 | 证据与限制 |
| --- | --- | --- |
| 文件在设备但 Music player 不见 | 检查是否放在默认/已注册目录，注册并更新数据库；默认目录不可注销。换 microSD 不清除注册目录信息。 | E-SONY-05、E-SONY-06；不能据“文件不见”直接判断文件已删除。 |
| 歌单能播却不能改 | 先识别电脑导入还是本机创建；导入型复制成新本机歌单再改。 | E-SONY-07、E-SONY-08；来源是操作权限的一部分。 |
| 音效按钮不可用 | Direct Source/ClearAudio+ 影响其他音效可选性；USB 输出及部分蓝牙场景限制音效。 | E-SONY-12；不要把灰按钮误认成设置丢失。 |
| HOLD 开启仍被触摸操作 | HOLD 只锁指定实体键；电源关屏是另一动作。 | E-SONY-04。 |
| 蓝牙跳音 | 官方建议缩短距离、改连接优先 codec；不是测得的性能结论。 | E-SONY-11；实际断连/续播未测试。 |

本轮补了卡未识别、蓝牙配对超时和USB-DAC退后台的文档分支；普通播放音频中断及自动恢复仍未知。空数据、索引中断、空间不足、账号切换、云端同步冲突、通话打断都尚无本案例证据；“本地文件可播放”不代表第三方流媒体离线授权可供 Music player 读取。

## 单帧观察

2026-10-26 亲自查看官方 PNG（不是设备截图采集）：封面占上部，曲名/作者/专辑置于中段，星标在其右；下部是时间条和随机、上曲、暂停、下曲、重复一排控制。该图证明空间分组，不能证明点击路径、状态切换速度或触控命中率。原图带标注引线且尺寸较小。[E-SONY-14]

2026-10-27另查看 [手册p22](../artifacts/sony-controls-deepening/manual-p022.png) 与 [p111](../artifacts/sony-controls-deepening/manual-p111.png)。前者确认移卡信息分成deleted/disappear两个列表；后者看到中央输入灯和格式框、顶部Library入口及底部Back。只记录印刷页与示意布局，不代表LED切换、恢复提示或后台行为已观察。[E-SYCD-07、E-SYCD-06]

## 系统与在线内容任务（第46批）

以下均为同版文档重建；外部App的“按指引继续”不补成已经观察其内部步骤。完整定位见E-SONY-15追加采集及E-SYSA-01～05。

| F编号与任务 | 前提 | 动作与文档结果 | 异常、恢复和对象边界 | 依据/文件页 |
| --- | --- | --- | --- | --- |
| F-M036-17 首次设置与选择主主页 | 首次开机且电量足够 | 长按电源5秒；选语言/START/许可Agree，按向导选主页OK。Wi-Fi/Google可稍后；向导中联网可选迁移App/数据。 | 未执行许可/迁移；完成后WALKMAN措辞与可选主主页有差异；中断、迁移对象、失败和恢复未知。 | E-SONY-15；11–12 |
| F-M036-18 切换主页、启动应用与使用快捷控制 | WALKMAN或Android主页 | 左右滑换当前页；长按空白/Home settings/Select Home screen/选择OK改变Home目标。上滑All Apps选应用；下拉快捷，点开关或音量对话框，部分长按入设置。 | 27/29页Home固定WALKMAN文案与33页条件并存；未测手势取消、后台、默认配置和持久化。 | E-SONY-15；E-SYSA-03；E-SYSA-05；27–33 |
| F-M036-19 设置锁屏认证并辨明找回代价 | Settings → Security → Screen lock | 选择认证方式，按该方式引导完成；启用后屏灭自动锁。忘记方式时手册指向付费服务格式化清锁信息。 | 不是HOLD或家庭账号；无损找回没有依据，具体认证界面、错码/离线解锁、格式化后恢复未知。 | E-SONY-15；12、55 |
| F-M036-20 联网、配置Google账户并安装服务App | Wi-Fi网络及相应服务条件 | Network & internet/Internet选网输入密码CONNECT；文档成功显示Connected。账户菜单可加Google账户，Play搜索/按引导安装。可配置多个Google账户。 | 系统版可改变菜单；Wi-Fi连接不等于服务授权，多账户不证明隔离。错密/门户/取消、权限、卸载数据和重装恢复未核。 | E-SONY-15；75–77、81 |
| F-M036-21 取得在线音乐并让本机曲库发现 | Wi-Fi、服务App、账户及购买条件 | 按提供商流程购得内容，Music player更新数据库。若下载不在默认/注册目录，用Files移动或注册目录后再更新；格式/支付/地区依服务。 | 不保证流媒体受保护缓存可导入；支付取消、下载中断、索引失败、重复和授权失效未知。 | E-SONY-15；E-SYSA-04；78–81 |
| F-M036-22 按应用与输出方式调音量及上限 | Music player或其他App；输出方式明确 | 音量键开面板，点标尺/数值进旋钮，或快捷音量入口。其他App可能需先调Media volume；Sound/AVLS开上限。蓝牙/USB输出需接收端调节，本机AVLS不生效。 | 警告OK后仍可调高，与AVLS不同；地区/阈值、切输出音量继承、跨App和重启恢复未测；不作儿童听力保证。 | E-SONY-15；E-SYSA-01；E-SYSA-05；56–58 |
| F-M036-23 辨认充电状态并设置电池保护与自动关机 | USB接运行中电脑或Settings → Battery | 充电灯充电亮/完成灭；Battery Care限制充电上限且图标非充电时也在。Auto power off为暂停后闲置计时；另有72小时无操作即使Off也关机的文档条款。 | 72小时对持续播放适用、计时选项/重置和恢复未知；USB hub措辞不统一，电量不是精确测量；未实测续航。 | E-SONY-15；E-SYSA-02；E-SYSA-05；13–20、53 |
| F-M036-24 查看版本并进入系统更新 | 电源、Wi-Fi及事先数据留存 | About device查看型号/系统版；System/System updates确认消息并按指引继续。可用歌曲Detailed song information/File location找原文件用于备份。 | 没有执行更新；进度/取消/失败回滚与完整恢复未知。歌曲副本不是队列/收藏/账户/App整机备份。 | E-SONY-15；126–127 |

原任务补充：F01的CD导入/Stop、ATRAC转换在电脑端，不是NW-A306原生处理；Mac章仅泛指适配软件。F02细化第8层音乐文件与该层文件夹的识别差别，根下音乐不识别；113图的内部存储根名sdcard不能凭名称当物理卡。F03把封面开播放器与媒体按钮控制、mini player内容区左右跳曲分开。F09补输出音量条件与360个性化对手机相机/兼容耳机/订阅的依赖；没有看到实际采集、传回与保存。[E-SONY-15，27–37、59–65、71、74、82、113–114；E-SYSA-01/04/05]

需要保留的文案差异：初始设置11页说完成后显示WALKMAN；27/29页均写Home返回WALKMAN，而33页允许选择主主页。按明确的可配置规则分析，不能宣称所有配置下固定回同一页。USB hub充电说明一处有条件支持、一处笼统列不可充，未实测前不自行统一。

58页警告有声音和文字，可OK取消后继续调高，并描述累计20小时再次提醒/降音量及重开降音量；它与AVLS上限不同。地区和实际阈值未固定，不能把这段当所有销售版本、所有输出或儿童听力安全承诺。AVLS例外来自57页及对应在线正文；未测切到耳机时的实际音量变化。

七页静态观察见[逐页记录](../artifacts/sony-system-audio/observed-pages.json)。56页数字68只是示例，不是默认或分贝；32页没有快捷面板截图，57页没有AVLS开关截图；53页卡挂载/数据库更新图标也不证明内容准备完成。

## 对本项目的分析（推断）

本例的核心价值是把 **文件、曲库索引、临时队列、长期歌单、收藏** 分开。G-002 的 AI 若不知道对象类型，可能把“从待听里拿掉”变成删除音频。若入口同名却因来源不同不可编辑，动作说明应说明原因和可执行替代路径。

对 R-018，音乐播放器的“下一曲”和长书“下一章”不能未经内容建模就等同。对 Plus，mini player 返回入口和实体控制可借鉴；封面变色、磁带动画及四向手势在墨水屏上的价值/刷新代价需另测。5–6 岁儿童纯听可以受益于稳定的暂停/音量入口，但多种列表对象最好先验证是否能理解。

下一项最小观察：同一短曲/长音频分别经 USB、非默认目录、电脑歌单进入；拍下移动文件、移除队列和删除文件后的差异，锁 HOLD 后操作电源与触屏，再切蓝牙并重启。完成条件是对每个对象确认存储和恢复结果。

## 结构化竞分（2026-10-28）

- 对象/信息架构
  - 文件/存储、曲库索引、临时队列、长期歌单、收藏、导入目录注册、microSD、USB‑DAC/USB输出为不同对象（F‑M036‑01/02/04/05/06/10/13/14）。
  - “deleted/disappear”用于移卡后的不同关联效果；不等于物理删除（F‑M036‑14）。
- 关键任务流（已证与限制）
  - 传入与注册（D2）：File Transfer→注册导入目录→Update database（F‑M036‑01/02）。
  - 队列/歌单/收藏（D2）：编辑队列、建立/导入歌单、收藏与找回（F‑M036‑04/05/06/07）。
  - 随机/重复/范围（D2）：按入口与范围规则推进（F‑M036‑11）。
  - USB‑DAC 与退出（D2）：连接/退出与后台无声恢复建议（F‑M036‑13）。
- 失败与恢复边界
  - Update 与 Rebuild 的对象影响未证；删除曲目与移出队列/歌单/收藏分离（F‑M036‑10）。
  - AVLS 与音效开关对不同输出的限制存在；锁屏/HOLD/实体键/触摸的防误按语义分离（F‑M036‑08/09/22）。
- 对 E‑Ink4 Plus 与 5–6 岁+成人家庭的启发与风险
  - 启发：以“文件/索引/队列/歌单/收藏/范围/输出”对象建模；显式“导入目录/更新库/删除”效果。
  - 风险：把“All range”误作全库随机；把导入电脑歌单当作可直接改写；把收藏当作可导回电脑。

## 证据

以下E-SONY系列为2026-10-26首次采集；各页未提供独立更新时间。E-SONY-14为单帧观察，E-SONY-15当时仅增加PDF目录与版本定位；第46批已在原记录追加59页正文及静态范围，不能继续按仅目录理解。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-SONY-01 | [Music player 各屏导航](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733527.html) | 启动、相邻页面、mini player。 |
| E-SONY-02 | [Playing music](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733545.html) | 分类播放、队列三种加入方式、实体键。 |
| E-SONY-03 | [Playback screen](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733553.html) | 进度条、菜单、播放/暂停状态。 |
| E-SONY-04 | [HOLD](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733560.html) | 锁定范围与关闭屏幕。 |
| E-SONY-05 | [Windows Explorer 传入](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733594.html) | USB 模式、Music 目录及传输注意事项。 |
| E-SONY-06 | [注册/注销导入目录](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000752542.html) | 注册、取消、确认、更新、默认目录限制。 |
| E-SONY-07 | [本机管理歌单](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000735088.html) | 新建、加曲、编辑、删除。 |
| E-SONY-08 | [电脑歌单导入](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733548.html) | m3u、Music Center 2.5+、复制成可编辑歌单。 |
| E-SONY-09 | [Favorites](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733582.html) | 三类对象、排序、移出、不可导入电脑。 |
| E-SONY-10 | [删除音轨](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733550.html) | 曲库与播放页删除，默认目录注意事项。 |
| E-SONY-11 | [连接已配对蓝牙设备](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733597.html) | 回连、手动选择、忘记、跳音。 |
| E-SONY-12 | [Adjusting the sound](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733581.html) | 两入口共享数值、互斥、出口限制。 |
| E-SONY-13 | [Music player Settings](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733588.html) | 播放范围、重复、数据库与重置目录索引。 |
| E-SONY-14 | [官方播放页静态图](https://helpguide.sony.net/dmp/1301/v1/en/contents/image/s_a_0050_play.png) | 2026-10-26 获取并用 view_image 查看；单帧，非连续流程。 |
| E-SONY-15 | [NW-A306完整Help Guide可打印PDF](https://helpguide.sony.net/dmp/1301/v1/en/print.pdf) | 143页，标识5-042-672-11(5)；首次目录p1–5，第46批追加59页正文范围，仍非整册全任务完成。 |

### 此前一轮的新增来源（2026-10-27）

正式短摘记见 [source-notes](../artifacts/sony-controls-deepening/source-notes.md)。原件路径及SHA256保存在新证据JSONL；18个HTML正常HTTP200，PDF复用旧原件，不重复访问/下载视频。E-SYCD-17/18是旧播放页/设置页的本次原文快照补证，不以新E数量代表新增能力。

| E | 官方来源 | 定位 |
| --- | --- | --- |
| E-SYCD-01 | [NW-A306完整帮助菜单（82项）](https://helpguide.sony.net/dmp/1301/v1/en/index.html) | menu导航：82个主题；PDF目录p1–5 |
| E-SYCD-02 | [Playback methods](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733513.html) | 正文；同版PDF p90–91 |
| E-SYCD-03 | [Adding lyric information using a computer](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733592.html) | 正文；同版PDF p72–73 |
| E-SYCD-04 | [Displaying lyrics](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733549.html) | 正文；同版PDF p93 |
| E-SYCD-05 | [Enjoying music content on a computer using the sound quality of the player (USB-DAC function)](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000729844.html) | 正文；同版PDF p109–110 |
| E-SYCD-06 | [USB-DAC screen in “Music player”](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000731800.html) | 正文；同版PDF p111–112 |
| E-SYCD-07 | [Using a microSD card](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733525.html) | 正文；同版PDF p21–22 |
| E-SYCD-08 | [Formatting a microSD card](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733595.html) | 正文；同版PDF p125 |
| E-SYCD-09 | [Restarting the player](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733532.html) | 正文；同版PDF p122 |
| E-SYCD-10 | [Formatting the player](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733530.html) | 正文；同版PDF p123–124 |
| E-SYCD-11 | [Importing content from “Music player” to Music Center for PC](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733563.html) | 正文；同版PDF p69–70 |
| E-SYCD-12 | [Library screen in “Music player”](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733528.html) | 正文；同版PDF p38–41 |
| E-SYCD-13 | [Play queue screen in “Music player”](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733551.html) | 正文；同版PDF p45–46 |
| E-SYCD-14 | [Connecting the player to an unpaired Bluetooth device for the first time](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733596.html) | 正文；同版PDF p102–103 |
| E-SYCD-15 | [Configuring the Bluetooth function](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733244.html) | 正文；同版PDF p106 |
| E-SYCD-16 | [Using the High-Res streaming function](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000735013.html) | 正文；同版PDF p119 |
| E-SYCD-17 | [Playback screen in “Music player”](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733553.html) | 正文；同版PDF p42–44 |
| E-SYCD-18 | [Settings menu in “Music player”](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733588.html) | 正文；同版PDF p120–121 |

此前该轮修改的是研究记录：6个新任务、6个旧任务补证/收窄；全部hands_on=false。没有改变任何设备、账户、音乐库或配对状态。后续跨产品判断见当前横向比较与交互依据表，仍保留文档/实际操作的区别。

第46批新增来源为E-SYSA-01音量、02耗电、03主页、04在线内容四篇官方正文，以及05同源静态摘页组。直接URL、PDF页码和短摘见[本批来源笔记](../artifacts/sony-system-audio/source-notes.md)。原E-SONY-15追加本批59页范围；不覆盖旧捕获，也不宣称143页整册或设备全部能力完成。
