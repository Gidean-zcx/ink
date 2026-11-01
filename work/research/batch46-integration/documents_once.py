import csv,json
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch46-integration'
brief='046-system-audio-and-content-state.md';bp=R/'docs/briefs'/brief;assert not bp.exists(),'Do not rerun'
details=json.loads((W/'new-task-details.json').read_text());cp=L/'cases/M-036-sony.md';s=cp.read_text()
s=s.replace('# M-036 Sony NW-A306：播放队列、歌单来源与文件索引','# M-036 Sony NW-A306：音频、系统控制与内容状态')
start=s.index('**D2 文档重建；');end=s.index('## 功能目录与覆盖')
s=s[:start]+'''**24项局部D2任务，没有实机或连续视觉操作。** 此前已有播放页及同版22/111页静态图；第46批复读59页正文，实际另看27、29、32、53、56、57、113共7页，其中32是文字步骤、57主要为文字和图标。四篇官方HTML与对应PDF核对；实际固件/构建/地区仍未知。全部静态页均不当执行结果，未安装、登录、付费、传输或修改设备。

本批逐页和取证范围见[系统与音频摘记](../artifacts/sony-system-audio/source-notes.md)。定时停止、时间点书签、来电打断与逐书续播仍无完整证据；音乐播放能力不自动等于有声书恢复能力。

'''+s[end:]
start=s.index('2026-10-26 复核官方143页');end=s.index('上述是此版 Help Guide')
s=s[:start]+'''[82项原始主题目录](../batches/sony-controls-deepening-inventory.csv)保留标题、URL和PDF定位。第46批在同一表更新39项，其中36项此前仅有目录、3项为复读；剩余16项仍只有目录，包含首次说明、介质注意、支持、维护和规格，不将它们误计为已分析任务。当前覆盖如下；此前“目录未展开”的判断随本批正文证据更新，历史来源及ID保留。

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

'''+s[end:]
section='''## 系统与在线内容任务（第46批）

以下均为同版文档重建；外部App的“按指引继续”不补成已经观察其内部步骤。完整定位见E-SONY-15追加采集及E-SYSA-01～05。

| F编号与任务 | 前提 | 动作与文档结果 | 异常、恢复和对象边界 | 依据/文件页 |
| --- | --- | --- | --- | --- |
'''
for d in details:
 section+=f"| F-M036-{d['n']:02d} {d['title']} | {d['pre']} | {d['steps']} | {d['gap']} | {d['ids'].replace(';','；')}；{d['pp']} |\n"
section+='''
原任务补充：F01的CD导入/Stop、ATRAC转换在电脑端，不是NW-A306原生处理；Mac章仅泛指适配软件。F02细化第8层音乐文件与该层文件夹的识别差别，根下音乐不识别；113图的内部存储根名sdcard不能凭名称当物理卡。F03把封面开播放器与媒体按钮控制、mini player内容区左右跳曲分开。F09补输出音量条件与360个性化对手机相机/兼容耳机/订阅的依赖；没有看到实际采集、传回与保存。[E-SONY-15，27–37、59–65、71、74、82、113–114；E-SYSA-01/04/05]

需要保留的文案差异：初始设置11页说完成后显示WALKMAN；27/29页均写Home返回WALKMAN，而33页允许选择主主页。按明确的可配置规则分析，不能宣称所有配置下固定回同一页。USB hub充电说明一处有条件支持、一处笼统列不可充，未实测前不自行统一。

58页警告有声音和文字，可OK取消后继续调高，并描述累计20小时再次提醒/降音量及重开降音量；它与AVLS上限不同。地区和实际阈值未固定，不能把这段当所有销售版本、所有输出或儿童听力安全承诺。AVLS例外来自57页及对应在线正文；未测切到耳机时的实际音量变化。

七页静态观察见[逐页记录](../artifacts/sony-system-audio/observed-pages.json)。56页数字68只是示例，不是默认或分贝；32页没有快捷面板截图，57页没有AVLS开关截图；53页卡挂载/数据库更新图标也不证明内容准备完成。

'''
anchor='## 对本项目的分析（推断）';assert anchor in s;s=s.replace(anchor,section+anchor)
s=s.replace('### 2026-10-27 本轮新增来源','### 此前一轮的新增来源（2026-10-27）')
s=s.replace('本轮修改的是研究记录：6个新任务、6个旧任务补证/收窄；','此前该轮修改的是研究记录：6个新任务、6个旧任务补证/收窄；')
s+='''
第46批新增来源为E-SYSA-01音量、02耗电、03主页、04在线内容四篇官方正文，以及05同源静态摘页组。直接URL、PDF页码和短摘见[本批来源笔记](../artifacts/sony-system-audio/source-notes.md)。原E-SONY-15追加本批59页范围；不覆盖旧捕获，也不宣称143页整册或设备全部能力完成。
'''
cp.write_text(s)

