import csv, hashlib, json
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch53-integration';A=L/'artifacts/quizlet-audio-and-study';K=L/'artifacts/khan-intro-route-audit'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def rows(p):return list(csv.DictReader(p.open()))
def writecsv(p,rr):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rr[0]));w.writeheader();w.writerows(rr)
data=[
dict(n='01',slug='49085623511565-Studying-with-Podcasts',title='Studying with Podcasts',raw='01',lines='L17–64',order='39',flows='F-M021-18',note='生成到Library的文档链；不纠正源材料错误，播放/恢复未知'),
dict(n='02',slug='47540466130829-Studying-with-Audio-Notes',title='Studying with Audio Notes',raw='01',lines='L17–186',order='43',flows='F-M021-02;F-M021-12',note='音频录制/上传、继续本机草稿、错语言新建指南；不能由音频直接生成Practice Tests'),
dict(n='03',slug='42790350723725-Studying-with-Ask-Quizlet',title='Studying with Ask Quizlet',raw='01',lines='L15–42',order='45',flows='F-M021-19',note='解释到建议动作；最小化保留，离开重置；US14岁及以上，生成卡组可编辑'),
dict(n='04',slug='48158602540685-Using-Study-with-friends',title='Using Study with friends',raw='02',lines='L17–90',order='41',flows='F-M021-21',note='公开集会话、匿名一次、模式不同结果；会话成员可见最新成绩/对错词'),
dict(n='05',slug='47712424067341-Playing-Charms',title='Playing Charms',raw='02',lines='L17–70',order='42',flows='F-M021-20',note='匹配得分、答对加步、错答不加步、步耗尽结束；实际反馈/保存未知'),
dict(n='06',slug='41655677850509-Using-Quizlet-with-Superhuman',title='Using Quizlet with Superhuman',raw='02',lines='L19–31',order='46',flows='F-M021-01',note='首次账号授权、选文字/文档/符合条件标签页、进入Quizlet；不证明全页面访问'),
dict(n='07',slug='5649137275789-Studying-with-Quizlet-Expert-Solutions',title='Studying with Quizlet Expert Solutions',raw='02',lines='L17–72',order='51',flows='F-M021-22',note='教材章页题定位、App扫描与近期解答；专家答案和AI追加解释分开'),
dict(n='08',slug='360058606332-Searching-your-flashcard-sets-by-keyword',title='Searching your flashcard sets by keyword',raw='03',lines='L17–28',order='58',flows='F-M021-23',note='网站按Recent/Created范围查词返回包含内容的卡组；手机需浏览器')]
es=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];em={e['evidence_id']:e for e in es}
assert not (L/'batches/quizlet-audio-and-study-evidence.jsonl').exists()
assert not (L/'batches/quizlet-audio-and-study-coverage.csv').exists()
sources=[]
for d in data:
    d['eid']='E-QZAS-'+d['n'];d['url']='https://help.quizlet.com/hc/en-us/articles/'+d['slug']
    raw=W/f"quizlet-pages-{d['raw']}.txt"
    assert d['eid'] not in em and not any(e['url']==d['url'] for e in es)
    assert d['url'] in raw.read_text() and d['title'] in raw.read_text()
    sources.append(dict(evidence_id=d['eid'],title=d['title'],url=d['url'],body_locator=d['lines'],body_read_by_root=True,
        raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw),raw_capture_kind='web_tool_response_text_not_source_HTML',
        inventory_order=d['order'],flow_ids=d['flows'].split(';'),hands_on=False,visual_observed=False))
dump(A/'source-audit.json',dict(checked_on='2026-10-27',sources=sources,account_used=False,images_viewed=False,
    directory=dict(raw_work_path=str((W/'quizlet-directory.txt').relative_to(R)),sha256=sha(W/'quizlet-directory.txt')),
    operation='read_only_public_help',limitations='No account, recording, generation, playback, answering, invitations or actual app observation.'))
