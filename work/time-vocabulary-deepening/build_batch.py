from pathlib import Path
import csv,json,hashlib,shutil,zipfile,collections
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/time-vocabulary-deepening';raw=W/'browser';A=L/'artifacts/time-vocabulary-deepening';B=L/'batches';AB=A/'browser'
assert not (B/'time-vocabulary-deepening-evidence.jsonl').exists(),'one-time builder'
AB.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def j(p):return json.loads(p.read_text())
def jw(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def cw(p,rows,fields=None):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields or list(rows[0]));w.writeheader();w.writerows(rows)
oldE={x['evidence_id']:x for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())};oldF={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())}
assert 'E-TKVD-01' not in oldE and 'F-M047-05' not in oldF
selected={3,9,11,12,15,16,18,20,22,24,27,30};reviewed=selected|{14,29}
descriptions=[
('直接打开2018年G3–4公开文章','HTTP200，未见可核定阅读级别选择器；URL的rl参数本身不证明切换'),
('将首个预标词滚到视区','记录弹窗之前的正文位置；不是用户任意选词'),
('点击consume','展开含音频、释义、词性、配图、例句及关闭键的卡片；卡片延伸到视区下方'),
('按Escape','DOM仍有一个可见词卡；未据此推所有键盘关闭均失败'),
('点原生音频播放并等1.8秒','终端读取consume paused=false、时间约1.80秒；本帧未归档media字段，后用portrait保存完整对照'),
('点词卡X关闭','卡片消失；不是播放停止的证据'),
('关闭后等1.2秒','终端读取consume仍paused=false、约3.22秒；后续采用持久media状态复核'),
('重新开consume','操作间隔已使音频自然到结尾；不把结束归因于关闭'),
('关consume，对portrait按Enter','键盘打开portrait词卡；不是通过全页Tab序列测试'),
('再次点portrait','词卡保持打开，未证明同词点击是开关'),
('点portrait音频播放，等1.2秒','可见暂停图标；随后DOM paused=false、time约1.36秒'),
('点X并等1秒','卡片消失，但portrait paused=false、time约2.58秒'),
('间隔后重新开始portrait并等0.7秒','第二次对照开始，paused=false、time约0.83秒'),
('关闭词卡并等0.7秒','paused=false、time约1.88秒，关闭未停止底层媒体'),
('播放中重新打开词卡','仍paused=false、time约2.23秒，没有重头播放'),
('点原生暂停','paused=true、time约2.24秒'),
('点卡片外标题','卡片仍可见；音频仍暂停在约2.24秒'),
('关卡、刷新，重新开portrait','paused=true、time=0；只证该匿名文章一次刷新结果，不推云端或账号规则'),
('关闭词卡，打开全局菜单','出现年级、主题、财商、成人区、资源和登录父入口'),
('展开年级入口','四个年级目录；它们不是单篇阅读难度选择'),
('点K–1目录','进入/k1/，保留未登录环境'),
('重新开菜单并展开Topics','看到长主题目录；DOM记录37项，不代表逐项进入'),
('点Science','进入K–1 Science；列表有日期/摘要/音频和西语标记，首条为2025年文章'),
('点Next','进入page/2，首条为2023年文章；有限分页，未测所有末页规则'),
('浏览器返回','回Science第一页；不推每一文章的阅读位置恢复'),
('打开菜单','核父级入口；不把隐藏调试角色字段当用户菜单'),
('点For Grown-ups','同URL弹出出生月/年表单；未填写或提交'),
('按Escape','两个选择框仍可见；随后只读诊断把Python列表错当JS数组slice，报错，不是产品故障'),
('点左上X','关闭过渡帧仍有左侧装饰；后续已能打开资源菜单，不称本帧完全静止'),
('重新打开菜单并展开Resource Spotlight','可见四个资源入口；未进入或注册这些关联服务'),
('点菜单X','结束本轮，随后关闭浏览器；未作提交、登录、课堂分配或打印')]
states=[]
for i,meta in enumerate(j(raw/'observed-states.json'),1):
 name=meta['name'];m=j(raw/(name+'.json'));action,result=descriptions[i-1]
 row=dict(capture=name,action=action,observed_result=result,url=m['url'],captured_at_utc=m['at'],scroll=m['scroll'],image_reviewed=i in reviewed,raw_image_path=str((raw/(name+'.png')).relative_to(R)),raw_image_sha256=sha(raw/(name+'.png')),raw_metadata_path=str((raw/(name+'.json')).relative_to(R)),raw_metadata_sha256=sha(raw/(name+'.json')),raw_html_path=str((raw/(name+'.html')).relative_to(R)),raw_html_sha256=sha(raw/(name+'.html')))
 if 'media' in m:
  row['media']=m['media'];row['media_order_note']='本篇DOM顺序：正文播放器、consume、portrait、grassroot；列表页为空'
 if i in selected:
  dst=AB/(name+'.png');shutil.copyfile(raw/(name+'.png'),dst);row['archived_image']=str(dst.relative_to(L))
 states.append(row)
