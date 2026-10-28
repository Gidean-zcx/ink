from pathlib import Path
import csv,json,hashlib,copy
R=Path('docs/research/interaction-library'); W=Path('work/supernote-notes-controls-deepening'); A=R/'artifacts/supernote-notes-controls-deepening'; B=R/'batches'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,x):Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvwrite(p,rows,fields):
 with Path(p).open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(rows)
oldf=json.loads((W/'coverage-before.json').read_text()); olde=json.loads((W/'evidence-before.json').read_text())
assert len(oldf)==10 and len(olde)==18
notes='''# Supernote 核心笔记正文复读摘记

查阅日：2026-10-27。仅复用 2026-10-26 已公开取得的原件，本轮没有 HTTP 请求、账号或设备动作。来源仍是 E-SNCD-02 和 E-SNCD-10；没有新来源编号。文字为短摘意，页图只作静态参考。

## 原件与范围

- E-SNCD-02：[X2 User Manual V3.28.42](https://ib.supernote.com/x2/X2_V3.28.42_EN.pdf)，182 页；p9列 A5 X2 Manta、A6 X2 Nomad，未载发布日期。原件复制至 `work/supernote-notes-controls-deepening/raw/X2_V3.28.42_EN.pdf`，SHA256 `9c7b24be22b8f06727381564412f192273e50e2157a2aa5b4350d12b6c3037d2`，与旧 E 一致。新摘取 39 页文字不代表这些页所有功能已建任务。
- E-SNCD-10：[X2 版本日志](https://support.supernote.com/changelog-for-manta-and-nomad)，旧快照页面更新 2026-10-23；新原件副本 `raw/10-changelog.html` 的 SHA256 `28ce34428094d1f9778d5071ccb79a4bfdc7ea946170099001cc9e61823b1030`。没有重新确认 2026-10-27 网页或设备最新版本。
- 140 项目录沿用旧 PDF 书签，包括父子章节和前后说明。新 CSV 的 `this_batch_review` 区分本轮正文复读和继承登记。4.5 书签 p63、正文 p64 的差异保留。

## 文字定位与限定

- p28、59、64–67：图层限标准笔记；换模板和增删层不在当前页最近20次撤销内。PDF模板先选源页再应用；模板匹配页数、撤回和失败恢复仍无完整规则。
- p67–70、130–131：标题选区不可直接继续书写；关键词可键入并 Add 或选最近项。星标与主图层边界沿用旧F01。
- p74–76 + 日志 3.25.39（2025-11-14），Optimized / Text Box：文本框可进入编辑、调整和删除；新增指点框外退出。退出不是已证明持久保存或原笔迹同步更正。
- p81–84：整页复制/移动需选目标页与前后位置并 OK；当前页另用 Paste Page。p77–79的笔迹剪贴板、p131–133的纯文本剪贴板是不同对象，不能互套。
- p135–137：关键词和星标允许空查询；星标可输入文件名筛选。实时识别文本搜索需要输入识别文本。文本框搜索由3.24.38日志声明，但这些手册页未列其新分类，不重建缺失界面。
- p148–155：日历事件可由主层手写创建、在日期列表查阅、编辑和删除。授权后自动同步覆盖事件新增/修改/删除；离线冲突、来源笔迹双向变动、删除恢复不明。p150先清除全部手写的提示紧邻日期设置，受影响范围没有细化。
- 日志3.28.42（2026-05-22），Fixed：隐藏层仍被TXT/DOCX导出、换模板后重启/关机保存卡住，以及擦空另一层后主层链接不能点击，均为历史修复声明；未复现实测，不外推当前所有失败已排除。3.29.42再次列同类条目，不当独立证明。

## 静态观察审计

2026-10-27用 PyMuPDF 渲染并经 view_image 实际查看以下五个完整手册页。纸面页码等于 PDF 页序号。每页多个插图仍只是官方并列示意，不是连续操作观察。手册封面版本和图中示例日期不能证明某一设备当时实际运行构建。

| 页 | 实际可见 | 不证明 |
| --- | --- | --- |
| 65 | 模板 Customization、选源PDF页、Apply as Template；可见笔记/源页选择计数 | 多页对应策略、应用成功或可撤销 |
| 66 | Layers、加号、删除、显隐按钮和主层说明 | 增删层成功、隐藏等于删除、层恢复 |
| 75 | 文本框编辑/套索编辑、字体工具和尺寸控制点 | 校订正确、退出后保存完成 |
| 83 | Copy to 的 Cancel/OK、Select another note、前/后位置；More整页剪贴菜单 | Cancel后事务回滚、跨类型笔记兼容 |
| 153 | New Event与New Task分列；事件表单Cancel/OK、时间；日期选择示意 | 识别结果正确、成功通知、事件回源 |

图片均位于本目录 `x2-v3.28.42-p{65,66,75,83,153}.png`；五图哈希由 manifest 与旧E追加采集建议列出。旧E没有把visual_audit_path指向M006案例，因此本轮改案例不破坏旧审计引用；旧图片不改。
'''
(A/'source-notes.md').write_text(notes)
# case update is always built from immutable baseline, never from global files.
s=(W/'case-before.md').read_text()
s=s.replace('查阅：2026-10-26。固定对象为','查阅：2026-10-26；本地原件复读：2026-10-27。固定对象为')
s=s.replace('官方当前下载页仍指向此版，发布日志已到','2026-10-26 取得的官方下载页指向此版，同日保存的发布日志已到')
s=s.replace('原 F01–06 保留并补证；新增 F07–10。主要为 D2 文档重建，日志的独立功能声明只作 D1 修正。四张实际查看的官方手册摘页是静态参考，`hands_on=false`；无 D3/D4、连续过程或耗时测量。历史帮助不自动覆盖两台设备的全部年代。','现有 F01–10 与新增 F11–15 均以 D2 文档重建为限，日志的独立功能声明只作 D1 条件修正。两轮实际查看共九张不同手册页，均为静态参考，`hands_on=false`；无 D3/D4、连续过程或耗时测量。历史帮助不自动覆盖两台设备的全部年代。本轮复用 E-SNCD-02、10，没有新增来源或重新联网确认版本。')
s=s.replace('全部目录依据 E-SNCD-02；下表为索引，不长篇重述整册手册。','全部目录依据 E-SNCD-02；[本轮目录状态](/home/workspace/reader/docs/research/interaction-library/batches/supernote-notes-controls-deepening-inventory.csv)保留这140项，并区分继承的目录状态、本轮已读正文、已关联任务和仍未展开的内容。下表为索引，不长篇重述整册手册。')
s=s.replace('| 新建、识别、命名、封面、模板、图层 | 4.1–4.6 p58–67 | F08、09；E-SNCD-05 | 模板／图层完整链未研究 |','| 新建、识别、命名、封面、模板、图层 | 4.1–4.6 p58–67 | F08、09、11、12；E-SNCD-02、05、10 | 命名/封面仅目录；模板分配、增删层恢复未知 |')
s=s.replace('| 文本框、贴纸、笔迹与页面整理 | 4.10–4.13 p74–84 | 仅目录及文本搜索版本说明 | 未补成独立任务 |','| 文本框、贴纸、笔迹与页面整理 | 4.10–4.13 p74–84 | F13、14；E-SNCD-02、10 | 贴纸/笔迹整理正文已读，未建独立任务；整页搬移的结构保真未知 |')
s=s.replace('| 文件／保护、直线、星标、剪贴板 | 14.1–14.5 p124–132 | F01、10 | 锁定／跨设备保护等未补任务 |','| 文件／保护、直线、星标、剪贴板 | 14.1–14.5 p124–133 | F01、10；F13/14区分剪贴对象 | 纯文本剪贴正文已读但未建跨端任务；文件保护未展开 |')
s=s.replace('| 邮件、日历 | 第15–16章 p139–155 | 仅目录 | 授权／附件／事件／离线未研究 |','| 邮件、日历 | 第15–16章 p139–155 | F15；E-SNCD-02 | 日历正文已读；授权失败/离线冲突未说明；邮件仍仅目录 |')
s=s.replace('全局回找见 F08。','全局回找见 F08。指南 p68另规定标题选区内不可直接继续书写；p70允许键入关键词后 Add 或选择最近使用项，纠正输入不必被理解为改写原笔迹。层资格与一般撤销边界见 F12。',1)
s=s.replace('3.24.38 又增加文本框／文字搜索；旧四分类帮助与手册不能证明最新界面只有四类。[E-SUP-05]','p136–137进一步区分输入条件：Keyword、Star可空查询全部标记；Star填写的是文件名筛选，Handwriting需输入识别文本。3.24.38增加笔记内及全局的文本框文字搜索，但手册这些页仍只列四类；有能力声明不等于新分类入口已完整重建。[E-SUP-05、E-SNCD-02]')
s=s.replace('输出目录、同名覆盖、存储不足／取消／断电恢复仍待查。[E-SNCD-02]','输出目录、同名覆盖、存储不足／取消／断电恢复仍待查。[E-SNCD-02] 3.28.42日志称修复隐藏层仍被TXT／DOCX导出的问题；这是限定格式的历史修复声明，不能推断PNG／PDF或所有导出都按同一显隐规则处理。[E-SNCD-10]')
new='''## 核心笔记与日历任务

以下依据 V3.28.42 手册正文；只把足够明确的入口、动作和目标状态列为 D2。已看静态图不代替执行结果，故保存回执、失败与恢复仍单列。

### F-M006-11 将模板应用到选定笔记页

笔记点 Templates，或在 Pages 选一页/多页后点 Templates。内置模板与图片点选即应用；自定义来源为 Customization 或 MyStyle。PDF分支先选源PDF的一页或多页，再点 Apply as Template；手册称原PDF内的页间链接仍可导航。[E-SNCD-02，p64–65]

p65图可见源页选择和应用按钮，不能说明目标页数与源页数不一致时怎样分配。p28明确**模板更改不在一般Undo范围内**；重选旧模板与恢复原状态不是已证实同一动作。3.28.42曾修复设模板后关机/重启卡在保存提示的问题，未据此保证任意失败均可恢复。InkHub只是另一个资源来源，本任务未执行其下载或发布。[E-SNCD-02、10]

### F-M006-12 用图层组织内容并控制显隐

标准笔记点 Layers，用加号增加层；背景层和主层之外最多再加三层。选层后可用眼睛按钮隐藏/重新显示、上下箭头调整顺序、改名按钮输入名称并 OK，删除按钮针对所选层。实时识别笔记不支持图层；主层才可设标题、识别关键词、建链接与日历事件，不能把其他层的可见笔迹都当结构化入口。[E-SNCD-02，p28、59、65–67]

显隐是图层显示状态，删除是另一动作。手册没有给删除确认或恢复链；一般Undo限当前页最近20次，排除**增加/删除图层**及换模板，Redo恢复已撤销的当前页动作。没有资料证明切页、重启后仍保留这20步。3.28.42日志中“擦空另一层后主层链接不能点击”的修复，只作历史故障条件；导出显隐分支见 F09。[E-SNCD-02、10]

### F-M006-13 将内容放入文本框并结束编辑

笔记 Text 工具可新建文本框、粘贴文本或插入 Digest；也可套索笔迹选 Recognize as Text。手指双击文本框、笔长按，或套索后 Edit Text，进入编辑；可改字体、大小、加粗/斜体，移动及缩放。删除可选框点 Delete，或用套索/橡皮路径。[E-SNCD-02，p74–76]

3.25.39（2025-11-14）日志补明：手指点框外退出编辑，较早只能用笔；同版增添笔长按/套索入口。这补齐结束编辑动作，但没有保存成功回执，也不证明校订文字会回写原笔迹或来源Digest。文字、手写选区与Digest仍分对象。p131–133的剪贴板处理**纯文本**；桌面Partner跨端剪贴还要求Keyboard Sharing，未实际连接。[E-SNCD-10；E-SNCD-02]

### F-M006-14 在本笔记或另一笔记重排整页

Pages长按页面进入选择工具栏，多选后 Copy/Move；留在当前笔记或 Select another note，选择目标页及 Before page/After page，OK完成该命令。p83静态图有Cancel，但未观察取消后是否保留选区。另一条入口是 More → Cut Current Page/Copy Current Page，选粘贴位置后 Paste Page；不得把它等同笔迹套索粘贴。[E-SNCD-02，p81–84]

插入空页可从More或Pages上方功能条选择当前页前/后；删除可用Delete Current Page或Pages多选Delete。页删除是否可撤销、剪切未粘贴时退出/重启怎样恢复、标准与实时识别笔记之间是否兼容、页内标题/外链身份能否完整保留，所读章节未说明。p77–79另有手写Copy/Cut后套索变Paste、落笔粘贴和清剪贴板回套索的规则，不能直接套到整页对象。[E-SNCD-02]

### F-M006-15 把手写事项建为日历事件并维护

笔记主层套索手写 → New Event → 填详情与时间 → OK；月/周视图也可从新增事件入口建立，日视图点时段。月/周选日期后点日期格或右下角看当天事件，再点事件看详情；编辑后OK，事件列表/详情或编辑页可进入删除。p153将New Event和New Task并列，日历事件不能直接继承F07待办的完成勾选或回源能力。[E-SNCD-02，p66、152–155]

授权成功后才可选日历Auto-sync；开启后事件创建、修改、删除触发同步，另有手动同步。没有证据说明本地日历事件是否必须先授权才能创建，也没有离线冲突/去重/删除恢复规则。月/周视图可书写、打开Today’s Notes；p150在日期设置段提示先删除全部手写，但具体受影响对象与未删除时反馈不明，不解释为系统自动清空。事件改名是否影响原笔迹、是否能回到源页未说明。[E-SNCD-02，p148–155]

'''
s=s.replace('## 静态观察与剩余缺口',new+'## 静态观察与剩余缺口')
s=s.replace('四页从官方 PDF 渲染并实际查看，','此前四页从官方 PDF 渲染并实际查看，')
s=s.replace('下一最小观察：','本轮另查看 p65、66、75、83、153，逐图范围见[独立来源与视觉摘记](/home/workspace/reader/docs/research/interaction-library/artifacts/supernote-notes-controls-deepening/source-notes.md)。这些页分别支撑模板选择、图层面板、文本框编辑、整页目标位置、日历事件；没有连续操作或成功保存观察。\n\n下一最小观察：')
s=s.replace('H-004／H-005 可借鉴分类公开恢复范围','（推断）AI动作还应声明作用对象是笔迹、文本框、整页、图层还是日历事件；同名的复制/删除不应共享未经证明的恢复保证。模板和增删层不能一般撤销，提示AI在可恢复性不足时保留独立副本，是设计候选，不是Supernote已有自动保障。\n\nH-004／H-005 可借鉴分类公开恢复范围')
s=s.replace('均查阅 2026-10-26；新证据的获取方式、原件哈希在批次 JSONL。未登录或执行设备动作。','以下来源首次查阅均为2026-10-26；2026-10-27仅本地复读E-SNCD-02、10，原件散列、章节及图页范围见独立摘记。没有新增URL或重新确认线上当前版本；未登录或执行设备动作。')
(R/'cases/M-006-supernote.md').write_text(s)
# old coverage suggestions; main/old batches remain untouched.
revisions={
'F-M006-01':{'known_branches':'标题可再套索编辑且选区内不可直接书写；关键词可键入Add/选最近项；主图层限制；p131 Erase Star不擦标题内星标；历史帮助称标记删除不可撤销','gaps':'删除标记后的原笔迹、当前版本恢复/星标识别误触未知；改关键词不证明改写原笔迹','checked_on':'2026-10-27'},
'F-M006-08':{'known_branches':'Keyword/Star可空查全部；Star输入文件名筛选；Handwriting需识别文本并回页；File/Star按File回最后页；后台识别延后/触摸暂停；3.24.38文字搜索仅日志能力补充','gaps':'3.24.38后文本框搜索完整界面、无结果/误识别修正、实际耗时及重启队列未知','checked_on':'2026-10-27'},
'F-M006-09':{'evidence_ids':'E-SNCD-02;E-SNCD-05;E-SNCD-07;E-SNCD-10','known_branches':'PNG/PDF与识别TXT/DOCX；标准笔记导出时识别；实时识别语言创建后固定；QR/InkHub另列；3.28.42称修复隐藏层仍进入TXT/DOCX导出','gaps':'其他格式显隐规则、输出目录、重名/取消/存储不足、编辑后结构关系往返与原笔迹回写未知','checked_on':'2026-10-27'}}
updates=[]
for obj in oldf:
 r=obj['record']; fid=r['flow_id']
 if fid not in revisions:continue
 after={**r,**revisions[fid]}
 updates.append({'flow_id':fid,'source_batch':obj['source_batch'],'changes':{k:{'before':r[k],'after':v} for k,v in revisions[fid].items() if r[k]!=v},'replacement_row':after})
