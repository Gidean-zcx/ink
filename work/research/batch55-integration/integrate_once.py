import csv, hashlib, json, copy
from pathlib import Path

R=Path('/home/workspace/reader'); L=R/'docs/research/interaction-library'; W=R/'work/research/batch55-integration'
A=L/'artifacts/shortcuts-entry-and-organization'; A.mkdir(exist_ok=True)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x): p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def rows(p): return list(csv.DictReader(p.open()))
def writecsv(p,rs):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rs[0]));w.writeheader();w.writerows(rs)
def eid(n): return f'E-SCEN-{n-1:02d}'
def fid(n): return f'F-M043-{n:02d}'
manifest=json.loads((R/'work/research/batch55-shortcuts-entry/http-manifest.json').read_text())
sources={int(x['key']):x for x in manifest}
assert len(sources)==46
for n,s in sources.items():
    assert s['status']==200 and s['body_read_by_root']
    assert s['selected_versions'][0]['text']=='iOS 27'
    if n!=1: assert s['body_id'].upper()==s['upstream_id']

# 每条为已读正文的短摘记；概念页不独立制造任务。
facts={
2:('概念与范围','建议基于设备使用历史；指令由动作构成，All/My/Share Sheet/Watch/App Shortcuts集合范围不同。','声明不证明所有App全部动作开放或本项目能访问所有数据。'),
3:('发现与导入','Gallery分类横向浏览、See All与搜索；打开卡片看说明和动作预览，Add或Set Up，完成设置后加入集合。','未核零结果、取消配置、同名重复、导入后权限与来源变更。'),
4:('运行与结果','集合卡片或编辑器运行；动作依次执行，缺数据/权限弹框；末步富内容可预览，运行中可停止。','与既有Completion页“编辑器外无预览”措辞范围不完全一致，保留冲突；停止不证明回滚。'),
5:('语音入口','唤起Siri后说指令名；文档称执行后报告结果。锁屏且要打开App时要求解锁；同名指令优先于标准Siri命令。','重名、误识别、取消解锁及结果准确性未核；不是任意自然语言意图映射的证明。'),
6:('系统入口','从右上向下或底部向上进入控制中心，添加控制项→Shortcut→Choose→选择指令。','手势依设备；文档主要说明配置，未给运行、取消及锁屏反馈。'),
7:('系统入口','支持机型的Settings→Action Button→滑到Shortcut→Choose a Shortcut→选指令。','原文机型范围写iPhone 15 Pro or later；不把配置步骤当实际按压时长、反馈或Plus按键规格。'),
8:('系统入口','适配iPad与Apple Pencil Pro；Settings→Apple Pencil→Squeeze→Shortcut→选择指令，挤压触发。','误挤压、运行中重复触发、锁屏与反馈未核。'),
9:('组件运行','长按桌面添加组件并选尺寸；小组件选单指令，大组件选集合；点按后显示进度、可取消，需输入时暂停，不适配动作转入主App。','大组件文字写first widgets，结合上下文保留为集合前列项目；数量/排序细节、取消后的结果和重进未知。'),
10:('系统入口','桌面中部下滑搜索指令名，结果含图标和名称；可Run后按提示执行，也可先打开App再运行，可关闭结果。','搜索无结果/重名、两路径选择条件、返回定位未知。'),
11:('系统入口','指令Details→Add to Home Screen；可选拍照/图库/文件图标，另改桌面显示名，Add后出现入口。','桌面显示名与指令本名联动、删图标是否删指令、重复入口及权限失败未知。'),
12:('共享输入','Details启用Show in Share Sheet并Done，链首出现输入定义；其他App选内容→共享→选指令，通知显示动作进度。','入口缺失可能是内容类型不匹配；未证任意App页面全量读取、取消或完成回传。'),
13:('手表入口','手机/平板Details开启Show on Apple Watch并Done；手表App或表盘复杂功能点按执行。','只显示已启用者；启用不证明每个动作可在手表完成，跨设备依赖/同步延迟未知。'),
14:('系统入口','Settings→Accessibility→Touch→Back Tap→双击/三击→选指令→Back Tap应用设置；相应敲击运行。','屏幕输入需另设；误触、锁屏、重复敲击与关闭恢复未知。'),
15:('概念与范围','动作从上到下运行，分别获取、转换或交付内容。','概念页，不提供失败/重试或持久结果保证。'),
16:('概念与范围','变量暂存内容；If分支、Repeat循环和Choose菜单改变执行路径。','与已读详细逻辑页关联，不把概述再计为新功能；取消/部分结果仍缺。'),
17:('概念与范围','按反复操作、步骤多和跨App缺口寻找自动化任务；学习/新闻/音乐等是用途举例。','例子未含完整实现、权限或结果证明；不自动升级成本项目功能。'),
18:('概念与范围','常见顺序获取→转换→交付，官方明确不是强制规则，可省略类别。','不能据排列模型承诺数据完整性或事务性。'),
19:('动作发现','用get/search/find/select关键词查获取动作；Date/Text/Contact/List等可直接提供有类型的值。','名称相似不证明取全库；具体权限、分页、空值与覆盖待核。'),
20:('对象转换','自动类型转换之外可主动筛选/裁切/转换，按make/filter/convert/edit/get/adjust查动作。','转换失败、精度、缺字段及原对象变化未给统一规则。'),
21:('交付对象','share动作包括新建对象、保存/追加文件和打开App；Open In发送工作副本，Open in Books可用于PDF/电子书。','副本与原件关系、保存回执、重复导入及目的App返回未知；不能从“交付动作”概括为已经写入。'),
22:('受限执行环境','相机/麦克风、选择器、Quick Look等可能需转入主App；数字Ask for Input、菜单/列表/Alert适合组件/手表。组件Unable to Load文档归因为内存不足，建议前置Continue Shortcut in App。Copy to Clipboard可Local Only或设Expire At。','建议不是已测试所有组合；没有精确内存阈值、实际转场或重试恢复结果。剪贴板过期不等于目标App副本删除。'),
23:('概念与范围','整理入口包括排序、自然语言修改、复制、删除、分享、同步与文件夹。','概览关联既有或本批具体页，不独立计任务。'),
24:('配置组织','建文件夹选名/图标并Add；改名图标Save；移入/新建时选择Move。自动集合里Remove保留All；删自建文件夹可Delete Shortcuts或Keep Shortcuts，最后Done。','空文件夹时没有Edit；删除撤销/跨设备恢复仍缺；Remove与Delete对象不同。'),
25:('配置组织','Grid/List切换；List可按加入顺序、名称、动作数、修改时间排序，再点同一方式切升降序。','列表排序与手动拖动、组件显示之间的优先级与持久化未知。'),
26:('配置组织','Edit后拖动，等待重排再松手，Done结束；拖跨分类会改变归属，文档称同账号其他设备同步更改。','组件顺序跟随指令顺序；实际同步完成、排序冲突、撤销未知。'),
27:('配置组织','编辑器名称旁下箭头→Rename→新名字→Done。','旧语音名称、URL或桌面入口是否继续有效、重名与改名撤销未核。'),
28:('配置组织','指令名旁图标可换颜色/图形，Done保存；新指令默认有15种颜色之一。','颜色/图形不是运行状态；桌面照片图标另有设置，灰阶可区分性未观察。'),
29:('配置组织','Edit多选后Duplicate，或长按Duplicate生成副本；同Apple Account其他设备也加入新指令。','未给副本权限/自动化/业务数据继承、命名冲突、重复执行去重与撤销保证。'),
30:('提醒回访','打开目标指令后对Siri说提醒此事，默认提醒列表添加以指令名为标题的项；可指定列表/时间，点提醒中的Shortcuts图标执行。','提醒出现不等于到点自动执行；改名/删除指令后链接、完成提醒对指令影响和恢复未知。'),
31:('查找范围','Find有输入时筛前一输出，无输入时自行从对应App取；Filter只处理传入内容。','找不到对象、空输入与显式Nothing的当前UI结果仍未运行。'),
32:('运行时输入','可用Ask/Alert/Notification或Ask Each Time在执行时补内容；日记/邮件为例子。','概述不保证通知等待确认、取消恢复或写入结果，继续沿已读具体页限定。'),
33:('共享输入','默认Any；限制类型影响共享菜单可见性；网页可含图像，接受图片的指令也可能出现在Safari。列18类输入，可用仅View Content Graph的指令探索宿主交出的对象。','URL、Safari网页、Article等并非同一对象；可识别字段不证明当前页数据完整，未执行探索样例。'),
34:('URL入口','App自己的scheme决定打开目标，指令中可用Open URL或Open X-Callback URL。','目标App需支持；不借概述推导完成或回调结果。'),
35:('URL入口','shortcuts://恢复App上次状态；create-shortcut进入新编辑器；open-shortcut?name=指向指定指令。','打开/编辑与run-shortcut执行分开；不存在/改名/同名、编码错误和来源校验未知。'),
36:('URL入口','gallery打开主Gallery；gallery/search?query=用URL编码词查询。','打开搜索结果不等于加入或运行指令；零结果、取消和保留查询未知。'),
37:('API概念','可从Web API取App未公开展示的原始数据，或调用服务；指向Typicode例子。','属于用途说明，必须结合鉴权/配额限制；不是系统全局访问权。'),
38:('API概念','说明网络API收发数据及读取/更新例子。','其“API可做则指令可做”的宽泛措辞不作无限能力保证；既有OAuth/额度限制仍适用。'),
39:('结构化内容写入','API返回列表逐项取name，用空格Split并取首/末项，取phone→Add New Contact映射字段，关闭Show Compose Sheet后运行会创建联系人。','页内Add to Contacts/Add New Contact/Add Contact命名混用；末段把变量指向“两次Split”但前文只有一次Split和两次Get Item，保留映射疑点。姓名分割不通用于所有语言；缺字段、重复/部分失败与回滚未知。'),
40:('日期转换','例子以1970-01-01 00:00:00 UTC为基点，用Adjust Date加UNIX秒，再Format Date输出；另提醒API也可用ISO格式。','长数字不能不核单位就套秒；文末“时间全为00:00”仅原例说明，不推广到任意时间戳；日期丢失/时区转换未测。'),
41:('日期转换','Format Date或日期变量属性可把日期格式化成文本。','概述不提供日期解析、目标App写入或持久化步骤。'),
42:('日期转换','日期和时间分别选Short/Medium/Long/None；None可省去一部分，时间Long含时区。示例明确限定英语且随locale变动。','展示字符串不能代替原时间对象；其他地区、夏令时及精度丢失未核。'),
43:('日期转换','How Long Ago/Until按最显著时间单位表示距今；Relative的Yesterday/Today/Tomorrow以运行当时日期时间为准。','显示是相对当前运行时间，不证明持续自动更新或事件真实到点。'),
44:('日期转换','帮助列ISO 8601和RFC 2822示例及排序建议，末段把编号写为RFC 2282。','保留原文编号差异；不推导混合时区/格式字符串能按真实时间正确排序。'),
45:('日期转换','Format Date选Custom出现Format String；按Unicode #35模式指定输出，可用单引号插入固定字样。','未运行模式、不推导自然语言理解/时区规则；链接的Unicode标准未在本批展开。'),
}
assert set(facts)==set(range(2,46))

