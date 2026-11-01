"""One-shot build of this M022 batch, without changing old ledgers."""
import csv
import hashlib
import json
from pathlib import Path

R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
W=R/'work/baicizhan-device-controls-deepening-2'
P='baicizhan-device-controls-deepening-2';A=L/'artifacts'/P
DATE='2026-10-27'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvwrite(p,rows,fields):
    with p.open('w',newline='') as f:
        wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(rows)
base=json.loads((W/'baseline.json').read_text());case=L/'cases/M-022-baicizhan-device.md'
assert sha(case)==base['case_sha256']
oldf={r['flow_id']:r for r in json.loads((W/'coverage-before.json').read_text())}
olde={r['evidence_id']:r for r in json.loads((W/'evidence-before.json').read_text())}
f_sources={};e_sources={}
for p in (L/'batches').glob('*-coverage.csv'):
    if p.name==P+'-coverage.csv':continue
    for r in csv.DictReader(p.open()):
        if r['flow_id'] in oldf:
            assert r==oldf[r['flow_id']]
            f_sources[r['flow_id']]=str(p.relative_to(R))
for p in (L/'batches').glob('*-evidence.jsonl'):
    if p.name==P+'-evidence.jsonl':continue
    for line in p.read_text().splitlines():
        r=json.loads(line)
        if r['evidence_id'] in olde:
            assert r==olde[r['evidence_id']]
            e_sources[r['evidence_id']]=str(p.relative_to(R))
assert len(f_sources)==3 and len(e_sources)==7
A.mkdir(parents=True,exist_ok=True)
(A/'source-notes.md').write_text('''# M022：公开帮助分支与型号边界

查阅2026-10-27。未使用设备、安装、登录、执行网页脚本或账户/学习接口；无新图像/音视频观察。

## E-BDCQ-01：dcj2 是何种帮助

此前已存官方`https://www.baicizhan.com/qa`入口HTML及其脚本`https://7n.bczcdn.com/baicizhan/h5/fe_qa/assets/index-CvNBM2bC.js`。本次仅本地复读；脚本字符偏移226396起的`sT()`把`deviceVersion=2.0`选择为`dcj2`，`ES.fetchQAList`以GET读取`/feedback_api/get_qa_info`。该参数不等于已核设备固件2.0；代码也没有Mini、Max或BCZ-Ace型号对应表。另一sxj分支未取，未猜测其他platform值。

按已存在的公开分支匿名GET：`https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=dcj2`，HTTP200。原件`work/baicizhan-device-controls-deepening-2/qa-dcj2.json`，SHA256 `774b63ea53111bdf9366af03e54563612949813c8266dd7d11da6f1a510c9bd9`。

`data.question_categories`含5类23问，全部文字已读，hot_questions为空、banner为空。正文自称超级单词机，未标SKU/修订日/build；Q10146/48/49涉及主机、手柄和扫描笔，Q10158—62讲投屏。不能把这一资料当Mini或3.52英寸Max手册。

防混用定位：Q10153离线打卡回传；Q10163设备收藏与App收藏范围；Q10166句子不同步；Q10144退出登录式解绑和会员账户限制；Q10145图背离线/小学模式限制；Q10148扫描笔圆键1秒。这里只保留范围和定位，不将这些步骤、数值或状态用于Mini/Max的F。23问逐项目录列为excluded_model_scope，不表示已覆盖23个目标功能。

## 旧E复读与目录覆盖

E-BCZH-01：复读`work/baicizhan-device-controls-deepening/01-official-home.html`及派生txt；Mini、3.52英寸Max和系列离线/词书/发音仍是宣传。22个href含重复，未给手册/帮助入口；不能据此证明售后或App内没有手册。

E-BDCD-01：复读同目录`02-52audio-ace-max.html`及txt68—88行；作者Wen5/2025-05-23，明确BCZ-Ace-Max。文字列键名、音量键和复位孔；孔或键存在不证明恢复操作、数据清除范围或退出位置。未重新请求此前403图片，不新增视觉事实。

Mini/Max目录17项分别标明旧静态记录、正文宣传、独立作者描述或既有历史证据记录。历史计划/同步教程仍无型号，关于页同步仍只沿用既有E记录；本轮没有其新原件，不伪称重新取得。完整型号化操作手册仍缺，3条原F均D1，0新增F。
''')
e=dict(evidence_id='E-BDCQ-01',product_id='M-022',title='dcj2公开帮助的超级单词机范围：未证实适用Mini/Max',
 url='https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=dcj2',checked_on=DATE,
 source_type='official_help',access_mode='opened_text',version_scope='帮助正文自称超级单词机；SKU/固件/App构建/发布日期均未知；不套Mini/Max',
 app_build=None,firmware_version=None,source_updated_on=None,locator='data.question_categories[0..4]，23问；Q10144/45/46/48/53/63/66',
 note='用途为型号适用性排除和目录记录，不补Mini/Max学习或同步流程；无图，无实操。',
 scope_role='model_applicability_boundary_only',hands_on=False,visual_observed=False,
 archived_artifact=f'artifacts/{P}/source-notes.md',sha256=sha(A/'source-notes.md'),capture_state='archived_source_notes',
 raw_work_path=f'work/{P}/qa-dcj2.json',raw_sha256=sha(W/'qa-dcj2.json'),http_status=200,supersedes=None,
 provenance=[dict(capture_url=url,access_mode='local_reread_of_existing_public_artifact',raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw),original_capture_on=DATE)
  for url,raw in [('https://www.baicizhan.com/qa',R/'work/baicizhan-controls-deepening/qa.html'),('https://7n.bczcdn.com/baicizhan/h5/fe_qa/assets/index-CvNBM2bC.js',R/'work/baicizhan-controls-deepening/qa-index.js')]])
