import csv,json,hashlib,shutil,zipfile
from pathlib import Path
from datetime import datetime,timezone
root=Path('/home/workspace/reader'); lib=root/'docs/research/interaction-library'; work=root/'work/week-junior-controls-deepening'; art=lib/'artifacts/week-junior-controls-deepening'; art.mkdir(exist_ok=True); browser=art/'browser';browser.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
url='https://reader.emagazines.com/issue/the-week-junior/october-23-2026?ipt=F0826FC3-4129-46CF-A5F3-FF70A931F1FF'
steps={
'01-initial':('打开官网公开样刊链接','新上下文从封面1/32开始'),
'02-next':('点击Next page','页码变2，显示对应新闻版面'),
'03-thumbnails':('点击Show page thumbnails','侧栏及底部横向缩略图展开，显示封面和成对页范围'),
'04-after-close':('点击缩略图右上关闭；随后寻找折叠状态下的页码输入框超时','留在第2页；自动定位失败，不算成功跳转'),
'05-search-open':('隐藏Toggle navigation定位超时后，点击可见搜索图标(40,38)','侧栏展开，底部页码输入框可见'),
'06-jump14':('在可见pageNumberInput填14并按Enter','页码14和对应科学版面，URL为#p14'),
'07-search-nobel':('搜索框输入Nobel并Enter','5个结果组，含页码、计数、缩略图及标亮摘要；当前页仍14'),
'08-result15':('点击结果Page 15，等待1秒后捕获','结果项高亮，但页图、页码与URL仍为14；未观察到目标页打开'),
'09-search-empty':('输入zzqzxq-no-such-term并Enter','结果数0，阅读页仍14'),
'10-save-place':('Clear search；点击Save my place','出现书签成功和本设备下次访问回此页的提示'),
'11-front':('点击Go to first page','回封面，页码1，未重新保存书签'),
'12-reopen-same-context':('同一浏览器上下文重新打开无页码的公开期刊入口','回第14页；一次实测支持手动书签优先于之后浏览到的封面'),
'13-new-context':('同一浏览器新建空上下文，打开同一入口','从封面1/32开始；没有复制任何原上下文状态'),
'14-last-page':('回到原上下文点击Go to last page','32/32和封底；之后Previous page被展开侧栏拦截而超时，未产生回31/再前32截图')}
states=[]
for name,(action,result) in steps.items():
 src=work/'browser'/(name+'.png'); shutil.copy2(src,browser/src.name)
 raw=json.loads((work/'browser'/(name+'.json')).read_text())
 states.append({'capture':name,'action':action,'observed_result':result,'url':raw['url'],'page_number':next(i['value'] for i in raw['inputs'] if i['id']=='pageNumberInput'),'search':next(i['value'] for i in raw['inputs'] if i['id']=='searchInput'),'captured_at_utc':datetime.fromtimestamp(src.stat().st_mtime,timezone.utc).isoformat(),'image':str((browser/src.name).relative_to(lib)),'sha256':sha(src)})
(browser/'observed-states.json').write_text(json.dumps(states,ensure_ascii=False,indent=2)+'\n')
# Durable operation log: only UI actions and their actual result/error; omit network bodies/cookies.
trace_manifest=[]
for name in ['trace.zip','fresh-context-trace.zip']:
 src=work/'browser'/name
 with zipfile.ZipFile(src) as z: events=[json.loads(l) for l in z.read('trace.trace').decode().splitlines()]
 before={e['callId']:e for e in events if e['type']=='before'}; after={e['callId']:e for e in events if e['type']=='after'}
 meta=next(e for e in events if e['type']=='context-options')
 actions=[]
 for cid,e in before.items():
  if e['method'] not in ['goto','click','fill','press','screenshot']:continue
  a=after.get(cid,{})
  actions.append({'method':e['method'],'class':e['class'],'params':e.get('params',{}),'start_ms':e['startTime'],'end_ms':a.get('endTime'),'error':a.get('error')})
 target=browser/(name.replace('.zip','-actions.json'))
 target.write_text(json.dumps({'origin_trace_sha256':sha(src),'context_metadata':meta,'actions':actions},ensure_ascii=False,indent=2)+'\n')
 trace_manifest.append({'path':str(src.relative_to(root)),'sha256':sha(src),'bytes':src.stat().st_size,'formal_action_log':str(target.relative_to(lib))})
