from pathlib import Path
import json,csv,hashlib,re,shutil
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/weread-controls-deepening';A=L/'artifacts/weread-controls-deepening';B=L/'batches'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
jwrite=lambda p,x:p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
b=json.loads((W/'baseline.json').read_text());caps=json.loads((W/'captures.json').read_text());cap={r['name']:r for r in caps}
assert sha(W/'case-before.md')==b['case_sha256']
assert not b['reverse_dependencies']
for f,h in b['old_batch_hashes'].items(): assert sha(R/f)==h,(f,'baseline drift')
for n in ['ai-entry.jpg','ai-citations.jpg','ai-selection.png']:shutil.copyfile(W/n,A/n)
# Source excerpts are summaries, not full-page mirrors. Raw material stays in work.
notes='''# M-013 本轮来源与取得范围

查阅2026-10-27。普通匿名公开GET；未执行网页脚本、安装Skill、取得Key或调用个人接口。原件与哈希见工作目录captures.json及交接清单。本文件为短摘记，不是完整帮助镜像。

## E-WRCD-01 官网内嵌Web帮助

[官网公开脚本](https://cdn.weread.qq.com/web/wrweb-next/_nuxt/C_fW_JUQ.js)。官网当日HTML仍引用该文件；FeedbackModal中的4条question/answer是帮助正文常量，按JSON字符串解码，未执行脚本。定位字符偏移654472、654693、654909、655148；构建号/帮助更新时间未给出。购买、背景、目录搜索、私密阅读4项均已读取，不能视为手机版完整帮助。功能和缺口见目录台账及F-M013-08/09。

## E-WRCD-02 2024年AI问书说明

[腾讯云大数据文章](https://developer.cloud.tencent.com/article/2433150)，“AI问书能力介绍”节前三项及阅读片段段落。页显示2024-07-01，另标原始发表2024-06-28；[作者简介](https://developer.cloud.tencent.com/user/1920198)自称腾讯云大数据官方账号。未给OS/构建号。操作补回F-M013-01；3张静态图审计独立保存。文章后半RAG架构、准确性和性能宣称不作为用户操作验证。

## E-WRCD-03/04 补齐固定提交的能力文件

[review.md](https://raw.githubusercontent.com/Tencent/WeChatReading/d98e66ee60a8c7bcd35da549d0643c4470439643/skills/review.md)区分公开点评和私人笔记，规定筛选、游标和嵌套对象；本轮只建目录，不新增点评任务。

[discover.md](https://raw.githubusercontent.com/Tencent/WeChatReading/d98e66ee60a8c7bcd35da549d0643c4470439643/skills/discover.md)的工作流区分关键词搜索、个性化及相似推荐；补回F-M013-03。默认12条仅适用于个性化推荐；相似推荐首次须显式count=12、maxIdx=0，后续携会话。返回空结果和调用失败分别处理。以上均为静态契约，未获真实返回。

## 旧E重读与版本边界

- E-WRD-01：[中国区App Store](https://apps.apple.com/cn/app/id952059546)当日正文10.2.3；HTML time datetime=2026-10-26，属于源HTML日期而非由“14小时前”倒算。10.2.2=2026-09-17的导入书字号与选中文本反馈声明不证明导入链。另有10.1.0重新绑定微信公告，不等于恢复或账号数据合并。
- E-WRD-03：旧SKILL.md顶部1.0.5；同提交旧README仍写1.0.4，不能抹平。异常约定含版本提示暂停；不由此推定升级成功。目录8能力行关联7详细文件，本轮全部正文已读，范围仅Skill。
- E-WRD-04/05：旧书架/搜索文档重读；列表条目数包括专辑及文章收藏入口，入口不含文章正文；分页结果不等于云端全库。
- E-WRD-06：旧book.md表格用100表示读完，输出格式又要求100且有finishTime；保留更严格输出条件及缺字段的不确定性。
- E-WRD-07：旧notes.md统计包括书签，内容导出排除书签；个人点评已计入reviewCount。热度、热门原文、他人评论另为对象，不混入私人笔记备份。
- E-WRD-08：旧readdata.md只接固定自然周期；非完整日期边界缺日明细时应标近似。preferAuthor.readTime字段表为格式化字符串，与末尾笼统“全部按秒”不一致，不能机械数值转换。

## 未取得与排除

官网Web帮助没有手机导入/离线/恢复完整说明；腾讯客服公开首页未发现该产品专属帮助目录。搜索的第三方桌面封装/聚合FAQ、非官方用户教程及下载站未作为事实证据。未访问登录后帮助、未尝试账户或内容接口。没有遇到本轮401/403/429；之前其他产品失败不转记本产品。缺口不等于不支持。
'''
(A/'source-notes.md').write_text(notes)
audit='''# E-WRCD-02 静态图核查

2026-10-27实际通过view_image逐张查看以下原图。来源为2024年文章“AI问书能力介绍”，OS/构建未知。没有操作、连续录屏或点击结果；仅为D2文档的静态参考。

| 文件 | 文章位置 | 实际可见内容及限制 |
| --- | --- | --- |
| ai-entry.jpg | 输入问题段后的首图 | 问题搜索栏、AI问书选中页签、带编号的回答；非输入或加载过程。 |
| ai-citations.jpg | 回答来源段后的图 | 编号书籍来源、作者书名和节选；不能单靠图证明已跳转原书。 |
| ai-selection.png | 阅读片段段后的图 | 正文选区与复制/划线/写想法/书摘/查询/AI问书菜单；高亮框是原图标记，未观察点击或保存。 |

第一图展示工具缩至1040×2048（原1080×2126）；其余查看原尺寸。引用跳至具体段落仅由正文说明支持。未看其余插图，不登记视觉观察。图片原URL、SHA和尺寸见evidence视觉附件；本审计独立于案例文本，防止后续编辑案例使散列失配。
'''
(A/'visual-audit.md').write_text(audit)
new=[]
def E(n,title,name,typ,scope,locator,date=None,visual=False):
 c=cap[name];r={'evidence_id':f'E-WRCD-{n:02d}','product_id':'M-013','title':title,'url':c['url'],'checked_on':'2026-10-27','source_type':typ,'access_mode':'public_html_body_and_static_images' if visual else ('public_javascript_documentation_literals' if name.endswith('.js') else 'public_repository_markdown'),'version_scope':scope,'app_build':None,'firmware_version':None,'source_updated_on':date,'locator':locator,'note':'静态公开材料；未登录、未执行产品动作；详见本轮短摘记与目录台账。','hands_on':False,'visual_observed':visual,'archived_artifact':str((A/'source-notes.md').relative_to(L)),'sha256':sha(A/'source-notes.md'),'capture_state':'short_note_and_raw_capture','raw_work_path':c['raw_work_path'],'raw_sha256':c['raw_sha256'],'supersedes':None}
 if name.endswith('.md'):r.update(source_updated_on=None,source_commit_on='2026-10-10')
 new.append(r);return r