rp_path=L/'reference-points.csv';rp=list(csv.DictReader(rp_path.open()));assert not any(r['product_id']=='M-036' for r in rp)
data=[
('音量;儿童;输出;限制','22','E-SYSA-01;E-SYSA-05','56–58','manual-p57.png','已知当前音频输出，进入Sound设置','开启AVLS并改变输出方式','文档明确蓝牙/USB输出不受本机AVLS限制；58页可取消的警告另有规则','manual_text_and_embedded_icons','未观察开关保存或切输出后的实际音量','地区/阈值;耳机控制;跨输出继承;重启'),
('音量;App;上下文','22','E-SYSA-01;E-SYSA-05','56–57','manual-p56.png','在Music player与其他App之间切换','媒体音量/实体键或旋钮调整','手册为其他App指出Media volume步骤；旋钮68是静态示例','manual_text_and_static_controls','没有跨App或重启后的音量保持实测','返回播放器;音量单位;外部输出;实际声音'),
('按键;锁屏;暂停;防误触','08;19','E-SONY-04;E-SONY-15','15、55','source-notes.md','音频任务、HOLD与认证设置各自明确','开HOLD、关屏或设置认证','HOLD仅限制播放/音量键，触屏和电源仍可用；启用认证后屏灭自动锁','manual_text_only','未观察锁屏后音频、通知或恢复','错码;触屏误触;后台;重启断点'),
('主页;手势;配置','18','E-SYSA-03;E-SYSA-05','27–33','manual-p29.png','任一主页','左右滑动；或长按空白设置主主页并OK','换当前页与改变Home返回目标是两个动作；旧Home文案范围差异保留','manual_text_and_static_layout','设置确认是文档步骤；重启保存未证','初始结束页;取消;布局变化;实际当前目标'),
('音频;小组件;打开;控制','03;18','E-SONY-15;E-SYSA-05','27、34–36','manual-p27.png','WALKMAN小组件已有当前曲目','点封面或媒体按钮；播放器mini player内容区左右划','文档分别为打开播放器、直接播放控制和跳曲；未实际触发','manual_text_and_static_layout','未观察页面间同步或后台持续播放','暂停/打开副作用;误划;返回位置;第三方App'),
('导入;下载;目录;索引','02;21','E-SYSA-04;E-SONY-15;E-SYSA-05','78–79、113–114','manual-p113.png','服务内容已取得且格式/权限适用','移动到扫描目录或注册目录，再更新数据库','手册说明不在扫描范围则不显示；根下文件与索引中的内容不同','manual_text_and_static_diagram','未观察文件落地、索引成功或离线打开','受保护缓存;半文件;重复;空间不足;重扫失败'),
('电量;充电;状态','23','E-SONY-15;E-SYSA-05','16–20、53','manual-p53.png','Battery Care已开启或USB充电','看充电灯与Battery Care图标','文档充电灯亮/完成灭；Care图标即使没充电也显示，不能互换含义','manual_text_and_static_icon_table','未测容量、图标更新或关机后保留','USB hub冲突;低电量;过温;实际充满状态'),
('后台;电源;计时;恢复','23','E-SYSA-02;E-SONY-15','13–18','source-notes.md','暂停后闲置或长时间无操作','设置Auto power off为Off或具体选项','文档仍有72小时无操作关机条款；不是已测持续播放行为','manual_text_only','关闭设置不能据此保证永不关机；实际计时未测','持续播放;计时重置;唤醒;播放断点'),
('锁屏;密码;数据恢复','19','E-SONY-15','12、55','source-notes.md','已启用锁屏认证而忘记解锁方式','按手册联系服务处理','文档说明付费格式化清除认证；没有无损找回链','manual_text_only','手册明确数据清除代价；未执行恢复','错码次数;离线;卡数据;备份回导'),
('更新;备份;文件;恢复','24;16','E-SONY-15;E-SYCD-11','126–127及原传回电脑流程','source-notes.md','电源、Wi-Fi及事先资料留存','About device查版本；System updates按消息继续','手册给更新入口，歌曲详细信息可查File location用于文件留存','manual_text_only','文件副本不证明收藏/队列/账户/App整机恢复','进度;取消;断电;失败回滚;升级后断点'),
]
for i,(tags,nums,eids,pp,artifact,pre,act,res,basis,persist,unknown) in enumerate(data,1):
 rp.append(dict(reference_id=f'RP-M036-{i:02d}',product_id='M-036',question_tags=tags,flow_ids=';'.join('F-M036-'+n.zfill(2) for n in nums.split(';')),evidence_ids=eids,source_locator='同版PDF/印刷'+pp+'页',artifact_path='artifacts/sony-system-audio/'+artifact,precondition=pre,action=act,visible_result=res,evidence_basis=basis,persistence_result=persist,unknown_branches=unknown,version_scope='NW-A306 YY1301B1；Help Guide 5-042-672-11(5)；固件/播放器构建/地区未知',checked_on='2026-10-27'))