assert len(states)==31
jw(AB/'observed-states.json',states)
with zipfile.ZipFile(raw/'trace.zip') as z:trace=[json.loads(s) for s in z.read('trace.trace').decode().splitlines()]
after={x['callId']:x for x in trace if x['type']=='after'};actions=[]
methods={'goto','click','reload','goBack','screenshot','waitForTimeout','press','keyboardPress','mouseClick','scrollIntoViewIfNeeded'}
for x in trace:
 if x['type']=='before' and x.get('method') in methods:
  y=after.get(x['callId'],{});actions.append(dict(method=x['method'],class_name=x['class'],params=x.get('params',{}),start_ms=x['startTime'],end_ms=y.get('endTime'),error=y.get('error')))
jw(AB/'trace-actions.json',dict(origin_trace_sha256=sha(raw/'trace.zip'),context_metadata=trace[0],actions=actions))
jw(AB/'raw-trace-manifest.json',dict(path=str((raw/'trace.zip').relative_to(R)),sha256=sha(raw/'trace.zip'),bytes=(raw/'trace.zip').stat().st_size,continuous_screenshots=False,snapshots=False))
for name in ['browser-errors.json','document-statuses.json']:shutil.copyfile(raw/name,AB/name)
audit=A/'browser-operation-audit.md'
lines=['# TIME for Kids：词汇卡与内容导航操作核查','','观察2026-10-27（UTC日志为10月26日）；F-M047-01、F-M047-03；E-TKVD-01、E-TKVD-02。',
'','Linux/Chromium Headless156.0.8078.4，1280×950，新匿名上下文；Playwright包1.64.1、trace内1.64.0-beta。网页构建未知。D4只表示此环境下实际操作与记录，不代表儿童实测、电子纸触控、全站或账户任务完成。',
'','2018-02-16的G3–4文章用于词汇功能，K–1用于目录；不能把高年级词卡、配套2018测验或英语难度直接套给5–6岁儿童。没有提交出生日期、登录、分配课堂、购买、打印或下载音频文件。媒体只经正常网页原生控件启动；没有实际听音，不评发音/音质/内容准确性。',
'','## 动作与状态','','共31捕获，14张作实际视觉判读，12张必要截图归档；其余由当时DOM/动作日志核对，不能称31张都看过。截图和DOM取得有时间差，native计时文字不作为精确秒表。',
'','| 捕获 | 动作 | 可核对结果及限制 |','| --- | --- | --- |']
for i,s in enumerate(states,1):
 label=f'[{s["capture"]}](browser/{s["capture"]}.png)' if i in selected else s['capture']+'（原件work）';lines.append(f'| {label} | {s["action"]} | {s["observed_result"]} |')
