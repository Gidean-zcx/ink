import csv,json,re,hashlib,shutil
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches';W=R/'work/remarkable-controls-deepening';A=L/'artifacts/remarkable-controls-deepening';A.mkdir(exist_ok=True)
assert not (B/'remarkable-controls-deepening-coverage.csv').exists(),'one-time builder'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def out(p,rs):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
def eid(n):return f'E-RMCD-{n:02}'
def fid(n):return f'F-M005-{n:02}'
# Distinct new article sources; repeat reads of existing sources stay at their existing E IDs.
specs=[
(2,'How-to-use-the-Meeting-Notes-tool','日历到会议笔记','设置、创建、周期事件、Trash恢复和FAQ',[13,16]),
(3,'Convert-to-Notebook','文档转换为可编辑笔记','导入路径、格式与版式丢失',[11]),
(4,'Read-on-reMarkable-Google-Chrome-Extension','网页文章导入','默认Notebook、EPUB、图片与扩展移除',[17]),
(5,'Web-app','网页文件与分享管理','上传、格式、100MB、Library与连接排查',[10,20]),
(6,'Desktop-app','桌面文件与笔记操作','配对、Connect编辑、Import/Export与平台条件',[10]),
(7,'Document-scanner','手机扫描生成PDF','Create new、Scanned doc、蓝色提示、过滤与保存',[18]),
(8,'Passcode','设备密码与恢复码','设码、等待、Paper Pro恢复码和无恢复码分支',[21]),
(9,'Selection-tool-copy-and-paste','墨迹选择与跨页剪贴板','移动、翻转、连续粘贴、Clear clipboard及转换粘贴',[4,7]),
(10,'Using-typed-text-on-your-reMarkable','键入文字与笔迹的编辑差别','选择手势、复选框、转字及PDF附加笔记页',[7,19]),
(11,'Screen-Share','呈现单文档与停止共享','网页/桌面入口、接受、指针和停止',[20]),
(12,'Performing-a-factory-reset','恢复出厂及同步副本','Check sync、确认、未同步内容及重新配对',[22])]
E=[];notes=['# reMarkable 交互补证记录','','查阅：2026-10-27。主样本11.8英寸Paper Pro；公开帮助、原手册与发布记录分开。浏览帮助页不计产品操作。原HTML/文本留work，正式只保存定位、修订及一张必要手册页。哈希证明字节，不能证明功能实测。','']
pdf=R/'work/eink-systems-research/remarkable-paper-pro.pdf';img=A/'manual-p020.png';shutil.copyfile(W/'guide-p20.png',img)
e=dict(evidence_id=eid(1),product_id='M-005',title='Paper Pro User Guide 原件目录与配对静态页',url='https://image.email.remarkable.com/lib/fe3511737364047c771479/m/1/13f08afe-723d-47df-8849-b850a77ec0de.pdf',checked_on='2026-10-27',source_type='official_manual',access_mode='existing_official_pdf_text_extraction_and_local_page_render',version_scope='RM02A Paper Pro; historical guide; no firmware/build/date stated',app_build=None,firmware_version=None,source_updated_on=None,locator='PDF共69页；p2–4目录、p20账户配对；系统正文p26–33',note='原E-RMK-01此次补原件解析及静态页观察；不是独立第二来源。目录含监管/安全章节，未完整阅读69页。旧设置位置、试用与恢复流程不代表3.29。',hands_on=False,visual_observed=True,visual_kind='manual_page',archived_artifact=str(img.relative_to(L)),sha256=sha(img),capture_state='one_static_manual_page_reviewed; no device operation',raw_work_path=str(pdf.relative_to(R)),raw_sha256=sha(pdf),derived_from='E-RMK-01',supersedes=None)
E.append(e);notes+=['## E-RMCD-01 原手册补观察','',f'- [原PDF]({e["url"]})；[已查看的配对页](manual-p020.png)。',f'- 原件 `{e["raw_work_path"]}`；SHA-256 `{e["raw_sha256"]}`。','- p20照片展示Pair your device、二维码及键盘，正文说明可跳过配对；未实际扫码、输入或验证五分钟时效。p14与p29亦看过，仅作文字核查，未重复归档。','- 目录0–2章覆盖初设、附件和系统；3–6章为各硬件的安全/监管，不能称为完整软件功能目录。','']
for n,slug,title,loc,fs in specs:
 m=json.loads((W/(slug+'.meta.json')).read_text());assert m['status']==200 and m['body_chars']>100
 date=m['published_raw'].split(',')[0];d,mo,y=date.split('.')
 e=dict(evidence_id=eid(n),product_id='M-005',title=title,url=m['url'],checked_on='2026-10-27',source_type='official_help_article',access_mode='public_HTTP_ArticleBody',version_scope='Paper Pro applicable shared help; device build unknown; version/platform conditions in case',app_build=None,firmware_version=None,source_updated_on=f'{y}-{mo}-{d}',locator=loc,note='正文文档重建；不把全系列共享措辞自动套入Paper Pro的显示/配件。',hands_on=False,visual_observed=False,archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256='',capture_state='HTTP200 full ArticleBody saved; text read; no product operation',raw_work_path=m['raw_work_path'],raw_sha256=m['raw_sha256'],parsed_text_path=str((W/(slug+'.txt')).relative_to(R)),http_status=200,supersedes=None)
 E.append(e);notes += [f'## {eid(n)} {title}','',f'- [来源]({m["url"]})，页面更新{e["source_updated_on"]}；定位：{loc}。',f'- 原件 `{m["raw_work_path"]}`；SHA-256 `{m["raw_sha256"]}`。',f'- 关联：{"；".join(fid(i) for i in fs)}。','']
