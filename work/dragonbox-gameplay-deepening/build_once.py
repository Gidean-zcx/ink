from pathlib import Path
import json,csv,hashlib,copy,shutil
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/dragonbox-gameplay-deepening';A=L/'artifacts/dragonbox-gameplay-deepening';B=L/'batches'
assert not A.exists(),'one-time build'
A.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
notes='''# DragonBox Numbers：历史操作与挫折分支

查阅2026-10-27。两个作者自述体验，只支持各自历史版本；没有安装或操作Numbers，没有听音或观看连续演示。不能推成当前Kahoot版保证或学习成效。源HTML留work，正式记录保留短摘及散列。

## E-DBGP-01：2015年的操作回执

[Jonathan H. Liu，GeekDad，2015-12-02](https://geekdad.com/2015/12/dragonbox-osmo-numbers/)，正文Numbers部分，作者披露获得评测样品。按顶端管道取得数块，拖入轮廓后着色；合并/划切改变数量，部分正确匹配后可描数字得星。完成拼图获币解锁。Ladder踩中炸弹所在数值会炸掉数块，上箭头使其增长，过关会补全选关图。作者还报告语音回执；本研究未听到。没有错放动画、生命扣减、撤销或保存时机。后半Osmo是另一产品，不能拼接其规则。

## E-DBGP-02：2018年的输入映射与受挫

[Kent Haines，Games for Young Minds，2018-03-16](https://www.gamesforyoungminds.com/blog/2018/3/16/dragonbox-numbers)，How to Play / Questions to Ask。作者描述Run按下数字决定跳高、收集星币；也报告两个孩子因数块无法放到预期处而受挫。它不揭示是规则不允许、触控失误还是判定范围，更没有内置提示/重试证据。作者建议暂停用实物探索，属于成人引导，不是App自动机制。文中拼图视频引用GeekDad，不能计两次独立演示。

## 本次没有取得的视觉证据

[DragonBox Numbers: Run! activity](https://www.youtube.com/watch?v=yxTpyBpVCuA)：web读取失败后，在既有Chromium156.0.8078.4中正常匿名导航一次。页面200，能见标题和DragonBox频道名；播放器要求登录证明非机器人，video readyState=0，没有媒体画面。自动页面请求另有401，未登录、绕过、换嵌入地址或重试。精确发布日期未核，不能把搜索引用的2016日期当已核元数据。

[采集记录](acquisition.json)与[失败页面截图](access-block.png)保存访问事实；root实际查看该截图。它不是产品截图，不计静态交互或D4。没有保存MP4。

## 对原任务的影响

F01/02增加历史动作反馈，F05增加受挫报告，F08增加历史数字与跳高映射。四个稳定任务仍分别D2/D2/D1/D1；没有新增F。F05缺错误原因、提示与重试链；F08缺选关、结束、失败与恢复，不能称完整任务重建。F06的退出恢复也没有新依据。

本项目建议：后续儿童原型区分“数学理解错误”与“孩子不会放置”，分别观察能否说明原因和完成动作。Plus是否适合实时跑跳仍需同机刷新/延迟验证；这里不设定玩法或硬件能力。这是研究启发，不新增用户需求。
'''
N=A/'source-notes.md';N.write_text(notes)
for src,dst in [(W/'acquisition.json',A/'acquisition.json'),(W/'browser/page.png',A/'access-block.png')]:shutil.copyfile(src,dst)
old=json.loads((W/'coverage-before.json').read_text());changes=[]
patches={
'F-M050-01':dict(evidence_ids='E-DRGN-02;E-DRGN-03;E-DBCD-01;E-DBGP-01',known_branches='旧指南与2015作者体验分开：点管道取块、拖放着色，部分正确匹配后描数得星；拼图完成获币解锁。作者报告语音反馈，本研究未听音。',gaps='当前构建、错放/提示/撤销、最后落位到结算连续过程、奖励数值/重玩收益与恢复未核；旧反馈不作当前保证。'),
'F-M050-02':dict(evidence_ids='E-DRGN-02;E-DBCD-01;E-DBGP-01',known_branches='旧p8的数轴教学范围与2015作者体验分开：恰中炸弹数值会炸掉Noom，上箭头使其增长，完成关卡补全选关图。',gaps='当前版本碰撞/生命/重试、结算和保留奖励未核；2015描述不证明现代规则，四教学目标不当关卡或成效。'),
'F-M050-05':dict(evidence_ids='E-DRGD-01;E-DRGD-02;E-DRGN-02;E-DBGP-02',known_branches='2018作者报告孩子因数块放不到预期位置而受挫；错误原因和程序反馈未说明，成人暂停建议不是内置提示。',gaps='错放原因及界面反馈、提示、撤销、重试到结算；当前版本亦未核。',checked_on='2026-10-27'),
'F-M050-08':dict(evidence_ids='E-DRGN-03;E-DBCD-02;E-DBGP-02',known_branches='2018作者描述按数字决定跳高并收集星币；与当前开发者描述分开。官方Run视频页面存在，但登录验证使播放失败。',gaps='完整入口、按键位置/映射、节奏、错误惩罚/生命、结算和恢复未核；历史部分输入不足以升级D2。')}
for f in old:
 if f['flow_id'] in patches:
  after=copy.deepcopy(f);after.update(patches[f['flow_id']]);changes.append(dict(flow_id=f['flow_id'],before=f,after=after))