dump(K/'source-audit.json',dict(checked_on='2026-10-27',evidence_id='E-KHD-04',url=em['E-KHD-04']['url'],
    page=dict(raw_work_path=str((W/'khan-intro-page.txt').relative_to(R)),sha256=sha(W/'khan-intro-page.txt'),body_read=True),
    image_attempts=dict(raw_work_path=str((W/'khan-image-attempts.txt').relative_to(R)),sha256=sha(W/'khan-image-attempts.txt'),link_ids=[2,4,6],results=[403,403,403],route_stopped=True),
    visual_observed=False,video_played=False,video_downloaded=False,hands_on=False,
    correction='Original E-KHD-04 already read page text; only embedded video remained unread. Helper item 1 is not a globally unread page.'))
new=[]
for d,s in zip(data,sources):
    new.append(dict(evidence_id=d['eid'],product_id='M-021',title=d['title'],url=d['url'],checked_on='2026-10-27',source_type='official_help',
        access_mode='opened_text_via_web_tool',version_scope='en-US官方帮助，平台/资格逐条限定；精确App构建与帮助修订日未知',app_build=None,firmware_version=None,source_updated_on=None,
        locator=d['lines'],note=d['note'],hands_on=False,visual_observed=False,archived_artifact='artifacts/quizlet-audio-and-study/source-notes.md',sha256=sha(A/'source-notes.md'),supersedes=None,
        raw_work_path=s['raw_work_path'],raw_sha256=s['raw_sha256'],capture_state='saved_web_tool_response_no_source_HTML_no_images',
        artifact_hashes={str((A/'source-audit.json').relative_to(L)):sha(A/'source-audit.json')}))
(L/'batches/quizlet-audio-and-study-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in new))
fields=list(rows(L/'coverage.csv')[0])
flowdata=[
('18','生成音频节目并从资料库找回','E-QZAS-01','资料来源/提示→生成→Library；公开节目分享；源错误不自动纠正','播放控件、离线/断点、默认可见性、重生成和删源联动未知'),
('19','Ask Quizlet解释、建议动作及会话中断','E-QZAS-03','首页/集页进入、输入/上传、追问及建议动作；最小化保持、离开重置；US14岁及以上','真实动作回执、引文、对话与卡组持久化、刷新/退出恢复和撤销未知'),
('20','Charms匹配、答题加步与结束','E-QZAS-05','至少六词对；同色匹配、问题元素→答对加步、错不加步；步耗尽结束；选项/教程入口','初始步数、错题解释/重试、结算画面、暂停恢复/保存、黑白可区分性未知'),
('21','共享学习会话加入与群组结果','E-QZAS-04','公开卡组选六模式、分享/加入/完成/结果；匿名一次；成员可见最新结果；不同模式统计不同','移除成员/撤回、旧成绩保留、改私有、断线恢复、同步或异步规则未知'),
('22','教材解答定位与AI追加帮助','E-QZAS-07','教材/题目搜索、章页题定位、目录换题；App扫描、题文修正、近期Solutions；部分解答可用AI解释','无结果/拒权、答案纠错、历史保留、练习保存和专家核验范围未知'),
('23','限定资料范围的关键词查找','E-QZAS-08','网站Library选Recent或Created范围、输入词/短语、返回包含内容的卡组；手机用浏览器','零结果、分词/语言、精确匹配、单卡定位、删除后索引更新未知')]
flows=[]
for no,task,eids,branches,gaps in flowdata:
    f=dict(flow_id='F-M021-'+no,product_id='M-021',task=task,depth='D2',evidence_ids=eids,known_branches=branches,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-021-quizlet.md',checked_on='2026-10-27')
    flows.append({k:f[k] for k in fields})
writecsv(L/'batches/quizlet-audio-and-study-coverage.csv',flows)
changes=[]
updates={
'F-M021-01':('E-QZAS-06','；Superhuman Go首次授权，选中文字/文档/符合条件标签页，打开Quizlet进一步处理','；标签页资格、授权范围与保存/撤销未知'),
'F-M021-02':('E-QZAS-02','；音频录制/上传→处理→指南和卡组；上传中断留本机草稿可继续，错语言需新建重传','；录音中断/强退/卸载、逐字转写校订及重建后的旧对象关系未知'),
'F-M021-12':('E-QZAS-02','；音频帮助明确不能直接由音频生成Practice Tests','；不推导通过其他中间对象可绕过此输入限制')}
for p in (L/'batches').glob('*-coverage.csv'):
    rr=rows(p);dirty=False
    for r in rr:
        if r['flow_id'] not in updates:continue
        old=r.copy();eid,known,gap=updates[r['flow_id']];r['evidence_ids']+=';'+eid;r['known_branches']+=known;r['gaps']+=gap;r['checked_on']='2026-10-27'
        changes.append(dict(file=str(p.relative_to(R)),before=old,after=r.copy()));dirty=True
    if dirty:writecsv(p,rr)
assert len(changes)==3;dump(W/'old-flow-updates.json',changes)
changes=[]
for p in (L/'batches').glob('*-evidence.jsonl'):
    ee=[json.loads(s) for s in p.read_text().splitlines()];dirty=False
    for e in ee:
        if e['evidence_id'] not in ['E-QZCD-03','E-KHD-04']:continue
        before=json.loads(json.dumps(e))
        if e['evidence_id']=='E-QZCD-03':
            c=dict(checked_on='2026-10-27',access_mode='opened_text_via_web_tool',raw_work_path=str((W/'quizlet-directory.txt').relative_to(R)),raw_sha256=sha(W/'quizlet-directory.txt'),note='原Studying目录定位八篇未展开正文；不计新增功能目录',hands_on=False,visual_observed=False)
        else:
            c=dict(checked_on='2026-10-27',access_mode='opened_text_and_three_image_requests_returned_403',raw_work_path=str((W/'khan-intro-page.txt').relative_to(R)),raw_sha256=sha(W/'khan-intro-page.txt'),note='旧页正文复核，三个图片均403后停止；没有图像/视频观察或新任务',hands_on=False,visual_observed=False,archived_artifact='artifacts/khan-intro-route-audit/source-notes.md',sha256=sha(K/'source-notes.md'),artifact_hashes={str((K/'source-audit.json').relative_to(L)):sha(K/'source-audit.json')})
        e.setdefault('additional_captures',[]).append(c);changes.append(dict(file=str(p.relative_to(R)),before=before,after=e));dirty=True
    if dirty:p.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in ee))
