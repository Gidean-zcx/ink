"""Build only M016-owned artifacts; old rows are proposals, never edited."""
import csv
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path('/home/workspace/reader')
W = ROOT / 'work/baicizhan-controls-deepening'
L = ROOT / 'docs/research/interaction-library'
A = L / 'artifacts/baicizhan-controls-deepening'
B = L / 'batches'
DATE = '2026-10-27'
PREFIX = 'baicizhan-controls-deepening'

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def dump(p, obj):
    p.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + '\n')

def write_csv(p, fields, rows):
    with p.open('w', newline='') as f:
        wr = csv.DictWriter(f, fieldnames=fields, lineterminator='\n')
        wr.writeheader()
        wr.writerows(rows)

baseline = json.loads((W / 'baseline.json').read_text())
case = L / 'cases/M-016-baicizhan.md'
assert sha(case) == baseline['case_sha256'], 'case no longer matches initial baseline'
old_f = {r['flow_id']: r for r in json.loads((W / 'coverage-before.json').read_text())}
old_e = {r['evidence_id']: r for r in json.loads((W / 'evidence-before.json').read_text())}
f_sources, e_sources = {}, {}
for p in B.glob('*-coverage.csv'):
    if p.name == PREFIX + '-coverage.csv':
        continue
    with p.open() as f:
        for r in csv.DictReader(f):
            if r['flow_id'] in old_f:
                assert r == old_f[r['flow_id']]
                f_sources[r['flow_id']] = str(p.relative_to(ROOT))
for p in B.glob('*-evidence.jsonl'):
    if p.name == PREFIX + '-evidence.jsonl':
        continue
    for line in p.read_text().splitlines():
        r = json.loads(line)
        if r['evidence_id'] in old_e:
            assert r == old_e[r['evidence_id']]
            e_sources[r['evidence_id']] = str(p.relative_to(ROOT))
assert len(f_sources) == 6 and len(e_sources) == 16