for p in B.glob('*-coverage.csv'):
 rows=list(csv.DictReader(p.open()));changed=False
 for i,f in enumerate(rows):
  for c in changes:
   if f['flow_id']==c['flow_id']:
    assert f==c['before'],f['flow_id'];rows[i]=c['after'];changed=True
 if changed:
  with p.open('w',newline='') as h:
   wr=csv.DictWriter(h,fieldnames=list(rows[0]),lineterminator='\n');wr.writeheader();wr.writerows(rows)
evidence=[];existing=[json.loads(s) for p in B.glob('*-evidence.jsonl') for s in p.read_text().splitlines() if s.strip()]
acq=json.loads((W/'acquisition.json').read_text())
for n,key,date,title in [(1,'geekdad','2015-12-02','2015作者体验：取块/描数/回执和Ladder障碍'),(2,'youngminds','2018-03-16','2018作者体验：Run输入映射与数块放置受挫')]:
 row=next(x for x in acq['pages'] if x['key']==key);assert row['status']==200
 e=dict(evidence_id=f'E-DBGP-0{n}',product_id='M-050',title=title,url=row['url'],checked_on='2026-10-27',source_type='historical_firsthand_parent_review',access_mode='public_article_text_read',version_scope=f'{date} historical Numbers; installed build unspecified',source_updated_on=date,app_build=None,locator='Numbers sections only' if n==1 else 'How to Play / Questions to Ask',note='作者体验非本项目实测；历史动作/问题不得套当前构建；嵌入视频未播放',hands_on=False,visual_observed=False,archived_artifact=str(N.relative_to(L)),sha256=sha(N),raw_work_path=row['path'],raw_sha256=row['sha256'],supersedes=None)
 assert all(x['url']!=e['url'] and x['evidence_id']!=e['evidence_id'] for x in existing);evidence.append(e)
(B/'dragonbox-gameplay-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evidence))
inventory=[]
for i,(task,eid,fid,status,gap) in enumerate([
 ('取得数块与组合/划切','E-DBGP-01','F-M050-01','historical_text_read','当前构建和触摸判定未知'),
 ('匹配着色与描数星星','E-DBGP-01','F-M050-01','historical_text_read','触发频率/错误回执未知'),
 ('拼图金币解锁','E-DBGP-01','F-M050-01','historical_text_read','奖励量/重玩/提交时机未知'),
 ('Ladder障碍及选关图','E-DBGP-01','F-M050-02','historical_text_read','生命/失败/恢复未知'),
 ('数块放置受挫','E-DBGP-02','F-M050-05','historical_firsthand_problem','不是规则或触摸错误的诊断'),
 ('Run数字输入与跳高','E-DBGP-02','F-M050-08','historical_text_read','结束/重试缺失，仍D1'),
 ('官方Run视频','', 'F-M050-08','blocked_not_watched','播放器要求登录验证，未绕过')],1):
 inventory.append(dict(item_id=f'DBGP-{i:02d}',product_id='M-050',functional_area=task,evidence_ids=eid,flow_ids=fid,status=status,gaps=gap,checked_on='2026-10-27'))
with (B/'dragonbox-gameplay-deepening-inventory.csv').open('w',newline='') as h:
 w=csv.DictWriter(h,fieldnames=list(inventory[0]),lineterminator='\n');w.writeheader();w.writerows(inventory)
C=L/'cases/M-050-dragonbox.md';assert C.read_bytes()==(W/'case-before.md').read_bytes()
C.write_text(C.read_text()+'''\n\n## 历史游戏操作与受挫补证（2026-10-27）\n\n新增E-DBGP-01/02来自2015和2018两位作者的亲子体验，见[来源摘记与访问失败边界](../artifacts/dragonbox-gameplay-deepening/source-notes.md)及[七个聚焦问题](../batches/dragonbox-gameplay-deepening-inventory.csv)。补回F01/02/05/08，0新F、等级不变；保留旧指南、历史作者与当前商店三种范围。作者曾获得评测样品，另一篇引用同一拼图视频，不算两段独立演示。\n\nF01/02的部分动作反馈有历史文字支持；F05仍不能判断放置困难属于数学错误还是操作判定，F08仍缺完整开始/失败/结束链。官方Run视频本次遇登录验证，未播放，失败页面不计产品图或D4。没有补齐F06退出恢复。后续儿童设计宜分别检查数量理解与操作可达性，此为助手建议，不是已确认需求或学习效果结论。\n''')
(W/'coverage-updates-applied.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2)+'\n')
manifest={str(p.relative_to(R)):sha(p) for p in [*A.iterdir(),B/'dragonbox-gameplay-deepening-evidence.jsonl',B/'dragonbox-gameplay-deepening-inventory.csv',C]}
(W/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(dict(new_evidence=len(evidence),new_flows=0,old_flows=len(changes),old_fields=sum(sum(c['before'][k]!=c['after'][k] for k in c['before']) for c in changes),inventory_rows=len(inventory),new_product_visuals=0),ensure_ascii=False))