new_specs=[
(19,'发现、预览并加入现成指令',[3,36],'Gallery分类/搜索、动作预览、设置后添加；URL可打开/搜索Gallery','取消配置、重复导入、来源更新、授权及加入结果未实际观察'),
(20,'用语音名称调用并接收结果',[5,27],'说指令名运行并报告结果；锁屏开App需解锁；同名指令优先标准Siri命令','误识别/重名、取消解锁、结果真实性、改名后旧命令及重复执行'),
(21,'配置系统手势、按键与桌面入口',[6,7,8,10,11,14],'控制中心、Action键、Pencil挤压、Back Tap、搜索和桌面入口分别配置；显示名可单独设置','各硬件适用、运行/取消/锁屏、误触与重复触发、桌面名联动和入口删除'),
(22,'在组件中执行并转入主应用',[9,22],'组件进度/取消、缺输入暂停；不适配动作打开App；Unable to Load建议前置Continue；数字输入和菜单有条件可用','精确内存/延迟、转场后位置、取消后部分产物、重试去重及返回'),
(23,'向手表开放指令并调用',[13,22],'Show on Apple Watch启用后出现；App/表盘复杂功能运行；按动作选择执行环境','同步延迟、手机依赖、离线/锁屏、全部动作兼容、取消恢复'),
(24,'整理、定位和复制个人指令',[24,25,26,27,28,29],'文件夹创建/移动；Remove与Delete；删夹保留/删内容；排序、改名图标、复制及同账号传播','取消/撤销、删除恢复、各排序优先级、权限/自动化继承、同步冲突与灰阶辨识'),
(25,'把指令放入提醒并回访执行',[30],'Siri上下文提醒写入默认或指定列表，名称为标题；指定时间/地点后点提醒中的图标执行','自动执行未证、旧链接失效、提醒完成/删除与指令关系、重复提醒及恢复'),
(26,'把结构化列表映射并批量创建联系人',[39],'逐项取姓名/电话、拆分首末项并映射联系人；关闭Compose Sheet后运行会创建对象','变量映射文内疑点、姓名结构/缺字段、取消、重复导入、部分成功与回滚'),
(27,'转换时间字段并生成显示文本',[40,41,42,43,44,45],'按UNIX秒示例调整基准日期；分别格式化日期/时间，运行时相对日期，Custom字符串','单位/时区/夏令时、格式丢失、跨locale解析、混合时区排序及目标写入；原文两处疑点'),
]
old_specs={
1:([2,4,15,17,18,19,20,23],'补27应用内逐步运行、缺输入/权限提示，以及动作发现/获取转换交付的非强制分类','图片按钮未观察，配置编辑与业务写入分开'),
3:([12,33],'共享入口开启后链首出现输入定义；类型匹配决定可见性，网页含图时图片指令也可能出现','宿主具体交出对象与类型转换未知，隐藏入口不等于动作不存在'),
4:([22,32],'组件/手表数字Ask for Input与菜单/Alert可用建议；相机/麦克风/选择器可能需转主App','提问类型与运行环境不同，不能承诺所有输入均留在组件'),
5:([5],'语音锁屏调用若要打开App，文档要求先解锁','取消解锁、先前动作及结果去留未知'),
6:([4],'应用内运行页说末步富内容可预览；既有Completion页称编辑器外无预览，适用范围有文档差异','两页预览条件未统一；停止不提供事务回滚或取消副作用保证'),
8:([20,21,22],'Open In明确交付副本；剪贴板可Local Only/Expire At，主动转换与自动转换分别处理','剪贴板到期不证明其他App副本清除，原件联动和类型损失未知'),
9:([31],'Find有输入筛上一输出，无输入才向自身App取；Filter处理传入集合','实际空输入/空结果及菜单反馈未验证'),
11:([24,26,29],'自动集合Remove保留All；删自建夹可保留或删内容；排序/复制有同账号传播说明','跨端实际生效、删除恢复、副本权限/自动化与业务记录继承未知'),
17:([34,35,36],'补打开上次状态、新建编辑器、按名称定位、打开/搜索Gallery的不同URL目的','打开与执行及真实写入分开；名称变动/缺目标/重复回调仍缺'),
18:([37,38,39,40],'补API列表映射到联系人及UNIX秒例子；宽泛API用途不替代鉴权/配额限制','字段映射文内疑点、写入去重/部分成功、时间单位校验未实际执行'),
}
newflows=[]
for n,task,ns,known,gaps in new_specs:
    newflows.append(dict(flow_id=fid(n),product_id='M-043',task=task,depth='D2',evidence_ids=';'.join(eid(i) for i in ns),known_branches=known,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-043-shortcuts.md',checked_on='2026-10-27'))
bodymap={n:[] for n in facts}
for n,_,ns,_,_ in new_specs:
    for k in ns:bodymap[k].append(fid(n))
for n,(ns,_,_) in old_specs.items():
    for k in ns:bodymap[k].append(fid(n))
for k in [16]:bodymap[k]=[fid(13),fid(14),fid(15)]

notes=['# 快捷指令：全局入口、执行环境与配置组织','',
'查阅2026-10-27。补读原目录46个条目：44篇操作/概念正文，另含欢迎和版权。版本选择器均为Shortcuts 10.0 / iOS 27；未安装、登录、运行指令、调用示例API、创建联系人、添加提醒或查看图片。部分按钮是图片，提取文本为空，不能凭空补图形。',
'', '原始HTML以gzip保存在work，分别记录压缩文件与解压原文SHA；正文定位、标题、目标ID和平台版本见[获取审计](fetch-audit.json)。HTTP200之后另核正文ID；欢迎页无article-section，改按明确内容区提取。欢迎/版权仅用于目录范围，没有新增操作任务。','',
'## 逐篇依据与未核分支','', '| E编号 / 官方出处 / 正文定位 | 已知内容 | 未核边界 |','| --- | --- | --- |']
for n,(category,fact,gap) in facts.items():
    s=sources[n];title=(R/s['text_work_path']).read_text().splitlines()[0]
    notes.append(f'| **{eid(n)}** [{title}]({s["final_url"]})；`{s["body_id"]}`；{category} | {fact} | {gap} |')
notes.extend(['','## 需要保留的文档差异','',
'应用内运行页描述末步富内容可显示预览；[先前Completion页](https://support.apple.com/guide/shortcuts/apda9578f70f/10.0/ios/27)却写编辑器外无预览。两页范围并未充分对齐，不能据无预览判断失败，也不应无条件承诺所有外部入口都有预览。旧E-SCDF-04保留，F06及RP-M043-10补入这项差异。',
'', '联系人例子的字段引用文字存在歧义，不能当可直接运行的正确程序；时间戳例子采用秒，日期格式页的英语示例不能套所有地区，RFC编号笔误和“时间全为00:00”原例说明均保留。此批没有执行验证这些示例。',
'','## 后续设计如何引用','',
'语音名称、入口配置、组件/手表执行、共享输入、提醒链接、内容副本与目标写入，分别查询对应F/E。新建9条文档任务，深化10条旧任务；新增14条RP并修订原预览依据。它们支持研究AI调用的入口和结果呈现，不证明本项目已经实现全局AI，也不直接确定Plus的按键、手势或运行时。',
'', '[案例](../../cases/M-043-shortcuts.md) · [按设计问题查依据](../../design-reference-guide.md) · [RP表](../../reference-points.csv)',''])
(A/'source-notes.md').write_text('\n'.join(notes))
existing=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()]; em={e['evidence_id']:e for e in existing}
audit=dict(checked_on='2026-10-27',sources=[],auxiliary_sources=[sources[1],sources[46]],existing_comparison_sources=[dict(evidence_id='E-SCDF-04',raw_work_path=em['E-SCDF-04']['raw_work_path'],raw_sha256=em['E-SCDF-04']['raw_sha256'],extracted_work_path=em['E-SCDF-04']['extracted_work_path'],extracted_sha256=em['E-SCDF-04']['extracted_sha256'],body_reread=True)],images_viewed=False,hands_on=False,account_used=False,api_requests_executed=False,limitations='No native UI, icon observation, device execution or actual side effects; some official wording conflicts retained.')
for n in facts:
    s=copy.deepcopy(sources[n]);s.update(evidence_id=eid(n),flow_ids=bodymap[n],locator='正文 '+s['body_id']);audit['sources'].append(s)
dump(A/'fetch-audit.json',audit)
new_e=[]
background={2,15,16,17,18,23,32,34,37,38,41}
for n in facts:
    s=sources[n]
    new_e.append(dict(evidence_id=eid(n),product_id='M-043',title=(R/s['text_work_path']).read_text().splitlines()[0],url=s['final_url'],source_type='official_help_background' if n in background else 'official_help',access_mode='ordinary_http_200_target_body_read',checked_on='2026-10-27',source_updated_on=None,version_scope='Captured official Shortcuts 10.0 / iOS 27 selection; installed build, device and region untested',app_build=None,firmware_version=None,locator='正文 '+s['body_id'],note=facts[n][1]+' 未核：'+facts[n][2],hands_on=False,visual_observed=False,archived_artifact='artifacts/shortcuts-entry-and-organization/source-notes.md',sha256=sha(A/'source-notes.md'),capture_state='target_article_id_and_version_verified_no_operations',supersedes=None,raw_work_path=s['raw_work_path'],raw_sha256=s['raw_sha256'],raw_encoding='gzip',decompressed_sha256=s['decompressed_sha256'],extracted_work_path=s['text_work_path'],extracted_sha256=s['text_sha256'],selected_version=s['selected_versions'],artifact_hashes={'artifacts/shortcuts-entry-and-organization/fetch-audit.json':sha(A/'fetch-audit.json')}))
assert not set(e['evidence_id'] for e in new_e)&set(em)
(L/'batches/shortcuts-entry-and-organization-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in new_e))
writecsv(L/'batches/shortcuts-entry-and-organization-coverage.csv',newflows)
changes=[]
for p in (L/'batches').glob('*-coverage.csv'):
    rs=rows(p);changed=False
    for r in rs:
        if r['product_id']=='M-043' and int(r['flow_id'][-2:]) in old_specs:
            n=int(r['flow_id'][-2:]);ns,k,g=old_specs[n];before=copy.deepcopy(r)
            r['evidence_ids']+=';'+ ';'.join(eid(i) for i in ns)
            if n==6:r['known_branches']='两篇27帮助对编辑器外预览的范围有差异；Stop and Output按接收上下文交结果，Alert确认/取消与Notification继续分开。'+k
            else:r['known_branches']+='；'+k
            r['gaps']+='；'+g;r['checked_on']='2026-10-27';changed=True;changes.append(dict(file=str(p.relative_to(R)),before=before,after=copy.deepcopy(r)))
    if changed:writecsv(p,rs)
assert len(changes)==10;dump(W/'old-flow-updates.json',changes)
# 目录证据补欢迎/版权边界记录，不把其内容计入44个新E。
ec=[]
for p in (L/'batches').glob('*-evidence.jsonl'):
    rs=[json.loads(x) for x in p.read_text().splitlines()];changed=False
    for e in rs:
        if e['evidence_id']=='E-SCCD-01':
            before=copy.deepcopy(e)
            for n in [1,46]:
                s=sources[n];e.setdefault('additional_captures',[]).append(dict(checked_on='2026-10-27',source_url=s['final_url'],role='welcome_or_copyright_reference_only',raw_work_path=s['raw_work_path'],raw_sha256=s['raw_sha256'],raw_encoding='gzip',decompressed_sha256=s['decompressed_sha256'],extracted_work_path=s['text_work_path'],extracted_sha256=s['text_sha256'],hands_on=False,visual_observed=False,note='只核原目录非操作条目正文；不新增功能或独立操作证据'))
            ec.append(dict(file=str(p.relative_to(R)),before=before,after=copy.deepcopy(e)));changed=True
    if changed:p.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in rs))
