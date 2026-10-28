# M-039 Audible：播放器控制、离线资格与读听定位

## 范围

查阅：2026-10-27；原批2026-10-26。固定 **美国市场、英语官方帮助、Audible Android/iOS手机App**；App构建号未知。Cloud Player、Kindle配套、Car Mode只在来源明确的范围内比较。帮助页日期为资料日期，不是本机软件版本。当前US文档把Whispersync for Voice称为Read & Listen；不能将2026帮助倒套旧版App或其他国家市场。（E-AUCD-07/20）

33条流程最高D2，`hands_on=false`、`visual_observed=false`。没有登录、购书、退还、提交、下载音频/APK或播放操作；只读取公开文档。网页读取工具能取到正文，普通HTTP取得的4个HTML却是相同前端壳；HTTP200/哈希不算正文归档。正文按工具摘取快照留work及哈希，不称原始完整HTML。页面的JS/CSS错误不等于Audible App故障。无图像观察、连续流程、时延测量。

免费/已购、会员包含目录和Standard月选有不同访问资格；Amazon账号不自动包含会员权限。已下载与有资格播放分开，不能从离线音频推定全书文字或笔记都已离线。

## 结构化竞分（2026-10-28）

- 交互（继续/下载/片段/计时/队列/同步）：
  - Library 按“未开始/进行中/下载/完成”筛选并一键继续；mini player/锁屏继续播放（F‑M039‑01）。
  - 下载完成方可离线收听；播客需逐单集下载；电脑端下载不等于手机App下载（F‑M039‑02）。
  - 片段与书签：+Clip（5–44秒）与书签分层，片段仅音频回放，不是导出（F‑M039‑04/05）。
  - 计时与延期：Timer 可覆盖/延期；可选 Shake to extend（F‑M039‑07）。
  - Up next 队列与推荐预览分离；可开关 Continuous listening（F‑M039‑06）。
  - Listen Log / Listening History 区分单书会话与全局历史（F‑M039‑08）。
  - 跨设备同步需联网；Read & Listen 为配对音频与Kindle文本的特例（F‑M039‑09/13）。
- 信息架构：
  - “移除下载 / 从库移除 / 永久删除”三层对象；Hidden 列表独立；交易与退款流程再分层（F‑M039‑10/12/23）。
  - 家庭/儿童：Kids Profile 单独限定；Family Sharing 由家庭成员关系与家长端控制（F‑M039‑15/16/17/26）。
  - 文字与音频、Cloud Player 与本机App、外部设备（Sonos/Alexa/Watch/车载）均属不同子系统（F‑M039‑30/31，E‑AUCD‑17/19）。
- 关键任务流（样例）：
  - 继续与离线：在 Library 找到“进行中”→ 播放 →（可）Download 完成 → 断网播放；Listen Log 回看并校正位置（F‑M039‑01/02/08）。
  - 片段与笔记：播放页 +Clip → （可）Edit Clip → 添加笔记 → 从 Clips & Notes 返回该段复听（F‑M039‑04）。
  - 计时与队列：设 Timer → 摇动延期 → 到点停止；Up next 维护下一本，不触发推荐预览（F‑M039‑06/07）。
  - 同步与配对：开启 Sync listening position → 在支持 Read & Listen 的标题中 Listen/Read 切换（F‑M039‑09/13）。
- 失败与恢复（当前证据）：
  - 下载/缺章：移除下载后重下；改音质后再下（F‑M039‑11）。不保证日志/片段/书签随即持久。
  - 清数据/重装：下载与已完成标记会丢失，需重标；重装后需重新登录与授权（E‑AUCD‑18）。
  - Remove download vs Remove from Library vs 永久删除：对象不同、后果不同；Hidden 与 Library 的关系文档仍不统一（F‑M039‑10）。
- 对 E‑Ink4 Plus 与 5–6 岁+成人家庭的启发与风险：
  - 启发：把“继续”拆解为（标题/位置/资格/下载/输出设备）多对象；将“记下刚才那段”落实为可回放锚点（片段/书签/日志）。
  - 风险：读听配对与Page Sync需网络与资格；下载可离线不代表文字/附件可离线；“移除/隐藏/退还/删除”混淆将导致错误的数据语义。

## 可取得的功能目录与覆盖

