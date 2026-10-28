import csv,json,hashlib,shutil,re
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/duolingo-controls-deepening';A=L/'artifacts/duolingo-controls-deepening';B=L/'batches'
A.mkdir(exist_ok=True);(A/'browser').mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def js(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rs):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
notes=[
('help-index','帮助中心公开目录','三组17篇FAQ：使用、账户、订阅；不是主App全部功能或设置目录。渲染后逐项展开，再点击Read more读取全文；浏览帮助不计产品实操。'),
('help-01','课程改变与路径重定位','课程更新可能重排学习位置；文档没有逐题迁移表，不能把路线位置改变直接解释为学习数据丢失。'),
('help-02','连续天数、冻结与修复','完成每日课程延续连续天数；冻结须提前取得，断签后商店修复另有条件。页面仍称网页货币为lingots，当前界面未核；未购买。'),
('help-03','排行榜、通知与私密档案','每周按XP分组升降级，结束时间取设备时区并有倒计时；关闭排名通知与退出排行榜不同，设为私密会同时关闭好友。13岁以下不开放相关社交，课堂另有设置。'),
('help-04','开源库说明','只作为帮助目录中的技术说明，未沿外链审计第三方库或主应用源码；不计用户任务。'),
('help-05','国际象棋匹配说明','目录有象棋匹配主题；本批未操作棋局、拆解规则或研究匹配结果，不据此补语言课程任务。'),
('help-06','修改用户名和邮箱','账户资料有App/Web设置分支；本批不操作，也不据此判断改名后好友、家庭或数据归属如何迁移。'),
('help-07','关注、屏蔽与社交资格','App由Profile→Following找用户→Block确认；Web从好友档案三点菜单屏蔽，可解除。关注会通知对方；私密和未满13岁不能互相关注。未发送关注、祝贺或报告。'),
('help-08','移除及重置课程','设置Manage courses选择课程并确认；删除该课程进度和XP且不可撤销，连续天数和排行榜名次不随之改变。重加需从头或定级；相同目标语言、不同基础语言须先切正确课程。'),
('help-09','登录与进度缺失','帮助提醒可能进入另一个账户；未验证账户恢复，不能把所有看不到进度的情况归为同步故障。'),
('help-10','个人数据导出与账户删除','Data Vault可请求个人数据副本，处理最多30天；未说明回导恢复。删除须邮件确认，确认后有7天取消期；完成后不可恢复，且不会取消商店订阅。未发出任何请求。'),
('help-11','账户数据遭侵害时的处理','目录保留账户问题处理文章；本批不开展安全事件演练，不把通用帮助当作恢复成功证据。'),
('help-12','Super订阅与权益','帮助仍以无限Hearts等描述权益；不能合并为所有端现行计费规则，2026 Practice免费说明需另读。未试用或购买。'),
('help-13','家庭计划及独立进度','管理者可邀请五人，需对方接受；现有付费订阅要到期后再加入。加入、建立或离开计划不改变个人进度和连续天数；13岁以下私密档案有链接/同机加入办法。此处不是家长可读所有学习数据的证明。'),
('help-14','取消订阅的渠道分支','帮助按购买渠道区分取消方式；本批只登记来源，未验证取消、到期后保留与退款。'),
('help-15','退款申请入口','退款为独立帮助主题；本批未申请，未将政策描述当作具体退款结果。'),
('help-16','优惠码使用范围','帮助含优惠码资格和入口；本批不兑换或比较实时价格，不补写不可见资格判断。'),
('help-17','Max课程及平台限制','帮助按课程、平台描述Video Call/Roleplay；Max不是网页端通用能力。Android版本13+措辞不作为儿童年龄规则。未进行语音会话、辨认音频或校核反馈。'),
('02-energy','Energy规则及推出范围','2025文章称小范围测试：课程消耗、连续正确可补充，课末错题回顾不消耗；约一天补满，广告/宝石可补。精确单位与阈值未给出。本次匿名桌面课实际仍显示Hearts，不能称全平台已切换。'),
('04-path','历史学习路线重构','2022路线介绍：点节点进入课程，单元指南、已完成节点重访及回当前节点入口；将复习混入路线。迁移口径属历史说明，不当2026全课程固定步骤。'),
('05-tslw','学习代理指标、任务和奖励调整','官方从课次/时长转向偏重路线推进的TSLW，描述每日任务由易到难、月挑战从XP改按任务及调整路线XP。它是厂商代理指标与实验报告，不能证明掌握、迁移或本家庭的收益。'),
('06-friends','好友任务与退出','文档写移动端、互相关注且当周活跃后随机配对，五天共同目标；可预写提醒和宝石赠礼，设置可关闭。文章2026更新但仍写不能选队友；未用当前账户核验，不实际发送任何消息。'),
('07-badges','个人纪录与成就','历史介绍将每日XP、完美课程、最长连续天数、最高联赛列为个人纪录；界面入口与当前构建仍需复核。纪录不是能力测评。'),
('08-practice','按技能主动练习','2026指南：iOS/Android底栏杠铃，未见时三点菜单进入；Mistakes/Words/Speak/Listen免费。Stories/Radio/Adventures以课程具备为前提，Max会话回顾另有资格。不是中途续课保证；沿用同来源E-DAD-01。')]
helps=json.loads((W/'help-article-results.json').read_text());hm={h['key']:h for h in helps};E=[]
for n,(key,title,note) in enumerate(notes,1):
 eid=f'E-DUCD-{n:02}'
 if key=='help-index':
  url='https://www.duolingo.com/help';raw=W/'help-rendered.html';mode='anonymous_browser_rendered_help_index';scope='English public Help Center; app build and document update date unknown';updated=None
 elif key.startswith('help-'):
  h=hm[key];url=h['url'];raw=R/h['raw_work_path'];mode='anonymous_browser_rendered_help_article';scope='English Help Center; platform and account conditions per article; unknown app build';updated=None
 else:
  m=json.loads((W/(key+'.meta.json')).read_text());url=m['url'];raw=W/(key+'.html');mode='direct_http_official_article_text';scope='Official dated product/design article; rollout and platform conditions retained; not a current build test';match=re.search(r'"dateModified"\s*:\s*"([^"]+)',raw.read_text());updated=match.group(1) if match else None
 e=dict(evidence_id=eid,product_id='M-031',title=title,url=url,checked_on='2026-10-27',source_type='official_help' if key.startswith('help') else 'official_product_blog',access_mode=mode,version_scope=scope,app_build=None,firmware_version=None,source_updated_on=updated,locator=key,note=note,hands_on=False,visual_observed=False,visual_kind=None,archived_artifact='artifacts/duolingo-controls-deepening/source-notes.md',sha256=None,capture_state='short_source_notes_archived_raw_html_in_work',supersedes=None,raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw))
 if key=='08-practice':e['derived_from']='E-DAD-01'
 E.append(e)
