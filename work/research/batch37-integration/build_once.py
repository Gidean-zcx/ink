from pathlib import Path
import csv, json, hashlib, copy, re, shutil
from datetime import datetime
from specs import DATE, MUDITA, MUDITA_FLOWS, KHAN, KHAN_FLOWS

R=Path('/home/workspace/reader'); L=R/'docs/research/interaction-library'; W=R/'work/research/batch37-integration'
M=R/'work/research/batch37-mudita'; K=R/'work/research/batch37-khan'
assert not (W/'applied.json').exists(), 'one-time builder already applied'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p): return list(csv.DictReader(p.open()))
def dump(p,x): p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def wc(p,x):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(x[0])); w.writeheader(); w.writerows(x)
E={e['evidence_id']:e for e in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
F={f['flow_id']:f for f in rows(L/'coverage.csv')}
assert len(E)==1518 and len(F)==906
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
before={}; created=[]; oldF=[]; oldE=[]
def save(p):
    if str(p) not in before:
        q=W/'before'/p.relative_to(R); q.parent.mkdir(parents=True,exist_ok=True); q.write_bytes(p.read_bytes()); before[str(p)]=sha(p)
def edit(p,fn): save(p); p.write_text(fn(p.read_text()))
def add(p,t):
    assert not p.exists(),str(p)
    p.parent.mkdir(parents=True,exist_ok=True); p.write_text(t); created.append(str(p))
def cp(a,b):
    assert not b.exists(),str(b)
    b.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(a,b); created.append(str(b))
for name in ['coverage.csv','evidence.jsonl','catalog.csv','case-index.md']: save(L/name)

mn=L/'artifacts/mudita-remaining-controls/source-notes.md'
kn=L/'artifacts/khan-adult-guidance/source-notes.md'
ma=json.loads((M/'source-audit.json').read_text()); assert ma['summary']['item_count']==46
logs=[json.loads(x) for x in (M/'fetch-log.jsonl').read_text().splitlines()]
log_by_id={r['inventory_id']:r for r in logs}
assert len(ma['items'])==len(MUDITA)==46
mlines=['# Kompakt剩余帮助：原始日期、操作与边界','',f'查阅：{DATE}。46篇此前仅目录的公开帮助，普通匿名GET各一次，全部200；root逐篇核读正文。没有设备操作、登录、拨号、刷机或图片观察。以下是可复查的短摘记，完整临时原件在work，原件散列和请求状态见[source-manifest.json](source-manifest.json)。','',
'正文修改日集中于2026-10-03，贪睡拆页为2026-09-17；这不是OS/Center构建号。贪睡的web缓存曾显示2026-06-29，日期采用本轮保存的原HTML/TXT，不静默改写先前采集。首发措辞、未来计划和硬件声明按原适用范围保留。','',
'相同操作的综合页与拆分页归入同一F任务。目录页、文章数和任务数不可互换；下面的射频/SAR/耐用性/配送等FAQ没有被造成功能任务。','']
manifest=[]; newE=[]
for a in ma['items']:
    n=int(a['inventory_id'].split('-')[-1]); finding,gap,fs=MUDITA[n]; eid=f'E-MKRD-{n:02}'
    assert a['hashes_match'] and not a['existing_evidence_ids']
    assert sha(R/a['text_path'])==a['text_sha256'] and sha(R/a['html_path'])==a['html_sha256']
    old_matches=[e['evidence_id'] for e in E.values() if a['article_id'] in e['url']]
    assert not old_matches,old_matches
    dt=datetime.strptime(a['modified_on_raw'].removeprefix('Modified on: '),'%a, %d %b, %Y at %I:%M %p').date().isoformat()
    mlines += [f'## {eid} · {a["title"]}', '', f'[官方正文]({a["url"]})；目录{a["inventory_id"]}；修改：{a["modified_on_raw"].removeprefix("Modified on: ")}。', '',finding+' '+gap,'']
    manifest.append(dict(evidence_id=eid,**a,http_status=log_by_id[a['inventory_id']]['http_status'],checked_on=DATE))
    newE.append(dict(evidence_id=eid,product_id='M-012',title=a['title'],url=a['url'],checked_on=DATE,source_type='official_help',access_mode='anonymous_http_text_read',version_scope='Mudita Kompakt English help; OS/Center build unspecified; launch-only wording retained where present',app_build=None,firmware_version=None,source_updated_on=dt,locator=f'{a["inventory_id"]}; article body headings and ordered steps',note=finding+' '+gap,hands_on=False,visual_observed=False,archived_artifact=str(mn.relative_to(L)),sha256=None,supersedes=None,capture_state='durable_short_notes_and_source_manifest_originals_in_work',raw_work_path=a['text_path'],raw_sha256=a['text_sha256']))
add(mn,'\n'.join(mlines)); add(mn.parent/'source-manifest.json',json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')

# Small visual excerpts are for mechanism comparison, not current product operation.
for name in ['goals-1.png','tracker-1.png','certificate-1.png']: cp(K/'pdf'/name,kn.parent/name)
for name in ['dialog.png','assignments.png']: cp(K/'assignment-images'/name,kn.parent/name)
cp(K/'assignment-images/capture-log.json',kn.parent/'image-capture-log.json')
cp(K/'pdf/fetch-log.json',kn.parent/'pdf-fetch-log.json')
cp(K/'index-title-audit.json',kn.parent/'index-title-audit.json')
klines=['# Khan Kids：成人引导、纸面激励与教师任务','',f'查阅：{DATE}。本次补10篇公开帮助正文、3份配套PDF共7页，以及两张教师操作静态图。没有登录App、操作学生记录或测量学习效果；均不升级D3/D4。PDF全页已查看，归档各一代表页；5页目标模板只换角色插画，不计5种机制。','',
'文章读取采用既有成功的web工具公开路线，未重试此前403的HTTP文章路线。PDF普通匿名获取200；教师两张图片经公开浏览器200加载后截图。删除界面图在web工具超时，未重试或宣称已查看；没有把文字返回的图像引用当作图像观察。','',
'## 目录纠正与可追溯性','',
'按原捕获的栏目、链接号、标题逐项重核112个文章入口。KLC-I025实际是更换头像，KLC-I026实际是西语家庭学习建议，旧中文标签相互错配；原稳定ID、URL定位和F/E映射没有互换。将原文标题加入原130行目录（18个控件行不伪造文章标题）。KLC-I011更名为阅读发展文章系列，避免与App Learning Path混同。核对表见[index-title-audit.json](index-title-audit.json)。','']
for n,inv,slug,raw,locator,updated,finding,gap,fs in KHAN:
    eid=f'E-KHAT-{n:02}'; url='https://khankids.zendesk.com/hc/en-us/articles/'+slug
    aid=slug.split('-')[0]
    assert not [e for e in E.values() if re.search(r'/articles/'+aid+r'(?:-|$)',e['url'])]
    klines += [f'## {eid} · {slug.split("-",1)[1]}','',f'[官方文章]({url})；KLC-I{inv:03}；web {locator}；原文更新{updated}。','',finding+' '+gap,'']
    ev=dict(evidence_id=eid,product_id='M-030',title=slug.split('-',1)[1],url=url,checked_on=DATE,source_type='official_help',access_mode='web_tool_public_page_text',version_scope='Khan Academy Kids help; app build unknown; family, Class Account and external paper activities separated',app_build=None,firmware_version=None,source_updated_on=updated,locator=locator,note=finding+' '+gap,hands_on=False,visual_observed=n==9,archived_artifact=str(kn.relative_to(L)),sha256=None,supersedes=None,capture_state='durable_short_notes_with_temporary_web_tool_return',raw_work_path=str((K/raw).relative_to(R)),raw_sha256=sha(K/raw))
    if n==9:
        ev.update(visual_kind='static_official_images',artifact_hashes={str((kn.parent/s).relative_to(L)):sha(kn.parent/s) for s in ['dialog.png','assignments.png','image-capture-log.json']})
    newE.append(ev)
klines += ['## 教师静态图能证明什么','',
'[分配弹窗](dialog.png)可见课程名、Main、Preview、Save、右上X、全选/个人勾选与日期编辑入口。[管理页](assignments.png)可见Future尚未给学生、Current、日期分组、0/20及20/20、Delete Assignments入口。人数分数含义由文章解释，不能从20/20推断答题得分。单张图不能证明Save持久化、X丢弃改动或删除成功；两张图也不是一次连续操作。图片本身未标App构建。','',
'## 配套PDF：努力、活动次数与证书不是同一数据','']
pdfs=json.loads((K/'pdf/fetch-log.json').read_text())
pdfspec=[(11,'goals',4,'目标模板：三行目标、五个工作日','五页逐一查看：同一3×5结构配不同角色，询问当日是否为目标努力；不记录答案/正确率。','goals-1.png'),(12,'tracker',5,'活动记录：次数与周合计','一页：姓名、周一至周五及每列三格、周合计；记录活动次数，文章允许额外叠贴纸，三格不是上限。','tracker-1.png'),(13,'certificate',5,'纸面庆祝证书','一页：儿童姓名和角色庆祝图，没有可见得分/级别/验证字段；不证明App自动授予或能力掌握。','certificate-1.png')]
for n,name,parent,title,note,img in pdfspec:
    p=next(x for x in pdfs if x['name']==name); assert sha(R/p['path'])==p['sha256'] and p['status']==200
    eid=f'E-KHAT-{n:02}'
    klines += [f'### {eid} · {title}','',f'[原PDF]({p["url"]}) · [代表摘页]({img})。{note}', '',f'原PDF SHA-256：`{p["sha256"]}`。','']
    newE.append(dict(evidence_id=eid,product_id='M-030',title=title,url=p['url'],checked_on=DATE,source_type='official_printable_attachment',access_mode='anonymous_pdf_read_and_static_page_render',version_scope='Official companion printable; NOT built-in app UI; historical classroom challenge where applicable',app_build=None,firmware_version=None,source_updated_on=None,locator='all 5 pages inspected; representative page1 archived' if name=='goals' else 'single page inspected and archived',note=note,hands_on=False,visual_observed=True,visual_kind='static_printable_pages',archived_artifact=str(kn.relative_to(L)),sha256=None,supersedes=None,derived_from=f'E-KHAT-{parent:02}',capture_state='representative_static_page_and_short_notes_original_pdf_in_work',raw_work_path=p['path'],raw_sha256=p['sha256'],artifact_hashes={str((kn.parent/img).relative_to(L)):sha(kn.parent/img)}))
klines += ['## 取用限制','',
'孩子口头回答阅读问题、成人建议拼读练习顺序、教师分配作业、App内部自适应分别记录。没有依据将前三者描述成自动语音判分或AI课程编排。课堂挑战页明确结束，2023材料与2026修改日期分开；不宣传历史奖品仍可领取。上述机制只作为本家庭5–6岁儿童原型的研究候选。','']
add(kn,'\n'.join(klines))
for e in newE: e['sha256']=sha(L/e['archived_artifact'])
assert len(newE)==59 and not set(e['evidence_id'] for e in newE)&set(E)
assert len({e['url'] for e in newE})==59
assert not {e['url'] for e in newE}&{e['url'] for e in E.values()}

def change_flow(fid,known,gap,add_ids,title=None):
    b=F[fid]; a=copy.deepcopy(b)
    a['known_branches']+=' '+known
    a['gaps']+=' '+gap
    if fid=='F-M012-15':
        a['gaps']=a['gaps'].replace('具体等级名、总开关优先级、','总开关与逐App等级的生效优先级、')
    a['evidence_ids']=';'.join(dict.fromkeys(a['evidence_ids'].split(';')+add_ids)); a['checked_on']=DATE
    if title: a['task']=title
    oldF.append(dict(flow_id=fid,before=b,after=a,fields_changed=[k for k in a if b.get(k)!=a[k]]))
change_flow('F-M012-06','照片传入经Center Photos；USB传出照片在DCIM、截图在Pictures，须启用MTP，Mac可能需额外工具。','传输、相册登记和双向同步分开，未测。',[f'E-MKRD-{n:02}' for n in [50,51,87]],'从电脑准备书、音乐和照片并传出文件')
change_flow('F-M012-08','第三方消息应用FAQ仍为首发范围，无应用商店；是否可侧载另受兼容条件约束。','修改日期不证明最新固件或指定APK运行。',['E-MKRD-74'])
change_flow('F-M012-09','root相关FAQ表示厂商不提供MuditaOS K ROM；此陈述不等于已验证刷机或回刷。','没有安装其他系统或取得回滚原件。',['E-MKRD-79'])
change_flow('F-M012-12','拆分六页已读：Outlook/Google登录选联系人，或电脑先导出CSV/vCard再上传；选择部分或全部后Import selected contacts。','第三方网站步骤仅厂商转述，导入不等于持续同步；未核去重、坏行、覆盖和失败回执。',[f'E-MKRD-{n:02}' for n in [38,39,40,41,42,43]])
change_flow('F-M012-14','FAQ没有内置语音转文字承诺，录音保存与转写分开。','FAQ未标构建，未核侧载转写。',['E-MKRD-78'])
change_flow('F-M012-15','GPS帮助明确总开关与逐App许可均须打开，后者有Share/While using app/Don’t share；定位及App显示可分别滞后。','授权状态不代表已定位或App显示已刷新。',['E-MKRD-89'])
change_flow('F-M030-01','阅读理解文建议孩子暂停、口头回答再翻页；家庭共读及学前活动建议不等于自动判分。','未有ASR评分或连续读听恢复证据。',['E-KHAT-03','E-KHAT-06','E-KHAT-07'])
change_flow('F-M030-02','阅读发展文章系列不同于App Learning Path；Letters/Reading拼读顺序是成人挑选建议。','不据文章顺序推导系统解锁、适应阈值或能力测评。',['E-KHAT-01','E-KHAT-02','E-KHAT-07'])
change_flow('F-M030-07','学前活动文建议成人利用Create引导绘画和故事表达。','没有自动识别口述答案或给作品评分的依据。',['E-KHAT-07'])
change_flow('F-M030-13','教师总览确认课堂报告、分配任务与家庭账户的范围分别处理；安排/改派另见F16。','完成人数不是分数；报告导出仍不等于可回导备份。',['E-KHAT-08'])

b=E['E-KLCD-01']; a=copy.deepcopy(b)
a.setdefault('additional_captures',[]).append(dict(checked_on=DATE,access_mode='local_index_title_audit_and_related_article_read',note='112文章标题按原栏目链接核对；纠正KLC-I025/026中文标签，不换稳定ID或F/E；新增10篇正文和3份配套PDF另建证据。',archived_artifact=str(kn.relative_to(L)),sha256=sha(kn),artifact_hashes={str((kn.parent/'index-title-audit.json').relative_to(L)):sha(kn.parent/'index-title-audit.json')}))
oldE.append(dict(evidence_id='E-KLCD-01',before=b,after=a,fields_changed=['additional_captures']))

newF=[]
for product,spec,prefix,case in [('M-012',MUDITA_FLOWS,'E-MKRD','cases/M-012-mudita.md'),('M-030',KHAN_FLOWS,'E-KHAT','cases/M-030-khan-kids.md')]:
    for n,title,es,pre,known,gap in spec:
        newF.append(dict(flow_id=f'F-{product.replace("-","")}-{n:02}',product_id=product,task=title,depth='D2',evidence_ids=';'.join(f'{prefix}-{i:02}' for i in es),known_branches=known,gaps=gap,visual_status='static_official_images_only' if product=='M-030' else 'not_observed',hands_on='false',case_path=case,checked_on=DATE))
assert len(newF)==14 and not set(f['flow_id'] for f in newF)&set(F)
for kind,changes,key in [('coverage.csv',oldF,'flow_id'),('evidence.jsonl',oldE,'evidence_id')]:
    for p in sorted((L/'batches').glob('*-'+kind)):
        rs=rows(p) if kind.endswith('csv') else [json.loads(x) for x in p.read_text().splitlines()]; changed=False
        for i,r in enumerate(rs):
            c=next((x for x in changes if x[key]==r[key]),None)
            if c:
                assert r==c['before']; rs[i]=c['after']; changed=True; c.setdefault('owning_files',[]).append(str(p.relative_to(R)))
        if changed:
            save(p)
            if kind.endswith('csv'): wc(p,rs)
            else: p.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in rs))