assert len(ec)==1;dump(W/'old-evidence-updates.json',ec)
ip=L/'batches/shortcuts-controls-deepening-inventory.csv';inv=rows(ip);byid={s['entry_id']:n for n,s in sources.items()};ic=[]
for r in inv:
    if r['entry_id'] not in byid:continue
    n=byid[r['entry_id']];before=copy.deepcopy(r);assert r['review_status']=='directory_only'
    if n in facts:
        r['evidence_id']=';'.join(dict.fromkeys([*r['evidence_id'].split(';'),eid(n)]));r['related_flow_ids']=';'.join(bodymap[n]);r['review_status']='body_read_documented_not_operated';r['gaps']=facts[n][2]
    else:r['review_status']='reference_body_read_no_flow';r['gaps']='欢迎/版权范围正文已读，不是独立操作任务；见本批fetch-audit auxiliary_sources'
    r['checked_on']='2026-10-27';ic.append(dict(before=before,after=copy.deepcopy(r)))
assert len(ic)==46;writecsv(ip,inv);dump(W/'inventory-updates.json',ic)

rp_path=L/'reference-points.csv';rp=rows(rp_path);rp_changes=[]
for r in rp:
    if r['reference_id']=='RP-M043-10':
        before=copy.deepcopy(r);r['evidence_ids']+=';'+eid(4);r['source_locator']='E-SCDF-04 Completion 对照 E-SCEN-03 Run from app；预览范围差异'
        r['artifact_path']='artifacts/shortcuts-entry-and-organization/source-notes.md';r['visible_result']='应用内运行页描述富内容预览，Completion页称编辑器外无预览；无连接线也不一律是错误';r['unknown_branches']+=';两篇文档预览条件的实际适用范围';rp_changes.append(dict(before=before,after=copy.deepcopy(r)))
