from pathlib import Path
import csv, hashlib, json

ROOT=Path('/home/workspace/reader'); LIB=ROOT/'docs/research/interaction-library'; B=LIB/'batches'
OUT=LIB/'artifacts/week-reader-recovery'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def jl(p):return [json.loads(l) for l in p.read_text().splitlines()]
def writejl(p,rows):p.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in rows))
def csvread(p):return list(csv.DictReader(p.open()))
def csvwrite(p,rows):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

p=B/'week-junior-controls-deepening-evidence.jsonl';rows=jl(p)
audit=str((OUT/'browser-operation-audit.md').relative_to(LIB));ah=sha(LIB/audit)
hashes={str(q.relative_to(LIB)):sha(q) for q in sorted(OUT.iterdir()) if q.is_file()}
notes={
'E-WJCD-02':'补桌面缩放/平移、重排听读UI与窄屏菜单/跳页。暂停2:52/1.5x后关闭外层再开自动续放，关闭播放器保留正文；第二次重启耳机入口未见，音频恢复未知。未听声音、未测实体设备。',
'E-WJCD-03':'续查封面两次搜索入口未展开、第2页展开；Page15标题等3秒及缩略图等1.5秒仍未跳转。旧空结果分支保留，失败原因未定。',
'E-WJCD-04':'手动保存14后回封面，同资料目录重启浏览器回14，第二次重启页码仍恢复；新窄屏空上下文从1。页级书签不证明音频秒点、跨端或账户恢复。'
}
for e in rows:
    if e['evidence_id'] not in notes:continue
    assert not any(c.get('batch')==38 for c in e.get('additional_captures',[]))
    e.setdefault('previous_operation_audits',[]).append({'path':e['operation_audit_path'],'sha256':e['operation_audit_sha256']})
    e['additional_captures']=e.get('additional_captures',[])+[{'batch':38,'checked_on':'2026-10-27','url':e['url'],'method':'automated_public_browser_actions_and_screenshot_review','scope':notes[e['evidence_id']],'audit_path':audit,'audit_sha256':ah,'state_range':'00–36; exact scope and failures in audit','raw_work_path':'work/research/batch38-week','environment_path':str((OUT/'environment.json').relative_to(LIB))}]
    e['operation_audit_path']=audit;e['operation_audit_sha256']=ah
    e['artifact_hashes'].update(hashes)
    e['last_checked_on']='2026-10-27'
writejl(p,rows)

p=B/'week-junior-controls-deepening-coverage.csv';rows=csvread(p)
for f in rows:
    if f['flow_id']=='F-M048-05':
        f['known_branches']='原桌面1→2、缩略图、14/首页/32；续查窄屏触摸菜单/页码14。第二次重启后Next捕获空白且仍14，Previous回14，未记成功到15。'
        f['gaps']='实体触控/手势、非法页码、断网与全部阅读完成未测；原隐藏控件/侧栏遮挡失败保留。'
    if f['flow_id']=='F-M048-06':
        f['known_branches']='Nobel有5页组；原空结果/清空已测。续查封面两次入口不展开，第2页可以；Page15标题等3秒/缩略图等1.5秒仍14。'
        f['gaps']='入口/结果未跳转原因未定；逐个命中、中文搜索、排名与断网未测；不外推全部环境。'
    if f['flow_id']=='F-M048-07':
        f['known_branches']='Save14后回封面，同上下文重访及同资料目录两次浏览器重启均回14；旧空上下文、新窄屏空上下文从1。'
        f['gaps']='存储机制、覆盖/删除书签、清站点数据、跨端/账户与刷机恢复未测；不能从页书签推导音频秒点恢复。'
    if f['flow_id'] in ['F-M048-05','F-M048-06','F-M048-07']:f['checked_on']='2026-10-27'
csvwrite(p,rows)
new=[]
for num,task,eids,known,gaps in [
('11','公开样刊版面放大、平移与返回','E-WJCD-02','第2页Zoom→加号放大→鼠标拖动→X回适页第2页，画面和控件实际观察。','缩小/滑块、触摸缩放、全屏、倍率持久化及电子纸性能未测。'),
('12','公开样刊重排听读UI、关闭与接续','E-WJCD-02;E-WJCD-04','耳机开重排/计时，高亮/滚动；暂停、1.5x、再播放；2:52暂停后同页关闭再开自动续放，关闭播放器保留正文；第二次浏览器重启无耳机入口，定位超时。','只观察UI未听声音；同页iframe保留不等于持久化，重启音频恢复未知；跨文章/用户/断网/来电/睡眠、精确音文对齐与音量未测。')]:
    new.append(dict(flow_id='F-M048-'+num,product_id='M-048',task=task,depth='D4',evidence_ids=eids,known_branches=known,gaps=gaps,visual_status='observed_browser_sequence',hands_on='true',case_path='cases/M-048-week-junior.md',checked_on='2026-10-27'))
dest=B/'week-reader-recovery-coverage.csv';assert not dest.exists();csvwrite(dest,new)

p=B/'tonie-controls-deepening-inventory.csv';rows=csvread(p)
for r in rows:
    if r['inventory_id']=='I-M038-139':
        r['article_url']='https://support.tonies.com/hc/en-gb/articles/29036585437714-How-much-help-will-my-child-need-with-playing-a-Tonieplay-game'
        r['coverage_status']='index_only_fetch_blocked'
        r['gap']='2026-10-27一次匿名GET返回403/cf-mitigated:challenge后停止，正文未读。命令另有本地解析exit1；归档artifacts/tonieplay-source-attempt/source-notes.md。未提高任务深度。'
csvwrite(p,rows)
p=B/'tonie-controls-deepening-evidence.jsonl';rows=jl(p)
for e in rows:
    if e['evidence_id']=='E-TNCD-03':
        e.setdefault('access_attempts',[]).append({'checked_on':'2026-10-27','inventory_id':'I-M038-139','http_status':403,'body_read':False,'note':'单次子文章请求受challenge阻挡；本地解析另exit1，未继续请求其他候选。不是新增行为证据。','audit_path':'artifacts/tonieplay-source-attempt/source-notes.md','attempt_path':'artifacts/tonieplay-source-attempt/attempt.json'})
        e.setdefault('artifact_hashes',{}).update({str(q.relative_to(LIB)):sha(q) for q in (LIB/'artifacts/tonieplay-source-attempt').iterdir()})
writejl(p,rows)
print('Updated 3 old flows, added 2 D4 flows; appended 3 browser E captures and 1 stopped-source attempt; 1 inventory row updated.')