assert all(c.get('owning_files') for c in oldF+oldE)
batch='adult-guidance-and-system-controls'
p=L/f'batches/{batch}-coverage.csv'; wc(p,newF); created.append(str(p))
add(L/f'batches/{batch}-evidence.jsonl',''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in newE))

# Update stable inventories, not a duplicate snapshot.
p=L/'batches/mudita-app-controls-deepening-inventory.csv'; save(p); rs=rows(p)
assert len(rs)==111
for r in rs:
    n=int(r['inventory_id'].split('-')[-1])
    if n in MUDITA:
        finding,gap,fs=MUDITA[n]; eid=f'E-MKRD-{n:02}'; e=next(e for e in newE if e['evidence_id']==eid)
        assert r['reading_status']=='index_only'
        r.update(reading_status='body_read',evidence_ids=';'.join(dict.fromkeys(r['evidence_ids'].split(';')+[eid])).strip(';'),flow_ids=';'.join(f'F-M012-{i:02}' for i in fs),gap=gap,read_basis='batch37_anonymous_http_root_full_text_read',covered_controls=finding,body_updated_on=e['source_updated_on'],checked_on=DATE)
wc(p,rs)
p=L/'batches/khan-library-controls-deepening-inventory.csv'; save(p); rs=rows(p)
assert len(rs)==130
audit={x['inventory_id']:x for x in json.loads((K/'index-title-audit.json').read_text())}
km={f'KLC-I{x[1]:03}':x for x in KHAN}
for r in rs:
    r['upstream_title']=audit.get(r['inventory_id'],{}).get('upstream_title','')
    if r['inventory_id']=='KLC-I025': r['function_domain']='更换头像'
    if r['inventory_id']=='KLC-I026': r['function_domain']='西语家庭学习建议'
    if r['inventory_id']=='KLC-I011': r['function_domain']='阅读发展文章系列'
    if r['inventory_id'] in km:
        n,inv,slug,raw,loc,dt,finding,gap,fs=km[r['inventory_id']]
        assert r['body_status']=='index_only'
        r.update(source_locator=r['source_locator']+'; https://khankids.zendesk.com/hc/en-us/articles/'+slug,evidence_ids=f'E-KLCD-01;E-KHAT-{n:02}',flow_ids=';'.join(f'F-M030-{i:02}' for i in fs),body_status='body_read_batch37',visual_status='static_official_images_only' if n==9 else 'not_observed',remaining_gap=gap,checked_on=DATE)
        if n==4: r['evidence_ids']+=';E-KHAT-11';r['visual_status']='static_printable_pages_only'
        if n==5: r['evidence_ids']+=';E-KHAT-12;E-KHAT-13';r['visual_status']='static_printable_pages_only'