r=E(1,'微信读书官网FeedbackModal四项Web帮助正文','web-entry.js','official_public_frontend_help','Web help text in bundle referenced by official homepage on 2026-10-27; build/update date unknown; not phone UI','FeedbackModal question/answer offsets 654472/654693/654909/655148')
r['provenance_captures']=[{k:cap['homepage.html'][k] for k in ['url','checked_on','raw_work_path','raw_sha256']}];r['provenance_captures'][0]['locator']='首页帮助与反馈文字及script src引用该bundle'
r=E(2,'腾讯云大数据：2024年微信读书AI问书说明','ai-rag.html','official_team_technical_article','Historical WeRead AI search/reading-selection UI, article 2024-07-01 with original-published 2024-06-28; OS/app build unspecified','AI问书能力介绍：输入问题、回答来源、猜你想问、阅读片段段；该节插图中的第1/2/4图','2024-07-01',True)
r['provenance_captures']=[{k:cap['author.html'][k] for k in ['url','checked_on','raw_work_path','raw_sha256']}];r['provenance_captures'][0]['locator']='作者简介“腾讯云大数据官方账号”；不额外推定认证标识'
r['visual_audit_path']=str((A/'visual-audit.md').relative_to(L));r['visual_audit_sha256']=sha(A/'visual-audit.md');r['visual_scope']='Three static article illustrations only, not continuous interaction'
r['visual_kind']='static_official_images'
r['artifact_hashes']={str((A/n).relative_to(L)):sha(A/n) for n in ['visual-audit.md','ai-entry.jpg','ai-citations.jpg','ai-selection.png']}
r['visual_artifacts']=[]
for n,loc in [('ai-entry.jpg','问题输入说明后的首图'),('ai-citations.jpg','回答来源说明后的图'),('ai-selection.png','阅读片段说明后的菜单图')]:
 c=cap[n];r['visual_artifacts'].append({'archived_artifact':str((A/n).relative_to(L)),'sha256':sha(A/n),'source_url':c['url'],'raw_work_path':c['raw_work_path'],'raw_sha256':c['raw_sha256'],'locator':loc,'observed_on':'2026-10-27','hands_on':False})