dump(W/'existing-coverage-updates.json',updates)
base={'product_id':'M-006','depth':'D2','evidence_ids':'E-SNCD-02;E-SNCD-10','visual_status':'static_reference_only','hands_on':'false','case_path':'cases/M-006-supernote.md','checked_on':'2026-10-27'}
flows=[
('11','将模板应用到选定笔记页','Templates/Pages选目标页；内置/图片点选，PDF选源页后Apply as Template；原PDF页间链接；模板修改排除一般Undo','多源/目标页数量不等的分配、失败/重启恢复未知；3.28.42修复声明非实测'),
('12','用图层组织笔记并控制显隐','仅标准笔记；背景/主层外最多3层；新增/显隐/排序/改名OK/删除；结构入口限主层；当前页20步Undo排除增删层及换模板','删层确认与恢复、切页/重启后的撤销历史、跨层链接异常的当前表现未知'),
('13','创建和校订文本框并退出编辑','Text/Recognize as Text；双击/笔长按/套索Edit Text；格式/尺寸/删除；3.25.39支持手指点框外退出；纯文本剪贴/桌面Keyboard Sharing分开','退出后的保存回执、OCR正确率、修改回写原笔迹或Digest、删除恢复未知'),
('14','在笔记内或跨笔记重排整页','Pages长按多选Copy/Move；目标笔记和目标页前/后；OK；当前页Cut/Copy另用Paste Page；插入/多选删除入口','Cancel后的选区、剪切未粘贴恢复、跨类型兼容、页内结构/外链保真、删除撤销未知'),
('15','从手写建立和维护日历事件','主层New Event/时间/OK；月周日视图查阅/编辑/删除；已授权且启用Auto-sync时事件变更触发同步；Today’s Notes分开','本地创建是否需授权、原笔迹双向变动/回源、离线冲突/删除恢复、日期设置清手写范围未知')]
rows=[]
for num,task,known,gap in flows:
 row={**base,'flow_id':f'F-M006-{num}','task':task,'known_branches':known,'gaps':gap}
 if num in ['14','15']:row['evidence_ids']='E-SNCD-02'
 rows.append(row)
