import csv, hashlib, json, re, sys
from pathlib import Path
R=Path('/home/workspace/reader'); L=R/'docs/research/interaction-library'; W=R/'work/research/batch50-integration'
assert not (W/'APPLIED').exists()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def rows(p):return list(csv.DictReader(p.open()))
def write_csv(p,rs):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rs[0]));w.writeheader();w.writerows(rs)
def add_text(p,t):p.write_text(p.read_text().rstrip()+'\n\n'+t.strip()+'\n')

capture=json.loads((W/'gemini-web-capture.json').read_text());assert isinstance(capture,str)
parts=capture.split('--------------------------------------------------------------------------------')
assert len(parts)==2
gem=[]
for n,(num,title) in enumerate([('15230597','笔记与列表'),('15230285','任务与提醒')],1):
    assert num in parts[n-1]
    p=W/f'gemini-{num}.txt';p.write_text(parts[n-1].strip()+'\n')
    gem.append(dict(evidence_id=f'E-GENR-{n:02d}',title=title,url=f'https://support.google.com/gemini/answer/{num}?co=GENIE.Platform%3DAndroid&hl=en',raw_work_path=str(p.relative_to(R)),raw_sha256=sha(p),access_mode='web_tool_full_article_text_read',http_status='not_exposed_by_web_tool',body_read=True,visual_observed=False,hands_on=False))
ga=L/'artifacts/gemini-notes-reminders/source-audit.json'
dump(ga,dict(checked_on='2026-10-27',captures=gem,failed_sources=[dict(url='https://support.google.com/gemini/answer/15305236?co=GENIE.Platform%3DAndroid&hl=en',result='web tool reported HTTP 429; no retry or alternate route',body_read=False),dict(url='https://support.google.com/gemini/answer/15229592?co=GENIE.Platform%3DAndroid&hl=en',result='not accessible via web tool; no body',body_read=False)],discovered_only=['17094507 Tasks agent workflow','17094710 Schedules','17094296 Skills'],version_scope='Android English official help at lookup; installed build unknown',no_product_actions=True))
sys.path.insert(0,str(R/'work/research/tools/pymupdf'));import pymupdf
qp=R/'work/research/extension-manuals/quaderno-gen2.pdf';qd=pymupdf.open(qp);qpages=[1,27,*range(51,58),124]
assert len(qd)==124 and sha(qp)=='7bedbd0094f1b4471c7fe2049dd9d379bb09ac4485765d1aafb267dfc474d870'
qt=W/'quaderno-gen2-read-pages.txt';qt.write_text('\n'.join(f'=== PDF page {n} ===\n'+qd[n-1].get_text() for n in qpages))
qurl='https://www.fmworld.net/digital-paper/support/download/pdf/B6FK5131.pdf'
qa=L/'artifacts/quaderno-gen2-ink/source-audit.json'
dump(qa,dict(evidence_id='E-QGNT-01',checked_on='2026-10-27',source_url=qurl,raw_work_path=str(qp.relative_to(R)),raw_sha256=sha(qp),text_work_path=str(qt.relative_to(R)),text_sha256=sha(qt),page_count=124,read_pages=qpages,remaining_pages_not_recorded_as_read=[n for n in range(1,125) if n not in qpages],toc_nodes_extracted=len(qd.get_toc()),toc_all_nodes_reviewed=False,source_version='B6FK-5131-08 Z0 / Z0-02; issued 2025-04',hands_on=False,visual_observed=False,acquisition='existing public PDF reused; original HTTP 200 log in work/research/extension-manuals/fetch-log.json; no new network download'))
ev=[]
for x in gem:
    a='artifacts/gemini-notes-reminders/source-notes.md'
    ev.append(dict(evidence_id=x['evidence_id'],product_id='M-042',title='Gemini Android：'+x['title'],url=x['url'],checked_on='2026-10-27',source_type='official_help',access_mode=x['access_mode'],version_scope='Android English help; account/activity/OEM restrictions; no installed build',app_build=None,firmware_version=None,source_updated_on=None,locator='What you need; Connect; Add/Create; Show/Get/Update; Undo Tips',note='完整正文重建；未观察界面或执行。原页、取得方式和未知见归档。',hands_on=False,visual_observed=False,archived_artifact=a,sha256=sha(L/a),capture_state='web_article_text_and_short_note_archived',supersedes=None,raw_work_path=x['raw_work_path'],raw_sha256=x['raw_sha256'],artifact_hashes={str(ga.relative_to(L)):sha(ga)}))
