from pathlib import Path
import json,csv,hashlib,re,shutil,collections
W=Path('work/kobo-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches';A=L/'artifacts/kobo-controls-deepening';A.mkdir(parents=True,exist_ok=True);C=L/'cases/M-002-kobo.md'
DATE='2026-10-27'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,v):Path(p).write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rows,fields):
 with open(p,'w',newline='') as f:
  q=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');q.writeheader();q.writerows(rows)
def aid(u):
 m=re.search(r'/articles/(\d+)',u);return m.group(1) if m else u
OE={e['evidence_id']:e for e in json.loads((W/'evidence-before.json').read_text())};OF={f['flow_id']:f for f in json.loads((W/'coverage-before.json').read_text())}
caps={c['name']:c for c in json.loads((W/'captures.json').read_text())};ims={c['name']:c for c in json.loads((W/'image-captures.json').read_text())}
sources=[('collections','合集创建与编辑','Create / Manage / Rename / Delete；跨设备显示可能延迟'),('toc','阅读目录跳章','Reading Menu / table of contents；长目录左右划页'),('bookmark','页书签创建与回查','页角dog-ear；Notes列表定位'),('reading-menu','阅读菜单控件目录','14个图标说明；不代表全部设置'),('dark-mode','深色阅读','兼容列表含Libra Colour；图像不反相；关闭保存'),('comfortlight','暖光自动与手动','兼容列表含Libra Colour；Auto / Natural light / Bedtime'),('gestures','触控手势及选区范围','tap/swipe/press-hold；PDF与漫画选区例外'),('bt-troubleshoot','蓝牙排错与忘记配对','兼容列表含Libra Colour；Wi-Fi开关依赖；Rescan / Forget'),('reading-index','官方Reading栏目逐项索引','28条Articles；排除网站多语言通知链接'),('access-design','无障碍设计概述中的计时声明','5–60分钟/关闭声明；未给音频计时入口或N428构建')]
notes='''# Kobo N428控制补证（2026-10-27）

固定Libra Colour/N428；当前设备固件、地区和无障碍软件分支均未核实。本批仅普通公开网页、两张官方静态图，没有实机、登录、媒体播放、耗时测量或账户操作。N428整册PDF/EPUB的上批403路径未重试，不能称已读完整型号手册。

## 文档、目录和型号边界

沿用E-KBD-01型号页、02在线指南、23听书页、18彩虹纹页；同URL追加采集，不重复创建正文E。官方在线指南同时列其他机型、旧Stylus和专用保护壳，不把这些都当N428能力。台账区分正文新读、历史证据、仅目录与明确他机配件。

E-KBCD-01合集：操作对象是分组及其成员。创建/Manage/重命名/删除有独立完成键，跨端显示可能延迟；删除数据后果、冲突/撤销未完整说明。02目录支持长目录横划，03书签有页角标记与Notes回查，但删除/跨端规则未给。04只有阅读菜单控件表，不补未出现的设置子项。

05深色页列Libra Colour，正文说明图像不反相；06暖光页区分Auto、手动调色及Bedtime。Bedtime不是已证实的音频停止计时，也不引用其健康/睡眠效果宣传。07长按选区排除PDF与漫画，不能从统一手势推定所有格式同能力。

08蓝牙排错正文列Libra Colour；启用蓝牙自动开Wi-Fi，指无线开关状态，不证明必须接入互联网。Rescan与Forget后重配是官方建议，不是成功保证。页面中的他机电源示意未查看或套用；本批未取得断连续播时点。09为Reading栏目目录，10设计概述只提睡眠计时范围，没有指出音频计时入口/作用对象；不拿手机App的Sleep Timer补N428。

## 两张实际静态观察

- E-KBCD-11：[彩虹纹帮助](https://help.kobo.com/hc/en-us/articles/21355003278615-Reduce-the-rainbow-effect-when-viewing-colour-content)中的Page 2 of 3。可见Refresh the screen every: Chapter、Reading orientation: Auto rotate、Adobe EPUB页码、Dark Mode及Reduce rainbow effect。只确认示例控件/值；未展开下拉，不认定Chapter是N428默认或完整档位。图片为Libra Colour/Clara Colour共用帮助，图自身无型号/固件。
- E-KBCD-12：[听书帮助](https://help.kobo.com/hc/en-us/articles/4406292712471-Listen-to-audiobooks-on-your-Kobo-eReader)的统计按钮示意。可见Back to My Books、速度、统计/音量/菜单、章内时间轴和蓝牙目标；只看静态，未触发返回或统计，不认定返回后继续/暂停。图片没有N428型号标识。

原件HTML、提取文本、结构链接和两图在work/kobo-controls-deepening/raw；正式图为原下载字节副本，散列见证据与HANDOFF。没有下载音频/视频/固件/App。全部新F为D2文档重建，未观察到连续流程。正文引用在各条限定内，目录使用短译名与链接，原长标题只保留work原件。
'''
(A/'source-notes.md').write_text(notes);N=str((A/'source-notes.md').relative_to(L));NH=sha(A/'source-notes.md')
E=[];nameE={}
allold=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()]
for no,(name,title,loc) in enumerate(sources,1):
 c=caps[name];assert c['http_status']==200 and c['text_chars']>0
 assert not any(aid(e.get('url',''))==aid(c['url']) for e in allold),(name,'duplicate URL')
 eid=f'E-KBCD-{no:02}';nameE[name]=eid
 E.append(dict(evidence_id=eid,product_id='M-002',title=title,url=c['url'],checked_on=DATE,source_type='official_support_index' if name=='reading-index' else 'official_help',access_mode='anonymous_public_html_GET',version_scope='Kobo Libra Colour N428研究边界；共用eReader帮助，当前固件/账户地区/无障碍分支未知；正文有型号名单时才注明',app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note='只读文档；未说明不等于不支持。共用UI不作为N428当前实机。',hands_on=False,visual_observed=False,archived_artifact=N,sha256=NH,raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],extracted_work_path=c['extracted_work_path'],extracted_sha256=c['extracted_sha256'],capture_state='raw_source_in_work_and_short_note_archived',supersedes=None))
