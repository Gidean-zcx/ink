from pathlib import Path
import csv,io,json,hashlib,shutil,copy
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/kindle-audio-family-deepening';A=L/'artifacts/kindle-audio-family-deepening';B=L/'batches'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text())
def dump(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def wc(p,rows):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
assert not A.exists(),'one-time build'
caps=load(W/'captures.json');assert len(caps)==15 and all(x.get('status')==200 and sha(R/x['raw_work_path'])==x['raw_sha256'] for x in caps)
A.mkdir(parents=True);ims=[]
for c in caps:
 if not c['name'].endswith('.png'):continue
 p=A/c['name'];shutil.copyfile(R/c['raw_work_path'],p)
 ims.append(dict(name=c['name'],url=c['url'],artifact=str(p.relative_to(L)),sha256=sha(p),observed_on='2026-10-27',visual_kind='static_official_image'))
note=(W/'observations.md').read_text()+'\n| 图像 | 来源 | SHA256 |\n| --- | --- | --- |\n'
for x in ims:note+=f"| [{x['name']}]({x['name']}) | [指南原图]({x['url']}) | `{x['sha256']}` |\n"
N=A/'source-notes.md';N.write_text(note)
es=[]
for i,name,title,locator in [(1,'ai.html','笔记AI的选页、失败、预览与插入','entire help: 25 words / 15 and 10 pages / Cancel / Add to Notebook / X'),(2,'child.html','Scribe儿童档案限制与家长PIN','Scribe exclusion note; parental PIN steps'),(3,'assistive.html','辅助朗读的资格、启用和中断','supported languages / Enhanced Typesetting / automatic pause / enable')]:
 c=next(x for x in caps if x['name']==name)
 es.append(dict(evidence_id=f'E-KDAF-{i:02}',product_id='M-001',title=title,url=c['url'],checked_on='2026-10-27',source_type='official_help',access_mode='public_HTML_GET_and_read',version_scope='现行Scribe通用帮助；美国或英国站；未标最低固件',app_build=None,firmware_version=None,source_updated_on=None,locator=locator,note=title+'；区域/型号冲突及未知见独立摘记',hands_on=False,visual_observed=False,archived_artifact=str(N.relative_to(L)),sha256=sha(N),raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],http_status=200,supersedes=None))
for i,title,names in [(4,'首页和阅读顶栏',['home.png','reading.png']),(5,'横竖屏点击区域示意',['portrait.png','landscape.png']),(6,'临时翻页与进度控件',['pageflip.png','progress.png']),(7,'阅读主题保存及管理入口',['themes.png']),(8,'手写工具选中及撤销灰态',['writing.png']),(9,'Audible播放器控件',['audible.png']),(10,'辅助朗读开关和导航播放器',['assistive-controls.png','assistive-setting.png'])]:
 selected=[x for x in ims if x['name'] in names]
 es.append(dict(evidence_id=f'E-KDAF-{i:02}',product_id='M-001',title=title,url=selected[0]['url'],checked_on='2026-10-27',source_type='official_guide_static_images',access_mode='public_static_image_GET_and_viewed',version_scope='Scribe 2024现行在线指南插图，固件未知；不是连续操作',app_build=None,firmware_version=None,source_updated_on=None,locator=', '.join(names),note=title+'；逐图所见及无法判定之处见摘记',hands_on=False,visual_observed=True,visual_kind='static_official_image',archived_artifact=str(N.relative_to(L)),sha256=sha(N),observed_artifacts=selected,supersedes=None,derived_from='E-KDCD-02'))