A.mkdir(parents=True, exist_ok=True)
shutil.copyfile(W / 'qa-rhyme-mode.jpg', A / 'qa-rhyme-mode.jpg')
(A / 'visual-audit.md').write_text('''# M016 本轮静态图审计

2026-10-27实际使用view_image查看`work/baicizhan-controls-deepening/qa-rhyme-mode.jpg`；原图1125×3346，工具展示689×2048整图。E-BCCD-02；不是App实操或连续录屏。

来源为官方帮助Q49唯一图片，URL：https://vol.bczcdn.com/adv/8bc113e183b9952c6ac0ce8329cb266f.jpg 。原图与正式图SHA256均为5791d1a6c0a0160d43dcd1a5826a28351356fa3c190cf18b5387287d62fc2fc2。

观察：上部三步图文指先更新版本、从主页“修改”进入计划，再换指定词书并选“押韵模式”。下部是计划页模式卡片，含押韵和图片选项；文字列四级高频、四级词汇大全、中考词汇、高考词汇。图文说明按同一路径选图片模式可切回。

边界：示例计划日期2023-06-22不是资料发布日期；“最新版本”无构建。“切换成功”来自图片说明，未实际执行或看到执行结果。没有题目作答、纠错、提示、音频或奖励画面。其余帮助附图未查看；前轮5图未重看，继续使用旧独立审计。
''')
(A / 'source-notes.md').write_text('''# M016 官方帮助、版本和数据边界短摘记

查阅2026-10-27；所有操作均为公开材料描述，没有安装或运行App。本轮仅查看Q49教学图，见独立visual-audit.md；其余配图只登记URL。

## 新帮助证据 E-BCCD-01 / 图片 E-BCCD-02

官方入口https://www.baicizhan.com/qa 的HTML只有帮助标题和前端资源引用。静态读取所引用的`https://7n.bczcdn.com/baicizhan/h5/fe_qa/assets/index-CvNBM2bC.js`：`ES.fetchQAList`调用`assistant.baicizhan.com/feedback_api/get_qa_info`，`sT()`选择非设备分支`bczApp`。仅普通匿名GET该地址并带`platform=bczApp`，未执行JS、统计POST、反馈或账户接口。入口、脚本和JSON均HTTP200。

原件`work/baicizhan-controls-deepening/qa-bczApp.json`的`data.question_categories`有5类50问；`hot_questions`8项为重复入口，不另计。49问有文字，Q49仅图；无发布日期、修订号或逐题OS/build。逐项定位用分类索引与question.id，台账包含5父项，未声称完成整个App。爱阅读11问保留栏目边界，积分不当主App铜板。

关键定位：Q11为已斩列表中选词恢复，不是最近一次作答的即时撤销；Q20区分登录身份、网络未读全和离线未上传前清缓存；Q51把完成计划与上传并列，10秒停留是传输建议，无确认回执。Q10的铜板补卡只作用于学习日历。Q22给收藏词的PDF出口，未给整本任意词表导出。具体动作见案例，缺口不按常识补全。

Q38/21给资源下载与缺失时联网补资源；不证明所有题型离线。Q12题型开关无构建；Q18否定导入词表的措辞与功能介绍“自定义上传”冲突。Q49实看一图，模式切回与书名范围仅限该图，不能证明当前构建；图中日期不作发布日期。

## 旧E追加原件 / 版本冲突

- E-BCZ-01：同URL新GET功能介绍；4段均是能力声明，不能补按钮流程。
- E-BCZ-02：复读前轮`work/chinese-vocab-deepening/bcz.html`，不是本轮GET；深度模式声明与2023公告不同。
- E-BCZ-03：同URL新GET中国区App Store。正文iPhone/iPad、最低OS13.0，版本7.10.25日期2026-10-19；TitledParagraph的版本/日期字段已提取到`app-store-version-index.json`。不采用评论、截图或watch参数推导平台。7.10.12的记忆曲线描述没有调度公式；7.10.25只有一般改进说明。
- E-BCZ-04：复读前轮`bcz-info.html`明示3.8.3的同步及两类离线范围，未新GET。
- E-BCZ-06：同URL新GET政策，更新/生效2026-09-04。派生`privacy.txt`322—354行：没有后台完整记录的集中查看入口；可按账号/类别/时段申请，经核验后15工作日内处理或说明。另有独立删除请求及保留例外，不能套Q20205“需注销”。经营者安全备份不等于用户可导回学习记录。
- E-BCD-07：复读2023-03-23原件`bcz-925.html`，文称拼错词会再现；没有错答提示、再答按钮或终点反馈。

上述原件和脚本/目录SHA见本轮manifest及追加采集建议。未将旧原件复读写成新网络采集；所有来源只支持其日期/构建范围。主学习链保持D1。
''')

common = dict(product_id='M-016', checked_on=DATE, app_build=None,
              firmware_version=None, source_updated_on=None, hands_on=False,
              supersedes=None)
new_e = [
    dict(common, evidence_id='E-BCCD-01', title='百词斩公开帮助：5类50问及数据/恢复边界',
         url='https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=bczApp',
         source_type='official_help', access_mode='opened_text',
         version_scope='官方/qa前端使用的bczApp帮助；无日期/build；逐题OS未知；爱阅读另列',
         locator='data.question_categories[0..4].questions；Q11/20/51/10/22等；热门8项重复',
         note='正文和目录已读；公开静态帮助GET，不是用户接口调用；Q49唯一图另列；新旧政策/导入/离线范围有冲突。',
         visual_observed=False, archived_artifact='artifacts/baicizhan-controls-deepening/source-notes.md',
         sha256=sha(A/'source-notes.md'), capture_state='archived_source_notes',
         raw_work_path='work/baicizhan-controls-deepening/qa-bczApp.json', raw_sha256=sha(W/'qa-bczApp.json'),
         provenance=[dict(capture_url=u, raw_work_path=str((W/n).relative_to(ROOT)), raw_sha256=sha(W/n), http_status=200)
                     for n,u in [('qa.html','https://www.baicizhan.com/qa'),
                                 ('qa-index.js','https://7n.bczcdn.com/baicizhan/h5/fe_qa/assets/index-CvNBM2bC.js')]]),
    dict(common, evidence_id='E-BCCD-02', title='Q49押韵模式官方教学合成图',
         url='https://vol.bczcdn.com/adv/8bc113e183b9952c6ac0ce8329cb266f.jpg',
         source_type='official_tutorial_image', access_mode='static_image_observed',
         version_scope='未标App构建/OS/发布日期；图内2023-06-22仅示例计划',
         locator='data.question_categories[0].questions[0] id49 answer[0]；上部步骤及下部模式页',
         note='实际看整张静态图；指定4本词书、修改计划选押韵/图片模式；无作答与执行反馈。',
         visual_observed=True, archived_artifact='artifacts/baicizhan-controls-deepening/qa-rhyme-mode.jpg',
         sha256=sha(A/'qa-rhyme-mode.jpg'), capture_state='selected_static_image',
         raw_work_path='work/baicizhan-controls-deepening/qa-rhyme-mode.jpg', raw_sha256=sha(W/'qa-rhyme-mode.jpg'),
         visual_audit_path='artifacts/baicizhan-controls-deepening/visual-audit.md', visual_audit_sha256=sha(A/'visual-audit.md'))]