for no,name,derived,parent,title,note in [(11,'reading-settings-page2','E-KBD-18','rainbow','阅读设置Page 2静态观察','可见刷新Chapter示例值；未展开档位，不认定默认。'),(12,'audiobook-stats','E-KBD-23','audiobook','听书统计入口静态观察','Back to My Books等可见，未触发返回或证明音频状态。')]:
 im=ims[name];shutil.copyfile(im['raw_work_path'],A/f'{name}.png');rel=str((A/f'{name}.png').relative_to(L))
 E.append(dict(evidence_id=f'E-KBCD-{no:02}',product_id='M-002',title=title,url=caps[parent]['url'],checked_on=DATE,source_type='official_help',access_mode='official_static_image_downloaded_and_viewed',version_scope='共用官方帮助示意图；适用说明含Libra Colour，图自身无型号/固件；非N428实机截图',app_build=None,firmware_version=None,source_updated_on=None,locator='帮助页内嵌图；单帧view_image实际查看',note=note,hands_on=False,visual_observed=True,archived_artifact=rel,sha256=sha(A/f'{name}.png'),raw_work_path=im['raw_work_path'],raw_sha256=im['raw_sha256'],image_source_url=im['url'],derived_from=derived,capture_state='selected_official_static_image',supersedes=None))
