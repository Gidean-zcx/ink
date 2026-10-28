from pathlib import Path
from collections import Counter
from html import unescape
import csv,json,re,hashlib,shutil

ROOT=Path('/home/workspace/reader');BASE=ROOT/'docs/research/interaction-library';WORK=ROOT/'work/boox-controls-deepening';ART=BASE/'artifacts/boox-controls-deepening';ART.mkdir(exist_ok=True)
CASE=BASE/'cases/M-004-boox-neoreader.md';DATE='2026-10-26'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
raw=WORK/'epaper-pc-articles.json';data=json.loads(raw.read_text());articles={a['id']:a for a in data['articles']}
sections={s['id']:s for s in json.loads((WORK/'sections.json').read_text())['sections']}
manual=json.loads((WORK/'manual-access.json').read_text())['article']
selected=[
 (1,None,'公开用户指南目录','14节59篇；含其他机型和新版本项目，不能当本机固件完整目录。','Guide category and section/article metadata'),
 (2,manual['id'],'机型手册的取得边界','官方仍要求本机下载，或登录对应send2boox服务器选型号；本轮未登录，未得到专用PDF。','Method 1/Method 2'),
 (3,25213043150996,'系统面板、导航栏与手势','控制/通知中心分侧；底部及侧边手势配置；导航球快捷项缺失时可编辑补入。通用图非本机。','Control Center; Notification Center; Navigation Bar; Bottom/Side Gestures; Navigation Ball'),
 (4,10701257505044,'EinkWise三种入口与机型限制','本类帮助默认右下上滑；另一E Ink Tablet类同名页写左下。按配置核对，不合成统一默认手势。','Access EinkWise; model-dependent notes'),
 (5,10701257029780,'应用刷新模式与一次全刷','当前通用页列HD/Speed/Smooth，按App可独立设置；性能星级不是实测，Smooth本机版本条件沿用E-BOOX-01。','Refresh Modes Explained; Manual Refresh'),
 (6,10701299044372,'第三方App优化与撤回配置','DPI原值/单App Reset All/Master Switch是不同恢复动作；后台、方向等选项有条件和副作用，未核对4.1.2构建。','Display; Color; Others; Handwriting'),
 (7,10701262170644,'应用管理、冻结与卸载入口','Apps & Notifications下分权限/通知/启动/冻结；Application只说明管理卸载，不说明卸载数据保留或恢复。','Application; Permission; Notifications; Default app; App Startup; Freeze Settings; Reset all app optimization'),
 (8,10701308914964,'安装渠道的资料边界','官方列Google Play和APK两渠道；安装确认/拒绝/签名冲突/数据迁移没有完整步骤，仅D1。没有下载APK。','Download from Google Play Store; Download APK files from web'),
 (9,10701452686484,'系统分屏换边与退出','控制中心Split Screen选择方向、换边、退出；返Home也是退出途径；Flexible原文型号名单未列本机，不作否定支持结论。','Split Screen for Third-Party Apps; Swap Sides; Exit Split Screen'),
 (10,10701493183764,'阅读器分屏的数据与返回','Current Doc两窗标记在刷新后同步；关联笔记与系统独立Notes区分，退出通过控制中心。','Current Doc; Handwritten/Text Notes; Swap Sides & Exit Split View'),
 (11,10701495414804,'阅读位置与PDF新增页','Progress历史往返不等于永久书签；Blank Page嵌入PDF，可在Preview或书签旁入口删除；未看删除反馈。','Progress; Prev View & Next View; Blank Page'),
 (12,10701492644372,'文档刷新设置与作用范围','正文和已看图列Regal/Speed、Normal/Deep、频率、文本平滑及Apply to All Documents；不据通用图证明本机全部模式。','Refresh Mode; Full Refresh; Full Refresh Rate; Text Smoothing; Apply to All Documents'),
 (13,10701578837268,'笔记保存、退出与页删除','Exit保存后回Notepad View；More中的Save为手动保存，Page-不可撤销；不将新章节能力倒填V4.1.2。','Exit; More > Page-/Save/Share and Export'),
 (14,10701171762452,'睡眠连接与后台排查条件','通用Tab系列帮助列自动睡眠/关机与睡眠断开连接设置；不保证Note Air5 C V4.1.2路径或TTS锁屏续播。','Sleep Mode; Advanced Power Settings; Battery Management'),
]
images={3:'control-center.jpg',6:'app-optimization-display.jpg',7:'app-management.jpg',12:'neoreader-refresh.png'}
for name in images.values():shutil.copyfile(WORK/name,ART/name)

