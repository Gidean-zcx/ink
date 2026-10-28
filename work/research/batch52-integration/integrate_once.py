import csv, hashlib, json, re
from pathlib import Path
R=Path('/home/workspace/reader'); L=R/'docs/research/interaction-library'; W=R/'work/research/batch52-integration'
A=L/'artifacts/quizlet-material-workflows'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x): p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def rows(p): return list(csv.DictReader(p.open()))
def writecsv(p,rr):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rr[0]));w.writeheader();w.writerows(rr)
assert not (L/'batches/quizlet-material-workflows-evidence.jsonl').exists()
data=[
('01','49258738112525-Create-flashcard-sets-directly-in-Claude','Create flashcard sets directly in Claude','01',19,41,2,'F-M021-01','连接→需求/文件→对话翻卡→打开Quizlet编辑；保存、账号、权限及失败未知'),
('02','360030936511-Creating-a-set-by-scanning-a-document','Creating a set by scanning a document','01',17,75,12,'F-M021-03','移动扫描框词、人工两列配对、校订、至少两卡、预览/确认；拒权/取消/重扫未知'),
('03','360036242411-Trouble-copying-and-saving-a-set','Trouble copying and saving a set','01',17,20,18,'F-M021-04','高级内容复制资格与部分地区家长确认；没有网络错误或重试步骤'),
('04','360029638892-Combining-flashcard-sets','Combining flashcard sets','01',17,35,19,'F-M021-04','临时活动退出不保存组合；网站保存新集保留原集，手机可学习编辑；去重/进度映射未知'),
('05','44716146144909-Create-flashcard-sets-directly-in-ChatGPT','Create flashcard sets directly in ChatGPT','02',19,35,44,'F-M021-01','连接和@Quizlet→对话翻卡→打开Quizlet编辑；没有全页面控制、发布前校订或持久回执证明'),
('06','360048331232-Using-error-checking-options-UK','Using error checking options (UK)','02',27,153,63,'F-M021-08','UK Learn等级与语言条件、网站保存/手机完成、Test单答案偏好跨活动；历史回算未知'),
('07','360029780752-Creating-flashcard-sets','Creating flashcard sets','02',19,82,8,'F-M021-03;F-M021-05','手动两列/语言、自动草稿与发布分开、手机在当前卡下插入；重启保存未知'),
('08','360030841732-Studying-on-Quizlet','Studying on Quizlet','02',31,89,50,'F-M021-07;F-M021-11;F-M021-12','功能关系总览：Test/AI Practice Tests分开、Write/Spell归Learn、Gravity退役；不计完整任务')]
known=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()]
sources=[]; new=[]
for no,slug,title,rawno,start,end,order,flows,note in data:
    url='https://help.quizlet.com/hc/en-us/articles/'+slug
    assert not any(e['url']==url for e in known)
    raw=W/f'quizlet-pages-{rawno}.txt';txt=raw.read_text()
    assert url in txt and title in txt
    eid='E-QZMW-'+no
    sources.append(dict(evidence_id=eid,title=title,url=url,body_locator=f'L{start}–{end}',body_read_by_root=True,
        raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw),raw_capture_kind='web_tool_response_text_not_source_HTML',
        inventory_order=order,flow_ids=flows.split(';'),hands_on=False,visual_observed=False))
    new.append(dict(evidence_id=eid,product_id='M-021',title=title,url=url,checked_on='2026-10-27',
        source_type='official_help_overview' if no=='08' else 'official_help',access_mode='opened_text_via_web_tool',
        version_scope='en-US公开帮助，UK评分文章按标题限定；Web/iOS/Android以正文分别限定，精确构建未知',
        app_build=None,firmware_version=None,source_updated_on=None,locator=f'网页工具正文L{start}–{end}',note=note,
        hands_on=False,visual_observed=False,archived_artifact='artifacts/quizlet-material-workflows/source-notes.md',
        sha256=sha(A/'source-notes.md'),supersedes=None,raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw),
        capture_state='official_web_tool_response_saved_in_work_no_source_HTML_no_images'))
dump(A/'source-audit.json',dict(checked_on='2026-10-27',sources=sources,
    directories=dict(raw_work_path=str((W/'quizlet-directories.txt').relative_to(R)),sha256=sha(W/'quizlet-directories.txt'),titles_checked=8),
    operation='read_only_public_help',account_used=False,images_viewed=False,
    limitations='Official documentation reconstruction only; no account linking, app execution or target device test. Source line numbers belong to the web response.',
    root_correction='UK error checking is original inventory order 63, not helper report order 64; matched exact title before editing.'))