fields=list(oldf[0]['record']);csvwrite(B/'supernote-notes-controls-deepening-coverage.csv',rows,fields)
(B/'supernote-notes-controls-deepening-evidence.jsonl').write_text('')
# Derivative inventory preserves 140 hierarchy entries and inherited versus reread states.
inv=list(csv.DictReader((R/'artifacts/supernote-controls-deepening/functional-inventory.csv').open()))
review={
'2.4':('F-M006-11;F-M006-12','p28撤销/层正文复读；其他工具栏条目未全展开'),
'4.1':('F-M006-12','只补标准/实时类型前提；命名/取消/默认值未分析'),
'4.2':('F-M006-08;F-M006-09;F-M006-12','p59不支持层；识别误差与重启仍未验证'),
'4.5':('F-M006-11','模板分配/失败/回退未知；Undo排除换模板'),
'4.6':('F-M006-12','标准笔记层限制、显隐/排序/改名；删除恢复未知'),
'4.7':('F-M006-01','标题选区不可直接书写；删后原笔迹与恢复未知'),
'4.8':('F-M006-01','键入Add/最近项；误识别校订与原笔迹关系未知'),
'4.9':('F-M006-02','复读原链；无新增任务或字段；目标删除/重命名稳定性未知'),
'4.10':('F-M006-13','编辑/删除/退出有文档；持久保存回执与回写未知'),
'4.11':('','正文已读；新贴纸/集合/导入范围尚未建独立任务'),
'4.12':('','正文已读；笔迹整理未建独立任务，不由整页流程覆盖'),
'4.12.1':('','正文已读；笔迹Copy/Cut/Paste/清空未建独立任务'),
'4.12.2':('','正文已读；拖动笔迹未建独立任务'),
'4.12.3':('','正文已读；缩放笔迹未建独立任务'),
'4.12.4':('','正文已读；旋转笔迹未建独立任务'),
'4.13':('F-M006-14','整页流程；结构保真与恢复未说明'),
'4.13.1':('F-M006-14','插入/删除支路；删除撤销未知'),
'4.13.2':('F-M006-14','跨笔记/目标页前后/OK；跨类型兼容未知'),
'4.13.3':('F-M006-14','当前页Cut/Copy/Paste；未粘贴中断未知'),
'4.14':('F-M006-09','导出入口复读；仅增隐藏层TXT/DOCX历史修复限制'),
'14.4':('F-M006-01','星标主层/标题内例外沿用；误触恢复未知'),
'14.5':('','纯文本及Keyboard Sharing条件已读，未建跨端独立任务'),
'14.8':('F-M006-08','补输入/空查询范围；文本框搜索完整入口未列'),
'16.1':('F-M006-15','授权成功后的同步条件；授权过程未实际执行'),
'16.2':('F-M006-15','仅设置/清全部手写提示的限制；受影响对象不明'),
'16.3':('F-M006-15','事件查阅前提；月周可书写；未独立重建视图任务'),
'16.4':('F-M006-15','事件日期选择前提；未独立建立日期导航任务'),
'16.5':('F-M006-15','创建/查阅/编辑/删除正文；回源/冲突/删除恢复未知'),
'16.6':('F-M006-15','相关入口Today’s Notes；未将其当事件回源')}
for r in inv:
 r['prior_coverage']=r['coverage']; r['this_batch_review']='inherited_prior_only';r['reviewed_on']='';r['source_snapshot']='E-SNCD-02 V3.28.42; captured 2026-10-26'
 if r['section'] in review:
  fs,gap=review[r['section']];r['this_batch_review']='body_read_this_batch';r['reviewed_on']='2026-10-27';r['flow_ids']=fs;r['gap']=gap;r['coverage']='partially_documented' if fs else 'body_read_no_new_flow';r['evidence_ids']='E-SNCD-02'
  if r['section'] in ['2.4','4.5','4.6','4.10','4.14','14.8']:r['evidence_ids']+=';E-SNCD-10'
 if r['section'] in ['4.5','4.6','4.10','4.13.2','4.13.3','16.5']:
  r['page_note']+=' 本轮实际查看p'+{'4.5':'65','4.6':'66','4.10':'75','4.13.2':'83','4.13.3':'83','16.5':'153'}[r['section']]+'；仅为静态摘页，未操作。'