lines+=['','## 可复核的关键对照','',
'portrait的11→12和13→14→15都记录媒体继续推进；16→17保持暂停，18刷新后回0。04/17/28分别记录Escape或卡片外点击后仍可见的DOM状态，不能推广成全站键盘可访问性结论。未测试音频结束自动收卡、双词同时播、系统打断或离线。',
'','六次主文档响应均200。页面日志有6次Load error与8次媒体ERR_ABORTED，时间接近页面初载/刷新；未定位因果，不把它们全解释为词音失败，也不据成功推进证明播放链总可靠。',
'','首次未分配PTY的进程stdin关闭，确认该进程已退出、没有网页操作后，以tty启动本次唯一有效会话。有效会话中一次只读诊断Python .slice报AttributeError，已保留动作日志；没有强制点击、修改业务变量、调用内容或账号接口。浏览器最终正常退出0。',
'',f'[动作日志](browser/trace-actions.json)含{len(actions)}条筛选后的Playwright调用，含31截图和13等待，不是点击数。[状态](browser/observed-states.json)、[错误](browser/browser-errors.json)、[响应](browser/document-statuses.json)与[原trace散列](browser/raw-trace-manifest.json)可交叉检查。完整DOM/原截图在work，正式摘记不复制整篇新闻。','']
audit.write_text('\n'.join(lines))
base=dict(product_id='M-047',checked_on='2026-10-27',app_build=None,firmware_version=None,source_updated_on=None,supersedes=None)
evidence=[]
for num,title,url,loc,parent in [
(1,'G3–4词汇卡开关、原生播放与刷新','https://www.timeforkids.com/g34/food-for-thought/?rl=en-760','捕获01–18；2018文章，不是K–1','E-TKD-02'),
(2,'K–1年级主题分页及成人区返回','https://www.timeforkids.com/k1/topics/science/','捕获19–31；K–1/Science及page/2，未提交成人表单',None)]:
  e=dict(**base,evidence_id=f'E-TKVD-{num:02}',title=title,url=url,source_type='public_web_application',access_mode='automated_public_browser_actions_and_screenshot_review',version_scope='Anonymous desktop official public website; Chromium156; current web build unknown; G3–4 2018 article and K–1 directory scopes separated',locator=loc,note='实际界面/媒体状态记录，未听音、未登录；详情及失败见操作核查。',hands_on=True,visual_observed=True,visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=str(audit.relative_to(L)),operation_audit_sha256=sha(audit),archived_artifact=str(audit.relative_to(L)),sha256=sha(audit),capture_state='audited_operations_with_selected_screenshots',artifact_hashes={str(p.relative_to(L)):sha(p) for p in AB.iterdir() if p.is_file()})
  if parent:e['derived_from']=parent
  evidence.append(e)