# Rendered directory evidence: rows with href # are titles, not verified destinations.
for n,key,title in [(13,'features-index','功能帮助目录'),(14,'troubleshooting-index','故障帮助目录'),(15,'account-index','账户帮助目录')]:
 links=json.loads((W/(key+'.rendered-links.json')).read_text());u=next(x['url'].split('#')[0] for x in links if '/s/topic/' in x['url'] and '#' in x['url']);p=W/(key+'.rendered.html')
 e=dict(evidence_id=eid(n),product_id='M-005',title=title,url=u,checked_on='2026-10-27',source_type='official_help_index',access_mode='anonymous_browser_read_of_help',version_scope='public help directory; cross-model; not full Paper Pro software menu',app_build=None,firmware_version=None,source_updated_on=None,locator='可见分类和文章标题；锚点href为#，未点击逐条解析目的URL',note='功能页仅Apps/Tools/Features三空分类；故障与账户可列标题。索引不能证明正文已读或功能存在于某构建。',hands_on=False,visual_observed=False,archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256='',capture_state='HTTP200 rendered HTML and visible text; no screenshot or product operation',raw_work_path=str(p.relative_to(R)),raw_sha256=sha(p),parsed_text_path=str((W/(key+'.rendered.txt')).relative_to(R)),http_status=200,supersedes=None)
 E.append(e);notes += [f'## {eid(n)} {title}','',f'- [来源]({u})；{e["locator"]}。',f'- 原件 `{e["raw_work_path"]}`；SHA-256 `{e["raw_sha256"]}`。','']
oldcaptures=[]
for oldid,slug in [('E-RMK-02','Release-notes-overview'),('E-RMK-03','Pair-your-reMarkable-with-the-cloud'),('E-RMK-11','Convert-handwritten-notes-into-text'),('E-RMK-12','Tags'),('E-RMK-18','Create-a-link')]:
 m=json.loads((W/(slug+'.meta.json')).read_text());oldcaptures.append(dict(evidence_id=oldid,checked_on='2026-10-27',**m));notes += [f'## {oldid} 沿用编号的复查','',f'- [来源]({m["url"]})；更新{m["published_raw"]}；原件 `{m["raw_work_path"]}`；SHA-256 `{m["raw_sha256"]}`。','']
notes += ['## 冲突和获取限制','','- Meeting Notes帮助（10月20日）写5场/未来10小时；3.29日志（10月23日）写未来三天。主流程以版本条件保留，三天内最大场数未知。','- 9月4日转字专页称选区转换自动替换原墨迹，Selection页称指定光标再粘贴、可粘另一文档；键入文字页又称选区破坏性。不得推断任意路径都自动粘贴或保留原稿。','- 共享Screen Share帮助称设备显示灰阶；主样本为彩色Paper Pro，未据此给该型号添加灰阶限制。','- 旧手册Check sync在Storage、密码无上限逐次等候及账户重置，与现行Cloud/恢复码路径不一致；旧资料不覆盖当前帮助。','- Notebooks、Organizing-folders、Archive、How-to-use-continuous-pages、Organizing-your-files及Taking-notes-in-the-desktop-app公开路径404；Archive正常浏览也404，停止。Features目录只显示三个空分类，因此没有宣称拿到全软件目录。未遇身份认证后继续访问，也未执行任何设备重置。','- Read on reMarkable文章的EPUB设置段含Print/PDF式参数，步骤疑似混用；只采Notebook/EPUB入口及明确条件，不编造EPUB分页选项。','']
(A/'source-notes.md').write_text('\n'.join(notes));h=sha(A/'source-notes.md')
for e in E:
 if e['archived_artifact'].endswith('source-notes.md'):e['sha256']=h