existing=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines() if x]
assert not {x['evidence_id'] for x in es}&{x['evidence_id'] for x in existing}
assert not {x['url'] for x in es}&{x['url'] for x in existing}
(B/'kindle-audio-family-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
tasks=[
(10,'笔记AI：选页、生成预览、插入或放弃','E-KDCD-02;E-KDAF-01','联网→AI摘要/整理→当前/全本/多选页→Start→加载→预览定制→插入首/末；开始前Cancel、预览X确认放弃；摘要≤25词失败、摘要15页/整理10页限制','地区/语言文档冲突；断网/超时/重试/重复插入/已插入恢复、正确性与最低固件未知','not_observed'),
(11,'Audible：下载、配对、播放及读听切换','E-KDCD-02;E-KDAF-09','书库下载→可播进度→Play配蓝牙；Rescan等排障；已下载区拖动、±30秒、章节选取即播、速度/输出切换；取消下载；匹配电子书回同位置','5%可播不等于整书离线；地区/内容权益；睡眠/断连/AI插话恢复、输出失败和匹配误差未测','static_official_images_only'),
(12,'有声书位置加书签、附注及回听','E-KDCD-02;E-KDAF-09','Add Bookmark保存当前音频位置→Add Note；书签列表选择即播，菜单Edit/Delete','音频书签不等于文字锚点；删除确认/撤销、跨端同步、进度覆盖和离线恢复未知','static_official_images_only'),
(13,'辅助TTS：启用、读听及导航打断','E-KDCD-02;E-KDAF-03;E-KDAF-10','蓝牙/支持书籍→Aa More开启→底部播放/速度/退30秒；导航或批注自动暂停；开关持续开启直到关闭','语言/Enhanced Typesetting/来源有限；关菜单自动续播、中文、USB文档、睡眠/重启续读未证','static_official_images_only'),
(14,'VoiceView：配对、读屏教程与切换','E-KDCD-02','首次开机等待/蓝牙配对/三按电源→最终声音反馈和教程；无声查配对；后续Accessibility切换、语速/音量/教程Exit','读屏不是语音命令；具体手势/焦点回退/盲操作失败/读书断点未知；文档6秒未实测','not_observed'),
(15,'设置家长限制并区分儿童档案','E-KDCD-02;E-KDAF-02','下拉→Settings→Your/My account→Parental control→设置并保存PIN；限制浏览器/商店/云内容/Goodreads','指南家庭建档与英国专页Scribe排除冲突；不建立Scribe儿童建档成功链；限制逐项确认/解除失败未知','not_observed'),
(16,'浏览网页正文、书签与文件入口','E-KDCD-02','More→Browser→网址/链接，Back/History；Article Mode正文列；Bookmark this Page，列表勾选Remove；下载需确认；清历史/Cookies独立','网络失败、登录适配、重复文件、下载中断、删除撤销及跨会话位置未知；格式仅指南列项','not_observed'),
(17,'阅读排版、主题保存与进度显示','E-KDCD-02;E-KDAF-06;E-KDAF-07','Aa字体/字号/粗细/布局；横屏才双栏；主题保存/管理；More选择位置/页/预计时间/隐藏；PDF专属对比和裁边','本机各书生效与系统UI字号分开；主题删除确认/撤销及跨设备未证；预计时间不等于理解进度','static_official_images_only'),
(18,'下拉快捷设置、全局显示与联网','E-KDCD-02;E-KDAF-04','下拉Quick Actions→飞行/亮度/冷暖/Sync/Settings；飞行关闭Wi-Fi和蓝牙；冷暖自动/手动日程；网络重扫/选网/密码/连接','只看到顶栏，未看快捷面板；系统全局与阅读排版分开；网络密码云保存删除不等于忘记本地网络；休眠/错误回退未知','static_official_images_only'),
(19,'按内容类型放大阅读与返回','E-KDCD-02;E-KDAF-05','部分儿童书Text Pop-Up滑动换段、双击正常；漫画Panel View/部分先启用；图像放大、表格平移缩放与X返回','支持书籍各异；漫画退出/跨页状态及误触恢复未全列；点击区示意不证明这些模式实操或60fps','static_official_images_only'),
(20,'查看人物线索与前情回顾','E-KDCD-02','长按人物/地点→X-Ray，时间轴超过最远阅读默认隐藏，可显示全部；库长按/书内菜单Recaps，系列前情与Story So Far分开','地区/英文/书籍资格；无条目不等于故障；剧透边界、回顾更新、返回位置、来源和AI生成方式未验证','not_observed')]
new=[]
for num,t,e,b,g,v in tasks:new.append(dict(flow_id=f'F-M001-{num:02}',product_id='M-001',task=t,depth='D2',evidence_ids=e,known_branches=b,gaps=g,visual_status=v,hands_on='false',case_path='cases/M-001-kindle.md',checked_on='2026-10-27'))
wc(B/'kindle-audio-family-deepening-coverage.csv',new)
old=load(W/'coverage-before.json');updates=[]
changes={
1:dict(known_branches='查词自动入本与关闭；词义卡/例句/Flashcards；Word Wise提示密度/多义词反馈及中英文提示独立',gaps='误标掌握/撤销与复习调度未知；中文提示不证明中文TTS或教学适龄'),
2:dict(evidence_ids='E-KDL-01;E-KDCD-02;E-KDAF-06',visual_status='static_official_images_only',known_branches='预览/点页跳转/Back原位；Go To历史退出书后清；脚注预览与跳到注释分开',gaps='静态三页与返回按钮；退出/断电/跨端历史恢复未知，不当连续视觉链'),
3:dict(evidence_ids='E-KDL-02;E-KDL-03;E-KDCD-02;E-KDCD-08;E-KDCD-09;E-KDAF-08',visual_status='static_official_images_only',known_branches='画布确认/取消及整块删除；便签X保存；页边栏宽/停靠/覆盖，关后便签显示；笔迹Undo/Redo与剪贴板分别',gaps='工具条静态非笔迹执行；整体删除/内容更新锚点与跨端恢复未知；页边栏关闭不等于删除'),
4:dict(evidence_ids='E-KDL-04;E-KDCD-02;E-KDCD-05;E-KDCD-06;E-KDCD-07',known_branches='100MB与关闭保存；概览单点选择/再点打开，Back回原页，多选X回概览；模板选页/全本；2025+复制/跨本移页另列',gaps='空间不足实际提示、删除页确认/恢复、模板更换数据保护未知；不把2025+套2024'),
6:dict(evidence_ids='E-KDL-06;E-KDL-07;E-KDCD-02;E-KDCD-03;E-KDCD-04;E-KDCD-11',known_branches='联网Sync云备份；移除下载/永久删除对象不同；Alexa附件单独副本；重启保内容，出厂重置清本地与设置后重注册',gaps='未同步笔迹、USB文件备份、跨端冲突及整机恢复未知；云下载不等于恢复本地全部状态'),
9:dict(known_branches='合集结构/下载分开，删合集不删书；音频筛选不删除；配套书音频合一条目，文档泛称删书连音频；Quick Archive/Manual Removal',gaps='合并音频删除层级/确认/撤销未知；USB无云副本；合集冲突、下载失败与Downloaded措辞冲突保留')}
for x in old:
 n=int(x['flow_id'][-2:])
 if n not in changes:continue
 y=copy.deepcopy(x);y.update(changes[n]);updates.append(dict(flow_id=x['flow_id'],before=x,after=y))
dump(W/'existing-coverage-updates.json',updates)
e=next(x for x in load(W/'evidence-before.json') if x['evidence_id']=='E-KDCD-02');c=copy.deepcopy(caps[0]);c.update(archived_artifact=str(N.relative_to(L)),sha256=sha(N),note='同SHA指南复读扩展；11图另立派生E，不把GET当看图')
dump(W/'existing-evidence-additional-captures.json',[dict(evidence_id=e['evidence_id'],before=e.get('additional_captures',[]),after=e.get('additional_captures',[])+[c])])
# Preserve source TOC identity, updating only inspected entries.
ip=B/'kindle-controls-deepening-inventory.csv';before=ip.read_bytes();(W/'inventory-before.csv').write_bytes(before);ir=list(csv.DictReader(io.StringIO(before.decode())))
mapping={3:18,5:18,6:3,7:14,10:18,11:18,12:18,24:19,25:2,26:17,27:17,28:3,29:6,30:19,32:3,33:4,34:3,36:11,37:11,38:11,39:12,40:11,41:9,43:20,44:20,48:16,49:15,51:17,52:13,54:18,55:6,57:10,58:3}
for x in ir:
 n=int(x['order'])
 if n in mapping:
  f=f'F-M001-{mapping[n]:02}'; x['flow_ids']=';'.join(dict.fromkeys(filter(None,[*x['flow_ids'].split(';'),f])));x['coverage']='正文及部分任务已核；仍缺实际操作';x['missing_controls_or_branches']='具体已知/缺失见关联F；父项不是独立功能'
 if n==49:x['missing_controls_or_branches']='儿童档案有Scribe型号排除冲突；成人家庭及付款/退出条件仅指南，未当现行服务核定'
 if n==30:x['missing_controls_or_branches']='部分特殊内容可重建；报刊服务当前可用性未核'
 if n==57:x['evidence_ids']+=';E-KDAF-01';x['missing_controls_or_branches']='地区/语言冲突，网络失败和已插入后恢复未知'
 if n==58:x['missing_controls_or_branches']='显示模式已补；宽度/跨端/内容更新后的锚点恢复未测'
 if n==50:x['coverage']='正文控件已读，未建完整任务';x['missing_controls_or_branches']='公开书架/评级/移除入口；未登录或验证跨端/删除恢复'
wc(ip,ir);dump(W/'inventory-update.json',dict(path=str(ip.relative_to(R)),before_sha256=hashlib.sha256(before).hexdigest(),after_sha256=sha(ip),rows=len(ir)))
rows=[]
groups={10:['单页/全本/指定页','25词摘要门槛','摘要15页/整理10页','开始前Cancel','等待同步与预览','字体/行距/摘要语气','插入首/末页','预览X确认放弃','地区/语言冲突'],11:['蓝牙配对/Rescan','已下载区域拖动','5%与当前阅读位置','取消下载两入口','选章节即播','配套电子书切换'],12:['音频位置加书签','Add Note','选书签即播','编辑/删除'],13:['Enhanced Typesetting和语言','Aa More开关','退30秒/速度/播放','导航或批注暂停','开关持续≠自动续播'],14:['首次无声到最后步骤','教程与Exit','快捷入口变化','语速/音量'],15:['家长PIN与限制对象','Scribe儿童建档排除','成人共享与儿童档案区别'],16:['Article Mode','书签勾选Remove','历史/Cookie分开','下载格式/确认'],17:['主题保存和管理','本机各书与UI字号分开','横屏双栏','PDF裁边/对比','预计时间/隐藏进度'],18:['下拉与飞行','冷暖自动/手动日程','系统深色与旋转锁','选网/密码/重扫','云保存网络密码删除'],19:['儿童文字弹出/双击退出','漫画Panel View','图片与表格缩放退出'],20:['X-Ray未来片段隐藏','系列前情/Story So Far','资格和缺入口']}
for n,items in groups.items():
 f=next(x for x in new if x['flow_id']==f'F-M001-{n:02}')
 for title in items:rows.append(dict(item_id=f'KDAF-{len(rows)+1:03}',product_id='M-001',item=title,source_locator=f['task'],evidence_ids=f['evidence_ids'],flow_ids=f['flow_id'],status='documented_partial_controls',gap=f['gaps'],checked_on='2026-10-27'))
wc(B/'kindle-audio-family-deepening-inventory.csv',rows)
case=(W/'case-before.md').read_text()
case=case.replace('本案例 9F，均为限定范围的 D2 文档流程：8 个旧 F 续补，1 个新书库任务。','此前9F；本轮续补6个既有任务，新增F10–20，合计20F，均为限定范围的D2文档流程。')
case=case.replace('本轮没有查看界面图片，HTML 只按文字核查。原件与 SHA-256 记在新 E 的 `raw_work_path/raw_sha256`；`archived_artifact` 留空。','此前仅文字；本轮实际查看11幅官方静态插图，7组派生E，与3篇新帮助分别登记；原件哈希和独立摘记见文末。静态插图不是连续流程。')
case=case.replace('| 笔记AI与页边栏 | 2024 现行指南有子项；前者未立 F，后者仅关联 F03 | AI 资格/失败/撤回及页边栏完整切换未拆；不倒推到出厂 |','| 笔记AI与页边栏 | F10及F03续补 | AI地区/语言冲突，断网/恢复及实际布局未知；不倒推到出厂 |')
case=case.replace('| 音频、蓝牙、无障碍、家庭、浏览器、系统设置 | 目录逐项登记，正文可继续 | 未展开完整任务；不能因有标题就声称已经研究 |','| 音频、蓝牙、无障碍、家庭、浏览器、系统设置 | F11–18及F06续补，正文与选图 | 核心文档链已补；播放打断/睡眠恢复、儿童档案冲突及实际动作仍缺 |')
case=case.replace('此处尚未拆完整家长退出或离线权限','本轮补家长PIN但有Scribe儿童档案排除冲突；完整家长退出及离线权限仍缺')
case=case.replace('目录中的音频/家庭/无障碍/系统与 AI 工具仍有正文可继续研究；本轮没有为了数量新增其流程。','音频/家庭/无障碍/系统与AI工具本轮已补正文任务；实际打断恢复、地区与型号冲突仍须验证。')
case=case.replace('本轮以下均为 2026-10-27 成功读取的官方文字','此前以下均为2026-10-27成功读取的官方文字')
case+='\n\n## 本轮续补：声音、控制范围与恢复\n\n[独立来源与逐图观察](../artifacts/kindle-audio-family-deepening/source-notes.md)保存具体定位、冲突和11幅原图；[控件目录](../batches/kindle-audio-family-deepening-inventory.csv)逐项反查任务。旧58项目录稳定编号回写，未读项保持未读。没有实机或音频测试。\n\n| 新任务 | 已知动作与状态 | 未闭合部分 | 证据 |\n| --- | --- | --- | --- |\n'
for f in new:case+=f"| {f['flow_id']} {f['task']} | {f['known_branches']} | {f['gaps']} | {f['evidence_ids'].replace(';','、')} |\n"
case+='\n既有任务的修订如下，保留原F编号：\n\n| 任务 | 补充/修订 | 仍缺 |\n| --- | --- | --- |\n'
for u in updates:
 f=u['after'];case+=f"| {f['flow_id']} | {f['known_branches']} | {f['gaps']} |\n"
case+='''
### 不能消除的来源差异

指南与英国帮助对Scribe儿童档案相冲突；F15只重建家长PIN，不推演儿童独立档案已经可用。AI地区/法语变体名单也不一致。文字中的Page Refresh设置路径前后不同。上述均保留来源范围，不能用插图存在就解决版本差异。

本项目启发仍是建议：AI需要识别当前对象、动作结果与恢复锚点。便签的X会保存，AI生成预览的X会放弃；音频书签点击会播放，翻页预览只是临时位置；同步云书、移除下载和清除本地设置不能被一个“已备份”概括。儿童是否能独立完成这些任务、中文听读质量及Plus可实现性另行验证。

新增文字来源：[AI笔记帮助](https://digprjsurvey.amazon.com/csad/help/node/TZNl60K13ehpn0s1O0)（E-KDAF-01）、[儿童档案与Scribe排除](https://digprjsurvey.amazon.co.uk/csad/help/node/G27XUUGLQ5TG84XC)（E-KDAF-02）、[辅助朗读帮助](https://digprjsurvey.amazon.co.uk/csad/help/node/TdHCSXXqsdhaVKt3Tc)（E-KDAF-03）。E-KDAF-04–10为指南插图派生证据，具体URL、哈希与观察均在独立摘记；同指南E-KDCD-02追加本次快照，不新增重复来源。
'''
(L/'cases/M-001-kindle.md').write_text(case)
dump(W/'build-result.json',dict(new_evidence=len(es),new_flows=len(new),old_tasks=len(updates),new_images=len(ims),inventory_rows=len(rows),existing_inventory_rows=len(ir),additional_captures=1))
print((W/'build-result.json').read_text())