oldmap={}
for line in (BASE/'evidence.jsonl').read_text().splitlines():
 r=json.loads(line)
 if r['product_id']=='M-004':
  m=re.search(r'/articles/(\d+)',r['url'])
  if m:oldmap.setdefault(int(m[1]),[]).append(r['evidence_id'])
with (BASE/'coverage.csv').open() as f: oldflows={r['flow_id']:r for r in csv.DictReader(f) if r['product_id']=='M-004'}
focus={
25213043150996:('F-M004-02','路径补强；设备构建仍未知'),10701257505044:('F-M004-03','对比另一分类的默认手势，不混用'),10701257029780:('F-M004-03','通用模式与本机条件分开'),10701299044372:('F-M004-03;F-M004-14','补优化撤回与后台条件'),10701262170644:('F-M004-03;F-M004-14','权限/通知/冻结/重置范围；卸载数据未知'),10701308914964:('','D1渠道声明；安装确认链未取得'),10701452686484:('F-M004-08','关闭分屏入口；Flexible型号未核'),10701493183764:('F-M004-08','Current Doc刷新后同步限该模式'),10701495414804:('F-M004-06;F-M004-07','PDF新增页与视图历史的状态不同'),10701492644372:('F-M004-03','文档级范围；通用静态图'),10701578837268:('F-M004-09;F-M004-10','只补Exit/Save；其余新功能不倒填'),10701171762452:('F-M004-14','仅后台问题相关条件，具体型号路径未知')}
existing_by_title={
'ONYX Account Sign-up and Deletion':'F-M004-01','Firmware Updates':'F-M004-14','Transfer with Your Computer':'F-M004-04','Translation and Dictionary':'F-M004-05','Take Notes on Books':'F-M004-06','TOC & Bookmarks':'F-M004-07','Reading Data Syncing and Backup':'F-M004-11;F-M004-12','Reading Data Syncing':'F-M004-11','Note Syncing':'F-M004-13','Integrated Third-Party Cloud Storage':'','NeoReader Split Screen':'F-M004-08'}
eid_by_article={a:f'E-BXCD-{n:02}' for n,a,*_ in selected if a is not None}
inv=[]
ordered=sorted(articles.values(),key=lambda a:(sections[a['section_id']]['position'],a['position'],a['id']))
for n,a in enumerate(ordered,1):
 aid=a['id']; f,gap=focus.get(aid,(existing_by_title.get(a['title'],''),'仅登记目录；该功能入口、状态和异常未重建'))
 state='partially_documented' if aid in focus and f else ('existing_partial' if f else 'inventory_only')
 scope='通用ePaper Tablet PCs帮助；Note Air5 C V4.1.2逐项适用未核'
 if a['title']=='Scan Documents App':state='other_model_excluded';gap='正文仅列Tab Ultra/Tab Ultra C/Tab Ultra C Pro相机；不并入本机'
 if a['title']=='InkSpire':state='model_scope_unconfirmed';gap='目录有条目不足以证明本机支持该笔'
 if a['title']=='Talk Insights':state='outside_fixed_baseline';gap='不把后续Lab能力倒填V4.1.2'
 if a['title']=='Work Profile':state='model_scope_unconfirmed';gap='管理帮助仅说部分机型；本机开关及数据删除边界未核'
 if a['title']=='Download and Install Apps':state='D1_only';gap='渠道已查，逐步安装/卸载/数据保留缺口未闭合'
 ev=oldmap.get(aid,[])+([eid_by_article[aid]] if aid in eid_by_article else [])
 inv.append(dict(order=n,section=sections[a['section_id']]['name'],article_id=aid,title=a['title'],url=a['html_url'],source_updated_on=a['updated_at'],edited_at=a.get('edited_at'),model_version_scope=scope,coverage=state,flow_ids=f,evidence_ids=';'.join(ev) or 'E-BXCD-01',gap=gap))
