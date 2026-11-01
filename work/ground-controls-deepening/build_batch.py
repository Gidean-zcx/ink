from pathlib import Path
import csv,json,re,hashlib,shutil
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches';W=R/'work/ground-controls-deepening';A=L/'artifacts/ground-controls-deepening';A.mkdir(exist_ok=True)
assert not (W/'applied-existing-updates.json').exists(),'one-time build already applied'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def jd(p,o):p.write_text(json.dumps(o,ensure_ascii=False,indent=2)+'\n')
def cw(p,rs):
 with p.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
S={r['name']:r for f in ['sources-initial.json','sources-categories.json','sources-selected.json','sources-more.json','sources-feedback-category.json'] for r in json.loads((W/f).read_text())}
assert all(r.get('status')==200 for r in S.values())
old=[json.loads(l) for l in (L/'evidence.jsonl').read_text().splitlines()];byurl={e['url']:e for e in old if e['product_id']=='M-046'}
entries=[('help-index','帮助中心完整可见目录','8个顶层类、3个子类；65篇独立article-card'),('factuality','事实性筛选与原始评级依据','Web/App筛选、来源页；3张已查看官方图'),('article-245249','偏向条与地区配色','US与UK/Canada/International语义'),('article-245313','修改个人政治偏向评级','两端新闻源页铅笔、Manage Sources & Topics'),('article-722625','In Focus事件流','世界事件持续更新；未给归档/结束规则'),('article-6049601','新闻源搜索与目录范围','没有完整静态可下载来源清单；两端搜索/关注'),('article-950337','正向主题与推荐依据','专题、For You输入、Custom Feeds套餐措辞'),('article-263489','新闻流不加载的恢复建议','网络/干扰排查、重新加载、重装及求助'),('article-263425','配色与新闻地区独立设置','Web Preferences Bias Bar；App Profile Settings'),('article-5237505','已知问题与临时处理','2026-06-17页面：编辑评级、登录、加载、Compact Cards'),('article-1750273','建议增加新闻来源','读者邮件建议/机构提交表单；不保证收录'),('article-6220033','新闻配置导览','链接式任务目录；不是独立界面操作'),('article-6692033','为同一账户关联登录方式','Account Information Sign-in methods；每provider一个'),('article-334913','请求删除账户数据','各设备先退出、删除表单和声明；订阅状态另管'),('article-245697','找回登录入口','邮件密码与社交登录分流'),('article-335041','密码重置失败','社交登录、垃圾邮件、链接过期求助'),('article-648577','家庭订阅边界','本文声明无家庭分享套餐；不外推数据共享全部能力')]
notes='''# Ground News 本轮来源与观察边界

查阅2026-10-27；Web/iOS/Android分开，未固定构建。全部帮助为官方公开HTTP200正文；正文中的帮助中心“提交失败”不是产品运行错误。没有登录、发送表单/邮件、改评级/偏好、重装或付费。

本轮匿名Chromium访问产品search返回403后正常退出，没有重试或绕过。未取得产品操作截图，不产生D4；先前普通HTTP取得的首页只作发现入口，没有从其新闻内容推断实际筛选行为。[访问记录](browser-access-attempt.json)。

- E-GRCD-01：根帮助页与11分类页。仅取article-card为正式文章入口，排除重复的快捷卡；8顶层+3子类+65篇=76节点。来源提供方合作、账单等也保留，但不是全部软件功能目录。相同category数字ID的不同slug不扩成独立类。目录项正文是否已读取另标。
- E-GRCD-02：2026-04-15教程正文及三图。图3移动端有Very Low/Low/Mixed/High/Very High/Unknown、多选、Clear、关闭X和Show results；图4Web筛选；图5来源页分别列Bias/Factuality/Ownership及评级机构链接。静态图中数量不能当实时结果/操作前后。正文说事实性来自三机构，E-GRND-03现行方法论却列两机构；图5本身也列两事实性机构。保留冲突，不因评级居中而认定新闻更真。
- E-GRCD-03/09：美国与国际配色的红蓝语义相反；地区版会影响颜色，但Web账户可独立设Bias Bar，App也有独立Color Scheme。颜色不是政治坐标本身，不能据切配色判断内容变化。
- E-GRCD-04/10：个人账户可修改政治偏向评级，两端从来源页铅笔进入并到Manage Sources & Topics回查；不支持同样改事实性/所有权。6月已知问题页称部分Android源页修改有异常，事件页为替代入口；无构建/修复发布日期，不推断10月全部设备仍有故障。
- E-GRCD-05/06：In Focus为更新中的事件流，未给结束机制；全来源目录不能静态下载，搜索/关注不证明搜索穷尽。平台声称来源数量不是本项目已核定的数据规模。
- E-GRCD-07/12：For You使用地点、关注主题/媒体和App使用模式；正向主题及每周邮件是选择入口，不是学习收益。Positive帮助称Custom Feeds为Pro+，专页E-GND-07称Premium/Vantage且Web专属；保留套餐措辞冲突，不自行统一。该导览的嵌入视频未观看。
- E-GRCD-08/10：重装/换网/关闭Compact Cards只是官方恢复建议，不证明成功或数据保全；Known Issues表和正文均有状态，但无对应App构建。卡片标题与事件页标题可在已知异常中不一致，需保存两者来源，不能自动当同一字段。
- E-GRCD-11：来源不存在时读者可向客服推荐名称及URL，机构走表单，均不保证被纳入或给定时限。本轮未打开/提交表单或邮件。
- E-GRCD-13/15/16：同一账户可在Web绑定Google/Apple/Facebook等登录，一provider一账户；已有保存/订阅不变，移除登录方式不等于删账户。旧找回步骤依原注册方式分流；Gmail域名不能证明当初必用Google登录，帮助中的google authenticator措辞不当作验证码App指令。
- E-GRCD-14：帮助要求各设备退出后填数据删除表并作声明；明确删除数据不取消订阅。文字有缺词，不能据此补出删哪些表、完成时点或撤回/备份恢复能力。
- E-GRCD-17：官方帮助声明无家庭分享套餐；不据此断言一个家庭绝无其他共同阅读方式。

复查仍沿用原E：E-GRND-01来源筛选；03方法论；04比较摘要；06隐藏已读；E-GND-07自定义流；17订阅未识别。比较摘要不足覆盖时可能退普通摘要；所谓人工参与不等于逐条核验。隐藏已读未给阈值/撤销。Restore In-App Purchase会重新激活已取消订阅的原文警告继续保留，本轮未执行。
'''
(A/'source-notes.md').write_text(notes);shutil.copyfile(W/'browser-access-attempt.json',A/'browser-access-attempt.json')
for n in [3,4,5]:shutil.copyfile(W/f'factuality-image-{n}.png',A/f'factuality-image-{n}.png')
E=[]
for i,(name,title,locator) in enumerate(entries,1):
 s=S[name];t=(W/(name+'.txt')).read_text();updated=re.search(r'Edited (?:Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday), ([A-Za-z]+ \d+ \d{4})',t)
 import datetime
 date=datetime.datetime.strptime(updated[1],'%B %d %Y').date().isoformat() if updated else None
 e=dict(evidence_id=f'E-GRCD-{i:02}',product_id='M-046',title=title,url=s['url'],checked_on='2026-10-27',source_type='official_help_index' if i==1 else 'official_help_article',access_mode='public_HTTP200_HTML_text_extraction',version_scope='Ground News Web/iOS/Android where stated; installed build unknown; article date not app version',app_build=None,firmware_version=None,source_updated_on=date,locator=locator,note='文档/静态图重建，非产品操作；条件、冲突与缺口见案例。',hands_on=False,visual_observed=i==2,archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md'),capture_state='public_body_read_no_product_operation',raw_work_path=s['raw_work_path'],raw_sha256=s['raw_sha256'],parsed_text_path=s['parsed_text_path'],supersedes=None)
 if i==1:e['additional_raw_sources']=[dict(raw_work_path=x['raw_work_path'],raw_sha256=x['raw_sha256'],url=x['url']) for x in S.values() if x['name'].startswith('category-')]
 if i==2:
  e.update(visual_kind='static_official_images',archived_artifact=str((A/'factuality-image-3.png').relative_to(L)),sha256=sha(A/'factuality-image-3.png'),artifact_hashes={str((A/f'factuality-image-{n}.png').relative_to(L)):sha(A/f'factuality-image-{n}.png') for n in [3,4,5]},image_sources=json.loads((W/'image-manifest.json').read_text()),capture_state='three_official_images_reviewed_not_continuous')
 assert s['url'] not in byurl,(s['url'],byurl[s['url']]['evidence_id']);E.append(e)
(B/'ground-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
# Add reinspection metadata to original records, keeping stable IDs.
ec=[]
for p in B.glob('*-evidence.jsonl'):
 if p.name=='ground-controls-deepening-evidence.jsonl':continue
 es=[json.loads(l) for l in p.read_text().splitlines()];dirty=False
 for e in es:
  if e['product_id']!='M-046':continue
  match=[s for s in S.values() if s['url']==e['url']]
  if not match:continue
  s=match[0];before=json.loads(json.dumps(e));e.setdefault('additional_captures',[]).append(dict(checked_on='2026-10-27',access_mode='public_HTTP200_recheck',raw_work_path=s['raw_work_path'],raw_sha256=s['raw_sha256'],parsed_text_path=s['parsed_text_path'],archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md')));ec.append(dict(file=str(p.relative_to(R)),evidence_id=e['evidence_id'],before=before,after=e));dirty=True
 if dirty:p.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in es))
jd(W/'applied-existing-evidence-updates.json',ec)
F=[]
def flow(n,task,eids,known,gaps,vis='not_observed'):
 F.append(dict(flow_id=f'F-M046-{n:02}',product_id='M-046',task=task,depth='D2',evidence_ids=eids,known_branches=known,gaps=gaps,visual_status=vis,hands_on='false',case_path='cases/M-046-ground-news.md',checked_on='2026-10-27'))
flow(17,'修改个人政治偏向评级并回查','E-GRCD-04;E-GRCD-10','Web/App搜索媒体→来源页铅笔进入修改；Manage Sources & Topics查看已改项；只限政治偏向。已知问题页为部分Android列源页已有评级/事件页可改的替代入口','编辑选项、确认/取消/恢复默认、跨端同步、对统计/筛选作用及当前构建是否仍有故障未核')
flow(18,'独立切换偏向条配色','E-GRCD-03;E-GRCD-09','Web账户Preferences→Bias Bar选US/International可不改地区版；App Profile→Settings→Bias Bar Color Scheme；美国与国际红蓝含义相反','未操作颜色生效/跨端同步；不能据颜色推断内容或用户立场发生变化')
flow(19,'为同一账户关联与移除登录方式','E-GRCD-13;E-GRCD-15;E-GRCD-16','Web Account Information→Sign-in methods查看/增加/移除；每provider一账户；文档称原邮箱、保存文章和订阅不变，移除登录方式不删账户；找回按原注册方式分流','授权弹窗、重复/冲突账户、最后登录方式移除、失败回滚及实际恢复未核；不将Gmail邮箱等同已用Google注册')
flow(20,'请求删除账户数据并区分订阅','E-GRCD-14','帮助要求全部设备先登出→填写数据删除表→声明删除数据不影响订阅；Web Account和App Settings底部退出','未打开/提交表单；字段、删除完成反馈/时限、影响对象、撤回或备份恢复未核；不是取消订阅')
flow(21,'处理新闻流加载失败及卡片标题错配','E-GRCD-08;E-GRCD-10','官方建议排查网络/VPN/广告拦截后重载，移动端重装重登；Compact Cards错配可Profile→Settings关该项；Known Issues标调查或修复中','仅官方建议；未执行/测恢复成功；未给构建、缓存/偏好保留、重试耗时或恢复后的滚动位置')
flow(22,'追溯媒体评级及未评级状态','E-GRCD-02;E-GRND-03','搜索媒体→来源专页→分别看Bias/Factuality/Ownership→评级机构链接查看依据；图含Unknown筛选与各机构分项；方法论说无评级不进入Bias Bar、无事实性评分则无score','未打开机构详情/实际点击；平均计算与更新版本、个人改值如何影响总览、未评级在筛选计数中的规则未知；评价对象是媒体不是单篇','static_official_images_only')
cw(B/'ground-controls-deepening-coverage.csv',F)
# Enrich existing tasks in their original batches.
upd={
1:dict(known_branches='Full Coverage→Sort & Filter；排序Bias/Factuality/Latest/Alphabetical，偏向/事实性/所有权/Locality/Paywall组合；Show Results应用，Clear或换事件复位。移动静态图含Unknown，比较摘要覆盖不足时只给普通摘要。',gaps='零结果、取消回滚、外链返回位置/付费墙与全局默认优先级未实测；教程三机构与方法论两机构口径不一',evidence_ids='E-GRND-01;E-GRND-03;E-GRND-04;E-GRND-07;E-GRCD-02',visual_status='static_official_images_only'),
3:dict(known_branches='Discover关注后进入个性化流；For You帮助明确使用地点、关注主题/来源和App使用模式，不是只按显式订阅。移动Premium+可Profile Settings打开Hide Read Stories。',gaps='已读阈值、撤销、隐式行为权重与关闭个性化入口未核；观看/打开不等于理解',evidence_ids='E-GRND-05;E-GRND-06;E-GRCD-07'),
4:dict(known_branches='Web旗帜、Android事件三点、iOS邮件入口报告问题；建议缺失来源时读者邮件提供名称/URL，机构另走表单且不保证收录或时限。',gaps='未提交；成功反馈、修复时延、聚合错配处理及新增来源审核结果未知',evidence_ids='E-GRND-08;E-GRCD-11'),
5:dict(known_branches='地区主题按钮；本地手动搜索；覆盖不足补更大地区。地区可能同时改变偏向条配色，但可独立设颜色，不能混为内容过滤。',evidence_ids='E-GND-02;E-GND-03;E-GRCD-09'),
6:dict(known_branches='标题/关键词/URL找覆盖，来源名找媒体页；In Focus持续更新事件。官方无静态可下载全来源清单，搜索未命中不代表来源永不支持。',gaps='检索穷尽/匹配失败提示、事件更新版本/终点、外链返回位置未核',evidence_ids='E-GND-01;E-GRCD-05;E-GRCD-06'),
8:dict(known_branches='取消不保存，Manage Sources & Topics回改，App屏蔽反映到Web；Positive帮助把Show Me Less写作减少而非完全禁止，实际算法效果未测。',evidence_ids='E-GND-04;E-GND-06;E-GRCD-07'),
9:dict(known_branches='Web For You Custom Feeds→命名→任一包含/全部包含/任一排除→Confirm刷新。专页称Premium/Vantage，Positive页称Pro+，不统一套餐含义。',gaps='编辑/删除、空结果、当前移动是否上线及套餐措辞冲突未核',evidence_ids='E-GND-07;E-GRCD-07'),
16:dict(known_branches='网站购买须回正确账户/原登录方法；绑定其他登录方式后仍属原账户，不是转移订阅。商店Restore In-App Purchase帮助警告会重启已取消订阅；网站购买用此入口可能持续加载。',gaps='恢复是否成功/完整保留未知；不能由邮箱域名推注册方式，不能把Restore当无副作用刷新',evidence_ids='E-GND-17;E-GRCD-13;E-GRCD-15;E-GRCD-16')}
changes=[]
for p in B.glob('*-coverage.csv'):
 if p.name=='ground-controls-deepening-coverage.csv':continue
 rs=list(csv.DictReader(p.open()));dirty=False
 for r in rs:
  if r['product_id']!='M-046':continue
  n=int(r['flow_id'][-2:])
  if n not in upd:continue
  for k,v in dict(upd[n],checked_on='2026-10-27').items():
   if r[k]!=v:changes.append(dict(file=str(p.relative_to(R)),flow_id=r['flow_id'],field=k,before=r[k],after=v));r[k]=v;dirty=True
 if dirty:cw(p,rs)
jd(W/'applied-existing-updates.json',changes)
# Exact article-card inventory; exclude repeated promoted shortcuts and same-ID category aliases.
catnames={'2273089':'Getting Started','2272705':'Understanding Ground News','2272769':'Personalize Your News Feed','2272897':'Advanced Browsing Tools','2272833':'Manage Your Account & Subscription','2272961':'Fix a Problem','2273025':'Share Your Feedback','2358337':'Partner With Us','2273153':'Account Questions','2273217':'Subscription Questions','2275393':'Share Your Feedback Reference Articles'}
parents={'2273153':'2272833','2273217':'2272833','2275393':'2273025'}
allbyurl={e['url']:e['evidence_id'] for e in old if e['product_id']=='M-046'};allbyurl.update({e['url']:e['evidence_id'] for e in E})
mapping={'225473':[2,3],'5609857':[6],'7541441':[15],'1750529':[6],'245249':[18],'3189505':[1],'722625':[6],'6049601':[6],'245313':[17],'4663041':[12],'950017':[3],'950337':[3,8,9],'1499905':[8],'911873':[7],'916353':[8],'1001537':[5],'5428225':[5],'6220033':[3],'898305':[13],'866177':[9],'6049857':[1],'5224577':[1,22],'7212801':[10],'3405953':[11],'3523649':[3],'2044545':[2],'1858561':[],'1096641':[11],'245569':[1],'3523713':[10],'263489':[21],'263425':[18],'5237505':[17,21],'6692033':[19],'245697':[16,19],'335041':[19],'334913':[20],'244865':[16],'648577':[],'1750273':[4],'9683777':[4]}
I=[];seen=set();currenturls={s['url'] for s in S.values()}
for cid,title in catnames.items():
 name='category-'+cid;s=S[name];I.append(dict(entry_id='M046-C'+cid,product_id='M-046',entry_type='help_category',parent_id='M046-C'+parents[cid] if cid in parents else '',title=title,source_url=s['url'],article_url='',evidence_ids='E-GRCD-01',flow_ids='',body_review_status='本轮目录已取得；非功能任务',gap='分类内文章逐项标阅读状态，目录不等于完成'))
 for a in json.loads((W/(name+'-links.json')).read_text()):
  if a.get('class')!='article-card' or '/articles/' not in a['url']:continue
  u=a['url'];assert u not in seen,u;seen.add(u);aid=u.rsplit('/',1)[-1];eid=allbyurl.get(u,'E-GRCD-01');fids=mapping.get(aid,[])
  state='本轮正文已读' if u in currenturls else ('既有正文E，本轮只核目录' if u in allbyurl else '仅目录发现，未读正文')
  I.append(dict(entry_id='M046-A'+aid,product_id='M-046',entry_type='help_article',parent_id='M046-C'+cid,title=a['title'].split('Edited')[0].strip(),source_url=s['url'],article_url=u,evidence_ids=eid,flow_ids=';'.join(f'F-M046-{n:02}' for n in fids),body_review_status=state,gap='关联不代表完整流程；套餐/构建及异常见案例' if fids else '尚未形成详细任务；可能是账单/合作或范围说明'))
assert len(seen)==65 and len(I)==76
cw(B/'ground-controls-deepening-inventory.csv',I)
# Preserve earlier analysis but explicitly date its evidence and add reviewed refinements.
p=L/'cases/M-046-ground-news.md';s=p.read_text();s=s.replace('查阅日 2026-10-26；','基线查阅2026-10-26，本轮复核2026-10-27；',1)
s=s.replace('以下均为打开官方正文；没有查看页面内截图或操作产品，`visual_observed=false`。','以下是2026-10-26基线：打开官方正文，当时未看截图或操作产品。2026-10-27新增三张官方静态图单列于后文，仍无产品实际操作。',1)
s+='''
## 2026-10-27：完整帮助目录与状态边界

[目录CSV](../batches/ground-controls-deepening-inventory.csv)登记8顶层、3子类和65篇独立帮助文章，共76节点。仅以article-card计正文入口，重复快捷卡及同数字ID的别名slug不另计。正文已读、仅目录和原有证据分开；包含账单/创作者合作，不能把76当软件功能总数。E-GRCD-01。

本轮正常匿名Chromium访问search入口HTTP403后即停止，未登录或绕过，没有产品动作或D4。帮助正文及图取得HTTP200；三张[移动筛选](../artifacts/ground-controls-deepening/factuality-image-3.png)、[Web筛选](../artifacts/ground-controls-deepening/factuality-image-4.png)、[来源评级](../artifacts/ground-controls-deepening/factuality-image-5.png)均实际查看，但不是连续操作。完整边界见[来源摘记](../artifacts/ground-controls-deepening/source-notes.md)。

### 已有任务修订

'''
for n,fields in upd.items():s+=f"- F-M046-{n:02}：{fields.get('known_branches','')} {fields.get('gaps','')}。{fields['evidence_ids'].replace(';','、')}。\n"
s+='\n### 新增任务\n\n'
for f in F:s+='#### '+f['flow_id']+' '+f['task']+'\n\n'+f['known_branches']+'。\n\nD2；'+f['evidence_ids'].replace(';','、')+'。缺口：'+f['gaps']+'。\n\n'
s+='''### 相互矛盾的来源与本项目推断

事实性教程称平均三机构，方法论明确两机构；政治偏向才列三机构。教程所附来源页图也在事实性栏列两机构。保留冲突，不宣称“居中一定更准确”；未评级不等于中间或低事实性，评级层级是媒体整体。静态筛选图包含Unknown、Clear和关闭X，不能据图证明取消是否回滚、数目何时变化或缺少评级如何计算。E-GRCD-02、E-GRND-03。

积极新闻页把Custom Feeds写成Pro+，专页写Premium/Vantage；同一日期不同名称未自行等同。For You明确结合使用模式，这比仅“关注主题”多一层隐式输入；减少某主题不保证从全部入口禁止出现。Known Issues最后编辑为6月，不代表已经验证当前10月App仍发生同样问题。E-GRCD-07/10、E-GND-07。

本项目启发（建议）：选题价值、媒体属性、某篇证据、个人改值和阅读行为分别保存来源；新闻卡片与事件对象也可能不同步。AI需要能报告显式过滤和隐式推荐的已知边界，不能把“政治偏向改成中间”当成报道事实变化。对儿童应另外研究年龄适配和理解支架，而不是把成人评级条缩小搬用。账户身份、登录凭据、内容记录、订阅及删除请求需分开，仍不等于已选定本项目账户设计。

本轮仍缺实际筛选零结果、外链返回、收藏撤销、已读阈值、个人统计更新和备份恢复；当前App未安装/登录，无法验证触感、时延、推荐变化或学习/认知收益。

### 本轮证据

'''
for e in E:s+='- '+e['evidence_id']+'：['+e['title']+']('+e['url']+')；'+e['locator']+'；更新日期'+str(e['source_updated_on'] or '未标')+'。\n'
s+='\n原E-GRND-01/03/04/06、E-GND-07/17复查沿用编号，追加原HTML与SHA；同一来源重抓不增加证据数量。没有发送反馈、申请删除、关联账号、重装或改变任何产品账户状态。\n';p.write_text(s)
print(json.dumps(dict(newE=len(E),newF=len(F),oldF=len(upd),oldFields=len(changes),recheckedE=len(ec),inventory=len(I),sources=len(S),visualImages=3,productActions=0),ensure_ascii=False))