(B/(PREFIX+'-evidence.jsonl')).write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in new_e))

common_f = dict(product_id='M-016', depth='D2', evidence_ids='E-BCCD-01', visual_status='not_observed',
                hands_on='false', case_path='cases/M-016-baicizhan.md', checked_on=DATE)
new_f = [
    dict(common_f, flow_id='F-M016-07', task='用铜板补学习日历打卡（未标版本帮助）',
         known_branches='Q10：我→我的日历→选日期→用铜板补卡；仅学习日历有效，不补小班或斩家活动',
         gaps='构建/OS与发布日期未知；铜板数、确认/失败/撤销及最终界面未给；补卡不证明新增学习或上传'),
    dict(common_f, flow_id='F-M016-08', task='将收藏词导出PDF（未标版本帮助）',
         known_branches='Q22：先收藏所需词到单词本，再点其底部导出PDF下载；不是任意整本词表打印承诺',
         gaps='收藏/下载成功反馈、输出字段/去重/保存位置、取消/失败/重试和导回未知；OS/build未标')]
write_csv(B/(PREFIX+'-coverage.csv'), list(next(iter(old_f.values())).keys()), new_f)

updates = {
 'F-M016-01': dict(evidence_ids=old_f['F-M016-01']['evidence_ids']+';E-BCCD-01;E-BCCD-02',
  known_branches='未标版本帮助补计划换书、已斩列表编辑恢复、资源下载、未同步前清缓存丢记录；完成计划与上传分开，停留10秒仅建议；Q49静态模式图',
  gaps='首学/复习完整错答→提示→再答→完成仍缺；即时误评撤销、恢复后队列/计数、离线题型和断点未知；FAQ无日期/构建且有政策/导入冲突', checked_on=DATE),
 'F-M016-04': dict(evidence_ids=old_f['F-M016-04']['evidence_ids']+';E-BCCD-01',
  known_branches='2026-09-04政策：后台完整记录无集中入口，可按账号/类别/时段邮件申请，经核验15工作日内处理或说明；与旧FAQ资料入口/需注销删除说法有差异',
  gaps='未实际申请；返回格式、导回/恢复、跨身份合并与冲突未知；安全备份不等于用户可恢复副本', checked_on=DATE),
 'F-M016-05': dict(task='学习题型开关（2023及未标版本帮助）',
  evidence_ids=old_f['F-M016-05']['evidence_ids']+';E-BCCD-01',
  known_branches='2023深度模式迁移公告给拼写设置；Q12另给我→设置→学习设置→增加学习题型开关；两者不自动对应当前构建',
  gaps='题型完整清单、保存/即时生效/在途队列未知；QA无日期/OS/build；当前官网深度模式与旧公告冲突未解', checked_on=DATE),
 'F-M016-06': dict(evidence_ids=old_f['F-M016-06']['evidence_ids']+';E-BCCD-01',
  known_branches='2022真题文中义、标记词表批量收藏；未标版本Q18另给开始背单词吧下方单词本→学习简单练习，不拼作同版本闭环',
  gaps='真题总入口、重复/取消收藏、原句保留、成功反馈、练习题型与调度未知；Q18不支持导入说法与功能介绍冲突', checked_on=DATE)
}
proposals=[]
for fid, changes in updates.items():
    for field, after in changes.items():
        before=old_f[fid][field]
        assert before != after
        proposals.append(dict(flow_id=fid, source_batch=f_sources[fid], field=field,
                              before=before, after=after, reason='同一任务补证；旧深度不变；仅建议合并'))