ip=ART/'functional-inventory.csv'
with ip.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(inv[0]));w.writeheader();w.writerows(inv)

notes=['# BOOX 控制补强：证据短记','',f'查阅 {DATE}。固定 Note Air5 C / Android 15 / V4.1.2 基线；以下是公开通用帮助，未取得该构建专用手册。原件仅 work，本文为研究摘要，不是原文副本。全部 hands_on=false；图像为共用帮助图，型号/固件不明，不能当 Note Air5 C 实拍。','']
for num,aid,title,note,loc in selected:
 a=manual if aid==manual['id'] else articles.get(aid)
 url='https://booxsupport.zendesk.com/hc/en-us/categories/38115205868180-User-Guide-ePaper-Tablet-PCs' if a is None else a['html_url']
 notes.extend([f'## E-BXCD-{num:02} {title}',f'来源：{url}；章节：{loc}。',note,''])
 if num in images:notes.extend([f'已实际看过的静态参考：[图]({images[num]})。不是连续动作、非本机固件验证。',''])
notes.extend(['## 获取记录','分类、章节及59篇正文通过官方公开Zendesk API正常HTTP200获取；next_page为空。web打开System Split Screen返回Internal Error，已有此前成功获取的分类API正文，不重试该页面。未遇401/403/429，未登录或使用代理。','官方手册帮助限定机内或登录网站下载；未从非官方聚合“manual”页面拼手册。搜索发现的FCC单页/商品摘要不作为操作依据。',''])
np=ART/'source-notes.md';np.write_text('\n'.join(notes))
evs=[]
for num,aid,title,note,loc in selected:
 a=manual if aid==manual['id'] else articles.get(aid)
 rawp=WORK/'manual-access.json' if num==2 else raw
 archive=ip if num==1 else ART/images[num] if num in images else np
 evs.append(dict(evidence_id=f'E-BXCD-{num:02}',product_id='M-004',title=title,url='https://booxsupport.zendesk.com/hc/en-us/categories/38115205868180-User-Guide-ePaper-Tablet-PCs' if a is None else a['html_url'],checked_on=DATE,
  source_type='official_help_index' if num==1 else 'official_help',access_mode='official_public_api_body_and_static_image' if num in images else 'official_public_api_body_read',
  version_scope='Note Air5 C / Android 15 / V4.1.2 feature baseline; public ePaper Tablet PCs guide not a build-specific manual; generic images are not model verification',app_build=None,firmware_version='V4.1.2 baseline; current help not pinned to this build',source_updated_on=None if a is None else a['updated_at'][:10],locator=loc,note=note,hands_on=False,visual_observed=num in images,
  archived_artifact=str(archive.relative_to(BASE)),sha256=sha(archive),capture_state='generic_official_static_reference' if num in images else 'derived_index_or_research_notes',supersedes=None,
  raw_work_path=str(rawp.relative_to(ROOT)),raw_sha256=sha(rawp)))
ep=BASE/'batches/boox-controls-deepening-evidence.jsonl';ep.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evs))
fields=list(next(iter(oldflows.values())))
with (BASE/'batches/boox-controls-deepening-coverage.csv').open('w',newline='') as f:csv.DictWriter(f,fieldnames=fields).writeheader()