(B/'kobo-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
newF=[]
def flow(no,task,eids,known,gaps):newF.append(dict(flow_id=f'F-M002-{no:02}',product_id='M-002',task=task,depth='D2',evidence_ids=eids,known_branches=known,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-002-kobo.md',checked_on=DATE))
flow(25,'创建、编辑与删除合集','E-KBCD-01','My Books/Collections菜单New collection→命名/Next→选书+/Create；长按合集Manage以勾/加号增删成员并Save，Rename后Save，Delete后再确认。跨端显示可能延迟。','取消、空名/重名、空合集、删除后的成员书/笔记细节、撤销及同步冲突未给；不等同于Remove download或账户Archive。')
flow(26,'添加页书签并回到标记页','E-KBCD-03','阅读中点右上角出现折角；中央Reading Menu/Notes→选Bookmark回查。','未给删除/再次点按行为、格式限制、多个书签排序、跨端同步或重排后锚点；不把书签等同阅读进度或音频断点。')
csvout(B/'kobo-controls-deepening-coverage.csv',newF,list(next(iter(OF.values()))))
updates=[]
def change(no,**kw):
 b=OF[f'F-M002-{no:02}'];a=dict(b,checked_on=DATE,**kw);updates.append(dict(flow_id=b['flow_id'],before=b,after=a,changed_fields={k:{'before':b[k],'after':v} for k,v in a.items() if v!=b[k]}))
change(2,evidence_ids='E-KOB-02;E-KOB-03;E-KBCD-07',known_branches='自动高亮/撤销及第三方书删除；通用手势页明确PDF和漫画不能按此长按方式选文本。',gaps='格式例外不等于不支持手写；取消/撤销全链、离线、冲突及当前N428构建未测。')
change(10,evidence_ids=OF['F-M002-10']['evidence_ids']+';E-KBCD-02;E-KBCD-04',known_branches='预览外退出/回原页保持旧链；中央Reading Menu→目录→点章，目录超过一页可左右划页。菜单另区分章节前后、书内搜索与Notes。',gaps='N428按键反转/长按快翻仍未闭合；目录跳转后的原页返回是否与预览相同、空目录/搜索及无障碍分支差异未核。')
change(13,evidence_ids=OF['F-M002-13']['evidence_ids']+';E-KBCD-05;E-KBCD-06;E-KBCD-11',known_branches='原彩虹纹取舍保留；深色由Reading Settings/Page appearance勾选后X保存，图像不反相。暖光Auto与手动分开，可设Bedtime。官方共用Page2静态图可见Refresh the screen every: Chapter。',gaps='仅刷新入口/示例值，非默认或完整档位；未测刷新/残影性能。Bedtime不等于音频计时；当前固件菜单、模式持久化/相互作用未核。',visual_status='static_reference_only')
change(18,evidence_ids=OF['F-M002-18']['evidence_ids']+';E-KBCD-08;E-KBCD-10;E-KBCD-12',known_branches='原下载/配对/跳章；统计按钮看章/全书剩余和下一章时长，菜单可详情/词典。排错给Rescan及Settings/Bluetooth connection设备菜单Forget后重配；启用蓝牙自动开Wi-Fi只指开关，不证明需互联网。',gaps='静态Back to My Books未触发，不推定退出续播；整本离线/断连/重启进度仍未知。设计概述计时范围未给N428音频入口/对象，不能套手机App睡眠计时。',visual_status='static_reference_only')
dump(W/'old-coverage-updates.json',updates)
adds=[]
for eid,name in [('E-KBD-01','model'),('E-KBD-02','guide'),('E-KBD-23','audiobook'),('E-KBD-18','rainbow')]:
 c=caps[name];a=dict(checked_on=DATE,access_mode='anonymous_public_html_GET',url=c['url'],http_status=200,raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],extracted_work_path=c['extracted_work_path'],extracted_sha256=c['extracted_sha256'],archived_artifact=N,sha256=NH,hands_on=False,visual_observed=False,locator={'model':'N428身份和官方指南链接','guide':'全部正文链接分层台账；未读子页单列','audiobook':'stats/dictionary、未缓存章；静态另E-KBCD-12','rainbow':'型号范围及Reading Settings Page2；静态另E-KBCD-11'}[name]);b=OE[eid].get('additional_captures',[]);adds.append(dict(evidence_id=eid,field='additional_captures',before=b,after=b+[a],append=a))
dump(W/'old-evidence-additional-captures.json',adds)
# Short translated titles make the index a locator, not a copied handbook.
shorts=['支持型号','功能','WiFi配置','电脑配置','开关机','充电','触控','首页','阅读','彩色','亮度','暖光','防水','旋转锁','Stylus2','Stylus1','笔电量','换电池','换笔尖','笔故障','手写手势','笔记本','书内手写','笔高亮','配对','听书','购音频','找音频','配对故障','加网络','网络状态','飞行模式','网络排错','WiFi同步','电脑同步','购书','加愿望单','看愿望单','删愿望单','看书库','搜书','下载全部','删一本','删全部','翻页','阅读菜单','排版','深色','跳页','目录','页书签','高亮','看高亮','删高亮','加笔记','查词','加词典','翻译','标已读','借书','电脑App','Sage保护壳','Elipsa保护壳','锁屏','大字','阅读字体','深色','故障','常见排错','重置','维修','求助','安全保修','查看法律']
guide=json.loads((W/'raw/guide-structure.json').read_text())['links'];modelIDs={'360053504173','4406285084183','12100658162071','17971574287895','6122943232407','4406285169175','18001606982679','18499096277015'}
guide=[x for x in guide if aid(x['url']) not in modelIDs];assert len(guide)==len(shorts)==74
allE=list(OE.values())+E;urlE={aid(e['url']):e['evidence_id'] for e in allE if not e.get('derived_from')};newbody={aid(c['url']) for c in caps.values()};fmap=collections.defaultdict(set)
for f in list(OF.values())+newF:
 for eid in f['evidence_ids'].split(';'):fmap[eid].add(f['flow_id'])
for u in updates:
 for eid in u['after']['evidence_ids'].split(';'):fmap[eid].add(u['flow_id'])
I=[]
def inv(domain,item,url,loc,eids,fs='',status='body_read_partial',gap='只核该项入口/目录；任务映射不表示所有分支闭合'):
 I.append(dict(inventory_id=f'I-KBCD-{len(I)+1:03}',domain=domain,item=item,source_url=url,locator=loc,coverage_status=status,evidence_ids=eids,flow_ids=fs,gap=gap))
for x,label in zip(guide,shorts):
 eid=urlE.get(aid(x['url']));status='body_read_this_batch' if aid(x['url']) in newbody else ('prior_evidence_not_reread' if eid else 'index_only')
 if label in ('Sage保护壳','Elipsa保护壳'):status='other_model_accessory_out_of_scope'
 inv('在线指南链接',label,x['url'],x['section'],'E-KBD-02'+(';'+eid if eid and eid!='E-KBD-02' else ''),';'.join(sorted(fmap[eid])) if eid else '',status,'共用指南；12个兼容型号引用另作范围说明、不计功能；配件/笔代际不套N428')
reading=json.loads((W/'raw/reading-index-structure.json').read_text())['links']
labels=['彩虹纹','防误触','装字体','阅读菜单','目录','翻页','排版','跳页','首页','搜书','删除概述','触控','亮度','暖光','看高亮','删高亮','加词典','看笔记','删一本','高亮','加笔记','删笔记','删多本','删全部','查词','页书签','翻译','标已读'];assert len(labels)==len(reading)==28
for x,label in zip(reading,labels):
 eid=urlE.get(aid(x['url']));inv('Reading栏目链接',label,x['url'],'Articles','E-KBCD-09'+(';'+eid if eid and eid!='E-KBCD-09' else ''),';'.join(sorted(fmap[eid])) if eid else '', 'body_read_this_batch' if aid(x['url']) in newbody else ('prior_evidence_not_reread' if eid else 'index_only'),'目录中仅新增关联与读取状态；不重复计独立任务')
for label in ['返回首页','方向锁','照明','字体','阅读统计','目录','设置','更多','前章','后章','进度拖动','回先前页','书内搜索','批注']:
 inv('阅读菜单控件',label,caps['reading-menu']['url'],'图标说明表','E-KBCD-04','F-M002-10',gap='通用菜单说明；不表示搜索/统计/设置所有选项已分析')
for label in ['刷新频次（示例Chapter）','方向（示例Auto rotate）','Adobe EPUB页码','Dark Mode','Reduce rainbow effect']:
 inv('静态设置项',label,caps['rainbow']['url'],'Page 2 of 3，已看单帧','E-KBCD-11','F-M002-13','static_control_only','图自身无型号/构建；未展开下拉/触发开关，不认定默认、保存或效果')
for label in ['亮度','Auto','Natural light','Bedtime']:
 inv('暖光设置项',label,caps['comfortlight']['url'],'步骤3–6','E-KBCD-06','F-M002-13',gap='文档动作；未实测随时间变化/持久化，也不是听书计时')
for label in ['开书/下载','下载状态','存储空间','播放/暂停','前后30秒','章内拖动','目录跳章','音量','速度','统计','详情/词典']:
 inv('音频文档章节',label,caps['audiobook']['url'],'相应正文小节','E-KBD-23','F-M002-18',gap='有章节不等于已下载全本；断连/重启和返回后的播放状态缺口保留')
for name in ['collections','bookmark','gestures','bt-troubleshoot','access-design']:
 inv('补充正文',dict((x[0],x[1]) for x in sources)[name],caps[name]['url'],dict((x[0],x[2]) for x in sources)[name],nameE[name],';'.join(sorted(fmap[nameE[name]])),gap='D2只限有操作链的分支；设计概述计时声明仅D1，不补音频流程')
csvout(B/'kobo-controls-deepening-inventory.csv',I,list(I[0]));counts=dict(collections.Counter(i['domain'] for i in I))

s=(W/'case-before.md').read_text();s=s.replace('查阅：2026-10-26。本轮固定','查阅：2026-10-26；2026-10-27补目录与控制。本例固定',1)
s=s.replace('未登录、未操作设备、未检视图片、未观察连续界面，全部流程仍为','未登录、未操作设备、未观察连续界面；上批未检视图片，本批实际查看两张共用官方静态示意（不是N428实机图）。任务仍为',1)
s=s.replace('PDF 与 EPUB 下载各遇 HTTP 403，预览页面无可抽取正文','上批PDF与EPUB下载各遇HTTP 403，预览页面无可抽取正文；本批未重试这些拒绝路径',1)
s=s.replace('书库排序、合集创建/批量移动、完成状态与检索空结果未重建。','合集创建/成员编辑/重命名/删除已补F25；书库排序、完成状态与检索空结果仍缺。')
s=s.replace('目录/书签有指南入口，未独立分析；','目录补F10，页书签补F26；')
s=s.replace('深色模式、暖光定时有指南入口，未核定完整分支；常规阅读的全刷频率入口/档位仍未知。','深色与暖光Auto/手动/Bedtime补F13；静态图确认刷新入口及Chapter示例值，完整档位/默认仍未知。')
s=s.replace('音频订阅权益、睡眠定时、侧载音频、断网换章、断连/重启续播未知。','统计/词典与配对排错补F18；音频睡眠计时、侧载、断网换章、断连/重启续播仍未知。')
section=f'''## 2026-10-27逐项目录与读取层级

[目录台账](/home/workspace/reader/docs/research/interaction-library/batches/kobo-controls-deepening-inventory.csv)共{len(I)}条：在线指南74个功能/帮助链接（含重复深色入口和两项他机配件），Reading栏目28条、阅读菜单14控件、静态设置5项、暖光设置4项、音频小节11项及补充正文5项。不同层级有交叉，不是{len(I)}项独立功能；在线指南另12个兼容型号链接不计功能。没有取得N428整册或全设置树。

每项区分本批正文、旧证据未复读、仅目录、他机配件及静态可见。当前固件仍未知；共用指南中的Sage/Elipsa保护壳、旧Stylus电池更换不能直接当N428/Stylus2流程。常规/欧盟无障碍分支保持原边界。正式短摘记见[本批来源说明](/home/workspace/reader/docs/research/interaction-library/artifacts/kobo-controls-deepening/source-notes.md)。

'''
s=s.replace('## 已有查词与文本批注流程',section+'## 已有查词与文本批注流程',1)
repl={
2:'| F-M002-02 标注与写笔记 | 长按松手出现选区手柄，拖端点选文本并自动高亮，可撤销；Add Note输入/Save。通用手势帮助明确PDF和漫画不走此文本选择链。 | 兼容书笔记可随账户同步；第三方书笔记仅本机，删书也删笔记；彩色机可改高亮颜色。格式例外不排除另有PDF手写；误触撤销/离线冲突仍待查。 | E-KOB-02、03；E-KBCD-07 |',
10:'| F-M002-10 翻页、预览、目录与回原位 | 普通点/划翻页；中央Reading Menu出章节前后/进度预览，点预览才进入，点预览外退出；黑点或Back to page回先前位置。目录图标→点章；目录多页时左右划目录页。 | 不把预览返回规则推定到所有目录/搜索跳转。N428实体键已确认，但通用键反转/长按快翻名单仍缺N428；无障碍软件预览限制保持F22。 | E-KBD-01、12、13、29；E-KBCD-02、04 |',
13:'| F-M002-13 亮度、深色、暖光与刷新边界 | 原左边划亮度及Reduce rainbow effect保留；深色：中央→齿轮→Page appearance/Dark Mode→X保存，图像不反相。Home亮度菜单中Auto开自动暖光，关后手动调Natural light；Bedtime下拉选通常就寝时刻。 | 两篇正文名单含Libra Colour。已看共用Page2图可见Refresh the screen every: Chapter，但未展开选项，不能定默认/完整档位或全刷效果；暖光Bedtime不是音频停止计时。彩虹纹改善/性能取舍未测。 | E-KBD-18、19；E-KBCD-05、06、11 |',
18:'| F-M002-18 下载并用蓝牙听书 | 账户有声书下载后开播放器；Play/Pause、前后30秒、章内拖动、目录、音量/速度保持原链。统计图标显示章/全书剩余及下一章时长，更多菜单可详情/词典。未连时点No Bluetooth Connected扫描；失败可Rescan或Settings/Bluetooth connection中设备菜单Forget，再重启双方/重配。 | 未缓存章仍需下载。排错文档说启用蓝牙自动开Wi-Fi，未说必须联网成功。静态可见Back to My Books，未触发或推定返回后续播。整本脱网、断连/重启位置仍未知；设计概述计时范围不足以确认N428音频睡眠入口。 | E-KBD-23；E-KBCD-08、10、12 |'}
lines=s.splitlines()
for k,v in repl.items():
 idx=[i for i,t in enumerate(lines) if t.startswith(f'| F-M002-{k:02} ')];assert len(idx)==1;lines[idx[0]]=v
s='\n'.join(lines)+'\n'
extra='''| F-M002-25 创建、编辑与删除合集 | My Books→Collections→菜单/New collection→命名/Next→书旁+/Create。长按合集→Manage，勾去成员或+加成员后Save；Rename输入/Save；Delete后再Delete确认。 | 文档说分组可在设备和App看到，新建后跨端可能延迟。空名/重名、空合集、删除成员书/笔记的细节、撤销和冲突未说明；与清下载/账户归档分开。 | E-KBCD-01 |
| F-M002-26 添加页书签并回查 | 阅读时点右上角，文档描述出现折角；中央Reading Menu→Notes→选择Bookmark回到标记位置。 | 删除/再次点按、格式限制、列表排序、跨端同步和重排锚点未给；不把页书签当自动阅读进度或音频断点。 | E-KBCD-03 |
'''
s=s.replace('\n\n## 容易被功能列表掩盖的边界','\n'+extra+'\n## 容易被功能列表掩盖的边界')
s+='''
## 本批新证据与反向依赖

2026-10-27新证据见[本批JSONL](/home/workspace/reader/docs/research/interaction-library/batches/kobo-controls-deepening-evidence.jsonl)，新增F25/26见[覆盖表](/home/workspace/reader/docs/research/interaction-library/batches/kobo-controls-deepening-coverage.csv)。旧F02/10/13/18仅补分支，不重复计数。E-KBD-01/02/18/23复抓沿用旧E；历史正文保留取得方式，追加采集由根合并。

| 新E | 官方来源 | 已取得范围 |
| --- | --- | --- |
'''
for i,(name,title,loc) in enumerate(sources,1):s+=f'| E-KBCD-{i:02} | [{title}]({caps[name]["url"]}) | {loc}；未实操。 |\n'
s+='''| E-KBCD-11 | E-KBD-18派生[阅读设置图](/home/workspace/reader/docs/research/interaction-library/artifacts/kobo-controls-deepening/reading-settings-page2.png) | 实际看单帧；共用Colour帮助，未标固件/机型；Chapter仅示例。 |
| E-KBCD-12 | E-KBD-23派生[音频统计入口图](/home/workspace/reader/docs/research/interaction-library/artifacts/kobo-controls-deepening/audiobook-stats.png) | 实际看单帧；可见返回入口，不能证明离开后的声音/位置。 |

修改前扫描全库JSONL/JSON中的本case路径及当前SHA，未发现旧E把此case当归档审计文件的反向依赖（当前M002旧E的archived_artifact/sha256均为空）。因此没有迁移旧E字段；交接保留扫描结果。未来审计应归档不可变摘记/原件，不以持续编辑的case承担证据哈希。
'''
C.write_text(s)
manifest=[C,B/'kobo-controls-deepening-evidence.jsonl',B/'kobo-controls-deepening-coverage.csv',B/'kobo-controls-deepening-inventory.csv',A/'source-notes.md',A/'reading-settings-page2.png',A/'audiobook-stats.png']
dump(W/'formal-manifest.json',[{'path':str(p),'sha256':sha(p)} for p in manifest])
dump(W/'counts.json',dict(new_evidence=len(E),new_flows=len(newF),old_flows=len(updates),old_fields=sum(len(x['changed_fields']) for x in updates),old_captures=len(adds),inventory=len(I),domains=counts))
print((W/'counts.json').read_text())
