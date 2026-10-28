from pathlib import Path
import csv, json, hashlib, copy, shutil

R=Path('/home/workspace/reader'); L=R/'docs/research/interaction-library'; W=R/'work/research/batch36-integration'
RW=R/'work/research/batch36-readwise'; MO=R/'work/research/batch36-moaan'; X=R/'work/research/batch36-x2'
assert not (W/'applied.json').exists(), 'one-time builder already applied'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p): return list(csv.DictReader(p.open()))
def wc(p,rs):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rs[0]));w.writeheader();w.writerows(rs)
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
E={e['evidence_id']:e for e in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
F={f['flow_id']:f for f in rows(L/'coverage.csv')}
assert len(E)==1517 and len(F)==899
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
before={}; created=[]
def save(p):
    if str(p) not in before:
        q=W/'before'/p.relative_to(R);q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(p.read_bytes());before[str(p)]=sha(p)
def edit(p,fn):save(p);p.write_text(fn(p.read_text()))
def add(p,body):
    assert not p.exists(), str(p)
    p.parent.mkdir(parents=True,exist_ok=True);p.write_text(body);created.append(str(p))
def cp(src,dest):
    assert not dest.exists(),str(dest)
    dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dest);created.append(str(dest))
for name in ['coverage.csv','evidence.jsonl','catalog.csv','case-index.md']:save(L/name)
rn=L/'artifacts/readwise-graph-renderer/source-notes.md'
xn=L/'artifacts/x2-remaining-tasks/source-notes.md'
mn=L/'artifacts/moaan-legacy-media/source-notes.md'
add(rn,(W/'readwise-notes.md').read_text());add(xn,(W/'x2-notes.md').read_text())
for n in ['highlights.json','connections.json','generation-log.json']:cp(RW/n,rn.parent/n)
for n in ['states.json','actions.json','environment.json','network.json','errors.json','narrow-fresh.json','00-initial.png','03-cross-source-from-panel.png','05-onscreen-esc.png','06-keyboard-escape.png','07-singleton-source.png','10-narrow-fresh-panel.png']:
    cp(RW/'renderer-observations'/n,rn.parent/n)
for n in [9,33,51]:cp(X/f'X2-file-{n}.png',xn.parent/f'X2-file-{n}.png')
mo=json.loads((MO/'root-media-sampling.json').read_text());assert len(mo['records'])==8 and not mo['stopped']
mt=['| E / 媒体 | 本地采样点 | 图像 |','| --- | --- | --- |'];mo_assets={};seen=set()
for rec in mo['records']:
    assert rec['http_status']==200
    for s in rec['screenshots']:
        if s['sha256'] in seen:continue
        seen.add(s['sha256']);p=R/s['path'];assert sha(p)==s['sha256']
        name=f'media-{rec["index"]:02}-{s["timepoint"]}.png';dest=mn.parent/name;cp(p,dest)
        mo_assets.setdefault(rec['evidence_id'],{})[str(dest.relative_to(L))]=sha(dest)
        mt.append(f'| {rec["evidence_id"]} / [{rec["alt"]}]({rec["url"]}) | 加载后约{s["timepoint"]} | [查看]({name}) |')
assert len(seen)==15
add(mn,(W/'moaan-notes.md').read_text().replace('<!-- CAPTURE_TABLE -->','\n'.join(mt)))
cp(MO/'root-media-sampling.json',mn.parent/'media-sampling.json');cp(MO/'media-sampling.json',mn.parent/'worker-media-sampling.json')

