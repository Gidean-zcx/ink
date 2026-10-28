from pathlib import Path
import json,csv,hashlib,shutil
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/readwise-controls-deepening';B=L/'batches';A=L/'artifacts/readwise-controls-deepening'
assert not (B/'readwise-controls-deepening-evidence.jsonl').exists(),'one-time builder already applied'
A.mkdir(parents=True,exist_ok=True)
def j(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def jw(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def cw(p,rows,fields=None):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields or list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
oldE={e['evidence_id']:e for e in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())};oldF={f['flow_id']:f for f in csv.DictReader((L/'coverage.csv').open())}
assert not any(e.startswith('E-RWCD-') for e in oldE)
captures=j(W/'captures.json');cap={x['name']:x for x in captures};assert len(cap)==15 and all(x['status']==200 for x in captures)
names=['saving','daily-digest','action-tags','navigation','eink','appearance','parsing','pdfs','email-newsletters','videos','sharing','filtered-views']
ids={n:f'E-RWCD-{i:02}' for i,n in enumerate(names,1)};ids.update(basics='E-RDD-01',global_='E-RDD-02',organizing='E-RDD-06');ids['global']=ids.pop('global_')
for name in names:assert cap[name]['url'] not in {e['url'] for e in oldE.values()},name
images=j(W/'images.json');byimage={}
for x in images:
 assert x['visual_observed'];p=R/x['path'];assert sha(p)==x['sha256'];q=A/p.name;shutil.copyfile(p,q);byimage.setdefault(x['source_name'],[]).append(str(q.relative_to(L)))
source_notes='''# Reader：内容处理、阅读控制与静态证据

查阅2026-10-27。15篇官方正文由正常匿名GET取得，均HTTP200；网页构建、App版本和套餐未固定。原始HTML、article正文、标题/链接/图片清单及SHA保存在work/readwise-controls-deepening。仅解析公开帮助，没有运行Reader、登录、上传、邮件、退订、分享发布或API动作。下面是短摘记，不是全文镜像。

## 正文定位与边界

| 来源简称 | 关键定位与保留的限制 |
| --- | --- |
| basics（旧E-RDD-01） | reading progress/undo/trash：滑读位置与最远阅读进度分开；Shift+R重设可短暂显示0；隐藏返回按钮仍跟踪。Z、iOS摇动与toast撤销不是无限回滚；Trash仍限原删除设备。单篇移动恢复写Search、批量写View，差异保留。 |
| saving | 扩展取得已渲染内容；移动分享后可加标签/备注、移动位置。iOS Safari已登录源站与Android/其他App仅传URL条件分开；没有宣称绕过来源权限。 |
| organizing（旧E-RDD-06） | 标签应用到文档或高亮；Enter确认并关菜单、Ctrl/Cmd+Enter确认留菜单；移动Feed文件夹选源/Next、后续修改Done。 |
| global（旧E-RDD-02） | 技能含名称、描述、指令、作用域与模型；说明中的读写类别不是完整API清单。Remember+Approve/Deny改变后续工具权限；没有取消/部分成功/回滚证据。 |
| daily-digest | 从Feed与Saved for Later挑选；手机Home入口，网页Configure添加同组文档但不是同一滚动界面。关Digest同时关其邮件；iOS徽标最多1，查看后消失，不是读完所有文章的证明。 |
| action-tags | 高亮note中的.h1/.c1/.qa语法影响同步到Readwise后的呈现；Enter保存、Shift+Enter换行。题卡在Readwise，不当Reader原生复习循环；删除/修改后卡片联动未知。 |
| navigation | 归档/移动默认返回列表，auto-advance可改下一文档；Bump仅Date moved排序置顶。侧栏固定四栏目，其他可排序/钉住；手势配置与真正识别冲突未观察。 |
| eink | Account的总开关启用推荐项；单独调整需关总开关。四项为动画、翻页动画、对比、音量键。BOOX冻结建议未标固件；原文将Daylight一并讨论，不据此确认其面板技术或支持Plus。 |
| appearance | 字体/主题/方向、分页默认与长文布局；同设备分页设置保留。短/长工具栏分别选Modern/Classic，Modern需AI开关。正文解释纵向分页，但另有横屏平板Horizontal pagination双栏条件，不概括为所有平台不支持横向。 |
| parsing | 保存快照不自动重解析；刷新要删后重存且会删Reader/Readwise批注。同URL去重与不同参数URL重复分开；付费来源需具备原站访问条件。 |
| pdfs | 区域截图存Notebook；原版/文字视图的高亮互不叠显但Notebook可查。可导出带批注PDF；他处既有高亮不识别为Reader高亮。未测往返，且不采用原文把PDF简化为图片的技术概括。 |
| email-newsletters | Library/Feed地址不同；旧随机地址仍工作，二次个性化后旧个性化地址失效。Reader退订按钮屏蔽发件人而非退出外部名单，恢复要联系支持；本轮没有联系。原样/文本偏好按发件人跨端保留。 |
| videos | 字幕定位、自由浏览后回播放段；增强文本可能不叠显旧高亮。换字幕语言创建带语言名的新文档，来源必须有字幕选项；没有播放、转录或执行第三方书签脚本。 |
| sharing | 原址与含批注公开副本分开；撤销公开链接需确认。Bundle从过滤视图创建，不自动更新；手动刷新把新增内容加入已有接收者账号，删改传播未知；未创建或分享。 |
| filtered-views | 条件视图按标签/源/作者/查询创建；可按Location或Seen分栏、配置Home与计数。计数定义、跨端排序、查询错误和删除视图后数据未核。 |

## 五张实际查看的静态图

这些都是官方帮助插图，不是本项目操作截图或连续观察。没有查看所嵌GIF，不能把其文件名或替代文字当演示结果。

| E | 图片 | 观察与不能推断的部分 |
| --- | --- | --- |
| E-RWCD-01 | [保存分享面板](annotated-share-sheet_mobile.jpeg) | Saved to Reader、Read Now、五个编号动作和点击任意位置关闭可见；这是示例画面，不证明一次实际保存成功。 |
| E-RWCD-05 | [E-ink Mode](eink-mode_boox_annotated.png) | 总开关及四个灰显子项，页面文字要求先关总开关才能个别调整；未点击，不测残影/功耗。 |
| E-RWCD-06 | [Toolbar layout](reader-toolbar-layout_settings.png) | 短/长文分别有Modern/Classic；示例短文选Modern、长文选Classic，不当默认值。底层Settings可见部分条目，不推完整设置清单。 |
| E-RWCD-13，源E-RDD-02 | [技能表单](global-ghostreader_add-skill-form.png) | 名称/描述/指令、斜杠触发、作用域/模型、Cancel及灰显Save；未填表，未验证校验/保存。 |
| E-RWCD-13，源E-RDD-02 | [工具确认卡](global-ghostreader_tool-approval-card.png) | Tagging、Show details、Remember、Deny、Approve可见；详情没有展开，也没有动作结果。 |

目录从两组帮助侧栏及这15篇h2/h3提取，另列五图的28个明确控件/选项。入口、父子标题、示例和功能有交叉，不能当全部功能分母。尚未读的指南正文保持未知。
'''
(A/'source-notes.md').write_text(source_notes);note='artifacts/readwise-controls-deepening/source-notes.md';notehash=sha(A/'source-notes.md')
newE=[]
for name in names:
 c=cap[name];e=dict(evidence_id=ids[name],product_id='M-015',title='Reader官方帮助：'+name,url=c['url'],checked_on='2026-10-27',source_type='official_help',access_mode='ordinary_public_html_read',version_scope='Current public help; web/iOS/Android/desktop conditions separated; app build unknown',app_build=None,firmware_version=None,source_updated_on=None,locator='article正文及h2/h3；短摘记按同名定位',note='文档重建；不是账户操作。适用条件、异常缺口和已看图范围见短摘记。',hands_on=False,visual_observed=name in byimage,archived_artifact=note,sha256=notehash,capture_state='short_note_and_selected_static_images',**{k:c[k] for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']})
 if name in byimage:e.update(visual_kind='official_static_screenshot',visual_audit_path=note,visual_audit_sha256=notehash,artifact_hashes={p:sha(L/p) for p in byimage[name]})
 newE.append(e)
newE.append(dict(evidence_id='E-RWCD-13',product_id='M-015',title='Global Ghostreader技能表单与确认卡静态观察',url=cap['global']['url'],checked_on='2026-10-27',source_type='official_documentation_images',access_mode='static_image_observed',version_scope='Two official static screenshots in current global guide; web build unknown',app_build=None,firmware_version=None,source_updated_on=None,locator='Add skill form and inline tool approval card',note='只见表单/按钮；未提交、展开详情或执行工具。',hands_on=False,visual_observed=True,visual_kind='official_static_screenshot',derived_from='E-RDD-02',archived_artifact=note,sha256=notehash,visual_audit_path=note,visual_audit_sha256=notehash,artifact_hashes={p:sha(L/p) for p in byimage['global']}))
(B/'readwise-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in newE))
updates=[]
for name in ['basics','global','organizing']:
 c=cap[name];updates.append(dict(evidence_id=ids[name],capture=dict(capture_kind='ordinary_public_html_reread',checked_on='2026-10-27',capture_url=c['url'],hands_on=False,visual_observed=False,archived_artifact=note,sha256=notehash,**{k:c[k] for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']})))
jw(W/'additional-captures-updates.json',updates)
specs=[
(13,'打开每日摘要并控制提醒',['daily-digest'],'手机Home顶部；网页Home/Configure勾选Today\'s Daily Digest，显示同组文档但无相同滚动形式。Account关闭Enable Daily Digest亦关闭邮件；iOS徽标查看后清除、最多1。','来源为Feed与Saved for Later；选择规则、去重、当日结束/补读、离线与掌握效果未给，不把消徽标当读完。'),
(14,'控制归档后的推进与列表定位',['navigation'],'默认移动文档后返回原列表；网页Command Palette Toggle auto-advance或手机设置可改为自动下一篇。Bump to top/快捷键B仅在Date moved排序生效；侧栏自定义与固定四栏目区分。','末篇、批量动作、失效过滤项、撤销后当前页及跨端设置未知；Customize swipes只给入口，没有完整手势表。'),
(15,'配置分页、长文布局与AI工具栏',['appearance'],'Appearance调整字形/主题；分页默认按文档类型设置且同设备保留，EPUB默认分页/长文。Toolbar layout分别选短/长Modern或Classic，Modern需先启用Ghostreader；横屏平板双栏有独立条件。','正文纵向分页说明与平板横向选项分开；字距重排锚点、跨端设置、误触和关闭AI后的布局回退未知。'),
(16,'设置墨水屏模式与恢复入口',['eink'],'Account Settings/E-ink Mode总开关启用推荐项；关总开关后可分别调动画、页动画、对比及音量键翻页。BOOX冻结提示指向系统Freeze Settings关闭Reader冻结。','只为兼容Android设备的帮助；不是Plus驱动或60fps证据。BOOX型号/固件、内存回收、耗电与异常重启恢复未核。'),
(17,'从浏览器或移动分享保存文章',['saving','parsing'],'扩展图标/Alt+R保存已渲染内容，条中可加标签/备注及移动位置；手机系统分享后可Read Now、删除、标签/备注、Later/Archive，Shortlist配置按钮不同。','iOS Safari已登录源站与其他App/Android仅URL分开；权限拒绝、网络失败、解析完整性及重试未知，静态Saved提示不是实际成功。'),
(18,'识别重复保存并更新文章快照',['parsing'],'同URL再保存去重并置顶/绿点；不同参数URL可能建第二份。保存内容不自动刷新；文档要求删旧后再存更新，会连同Reader/Readwise批注删除。','不保证内容语义去重；Trash恢复旧版与新版本并存、批注迁移、引用修复及删除失败未知。不能用重存同URL保证更新。'),
(19,'将阅读高亮制成问答卡',['action-tags'],'在高亮note以.qa [QUESTION?][ANSWER]写问答，Enter保存；Shift+Enter可换行追加普通注释。作用对象是同步到Readwise的mastery card。','不是Reader内复习实操；错误语法反馈、生成时机、重复/改题/删卡联动、评分调度与离线同步未知。'),
(20,'标注PDF并在原版与文字视图间查回',['pdfs'],'网页区域截图图标→拖选→Notebook存图；Text view与原PDF高亮互不叠显，Notebook仍可查。可Shift+D导出带批注PDF；既有外部PDF高亮不识别为Reader高亮。','截图文字识别/AI读取、图文锚点、跨页选择、笔迹/格式往返、导入失败和批注回写未知；没有实际文件操作。'),
(21,'从视频字幕定位并切换文本版本',['videos'],'保存YouTube链接后字幕与播放相连；网页关autoscroll可自由浏览，Ctrl/Cmd+Enter回播放段。切增强文本可能不叠显旧高亮；换字幕语言建立新文档。','可用语言受原字幕限制；高亮留Notebook不代表新旧锚点修好。字幕缺失/改版、准确性、离线和音频焦点未核，未播放视频。'),
(22,'接收邮件内容并控制发件人',['email-newsletters'],'Library/Feed地址分开；可个性化，原随机地址仍有效、旧个性化地址会失效。Reader Unsubscribe屏蔽发件人含转发，恢复需支持；原样/文本偏好按发件人跨端保存。','外部名单退订需邮件原链接，不能把本地屏蔽当退订成功；现有订阅总表不可得，丢信/撤销/自助解封及数据删除规则未核，未发信。'),
(23,'发布带批注链接并撤回访问',['sharing'],'网页More/Enable public link→蓝链接复制；手机Share document/Share with annotations。蓝链接Disable后确认撤回；shared:true可查已分享文档。','某些用户上传PDF/EPUB不适用。后续批注更新、接收者已保存副本、撤回缓存与旧链接重启未知；没有实际发布。'),
(24,'分享主题内容包并显式更新',['sharing'],'网页过滤视图菜单Enable public link创建Bundle；接收者Open in Reader建立视图及内容。新增文章不自动传播，Refresh public link显式增加到公开包及已有接收者账号。','删除/改题/移出视图、重复导入、接收者批注冲突、断网部分成功与撤回后本地副本未知；不把Bundle当连续订阅或备份。')]
newF=[]
for n,title,ns,branches,gaps in specs:
 fid=f'F-M015-{n:02}';assert fid not in oldF
 newF.append(dict(flow_id=fid,product_id='M-015',task=title,depth='D2',evidence_ids=';'.join(ids[x] for x in ns),known_branches=branches,gaps=gaps,visual_status='static_reference_only' if any(x in byimage for x in ns) else 'not_observed',hands_on='false',case_path='cases/M-015-readwise.md',checked_on='2026-10-27'))
cw(B/'readwise-controls-deepening-coverage.csv',newF)
changes=[]
def amend(n,**fields):
 fid=f'F-M015-{n:02}';before=oldF[fid];after={**before,**fields,'checked_on':'2026-10-27'};changes.append(dict(flow_id=fid,before=before,after=after));return after
amend(3,evidence_ids=oldF['F-M015-03']['evidence_ids']+';E-RDD-06;E-RWCD-03',known_branches='网页列表T/文内Shift+T标文档，选高亮T标高亮；Enter确认关闭，Ctrl/Cmd+Enter确认留菜单。Action Tag置于note影响Readwise：.h1标题、.c1/.c2合并；旧锚点失败仍从Notebook查回。',gaps='标签不互相继承；合并/删除、锚点修复、离线冲突、Action Tag更新回滚未核；不是全部高亮自动制题。')
amend(5,known_branches='最远进度与最后位置分开；网页箭头/手机Return返回，手机X重设当前位置。网页Shift+R重设可能暂显灰色0%，持续阅读后更新；关闭Return indicator只隐藏按钮仍跟踪。',gaps='实际阅读判定阈值、刷新/跨端并发、音频与文本锚点、重设失败及误关按钮恢复未核。')
amend(6,known_branches='More/Delete或网页D删除；原设备Trash恢复批注。网页Z可逐步撤销近几项、iOS系统摇动或部分toast可撤销；批量恢复Restore all，移动Search/View措辞差异保留。',gaps='撤销深度/时限/重启、Android通用Undo、永久清空、换设备恢复、更新文章后旧批注与版本冲突未知；即时撤销不等同备份。')
amend(8,evidence_ids=oldF['F-M015-08']['evidence_ids']+';E-RWCD-13',known_branches='Global网页/桌面启用后问答带段落引用；读写工具分组、每工具允许/确认/禁用。确认卡Remember加Approve/Deny将该工具后续权限改为允许/禁用；静态卡未展开详情。',visual_status='static_reference_only',gaps='文档只举工具类别不是完整工具清单；拒绝后对话、部分成功/取消/重试、并发状态、失败回滚与移动全库能力未核。')
amend(9,evidence_ids=oldF['F-M015-09']['evidence_ids']+';E-RWCD-13',known_branches='技能编辑名称/描述/指令/作用域/模型，斜杠触发；文内会话可展开同聊天，菜单新建/改名/复制/删除/检索历史。静态空表Save灰显；移动清除聊天仍可能在网页历史。',visual_status='static_reference_only',gaps='未填表；保存校验、技能加载与模型上下文成本、触发歧义、删除与跨端历史一致性未知；空表灰显不证明具体必填规则。')
amend(11,evidence_ids=oldF['F-M015-11']['evidence_ids']+';E-RWCD-08;E-RWCD-11',known_branches='Notebook笔记、账户CSV/OPML/内容ZIP分开；完整包邮件异步。网页Shift+D导出带批注PDF；Print with annotations调用浏览器打印，未实际输出。',gaps='完整账户回导、ZIP进度/失败、文字视图高亮在PDF出口中的位置、外部PDF批注往返和第三方删改同步未核。')
amend(12,evidence_ids=oldF['F-M015-12']['evidence_ids']+';E-RWCD-12',known_branches='命名保存查询；可从标签/作者/源建立视图；按Location或Seen分栏、启计数及在Home配置排序。移动Feed文件夹选源Next创建，之后Done确认增减；视图是查询而非内容副本。',gaps='计数范围、空/错误查询、重名/删视图、移出结果后的当前阅读状态、跨端排序和批量恢复未知。')
jw(W/'existing-coverage-updates.json',changes)
flows_by_name={'basics':[5,6],'saving':[17],'organizing':[3,12],'global':[8,9],'daily-digest':[13],'action-tags':[3,19],'navigation':[14],'eink':[16],'appearance':[15],'parsing':[17,18],'pdfs':[11,20],'email-newsletters':[22],'videos':[21],'sharing':[11,23,24],'filtered-views':[12]}
urlname={c['url']:c['name'] for c in captures};knownurl={e['url']:e['evidence_id'] for e in oldE.values()};inventory=[]
def add(kind,title,url,eids,flows,status,gap):
 inventory.append(dict(entry_id=f'M015-RWCD-{len(inventory)+1:03}',entry_type=kind,title=title,source_url=url,evidence_ids=eids,flow_ids=';'.join(f'F-M015-{x:02}' for x in flows),review_status=status,gap=gap))
seen=set()
for page in ['basics','global']:
 for x in j(W/'raw'/f'{page}-structure.json')['links']:
  h=x['href']
  if x['in_article'] or not h.startswith('/reader/') or h in ['/reader/guides','/reader/guides/ghostreader'] or h in seen:continue
  seen.add(h);u='https://docs.readwise.io'+h;name=urlname.get(u);eid=ids[name] if name else knownurl.get(u,ids[page]);status='本轮正文已读' if name else ('旧证据正文，本轮仅目录' if u in knownurl else '仅目录，本轮未读正文')
  title='What is Reader?' if h=='/reader/docs' else x['text']
  add('official_sidebar_page',title,u,eid,flows_by_name.get(name,[]),status,'目录存在不代表完整任务；需固定平台/版本与异常')
for c in captures:
 for h in j(W/'raw'/f'{c["name"]}-structure.json')['headings']:
  if h['level']=='h1':continue
  add('article_'+h['level'],h['title'],c['url']+('#'+h['id'] if h['id'] else ''),ids[c['name']],flows_by_name[c['name']],'本轮已读正文标题及内容','父子标题有重叠；相关F不是本章节所有分支已完成')
controls={'eink':['E-ink Mode','Reduce motion','Reduce page animations','High contrast','Use volume buttons to turn pages'],'appearance':['Short form / Modern','Short form / Classic','Long form / Modern','Long form / Classic'],'global':['Name','Trigger in chat','Description','Instructions','Scope','Model','Cancel','Save','Show details','Remember','Deny','Approve'],'saving':['Read Now','Delete','Add tags','Add document note','Move to Later','Move to Archive','Tap anywhere to dismiss']}
for name,items in controls.items():
 for title in items:add('observed_static_control',title,cap[name]['url'],'E-RWCD-13' if name=='global' else ids[name],flows_by_name[name],'官方单帧控件已查看','没有点击/展开/保存；示例选中状态不是默认值')
cw(B/'readwise-controls-deepening-inventory.csv',inventory)
case=L/'cases/M-015-readwise.md';before=case.read_text();(W/'case-before.md').write_text(before)
s=before.replace('查阅：2026-10-26。','查阅：2026-10-26；2026-10-27补公开目录、内容生命周期及五张静态图。',1).replace('**D2 文档重建；无亲自操作或连续视觉证据**','**D2文档及选定静态图；无亲自操作或连续视觉证据**',1)
s=s.replace('| 收集与订阅 | F-M015-01、F-M015-12 | 邮件订阅、批量迁移、浏览器保存失败、解析修复完整链 |','| 收集与订阅 | F-M015-01/12/17/18/22 | 补分享/邮件/更新快照；批量迁移、保存失败、重试与完整恢复仍缺 |')
s=s.replace('| 阅读与定位 | F-M015-02、F-M015-05 | EPUB/PDF/视频各自导航，版式、手势、快捷动作冲突 |','| 阅读与定位 | F-M015-02/05/14/15/16/20/21 | 补分页/布局/媒体文本条件；实际手势冲突、重排定位和恢复仍缺 |')
s=s.replace('| 批注与组织 | F-M015-03、F-M015-12 | 批量修改、标签合并、锚点修复、Action Tags |','| 批注与组织 | F-M015-03/12/19 | 补标签确认、Action Tags及问答卡入口；同步后的学习/修改/删除链仍缺 |')
s=s.replace('| 阅读节奏与设备适配 | 文档导航有 Daily Digest、E-ink、Appearance | 尚无足够流程；不以存在页面推断推荐效果、刷新体验 |','| 阅读节奏与设备适配 | F-M015-13～16，文档及选定静态图 | Digest选择规则、儿童适配、刷新与触控体验未测 |')
# Keep task numbers and synchronize the compact original table rather than hiding amendments in an appendix.
byid={x['flow_id']:x['after'] for x in changes};lines=s.splitlines()
for i,line in enumerate(lines):
 for fid,row in byid.items():
  if line.startswith('| '+fid+' '):lines[i]='| '+fid+' '+row['task']+' | '+row['known_branches']+' | '+row['gaps']+' | '+row['evidence_ids']+' |'
s='\n'.join(lines)+'\n'
s+='''
## 2026-10-27：内容生命周期、阅读节奏与可见控制

本批15篇官方正文、五张静态图；没有登录或实际App操作。目录分帮助侧栏、正文h2/h3和单帧控件，不是完整菜单或完成率。参见[逐条目录](../batches/readwise-controls-deepening-inventory.csv)及[原件定位与静态审计](../artifacts/readwise-controls-deepening/source-notes.md)。

| F编号/目标 | 文档所述操作与状态 | 未核范围 | E依据 |
| --- | --- | --- | --- |
'''
for f in newF:s+='| '+f['flow_id']+' '+f['task']+' | '+f['known_branches']+' | '+f['gaps']+' | '+f['evidence_ids']+' |\n'
s+='''
新增来源按短摘记同名章节定位；原始URL、日期、HTML/正文SHA、静态图SHA与派生关系在E记录。旧Basics、Global、Organizing沿原E追加本轮原件，不另算第二来源。全库AI工具只取得文档列举类别，不是完整工具协议；静态技能表单说明描述有助选择，但没有验证按需加载过程或成本。

对G-001/G-002的具体启发仍是研究推断：Daily Digest徽标表示当日查看而非学会；更新文章可能影响批注，需确认原文版本；邮件退订与屏蔽发件人含义不同；公开内容包的更新会影响接收者；可见高亮与当前文本锚点是不同状态。分页、工具栏和墨水屏模式只证明App级配置，不证明E-Ink4 Plus适配或60fps。没有把这些机制自动升级为需求。

下一证据：固定App构建后核保存失败/重复与文章更新、分页跨页高亮、动作后自动推进的末篇/撤销、Digest结束反馈、问答卡生成后练习、分享/包的删改传播及离线冲突。其余未读指南、完整AI工具与原生设置仍保留目录缺口。
'''
case.write_text(s)
report=dict(newE=len(newE),newF=len(newF),oldF=len(changes),old_fields=sum(sum(x['before'][k]!=v for k,v in x['after'].items()) for x in changes),oldE_captures=len(updates),inventory_rows=len(inventory),sidebar_rows=sum(x['entry_type']=='official_sidebar_page' for x in inventory),heading_rows=sum(x['entry_type'].startswith('article_') for x in inventory),static_control_rows=sum(x['entry_type']=='observed_static_control' for x in inventory),static_images=len(images),actual_operations=0)
jw(W/'build-result.json',report);print(json.dumps(report))
