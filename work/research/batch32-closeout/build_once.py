from pathlib import Path
import csv,json,hashlib,shutil,copy
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';out=root/'work/research/batch32-closeout'
assert not (out/'applied.json').exists(), 'Already applied'
prefix='learning-games-and-completion'
assert not (lib/'batches'/f'{prefix}-evidence.jsonl').exists()
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvread(p):return list(csv.DictReader(p.open()))
def csvwrite(p,rows,fields=None):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields or list(rows[0]));w.writeheader();w.writerows(rows)
mastere={e['evidence_id']:e for e in map(json.loads,(lib/'evidence.jsonl').read_text().splitlines())}
masterf={f['flow_id']:f for f in csvread(lib/'coverage.csv')}
quiz='artifacts/quizlet-game-modes/source-notes.md';anki='artifacts/anki-session-completion/source-notes.md'
duo='artifacts/duolingo-chess-interactions/source-notes.md';audit='artifacts/duolingo-chess-interactions/browser-operation-audit.md'
variants={'checked_on':'2026-10-27','url':'https://help.quizlet.com/hc/en-us/articles/27393733229965-Playing-a-game-of-Blast','helper_snapshot':{'path':'work/research/batch32-quizlet/raw/blast.txt','sha256':digest(root/'work/research/batch32-quizlet/raw/blast.txt'),'observation':'搜索抽取摘要为teacher-only，缺单人段落；不是HTTP原件。'},'root_review':{'method':'web_open_official_source','locator':'正文17–71行','observation':'多人Web主持为学生和教师；另含Web/App单人入口；移动端可加入多人。','http_status':None},'version_relation':'两次无修订日期；不推断准确改版时间。'}
dump(lib/'artifacts/quizlet-game-modes/source-variant-record.json',variants)
def evidence(eid,pid,title,url,notes,locator,note,mode='official_help_web_text',scope='Web/iOS/Android帮助；构建与修订日未知'):
    return dict(evidence_id=eid,product_id=pid,title=title,url=url,source_type='official_help' if pid=='M-021' else 'official_product_article',access_mode=mode,checked_on='2026-10-27',source_updated_on=None,version_scope=scope,app_build=None,locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=notes,sha256=digest(lib/notes),supersedes=None)
urls=[
('Match','360031183611-Playing-Match','19–61行','配对、错配加时、完成分数及手机重开；未操作。'),
('Blocks','37203179032717-Playing-a-game-of-Blocks','17–74行','放块与答题交替、星标/题型、无步结束；错误和恢复未知。'),
('Blast','27393733229965-Playing-a-game-of-Blast','17–71行','root当前正文含学生/教师Web主持和单人入口；保留搜索摘要差异，不套teacher-only。'),
('Classic Live加入','360031249731-Helping-players-join-a-game-of-Classic-Quizlet-Live','17–67行','仅加入流程，主持身份与玩家资格分开；不是完整对局。')]
new_e=[]
for i,(title,slug,loc,note) in enumerate(urls,1):
    lang='en-au' if i==4 else 'en-us'
    e=evidence(f'E-QZGM-{i:02}', 'M-021',title,f'https://help.quizlet.com/hc/{lang}/articles/{slug}',quiz,loc,note)
    if i==3:e['artifact_hashes']={'artifacts/quizlet-game-modes/source-variant-record.json':digest(lib/'artifacts/quizlet-game-modes/source-variant-record.json')}
    new_e.append(e)
