from pathlib import Path
import csv,json,hashlib,shutil,collections
W=Path('work/palma-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches';A=L/'artifacts/palma-controls-deepening';A.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
dump=lambda p,x:Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
case=L/'cases/M-010-palma.md';assert sha(case)==json.loads((W/'baseline.json').read_text())['case_sha256']
allE=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()];ed={x['evidence_id']:x for x in allE}
oldF=json.loads((W/'coverage-before.json').read_text());articles=json.loads(Path('work/eink-systems-research/boox-articles.json').read_text());ad={x['id']:x for x in articles}
caps={x['key']:x for x in map(json.loads,(W/'captures.jsonl').read_text().splitlines())}
imgs=['mobile-v4-p001.png','mobile-v4-p086.png','mobile-v4-p095.png','mobile-v4-p097.png','palma2-smart-button.jpg']
for n in imgs:shutil.copyfile(W/'raw'/n,A/n)

notes='''# M010 Palma 2：版本、静态观察及材料来源

查阅/观察2026-10-27。未操作设备或App，无连续流程、帧时间轴或耗时。Palma 2黑白Android13保持原对象；不借Palma 2 Pro/Palma 3的手写、SIM或其他配置。机上固件及NeoReader构建仍未知。

## 原件与版本

- E-PALMA-01/02：原URL普通GET200再取，分别为Palma 2产品页/2024-10-22发布稿。只采本机正文，相关商品不采；宣传图不是当前固件画面保证。
- E-PALMA-15：原零售商URL取得103页PDF，SHA256 `66a194e882c852677433b96a8d5ac128adf9f9cfb652f73da6738d7f6f937808`。封面为Mobile ePaper Series Universal User Manual V4.0，作者元数据ONYX INTERNATIONAL INC.，创建/修改日2024-12-10；元数据不是发布日期。文件名含Palma 2 Pro不证明适用Pro或Palma 2。p2–3有版本差异说明，p94的Android11密码说法也不能套Android13。
- 通用帮助沿用旧官方公开JSON：work/eink-systems-research/boox-articles.json；PC分类完整索引沿用work/boox-controls-deepening/epaper-pc-articles.json（59篇、14节、next_page=null）。本轮未请求API、账户接口或全站重抓。目录完整只指这份已存分类索引/PDF书签，不是Palma 2功能全集。

## 实际静态观察

四张PDF摘页均由本轮原PDF渲染并实际view_image查看。所看页没有印刷页号，以下全用1-based PDF页序，未建立印刷页码对应。图片保存在本目录；不能把任意两页拼作连续执行过程。

| 依据 | 摘页 | 只观察到什么 | 限制 |
| --- | --- | --- | --- |
| E-PLCD-04 ← E-PALMA-15 | [封面](mobile-v4-p001.png)，PDF p1 | 系列通用V4.0署名 | 不是机型/已装固件证明 |
| 同上 | [Smart Button](mobile-v4-p086.png)，PDF p86 | 三种按法；图中短按Full refresh、双击AI Assistant、长按None | 示例值；没有进入动作、选择列表或按下结果 |
| 同上 | [Power](mobile-v4-p095.png)，PDF p95；正文p94–95 | 休眠后Wi-Fi、蓝牙、音频三项分开；示例前两项Turn On、音频Pause，自动休眠5分钟、自动关机Never | 未打开下级选项；不是默认值。正文给睡眠后蓝牙在线播放的条件，未验证TTS、断点或恢复 |
| 同上 | [System Navigation](mobile-v4-p097.png)，PDF p97 | Gestures/Navigation Bar、More Settings、Side Gestures及Screenshots入口 | 未触发、无返回状态观察 |
| E-PLCD-03 ← E-PALMA-01 | [本机官方宣传图](palma2-smart-button.jpg) | Smart Button三种按法设置，三项显示None；旁列可用功能示意 | 本机宣传静态图，与V4.0例值不同不构成固件变化证明 |

根代理另实际查看Power摘页和本机宣传图，并复核PDF p1/86/94/95/97文本。此复核不另增E，不替代缺失的实机过程。

## 文字补证和排除

- E-PLCD-01：通用Troubleshooting，仅用第三方App把音量键用于翻页、USB从充电切File Transfer的问答。Notes回收站等其他问答不借给Palma 2。
- E-PLCD-02与既有E-BXCD-14：同名Sleep and Battery分属两类指南，自动休眠设置入口分别为Power和Display；保留差异，不给Palma 2拼成统一入口。
- E-BXCD-06与E-PALMA-13：Stay active、冻结、自启动是不同控制。资料未给它们与Power三项的优先级；开保活不等于音频恢复保证。
- E-PALMA-09/10/14/16本地复读：只补文字批注导出、书签反馈、文件接收和同步/备份条件；手写和Notes子流程不纳入。

历史论坛“Palma 2手册”条目及其独立附件普通GET均404，停止；搜索索引中的历史答复仅作发现线索，没有当作当前可读官方手册或员工身份证明。两条失败URL及哈希见work/captures.jsonl。没有换域重试、登录、提交、安装、下载固件/APK/音视频。

本批0新F；旧任务的通用文档分支仍有条件适用。听书暂停/返回后的句子、闹钟/蓝牙断连、强退/重启、文件同名冲突与恢复成功均未知。正式路径和哈希见work/formal-manifest.json与HANDOFF。
'''
(A/'source-notes.md').write_text(notes)