wc(p,rs)

def table_for(spec,product,prefix):
    out=['| 任务 | 前提/入口 | 操作与文档预期结果 | 未闭合分支 | 依据 |','| --- | --- | --- | --- | --- |']
    for n,title,es,pre,known,gap in spec:
        out.append(f'| F-{product.replace("-","")}-{n:02} {title} | {pre} | {known} | {gap} | '+ '、'.join(f'{prefix}-{i:02}' for i in es)+' |')
    return '\n'.join(out)
def mudita_case(t):
    t=t.replace('16 条任务最高 D2；','27 条任务最高 D2；')
    t=t.replace('核心App续补来自18篇帮助正文，没有新增图像观察。','此前核心App补18篇帮助；本次再读剩余46篇，未新增图像观察。')
    a=t.index('[逐条功能目录及当前阅读状态]'); b=t.index('\n\n| 功能域',a)
    t=t[:a]+'[逐条功能目录及当前阅读状态](../batches/mudita-app-controls-deepening-inventory.csv)保留111个稳定条目；原46项index_only已逐篇读正文，现80项body_read、1项prior_body_read、30项text_read。没有仅据标题断言的剩余行，但这只是已有来源目录，尚未核全机功能，也未刷新全部官网目录。综合/拆分帮助的重复路径合并在同一任务；硬件/市场FAQ不计操作任务。原[目录快照](../artifacts/mudita-controls-deepening/function-inventory.csv)作为历史保留。'+t[b:]
    t=t.replace('截图、亮度刷新与按App优化未闭合','截图文字见F25，亮度刷新与按App优化未闭合')
    t=t.replace('Notes新建/删除见F13；其余仍是目录/发布变化；备份范围F10','Notes见F13；本次F20–23补日历/闹钟/地图/天气，备份范围F10')
    t=t.replace('其余App完整操作、地图离线暂停后的恢复仍未核','各App错误/返回、地图暂停下载后的恢复仍未核')
    t=t.replace('拆分导入页未单独读取；去重、覆盖、字段错误、导入与持续同步区别','拆分六页已读；去重、覆盖、字段错误与持续同步仍未验证')
    t=t.replace('Wi-Fi/热点/漫游正文未核','Wi-Fi/热点/漫游正文见F26；断连恢复仍未核')
    t=t.replace('定位等级/优先级、其他权限','定位等级生效优先级、其他权限')
    t=t.replace('未读FAQ不据标题断言','全部原列FAQ正文已读，仍不据厂商安全/性能声明推定本项目实测')
    t=t.replace('| F-M012-06 从电脑准备书和音乐 |','| F-M012-06 从电脑准备书、音乐和照片并传出文件 |')
    t += '\n## 第37批补证：现有任务的数据与作用范围\n\n'
    for c in oldF:
        if c['after']['product_id']=='M-012':
            f=c['after']; extra=f['known_branches'][len(c['before']['known_branches']):].strip()
            t+=f'- {f["flow_id"]}：{extra} {f["gaps"][len(c["before"]["gaps"]):].strip()} 依据：'+ '、'.join(x for x in f['evidence_ids'].split(';') if x not in c['before']['evidence_ids'].split(';'))+'。\n'
    t+='\n## 本次新增文档任务\n\n'+table_for(MUDITA_FLOWS,'M-012','E-MKRD')+'\n\n'
    t+='详见[46篇逐源摘记与原件日期](../artifacts/mudita-remaining-controls/source-notes.md)。SIM/eSIM先安装还是先切模式的疑点、Alarm隐藏图标入口差异均保留，不能自行补成统一成功路径。截图无即时反馈、地图下载完成后离线可用、GPS许可/定位/天气更新是不同状态，值得作为G-002执行结果的比较问题；这些仍是文档依据，并非本项目已定方案。\n\n'
    t+='FAQ中的无闪烁措辞及标准引用没有给出波形、残影或帧率测量，不能用于兑现R-011的60fps目标。射频、续航、SAR、耐用性均属Kompakt厂商声明，不是Plus硬件验证或儿童使用效果结论。\n'
    return t
