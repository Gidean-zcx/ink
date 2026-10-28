from pathlib import Path
import csv, json, hashlib, copy
from urllib.parse import urlsplit, unquote

R=Path('/home/workspace/reader'); L=R/'docs/research/interaction-library'; W=R/'work/research/batch33-closeout'
assert not (W/'applied.json').exists(), 'already applied; do not rerun'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p, obj): p.parent.mkdir(parents=True,exist_ok=True); p.write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n')
def csvread(p): return list(csv.DictReader(p.open(encoding='utf-8-sig')))
def csvwrite(p, rows):
    p.parent.mkdir(parents=True,exist_ok=True)
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
F={r['flow_id']:r for r in csvread(L/'coverage.csv')}
E={r['evidence_id']:r for r in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
assert len(F)==865 and len(E)==1443
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
intake=json.loads((R/'work/research/batch33-audible/intake.json').read_text())
new_e=[];new_f=[];notes={};old_f=[];old_e=[];inventory_updates=[]
cases={p:(L/'cases'/fn) for p,fn in [('M-037','M-037-yoto.md'),('M-038','M-038-tonie.md'),('M-039','M-039-audible.md')]}
folders={'M-037':'yoto-podcast-controls','M-038':'tonie-playback-recovery','M-039':'audible-family-controls'}
for pid,title in [('M-037','Yoto：播客导入与实体入口'),('M-038','Toniebox 2：对象、缓存与恢复'),('M-039','Audible：家庭档案、语音与播放端')]:
    notes[pid]=[f'# {title}：来源定位','', '2026-10-27。仅公开文档或已有原件复读，无登录、设备/App操作、图片观察、连续录像或音频播放。正式保留短摘记与定位，工具返回文本存work，不称原始HTML。事实及未知见对应案例；参考不自动成为需求。','', '| E编号 | 来源 | 正文定位与证据边界 |','| --- | --- | --- |']
def evidence(eid,pid,title,url,date,raw,locator,note,access='official_web_tool_extracted_text',**extra):
    row=dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-27',source_type='official_help',access_mode=access,version_scope={'M-037':'Yoto Player 3rd / Mini 2024 scope; article is general MYO help; actual firmware/app build unknown','M-038':'Toniebox 2 US English help; current installed firmware/app unknown; stated v1.6 is a troubleshooting threshold','M-039':'Audible US English help; platform per article; actual app/OS build unknown; Kids Profile distinct from Amazon Family child account'}[pid],app_build=None,firmware_version=None,source_updated_on=date,source_date_basis='displayed article/index date or embedded updatedAt; not a tested software version',locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=f'artifacts/{folders[pid]}/source-notes.md',sha256=None,capture_state='web_tool_text_snapshot_not_raw_html' if pid!='M-037' else 'previously_captured_category_article_local_read',supersedes=None,raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw),**extra)
    new_e.append(row)
    notes[pid].append(f'| {eid} | [{title}]({url}) | {locator}；{note} |')
    return row
for n,title,slug,date,snapshot,locator,note in intake['evidence']:
    evidence('E-AUFC-'+n,'M-039',title,'https://help.audible.com/s/article/'+slug,date,R/'work/research/batch33-audible'/snapshot,locator,note,'official_search_index_body' if n in ['13','14','15','23'] else 'official_web_tool_extracted_text')

tonie=[
('01','快速换公仔后播放了另一内容','29422419667346-Tonie-is-playing-the-audio-from-a-different-Tonie','pages-1.json','正文19行','移下、等几秒、再放目标公仔；可能原因不是内部竞态已证，也未给原秒点'),
('02','敲击切曲失效','29422120806546-Tapping-the-Toniebox-2-to-skip-forward-and-back-isn-t-working','pages-2.json','正文19行','无活动约五分钟自动关机后唤醒再试；没有位置持久性说明'),
('03','部分不播放故障的固件检查','29544159642514-My-Tonie-isn-t-playing','pages-2.json','正文19–36行','官方特定故障建议自动关机后开机联网，在App核v1.6或更高；非所有故障统一修复或当前最新版本'),
('04','普通内容与游戏的首次下载和取盘','29254033744914-Toniebox-2-download-speed-seems-slow','pages-2.json','正文19–36行','普通Tonie首章下完先播；游戏需全量。取走游戏盘下载仍后台继续，不等于取消或断电续传')]
for n,title,slug,snapshot,loc,note in tonie:
    evidence('E-TNPR-'+n,'M-038',title,'https://support.tonies.com/hc/en-us/articles/'+slug,None,R/'work/research/batch33-tonie'/snapshot,loc,note)

yp=R/'work/yoto-controls-deepening/extracted/category-cards-content-make-your-own.json'
yd=json.loads(yp.read_text()); found={}
def walk(x):
    if isinstance(x,dict):
        if x.get('hash') in ['BJCo_FsmMx','ByJ3uYjQGe']: found[x['hash']]=x
        for v in x.values():walk(v)
    elif isinstance(x,list):
        for v in x:walk(v)
walk(yd)
for n,key in [('01','BJCo_FsmMx'),('02','ByJ3uYjQGe')]:
    article=found[key]; p=R/f'work/research/batch33-yoto/article-{key}.json';dump(p,article)
    evidence('E-YTPD-'+n,'M-037',article['title'],'https://support.yotoplay.com/en_gb/'+article['slug']+'-'+key,article['updatedAt'],yp,f'embedded article hash={key}; body; root extracted full article to {p.name}','旧分类原件内正文，本轮无新HTTP；两个来源内容重合，不当独立验证。RSS与单集流URL不同。','local_reread_previously_captured_category_article',extracted_work_path=str(p.relative_to(R)),extracted_sha256=sha(p))

def flow(fid,pid,title,eids,known,gaps):
    return dict(flow_id=fid,product_id=pid,task=title,depth='D2',evidence_ids=eids,known_branches=known,gaps=gaps,visual_status='not_observed',hands_on='false',case_path=str(cases[pid].relative_to(L)),checked_on='2026-10-27')
for n,title,refs,known,gaps in intake['flows']:
    eids=refs[4:] if refs.startswith('old:') else ';'.join('E-AUFC-'+x for x in refs.split(';'))
    new_f.append(flow(f'F-M039-{n:02}','M-039',title,eids,known,gaps))
new_f.append(flow('F-M037-20','M-037','把内置电台、外部音频流或RSS播客绑定MYO','E-YTPD-01;E-YTPD-02;E-YOT-01','App内置Radio/Podcasts详情可直接绑卡；外部直播/单集URL在网页Playlist用Add Stream保存。播客订阅用RSS→Grab podcast details→见名称/集列表→Save，再回App MYO绑定，红图标转绿勾后取卡再插播放。','RSS不是网站地址；单集直链需手动更新，新集抓取/集序/重复/失败未知。播客为在线流，不把绑卡成功当已下载或故事断点保存。'))

def amend(fid, **changes):
    before=copy.deepcopy(F[fid]);after={**before,**changes}; fields=[k for k in before if before[k]!=after[k]]
    assert fields
    old_f.append(dict(flow_id=fid,before=before,after=after,fields_changed=fields))
amend('F-M038-09',known_branches=F['F-M038-09']['known_branches']+' US同ID帮助另给未接电时双耳持续60秒到关机再接电的重启，或等自动关机再开；联网开机后至少等30秒再放公仔。',gaps='60秒/30秒是官方操作提示非实测；普通故事的关机持久断点、网络差时同步完成确认和版本原子性未核。',checked_on='2026-10-27')
amend('F-M038-11',evidence_ids=F['F-M038-11']['evidence_ids']+';E-TNPR-01;E-TNPR-02;E-TNPR-03',known_branches=F['F-M038-11']['known_branches']+' 快换公仔播错对象可移下等几秒再放；敲击失效可等无活动自动关机再唤醒。特定下载故障页要求开机/自动关机/再开联网后在App核v1.6或更高。',gaps='文档排错不是保证恢复；普通故事位置、全部错误词、重置对云/游戏存档范围未核。v1.6是该故障门槛，不表示最新版本。')
amend('F-M038-12',evidence_ids=F['F-M038-12']['evidence_ids']+';E-TNPR-04',known_branches=F['F-M038-12']['known_branches']+' US正文明确取走Tonieplay盘后下载仍在后台继续；取盘不等于取消。普通Tonie卡住可移下等待再放，持续失败再重启。',gaps='两页开始阶段、逐步输入反馈及游戏判分仍缺；真正取消入口、断网/断电续传、完整缓存回执及存档恢复未知。官方等待时长不作实测。')
amend('F-M039-09',evidence_ids=F['F-M039-09']['evidence_ids']+';E-AUFC-02',known_branches=F['F-M039-09']['known_branches']+' Kids Profile是例外：帮助要求换设备时重新定位，多机同时听不等于位置跟随个人。',gaps=F['F-M039-09']['gaps']+' 儿童同机共享的个人进度隔离未证。')

# Additional captures preserve the identity and original source URL of existing evidence.
captured=[
('E-AUCD-01',R/'work/research/batch33-audible/directory-pages.json','https://help.audible.com/s/listening?language=en_US','首屏35标题及链接重新核对；仍有未展开Load more','M-039'),
('E-AUCD-23',R/'work/research/batch33-audible/lists-family-reviews-pages.json','https://help.audible.com/s/article/manage-collections','本次打开有正文28–129行；补创建/增删/跟随公开清单，原搜索采集保留','M-039'),
('E-TONI-14',R/'work/research/batch33-tonie/pages-2.json','https://support.tonies.com/hc/en-us/articles/34276878021138-My-Tonie-Creative-Tonie-is-not-playing-the-updated-content-on-the-Toniebox-2','同一文章ID的US地区正文19–39行；原en-150 URL及采集保留，不增同义来源编号','M-038'),
('E-TNCD-24',R/'work/research/batch33-tonie/pages-1.json','https://support.tonies.com/hc/en-us/sections/29258108517522-Troubleshooting','当前打开为10标题及可解析链接；新排错依据另由正文E-TNPR记录','M-038')]
for eid,raw,url,loc,pid in captured:
    before=copy.deepcopy(E[eid]);after=copy.deepcopy(before)
    capture=dict(checked_on='2026-10-27',access_mode='official_web_tool_extracted_text',capture_url=url,raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw),locator=loc,hands_on=False,visual_observed=False,archived_artifact=f'artifacts/{folders[pid]}/source-notes.md',sha256=None)
    after.setdefault('additional_captures',[]).append(capture);old_e.append(dict(evidence_id=eid,before=before,after=after))
    notes[pid].append(f'| {eid}追加 | [本次公开正文/目录]({url}) | {loc}；无产品操作 |')