changes={
 2:dict(evidence_ids='E-BOOX-05;E-BXCD-03',known_branches='控制/通知中心分侧；快捷项可Edit补入；侧边内滑需启用；导航栏隐藏先上滑调出；双击导航球回桌面不等于保存或终止进程',gaps='未取得V4.1.2机型手册；面板关闭动作及选区/阅读页保留、手势与app冲突未观察',visual_status='static_reference_only'),
 3:dict(evidence_ids='E-BOOX-06;E-BOOX-07;E-BOOX-08;E-BXCD-04;E-BXCD-05;E-BXCD-06;E-BXCD-07;E-BXCD-12',known_branches='单次全刷/App显示配置/当前文档/全部文档不同；原DPI、关闭Master Switch、单App Reset All及全App重置不同；NeoReader文档帮助Regal/Speed不与通用HD/Speed/Smooth混表',gaps='本机4.1.2完整模式/默认参数/生效及退出反馈，重置关联、卸载后优化数据保留、真实残影/耗电未知',visual_status='static_reference_only'),
 6:dict(evidence_ids='E-BOOX-12;E-BXCD-11',known_branches='文本批注与手写分开管理；Progress的Blank Page会嵌入PDF，可在Preview或书签旁入口删除新增页',gaps='PDF新增页删除确认/撤销/锚点变化、跨软件对象可编辑性、导出失败和存储不足未观察'),
 7:dict(evidence_ids='E-BOOX-13;E-BOOX-14;E-BXCD-11',known_branches='Progress内Prev View/Next View逐次往返；书签是独立持久标记，不承诺视图历史跨关书或重启',gaps='关书/系统回收后历史、EPUB重排定位与重复书签未知'),
 8:dict(evidence_ids='E-BOOX-15;E-BXCD-09;E-BXCD-10',known_branches='Current Doc两窗标记在刷新后同步；关联笔记与系统独立Notes不同；控制中心换边/退出，系统分屏也可回Home退出；Flexible帮助型号名单未列本机',gaps='关联Notes的写入时刻不能由Current Doc推断；源文件移动/删除、退出后页锚、Flexible本机适用未知'),
 9:dict(evidence_ids='E-BOOX-16;E-BXCD-13',known_branches='Exit保存并返回Notepad View；More内Save手动保存，两入口不合并；Page-帮助称不能撤销',gaps='Home/切App/系统回收是否持久保存、断电写入、.note往返、删整本恢复未知'),
 14:dict(evidence_ids='E-BOOX-14;E-BOOX-21;E-BOOX-22;E-BOOX-23;E-BXCD-06;E-BXCD-07;E-BXCD-08;E-BXCD-14',known_branches='TTS引擎、通知权限、App Startup、Stay active和Freeze是不同控制；冻结阻后台/通知；睡眠连接设置另列；安装/卸载只取得入口D1',gaps='各后台开关优先级、TTS锁屏/蓝牙/焦点中断续播、安装失败/卸载后文件账户和偏好保留/恢复未知',visual_status='static_reference_only')
}
updates=[]
for n,fields_to_set in changes.items():
 before=oldflows[f'F-M004-{n:02}'];after=before|fields_to_set
 updates.append(dict(flow_id=after['flow_id'],changes={k:dict(before=before[k],after=v) for k,v in after.items() if before[k]!=v},replacement_row=after))
(WORK/'old-coverage-updates.json').write_text(json.dumps(updates,ensure_ascii=False,indent=2)+'\n')
(WORK/'manifest.json').write_text(json.dumps(dict(checked_on=DATE,articles=59,sections=14,new_evidence=14,new_flows=0,old_flow_updates=len(updates),hands_on=False,raw_files=[dict(path=str(p.relative_to(ROOT)),sha256=sha(p)) for p in [raw,WORK/'categories.json',WORK/'sections.json',WORK/'manual-access.json',ROOT/'work/eink-systems-research/boox-articles.json']],formal_files=[dict(path=str(p.relative_to(BASE)),sha256=sha(p)) for p in [ip,np]+[ART/v for v in images.values()]],static_images=json.loads((WORK/'image-fetch.json').read_text())),ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'new_evidence':len(evs),'new_flows':0,'old_updates':len(updates),'directory_articles':len(inv),'sections':len(set(a['section_id'] for a in articles.values())),'static_images':len(images)}))
