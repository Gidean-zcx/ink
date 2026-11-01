# FINALIZED: do not rerun after root review/merge.
from pathlib import Path
import json,csv,re,hashlib,shutil,difflib
from urllib.parse import urlsplit,urlunsplit,unquote
W=Path('work/hiby-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches';A=L/'artifacts/hiby-controls-deepening';A.mkdir(parents=True,exist_ok=True);D='2026-10-27'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def norm(u):
 s=urlsplit(u);return urlunsplit((s.scheme,s.netloc.lower(),unquote(s.path).rstrip('/'),'',''))
m={r['name']:r for r in json.loads((W/'source-manifest.json').read_text())};oe=json.loads((W/'old-evidence-before.json').read_text());of=json.loads((W/'old-coverage-before.json').read_text())
allold=[]
for p in L.rglob('*.jsonl'):
 if p.name!='hiby-controls-deepening-evidence.jsonl':allold.extend(json.loads(s) for s in p.read_text().splitlines() if s.strip())
assert not any(x.get('evidence_id','').startswith('E-HBCD-') for x in allold)
oldurls={norm(x['url']) for x in allold if x.get('url')}
notes='''# HiBy R3II 2025：本轮原件、冲突与不足

查阅2026-10-27。沿用型号，实际固件未知；1.1_202501061153仍只是发布页列项。普通GET取得7份HTML，store帮助页403后停止；未下载固件/应用/音频视频。旧原件检索只找到旧案例工作稿，故本轮重取并给旧E追加采集建议。

| E/旧采集 | 使用与限制 |
| --- | --- |
| E-HBCD-01 中文型号指南 | 与英文同源语言版本，不是独立验证。逐项目录/菜单，播放模式/跳目录；Wi-Fi传歌只有进入、按提示和左滑/左上退出。队列/删除仍无操作细节。 |
| E-HBCD-02 中文X1600 FAQ | 明列R3II2025但为跨型号平台文档。睡眠/省电的重启开关不同；电脑USB/充电大图标界面不响应计时关机。扫描重计与任何条件不重计同时存在。 |
| E-HBCD-03 中文规格 | 与手册语言版本和自身按键行冲突，只用于指出资料适用疑点，不选择性拼成硬件定论。 |
| E-HBCD-04 两张官方型号页插图 | 实际看过右侧旋钮、前屏播放控件、底部接口；图上没有可确认2025装配的型号文字。文件名/页归属不能证明实机身份。 |
| E-HBD-01/02/03/04 | 重取英文指南、固件记录、平台FAQ、USB帮助；沿用旧E，不重复登记。E-HBY-01与E-HBD-01同URL，采集挂后者，不重复算来源。 |

硬件冲突：英文指南写CS43198、蓝牙5.0与音量加减键；中文指南写ES9219C、蓝牙5.1，中文规格写ES9219C、蓝牙5.0且硬件按键表写旋钮、软件行写音量按键；两幅插图也有旋钮。保留原文，不据此判定2025硬件实际版本。

数据冲突：FAQ封面优先内嵌，型号手册把内嵌放最后；MSEB上限96kHz与384kHz也不一致。Wi-Fi Scan段误写蓝牙搜索；传歌没有浏览器地址/上传按钮/落盘目录/成功失败反馈。中文HTML存在少量NUL字符，原件与提取均保留，阅读检索用rg -a；不把文本提取当布局完整性证明。

正式档案仅短摘记和实际看过的两张插图；HTML在work。全部hands_on=false，无连续观察、设备操作、登录或提交。目录项不是任务完成，当前队列/歌单/文件删除仍D1。
'''
(A/'source-notes.md').write_text(notes)
E=[]
for n,name,title,loc in [('01','manual-zh','R3II2025中文型号手册：目录及语言版本差异','§1–11；播放设置24项、系统23项；Wi-Fi传歌与功能菜单'),('02','faq-zh','X1600中文FAQ：计时、列表和路径','说明点名本机；电源2小节、音频列表限制/排序、Other歌单'),('03','spec-zh','R3II2025中文规格：与手册及插图的冲突','硬件平台按键/DAC/蓝牙；软件按键行；不作为装配实证')]:
 r=m[name];assert norm(r['url']) not in oldurls
 E.append(dict(evidence_id='E-HBCD-'+n,product_id='M-035',title=title,url=r['url'],checked_on=D,source_type='official_manual' if n=='01' else 'official_help' if n=='02' else 'official_specification',access_mode='anonymous_public_html_GET',version_scope='HiBy R3II2025 / HiBy OS；型号与平台文档存在复用冲突；实际固件未知；不套手机HiBy Music',app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note='同源中文版本/规格补证，非独立实机验证。',hands_on=False,visual_observed=False,raw_work_path=r['raw_work_path'],raw_sha256=r['raw_sha256'],extracted_work_path=r['extracted_work_path'],extracted_sha256=r['extracted_sha256'],archived_artifact='artifacts/hiby-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),capture_state='raw_html_in_work_and_short_note_archived',supersedes=None))
im=json.loads((W/'image-manifest.json').read_text());assert len(im)==2
for i,x in enumerate(im,1):shutil.copyfile(x['raw_work_path'],A/f'official-product-{i}.png')
E.append(dict(evidence_id='E-HBCD-04',product_id='M-035',title='型号指南附带的两张旋钮机身插图：型号身份存疑',url=im[0]['url'],related_urls=[im[1]['url']],checked_on=D,source_type='official_product_illustration',access_mode='anonymous_image_GET_and_view_image',version_scope='页面称R3II2025；插图本身未能确认2025装配，不能调和按键/旋钮冲突',app_build=None,firmware_version=None,source_updated_on=None,locator='型号手册开头两幅产品图；无视频或时间戳',note='两图实际查看；均见右侧旋钮与播放控件示意，不是物理设备或连续操作。',hands_on=False,visual_observed=True,observation_type='static_product_illustrations_only',observed_image_count=2,derived_from=['E-HBD-01','E-HBCD-01'],raw_work_path=im[0]['raw_work_path'],raw_sha256=im[0]['raw_sha256'],additional_raw_files=[im[1]],archived_artifact='artifacts/hiby-controls-deepening/official-product-1.png',sha256=sha(A/'official-product-1.png'),rendered_artifacts=['artifacts/hiby-controls-deepening/official-product-1.png','artifacts/hiby-controls-deepening/official-product-2.png'],artifact_hashes={f'artifacts/hiby-controls-deepening/official-product-{i}.png':sha(A/f'official-product-{i}.png') for i in [1,2]},capture_state='two_static_images_archived',supersedes=None))
assert all(norm(x['url']) not in oldurls for x in E)
(B/'hiby-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
recaps=[]
for eid,name in [('E-HBD-01','manual-en'),('E-HBD-02','firmware-en'),('E-HBD-03','faq-en'),('E-HBD-04','usb-en')]:
 before=next(x for x in oe if x['evidence_id']==eid);r=m[name];cap={k:r[k] for k in ['url','checked_on','raw_work_path','raw_sha256','extracted_work_path','extracted_sha256']};cap.update(access_mode='anonymous_public_html_GET',locator='article正文；章节/设置定位见本批inventory',archived_artifact='artifacts/hiby-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),hands_on=False,visual_observed=False);after=json.loads(json.dumps(before));after.setdefault('additional_captures',[]).append(cap);recaps.append(dict(evidence_id=eid,before=before,after=after,changed_fields={'additional_captures':{'before':before.get('additional_captures'),'after':after['additional_captures']}}))
(W/'old-evidence-additional-captures.json').write_text(json.dumps(recaps,ensure_ascii=False,indent=2)+'\n')
updates={
'02':dict(evidence_ids='E-HBY-01;E-HBD-03;E-HBCD-01;E-HBCD-02',known_branches='睡眠从开关打开计时，重启保留时长但开关关；省电关机需未播、无无线音频、USB未连，FAQ称其时长和开关重启保留。平台中文另列电脑USB/充电大图标界面不响应两类计时关机。',gaps='FAQ扫描/格式化重计与任何条件不重计矛盾未解；手册USB进入充电界面与平台电脑USB例外分开保留。A≥B/切曲A-B、计时显示/取消与边界时序未观察。'),
'03':dict(evidence_ids='E-HBD-01;E-HBD-04;E-HBCD-01;E-HBCD-02',known_branches='自动重扫触发写为检测到卡或插USB；文件/索引仍分层。平台FAQ另给无CUE5800/有CUE6200文件上限及标签排序；不是当前固件实测容量。Wi-Fi传歌仅取得入口、按提示及退出描述，不能视为完成入库。',gaps='无卡/空间满/扫描取消完成、坏标签、重扫去重、USB触发时机；无线传歌浏览器地址/上传/落盘目录/成功失败/断线续传未知。'),
'04':dict(evidence_ids='E-HBD-01;E-HBCD-01;E-HBCD-02',known_branches='功能菜单仍只列当前列表、加歌单、删除等名称；不能确认Delete对象。FAQ称最近播放只保留50曲、内部歌单/收藏无数量上限，需区别文件扫描上限，均非实测。播放范围另见F10，不能补作编辑队列证据。',gaps='新建/重排/去重/批量选择、确认取消、删引用或文件、当前曲删除后选曲和撤销仍未知，维持D1；插图不是删除界面。'),
'05':dict(evidence_ids='E-HBD-03;E-HBCD-02',known_branches='平台规定导入导出m3u放playlist_data；支持m3u/m3u8及多种编码，推荐UTF-8 BOM和相对路径。相对路径从歌单所在目录解析；独立搬移后需核查引用。CUE/ISO分轨不在此解析范围。',gaps='设备导入导出按钮、错路径提示、重复成员、删除歌单与音频关系、批量恢复未说明。b盘是平台第二卡示例，不能断言本机有第二卡槽。'),
'06':dict(evidence_ids='E-HBD-01;E-HBCD-01',known_branches='中文同源手册仍只写播放中关机、下次记住歌曲并继续自动播放；未补精确时间点。开关入口与原F保留。',gaps='秒点、暂停后关机、崩溃/没电、缺卡和未接耳机时行为均未知；不将曲目记忆当逐书断点。')}
up=[]
for n,ch in updates.items():
 before=next(x for x in of if x['flow_id']=='F-M035-'+n);after={**before,**ch,'checked_on':D};up.append(dict(flow_id=before['flow_id'],before=before,after=after,changed_fields={k:{'before':before[k],'after':after[k]} for k in before if before[k]!=after[k]}))
(W/'old-coverage-updates.json').write_text(json.dumps(up,ensure_ascii=False,indent=2)+'\n')
newf=dict(flow_id='F-M035-10',product_id='M-035',task='选择播放范围与跨目录/专辑连续播放',depth='D2',evidence_ids='E-HBD-01;E-HBCD-01',known_branches='播放设置或播放页最左模式按钮选择列表/单曲循环/随机/列表循环；列表到尾停止，其余有循环规则。Play through folders/albums另控制到末曲后跨组；单曲循环自动切曲时跨组无效。',gaps='“顺序播放”与四模式术语未完全对齐；文件夹跳转时模式逻辑失效的实际顺序、手动下一曲例外、缺文件、随机恢复及与A-B/计时叠加未知。不是队列编辑/持久歌单任务。',visual_status='not_observed',hands_on='false',case_path='cases/M-035-hiby.md',checked_on=D)
with (B/'hiby-controls-deepening-coverage.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=list(of[0]),lineterminator='\n');wr.writeheader();wr.writerow(newf)
# Exact headings and first-level named settings; no claim of every nested option.
labels={
'quick-operation':['Bluetooth','WiFi','DLNA','AirPlay','HiByLink','MSEB','Output selection','Gain settings','Fade in and out','Gapless playback','Sleep shutdown'],
'playback-settings':['MSEB','Equalizer','Update database','Music update mode','Playback mode','Output selection','DSD output mode','DSD Gain Compensation','Power-off memory playback','Gapless playback','Sound field','Maximum digital volume output','Maximum volume limit','Default volume on power','Fade in and out','Gain settings','ReplayGain','Balance','Digital filter','Play through folders','Play through albums','Repeat function','Speed play','Automatically slide to playlist'],
'bluetooth':['Bluetooth switch','Sound quality (Bluetooth encoding)','Bluetooth volume adjustment','Bluetooth search','Matched devices','Available equipment'],
'wi-fi':['Wi-Fi switch','Wi-Fi information','Add a network','Wi-Fi Scan','DNS settings','Matched devices','Available networks'],
'10-system':['Language','Backlight settings','Theme color','UI themes','Font size','USB working mode','USB DAC feedback','Time settings','Power saving shutdown','Sleep shutdown','Power percentage display','Wire control','LED indicators','Button operation when screen off','Volume knob operation when the screen is turned off','Shortcut menu','Screensaver settings','Screen rotation','Restore factory settings','Firmware update','Certification information','About','Developer Options']}
mi=json.loads((W/'menu-items.json').read_text());assert len(mi)==71
inv=[]
def add(group,item,url,loc,eids,flows='',status='inventory_only',gap='只有目录/功能说明；未闭合任务，不等于实机菜单'):
 inv.append(dict(inventory_id=f'I-HBCD-{len(inv)+1:03}',domain=group,item=item,source_url=url,locator=loc,coverage_status=status,evidence_ids=eids,flow_ids=flows,gap=gap))
for h in m['manual-en']['headings']:
 if h['level']!='h1':add('型号手册章节',h['text'].replace('\u200b',''),m['manual-en']['url'],h['id'],'E-HBD-01;E-HBCD-01')
for x in mi:
 anchor=x['anchor'];pos=x['position'];title=labels[anchor][pos-1];flow='';status='inventory_only';gap='同章部分操作已读；其他选项/状态仍未闭合'
 if anchor=='playback-settings' and pos in [5,20,21]:flow='F-M035-10';status='documented_partial_task'
 if title in ['Update database','Music update mode']:flow='F-M035-03';status='documented_partial_task'
 if title=='Power-off memory playback':flow='F-M035-06';status='documented_partial_task'
 if title in ['Sleep shutdown','Power saving shutdown','Repeat function']:flow='F-M035-02';status='documented_partial_task';gap='省电/睡眠区别；平台重计和USB条件冲突；A-B异常未核'
 if title=='Firmware update':flow='F-M035-08';status='documented_partial_task'
 if title=='USB working mode':flow='F-M035-07';status='documented_partial_task'
 if title=='Wi-Fi Scan':gap='本段正文写Bluetooth search，保留疑似复用错误'
 if title=='Volume knob operation when the screen is turned off':gap='与音量按键/旋钮资料冲突；不确认实际控制类型'
 add('一级设置：'+anchor,title,m['manual-en']['url'],anchor+': item '+str(pos),'E-HBD-01;E-HBCD-01',flow,status,gap)
for name in ['current playlist','adding to the playlist','jumping to the equalizer','viewing albums','viewing song details','deleting']:
 add('播放功能菜单',name,m['manual-en']['url'],'playback-interface: Function Menu','E-HBD-01;E-HBCD-01','F-M035-04','D1_entry_only','只有入口名称；删除对象/确认和队列修改结果未知')
for h in m['faq-en']['headings']:
 if h['level']!='h1':add('X1600平台FAQ',h['text'].replace('\u200b',''),m['faq-en']['url'],h['id'],'E-HBD-03;E-HBCD-02','','platform_scope_inventory','平台共用；仅点名本机不足以排除每项的固件/型号差异')
assert len(inv)==145,len(inv)
with (B/'hiby-controls-deepening-inventory.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');wr.writeheader();wr.writerows(inv)
s=(W/'case-before.md').read_text().replace('查阅：2026-10-26。','查阅：2026-10-26；本轮补证2026-10-27。',1)
s=s.replace('无实机、无连续观察、无单帧图像观察。','无实机、无连续观察；本轮实际看过两幅型号页产品插图，但图中旋钮与文字按键冲突，不能确认2025装配。',1)
s=s.replace('下表对照型号手册全部 11 个一级章节；','下表是2026-10-26基线，对照型号手册全部11个一级章节；旧缺口以本轮更新为准。',1)
s=s.replace('原流程编号保留；本轮从 F-M035-03 追加。','原流程编号保留；历史追加03–09，本轮仅新增F10，相同目标补原号。',1)
s=s.replace('A≥B、切曲后循环状态未核实；计时重启边界见下。','A≥B、切曲后循环未知。睡眠重启保留时长、开关关闭；省电则FAQ称时长/开关均保留。电脑USB/充电大图标不响应计时关机；扫描重计矛盾见下。',1)
s=s.replace('E-HBY-01 §4、§5；E-HBD-03 |','E-HBY-01 §4、§5；E-HBD-03；E-HBCD-01、02 |',1)
s=s.replace('文件与索引是两层；回 Music 分类查找。','文件与索引是两层。Auto原文检测到卡或插USB触发；FAQ文件上限无CUE5800/有CUE6200是平台描述，不作实测容量。Wi-Fi传歌仍只有入口/按提示/退出，未闭合入库。',1)
s=s.replace('| E-HBD-01 §5；E-HBD-04 |','| E-HBD-01 §5；E-HBD-04；E-HBCD-01、02 |',1)
s=s.replace('未证明 Delete 删文件还是引用。','未证明Delete删文件还是引用。FAQ最近播放只留50曲、内部歌单/收藏称无数量上限；不等于扫描/内存无限。F10仅补播放范围。',1)
s=s.replace('| E-HBD-01 §5 | 新建','| E-HBD-01 §5；E-HBCD-01、02 | 新建',1)
s=s.replace('歌单引用不是音频副本；CUE/ISO 分轨有解析限制。','歌单引用不是音频副本；支持m3u/m3u8，平台推荐UTF-8 BOM/相对路径；相对路径基准是歌单所在目录，独立搬移后需核查引用。CUE/ISO分轨不支持该解析。',1)
s=s.replace('| E-HBD-03 Other：Import/export、m3u parsing |','| E-HBD-03 Other：Import/export、m3u parsing；E-HBCD-02 |',1)
s=s.replace('手册称记住曲目并继续自动播放。','中英文同源手册均只足以确认记曲目并自动播放，没有新增秒点依据。',1)
s=s.replace('| E-HBD-01 §5.9 |','| E-HBD-01 §5.9；E-HBCD-01 |',1)
row='| F-M035-10 | 选择播放范围与跨目录/专辑连续播放 D2 | 已有本地列表；模式/跨组开关互相影响 | Playback settings或播放页最左模式键选列表、单曲循环、随机、列表循环；Play through folders/albums控制跨组。列表从所选曲至末曲停止；文件夹跨组从父级寻找后续目录，专辑结束可转下专辑。 | 单曲循环自动切曲时跨组无效。手册另称跳目录时模式逻辑无效，并使用未统一的“顺序播放”术语；没有据此拼出确定随机队列。 | E-HBD-01；E-HBCD-01 | 选项实际回退/保存、缺文件、手动切曲边界、随机恢复及与A-B/计时组合未知；不是编辑当前队列或持久歌单。 |'
s=s.replace('\n\n## 分支、冲突与数据边界','\n'+row+'\n\n## 分支、冲突与数据边界',1)
s=s.replace('型号手册实体布局为音量加减键，系统项却写“熄屏音量旋钮”；MSEB 上限也与平台 FAQ 不同。均为文档适用性疑点，不合成为已验证配置（E-HBD-01、03）。','英文型号手册写音量加减键，系统项却写“熄屏音量旋钮”；本轮中文规格及两幅实际看过的插图又显示旋钮。英文DAC/蓝牙为CS43198/5.0，中文型号指南为ES9219C/5.1，中文规格为ES9219C/5.0；资料互相不一致，不能择项拼成2025硬件（E-HBD-01、E-HBCD-01、03、04）。MSEB上限96kHz/384kHz、封面内嵌优先/最后也冲突（E-HBD-03、E-HBCD-02）。',1)
s=s.replace('保留矛盾，不能保证扫描期间准时停播（E-HBD-03）。','2026-10-27中文同源FAQ仍保留矛盾。它清楚列电脑USB连接/充电大图标界面不响应睡眠和省电关机；型号手册却概括插USB转充电界面，不能混成所有USB角色的保证（E-HBD-03、E-HBCD-01、02）。',1)
s+='''
## 2026-10-27逐项目录与补证限制

[目录CSV](../batches/hiby-controls-deepening-inventory.csv)共145项：35个手册层级标题（不含总标题，含11个一级章）、71个一级设置（快捷11、播放24、蓝牙6、Wi-Fi7、系统23）、6个播放功能菜单名、33个平台FAQ层级标题。只声明这些层级清点完整，不声称所有嵌套选项、隐藏设置或现行固件界面已验证。目录与正文任务覆盖分开。

无线传歌：型号中英文§7均只有进入、按提示、左滑或左上返回的叙述；主界面/播放页的右划返回不能替代这里的左划。没有浏览器地址、同网前提的该型号完整链、目标目录、进度/完成/失败提示及恢复证据。Wi-Fi Scan段还错误写成蓝牙搜索；因此不新增“已完成无线传歌”流程。官方商城帮助正常访问403后停止，搜索索引亦明确旧R3II固件不用于2025，不用旧机教程补本机。

两张官方型号页插图已实际查看：[红色图](../artifacts/hiby-controls-deepening/official-product-1.png)、[黑色图](../artifacts/hiby-controls-deepening/official-product-2.png)。只能说明附图展示右侧旋钮/播放控件示意；图片没有能确认2025装配的型号文字，不能用页面/文件名覆盖相反文字证据。它们不是队列/删除确认界面，也不是连续流程。

本轮仅新增F10，补原02–06；当前队列/删除和外部歌单仍D1。睡眠/省电、文件/索引/歌单引用、重启自动播放/秒点继续分别保留，不因目录齐全升为实测。原件HTML有少量NUL字符，已原样保留；检索使用rg -a，未把提取成功当完整布局证明。

| 新E | 官方来源 | 定位 |
| --- | --- | --- |
'''
for e in E:s+=f"| {e['evidence_id']} | [{e['title']}]({e['url']}) | {e['locator']} |\n"
s+='\n旧E-HBD-01/02/03/04仅提additional_captures合并建议；E-HBY-01与01同URL不重复采集计数。原件SHA/路径见本批证据和work清单；[source-notes](../artifacts/hiby-controls-deepening/source-notes.md)保留限制。未下载固件、登录、升级、删除或传歌；仅研究状态改变。\n'
(L/'cases/M-035-hiby.md').write_text(s)
(W/'case-change.diff').write_text(''.join(difflib.unified_diff((W/'case-before.md').read_text().splitlines(keepends=True),s.splitlines(keepends=True),fromfile='case-before.md',tofile='M-035-hiby.md')))
print(json.dumps({'new_E':len(E),'new_F':1,'case_F':10,'old_F':len(up),'old_E_captures':len(recaps),'inventory':len(inv),'visual_sources':1,'static_images':2},ensure_ascii=False))
