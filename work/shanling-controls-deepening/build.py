"""Assigned M063 research only; do not rerun after root merges."""
from pathlib import Path
import csv,json,hashlib,collections,difflib
w=Path('work/shanling-controls-deepening');lib=Path('docs/research/interaction-library');a=lib/'artifacts/shanling-controls-deepening';bp=lib/'batches/shanling-controls-deepening';date='2026-10-27'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,x):Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def writecsv(p,rs,fs):
 with Path(p).open('w',newline='') as f:r=csv.DictWriter(f,fieldnames=fs,lineterminator='\n');r.writeheader();r.writerows(rs)
caps=json.loads((w/'captures.json').read_text());cm={x['name']:x for x in caps}
notes='''# 山灵M1 Plus：本次来源范围与静态观察

查阅2026-10-27，hands_on=false；没有设备或App操作，没有连续演示。

- 固定M1 Plus非Android MTouch，旧英文V2.1发布基线；中国站/download/134仍为V1.8，本次HTTP200。两页面不证明设备安装版本。M1快速指南V1.0为文档修订号，24页原件本地复读，没有再次下载。
- E-SLCD-01官网Eddict下载页本次列Android2.3.7、发布日期2026-09-11；其SyncLink名单不列M1 Plus，与M1产品介绍明示支持SyncLink并列保留，不能将名单缺项推成不支持。App页面的云下载/播放队列是App能力，不替换M1设备能力。
- E-SLCD-02为该下载页“说明书”链接指向的官方PDF，16页、末页V1.2；未标日期，也未声明对应Android2.3.7或某个iOS构建。p2自述不是单一型号手册；中文p9的定时/记忆等列表须结合英文p15：播放设置只影响App本地播放，不影响连接的山灵设备。
- SyncLink中文p6写设备控制入口、开控制端搜索并连接、成功提示，iOS只能控制端；英文p12另写必要时Music Scan，以及BT/Wi-Fi同时开启。后者不可直接移植到旧M1 Plus评测所报无线互斥的环境。共用图/文不是M1型号绑定测试。
- Wi-Fi传歌中文p5的“电脑传歌”和“手机传歌”步骤重复；英文p12才给电脑浏览器访问显示地址。均是App接收界面，不能据此填M1自身Wi-Fi传歌的地址、落盘、取消/失败和完成状态。
- Eddict SyncLink、DLNA的媒体渲染器选择、无线投屏是不同控制范围；p13把投屏对象写为streamer。没有证据将M1 Plus补作1:1投屏。
- HiFi Oasis旧E-SHCD-03复抓HTTP200；作者自报V2.1。仅用于型号菜单台账和既有F01/10补分支，未把商业评测升级为官方确认/实测，未查看其图片/视频。
- 普通GET对英文MTouch旧帮助/article/101376、Eddict旧介绍/article/101309、M1下载/download/118返回403，均停止；搜索索引先前可见不能当本次完整页面原件。未改地址绕过，也未下载App、固件或媒体。

## E-SLCD-03静态观察（源自E-SLCD-02）

实际view_image查看PDF页12、15（印刷页相同）。p12可见SyncLink Controller、Connected/Paired/Available分组，但示例已连接的是SCD3.3，列表有EH3/H5/M9 Plus，**不是M1 Plus截图**。右上Wi-Fi Transfer是App界面，示例局域网地址不是本设备可复用地址。p15可见Timed off/Resume mode等App设置，同时正文明确不影响连接设备；下方USB权限弹窗示例为UA7，不是M1 DAC连接验证。按钮、开关位置只有单帧证据，未触发成功/失败。

原件及SHA见work/shanling-controls-deepening/captures.json；正式短摘记/图片SHA见批次E与formal-manifest.json。M1旧图仍按E-SHCD-01旧观察保留，本次没有重复计为新观察。
'''
(a/'source-notes.md').write_text(notes)
scope='M1 Plus非Android MTouch；Eddict伴随App资料仅限定其本地/遥控范围；不能替代M1固件菜单或当前设备实测'
E=[]
for n,name,title,typ,loc in [(1,'eddict-download','Eddict官方下载页：Android版本及能力/机型名单边界','official_release_notes','Android2.3.7/2026-09-11；说明书链接；SyncLink机型名单'),(2,'eddict-manual','Eddict官方使用说明书V1.2：本地设置与遥控的作用范围','official_manual','PDF16页；p2范围、p6/12 SyncLink、p9/15设置、p5/12 Wi-Fi传歌；p16文档V1.2')]:
 c=cm[name];x=dict(evidence_id=f'E-SLCD-{n:02d}',product_id='M-063',title=title,url=c['url'],checked_on=date,source_type=typ,access_mode='anonymous_public_pdf_GET_and_local_text' if n==2 else 'anonymous_public_html_GET',version_scope=scope+('；PDF文档V1.2，发布日期及绑定App构建未标' if n==2 else '；Android下载页列2.3.7，不推定已安装或iOS构建'),app_build=None,firmware_version=None,source_updated_on='2026-09-11' if n==1 else None,locator=loc,note='完整官方伴随App资料；仅case限定范围；SyncLink已连接设备与手机本地设置分开。',hands_on=False,visual_observed=False,raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],archived_artifact='artifacts/shanling-controls-deepening/source-notes.md',sha256=sha(a/'source-notes.md'),capture_state='raw_source_in_work_and_short_note_archived',supersedes=None)
 if n==1:x.update({k:c[k] for k in ['extracted_work_path','extracted_sha256']})
 else:x.update(extracted_work_path=str(w/'eddict-pages.json'),extracted_sha256=sha(w/'eddict-pages.json'))
 E.append(x)