edit(L/'cases/M-012-mudita.md',mudita_case)
def khan_case(t):
    t=t.replace('前轮观察官方动画抽样帧，本轮另看两张官方静态图，没有操作App或听到声音。','早前观察官方动画抽样帧及两张静态图；第37批另看三份PDF全部七页及两张教师界面图，没有操作App或听到声音。')
    t=t.replace('栏目由E-KLCD-01支持。','栏目由E-KLCD-01支持。第37批另核112个上游标题，纠正I025/I026中文标签错配，稳定ID及原F/E映射保持；10项原index_only补正文，仍有77项只到目录，具体看该CSV。')
    section='\n## 成人引导、纸面激励与课堂任务\n\n'
    section+='第37批取得的目标表和课堂挑战属于成人组织的配套活动，不能加入App自动奖励的统计。阅读发展文章系列也不等于Learning Path。\n\n'
    section+=table_for(KHAN_FLOWS,'M-030','E-KHAT')+'\n\n'
    section+='[来源摘记、原始标题审计与代表画面](../artifacts/khan-adult-guidance/source-notes.md)区分三种记录：目标表记录努力，贴纸表记录活动次数，教师Assignments泡泡记录完成人数；都不能直接推断独立掌握。官方教师截图可见Save和X，但未操作，关闭是否保存未知。已结束课堂活动不当现行比赛，2026文章更新日也不是截图的App构建。\n\n'
    for c in oldF:
        if c['after']['product_id']=='M-030':
            f=c['after'];extra=f['known_branches'][len(c['before']['known_branches']):].strip()
            section+=f'- {f["flow_id"]}补证：{extra} 依据：'+ '、'.join(x for x in f['evidence_ids'].split(';') if x not in c['before']['evidence_ids'].split(';'))+'。\n'
    section+='\n口头回答不是自动语音评分；Main/Basic/Practice的安排建议不是强制解锁规则。纸面坚持与App答题反馈的组合值得研究，但学习收益和本家庭5–6岁儿童适用性仍需观察。\n'
    assert '\n## 后续必须补的观察' in t
    return t.replace('\n## 后续必须补的观察',section+'\n## 后续必须补的观察')
