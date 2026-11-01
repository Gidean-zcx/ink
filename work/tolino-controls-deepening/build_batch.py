from pathlib import Path
import json,csv,re,hashlib,shutil
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches';W=R/'work/tolino-controls-deepening';A=L/'artifacts/tolino-controls-deepening';A.mkdir(exist_ok=True)
assert not (W/'applied-existing-updates.json').exists(), 'one-time builder already applied'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def jd(p,o):p.write_text(json.dumps(o,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rs):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
S={x['name']:x for n in ['sources.json','sources-extra.json','sources-new.json'] for x in json.loads((W/n).read_text())}
notes='''# Tolino T302 本轮来源摘记

查阅2026-10-27。固定黑白shine第五代T302。英文2024年12月指南、当前德/英FAQ和第三方借阅服务分别处理；未操作电子纸设备。原件留在work，正式材料只保留必要摘记、书签图标所在手册页和公开网页流程截图。

- E-TOL-01：复用50页原PDF，解析p2–4目录；主要核读p5–15、20–50的使用章节，未完整研读涉水/安全章节。p32/36渲染为空页不作界面证据。p39实际查看：书签回访步骤包含纸页形图标，但第三步英文措辞不清，不能据其补出完整列表操作。E-TLCD-01是同一原件静态观察，非独立来源。
- E-TOL-02：重读英文型号FAQ的49问及8分组。4.1/4.2大字模式重复；9.4混入旧11.2.2版本条件，不作为T302的5.x最低构建。删除云端与仅删本地、收藏与书籍、家庭与书店关联、TTS与Screen Reader逐项区分。
- E-TOL-03：重取英文更新页仍列5.12.232736。E-TLCD-06德文当前T302分组列5.18.264769；范围5.15.253099/253009不一致，另一行出现升级目标未递进。只保存差异，不替厂商修正、不下载或试刷固件。该代专用步骤为.kobo内KoboRoot.tgz/update.tar，型号FAQ却写update.zip。
- E-TLCD-02：德文型号FAQ仅深入读取1.3自制休眠图、5音频、6辅助读屏、7家庭/跨书店、8TTS等相关段；其余标题列目录不代表逐问研读。TTS段限定Kobo EPUB和EPUB。自制图需USB复制、两项显示开关及先打开一本书，多个文件在睡眠时随机选；没有自定轮播顺序证据。
- E-TLCD-03：家庭FAQ29问及父组。明确各自账户保存各自进度和笔记，不能互看完整书库；分享受出版方授权限制，自传书/PDF不适用；每次选一成员，重复分享给其他人。最多六位同住成员、12个月换家庭一次是该服务规则，不是本项目需求。离开后双方分享书访问被撤销；离线缓存何时失效、退出后的个人注释保留不明。1.20旧文称阅读器不能听/分享有声书，与当前T302专页不一致，不用于否定新机音频能力。
- E-TLCD-04/05：新Onleihe入口从5.15.253009起为More→Onleihe，保留旧代浏览器路线单列。只直接借电子书；首次LCP书需四位reader code。选择记录、图书馆登录、凭证可选保存、借期/版权码是不同状态。清选馆记录的帮助操作会清Web-Browser-Daten，未证明只清该记录。该页OverDrive Adobe旧说法不能覆盖E-TOL-05现行ByteBooks迁移。
- E-TLCD-07：桌面匿名公开选馆器实际操作；搜索zzqvexample为空列表，Hamburg出现两结果；点Bücherhallen Hamburg跳转meine.onleihe.de/welcome并仍需选馆。浏览器后退时输入Hamburg仍在而列表回A字头，回Tolino页并刷新后出现该馆最近选择。没登录也出现记录；不推断其服务端持久性。完整操作边界见审计。

文字摘记为中文转述。以上资料没有给出批注完整编辑/导出、TTS离线包、断电同步冲突、真实耗电/刷新或5–6岁独立操作效果。
'''
(A/'source-notes.md').write_text(notes)
shutil.copyfile(W/'manual-p039.png',A/'manual-p039.png')
# Audit the bounded actual browser operation. Captures are not atomic screenshot+DOM snapshots.
ba=A/'browser';ba.mkdir(exist_ok=True)
reviewed={'01-home','03-select-library','04-no-matches','05-hamburg-results','07-library-loaded','08-back-to-finder','10-home-reloaded'}
selected=reviewed-{'03-select-library'}
observations={
'01-home':'截图为欢迎和选择按钮；随后DOM已有cookie对话框。截图/DOM非原子，不据DOM断言截图可见弹窗。',
'02-essential-cookies':'点击仅必要cookie后的原始捕获，未逐图审阅；随后导航成功。',
'03-select-library':'图像实际查看为空白过渡帧，DOM已有初始列表；不纳入正式UI截图。',
'04-no-matches':'搜索zzqvexample：输入保留，列表区域空白，没有可见无结果说明。',
'05-hamburg-results':'搜索Hamburg：Bücherhallen Hamburg、Wentorf bei Hamburg两结果。',
'06-selected-library':'点击首结果后捕获处于外部站加载态；仅原始记录，未逐图审阅。',
'07-library-loaded':'meine.onleihe.de/welcome，DOM显示Onleihe3.17.0、仍需选馆；截图有cookie遮罩，未登录。',
'08-back-to-finder':'浏览器后退：搜索框仍Hamburg，实际列表从Aabenraa/Aachen开始；未重新触发检索。',
'09-home-after-selection':'点击页面左上返回后早期DOM无最近项；未逐图审阅，不据此否认持久性。',
'10-home-reloaded':'刷新首页后最近选择出现Hamburg，未进行登录；不证明跨浏览器/设备保留。'}
states=[];raw=[]
for p in sorted((W/'browser').glob('[0-9][0-9]-*.json')):
 d=json.loads(p.read_text());name=d['name'];s=dict(state=name,url=d['url'],utc=d['utc'],observation=observations[name],screenshot_reviewed=name in reviewed,visible_inputs=[i for i in d['inputs'] if i['visible']])
 if name in selected:
  shutil.copyfile(p.with_suffix('.png'),ba/(name+'.png'));s['archived_screenshot']=str((ba/(name+'.png')).relative_to(L))
 states.append(s)
for p in sorted((W/'browser').glob('*')):
 if p.is_file():raw.append(dict(path=str(p.relative_to(R)),sha256=sha(p)))
jd(ba/'observed-states.json',states);jd(ba/'raw-capture-manifest.json',raw)
actions=[dict(utc='2026-10-26T18:32:31+00:00',command="Fresh anonymous Chromium context; page.goto('https://mytolino.de/onleihe/') HTTP200; capture('01-home')",note='Initial command from browser_session.py, timestamp first capture; not navigation timing.')]+[json.loads(x) for x in (W/'browser/actions.jsonl').read_text().splitlines()]
actions.append(dict(command='quit; browser context closed',exit_status=0))
jd(ba/'trace-actions.json',actions)
jd(ba/'browser-errors.json',[json.loads(x) for x in (W/'browser/errors.jsonl').read_text().splitlines()] if (W/'browser/errors.jsonl').exists() else [])
audit='''# 公开选馆器有限实际操作审计

E-TLCD-07；F-M058-19。2026-10-27（UTC日志为10月26日晚），Linux Chromium156.0.8078.4，1280×950，de-DE，新建匿名上下文；研究输入Hamburg和zzqvexample，不是用户所在城市。目标是公开选馆器，而非Tolino设备、已登录借书或Onleihe全站。

从公开Tolino首页进入，接受仅必要cookies，打开选馆器，填无匹配词再改Hamburg，点击Bücherhallen Hamburg；外部Onleihe欢迎页仍要求选馆。随后浏览器后退，点可见左上箭头回Tolino首页并刷新，出现最近选馆记录。这里未登录，不能把该记录当成已取得借阅授权。会话以quit正常关闭，exit0；没有账号、凭证、借书、购买、消息或文件下载。

10次状态捕获；逐图查看01/03/04/05/07/08/10，正式保留6张（03为空白过渡帧）。未查看的02/06/09不冒充视觉确认。截图和随后DOM按顺序采集，不是原子状态：01图尚无cookie层而DOM已有；03图为空白而DOM有列表；09早期DOM尚无最近项，10刷新后明确出现。不能把这些过渡差异当成稳定产品状态。

已观察：无结果空列表、两条Hamburg结果、外部欢迎页的重新选馆分支、后退后输入词和列表不一致、匿名同会话刷新后的最近项。未观察：再登录、借阅成功、退出登录、清浏览器数据后的结果、跨设备同步、真实T302点击和刷新。帮助承诺直接登录与本次落点不符，原因可能包括服务迁移或上下文条件，未定位根因，不能宣布所有用户均失败。

[逐状态记录](browser/observed-states.json)、[实际命令日志](browser/trace-actions.json)、[错误记录](browser/browser-errors.json)、[原始捕获散列](browser/raw-capture-manifest.json)。命令日志是Playwright显式动作日志，不是浏览器原生trace包；没有用脚本修改产品内部状态。

- [进入页](browser/01-home.png)
- [无结果](browser/04-no-matches.png)
- [匹配结果](browser/05-hamburg-results.png)
- [外部欢迎页](browser/07-library-loaded.png)
- [后退后的输入与列表](browser/08-back-to-finder.png)
- [刷新后最近选择](browser/10-home-reloaded.png)
'''
(A/'browser-operation-audit.md').write_text(audit)
oldE=[json.loads(l) for l in (B/'extended-readers-evidence.jsonl').read_text().splitlines()];byE={e['evidence_id']:e for e in oldE}
echanges=[]
manual=R/'work/extended-readers/tolino-shine5.pdf'
for eid,name in [('E-TOL-01',None),('E-TOL-02','faq'),('E-TOL-03','updates')]:
 e=byE[eid];before=json.loads(json.dumps(e));cap=dict(checked_on='2026-10-27',archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md'),access_mode='existing_PDF_reinspection' if name is None else 'public_HTTP200_recheck',raw_work_path=str(manual.relative_to(R)) if name is None else S[name]['raw_work_path'],raw_sha256=sha(manual) if name is None else S[name]['raw_sha256'])
 e.setdefault('additional_captures',[]).append(cap);echanges.append(dict(evidence_id=eid,before=before,after=e))
(B/'extended-readers-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in oldE));jd(W/'applied-existing-evidence-updates.json',echanges)
E=[]
def evidence(num,title,name,locator,note):
 s=S[name];e=dict(evidence_id=f'E-TLCD-{num:02}',product_id='M-058',title=title,url=s['url'],checked_on='2026-10-27',source_type='official_help_article',access_mode='public_HTTP200_HTML_text_extraction',version_scope='tolino shine fifth generation T302; current shared help, unknown installed build',app_build=None,firmware_version=None,source_updated_on=None,locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md'),capture_state='body sections read; no device operation',raw_work_path=s['raw_work_path'],raw_sha256=s['raw_sha256'],parsed_text_path=s['parsed_text_path'],supersedes=None);E.append(e);return e
E.append(dict(evidence_id='E-TLCD-01',product_id='M-058',title='T302原手册p39书签图标与回访措辞',url=byE['E-TOL-01']['url'],checked_on='2026-10-27',source_type='official_manual',access_mode='existing_official_PDF_render_and_visual_review',version_scope='December2024 English T302 guide; no installed build',app_build=None,firmware_version=None,source_updated_on=None,locator='PDF50页；p2–4目录；p39实际查看；阅读章节见摘记',note='同E-TOL-01原件；图标存在，第三步英文措辞不清；不是设备操作。p32/36空白不作界面证据。',hands_on=False,visual_observed=True,visual_kind='manual_page',archived_artifact=str((A/'manual-p039.png').relative_to(L)),sha256=sha(A/'manual-p039.png'),raw_work_path=str(manual.relative_to(R)),raw_sha256=sha(manual),capture_state='one necessary static manual page archived',derived_from='E-TOL-01',supersedes=None))
evidence(2,'当前德文T302 FAQ与休眠图/TTS条件','faq-de','1.3、5、6、7、8；全标题登记但部分正文深入阅读','不把其他代际条件继承到T302；8.2限定Kobo EPUB/EPUB；自制图需先开书。')
evidence(3,'家庭分享：成员、选书、权限与离开','family','1.1–1.29','旧1.20音频限制与T302冲突；仅按适用范围采用。')
evidence(4,'新Onleihe入口与最低版本','onleihe-new','新版入口、支持型号、5.15.253009、清数据及媒体范围','不把服务登录、最近图书馆和DRM授权合一。')
evidence(5,'按代际的Onleihe借阅步骤','onleihe-help','A当前代I–IV；B旧代仅作范围区分','OverDrive旧Adobe段与2026服务现行文档有冲突；不以该段覆盖E-TOL-05。')
evidence(6,'德文T302更新表与专用USB步骤','updates-de','shine5/shine color版本表；该代专用USB步骤','列5.18.264769；表内253099/253009及非递进目标疑点；未下载或安装固件。')
e=dict(evidence_id='E-TLCD-07',product_id='M-058',title='公开图书馆搜索、跳转、后退与最近选择',url='https://mytolino.de/onleihe/',checked_on='2026-10-27',source_type='public_web_application',access_mode='automated_public_browser_actions_and_screenshot_review',version_scope='Anonymous desktop de-DE; Chromium156.0.8078.4 Linux1280x950; not T302; destination Onleihe footer3.17.0',app_build=None,firmware_version=None,source_updated_on=None,locator='01–10；6正式截图；7图已查看含1空白过渡帧',note='只验证公开选馆器有限任务；没有登录/借阅；外部落点与帮助理想流程不同，原因未知。',hands_on=True,visual_observed=True,visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=str((A/'browser-operation-audit.md').relative_to(L)),operation_audit_sha256=sha(A/'browser-operation-audit.md'),archived_artifact=str((A/'browser-operation-audit.md').relative_to(L)),sha256=sha(A/'browser-operation-audit.md'),capture_state='actions, state metadata and six reviewed images archived',supersedes=None,artifact_hashes={str(p.relative_to(L)):sha(p) for p in sorted(ba.glob('*'))});E.append(e)
(B/'tolino-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in E))
F=[]
def flow(n,task,es,known,gap,depth='D2',visual='not_observed'):
 F.append(dict(flow_id=f'F-M058-{n:02}',product_id='M-058',task=task,depth=depth,evidence_ids=es,known_branches=known,gaps=gap,visual_status=visual,hands_on='true' if depth=='D4' else 'false',case_path='cases/M-058-tolino.md',checked_on='2026-10-27'))
flow(10,'按书库或商店范围检索标题作者','E-TOL-01','首页放大镜→下拉选Shop或My Books→输入标题/作者→Go；后者搜索账户及本机内容','空结果提示、去重、全文检索、离线商店回退未核')
flow(11,'建立与维护主题收藏集合','E-TOL-02','My Books→Collections→三点New collection→命名选书；长按集合Edit→Add books加号加入/取消勾选移出；长按Rename/Delete；按名称/书数/创建日排序；文档称可在app/web看集合','重名、空集合、删除是否影响书、离线冲突及撤销未核')
flow(12,'目录跳转与书签回访','E-TOL-01;E-TLCD-01','阅读页中→滑杆快速翻页，或底部目录→选章；点正文右上加书签可见标记；回访页中→底部纸页图标→Bookmarks，手册第三步措辞不清','书签列表完整顺序、删除/撤销及跳转后回原处未核','D2','static_manual_pages_only')
flow(13,'排版与系统大字分别调整','E-TOL-01;E-TOL-02','阅读页中→文字图标→字体下拉、字号滑杆或加减、行距/边距加减、对齐；点外部回阅读；设置应用后续书；PDF只缩放；系统Accessibility另设150/200%菜单大字','极端字号重排、阅读位置稳定性与格式例外未核')
flow(14,'家庭成员建立、按书分享与退出','E-TOL-02;E-TLCD-03','More→Settings→Accounts→My Family View；昵称→设备交给邀请对象选书店并登录→昵称；合资格书长按Share选一个成员，可重复；Info查看访问者；各自进度笔记，完整书库互不可见；成员旁菜单移除，自己菜单离开撤销相互共享访问','未实际建家庭；离线撤权时点、孩子独立账号资格、退出后注释保留和有声书分享当前控件未核')
flow(15,'连接同一用户的多个书店书库','E-TOL-02;E-TOL-06','Webreader My Accounts→Library link→选已有账户书店Link→用该店凭证确认；App先选国家/书店再同路径；成功后经cloud载入购买书','地区条件、解除关联、重名去重、不同DRM及认证失败恢复未核；不等同家庭档案')
flow(16,'蓝牙键盘辅助读屏与退出阅读','E-TOL-02','需BT音频和键盘；More Settings Accessibility Screen Reader开启或Alt+Windows/Command+S；箭头选书，空格下载/开书；Ctrl+T读/停，箭头翻页，Ctrl+Q回My Books','焦点顺序、断连恢复、首次下载反馈、语音资源离线和儿童操作均未核')
flow(17,'自制休眠图启用与撤销','E-TLCD-02','USB连接确认→.kobo/screensaver放PNG/JPEG→安全弹出；Energy saving & PIN中当前书及全屏两项开启，先开一本书再睡眠；多图随机；移除图文件回书封面或关闭两项显示','分辨率/裁切、锁屏信息遮挡、文件损坏与批量同步未核')
flow(18,'PIN设置、解锁与遗忘恢复','E-TOL-02','More Settings Energy saving & PIN→设四位码；Change PIN可关闭；忘记密码从PIN提示Log out并确认，退号后重新设置','退出会丢最后同步后阅读进度/批注；尝试次数、锁定时间、未联网恢复未核')
flow(19,'公开选馆器搜索、返回与最近项','E-TLCD-04;E-TLCD-05;E-TLCD-07','匿名桌面：选择图书馆→无匹配词空列表→Hamburg两结果→选Bücherhallen Hamburg跳外站welcome仍需选馆→浏览器后退保留词但列表A字头→左上返回→刷新出现最近图书馆；没有登录','只限桌面公开选择器；未借阅、未测跨会话/设备或清数据；无法判定外部重新选馆原因','D4','observed_browser_sequence')
csvout(B/'tolino-controls-deepening-coverage.csv',F)
# Existing tasks are enriched in place, never duplicated with new F numbers.
p=B/'extended-readers-coverage.csv';rows=list(csv.DictReader(p.open()));changed=[]
extra={
1:('睡眠回原屏幕/页面，完整关机再开回首页；睡眠会关闭WiFi并可能中断同步。','未保存编辑、睡眠中断后重试与书签同步结果未核','E-TOL-01'),
2:('USB支持PDF/EPUB/TXT/MOBI/CBZ/CBR复制至Books并安全弹出；云上传EPUB/PDF，同步后点封面下载再离线读；封面可见不等于本机有文件。','重名、半文件、云配额/冲突及自传书的跨端进度未核','E-TOL-01;E-TOL-02;E-TOL-06'),
3:('正文长按词查词/翻译；More Settings Language & Dictionaries→Installed Edit；加号增词典、取消勾选删后Save。','无命中提示、取消与回原句、词典包离线和生词练习未核','E-TOL-02'),
4:('蓝牙Connected后My Books打开有声书；播放/停、前后30秒、时间条、章节、1x速度、音量及章/全书剩余时长；章节需下载；播放三点有Details/Dictionary；配对失败两端关开蓝牙重试。','断连/重启续播、收听进度同步、与正文锚点对齐及实际声输出未核','E-TOL-01;E-TOL-02;E-TLCD-02'),
5:('页中→三点Read Aloud，原菜单Stop；德文8.2限定Kobo EPUB/EPUB。','语言/音色、离线包、最低构建、PDF支持和读听定位未核；不得把Screen Reader条件套此入口','E-TOL-02;E-TLCD-02'),
6:('长按封面Remove选择本地或cloud；本地默认只去下载且封面保留，云删除影响多端；Manage books可多选/全选；误删云书需联系书店。','客服恢复不保证；自传书删除回收、断网传播、撤销及注释连带变化未核','E-TOL-01;E-TOL-02'),
7:('手动Sync now或关闭电子书自动同步，后者需两端在线；最多五设备是账户限制。普通强制关机不等于重置。退出账户需先同步未保存书签/笔记；重置清本机书/侧载文件/未同步注释及设置。手动恢复需接电、按至关机再长按至LED快闪结束，出现Restore后重设联网。','同步冲突、未联网登录、恢复完整性、刷机回退和侧载原件备份未核','E-TOL-01;E-TOL-02'),
8:('OverDrive按原服务ByteBooks迁移；Onleihe当前T302从5.15.253009 More→Onleihe→选城市/馆→登录→搜书Borrow/OK→打开；LCP首次四位reader code。三点或长按封面Return；只直接借电子书。帮助称清Web-Browser-Daten可清最近馆，但范围不止选馆未证明。','两服务授权不可混用；Onleihe直接登录在桌面选馆器本次未实现，见F-M058-19；实际T302借书/到期/断网仍未核','E-TOL-04;E-TOL-05;E-TLCD-04;E-TLCD-05'),
9:('优先联网同步检查更新；USB依型号/当前版本选文件、电量>20%、连接确认、KoboRoot.tgz/update.tar放.kobo安全弹出后自动重启。英站仍5.12.232736，德文T302表列5.18.264769；FAQ的update.zip与专页不一，德文表亦有版本拼写/非递进目标疑点。','未下载固件；不把网站current当装机版本；升级链冲突、失败回退和数据完整性需官方/实机核实','E-TOL-02;E-TOL-03;E-TLCD-06')}
for r in rows:
 if r['product_id']!='M-058':continue
 n=int(r['flow_id'][-2:]);known,gap,es=extra[n]
 for field,value in dict(known_branches=known,gaps=gap,evidence_ids=es,checked_on='2026-10-27').items():
  if r[field]!=value:changed.append(dict(flow_id=r['flow_id'],field=field,before=r[field],after=value));r[field]=value
csvout(p,rows);jd(W/'applied-existing-updates.json',changed)
# Inventory: keep parent headings and duplicates; it is not a feature denominator.
I=[]
def inv(source,items,eid,kind,scope):
 parent=''
 for title,locator,isparent in items:
  num=len(I)+1;entry=f'M058-I{num:03}';links=[]
  lt=title.lower()
  for pat,ns in [('audiobook',[4]),('hörb',[4]),('bluetooth',[4,16]),('screen',[16,17]),('pin',[18]),('collection',[11]),('samml',[11]),('family',[14]),('famil',[14]),('bibliothe',[15,19]),('link',[15]),('bookmark',[12]),('font',[13]),('print',[13]),('read aloud',[5]),('vorles',[5]),('sync',[7]),('reset',[7]),('update',[9]),('dictionary',[3]),('wörter',[3]),('delete',[6]),('borrow',[8]),('onleihe',[8,19]),('wi',[7]),('sleep',[1,17]),('turning',[1]),('flick',[12]),('contents',[12])]:
   if pat in lt:links+=ns
  links=sorted(set(links));fids=';'.join(f'F-M058-{n:02}' for n in links)
  I.append(dict(entry_id=entry,product_id='M-058',source=source,entry_type=kind+('_group' if isparent else '_topic'),parent_id='' if isparent else parent,title=title,locator=locator,evidence_ids=eid,flow_ids=fids,body_review_status=scope,coverage='task_reference_partial' if links else 'inventory_only_or_nonfunctional',gap='标题/关联不是完整任务验证；细节与限制见案例，未操作T302'))
  if isparent:parent=entry
pages=json.loads((W/'manual-pages.json').read_text());mi=[]
for text in pages[1:4]:
 for line in text.splitlines():
  m=re.match(r'(.+?)\s*\.{2,}\s*(\d+)\s*$',line)
  if m:mi.append((m.group(1).strip(),'PDF p'+m.group(2),len(re.findall(r'\.',line))<60))
# Chapter headers are determined by their exact TOC titles, not inferred from dot widths.
chapters={'tolino shine - the basics','Using the WiFi on your eReader','Listening to audiobooks','Buying a book','How to find new books on your eReader','Reading a book on your eReader','Family Sharing','Borrowing books from a public library','Troubleshooting on your eReader','Need help?'}
mi=[(t,l,t in chapters) for t,l,_ in mi]
inv('English Dec2024 PDF',mi,'E-TOL-01','manual','目录全文提取；使用相关正文选读，非50页逐页分析')
for name,eid,scope in [('faq','E-TOL-02','英文问题正文核读；非交互问题仅作边界'),('faq-de','E-TLCD-02','仅指定相关段深读；其他为目录发现，见source-notes'),('family','E-TLCD-03','29问正文核读；旧型号/音频冲突保留')]:
 hs=json.loads((W/(name+'-headings.json')).read_text());items=[(x['title'],re.match(r'\d+(?:\.\d+)*',x['title']).group(),x['tag']=='h3') for x in hs if re.match(r'^\d+\.',x['title'])]
 inv(name,items,eid,'faq',scope)
csvout(B/'tolino-controls-deepening-inventory.csv',I)
# Replace the case with a coherent current version; the prior case is retained in work.
case=['# M-058 Tolino：shine第五代T302，家庭书库与借阅状态','','查阅2026-10-27。固定**黑白tolino shine第五代T302**，英文2024年12月指南与现行德/英FAQ分开。英站列5.12.232736，德文型号更新表列5.18.264769且有表内疑点；均非实机已安装构建。地区、联盟书店和图书馆资格分别适用，不混用shine3/4、color或vision的操作。','','本案例含19项任务，其中设备及账户任务来自D2文档/静态页；**唯一D4是桌面匿名公开图书馆选择器**，不代表T302实机、账号登录或借阅已验证。[逐条目录](../batches/tolino-controls-deepening-inventory.csv)与[来源摘记](../artifacts/tolino-controls-deepening/source-notes.md)保留父项、重复问题和未深读条目，不是全功能完成率。','','## 已有任务的分支补强','','| 任务 | 条件、动作和数据变化 | 缺口与证据 |','| --- | --- | --- |']
for r in rows:
 if r['product_id']=='M-058':case.append(f"| {r['flow_id']} {r['task']} | {r['known_branches']} | {r['gaps']}。{r['evidence_ids'].replace(';','、')}；D2 |")
case+=['','## 本轮补入的操作任务','']
for f in F:
 case+=['### '+f['flow_id']+' '+f['task'],'',f['known_branches']+'。','',f['depth']+'；'+f['evidence_ids'].replace(';','、')+'。尚未核实：'+f['gaps']+'。','']
case+=['## 版本、服务与数据边界','','1. 同账户最多五设备与家庭最多六人是不同维度。家庭各自账户、各自读书位置与笔记，内容共享不等于共享完整书库；自传书/PDF不因家庭关系自动获得分享能力。当前T302音频帮助不能被旧家庭FAQ“阅读器不能听书”覆盖。E-TOL-02、E-TLCD-03。','2. 普通TTS、辅助Screen Reader、有声书各有入口和条件。辅助读屏需蓝牙键盘+音频，不能据此认定所有TTS也必须键盘；德文普通TTS限定EPUB。听书位置与阅读位置未证明逐句对齐。E-TOL-01、02、E-TLCD-02。','3. 退出账户、PIN遗忘、菜单出厂恢复和电源键手动工厂恢复都可能丢未同步数据；普通强制关机是另一种行为。睡眠中断联网同步不能自动解释为数据已经备份。E-TOL-01、02。','4. 2026 OverDrive使用ByteBooks ID的迁移与既有Adobe例外只适用于该服务；Onleihe的LCP四位码、借期及图书馆登录另算。Tolino借阅帮助保留旧OverDrive Adobe段，不能取代服务现行说明。E-TOL-04、05、E-TLCD-05。','5. 更新表与FAQ文件名冲突尚未解决。德文T302表253099/253009不一致，5.13.241423到5.15.245253区间却给5.13.241423目标；不改名试刷、不推导完整升级链。E-TOL-02、03、E-TLCD-06。','6. p39实际查看发现书签入口图标存在；不能把文字提取漏图当成产品没按钮。原句“Next to the bookmark...tap Bookmarks”仍不清，保留列表操作缺口。E-TLCD-01。','','## 公开选馆器：观察与帮助的差异','','[操作审计](../artifacts/tolino-controls-deepening/browser-operation-audit.md)保存10次状态、6张必要图、命令与原件散列。搜索空列表没有可见无结果说明；选Hamburg后实际落在外站欢迎页仍需选馆。后退时输入Hamburg而列表从A字头开始；回首页刷新后显示最近选择，期间未登录。这个记录证明“最近选择”和“认证成功”可分离；不证明服务器保存、跨会话保留或所有图书馆都同样跳转。初始过渡帧和截图/DOM时间差单列，没把空白加载视作借阅失败。E-TLCD-07。','','## 对本项目的参考与剩余缺口','','以下为助手推断，未升级需求：儿童与成人可以共用内容，但个人进度、学习记录和共享权限宜分别建模；AI应能判断某个封面代表本地文件、云记录还是借阅授权，执行“清理”时应说明对象。读听切换需要稳定原文锚点，不能把多个播放器都能播放等同于支持无缝续听。最近选择、登录和版权码应作为不同可查询状态。','','尚缺批注编辑/导出、书内全文搜索、删除撤销、音频/阅读互跳、云冲突、真实T302图书馆和升级恢复链；采购资格、地区和商业服务条款也未替本项目确认。下一轮可用无DRM自有短书在真实T302核导入→查词→回原句→TTS→睡眠，再按合法测试账户核分享撤权与未同步数据恢复；本轮未执行这些设备任务。','','## 证据入口','']
for e in [byE[f'E-TOL-{n:02}'] for n in range(1,8)]+E:
 case+=['- '+e['evidence_id']+'：['+e['title']+']('+e['url']+')；'+e.get('locator','')+'。'+e.get('note','')]
(L/'cases/M-058-tolino.md').write_text('\n'.join(case)+'\n')
print(json.dumps(dict(new_evidence=len(E),new_flows=len(F),updated_old_flows=len({x['flow_id'] for x in changed}),old_fields=len(changed),inventory_rows=len(I),browser_captures=len(states),archived_browser_images=len(selected)),ensure_ascii=False))