newE=[dict(evidence_id='E-RWGR-01',product_id='M-015',title='Readwise高亮图生成器：源码与合成样本控件',url='https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/highlight-graph/build_graph.py',checked_on='2026-10-27',source_type='official_example_renderer_source',access_mode='anonymous_source_read_and_local_synthetic_fixture',version_scope='Fixed commit 2d1ce9627c611d24f510dfc2e05a123fa509d2f6; local synthetic HTML only, NOT Reader native app or account',app_build=None,firmware_version=None,source_updated_on=None,locator='22–38 filtering; 124–159 legend/count; 315–398 selection/filter/panel; 562–568 Escape; CSS panel width460',note='完整源码核读；合成数据运行核对过滤、来源计数、关联跳转、关闭/清筛选、重载和320px窄屏。没有AI生成、真实书库或产品端到端操作，F33仍D2。',hands_on=False,visual_observed=True,visual_kind='synthetic_fixture_static_frames',local_fixture_run=True,local_fixture_audit_path=str(rn.relative_to(L)),local_fixture_audit_sha256=sha(rn),archived_artifact=str(rn.relative_to(L)),sha256=sha(rn),capture_state='source_notes_synthetic_inputs_logs_and_representative_frames_archived',supersedes=None,raw_work_path=str((RW/'build_graph.py').relative_to(R)),raw_sha256=sha(RW/'build_graph.py'),source_commit='2d1ce9627c611d24f510dfc2e05a123fa509d2f6',git_blob='263ac15b38e5f809fa7e5a5e4321a9913d28af09',artifact_hashes={str(p.relative_to(L)):sha(p) for p in rn.parent.iterdir() if p.is_file() and p!=rn})]
oldE=[]
def evchange(eid,a):
    oldE.append(dict(evidence_id=eid,before=E[eid],after=a,fields_changed=[k for k in set(E[eid])|set(a) if E[eid].get(k)!=a.get(k)]))
a=copy.deepcopy(E['E-RWSK-10']);a['note']='原工作流定义的初次核查仅限文字；2026-10-27另读并以合成数据运行其渲染器，见E-RWGR-01。真实工具取数、AI语义关联和Reader账户执行仍未验证。'
a.setdefault('additional_captures',[]).append(dict(checked_on='2026-10-27',access_mode='linked_renderer_source_review_and_synthetic_fixture',related_evidence_id='E-RWGR-01',note='同一任务补实现边界；原工作流没有安装或端到端执行。',archived_artifact=str(rn.relative_to(L)),sha256=sha(rn)))
evchange('E-RWSK-10',a)
a=copy.deepcopy(E['E-IXCD-01']);a.update(visual_observed=True,visual_kind='static_manual_pages',archived_artifact=str(xn.relative_to(L)),sha256=sha(xn),capture_state='short_notes_and_static_pages_archived_original_in_work',artifact_hashes={str(p.relative_to(L)):sha(p) for p in xn.parent.glob('*.png')})
a.setdefault('additional_captures',[]).append(dict(checked_on='2026-10-27',access_mode='existing_pdf_targeted_text_reread_and_static_page_render',raw_work_path=a['raw_work_path'],raw_sha256=a['raw_sha256'],locator='printed4–5,9–10,16,26,28–29,36,39,41,44–47; viewed file9/33/51',note='七条新文档任务；图片插入、PDF保存和当前页选择补旧任务。书城试读仍高层描述；无设备操作。',archived_artifact=str(xn.relative_to(L)),sha256=sha(xn),artifact_hashes=a['artifact_hashes']))
evchange('E-IXCD-01',a)
mo_notes={
'E-MOD-03':'补方法二JPG及方法三WLAN两画面；Books目录、二维码和保持同网/勿关闭页可见。方法一请求未知未重取；文字books/book冲突未解。',
'E-MOD-04':'补通知栏接收状态和不加入/加入书架弹窗；未看触发返回动作，不能保证两帧同一文件。',
'E-MOD-05':'补复制/剪切菜单及粘贴进去菜单；两帧时钟/文件不同，未闭合目标目录及实际移动结果。',
'E-MOD-07':'补重排与批注入口箭头；未展示排版变化、保存或裁边结果。',
'E-MOD-08':'补日期名入口和标题编辑的取消/确认；未验证取消/保存结果。',
'E-MOD-09':'补6/6页码和四格预览中的九格入口；未看到九格结果或返回原页。',
'E-MOD-10':'补导出/分享/上传独立按钮及Note目录路径提示；未取得或打开导出文件。'}
for eid,note in mo_notes.items():
    a=copy.deepcopy(E[eid]);a.update(visual_observed=True,visual_kind='sampled_legacy_tutorial_frames',archived_artifact=str(mn.relative_to(L)),sha256=sha(mn),capture_state='notes_and_sampled_static_frames_archived_original_media_not_downloaded',artifact_hashes=mo_assets[eid])
    a['note']=E[eid]['note']+' 此为首次采集记录；2026-10-27补媒体静态采样：'+note
    a.setdefault('additional_captures',[]).append(dict(checked_on='2026-10-27',access_mode='anonymous_browser_remote_img_static_sampling',source_urls=[r['url'] for r in mo['records'] if r['evidence_id']==eid],status=200,locator='local post-load samples approx0/2/5seconds; not GIF source timestamps',note=note+' 型号仍未知，非连续操作或产品实操。',archived_artifact=str(mn.relative_to(L)),sha256=sha(mn),artifact_hashes=mo_assets[eid],sampling_log_path=str((mn.parent/'media-sampling.json').relative_to(L)),sampling_log_sha256=sha(mn.parent/'media-sampling.json')))
    evchange(eid,a)

