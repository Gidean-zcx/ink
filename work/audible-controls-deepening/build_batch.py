from pathlib import Path
from collections import Counter
from urllib.parse import urlparse
import csv,json,re,hashlib
R=Path('/home/workspace/reader');W=R/'work/audible-controls-deepening';L=R/'docs/research/interaction-library';A=L/'artifacts/audible-controls-deepening';A.mkdir(exist_ok=True);(W/'extracted').mkdir(exist_ok=True)
C=L/'cases/M-039-audible.md';oldcase=C.read_text();(W/'case-before.md').write_text(oldcase)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
# Each snapshot is the literal result of web.run, not an HTTP HTML body.
S={p.name:json.loads(p.read_text()) for p in W.glob('web-*-snapshot.json')}
def block(file,url):
 for b in S[file].split('--------------------------------------------------------------------------------'):
  if b.splitlines() and any(line.endswith('('+url+')') for line in b.splitlines()[:3]):return b.strip()+'\n'
 raise ValueError((file,url))
base='https://help.audible.com/s/article/'
specs=[
('官方 Listening / Player 可见帮助目录','https://help.audible.com/s/listening?language=en_US','web-index-snapshot.json',None,'Ways to listen / Library / Player / Reviews / Profiles','目录首屏35个不同条目；Ways to listen有Load more，不宣称全产品目录。','official_help_index'),
('App 播放器设置入口与功能清单',base+'listen-in-the-app','web-index-snapshot.json','2026-10-13','Player settings; nine listed settings; resume/sync','只枚举可调整项；未取得具体可选秒数或全套手势。','official_help'),
('书库筛选、移除及永久删除分层',base+'manage-your-library','web-index-snapshot.json','2026-09-09','Library sections; In your library; Remove download / Library / permanent delete','移除下载、本地显示、账号永久删除各自作用；不把移除等同退款。','official_help'),
('音频下载与设备离线边界',base+'download-titles','web-core-snapshot.json','2026-09-03','App download; computer entitlement; Apple Watch / Fire scope','手机单本完成才离线；设备段落分开，不把Watch边播边下套手机。','official_help'),
('片段和书签的新增、返回与删除',base+'manage-clips-bookmarks','web-core-snapshot.json','2026-09-09','Save/Edit/Delete clip; app podcast vs Cloud bookmarks','片段5–44秒、附带书签；编辑时长段未单列Save，笔记段有Save，保持文档差别。','official_help'),
('缺书与Hidden恢复路径',base+'title-is-missing?language=en_US','web-core-snapshot.json','2026-10-01','Check your archive; Refresh; search/filter','现行Check your archive指Library > Lists > Hidden > Remove from list；加入Hidden入口未取得。','official_help'),
('Read & Listen（原Whispersync for Voice）',base+'listen-with-whispersync-for-voice?language=en_US','web-core-snapshot.json','2026-10-01','eligibility; switch; manual sync; companion return','同名不代表配对；需支持版本和双格式；Sync device position与一般帮助命名有别。','official_help'),
('退还标题：资格、网页入口和结果',base+'return-a-title','web-state-snapshot.json','2026-09-03','Eligibility; mobile/desktop; linked Kindle return','只复述US公开退还操作边界，未下单/退款；自助资格不是保证。','official_help'),
('播放故障、重下载与Android后台限制',base+'title-won-t-play-has-poor-audio-quality','web-state-snapshot.json','2026-09-09','App; Samsung/Pixel battery','重下保原位置是官方描述；不扩展为任意清数据或来电后自动恢复。','official_help'),
('沉浸读听与文字联网条件',base+'learn-about-immersion-reading','web-state-snapshot.json','2026-10-12','requirements; start; customization','Audible中文字从网络加载，仅部分页缓存；专业旁白而非TTS。','official_help'),
('Page Sync：纸书扫描到音频',base+'use-page-sync','web-state-snapshot.json','2026-09-03','use; eligible titles; troubleshooting','联网、相机权限；只纸书到音频单向，排除Kids Profile与绘本；未实测识别。','official_help'),
('睡眠计时、覆盖与摇动延期',base+'use-timer','web-player-snapshot.json','2026-09-03','set/extend/shake/Live Activity','重新确认既有流程；摇动窗口从声音渐弱起算，非任意时候摇动。','official_help'),
('单标题速度与设备默认速度',base+'set-narration-speed?language=en_US','web-player-snapshot.json','2026-09-03','particular title / default / Cloud','官方称单标题速度跨apps保存，默认可逐设备不同；未推定所有输出端。','official_help'),
('Up next与推荐预览',base+'manage-your-auto-play-settings?language=en_US','web-player-snapshot.json','2026-09-03','continuous listening / add/edit queue','修正既有路径补Settings；连续播放不保证离线失败自动跳过。','official_help'),
('章节/播客跳转与分章差异',base+'browse-chapters-and-episodes','web-player-snapshot.json','2026-09-03','app/Cloud; why chapter mismatch','章节重组不等于错误；完整跨文本映射另见配对与Page Sync。','official_help'),
('书内Listen Log与标题Listening History',base+'view-listening-log?language=en_US','web-player-snapshot.json','2026-09-03','find log; remove title from history','桌面Listening History单项Remove不同于清书内日志；均无导出。','official_help'),
('Car Mode的控制与退出',base+'listen-in-the-car?language=en_US','web-boundaries-snapshot.json','2025-10-24','Car Mode L107–134','历史帮助列4控件、X退出与蓝牙自动进入；没有观看现行App，不称完整现代手势。','official_help'),
('重装/清缓存与本地状态损失',base+'app-keeps-crashing?language=en_US','web-controls-snapshot.json','2026-08-27','Clear data/cache; reinstall note','官方重装注记列下载与Finished标记需重建；其它笔记/片段不能据此推定保留或丢失。','official_help'),
('Cloud Player需联网及浏览器入口',base+'listen-on-a-browser?language=en_US','web-final-snapshot.json','2026-08-27','requirements; desktop/mobile','网页播放器流播需联网，手机App下载不使浏览器离线。','official_help'),
('切换marketplace不是迁移账号',base+'update-your-country-marketplace?language=en_US','web-final-snapshot.json','2026-09-03','app switch; purchased titles remain; Canada note','本批US限定；换店面不自动搬已购库，不执行迁移。','official_help'),
('Standard月选、离线下载与失效状态',base+'learn-about-standard-plan?language=en_US','web-final-snapshot.json','2026-10-06','monthly titles / cancellation / return','可离线下载仍受会员访问资格；到期锁图标、不可播放；月选不能退换。','official_help'),
('Plus包含目录与访问权限',base+'learn-about-audible-plus?language=en_US','web-boundaries-snapshot.json','2026-09-03','benefits / cancellation','只取已读取段落：包含目录可下载，取消会失去访问；不承诺离线授权周期。','official_help'),
('Lists管理的官方搜索正文',base+'manage-collections?language=en_US','web-controls-snapshot.json',None,'search result Manage your Lists; add/remove/delete','直接打开仅CSS Error；官方搜索正文给清单管理。没有隐藏入口证据；不把普通list等同Hidden。','official_help')]
E=[]
for i,(title,url,file,date,loc,note,typ) in enumerate(specs,1):
 b=block(file,url);ep=W/'extracted'/f'{i:02}.txt';ep.write_text(b)
 e=dict(evidence_id=f'E-AUCD-{i:02}',product_id='M-039',title=title,url=url,checked_on='2026-10-27',source_type=typ,access_mode='official_search_index_body' if i==23 else 'official_web_tool_extracted_text',version_scope='Audible US English help; Android/iOS mobile primary; Cloud/Car/Kindle only explicitly scoped; app build unknown',app_build=None,firmware_version=None,source_updated_on=date,locator=loc,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='web_tool_text_snapshot_not_raw_html',supersedes=None,raw_work_path=(W/file).relative_to(R).as_posix(),raw_sha256=sha(W/file),extracted_work_path=ep.relative_to(R).as_posix(),extracted_sha256=sha(ep))
 E.append(e)
