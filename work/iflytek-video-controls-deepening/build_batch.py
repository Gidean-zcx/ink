from pathlib import Path
import csv,io,json,hashlib,shutil,copy
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/iflytek-video-controls-deepening';A=L/'artifacts/iflytek-video-controls-deepening';B=L/'batches'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text())
def dump(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def wc(p,rs):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
assert not A.exists(),'one-time build'
assert (W/'network-events.json').exists(),'browser must finish first'
A.mkdir(parents=True)
selected=[
 ('小学数学AI同步精准学','05-math-player', [('math-sync-hd',20),('math-sync-hd',40),('math-sync-hd',70),('math-sync-hd',80),('math-sync-hd',90),('math-sync-extra',94),('math-sync-hd',110),('math-sync-extra',114)]),
 ('小数AI组卷阶段测','06-math-assessment-open',[('assessment-extra',35),('assessment-extra',73),('assessment-extra',85),('math-assessment',80),('math-assessment',90),('math-assessment',100)]),
 ('小学英语分级阅读','07-english-reading-open',[('english-reading',30),('english-extra',35),('english-extra',70),('english-extra',80),('english-reading',105),('english-extra',127),('english-extra',140),('english-reading',150),('english-extra',155)]),
 ('听一听','08-listening-open',[('listening',40),('listening',60),('listening-extra',74),('listening',80)])]
media=[];all_observed=[]
for i,(title,cap,frames) in enumerate(selected,1):
 base=load(W/'browser'/f'{cap}.json');m=base['videos'][0];obs=[]
 for pre,t in frames:
  src=W/'frames'/pre/f'{t:06.2f}.png';meta=load(src.with_suffix('.json'));assert sha(src)==meta['sha256']
  dst=A/f'video-{i:02}-{t:03}s.png';shutil.copyfile(src,dst)
  # PNG IHDR gives actual dimensions; requested CDP scale is not output scale.
  raw=dst.read_bytes();dims=[int.from_bytes(raw[16:20],'big'),int.from_bytes(raw[20:24],'big')];assert dims==[1280,720]
  x=dict(path=str(dst.relative_to(L)),sha256=sha(dst),position_seconds=t,source_url=m['src'],actual_pixels=dims,observed_on='2026-10-27',method='browser_seek_paused_element_screenshot',product_hands_on=False,audio_observed=False)
  obs.append(x);all_observed.append(x)
 media.append(dict(evidence_id=f'E-IFVC-{i:02}',title=title,url=m['src'],help_player_url=base['url'],duration_seconds=m['duration'],native_dimensions=[m['width'],m['height']],selected_frames=obs))
for name in ['04-t30pro-directory.json','04-t30pro-directory.png']:
 shutil.copyfile(W/'browser'/name,A/name)
dump(A/'video-samples.json',media)
shutil.copyfile(W/'network-events.json',A/'network-events.json')
note=(W/'observations.md').read_text()+'\n## 必要摘帧与定位\n\n以下全部由root逐图查看。完整采样元数据见[视频采样记录](video-samples.json)；未选入正式库的试采图留work。\n\n| 证据 | 原视频 | 正式摘帧 |\n| --- | --- | --- |\n'
for m in media:
 note+=f"| {m['evidence_id']} | [{m['title']}]({m['url']}) | "+'、'.join(f"[{x['position_seconds']}s]({Path(x['path']).name})" for x in m['selected_frames'])+' |\n'
N=A/'source-notes.md';N.write_text(note)
es=[]
for m in media:
 es.append(dict(evidence_id=m['evidence_id'],product_id='M-027',title=m['title']+'：官方剪辑教程抽样画面',url=m['url'],checked_on='2026-10-27',source_type='official_help_video_sampled_frames',access_mode='anonymous_browser_media_stream_seek_and_view',version_scope='T30Pro帮助目录所列视频；固件、App构建、发布日期未知，可能跨型号共用',app_build=None,firmware_version=None,source_updated_on=None,locator='sample positions: '+','.join(str(x['position_seconds']) for x in m['selected_frames'])+' seconds',note='逐图查看，不是连续完整观看或设备操作；未听音，保留剪辑断点与未验证状态',hands_on=False,visual_observed=True,visual_kind='sampled_video_frames',archived_artifact=str(N.relative_to(L)),sha256=sha(N),artifact_hashes={x['path']:x['sha256'] for x in m['selected_frames']},observed_artifacts=m['selected_frames'],help_player_url=m['help_player_url'],duration_seconds=m['duration_seconds'],http_status=206,supersedes=None,derived_from='E-ILD-17'))
existing=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()]
assert not {x['evidence_id'] for x in es}&{x['evidence_id'] for x in existing}
assert not {x['url'] for x in es}&{x['url'] for x in existing}
(B/'iflytek-video-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
tasks=[
 (16,'按需求找题、挑题及选择阶段测模式','E-IFVC-02','分类/筛选与AI找题分开；候选勾选/取消→去作答；考试倒计时、练习未作答不入错题；答后图谱可进弱项','剪辑跳过完整交卷；找题来源/生成方式、空结果/误匹配、超时/漏答确认/排名/重做/恢复未知'),
 (17,'英语分级选读、页内求助及跟读','E-IFVC-03','定级说明/报告→分级书目；精读读练玩、泛读词义卡/收下；句级录音/回放、跟读句数、末页完成入口和报告分别','定级测量/评分准确度未知；错答/提示/重录/保存/完成点击后计数、离线与读听恢复未观察；样例不是固定周目标'),
 (18,'选音频播放并配置早晚听','E-IFVC-04','学习工具→听一听→分类/专辑/条目；播放器/历史/收藏分别；早晚听选时间/场景/重复/时长/章节并有提交；浮条暂停/结束','未听音或操作设备；定时提交回执/取消/休眠唤醒/离线/AI打断与秒级断点未知；历史不证明续播')]
fs=[dict(flow_id=f'F-M027-{n:02}',product_id='M-027',task=t,depth='D2',evidence_ids=e,known_branches=b,gaps=g,visual_status='static_reference_only',hands_on='false',case_path='cases/M-027-iflytek-learning.md',checked_on='2026-10-27') for n,t,e,b,g in tasks]
wc(B/'iflytek-video-controls-deepening-coverage.csv',fs)
updates=[]
for f in load(W/'coverage-before.json'):
 n=int(f['flow_id'][-2:]);y=copy.deepcopy(f)
 if n==1:
  y.update(depth='D2',evidence_ids=f['evidence_ids']+';E-IFVC-01',known_branches='局部教程重建：选知识点/方案→讲解/典例/练习；手写、思路点拨入口、批改正确反馈/解析/下一题；全绿完成与下一节',gaps='视频剪辑换题，无同题错答→提示→重试链；首次诊断、误识别校订、阈值、完成持久化/离线/重启恢复未知',visual_status='static_reference_only',checked_on='2026-10-27')
 elif n==14:
  y.update(evidence_ids=f['evidence_ids']+';E-IFVC-02',known_branches=f['known_branches']+'；T30目录教程90s可见加载中的打印配置，目标/纸张/浓度/份数/字号',gaps=f['gaps']+'；视频仅配置及加载，未证明打印完成或与旧型号帮助完全一致',visual_status='static_reference_only',checked_on='2026-10-27')
 else:continue
 updates.append(dict(flow_id=f['flow_id'],before=f,after=y))
dump(W/'existing-coverage-updates.json',updates)
old=next(x for x in load(W/'evidence-before.json') if x['evidence_id']=='E-ILD-17')
cap=dict(checked_on='2026-10-27',access_mode='anonymous_public_browser_help_model_selection',raw_work_path=str((W/'browser/04-t30pro-directory.json').relative_to(R)),raw_sha256=sha(W/'browser/04-t30pro-directory.json'),archived_artifact=str(N.relative_to(L)),sha256=sha(N),note='实际选择T30Pro，34功能标题；四条所选教程另立视频URL证据；不计产品实操',hands_on=False,artifact_hashes={str((A/x).relative_to(L)):sha(A/x) for x in ['04-t30pro-directory.json','04-t30pro-directory.png','video-samples.json','network-events.json']})
dump(W/'existing-evidence-additional-captures.json',[dict(evidence_id=old['evidence_id'],before=old.get('additional_captures',[]),after=old.get('additional_captures',[])+[cap])])
ip=L/'inventories/M027-help-inventory.csv';assert ip.read_bytes()==(W/'inventory-before.csv').read_bytes()
rs=list(csv.DictReader(io.StringIO(ip.read_text())));mapped={'I-M027-T591':(1,1),'I-M027-T604':(2,16),'I-M027-T595':(3,17),'I-M027-T590':(4,18)}
for r in rs:
 if r['entry_id'] not in mapped:continue
 e,f=mapped[r['entry_id']];r['review_status']='sampled_official_video_frames_partial_controls_not_continuous_or_hands_on';r['evidence_id']=';'.join(dict.fromkeys(r['evidence_id'].split(';')+[f'E-IFVC-{e:02}']));r['related_flow_ids']=f'F-M027-{f:02}'+(';F-M027-14' if f==16 else '');r['checked_on']='2026-10-27'
assert len(rs)==70 and sum(x['entry_id'] in mapped for x in rs)==4
wc(ip,rs);dump(W/'inventory-update.json',dict(path=str(ip.relative_to(R)),before_sha256=sha(W/'inventory-before.csv'),after_sha256=sha(ip),rows=70,updated=4))
case=(W/'case-before.md').read_text()
reps={
 '查阅日：2026-10-26。':'查阅日：2026-10-26；2026-10-27补四条官方教程的抽样画面。',
 '没有登录、设备操作、视频观看或连续视觉观察。产品功能清单未完整核定，学习主任务仍为 D1，其他部分任务为 D2。':'没有登录或设备操作；本轮抽样查看四条官方视频，非连续完整观看，未听音。F01补为局部D2，新增F16–18；完整功能目录和错误恢复仍未核定。',
 '只确认教程条目存在，未看视频，不能据目录补按钮或完成规则':'原来仅目录；本轮只补数学同步、阶段测、英语分级阅读和听一听，其余仍仅入口',
 '| F-M027-01 薄弱点学习路线 | 有相应年级科目内容；测评→查看弱项→学习讲解→对应练习 | 官方定义诊学练关系；学情如何由一道错题变化、手写如何提交未知 | 一次练习完成的判定和返回路径未知 | 01、02；不能填入臆测的题数、提示次数或“已掌握”阈值 |':'| F-M027-01 薄弱点学习路线 | 选知识点/方案→讲解、典例或练习；多空手写和去批改 | 教程有正确反馈/解析、下一题、全绿完成与下一节；中间换题 | 没有同题错答、提示、重试与持久恢复；全绿不证明真实掌握 | E-IFVC-01补局部D2；示例百分比不当通用阈值 |',
 '公开 T30 Pro 目录已提供“小学数学 AI 同步精准学”教程入口，作为下一步准确的观察对象，而不是用“AI 讲透”宣传补齐这些状态。':'本轮该教程补了题面、正确反馈与完成态，但剪辑和抽帧仍不能填满这一循环；不把中间不同题拼成一次操作。',
 '接口来源在批次索引记录；视频未观看。':'接口来源在批次索引记录；当时未看视频，本轮四条抽样另见E-IFVC-01～04。',
 '功能视频尚未观看；仅保存目录与定位，不将其中 58 条视频入口写成 58 条 D2 流程。':'此前只保存目录；本轮四条功能视频抽样观察，其余30功能视频和24问题视频未观看，不将58个入口写成58条已还原流程。',
 '本轮未获得可读取的 T30 Pro 单题连续学习视频或完整型号手册。':'本轮已取得四条T30 Pro目录所列公开教程并抽样查看，仍未取得同题连续错误/恢复链或完整型号手册。'}
for a,b in reps.items():assert a in case,a;case=case.replace(a,b)
case+='\n\n## 四条教程的画面与任务补证\n\n[逐时间点摘记及必要截图](../artifacts/iflytek-video-controls-deepening/source-notes.md)保留来源、播放器显示调整、样例差异和取证失败。F01仅升级为局部D2；官方视频仍是剪辑演示，未构成D3连续视觉链或D4实操。\n\n| 任务 | 观察到的状态与路线 | 未知 | E依据 |\n| --- | --- | --- | --- |\n'
for f in fs:case+=f"| {f['flow_id']} {f['task']} | {f['known_branches']} | {f['gaps']} | {f['evidence_ids']} |\n"
case+='''
数学图谱把难度层级用形状编码、掌握状态用颜色编码，这两个维度迁到Plus灰阶屏需分别保留；不能简单保留形状就宣称完成可读性适配。英语界面将阅读计数、跟读评测、作品和报告分开，完成标签不等于能力验证。听一听的历史、当前播放器、早晚听计划也不能合并成一个断点字段。这些是候选设计问题，未成为新增需求。

阶段测73/85s的考试/练习模式、未作答不入错题说明，以及90s的加载中打印配置分别登记；F14仅补配置画面，没有打印完成结论。视频型号关联来自帮助目录，当前固件、权益与全部题型仍未知。图中数值、样例日期、每周书数和时长不当默认值或用户研究结果。
'''
(L/'cases/M-027-iflytek-learning.md').write_text(case)
dump(W/'build-result.json',dict(new_evidence=len(es),new_flows=len(fs),old_flows=len(updates),old_flow_fields=sum(sum(u['before'].get(k)!=v for k,v in u['after'].items()) for u in updates),new_video_frames=len(all_observed),directory_images=1,old_evidence_captures=1,updated_inventory_rows=4))
print((W/'build-result.json').read_text())