with rp_path.open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(rp[0]),lineterminator='\n');w.writeheader();w.writerows(rp)
assert len(rp)==88

bp.write_text('''# 第四十六期：系统音频与内容状态

2026-10-27。继续补既有产品的功能与交互；没有进入系统实现。

本轮从Sony NW-A306原82主题目录入手，将36个只有目录的条目补到正文，另复读3项。新增首次设置、桌面快捷、锁屏、联网安装、在线内容、音量、电池及更新8个局部D2任务；4个原任务补分支。固定手册复读59页，另核四篇官方在线帮助，实际看7页静态摘页。没有实机、听音或连续操作验证，所有65组产品仍部分覆盖。

## 从功能名深入到有效条件

| 常见概括 | 本轮取得的具体依据 | 对本项目的研究问题，尚非设计决定 |
| --- | --- | --- |
| 可以限制音量 | 本机AVLS对蓝牙和USB输出不生效；可取消的音量警告是另一机制 | 儿童听书切换耳机后，限制是否仍有效？AI应查询哪个输出的控制状态？ |
| 可以锁键/锁屏 | HOLD锁播放和音量键，触屏/电源仍可用；认证锁屏另设 | 暂停、关屏、防误触、访问控制分别需要什么反馈？ |
| 主页可以切换 | 左右滑换当前页；主主页设置决定Home返回目标 | 导航状态和用户配置怎样分别恢复？ |
| 有桌面播放器 | 点封面打开App，点媒体键直接控制；mini player另有左右跳曲 | AI怎样知道用户要查看内容还是改变播放状态？ |
| 可以在线下载 | 内容不在扫描目录就不进曲库，还需移动/注册目录及更新数据库 | 什么时候才足以回答“已导入，可以播放”？ |
| 自动关机可以关闭 | 手册仍有72小时无操作关机条款；持续播放适用范围未展开 | 后台计划、长时间听书和恢复需要验证哪些条件？ |
| 支持更新前备份 | 手册给歌曲文件定位与留存依据，没有完整整机恢复链 | 文件、队列、收藏、账户及App数据的备份结果如何核对？ |

音量输出范围见[Sony音量帮助](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733590.html)，下载到曲库的条件见[在线内容帮助](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733547.html)，72小时条款见[电源帮助](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733557.html)。这些是型号手册的规则，没有测试实际阈值、音量变化、持续播放或恢复，不能直接作为Plus实现和儿童听力效果的保证。

## 可反复引用的材料

[Sony案例](../research/interaction-library/cases/M-036-sony.md)现按24项任务整理；[独立来源摘记](../research/interaction-library/artifacts/sony-system-audio/source-notes.md)保留精确页码、四个HTML来源及7页观察。静态页中两页主要是文字，未当已看过快捷面板/AVLS开关。主页默认与充电hub文案差异也保留，没有自行补全。

[交互依据表](../research/interaction-library/reference-points.csv)新增RP-M036-01～10，可按前提、动作、结果依据、保存和未知分支筛选。资料库的功能目录入口同时修正几处过期概述，当前覆盖以原台账每行为准；历史简报仍供追溯，不替代当前案例。

尚缺实机取消、错误、断网、切输出、退出/重启与恢复。文件和引用检查只验证记录一致；学习效果、长期动力以及系统全局AI能力仍需各自证明，不能从播放器文档推导出来。
''')