(B/'remarkable-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
dump(A/'rechecked-existing-evidence.json',oldcaptures)
# Complete primary task records: fields mirror task table, no runtime claims.
new=[
(16,'从日历建立与回访会议笔记','Connect、3.27+、Google/Outlook主日历；一次绑定一个服务','my.remarkable Integrations连接→My files日历→点事件创建含标题/时间/参与者/议程的Notebook；周期事件进入已有本添加一页；对应本在Trash时点事件恢复并打开','编辑笔记不写回日历；创建后保存日历信息副本。Outlook改周期可能产生新series ID和新本','E-RMCD-02;E-RMK-02','帮助5场/10小时与3.29三天不同；取消会议、重新授权、离线、多人和重复创建未核'),
(17,'把网页文章送入可写或可读文档','Chrome扩展已安装/配对；默认Notebook需Connect、图片需3.27','点扩展一键发Notebook；右键选EPUB或Options改默认；另可浏览器Print存PDF后导入','Notebook可编辑，EPUB正文不可直接改但可标注；PDF是另一份导入对象','E-RMCD-04','网页剪裁/登录墙/图片缺失、重复发送、撤销和失败队列未知；EPUB设置段疑似混用打印步骤'),
(18,'用手机扫描多页并存PDF','手机App、网络；摄像头授权流程未给','Create new→Scanned doc→对准纸张，蓝色提示扫描→继续加扫描或保存；生成前选过滤','扫描成为PDF并可同步；不证明OCR、文字层或原照片可回溯','E-RMCD-07','裁剪/重拍/排序/拒绝权限/断网保存、取消和离线结果未知'),
(19,'编辑键入文字及勾选清单','Notebook；屏幕键盘或已连接Type Folio','T→Add/Edit text→宽/窄列；双/三/四击选词/段/本全文→拖柄→剪切/复制/加粗；格式菜单建checkbox，点或打勾后文本划线','键入文字与笔画对象分开；橡皮不擦键入文字；PDF需Add note page，不能直接在原PDF页打字','E-RMCD-10','选区手势容错、撤销历史、跨页全选与键入清单状态对外接口未知'),
(20,'呈现文档并停止共享','已配对云；电脑/网页；网页需3.27+；双方网络可不同','网页先Screen Share，再设备三点→Share→Present；桌面走设备入口后Accept邀请；停止用Stop sharing your screen；桌面悬笔显示指针','每次显示一份文档，My files主页不投出；可切文档/方向。官方网络/防火墙条件不保证任意网络成功','E-RMCD-11;E-RMCD-05','断线/重连、停止是否所有客户端立即生效、音频/延迟未知；灰阶通用措辞不套Paper Pro'),
(21,'设置设备密码并在忘记后恢复','Paper Pro；4/6/8位；恢复码另行保管','Settings Security启用→输入再确认；不匹配重输。错码逐渐延长等待至1小时；多次失败后Reset passcode→恢复码→新密码','恢复码路径重设访问凭据；没有恢复码则官方转恢复出厂，未同步内容不能凭账户找回','E-RMCD-08;E-RMCD-12','错误次数阈值、恢复码丢失与轮换、等待后状态未知；不沿用RM1/2账户审批路径'),
(22,'恢复出厂后重建已同步资料','设备可访问；先检查Cloud同步；不可撤销','Menu→Settings→General Account→Reset→Factory reset确认；重走设置后重新配对','清除设备文件/设置；仅已同步文件可回流，未同步内容永久丢失；不是含所有应用状态的整机镜像','E-RMCD-12;E-RMCD-08','恢复耗时、选择性恢复、同步冲突、断电和固件降级未知；未执行重置')]
F=[]
for n,t,pre,action,data,es,gap in new:F.append(dict(flow_id=fid(n),product_id='M-005',task=t,depth='D2',evidence_ids=es,known_branches=pre+'；'+action+'；'+data,gaps=gap,visual_status='not_observed',hands_on='false',case_path='cases/M-005-remarkable.md',checked_on='2026-10-27'))
out(B/'remarkable-controls-deepening-coverage.csv',F)
# Update old tasks in original batches once, recording every changed field.
changes={
1:dict(evidence_ids='E-RMK-01;E-RMK-03;E-RMCD-01',known_branches='本机可离线；原手册p20可跳过配对；云配对一次性码；支付/Activate Connect为另一阶段；旧指南五分钟时效未实测',gaps='3.29全部首次逐屏、验证码过期实际反馈、初次同步中断；旧单设备账户限制未采',visual_status='observed_static'),
4:dict(evidence_ids='E-RMK-08;E-RMK-09;E-RMCD-09',known_branches='圈选后移动/缩放/旋转；复制或剪切后可跨页重复粘贴，Clear clipboard清空；翻转可拖角越过对边；Undo/Redo与删页分开'),
7:dict(evidence_ids='E-RMK-11;E-RMCD-09;E-RMCD-10',known_branches='整页/多页转换新增文字页保留原稿；选区转换专页称自动替换，Selection页称光标后粘贴并可跨文档，冲突未实测消解',gaps='选区自动/手动粘贴冲突、替换撤销、取消/超时与批量纠错；PDF原页/附加笔记页适用性仍冲突'),
8:dict(evidence_ids='E-RMK-12',known_branches='页与文件/文件夹标签分开；白黑状态、移除关联；Tags总览长按Rename保存；Delete→Yes delete tag移除所有文件/页关联',gaps='多标签组合、改名跨端传播、删除标签恢复、同名大小写未核'),
10:dict(evidence_ids='E-RMK-14;E-RMCD-05;E-RMCD-06',known_branches='USB/app/web格式与大小条件；网页上传PDF/EPUB/JPG/PNG上限100MB，Connect另可PDF/DOCX转Notebook；桌面PDF/PNG/SVG导出不等于原生备份',gaps='rmdoc往返保真、网页PDF转换版式/图层、取消断线、同名重复与超大文件错误'),
11:dict(evidence_ids='E-RMK-10;E-RMK-16;E-RMCD-03',known_branches='云盘导入时选择PDF/Notebook及位置；3.25+Connect文档转换支持DOCX/GoogleDocs/TXT，但去表格/分栏/边距；新版网页PDF转换另列',gaps='绑定授权过期、离线下载、改后上传重名、表格内容可读性和原文件回溯'),
13:dict(evidence_ids='E-RMK-04;E-RMK-02;E-RMCD-02',known_branches='文件Trash恢复/永久删除，3.29加30天自动删；单页删除不进Trash；周期会议本在Trash可点相关日历事件恢复并打开',gaps='30天起算/离线计时、云传播、单页Undo、会议事件恢复是否回原文件夹未核'),
15:dict(evidence_ids='E-RMK-18',known_branches='Share→Convert and share→二维码/自己邮箱；任何持链接者可看/下载，只有所有者登录可编辑；可下载DOCX；链接默认6月到期，所有者菜单可改到期；本机后续修改不更新网页',gaps='范围选择、即时撤销/删除链接、AI错误回溯与语言/地区、多个链接与网页编辑回流未知')}
audit=[]
for p in B.glob('*-coverage.csv'):
 if p.name=='remarkable-controls-deepening-coverage.csv':continue
 rs=list(csv.DictReader(p.open()));dirty=False
 for r in rs:
  for n,fields in changes.items():
   if r['flow_id']!=fid(n):continue
   for k,v in {**fields,'checked_on':'2026-10-27'}.items():
    if r[k]!=v:audit.append(dict(file=str(p.relative_to(R)),flow_id=r['flow_id'],field=k,before=r[k],after=v));r[k]=v;dirty=True
 if dirty:out(p,rs)
dump(W/'applied-existing-updates.json',audit)
# Preserve repeat-source acquisition metadata without duplicating E IDs.
for p in B.glob('*-evidence.jsonl'):
 rs=[json.loads(l) for l in p.read_text().splitlines() if l];dirty=False
 for e in rs:
  cap=next((c for c in oldcaptures if c['evidence_id']==e['evidence_id']),None)
  if cap:e.setdefault('additional_captures',[]).append(cap);dirty=True
 if dirty:p.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in rs))