(L/'batches/quizlet-material-workflows-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in new))
flow_changes={
'F-M021-01':dict(evidence_ids='E-QZLT-01;E-QZMW-01;E-QZMW-05',known_branches='Smart Assist手动/AI切换；外部Claude/ChatGPT连接后对话生成翻卡，打开Quizlet进一步编辑学习',gaps='生成失败、来源校验；外部AI账户归属、可见性、保存回执、重复请求与断开连接后果未知'),
'F-M021-03':dict(task='学习资料输入与结构化导入',evidence_ids='E-QZLT-03;E-QZMW-02;E-QZMW-07',known_branches='文本分隔符/语言；手动两列/插卡；移动扫描框词后人工配对、键盘校订、至少两卡，预览删除后确认',gaps='空行、重复及失败；相机拒绝、无字/错行、取消重扫、扫描草稿与最终提交时点未知'),
'F-M021-04':dict(evidence_ids='E-QZLT-04;E-QZLT-11;E-QZLT-12;E-QZCD-05;E-QZCD-06;E-QZCD-07;E-QZCD-08;E-QZMW-03;E-QZMW-04',known_branches='已有编辑/删除/可见性分支；高级内容复制受限；网站合并可临时学习或保存新集，原集保留，保存后手机可学习编辑',gaps='误删撤销/并发编辑/隐藏找回/删除确认；合并去重、进度星标映射、源更新、重试与撤销未知'),
'F-M021-05':dict(evidence_ids='E-QZLT-05;E-QZMW-07',known_branches='创建页自动草稿与显式发布分开；既有断网警告/退出账号丢弃规则；不将普通草稿规则套外部AI或扫描',gaps='进程终止、系统重启及编辑冲突的保存边界未知'),
'F-M021-08':dict(evidence_ids='E-QZLT-07;E-QZLT-08;E-QZCD-09;E-QZCD-17;E-QZMW-06',known_branches='原US/连胜误判分支保留；UK文档列Learn三级默认语言条件、网站保存/手机完成；Test单答案设置仅网站，作用于同集其他活动',gaps='地区/构建实际边界、中文数学等例外、历史成绩回算和调度影响未知；不将Learn/Write误判按钮套到Test')}
changes=[]
for p in (L/'batches').glob('*-coverage.csv'):
    rr=rows(p);dirty=False
    for r in rr:
        if r['flow_id'] in flow_changes:
            before=r.copy();r.update(flow_changes[r['flow_id']]);r['checked_on']='2026-10-27'
            changes.append(dict(file=str(p.relative_to(R)),before=before,after=r.copy()));dirty=True
    if dirty:writecsv(p,rr)
assert len(changes)==5
dump(W/'old-flow-updates.json',changes)
ec=[]
for p in (L/'batches').glob('*-evidence.jsonl'):
    ee=[json.loads(s) for s in p.read_text().splitlines()];dirty=False
    for e in ee:
        if e['evidence_id'] in ['E-QZCD-02','E-QZCD-03']:
            before=json.loads(json.dumps(e));e.setdefault('additional_captures',[]).append(dict(checked_on='2026-10-27',
                access_mode='opened_text_via_web_tool',raw_work_path=str((W/'quizlet-directories.txt').relative_to(R)),
                raw_sha256=sha(W/'quizlet-directories.txt'),note='重新打开原目录定位八篇文章；不宣称目录全读或新增完整功能',hands_on=False,visual_observed=False))
            ec.append(dict(file=str(p.relative_to(R)),before=before,after=e));dirty=True
    if dirty:p.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in ee))
assert len(ec)==2;dump(W/'old-evidence-updates.json',ec)
p=L/'batches/quizlet-controls-deepening-inventory.csv';rr=rows(p);inv=[]
byorder={str(d[6]):d for d in data}
for r in rr:
    if r['order'] not in byorder:continue
    no,slug,title,rawno,start,end,order,flows,note=byorder[r['order']]
    assert r['functional_domain']==title
    before=r.copy();r.update(article_url='https://help.quizlet.com/hc/en-us/articles/'+slug,
        evidence_ids=r['evidence_ids']+';E-QZMW-'+no,flow_ids=flows,
        coverage='正文已读；功能关系总览，不独立计任务' if no=='08' else ('正文已读；资格原因，非错误恢复闭环' if no=='03' else 'D2部分；文档重建，未操作'),
        missing_controls_or_branches=note.split('；')[-1],checked_on='2026-10-27')
    inv.append(dict(before=before,after=r.copy()))
