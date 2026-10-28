from pathlib import Path
import json,csv,hashlib,shutil,io
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/iflytek-planning-reading-deepening';A=L/'artifacts/iflytek-planning-reading-deepening';B=L/'batches'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text())
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def wc(p,rs):
 with p.open('w',newline='') as h:
  w=csv.DictWriter(h,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
assert not A.exists(),'one-time build'
assert (L/'cases/M-027-iflytek-learning.md').read_bytes()==(W/'case-before.md').read_bytes()
A.mkdir()
selected=[
 ('小学语文讯飞书城','03-bookstore-open',[('bookstore',40),('bookstore',55),('bookstore-extra',60),('bookstore',70),('bookstore-extra',75),('bookstore',85),('bookstore',100),('bookstore',115),('bookstore',130),('bookstore',145),('bookstore',175),('bookstore-extra',200)]),
 ('小学语文同步学习','04-chinese-sync-open',[('chinese-sync',55),('chinese-sync-extra',75),('chinese-sync',85),('chinese-sync',100),('chinese-sync',115),('chinese-sync-extra',120),('chinese-sync',130),('chinese-sync-extra',135)]),
 ('小学自定义学习计划','05-custom-plan-open',[('custom-plan',25),('custom-plan',55),('custom-plan-extra',65),('custom-plan-extra',76),('custom-plan-extra',90),('custom-plan',100),('custom-plan',115),('custom-plan-extra',121),('custom-plan',127.8)]),
 ('小学语文看图背诗','06-poem-open',[('poem-extra',32),('poem',40),('poem',55),('poem',70),('poem',85),('poem-extra',93)])]
media=[]
for i,(title,cap,fs) in enumerate(selected,1):
 base=load(W/'browser'/f'{cap}.json');v=base['videos'][0];obs=[]
 for pre,t in fs:
  src=W/'frames'/pre/f'{t:06.2f}.png';m=load(src.with_suffix('.json'));assert sha(src)==m['sha256'] and m['actual_pixels']==[1280,720]
  dst=A/f'video-{i:02}-{t:06.2f}s.png';shutil.copyfile(src,dst)
  obs.append(dict(path=str(dst.relative_to(L)),sha256=sha(dst),position_seconds=m['position'],requested_seconds=t,actual_pixels=m['actual_pixels'],source_url=v['src'],observed_on='2026-10-27',method='browser_seek_paused_element_screenshot',product_hands_on=False,audio_observed=False))
 media.append(dict(evidence_id=f'E-IFPR-{i:02}',title=title,url=v['src'],help_player_url=base['url'],duration_seconds=v['duration'],native_dimensions=[v['width'],v['height']],selected_frames=obs))
dump(A/'video-samples.json',media)
for name in ['02-t30pro-directory.json','02-t30pro-directory.png']:shutil.copyfile(W/'browser'/name,A/name)
for name in ['network-events.json','commands.json']:shutil.copyfile(W/name,A/name)
note=(W/'observations.md').read_text()+'\n## 正式摘帧定位\n\n其余已看试采帧留work；本表是支持后续引用的必要画面，不是全片存档。[采样元数据](video-samples.json)保存原URL、请求/实际时间、尺寸和散列。\n\n| E | 原视频 | 正式帧 |\n| --- | --- | --- |\n'
for m in media:note+=f"| {m['evidence_id']} | [{m['title']}]({m['url']}) | "+'、'.join(f"[{x['requested_seconds']}s]({Path(x['path']).name})" for x in m['selected_frames'])+' |\n'
N=A/'source-notes.md';N.write_text(note)
es=[]
for m in media:
 es.append(dict(evidence_id=m['evidence_id'],product_id='M-027',title=m['title']+'：官方教程抽样控件',url=m['url'],checked_on='2026-10-27',source_type='official_help_video_sampled_frames',access_mode='anonymous_browser_media_stream_seek_and_view',version_scope='T30Pro帮助目录关联；固件/App构建/发布日期未知；计划视频内2025样例不是当前开放时间',app_build=None,firmware_version=None,source_updated_on=None,locator='seconds: '+','.join(str(x['requested_seconds']) for x in m['selected_frames']),note='root逐图查看；剪辑与采样不构成完整连续操作；未听音或实机，保存状态和恢复未知',hands_on=False,visual_observed=True,visual_kind='sampled_video_frames',archived_artifact=str(N.relative_to(L)),sha256=sha(N),artifact_hashes={x['path']:x['sha256'] for x in m['selected_frames']},observed_artifacts=m['selected_frames'],help_player_url=m['help_player_url'],duration_seconds=m['duration_seconds'],http_status=206,supersedes=None,derived_from='E-ILD-17'))
old=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()]
assert not {x['url'] for x in es}&{x['url'] for x in old}
assert not {x['evidence_id'] for x in es}&{x['evidence_id'] for x in old}
(B/'iflytek-planning-reading-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
tasks=[
 (19,'安排一本书的精读并查看挑战与收获','E-IFPR-01','书城精读：计划天数/时长/提醒→立即开启入口；读/闯/素材的勾与锁；正确单选、碎片拼图、素材原书和精读报告分别呈现','样例关数变化和剪辑；完整开计划回执、错答/提示/解锁、重复奖励、保存/恢复未知；阅读量与互动次数非掌握'),
 (20,'分类找书、加入书架并定位章节读听','E-IFPR-01','藏书阁年级/分类→书页加入书架；目录标当前章节；绘本页音频浮条；阅读周报/累计报告含书目进度','加入未证下载/备份；未听音，读听对齐/关闭语义/断点/离线/删除未知；阅读本数非读完本数'),
 (21,'按课文朗读、跟读或背诵并看报告','E-IFPR-02','语文电子课本→目录/片段评入口；朗读/跟读/背诵分模式；报告准确度/完整度/流畅度；同步讲解另入口','录音启动/提交全链、重录、评分准确度、文本定位/存储/断网和重进未知；纸书摄像指读另F15'),
 (22,'按字词学习并配置屏上或纸上听写','E-IFPR-02','按字查笔顺/释义/同音/形近；候选会认/会写、屏上/纸上、今日错字、随机数量；屏上手写清除/前后项/暂停','不同字表示例不可拼事务链；误识别/纸面批改/错字写入移出、结束结算与恢复未知'),
 (23,'制定或调整每周学习计划','E-IFPR-03','推荐计划→教材章节/基础拓展→取消确定；按周排序/删除/新增/确定；家长端按孩子选计划与任务库；本周无任务提示','2025样例、三/四年级剪辑不一致；修改保存回执、已完成数据/逾期/移除/提醒/双端冲突未知；任务库非开放式生成'),
 (24,'用图理解诗句、提示试背并查看发音报告','E-IFPR-04','选诗→配图/看图记忆/试背/正式背；正确配图、逐图诗意、眼睛提示；报告含再试/两种发音回放/发父母入口','错配重试、录音提交/低分引导、提示影响分数、再试覆盖、发送接收和退出恢复未知；颜色和星数非记忆效果')]
fs=[dict(flow_id=f'F-M027-{n:02}',product_id='M-027',task=t,depth='D2',evidence_ids=e,known_branches=b,gaps=g,visual_status='static_reference_only',hands_on='false',case_path='cases/M-027-iflytek-learning.md',checked_on='2026-10-27') for n,t,e,b,g in tasks]
wc(B/'iflytek-planning-reading-deepening-coverage.csv',fs)
oldcap=next(x for x in old if x['evidence_id']=='E-ILD-17')
cap=dict(checked_on='2026-10-27',access_mode='anonymous_public_browser_help_model_selection',raw_work_path=str((W/'browser/02-t30pro-directory.json').relative_to(R)),raw_sha256=sha(W/'browser/02-t30pro-directory.json'),archived_artifact=str(N.relative_to(L)),sha256=sha(N),note='再选T30Pro并打开四条新的教程；样本视频另E-IFPR-01至04，不计产品操作',hands_on=False,artifact_hashes={str((A/x).relative_to(L)):sha(A/x) for x in ['02-t30pro-directory.json','02-t30pro-directory.png','video-samples.json','network-events.json','commands.json']})
dump(W/'existing-evidence-additional-captures.json',[dict(evidence_id=oldcap['evidence_id'],before=oldcap.get('additional_captures',[]),after=oldcap.get('additional_captures',[])+[cap])])
ip=L/'inventories/M027-help-inventory.csv';assert ip.read_bytes()==(W/'inventory-before.csv').read_bytes();rs=list(csv.DictReader(io.StringIO(ip.read_text())))
mapped={'I-M027-T602':(1,[19,20]),'I-M027-T599':(2,[21,22]),'I-M027-T746':(3,[23]),'I-M027-T601':(4,[24])}
for row in rs:
 if row['entry_id'] not in mapped:continue
 e,fn=mapped[row['entry_id']];row.update(review_status='sampled_official_video_frames_partial_controls_not_continuous_or_hands_on',evidence_id=row['evidence_id']+f';E-IFPR-{e:02}',related_flow_ids=';'.join(f'F-M027-{n:02}' for n in fn),checked_on='2026-10-27')
wc(ip,rs)
case=(W/'case-before.md').read_text()
case=case.replace('2026-10-27补四条官方教程的抽样画面。','2026-10-27两批合计补八条官方教程的抽样画面。')
case=case.replace('本轮抽样查看四条官方视频，非连续完整观看，未听音。F01补为局部D2，新增F16–18','已分两批抽样查看八条官方视频，非连续完整观看，未听音。F01补为局部D2，新增F16–24')
case=case.replace('本轮只补数学同步、阶段测、英语分级阅读和听一听，其余仍仅入口','两批补数学同步、阶段测、英语分级阅读、听一听、语文书城/同步/计划/看图背诗；其余仍仅入口')
case=case.replace('本轮四条功能视频抽样观察，其余30功能视频和24问题视频未观看','两批八条功能视频抽样观察，其余26功能视频和24问题视频未观看')
case+='\n\n## 继续补证：阅读、计划与语文\n\n[逐时间点短摘和必要画面](../artifacts/iflytek-planning-reading-deepening/source-notes.md)支持以下局部D2，未补成连续D3。每个任务按用户目标保留编号，控件在任务内列出，不逐按钮增加F。\n\n| F任务 | 可见路线与状态 | 仍缺 | E |\n| --- | --- | --- | --- |\n'
for f in fs:case+=f"| {f['flow_id']} {f['task']} | {f['known_branches']} | {f['gaps']} | {f['evidence_ids']} |\n"
case+='\n拼图完成、任务完成、阅读百分比、发音评分与知识掌握不能互换。自定义计划的修改范围比一个“AI规划”标签更有参考价值，但本教程没有证明后台保存、冲突处理或系统接口。5–6岁独立阅读者是否需要这些年级任务、提示和奖励仍需原型讨论；参考没有升级为新需求。\n'
(L/'cases/M-027-iflytek-learning.md').write_text(case)
dump(W/'build-result.json',dict(new_evidence=4,new_flows=6,frames=sum(len(m['selected_frames']) for m in media),directory_images=1,old_evidence_captures=1,updated_inventory_rows=4))
print((W/'build-result.json').read_text())