dump(W/'existing-coverage-updates.json',proposals)

captures=[]
capture_specs=[
 ('E-BCZ-01',W/'introductions.html','功能介绍4标题；能力声明无操作',None),
 ('E-BCZ-02',ROOT/'work/chinese-vocab-deepening/bcz.html','官网模式声明；本轮bcz-reused.txt', '2026-10-26'),
 ('E-BCZ-03',W/'app-store.html','TitledParagraph版本7.10.25/2026-10-19；兼容性；本轮version-index.json',None),
 ('E-BCZ-04',ROOT/'work/chinese-vocab-deepening/bcz-info.html','3.8.3实时同步、边走边听与离线两项；本轮bcz-info-reused.txt','2026-10-26'),
 ('E-BCZ-06',W/'privacy.html','更新/生效2026-09-04；查阅复制/删除；privacy.txt322—354行',None),
 ('E-BCD-07',ROOT/'work/chinese-vocab-deepening/bcz-925.html','2023-03-23拼写错误词再出现声明；本轮bcz-925-reused.txt','2026-10-26')]
for eid, raw, locator, original_date in capture_specs:
    e=old_e[eid]
    cap=dict(capture_url=e['url'], checked_on=DATE,
             access_mode='local_reread_of_existing_public_artifact' if original_date else 'ordinary_anonymous_GET',
             raw_work_path=str(raw.relative_to(ROOT)),raw_sha256=sha(raw), locator=locator,
             archived_artifact='artifacts/baicizhan-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),
             hands_on=False,visual_observed=False)
    if original_date:
        cap['original_capture_on']=original_date
        cap['limitation']='复读已存原件，未再次GET；不能以本轮查阅日冒充页面修订日'
    else:
        meta=json.loads(Path(str(raw)+'.capture.json').read_text())
        cap.update(http_status=meta['status'],final_url=meta['final_url'])
        cap['limitation']='公开HTML正文；未安装、未使用图片/评论补操作'
    if eid=='E-BCZ-03':cap.update(app_build='7.10.25（商店正文，未安装）',source_updated_on='2026-10-19')
    if eid=='E-BCZ-06':cap.update(source_updated_on='2026-09-04')
    before=e.get('additional_captures',[])
    captures.append(dict(evidence_id=eid,source_batch=e_sources[eid],field='additional_captures',
                         before=before,after=before+[cap],reason='同URL新采集或旧原件复读，复用E'))
dump(W/'existing-evidence-additional-captures.json',captures)

