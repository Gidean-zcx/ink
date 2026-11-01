import csv, json, hashlib, shutil, zipfile
from pathlib import Path
from datetime import datetime, timezone

root=Path('/home/workspace/reader'); lib=root/'docs/research/interaction-library'
work=root/'work/jiakao-controls-deepening'; raw=work/'browser'
art=lib/'artifacts/jiakao-controls-deepening'; browser=art/'browser'
browser.mkdir(parents=True,exist_ok=True)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x): p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
steps=[
('01-initial','打开官方顺序练习，HTTP200','匿名页有下载App提示层；背后第1/100题，对错均0'),
('02-dismiss-promo','点击提示层关闭(920,234)','显示题目、收藏、前后题、详解、答题卡；自动下一题已勾选'),
('03-wrong-answer','第1题点B，等700ms','错题计数1，停留第1题；解析没有自动展开'),
('04-reselect-after-wrong','同题再点A，等350ms','首次错答计数未改变，A蓝勾/B红叉'),
('05-explanation','点击显示详解','详解展开，按钮改收起；含说明及法规引用，未审计法律内容'),
('06-favorite','点击收藏','出现手机号/短信验证码登录框；未登录，收藏成功未验证'),
('07-answer-card','关闭登录框，展开答题卡','显示1–100题号，第1题红色'),
('08-question-two','点击下一题','第2/100题；对0/错1；此前展开的详解与答题卡状态仍在'),
('09-correct-auto-next','第2题选择C，等1200ms','前进第3题，对1/错1、50%；第2题绿色'),
('10-auto-next-disabled','取消自动下一题、收起答题卡和详解；定位不存在的A正确超时后，按实际题面点A选项','对2/错1、67%，仍第3题；错误定位没有作答'),
('11-return-answered','点上一题','回第2题，已选C与对2/错1仍在，自动下一题未勾选'),
('12-reload','刷新当前带题目ID的网址，等1200ms','下载提示重现；截图时题面尚未完成，不能据此判断题目丢失'),
('13-cloud-sync-gate','关闭下载提示，点击云同步做题进度','登录框；背后第2题已显示，对错均0，选项次序已改变'),
('14-reload-settled','关闭登录框，再等1500ms','仍第2题但无已选项；对错均0，自动下一题恢复勾选；同题驾驶证从C变A'),
('15-kemu-one-menu','点小车科目一面包屑','进入北京科一目录；原顺序页头部是全国，范围变化须保留'),
('16-mock-exam-entry','点目录的模拟考试链接，等1500ms','当次截图仍为目录，不能声称已开始考试'),
('17-mock-new-tab','检查上下文页列表并再截帧','只有原页；文件名沿用当时假设，并非真的打开新标签；此冗余帧未作视觉结论'),
('18-mock-after-timeout','再次点链接仍无跳转；直接打开页面上已有链接，5秒/45秒导航等待超时，随后截帧','最终已显示北京模拟页及考试规则确认框；45:00。超时不等于内容始终未到'),
('19-mock-start','点规则框确定，等1200ms','框关闭，计时44:59，实际倒计时已开始'),
('20-mock-wrong','第1题点B，等600ms','第1题红色，自动前进第2题，计时44:53'),
('21-mock-reselect','在已经前进的第2题点A，等300ms','第2题绿色，自动前进第3题；这是第二次独立作答，文件名不代表改答第一题'),
('22-submit-incomplete','仅作答2题时点交卷','出现确定/取消确认框，未显示未答数量；计时44:44'),
('23-cancel-return-reselect','取消交卷，上一题回第2题；尝试定位B控件超时后重新截帧','显示回答正确/正确答案及为什么；选项作答按钮已不在可见控件中，原结果保留'),
('24-submitted-result','再次交卷并确定，等1200ms','转到考试结果URL，中心内容空白；URL参数含答对1/错1/未答98，不等于可见成绩或云保存'),
('25-result-after-wait','再显式等5秒后截帧','中心仍空白；没有观察到成绩、分析或再做一套，结算反馈未闭合')]
selected={2,4,5,6,7,9,10,11,14,18,19,20,21,22,23,25}
states=[]
for index,(name,action,result) in enumerate(steps,1):
    src=raw/(name+'.png'); meta=json.loads((raw/(name+'.json')).read_text())
    row=dict(capture=name,action=action,observed_result=result,url=meta['url'],captured_at_utc=datetime.fromtimestamp(src.stat().st_mtime,timezone.utc).isoformat(),raw_image_path=str(src.relative_to(root)),sha256=sha(src),image_reviewed=index!=17)
    if index in selected:
        dst=browser/src.name; shutil.copy2(src,dst); row['archived_image']=str(dst.relative_to(lib))
    states.append(row)
dump(browser/'observed-states.json',states)
with zipfile.ZipFile(raw/'trace.zip') as z:
    events=[json.loads(x) for x in z.read('trace.trace').decode().splitlines()]