oldF=[]
def fc(fid,**kw):
    a=copy.deepcopy(F[fid]);a.update(checked_on='2026-10-27',**kw)
    oldF.append(dict(flow_id=fid,before=F[fid],after=a,fields_changed=[k for k in F[fid] if F[fid][k]!=a[k]]));return a
fc('F-M015-33',evidence_ids='E-RWSK-10;E-RWGR-01',known_branches='工作流先取高亮再生成HTML/补关系；固定渲染器合成样本核出短文本过滤、单条来源不计数、关联跳转保留筛选、屏幕esc与键盘Escape范围不同、重载清视图状态。',gaps='只运行未修改的官方示例和合成数据，非Reader原生或真实AI执行；标题/关系质量、取数权限、写回与恢复未知。HTML依赖CDN；320px样本面板截断，不当Plus表现。',visual_status='static_reference_only')
for fid,extra,gap in [
('F-M009-09','手机读写客云图片或华为分享可供写作插图，电磁笔长按可删插图。','插图完整接收/插入控件和云原图删除范围未知。'),
('F-M009-12','记录也可插入读写客/华为分享图片，长按删除。','图片接收与插入确认、删除关联云原图未知。'),
('F-M009-14','PDF批注可保存到PDF并经邮件/读写客分享；看批注按含笔迹页面汇集并支持排序。','保存覆盖原件还是副本、上传失败及批注保真未知。'),
('F-M009-18','批量删除先长按选邮件，可取消勾选；全选限当前页。已删除目录仍可查看/回复/转发。','')]:
    fc(fid,known_branches=F[fid]['known_branches']+extra,gaps=F[fid]['gaps']+gap)
mo_flow={
'F-M054-05':('历史PDF抽帧可见智能重排和批注图标；没有排版前后或保存结果。','固定机型导入/查词/返回/续读主链仍缺；裁边保存、锚点、切回原页和显示效果未知，历史图不归属Plus/W7/W8。'),
'F-M054-06':('历史方法二图列Books目录；WLAN菜单进入二维码/地址页，并提示同网且传输中勿关闭；蓝牙抽帧出现接收通知及不加入/加入书架弹窗。','机型未知；USB方法一未取得画面，books/book文字冲突未解；蓝牙返回动作和同文件连续链缺，拒绝后重加、断传/重名/取消/删除恢复未核。'),
'F-M054-07':('历史抽帧可见长按复制/剪切和粘贴进去菜单；不同画面时钟/文件不一致，不能闭合搬移。APK取得入口沿旧文字，不代表安装成功。','目标目录的选择与结果、覆盖/剪切中断、机型和安装权限/兼容/卸载数据保留仍未知；仅多了静态控件，D1不升级。'),
'F-M054-08':('历史日期名入口→标题编辑有取消/确认；页码入口与四格预览含九格按钮；笔记弹窗区分文本/导出/分享/上传，导出画面提示Note目录PDF路径。','机型未归属；改名确认/取消结果、九格结果、返回原页、导出文件可读性/原稿关系、重名/失败/删除同步恢复仍缺。')}
for fid,(known,gap) in mo_flow.items():fc(fid,known_branches=known,gaps=gap,visual_status='static_reference_only')