E=[]
for eid,i,title in [('E-PLCD-01',8569506159124,'通用故障帮助：音量翻页与USB文件传输条件'),('E-PLCD-02',8569254233620,'E Ink Tablets通用休眠帮助：与PC分类入口差异')]:
 a=ad[i];p=W/'raw'/f'existing-{i}.txt'
 E.append(dict(evidence_id=eid,product_id='M-010',title=title,url=a['html_url'],checked_on='2026-10-27',source_type='official_help',access_mode='local_saved_official_public_article_body_reread',version_scope='Generic BOOX E Ink Tablets help; no Palma 2 build identification; conditional comparison only',app_build=None,firmware_version=None,source_updated_on=a['updated_at'][:10],locator=('Software:first question; Hardware:USB question; Notes recovery excluded' if i==8569506159124 else 'Advanced Power Settings; Sleep/Shutdown'),note='通用正文不能当作Palma 2当前构建或操作验证；范围见case。',hands_on=False,visual_observed=False,supersedes=None,raw_work_path='work/eink-systems-research/boox-articles.json',raw_sha256=sha('work/eink-systems-research/boox-articles.json'),extracted_work_path=str(p),extracted_sha256=sha(p),archived_artifact='artifacts/palma-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),capture_state='existing_official_raw_source_reread_and_short_note_archived'))
im=json.loads((W/'image-fetch.json').read_text())
E.append(dict(evidence_id='E-PLCD-03',product_id='M-010',title='Palma 2官方Smart Button宣传静态图',url=im['url'],checked_on='2026-10-27',source_type='official_product_image',access_mode='anonymous_GET_and_viewed_static_image',version_scope='Palma 2 product-page illustration; firmware/build and default values not established',app_build=None,firmware_version=None,source_updated_on=None,locator='Smart Button fields in product-page image',note='三项None只是该图值，无动作/结果；与通用手册不同例值不证明版本迁移。',hands_on=False,visual_observed=True,supersedes=None,derived_from='E-PALMA-01',raw_work_path=im['raw_work_path'],raw_sha256=im['raw_sha256'],archived_artifact='artifacts/palma-controls-deepening/palma2-smart-button.jpg',sha256=sha(A/'palma2-smart-button.jpg'),visual_audit_path='artifacts/palma-controls-deepening/source-notes.md',visual_audit_sha256=sha(A/'source-notes.md'),observed_on='2026-10-27',capture_state='static_image_and_observation_note_archived'))
E.append(dict(evidence_id='E-PLCD-04',product_id='M-010',title='Mobile ePaper V4.0四页静态观察：封面/智能键/电源/导航',url=ed['E-PALMA-15']['url'],checked_on='2026-10-27',source_type='manufacturer_manual_static_pages',access_mode='rendered_existing_source_PDF_and_viewed_four_static_pages',version_scope='Generic Mobile ePaper Series V4.0, PDF metadata2024-12-10; not Palma 2-specific or installed-build proof',app_build=None,firmware_version='V4.0 generic manual',source_updated_on=None,locator='1-based PDF pages1,86,95,97; no printed page numbers visible on these pages',note='只是旧E-PALMA-15的四页衍生静态证据，不另建整册手册来源。Power示例Audio Pause不是默认或执行结果。',hands_on=False,visual_observed=True,supersedes=None,derived_from='E-PALMA-15',raw_work_path=caps['mobile-v4-existing']['raw_work_path'],raw_sha256=caps['mobile-v4-existing']['raw_sha256'],archived_artifact='artifacts/palma-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),observed_on='2026-10-27',observed_pdf_pages=[1,86,95,97],observed_artifacts=[{'path':'artifacts/palma-controls-deepening/'+n,'sha256':sha(A/n)} for n in imgs[:4]],capture_state='derived_static_pages_and_observation_note_archived'))
(B/'palma-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in E))
with (B/'palma-controls-deepening-coverage.csv').open('w',newline='') as f:csv.DictWriter(f,fieldnames=list(oldF[0]),lineterminator='\n').writeheader()

change={
1:dict(evidence_ids='E-PALMA-01;E-PALMA-02;E-PLCD-01;E-PLCD-03;E-PLCD-04',known_branches='Palma 2宣传图有短按/双击/长按三项且例值均None；通用V4.0例值不同，均不是出厂默认。通用FAQ要求部分App另开音量翻页。',gaps='本机设置入口、动作列表、按App事件映射、默认值、锁屏/休眠触发及指纹失败；静态图不闭合执行链。',visual_status='static_reference_only'),
2:dict(evidence_ids='E-PALMA-04;E-PALMA-15;E-PLCD-04',known_branches='通用帮助给双下拉/导航球/导航路径，V4.0图可见手势或导航栏与侧手势入口；回桌面、返回阅读与结束媒体不是同一动作。',gaps='Palma 2已装版本的入口、默认手势、通知媒体键、退出选区和返回位置；通用图未操作。',visual_status='static_reference_only'),
5:dict(evidence_ids='E-PALMA-08;E-PALMA-09',known_branches='通用帮助：Contents→Annotations管理/导出到本地、邮件或其他账户；中央菜单→More→Settings→Annotation Export Settings。文字与手写出口分开。',gaps='Palma 2实际导出格式/路径、覆盖及取消、原书与副本关系、重装恢复；关闭浮层后选区、离线词库及重复词合并。'),
6:dict(evidence_ids='E-PALMA-10;E-PALMA-11',known_branches='通用帮助用页角书签图标空心变实心表示添加，Contents定位；Prev/Next View为文内视图历史，不是跨App/重启恢复。',gaps='本机当前构建、字号变化后的锚点、关闭书后的历史长度、书签删除与同步冲突。'),
7:dict(evidence_ids='E-PALMA-11;E-PALMA-12;E-PALMA-13;E-PALMA-15;E-BXCD-06;E-BXCD-14;E-PLCD-02;E-PLCD-04',known_branches='TTS入口/引擎配置与第三方App保活、冻结分开；V4.0通用手册另要求休眠后蓝牙在线播放时保留Wi-Fi/蓝牙/音频，图中音频Pause仅示例。同名帮助Power/Display入口不同。',gaps='本机固件/入口及开关选项、保活/冻结/自启动/休眠优先级；锁屏与返回后的TTS句子、来电/闹钟/蓝牙恢复、离线包和计时结果。',visual_status='static_reference_only'),
8:dict(evidence_ids='E-PALMA-14;E-PLCD-01',known_branches='LAN Receive与远程Push不同；通用USB排错先检查线/电脑，并在Charging this device via USB中选择File Transfer；未证明传输后自动入库。',gaps='Palma 2当前USB界面、书架扫描/打开、重名/中断/取消、卡移除及空间不足后的残留。'),
9:dict(evidence_ids='E-PALMA-16;E-PALMA-03;E-PALMA-15',known_branches='通用V4.1+：源文件每份≤200MB；总同步关闭时单书及手动同步不可用；同账号/服务器/目录。booxc含本地书与关联数据，两端均需V4.1+。',gaps='Palma 2是否已装适用版本、恢复过程/失败、覆盖与重复名、跨版冲突、删除传播；第三方App数据不在该书库备份承诺内。'),
10:dict(evidence_ids='E-PALMA-15',known_branches='通用V4.0区分清密码默认设置、书阅读数据、缓存/封面与整机重置；清阅读数据不可恢复，重置清用户数据。阅读锁仅NeoReader有效；Android11锁屏密码说明不套Palma 2。',gaps='Palma 2现固件菜单/确认、完整删除/回收站、账号切换、备份是否完整和恢复成功；尚无可操作本机恢复指南。')}
updates=[]
for bef in oldF:
 n=int(bef['flow_id'][-2:])
 if n not in change:continue
 aft={**bef,**change[n],'checked_on':'2026-10-27'}
 updates.append(dict(flow_id=bef['flow_id'],before=bef,after=aft,changed_fields={k:{'before':bef[k],'after':aft[k]} for k in bef if bef[k]!=aft[k]}))
dump(W/'old-coverage-updates.json',updates)

adds=[]
for eid,key in [('E-PALMA-01','palma-product'),('E-PALMA-02','palma-launch'),('E-PALMA-15','mobile-v4-existing')]:
 c=caps[key];item={k:c[k] for k in ['checked_on','access_mode','http_status','raw_work_path','raw_sha256']}
 for k in ['extracted_work_path','extracted_sha256']:
  if k in c:item[k]=c[k]
 if eid=='E-PALMA-15':item.update(pdf_pages=103,pdf_metadata_creation_date='2024-12-10',metadata_date_is_not_publication_date=True,locator='PDF physical pages; outline73; selected body read; observations separately E-PLCD-04')
 item.update(hands_on=False,visual_observed=False,archived_artifact='artifacts/palma-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'))
 adds.append(dict(evidence_id=eid,url=ed[eid]['url'],before_additional_captures=ed[eid].get('additional_captures',[]),append_additional_captures=[item]))
for eid,i in [('E-PALMA-04',25213043150996),('E-PALMA-09',8569296110100),('E-PALMA-10',10701496119060),('E-PALMA-11',10701495414804),('E-PALMA-12',360031058312),('E-PALMA-13',10701262170644),('E-PALMA-14',10701399613076),('E-PALMA-16',25401838386196),('E-BXCD-06',10701299044372),('E-BXCD-14',10701171762452)]:
 p=W/'raw'/f'existing-{i}.txt';assert p.exists()
 item=dict(checked_on='2026-10-27',access_mode='local_saved_public_help_body_reread_no_network',raw_work_path='work/eink-systems-research/boox-articles.json',raw_sha256=sha('work/eink-systems-research/boox-articles.json'),extracted_work_path=str(p),extracted_sha256=sha(p),locator=f'article id{i}; conditional context for M-010 Palma 2',hands_on=False,visual_observed=False,archived_artifact='artifacts/palma-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'))
 adds.append(dict(evidence_id=eid,url=ed[eid]['url'],before_additional_captures=ed[eid].get('additional_captures',[]),append_additional_captures=[item]))
eid='E-BXCD-01';item=dict(checked_on='2026-10-27',access_mode='local_saved_category_index_reread_no_network',raw_work_path='work/boox-controls-deepening/epaper-pc-articles.json',raw_sha256=sha('work/boox-controls-deepening/epaper-pc-articles.json'),locator='59 articles/14 sections, next_page=null; not Palma 2 model inventory',hands_on=False,visual_observed=False,archived_artifact='artifacts/palma-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'))
adds.append(dict(evidence_id=eid,url=ed[eid]['url'],before_additional_captures=ed[eid].get('additional_captures',[]),append_additional_captures=[item]));dump(W/'old-evidence-additional-captures.json',adds)

rows=[];toc=json.loads((W/'raw/manual-outline.json').read_text());observed={1,86,95,97};read={1,2,3,4,5,6,23,24,30,31,32,33,34,35,36,38,39,43,47,48,49,50,51,58,63,64,70,86,87,88,89,93,94,95,97,98,99,100,101}
def add(**x):rows.append({'inventory_id':f'I-PLCD-{len(rows)+1:03}',**x})
def flow_for(title):
 s=title.lower();out=[]
 for words,n in [(['smart button'],1),(['control center','notification center','navigation','naviball'],2),(['refresh','e-ink','enhancement','filter','contrast','display','color'],3),(['dictionary','annotation'],5),(['contents','progress','floating toolbar'],6),(['power','sleep','startup','freeze','optimization'],7),(['transfer','storage','scan'],8),(['sync','backup','account'],9),(['book property','reset device'],10)]:
  if any(a in s for a in words):out.append(f'F-M010-{n:02}')
 return ';'.join(dict.fromkeys(out))
for level,title,p in toc:
 add(source_family='V4.0 universal PDF bookmark outline',level=level,item=title.strip(),locator=f'PDF page {p}; printed-page mapping not established',source_url=ed['E-PALMA-15']['url'],evidence_ids='E-PALMA-15'+(';E-PLCD-04' if p in observed else ''),flow_ids=flow_for(title),coverage_status=('static_page_observed_generic' if p in observed else 'selected_body_read_generic' if p in read else 'outline_only'),model_version_scope='Mobile ePaper Series V4.0; Palma 2 current applicability unverified',gap='书签目录≠正文全读；通用章节不计Palma 2已验证功能')
pc=json.loads(Path('work/boox-controls-deepening/epaper-pc-articles.json').read_text())['articles'];sec={x['id']:x['name'] for x in json.loads(Path('work/boox-controls-deepening/sections.json').read_text())['sections']}
readids={25213043150996,8569296110100,10701496119060,10701495414804,360031058312,10701262170644,10701399613076,25401838386196,10701299044372,10701171762452}
for a in sorted(pc,key=lambda a:(a['section_id'],a['position'],a['id'])):
 matches=[e['evidence_id'] for e in allE if e['url']==a['html_url']];aids=','.join(matches) if matches else 'E-BXCD-01'
 category=sec.get(a['section_id'],'unknown')
 add(source_family='Stored official ePaper Tablet PCs index',level=1,item=category+' / '+a['title'],locator='article '+str(a['id']),source_url=a['html_url'],evidence_ids=aids,flow_ids=flow_for(a['title']),coverage_status='selected_body_read_generic' if a['id'] in readids else 'index_only',model_version_scope='Generic category snapshot2026-10-26; no Palma 2 build association',gap='手写/笔/键盘/分屏章节仅目录，不把其他机型能力纳入Palma 2')
for title,f,gap in [('Palma 2专用当前手册/固件','', '旧论坛与附件404停止；通用PDF不能替代本机版本'),('实体键应用事件及锁屏作用','F-M010-01','静态例值不等于默认或按下结果'),('TTS后台及逐句恢复','F-M010-07','需本机TTS/引擎/固件及中断过程'),('源书与阅读数据恢复成功','F-M010-09','备份条件不等于恢复验证；需重复名/覆盖/取消'),('清数据与重装/重置恢复','F-M010-10','暂无当前Palma 2对象范围与可恢复性验证')]:
 add(source_family='Research gap',level=0,item=title,locator='current case gap',source_url='',evidence_ids='',flow_ids=f,coverage_status='not_established',model_version_scope='Palma 2 actual firmware unknown',gap=gap)
with (B/'palma-controls-deepening-inventory.csv').open('w',newline='') as f:wr=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');wr.writeheader();wr.writerows(rows)
dump(W/'inventory-summary.json',dict(total=len(rows),manual_outline=len(toc),help_index=len(pc),help_sections=len({a['section_id'] for a in pc}),gaps=5,new_evidence=4,new_flows=0,old_flow_updates=len(updates),changed_fields=sum(len(x['changed_fields']) for x in updates),old_evidence_captures=len(adds)))

text=(W/'case-before.md').read_text()
text=text.replace('查阅：2026-10-26。','初查2026-10-26，本轮补读/静态观察2026-10-27。',1)
text=text.replace('**D1+D2 文档研究，无实机、无连续视觉观察。**','**D1+D2 文档研究，无实机、无连续视觉观察。D2只表示下表明确条件下的通用文档路径，不表示Palma 2当前构建已经验证。**',1)
text=text.replace('另读到零售商托管、文件名含 Palma 2 Pro 的厂商通用 V4.0 手册，封面实际为 Mobile ePaper Series；仅用于识别历史版本/数据清理语义，不能作为 Palma 2 专属手册。','本轮取得原零售商URL的103页PDF，文件名含Palma 2 Pro，封面实际为Mobile ePaper Series通用V4.0；只作历史通用路径与静态对照，不能作为Palma 2专属或已装固件手册。')
text=text.replace('## 主要任务','''## 本轮来源目录与观察边界

[逐项目录](../batches/palma-controls-deepening-inventory.csv)列出V4.0 PDF全部73个书签目录项、旧官方PC类帮助59篇/14节及5项关键缺口。它分别标注目录、选定正文与静态观察；不把这些条目全部算作Palma 2功能。PDF没有另见印刷总目录；页号使用PDF页序，所看4页无印刷页号，不能冒称印刷页码。

Palma 2官网Smart Button宣传图显示短按、双击、长按三项且例值均None；通用手册p86示例分别是Full refresh、AI Assistant、None。两图都没有执行动作；不能据此指定默认值或推断某次固件改变。[E-PLCD-03～04]

本轮实际看图仅有这张本机宣传图和通用PDF p1/86/95/97。具体观察、图片与来源身份见[静态审计](../artifacts/palma-controls-deepening/source-notes.md)。历史“Palma 2手册”论坛帖及链接附件GET404已停止；搜索索引的答复不能补成当前可得的专用手册。

## 主要任务''',1)
ops={
1:'型号页确认智能键及指纹电源键；官方静态图可见三种按法设置。通用FAQ提示某些阅读App另需启用音量翻页；未取得本机完整设置链。',
2:'通用帮助：右上控制中心、左上通知中心；Settings→System Navigation可配置底/侧导航，导航球双击回桌面。V4.0图仅确认相应入口存在。',
5:'通用帮助：长按选文→标注；Contents→Annotations可管理和导出。中央菜单→More→Settings→Annotation Export Settings调整出口；未执行导出。',
6:'通用帮助：点页角书签，空心变实心；Contents定位书签/目录。Progress跳页/章，Prev View和Next View往返文内视图。',
7:'Progress→TTS及系统引擎设置仍是通用路径。App Optimize→Others的保活、App Management冻结与Power休眠控制分别检查；通用PDF p94–95仅给蓝牙在线播放时保留连接/音频的条件。',
8:'BOOXDrop同LAN上传后看Receive；远程登录推送看Push。通用USB排错另给通知中的Charging this device via USB→File Transfer；这不是自动入库证明。',
9:'通用V4.1+：长按书→Sync→Cloud Sync→Upload source file；书库设置→Backup and Restore→Local Backup and Restore生成/恢复booxc。需满足开关、版本与文件条件。',
10:'通用V4.0列Clear settings各对象；清密码默认设置不清密码。整机Reset Device另清全部用户数据；这些菜单及确认仍未在Palma 2验证。'}
start=text.index('| F 编号 | 任务 |',text.index('## 主要任务'));end=text.index('\n## 分支与未闭环状态',start)
oldlines=text[start:end].splitlines();out=oldlines[:2]
for line in oldlines[2:]:
 if not line.startswith('| F-M010-'):continue
 cells=[x.strip() for x in line.split('|')[1:-1]];n=int(cells[0][-2:])
 if n in change:
  cells[3]=ops[n];cells[4]=change[n]['known_branches'];cells[5]=change[n]['evidence_ids'].replace(';','；');cells[6]=change[n]['gaps']
 out.append('| '+' | '.join(cells)+' |')
text=text[:start]+'\n'.join(out)+'\n'+text[end:]
text=text.replace('本机有翻页键不代表每个 app 使用同一输入事件；目前无本机官方逐 app 映射路径。','通用FAQ提示音量翻页前提；本机存在按键仍不能证明每个App事件相同，逐App映射未核。')
text=text.replace('通用帮助指出冻结阻止后台；但不能单因该开关存在，就说取消冻结必然修复。','通用资料把冻结/保活与睡眠后的Wi-Fi、蓝牙、音频分开；Power图的Audio Pause仅示例，不能说取消冻结必然修复。')
text=text.replace('完整新机手册仍需设备或账号获取；没有为此注册/登录/安装应用。','官方下载帮助要求设备或账号获取对应手册；本轮未取得专用当前手册，没有为此注册/登录/安装应用。')
text=text.replace('本轮阅读官方产品/帮助及厂商署名资料。没有可用的连续视觉记录，网页图不算真实操作；PDF截图请求未得到可观察画面，未计视觉证据。','初查只有文本，原PDF截图请求失败未计观察。本轮正常取得原PDF并实际查看4张摘页及1张型号宣传图，均是静态参考，不是连续过程、默认值或本机操作。没有测刷新/解锁/音频耗时。')
text=text.replace('全部查于2026-10-26；共享 BOOX 帮助通过官方正文/公共API读取，更新日期写入批次。15为厂商署名、零售商托管通用手册，绝非已固定机型截图。','旧来源初查2026-10-26；本轮复读既存官方JSON并再取原产品页/发布稿/PDF，增量见旧E追加建议。15仍是厂商署名、零售商托管通用手册；4页静态观察用衍生E-PLCD-04，不重复登记整册。')
text+='''
| E-PLCD-01 | [通用故障帮助](https://booxsupport.zendesk.com/hc/en-us/articles/8569506159124-Troubleshooting) | 旧官方JSON本地复读；只采用音量翻页和USB问答，不套Notes恢复。 |
| E-PLCD-02 | [E Ink Tablets休眠帮助](https://booxsupport.zendesk.com/hc/en-us/articles/8569254233620-Sleep-and-Battery) | 与既有E-BXCD-14的PC类Power/Display入口差异；非本机当前路径。 |
| E-PLCD-03 | [Palma 2 Smart Button图](../artifacts/palma-controls-deepening/palma2-smart-button.jpg) | E-PALMA-01产品页衍生静态证据。 |
| E-PLCD-04 | [通用手册4页观察](../artifacts/palma-controls-deepening/source-notes.md) | derived_from E-PALMA-15，PDF p1/86/95/97；非当前机型执行链。 |
'''
text=text.replace(' |\n\n| E-PLCD-01',' |\n| E-PLCD-01');case.write_text(text)
formal=[case,*[B/f'palma-controls-deepening-{n}' for n in ['evidence.jsonl','coverage.csv','inventory.csv']],A/'source-notes.md',*[A/n for n in imgs]]
dump(W/'formal-manifest.json',[{'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size} for p in formal]);print(json.dumps(json.loads((W/'inventory-summary.json').read_text()),ensure_ascii=False))