assert len(inv)==8 and len(rr)==68;writecsv(p,rr);dump(W/'inventory-updates.json',inv)
# Seven retrieval examples; no additional product task IDs.
p=L/'reference-points.csv';rr=rows(p);fields=list(rr[0]);assert not any(r['reference_id'].startswith('RP-M021-') for r in rr)
rpdata=[
('01','AI;生成;校订;账户;保存','F-M021-01','E-QZMW-01;E-QZMW-05','外部AI帮助How it works','接入Quizlet连接，提出主题/资料','生成后翻卡，打开Quizlet编辑','帮助称对话显示卡片并提供打开入口','账号归属、可见性与持久回执未知','拒绝接入、重复生成、断开和删除；不能套Smart Assist校订'),
('02','OCR;资料导入;校订;儿童','F-M021-03','E-QZMW-02','扫描帮助L17–75','移动端订阅/年龄条件及照片输入','框词后逐列选择并键盘改错，加卡/预览/确认','文档列识别框、人工配对及至少两卡门槛','Done描述含发布或预览，真实提交时点未测','拒权、无字、错行、取消重扫和误删撤销'),
('03','合并;临时;退出;复习','F-M021-04','E-QZMW-04','合并帮助L19–27','网站选多个已有集','选择学习活动后执行，随后退出','按帮助可直接学习这些集合','退出不保存组合；答题统计是否留存未知','进度与星标映射、恢复临时集合'),
('04','合并;保存;副本;跨端','F-M021-04','E-QZMW-04','合并帮助L17–35','网站多个集，高级内容另需订阅','选择创建新集并执行','帮助说明保存新集，原集保留','新集可在手机学习编辑；持久结果未实测','重复词、源更新、重试重复副本与撤销'),
('05','复制;权限;儿童;失败','F-M021-04','E-QZMW-03','复制受限帮助L17–20','基础/高级内容及账号资格','尝试复制保存，按资格区分限制原因','帮助给出订阅和部分地区家长确认原因','未提供失败后保存的对象状态','无错误画面、网络失败/重试/恢复步骤'),
('06','草稿;发布;编辑;中断','F-M021-03;F-M021-05','E-QZMW-07','手动创建帮助L19–82','建立题名/两列及语言','输入及插卡后显式创建或完成','帮助将自动草稿和发布分开','支持自动草稿的文档声明，进程重启未测','自动保存时点、编辑冲突与跨端草稿'),
('07','评分;语言;偏好;作用范围','F-M021-08','E-QZMW-06;E-QZCD-17','UK L27–153与既有US帮助','Learn语言/卡数条件；Test多答案按规定分隔','选择评分等级或单答案偏好','帮助说明等级、默认条件和设置提交入口','Test单答案偏好影响该集其他活动','历史成绩、误判重算、长期调度及当前地区构建')]
for no,tags,fids,eids,locator,pre,action,result,persist,unknown in rpdata:
    r={k:'' for k in fields};r.update(reference_id='RP-M021-'+no,product_id='M-021',question_tags=tags,flow_ids=fids,evidence_ids=eids,
        source_locator=locator,artifact_path='artifacts/quizlet-material-workflows/source-notes.md',precondition=pre,action=action,
        visible_result=result,evidence_basis='official_help_text_reconstruction; no visual observation or actual operation',
        persistence_result=persist,unknown_branches=unknown,version_scope='2026-10-27公开帮助；平台/地区逐条限定，构建未知',checked_on='2026-10-27');rr.append(r)
writecsv(p,rr)
dump(W/'integration-corrections.json',dict(helper_inventory_order_correction={'topic':'Using error checking options (UK)','reported':64,'verified':63},
    catalog_note='merge_batches derives next_gap from first two flow gaps only; retain full gaps in coverage/case, do not claim catalog completeness',
    new_tasks=0,old_tasks_updated=5,new_evidence=8,old_directory_captures=2,inventory_rows_updated=8,new_reference_points=7))
print('Applied: 8 E, 5 existing F, 2 directory captures, 8 of 68 inventory rows, 7 RP; 0 new F.')