latest='- [第四十六期：系统音频与内容状态](/home/workspace/reader/docs/briefs/'+brief+')：补音量输出范围、主页配置、锁屏、内容索引及电源条件；新增10条可检索依据。'
for p in [R/'README.md',L/'README.md']:
 lines=p.read_text().splitlines();i=next(i for i,x in enumerate(lines) if x.startswith('- [第四十五期：'));lines[i]=latest;p.write_text('\n'.join(lines)+'\n')
p=R/'docs/briefs/README.md';s=p.read_text();i=s.index('- [第四十五期：');p.write_text(s[:i]+latest+'\n'+s[i:])

add={
'analysis-status.md':'\n第46批继续Sony原82主题目录，36项仅目录补正文，另3项复读；59页固定PDF、4篇在线HTML与7静态页，8新D2、4旧F、5新E/4新URL、原PDF一次追加采集。RP增10条至88条。未新增实操，全部65组仍部分覆盖；见[简报46](../../briefs/046-system-audio-and-content-state.md)。\n',
'backlog.md':'''\n## 第46批后的Sony具体缺口

原82项目录已有36项从仅目录补正文，剩16项未展开；不再把桌面、音量、锁屏、电池和更新笼统写成未查。下一层需同构建实测：HOLD/屏灭/认证与音频的相互关系、应用/输出切换后的音量和AVLS、主页设置与左右滑的恢复、下载/注册/扫描的失败及取消、Battery Care指示和72小时条件、更新失败与完整恢复。

360个性化只查到跨手机相机/耳机/订阅的前提，外部App内部链没有展开。旧App名称、规格或正文“最新版”不当已固定版本。Mac章仅泛指传输工具，不编造当前软件教程；根目录图名sdcard不推物理卡。图像32/57页主要文字，不能补足真实开关界面。\n''',
'comparison.md':'''\n## 系统音频的有效范围（第46批）

[Sony F18/19/22/23](cases/M-036-sony.md)将当前主页/默认主页、HOLD/认证、应用音量/输出端上限和设置Off/额外闲置规则分开。与[PocketBook调光及目录同步](cases/M-003-pocketbook.md)、[BOOX通道静音和后台](cases/M-004-boox-neoreader.md)可按同一问题比较：动作名称相同不保证作用对象、前提和结果相同。以上是文档条件，不把静态设置当有效状态或设备实测。

对儿童听书，切耳机后控制范围与误触恢复值得优先验证；对成人连续学习，播放位置、App返回和索引就绪要分别保存依据。它们服务G-001/G-002的研究，未决定家长机制、系统接口或音量指标。RP-M036-01～10可直接回查。\n''',
'design-reference-guide.md':'''\n## 查询控制是否在当前条件下生效（第46批）

新增[Sony任务](cases/M-036-sony.md)和RP-M036-01～10：

| 要查的问题 | 直接依据 | 限制 |
| --- | --- | --- |
| 耳机切换后音量限制还有效吗 | [57页输出例外](artifacts/sony-system-audio/manual-p57.png)，RP01/02 | 这是规则文字，没有实际测音量；旋钮示例值不是分贝 |
| 锁按键是否也保护触屏/数据 | [15/55页摘记](artifacts/sony-system-audio/source-notes.md)，RP03/09 | HOLD、屏灭与认证分开；忘码格式化不是无损恢复 |
| 点主页封面与播放键是否同一动作 | [27页小组件](artifacts/sony-system-audio/manual-p27.png)，RP05 | 文档分别打开和控制，尚未实操 |
| 下载完为什么还不在曲库 | [113页路径图](artifacts/sony-system-audio/manual-p113.png)，RP06 | 文件、扫描范围和索引分开，图中sdcard不凭名称当物理卡 |
| 状态图标和设置开关说明什么 | [53页状态表](artifacts/sony-system-audio/manual-p53.png)及[电源摘记](artifacts/sony-system-audio/source-notes.md)，RP07/08 | Care图标不等于正在充电；Off不排除文档的72小时规则 |

更新前文件留存与完整数据恢复另看RP10。正式引用带上输出/应用/版本及未知分支；88条RP仍是增量依据，不是全库全部状态已结构化。\n''',
'batches/README.md':'\n第46批 `sony-system-audio-*` 新增8D2、5E（4篇对应HTML和1组同源静态页），4旧F及E-SONY-15追加采集在原持有批次更新。59页正文复读，7页实看静态页，39原目录行更新含36原仅目录；不新增目录副本。RP增10条，无新增产品操作；见[简报46](../../../briefs/046-system-audio-and-content-state.md)。\n',
}
for rel,t in add.items():
 p=L/rel;p.write_text(p.read_text().rstrip()+'\n'+t)