assert len(changes)==2;dump(W/'old-evidence-updates.json',changes)
p=L/'batches/quizlet-controls-deepening-inventory.csv';rr=rows(p);byorder={d['order']:d for d in data};changes=[]
oldgames={'48':('Playing a game of Blocks','E-QZGM-02','F-M021-15'),'59':('Joining a game of Classic Quizlet Live','E-QZGM-04','F-M021-17'),'60':('Playing Match','E-QZGM-01','F-M021-14')}
for r in rr:
    if r['order'] in byorder:
        d=byorder[r['order']];assert r['functional_domain']==d['title'];old=r.copy()
        r.update(article_url=d['url'],evidence_ids=r['evidence_ids']+';'+d['eid'],flow_ids=d['flows'],coverage='D2部分；已读正文，未操作',missing_controls_or_branches=d['note'],checked_on='2026-10-27')
        changes.append(dict(kind='new_body',before=old,after=r.copy()))
    elif r['order'] in oldgames:
        title,eid,fid=oldgames[r['order']];assert r['functional_domain']==title;old=r.copy()
        r.update(article_url=em[eid]['url'],evidence_ids=r['evidence_ids']+';'+eid,flow_ids=fid,coverage='D2部分；回填既有第32批正文，非本批新取证',missing_controls_or_branches='原游戏案例已读；取消/恢复仍缺，Live仅加入流程',checked_on='2026-10-27')
        changes.append(dict(kind='stale_status_correction',before=old,after=r.copy()))