e=evidence('E-DUCX-01','M-031','Chess课程范围与渐进任务','https://blog.duolingo.com/chess-course/',duo,'Learning by doing/Scaffolded progression/FAQ','文章未看图；不采用能力与效果宣传指标。','anonymous_public_HTML_GET','官方介绍发布2025-06-10、修改2026-10-01；不等于App构建')
e.update(source_updated_on='2026-10-01',source_published_on='2025-06-10',raw_work_path='work/duolingo-chess-interactions/chess-course.html',raw_sha256=digest(root/'work/duolingo-chess-interactions/chess-course.html'))
new_e.append(e)
e=evidence('E-DUCX-02','M-031','Chess匿名入门课82状态操作链','https://www.duolingo.com/chess',audit,'状态01–82；F-M031-19，另补F-M031-16/18','11题面、同题错答/提示/重试、退出重进、结算及刷新；7天目标保存请求403，不称成功。','automated_public_browser','2026-10-27匿名英语Web；Chromium156.0.8078.4；不是原生App/儿童实测')
e.update(source_type='actual_public_browser_operation',hands_on=True,visual_observed=True,visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=audit,operation_audit_sha256=digest(lib/audit))
e['artifact_hashes']={str(p.relative_to(lib)):digest(p) for p in sorted((lib/'artifacts/duolingo-chess-interactions').rglob('*')) if p.is_file()}
new_e.append(e)
assert not {e['evidence_id'] for e in new_e}&set(mastere)
assert not {e['url'] for e in new_e}&{e['url'] for e in mastere.values()}
def flow(n,pid,task,eids,known,gaps,actual=False):
    name='M-021-quizlet.md' if pid=='M-021' else 'M-031-duolingo-adventures.md'
    return dict(flow_id=f'F-M{pid[2:]}-{n:02}',product_id=pid,task=task,depth='D4' if actual else 'D2',evidence_ids=eids,known_branches=known,gaps=gaps,visual_status='observed_browser_sequence' if actual else 'not_observed',hands_on='true' if actual else 'false',case_path='cases/'+name,checked_on='2026-10-27')
new_f=[
flow(14,'M-021','Match配对、计时与重开','E-QZGM-01','集→Match→开始；每局6对，错配加1秒，配完看分；手机Options重开、图示集配对类型。','重开确认、退出/断线恢复、历史分数及套餐未核。'),
flow(15,'M-021','Blocks答题换拼块与结束','E-QZGM-02','至少6对；3块放完答题取得新块，成行/列得分，无步结束；可换题、只学星标、改题型。','错答/换题代价、取消/棋盘恢复、最高分保存和具体资格未实测。'),
flow(16,'M-021','Blast单人与多人模式入口及规则','E-QZGM-03','root正文含学生/教师Web主持多人、单人集→Blast→Play；主持设置速度/选项/时长，玩家匹配目标计分，计时结束；地区/英语集限制。','与早先搜索teacher-only摘要冲突；版本、错误反馈、提前结束/重连、同分/历史成绩及套餐未核。'),
flow(17,'M-021','加入Classic Live并等主持开始','E-QZGM-04','浏览器代码→Join→名字→Next；App登录从Search/Play Quizlet Live加入，可扫QR；主持开始。','无效码、游客资格、重名、取消/重连、实际答题/结算未核；仅en-AU帮助。'),
flow(19,'M-031','国际象棋入门解谜、纠错与课后结算','E-DUCX-01;E-DUCX-02','匿名棋课11题面；自由移动与3步约束不同，错序5→4心、同题重置、提示开关、重试成功；结算20XP/92%、任务/奖励；Later后刷新保留当前字段。','7天目标PUT403，保存未证；提示完整表达、评分公式、清存储/跨进程/账号恢复、完整课程/对局及儿童学习效果未核。',True)]
assert not {f['flow_id'] for f in new_f}&set(masterf)
updates={
'F-M020-01':{'known_branches':masterf['F-M020-01']['known_branches']+'；S回概览、Decks回列表；结束只到当天应显示队列耗尽。','gaps':masterf['F-M020-01']['gaps']+'；未评分卡导航恢复和完成页文案未知。'},
'F-M020-12':{'known_branches':masterf['F-M020-12']['known_branches']+' Empty归还卡并保留空容器；Rebuild按条件重取，非旧集合快照；删除另移除容器。','gaps':'Empty/Delete确认及Undo、Rebuild失败反馈、预览练习历史口径未知。','checked_on':'2026-10-27'},
'F-M020-17':{'known_branches':masterf['F-M020-17']['known_branches']+' Stats/T打开，默认当前及子牌组；可切全集、搜索和历史范围，Today不随历史范围；选择器2.1.61+。','gaps':'全部统计图、选择持久化、跨时区与历史修正；正文上下位置措辞未看图核对。','checked_on':'2026-10-27'},
'F-M031-16':{'evidence_ids':masterf['F-M031-16']['evidence_ids']+';E-DUCX-02','known_branches':masterf['F-M031-16']['known_branches']+'；另实际从Chess页选零基础、5分钟、从零开始；课后Later跳过注册。','gaps':'不同选择/回退、儿童与中文母语、正式账号及原生端未核；匿名会建立临时服务端状态。'},
'F-M031-18':{'evidence_ids':masterf['F-M031-18']['evidence_ids']+';E-DUCX-02','known_branches':masterf['F-M031-18']['known_branches']+'；Chess首题取一星后X直接离开，无语言课确认；同会话重进首题初盘。','gaps':'棋课其余题和正式账号退出规则、跨进程/设备、离线恢复未知；法语课先前未结算原因仍缺。'}}
capture_specs=[('E-ANK-01','studying.md','Decks/Study Overview 20–47'),('E-AND-11','filtered-decks.md','Home Decks/Creating Manually 70–115'),('E-AND-14','stats.md','Statistics/Selecting Decks/Today 41–109')]
e_updates={}
for eid,name,loc in capture_specs:
    row=copy.deepcopy(mastere[eid]);cap=dict(checked_on='2026-10-27',access_mode='local_reread_existing_official_markdown',raw_work_path='work/research/batch32-anki/raw/'+name,raw_sha256=digest(root/'work/research/batch32-anki/raw'/name),locator=loc,source_notes_path=anki,source_notes_sha256=digest(lib/anki),note='本次本地复读，HTTP200为先前采集记录，不冒充新请求。')
    row.setdefault('additional_captures',[]).append(cap);e_updates[eid]=row
