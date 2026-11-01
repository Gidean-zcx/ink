from pathlib import Path
import json,csv,hashlib,shutil,collections,re
W=Path('work/ak-directory-deepening')
L=Path('docs/research/interaction-library')
A=L/'artifacts/ak-directory-deepening'; A.mkdir(parents=True,exist_ok=True)
B=L/'batches'; C=L/'cases/M-065-astell-kern.md'
DATE='2026-10-27'; PDF='https://cdn.astellnkern.com/bbs/download/202305/20230503165340_8b122109-9c25-4ef8-8ecc-69290747288f.pdf'
RAW=Path('work/ak-controls-deepening/SR35_instruction_book_ENG.pdf')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,v):Path(p).write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rows,fields):
 with open(p,'w',newline='') as f:
  wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(rows)
oldE={e['evidence_id']:e for e in json.loads((W/'evidence-before.json').read_text())}
oldF={e['flow_id']:e for e in json.loads((W/'coverage-before.json').read_text())}
caps={e['name']:e for e in json.loads((W/'captures.json').read_text())}
notes='''# SR35 目录与控制补证（2026-10-27）

固定 SR35、V1.22 发布基线；手册仍是 2023 年英文原件，并非 V1.22 界面保证。全部为文档读取或印刷静态图，无设备/账号操作。

## 原手册及已看摘页

沿用 E-AKCD-01，[官方 PDF](https://cdn.astellnkern.com/bbs/download/202305/20230503165340_8b122109-9c25-4ef8-8ecc-69290747288f.pdf)，73 文件页，文件页=印刷页+1。原件在 work/ak-controls-deepening/SR35_instruction_book_ENG.pdf；SHA256 `2fc0dd11a7b6babfcc11028e2f1223c16320287f47adb18bb72ae10d97155182`。本批本地复读，不重新下载。

E-AKDD-03 为同一原件的派生静态观察：印刷1/文件2核目录；40/41核歌单确认与校准图标；62/63核EQ图文。五张单帧已用 view_image 查看，无连续流程、时间戳或操作成功记录。旧24/30/48/59/60图本批未重复观察。

| 印刷页 | 定位与谨慎解释 |
| --- | --- |
| 1 | 36个实际目录条目；不是按73文件页凑目录。 |
| 37 | Folder中copy/move/remove/edit tags只给入口；目标选择、冲突、保存/撤销均未给。 |
| 39–40 | 成员移除明确不删存储音频；整歌单删除另有确认，不把前一句扩大为所有Delete的保证。 |
| 41 | 编辑选曲后校准，完成项有标记；启用开关与校准分离。文件是否被改写、取消/失败无说明。 |
| 52、62–63 | 新建命名、调频、Advanced参数及同型号EQ文件共享；62正文SAVE与图中Rename EQ/CHANGE不一致。没有自动保存/删除预设/冲突合并的闭环。 |
| 66–67 | 故障正文延续到67，67亦开始Copyright；旧粗目录只列66，不能漏掉坏文件分支。 |

## 日本型号支持页与共用 FAQ

沿用 E-AK-12，[SR35支持索引](https://www.iriver.jp/support/product_235.php)：本批读到24条FAQ、1手册、6固件条目、1驱动和1Mac传输软件。链接存在不等于正文已读，台账逐项区分。本批仍列V1.22；旧全球日期不重查，日区V1.22仅复核已知锁键/Chrome说明，不下载固件。

E-AKDD-01，[封面显示FAQ](https://www.iriver.jp/support/faq_965.php)：正文范围写AK70及第二代以后系列，SR35索引有链接；提供嵌入图片或专辑文件夹图片加重启/数据库更新的办法。其通用旧界面提及清空播放列表复选框，不能据此确认SR35当前有同一选项。图片未查看。

E-AKDD-02，[封面大小FAQ](https://www.iriver.jp/support/faq_1035.php)：列15MB、约1024×1024；另一较长FAQ还列颜色模式例外。这里仅保留共用文档条件，未测SR35边界值，不据成功显示承诺解码/载入时间。

沿用 E-AKCD-11，[ReplayGain FAQ](https://www.iriver.jp/support/faq_1104.php)：格式上限与手册一致，另写Line Out时不工作。沿用 E-AK-10，[M3U FAQ](https://www.iriver.jp/support/faq_1033.php)：有支持声明及路径匹配限制，没有导入/修复流程；不把其“无规范”说法当本项目格式标准判断。

原始HTML及文本在work/ak-directory-deepening/raw，哈希见captures.json及HANDOFF。正式摘记只保留关键定位与限制；菜单台账以短名称为主，不复制长段手册。没有测听、耗时测量、服务账户或文件操作。
'''
(A/'source-notes.md').write_text(notes)
for n in [1,40,41,62,63]:shutil.copyfile(W/f'sr35-printed-{n}.png',A/f'sr35-printed-{n}.png')
note_rel=str((A/'source-notes.md').relative_to(L)); note_sha=sha(A/'source-notes.md')
base=dict(product_id='M-065',checked_on=DATE,app_build=None,firmware_version=None,source_updated_on=None,hands_on=False,supersedes=None)
E=[]
for no,name,title,locator in [(1,'album-art','SR35关联共用封面FAQ：旧界面与数据库更新边界','AK70及第二代以后范围；封面方法、数据库更新、图像条件'),(2,'art-size','SR35关联共用封面大小FAQ','正文最大容量与近似像素；非当前固件边界测试')]:
 cap=caps[name];E.append(dict(base,evidence_id=f'E-AKDD-{no:02}',title=title,url=cap['url'],source_type='official_help',access_mode='anonymous_public_html_GET',version_scope='SR35日区支持页关联的共用FAQ；正文无SR35构建号，旧图不用于型号观察；V1.22仅研究基线',locator=locator,note='只作封面/重建条件补证；共用旧菜单不能补成SR35当前确认链。',visual_observed=False,raw_work_path=cap['raw_work_path'],raw_sha256=cap['raw_sha256'],extracted_work_path=str(W/'raw'/f'{name}.txt'),extracted_sha256=sha(W/'raw'/f'{name}.txt'),archived_artifact=note_rel,sha256=note_sha,capture_state='raw_source_in_work_and_short_note_archived'))