csvwrite(B/'supernote-notes-controls-deepening-inventory.csv',inv,list(inv[0]))
# Additional-capture proposals; do not overwrite old evidence or old source artifacts.
cap=[]
for eid,raw,locator,visual in [
('E-SNCD-02',W/'raw/X2_V3.28.42_EN.pdf','p28,58–59,64–84,130–133,135–137,148–155; actual static pages65/66/75/83/153',True),
('E-SNCD-10',W/'raw/10-changelog.html','Chauvet3.24.38 Added Text Box/search; 3.25.39 Optimized Text Box exit; 3.28.42 Fixed hidden-layer export/template shutdown/link issue',False)]:
 obj=next(x for x in olde if x['record']['evidence_id']==eid)
 capture={'checked_on':'2026-10-27','capture_method':'local_reread_of_existing_public_snapshot_no_network','original_capture_on':'2026-10-26','url':obj['record']['url'],'raw_work_path':str(raw),'raw_sha256':sha(raw),'source_updated_on':obj['record']['source_updated_on'],'locator':locator,'archived_artifact':str((A/'source-notes.md').relative_to(R)),'sha256':sha(A/'source-notes.md'),'hands_on':False,'visual_observed':visual,'note':'复读既有原件，不是新HTTP抓取或独立来源；版本不自动延伸为当前实机状态'}
 if visual:
  capture['artifact_hashes']={str(p.relative_to(R)):sha(p) for p in sorted(A.glob('*.png'))}
  capture['visual_audit_path']=str((A/'source-notes.md').relative_to(R));capture['visual_audit_sha256']=sha(A/'source-notes.md')
 before=obj['record'].get('additional_captures',[])
 cap.append({'evidence_id':eid,'source_batch':obj['source_batch'],'field':'additional_captures','before_exists':'additional_captures' in obj['record'],'before':before,'after':before+[capture],'append':capture})
dump(W/'existing-evidence-additional-captures.json',cap)
dump(W/'build-results.json',{'new_evidence':0,'new_flows':len(rows),'old_flow_rows':len(updates),'old_flow_fields':sum(len(x['changes']) for x in updates),'old_evidence_capture_rows':len(cap),'inventory_rows':len(inv),'body_read_this_batch':sum(x['this_batch_review']=='body_read_this_batch' for x in inv),'new_static_pages':[65,66,75,83,153],'external_state_changed':False})
print((W/'build-results.json').read_text())