[逐条目录](../batches/audible-controls-deepening-inventory.csv) 现为54行（原53行加儿童共享移除入口）：Listening首屏35个不同文章入口、9项补充帮助、9个Player设置项。Player子目录的10条已包含在首屏项目中，不重复计数。Ways to listen显示Load more而未取得后续全集，因此这不是完整App菜单或全部设备支持清单。每项区分正文阅读、搜索正文、仅目录及设置列举；目录存在不代表完成任务。（E-AUCD-01/02）

| 功能域 | 可见目录及操作材料 | 关联任务 | 明确缺口 |
| --- | --- | --- | --- |
| Ways to listen | App、沉浸读听、Read & Listen、Alexa Tablet、AudibleSync、macOS、Sonos、Alexa、Windows停用、Apple Watch等首屏 | F01/02/09/13；Cloud和Car另补 | 后续分页未展开；不以手机流程覆盖所有设备 |
| Library | Library、Lists、Wish List、PDF、Page Sync、Goodreads、Family Sharing、下载、Maven | F01/02/10/11/14 | 本轮已补愿望单、PDF、共享、Goodreads和Maven；实际操作仍缺 |
| Player | 片段/笔记、Listen Log、PDF、Timer、分享、速度、自动播放、提醒、挑战、章节 | F03–08 | 本轮补提醒、挑战、分享/PDF文档路径；实际结果/恢复未闭合；UI可定制导致按钮位置需版本核对 |
| 设置 | 定制、章节/全书进度条、前后跳秒、mini player时间、摇动延期、锁屏、自动Car Mode、同步、连续听 | F03/06/07/09 | 列举了入口和选项域，没取得全部值、默认值、定制拖动与保存反馈 |
| Profiles/社交 | Kids Profile创建/删除/收听/分享、统计、部分共享、评论 | F15–19/25/26；Page Sync排除见F14 | 已补儿童建立/共享/删除与跨端限制；不等同Amazon儿童账号，仍无UI验证 |
| 访问/恢复/交易 | 缺书、质量、崩溃、市场、退还、Standard/Plus资格 | F02/10/11/12 | 不推定离线授权周期；不执行清存储、卸载、退还或市场迁移 |

## 任务流程（原11条保留编号）