imgs=[str((A/f'sr35-printed-{n}.png').relative_to(L)) for n in [1,40,41,62,63]]
E.append(dict(base,evidence_id='E-AKDD-03',title='SR35手册目录/歌单/ReplayGain/EQ五页静态观察',url=PDF,source_type='official_manual',access_mode='local_pdf_page_render_viewed',version_scope='2023英文SR35手册，未绑定固件；不能认定V1.22界面一致',locator='印刷1/40/41/62/63；文件2/41/42/63/64，各一张静态页',note='同一PDF派生，不是第二独立来源；62页SAVE与配图Rename EQ/CHANGE差异保留。',visual_observed=True,raw_work_path=str(RAW),raw_sha256=sha(RAW),archived_artifact=imgs[0],sha256=sha(L/imgs[0]),rendered_artifacts=imgs,artifact_hashes={p:sha(L/p) for p in imgs},derived_from='E-AKCD-01',capture_state='selected_static_page_excerpt'))
(B/'ak-directory-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
updates=[]
def change(no,**kw):
 before=oldF[f'F-M065-{no:02}'];after=dict(before,checked_on=DATE,**kw)
 updates.append(dict(flow_id=before['flow_id'],before=before,after=after,changed_fields={k:{'before':before[k],'after':v} for k,v in after.items() if v!=before[k]}))
change(1,evidence_ids='E-AK-02;E-AKCD-01;E-AKDD-01;E-AKDD-02',known_branches='Media Device复制后断线；Safely Remove/Unmount。Folder右侧菜单有文件/目录复制、移动、移除，文件另有edit tags；仅入口明确。关联封面FAQ提嵌图或专辑目录图片后重启/重建，属通用旧文档。',gaps='扫描触发、同名/中断、移动后歌单修复；设备端目标选择/确认/撤销、标签保存规则未给。旧封面FAQ的清空歌单复选框不认定SR35现版具备；数据库重扫不保证无损。')
change(2,evidence_ids='E-AK-02;E-AKCD-01;E-AKDD-03',known_branches='Next/Last与持久歌单分开；创建+命名/Add、加曲/Complete。可选部分曲加入Now Playing顶部/底部/当前后；成员DELETE明确不删存储音频。Playlists编辑选歌单→重命名图标→RENAME窗口→命名/RENAME；删除图标→DELETE确认整歌单。',gaps='当前项移除后的播放、撤销、队列排序持久化、重名/空名、整歌单恢复与源文件移动后的引用未核；整表删除不可借成员移除说明推定所有数据后果。',visual_status='static_reference_only')
change(9,known_branches='共用FAQ声明M3U支持，同时指出Android路径不匹配可能无法识别；本轮复读未新增导入操作证据。',gaps='SR35导入/扫描入口、编码、相对路径、重扫去重及批量修复未知；不将FAQ“无规范”措辞当标准事实，仍D1。')
change(16,known_branches='Mini Player上滑/点按展开，下滑/返回收起；拖进度、封面歌词、时长切换。More可回歌曲的Album/Artist/Folder或File Info；Songs/Albums/Artists/MQS分类支持名称关键词检索，结果编辑可播/加入歌单。',gaps='无歌词/空搜索反馈、定位粒度、章节/持久时间书签、重启进度和返回滚动位置未核；分类排序/首页次序只有入口，不推定拖拽或持久化规则。')
dump(W/'old-coverage-updates.json',updates)
F=[]
def flow(no,task,known,gaps):F.append(dict(flow_id=f'F-M065-{no:02}',product_id='M-065',task=task,depth='D2',evidence_ids='E-AKCD-01;E-AKDD-03'+(';E-AKCD-11' if no==18 else ''),known_branches=known,gaps=gaps,visual_status='static_reference_only',hands_on='false',case_path='cases/M-065-astell-kern.md',checked_on=DATE))
flow(17,'创建、调整及同型号复制用户EQ','长按通知EQ或Settings/Equalizer启用→Normal EQ/+→命名；正文写SAVE但配图Rename EQ/CHANGE。调频与数值，Advanced调Freq/Gain/Q；EQ Sync所得User EQ.dat可经电脑从EQlists复制到另一SR35同目录。','命名确认图文不一致；参数保存时点、预设删除/恢复、重名合并、跨固件共享与第三方app作用域未说明；格式条件只按手册，不代表实测效果。')
flow(18,'校准选曲响度并启停ReplayGain','Albums/Playlists/Folders曲目列表编辑→选曲→校准图标；手册称已校准曲出现标记；通知ReplayGain或Settings/ReplayGain控制启停。官方共用FAQ明确Line Out时不工作。','校准进度/取消/失败、是否改写音频或存数据库、重复校准、重扫后保留、第三方app及实际听感未核；24bit/192kHz以下条件不扩写为全格式支持。')
csvout(B/'ak-directory-deepening-coverage.csv',F,list(next(iter(oldF.values())).keys()))
add=[]
for eid,name in [('E-AK-12','sr35-support'),('E-AKCD-11','replaygain'),('E-AK-10','m3u'),('E-AK-04','v122')]:
 cap=caps[name];app=dict(checked_on=DATE,access_mode='anonymous_public_html_GET',hands_on=False,visual_observed=False,url=cap['url'],http_status=200,raw_work_path=cap['raw_work_path'],raw_sha256=cap['raw_sha256'],extracted_work_path=str(W/'raw'/f'{name}.txt'),extracted_sha256=sha(W/'raw'/f'{name}.txt'),archived_artifact=note_rel,sha256=note_sha,locator={'sr35-support':'24 FAQ及9下载条目，仅选择性正文读取','replaygain':'格式上限、Line Out不工作','m3u':'支持声明与路径限制；无新导入链','v122':'创建/更新时间、OTA、锁键与Chrome'}[name]);before=oldE[eid].get('additional_captures',[]);add.append(dict(evidence_id=eid,field='additional_captures',before=before,after=before+[app],append=app))
app=dict(checked_on=DATE,access_mode='existing_pdf_reread_PyMuPDF',hands_on=False,visual_observed=False,raw_work_path=str(RAW),raw_sha256=sha(RAW),extracted_work_path=str(W/'manual-pages.json'),extracted_sha256=sha(W/'manual-pages.json'),archived_artifact=note_rel,sha256=note_sha,locator='73文件页提取；本批逐条核印刷1目录与8–11、13–26、35–41、49–63及66–67目标正文；其他章节仅目录/旧批依据。新静态观察另见E-AKDD-03。')
before=oldE['E-AKCD-01'].get('additional_captures',[]);add.append(dict(evidence_id='E-AKCD-01',field='additional_captures',before=before,after=before+[app],append=app));dump(W/'old-evidence-additional-captures.json',add)

# Explicit layered inventory: actual printed TOC, numbered UI controls, settings,
# command menus and the model support index. A mapped F covers a branch, never all items.
I=[]
def row(domain,item,loc,fs='',es='E-AKCD-01',status='body_read_partial',gap='仅文档目录/入口；关联F不表示本项全部分支已完成',url=PDF):
 I.append(dict(inventory_id=f'I-AKDD-{len(I)+1:03}',domain=domain,item=item,source_url=url,locator=loc,coverage_status=status,evidence_ids=es,flow_ids=';'.join('F-M065-'+n.zfill(2) for n in fs.split(',') if n),gap=gap))
toc=[('Package Contents',2,''),('Product Overview',3,'11,16'),('Power Management',5,'11'),('Charging',6,''),('Connections',7,'01,14'),('Firmware Updates',12,'10'),('Basic Operations',13,'16'),('Home Screen',17,'16'),('Menu Category Screen',18,'02,16'),('Main Menu Screen',20,'04'),('Music Player Screen',21,'16'),('Notification Bar',24,'04,10'),('Using Open APP Service',27,'06,07'),('AK CONNECT',28,'12'),('AK File Drop',33,'05'),('Listening to Music',35,'01,02,16'),('Playlist Management',38,'02'),('ReplayGain',41,'18'),('USB DAC Installation',42,'14'),('How to Use USB Audio',43,''),('CD Ripping',44,''),('AK CD-Player',46,''),('Car Mode',47,''),('Bookmark Function',48,'03'),('Favorites Function',49,''),('Settings',50,'04,10,13,17,18'),('Connecting to Wi-Fi',58,''),('Using Bluetooth',59,'08,15'),('Equalizer Setup',62,'17'),('Precautions',64,''),('Troubleshooting',66,'11'),('Copyright',67,''),('Certifications',68,''),('Registered Trademarks',70,''),('Disclaimers',71,''),('Specifications',72,'')]
for n,p,fs in toc:row('印刷目录',n,f'印刷目录1列起页{p}；文件页{p+1}',fs,es='E-AKCD-01;E-AKDD-03',status='toc_verified_body_selective',gap='本批核目录标题/起页；目标正文读取范围见摘记；法律/规格及其余章节不扩展任务')
settings=[(50,'Wi-Fi',''),(50,'Bluetooth','08,15'),(50,'Bluetooth Codec','15'),(51,'AK CONNECT','12'),(51,'AK File Drop','05'),(51,'Roon Ready',''),(51,'DAC',''),(51,'AMP',''),(52,'Equalizer','17'),(52,'Gapless',''),(52,'Line Out','18'),(53,'USB Mode','01,14'),(53,'CD Ripper',''),(53,'Playback Settings','02'),(53,'ReplayGain','18'),(53,'Notification Panel','04'),(53,'LR Balance',''),(54,'Car Mode',''),(54,'USB Audio',''),(54,'Screen Brightness',''),(54,'Volume Limit',''),(54,'Date & Time',''),(54,'Device Name',''),(55,'Input Method',''),(55,'Language',''),(55,'Timer Settings','13'),(56,'Downloads','12'),(56,'Update','10'),(56,'System Information','01'),(57,'System Reset','')]
for p,n,f in settings:row('设置标题',n,f'印刷{p}；入口50',f,gap='手册声明设置随固件变化；完整选项、保存/取消/失败未全部闭合')
subs=[(51,'DAC / DAC Mode',''),(51,'DAC / DAC Filter',''),(52,'Equalizer / USER EQ','17'),(52,'Equalizer / EQ Sync / EQlists','17'),(53,'USB Mode / Media Device','01'),(53,'USB Mode / DAC Input','14'),(54,'Car Mode / on-off',''),(54,'Car Mode / landscape-portrait',''),(54,'USB Audio / DSD output type',''),(54,'USB Audio / MQA decoding',''),(55,'Input Method / Keyboard',''),(55,'Timer / Auto Power Off','13'),(55,'Timer / Screen Timeout','13'),(55,'Timer / Sleep Timer','13'),(56,'Downloads / Default folder','12'),(56,'Downloads / Select Folder','12'),(56,'Update / System Update','10'),(56,'Update / Application Update','06'),(56,'System Information / System Information','01'),(56,'System Information / Legal information',''),(57,'System Reset / Initialize Media Scan','01'),(57,'System Reset / Settings Reset',''),(57,'System Reset / Factory Data Reset','')]
for p,n,f in subs:row('设置明列子项',n,f'印刷{p}',f,gap='仅列手册明确命名子项；不自造未给选项/确认/恢复，重扫/重置/清空分开')
groups=[('Home Screen',17,['Menu','Album Cover','Menu Category','Album Seek Bar','Music Player Screen'],'16'),('Menu Category Screen',18,['Menu','Category','Home/Category Setting','Play All','Music List','Music Player Screen','Services','Search','Edit Mode','Sort','More','Previous Screen'],'02,16'),('Main Menu Screen',20,['AK CONNECT','Services','Key Settings','Close Menu','Settings'],'04,06,12'),('Music Player Screen',21,['Previous','Album Cover','Artist','Title','Favorites','Current Play Time','Repeat Mode','Previous/Rewind','Now Playing','My Device','Format Information','More','Playback Progress Bar','Song Duration','Shuffle Mode','Next/FF','Play/Pause'],'02,16'),('Notification Bar',24,['Wi-Fi','Bluetooth','DAC','EQ','AMP','Car Mode','Gapless Playback','Wheel Lock','ReplayGain','Line-out','AK CONNECT','AK File Drop','Settings','Brightness','Notifications Display'],'04,10,17,18')]
for dom,p,names,fs in groups:
 for n in names:row('编号界面项',dom+' / '+n,f'印刷{p}起；说明至'+str({'Menu Category Screen':19,'Music Player Screen':23,'Notification Bar':26}.get(dom,p)),fs,gap='界面项清点≠完整流程；Wheel Lock为旧版，V1.22变化单列F10；Favorites无独立新F')
commands=[(36,'Current song More',['Add to Playlist','Go to Album','Go to Artist','Go to Folder','File Info'],'02,16'),(36,'Edit mode',['Select/deselect entire list','Now Playing top/bottom/after current','Add selected list to playlist','Delete selected list','Change playlist name','Change CD library album image','Close'],'02'),(37,'Folder selected folder',['Play','Add to bookmarks','Copy','Move','Remove'],'01,03'),(37,'Folder selected file',['Play','Copy','Move','Edit tags','Remove'],'01'),(37,'Category',['Songs/Albums/Artists/MQS search','Artists/album artists sort','Album/CD library viewing options','Playback settings'],'16')]
for p,dom,names,fs in commands:
 for n in names:row('操作菜单明列项',dom+' / '+n,f'印刷{p}',fs,gap='菜单因分类不同；copy/move/remove/tag编辑缺目标/确认/保存/恢复；不推断所有Delete只删引用')
lookup={e['url']:e['evidence_id'] for e in list(oldE.values())+E}
faqbody={'https://www.iriver.jp/support/faq_965.php','https://www.iriver.jp/support/faq_1035.php','https://www.iriver.jp/support/faq_1104.php','https://www.iriver.jp/support/faq_1033.php'}
fmap={'faq_974':'06','faq_1014':'07','faq_965':'01','faq_1012':'05','faq_1011':'08','faq_1029':'11','faq_1033':'09','faq_1104':'18','faq_1035':'01','faq_1046':'08','faq_1103':'04','faq_1107':'14','file_1596':'10','file_1504':'14'}
for a in json.loads((W/'support-links.json').read_text()):
 h=a['href'] or ''
 if 'faq_' not in h and 'file_' not in h:continue
 url='https://www.iriver.jp'+h if h.startswith('/') else 'https://www.iriver.jp/support/'+h
 es='E-AK-12'+(';'+lookup[url] if url in lookup else '')
 fs=next((v for k,v in fmap.items() if k in h),'')
 status='body_reread' if url in faqbody or 'file_1596' in h else ('prior_evidence_body_not_reread' if url in lookup else 'index_link_only')
 row('型号支持FAQ' if 'faq_' in h else '型号支持下载索引',a['title'],'SR35日区支持索引相应列表',fs,es,status,'本轮只逐条登记标题/关联；未读正文不确认适用、操作或新版条件；未下载软件/驱动/固件',url)
csvout(B/'ak-directory-deepening-inventory.csv',I,list(I[0]))
counts=dict(collections.Counter(x['domain'] for x in I));dump(W/'inventory-counts.json',counts)

text=(W/'case-before.md').read_text()
text=text.replace('# M-065 Astell&Kern SR35：文件夹书签、网络下载与音频角色','# M-065 Astell&Kern SR35：文件与歌单、音频角色及声音设置')
text=text.replace('查阅2026-10-26。固定','查阅2026-10-26，2026-10-27补目录/控制。固定',1)
text=text.replace('本轮从官网手册索引所链','2026-10-26从官网手册索引所链',1)
text=text.replace('已实际查看印刷24、30、48、59、60页静态摘页','上批已实际查看印刷24、30、48、59、60页静态摘页；本批新增实际查看1、40、41、62、63页',1)
text=text.replace('完整目录已核对，操作章节按需读取；法律/认证页不展开。','完整印刷目录已核对，操作章节按需读取；法律/认证页不展开。2026-10-27新增分层逐项台账，具体读取与未读状态分列。')
text=text.replace('AKCD-01/11；无新增F','F17/18；AKCD-01/11、AKDD-03')
text=text.replace('校准取消/数据保存和第三方app范围未闭环','校准操作/开关及EQ创建已补，取消/数据保存和第三方app范围未闭环')
text=text.replace('Troubleshooting66；版权/认证67–71','Troubleshooting66–67（67同时开始版权）；版权/认证67–71')
text=text.replace('表中 AK 是旧证据前缀，AKCD 是本批；','表中 AK、AKCD 是此前证据前缀，本批为 AKDD；')
newsec=f'''### 2026-10-27逐条台账与适用范围

[目录台账](/home/workspace/reader/docs/research/interaction-library/batches/ak-directory-deepening-inventory.csv)共 **{len(I)}条**：印刷目录36、设置标题30、设置明列子项23、编号界面项54、操作菜单项26、型号支持FAQ24、下载索引9。层级间有明确交叉，不是{len(I)}项独立功能或已完成流程。36个目录起页来自已看印刷1；所有页码均为印刷页，文件页加1。

菜单/设置正文重点复读8–11、13–26、35–41、49–63与66–67；其他章节只保留目录或旧批分析。目录中的Troubleshooting起于66，正文延续至67，不能因Copyright也从67开始而漏掉坏文件分支。SR35日区索引的24条FAQ不等于24篇本轮全文已读；共用旧封面FAQ虽被型号页链接，其数据库更新与清空歌单界面仍不直接套到V1.22。[E-AK-12；E-AKCD-01；E-AKDD-01/02/03]

新增材料只闭合EQ与ReplayGain的文档任务。Folder复制/移动/删除/标签编辑仅有菜单，分类/首页次序设置缺详细保存规则，继续标部分覆盖。收藏49的开关与Favorites列表已读，未为重复播放管理另增F。封面大小FAQ约1024×1024、15MB是共用文档条件，未测边界值；较长FAQ另列颜色模式例外，不据此承诺所有图片可显示。

'''
text=text.replace('## 任务流程\n',newsec+'## 任务流程\n')
replacements={
1:'| F-M065-01 | USB传入与文件管理边界；电脑/数据线 | Settings/USB Mode选Media Device→复制完成后断线；Safely Remove或System Information/Unmount再移卡。Folder内文件/目录菜单列copy/move/remove，文件另列edit tags。 | 设备端只知入口，目标选择/确认/撤销、标签保存、重名及旧歌单引用修复未给。共用封面FAQ提嵌图或专辑目录图片后重启/重建；旧清空歌单选项不认定SR35现版具备。 | E-AK-02；E-AKCD-01；E-AKDD-01；E-AKDD-02 |',
2:'| F-M065-02 | 队列与持久歌单 | More选Next/Last；Playlist的+→命名/Add；选曲/Complete。部分曲可加到Now Playing顶部/底部/当前后。成员删除需DELETE，明确不删存储音频。Playlists编辑→选歌单→重命名图标→命名/RENAME；整表删除图标→DELETE。 | 静态40有命名和整表删除确认示例。空名/重名、撤销、当前项移除后播放、排序持久化与整表恢复未知；成员移除的无损说明不能套所有Delete。 | E-AK-02；E-AKCD-01；E-AKDD-03 |',
9:'| F-M065-09 | M3U路径兼容性（D1） | 官方共用FAQ声明M3U支持，同时保留Android文件路径不匹配时无法识别的条件。 | 本轮复读仍无完整导入/修复；编码、相对路径、重扫去重未知。FAQ“无规范”措辞不作为本项目标准事实判断。 | E-AK-10 |',
16:'| F-M065-16 | 返回播放页、曲内定位与来源导航（D2文档） | Mini Player展开/收起；拖进度、封面歌词、时长切换。More可去当前曲的Album/Artist/Folder或File Info；Songs/Albums/Artists/MQS按名称关键词搜索，结果可播/加入歌单。 | 视图/检索不构成持久断点。无歌词/空搜索反馈、精确粒度、章节、重启保存、原滚动位置未知；分类排序与首页次序只有入口，不补拖拽/保存规则。 | E-AKCD-01 |'}
lines=text.splitlines()
for k,new in replacements.items():
 idx=[i for i,l in enumerate(lines) if l.startswith(f'| F-M065-{k:02} |')];assert len(idx)==1;lines[idx[0]]=new
text='\n'.join(lines)+'\n'
extra='''| F-M065-17 | 创建、调整与同型号复制用户EQ（D2文档） | 长按通知EQ或Settings/Equalizer启用→Normal EQ/+→命名；62正文写SAVE，配图却为Rename EQ/CHANGE，确认标识差异保留。上下调频、数值微调，Advanced调Freq/Gain/Q。 | EQ Sync所得User EQ.dat可由电脑从SR35/Internal Storage/EQlists复制至另一SR35的EQlists；文件在设备列表不可见，电脑可见。最多显示20个用户EQ；手册限PCM 8–192kHz、8/16/24bit。参数保存时点、删预设/重名、跨固件合并及第三方app范围未知。 | E-AKCD-01；E-AKDD-03 |
| F-M065-18 | 校准选曲响度并启停ReplayGain（D2文档） | Albums/Playlists/Folders的曲目列表编辑→选曲→校准图标；手册称校准后曲目出现标记。通知栏或Settings/ReplayGain另开关；校准与启用不是同一个动作。 | 仅按手册24bit/192kHz以下条件，共用FAQ明确Line Out时不工作。音频是否被改写或状态仅存数据库、进度/取消/失败、重复校准/重扫后保留均未知；无测听。 | E-AKCD-01；E-AKCD-11；E-AKDD-03 |
'''
text=text.replace('\n\n## 版本、异常与数据边界','\n'+extra+'\n## 版本、异常与数据边界')
text=text.replace('更细的功能限制只按来源范围记录：ReplayGain共用FAQ说明Line Out时不工作；该项可定位，尚未建立校准失败/恢复任务。[E-AKCD-11]','F18已补校准入口与标记，失败/恢复仍缺；Line Out例外有共用FAQ依据，不能扩写成各音频路由/第三方app实测。[E-AKCD-11]')
text=text.replace('全部查于2026-10-26。原PDF','下表为2026-10-26采集记录；本批2026-10-27复读项及新增材料另列。原PDF')
text+='''
### 本批新增与复读

[正式短摘记](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-directory-deepening/source-notes.md)与[结构化证据](/home/workspace/reader/docs/research/interaction-library/batches/ak-directory-deepening-evidence.jsonl)。2026-10-27沿用E-AKCD-01本地原件，复读E-AK-12型号索引、E-AK-04日区V1.22、E-AK-10 M3U、E-AKCD-11 ReplayGain；复抓仅追加采集建议，不再创建同正文E。

| E | 来源及定位 | 证据边界 |
| --- | --- | --- |
| E-AKDD-01 | [封面显示与数据库更新FAQ](https://www.iriver.jp/support/faq_965.php) | 共用AK70及第二代后系列，SR35支持页关联；旧UI图片未查看，未当SR35现版。 |
| E-AKDD-02 | [封面大小FAQ](https://www.iriver.jp/support/faq_1035.php) | 共用容量/像素条件；非格式兼容或耗时测试。 |
| E-AKDD-03 | 同E-AKCD-01原PDF；[目录1](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-1.png)、[歌单40](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-40.png)、[校准41](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-41.png)、[EQ62](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-62.png)、[EQ63](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-directory-deepening/sr35-printed-63.png) | 五页本轮实际静态查看；文件页分别2/41/42/63/64。图文差异保留；没有实机验证。 |
'''
C.write_text(text)
manifest=[C,B/'ak-directory-deepening-evidence.jsonl',B/'ak-directory-deepening-coverage.csv',B/'ak-directory-deepening-inventory.csv',A/'source-notes.md']+[L/p for p in imgs]
dump(W/'formal-manifest.json',[{'path':str(p),'sha256':sha(p)} for p in manifest])
dump(W/'build-counts.json',dict(new_evidence=len(E),new_flows=len(F),old_flows=len(updates),old_fields=sum(len(u['changed_fields']) for u in updates),old_captures=len(add),inventory=len(I),domains=counts))
print(json.dumps(json.loads((W/'build-counts.json').read_text()),ensure_ascii=False))
