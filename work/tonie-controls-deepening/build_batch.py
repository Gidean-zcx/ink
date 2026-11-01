# 一次性初稿构建器。输出已有局部修订，不要重跑覆盖case/CSV/建议；以当前文件和check_batch.py为准。
import csv, hashlib, json, pathlib, re

ROOT=pathlib.Path('/home/workspace/reader')
LIB=ROOT/'docs/research/interaction-library'
W=ROOT/'work/tonie-controls-deepening'
DATE='2026-10-27'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def writecsv(p, rows, fields):
    with p.open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(rows)
def eid(n):return f'E-TNCD-{n:02}'
def fid(n):return f'F-M038-{n:02}'
source_map=json.loads((W/'source-map.json').read_text())
meta=[
('1代官方帮助目录','Quick Solutions至All Other Articles；63个标题','只登记可见目录，不代表63项已研究。'),
('2代官方帮助目录','Product Information至Error Codes；65个标题','搜索目录65项；直接打开的目录显示部分分组较少，保留快照差异。'),
('Tonieplay官方帮助目录','L24–75；14个条目','目录完整取得；个别游戏随盒说明书未取得。'),
('US家庭管理帮助目录','8个标题','与UK账户页显示5/7项不同；不拼成唯一全球目录。'),
('Tonieplay设备与首次联网条件','L18–22','2代、Controller及首次Wi-Fi；后续离线以完整下载为条件。'),
('2代Features FAQ的Tonieplay启动步骤','TONIEPLAY / Ready to play','放盘、下载、中央Controller按键；该页含宣传，未据宣传判定学习效果。'),
('Tonieplay下载和白灯排错','Download / disc / Light Ring / Not Recognised','完整游戏先下载；绿进度、自动开始；白闪取下盘和控制器再放。'),
('Tonieplay读盘失败的重启与重置','restart / factory reset','先不插电重启，Welcome Lights；重置另走Parent Access小耳。'),
('取盘后的游戏类型恢复差异','L18–20','quiz不保留未完轮次；互动故事最后位置；Discover新轮。5题为举例。'),
('Tonieplay重启与检查点','L22–28','取盘数五再放，起点或已达checkpoint；未通关不能全重置；pause用语不证明精确暂停。'),
('Tonieplay跳过开场限制','L18–20','多数开场可跳；作答/选择不能马上重播；具体按键未说明。'),
('Tonieplay章节选择限制','L18–20','当前帮助称多数不可，仅点名Winnie The Pooh可选故事；不推广到未来所有游戏。'),
('2代睡眠与日出闹钟条件','Sleep timer / Sunrise Alarm / Preferences','UK官方索引正文；EU原址一次直接HTTP403后停止。3+、充电要求、正在播放Tonie阻止闹钟。'),
('家庭成员权限设置','app步骤1–8；web步骤1–6','Owner可改权限；Full与按Creative授权区分。Full管理设备的正文明确写Toniebox 1。'),
('邀请家庭成员与接收','L25–60','发邀请、接受、指定Creative权限分别完成；未发送邮件或加入家庭。'),
('Creative Tonie已绑定其他家庭','正文与Useful tips','带内容的公仔不能携内容换云账号；原Owner/Full释放或邀请访问。'),
('Creative Live开关与1代同步','app步骤1–6；web步骤1–6','Live是新内容置顶，不是直播；同步段明确1代，不套2代。'),
('UK手动排序的Live前置条件','正文首段及拖动段','只有关闭Live才手动排序；作为UK/未知构建限定，不强行覆盖US不同措辞。'),
('家庭所有权移交','L20–38','接受者不能已拥有另一家庭；Set New Owner后旧Owner退出，重返需邀请。'),
('删除整个家庭及录音','L18、25–54','App Dissolve Household后Delete；官网二次确认；永久删除含Creative录音。'),
('2代恢复出厂与App说明入口','Factory Reset步骤1–6；in app guidance','帮助明确最后手段/初始状态；App入口为说明页，不是已证明远程执行重置。'),
('Tonieplay交互数据类别','L18–20','使用频次和导致终止的错误等；未证实家长可读取逐题数据或开放API。'),
('Tonieplay控制器动作宣传与FAQ','How to Use / Questions & Answers','仅D1的转动/按压和共同游玩介绍；无具体判分、奖励或完整游戏规则。'),
('2代故障目录中的年龄模式路径','Why does…plugged in；10条故障目录','该条预览给出App→盒子→Settings→Age Mode；其余未读正文不当完整排错。')]
evidence=[]
for m,(title,loc,note) in zip(source_map,meta):
    n=m['id'];url=re.search(r'\((https?://[^\s]+)\)$',m['header']).group(1)
    mode='official_search_index_text' if 'search' in m['ref'] else 'official_web_tool_extracted_text'
    scope=('Toniebox 1; US English help' if n==1 else 'Toniebox 2 / Tonieplay' if n in [2,3,5,6,7,8,9,10,11,12,13,21,22,23,24] else 'tonies app / my.tonies.com Household and Creative Tonie; box generation only where explicit')
    row=dict(evidence_id=eid(n),product_id='M-038',title=title,url=url,checked_on=DATE,source_type='official_help_index' if n in [1,2,3,4,24] else 'official_product_page' if n==23 else 'official_help',access_mode=mode,version_scope=scope+'; locale follows URL; firmware and app build unknown',app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='web_tool_text_snapshot_not_raw_html',supersedes=None,raw_work_path=m['snapshot'],raw_sha256=sha(ROOT/m['snapshot']),extracted_work_path=m['extracted'],extracted_sha256=sha(ROOT/m['extracted']))
    if n in [6,13,18]:row['qualifies']={6:['E-TONI-12'],13:['E-TONI-11'],18:['E-TONI-16']}[n]
    evidence.append(row)