spec=[
(25,'AIUI按日期或主题语音查找笔记','桌面语音图标或轻压指纹电源键→提出日期/主题查询→文档称可多轮直到找到笔记；静态图仅见语音提示和示例。','结果列表/自动打开、无结果、退出、唤醒词、加密与跨账号范围未知；历史AIUI不等于全局大模型Agent。','static_manual_pages_only','历史X2手册；已有笔记'),
(26,'将详情页手写转为文字并选择出口','写作工具栏转文字→详情页所有手写转换→关联高亮结果→Word/PDF、扫码、邮件或打印出口。','原笔迹保留、错误修订、重复转换覆盖、原位置锚点与导出保真/失败均未知。','not_observed','历史写作详情页有手写内容'),
(27,'查看Office文件并保存批注','列表开文档；doc两侧翻页/中部导航，ppt每页两幻灯片/播放横屏，xls二维滑动；批注可保存文档或上传读写客/邮件分享。','原件还是副本、格式保真、撤销/只读、上传冲突/失败未知；导入路径沿F01/02。','not_observed','已导入受支持Office格式'),
(28,'通过搜索和星标找回邮件','邮件列表搜邮件→输入主题或发件人→对应邮件；列表/详情空心星标记→左侧星标邮件目录。','取消星标、零结果、全文/模糊匹配、离线索引与服务器同步未知。','not_observed','已设置邮箱且有邮件'),
(29,'维护联系人并开始给联系人写信','邮件列表通讯录→字母/搜索选人→编辑信息或写邮件；新增填表确认。正文称完成设置，静态图为确定；图有取消。','重复合并、删除确认/恢复、同步和离线修改、写信返回状态未核；有按钮不证明保存/取消成功。','static_manual_pages_only','历史邮件模块'),
(30,'配置网络、蓝牙和蜂窝数据','WiFi开关→选网→密码→连接；蓝牙选设备后双端确认；SIM可开关流量/看月统计。飞行模式关闭恢复先前无线状态，开启时可再开WiFi/蓝牙。','错误密码/配对失败/断连恢复、APN与当前固件未知；仅历史4G数据设置，不能证明短信/通话或Plus能力。','not_observed','历史X2设置；对应网络/设备/SIM可用'),
(31,'设置锁屏验证与无操作休眠','设备密码开关→四位密码两次确认；关闭须旧密码。指纹先验密码后录入/命名/删除/开关；设置无操作休眠，历史默认15分钟。','笔记独立密码另见F13；锁屏后录音/同步/TTS、唤醒恢复、忘记密码与指纹失败回退未知，图1/5不当实测容量。','static_manual_pages_only','历史通用设置；密码与笔记加密分开')]
newF=[]
for n,title,known,gap,visual,pre in spec:
    newF.append(dict(flow_id=f'F-M009-{n:02}',product_id='M-009',task=title,depth='D2',evidence_ids='E-IXCD-01',known_branches=known,gaps=gap,visual_status=visual,hands_on='false',case_path='cases/M-009-iflytek-office.md',checked_on='2026-10-27'))
assert not set(f['flow_id'] for f in newF)&set(F)
prefix='renderer-and-legacy-controls'
wc(L/f'batches/{prefix}-coverage.csv',newF);created.append(str(L/f'batches/{prefix}-coverage.csv'))
add(L/f'batches/{prefix}-evidence.jsonl',''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in newE))
for kind,changes,key in [('coverage.csv',oldF,'flow_id'),('evidence.jsonl',oldE,'evidence_id')]:
    for p in sorted((L/'batches').glob('*-'+kind)):
        rs=rows(p) if kind.endswith('csv') else [json.loads(x) for x in p.read_text().splitlines()];changed=False
        for i,r in enumerate(rs):
            c=next((v for v in changes if v[key]==r[key]),None)
            if c:
                assert r==c['before'];rs[i]=c['after'];changed=True;c.setdefault('owning_files',[]).append(str(p.relative_to(R)))
        if changed:
            save(p)
            if kind.endswith('csv'):wc(p,rs)
            else:p.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rs))
assert all(c.get('owning_files') for c in oldF+oldE)

# Stable inventories are updated in place, preserving their IDs and row counts.
p=L/'inventories/M009-X2-manual-inventory.csv';save(p);rs=rows(p);assert len(rs)==64
mapping={'X02':[31],'X08':[25],'X15':[9,12],'X17':[9,26],'X38':[28],'X41':[28],'X42':[29],'X50':[27],'X51':[27],'X60':[30],'X61':[21,22,31]}
for r in rs:
    key=r['entry_id'].split('-')[-1]
    if key in mapping:r['related_flow_ids']=';'.join(f'F-M009-{n:02}' for n in mapping[key]);r['checked_on']='2026-10-27'
    if key=='X47':r['review_status']='manual_text_read_trial_controls_incomplete_no_task_closure';r['checked_on']='2026-10-27'