data=json.loads((W/'qa-bczApp.json').read_text())['data']
hot={q['id'] for q in data['hot_questions']}
flowmap={49:'01',38:'01',39:'01',18:'06',20:'01;04',21:'01',22:'08',23:'01',1:'04',2:'04',4:'04',6:'04',35:'04',37:'04',20205:'04',30144:'04',8:'01',9:'01',10:'07',11:'01',12:'05',13:'01',15:'01',36:'01',51:'01'}
gaps={
49:'构建未知；示例日期不是发布日期；题目与切换执行未见',38:'下载完成/取消及离线题型未知',39:'保存确认、跨词书状态与日量规则未知',18:'与功能介绍上传声明冲突；简单练习题型/调度未知',19:'无日期，不作为现行英音能力判断',20:'切网仅建议；缓存清除后恢复与绑定合并未知',21:'资源检测不证明学习断点恢复；囧记下线日期未知',22:'只给收藏词PDF出口；字段/保存/失败未知',23:'累计统计清空限制无日期；不同于删词书',
41:'内测范围/当前入口未知；发现与一起背措辞并存',42:'正文给自然周缺卡和手动解约不可撤回；未建社交任务',43:'只读发收口令路径，未发送或接受邀请',44:'内测范围不对应当前构建',45:'纸条不支持自定义声明无日期',46:'上新频率为计划，不是已发生记录',47:'升级/换网建议不保证接收成功',48:'申请需对方同意；未操作',
1:'资料位置与2026政策不同',2:'更换号码与旧账户数据合并未知',4:'绑定结果与跨身份记录合并未知',6:'退出后的缓存/待上传持久化未知',35:'注销说明不可恢复，不等于单独删除路径；未请求',37:'先具备另一登录方式；解绑不证明数据迁移',20205:'需注销删除措辞与2026政策独立请求冲突',30144:'旧客服路径不同于2026政策权限设置',
40:'仅入口和通知要求；内容播放/进度未读',7:'无日期，不作为现行夜间能力判断',8:'先切换才能删当前词书；记录不可恢复声明，重置范围未知',9:'铜板渠道与领取点；数额/到账失败/去重未知',10:'仅学习日历；花费、确认/余额不足/撤回未知',11:'已斩对象恢复，非即时误评撤销；队列和计数未知',12:'具体题型、保存时机/当前版本未知',13:'入口已给；列表筛选/搜索/返回状态未给',14:'无日期，不作为当前PC可用性判断',15:'修改日量/天数，算法与保存未知',17:'小班加入创建路径，未建社交任务/未操作',36:'已有或新增词书入口，保存与冲突未知',50:'只给更新入口，未更新/安装；当前构建未绑定',51:'北京时间截止且需上传；停留10秒不是回执'}
rows=[]
for ci,c in enumerate(data['question_categories']):
    excluded=c['name']=='爱阅读相关问题'
    rows.append(dict(inventory_id=f'BC-P{ci+1:02}',product_id='M-016',node_type='category',function_domain=c['name'],source_locator=f'data.question_categories[{ci}]',question_id='',evidence_ids='E-BCCD-01',flow_ids='',platform_version='bczApp公开帮助；无日期/OS/build',body_status='category_index_read',visual_status='not_observed',scope='爱阅读栏目，排除普通背词推论' if excluded else '百词斩App帮助',hot_duplicate='',remaining_gap='父目录，不计完成任务',checked_on=DATE))
    for qi,q in enumerate(c['questions']):
        qid=q['id']; blocks=[b for b in q['answer'] if len(b)==2]
        images=[v for k,v in blocks if k=='4']; texts=[v for k,v in blocks if k!='4']
        ids=';'.join('F-M016-'+i for i in flowmap.get(qid,'').split(';') if i)
        rows.append(dict(inventory_id=f'BC-Q{qid}',product_id='M-016',node_type='question',function_domain=q['question'],source_locator=f'data.question_categories[{ci}].questions[{qi}] id={qid}',question_id=qid,evidence_ids='E-BCCD-01'+(';E-BCCD-02' if qid==49 else ''),flow_ids=ids,platform_version='bczApp公开帮助；无日期/OS/build',body_status='image_only_observed' if qid==49 else ('text_read_images_unobserved' if images else 'text_read'),visual_status='static_reference_only' if qid==49 else 'not_observed',scope='爱阅读栏目，排除普通背词推论' if excluded else '百词斩App帮助',hot_duplicate='true' if qid in hot else 'false',remaining_gap='正文已读，产品栏目独立；未建立本轮任务/不套普通背词奖励' if excluded else gaps[qid],checked_on=DATE))
assert len(rows)==55 and len(hot)==8
write_csv(B/(PREFIX+'-inventory.csv'),list(rows[0].keys()),rows)
shutil.copyfile(W/'case-after.md',case)
dump(W/'build-result.json',dict(new_evidence=2,new_flows=2,inventory_rows=55,unique_questions=50,category_parents=5,old_flow_records=len(updates),old_flow_fields=len(proposals),old_evidence_captures=len(captures),visual_case_dependencies=baseline['visual_case_dependencies'],old_sources=sorted(set(f_sources.values())|set(e_sources.values()))))
print(json.dumps(json.loads((W/'build-result.json').read_text()),ensure_ascii=False))