(LIB/'batches/tonie-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in evidence))

# 新任务是新用户目标；旧任务只给合并建议。
new=[
(12,'2代准备并开始一款Tonieplay游戏','Toniebox 2、游戏盘和Controller；首次联网','放盘，等完整游戏下载；帮助以绿进度表示下载，完成后自动开始；将Controller置于灯环中央并按键开始。卡住可取盘、重启盒子再试；持续白闪则同时取下盘与Controller，稍候再放。','完整缓存后可离线再玩；普通Tonie首章可先播的规则不能套给游戏。','E-TNCD-05;E-TNCD-06;E-TNCD-07;E-TNCD-23','控制器旋转选择/按压确认的逐步反馈、每款游戏规则/判分/退出、下载取消与续传未知；官网30分钟至1小时以上仅估计，不是本项目测速。'),
(13,'2代Tonieplay中断、回检查点与重开','明确游戏类型、是否已达检查点或通关','重启帮助：取盘数五再放；未到检查点从开头，已到则回最近点，通关前不能全重置。另页按类型说明：quiz未完轮次不续，互动故事回最后位置，Discover开新轮。','这里的恢复不是统一秒点暂停；5题只是quiz轮次举例。多数开场可跳，但答题/选择不能立即重播；章节页当时仅点名Winnie The Pooh可选故事。','E-TNCD-09;E-TNCD-10;E-TNCD-11;E-TNCD-12','两页“pause/从头/last position/checkpoint”如何对应具体游戏未知；跳过具体按键、通关全重置入口、断电/跨盒持久性、错误答案和奖励均未核。'),
(14,'2代设置日出闹钟并识别不响条件','盒子在线；页面要求连接充电器且仅3+模式','Mine→盒子→Sleep Timer with Light→Set time & Sunrise Alarm；设时间、启用，调音量，Tone选音，可加入日出光。','若顶部Tonie仍在播放，闹钟不响；页面另提醒电量耗尽不启动。首次设定与次日实际响铃未观察。','E-TNCD-13;E-TONI-09','仅有设置路径：停止响铃/贪睡/重复日程、时区、断网/断电恢复未知；“需充电器”与“电量足够”并列，不能推出拔线可用。'),
(15,'邀请家庭成员并限制可编辑的Creative Tonie','邀请者具家庭管理权；对方有免费账号','App头像→Household→＋→Full或Creative成员→邮箱→Next→Confirm。对方从邮件Accept Invitation登录确认；Creative成员还须指定公仔。Owner在Household→成员→Membership改角色；返回成员页设置图标勾选公仔，绿勾表示授权。','受限成员只见/编辑获授权Creative对象；Full可编辑全部Creative，权限页设备管理措辞只明确1代。官网Members→成员→选公仔→Save Changes。','E-TNCD-14;E-TNCD-15','邀请过期、撤回/移除成员、并发修改、降权对盒子离线缓存的影响；2代设备细粒度管理权未逐项说明。'),
(16,'收到外家庭Creative Tonie后取得编辑权限','公仔有内容却不出现在自己的云账户','先核是否绑定原家庭。保留原家庭内容供编辑的官方路径是请原家庭邀请为Full或获指定权限的Creative成员；换云账户则由原Owner/Full删内容或从原家庭移除。','拿到实体公仔不等于获得云端编辑权；带内容公仔不能携内容转入另一云账户。二手公仔需原持有人释放，支持方说明不会替用户移除内容。','E-TNCD-16;E-TNCD-14','原持有人失联、释放后旧盒离线缓存、解绑撤销、内容导出与自动备份未核；不能把公仔移交等同全家数据迁移。'),
(17,'把家庭所有权移交给已邀请成员','接收者已加入且不是另一家庭Owner','官网Members→Household preferences→Leave this household；窗口选接收成员→Set New Owner。若其已拥有别的家庭，列表不会出现；同窗口另有解散家庭分支。','完成后新成员成为Owner，旧Owner自动退出；旧Owner要回家庭须由新Owner重新邀请。','E-TNCD-19','未操作确认/失败/撤销；接收者原家庭有数据时的合并方案未说明，不把“删除自己的家庭”当无损步骤。'),
(18,'删除整个家庭并确认数据损失范围','家庭管理端；已了解将永久删除数据','App头像→Household→设置→Dissolve Household→红色Delete确认；官网Members→Household preferences→Delete this household→弹窗Delete Household确认。','官方明确永久删除家庭全部数据，包括Creative Tonie录音；不等同删除某段音频、释放公仔、退出家庭或重置盒子。','E-TNCD-20;E-TONI-18','未执行；回收站/撤销/客服恢复未说明，盒子离线旧缓存清除时机未知。')]
before=json.loads((W/'old-coverage-before.json').read_text());fields=list(before[0]);rows=[]
for n,task,pre,action,data,es,gap in new:
    rows.append(dict(flow_id=fid(n),product_id='M-038',task=task,depth='D2',evidence_ids=es,known_branches=data,gaps=gap,visual_status='not_observed',hands_on='false',case_path='cases/M-038-tonie.md',checked_on=DATE))
writecsv(LIB/'batches/tonie-controls-deepening-coverage.csv',rows,fields)

case=(W/'case-before.md').read_text()
case=case.replace('查阅：2026-10-26。','初查：2026-10-26；本批补证：2026-10-27。',1)
case=case.replace('未核定全部 Tonieplay 游戏、用户/家庭权限和耳机出口，未把图像替代文字说明或当成完整流程。','本批补Tonieplay准备/恢复、闹钟条件、家庭授权与数据迁移/删除；具体游戏随盒说明书、逐题规则、耳机出口仍未核。没有播放或下载网页内嵌视频，未查看单帧图片，不能据“Image/iframe”标签记视觉观察。')
case=case.replace('## 两代任务路径','''## 官方目录与覆盖边界

[逐项台账](../batches/tonie-controls-deepening-inventory.csv)按本次返回的目录登记150个入口：1代63、2代65、Tonieplay14、US家庭管理8；这些是帮助条目，不是150项功能已完成。地区及搜索/直接打开快照数目不同，不能据此声称全站目录完整或内容缺失。其余Creative、App、Audio Library等入口只取得部分专页，完整目录仍待查。

| 目录域 | 本批定位与已关联任务 | 仍未覆盖 |
| --- | --- | --- |
| 1代：Quick Solutions、Setup、Care、Settings、Playing、Battery、Error、Other | E-TNCD-01；原F01–04；旧2018手册继续单列 | 多网络/初设失败、具体错误词、耳机/电量等未逐条分析；目录不更新旧固件行为 |
| 2代：Product、Setup、Settings、Compatibility、Purchasing、Battery、Troubleshooting、Error | E-TNCD-02；F07–11、14；购买标题仅台账 | 初设/蓝牙配网、全部12错误词、充电完整路径与新旧关机页冲突未闭合 |
| Tonieplay：14项帮助 | E-TNCD-03；F12–13及F11排错 | 年龄/共同游玩为功能说明；操作频次数据不证明可查孩子成绩；具体游戏纸本规则未取得 |
| 家庭管理：US 8项 | E-TNCD-04；F15–18 | 注册、改名、登录故障、移除盒子只登记；Full权限页设备措辞限1代 |
| Creative局部专页 | F05–06、16；Live补E-TNCD-17/18 | 通用Creative与各特殊公仔不能混成统一备份/恢复模型 |

范围以各证据URL的US/UK/Europe/Canada地区为准；所有新资料的App构建与盒子固件未知，网页抓取日期不代表功能上线日期。E-TNCD-23是产品介绍，只用于控制器“转/按”和共同游玩的D1发现，不证明判分或学习成效。

## 两代任务路径''',1)

changes={
5:dict(pre='家庭账号、公仔已加入家庭；成员还须有该Creative编辑权（F15）',action='App选择Creative Tonie，可录音或上传已有MP3/WAV、保存；原教程列90分钟总量，再按盒子代际同步。若公仔未出现，先查成员授权及是否仍在其他家庭（F16）。',data='云内容清单、账号授权、盒子缓存分开；上传不等于盒子离线可播，拿到实体公仔也不等于拥有编辑权。',es='E-TONI-15;E-TONI-05;E-TONI-10;E-TNCD-14;E-TNCD-16',gap='录音权限、取消上传、格式失败和容量超限提示；授权撤销后旧离线缓存是否仍播放未知。'),
6:dict(pre='已有自制内容及编辑权；UK帮助限定手动排序须关闭Live',action='原路径Collection→Creative Tonies→目标→Edit；拖柄排序Done；选圆圈Delete→Done，官网Edit Content后Save Content。Live另在目标→Advanced settings开启；官网开关On后Save Changes；它把以后新加内容置顶。',data='手动顺序与自动新内容置顶是两种规则；UK页给出Live前置条件，US页未同样明确。1代按专页同步，2代仍按自动同步专页；原文件应另留。',es='E-TONI-16;E-TONI-17;E-TONI-18;E-TNCD-17;E-TNCD-18',gap='Live关闭的确认/旧顺序恢复、两地区帮助对应构建、删除撤销、旧缓存清理及正播章节进度映射未知；Live不是直播。'),
8:dict(pre='顶部无Tonie/游戏；所需内容完整缓存，Tonieplay首次须下完完整游戏',action='倒置盒子长按双耳，白环进入Parent Access；捏大耳听离线语音；同入口再捏大耳退出。游戏首次准备详见F12。',data='完整已缓存Tonie/Tonieplay可用；新内容不能激活，App不同步，日出闹钟等App功能被列为停用；需手动恢复在线。',es='E-TONI-09;E-TNCD-05;E-TNCD-07',gap='实体睡眠键离线范围、游戏断电/重置后缓存与进度持久性；不能把后续离线游戏推广为首次无网可玩。'),
10:dict(pre='App控制要求盒子在线；2代，1代不套用',action='Mine→盒子→Sleep Timer with Light，5–60分钟，显示剩余时间/橙光；可重启、停止或立即关机。双耳5秒按上次配置启用（默认30分钟）；再按5秒至橙光消失、提示音/绿灯关闭。',data='Settings→Bedtime & Sleep Light→Preferences调音量/亮度；设置在计时运行或日出闹钟启用时生效。日出闹钟另见F14。',es='E-TONI-11;E-TONI-07;E-TNCD-13',gap='换公仔、重启、离线转换及下一晚记忆的具体范围未测；灯持续整夜/到计时结束的条件与关盒后灯状态未说明清楚。'),
11:dict(pre='2代；需知道年龄模式、供电、灯环设置',action='1+充电不播且禁倾斜；App→盒子→Settings→Age Mode可切3+。无灯/蓝闪/红闪按原灯环帮助分流。读游戏盘失败：拔电开机，长按双耳越过睡眠设置至重启/Welcome Lights，稍候再试；详见下方恢复分支。',data='年龄模式、灯光、网络和固件分别解释。重启不等于恢复出厂；App中的Factory reset instructions是操作指南入口，不证明远程执行。',es='E-TONI-12;E-TONI-13;E-TNCD-08;E-TNCD-21;E-TNCD-24',gap='年龄切换确认、普通Tonie的2代持久续播、每个错误词及重置对云数据/游戏存档的具体范围仍未知；未执行重启/重置。')}
old_proposals=[]
for line in list(case.splitlines()):
    if not re.match(r'\| F-M038-\d\d \|',line):continue
    cells=[c.strip() for c in line.strip('|').split('|')]
    n=int(cells[0][-2:])
    if n not in changes:continue
    d=changes[n]
    updated=[cells[0],cells[1],d['pre'],d['action'],d['data'],d['es'].replace(';','；'),d['gap']]
    case=case.replace(line,'| '+' | '.join(updated)+' |')
    original=next(x for x in before if x['flow_id']==fid(n))
    replacement=original|dict(evidence_ids=d['es'],known_branches=d['data'],gaps=d['gap'],checked_on=DATE)
    old_proposals.append(dict(flow_id=fid(n),before=original,after=replacement,changes={k:{'before':original[k],'after':v} for k,v in replacement.items() if original[k]!=v}))
newlines=[]
for n,task,pre,action,data,es,gap in new:newlines.append('| '+' | '.join([fid(n),task,pre,action,data,es.replace(';','；'),gap])+' |')
case=case.replace('## 主要异常、世代差异和材料冲突','\n'.join(newlines)+'\n\n## 主要异常、世代差异和材料冲突',1)
case=case.replace('睡眠专页与离线页对离线后的本地计时范围没有说清，保留未知。','本批UK睡眠页补足本地关计时反馈和闹钟设置；与离线页合看仍不能确定所有离线本地计时行为。')
marker='## 对本项目的分析（推断）'
extra='''## 本批新增的异常与证据限制

- **游戏“暂停”不是单一状态**：E-TNCD-09按游戏类型说恢复，E-TNCD-10又以起点/检查点解释取盘重开，同时使用pause一词。两页并列保留，不补造逐秒暂停、统一存档或固定倒退五题。E-TNCD-11/12只给跳过/章节限制，未给手势和控制器细节，不能据此写出完整玩法。
- **2代恢复分级**：游戏盘未识别时，官方先给不插电、持续捏双耳重启至Welcome Lights，再等约一分钟重试；20–60秒是文档提示，非测速。最后手段为不插电、绿灯、倒置双耳5秒至提示音、扶正白环、长按小耳并依提示完成重置。App路径Mine→盒子→Settings→Factory reset instructions只打开说明。恢复出厂清回初始状态，但未列清云端录音及各游戏存档的传播范围。[E-TNCD-08/21]
- **家庭归属、设备重置和云删除分开**：受邀、接受、按公仔授权是三个步骤；Full权限帮助写的是Toniebox 1管理，不能推广成已核定2代全部权限。移交家庭会让旧Owner退出；删整个家庭则明确永久删录音。它们都不能替代原文件备份。[E-TNCD-14/15/19/20；E-TONI-18]
- **获取限制**：EU睡眠页一次正常HTTP请求返回403，已停止该直接抓取路线；权限页/US移交页web工具另有超时。部分事实来自官方搜索索引正文，不能称已取得原HTML或当前App界面。公开页中发现iframe但未播放；静态Image标签也没有作为观察证据。原始工具返回与逐源摘取保存在work并校验散列；正式库仅存短摘记。

'''
case=case.replace(marker,extra+marker,1)
case=case.replace('下一项最小观察：','本批启发（推断）：为G-002提供“重新开始”时，至少区分重新播音频、重开本轮、回检查点、清游戏进度与重置设备；儿童是否学会不能由按了控制器或游戏使用频次推断。家庭邀请的最小权限也应区分可编辑的内容对象与可管理的设备，仍是候选问题而非本项目既定权限方案。\n\n下一项最小观察：',1)
case=case.replace('均于 2026-10-26 获取官方正文；未实机。','E-TONI为2026-10-26初查；下列E-TNCD为2026-10-27续补。均未实机。',1)
case+='\n## 本批证据索引\n\n具体取得方式、地区与临时原件散列见[本批JSONL](../batches/tonie-controls-deepening-evidence.jsonl)；[短摘记](../artifacts/tonie-controls-deepening/source-notes.md)不包含完整转载。无新增视觉观察。\n\n| E编号 | 官方来源 | 定位/限制 |\n| --- | --- | --- |\n'
for e in evidence:case+=f"| {e['evidence_id']} | [{e['title']}]({e['url']}) | {e['locator']}；{'官方索引正文' if e['access_mode']=='official_search_index_text' else 'web正文文本'} |\n"
(LIB/'cases/M-038-tonie.md').write_text(case)
(W/'old-coverage-updates.json').write_text(json.dumps(old_proposals,ensure_ascii=False,indent=2)+'\n')

# 四个目录逐项登记；正式台账用简短功能标签，完整原标题仅留work原件。
labels={
1:'红灯;工作方式;同步;重启;恢复出厂;离线播放;无线干扰;扬声器无声;敲击失效;蓝灯卡住;赠礼;启动清单;初设;Wi-Fi类型;热点;多网络;换网络;无向导联网;异家庭绑定;搜不到网;Wi-Fi列表;连接失败;ID找不到;以太网初设;WEP;清洁;电池保养;印迹;磁体;ID分享;重新联网;离线模式;改名;亮度;语言;扬声器音量;App遥控范围;关机;调音量;段内跳转;跳曲;电量查看;充电时长;离底座关机;续航;替代电源;电池类型;仅底座运行;Turtle;Hedgehog;Groundhog;Elephant;Koala;Owl;Squirrel;Ant;Meerkat;密码错误;下载中断;乘机;继续支持;销售期限;保修替换'.split(';'),
2:'功能概览;产品介绍;更新同步;灯环指示;材质;磁体;使用数据;光敏提示;启动清单;初设用时;初设;热点;Wi-Fi要求;已有家庭;首次充电;蓝牙配网排错;初设排错;重新联网;播放控制;手动关机;多网络;离线模式;自动关机;App自定义;睡眠与闹钟;灯环用途;关闭灯环;年龄模式;旧Tonie兼容;新Tonie兼容;试用地点;预购;分批交付;旧机升级;促销;旧机抵扣;旧机折扣;电池类型;充电;电量;续航;电源;换电池;重启;恢复出厂;卡住;不开机;充电不播;Tonie不播;播错内容;下载慢;敲击失效;内容未更新;Squirrel;Frog;Rabbit;Parrot;Dog;Tiger;Horse;Cat;Llama;Zebra;Sheep;Fox'.split(';'),
3:'定义;故障提示;读盘失败;中断存档;章节选择;重启游戏;所需设备;适用年龄;1代兼容性;共同游玩;成人协助;跳过开场;App可见性;交互数据'.split(';'),
4:'移交家庭;创建账号;邀请成员;成员权限;家庭改名;移除1代;登录排错;删除家庭'.split(';')}
# 行号→已研究证据/任务；未列者只保留目录，不声称其正文已读。
links={
1:{3:('E-TONI-05',4),6:('E-TONI-02',2),32:('E-TONI-06',4),37:('E-TONI-07',10),40:('E-TONI-03',3),41:('E-TONI-04',3)},
2:{1:('E-TNCD-06',11),3:('E-TONI-10',9),4:('E-TONI-13',11),19:('E-TONI-08',7),22:('E-TONI-09',8),25:('E-TNCD-13','10;14'),28:('E-TNCD-24',11),44:('E-TNCD-08',11),45:('E-TNCD-21',11),48:('E-TNCD-24',11),51:('E-TNCD-07',12),53:('E-TONI-14',9)},
3:{1:('E-TNCD-23',12),2:('E-TNCD-07',12),3:('E-TNCD-08',11),4:('E-TNCD-09',13),5:('E-TNCD-12',13),6:('E-TNCD-10',13),7:('E-TNCD-05',12),9:('E-TNCD-23',12),10:('E-TNCD-23',''),12:('E-TNCD-11',13),14:('E-TNCD-22','')},
4:{1:('E-TNCD-19',17),3:('E-TNCD-15',15),4:('E-TNCD-14',15),8:('E-TNCD-20',18)}}
inventory=[]; originals=[]
for n in range(1,5):
    txt=(W/'extracted'/f'{n:02}.txt').read_text();domain='';items=[]
    for line in txt.splitlines():
        line=re.sub(r'^L\d+:\s?','',line)
        if line.startswith('## '):domain=line[3:]
        elif line.startswith('### ') and domain not in ['', 'Popular Articles','Topics']:items.append((domain,line[4:]))
    assert len(items)==len(labels[n]),(n,len(items),len(labels[n]))
    for j,((domain,title),label) in enumerate(zip(items,labels[n]),1):
        mapping=links[n].get(j);es=eid(n);fs='';status='index_only_unread';url='';gap='仅目录入口；正文任务/异常/返回未核，不提升D2。'
        if mapping:
            pe,f=mapping;es+=';'+pe
            fs=';'.join(fid(int(x)) for x in str(f).split(';') if x)
            status='related_evidence_partial';gap='关联已有或本批局部证据；非该条所有分支已完成，见case缺口。'
            if pe.startswith('E-TNCD'):url=next(e['url'] for e in evidence if e['evidence_id']==pe)
        if n==3 and j in [1,9,10,14]:status='discovery_only';gap='功能/数据类别说明；无完整交互或具体游戏规则。'
        inventory.append(dict(inventory_id=f'I-M038-{len(inventory)+1:03}',domain=f'{n}:{domain}',item=label,source_url=evidence[n-1]['url'],locator=f'{domain} / item {j}',article_url=url,coverage_status=status,evidence_ids=es,flow_ids=fs,gap=gap))
        originals.append(dict(directory_evidence=eid(n),item_number=j,domain=domain,original_title=title,label=label))
writecsv(LIB/'batches/tonie-controls-deepening-inventory.csv',inventory,['inventory_id','domain','item','source_url','locator','article_url','coverage_status','evidence_ids','flow_ids','gap'])
(W/'inventory-original-titles.json').write_text(json.dumps(originals,ensure_ascii=False,indent=2)+'\n')
(LIB/'artifacts/tonie-controls-deepening/source-notes.md').write_text('''# Toniebox 1/2 本批证据短摘记

查阅2026-10-27。24条官方证据记录，含目录和功能介绍；不是24次操作。E-TNCD-01/02/04/06/07/08/13/14/16/17/18/21/23/24采用官方搜索索引返回，其他为web工具打开所得文本。原件是工具返回快照，不是网页原HTML；没有取得实机或连续视觉证据。正式证据各项archived_artifact为空，本文件只是短摘记。

- 游戏恢复应按内容类型核对：F-M038-13并列保留auto-save与checkpoint帮助差异。
- 日出闹钟不能仅凭时间已保存判定会响：条件及未知见F-M038-14。
- 家庭授权、公仔归属、所有权移交、云数据删除是不同目标：F-M038-15至18。
- Live与手动排序的地区文案差异补回F-M038-06；没有将新功能重复计为任务。

原始快照/逐源摘取路径及SHA-256见同名evidence.jsonl。四目录台账共150条是所取得快照，不是全站完整目录。原始完整标题在work，正式台账仅简短功能标签。
''')
print(json.dumps(dict(evidence=len(evidence),new_flows=len(rows),old_updates=len(old_proposals),inventory=len(inventory)),ensure_ascii=False))