wc(p,rs)
p=L/'batches/readwise-ai-workflows-inventory.csv';save(p);rs=rows(p);assert len(rs)==14
for r in rs:
    if r['entry_id']=='M015-RWSK-10':r.update(evidence_ids='E-RWSK-10;E-RWGR-01',review_status='定义已读；生成器另用合成数据运行，非工作流端到端',gap='源码与本地控件有据；真实AI取数/关联/写回及Reader原生行为未核')
rs.append(dict(entry_id='M015-RWSK-15',entry_type='official_example_renderer',title='关系图生成器实现与合成样本',source_url=newE[0]['url'],evidence_ids='E-RWGR-01',flow_ids='F-M015-33',review_status='源码全文已读；合成数据本地运行',gap='不是Reader真实书库/原生App/AI执行；CDN、窄屏、关系验证及持久恢复边界另记'))
wc(p,rs)
p=L/'batches/moaan-controls-deepening-inventory.csv';save(p);rs=rows(p);assert len(rs)==82
for r in rs:
    eid=r['evidence_ids']
    if eid in mo_notes and r['source_family']=='官网历史支持目录':
        if r['inventory_id']=='I-MOCD-038':r['gap']='USB文字已读，方法一媒体首请求状态未知，未重试；型号与传输完整链仍缺';continue
        r['coverage_status']='official_body_and_sampled_static_media_model_unassigned'
        r['gap']=mo_notes[eid]+' 历史型号未归属；不当连续操作。'
wc(p,rs)

# Cases keep existing tasks, replacing stale gap descriptions and adding only new task objects.
def change_x2(t):
    t=t.replace('查阅2026-10-26。','初查2026-10-26；2026-10-27补七条历史手册任务及三张静态页。',1)
    t=t.replace('通讯录/星标仍待任务化','通讯录/星标已补任务，当前同步与恢复未知')
    t=t.replace('本轮归档6张关键页','此前归档6张关键页，本轮另补3张')
    for c in oldF:
        if c['after']['product_id']!='M-009':continue
        a=c['after'];line=next(s for s in t.splitlines() if s.startswith('| '+a['flow_id']+' |'));cells=line.split('|');cells[5]=' '+a['known_branches']+' ';cells[7]=' '+a['gaps']+' ';t=t.replace(line,'|'.join(cells))
    lines=[]
    for f,s in zip(newF,spec):
        lines.append(f'| {f["flow_id"]} | {f["task"]} | {s[5]} | {f["known_branches"]} | 预期结果依据历史手册；实际保存/执行未验证 | E-IXCD-01 | {f["gaps"]} |')
    t=t.replace('\n## 分支、矛盾与修订','\n'+'\n'.join(lines)+'\n\n新增任务的逐页定位、图片插入/PDF保存/邮件选择分支见[本轮原件摘记](../artifacts/x2-remaining-tasks/source-notes.md)。书城试读仍只到能力说明，未补成完整交易任务。\n\n## 分支、矛盾与修订')
    return t
edit(L/'cases/M-009-iflytek-office.md',change_x2)
def change_rw(t):
    a=next(c['after'] for c in oldF if c['flow_id']=='F-M015-33');line=next(s for s in t.splitlines() if s.startswith('| F-M015-33 '))
    t=t.replace(line,f'| F-M015-33 {a["task"]} | {a["known_branches"]} | {a["gaps"]} | {a["evidence_ids"]} |')
    return t+'\n### 关系图的实现与样本核查（2026-10-27）\n\n[E-RWGR-01源码与本地样本审计](../artifacts/readwise-graph-renderer/source-notes.md)补过滤、来源计数、选中/筛选、两种退出、重载和窄屏范围。未修改官方生成器，输入全部为合成数据；本地示例运行不等于Reader原生App或真实AI执行。F-M015-33仍D2，hands_on=false。原工作流的未运行结论保留在原采集记录中，辅助渲染器已读/已运行的范围以本次审计为准。\n'