(L/'batches'/f'{P}-evidence.jsonl').write_text(json.dumps(e,ensure_ascii=False)+'\n')
csvwrite(L/'batches'/f'{P}-coverage.csv',[],list(next(iter(oldf.values())).keys()))

gap_updates={
'F-M022-01':'型号映射、解锁后状态、长按冲突未知；dcj2的扫描笔圆键1秒不是Mini锁定操作证据',
'F-M022-02':'官方按键规则、长短按及状态作用、进入/答错/误斩撤销/结束、第二页、复习与同步影响未知；固件和App构建未知；dcj2超级单词机帮助不能补Max流程',
'F-M022-03':'历史型号适用性、首次绑定、缓存、取消/断网、同步对象/冲突与恢复未知；dcj2离线打卡/收藏/解绑条款未证实适用Mini/Max'}
updates=[]
for fid,after in gap_updates.items():
    changes={'evidence_ids':oldf[fid]['evidence_ids']+';E-BDCQ-01','gaps':after,'checked_on':DATE}
    for field,value in changes.items():
        before=oldf[fid][field]
        if value!=before:updates.append(dict(flow_id=fid,source_batch=f_sources[fid],field=field,before=before,after=value,
         reason='E-BDCQ-01仅限定型号适用性，不提供Mini/Max操作；旧任务深度不变'))
dump(W/'existing-coverage-updates.json',updates)
caps=[]
for eid,raw,locator in [
 ('E-BCZH-01',R/'work/baicizhan-device-controls-deepening/01-official-home.html','Mini/Max/系列功能正文；旧links.json的22个href；无新增界面观察'),
 ('E-BDCD-01',R/'work/baicizhan-device-controls-deepening/02-52audio-ace-max.html','原件开箱正文，派生txt68—88行；按键/接口/复位孔描述，不推恢复功能')]:
    before=olde[eid].get('additional_captures',[])
    cap=dict(capture_url=olde[eid]['url'],checked_on=DATE,original_capture_on=DATE,
     access_mode='local_reread_of_existing_public_artifact',raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw),
     locator=locator,limitation='未重新GET或看图；保持原型号、来源等级及失败路径边界',
     archived_artifact=f'artifacts/{P}/source-notes.md',sha256=sha(A/'source-notes.md'),hands_on=False,visual_observed=False)
    caps.append(dict(evidence_id=eid,source_batch=e_sources[eid],field='additional_captures',before=before,after=before+[cap],reason='同URL旧原件复读，不新增编号'))
dump(W/'existing-evidence-additional-captures.json',caps)