notes=A/'source-notes.md'
notes.write_text('''# TIME：纸面测验与内容版本

查阅2026-10-27。公开文章末尾Assessment链接指向2018-02的两页G3–4 Cover Quiz，PDF题头日期为2018-02-23，网页发布时间为2018-02-16；不将两种日期强行统一。原件HTTP200，完整两页文字已读，未把渲染图作为已看证据。

试卷要求按当期封面故事回答，10道选择题及1道开放延伸题，标签区分细节、信息分析、文本特征、词句意义和推断。第4题涉及Greta Moeller；本轮网页可见正文及HTML均未找到这个名字。不能据此断言原纸刊缺资料或试卷错误，但现有网页文本不足以闭合该题。答案页依网页说明在教师订阅指南中，本轮没有取得或登录。

这是一条纸面阅读检查链，不是网站内自动作答/评分；没有学生答案、掌握率、错题重练或积分记录。问题中的历史事实未作为当前健康建议。对于AI自动收集资料/题库，需保留文章版本、刊期、题目依据和答案来源；这是研究推断，尚未定系统方案。

[文章原址](https://www.timeforkids.com/g34/food-for-thought/?rl=en-760)；[公开测验](https://www.timeforkids.com/wp-content/uploads/2018/02/edition-3-4-cover-quiz-022318.pdf)。公开教育页本轮普通GET200复核，课程辅助和分级仍是按文章/权限提供的声明，当前匿名文章没有可核定的分级切换器。
''')
log=j(W/'fetch-log.json');quiz=log[1]
evidence.append(dict(**base,evidence_id='E-TKVD-03',title='Food for Thought G3–4纸面测验',url=quiz['url'],source_type='official_worksheet',access_mode='ordinary_public_pdf_text_read',version_scope='Edition3–4; worksheet dated2018-02-23; current article date2018-02-16; not a live digital assessment',locator='PDF p1–2；题4文本特征、10选择+Bonus；答案另需教师指南',note='两页文字读取；网页未找到题4人名，版本匹配待核。没有答题或自动评分观察。',hands_on=False,visual_observed=False,archived_artifact=str(notes.relative_to(L)),sha256=sha(notes),capture_state='short_note_archived_raw_temporary',raw_work_path=quiz['raw_work_path'],raw_sha256=quiz['raw_sha256'],text_work_path='work/time-vocabulary-deepening/quiz-text.json',text_sha256=sha(W/'quiz-text.json')))
(B/'time-vocabulary-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evidence))
f=dict(flow_id='F-M047-05',product_id='M-047',task='从文章查找配套纸面测验并核对版本',depth='D2',evidence_ids='E-TKD-02;E-TKVD-03',known_branches='文章末Assessment→两页G3–4测验→按当期封面故事圈选与开放回答；答案指向教师订阅指南。网页与试卷日期不同；题4人名未在所取网页找到。',gaps='未取教师答案/当期完整纸刊，不能保证每题有足够依据；无软件作答评分、错误反馈、保存或儿童验证。',visual_status='not_observed',hands_on='false',case_path='cases/M-047-time-for-kids.md',checked_on='2026-10-27')
cw(B/'time-vocabulary-deepening-coverage.csv',[f])
updates=[]
revisions={
'F-M047-01':dict(evidence_ids=oldF['F-M047-01']['evidence_ids']+';E-TKVD-02',known_branches=oldF['F-M047-01']['known_branches']+' 本轮另实际展开年级→K–1→Science→Next→浏览器返回；成人区出生年月表单未提交，X退出后可再开资源菜单。',gaps='仍未核所有年级/主题、阅读级别、打印/课堂分配/账号与精准续读；Science分页不能证明所有检索正确。',checked_on='2026-10-27'),
'F-M047-03':dict(task='使用词汇辅助并核对分级与分配范围',depth='D4',evidence_ids=oldF['F-M047-03']['evidence_ids']+';E-TKD-02;E-TKVD-01',known_branches='D4仅2018 G3–4公开词汇卡：预标词点击/Enter开卡，X关卡；词音关闭后仍推进，重开继续，暂停保持；刷新后回0。年级目录不等于阅读级别。',gaps='未实际听音；词卡不是任意选词词典或生词本，收藏/测验关联未知；本篇未见级别选择器，课堂分配仍仅声明；不泛化K–1或账号。',visual_status='observed_browser_sequence',hands_on='true',checked_on='2026-10-27')}
for fid,aftervalues in revisions.items():
 before={k:oldF[fid][k] for k in aftervalues};updates.append(dict(flow_id=fid,before=before,after=aftervalues))
jw(W/'existing-coverage-updates.json',updates)
caps=[dict(evidence_id='E-TFK-01',capture=dict(checked_on='2026-10-27',access_mode='ordinary_public_html_reread',capture_url=log[0]['url'],**{k:log[0][k] for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']},note_artifact=str(notes.relative_to(L)),note_sha256=sha(notes),note='核教育权益/分级/样刊目录，不是账号操作。')),
dict(evidence_id='E-TKD-02',capture=dict(checked_on='2026-10-27',access_mode='browser_rendered_dom_capture',capture_url=oldE['E-TKD-02']['url'],raw_work_path='work/time-vocabulary-deepening/browser/01-article.html',raw_sha256=sha(raw/'01-article.html'),note_artifact=str(notes.relative_to(L)),note_sha256=sha(notes),note='此原件为浏览器渲染DOM，不是原站HTTP原始HTML；实际词卡操作另记E-TKVD-01。'))]
jw(W/'additional-captures-updates.json',caps)
# Public menu and source-defined domains only; grouped parents do not count as completed features.
inv=[]
def add(kind,title,url,eids,flows,status,gap):
 inv.append(dict(entry_id=f'M047-TKVD-{len(inv)+1:03}',entry_type=kind,title=title,source_url=url,evidence_ids=eids,flow_ids=flows,review_status=status,gap=gap))
menu=j(raw/'menu-inventory.json')
for g in menu:
 add('public_menu_parent',g['label'],'https://www.timeforkids.com/k1/','E-TKVD-02','F-M047-01','当前DOM目录，已观察部分展开','父子层级；未走所有目标')
 for link in g['links']:add('public_menu_link',link['title'],link['href'],'E-TKVD-02','F-M047-01','链接/标签已核；K–1、Science实际进入，其余目录发现','关联资源未分析成主阅读站功能；登录/成人提交未执行')
for title,fs in [('Article date / author / credits','F-M047-01'),('Read Aloud','F-M047-04'),('Language switch','F-M047-01'),('Print',''),('Marked word / definition / image / example','F-M047-03'),('Word audio / pause / close','F-M047-03'),('Assessment link / paper quiz','F-M047-05'),('More from topic','F-M047-01'),('Audio / Spanish availability labels','F-M047-01'),('Numbered page / Next','F-M047-01'),('Birth month / year / Submit / close','F-M047-01')]:
 add('observed_article_or_list_control',title,'https://www.timeforkids.com/g34/food-for-thought/?rl=en-760' if 'Article' in title or 'word' in title.lower() or 'Word' in title or 'Assessment' in title else 'https://www.timeforkids.com/k1/','E-TKVD-01;E-TKVD-02',fs,'DOM控件/选定实际操作；正文为G3–4，列表为K–1','Print/Submit没有触发；不声称全部控制完成')
for title in ['Included Content','Accessibility Features','TIME Archives','View Samples / Teacher Guides','Title 1','Common Core State Standards','Response to Intervention','Grades K-1 and Grade2 bundles','Grades3-6 current events','Homeschool Subscription','Career Exploration']:
 add('educators_declared_domain',title,oldE['E-TFK-01']['url'],'E-TFK-01','','普通HTML复读；仅栏目/功能声明','没有登录教师后台，不把课程权益当当前匿名能力')
for title in ['Read for Detail','Analyze Information','Analyze Text Features','Define Words and Phrases','Make an Inference','Bonus open response']:
 add('worksheet_task_type',title,quiz['url'],'E-TKVD-03','F-M047-05','两页测验文字读取，非软件题型目录','历史G3–4；答案/完整纸刊和儿童适配未核')
cw(B/'time-vocabulary-deepening-inventory.csv',inv)
# Keep history explicit while updating current task scope.
case=L/'cases/M-047-time-for-kids.md';s=case.read_text();shutil.copyfile(case,W/'case-before.md')
s=s.replace('查阅日 2026-10-26。','查阅日2026-10-26；2026-10-27补词汇卡、目录与配套测验。',1)
s=s.replace('**D4 限定公开网页两项任务，其余 D1/D2：**','**D4限定公开网页三项任务，其余为文档研究：**',1)
s=s.replace('已查分级入口、主题编排、辅助阅读声明、单篇结构、教学任务与订阅；未核定全部阅读器菜单、孩子账号、收藏/离线、自动评分及家长报告。','已查年级/主题目录、辅助阅读、单篇结构、教学任务与订阅。2026-10-27实际补词汇卡与有限分页；仍未核定单篇阅读级别切换、孩子账号、收藏/离线、自动评分及家长报告。')
old='| F-M047-03 | 找到支持辅助工具的具体文章/教育账号 | 选阅读级别或辅助阅读；成人可用课堂分享分配内容 | 能力见教育说明，实际选级、词汇弹窗和分配成功状态未获取 | 分配后孩子怎样打开、完成、重做和反馈未知 | E-TFK-01、03；D1 |'
new='| F-M047-03 | 2018年G3–4公开文章预标词；分级/课堂另有权限条件 | 点击或Enter开词卡、原生播放/暂停、X关闭、刷新重开 | 词卡关闭后媒体仍推进；暂停保持，刷新后回0。D4仅该词汇分支，年级目录不等于阅读级别 | 未听音；本篇未见可核定级别选择器，课堂分配及保存练习仍未操作 | E-TFK-01、03；E-TKD-02、E-TKVD-01 |'
assert old in s;s=s.replace(old,new)
s=s.replace('F-M047-03 同时补读一篇 **2018 年、3–4 年级**历史文章的 HTML：词汇节点含释义、词性、例句、发音控件和关闭按钮；未执行弹窗，保持 D1，也不归给 K–1。','F-M047-03早期只读一篇 **2018年、3–4年级**历史文章HTML，当时未执行弹窗而保持D1；2026-10-27实际补词卡分支，见下节。此能力仍不归给K–1。')
s+='''
## 2026-10-27：词卡、目录与测验版本深化

[本轮操作核查](../artifacts/time-vocabulary-deepening/browser-operation-audit.md)记录31个捕获、14张实际视觉判读及12张正式截图。词卡与正文朗读是不同播放器：本轮词卡音频关闭后继续，不能用上一轮Read Aloud关闭重播规则代替。没有实际听音或儿童参与。

| 任务 | 本轮动作与结果 | 尚缺 |
| --- | --- | --- |
| F-M047-01 年级、主题与分页补证 | 菜单展开四年级→选K–1→Topics/Science→Next→浏览器返回；成人入口出现出生年月表单，未提交，X后可再进资源菜单。 | 当前仅Science两页，全部主题/年级与搜索准确性未核；四个关联资源只发现入口。E-TKVD-02 |
| F-M047-03 词汇辅助补证 | 点击consume、Enter打开portrait；X关闭和点击卡外/按Escape分开记录。portrait两组对照显示关卡后paused=false、时间推进，重开接着播；暂停约2.24秒，刷新后回0。 | 仅2018 G3–4匿名桌面；未听音、未用触屏；收藏/词本/练习、双音频冲突、跨账号持久化未知。E-TKVD-01 |
| F-M047-05 文章与纸面测验 | 文章Assessment链接→两页历史G3–4测验：10选择题和1开放题，按当期封面故事回答，答案另在教师订阅指南。 | 网页发布日期与试卷刊期不同；题4人名未在现取网页正文/HTML找到。没有完整纸刊或答案，不能强行认定出题错，更不能自动评分。E-TKD-02、E-TKVD-03 |

[逐项目录](../batches/time-vocabulary-deepening-inventory.csv)分当前菜单、文章/列表控制、教育栏目和纸面任务类型，含父子/重复入口。词汇是预标词，未证明任意选词查询；年级目录不是文本改写难度。教师指南与关联资源不当内置游戏、学习记录或自动推荐机制。

对G-001/G-002的候选启发：用“读后是否能找到依据并解释”作为一种反馈，AI导入题目时应匹配原材料版本/刊期及答案出处；界面可见性、媒体状态、内容理解与保存记录分别确认。[测验版本说明](../artifacts/time-vocabulary-deepening/source-notes.md)保留证据不足处，未将本历史内容作为健康建议或5–6岁中文课程。

新增来源：E-TKVD-01为[原2018文章](https://www.timeforkids.com/g34/food-for-thought/?rl=en-760)实际操作，沿E-TKD-02派生；E-TKVD-02为[K–1 Science](https://www.timeforkids.com/k1/topics/science/)导航操作；E-TKVD-03为[公开纸面测验](https://www.timeforkids.com/wp-content/uploads/2018/02/edition-3-4-cover-quiz-022318.pdf)。官网教育页只追加原E-TFK-01复读，未另造来源。
'''
case.write_text(s)
jw(W/'build-summary.json',dict(new_evidence=3,new_flows=1,new_D2=1,old_flows=2,promoted_D1_to_D4=1,inventory_rows=len(inv),browser_captures=31,reviewed_images=len(reviewed),archived_images=len(selected),browser_evidence=2,selected_trace_calls=len(actions),ui_calls_failed=sum(bool(x['error']) for x in actions)))
print((W/'build-summary.json').read_text())