edit(L/'cases/M-015-readwise.md',change_rw)
def change_mo(t):
    for c in oldF:
        if c['after']['product_id']!='M-054':continue
        a=c['after'];line=next(s for s in t.splitlines() if s.startswith('| '+a['flow_id']+' '))
        t=t.replace(line,f'| {a["flow_id"]} {a["task"]}，D1 | {a["known_branches"]} | 历史静态采样只补控件/提示；未操作产品 | {a["gaps"]} 证据：{a["evidence_ids"]} |')
    return t+'\n## 2026-10-27：历史教程媒体补看\n\n[七篇文章的媒体观察与15张不同画面](../artifacts/moaan-legacy-media/source-notes.md)补WLAN二维码、蓝牙加入书架、文件菜单、PDF入口、标题取消/确认、页码预览及导出路径。八个媒体普通加载200；首次方法一发生本地脚本错误，状态未知，未重复取该媒体。采样间隔不当操作时延，没有原始GIF归档或产品实操。\n\nF-M054-05～08继续D1：机型和完整主链仍缺。画面可见W7_User_Guide.pdf只是文件名；不能据此把历史界面归属W7。文件移动两帧时钟/文件不同；九格箭头仅指入口；导出路径提示不验证实际PDF。旧“GIF未看”属于此前采集状态，本次以追加记录为准。\n'
edit(L/'cases/M-054-moaan.md',change_mo)

brief=R/'docs/briefs/036-controls-and-state-evidence.md';add(brief,(W/'brief.md').read_text())
nav='- [第三十六期：控件与状态依据](/home/workspace/reader/docs/briefs/036-controls-and-state-evidence.md)：Readwise官方渲染器合成样本、X2剩余手册任务及墨案历史控件；区分按钮、状态和完成证据。\n'
for p in [R/'README.md',L/'README.md']:
    edit(p,lambda t:t.replace('- [第三十五期',nav+'- [第三十五期',1))