notes['M-039'] += ['', '四页（愿望单、Goodreads、评论、Alexa）直接打开仅CSS Error，但官方搜索结果返回正文；只把返回正文记为搜索文本依据，不将CSS错误视为App故障。其余页面虽含前端错误字样，正文仍可读。本轮没有重试登录、权限或验证码路线。', '', '日期来自文章显示日期或同批官方目录，不等同软件构建。Kids Profile平台范围不套到Apple Watch、Sonos或Amazon儿童账号。Alexa命令表的播客注释只点名iOS，后文明确排除iOS/Android内置Alexa，保留该表述差别；Sonos限制也未逐连接方式分拆。', '', '本批补完既有Listening首屏中22项仅目录文章的正文依据，并补Lists的打开正文；macOS安装和Windows退役仍是范围说明，不虚构播放器任务。首屏之外的分页、账户/交易完整目录、每个控件值以及所有实际错误/恢复仍缺。']
notes['M-038'] += ['', '本地helper提供索引摘要线索后，root通过US目录解析文章链接并打开正文。未把“可能换得太快”写成已证内部竞态；未把动作建议写成恢复成功。下载等待估计及“马上播放”等宣传措辞不当测速。', '', '同ID的US旧内容排错作为E-TONI-14追加地区采集；60秒重启与其他快速重启步骤按问题场景分别保留，不拼成统一按键规则。游戏取盘后的后台下载不证明断网/断电续传，更不证明普通故事断点。']
notes['M-037'] += ['', 'root从既有分类JSON中按hash查到两篇完整body并复读，提取JSON另存work并记散列。新卡绑定、云列表创建和可离线媒体是不同状态；选择手机直接写卡只据文章声明，具体弹窗未知。', '', 'helper还提到v2.7.24播客断点条件；旧E-YOD-20及M037案例已明确记录，因此本批不当新发现、不新增相同来源或重复流程。普通故事A→B→A、换设备及关机秒点仍未知。']