assert len(changes)==11 and len(rr)==68;writecsv(p,rr);dump(W/'inventory-updates.json',changes)
rpdata=[
('08','音频;上传;中断;恢复','F-M021-02','E-QZAS-02','Audio Notes / interrupted upload','上传尚未完成','Library找进行中草稿并继续上传','文档保留本机草稿入口','不证明卸载后可恢复或按字节续传','取消、重启、冲突与上传后处理失败'),
('09','语言;重生成;原始资料','F-M021-02','E-QZAS-02','Audio Notes / wrong language','处理时语言选错','下载原音频，另建指南按正确语言生成','文档要求重新创建','旧指南/卡组如何保留或关联未说明','覆盖校订、进度迁移和删源联动'),
('10','播客;生成;来源;校验','F-M021-18','E-QZAS-01','Podcasts / create and tips','有来源材料','选择来源与提示后生成，在Library找回','文档给生成到收听入口','未给播放位置或离线持久状态','输入错误不自动修正；重生成、删除和恢复'),
('11','AI;会话;最小化;返回','F-M021-19','E-QZAS-03','Ask Quizlet / Tips','符合地区年龄条件且在对话中','最小化或离开对话','前者保持会话，后者重置','对话重置与生成卡组保存不可混同','刷新、历史、撤销和真实写入回执'),
('12','游戏;奖励;错答;结束','F-M021-20','E-QZAS-05','Charms / play and tips','至少六词对','匹配问题元素后作答','答对加步，错不加步；耗尽结束','没有存盘/进度继承依据','错题讲解重试、结算与黑白显示'),
('13','共享;结果;隐私;学习统计','F-M021-21','E-QZAS-04','Study with friends / results','公开卡组的共享会话','选模式、加入并完成活动','模式决定分数榜或词项结果；成员看到最新结果','历史记录与退组后结果未交代','撤回、改私有、断线和统计口径'),
('14','AI;授权;上下文;页面','F-M021-01','E-QZAS-06','Superhuman Go / agent steps','写作环境，首次需Quizlet账号授权','选文字/文档/符合条件标签页生成','打开Quizlet查看完整集','授权和内容选择不证明持久写入','标签页资格、权限范围、重复生成和撤销'),
('15','解答;AI解释;来源;校验','F-M021-22','E-QZAS-07','Expert Solutions / search and tips','教材/题目及对应资格','定位章页题，按支持项追加AI帮助','题库解答与AI解释是不同来源环节','近期入口不证明完整历史或练习保存','专家核验是否覆盖AI输出、纠错和无结果'),
('16','搜索;范围;内容;零结果','F-M021-23','E-QZAS-08','Keyword / Library steps','网站资料库','选择Recent或Created后查词','返回包含词/短语的卡组','未给单卡位置或索引更新规则','不能推导全站无内容；分词/删除更新未知'),
('17','音频;生成;能力边界','F-M021-02;F-M021-12','E-QZAS-02','Audio Notes / settings','音频材料输入','选择生成学习指南','文档提供指南/卡组，排除音频直接生成Practice Tests','不能据其他生成入口假设同样支持音频','中间对象再输入是否可行未验证')]
p=L/'reference-points.csv';rr=rows(p);fields=list(rr[0]);existing={r['reference_id'] for r in rr}
for no,tags,fids,eids,loc,pre,act,result,persist,unknown in rpdata:
    rid='RP-M021-'+no;assert rid not in existing
    r={k:'' for k in fields};r.update(reference_id=rid,product_id='M-021',question_tags=tags,flow_ids=fids,evidence_ids=eids,source_locator=loc,
        artifact_path='artifacts/quizlet-audio-and-study/source-notes.md',precondition=pre,action=act,visible_result=result,evidence_basis='official_help_text_reconstruction; no visual observation or actual operation',
        persistence_result=persist,unknown_branches=unknown,version_scope='2026-10-27官方帮助；平台/资格按正文，构建未知',checked_on='2026-10-27');rr.append(r)
writecsv(p,rr)
dump(W/'integration-summary.json',dict(new_evidence=8,new_flows=6,old_flows_updated=3,old_captures=2,new_body_inventory_rows=8,stale_game_inventory_rows=3,new_reference_points=10,
    corrections=['Khan E-KHD-04 page body already read; only video unobserved','Quizlet old game rows updated from existing E-QZGM evidence; not new reading'],new_actual_operations=0))
print('Applied: 8 E, 6 D2 tasks, 3 existing F, 2 captures, 11 inventory rows (8 new bodies + 3 old corrections), 10 RP.')