points=[
(11,'语音;命名;锁屏',[20,5],[5,27],'已保存指令；Siri可用','说指令名；必要时解锁','执行后报告结果；同名指令优先标准命令','报告不证明业务结果真实','重名/误识别/取消解锁/旧名'),
(12,'按键;手势;入口',[21],[6,7,8,10,11,14],'支持相应硬件与系统入口','配置指令；按对应入口调用','可选择目标、桌面显示名和图标','配置完成与动作完成分开','误触/重复触发/锁屏/入口删除'),
(13,'组件;进度;转场;内存',[22,4],[9,22],'桌面组件与目标动作','点按运行，补输入，必要时转入App','组件进度/取消；Unable to Load有转App建议','未证明转场后进度持久保存','内存阈值/重复运行/部分产物'),
(14,'共享;类型;上下文',[3],[12,33],'宿主支持共享且传入匹配类型','启用共享入口，选内容后调用','可见性依输入，网页也可能提供图片','入口可见不证明页面全量读取','对象字段/类型转换/取消/回传'),
(15,'手表;设备;能力',[23],[13,22],'手机已启用Show on Apple Watch','Done后在手表App或表盘点按','已启用指令出现；动作有执行环境条件','开放入口不等于所有动作可独立完成','手机依赖/同步延迟/离线/中断'),
(16,'提醒;回访;自动化',[25],[30],'编辑目标指令并唤起Siri','提醒此事，之后点提醒内图标','默认或指定列表出现指令标题','提醒链接供回访；未证到点自动执行','改名/删除/完成提醒影响/重复'),
(17,'移除;删除;作用对象',[24,11],[24],'自动集合中的指令','选择Remove','从该集合移除但保留All','不是删除指令对象','跨端传播/撤销/其他入口'),
(18,'文件夹;删除;保留',[24,11],[24],'非空自建文件夹','删除文件夹后选择保留或删除其中指令','Keep移至All；Delete删除内容','容器和内容的删除有独立分支','取消/恢复/共享副本影响'),
(19,'排序;副本;同步',[24,11],[25,26,27,28,29],'个人指令集合与同账号设备','改布局/排序，拖动或Duplicate','排序、类别归属或副本改变','说明同账号传播，未核业务记录继承','排序优先级/副本权限/去重/冲突'),
(20,'能力发现;导入;预览',[19],[3,36],'Gallery可用','搜索/看动作预览，完成Set Up并Add','加入个人集合；URL仅打开或搜索Gallery','加入定义不证明运行或授权完成','取消配置/重名/来源变更'),
(21,'URL;导航;执行',[17,19],[34,35,36],'已存在目标或可访问Gallery','打开App/新编辑器/指定指令或Gallery','各scheme指向不同页面','导航成功不证明执行或数据写入','缺目标/改名/编码/取消'),
(22,'AI;导入;字段;批量写入',[26,18],[39],'API返回联系人样例列表','拆姓名、取电话、映射字段、关闭Compose Sheet','文档声明运行会创建联系人','真实写入未执行；源文字变量引用有疑点','姓名结构/缺字段/重复/部分失败/回滚'),
(23,'时间;单位;时区;显示',[27],[40,42,43,44,45],'确认源时间单位和格式','转换后格式化或按运行时生成相对日期','None可省字段；英语示例随locale变化','显示文本不等于原时间对象或持续更新','毫秒/秒/时区/夏令时/丢失精度'),
(24,'剪贴板;副本;生命周期',[8,22],[22],'Copy to Clipboard动作','设置Local Only或Expire At','限制Handoff或指定到期清除','不证明已粘贴到别处的副本被删除','到期执行/跨设备/目标副本/恢复'),
]
for num,tags,ff,ee,pre,action,result,persist,gaps in points:
    row={k:'' for k in rp[0]};row.update(reference_id=f'RP-M043-{num:02}',product_id='M-043',question_tags=tags,flow_ids=';'.join(fid(i) for i in ff),evidence_ids=';'.join(eid(i) for i in ee),source_locator='source-notes逐篇E编号及fetch-audit正文ID',artifact_path='artifacts/shortcuts-entry-and-organization/source-notes.md',precondition=pre,action=action,visible_result=result,evidence_basis='official_help_text_reconstruction_no_ui_or_execution',persistence_result=persist,unknown_branches=gaps,version_scope='官方Shortcuts 10.0 / iOS 27指南；设备/构建未实测',checked_on='2026-10-27');rp.append(row)
writecsv(rp_path,rp);dump(W/'old-reference-updates.json',rp_changes)
dump(W/'integration-summary.json',dict(new_flows=9,new_evidence=44,old_flows_updated=10,old_evidence_updated=1,auxiliary_captures=2,inventory_rows_updated=46,new_reference_points=14,old_reference_points_updated=1,actual_operations=0))
print('Applied 44 E, 9 D2, 10 old F, 46 inventory updates, 14 new RP and one corrected RP.')