| F 编号 | 任务与前置状态 | 操作、官方反馈及退出 | 数据变化和异常 | 证据/未核 |
| --- | --- | --- | --- | --- |
| F-M039-01 | 找到并继续一本书；正确账号/市场 | Library顶部类型可左滑；用未开始/进行中/下载/完成筛选。目录搜索可加In your library筛选找到已购但不再列于销售目录的书。选标题播放；mini player或锁屏Play恢复。 | 库成员、下载和收听状态各自独立；过滤不改访问资格。 | E-AUD-01/09；E-AUCD-02/03。空库引导、筛选保持、双设备并发未知。 |
| F-M039-02 | 下载后无网收听；有资格、联网且有空间 | Library → All Titles → 三点或长按标题 → Download；播客进入Podcasts的具体单集再下载；完成才可离线。手机仍无批量下载选项。 | 手机与电脑不同：电脑下载只明确信用点/卡购买内容，Cloud仍需联网。包含目录/Standard月选的下载不免除资格条件；见下表。 | E-AUD-02；E-AUCD-04/19/21/22。下载进度、取消/续传、未完成可听范围和离线检查周期未知。 |
| F-M039-03 | 按章跳转和调速度；当前标题已开 | Chapters → 章节/单集；Speed改变本书速度；Profile → Settings → Player → Narration Speed → Default改变新听默认值。播放器三点 → Player settings也是设置入口，可调整进度条与跳秒等。 | 单书速度按官方描述跨apps保存，默认值可因设备不同；没有据此推定所有硬件端。章节可能重分，不一定与纸书一致。设置/Car Mode局部控制见下段。 | E-AUD-05/08；E-AUCD-02/13/15/17。跳转撤销、具体跳秒范围、定制保存和通用封面手势未知。 |
| F-M039-04 | 标记值得重听的一段并加笔记；手机有声书 | +Clip保存30秒；确认条Edit或Clips & Notes → 三点 → Edit Clip调起止手柄；笔记填写后Save。列表中Clip只听片段，Start in Player从该处续听；三点Delete → 再Delete删除。 | 片段5–44秒并产生书签；不是音频导出。官方“时长编辑”段未单列保存按钮，“笔记”段明确Save，不将二者补成实测同一结束方式。 | E-AUD-03；E-AUCD-05。30秒前后范围、离线冲突、关联书签是否同删、取消保留未知。 |
| F-M039-05 | 播客书签与回到原文；App或Cloud | App单集+Bookmark → 确认条Add note → Save；以后由Bookmarks & Notes进列表。Cloud在播放器Add a bookmark或主菜单Bookmarks添加/编辑笔记，Start in Player返回时间点。两端单项Delete再确认。 | App有声书片段与播客书签入口不同；网页可管书签但不能创建/管理片段，且仍需联网流播。 | E-AUD-03；E-AUCD-05/19。取消编辑、关联删除、精度及跨端冲突未知。 |
| F-M039-06 | 排好下一本并控制自动延续 | Library三点/长按 → Play next；当前播放器Chapters → Up next → 单项菜单上下/置顶底或Remove from queue。Profile → Settings → Player → Continuous listening中开关Up next titles。 | Recommended titles另管推荐预览；只有没排下一本时才播预览，可单独关。移出队列不按删书处理。 | E-AUD-06；E-AUCD-14。重启/跨端保存、重复项、未下载下一本如何失败未知。 |
| F-M039-07 | 睡眠计时与延期；正在播放 | Timer选预设/自定义；运行时点Timer或封面倒计时选新时长覆盖。到时停止并提醒；允许延期同长或重设。开启Shake to extend后，声音渐弱起30秒内摇动可延长同样时长。 | 设置在Profile → Settings → Player → Timer。iOS17+ Live Activity显示倒计时，依赖权限；不能由锁屏显示推定计时后台一定持续。 | E-AUD-04；E-AUCD-12。暂停/换书/重启/强退、误摇、闹钟/电话优先级未知。 |
| F-M039-08 | 找回误拖或睡着后的位置 | 当前/已听完书的播放器封面下Listen Log → 选会话段重放。另有Profile → Activity & Stats → Listening History；网页姓名 → Listen History → 某标题Remove。 | 书内日志记录会话起停、时间和长度；标题历史列表不同。可清书内历史，但未取得确认步骤。两类记录均无官方导出。网页Remove不等于删书或撤销已听进度。 | E-AUD-07；E-AUCD-16。保留期、清除恢复/跨端汇总未知，不代表理解或清醒程度。 |
| F-M039-09 | 换设备续听与纠正同步；同账号联网 | Profile → Settings → Player → Sync listening position；不同步可关再开。Read & Listen配对格式另要求有声书和对应Kindle电子书；其帮助手动修复写作Sync device position。 | 保留两文档标签差异，不能认定新版改名。配对位置同步不是下载、设置、笔记全量备份；手动同步明确需联网。 | E-AUD-01；E-AUCD-07。离线并发胜出规则、回退确认、标记/笔记合并未知。 |
| F-M039-10 | 清空间、移出书库与永久删除；先明确对象 | Remove download清本机音频，可Select批量；Remove from Library另管库显示。已隐藏书可Library → Lists → Hidden → 三点 → Remove from list → 回Library。永久删除在Amazon内容与设备 → Content → Delete → Yes, delete permanently。 | 清下载不删库成员，可重下；当前帮助未证明Remove from Library一定就是加入Hidden。永久删除后账号各设备不可从库访问；退还另见F12，不是本流程的退款结果。 | E-AUD-09；E-AUCD-03/06/08/23。隐藏加入入口、笔记生命周期和永久删除补救未知。 |
| F-M039-11 | 缺章、停播或找不到书；区分故障范围 | 单书缺章：Remove download再下，官方称从原位置继续；改质量先Profile → Settings → Data & Storage选Standard/High再重下。缺书下拉至转圈或Settings → Library → Refresh Library，核市场/筛选/Hidden。 | Samsung/Pixel后台限制可查设备Apps → Audible → Battery；优化模式仍可能停。崩溃帮助另给清缓存/清存储、重装，明确重装失去下载与Finished标记并需重新标记；不把普通重下载保进度推广到清数据。 | E-AUD-10/11；E-AUCD-06/09/18/20。具体系统/构建、来电后实际恢复、未同步笔记和清存储影响未知。 |

## 控制与内容状态的补充边界

已得手势只包括类型横滑、书库下拉刷新、标题长按、片段手柄拖动和摇动延长计时。官方播放器设置列出定制、进度条、跳秒、时间显示、锁屏等，但没有本批可核的完整封面手势/双击区域/按钮拖拽顺序。不能用历史iPhone指南或第三方截图补成当前US App。