(browser/'raw-trace-manifest.json').write_text(json.dumps(trace_manifest,indent=2)+'\n')
lines=['# M048公开样刊：浏览器操作核查','', '观察日：2026-10-26。E-WJCD-02、E-WJCD-03、E-WJCD-04；F-M048-05、F-M048-06、F-M048-07。','',
'这是根代理用Playwright控制真实网页并逐张查看截图的记录。`hands_on=true`只表示自动化网页实际操作，`operation_method=automated_public_browser`；不是人在设备上的手感试用，也不是手机App实测。D4按这些有限任务登记，不表示全部成功分支、故障或整个产品已完成。','',
'环境：Linux，Chromium Headless Shell 156.0.8078.4，1200×900视口，匿名临时上下文，正常联网。Python安装分发版本为Playwright 1.64.1；trace内部版本字段为1.64.0-beta-1791568591000，分别保留。网站未给前端构建号。内容为美国版2026-10-23样刊、340期、界面32页；仅观察所列版面，没有逐页读完一期。','',
'入口由[官网样刊按钮](https://theweekjunior.com/in-the-magazine)经`ema.gs/WiL`导向[公开阅读器]('+url+')；URL中的ipt来自公开样刊链接，没有猜测其他资源或绕过订阅。未登录、订阅、投票、投稿、分享或听取音频。','',
'## 实际序列与失败','',
'| 截图 | 动作 | 观察结果 |','| --- | --- | --- |']
for s in states:lines.append(f'| [{s["capture"]}](browser/{s["capture"]}.png) | {s["action"]} | {s["observed_result"]} |')
lines += ['', '两个隐藏控件定位各超时30秒，末尾Previous page点击因侧栏拦截超时5秒；这些是本次自动操作的失败，不能直接推定普通用户必然遇到产品缺陷。未强制点击、未直接调用站点内部跳页函数。Page 15结果未跳转只记录本次1秒等待后的状态，原因未定，也没有将其补写成成功。', '',
'## 复查方法与判断边界','',
'1. 用上述视口、新浏览器上下文打开公开入口；点Next，再点Thumbs并关闭。点击左上搜索图标展开侧栏，在可见页码框填14并Enter。',
'2. 输入Nobel，观察结果数量及页码组；点击Page 15后核对主图、页码、URL三者。再输入无匹配字符串，最后清空。结果随期刊入口更新可能改变，复查须固定同一期。',
'3. 在14页点Save my place，等成功提示退去，回封面；重新打开无页码的期刊入口。另建空上下文打开相同入口，对比是否回到14。',
'4. 末页入口只证明可以定位32页；本次没有“全部阅读完成”的判定或结算反馈。', '',
'本轮主序列执行1次，另有1个空上下文对照。没有重复统计稳定性、测延迟、离线、浏览器进程重启、清除站点数据或跨设备同步。书签在同一上下文重访有效、空上下文从封面开始；底层是cookie、localStorage还是其他存储没有核查，因此不宣称永久本地保存或支持账号同步。', '',
'正式保留[观察状态与截图散列](browser/observed-states.json)、[主操作日志](browser/trace-actions.json)、[新上下文日志](browser/fresh-context-trace-actions.json)。原始trace含网络/页面快照，仅留work；[原件散列与路径](browser/raw-trace-manifest.json)不冒充原件已正式归档。正式资料足以复查动作和可见结果，不把这些静态截帧当独立来源，也没有宣称已归档连续视频。','']
audit=art/'browser-operation-audit.md';audit.write_text('\n'.join(lines))
for n in ['opinion','trivia','scavenger']:shutil.copy2(work/(n+'.png'),art/(n+'.png'))
# Separate static-paper evidence from operated browser evidence.
e=[]
def add(i,title,u,loc,note,**kw):
 d=dict(evidence_id=f'E-WJCD-{i:02}',product_id='M-048',title=title,url=u,checked_on='2026-10-26',source_type='official_website',access_mode='public_page_text_read',version_scope='The Week Junior US; public content; not native US app',app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
 d.update(kw);e.append(d)
add(1,'官网公开样刊入口','https://theweekjunior.com/in-the-magazine','View Sample Issue','按钮经ema.gs/WiL进入2026-10-23的32页样刊；公开入口会换期，不等同已订阅原生App。')
for i,title,loc,note in [(2,'公开样刊页码与缩略图操作','01–06、11、14；失败记录见审计','实际翻页、展开/关闭缩略图、跳14、回首页和末页；隐藏控件与遮挡点击失败保留。未逐页阅读。'),(3,'公开样刊搜索与空结果','07–09','Nobel产生5个页码结果组；点Page15后本次仍14。无匹配字符串显示0，清空后回侧栏。未推断词内定位或搜索准确率。'),(4,'公开样刊手动书签与重访','10–13','标记14后翻回封面，再访问同一期仍回14；空上下文从1开始。仅一次主序列及一次对照，不证明账号同步或永久保存。')]:
 add(i,title,url,loc,note,source_type='public_web_application',access_mode='automated_public_browser_actions_and_screenshot_review',hands_on=True,visual_observed=True,visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=str(audit.relative_to(lib)),operation_audit_sha256=sha(audit),archived_artifact=str(audit.relative_to(lib)),sha256=sha(audit),capture_state='operation_audit_actions_and_screenshots_archived',artifact_hashes={str(p.relative_to(lib)):sha(p) for p in browser.glob('*')},version_scope='US official public sample; issue 340 dated 2026-10-23; Chromium156.0.8078.4 desktop1200x900; web build unknown; no native app test')
add(5,'儿童活动公开入口','https://kids.theweekjunior.com/activities','Write an opinion piece / Host a book trivia night / Have a scavenger hunt','官网列活动下载；本次只分析三项单页指导，不当已核全活动目录或内置游戏任务。')
acts=json.loads((work/'activity-fetch.json').read_text())
for i,n,date,title,note in [(6,'opinion','2026-10-16','观点写作活动','个人议题→立场与事实支持→考虑对方→行动建议→请成人校读。是作品引导，未见自动评分或掌握度。'),(7,'trivia','2026-06-05','读书问答聚会','参与者各备5题，个人或小队作答，每题1分钟后共同揭晓，志愿者记分；由参与者组织，不是App自动奖励。'),(8,'scavenger','2026-07-31','寻物活动','设边界和时限、组队，物品清单可换更难谜面，勾选/拍照，结束比较结果。是线下组织规则，不证明数字识别、进度或AI主持。')]:
 src=next(x for x in acts if x['name']==n);asset=art/(n+'.png')
 add(i,title,src['url'],'单页PDF；印刷24页；全文读取并实看',note,source_type='official_activity_pdf',access_mode='public_http200_pdf_text_and_page_viewed',source_updated_on=date,visual_observed=True,archived_artifact=str(asset.relative_to(lib)),sha256=sha(asset),capture_state='static_page_excerpt_archived',raw_work_path=str((work/(n+'.pdf')).relative_to(root)),raw_sha256=src['sha256'])
(lib/'batches/week-junior-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in e))
fields=list(csv.DictReader((lib/'coverage.csv').open()).fieldnames)
f=[]
for i,title,depth,ids,known,gaps in [
(5,'公开样刊导航、缩略图与页码跳转','D4','E-WJCD-01;E-WJCD-02','实际1→2、展开/关闭缩略图、14、首页、32；隐藏输入与侧栏遮挡失败留痕。','桌面单次；触摸/缩放、非法页码、断网、前后退与移动端未测。'),
(6,'公开样刊页内搜索与清空','D4','E-WJCD-03','Nobel有5页组；点击Page15后本次仍14；零结果与清空实际观察。','结果未跳转原因未定；逐个命中定位、中文搜索、排名与断网未测。'),
(7,'公开样刊保存位置并重访','D4','E-WJCD-04','手动标记14、浏览回1、同上下文重访14；空上下文从1。','未查存储机制、进程重启、跨设备、自动最后位置、删除/覆盖书签；不代表原生App。'),
(8,'以新闻议题写观点作品','D2','E-WJCD-05;E-WJCD-06','事实支持、反方观点、行动建议和成人校读组成作品路径。','活动面向原产品8–14岁；5–6岁改编、口述、评价标准待研究；未投稿。'),
(9,'围绕读书组织知识问答','D2','E-WJCD-05;E-WJCD-07','每人备题，限时写答、同时揭晓、人工记分和可选换书。','争议答案、难度、年龄差与长期收益未知；不是内置自适应练习。'),
(10,'以线索组织观察和寻物','D2','E-WJCD-05;E-WJCD-08','清单/谜面两种挑战，分组记录发现，时间结束后比较。','活动规则不证明App计时/识别；5–6岁执行难度和学习效果未知。')]:
 f.append(dict(flow_id=f'F-M048-{i:02}',product_id='M-048',task=title,depth=depth,evidence_ids=ids,known_branches=known,gaps=gaps,visual_status='observed_browser_sequence' if depth=='D4' else 'static_reference_only',hands_on='true' if depth=='D4' else 'false',case_path='cases/M-048-week-junior.md',checked_on='2026-10-26'))
with (lib/'batches/week-junior-controls-deepening-coverage.csv').open('w',newline='') as out:
 w=csv.DictWriter(out,fieldnames=fields);w.writeheader();w.writerows(f)
print(json.dumps({'new_evidence':len(e),'new_flows':len(f),'browser_captures':len(states),'formal_artifact_bytes':sum(p.stat().st_size for p in art.rglob('*') if p.is_file())}))