# Raw manual TOC, including non-function sections; preserve printed pagination.
pages=json.loads((W/'guide-pages.json').read_text());pairs=[];pending=[]
for line in '\n'.join(pages[1:4]).splitlines():
 line=line.strip()
 if not line:continue
 if line.isdigit():pairs.append((' '.join(pending),int(line)));pending=[]
 else:pending.append(line)
assert not pending
inv=[]
for i,(title,page) in enumerate(pairs,1):
 code=re.match(r'\d+(?:\.\d+)*',title);code=code.group() if code else '';non=bool(code and int(code.split('.')[0])>=3)
 mapped=[]
 for needle,ns in [('account',[1,22]),('passcode',[21]),('Security',[21]),('Factory reset',[22]),('syncing',[12]),('Handwriting',[7]),('onscreen',[19]),('Type Folio',[19])]:
  if needle.lower() in title.lower():mapped+=ns
 inv.append(dict(entry_id=f'M005-P{i:03}',parent_id=code.rsplit('.',1)[0] if '.' in code else '',entry_type='manual_nonfunctional_heading' if non else 'manual_heading',scope='historical RM02A/RM08B/RM04C/RM05C guide',title=title,url=E[0]['url']+'#page='+str(page),evidence_ids=eid(1),related_flow_ids=';'.join(fid(n) for n in sorted(set(mapped))),review_status='toc_only_nonfunctional' if non else 'toc_and_selected_body_read',gaps='旧指南印刷页码；非当前软件全菜单，正文核读仅p6–7/12–15/20/26–33，任务与版本以案例为准',checked_on='2026-10-27'))