2025-10-24车载帮助描述手机Car Mode：播放器Car Mode图标进入，X退出；简化页列语音、播放暂停、回退30秒、书签四项。Profile → Settings → Player可开Automatic Car Mode，使蓝牙连车时进入。这是历史文档分支，非2026构建画面；不把Car Mode、Android Auto、AAOS和CarPlay合成一个平台，也没有断连/电话后自动续播结论。（E-AUCD-17）

| 动作/状态 | 改变什么 | 已知恢复/限制 |
| --- | --- | --- |
| Remove download | 当前设备的下载文件 | 书库仍在，可重下；没有保证所有笔记或离线授权状态完全不变。 |
| Remove from Library | 库中的显示/成员动作 | 官方说不等于永久删除；确切已购/包含内容分支和与Hidden关系未统一。 |
| Hidden → Remove from list | 从隐藏列表移出 | 回Library核书是否出现；加入Hidden的路径本轮未知。 |
| 普通List中Remove / 删除List | 清单成员或清单 | 官方搜索正文给Lists/Collections两种标签；删清单不可恢复需重建，不据此认定音频/购买权一并删除。 |
| Amazon永久Delete | 账号可访问内容 | 有明确永久确认；各设备不能再从库访问。 |
| Return this title | 合资格购买交易 | F12；涉及信用点返还，不能把清下载或归档说成退款。 |
| Standard月选失效 | 会员给定访问权 | 最终账期结束后显示锁及不可用消息；可重新入会或购买恢复，若已无购买选项可能目录不再提供。 |

上述分层分别据E-AUCD-03/06/08/21/23。退订对Standard月选的影响不同于信用点/现金购买；Plus包含目录也可轮换或失去资格（E-AUCD-06/22）。本批未发现可靠的离线授权检查周期，不填“30天”等常见猜测。

市场恢复也不等于跨市场迁移：当前帮助有Profile → Settings → Marketplace → 选择 → Switch，或退出后选市场；购买仍留在原市场。地区限制可能不同，故不将US Read & Listen/Car Mode直接套用加拿大等市场。未执行账号迁移。（E-AUCD-20）

## 新增独立任务

### F-M039-12 按购买来源确认资格并退还标题

US帮助把自助退还限定为状态良好的有效Premium会员、信用点购买、购买后365天内；退还次数/历史仍可能限制入口。Standard月选不能退换；App Store/Google Play来源按原购买渠道处理，不保证网页路径可退所有交易。（E-AUCD-08/21）

手机用网页而非App：菜单 → 姓名 → Purchase history → 选标题 → Return this title → 确认；桌面从用户名或Membership Details到Purchase history，同样选择并确认。文档称确认后返1个信用点，返还信用点自发放起12个月到期。未操作，没有确认界面观察、处理耗时或到账验证。（E-AUCD-08）

Add Narration折扣组合有联动：合资格退回配套Kindle电子书时，官方说明有声书会自动退款并移出库，退到原付款方式且发通知。这是交易依赖，不是一般进度同步；离线文件/书签何时失效未核。（E-AUCD-07/08）

### F-M039-13 在支持双格式的标题中开始沉浸读听

前提是同一书的支持Read & Listen的有声书及Kindle电子书均具访问资格；徽标/Library筛选用来发现可用配对，不由同名或相同章节推定。Kindle App可三点 → Listen on Audible → 确认打开Audible；Audible播放时用封面上方Listen/Read & Listen切换，沉浸模式随专业旁白高亮文本。E-AUCD-07描述两模式之间切换；反向切换后播放/文本位置是否保持未核。（E-AUCD-07/10）

Audible App中的文字通过互联网加载，仅部分页可缓存。音频下载完整不能保证整本离线读听；缓存页数、驱逐、断网提示和重连后定位未说明。帮助列字体、字号滑条、明暗主题、速度和章节导航，但未给完整设置入口/保存反馈，保持D1配置发现。该功能用录制旁白而非TTS；与Kindle阅读器能否切换音频的支持范围分别看待，不称全部Kindle设备可同步高亮。（E-AUCD-10）

### F-M039-14 扫描纸书页跳到有声书位置

在App打开对应有声书 → 三点More → Page Sync → 首次授予相机权限 → 对准平整、光线足、完整可见的文字页。文档描述自动捕捉/识别、给可读性反馈并跳到匹配音频位置；不用先下载音频，但需活动网络。失败先核网络、相机权限和取景条件；没有实际识别结果或耗时。（E-AUCD-11）

