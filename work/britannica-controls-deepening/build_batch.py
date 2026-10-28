from pathlib import Path
import csv,json,hashlib,shutil,copy
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/britannica-controls-deepening';B='britannica-controls-deepening';A=L/'artifacts'/B
assert not (W/'build-result.json').exists(),'one-time build already applied'
A.mkdir(exist_ok=True);(A/'browser').mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def jr(p):return json.loads(p.read_text())
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def rel(p):return str(p.relative_to(L))
def csvwrite(p,rows,fields=None):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields or list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
caps={x['name']:x for x in jr(W/'captures.json')};oldE={x['evidence_id']:x for x in jr(W/'evidence-before.json')};oldF={x['flow_id']:x for x in jr(W/'coverage-before.json')}
catalog=caps['content-catalog']['url'];frame='https://cdn.britannica.com/11/288011-184-BF60C826/default.html?autoplay=1'
selected=['01-preview.png','04-title-frame.png','08-start-coordinate-frame.png','09-introduction-later-frame.png','10-selection-frame.png','11-clue-one-frame.png','12-city-choices-frame.png','13-wrong-doctor-frame.png','15-reopen-clue-frame.png','16-correct-librarian-frame.png','17-correct-after-feedback-frame.png','19-clue-two-ready-frame.png','20-back-from-clue-two-frame.png','21-back-settled-frame.png','23-construction-correct-frame.png','24-clue-three-frame.png','28-clue-five-frame.png','29-completion-frame.png','30-after-completion-next-frame.png','32-start-again-frame.png','34-frame-reloaded-frame.png','36-catalog-error.png']
assert len(selected)==22
for n in selected:shutil.copy2(W/'browser'/n,A/'browser'/n)
for p in (W/'browser').glob('*-frame.json'):shutil.copy2(p,A/'browser'/p.name)
for n in ['trace-actions.json','observed-states.json','browser-errors.json','document-statuses.json']:shutil.copy2(W/'browser'/n,A/'browser'/n)
states=jr(W/'browser/observed-states.json');ops=jr(W/'browser/trace-actions.json');assert len(states)==35 and len(ops)==29
for x in jr(W/'render-manifest.json'):
 assert x['visual_observed'];shutil.copy2(R/x['path'],A/Path(x['path']).name)