for n,key in [(13,'features-index'),(14,'troubleshooting-index'),(15,'account-index')]:
 links=json.loads((W/(key+'.rendered-links.json')).read_text());rows=[x for x in links if x['url'].endswith('#') and x['text'] not in ['×','Refresh']]
 if n==13:rows=[dict(text=t,url=next(e['url'] for e in E if e['evidence_id']==eid(n))) for t in ['Apps','Tools','Features']]
 for j,x in enumerate(rows,1):inv.append(dict(entry_id=f'M005-H{n}-{j:02}',parent_id='',entry_type='help_empty_category' if n==13 else 'help_visible_title',scope='current shared support index; no exact device build',title=x['text'],url=x['url'],evidence_ids=eid(n),related_flow_ids='',review_status='visible_index_only',gaps='标题可见；href#不提供文章地址；Features三类为空；不将此计为正文/全功能覆盖',checked_on='2026-10-27'))
out(B/'remarkable-controls-deepening-inventory.csv',inv)
# Keep prior narrative/history and correct superseded claims explicitly.
case=L/'cases/M-005-remarkable.md';s=case.read_text();assert s==(W/'M-005-before.md').read_text()
s=s.replace('查阅：2026-10-26。','查阅：2026-10-26；原件及交互补证：2026-10-27。',1)
s=s.replace('官方PDF成功下载但未本地渲染/完整解析，不将其算视觉证据。','原轮PDF仅取得；10月27日已解析69页文字、逐项整理印刷目录并查看p14/p20/p29，正式归档p20单帧。没有逐页完整研读或设备操作。')
s=s.replace('Meeting Notes、Miro/Slack、Methods所有模板、企业管理未深入','Meeting Notes见后续任务；Miro/Slack、Methods所有模板、企业管理仍未深入')
s=s.replace('完整首次逐屏、密码错误/重置、休眠唤醒位置、后台中断','完整首次逐屏、密码见后续补证、休眠唤醒位置、后台中断')
s=s.replace('没有点击本机、测试手写、测耗时或实拍屏幕。官网下载的历史PDF未成功经web截图/本地渲染观察，不能计单帧；网页索引的图片描述也不替代查看。','没有点击本机、测试手写、测耗时或实拍屏幕。10月27日补看原手册静态配对图；此前未渲染的限制已解除，但仍不是当前UI实操。')
s=s.replace('空库、满盘、错误密码、强制重启、同步冲突、误删单页的可恢复性仍需设备级观察。','空库、满盘、密码恢复与重置的实际结果、同步冲突、误删单页的可恢复性仍需设备级观察。')
# Edit existing rows so readers cannot miss revised behavior by reading only the original table.
replacements={
1:('云配对从my.remarkable.com取得一次性码，输入设备后Continue。','云配对从my.remarkable.com取得一次性码，输入设备后Continue；旧指南p20明确可跳过。'),
4:('笔画编辑不等于删页；工具栏可移动，部分工具随位置移进Document settings。','剪切/复制的内容可跨页重复粘贴直至Clear clipboard；移动/缩放/旋转与翻转均属选区操作。笔画编辑不等于删页。'),
7:('Selection选手写→小写a转换则同页替换。','转字专页说Selection小写a自动同页替换；Selection帮助说指定光标再粘贴且可跨文档，冲突尚未实测。'),
8:('页标签与文件标签是两层对象；去标签不删内容。','Tags总览长按可Rename→Save，Delete→Yes delete tag移除全部文件/页关联；移除某项标签与删除标签本身不同。'),
10:('普通输出格式不可自动等同原生可编辑笔记备份。','网页可上传PDF/EPUB/JPG/PNG；Connect可将PDF/DOCX转Notebook。普通输出格式不可自动等同原生备份。'),
11:('转换成Notebook改变后续编辑能力。','Convert to Notebook文档页列DOCX/GoogleDocs/TXT且会去表格、分栏、边距；PDF另见新版网页帮助。'),
13:('恢复时限随版本变化。','恢复时限随版本变化；会议笔记在Trash时点对应日历事件也会恢复并打开。'),
15:('官方说明AI汇总和下一步；访问者可不注册查看，编辑需登录。原稿、识别文字与AI总结应区分。','持链接者可查看/下载DOCX；只有所有者登录可编辑。默认6个月到期可改；设备后续修改不会更新网页。')}
lines=s.splitlines()
for i,line in enumerate(lines):
 for n,(a,b) in replacements.items():
  if line.startswith('| '+fid(n)+' |'):
   assert a in line,(n,a);line=line.replace(a,b);cells=line.split('|');cells[6]=' '+changes[n].get('evidence_ids',cells[6].strip()).replace(';','；')+' '
   if 'gaps' in changes[n]:cells[7]=' '+changes[n]['gaps']+'。 '
   lines[i]='|'.join(cells)