# Build all planned text and assert new identifiers/URLs before mutating formal records.
assert len(new_e)==29 and len(new_f)==20
assert not (set(e['evidence_id'] for e in new_e)&set(E))
assert not (set(f['flow_id'] for f in new_f)&set(F))
def canonical(url):
    u=urlsplit(url);return u.netloc,unquote(u.path).rstrip('/')
assert not ({canonical(e['url']) for e in new_e}&{canonical(e['url']) for e in E.values()}), 'duplicate article requires append, not new E'
backups={}
def save_before(p):
    if p not in backups:
        backup=W/'before'/p.relative_to(R);backup.parent.mkdir(parents=True,exist_ok=True);backup.write_bytes(p.read_bytes());backups[p]=sha(p)
for p in cases.values():save_before(p)
for pid,lines in notes.items():
    p=L/f'artifacts/{folders[pid]}/source-notes.md';assert not p.exists();p.parent.mkdir(parents=True,exist_ok=True);p.write_text('\n'.join(lines)+'\n')
for e in new_e:e['sha256']=sha(L/e['archived_artifact'])
for c in old_e:
    capture=c['after']['additional_captures'][-1];capture['sha256']=sha(L/capture['archived_artifact'])

newprefix='family-audio-and-voice-controls'
csvwrite(L/f'batches/{newprefix}-coverage.csv',new_f)
(L/f'batches/{newprefix}-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in new_e))
for path in sorted((L/'batches').glob('*-coverage.csv')):
    rows=csvread(path);changed=False
    for i,row in enumerate(rows):
        match=next((x for x in old_f if x['flow_id']==row['flow_id']),None)
        if match:
            assert row==match['before']; rows[i]=match['after'];changed=True
            match.setdefault('owning_files',[]).append(str(path.relative_to(R)))
    if changed:save_before(path);csvwrite(path,rows)
for path in sorted((L/'batches').glob('*-evidence.jsonl')):
    rows=[json.loads(x) for x in path.read_text().splitlines()];changed=False
    for i,row in enumerate(rows):
        match=next((x for x in old_e if x['evidence_id']==row['evidence_id']),None)
        if match:
            assert row==match['before'];rows[i]=match['after'];changed=True
            match.setdefault('owning_files',[]).append(str(path.relative_to(R)))
    if changed:save_before(path);path.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in rows))