edit(L/'cases/M-030-khan-kids.md',khan_case)

brief=R/'docs/briefs/037-adult-guidance-and-system-controls.md'
add(brief,'''# 第三十七期：谁在引导，动作完成后留下什么

2026-10-27。现有研究已超出宣传页，但尚未深入每个产品的每个功能。全部65组仍为部分覆盖，没有手机或电子纸实机验证；本期也没有新增产品实际操作。

## 儿童激励：先分清谁记录、记录什么

Khan Kids的目标表由成人与孩子填写，记录每天是否为目标努力；历史课堂挑战的贴纸记录完成活动次数，最后以纸面证书庆祝。它们不是App自动发放的成就，也不能证明孩子已经掌握。教师作业的完成泡泡则记录完成人数。三者的记录对象不同，详见[Khan案例](../research/interaction-library/cases/M-030-khan-kids.md)及[原件、画面与边界](../research/interaction-library/artifacts/khan-adult-guidance/source-notes.md)。

阅读理解文章建议孩子暂停并口头回答，不足以推断自动语音判分；拼读文章建议的练习顺序也不是已验证的系统解锁规则。对本家庭的启发，是分别研究成人引导、自主选择、系统反馈与真实掌握，不把“坚持”直接当成学习效果。

## 日常系统能力：有按钮还要知道结果

Kompakt帮助写明截图没有屏幕提示，需去相册查找；地图要先联网下载，之后才能离线搜索和查看；闹钟贪睡需手动触发且固定五分钟。GPS总开关、逐App许可、实际定位与天气显示刷新也并非一个状态。见[Kompakt案例](../research/interaction-library/cases/M-012-mudita.md)及[逐源摘记](../research/interaction-library/artifacts/mudita-remaining-controls/source-notes.md)。

本次保留了两处文档疑点：eSIM安装与模式切换先后，以及隐藏闹钟图标后的入口。公开帮助没有给出的失败、撤销、重启及同步结果，继续标未知。无闪烁宣传没有提供60fps、波形或残影测量，不能满足本项目高刷目标的验证。

## 资料怎样留下来供设计复用

按产品保留稳定任务编号；按设计问题比较机制；每个事实再连到原文、日期、图像和限制。入口是[设计参考指南](../research/interaction-library/design-reference-guide.md)，研究不足看[深度审计](../research/interaction-library/analysis-status.md)。新增的是可引用的依据，具体机制尚未转为本项目需求。

本期补读Kompakt原目录剩余46篇、Khan Kids 10篇文章与3份PDF；查看两张教师静态图。新增14条D2任务，10条旧任务补分支；59条新证据及1条旧目录证据更新。112个上游标题审计修正两条中文标签，原ID不变。正文已读、已有图片与完整交互已验证分别标记，不用资料数量代表研究完成率。
''')