仅部分英语书；不支持播客、无纸书对应的Originals、Kids Profile和儿童绘本。只支持纸书→音频，不能反查音频所在纸书页码；版本文字匹配才有意义。误匹配后直接撤销、权限拒绝弹窗、图像上传/保存和重扫合并未核；一般Listen Log可作为待测试恢复线索，尚不把它当已验证Page Sync撤销。（E-AUCD-11）

## 对本项目的分析（推断）

对R-018/G-002，“继续”至少涉及标题、音频位置、内容资格、下载完成和输出设备；“记下刚才那段”应产生能返回的锚点。进度、片段、书签、日志、清单和交易分别建模，AI不能把Remove、Hide、Return视为同一个删除动作。对G-001，主动队列和自动推荐预览的分离值得保留为设计候选。

沉浸读听说明“音频可离线”和“文字可离线”可以不同；Plus若实现读听，需要明确内容配对、文字资源、位置映射及缓存状态，不能只显示一个已下载标记。Page Sync提供纸书到音频的思路，但其联网/相机/内容限制不能忽略，尤其官方排除Kids Profile和绘本，不能直接当5–6岁儿童方案。Audible的服务、DRM和App并不因此能移植到Plus。

最小后续观察：固定US账号、同标题配对版本和Android/iOS构建，记录下载完成→断网纯听/读听→误拖/日志回退→联网位置同步；再分别测电话、闹钟、耳机断连和重装的数据边界。所有操作尚未执行；未读目录和未取得动作保持未知，不写成不支持。

## 证据

原批来源保留；本轮复读日期2026-10-27。重复URL是保存正文/精化范围，不是独立验证次数。完整来源列表、摘取文件及哈希见新证据批；正式[获取与限制记录](../artifacts/audible-controls-deepening/source-notes.md)不复制全文。