assert all(c.get('owning_files') for c in old_f+old_e)

# Preserve existing inventory IDs; update newly read rows rather than append duplicate snapshots.
ai=L/'batches/audible-controls-deepening-inventory.csv';save_before(ai);rows=csvread(ai)
by_number={int(r['inventory_id'].rsplit('-',1)[1]):r for r in rows}
imap={3:('16',31),4:('17',27),6:('18',None),7:('20',30),8:('23',29),9:('21',27),10:('19',28),13:('13',23),14:('09',22),16:('14',24),17:('12',26),19:('08',20),23:('22',33),26:('10',21),27:('11',19),29:('15',25),30:('01',15),31:('04',17),32:('02',15),33:('03',16),34:('07',18),35:('06',16)}
newemap={e['evidence_id']:e for e in new_e}
for n,(en,fn) in imap.items():
    row=by_number[n];eid='E-AUFC-'+en;e=newemap[eid]
    row.update(article_url=e['url'],coverage_status='body_read_scope_only' if n in [6,9] else ('official_search_body_partial' if en in ['13','14','15','23'] else 'body_read_partial_task'),evidence_ids='E-AUCD-01;'+eid,flow_ids=f'F-M039-{fn:02}' if fn else '',gap='本轮已读正文；实际UI/错误/恢复仍缺，详见case及F；安装/退役说明不当播放器完整链' if n in [6,9] else '已补本轮任务；版本、错误与恢复缺口见case/F，未实操')
by_number[12].update(coverage_status='body_read_partial_task',flow_ids='F-M039-10;F-M039-32',gap='打开正文补清单创建与管理；排序/公共更新/恢复未核')
row=copy.deepcopy(rows[0]);row.update(inventory_id='I-M039-054',domain='Profiles',item='Remove titles from Kids Profile',source_url='https://help.audible.com/s/topic/0TOar00000004TRGAY/kids-profile?language=en_US',locator='Kids Profile category / Remove titles',article_url=newemap['E-AUFC-05']['url'],coverage_status='body_read_partial_task',evidence_ids='E-AUFC-05',flow_ids='F-M039-16',gap='确认/延迟有文档，离线撤权与播放中变化未知');rows.append(row);csvwrite(ai,rows);inventory_updates.append(dict(path=str(ai.relative_to(R)),rows_before=53,rows_after=54,old_rows_updated=23,new_rows=1))
yi=L/'batches/yoto-controls-deepening-inventory.csv';save_before(yi);rows=csvread(yi)
for row in rows:
    if any(str(v) in ['I-YTCD-018','I-YTCD-019'] for v in row.values()):
        for key in list(row):
            if key in ['coverage_status','status']:row[key]='body_read_partial_task'
            elif key=='evidence_ids':row[key]+=';E-YTPD-01' if 'I-YTCD-018' in row.values() else ';E-YTPD-02'
            elif key=='flow_ids':row[key]='F-M037-20'
            elif key in ['gap','remaining_gap']:row[key]='本地完整正文已读；RSS更新、失败、在线流与断点仍需区分'