guide_rows='''| AI展示“知识关系”时，原文、推断和视图状态怎样区分 | [Reader F33](cases/M-015-readwise.md) | [固定渲染器与合成样本](artifacts/readwise-graph-renderer/source-notes.md) | 同源边不证明语义，单条来源计数、筛选/选中/退出不同；这是示例本地运行，非Reader原生实测 |
| 语音找到笔记之后，是否真的打开了正确内容 | [X2 F25/26](cases/M-009-iflytek-office.md) | [历史手册原页与任务](artifacts/x2-remaining-tasks/source-notes.md) | 入口、候选、打开、转换、保存分别核；AIUI不等于大模型，搜索结果和恢复仍缺 |
| 导入或导出后，怎样确认AI完成了任务 | [墨案 F06/08](cases/M-054-moaan.md)，对照[X2 F27](cases/M-009-iflytek-office.md) | [接收/书架/导出路径画面](artifacts/moaan-legacy-media/source-notes.md) | 文件收到、登记、打开、完整性分开；历史型号未知，路径提示不等于文件验证 |
'''
edit(L/'design-reference-guide.md',lambda t:t.replace('| AI怎样给我一批新闻',guide_rows+'| AI怎样给我一批新闻',1))
edit(L/'comparison.md',lambda t:t+'\n## 控件、状态与实际产物的区分（2026-10-27）\n\n| 比较问题 | 依据 | 对本项目的候选启发及边界 |\n| --- | --- | --- |\n| 关闭详情还是清除选择 | [Readwise F33合成样本](artifacts/readwise-graph-renderer/source-notes.md) | 屏幕esc/键盘Escape清除范围不同；AI动作应描述影响状态，不由按钮名称判断。不是原生Reader或Plus性能验证 |\n| 查到、转换和保存是否同一步 | [X2 F25–27](artifacts/x2-remaining-tasks/source-notes.md) | 历史语音检索结果页缺失，转换/批注出口明确但原件关系未知；候选动作和执行结果分别核 |\n| 路径提示是否足够确认成功 | [墨案F06/08](artifacts/moaan-legacy-media/source-notes.md) | 接收、书架登记、导出路径、文件可读性分别核；静态历史图不能保证运行结果或当前型号 |\n')
edit(L/'backlog.md',lambda t:t+'\n## 第36批补证后的剩余项\n\n- Reader F33：固定生成器已全文读并运行合成数据，仍缺真实授权取数、AI关系质量、书库写回及持久恢复；不要再把“生成器未读”作为当前缺口。未固定CDN和窄屏面板问题只属官方示例，不归因Reader原生App。\n- X2：七条历史任务已补，当前固件、AIUI搜索结果/退出、转换覆盖、Office原件保真、联系人保存与删除恢复、锁屏后台仍缺。书城只保留试读能力，支付/下载全链未闭合；2023纪要F08仍D1。\n- 墨案：七篇历史指南已补15张不同静态画面，型号仍未知；原传输方法一请求状态未知，未重试。移动目标、标题取消结果、九格结果、接收后入库、实际PDF输出及恢复仍缺，F05–08保持D1。受限旧下载路线不重复绕访。\n')
edit(L/'analysis-status.md',lambda t:t+'\n第36批补七条X2历史文档任务，四条旧X2分支、Reader图任务和墨案四条旧任务；只新增一个固定源码E，同一PDF和七篇文章追加捕获，原工作流E追加关联核查。六张合成渲染器代表图、三张X2静态页、十五张墨案不同采样图均有来源和哈希。合成示例运行单独标注，不增加D4；墨案型号未核不升级主链。65组仍部分覆盖，完整功能总数未知。\n')
edit(L/'inventories/README.md',lambda t:t+'\n第36批就地更新[X2原64行](M009-X2-manual-inventory.csv)的任务映射及[墨案原82行](../batches/moaan-controls-deepening-inventory.csv)的媒体观察状态，不复制目录。[Readwise工作流目录](../batches/readwise-ai-workflows-inventory.csv)由14行增至15行：新增官方渲染器组件；原工作流未端到端执行，生成器单独运行合成输入。正文已读、具有F映射和完整功能验证是不同状态。\n')
edit(L/'data-dictionary.md',lambda t:t+'\n### 合成样本与产品实际操作\n\n`local_fixture_run=true`表示运行公开组件及明确合成输入，`local_fixture_audit_path`和哈希指向范围/命令/结果记录；不意味着登录或操作真实产品，不能因此把`hands_on`置true或升D4。`visual_kind=synthetic_fixture_static_frames`表示本地样本代表图，计入“含静态图的证据记录”时也必须保留此类型，不能当产品界面实测。E-RWGR-01为首个实例；F-M015-33仍D2。\n')
edit(L/'batches/README.md',lambda t:t+'\n第36批 `renderer-and-legacy-controls-*` 仅登记1新E/7新F；9旧F及9旧E就地更新原持有批次。旧原件/摘记不覆盖，同URL用additional_captures保存本次原页/媒体和范围。Readwise本地合成样本不是产品D4；X2和墨案静态图不当连续动作。\n')
edit(R/'docs/ideas.md',lambda t:t+'\n2026-10-27继续I-021/I-022：Readwise官方示例补源码和合成样本的过滤/计数/退出状态，X2历史手册补七任务，墨案补历史媒体控件与未闭合结果。沿F/E与目录就地维护，见[第三十六期简报](/home/workspace/reader/docs/briefs/036-controls-and-state-evidence.md)。合成组件运行不当原生产品实测，型号和错误/恢复缺口保留；未升级需求或进入全系统设计。\n')
edit(L/'verification-log.md',lambda t:t+'\n## 第36批：源码样本与历史静态材料\n\n根代理核Readwise固定文件、Git blob及完整源码，生成器未改，合成输入运行exit0；Chromium样本动作记录及320px全新页面核对，均非真实Reader账户或原生App。X2旧PDF哈希未变，选定正文复读与文件9/33/51看图；worker只读核对目录和现任务，root决定任务划分。墨案worker首次参数错误exit1停路，root一次接手仅余8URL，全部200，24采样图15不同哈希全部查看；首URL请求未知未重取。没有安装产品/技能、登录、发送或购买。原脚本/报告在work/research/batch36-*，正式摘记、图与元数据在对应artifacts。\n\n本批不增加D4；F/E、文件和链接检查结果见本轮集成记录。工具检查只核结构/哈希，不代表全功能或实机验证。\n')
dump(W/'changes.json',dict(new_evidence=[e['evidence_id'] for e in newE],new_flows=[f['flow_id'] for f in newF],old_evidence=oldE,old_flows=oldF,created=created))
dump(W/'before-manifest.json',before)
dump(W/'applied.json',dict(applied=True,needs_merge_and_checks=True,new_evidence=len(newE),new_flows=len(newF),old_evidence=len(oldE),old_flows=len(oldF)))
print(json.dumps(dict(newE=len(newE),newF=len(newF),oldE=len(oldE),oldF=len(oldF),before_files=len(before),new_files=len(created))))