# Make the design entry point useful without reading chronological reports.
new_rows='''| 儿童奖励到底记录努力、活动次数还是掌握 | [Khan F14/15](cases/M-030-khan-kids.md) | [目标表/贴纸/证书及原PDF](artifacts/khan-adult-guidance/source-notes.md) | 官方配套纸面活动、成人组织；不是App自动成就，没有学习效果验证 |
| AI安排的学习任务何时对孩子可见，修改影响谁 | [Khan F16](cases/M-030-khan-kids.md) | [分配弹窗](artifacts/khan-adult-guidance/dialog.png)、[日期分组](artifacts/khan-adult-guidance/assignments.png)与[文档边界](artifacts/khan-adult-guidance/source-notes.md) | 学生范围、日期、Save、归档与删除分别核；静态X按钮不证明取消语义，家庭版不同 |
| 没有屏幕反馈时，AI怎样确认截图、下载和定位结果 | [Kompakt F22/23/25](cases/M-012-mudita.md) | [帮助原文与日期](artifacts/mudita-remaining-controls/source-notes.md) | 截图文件、地图包、授权、GPS位置和天气显示分别核；未实测错误、恢复或时延 |
'''
def guide(t):
    marker='| --- | --- | --- | --- |\n'; assert marker in t
    t=t.replace(marker,marker+new_rows,1)
    t+='''
## 正式设计时，一条依据怎样才够用

比较机制可用有版本范围的D2文档；决定布局需相应画面；决定手势、时延、错误恢复与持久化，则要补对应实际操作。证据深度按具体结论判断，不因同产品某处有D4就给全部功能背书。

每个拟采用的交互，留下“待解决问题 → 型号/版本 → F任务 → E原始依据 → 适用条件/缺口 → 本项目选择及理由”。最后一项等正式设计获认可后填写；现在不把助手建议写成既定需求。一个功能可以有多条来源，同一篇帮助也可以支持多个任务；新增文章或截图不重复计功能。

尚未补齐的原型关键问题要有明确观察任务。例如研究“继续上次故事”，应指定同一内容、档案与播放位置，分别核退出、断网、重启后的结果；没有这种证据时，只能引用已说明的恢复范围。资料不足时记录已查路线、失败原因和下一步，保留未知，避免以后重复从宣传页重新猜测。
'''
    return t
