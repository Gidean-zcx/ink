import pathlib,json,csv,re
R=pathlib.Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/antennapod-controls-deepening';P=L/'cases/M-040-antennapod.md'
old=P.read_text();(W/'case-before.md').write_text(old)
# tuple: task/precondition/action/data/evidence/gap
changes={
'01':('新增来源、取消订阅与归档恢复','新增需网络与可用源；已有订阅可管理','原帮助：Add Podcast搜索/RSS/OPML。3.12.3订阅菜单 Unsubscribe / archive → Archive 或 Unsubscribe；后者先出现确认按钮，确认后执行。订阅页菜单 Archive → 归档对象 Unsubscribe / restore → Restore。处理时显示进度并隐藏取消，成功或错误均关闭表单（错误记录日志）。','新增只建目录；Archive改状态并保留下载/历史，文案称停止找新集；Unsubscribe删该源记录、移出队列、取消下载并清在线源下载，当前媒体发STOP。本地文件夹退订不删底层原文件。归档Restore与删除后的重订阅不同。','E-ANTP-01；E-APCD-01；E-APCD-13；E-APCD-14','重复源合并、私有认证；删除后无逐项撤销证据，重订阅能否保留原历史未知；同步远端处理、批量中途失败回滚未闭合。'),
'02':('把想听内容放进队列','已发现具体单集','长按单集 → Add to queue；3.12.3 Playback设置另有 Enqueue downloaded，XML声明true，因此“下载入队”应按该开关判断。Downloads查看文件，Queue安排后续播放。','排队、已下载、已播放分开；下载入队是可配规则，不能写成无条件。','E-ANTP-02；E-ANTP-06；E-APCD-03','具体安装/迁移默认值、重复加入和退出恢复未实测；菜单文本及用户自定排序仍依旧帮助。'),
'04':('自动准备下一批离线节目','已订阅；自动下载资格与网络条件满足','沿旧帮助：Settings → Downloads → Automatic download，或播客齿轮 Include in auto downloads；新集进Inbox，符合条件才下载/入队。3.12.3声明自动下载、队列下载、数量及充电条件开关。','数量不能统一报默认值：旧帮助50、该tag XML25、UserPreferences缺值回退20；三者适用层不同，真实安装默认未定。','E-ANTP-05；E-APCD-03','半文件、失败重试、断网恢复、下载顺序；未观察通知进度，也未闭合首次初始化/迁移如何决定默认。'),
'05':('清下载并区分退订与本地原文件删除','下载或本地文件夹媒体存在','沿文档在列表Delete或Downloads垃圾桶；是否出队另由设置控制。3.12.3删当前媒体会清当前标记并发STOP。手动删本地文件使用DocumentFile.delete；取消其文件夹订阅则跳过底层文件删除（F01）。','在线源下载删除同时处理转录文件/下载状态并排待同步DELETE；文件删除失败可能只记录日志，代码仍清下载标记，不能据界面变化保证物理文件已删。收藏/自动清理条件沿旧帮助。','E-ANTP-06；E-ANTP-07；E-APCD-14','逐集删除确认与撤销UI、存储损坏、物理删除失败的可见提示未闭合；本地原文件无可重下载保证。'),
'07':('从播放封面查章并跳转（3.12.3静态）','已选媒体；章节非空或媒体标记有尚未载入章节','播放封面页章按钮 → 章节表。加载进度；点章时若未播放先play，再seek起点、更新当前章；Close退出。外部章节URL存在才有Refresh。封面上一章在当前章起点附近跳前一章，否则回本章起点；末章隐藏下一章。','显隐由章节数据/标记决定；上一章窗口按10000×播放速度换算媒体毫秒。空集关闭并提示无章；异常回调仅日志，未证明可见重试。','E-ANTP-14；E-APCD-05；E-APCD-06','无当前媒体/网络章节错误的最终UI与残留进度未知；章节按键、无障碍焦点、跨章实机时间未观察。'),
'08':('设分钟或集数睡眠计时（3.12.3静态）','播放状态满足源码isRunning条件','播放菜单 → Sleep timer；选分钟/集数、输入、Set。0/解析失败及无播放走提示。可Disable或延期（分钟5/10/30，集数1/2/3）。连续播放关闭时集数设置禁用，给Playback设置入口；超队列或剩余时长有提示条件。','类型/上次数值/自动时段是偏好；当前剩余值在计时对象。分钟计时由位置事件触发，以System.currentTimeMillis两次事件到达差扣减，间隔>10秒跳过，绝非用position差计算。临近10秒渐弱/可振动，摇动可重置初值；到期pause，集数完结阻止接播；服务销毁停止计时。','E-ANTP-15；E-ANTP-16；E-APCD-07；E-APCD-08；E-APCD-09；E-APCD-10','短暂停、缓冲、未知duration、后台杀进程和时钟变动的真实表现未测；传感器不可用异常未闭合；不得承诺纯墙钟到期。'),
'09':('顺序接播、输出断开与音频焦点变化（3.12.3静态）','固定3.12.3默认Media3服务；下一项及网络许可满足','正式配置启用Media3，旧服务禁用。完成后受集数计时/连续播放控制；无下一项清当前媒体，禁止移动流播时转确认项并停止自动播放。ExoPlayer以speech媒体属性委托Media3自动焦点，按断开偏好处理noisy；焦点临时抑制解除后可回退上下文。','停播回调保存位置并排同步播放动作；不能把旧服务的蓝牙重连分支搬来。API31+隐藏耳机/蓝牙重连设置，较低API有设置不等于新服务相同恢复。','E-ANTP-17；E-APCD-04；E-APCD-10；E-APCD-11；E-APCD-12','Media3库底层永久/暂时焦点、电话与导航策略、蓝牙恢复及各系统实际通知未操作；源码委托不构成恢复保证。'),
'11':('核对锁屏/通知控制与后台排错的版本适用性','Android系统和安装版本已知；本案仅固定代码tag','旧帮助建议通知无法恢复时加Quick Settings tile，另查电池优化；3.12.3配置却禁用旧tile服务，故该建议不直接套用。当前Media3 Session提供快退/快进及可配速度、下一章、跳集、睡眠动作，睡眠按钮随状态切换命令。','设置中的高优先通知API26+隐藏、持久控制API30+隐藏；声明的按钮偏好不代表系统一定全部呈现。旧帮助根因未定，不升级成3.12.3已复现问题。','E-ANTP-10；E-ANTP-11；E-APCD-03；E-APCD-04；E-APCD-17','锁屏布局/槽位、厂商电池行为、后台被杀与暂停后重建均未验证；须另做固定Android版本观察。'),
'12':('备份恢复、部分同步与删除后的恢复边界','先导出现库备份；同版本迁移建议沿官方帮助','3.12.3 Settings → Backup & restore → Database export选文件；导入先显示替换订阅/播放历史警告，可No或取消文件选择。确认选择后等待；成功需OK重启，失败显示错误。OPML勾选追加现有订阅；同步配置沿旧帮助。自动DB备份选择可写文件夹并持久授权，周期执行仅目录。','SQLite导出保留库数据，不含音频；全局UserPreferences在SharedPreferences，不能以官网all data推成全部设置备份。导入拒绝比当前更高的DB schema，随后替换旧库；非合并。OPML/同步/HTML收藏输出范围不同。归档Restore不等同退订撤销；数据库恢复会覆盖当前库。','E-ANTP-12；E-ANTP-13；E-APCD-03；E-APCD-15；E-APCD-16；E-APCD-18','失败后事务回滚未证明；同步冲突/账户切换未核；历史损库修复帮助不能当正常删除恢复。全局设置迁移、自动备份周期执行与文件权限撤销待验证。')}
intro='''# M-040 AntennaPod：订阅、队列、音频中断与可恢复数据

## 范围与方法

初查2026-10-26，深化2026-10-27。对象为Android **AntennaPod 3.12.3**，官方正式发布于2026-10-13，tag提交 **175e5ef8fe3e125a5014d70ef0c57cf85661c71b**，声明versionCode 3120395。未取得/运行发行APK，固定的是发行源码，不声称验证了安装产物或任一Android设备。英文资源为定位依据，中文是分析转述；地区、厂商系统、账号和用户既有配置均未实测。

旧E-ANTP帮助页没有逐页构建边界；旧代码E-ANTP-14～17来自develop `d3e41c6...`，保留为历史。3.12.3明确开启Media3、禁用旧服务，不能把旧服务中的重连逻辑当本版承诺。官方3.12.0发布记录也说明服务重写。[E-APCD-01、04]

覆盖为**D2文档/定点源码重建**；目录和仅声明的设置为D1。没有构建、安装、登录、数据提交、APK/视频下载、实际操作或产品截图观察，`hands_on=false`、`visual_observed=false`。本轮复用12F，未新增流程编号。

## 功能目录与覆盖

[逐项目录](../artifacts/antennapod-controls-deepening/functional-inventory.csv)列出官方帮助索引全部**47条（8分类、39文章）**及3.12.3全局**11个preferences XML的84个带key节点**；[定点来源记录](../artifacts/antennapod-controls-deepening/source-notes.md)给出代码范围。它们只核定这两个明确集合，**不是全应用功能已分析**：每播客动态设置、播放器全部菜单、转录、统计、扩展/车载等仍有缺口。

| 功能域 | 目录/设置定位 | 关联F/E | 已查与未覆盖 |
| --- | --- | --- | --- |
| 入门、Home、订阅来源 | Getting started；Home/Subscribe | F01；E-ANTP-01、E-APCD-02 | 新增入口已有D2；Home布局/首次引导待查。 |
| 订阅管理 | Subscriptions tags；SubscriptionFragment/RemoveFeedDialog | F01；E-APCD-13、14 | 取消/归档/恢复与删除状态本轮补齐静态链；标签管理、每播客设置目录未完全核定。 |
| 待听队列 | Queue 4篇；Playback/Queue设置 | F02、03、09；E-ANTP-02～04、E-APCD-03 | 入队/排序/锁定已查；multiple-queues文章仅登记，未新增流程。 |
| 订阅刷新与下载 | Automation 4篇；Downloads、Automatic download XML | F04、10；E-ANTP-05、09、E-APCD-03 | 自动下载条件已有D2；刷新周期、Tasker、代理、外部存储/权限只目录。默认数量冲突保留。 |
| 清理与状态 | Deletion/Automatic deletion；Auto deletion XML | F01、05；E-ANTP-06、07、E-APCD-14 | 媒体删/退订/归档分开；撤销UI、物理失败与清理候选预览未闭合。 |
| 播放、章节 | Playback/Auto rewind；Cover/Chapters源码 | F06、07；E-ANTP-08、E-APCD-05、06 | 章节入口/空集/跳转本轮D2；倍速/静音跳过/转录/单集笔记完整任务未查。 |
| 睡眠计时 | 播放菜单/底部表单/计时类 | F08；E-APCD-07～10 | 分钟与集数、事件扣时、结束/停用及部分异常D2；短暂停/休眠/传感器组合未实测。 |
| 中断、外设、通知 | Playback Interruptions；User interface；Session | F09、11；E-APCD-04、10～12、17 | 3.12.3服务、焦点委托、断开偏好和通知命令D2；来电/重连实际表现未知。 |
| 备份/恢复/同步 | General Backup/Sync；Backup & restore XML | F12；E-ANTP-12、13、E-APCD-15、16、18 | DB确认/替换/失败路径D2；自动备份周期、HTML/收藏回导和双端冲突未闭合。 |
| 显示、导航与手势 | User interface、Swipe XML | E-APCD-03 | 主题、动态色、剩余时长、封面、底部导航、列表/滑动入口仅D1；不能从XML保证可见。 |
| 错误、数据损坏 | Bugs/errors 8篇 | F10～12；E-ANTP-09～11、18；E-APCD-18 | 原下载/后台排错已有D2；历史库损坏恢复提示与正常退订区分，未执行修复。 |
| 车载、投放、键盘 | Android Auto、Chromecast、Shortcuts帮助；硬件键设置 | E-APCD-02、03 | 仅目录；外设路由、焦点与按钮映射组合未验证。 |
| 统计、项目、平台/权限、播客主 | General其余条目、Podcasters/hosters、About/Notifications XML | E-APCD-02、03 | 统计、广告、F-Droid、Beta、权限、募资/收录/Feed URL等只目录，未提升D2。 |

## 功能与流程

下表“显示/暂停/删除”等均是文档描述或静态回调，未记录实际反馈耗时。

| F 编号 | 任务 | 前置状态 | 操作与资料中的反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
'''
rows=[]
for line in old.splitlines():
 if line.startswith('| F-M040-'):
  n=line.split('|')[1].strip().rsplit('-',1)[-1]
  if n in changes:line='| F-M040-'+n+' | '+' | '.join(changes[n])+' |'
  rows.append(line)