E(3,'WeRead Skill公开点评详细文档','review.md','official_repository_docs','Tencent/WeChatReading fixed commit d98e66ee60a8c7bcd35da549d0643c4470439643; Skill frontmatter 1.0.5 / README 1.0.4 discrepancy; not executed','/review/list参数、双层review对象、工作流与空结果/失败输出','2026-10-10')
E(4,'WeRead Skill个性化和相似推荐文档','discover.md','official_repository_docs','Same fixed Tencent commit; public read-only contract, no user endpoint called','/book/recommend、/book/similar参数与工作流','2026-10-10')
(B/'weread-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in new))
fields=[k for k in b['flows'][0] if k!='batch']
newflows=[]
def F(num,task,branch,gaps):
 newflows.append(dict(zip(fields,[f'F-M013-{num:02d}','M-013',task,'D2','E-WRCD-01',branch,gaps,'not_observed','false','cases/M-013-weread.md','2026-10-27'])))
F(8,'Web展开目录并在本书内搜索','阅读右下工具栏第2按钮展开目录；目录顶部输入关键词搜当前书','结果点击、无匹配、搜索清除/取消、返回与位置保留未知；帮助更新时间及构建未知')
F(9,'Web切换深浅背景','阅读右下工具栏月亮/太阳切换深浅模式；帮助描述默认深色','跨书/重开/多端是否保留、与系统主题关系未知；未实际观察界面')
with (B/'weread-controls-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(newflows)
changes={
'F-M013-01':{'depth':'D2','evidence_ids':'E-WRD-01;E-WRD-07;E-WRCD-02','known_branches':'仅2024历史AI子链D2：搜索问题→AI问书→回答及来源→文档称可跳书中段落；正文选区入口有静态图','gaps':'当前版本沿用性、查词结果/划线保存撤销、AI取消失败/返回原位仍未知；2024 OS与构建未给','visual_status':'static_reference_only'},
'F-M013-02':{'evidence_ids':'E-WRD-01;E-WRD-08','known_branches':'10.2.2公告导入书独立字号；定时听书公告；读听分项统计为条件字段','gaps':'格式导入/重复与失败、离线、续播和读听位置映射；分项字段缺失不代表零'},
'F-M013-03':{'evidence_ids':'E-WRD-02;E-WRD-03;E-WRD-04;E-WRD-05;E-WRCD-04','known_branches':'搜索类别/分页/空数据；书架含专辑及文章入口；推荐与相似推荐游标不同，后者显式count/maxIdx并续sessionId','gaps':'是否覆盖所有私有内容和写动作、推荐服务实际可用性；未调用接口'},
'F-M013-04':{'known_branches':'0/1—99/100进度；输出规则要求100且有finishTime才标读完；目录分层与付费状态','gaps':'100但finishTime缺失的真实状态、位置回写、读听切换与同步冲突未知'},
'F-M013-05':{'known_branches':'统计含书签而内容排除；reviewCount已含个人点评；notebooks至hasMore=0、mine用synckey；公开点评与个人内容分开','gaps':'导出文件保存/备份完整性、修改回写、删除撤销；热门top20与热度不替代个人全量'},
'F-M013-06':{'known_branches':'默认月；固定自然周期拼接，边界无日明细须标近似；自然日日均；时段从6时起，作者时长是格式化字符串','gaps':'离线补传/时区/分项保真；排行和读听字段可缺省；字段表与全部按秒的概括存在例外'},
}
updates=[]
for old in b['flows']:
 if old['flow_id'] not in changes:continue
 after=changes[old['flow_id']]|{'checked_on':'2026-10-27'}
 for k,v in after.items():
  if old[k]!=v:updates.append({'flow_id':old['flow_id'],'source_batch':old['batch'],'field':k,'before':old[k],'after':v,'reason':'本轮公开文档/静态图补原任务；由root核验合并，不修改旧批'})
jwrite(W/'existing-coverage-updates.json',updates)
# Existing URL/source captured again keeps the old evidence ID.
adds=[]
for e in b['evidence']:
 id=e['evidence_id'];num=int(id[-2:])
 if num==1:
  c=cap['appstore.html'];ac={'checked_on':'2026-10-27','capture_url':c['url'],'final_url':c['final_url'],'access_mode':'public_html_body','raw_work_path':c['raw_work_path'],'raw_sha256':c['raw_sha256'],'locator':'10.2.3 HTML time datetime=2026-10-26；10.2.2 datetime=2026-09-17','app_build':'10.2.3','source_updated_on':'2026-10-26','limitation':'日期来自HTML标记；历史日志不证明当前操作链'}
 elif 3<=num<=8:
  name={3:'SKILL',4:'shelf',5:'search',6:'book',7:'notes',8:'readdata'}[num];p=R/f'work/chinese-apps/wrd-{name}.md'
  ac={'checked_on':'2026-10-27','capture_url':e['url'],'access_mode':'local_reread_of_existing_public_artifact','raw_work_path':str(p.relative_to(R)),'raw_sha256':sha(p),'original_capture_on':'2026-10-26','locator':e['locator'],'limitation':'本轮重读旧固定提交原件，未重新联网取得；API未执行'}
  if num==3:
   p2=R/'work/chinese-apps/wrd-README.md';ac['auxiliary_original']={'raw_work_path':str(p2.relative_to(R)),'raw_sha256':sha(p2),'locator':'版本段1.0.4，与SKILL顶部1.0.5不同'}
 else:continue
 ac.update(archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md'),hands_on=False,visual_observed=False)
 before=e.get('additional_captures',[])
 adds.append({'evidence_id':id,'source_batch':e['batch'],'field':'additional_captures','before':before,'after':before+[ac],'reason':'同URL/原件补采集元数据；保留原始记录，不另编E'})
jwrite(W/'existing-evidence-additional-captures.json',adds)
# Read directory entries are distinguished from achieved task coverage.
inv=[]
def I(id,platform,domain,section,status,es,fs,gap):inv.append({'inventory_id':id,'product_id':'M-013','platform_version':platform,'function_domain':domain,'source_locator':section,'body_status':status,'evidence_ids':es,'flow_ids':fs,'remaining_gap':gap,'checked_on':'2026-10-27'})
web='Web官网现载帮助；文档更新日/构建未知'
for j,(d,s,f,g) in enumerate([
('购买书与会员','FAQ1','', '付费章节末购买范围与App充值分支有正文；支付确认/取消/失败未分析；会员限制仅限该帮助快照'),
('阅读背景','FAQ2','F-M013-09','持久化与系统主题关系未知'),
('目录/本书搜索','FAQ3','F-M013-08','结果、无匹配和返回状态未知'),
('书架与私密阅读','FAQ4','F-M013-03','帮助声称设置在App、Web同步状态；无手机具体入口/冲突恢复，不据此断言现行Web能力')],1):I(f'WR-W{j:02}',web,d,s,'body_read','E-WRCD-01',f,g)
sk='Skill固定提交d98e66e；SKILL1.0.5/README1.0.4；未调用'
I('WR-S00',sk,'支持能力父目录','SKILL.md支持的能力8行/7个详细文件','index_and_all_linked_body_read','E-WRD-03;E-WRCD-03;E-WRCD-04','','父目录不是8个已完成任务，不涵盖手机App全功能')
rows=[('搜索','search.md /store/search','E-WRD-05','F-M013-03','类别、分页；结果后用户实际跳转未测'),('书架','shelf.md /shelf/sync','E-WRD-04','F-M013-03','只读列表与计数；归档/删除/私密写入无契约'),('书籍信息','book.md /book/info','E-WRD-06','F-M013-04','书籍无权访问/已下架等真实返回未测'),('章节目录','book.md /book/chapterinfo','E-WRD-06','F-M013-04','层级/收费/锚点字段；客户端跳转未知'),('阅读位置','book.md /book/getprogress','E-WRD-06','F-M013-04','100与finishTime条件；无进度写回'),('笔记本概览','notes.md /user/notebooks','E-WRD-07','F-M013-05','计数与内容范围不同'),('个人划线','notes.md /book/bookmarklist','E-WRD-07','F-M013-05','过滤书签；文件保存/回写未知'),('个人想法/点评','notes.md /review/list/mine','E-WRD-07','F-M013-05','游标与非选区点评；不重复加计点评数'),('章节热度','notes.md /book/underlines','E-WRD-07','','有正文但未形成独立任务；只有计数/热度/位置，不含原文'),('热门划线','notes.md /book/bestbookmarks','E-WRD-07','','有正文但未建任务；固定top20无分页，不是用户全部划线'),('划线下的想法','notes.md /book/readreviews','E-WRD-07','','有正文但未建任务；每请求count上限20/各range分页，结束分支未展开'),('单条想法详情','notes.md /review/single','E-WRD-07','','有正文但未建任务；详情/评论/点赞对象，不执行互动'),('公开点评','review.md /review/list','E-WRCD-03','','正文读过未建新F；筛选与游标链可续析；不属于私人笔记'),('个性化推荐','discover.md /book/recommend','E-WRCD-04','F-M013-03','推荐原因和searchIdx，实际推荐不可用情况未测'),('相似推荐','discover.md /book/similar','E-WRCD-04','F-M013-03','首次count/maxIdx必传，继续携sessionId；没有会话失效策略'),('阅读统计','readdata.md /readdata/detail','E-WRD-08','F-M013-06','周期/字段条件已析，离线上传/跨时区规则缺失')]
for j,(d,s,e,f,g) in enumerate(rows,1):I(f'WR-S{j:02}',sk,d,s,'body_read',e,f,g)
I('WR-S17',sk,'请求/错误/版本','SKILL.md请求格式/通用规则','body_read','E-WRD-03','F-M013-03;F-M013-04;F-M013-05;F-M013-06','只读静态契约，未请求/_list；版本提示不证明升级已完成')
app='中国区iPhone/iPad商店2026-10-27快照；当前10.2.3，按历史版本分列'
for j,(d,s,f,g) in enumerate([
('划线/想法/查词','描述及9.4.0','F-M013-01','仅功能/优化日志；保存、纠错、撤销未取得'),('导入书字号','10.2.2 2026-09-17','F-M013-02','单书字号声明；文件选择/解析/重复/失败未取得'),('TTS/有声内容/定时','描述及10.1.1','F-M013-02','未取得下载、换声、离线、续播和锚点转换操作'),('想法/书评/评论配图','10.2.0 2026-06-30','F-M013-01','仅日志；添加取消/提交失败未知'),('选中文本反馈','10.2.2 2026-09-17','F-M013-01','疑似AI文本反馈声明；未提交，受理/撤回未知'),('重新绑定微信','10.1.0 2026-03-27','','仅日志，不是迁移/合并/恢复保证'),('简繁阅读设置','9.3.0 2025-04-29','','日志只称工具栏转换；不展开未核按钮/保存机制')],1):I(f'WR-A{j:02}',app,d,s,'version_declaration_only','E-WRD-01',f,g)
I('WR-A08','2024-06/07历史App；OS/构建未知','AI问书及来源','AI问书能力介绍；3张实际查看图','body_and_static_images_read','E-WRCD-02','F-M013-01','无取消/错误/返回原位；不自动套10.2.3')
I('WR-A09','注销须知；客户端构建未知','注销及等待期撤回','注销须知1—5/7；沿用旧正文','previous_body_only','E-WRD-09','F-M013-07','本轮未重抓；进入路径和备份未知')
I('WR-G01','手机App；未获版本化帮助','备份/恢复/多端冲突','已有Skill只读契约及注销说明边界','gap_not_directory_entry','E-WRD-03;E-WRD-09','','未取得独立备份/回滚帮助；不是断言功能不存在')
with (B/'weread-controls-deepening-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');w.writeheader();w.writerows(inv)
# Preserve the old evidence bibliography and append only the new material.
old=(W/'case-before.md').read_text();bibliog=old[old.index('## 证据'):]
case='''# M-013 微信读书：手机阅读、Web帮助与Skill只读接口分开核查

查阅日2026-10-27。中国区iPhone/iPad商店正文当前10.2.3（HTML日期2026-10-26）；历史日志只证明公告能力。2024年AI问书文章未给OS/构建，不能自动套到当前App。官网Web帮助来自当日仍被首页引用的公开脚本中的帮助正文常量，未执行脚本；其更新日/构建未知。Skill固定提交`d98e66ee60a8c7bcd35da549d0643c4470439643`（2026-10-10），SKILL顶部1.0.5与README的1.0.4保留差异。

本轮只读公开资料，未安装、登录、取Key或请求用户接口。实际看过3张历史静态图，不是连续体验。原7F保留，补2个Web任务；F01的D2仅限历史AI子链，普通手机查词/笔记修改主链仍D1/未知；Skill的D2为静态契约，不代表调用成功。

## 功能目录与覆盖

[逐项台账](../batches/weread-controls-deepening-inventory.csv)含Web四项帮助、Skill父目录及16个已读取接口章节、通用请求约定、商店版本声明、历史AI与注销边界；其中父子条目与缺口行不等于独立完成任务。Skill8项能力目录对应7个详细文件，本轮均读正文，但不构成App完整功能目录。

| 功能域 | 来源/读取范围 | 关联任务与剩余范围 |
| --- | --- | --- |
| Web购买、背景、目录搜索、私密阅读 | E-WRCD-01四条帮助正文 | F08/09；购买及私密设置有分支描述，未形成完整支付/写入任务；未读取手机帮助目录 |
| 手机阅读、划线、想法、听书、导入、字号、重绑微信 | E-WRD-01历史日志；2024选区静态图 | F01/02；只有局部入口与声明，未补出创建/撤销/离线/恢复链 |
| AI问题与引用 | E-WRCD-02历史文章正文及3图 | 补F01；来源返回原位、失败和现行适用性未知 |
| Skill搜索/推荐/书架 | E-WRD-03—05、E-WRCD-04正文 | F03；只读枚举和推荐，不等于书架编辑或完整备份 |
| Skill书籍/章节/位置 | E-WRD-06正文 | F04；读完条件、层级与付费字段；无位置写回 |
| Skill个人笔记与公开讨论 | E-WRD-07、E-WRCD-03正文 | F05个人导出；热度、热门划线、划线评论、单条详情及公开点评读过未独立展开，仍可按原文继续 |
| Skill统计与账户终止 | E-WRD-08/09 | F06/07；统计的条件字段及注销撤回，不能回答备份/恢复 |

## 流程与条件

| F编号 | 入口/前置 | 动作、反馈与结束 | 异常/恢复边界 | 证据/深度 |
| --- | --- | --- | --- | --- |
| F-M013-01 手机阅读与理解 | 2024历史AI界面；一般手机功能另限商店声明 | 搜索框输入问题→AI问书→回答及来源；正文称点来源可到原书段落；阅读选区亦有问书入口 | AI取消/失败/返回原位及现行沿用性未知。普通查词结果、划线保存撤销仍未核 | 历史AI子链D2，其余D1；E-WRCD-02、E-WRD-01/07。静态图仅参考 |
| F-M013-02 听书/导入 | App描述及历史日志 | TTS/有声、定时、导入书独立字号有声明；E08统计可条件区分文字与收听 | 文件格式/送入/解析/重复、离线、换声续播、读听锚点映射未知；无分项字段不等于零 | D1；E-WRD-01/08 |
| F-M013-03 Skill找书/书架/推荐 | 用户已自行配置Key；本轮未执行 | 搜索类别与分页；书架按电子书+专辑+非空文章入口计数。推荐后可按bookId取详情；相似推荐首次显式count/maxIdx，继续携sessionId及上页末idx；仅使用返回阅读/详情链接 | 空推荐与失败分开；书架入口不含文章正文，未证私有内容全量/写操作；相似会话失效未知 | D2契约；E-WRD-02—05、E-WRCD-04 |
| F-M013-04 Skill位置/章节 | 已确定bookId | 读信息/分层目录/进度并展示百分比。表格称100读完，输出规则另要求finishTime存在；依后者才标读完 | 100但时间缺失不补猜；读取失败与无记录分开；无自动翻页、位置修改或冲突合并 | D2；E-WRD-03/06 |
| F-M013-05 Skill个人笔记整理 | notebooks概览→选bookId | 概览按lastSort至hasMore=0；划线列表过滤书签，个人想法按synckey续取；保留原文及位置（存在时）。总数含书签，reviewCount已含个人点评，不再加计 | 文件落盘由调用端决定；未证完整备份、回写或恢复。热门top20/章节热度/公开评论不是私人全量笔记 | D2；E-WRD-03/07 |
| F-M013-06 Skill阅读回顾 | 指定自然周期，缺省月 | 跨年按年分段；完整周期用totalReadTime，边界有日明细才精确，否则标近似。自然日日均与有效阅读日不同；时段数组从6时起，作者时长字段为已格式化文本 | 未证明离线补传/时区；条件字段缺省不是零。末尾全部按秒的概括不能覆盖作者字段表的例外 | D2；E-WRD-03/08 |
| F-M013-07 注销与撤回 | 已到注销申请页、先关连续包月 | 申请退出登录→15天等待；期间重登撤回；期满后原数据不能通过同微信登录恢复 | 进入页路径、可用备份及卸载恢复范围未取得；本轮沿用旧证据 | D2；E-WRD-09 |
| F-M013-08 Web目录及本书搜索 | 已处阅读页；官网现载帮助未给构建 | 右下工具栏第2按钮打开目录→顶部输入关键词→搜索本书内容 | 结果选择、无匹配、清除/返回与位置保留未述；未实际操作 | D2文档；E-WRCD-01 |
| F-M013-09 Web深浅背景 | 阅读页工具栏；帮助称默认深色 | 月亮/太阳图标可切换深浅模式；阅读背景为目标状态 | 跨书、重开持久化、系统主题/多端同步未知；未观察实际界面 | D2文档；E-WRCD-01 |

## 异常、数据对象与未覆盖

- E-WRD-03约定字段裁剪、非零错误码与upgrade_info暂停；文档中的版本处理不能证明已完成升级或网络重试。参数平铺、分页终点与空数据须分别记录。
- E-WRD-07的笔记数量包括书签，内容导出却排除书签；热门划线固定top20无分页，热度只有位置/计数，均不能填补个人笔记缺页。E-WRCD-03公开点评有自己的筛选/游标，是他人评价，不加入私人笔记数。
- Web帮助声称私密状态与手机同步、设置在App；这是未标日期的帮助说明，不能据此给现行Web能力下无条件结论，更不保证删除恢复。读接口、云同步、备份和恢复是不同能力。
- 没有取得手机完整帮助目录，也没有得到导入格式/大小/失败回滚、误划线撤销、下载不完整、离线听书、进程中断位置、多端冲突/账户合并的完整说明。商店重新绑定微信公告不等于数据恢复。

## 静态观察与本项目推断

[独立视觉审计](../artifacts/weread-controls-deepening/visual-audit.md)只记录2024文章的3张图片：问答带编号、来源列表、正文选区菜单。跳转动作由文字支持，未看到实际跳转或返回；没有D3/D4。旧E无视觉审计散列指向本案例，新审计独立保存。

对G-002/R-007，本项目可借鉴“回答→来源位置”的对象联系及结构化只读接口；应分别确认解释、修改、撤销、持久化和恢复权限。参与时长/排行不是理解或掌握证据。对G-001及5–6岁自主读听，解释后回原位、读听位置衔接仍是本案例待观察的成本；不据此断言本项目硬件能运行微信读书。

下一类必要材料是固定手机版本的官方导入/笔记/听书帮助，或经授权的自有短文连续观察（导入→查词→笔记→问书→返回→听书→离线重启）；本轮未做。现有公开Skill可继续分析热门划线/点评，但不能替代缺失的手机动作链。本轮停止扩源，旧F/E字段建议交root合并，未改旧批或主索引。

'''+bibliog+'''
- E-WRCD-01：[官网Web帮助常量](https://cdn.weread.qq.com/web/wrweb-next/_nuxt/C_fW_JUQ.js)；FeedbackModal四项；正文，不是执行脚本或界面观察。
- E-WRCD-02：[2024 AI问书说明](https://developer.cloud.tencent.com/article/2433150)；AI问书能力介绍；正文及3张实看静态图，平台/构建未知。
- E-WRCD-03：[公开点评文档](https://raw.githubusercontent.com/Tencent/WeChatReading/d98e66ee60a8c7bcd35da549d0643c4470439643/skills/review.md)；固定提交，目录及正文已读，未调用。
- E-WRCD-04：[发现推荐文档](https://raw.githubusercontent.com/Tencent/WeChatReading/d98e66ee60a8c7bcd35da549d0643c4470439643/skills/discover.md)；固定提交，推荐/相似的参数与分页，未调用。

本轮原件与来源边界另见[短摘记](../artifacts/weread-controls-deepening/source-notes.md)。旧E-WRD-01原记录的相对日期保留；本轮新增采集明确取得HTML日期，尚不覆盖更早或以后版本。
'''
(L/'cases/M-013-weread.md').write_text(case)
jwrite(W/'build-result.json',{'new_evidence':len(new),'new_flows':len(newflows),'inventory_rows':len(inv),'old_flow_count':len(changes),'old_flow_field_changes':len(updates),'old_evidence_additional_captures':len(adds),'static_images':3,'case_audit_reverse_dependencies':0})
print(json.dumps(json.loads((W/'build-result.json').read_text()),ensure_ascii=False))