lines=['# Duolingo主学习应用：来源定位与边界','','查阅：2026-10-27。帮助正文经浏览器公开渲染取得；产品博文以HTTP正文读取。未把阅读帮助计为实际使用。短摘记为中文概括，日期、原文和散列逐条见证据批。','','帮助目录只覆盖三组17篇FAQ；其他学习模式、路径设置和动态课程未穷尽。','']
for e in E:lines+=['## '+e['evidence_id']+' '+e['title'],'','[官方来源]('+e['url']+')；定位：'+e['locator']+'。','',e['note'],'']
lines+=['## 取证过程中的限制','','初次HTTP帮助页是动态壳；首次脚本误把展开FAQ当导航而超时，停止后改为正常点击Read more，17篇正文均取得。静态日期或页面可读不保证所有账户显示相同行为。博文正文初版抽取在嵌套div截断，已从完整文本重新提取；不保留截断文本作为正面结论。','', '实际课程操作另有审计；没有登录个人账号、购买、发送邀请、删除课程或请求数据导出。','']
(A/'source-notes.md').write_text('\n'.join(lines))
for e in E:e['sha256']=sha(A/'source-notes.md')
# Browser evidence, flow rows and inventory are appended by finalize_batch.py after the run.
js(W/'prepared-evidence.json',E)
print({'prepared_docs':len(E),'source_notes':str(A/'source-notes.md')})
