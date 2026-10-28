from pathlib import Path
import csv, json, hashlib, copy
R=Path('/home/workspace/reader'); L=R/'docs/research/interaction-library'; W=R/'work/research/batch34-anki'
assert not (W/'applied.json').exists(), 'already applied'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def readcsv(p): return list(csv.DictReader(p.open()))
def writecsv(p, rows):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def dump(p,x): p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
E={e['evidence_id']:e for e in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
F={f['flow_id']:f for f in readcsv(L/'coverage.csv')}
assert len(E)==1472 and len(F)==885
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
ip=L/'batches/anki-controls-deepening-inventory.csv'; inv=readcsv(ip)
target=[r for r in inv if r['status']=='directory_only'];assert len(target)==30
logs={}
for name in ['fetch-log.json','root-fetch-log.json']:
    for rec in json.loads((W/name).read_text())['records']:
        if rec.get('saved'): logs[rec['item_id']]=dict(rec,log=name)
assert len(logs)==30
# Concise root-reviewed source locators. A parent page is not counted as a task.
facts={
1:('Mobile Clients; Legacy Documentation','桌面手册与移动手册分开，翻译可能滞后；仅范围目录。'),
2:('Key Concepts; Shared Decks','笔记可派生多卡并分别调度；牌组与笔记类型独立。Get Shared下载后在客户端导入，再同步；不能直接把共享包加入AnkiWeb。'),
3:('Active Recall Testing; Spaced Repetition; Why Anki','介绍主动回忆和间隔复习；效果、遗忘比例与对其他产品评价为上游叙述，本批未据此证明学习效果或比较优势。'),
4:('Asking good questions; platform support routes','先查排错与已有问题；求助应提供操作、实际错误和已尝试步骤。论坛身份与AnkiWeb不同；没有提交求助或创建账户。'),
19:('MathJax; LaTeX / Web-Mobile; Cloze Conflicts','选TeX文本→MathJax inline→Cards预览；换行方式有要求。LaTeX图片须电脑生成，可经Check Media批量生成后同步；移动端显示已有图，不能生成。'),
23:('Multiple Users; Storage Location; Client Setup; Reverse Proxies','独立同步服务存集合/媒体副本；客户端Preferences配URL与AnkiWeb account凭据。协议版本、子路径末尾斜杠与媒体上传限制影响连接；自托管不同于离线备份。'),
25:('Sharing Decks Publicly; Sharing Decks Privately','公开分享先同步、网页Share；改牌组路径可能使更新无法匹配。接收者须重新下载导入，非自动推送。私享导出单牌组且去调度；新版包删除项不自动删接收者卡。'),
26:('Resources','只有外部资源清单链接，本页不证明具体操作；未声称读完所链论坛。'),
27:('Platform Notes','Windows/macOS/Linux目录父页；不作为独立功能。'),
28:('Installing; Problems','Windows安装与故障目录父页。'),
29:('Requirements; Installing; Upgrading; Add-on Compatibility','按x64/ARM64与Windows版本选包；关闭程序再升级，文档称保留卡数据。旧版本/不同发行包另有卸载说明，插件兼容和降级不能由升级承诺推出。'),
30:('Error opening file for writing; Antivirus issues','安装写文件失败：关闭程序/浏览器，仍失败则重启后重试。仅问题分流，未测修复结果。'),
31:('No error; Multiple installations; Debugging','启动无窗、缺依赖、多安装副本各有分支；控制台输出用于定位，25.07–25.09.4另有入口路径。历史Windows排错不冒充当前支持范围。'),
32:('Changing Driver; Full Screen','23.10+可在Preferences改显示驱动；旧版配置文件可切换/恢复。正文早段默认software与末段默认Direct3D表述不齐，保留版本疑点。'),
33:('Copy and paste issues','剪贴板监控工具可能干扰复制粘贴；文档没有稳定复现、具体错误UI或必然修复承诺。'),
34:('Text Size','网页区域缩放与菜单等原生界面缩放分开；环境变量是桌面排错方法，不是移动手势或电子纸刷新算法。'),
35:('Permission Problems; Debugging Permission Problems','正文讨论本地文件只读/权限错误，含access denied字样；这不是网站访问挑战。记录权限排错与日志提供路径，未执行权限或安全软件变更。'),
36:('Installing; Problems','macOS目录父页。'),
37:('Installing; Upgrading; Add-on Compatibility','下载并拖放应用；升级关闭后覆盖，文档称保留卡数据；旧macOS对应旧构建，插件兼容另核。'),
38:('Change Video Driver; eGPUs','23.10+ Preferences改驱动后需重启；旧版配置文件可恢复auto；外置显卡空白另有选项，未实测。'),
39:('Installing; Problems','Linux目录父页。'),
40:('Requirements; Installing; Upgrading; System Qt versions','官方包有架构/glibc/依赖条件，升级保留用户数据；发行版包与官方包、Qt/输入法适配不同，不能推断Plus可直接运行。'),
41:('Missing Libraries','从终端启动并依错误补依赖；诊断可能多轮，不把执行命令等同恢复完成。'),
42:('Changing Driver From Preferences / Terminal','Linux默认硬件加速；空白可试软件渲染，23.10+设置入口与旧配置方式分开。'),
43:('Blank Main Window','文档将部分空窗与glibc/Qt兼容关联，建议新版Qt6或所链历史讨论；链接方案未逐个执行或验证。'),
44:('Linux Distro Packages','发行版可能替换依赖或关闭更新检查，问题支持对象随包来源不同；不概括所有发行版包都不可用。'),
45:('Incorrect GTK Theme','指定GTK主题后需退出登录再登录；是环境排错，不是App内已观察设置过程。'),
46:('Wayland','2.1.48起可显式启用；文档列无边框、窗口无法前置等条件，实际发行版/当前构建未知。'),
47:('Fcitx','标准构建声称含fcitx支持但非所有发行版可用；与安装页关于PyQt特性受限一起保留，未测试中文输入。'),
51:('Card/Field Styling; Browser Appearance; Fonts; Fading; Javascript','Styling默认影响同笔记类型所有卡，可限定卡/字段；编辑字体与复习样式、浏览摘要模板分开。字体作为媒体且名称匹配；JS跨客户端/升级兼容不保证。')}
assert {int(r['item_id'].split('P')[1]) for r in target}==set(facts)
np=L/'artifacts/anki-content-and-maintenance/source-notes.md';assert not np.exists()
notes=['# Anki：内容对象、分发与维护来源摘记','','2026-10-27。官方Desktop手册公开Markdown正文；无Anki安装、登录、执行手册命令、图片观察或软件实操。当前实际构建未知。页内最低版本和历史排错保留，读取全部目录页面不等于所有分支已验证。','','| E编号 | 来源 | 章节与短摘记 |','| --- | --- | --- |']
newE=[]; mapid={}
for r in target:
    n=int(r['item_id'].split('P')[1]); eid=f'E-ANCM-{n:02}';mapid[n]=eid;cap=logs[r['item_id']];p=R/cap['path'];assert sha(p)==cap['sha256']
    assert r['source_url'] not in {e['url'] for e in E.values()}
    loc,note=facts[n]
    newE.append(dict(evidence_id=eid,product_id='M-020',title=r['title'],url=r['source_url'],checked_on='2026-10-27',source_type='official_manual',access_mode='downloaded_official_markdown_read',version_scope='Anki Desktop manual; explicit historical/minimum versions retained; actual build unknown; not complete AnkiMobile/AnkiDroid coverage',app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=False,visual_observed=False,archived_artifact=str(np.relative_to(L)),sha256=None,capture_state='official_markdown_and_durable_short_notes',supersedes=None,raw_work_path=cap['path'],raw_sha256=cap['sha256'],fetch_log_path=str((W/cap['log']).relative_to(R))))
    notes.append(f'| {eid} | [{r["title"]}]({r["source_url"]}) | {loc}；{note} |')
notes += ['', '## 同一来源的补读','','| 旧E | 本次复读章节 | 增补范围 |','| --- | --- | --- |',
'| E-AND-05 | Customizing Fields; Using Tags/Flags | 编辑字体/方向仅编辑域，排序字段只选一个；标签作用笔记，旗标作用单卡。 |',
'| E-AND-09 | Rows; Notes/Cards; Find and Replace; Finding Duplicates | 卡/笔记选区换算；多选隐藏编辑器。批量Toggle按当前项状态决定目标；替换默认仅选中笔记，可去限制。查重跨含同名字段的笔记类型，标签标记不等于合并删除。 |',
'| E-AND-10 | Simple searches; Limiting to a field | 普通词为子串匹配，限定字段默认全字段精确；富文本分段可影响命中。查询规则仍非完整操作符实测。 |',
'', '## 取证记录与冲突',
'', 'worker获取16页后，将权限帮助中的access denied字样误判为挑战并停止。本路线日志如实保留。root正常打开对应官方HTML，确认是本地权限排错正文，再以普通公开GET取得剩余14页；两份日志分别记录，没有变更身份、绕过访问限制或执行手册中的系统命令。原始16页未重抓。',
'', '目录初筛把Contributing当概念页；读正文后发现有公开/私下分发流程，改为F-M020-25。Background中的学习优势、遗忘比例及对竞品评价只记为上游说法，不当独立研究结论。Windows默认渲染器表述与Linux输入法条件不强行拼成单一当前版本事实。',
'', '本次30个新URL对应目录页，不是30个新功能。仅三项新任务；既有制卡、模板、浏览、导入及排错补回旧F。公式生成、同步与分发均没有实际成功/错误/恢复记录。']
before={}
def save(p):
    if str(p) not in before:
        q=W/'before'/p.relative_to(R);q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(p.read_bytes());before[str(p)]=sha(p)
oldF=[]; oldE=[]
def amend(fid, refs, known, gap, title=None):
    b=F[fid];a=copy.deepcopy(b);a['evidence_ids']=';'.join(dict.fromkeys(b['evidence_ids'].split(';')+refs));a['known_branches']+=' '+known;a['gaps']=gap
    if title:a['task']=title
    oldF.append(dict(flow_id=fid,before=b,after=a,fields_changed=[k for k in b if b[k]!=a[k]]))
amend('F-M020-05',[mapid[2],mapid[25]],'共享包需在客户端导入后同步到AnkiWeb；发布者更新不会自动送达既有用户，删除项也不自动从接收者集合删除。','23.10+包更新策略与旧分享说明须按实际版本核对；尚未操作更新预览、冲突、撤销或跨设备恢复。')
amend('F-M020-06',[mapid[2]],'Fields中排序字段仅一个；编辑字体/RTL与复习样式分开。类型与牌组独立，标签作用笔记及兄弟卡，旗标仅单卡。','字段移位/删除后的模板错误和恢复仍缺；尚未实测取消、输入法与批量撤销。')
amend('F-M020-07',[mapid[51]],'Styling默认跨同类型卡；可限定卡/字段，Browser Appearance仅表格摘要。媒体字体精确匹配名称；id=answer决定滚到哪里，淡入可关闭。JS不保证跨端或升级兼容。','已读CSS/HTML章节但非全部语言特性测试；客户端渲染差异、取消/保存/恢复与电子纸表现未测。')
amend('F-M020-10',[],'卡片模式选任一派生卡即可选中所属笔记；笔记模式隐含其全部卡。批量标记/停用/旗标按当前项状态设置整组选中对象。Find and Replace默认仅选中笔记，取消限制可扩大；Find Duplicates可跨同名字段类型，结果可统一打标签，非自动合并。','当前版本批量预览/确认/误操作撤销、跨重启历史和全部搜索组合未测；普通子串与字段精确匹配、HTML格式分段可能改变检索范围。')
maintenance=[4,29,30,31,32,33,34,35,37,38,40,41,42,43,44,45,46,47]
amend('F-M020-21',[mapid[n] for n in maintenance],'平台手册补安装/升级与启动排错：先区分安装源、依赖、权限、显示与输入法；macOS改驱动需重启。升级保留卡数据是文档承诺，不涵盖插件兼容或完整备份。','没有执行安装、升级、权限/驱动修复或数据库恢复；旧排错和当前安装要求混列，默认渲染器/输入法差异保留，不能据此保证修复。',title='安装、升级、扩展与启动故障定位')
def flow(num,title,refs,known,gaps):
    return dict(flow_id=f'F-M020-{num:02}',product_id='M-020',task=title,depth='D2',evidence_ids=';'.join(refs),known_branches=known,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-020-anki.md',checked_on='2026-10-27')
newF=[flow(23,'制作公式卡并准备跨端媒体',[mapid[19]],'字段输入TeX→选中→MathJax inline→Cards预览；式内换行用Shift+Enter。LaTeX另由桌面生成媒体，可Check Media批量预生成再同步；网页/移动端仅显示已有图片。','当前构建默认生成开关、渲染错误/缺图/同步中断及遮挡冲突未实测；公式显示不证明自动判题，Plus运行时与预处理方案未定。'),
flow(24,'把客户端连接到自托管同步服务',[mapid[23]],'服务端配置独立用户和集合/媒体目录；客户端Preferences指定URL、AnkiWeb account填该服务凭据；各端实际同步建立副本，不手工复制客户端数据目录。','未部署或验证登录/同步。客户端升级可能要求同步服务升级；UI仍称AnkiWeb，URL子路径需尾斜杠。备份保留、版本恢复和整机恢复不在此功能证明范围。'),
flow(25,'分享题库并发布后续修订',[mapid[25],mapid[2],'E-AND-04','E-AND-16'],'公开：同步后网页牌组Share，更新需同原名称/路径；私享：导出单牌组apkg且不含调度，再交给接收者导入。既有接收者需重新下载导入，新包删除项不会自动删其卡。','分享页确认、撤回影响、更新冲突/媒体缺失未实测；包导入策略以客户端版本为准，公开分发与家庭个人进度同步不同。')]
for eid,raw,loc,note in [('E-AND-05','editing.md','Customizing Fields; Using Tags/Flags','补字段属性及对象范围。'),('E-AND-09','browsing.md','Rows; Notes/Cards; Find and Replace; Finding Duplicates','补选区换算、批量目标及查重。'),('E-AND-10','searching.md','Simple searches; Limiting to a field','补子串/精确匹配与格式条件，非全部语法验证。')]:
    b=E[eid];a=copy.deepcopy(b);p=R/'work/anki-deepening'/raw
    a.setdefault('additional_captures',[]).append(dict(checked_on='2026-10-27',access_mode='local_reread_previously_captured_markdown',locator=loc,note=note,raw_work_path=str(p.relative_to(R)),raw_sha256=sha(p),archived_artifact=str(np.relative_to(L)),sha256=None))
    oldE.append(dict(evidence_id=eid,before=b,after=a))
assert not set(e['evidence_id'] for e in newE)&set(E)
assert not set(f['flow_id'] for f in newF)&set(F)
np.parent.mkdir(parents=True,exist_ok=True);np.write_text('\n'.join(notes)+'\n')
for e in newE:e['sha256']=sha(np)
for e in oldE:e['after']['additional_captures'][-1]['sha256']=sha(np)
prefix='anki-content-and-maintenance'
writecsv(L/f'batches/{prefix}-coverage.csv',newF)
(L/f'batches/{prefix}-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in newE))
for kind,changes,key in [('coverage.csv',oldF,'flow_id'),('evidence.jsonl',oldE,'evidence_id')]:
    for p in sorted((L/'batches').glob('*-'+kind)):
        rows=readcsv(p) if kind.endswith('csv') else [json.loads(x) for x in p.read_text().splitlines()];changed=False
        for i,r in enumerate(rows):
            c=next((x for x in changes if x[key]==r[key]),None)
            if c:
                assert r==c['before'];rows[i]=c['after'];changed=True;c.setdefault('owning_files',[]).append(str(p.relative_to(R)))
        if changed:
            save(p)
            if kind.endswith('csv'):writecsv(p,rows)
            else:p.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows))
assert all(x.get('owning_files') for x in oldF+oldE)
save(ip);no_task={1,3,26,27,28,36,39}
fm={2:'F-M020-05;F-M020-06',19:'F-M020-23',23:'F-M020-24',25:'F-M020-25',51:'F-M020-07'}
for r in target:
    n=int(r['item_id'].split('P')[1]);cap=logs[r['item_id']]
    r.update(status='body_read_no_task' if n in no_task else 'body_read_partial',evidence_ids=mapid[n],flow_ids='' if n in no_task else fm.get(n,'F-M020-21'),gaps='正文已读；范围/概念/目录页不计独立操作' if n in no_task else '已补本文步骤；实际版本、错误/取消/恢复未验证，详见案例与来源摘记',raw_work_path=cap['path'],raw_sha256=cap['sha256'],checked_on='2026-10-27')
writecsv(ip,inv)
cp=L/'cases/M-020-anki.md';save(cp);t=cp.read_text()
t=t.replace('其中25页有已读正文、30页仅目录','第34批补读后55页均已取得并阅读正文（含概念/目录父页）')
t=t.replace('也不是25页全部分支完成','也不是55页全部分支完成')
t=t.replace('CSS/JS全能力、删除与不同客户端恢复仍缺','已补样式、字体和JS兼容边界；语言全能力测试、删除与不同客户端恢复仍缺')
t=t.replace('所有字段属性、输入法、批量修改后的撤销边界','已补字段属性与输入法文档条件；批量修改后的实际撤销边界仍缺')
t+='\n## 第34批：内容范围、分发与维护\n\n原目录30项仅入口现已补正文，60行保持原编号；概念/父页不造新功能。Contributing初筛归类经正文修正为分享任务。来源见[逐页摘记](../artifacts/anki-content-and-maintenance/source-notes.md)，本批全为D2文档研究，没有安装运行、实操或观看嵌入图片/视频。\n\n| 任务 | 已补行为 | 仍未证明 |\n| --- | --- | --- |\n'
for c in oldF:
    a=c['after'];t+=f'| {a["flow_id"]} | {a["known_branches"]} | {a["gaps"]} |\n'
for a in newF:t+=f'| {a["flow_id"]} {a["task"]} | {a["known_branches"]} | {a["gaps"]} |\n'
t+='\n本项目启发（建议）：AI修改学习材料前应区分原始内容、派生练习、共享模板与个人记录；同步、备份、分发更新分别定义结果。数学内容的源文本、可显示资源和判题语义分开准备；并不要求Plus复用Anki运行时或自托管协议。\n'
cp.write_text(t)
dump(W/'changes.json',dict(new_evidence=[e['evidence_id'] for e in newE],new_flows=[f['flow_id'] for f in newF],old_flows=oldF,old_evidence=oldE,inventory_rows_updated=30))
dump(W/'before-manifest.json',before)
dump(W/'applied.json',dict(newE=len(newE),newF=len(newF),oldF=len(oldF),oldE=len(oldE),requirements_sha256=sha(R/'docs/requirements.md')))
print(json.dumps(dict(newE=len(newE),newF=len(newF),oldF=len(oldF),oldE=len(oldE)),ensure_ascii=False))