rows=[]
target=[
 ('Mini锁定提示','Mini区产品图，长按圆键3秒','E-BCZH-02','F-M022-01','Mini名/图印BCZ-Ace-1S映射未知','prior_static_record_reused','解锁后状态与长按职责未知'),
 ('Max单词新学','Max官方图首页第1项','E-BCZH-03','F-M022-02','3.52英寸Max，固件未知','prior_static_record_reused','进入、作答、判错、结束未知'),
 ('Max极速刷词','Max官方图首页第2项','E-BCZH-03','F-M022-02','3.52英寸Max，固件未知','prior_static_record_reused','题型、队列与调度未知'),
 ('Max神秘小屋','Max官方图首页第3项','E-BCZH-03','F-M022-02','3.52英寸Max，固件未知','prior_static_record_reused','目标、入口后状态、奖励未知'),
 ('Max单词游戏','Max官方图首页第4项','E-BCZH-03','F-M022-02','3.52英寸Max，固件未知','prior_static_record_reused','规则、结算、恢复未知'),
 ('Max单词速听','Max官方图首页第5项','E-BCZH-03','F-M022-02','3.52英寸Max，固件未知','prior_static_record_reused','播放/暂停、离线音频、续播未知'),
 ('Max填空拼写','Max官方图首页第6项','E-BCZH-03','F-M022-02','3.52英寸Max，固件未知','prior_static_record_reused','输入/判错/提示/重试未知'),
 ('Max主页分页','Max官方图页码1/2','E-BCZH-03','F-M022-02','3.52英寸Max，固件未知','prior_static_record_reused','第二页内容及分页动作未知'),
 ('Max实体键/接口','2025拆解开箱正文；txt78—85行','E-BDCD-01','F-M022-02','Wen5的BCZ-Ace-Max样机；2025-05-23','body_reread_independent_description','键名/复位孔不等于操作反馈、复位步骤或恢复效果'),
 ('系列图形助记','系列词卡图、首页图背宣传','E-BCZH-04;E-BCZH-01','F-M022-02','系列范围，具体机型未知','prior_static_record_and_marketing_text','题面与计数不等于Mini/Max同构建'),
 ('系列词书类别','官网全覆盖/词书类别正文','E-BCZH-01','F-M022-03','系列宣传，逐型号未知','body_reread_marketing','词书菜单、资源大小、选书保存/删除未知'),
 ('系列离线声明','官网没网也能学正文','E-BCZH-01','F-M022-03','系列宣传，逐型号未知','body_reread_marketing','缓存条件、学习与发音离线范围未知'),
 ('系列词/例句发音','官网更地道正文','E-BCZH-01','F-M022-02','系列宣传，逐型号未知','body_reread_marketing','输入、播放状态、耳机/音量/中断未知'),
 ('Max宠物小狗','Max区文字与图标','E-BCZH-01','F-M022-02','Max区宣传，固件未知','body_reread_marketing','养成、消耗、奖励、离家找回未知'),
 ('历史App配置单词本计划','2022-11-08教程App段，既有E记录','E-BCDVC-01','F-M022-03','硬件型号/固件未标','existing_evidence_record_only','不能当Mini/Max当前计划规则；本轮未新取该原件'),
 ('历史设备确认/切书','2022-11-08教程设备段，既有E记录','E-BCDVC-01','F-M022-03','硬件型号/固件未标','existing_evidence_record_only','取消、缺资源、覆盖与冲突未知；本轮未新取原件'),
 ('系列多端同步声明','关于页生态协同段，既有E记录','E-BCDVC-02','F-M022-03','系列宣传，逐型号未知','existing_evidence_record_only','方向、对象、回执、备份恢复未知；未本轮重取正文')]
for i,(name,loc,eids,fids,scope,status,gap) in enumerate(target,1):
    rows.append(dict(inventory_id=f'BDQ-T{i:02}',product_id='M-022',node_type='existing_target_or_historical_item',function_domain=name,source_locator=loc,evidence_ids=eids,flow_ids=fids,model_scope=scope,body_status=status,visual_status='not_observed',applicability='scope_conditioned_existing_record',remaining_gap=gap,checked_on=DATE))
d=json.loads((W/'qa-dcj2.json').read_text())['data']
for ci,c in enumerate(d['question_categories']):
    rows.append(dict(inventory_id=f'BDQ-P{ci+1}',product_id='M-022',node_type='excluded_category',function_domain=c['name'].strip(),source_locator=f'data.question_categories[{ci}]',evidence_ids='E-BDCQ-01',flow_ids='',model_scope='超级单词机；精确SKU/日期/构建未知',body_status='category_index_read',visual_status='not_observed',applicability='excluded_model_scope',remaining_gap='父项不计任务；未证实对Mini/Max适用',checked_on=DATE))
    for qi,q in enumerate(c['questions']):
        rows.append(dict(inventory_id=f"BDQ-Q{q['id']}",product_id='M-022',node_type='excluded_question',function_domain=q['question'].strip(),source_locator=f"data.question_categories[{ci}].questions[{qi}] id={q['id']}",evidence_ids='E-BDCQ-01',flow_ids='',model_scope='超级单词机；精确SKU/日期/构建未知',body_status='text_read',visual_status='not_observed',applicability='excluded_model_scope',remaining_gap='该回答仅作范围核定，不迁移入口/规则/数值到Mini/Max',checked_on=DATE))