(L/'batches/audible-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in E))
# A bounded obtainable index, not a claim of all hidden or paginated functions.
index=block('web-index-snapshot.json','https://help.audible.com/s/listening?language=en_US')
items=[];domain='';seen=set()
for line in index.splitlines():
 m=re.match(r'L\d+: ## (.+)',line)
 if m:domain=m.group(1)
 m=re.match(r'L\d+:\s+\* 【\d+†(.+?)】',line)
 if m and domain in ('Ways to listen','Library','Player','Reviews & social sharing','Profiles'):
  title=m.group(1)
  if title not in seen:items.append((domain,title));seen.add(title)
assert len(items)==35,len(items)
known={
'Listen in the app':(2,'F-M039-01;F-M039-03;F-M039-09'), 'Learn about immersion reading':(10,'F-M039-13'),'Learn about Read & Listen':(7,'F-M039-09;F-M039-13'),'Manage your Library':(3,'F-M039-01;F-M039-10'),'Manage your Lists':(23,'F-M039-10'),'Use Page Sync':(11,'F-M039-14'),'Download titles':(4,'F-M039-02'),'Manage clips & notes':(5,'F-M039-04;F-M039-05'),'View listening log':(16,'F-M039-08'),'Use timer':(12,'F-M039-07'),'Set narration speed':(13,'F-M039-03'),'Manage your auto-play settings':(14,'F-M039-06'),'Browse chapters and episodes':(15,'F-M039-03')}
I=[]
def add(domain,title,eid=None,flows='',source='index'):
 e=E[eid-1] if eid else None
 I.append(dict(inventory_id=f'I-M039-{len(I)+1:03}',domain=domain,item=title,source_url=E[0]['url'] if source=='index' else e['url'],locator=domain+' / '+title,article_url=e['url'] if e else '',coverage_status=('official_search_body_partial' if eid==23 else 'body_read_partial_task') if e else 'index_only',evidence_ids='E-AUCD-01'+(';' +e['evidence_id'] if e else '') if source=='index' else e['evidence_id'],flow_ids=flows,gap='目录不等于任务完成；具体未核分支见case' if flows else '仅功能发现；未读正文或未闭合任务'))
for d,t in items:
 num,flows=known.get(t,(None,''));add(d,t,num,flows)
for num,flows in [(6,'F-M039-11'),(8,'F-M039-12'),(9,'F-M039-11'),(17,'F-M039-03'),(18,'F-M039-11'),(19,'F-M039-02;F-M039-05'),(20,'F-M039-11'),(21,'F-M039-02;F-M039-12'),(22,'F-M039-02')]:add('补充帮助',E[num-1]['title'],num,flows,'body')
for t,ff in [('Player customization','F-M039-03'),('Progress bar per chapter / whole title','F-M039-03'),('Skip forward / back seconds','F-M039-03'),('Mini player time display','F-M039-03'),('Shake to extend','F-M039-07'),('Auto-lock',''),('Automatic Car Mode','F-M039-03'),('Sync listening position','F-M039-09'),('Continuous listening','F-M039-06')]:
 add('Player settings列举项',t,2,ff,'body');I[-1]['coverage_status']='setting_listed_not_full_options';I[-1]['gap']='可发现设置；全部选项/默认值/保存反馈未核；有任务补证者见case'
with (L/'batches/audible-controls-deepening-inventory.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=I[0],lineterminator='\n');w.writeheader();w.writerows(I)
old=[r for r in csv.DictReader((L/'batches/audio-coverage.csv').open()) if r['product_id']=='M-039'];assert len(old)==11
(W/'old-coverage-before.json').write_text(json.dumps(old,ensure_ascii=False,indent=2)+'\n')
changes=[
('E-AUCD-02;E-AUCD-03','顶部类型横滑；搜索可用In your library找已购但退出目录的书；mini player/锁屏恢复。','空库、筛选保持、两端同时播放冲突未验证。'),
('E-AUCD-04;E-AUCD-19;E-AUCD-21;E-AUCD-22','手机单本下载完成后离线；Cloud仍需联网；Standard月选和包含目录受资格限制，下载不等于永久访问。','离线授权检查周期、暂停取消下载、断点续传、下载不全可播放范围未知。'),
('E-AUCD-02;E-AUCD-13;E-AUCD-15;E-AUCD-17','Player三点或Profile设置可进入控制选项；章节/速度边界保持；Car Mode有X退出，历史帮助单列。','跳转撤销、具体跳秒范围、定制保存、通用封面手势与路由恢复未知。'),
('E-AUCD-05','保留5–44秒/附带书签；时长编辑段未单列保存，笔记段有Save；二次删除及Clip/Start in Player分开。','默认30秒范围、离线同步、导出、关联书签删除和取消编辑未知。'),
('E-AUCD-05;E-AUCD-19','播客Bookmarks & Notes与Cloud主菜单Bookmarks分别进入；笔记保存、单项Delete需确认；网页不支持片段管理。','书签/片段互删、离线编辑与跨端冲突未知；不把网页书签当离线音频。'),
('E-AUCD-14','补齐Profile > Settings > Player；Up next排队与Recommended预览分别开关，无下一本才播预览。','队列持久化/同步、重复加入、未下载下一本失败行为未知。'),
('E-AUCD-12','计时可被新时长覆盖；渐弱起30秒内摇动延期；iOS17+ Live Activity可在权限下看倒计时。','暂停/换书/强退/重启计时是否继续及闹钟来电优先级未知。'),
('E-AUCD-16','书内Listen Log与标题Listening History分开；网页姓名 > Listen History > Remove仅移除历史条目；无导出。','历史清除确认/撤销、保留期、跨设备汇总与清除后同步未知。'),
('E-AUCD-07','普通进度同步与Read & Listen双格式配对分开；手动重新同步需联网；两文档分别称Sync listening position/Sync device position。','离线并发进度冲突、标记/笔记合并、回退确认与全设置同步未知。'),
('E-AUCD-03;E-AUCD-06;E-AUCD-08;E-AUCD-23','移除下载、移出书库、Hidden移回、永久删除、退还分层；Hidden > Remove from list恢复可见性；普通清单删除不可恢复但不据此推断删书。','加入Hidden入口、移除后的笔记/片段、永久删除补救与本机离线撤权时点未知。'),
('E-AUCD-06;E-AUCD-09;E-AUCD-18;E-AUCD-20','不同步/缺书先刷新与核市场；重下保进度；Android电池限制排查；重装帮助明确下载及Finished标记需重建。','来电实际恢复、清缓存/清存储分开影响、未同步笔记/片段、具体系统/构建未知。')]
U=[]
for r,(ids,branches,gaps) in zip(old,changes):
 n=dict(r,evidence_ids=r['evidence_ids']+';'+ids,known_branches=branches,gaps=gaps,checked_on='2026-10-27')
 U.append(dict(flow_id=r['flow_id'],changes={k:dict(before=r[k],after=n[k]) for k in r if r[k]!=n[k]},replacement_row=n))
(W/'old-coverage-updates.json').write_text(json.dumps(U,ensure_ascii=False,indent=2)+'\n')
new=[]
for num,title,ids,branch,gap in [(12,'按购买来源确认资格并退还标题','E-AUCD-08;E-AUCD-21;E-AUCD-07','US有效Premium信用点购买365天内等条件；网页Purchase history确认后退1credit；Standard月选不退换。','自助资格和退款不是承诺；未操作；离线文件/笔记撤回时点未知。'),(13,'在支持双格式的标题中开始沉浸读听','E-AUCD-07;E-AUCD-10','匹配有声书+Kindle电子书；Listen/Read & Listen切换；专业旁白高亮；文字需联网仅部分页缓存。','具体App构建、缓存范围/驱逐、离线提示、注释/选字与样式保存未知。'),(14,'扫描纸书页跳到有声书位置','E-AUCD-11','More > Page Sync；首次相机权限；在线识别后跳转；单向且不支持Kids Profile/绘本/播客。','误匹配撤销、识别置信度、上传保存规则和耗时未验证。')]:
 new.append(dict(flow_id=f'F-M039-{num:02}',product_id='M-039',task=title,depth='D2',evidence_ids=ids,known_branches=branch,gaps=gap,visual_status='not_observed',hands_on='false',case_path='cases/M-039-audible.md',checked_on='2026-10-27'))
with (L/'batches/audible-controls-deepening-coverage.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=old[0],lineterminator='\n');w.writeheader();w.writerows(new)
manifest=[]
for p in sorted([*W.glob('web-*-snapshot.json'),*W.glob('*.html'),* (W/'extracted').glob('*.txt')]):manifest.append(dict(path=p.relative_to(R).as_posix(),sha256=sha(p),bytes=p.stat().st_size,kind='http_frontend_shell_no_article_body' if p.suffix=='.html' else 'web_tool_snapshot' if p.suffix=='.json' else 'derived_per_source_web_text'))
(W/'source-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(E=len(E),newF=len(new),old_updates=len(U),inventory=len(I),manifest=len(manifest)),ensure_ascii=False))