# Stage owning files and validate all before mutating.
staged={};found_f=set();found_e=set();f_change=[]
for p in sorted((lib/'batches').glob('*-coverage.csv')):
    rows=csvread(p);changed=False
    for row in rows:
        fid=row['flow_id']
        if fid in updates:
            assert row==masterf[fid],fid
            before=copy.deepcopy(row);row.update(updates[fid]);changed=True;found_f.add(fid)
            f_change.append(dict(flow_id=fid,owning_file=str(p.relative_to(root)),before=before,after=row,fields_changed=[k for k in row if row[k]!=before[k]]))
    if changed:staged[p]=rows
for p in sorted((lib/'batches').glob('*-evidence.jsonl')):
    rows=[json.loads(l) for l in p.read_text().splitlines() if l.strip()];changed=False
    for i,row in enumerate(rows):
        if row['evidence_id'] in e_updates:
            eid=row['evidence_id'];assert row==mastere[eid],eid
            rows[i]=e_updates[eid];found_e.add(eid);changed=True
    if changed:staged[p]=rows
assert found_f==set(updates) and found_e==set(e_updates)
before_dir=out/'before';before_dir.mkdir(exist_ok=True)
for p,rows in staged.items():
    shutil.copy2(p,before_dir/p.name)
    if p.suffix=='.csv':csvwrite(p,rows)
    else:p.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in rows))
for name in ['M-020-anki.md','M-021-quizlet.md','M-031-duolingo-adventures.md']:shutil.copy2(lib/'cases'/name,before_dir/name)
(lib/'batches'/f'{prefix}-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in new_e))
csvwrite(lib/'batches'/f'{prefix}-coverage.csv',new_f)
dump(out/'coverage-updates-applied.json',f_change)
dump(out/'evidence-updates-applied.json',[dict(evidence_id=eid,before=mastere[eid],after=row) for eid,row in e_updates.items()])
dump(out/'applied.json',dict(new_evidence=len(new_e),new_flows=len(new_f),new_D2=4,new_D4=1,old_flows=len(updates),old_fields=sum(len(x['fields_changed']) for x in f_change),old_evidence_captures=3,formal_images=24))
print((out/'applied.json').read_text())