note=A/'source-notes.md'
note.write_text('''# M049 本轮文档与静态页范围

2026-10-27。两份既有PDF原件复读，没有重新下载；主指南20页、Early11页的章节/工具与两份2026单页自查表分别登记。正式库保存短摘记，完整文本与原件留work。没有通过机构/个人账号进入百科正文。

- **E-BRCD-01/02**：当前onboarding所链Early与Grades3–12教师自查表，各一页。动作清单是建议尝试的入口，不是用户已完成的流程、掌握记录或平台全菜单。高学段ASK不能套到儿童。
- **E-BRCD-03**：Early指南p7，来自E-BRIT-03。根实际看图：图片弹层含关闭、Media Details/Sharing Options；视频图含下载、AD、CC、设置和全屏。文字支持说明/来源、字幕和描述音轨，未点击或下载媒体；不证明完整离线或文件再导入。
- **E-BRCD-04**：主指南p7、13，来自E-BRIT-02。根实际看图：Early原生英/西语和图标组，与Elementary级别/章节/保存/引用等分开；新闻三入口、蝴蝶滑块与生态地图仅静态示例。选中值不当默认，地图不当已经玩过Fundamentals。
- **E-BRIT-04追加**：重新取得onboarding正文及18个FAQ问题。个人保存、教师角色和机构权限不是同一状态；打印工具生成含引用的排版页；词典双击仅正文。翻译后的工具限制保留。
- 主指南p8–9正文混用Library与School，未用其Lexile数值、ImageQuest或高级搜索填儿童School缺口。新闻更新频率与旧产品页冲突仍保留。
- **E-BRCD-05**：公开内容目录HTML提供筛选入口和Community Helpers预览；文章列表的数量是厂商声明，实际浏览器列表加载失败。Early产品页仅发现路径，不据营销说明新增行为结论。

## 静态原件

| E | 实际查看的页 | 限制 |
| --- | --- | --- |
| E-BRCD-03 | [Early p7](brit-early-p7.png) | 非连续画面，不说明控件成功结果。 |
| E-BRCD-04 | [School p7](brit-school-p7.png)、[p13](brit-school-p13.png) | 型号/浏览器构建未知，各学段不能互套。 |

公开互动的实际步骤、失败、返回和重新加载另见[操作审计](browser-operation-audit.md)。这轮互动属于Early的Community Helpers预览；Fundamentals四模式依旧没有实际操作证据。
''')
audit=A/'browser-operation-audit.md'
audit.write_text('''# Britannica公开预览：实际网页操作审计

2026-10-27（Asia/Shanghai）；日志UTC为2026-10-26。证据 **E-BRCD-06/07**，任务 **F-M049-08/05**。研究浏览器Chromium156.0.8078.4，1280×950，匿名新上下文；嵌入预览画面约488×275。不是手机、电子纸、机构账号或真实儿童试用，未听音或评估语言能力。

## 路径与观察

从[公开目录](https://britannicaeducation.com/resources/britannica-school-early-elementary-content-catalog/)点击可见Community Helpers预览，加载[官方嵌入活动](https://cdn.britannica.com/11/288011-184-BF60C826/default.html?autoplay=1)。同页文章目录显示接口429，没有重试目录、改身份或调用隐藏接口。预览为另一公开入口，正常文档200；重载只导航该iframe，没有刷新目录主页面。

| 捕获 | 实际动作与结果 | 边界 |
| --- | --- | --- |
| 01–04 | 公开预览→点击启动→标题Start | 目录正文声明不代替订阅权限。 |
| 06–09 | 按屏幕坐标点击Start，介绍动画后出现Let’s go | 无障碍Start遮挡导致常规locator超时；改点实际可见画面，无强制点击或内部状态修改。 |
| 10–12 | 第一条职业线索→放大镜→八个人物场景 | 文本和画面支持线索内容；八个人物不等于八道题。 |
| 13–15 | 故意选医生→否定反馈与Try again→回场景→重新查看线索 | 未见扣分/生命消耗，不据未见断言产品没有积分。 |
| 16–19 | 选择图书馆员→两段职业解释→Next→第二题 | 部分讲解画面Next已出现而播放图标仍显示暂停；不声称必须听完才能继续。 |
| 20–22 | 第二题Back→前题动画中间状态→稳定后前题正确讲解→Next回第二题 | 20的过渡图不能单独判定恢复到自由选人；21稳定结果才支持返回结论。 |
| 23–27 | 完成建筑工人、医生、警察三条线索 | 加上第一题共四题；只故意验证一次错答，不推广所有错误分支。 |
| 28–31 | 四项勾选回顾→Next→庆祝场景、开放问题、Start Again/Credits | 文件名28-clue-five是取图前命名，实际是完成回顾，没有第五题；无数字成绩或账号成就证据。 |
| 32 | Start Again回标题 | 不是退出应用或服务器清除记录。 |
| 33–35 | 再入活动，第一题选错后正常重载iframe，200后回标题 | 当前匿名同上下文未看到续答提示；不证明全部平台/订阅版本不保存。 |
| 36 | 查看仍在同页的目录错误 | 页面200但目录区明确显示接口429；不运行搜索/筛选或反复请求。 |

35次状态捕获、29个研究命令组；命令包括读取、等待和截图，不等于29次点击。根实际看过并归档22张截图，其他状态仅用DOM/日志核对。全部操作日志与frame JSON已保存；原始DOM HTML留work，未把DOM序列冒称观看完整录像。

## 失败与未验证项

三条命令报错：捕获函数的本地引号SyntaxError（发生在动作前）；Start的无障碍覆盖层使locator点击超时；从第二题返回后过早寻找Librarian，实际稍后恢复正确讲解使其不再可选。均保留原日志，后两项需结合后续画面，不能算答题判错。

浏览器另记录20条pageerror/requestfailed，含广告/统计请求中止和reCAPTCHA超时；未填写/解决挑战，不能把全部错误归因于活动。主页面、活动初载及iframe重载文档均200。目录429是页面可见错误文案，document-statuses仅记录文档请求，不能假称已记录该接口的网络响应体。

音频、AD、Music、标签朗读、Credits可见但本轮未分别操作；声音、听觉反馈、键盘无障碍、触屏、断网、进程重启、跨设备和真正儿童学习效果未知。Back和正常iframe重载是不同操作。订阅内全文、媒体下载、个人保存、Fundamentals游戏仍未验证。

可追溯附件：[动作日志](browser/trace-actions.json)、[状态清单](browser/observed-states.json)、[错误日志](browser/browser-errors.json)、[文档状态](browser/document-statuses.json)。没有注册、登录、提交数据给他人或改变个人账户；匿名页面内状态随操作改变，浏览器已正常关闭。
''')
formal_hashes={rel(p):sha(p) for p in (A/'browser').iterdir() if p.is_file()}
E=[]
def ev(n,title,url,kind='official_help',**kw):
 e=dict(evidence_id=f'E-BRCD-{n:02d}',product_id='M-049',title=title,url=url,checked_on='2026-10-27',source_type=kind,access_mode='local_document_read',version_scope='US School Early/Elementary; public guides copyright2026; web build unknown',app_build=None,firmware_version=None,source_updated_on=None,locator='见本轮短摘记与任务',note='公开资料有限范围；未知不当不支持',hands_on=False,visual_observed=False,archived_artifact=rel(note),sha256=sha(note),capture_state='short_note_archived_raw_temporary',supersedes=None)
 e.update(kw);E.append(e);return e