state='''
## 状态与异常边界

“归档”“删除下载”“取消订阅”“OPML加入”和“数据库替换”操作不同：前者保留订阅数据待恢复；删除下载保留源；取消订阅会清该源记录并触发队列/下载处理；OPML是勾选后追加；数据库导入则覆盖当前库。取消订阅的错误回调也关闭表单，不能把退出当作成功反馈；没有找到删除后逐项回收站或撤销承诺。[E-APCD-13～16、E-ANTP-12]

分钟睡眠计时**不是按音频position差值扣减**。ClockSleepTimer在收到PlaybackPositionEvent时读取`System.currentTimeMillis()`，扣两次事件到达的时间差；差值大于10秒则更新lastTick后返回。Media3服务的位置观察按状态启停，正常周期为每秒、且仅duration>0才发事件。这只能说明事件驱动的实现；短暂停可能仍落入计时差值，缓冲、焦点抑制和休眠的具体结果未运行，不能称“暂停必冻结”或“设置后墙钟到期”。参数偏好保存不代表剩余倒计时持久保存。[E-APCD-08、10]

“临时音频焦点抑制解除后可回退”来自Media3服务回调；真正焦点夺取/回授由依赖库和Android参与处理。断开暂停、重连自动恢复、用户主动暂停是不同任务。本版API31+隐藏重连开关，旧服务代码的同名函数不足以证明Media3行为。通知/锁屏的命令集合也不能代替系统实际布局或后台生存测试。[E-APCD-10～12、17]

备份帮助用“all data”概括数据库恢复，但本tag只导出SQLite，UserPreferences另在SharedPreferences。故本案收窄为库内订阅/单集/队列等状态，音频须重新取得，全局偏好迁移不能保证。导入拒绝更高DB schema，并非仅比较app版本字符串；验证后删除旧库再移动新库，未见足够证据承诺任何失败都有回滚。损库帮助针对历史问题，残留音频可再作为本地文件夹加入不等同恢复RSS更新或已删历史。[E-APCD-15、16、18]

## 对本项目的分析（推断）

对G-001/R-020，Inbox、Queue、Downloads分别表示新到、计划、文件可用，不能被AI合并成“我的内容”。对G-002/R-018，AI执行“清掉不听的”至少应明确归档、删媒体、退订、同步动作与可恢复范围；“晚上停播”还需明确按分钟、按集数和中断规则。状态可读可控的思想有依据可研究，Android实现未在Plus验证。

儿童前台可只暴露已准备的离线集合，家长负责订阅、下载资格和保留规则，这是助手推断，不是新增需求。自动回退、计时渐弱也只是候选体验；没有Plus后台、传感器、功耗和解码测试支持性能承诺。

## 后续最小核查

固定3.12.3安装产物与一个Android版本，使用两个可控RSS源及本地文件夹，观察章节空/非空、暂停超过/不足10秒时计时、电话/导航焦点、拔插耳机、归档恢复/确认退订、DB覆盖及取消。记录UI、状态和媒体文件分别变化；本轮没有执行这些任务。先确认API31+重连入口与旧tile是否可用，再决定扩测厂商差异。

## 证据

原E-ANTP查阅日期2026-10-26；新E-APCD为2026-10-27。新源码URL固定3.12.3提交，原帮助保持版本未指定。原件在work，正式归档为来源笔记和目录；没有视觉观察或连续时间戳。

'''
evidence=old[old.index('| E 编号 | 来源 | 定位 |'):]
new_e=[json.loads(x) for x in (L/'batches/antennapod-controls-deepening-evidence.jsonl').read_text().splitlines()]
newtable='\n| E 编号 | 本轮来源 | 定位 |\n| --- | --- | --- |\n'+'\n'.join('| '+x['evidence_id']+' | ['+x['title']+']('+x['url']+') | '+x['locator']+' |' for x in new_e)+'\n'
P.write_text(intro+'\n'.join(rows)+'\n'+state+evidence+newtable)
# Preserve master, generate explicit per-field suggestions from the old batch.
base=list(csv.DictReader((L/'batches/audio-coverage.csv').open()));updates=[]
for r in base:
 if r['product_id']!='M-040':continue
 n=r['flow_id'].rsplit('-',1)[-1]
 if n not in changes:continue
 task,pre,act,data,e,gap=changes[n];after=dict(r);after.update(task=task,evidence_ids=e.replace('；',';'),known_branches=data,gaps=gap,checked_on='2026-10-27')
 updates.append({'flow_id':r['flow_id'],'changes':{k:{'before':r[k],'after':after[k]} for k in r if r[k]!=after[k]},'replacement_row':after})
(W/'old-coverage-updates.json').write_text(json.dumps(updates,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'case_flows':len(rows),'old_rows_to_update':len(updates),'fields':sorted({k for x in updates for k in x['changes']})},ensure_ascii=False))