a='artifacts/quaderno-gen2-ink/source-notes.md'
ev.append(dict(evidence_id='E-QGNT-01',product_id='M-060',title='QUADERNO Gen.2 活用指南：校准与笔迹状态',url=qurl,checked_on='2026-10-27',source_type='official_manual',access_mode='existing_public_pdf_selected_pages_text_read',version_scope='FMVDP41/FMVDP51; B6FK-5131-08 Z0 / Z0-02; issued 2025-04; firmware unknown',app_build=None,firmware_version=None,source_updated_on='2025-04',locator='PDF/print p1,27,51–57,124; selected pages only',note='10页正文已读，目录197节点仅提取。与Gen.3C分开，未看图、未实操。',hands_on=False,visual_observed=False,archived_artifact=a,sha256=sha(L/a),capture_state='existing_pdf_selected_text_and_short_note_archived',supersedes=None,raw_work_path=str(qp.relative_to(R)),raw_sha256=sha(qp),text_work_path=str(qt.relative_to(R)),text_sha256=sha(qt),artifact_hashes={str(qa.relative_to(L)):sha(qa)}))
(L/'batches/notes-and-ink-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in ev))
updates={
'F-M042-02':('E-GENR-01;E-GENR-02','；笔记/待办须活动开启，多个同类连接默认最近访问者，@指定；Workspace只补两类子帮助','；其完整帮助/日历本次未取得，不能外推全部Workspace能力'),
'F-M042-10':('E-GENR-01;E-GENR-02','；专门帮助也明确Live后台所建笔记/动作不能由Gemini Undo，可经回复链接回目标对象编辑','；专门帮助没有证明写前确认，撤销通知与写前批准分开'),
'F-M060-11':('E-QGNT-01','；Gen.2 p56另核手写/高亮列表分栏、点项跳页和外点关闭','；Gen.2未继承Gen.3C全库颜色/图形检索结论'),
'F-M060-12':('E-QGNT-01','；Gen.2 p27/52–57独立确认即时保存、单次剪贴、翻页清撤销；灰度显示仍有颜色数据，默认限制编辑其他设备笔迹','；两代分列；未核Gen.2往返传输、外部笔迹实际兼容或崩溃恢复')}
changes=[]
for p in sorted((L/'batches').glob('*-coverage.csv')):
    rs=rows(p);changed=False
    for r in rs:
        if r['flow_id'] in updates:
            old=r.copy();es,known,gap=updates[r['flow_id']]
            r['evidence_ids']=';'.join(dict.fromkeys((r['evidence_ids']+';'+es).split(';')))
            r['known_branches']+=known;r['gaps']+=gap;changed=True
            changes.append(dict(owner=str(p.relative_to(R)),before=old,after=r.copy()))
    if changed:write_csv(p,rs)
assert len(changes)==4;dump(W/'old-flow-updates.json',changes)
new=[]
def flow(fid,pid,task,es,known,gap,case):
    new.append(dict(flow_id=fid,product_id=pid,task=task,depth='D2',evidence_ids=es,known_branches=known,gaps=gap,visual_status='not_observed',hands_on='false',case_path=case,checked_on='2026-10-27'))
flow('F-M042-12','M-042','从对话创建、查找笔记并追加既有列表','E-GENR-01','连接→指定应用/最近应用→生成或查找→Keep列表追加；通知Undo或回链接编辑；后台Live不可同样撤销','同名消歧、写前确认、失败回执、重复请求和重启未知；OEM动作和内容类型受限','cases/M-042-gemini.md')
flow('F-M042-13','M-042','把前文或图片转成待办并管理完成状态','E-GENR-02','连接Google Tasks或Samsung Reminder→请求添加/查看→改名日期、标完成或删除；默认最近访问应用','相对时间/多项识别、删除确认、通知实际触达、部分失败与离线保存未核；不是代理后台Tasks工作流','cases/M-042-gemini.md')
flow('F-M060-17','M-060','校准笔尖位置并复核后保存','E-QGNT-01','Gen.2设置→描虚线→抬笔显线→方向调整→确认描线→必要时再调整→保存','仅Gen.2手册；没有设备操作，取消/断电/重启保存和校准误差未知','cases/M-060-quaderno.md')
write_csv(L/'batches/notes-and-ink-coverage.csv',new)

ip=L/'batches/gemini-controls-deepening-inventory.csv';rs=rows(ip);ic=[]
prior={'I-GECD-002':('E-GECD-04','F-M042-08'),'I-GECD-003':('E-GECD-03','F-M042-07'),'I-GECD-012':('E-GEM-03','F-M042-03;F-M042-04')}
notes={'I-GECD-048','I-GECD-050','I-GECD-055','I-GECD-057','I-GECD-063','I-GECD-067','I-GECD-068','I-GECD-072'}
for r in rs:
    k=r['entry_id'];before=r.copy()
    if k in prior:
        eid,fid=prior[k];r.update(review_status='child_help_read_previously_task_partial',related_flow_ids='F-M042-02;'+fid,gap=f'纠正旧状态：{eid}早前已读；非本轮新正文；实际回执/全部动作仍缺')
    elif k in notes:
        r.update(review_status='child_help_text_read_task_partial',related_flow_ids='F-M042-02;F-M042-12',gap='E-GENR-01具体OEM章节正文已读；型号/安装/语言条件保留；未执行、未核全部写入/恢复')
    elif k=='I-GECD-064':
        r.update(review_status='child_help_text_read_task_partial',related_flow_ids='F-M042-02;F-M042-13',gap='E-GENR-02已读；默认连接仍有机型/安装条件，非实操')
    elif k=='I-GECD-045':
        r.update(review_status='selected_child_help_read_parent_help_not_read',related_flow_ids='F-M042-02;F-M042-12;F-M042-13',gap='仅Keep/Google Tasks子帮助E-GENR-01/02已读；Workspace主体及其他子能力未完成')
    if r!=before:ic.append(dict(entry_id=k,before=before,after=r.copy()))
for i,(title,eid,fid,loc,gap) in enumerate([
    ('对话生成、指定落点和列表追加','E-GENR-01','F-M042-12','Connect / Create / Add item','同名目标、重复写入、回读未知'),
    ('前台Undo与Live后台例外','E-GENR-01','F-M042-10;F-M042-12','Add item: final Tip','没有写前批准或无限撤销保证'),
    ('前文/图片转待办','E-GENR-02','F-M042-13','Add reminders & tasks','识别错误、时间解析、批量部分成功未知'),
    ('查询、修改、标完成和删除待办','E-GENR-02','F-M042-13','Show or update','删除确认、通知触达、真实保存未知')],130):
    r={k:'' for k in rs[0]};r.update(entry_id=f'I-GECD-{i:03d}',scope='Notes and reminders',entry_type='documented_control_branch',title=title,locator=loc,url=gem[int(eid[-2:])-1]['url'],evidence_id=eid,related_flow_ids=fid,review_status='body_read_task_partial',gap=gap,checked_on='2026-10-27',declared_requirements='Signed in; Keep Activity; Workspace or supported OEM conditions',declared_platforms_and_experiences='Android help; actual build unknown',detail_urls=gem[int(eid[-2:])-1]['url']);rs.append(r)
assert len(rs)==133 and len(ic)==13;write_csv(ip,rs);dump(W/'gemini-inventory-updates.json',ic)
qi=[]
for i,(level,title,page) in enumerate(qd.get_toc(),1):
    fid='F-M060-17' if page==51 and '調整' in title else ('F-M060-11' if '一覧' in title and page==56 else ('F-M060-12' if page in [27,52,53,54,55,56,57] else ''))
    qi.append(dict(inventory_id=f'I-QGNT-{i:03d}',model='Gen.2 FMVDP41/FMVDP51',item=title,locator=f'PDF/print p{page}; outline level {level}',coverage_status='start_page_text_read_not_whole_section' if page in qpages else 'toc_extracted_not_reviewed',evidence_ids='E-QGNT-01',flow_ids=fid,gap='仅所列正文页；父项覆盖不能由首页推定，其余未读/未操作'))
assert len(qi)==197;write_csv(L/'batches/quaderno-gen2-ink-inventory.csv',qi)

rp=rows(L/'reference-points.csv')
def ref(rid,pid,tags,fids,eids,loc,artifact,pre,action,result,basis,persist,unknown,version):
    rp.append(dict(reference_id=rid,product_id=pid,question_tags=tags,flow_ids=fids,evidence_ids=eids,source_locator=loc,artifact_path=artifact,precondition=pre,action=action,visible_result=result,evidence_basis=basis,persistence_result=persist,unknown_branches=unknown,version_scope=version,checked_on='2026-10-27'))
g='artifacts/gemini-notes-reminders/source-notes.md';gv='Android English official help 2026-10-27; build unknown; account/activity/OEM limits'
ref('RP-M042-01','M-042','AI;目标应用;笔记','F-M042-02;F-M042-12','E-GENR-01','Connect / Tips',g,'已连接多个笔记应用','不指定应用或用@指定','文档默认最近访问者；@可选目标','official_help_text_reconstruction','未操作或回读对象','同名;重复提交;实际路由',gv)
ref('RP-M042-02','M-042','AI;内容读取;动作范围','F-M042-12','E-GENR-01','Unsupported / Create / Get content',g,'支持的笔记连接','检索内容、创建或追加','文字任务有说明；绘图/图片、分享、部分OEM修改删除有边界','official_help_text_reconstruction','没有全数据或附件可读证明','对象消歧;附件理解;动作失败',gv)
ref('RP-M042-03','M-042','AI;撤销;后台;确认','F-M042-10;F-M042-12;F-M042-13','E-GENR-01;E-GENR-02','Add末尾Undo Tips',g,'前台或Live后台创建动作','请求撤销或打开回复内对象链接','文档列通知Undo与目标编辑；Live后台例外','official_help_text_reconstruction','未验证撤销落盘；不是写前批准','时限;部分完成;重启;回滚',gv)
ref('RP-M042-04','M-042','AI;待办;状态;图片','F-M042-13','E-GENR-02','Add / Show or update',g,'已连接Google Tasks或受支持Reminder','由前文/图建任务，再查询或改状态','有改日期、改名、标完成和删除的文档路径','official_help_text_reconstruction','未验证真实记录、通知或同步','时间解析;部分识别;确认;离线',gv)
q='artifacts/quaderno-gen2-ink/source-notes.md';qv='QUADERNO Gen.2 FMVDP41/FMVDP51; B6FK-5131-08 Z0-02 issued 2025-04; firmware unknown'
ref('RP-M060-01','M-060','手写;校准;反馈;保存','F-M060-17','E-QGNT-01','p51',q,'Gen.2笔尖位置设置','描线→抬笔→调位→再描→保存','文档称抬笔才显线；不合适可回上步调整','official_manual_text_reconstruction','只有保存按钮说明，未测落盘','取消;断电;精度;重启',qv)
ref('RP-M060-02','M-060','手写;剪贴;撤销;重启','F-M060-12','E-QGNT-01','p27/52/56',q,'有笔迹的文档','圈选、复制/剪切、粘贴调位、确认','文档单次粘贴后入口隐藏；翻页重置撤销','official_manual_text_reconstruction','手册称笔迹即时保存；剪贴重启消失','跨页切取后崩溃;历史恢复',qv)
ref('RP-M060-03','M-060','显示;颜色;数据;导出','F-M060-12','E-QGNT-01','p53',q,'Gen.2已选择笔色','设备显示、PC传出或截图查看','手册称设备灰度与电脑颜色不同','official_manual_text_reconstruction','不能由显示判断颜色数据不存在','接收阅读器;往返可编辑;色彩',qv)
ref('RP-M060-04','M-060','手写;编辑限制;外部内容','F-M060-11;F-M060-12','E-QGNT-01','p56–57',q,'文档含不同来源批注','批注列表定位；设置编辑限制','文档默认限制其他设备添加的批注','official_manual_text_reconstruction','未证明用户身份、全文件只读或恢复','外部PDF批注兼容;权限拒绝',qv)
assert len(rp)==122;write_csv(L/'reference-points.csv',rp)

add_text(L/'cases/M-042-gemini.md','''## 2026-10-27：笔记、待办及目标应用

本次只补两篇Android帮助正文，见[逐项摘记与失败边界](../artifacts/gemini-notes-reminders/source-notes.md)。日历返回429、Workspace正文未取得；没有观察界面、授权或写入。

| F编号 | 操作链 | 条件与未知 | 证据 |
| --- | --- | --- | --- |
| F-M042-12 创建/查询笔记并追加列表 | 登录及活动开启→连接→@指定或默认最近访问应用→根据对话生成、查找，向Keep既有列表追加→通知Undo或打开链接编辑。 | Keep需同账号Workspace；OEM限制分别查，图片/绘图和分享不在支持范围。Live后台笔记不可同样撤销；同名、失败、重复请求、保存回读未知。 | E-GENR-01；D2 |
| F-M042-13 管理待办状态 | 连接Google Tasks/受支持Samsung Reminder→由前文/图片等添加→查询→改日期/名称、标完成或删除。 | 默认最近访问应用，@可指定。实际通知、删除确认、部分成功、时间解析和离线未知；此待办不是后台代理工作流。 | E-GENR-02；D2 |

F02补具体连接条件，F10补Undo背景范围。通知中的确认反馈不能当作写入前的人为批准；另一个自定义连接器的写前确认也不自动适用于这些应用。笔记页Tips有日历措辞，按原文疑点保留。

原目录从129增至133行，85连接器条目数未变；校正3条以前已读却仍标未读的记录，并为本次8种OEM笔记、Reminder及Workspace局部子帮助标明真实覆盖。详细任务仍全部D2，不代表全部连接器已读。
''')
add_text(L/'cases/M-060-quaderno.md','''## 2026-10-27：Gen.2另列的笔迹与校准

本节对象是FMVDP41/FMVDP51，B6FK-5131-08 Z0 / Z0-02、发行2025-04；不把此前Gen.3C界面当作这一代的事实。[原页短摘与读取范围](../artifacts/quaderno-gen2-ink/source-notes.md)对应E-QGNT-01，10页正文已读，无看图/设备操作。[197节点目录](../batches/quaderno-gen2-ink-inventory.csv)是提取台账，未读条目保留，父项不能因首页已读而算整章完成。

F11追加Gen.2 p56批注列表分栏/跳页/退出；F12追加p27、52–57的保存、剪贴、撤销、颜色与外部批注限制。设备灰度呈现与颜色数据不同；范围选择后的确认、笔迹即时保存、翻页清空撤销及重启清空剪贴也不是同一状态。原Gen.3C分支继续独立保留。

| F编号 | 前置、操作与反馈 | 缺口 | 证据 |
| --- | --- | --- | --- |
| F-M060-17 校准笔尖位置 | Gen.2 Home→设置→本体设置→笔尖调整→描虚线，抬笔才显线→下一步用方向键对齐→再描验证，必要时再调整→保存。 | 最初描线不能擦除；取消、断电、重启后是否保留及误差未测。不能当作Gen.3C实测。 | E-QGNT-01 p51；D2 |

Gen.2传出/同步/备份、删除及故障章节尚未核读；目前不能保证笔迹在传出再导入后仍可编辑。Gen.3C仍有58页无正文阅读记录。目录检查所得未知已保留，不以PDF下载成功代替通读或交互完成。
''')
# Replace a stale handoff sentence, not a behavior claim.
p=L/'cases/M-060-quaderno.md';t=p.read_text().replace('覆盖主表/旧批未改，交根代理逐字段建议。','对应主表与所属批次已按稳定编号维护。');p.write_text(t)
(W/'APPLIED').write_text('Formal records applied once; do not rerun.\n')
print(json.dumps(dict(new_evidence=3,new_flows=3,old_flows_updated=4,new_reference_points=8,gemini_inventory_rows=133,gen2_toc_nodes=197,requirements_sha256=sha(R/'docs/requirements.md')),ensure_ascii=False))