for n,key in [(1,'early-checklist'),(2,'school-checklist')]:
 c=caps[key];e=ev(n,key,c['url'],'official_checklist',access_mode='ordinary_public_pdf_read',locator='单页全部教师自查动作；不是完成记录')
 e.update({k:c[k] for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']})
for n,parent,names,pages in [(3,'E-BRIT-03',['brit-early-p7.png'],[7]),(4,'E-BRIT-02',['brit-school-p7.png','brit-school-p13.png'],[7,13])]:
 e=ev(n,'官方指南静态控件补证',oldE[parent]['url'],'official_manual',access_mode='local_pdf_text_and_static_pages',visual_observed=True,visual_kind='static_manual_pages',observed_on='2026-10-27',observed_pdf_pages=pages,derived_from=parent,visual_audit_path=rel(note),visual_audit_sha256=sha(note),artifact_hashes={rel(A/name):sha(A/name) for name in names})
 e.update({k:oldE[parent][k] for k in ['raw_work_path','raw_sha256']})
c=caps['content-catalog'];e=ev(5,'Early公开内容目录与互动预览入口',catalog,'official_public_catalog',access_mode='ordinary_public_html_read')
e.update({k:c[k] for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']})
for n,title,url,locator in [(6,'公开目录的加载失败',catalog,'F-M049-08；01与36，目录429文案；未执行筛选'),(7,'Community Helpers四题与错误、返回、完成及重载',frame,'F-M049-05；04–35，公开匿名预览限定')]:
 ev(n,title,url,'direct_browser_observation',access_mode='automated_public_browser',version_scope='Public Community Helpers preview only; Chromium156; anonymous desktop; not subscription platform or Fundamentals',locator=locator,hands_on=True,visual_observed=True,visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=rel(audit),operation_audit_sha256=sha(audit),archived_artifact=rel(audit),sha256=sha(audit),artifact_hashes=formal_hashes,capture_state='audited_browser_operation_with_selected_screenshots',observed_on='2026-10-27')
E[-2]['derived_from']='E-BRCD-05'
(L/f'batches/{B}-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
F=[]
def flow(n,task,depth,eids,known,gaps,visual='not_observed',hands='false'):
 f=dict(flow_id=f'F-M049-{n:02d}',product_id='M-049',task=task,depth=depth,evidence_ids=eids,known_branches=known,gaps=gaps,visual_status=visual,hands_on=hands,case_path='cases/M-049-britannica.md',checked_on='2026-10-27');F.append(f);return f
flow(7,'Early图片与视频的来源、辅助和分享工具','D2','E-BRIT-03;E-BRCD-03;E-BRCD-01','首页分类/文章画廊进入媒体；图片说明/来源和共享，视频字幕/描述/下载按钮分别有文档与静态依据','真实弹层返回、字幕/音轨切换、格式/下载失败、离线及分享权限结果未知','static_manual_pages_only')
flow(8,'打开公开内容目录查看文章入口','D4','E-BRCD-05;E-BRCD-06','主页面200，目录区显示加载失败和接口429；搜索/类别/类型/清除控件可见，停止目录重试','没有取得列表或执行筛选；不是订阅内搜索的失败结论','observed_browser_sequence','true')
csvwrite(L/f'batches/{B}-coverage.csv',F)
updates=[]
def amend(n,**kw):
 b=oldF[f'F-M049-{n:02d}'];a=copy.deepcopy(b);a.update(kw,checked_on='2026-10-27');updates.append(dict(flow_id=b['flow_id'],before=b,after=a))
amend(1,evidence_ids=oldF['F-M049-01']['evidence_ids']+';E-BRCD-04;E-BRCD-02',known_branches='学段导航器分别配置；章节/媒体/相关内容、字号与引用；个人级别选择不改变其他用户。主指南p8–9的Library措辞不套School',gaps='真实搜索返回、选级后原段锚点、精确播放恢复；新闻频率冲突未解')
amend(2,evidence_ids=oldF['F-M049-02']['evidence_ids']+';E-BRCD-01;E-BRCD-04',known_branches='六类/关键词、分节与画廊；原生英西语及逐词朗读。Early导航图与Elementary级别工具分开',gaps='自动续节、换语言位置、退出和离线恢复；未验证触屏词典或中文原生内容')
amend(3,evidence_ids=oldF['F-M049-03']['evidence_ids']+';E-BRCD-04',known_branches='保存需个人账号；教师课程需教师身份，课堂分享取决机构权限；打印工具带引用；机器翻译工具限制仍在',gaps='保存成功/重名/删除/冲突和跨端未知；原生西语不等于Google翻译副本')
amend(4,known_branches='Read/Explore/Play/Create需订阅启用；绘画有擦除/重置/打印声明，打印不证明云存储或可再编辑',gaps='Fundamentals错答/提示/重试/结算及退出保存仍未知；本轮Community Helpers实际预览不是该四模式')
amend(5,depth='D4',evidence_ids=oldF['F-M049-05']['evidence_ids']+';E-BRCD-05;E-BRCD-07',known_branches='限定Community Helpers公开预览：四道线索、场景选人；一次错答→重试→回看线索→正确解释；第二题Back恢复前题讲解；四项勾选后开放问题，Start Again回标题',gaps='只核一个预览和一次错答；iframe重载回标题，不推广其他场景或账号保存。音频开关/听音/键盘/触屏/断网及学习成效未知',visual_status='observed_browser_sequence',hands_on='true')
dump(W/'existing-coverage-updates.json',updates)
captures=[]
for eid,name,txt in [('E-BRIT-02',None,'work/news-interactions/brit-school.txt'),('E-BRIT-03',None,'work/news-interactions/brit-early.txt'),('E-BRIT-04','onboarding',None)]:
 e=oldE[eid];c=dict(checked_on='2026-10-27',capture_url=e['url'],access_mode='ordinary_public_html_read' if name else 'local_pdf_reread_no_refetch',hands_on=False,note_artifact=rel(note),note_sha256=sha(note))
 if name:c.update({k:caps[name][k] for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']})
 else:c.update(raw_work_path=e['raw_work_path'],raw_sha256=e['raw_sha256'],text_work_path=txt,text_sha256=sha(R/txt))
 captures.append(dict(evidence_id=eid,capture=c))
dump(W/'additional-captures-updates.json',captures)
inv=list(csv.DictReader((W/'document-inventory.csv').open()))
for i,x in enumerate(inv,1):
 x['draft_id']=f'I-BRCD-{i:03d}'
 if x['source_key'] in ['early_checklist','school_checklist']:
  n,key=(1,'early-checklist') if x['source_key']=='early_checklist' else (2,'school-checklist');x.update(evidence_id=f'E-BRCD-{n:02d}',source_url=caps[key]['url'])
extra=[
 ('static','Early媒体详情/共享与视频辅助', 'E-BRCD-03','PDF p7','F-M049-07','静态控件组，未操作'),
 ('static','四学段Navigator并列','E-BRCD-04','PDF p7','F-M049-01;F-M049-02;F-M049-03','并列图不当默认或跨学段可用'),
 ('static','新闻三入口/滑块/生态地图','E-BRCD-04','PDF p13','','仅静态例子，与正文功能重复'),
 ('browser','公开预览启动与Start','E-BRCD-07','01–09','F-M049-05','坐标点击可见画面；无障碍定位失败保留'),
 ('browser','介绍结束的Let’s go','E-BRCD-07','09–10','F-M049-05','字幕读取，未听音'),
 ('browser','线索与放大镜进入场景','E-BRCD-07','11–12','F-M049-05','本预览四题，非八题'),
 ('browser','八个人物选择','E-BRCD-07','12–13','F-M049-05','四正确目标与其他人物，未枚举全部错误反馈'),
 ('browser','错答与Try again','E-BRCD-07','13–14','F-M049-05','一次故意错答'),
 ('browser','See clue再次提示','E-BRCD-07','15','F-M049-05','返回同题线索'),
 ('browser','正确解释与Next','E-BRCD-07','16–17/23/25/27','F-M049-05','不把Next可见等同音频完成'),
 ('browser','Back恢复前题讲解','E-BRCD-07','19–22','F-M049-05','过渡图与稳定图分开'),
 ('browser','四项勾选完成回顾','E-BRCD-07','28–29','F-M049-05','不是分数或账号成就'),
 ('browser','最终开放问题与Start Again','E-BRCD-07','30–32','F-M049-05','重开回标题；没有自动评判开放回答'),
 ('browser','正常iframe重载','E-BRCD-07','33–35','F-M049-05','当前匿名预览回标题；未重载429目录'),
 ('browser_visible_only','AD/音乐/标签音频/Credits','E-BRCD-07','08–31','F-M049-05','可见但未分别操作，效果未知'),
 ('browser','目录加载失败','E-BRCD-06','01/36','F-M049-08','页面报接口429后停止'),
 ('browser_visible_only','目录搜索/类别/类型/清除','E-BRCD-06','36','F-M049-08','没有执行筛选，未取得结果')]
for typ,title,eid,loc,flows,gap in extra:
 x={k:'' for k in inv[0]};x.update(draft_id=f'I-BRCD-{len(inv)+1:03d}',source_key=typ,source_url=frame if eid=='E-BRCD-07' else catalog if eid=='E-BRCD-06' else next(e['url'] for e in E if e['evidence_id']==eid),evidence_id=eid,entry_type=typ,title=title,locator=loc,applicable_level='本预览或指定指南学段',text_supported_scope=title,coverage_status='observed_browser' if typ=='browser' else 'visible_controls_only',related_flow_ids=flows,mapping_status='partial_scope_existing_flow' if flows else 'unmapped',overlap_note='与任务及正文目录可能重叠，不累加完成数',gap_or_limit=gap,review_method='root_browser' if typ.startswith('browser') else 'root_static_pdf',checked_on='2026-10-27');inv.append(x)
csvwrite(L/f'batches/{B}-inventory.csv',inv)
p=L/'cases/M-049-britannica.md';s=p.read_text();s=s.replace('查阅日 2026-10-26；','初查2026-10-26，续查2026-10-27；',1)
s=s.replace('**D2 操作手册，静态图已观察；没有登录或连续操作。**','**多数任务为D2文档重建；F-M049-05的Community Helpers公开预览及F-M049-08的目录失败已有D4操作记录。没有登录机构/个人账号。**',1)
s=s.replace('F-M049-05 | Early 的互动目录或文章内嵌活动 | 简介/说明→点选互动→动画收束；提供暂停、描述音轨与音频开关 | D2 文档路径；具体题目的错误反馈、返回是否保存、关闭后恢复未知 | E-BRED-01，指南第 6 页','F-M049-05 | Early互动；本轮仅Community Helpers公开预览实际操作 | 线索→场景选人→错答重试/回看线索→正确讲解→Next；四题后勾选回顾与开放问题，Start Again回标题 | D4仅该预览；Back稳定恢复前题讲解，iframe重载回标题；音频开关效果、订阅内保存与其他游戏未知 | E-BRED-01、E-BRCD-05/07')
s=s.replace('当前仅是文档和静态图研究。','这些订阅内任务仍缺实操；本轮另有下述公开预览操作。')
s+='''

## 2026-10-27：公开互动的一轮与恢复边界

官方公开目录中的Community Helpers与Fundamentals四模式分开。根用匿名桌面浏览器完成四条职业线索：首次故意选错→Try again回场景→See clue回看→选对后解释；其后完成另外三题。四项勾选回顾后，Next到庆祝场景及联系自己社区的开放问题，Start Again回标题。没有观察到对开放回答自动评分或账号奖励。

第二题Back后，先经过动画中间状态，稳定后恢复上一题正确讲解；因此不能只拿过渡截图推断进度丢失。另一次在首题错答处正常重载活动iframe，回到标题，未出现续答提示。该结果仅针对当前匿名公开预览，不证明机构平台或全部游戏的保存规则。具体动作、22张实看截图和失败见[操作审计](../artifacts/britannica-controls-deepening/browser-operation-audit.md)。F-M049-05仅此子范围升D4，旧通用音频开关说明仍未实际验证。

| F | 新任务 | 已有依据与终点 | 未知 |
| --- | --- | --- | --- |
| F-M049-07 | Early图片/视频的来源、辅助与分享工具 | D2：分类/画廊进入媒体，图注/来源与共享；视频字幕、AD、下载有文字和静态控件，E-BRIT-03、E-BRCD-01/03 | 返回原文位置、下载格式/失败、离线与分享执行结果；未下载媒体。 |
| F-M049-08 | 打开公开内容目录查看文章入口 | D4有限失败：主页面200，目录区显示接口429；搜索/类别/类型/清除控件可见，停止重试，E-BRCD-05/06 | 没有执行筛选或取得文章列表；不外推订阅内搜索。 |

另补[逐项目录](../batches/britannica-controls-deepening-inventory.csv)：主指南章节、Early细项、18个FAQ、两张教师自查表及本轮可见控件。父子/重复/研究缺口均单独标识。两张checklist是教师探索建议，不能充当学生能力测评；主指南p8–9的Library措辞继续保留。图片/字幕/描述音轨不是同一种辅助；打印绘画不证明云端保存或可再编辑。

新文档与静态来源E-BRCD-01–05见[短摘记](../artifacts/britannica-controls-deepening/source-notes.md)，操作E-BRCD-06/07见审计；新取原件与既有PDF复读分别登记。5–6岁中文独立阅读不等于能独立理解英文线索。对G-001的启发是比较纠错、职业解释、回顾与生活联系的作用；对G-002的启发是区分动画中间态、可继续按钮、任务完成和持久保存。这些仍是研究推断，没有确定游戏奖励、账户制度或系统架构。
''';p.write_text(s)
report=dict(newE=len(E),newF=len(F),new_D2=1,new_D4=1,oldF=len(updates),old_fields=sum(sum(b['before'][k]!=v for k,v in b['after'].items()) for b in updates),oldE_captures=len(captures),inventory_rows=len(inv),browser_states=len(states),browser_command_groups=len(ops),browser_screenshots=22,static_images=3)
dump(W/'build-result.json',report);print(json.dumps(report,ensure_ascii=False))