csvwrite(yi,rows);inventory_updates.append(dict(path=str(yi.relative_to(R)),rows_before=len(rows),rows_after=len(rows),old_rows_updated=2,new_rows=0))
ti=L/'batches/tonie-controls-deepening-inventory.csv';save_before(ti);rows=csvread(ti);tc=0
tm={'I-M038-112':('03','F-M038-11'),'I-M038-113':('01','F-M038-11'),'I-M038-114':('04','F-M038-12'),'I-M038-115':('02','F-M038-11')}
for row in rows:
    if row['inventory_id'] in tm:
        n,fid=tm[row['inventory_id']]
        row.update(coverage_status='body_read_partial_task',evidence_ids=row['evidence_ids']+';E-TNPR-'+n,flow_ids=fid,article_url=newemap['E-TNPR-'+n]['url'],gap='US正文已读；建议动作未实测，断点与持久恢复未证');tc+=1
    elif row['inventory_id']=='I-M038-116':
        row.update(coverage_status='body_read_partial_task',article_url=captured[2][2],gap='US同ID正文补等待/重启细节；同步原子性及普通故事断点未核');tc+=1
assert tc==5
csvwrite(ti,rows);inventory_updates.append(dict(path=str(ti.relative_to(R)),rows_before=len(rows),rows_after=len(rows),old_rows_updated=tc,new_rows=0))

for pid,p in cases.items():
    text=p.read_text()
    if pid=='M-039':
        text=text.replace('14条流程最高D2','33条流程最高D2')
        text=text.replace('共53行：','现为54行（原53行加儿童共享移除入口）：')
        text=text.replace('愿望单、PDF、分享、Maven正文未研究','本轮已补愿望单、PDF、共享、Goodreads和Maven；实际操作仍缺')
        text=text.replace('提醒/挑战/分享/PDF未闭合','本轮补提醒、挑战、分享/PDF文档路径；实际结果/恢复未闭合')
        text=text.replace('仅目录；Page Sync排除见F14','F15–19/25/26；Page Sync排除见F14')
        text=text.replace('儿童完整权限与成人账号不同，本批不补想象步骤','已补儿童建立/共享/删除与跨端限制；不等同Amazon儿童账号，仍无UI验证')
    text+='\n## 2026-10-27：家庭音频与执行状态补证（第33批）\n\n'
    text+=f'本轮来源定位见[正式摘记](../artifacts/{folders[pid]}/source-notes.md)。所有新材料保持D2文档依据；没有新增设备、App或网页实际操作。\n\n'
    if pid=='M-039':text+='既有Listening首屏中22项仅目录的文章已取得正文（其中4项来自官方搜索正文）；另补Lists的直接打开正文。macOS仅安装、Windows仅退役范围，仍不补成完整播放流程；Ways to listen后续分页及全部设置值仍未穷尽。\n\n'
    subset=[f for f in new_f if f['product_id']==pid]+[c['after'] for c in old_f if c['after']['product_id']==pid]
    text+='| F任务 | 步骤、状态与分支 | 仍未知 | E依据 |\n| --- | --- | --- | --- |\n'
    for f in subset:text+=f'| {f["flow_id"]} {f["task"]} | {f["known_branches"]} | {f["gaps"]} | {f["evidence_ids"]} |\n'
    if pid=='M-037':text+='\n导入网站/RSS、云列表保存、实体卡关联、设备离线缓存和可恢复位置不是同一个完成状态。旧播客断点日志已在原案例记录，本轮没有将它当新发现；两篇MYO正文相互重合。\n'
    if pid=='M-038':text+='\n这些排错步骤没有证明目标音频已恢复，更没有证明普通故事重启后续播。取消下载、取走实体入口和停止声音应分别比较；不从游戏检查点推普通故事秒点。US与旧en-150同ID内容排错均留存，实际构建未知。\n'
    if pid=='M-039':text+='\n对本项目的启发仍是建议：共享内容资格、某个人的播放位置、设备上的设置、云端档案和当前输出需要分别辨认。Maven只提供推荐/比较，App内Alexa又受前台范围限制，不能将二者合称系统AI已能操作所有数据与页面。提醒送达、挑战计数和实际理解收益也须分别验证。\n'
    p.write_text(text)

dump(W/'coverage-updates-applied.json',old_f);dump(W/'evidence-updates-applied.json',old_e);dump(W/'inventory-updates-applied.json',inventory_updates)
dump(W/'before-manifest.json',{str(p.relative_to(R)):h for p,h in backups.items()})
applied=dict(new_evidence=len(new_e),new_flows=len(new_f),new_D2=len(new_f),new_D4=0,old_flows=len(old_f),old_fields=sum(len(c['fields_changed']) for c in old_f),old_evidence_captures=len(old_e),new_images=0,inventory_updates=inventory_updates)
dump(W/'applied.json',applied)
print(json.dumps(applied,ensure_ascii=False))