p=L/'backlog.md';s=p.read_text().replace('目前16条为增量示例，其余任务仍需','当前条目为增量示例，最新规模见reference-points.csv；其余任务仍需');p.write_text(s)
p=L/'inventories/README.md';s=p.read_text();replace={
'M004-BOOX阅读与笔记控件':'| [M004-BOOX阅读与笔记控件](../batches/boox-reading-controls-deepening-inventory.csv) | 59原文章加68正文标题/控件组，127行当前台账 | 已随第44批补31篇正文范围；跨型号/版本条件及当前状态看每行，不把全部目录当本机功能 |',
'M003-PocketBook应用与设置':'| [M003-PocketBook应用与设置](../batches/pocketbook-app-controls-deepening-inventory.csv) | 原65项当前台账，第45批又更新27项 | 历次正文/静态观察分别保存；父子目录不当独立功能，没有设备操作 |',
'M012-Kompakt核心App帮助':'| [M012-Kompakt核心App帮助](../batches/mudita-app-controls-deepening-inventory.csv) | 原111项当前台账，第37批已补原46个仅目录项 | 各型号/文档版/已读范围逐行记录，旧快照不当新增功能；无设备操作 |',
'M036-NW-A306帮助目录':'| [M036-NW-A306帮助目录](../batches/sony-controls-deepening-inventory.csv) | 82主题，第46批更新39项含36个原仅目录项；16项仍目录层 | 固定5-042-672-11(5)，不等同固件或Android全功能；当前任务及未验证分支另列 |',
'M044-Inoreader官方帮助目录':'| [M044-Inoreader官方帮助目录](../batches/inoreader-controls-deepening-inventory.csv) | 5大类下58分类、258篇主题，共316节点 | 已随第41批深化正文；具体审读状态看当前行，含Team/账单，构建与完整App菜单未固定 |',
}
lines=s.splitlines()
for i,line in enumerate(lines):
 for key,value in replace.items():
  if line.startswith('| ['+key+']'):lines[i]=value
p.write_text('\n'.join(lines)+'\n\n第46批Sony在原82行台账就地补39项范围；资料库入口同时修正PocketBook、BOOX、Kompakt和Inoreader的过期概述，历史批次数不当当前未读数。正文/目录和静态/操作继续分开。\n')
p=R/'docs/ideas.md';p.write_text(p.read_text().rstrip()+'\n\n2026-10-27继续I-021/I-022：Sony补系统主页、锁屏、应用/输出音量、内容索引、电源与更新条件，保留手册差异和实际状态未知；新增10条按条件检索依据。见[第四十六期简报](/home/workspace/reader/docs/briefs/046-system-audio-and-content-state.md)。儿童听书限制、成人恢复及全局AI动作结果仍为研究问题，未新增已确认需求或进入全系统设计。\n')
print(json.dumps(dict(case_tasks=24,new_references=10,total_references=len(rp),brief=brief),ensure_ascii=False))