before={x['callId']:x for x in events if x['type']=='before'}
after={x['callId']:x for x in events if x['type']=='after'}
actions=[]
for cid,e in before.items():
    if e['method'] not in ['goto','click','mouseClick','uncheck','reload','screenshot','waitForTimeout']: continue
    a=after.get(cid,{})
    actions.append(dict(method=e['method'],class_name=e['class'],params=e.get('params',{}),start_ms=e['startTime'],end_ms=a.get('endTime'),error=a.get('error')))
dump(browser/'trace-actions.json',dict(origin_trace_sha256=sha(raw/'trace.zip'),context_metadata=next(x for x in events if x['type']=='context-options'),actions=actions))
dump(browser/'raw-trace-manifest.json',dict(path=str((raw/'trace.zip').relative_to(root)),sha256=sha(raw/'trace.zip'),bytes=(raw/'trace.zip').stat().st_size,screenshots_in_trace=False,snapshots_in_trace=False,formal_screenshots=len(selected)))
audit=art/'browser-operation-audit.md'
lines=['# M033：公开练习网页操作核查','','观察日：2026-10-26。E-JKCD-01、E-JKCD-02、E-JKCD-03、E-JKCD-04；F-M033-01、F-M033-02、F-M033-03、F-M033-04、F-M033-07。','',
'根代理用Playwright操作公开网页。`hands_on=true`、D4仅表示所列桌面网页任务已实际尝试和观察；不代表人手触摸、原生App、整个任务成功或全功能完成。F02错题/收藏主链仍D1，F04实际云同步仍仅文档D2；看见登录框不升级它们。','',
'环境：Linux，Chromium Headless Shell 156.0.8078.4，1280×950，新建匿名上下文。Playwright分发包1.64.1，trace内标1.64.0-beta-1791568591000。网站前端构建和题库版本未知。顺序页初始头部全国，面包屑转北京目录后模拟页为北京；不把它们当原生App全国题库。主序列一次，没有离线、重启、多账户、稳定性或延迟测量。','',
'[顺序练习](https://www.jiakaobaodian.com/mnks/exercise/0-car-kemu1.html)正常浏览器访问HTTP200，补充此前直接HTTP403/搜索索引的访问记录。只答3道练习与2道匿名模拟题，没有正式考试、报名、账号登录、验证码发送或用户投稿。画面里的题目只作为控件和状态取证，不复制题库、不审计法律答案。','',
'## 顺序记录','',
'下列25个状态都留有原始捕获及散列；16个必要截图正式保存。其余原件留work，不能称为全部截图已长期归档。17号只检查结构状态，没有独立视觉判读。','',
'| 捕获 | 动作 | 可见结果及限制 |','| --- | --- | --- |']
for index,s in enumerate(states,1):
    name=s['capture']; label=f'[{name}](browser/{name}.png)' if index in selected else name+'（原件work）'
    lines.append(f'| {label} | {s["action"]} | {s["observed_result"]} |')
lines += ['', '## 失败与结果边界','',
'共4次操作超时：错误地在单选题查找“A、正确”（5秒）；打开模拟页等待DOMContentLoaded两次（5秒、45秒）；返回已答题时尝试点击已消失的B按钮（5秒）。没有强制点击、改时钟、直接调用题库接口或修改页面业务状态。导航超时后页面最终可见，故同时保留超时和之后状态。两次目录点击没有立刻导航；没有观察到原生dialog或新标签，原因未查明。','',
'原始文件名是当时的操作意图，不是结论：17-mock-new-tab没有新标签；21-mock-reselect实际回答第二题；24-submitted-result只有结果页面外壳。正式描述以截图和日志为准。根代理两次过早读取尚未产生的截图文件失败，随后读取实际产物；不是产品界面失败。','',
'练习中答错停留、答对前进受开关控制；考试中本次错题和正确题均自动前进。首次交卷框只有通用确认，没有逐项提示98题未答。取消后能回到已答题查看结果；再次确认后到结果网址但中心空白，显式追加5秒仍未出现成绩。地址参数不足以证明已显示成绩、已保存记录或云端提交成功。到零自动交卷、完整100题、离线和恢复仍未知。','',
'刷新对照只执行一次：同一带ID网址回第2题，作答/计数重置、自动下一题重新勾选；选项文本与字母映射变化。不能由此推断所有匿名路径、账户用户或原生App都会如此，也未检查底层存储机制。','',
'正式保留[观察状态](browser/observed-states.json)、[动作日志](browser/trace-actions.json)和[原trace散列](browser/raw-trace-manifest.json)。trace未启用连续快照/截图，正式PNG是显式截帧；未归档视频。重现时须重新核对每题文本和选项，不能盲按字母；实验只检查反馈，不用于评价考试能力。','']
audit.write_text('\n'.join(lines))
evidence=[]
for i,title,url,loc,note in [
(1,'匿名顺序练习错误反馈、解析与自动前进','https://www.jiakaobaodian.com/mnks/exercise/0-car-kemu1.html','01–11','错答停留，解析手动展开，重选未改首次成绩；对答自动前进可关闭；不代表App。'),
(2,'收藏和云同步的游客登录前置','https://www.jiakaobaodian.com/mnks/exercise/0-car-kemu1.html','06、13','两入口实际要求手机号验证码登录；未登录，未完成收藏或同步。'),
(3,'匿名练习刷新后的题目与作答状态','https://www.jiakaobaodian.com/mnks/exercise/0-car-kemu1.html','11–14','同题ID定位保留，但已选项、对错计数、自动下一题设置未恢复，选项字母映射改变；单次观察。'),
(4,'北京模拟考试计时、取消交卷及空白结果页','https://www.jiakaobaodian.com/mnks/exam/car-kemu1-beijing.html','15–25','倒计时运行；错与对均前进；未完成交卷确认可取消；已答题不能再选；确认后到结果网址但未见成绩。')]:
    evidence.append(dict(evidence_id=f'E-JKCD-{i:02}',product_id='M-033',title=title,url=url,checked_on='2026-10-26',source_type='public_web_application',access_mode='automated_public_browser_actions_and_screenshot_review',version_scope='Desktop anonymous official web; practice national header / mock Beijing; Chromium156.0.8078.4; no native app test; build unknown',app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=True,visual_observed=True,visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=str(audit.relative_to(lib)),operation_audit_sha256=sha(audit),archived_artifact=str(audit.relative_to(lib)),sha256=sha(audit),capture_state='operation_audit_actions_and_selected_screenshots_archived',supersedes=None,artifact_hashes={str(p.relative_to(lib)):sha(p) for p in browser.iterdir() if p.is_file()}))