均于 2026-10-26 读取官方美国英文帮助正文；页面前端错误未阻断文章读取。发布日期见定位。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-AUD-01 | [Listen in the app](https://help.audible.com/s/article/listen-in-the-app) | 2026-10-13；Library、mini player、锁屏、进度同步。 |
| E-AUD-02 | [Download titles](https://help.audible.com/s/article/download-titles) | 2026-09-03；app 与播客下载、完成后离线、无批量下载。 |
| E-AUD-03 | [Manage clips & notes](https://help.audible.com/s/article/manage-clips-bookmarks) | 2026-09-09；片段/书签、笔记、删除与返回。 |
| E-AUD-04 | [Use timer](https://help.audible.com/s/article/use-timer) | 2026-09-03；计时覆盖、摇动延期、iOS Live Activity。 |
| E-AUD-05 | [Browse chapters and episodes](https://help.audible.com/s/article/browse-chapters-and-episodes) | 2026-09-03；章节入口与编号不一致原因。 |
| E-AUD-06 | [Manage auto-play settings](https://help.audible.com/s/article/manage-your-auto-play-settings?language=en_US) | 2026-09-03；Up next 排序与推荐预览条件。 |
| E-AUD-07 | [View listening log](https://help.audible.com/s/article/view-listening-log?language=en_US) | 2026-09-03；Listen Log、Listening History、不可导出。 |
| E-AUD-08 | [Set narration speed](https://help.audible.com/s/article/set-narration-speed?language=en_US) | 2026-09-03；单书速度与每设备默认值。 |
| E-AUD-09 | [Manage your Library](https://help.audible.com/s/article/manage-your-library) | 2026-09-09；筛选、移出、清下载、永久删除分层。 |
| E-AUD-10 | [Audio quality issues](https://help.audible.com/s/article/title-won-t-play-has-poor-audio-quality) | 2026-09-09；重下载保进度、质量、系统电池限制。 |
| E-AUD-11 | [Missing title issues](https://help.audible.com/s/article/title-is-missing?language=en_US) | 2026-10-01；刷新、marketplace、归档与目录轮换。 |

本轮补证：

- E-AUCD-01：[官方 Listening / Player 可见帮助目录](https://help.audible.com/s/listening?language=en_US)；Ways to listen / Library / Player / Reviews / Profiles。
- E-AUCD-02：[App 播放器设置入口与功能清单](https://help.audible.com/s/article/listen-in-the-app)；Player settings; nine listed settings; resume/sync。
- E-AUCD-03：[书库筛选、移除及永久删除分层](https://help.audible.com/s/article/manage-your-library)；Library sections; In your library; Remove download / Library / permanent delete。
- E-AUCD-04：[音频下载与设备离线边界](https://help.audible.com/s/article/download-titles)；App download; computer entitlement; Apple Watch / Fire scope。
- E-AUCD-05：[片段和书签的新增、返回与删除](https://help.audible.com/s/article/manage-clips-bookmarks)；Save/Edit/Delete clip; app podcast vs Cloud bookmarks。
- E-AUCD-06：[缺书与Hidden恢复路径](https://help.audible.com/s/article/title-is-missing?language=en_US)；Check your archive; Refresh; search/filter。
- E-AUCD-07：[Read & Listen（原Whispersync for Voice）](https://help.audible.com/s/article/listen-with-whispersync-for-voice?language=en_US)；eligibility; switch; manual sync; companion return。
- E-AUCD-08：[退还标题：资格、网页入口和结果](https://help.audible.com/s/article/return-a-title)；Eligibility; mobile/desktop; linked Kindle return。
- E-AUCD-09：[播放故障、重下载与Android后台限制](https://help.audible.com/s/article/title-won-t-play-has-poor-audio-quality)；App; Samsung/Pixel battery。
- E-AUCD-10：[沉浸读听与文字联网条件](https://help.audible.com/s/article/learn-about-immersion-reading)；requirements; start; customization。
- E-AUCD-11：[Page Sync：纸书扫描到音频](https://help.audible.com/s/article/use-page-sync)；use; eligible titles; troubleshooting。
- E-AUCD-12：[睡眠计时、覆盖与摇动延期](https://help.audible.com/s/article/use-timer)；set/extend/shake/Live Activity。
- E-AUCD-13：[单标题速度与设备默认速度](https://help.audible.com/s/article/set-narration-speed?language=en_US)；particular title / default / Cloud。
- E-AUCD-14：[Up next与推荐预览](https://help.audible.com/s/article/manage-your-auto-play-settings?language=en_US)；continuous listening / add/edit queue。
- E-AUCD-15：[章节/播客跳转与分章差异](https://help.audible.com/s/article/browse-chapters-and-episodes)；app/Cloud; why chapter mismatch。
- E-AUCD-16：[书内Listen Log与标题Listening History](https://help.audible.com/s/article/view-listening-log?language=en_US)；find log; remove title from history。
- E-AUCD-17：[Car Mode的控制与退出](https://help.audible.com/s/article/listen-in-the-car?language=en_US)；Car Mode L107–134。
- E-AUCD-18：[重装/清缓存与本地状态损失](https://help.audible.com/s/article/app-keeps-crashing?language=en_US)；Clear data/cache; reinstall note。
- E-AUCD-19：[Cloud Player需联网及浏览器入口](https://help.audible.com/s/article/listen-on-a-browser?language=en_US)；requirements; desktop/mobile。
- E-AUCD-20：[切换marketplace不是迁移账号](https://help.audible.com/s/article/update-your-country-marketplace?language=en_US)；app switch; purchased titles remain; Canada note。
- E-AUCD-21：[Standard月选、离线下载与失效状态](https://help.audible.com/s/article/learn-about-standard-plan?language=en_US)；monthly titles / cancellation / return。
- E-AUCD-22：[Plus包含目录与访问权限](https://help.audible.com/s/article/learn-about-audible-plus?language=en_US)；benefits / cancellation。
- E-AUCD-23：[Lists管理的官方搜索正文](https://help.audible.com/s/article/manage-collections?language=en_US)；search result Manage your Lists; add/remove/delete。

## 2026-10-27：家庭音频与执行状态补证（第33批）

本轮来源定位见[正式摘记](../artifacts/audible-family-controls/source-notes.md)。所有新材料保持D2文档依据；没有新增设备、App或网页实际操作。

既有Listening首屏中22项仅目录的文章已取得正文（其中4项来自官方搜索正文）；另补Lists的直接打开正文。macOS仅安装、Windows仅退役范围，仍不补成完整播放流程；Ways to listen后续分页及全部设置值仍未穷尽。

| F任务 | 步骤、状态与分支 | 仍未知 | E依据 |
| --- | --- | --- | --- |
| F-M039-15 建立并切换儿童收听档案 | 成人Profile创建；Profile姓名下拉选Kids，回成人档案需重新验证。每个账户/家庭仅一个Kids Profile；多机可同时听，但换设备需重新定位，同机语速与成人共享。 | 当前构建、同机多孩子区分、离线切换和重装恢复未测；不等同Amazon儿童账号。 | E-AUFC-01;E-AUFC-02 |
| F-M039-16 共享、更新并撤回儿童书目 | 成人书库单书/单集/书单菜单共享；内容资格不足或编辑新加标题可显示部分共享，后者先更新再共享。移除需确认，儿童端更新可能延迟；会员包含内容失去资格后锁定，已购内容另算。 | 共享失败/重复提交、离线撤回、下载与播放中撤权、重新共享是否恢复原进度未知。 | E-AUFC-03;E-AUFC-05;E-AUFC-06 |
| F-M039-17 删除儿童档案并辨认保留对象 | App的Profile→Manage Kids Profile→Delete并确认；桌面Membership details→Profiles删除。儿童个人数据和历史不可恢复，成人书库原书及清单仍保留；新建须重新共享。 | 取消确认、删除处理时间、离线设备何时生效和成人笔记是否受影响未测。 | E-AUFC-04 |
| F-M039-18 查看收听统计并分享里程碑 | 登录App→Profile→Listening stats；收听统计随播放更新，部分类型洞察按月刷新；Share my stats可分享选定项目。 | 具体字段算法、离线合并、多档案归属、删除历史后重算及分享回执未知。 | E-AUFC-07 |
| F-M039-19 参加收听挑战并核对结算 | 主页Join a Challenge选每日10/20/30分钟或3/4/5本目标后确认；开始后不可更改，同一挑战不可重做。分钟在会话结束更新，本数在书完成更新；播客和非US书不计多本目标。完成得徽章；取消会员失进度且不能重启。 | 会话结束与听完判定、倍速/跳章计数、断网、跨日/跨端、退出挑战和恢复未知；不证明理解收益。 | E-AUFC-11 |
| F-M039-20 用Maven对话筛选有声书 | App Search→Ask Maven；网页从搜索入口开Maven。用自然语言提出主题、时长或演播偏好，追问缩小、比较及概述；帮助称可在24小时内恢复会话。 | 仅推荐有声书/Originals，不荐播客或作者，不提供深入书内解答或账户技术支持；历史范围、引用、纠错及代购/导入能力未证。 | E-AUFC-08 |
| F-M039-21 创建、修改和删除收听提醒 | 播放器菜单进入提醒，或Profile设置→Listening reminders→Create先选书；设星期/时间后Save。编辑再Save；删除须再次确认。只限有声书，需系统允许通知。 | 通知到达、点击后的定位、静音/时区/离线/重启、多设备冲突未知；不等同到点自动播放。 | E-AUFC-10 |
| F-M039-22 找到并保存随书PDF | 先在书籍详情核是否附PDF；App书库打开对应书点PDF，Android打开后可另存；桌面书库View PDF自动下载。音频提到附件但库中没有时联系支持。 | iOS文件保存出口、离线可用、附件版本、与音频定位关联及缺文件错误反馈未知。 | E-AUFC-09 |
| F-M039-23 管理愿望单并区分购买 | App详情Add to Wish List；Library→Lists→Wish List查看，标题菜单移除。购买需进入详情选支付方式；移动网页确认购买、桌面购物车结账确认后才入库并邮件通知。 | App购买最终确认与渠道差别、撤销移除、跨端冲突及购买后愿望单是否自动移除未核；没有执行交易。 | E-AUFC-13 |
| F-M039-24 关联Goodreads待读书架并隐藏或解除 | Goodreads设置连接Amazon并授权；Audible Library→Lists查看，需会员/试用，文档提示最多24小时出现。只显示匹配的可用音频版本；增删在Goodreads进行。Audible设置可隐藏，彻底解除需回Goodreads。 | 版本错配修正、授权失败/撤回回执、双端延迟未知；解除会影响其他Amazon/Kindle关联，不等同本地隐藏。 | E-AUFC-14 |
| F-M039-25 撰写修改删除评价及显示名称 | 听至少20%才可评；播放器或书库Rate & Review填三维评分、标题和至少15词后提交，网页先预览。批准后可能再等两工作日；删除仅网站且二次确认。Listener Page可汇总评价及修改公开名。 | 退回修改、未提交草稿、离线、审批中编辑、评分对推荐影响和删除传播未知；未发送公开内容。 | E-AUFC-15 |
| F-M039-26 按Amazon家庭成员共享已购音频 | Family中添加另一成人或儿童；成人经内容设备页Audiobooks→Add to Library，儿童经Parent Dashboard内容页逐书开关。成人邀请14天内接受；会员积分与包含目录不按已购书共享。 | 文档最多另一成人/四儿童；Kids Profile另行适用。撤回、成员离开、多个档案进度与各端可用表未穷尽，未创建家庭。 | E-AUFC-12 |
| F-M039-27 经AudibleSync下载激活并复制到兼容设备 | Windows10/11登录AudibleSync先下载，完成后Download按钮消失；连接受支持设备并Activate Device验证账户，再Copy to Device或拖放。电脑文件需转到兼容播放设备使用。 | 传输完成/取消/重试、重复文件、激活上限与断点回传未知；不把下载文件当无保护MP3，也不证明E-Ink4 Plus兼容。 | E-AUFC-17;E-AUFC-21 |
| F-M039-28 Apple Watch独立收听与空库恢复 | watchOS9+、蓝牙耳机及库内标题；联网播放可后台下载，未下载则播放器Data & Storage开启Stream & download。Manage Library移除下载保留库条目；近iPhone重新认证保留下载和设置，重装会清下载。 | 空库排查却要求先点标题进入播放器，入口不闭合；Kids Profile、跨端秒点、下载进度与断网恢复未知。 | E-AUFC-19 |
| F-M039-29 Alexa唤醒选书、续播和定位 | 同一Amazon账号；App设置Alexa开启免手并授麦克风，仅前台响应，锁屏/别App不响应。语音可选书、暂停续播、指定章节/时间和播放后设停播计时；Stories声明记上次位置。Echo播客与App限制分开。 | App不支持播客语音；不支持片段/书签、评分、Listen Log和标记完成。歧义、取消、购买确认及语音失败恢复未核；不是系统全局AI。 | E-AUFC-23 |
| F-M039-30 选择Sonos输出或授权Sonos内服务 | Audible播放器输出图标选择可用Sonos；另一路在Sonos的More→Add Music Services添加Audible后登录授权。帮助列语速/计时/书签/章节/统计限制，但未逐路径细分。 | 手机直出与Sonos内播放的控件差别、断连/来电/输出切换的位置及授权撤回未知。 | E-AUFC-20 |
| F-M039-31 从Alexa平板桌面继续听并跨设备接续 | 该文所列Alexa Tablet用同Amazon账户开内置Audible；桌面长按空白→Widgets拖入Audible，另有继续听入口。跨手机/Echo/车/平板的位置同步要求各端联网；儿童内容由Kids+/家长端管理。 | 型号/国家特性不同；离线合并、桌面状态刷新与多成员定位未知，未验证实际平板或倒套旧Fire型号。 | E-AUFC-16 |
| F-M039-32 建立个人书单与跟随公开书单 | App Library→Lists→New Lists填名/描述后Create；桌面对应Collections，移动网页不能创建个人书单。书库菜单添加条目，书单内移出；公开书单Save to Collections后出现在库中，取消跟随需删除该清单入口。 | 排序/重名/批量、公共书单更新提醒和个人编辑范围未知；删除清单不可恢复需重建，不能据此推定删掉书本。 | E-AUCD-23 |
| F-M039-33 把书籍详情作为推荐发送 | 库内标题菜单或长按Share；库外书从详情分享图标选目标App。发送的是推荐详情，收件人自行决定购买；家庭收听授权用另一机制。 | 分享落地页、取消、发送失败和统计未知；旧Send this book已停用，不把分享成功说成对方可完整收听。 | E-AUFC-22 |
| F-M039-09 换设备续听与纠正同步 | 普通进度同步与Read & Listen双格式配对分开；手动重新同步需联网；两文档分别称Sync listening position/Sync device position。 Kids Profile是例外：帮助要求换设备时重新定位，多机同时听不等于位置跟随个人。 | 离线并发进度冲突、标记/笔记合并、回退确认与全设置同步未知。 儿童同机共享的个人进度隔离未证。 | E-AUD-01;E-AUCD-07;E-AUFC-02 |

对本项目的启发仍是建议：共享内容资格、某个人的播放位置、设备上的设置、云端档案和当前输出需要分别辨认。Maven只提供推荐/比较，App内Alexa又受前台范围限制，不能将二者合称系统AI已能操作所有数据与页面。提醒送达、挑战计数和实际理解收益也须分别验证。