rs=json.loads((w/'rendered.json').read_text());paths=[str(Path(x['path']).relative_to(lib)) for x in rs]
E.append(dict(evidence_id='E-SLCD-03',product_id='M-063',title='Eddict文档V1.2 p12/15静态观察：非M1示例与App本地设置',url=cm['eddict-manual']['url'],checked_on=date,source_type='official_manual',access_mode='local_pdf_page_render_viewed',version_scope=scope+'；PDF文档V1.2，示例含SCD3.3/UA7，不是M1 Plus界面',app_build=None,firmware_version=None,source_updated_on=None,locator='PDF/印刷p12、15，两张单帧实际view_image',note='用于F08范围辨别，不证明M1配对成功或UI一致。',hands_on=False,visual_observed=True,raw_work_path=cm['eddict-manual']['raw_work_path'],raw_sha256=cm['eddict-manual']['raw_sha256'],archived_artifact=paths[0],sha256=rs[0]['sha256'],rendered_artifacts=paths,artifact_hashes={p:r['sha256'] for p,r in zip(paths,rs)},capture_state='selected_static_page_excerpt',derived_from='E-SLCD-02',supersedes=None))
Path(str(bp)+'-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
old=json.loads((w/'coverage-before.json').read_text());fields=list(old[0]);updates=[]
mods={1:dict(evidence_ids='E-SHAN-01;E-SHCD-01;E-SHCD-03',known_branches='文件传入后仍需扫描；HiFi Oasis作者自报V2.1并描述可选指定文件夹扫描，这是评测补证，非官方过滤规则。',gaps='自动扫描、空卡、重名、路径移动、取消/完成提示及扫描过滤/上限缺当前官方步骤。'),8:dict(evidence_ids='E-SHAN-02;E-SHAN-04;E-SHCD-04;E-SLCD-01;E-SLCD-02;E-SLCD-03',known_branches='M1蓝牙D2仍基于作者链；共用Eddict手册补手机控制端搜索/连接成功反馈、iOS仅控制端。p15明确App播放设置不影响连接设备，不能用手机定时/记忆替代M1设置；SyncLink与DLNA/投屏分开。',gaps='Eddict文档V1.2不等于Android2.3.7；名单漏M1不等于不支持。英文BT/Wi-Fi同时开与评测互斥未解；M1权限、失败、退出/中断、服务账户/离线仍缺。两静态页为其他机型示例。',visual_status='static_reference_only'),10:dict(known_branches='作者自报V2.1：A:\\音频引用、_explaylist_data及Import playlist；另描述可导出，歌单按创建次序、后添在末尾。此为评测版本口径，非当前固件排序保证。',gaps='导出入口/文件名/重导冲突、相对路径/M3U8、失效项和换卡未核；无删除对象确认或撤销，不能用Eddict手机队列能力补成M1队列。')}
for n,changes in mods.items():
 b=old[n-1];af=dict(b,**changes,checked_on=date);updates.append(dict(flow_id=b['flow_id'],before=b,after=af,changed_fields={k:{'before':b[k],'after':af[k]} for k in b if b[k]!=af[k]}))
dump(w/'old-coverage-updates.json',updates)
writecsv(str(bp)+'-coverage.csv',[],fields)
# Functional inventory: physical chapter headings, not synthetic outline page labels.
inv=[];manual='https://shanling.com/uploads/image/M1_Plus-V1.0.pdf';app=cm['eddict-manual']['url'];review=cm['review']['url']
def add(domain,item,url,loc,e,flows='',status='catalogued_only',gap='未闭合该项全部入口/反馈/失败，目录不等于任务分析'):
 inv.append(dict(inventory_id=f'I-SLCD-{len(inv)+1:03d}',domain=domain,item=item,source_url=url,locator=loc,coverage_status=status,evidence_ids=e,flow_ids=flows,gap=gap))
headings=[('封面/版本名','1'),('安全指示','3；英10；日17'),('环保支持','4'),('部件名称','5；英11；日18'),('操作及接口说明','6；英12；日19'),('开机/关机/锁屏','6；英12；日19'),('上一曲/下一曲/播放及暂停','6；英12；日19'),('音量控制','6；英12；日19'),('充电','6；英13；日20'),('MicroSD卡插口','7；英13；日20'),('耳机输出接口','7；英13；日20'),('USB接口','7；英13–14；日20–21'),('数据传输至MicroSD卡','7；英13–14；日20–21'),('作为USB DAC','7；英14；日21'),('USB驱动下载入口','7；英14；日21'),('固件升级前提','8；英14；日21'),('OTA在线升级','8；英14；日22'),('MicroSD卡升级','8；英15；日22'),('常见问题','9；英15；日22–23'),('随机附件（含保修卡）','9；英16；日23'),('技术证明（日文另列）','23'),('厂商/联系/变更声明','24')]
for i,(t,p) in enumerate(headings):
 flow={5:2,6:2,7:2,12:1,13:3,16:5,17:4,18:6}.get(i);add('M1快速指南章节（合并译文定位）',t,manual,'PDF文件页'+p,'E-SHCD-01',f'F-M063-{flow:02d}' if flow else '', 'task_partially_analysed' if flow else 'body_read_not_closed')
for t in ['MicroSD卡槽','USB-C数据口','氛围灯','触摸屏','电源/锁屏与音量旋钮组','上一曲键','播放/暂停键','下一曲键','3.5mm耳机/线路/同轴输出','4.4mm平衡输出','3.5mm同轴插头接线']:
 add('M1部件/接口标签',t,manual,'PDF文件页5/11/18','E-SHCD-01',gap='旧p5图已观察；本批仅文本复读，不新增观察')
for t in ['适配器规格','电量低/耗尽无法开机','无响应长按强制关闭','耳机杂音/插头接触','无声/线缆耳机与音量检查']:
 add('M1官方FAQ条目',t,manual,'PDF文件页9/15/22–23','E-SHCD-01','F-M063-06' if t.startswith('无响应') else '', 'task_partially_analysed' if t.startswith('无响应') else 'body_read_not_closed')
menus={'主屏与分类':['主屏显隐/重置','Now Playing','All Songs','Folder','Artist/Album Artist','Playlist','Update Library','CD Ripper'], '播放设置':['Gain','Filter','EQ','Play mode','Max/Default volume','Volume memory','USB DAC volume','Memory playback: Off/Track/Position','Gapless','ReplayGain','Digital output','Channel balance','Folder Album Cover','Folder Skip','Enlarged Album Art'], '系统设置':['Language','Wireless/Bluetooth','SyncLink','Network Services: DLNA/AirPlay/WiFi transfer','Display/Atmosphere Lamp','Idle: Sleep Timer','Idle: Auto-standby','Idle: Auto-Shutdown','Vehicle mode','Tap Screen to Activate','Button lock','Screen Rotation','Theme','System Update','Card Format','Reset']}
for domain,ts in menus.items():
 for t in ts:
  fs={'Now Playing':'F-M063-09','Artist/Album Artist':'F-M063-09','Playlist':'F-M063-10','Update Library':'F-M063-01','SyncLink':'F-M063-08','System Update':'F-M063-04;F-M063-05','Card Format':'F-M063-07','Button lock':'F-M063-02'}.get(t,'')
  add('M1评测目录/'+domain,t,review,'MTouch OS and UI；作者自报V2.1，截图未逐项核版','E-SHCD-03',fs,'task_partially_analysed' if fs else 'third_party_inventory_only','作者菜单记录；不是官方完整设置表；断点/计时只有名称及部分说明，不升D2')
appheads=[('封面与版本','1/16'),('型号/版本适用说明','2'),('介绍/软件概述/目标','3；英10'),('功能特性','3；英10'),('注册登录（中文）/Installation（英文）','4/11'),('无损音乐/文件解析与传输','4；英11'),('扫描入库/直接文件夹播放','4；英11'),('本地音乐分类','4；英11'),('播放界面','4；英11'),('云音乐服务','4–5；英11'),('索尼精选套餐（仅文档历史描述）','5'),('Wi-Fi传歌：电脑','5；英12'),('Wi-Fi传歌：App间','5；英12'),('SyncLink','6；英12'),('无线投屏','6；英13'),('USB耳放控制','7；英13'),('NAS','7；英14'),('蓝牙耳放控制','8；英14'),('DLNA推歌','8；英14'),('播放设置','9；英15'),('USB独占','9；英15')]
for t,p in appheads:
 add('Eddict共用手册章节（非M1功能声明）',t,app,'PDF/印刷页'+p,'E-SLCD-02','F-M063-08' if t in ['SyncLink','播放设置','型号/版本适用说明'] else '', 'scope_boundary_read','App文档V1.2；未指定M1，功能不能直接移植；未注册/安装/登录')
for t in ['扫描音乐','定时关闭','记忆播放','自动进入播放页','文件夹跳曲','专辑跳曲','淡入淡出','回放增益','均衡器','USB输出','固定采样率','拔耳机暂停']:
 add('Eddict本地播放设置（对连接设备无影响）',t,app,'PDF/印刷p9、15','E-SLCD-02;E-SLCD-03','F-M063-08','scope_boundary_read','p15明确仅App本地；不能填M1定时/断点/输出能力')
for t in ['Android2.3.7/2026-09-11','说明书下载入口','SyncLink兼容名单（未列M1 Plus）','投屏/蓝牙耳放/USB耳放分组名单']:
 add('Eddict下载页范围',t,cm['eddict-download']['url'],'正文版本及功能/机型分组','E-SLCD-01','F-M063-08','scope_boundary_read','名单缺项不等于不支持；不是M1固件清单或已装App版本')
writecsv(str(bp)+'-inventory.csv',inv,list(inv[0]))
# append captures: preserve originals, don't mutate master.
oldE=json.loads((w/'evidence-before.json').read_text());em={x['evidence_id']:x for x in oldE};uc=[]
for eid,name in [('E-SHCD-01',None),('E-SHCD-02','m1plus-v18'),('E-SHCD-03','review')]:
 c=dict(checked_on=date,access_mode='existing_pdf_reread_PyMuPDF' if name is None else 'anonymous_public_html_GET',hands_on=False,visual_observed=False,archived_artifact='artifacts/shanling-controls-deepening/source-notes.md',sha256=sha(a/'source-notes.md'))
 if name is None:c.update(raw_work_path='work/hifi-controls-deepening/M1_Plus-V1.0.pdf',raw_sha256=sha('work/hifi-controls-deepening/M1_Plus-V1.0.pdf'),locator='24页完整文本复读；本批不重复旧5/6页观察；目录台账按实际章节，不按PDF页N书签充数')
 else:c.update({k:v for k,v in cm[name].items() if k in ['url','status','raw_work_path','raw_sha256','extracted_work_path','extracted_sha256']})
 b=em[eid].get('additional_captures',[]);uc.append(dict(evidence_id=eid,field='additional_captures',before=b,after=b+[c],append=c))
dump(w/'old-evidence-additional-captures.json',uc)
dump(w/'counts.json',dict(new_E=len(E),new_F=0,total_case_F=10,old_F_rows=len(updates),old_F_fields=sum(len(x['changed_fields']) for x in updates),old_E_captures=len(uc),inventory_rows=len(inv),inventory_domains=dict(collections.Counter(x['domain'] for x in inv))))
print((w/'counts.json').read_text())