edit(L/'design-reference-guide.md',guide)
def status(t):
    a=t.index('## 当前深度，一眼看清');b=t.index('\n## 范围基线',a)
    return t[:a]+'''## 当前深度，一眼看清

当前登记产品均为部分覆盖，尚无一组可称全部功能分析完成。实时数量统一取[生成的案例索引](case-index.md)，避免多份手工“当前统计”随批次更新而互相矛盾。下表解释证据的用途；后面的批次数字保留为历史快照。

| 证据层次 | 后续设计可怎样使用 |
| --- | --- |
| D1：发现功能，核心过程仍缺 | 只作候选和待查线索 |
| D2：依据文档等重建任务 | 比较有来源的步骤、条件和状态；不能声称已经操作产品 |
| D3：连续视觉过程 | 须连续观察同一任务；离散截图、剪辑或零散视频帧不足 |
| D4：有日志的实际操作 | 仅适用于记录的环境和动作；当前有界样本为公开网页，不能外推原生App或实体设备 |

尚无本项目手机/电子纸实机测试，未验证儿童实际使用效果。最明显的缺口仍是同型号/版本的完整主流程、错误与中断恢复、持久保存和实际操作体验。原目录正文读完也不能关闭这些缺口。

从[按设计问题查依据](design-reference-guide.md)沿“问题 → 产品/版本 → F任务 → E证据 → 画面或日志 → 限制”取用。逐产品查漏看[功能目录](inventories/README.md)和[待核清单](backlog.md)；不需要按时间顺序读完全部简报。
''' +t[b:]
edit(L/'analysis-status.md',status)
edit(L/'analysis-status.md',lambda t:t+'\n第37批补Kompakt原46目录正文；Khan10篇、3PDF及2静态图；14D2、10旧F补证，修正两条目录标签并保留原ID。仍无手机/电子纸实测；纸面激励不当内置成就，来源读完不等于功能完成。当前统计只保留在生成的案例索引，避免旧手工汇总误导。\n')
edit(L/'backlog.md',lambda t:t.replace('## 本轮深化后仍需补的具体问题','''## 本轮深化后仍需补的具体问题

- Kompakt：原目录46个仅标题项已补正文，下一步应补同构建的截图回执、闹钟响铃/停止/重启、地图下载失败与离线结果；核eSIM步骤和隐藏图标入口疑点，不再重复抓同一批文章。
- Khan Kids：两种纸面活动、教师分配/归档已有依据；下一步是同构建同题的错误→提示→正确→结算→重进、家长版设置和实际学习效果。原目录仍77个index_only，按上游标题定位；纸面PDF不补齐App主学习链。
''',1))
edit(L/'comparison.md',lambda t:t+'''\n## 成人引导与系统完成结果（第37批）

| 问题 | 可复查比较 | 当前限制 |
| --- | --- | --- |
| 学习记录代表什么 | [Khan F14–16](cases/M-030-khan-kids.md)：努力格、活动贴纸、教师完成人数与应用答题反馈分开 | 成人组织与App自动反馈不可混用；不据计数推定掌握 |
| AI何时能报告动作成功 | [Kompakt F22/23/25](cases/M-012-mudita.md)：下载包、定位许可/位置/天气、截图文件 | 入口或开关不等于最终结果；文档重建，实际回执与恢复仍缺 |

这些比较服务原型讨论，不指定本项目采用纸面奖励、教师账户或任何系统协议。
''')
link='- [第三十七期：成人引导与系统结果](/home/workspace/reader/docs/briefs/037-adult-guidance-and-system-controls.md)：区分纸面激励、教师任务和App反馈；补Kompakt闹钟/地图/截图等操作与结果边界。\n'
for p in [R/'README.md',L/'README.md']:
    edit(p,lambda t:t.replace('- [第三十六期',link+'- [第三十六期',1))
