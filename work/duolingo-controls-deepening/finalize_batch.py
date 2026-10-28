import csv,json,hashlib,shutil
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/duolingo-controls-deepening';A=L/'artifacts/duolingo-controls-deepening';B=L/'batches'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def js(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rs):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
assert not (B/'duolingo-controls-deepening-coverage.csv').exists(),'one-time finalizer already applied'
selected={2,11,16,18,23,24,26,27,45,46,50,53,56,57}
viewed={1,2,4,11,16,18,23,24,26,27,45,46,50,53,56,57}
states=[];manifest=[]
for f in sorted((W/'browser').glob('[0-9]*.json')):
 d=json.loads(f.read_text());n=int(f.name[:2]);d['screenshot_reviewed']=n in viewed
 d['archived_screenshot']='browser/'+f.stem+'.png' if n in selected else None
 if n in selected:shutil.copyfile(f.with_suffix('.png'),A/'browser'/f.with_suffix('.png').name)
 states.append(d)
for f in sorted((W/'browser').glob('*')):
 if f.is_file():manifest.append(dict(path=str(f.relative_to(R)),sha256=sha(f),bytes=f.stat().st_size))
js(A/'browser/observed-states.json',states)
js(A/'browser/trace-actions.json',[json.loads(x) for x in (W/'browser/actions.jsonl').read_text().splitlines()])
js(A/'browser/browser-errors.json',[json.loads(x) for x in (W/'browser/errors.jsonl').read_text().splitlines()])
js(A/'browser/raw-capture-manifest.json',manifest)
audit=(W/'browser-audit-draft.md').read_text()+'\n## 正式截图索引\n\n'
for s in states:
 if s['archived_screenshot']:audit+=f'- [{s["name"]}]({s["archived_screenshot"]})\n'
(A/'browser-operation-audit.md').write_text(audit)
E=json.loads((W/'prepared-evidence.json').read_text());auditrel=str((A/'browser-operation-audit.md').relative_to(L));ah=sha(A/'browser-operation-audit.md')
art={str(p.relative_to(L)):sha(p) for p in sorted((A/'browser').glob('*')) if p.is_file()}
for i,title,loc,note in [
 (25,'匿名课程与目标选择','01–16','真实Web选择法语、零基础、5分钟与从头开始进入课；未提供身份或登录。'),
 (26,'首课错答、词义提示、听力暂缓与结束尝试','16–24、28–53','首题故意错误5→4心；悬停提示/正确反馈；听力暂缓提示15分钟。最后回基础选择，未观察错题回顾或结算。'),
 (27,'退出警告、取消退出与确认结束','25–28、53–57','取消退出回原题；重新进入答一题后确认End session回学习页。未在确认退出后第三次进入测试保存。')]:
  E.append(dict(evidence_id=f'E-DUCD-{i:02}',product_id='M-031',title=title,url='https://www.duolingo.com/' if i==25 else 'https://www.duolingo.com/lesson',checked_on='2026-10-27',source_type='public_web_application',access_mode='automated_public_browser_actions_and_screenshot_review',version_scope='Anonymous English desktop Web; French beginner; Chromium156.0.8078.4 Linux 1280x950; unknown build; no child/native/device test',app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=True,visual_observed=True,visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=auditrel,operation_audit_sha256=ah,archived_artifact=auditrel,sha256=ah,capture_state='operation_audit_actions_and_selected_screenshots_archived',supersedes=None,artifact_hashes=art))
