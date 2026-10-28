from pathlib import Path
import json,csv,re,hashlib
W=Path('work/fiio-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches';A=L/'artifacts/fiio-controls-deepening';C=L/'cases/M-034-fiio.md'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest=json.loads((W/'source-manifest.json').read_text());assert len(manifest)==14
old=json.loads((W/'old-coverage-before.json').read_text());oldby={r['flow_id']:r for r in old}
pdfurl='https://fiio-user-manual.oss-cn-hangzhou.aliyuncs.com/EN/JM21%20complete%20user%20manual.pdf'
new=[
(10,'将外部M3U歌单转成应用内歌单','电脑已有歌单编辑工具及同一组音频；JM21处于存储连接；不是安装或实操记录','官方示例用foobar保存M3U，再把电脑盘符改成机内/storage/emulated/0/或卡/storage/external_sd/，反斜线改正斜线；歌单放在对应介质根目录。断开电脑后从FiiO Music的Local music → Folder打开M3U，点右上勾选 → Select all → 歌单图标 → 选已有或新建歌单，返回Playlist检查。','外部文件先从Folder访问，和应用内Playlist分开；导入是把成员加入内部歌单，并未证明编辑会回写原M3U。原文另列M11双卡路径，不用于JM21。','E-FICD-09','标题明确含JM21，但页面含旧图，具体App构建未给出；实际JM21挂载路径、错路径提示、重复导入、重名、移卡恢复及导出回写未知。'),
(11,'接收电脑USB音频作为DAC并退出','Windows已具备官方DAC驱动；Mac按JM21 FAQ免驱；此处未下载或安装','下拉模式或Settings → Mode choose → USB DAC → USB接电脑 → 在电脑及播放软件选择JM21音频设备。完成后按F04的屏上模式选择或Settings → Exit回Android。','这是电脑向JM21送音频，不是F06的JM21向外置DAC输出。USB DAC时不支持蓝牙发射；官方提醒有延迟，不建议用于看视频。','E-FICD-04;E-FICD-03;E-FICD-12','英文FAQ/手册把电脑端选择项写成audio input，方向措辞可疑，未冒充已确认的Windows输出菜单；失败提示、原曲/队列/音量恢复和退出后自动发声未核。'),
(12,'接收iOS端AirPlay音频并返回本地模式','JM21与iOS设备同一Wi-Fi；以此型号FAQ支持范围为限','JM21切AirPlay模式；在iOS下拉菜单选择JM21为播放设备。结束时按F04模式入口或Settings → Exit回Android。','音乐来源在iOS设备；JM21接收音频，未描述把文件存入本地曲库。FAQ明确此路径面向iOS而非Android。','E-FICD-06;E-FICD-03','找不到设备、密码/权限、路由器隔离、断网/锁屏中断、播放点和退出后的恢复未知；iOS版与具体菜单外观未固定。'),
(13,'给本地歌曲配LRC并调节歌词显示','音频及同名LRC在同一文件夹；曲目可播放','FAQ要求LRC与音乐同名同目录；播放页点中央封面到歌词。手册说明可调歌词时间轴和字号；p33静态图可见Shift ±0.5s、Restore、Zoom in/out、Cancel。','歌词文件与播放进度是不同对象；图中还可见No lyrics found状态，仅证明提示/控件在示例出现。','E-FICD-10;E-FICD-15','未操作偏移、取消、恢复；是否改写LRC、每曲保存偏移、编码错误、退出后保持及3.3.4新播放页对应入口未知。'),
(14,'更新JM21固件并区分包错误与启动恢复','已确认JM21专用包；在线路径需网络，手册要求电量高于30%；未下载或执行固件','在线从通知或Technical Support → Firmware Upgrade → Check for updates进入；本地则将未解压包置内存或卡，选Local Upgrade定位文件。','FAQ列升级后自动重启；在线报invalid image时，建议删除内存下原文所称“android-data-com.android.fiioupdate-files”目录后重试，也可改本地更新。包损坏需重新取得包；升级后卡启动可按F09强制重启，Recovery先选reboot system now与清除数据分开。','E-FICD-11;E-FICD-15;E-FIIO-07','未核包校验、签名/降级、成功验收、配置和歌单保留；官方耗时与恢复建议非实测，重试不保证成功。'),
(15,'将手机蓝牙音频送入JM21并尝试恢复连接','手机为发射端，JM21为接收端；区别F05连接蓝牙耳机','JM21下拉或Settings切Bluetooth receiving；手机蓝牙设备列表选择JM21。若连接失败，FAQ建议双方蓝牙关后重开再配对连接；结束用屏上模式入口或Settings → Exit回Android。','改变的是音频输入角色；不同于FiiO Link遥控，也不表示把手机音频文件复制进曲库。','E-FICD-05;E-FICD-03','重配为官方建议，未验证成功率；手机音量映射、接收界面反馈、通话打断、断连暂停和回本地后的断点未知。')
]
updates={
1:dict(evidence_ids='E-FIIO-02;E-FICD-08;E-FICD-15',known_branches='文件传入不等于索引完成。卡插入方向按JM21 FAQ为触点向下、推入锁定，再推弹出；不能套用Sony方向。手册空库图出现Scan for music入口，但本轮没有扫描完成流程。',gaps='扫描过滤/增量/移动或重复文件策略、弹卡后的队列及歌单状态、当前macOS兼容性、传输取消与索引失败。'),
2:dict(evidence_ids='E-FIIO-01;E-FIIO-11;E-FICD-15',known_branches='手册给出添加时间/文件名/首字母/年份排序；行菜单图可见Play next（当前曲后播放）与Cancel，批量图可见Select all和Cancel。静态入口不证明取消回滚、重排、持久化或删除对象范围。',gaps='扫描过滤、完整队列插入/替换/重排/去重、删除确认/是否删文件、撤销；旧手册界面不当作3.3.4已验证界面。',visual_status='static_manual_pages_only'),
4:dict(evidence_ids='E-FIIO-03;E-FICD-03',known_branches='五种工作角色总入口；非Android模式的屏上选择或Settings→Exit返回Android。USB-DAC、AirPlay和蓝牙接收的具体音频目标分别见F11/F12/F15，未作为此处重复任务。',gaps='切换时原曲位置、队列、连接和音量是否保留；退出是否立即出声。'),
6:dict(evidence_ids='E-FIIO-05;E-FIIO-10;E-FICD-14;E-FICD-15',known_branches='USB输出开关与端口角色影响发声；手册描述蓝牙发射与模拟/同轴间遵循后接入优先。两个模拟口仅写按插入顺序优先，未擅自补先/后规则。LO音量可调开关关闭时为最大固定音量。',gaps='切出口音量继承、两个模拟口的明确优先顺序、热插拔瞬间声音、失败UI提示和自动回退；未实测手册优先级。'),
7:dict(evidence_ids='E-FIIO-08;E-FIIO-09;E-FICD-07;E-FICD-13',known_branches='Link是Music遥控，DLNA是远端媒体；另有JM21 FiiO Cast与手机FiiO Control连接路径，失联排查网络/后台服务/IP后可删除旧设备、重启Cast、重加。型号旧FAQ仅接收的DLNA表述与通用App实验发射日志并存，不能断言当前JM21不支持发射。',gaps='当前JM21构建DLNA发射入口和支持范围、Cast退出/授权/多客户端/断点、Wi-Fi传歌目标目录与失败处理、远端缓存与队列冲突。'),
8:dict(evidence_ids='E-FIIO-06;E-FIIO-11;E-FICD-15',known_branches='系统关机与App停止/记曲目不同；手册Audio的Auto pause without headphones仅列Android/Pure Music模式无耳机2分钟后暂停，不能推成立即拔耳机暂停或蓝牙断连规则。',gaps='App timer当前入口、换曲是否延时、计时重启保留、逐曲时间点与断电保存；无耳机暂停的计时起点/多出口判定未核。'),
9:dict(evidence_ids='E-FIIO-07;E-FICD-15',known_branches='重启、清App数据、出厂清除分层。手册System→Reset options另列网络重置、App偏好重置、清全部内存数据；不能以清数据替代扫描。',gaps='日志敏感信息、偏好重置完整影响、歌单/进度备份恢复、断电后索引损坏与清除失败未核。')
}
limits=[
'23项型号FAQ目录；不是23个任务完成。',
'官方手册/驱动/固件入口；只读链接，没有安装或执行。',
'模式总入口及Exit；不能推出原播放自动恢复。',
'电脑USB-DAC路径；原文audio input方向措辞保留疑点。',
'手机发射/JM21接收；重配仅为建议。',
'iOS同网AirPlay接收；不扩展Android或离线下载。',
'Cast/Control与Link/Music分开；页中指向M33的视频未访问。',
'本机卡槽触点向下；无逻辑卸载/数据保留说明。',
'正文点名JM21；旧示例图和M11双卡例外不当JM21实机。',
'LRC同名同目录；Music语言路径写ME，和旧手册Settings措辞不同。',
'升级失败建议不是成功保证；本轮不下载任何更新包。',
'固定原版3GB/32GB Android13文档对象，4GB/64GB另列；宣传性能不作测试。',
'官方FIIO账号发布，JM21分支3.3.4及历史日志；不是已安装版本或整机固件。',
'LO音量开关与输出；ES9038Q2M滤波器文案疑似复用，保留冲突。']
notes=['# JM21 本轮来源与边界','', '查阅2026-10-27。研究装配限定JM21原版3GB/32GB、Android13；官网另有4GB/64GB，不能混用续航/可用内存。42页手册无修订号，设备固件及实际预装Music构建仍未知。2026-09-11发布页列3.3.4的JM21适配分支并说明播放页/EQ重做；不把旧图冒充此版实机。','', '| E | 使用范围/限制 | 原件定位 |','| --- | --- | --- |']
for i,r in enumerate(manifest,1):notes.append(f"| E-FICD-{i:02} | {limits[i-1]} | `{r['path']}` |")
notes+=['| E-FICD-15 | 同一JM21官方手册：目录p3–4；实际查看p29/30/31/33，仅静态。 | `work/fiio-controls-deepening/raw/jm21-existing.pdf` |','', '目录按45个印刷目录条目（含章标题）逐项登记，另登记正文小节及23项FAQ；重叠条目不是新增功能/任务。','', '扫描过滤与Wi-Fi传歌：搜索命中的`https://fiio.com/newsinfo/69487.html?s=createtime%2Casc`原文点名X5，未作为JM21步骤；检索快照在`work/fiio-controls-deepening/discovery-search.json`。JM21手册图只证实Scan for music与WiFi music transfer入口，V3.2.8日志仅证实文件夹传输被加入，尚无当前构建完整链。','', '手册图p33有均衡器/PEQ相关标签，但不能据此补出完整PEQ操作。2022通用日志列实验DLNA发射，与JM21旧FAQ“只接收”存在时间/分支差异，保留待核；没有把其他机型截图、视频或旧X系列步骤套入。','', '原PDF SHA256：`'+sha(W/'raw/jm21-existing.pdf')+'`。所有原件和摘页散列见本批证据与work清单。没有实机、连续观察、登录、配对、传歌或更新。']
(A/'source-notes.md').write_text('\n'.join(notes)+'\n')
# Evidence
headers={1:'JM21官方FAQ目录（23项）',2:'JM21官方资料入口',3:'JM21五种工作模式',4:'JM21 USB DAC',5:'JM21蓝牙接收',6:'JM21 AirPlay',7:'JM21 FiiO Cast',8:'JM21 microSD插入/弹出',9:'外部M3U创建和内部歌单导入（含JM21）',10:'JM21语言/歌词/截图',11:'JM21固件升级与失败分支',12:'JM21产品装配范围',13:'FiiO Music官方发布说明与JM21适配分支',14:'JM21 Audio菜单'}
loc={1:'FAQ 1–23；目录日期2024-12-11至2025-07-29',2:'User manual / USB DAC driver / Firmware三个链接',3:'五角色与非Android退出；手册p8–9',4:'Mac/Windows步骤及蓝牙发射限制；手册p13',5:'三段：进入、手机连接、失败重配；手册p19',6:'iOS三步骤；手册p9只作功能简介',7:'连接四步骤及remote connection failures；页面链接M33视频未访问',8:'插入与弹出四条；连接指触点方向',9:'2025-06-12修改；Detailed operation及How to import',10:'Lyrics display；语言ME路径；手册p12–13/32–33/36',11:'Online/Local upgrade及问题1–4；手册p11–12/39–41',12:'RAM and Storage / Android / USB DAC备注',13:'FIIO署名；2026-09-11 V3.3.4、2025-06-12 V3.2.8、2022-12-30实验DLNA；未下载APK',14:'9.Adjustable volume at LO；芯片文案冲突；手册p20–23'}
e=[]
for i,r in enumerate(manifest,1):
 ex=W/'extracted'/f"{r['name']}.txt";assert ex.exists()
 date='2024-12-11' if i in [2,3,4,5,6,10,11,14] else ('2025-07-05' if i==7 else ('2025-07-29' if i==8 else ('2025-06-12' if i==9 else None)))
 e.append(dict(evidence_id=f'E-FICD-{i:02}',product_id='M-034',title=headers[i],url=r['url'],checked_on='2026-10-27',source_type='official_help_index' if i==1 else ('official_release_notes' if i==13 else ('official_product_page' if i==12 else 'official_help')),access_mode='official_https_html_read',version_scope='JM21 original 3GB/32GB Android13 documentary baseline; hardware firmware and installed Music build unknown; source release ranges stated separately; no other-model generalization',app_build=None,firmware_version=None,source_updated_on=date if i==9 else None,source_index_date=date if i!=9 else None,date_basis='Freshdesk modified date' if i==9 else 'JM21 FAQ index date where present; page update timestamp not established',locator=loc[i],note=limits[i-1],hands_on=False,visual_observed=False,archived_artifact='artifacts/fiio-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),capture_state='http_200_original_html',supersedes=None,raw_work_path=r['path'],raw_sha256=r['sha256'],extracted_work_path=str(ex),extracted_sha256=sha(ex)))
arts=[f'artifacts/fiio-controls-deepening/manual-p{n:03}.png' for n in [29,30,31,33]]
e.append(dict(evidence_id='E-FICD-15',product_id='M-034',title='JM21完整手册复读与四页静态摘页',url=pdfurl,checked_on='2026-10-27',source_type='official_manual',access_mode='existing_pdf_text_read_and_static_view',version_scope='JM21 42-page English complete manual; no revision or Music build; original 3GB/32GB Android13 research scope',app_build=None,firmware_version=None,source_updated_on=None,locator='目录p3–4；正文p6/23/28/29–34/39–42；实际看图仅p29/30/31/33',note='四页同属一本来源；前次文字证据补图，不计连续流程；和Music3.3.4新UI分开。',hands_on=False,visual_observed=True,archived_artifact=arts[0],sha256=sha(L/arts[0]),rendered_artifacts=arts,artifact_hashes={p:sha(L/p) for p in arts},capture_state='existing_original_pdf_reused',supersedes=None,qualifies=['E-FIIO-01'],raw_work_path=str(W/'raw/jm21-existing.pdf'),raw_sha256=sha(W/'raw/jm21-existing.pdf'),extracted_work_path=str(W/'extracted/manual-pages.json'),extracted_sha256=sha(W/'extracted/manual-pages.json'),visual_kind='static_manual_pages',visual_scope='four_static_printed_pages_not_continuous',observed_pages=[29,30,31,33]))
(B/'fiio-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in e))
coverage=[]
for n,t,p,o,d,ids,g in new:coverage.append(dict(flow_id=f'F-M034-{n:02}',product_id='M-034',task=t,depth='D2',evidence_ids=ids,known_branches=d,gaps=g,visual_status='static_manual_pages_only' if n==13 else 'not_observed',hands_on='false',case_path='cases/M-034-fiio.md',checked_on='2026-10-27'))
with (B/'fiio-controls-deepening-coverage.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=list(old[0]),lineterminator='\n');wr.writeheader();wr.writerows(coverage)
changes=[]
for n,data in updates.items():
 before=oldby[f'F-M034-{n:02}'];after={**before,**data,'checked_on':'2026-10-27'};changes.append({'flow_id':before['flow_id'],'before':before,'after':after,'changed_fields':{k:{'before':before[k],'after':after[k]} for k in before if before[k]!=after[k]}})
(W/'old-coverage-updates.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2)+'\n')
# Inventory: original table-of-contents entries preserved, then uncatalogued body headings, then model FAQ.
toc=json.loads((W/'manual-toc-items.json').read_text());faq=json.loads((W/'faq-index-items.json').read_text());assert len(toc)==45 and len(faq)==23
mt={1:[3,6],2:[3],3:[3,6],6:[4],7:[1],8:[14],11:[11],12:[5],16:[5,15],17:[5],18:[15],19:[6,8],22:[5],25:[6],27:[3,8],31:[9],32:[2,13],35:[6],36:[7],37:[7],38:[7],39:[6],40:[9],42:[14],43:[9],44:[9],45:[9,14]}
inv=[]
def add(domain,title,url,loc,flows,ids,status=None,gap=None):
 inv.append(dict(inventory_id=f'I-M034-{len(inv)+1:03}',domain=domain,item=title,source_url=url,locator=loc,article_url=url,coverage_status=status or ('partial_documented' if flows else 'directory_only_not_task_complete'),evidence_ids=ids,flow_ids=';'.join(f'F-M034-{n:02}' for n in flows),gap=gap or ('仅关联F明确范围；不表示整章任务/异常完成。' if flows else '目录定位；未建立完整任务，不提升D2。')))
for i,r in enumerate(toc,1):add('手册印刷目录（含章标题）',r['title'],pdfurl,f"目录p{r['toc_page']}；正文起始p{r['page']}",mt.get(i,[]),'E-FICD-15')
body=[('Audio','Output selection',20,[6]),('Audio','SPDIF out',20,[6]),('Audio','Auto pause without headphones',23,[8]),('Display','Sample rate display',23,[]),('Display','Dark theme',24,[]),('Display','Brightness level',24,[]),('Display','Indicator light control',24,[]),('Display','Night light',24,[]),('Display','Screen timeout',24,[]),('Display','Font size',25,[]),('Display','Display size',25,[]),('Display','Screen saver',25,[]),('Display','Lock screen',25,[]),('Global','System navigation',26,[]),('Global','Sleep / Idle poweroff',26,[8]),('Global','Key-lock settings',26,[3]),('Global','Volume setting',26,[8]),('Global','Rotate screen',26,[]),('System','Reset WiFi, mobile & Bluetooth',28,[9]),('System','Reset app preferences',28,[9]),('System','Erase all data',28,[9]),('Music','Homepage / Quickly locate all songs',29,[2]),('Music','Hidden menu',30,[2]),('Music','Sort songs',30,[2]),('Music','Bulk management',31,[2]),('Music','Playing page / Switch page / Playing progress',32,[2,13]),('Music','Equalizer / Reset EQ',33,[]),('Music','UV meter',34,[]),('Music','Frequency spectrum / Square cover and large cover',35,[])]
for dom,t,p,fs in body:add('手册正文补充 / '+dom,t,pdfurl,f'印刷p{p}',fs,'E-FICD-15',status='partial_documented' if fs else 'read_description_only_not_task_complete')
fmap={2:[9],3:[1],4:[14],6:[4],7:[11],8:[15],9:[5],10:[6],11:[6],13:[7],14:[12],15:[7],16:[6],18:[3,8],19:[13],22:[7],23:[1]}
oldrefs={2:7,3:2,9:4,10:5,11:10,13:8,15:9,18:6}
newurls={r['url']:r['evidence_id'] for r in e}
for i,r in enumerate(faq,1):
 url='https://www.fiio.com'+r['url'];eid=newurls.get(url) or (f'E-FIIO-{oldrefs[i]:02}' if i in oldrefs else '')
 add('JM21官方FAQ',r['title'],url,f'jm21_faq第{i}项',fmap.get(i,[]),'E-FICD-01'+(';' + eid if eid else ''))
with (B/'fiio-controls-deepening-inventory.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');wr.writeheader();wr.writerows(inv)
# Case: keep prior analysis and explicitly date the historical gap table.
s=(W/'case-before.md').read_text()
s=s.replace('查阅日期：2026-10-26。对象为 **JM21 的 Android 系统及预装 FiiO Music**；','查阅日期：2026-10-26；本轮深化2026-10-27。研究装配固定为 **原版JM21 3GB/32GB、Android13及预装FiiO Music**（文档范围，不是实机样本）；',1)
s=s.replace('官网现在同时列 3GB/32GB 与 4GB/64GB 规格，不把不同装配当成同一测试机。','官网另列4GB/64GB，二者的续航/电池/可用存储不混用。')
s=s.replace('深度为 **D2 文档重建**。已查文件传入、曲库入口、批量操作、播放控制、模式、蓝牙/USB/模拟输出、遥控、定时和故障恢复。未完成当前构建全部菜单核定，尤其扫描选项、临时队列、逐书断点和删除确认。手册图仅尝试获取，未形成可审阅的界面视觉记录；不记连续观察。','深度为 **D2 文档重建**。现有15F；本轮新增外部M3U转内部歌单、电脑USB-DAC、AirPlay、歌词、升级及蓝牙音频接收，旧任务补回原号。实际查看手册p29/30/31/33的静态页；无连续观察或实机操作。扫描过滤、完整临时队列、逐书断点和删除确认仍未闭合。\n\n官方FIIO发布页于2026-09-11列出Music **V3.3.4的JM21适配分支**，并说明播放页和均衡器重做。它是可查发布版本，不能当作本案例已安装版本；旧手册图片也不能冒充3.3.4画面。V3.2.8日志列WiFi文件夹传输，但没有当前端到端操作。本轮不执行更新。[E-FICD-12、E-FICD-13]')
s=s.replace('E-FIIO-01 继续作为同一本手册的依据，不重复建立证据或增加 F。当前固件全部菜单、第三方 app 及其账户服务仍未核定。','下表保留2026-10-26历史基线；其中“未覆盖”不是本轮结论，以下更新表和逐项台账为准。当前固件全部菜单、第三方app及其账户服务仍未核定。')
insert='''\n2026-10-27逐项台账见 [本轮目录CSV](../batches/fiio-controls-deepening-inventory.csv)：45个手册印刷目录条目（含章标题）、29个正文补充小节、23项型号FAQ，共97条来源入口；同功能重叠不合并算成新任务。\n\n| 功能域 | 本轮已补 | 仍待核 |\n| --- | --- | --- |\n| 曲库/队列/歌单 | F02排序、行菜单Play next及批量取消的静态入口；F10外部M3U转内部歌单 | Scan for music仅见空库按钮；过滤/增量/停止扫描、完整队列重排及删除确认未知。 |\n| 歌词/设置 | F13同名LRC和歌词显示；F06/08补输出优先级文案、LO固定音量与无耳机暂停条件 | 偏移是否回写文件、播放点保存、3.3.4新UI/EQ完整路径未知。 |\n| 网络/遥控/音频输入 | F07补Cast连接和重连排错；F11/12/15分别接收USB、AirPlay、蓝牙音频；F04保留总模式入口与退出 | WiFi传歌目标目录/取消/索引反馈、一般音频焦点、模式退出后续播未知。 |\n| 恢复 | F14在线/本地更新及错误分流；F09补网络/偏好/内存清除范围 | 更新保留和回滚、备份恢复、误删撤销未知。 |\n\n'''
s=s.replace('本表是所读手册的功能索引',insert+'本表是所读手册的功能索引')
lines=s.splitlines()
for i,line in enumerate(lines):
 if not line.startswith('| F-M034-'):continue
 cols=[x.strip() for x in line.strip('|').split('|')];n=int(cols[0][-2:])
 if n==1:
  cols[4]='文件到设备不等于已进入App曲库。卡槽按JM21 FAQ触点向下、推入锁定、再推弹出；扫描完成步骤仍未知。';cols[5]=updates[n]['evidence_ids'].replace(';','；');cols[6]=updates[n]['gaps']
 if n==2:
  cols[3]+=' 排序菜单可按添加时间/文件名/首字母/年份；手册图行菜单出现Play next及Cancel，批量图出现Select all/Cancel。';cols[4]+=' 静态菜单不证明已触发或返回后的数据结果。';cols[5]=updates[n]['evidence_ids'].replace(';','；');cols[6]=updates[n]['gaps']
 if n==4:
  cols[4]+=' 本行只分析角色总入口；实际接收目标见F11/F12/F15。';cols[5]=updates[n]['evidence_ids'].replace(';','；')
 if n==6:
  cols[4]+=' 手册仅把蓝牙发射与模拟/同轴间写为后接入优先；两模拟口的“按插入顺序”未明确先/后。LO可调关闭时固定最大音量。';cols[5]=updates[n]['evidence_ids'].replace(';','；');cols[6]=updates[n]['gaps']
 if n==7:
  cols[2]='Link两端FiiO Music；Cast为JM21 Cast/手机Control；网络条件按分支';cols[3]+=' Cast分支：同局域网，JM21 FiiO Cast → Start；手机FiiO Control → Add New Device → Player，选IP或扫JM21显示的二维码。';cols[4]='Link/Music与Cast/Control是不同控制入口，DLNA是媒体来源。Cast失联先查网络、后台服务和IP；仍失败可删手机旧设备、重启JM21 Cast后重加。型号旧FAQ“只接收”与通用Music实验DLNA发射日志冲突，当前JM21发射能力待核。';cols[5]=updates[n]['evidence_ids'].replace(';','；');cols[6]=updates[n]['gaps']
 if n==8:
  cols[4]+=' 手册Audio另有Auto pause without headphones：Android/Pure Music无耳机2分钟后暂停，不能写成立即拔耳机暂停。';cols[5]=updates[n]['evidence_ids'].replace(';','；');cols[6]=updates[n]['gaps']
 if n==9:
  cols[4]+=' 手册System → Reset options另列网络、App偏好、全部内存数据三类重置，影响不同。';cols[5]=updates[n]['evidence_ids'].replace(';','；');cols[6]=updates[n]['gaps']
 lines[i]='| '+' | '.join(cols)+' |'
s='\n'.join(lines)+'\n'
rows='\n'.join('| '+' | '.join([f'F-M034-{n:02}',t,p,o,d,ids.replace(';','；'),g])+' |' for n,t,p,o,d,ids,g in new)
s=s.replace('\n\n## 分支、限制与未闭合的状态','\n'+rows+'\n\n## 分支、限制与未闭合的状态')
s=s.replace('文件缺失、空卡、误删、重复导入、换卡后的失效歌单、账号切换、跨设备冲突没有足够当前版本证据；','已补的空库按钮、错误升级分流和遥控重连只是特定分支。文件缺失、扫描完成反馈、误删、重复导入、换卡后的失效歌单、账号切换、跨设备冲突没有足够当前版本证据；')
s=s.replace('## 对本项目的分析（推断）','''## 本轮静态证据及排除项\n\n实际看过的 [p29](../artifacts/fiio-controls-deepening/manual-p029.png)、[p30](../artifacts/fiio-controls-deepening/manual-p030.png)、[p31](../artifacts/fiio-controls-deepening/manual-p031.png)、[p33](../artifacts/fiio-controls-deepening/manual-p033.png) 分别支持空库/引导、行菜单、排序/批量及歌词调节控件的布局。图的排列不是本项目连续操作；菜单取消、进度变化及文件写回未实测。[E-FICD-15]\n\n搜索找到的60秒/文件大小过滤说明点名旧X5，未套给JM21。当前手册和FAQ仍不足以闭合扫描过滤及WiFi传歌任务。均衡器文本、图中的PEQ标签和3.3.4重做说明也未拼成一个虚构当前界面。Music语言入口：旧手册写Settings → Languages，当前型号FAQ写ME → Languages，版本未注明，保留差异。[E-FICD-10、E-FICD-13、E-FICD-15]\n\n## 对本项目的分析（推断）''')
s+='''\n### 2026-10-27新增来源与原件\n\n本轮14份官方HTML正常HTTP200，复用原42页PDF。短摘记见 [source-notes](../artifacts/fiio-controls-deepening/source-notes.md)，原件/摘页SHA及页码在新证据JSONL；不下载固件、驱动、APK或视频。\n\n| E | 官方来源 | 定位 |\n| --- | --- | --- |\n'''
for r in e:s+=f"| {r['evidence_id']} | [{r['title']}]({r['url']}) | {r['locator']} |\n"
s+='\n本轮仅改变研究记录。全部hands_on=false；未改变设备、账户、配对、曲库或固件状态。\n'
C.write_text(s)
print(json.dumps({'new_E':len(e),'new_F':len(new),'old_F_updates':len(changes),'toc':len(toc),'body_sections':len(body),'faq':len(faq),'inventory':len(inv),'case_F':len(re.findall(r'^\| F-M034-',s,re.M))}))