assert len(rows)==45
csvwrite(L/'batches'/f'{P}-inventory.csv',rows,list(rows[0].keys()))

s=(W/'case-before.md').read_text().replace('本轮','上轮')
s=s.replace('正式记录共三 F、七 E，其中来源重复关系和历史型号边界保留；不是七份型号化手册。','正式记录共三 F、八 E，其中新E-BDCQ-01仅用于适用范围核查；来源重复关系和历史型号边界保留，不代表八份型号化手册。')
section='''## 新取得公开帮助：型号范围未匹配

2026-10-27本批从此前已保存的官方帮助前端确认`deviceVersion=2.0`选择`platform=dcj2`，随后仅普通匿名GET该公开帮助内容。这个参数值不是已核硬件代次、固件号或Mini/Max映射。没有执行JS、探测账户接口、猜测其他platform或读取sxj分支。

E-BDCQ-01正文共有5类23问，自称**超级单词机**，并描述扫描笔/手柄、拍照查词、下拉控制中心和投屏；没有Mini、Max或BCZ-Ace对应说明，也没有发布日期、修订号和精确SKU。因此以下帮助不能补本案例的设备流程：

| 取得的帮助条目 | 对Mini/Max仍不能下的结论 |
| --- | --- |
| Q10148扫描笔圆键开机和灯色；Q10164熄屏设置 | 不替代Mini图中的圆键3秒锁定提示，也不证明Max系统手势 |
| Q10153离线打卡回传；Q10145离线范围与下载条件 | 不证明Mini/Max自动回传对象、时点或图背/小学模式限制 |
| Q10163设备收藏与App收藏的区别；Q10166句子同步 | 不推定Mini/Max的收藏队列和句子支持情况 |
| Q10144退出登录式解绑及会员账户范围 | 不推定Mini/Max解绑入口、记录清空或权益迁移 |
| Q10158—62投屏；Q10146/49主机、手柄和扫描笔 | 不据共用品牌补Mini/Max硬件或系统能力 |

[本批逐项台账](../batches/baicizhan-device-controls-deepening-2-inventory.csv)分两组：17条已有目标/历史条目，标明正文宣传、旧静态观察记录、独立作者说明或仅沿用既有E记录；另5个父项+23问全部列为超级单词机范围排除，没有关联Mini/Max流程。目录数不是完成数，仍未取得Mini/Max的完整型号化手册。3条原F保持D1，0新增F；新E只是防止范围混用。

本批没有新静态图观察；旧3图只沿用既有记录。复读官网22个href及2025拆解键位正文，未重复请求京东、抖音或52audio图片失败路径。公开帮助之外的首学反馈、误斩撤销、宠物、听词、资源下发失败与重启恢复仍缺；未取得不等于设备不支持。详见[独立来源短摘记](../artifacts/baicizhan-device-controls-deepening-2/source-notes.md)。

'''
s=s.replace('## 主任务的可证据化部分\n',section+'## 主任务的可证据化部分\n')
s=s.replace('## 已观察图片与证据\n','## 已观察图片与证据\n\n- E-BDCQ-01：[dcj2公开帮助正文](https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=dcj2)，本批取得并读完23问；只核定其自称超级单词机的范围，不证明Mini/Max流程。\n')
s=s.replace('不是上轮执行记录','不是执行记录').replace('上轮还不能判断','仍不能判断').replace('| 问题 | 上轮结果 |','| 问题 | 已知边界 |').replace('截至上轮仍未取得','截至本次资料核查仍未取得')
case.write_text(s);(W/'case-after.md').write_text(s)
dump(W/'build-result.json',dict(new_evidence=1,new_flows=0,inventory_rows=45,target_or_historical_items=17,excluded_categories=5,excluded_questions=23,old_flow_fields=len(updates),old_evidence_captures=len(caps),old_source_batches=sorted(set(f_sources.values())|set(e_sources.values()))))
print((W/'build-result.json').read_text())