(lib/'batches/jiakao-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in evidence))
fields=list(csv.DictReader((lib/'coverage.csv').open()).fieldnames)
new=dict(flow_id='F-M033-07',product_id='M-033',task='匿名网页刷新与练习状态恢复',depth='D4',evidence_ids='E-JKCD-03',known_branches='同题ID网址保留题目位置；刷新后计数/选答清空、自动下一题重新勾选；选项字母映射改变。',gaps='单次桌面网页；存储机制、断网、进程重启、多端及App恢复未知。',visual_status='observed_browser_sequence',hands_on='true',case_path='cases/M-033-jiakaobaodian.md',checked_on='2026-10-26')
with (lib/'batches/jiakao-controls-deepening-coverage.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n'); w.writeheader(); w.writerow(new)
# Update the same stable tasks in the owning old batch, without duplicating IDs.
path=lib/'batches/chinese-apps-coverage.csv'; rows=list(csv.DictReader(path.open()))
updates={
'F-M033-01':dict(depth='D4',evidence_ids='E-JKB-02;E-JKB-03;E-JKCD-01',known_branches='网页错答停留、解析手动展开；改点正确不改首次记录；正确自动前进可取消；返回保留当次选答。',gaps='只核匿名顺序网页三题；App题型、声音、错题集合再练、离线及更多分支未知。',visual_status='observed_browser_sequence',hands_on='true'),
'F-M033-02':dict(evidence_ids='E-JKB-02;E-JKB-03;E-JKB-04;E-JKCD-02',known_branches='错题/收藏入口；匿名网页点收藏出现登录框，未完成收藏。',gaps='登录后加入/移出、错题再练、撤销与题目更新未知；入口前置观察不等于收藏链完成。'),
'F-M033-03':dict(depth='D4',evidence_ids='E-JKB-05;E-JKCD-04',known_branches='北京匿名模拟页倒计时运行；错/对均前进；交卷通用确认可取消；返回已答题显示结果；确认后结果中心空白。',gaps='未见成绩/复盘/再做一套；完整100题、到零、中断恢复、记录保存与App未知。',visual_status='observed_browser_sequence',hands_on='true'),
'F-M033-04':dict(evidence_ids='E-JKB-04;E-JKCD-02',known_branches='原文档App同步→立即同步→Web返回查看；匿名网页点云同步实际先要求登录。',gaps='未登录或同步；成功提示、冲突、重装后恢复仍未知。')}
changes=[]
for row in rows:
    fid=row['flow_id']
    if fid in updates:
        assert row['depth']==('D2' if fid.endswith('04') else 'D1'),fid
        changes.append(dict(flow_id=fid,before=dict(row),updates=updates[fid])); row.update(updates[fid])
assert len(changes)==4
dump(work/'applied-existing-coverage-updates.json',changes)
with path.open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n'); w.writeheader(); w.writerows(rows)
print(json.dumps(dict(new_evidence=4,new_flows=1,old_flows_updated=4,captured_states=25,archived_screenshots=len(selected),actions=len(actions),errors=sum(bool(x['error']) for x in actions))))