s='\n'.join(lines)+'\n'
s=s.replace('Selection转换替换选中墨迹。先辨入口，不能笼统说“转换永远保留原稿”。[E-RMK-11]','Selection专页说替换选中墨迹，另一帮助说选择光标粘贴；保留该冲突，不能笼统说“转换永远保留原稿”。[E-RMK-11、E-RMCD-09、10]')
s += '\n## 2026-10-27：目录、跨端副本及恢复补证\n\n'
s += f'新增[逐条目录](../batches/remarkable-controls-deepening-inventory.csv)共{len(inv)}行：原指南{len(pairs)}个章/节与帮助可见标题/空分类。父项与非功能安全章都保留，不是独立功能数；功能帮助当前只显示Apps/Tools/Features空分类。原手册没有完整应用工具栏目录，不能用其覆盖全软件。原件、复查记录和限制见[来源记录](../artifacts/remarkable-controls-deepening/source-notes.md)。\n\n'
s += '| F编号 | 任务/前提 | 操作及结果 | 数据与恢复 | E证据 | 待验证 |\n| --- | --- | --- | --- | --- | --- |\n'
for n,t,pre,action,data,es,gap in new:s+=f'| {fid(n)} | {t}；{pre} | {action} | {data} | {es.replace(";","；")} | {gap} |\n'
s += '\n需要保留的版本条件：会议帮助10月20日写5场/未来10小时，10月23日3.29发布记录改为未来三天，三天内数量上限未知。旧手册的Storage同步入口与账户密码恢复不能覆盖当前Cloud和Paper Pro恢复码路径。Screen Share共享帮助的灰阶描述不套入彩色Paper Pro。网页导入支持PDF转笔记与较早Convert to Notebook格式列表按入口分别记录。\n\n对G-002的候选启发：页面相似不代表数据联动——日历笔记是副本，分享网页不随设备修改，投屏是单文档呈现。AI执行前后需能够说明修改哪个对象、是否传播、如何恢复；这只是从竞品得到的设计问题，还未确定本项目接口或账户方案。对G-001，网页导入路径可帮助积累主题资料，但文章内容质量与转换可读性仍须评估，不能用成功上传替代学习收益。\n'
s += '\n补充证据链接及版本、原件散列见[来源记录](../artifacts/remarkable-controls-deepening/source-notes.md)与[原有来源复查](../artifacts/remarkable-controls-deepening/rechecked-existing-evidence.json)。所有新增任务均D2；只有原指南一张静态页，没有新增设备/网页产品实操。\n'
case.write_text(s)
dump(W/'build-report.json',dict(new_evidence=len(E),new_flows=len(F),old_flows=len(changes),changed_fields=len(audit),inventory_rows=len(inv),manual_toc=len(pairs),help_rows=len(inv)-len(pairs),static_images=1))
print((W/'build-report.json').read_text())