edit(L/'batches/README.md',lambda t:t+'\n第37批 `adult-guidance-and-system-controls` 新增59E/14D2，10旧F及1旧E就地补证。Mudita111行更新46原index_only，Khan130行更新10原index_only并补112上游标题；未复制目录或升级产品实测。见[简报37](../../../briefs/037-adult-guidance-and-system-controls.md)。\n')
edit(R/'docs/ideas.md',lambda t:t+'\n2026-10-27继续I-021/I-022：Khan补成人引导、纸面目标/贴纸和教师分配的不同记录；Kompakt补剩余帮助中的操作、反馈及冲突。修正两条中文目录标签，并将当前统计收敛到生成索引。见[第三十七期简报](/home/workspace/reader/docs/briefs/037-adult-guidance-and-system-controls.md)。资料按产品与设计问题双向取用；完整交互、恢复与儿童效果仍有缺口，没有新增既定功能或进入系统设计。\n')
dump(W/'changes.json',dict(new_evidence_ids=[e['evidence_id'] for e in newE],new_flow_ids=[f['flow_id'] for f in newF],updated_flows=oldF,updated_evidence=oldE))
dump(W/'before-manifest.json',before); dump(W/'applied.json',dict(checked_on=DATE,created=created,modified=list(before)))
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
print(json.dumps(dict(new_flows=len(newF),new_evidence=len(newE),updated_flows=len(oldF),updated_evidence=len(oldE),created_files=len(created),modified_files=len(before)),ensure_ascii=False))