(B/'duolingo-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
F=[]
for n,task,depth,eids,branches,gaps in json.loads((W/'flow_specs.json').read_text()):
 F.append(dict(flow_id=f'F-M031-{n:02}',product_id='M-031',task=task,depth=depth,evidence_ids=eids,known_branches=branches,gaps=gaps,visual_status='observed_browser_sequence' if depth=='D4' else 'not_observed',hands_on='true' if depth=='D4' else 'false',case_path='cases/M-031-duolingo-adventures.md',checked_on='2026-10-27'))
csvout(B/'duolingo-controls-deepening-coverage.csv',F)
inv=[]
for n,title in [(1,'Using Duolingo'),(2,'Account Management'),(3,'Subscription & Payments')]:
 inv.append(dict(entry_id=f'M031-HG{n}',parent_id='',entry_type='help_group',title=title,url='https://www.duolingo.com/help',evidence_ids='E-DUCD-01',related_flow_ids='',review_status='index_only',gaps='父分类不是独立功能；组内正文逐项登记',checked_on='2026-10-27'))
for n,h in enumerate(json.loads((W/'help-article-results.json').read_text()),1):
 eid=f'E-DUCD-{n+1:02}';fs=';'.join(f['flow_id'] for f in F if eid in f['evidence_ids'].split(';'))
 inv.append(dict(entry_id=f'M031-H{n:02}',parent_id=f'M031-HG{1 if n<=5 else 2 if n<=11 else 3}',entry_type='help_article',title=h['title'],url=h['url'],evidence_ids=eid,related_flow_ids=fs,review_status='text_read_partial_task_analysis' if fs else 'text_read_no_task_reconstruction',gaps='正文可读不等于当前界面/全部分支已核；见关联任务，未映射处本批只登记范围',checked_on='2026-10-27'))
for e in E[18:24]:
 inv.append(dict(entry_id='M031-B'+e['evidence_id'][-2:],parent_id='',entry_type='supplementary_blog',title=e['title'],url=e['url'],evidence_ids=e['evidence_id'],related_flow_ids=';'.join(f['flow_id'] for f in F if e['evidence_id'] in f['evidence_ids'].split(';')),review_status='dated_article_text_read',gaps='非帮助目录条目；日期/灰度/平台范围及操作缺口见案例',checked_on='2026-10-27'))
csvout(B/'duolingo-controls-deepening-inventory.csv',inv)
case=L/'cases/M-031-duolingo-adventures.md';old=case.read_text();assert old==(W/'M-031-before.md').read_text(),'case changed since backup'
old=old.replace('# M-031 Duolingo Adventures：情境目标与纠错，不等同于儿童识字','# M-031 Duolingo：主学习应用与 Adventures 分开研究',1)
old=old.replace('查阅日：2026-10-26。','查阅日：2026-10-26；主学习应用及匿名网页补证：2026-10-27。',1)
old=old.replace('以下仅研究 Adventures 相关任务域，完整关卡/子功能目录及整个 Duolingo 的功能目录均未核定。','以下历史部分仅研究 Adventures；本页后半另补主学习应用。完整关卡、子功能及整个App目录仍未核定。',1)
old=old.replace('无亲自操作、无连续视觉观察。','此段Adventures无亲自操作、无连续视觉观察；后续普通课另有有限网页操作。',1)
rows=['| F编号 / 深度 | 任务及路径/分支 | 证据 | 仍缺 |','| --- | --- | --- | --- |']
for f in F:rows.append(f'| {f["flow_id"]} / {f["depth"]} | {f["task"]}：{f["known_branches"]} | {f["evidence_ids"].replace(";","、")} | {f["gaps"]} |')
summary='F-M031-16～18实际保存57个状态、14张必要截图、动作和定位失败。首题错误后5→4心；词义悬停得到提示；不能听时显示15分钟后恢复，并进入文字题。课中取消退出回同一题。最后一项答题后回到学习页/基础选择，未见错题回顾或奖励结算，原因未定位；再次进入从coffee题开始，红心保持4。第二次课答一题后确认退出返回学习页，未第三次进入核恢复。详见[逐状态审计](../artifacts/duolingo-controls-deepening/browser-operation-audit.md)。'
addition=(W/'case-addition.md').read_text().replace('{{FLOW_TABLE}}','\n'.join(rows)).replace('{{BROWSER_SUMMARY}}',summary)
case.write_text(old+addition)
products=list(csv.DictReader((L/'catalog.csv').open()))
for p in products:
 if p['product_id']=='M-031':p['name']='Duolingo（主学习应用与 Adventures）'
csvout(L/'catalog.csv',products)
js(W/'finalize-report.json',dict(evidence=len(E),new_flows=len(F),inventory=len(inv),browser_states=len(states),screenshots=len(selected),actions=len(json.loads((A/'browser/trace-actions.json').read_text())),errors=len(json.loads((A/'browser/browser-errors.json').read_text()))))
print((W/'finalize-report.json').read_text())
