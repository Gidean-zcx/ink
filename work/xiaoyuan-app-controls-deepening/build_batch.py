from pathlib import Path
import csv,io,json,hashlib,shutil,copy,re
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/xiaoyuan-app-controls-deepening';B=L/'batches';A=L/'artifacts/xiaoyuan-app-controls-deepening'
def load(p):return json.loads(p.read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def wc(p,rows,fields=None):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields or list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
assert not A.exists(),'one-time build'
old=load(W/'coverage-before.json');oldes=load(W/'evidence-before.json');ims=load(W/'image-captures.json');caps=load(W/'captures.json')
assert len(ims)==35 and all(x['status']==200 and sha(R/x['raw_work_path'])==x['raw_sha256'] for x in ims)
A.mkdir(parents=True);observed=[]
for x in ims:
 p=A/Path(x['raw_work_path']).name;shutil.copyfile(R/x['raw_work_path'],p)
 observed.append(dict(block=x['block'],index=x['index'],url=x['url'],artifact=str(p.relative_to(L)),sha256=sha(p),raw_work_path=x['raw_work_path'],observed_on='2026-10-27'))
oldims=[]
for eid,name in [('E-XYCD-02','xy-math-error.png'),('E-XYCD-03','xy-dictation.png'),('E-XYCD-04','xy-speaking.png')]:
 e=next(x for x in oldes if x['evidence_id']==eid);p=A/name;q=R/'work/chinese-app-depth'/name;shutil.copyfile(q,p)
 oldims.append(dict(evidence_id=eid,url=e['url'],artifact=str(p.relative_to(L)),sha256=sha(p),raw_work_path=str(q.relative_to(R)),observed_on='2026-10-27'))
note=(W/'observations.md').read_text()+'\n## 正式图像归档\n\n以下均已实际查看，仅静态；同一作者来源的派生材料不增加独立验证数量。\n\n| 定位 | 图像 | SHA256 |\n| --- | --- | --- |\n'
for x in observed+oldims:note+=f"| {x.get('evidence_id',str(x.get('block'))+'-'+str(x.get('index')))} | [{Path(x['artifact']).name}]({Path(x['artifact']).name}) | `{x['sha256']}` |\n"
N=A/'source-notes.md';N.write_text(note);dump(W/'observed-artifacts.json',observed+oldims)
groups=[(5,'口算入口、草稿、退出与结果',2),(6,'同步练的选择与下一题',2),(7,'竖式的逐列输入与显式完成',2),(8,'单位换算的手写输入',2),(9,'听算的语音回答与错答',7),(12,'字词听写模式、输入与结果',5),(13,'趣味识字的学读练与字卡',8),(14,'背古诗的提示、重背与成绩',9),(15,'语文练习的题目入口',None),(16,'练字挑战的描写入口',None),(19,'背单词的多阶段作答与重练',10),(20,'单词听写的横屏输入',5),(21,'口语分支与单词结果',6)]
es=[];byblock={}
for i,(b,title,f) in enumerate(groups,1):
 selected=[x for x in observed if x['block']==b];eid=f'E-XYAC-{i:02}';byblock[b]=eid
 es.append(dict(evidence_id=eid,product_id='M-028',title=title+'：历史作者静态拼图',url=selected[0]['url'],checked_on='2026-10-27',source_type='firsthand_author_static_composites',access_mode='public_static_image_GET_and_viewed',version_scope='小猿AI手机软件；作者2025-05-12体验，构建及平台未核定',app_build=None,firmware_version=None,source_updated_on=None,source_published_on='2025-05-12',locator=f'author subsection block {b}; image indices '+','.join(str(x['index']) for x in selected),note='正文与选择的静态拼图局部重建；非本项目连续操作，规则缺口见独立摘记',hands_on=False,visual_observed=True,visual_kind='static_third_party_reference',archived_artifact=str(N.relative_to(L)),sha256=sha(N),capture_state='formal_images_and_independent_notes',supersedes=None,derived_from='E-XYCD-01',observed_artifacts=selected))
existing=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines() if x]
assert not {e['evidence_id'] for e in es}&{e['evidence_id'] for e in existing}
assert not {e['url'] for e in es}&{e['url'] for e in existing}
(B/'xiaoyuan-app-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
changes={
1:dict(known_branches='FAQ解析/VIP与错题关系；检查历史保留，非同题连续链',gaps='拍照识别纠错、提示、再练及错题状态全链；未借历史练习补纸面检查'),
2:dict(evidence_ids='E-XYAI-03;E-XYCD-01;E-XYCD-02;E-XYAC-01;E-XYAC-02;E-XYAC-03;E-XYAC-04',known_branches='口算重写/跳过、独立草稿撤销/重做、退出确认；竖式显式完成；同步练下一题；结果完成可仅部分答对',gaps='各题型规则不可互换；识别误判修订、保存/强退恢复未知；题量200只正文非可见菜单'),
3:dict(known_branches='文本/语音/图像、历史与单条反馈；撤回OS权限不同于删除既有记录',gaps='生成失败重试、单轮删除/恢复、反馈撤销；不证明系统级AI读取所有App'),
4:dict(known_branches='旧商店活动与同年级知识点；独练周联赛/金币图不当PK规则',gaps='PK结算奖励、退出掉线及赛季常设性未知；当前3.145.0日志无具体补证'),
5:dict(task='字词听写：在线、纸笔、自由输入及结果',evidence_ids='E-XYCD-01;E-XYCD-03;E-XYAC-06;E-XYAC-12',known_branches='中文提交前清写、提交后正确写法/下一项；课文/范围与声音/间隔/次数/顺序；纸笔和自由输入；英文只横屏输入局部',gaps='撤回、误判修订、强退恢复未知；中文错误规则不套英文；完成数非答对数'),
6:dict(evidence_ids='E-XYCD-01;E-XYCD-04;E-XYAC-13',known_branches='示范/录音与单词星级结果；作者述首次失败重试再次前进；句子/情景仅入口',gaps='评分阈值、噪声漏录、录音回放内容/保存与中断未知；拼图题号不连续')}
updates=[]
for x in old:
 y=copy.deepcopy(x);y.update(changes[int(x['flow_id'][-2:])],checked_on='2026-10-27');updates.append(dict(flow_id=x['flow_id'],before=x,after=y))
dump(W/'existing-coverage-updates.json',updates)
new=[]
for num,task,evid,branches,gaps,visual in [
(7,'听算：听题、语音回答与有限时重试','E-XYCD-01;E-XYAC-05','选范围/题量→听题/重播→语音→对错；作者述倒计时内可再试','超时、噪声误判、拒麦、完成及中断保存未知；未测音频','static_reference_only'),
(8,'趣味识字：学读练、反馈与字卡','E-XYCD-01;E-XYAC-07','单元地图→学/读/练；示范/录音星级，选字/缺字；字卡与继续','解锁门槛、答错/跳过、重试上限和退出恢复未知；无儿童实测','static_reference_only'),
(9,'背古诗：提示、重背与录音成绩','E-XYCD-01;E-XYAC-08','选诗开始→呈字/提示/重背→错字/提示次数/用时/分数→回放或再背','提示扣分公式、误识别修正、漏录、录音删除/导出和恢复未知','static_reference_only'),
(10,'背单词：学习、辨认、提交与错题重练','E-XYCD-01;E-XYAC-11','计划新学/复习→释义图音→选图/释义提交→反馈继续→重练→统计；补字母仅作者文字','首次与重练正确率口径、复习调度、改计划数据与退出保存未知；未证学习效果','static_reference_only'),
(11,'申请注销手机软件账号与学习记录','E-XYAI-04','政策我→设置→关于→注销→确认→二次确认；列检查/练习/榜/金币范围','未执行；失败/撤回/恢复及具体时限未知；关联产品需分别处理','not_observed'),
(12,'获取个人资料的截图副本','E-XYAI-04','政策我→头像框→截屏；资料访问/副本局部路径','非完整学习记录导出；格式、范围、回灌/备份恢复未知，也不证明全产品没有其他导出','not_observed')]:
 new.append(dict(flow_id=f'F-M028-{num:02}',product_id='M-028',task=task,depth='D2',evidence_ids=evid,known_branches=branches,gaps=gaps,visual_status=visual,hands_on='false',case_path='cases/M-028-xiaoyuan-ai.md',checked_on='2026-10-27'))
wc(B/'xiaoyuan-app-controls-deepening-coverage.csv',new)
cu=[]
for name,eid in [('home','E-XYAI-01'),('faq','E-XYAI-02'),('child-policy','E-XYAI-03'),('privacy','E-XYAI-04'),('store','E-XYAI-05')]:
 c=copy.deepcopy(next(x for x in caps if x['name']==name));c={k:v for k,v in c.items() if k not in ('links','images')};c.update(archived_artifact=str(N.relative_to(L)),sha256=sha(N),visual_observed=False,hands_on=False)
 if name=='store':c.update(app_build='3.145.0',source_updated_on='2026-10-26',note='最新版本datetime；不改旧快照构建，不采用户评论，不当具体功能验证')
 cu.append((eid,c))
c=load(W/'historical-local-capture.json');c.update(archived_artifact=str(N.relative_to(L)),sha256=sha(N),note='原HTML本地复读；13小节正文和选图审查，图片另登记派生证据');cu.append(('E-XYCD-01',c))
for x in oldims:cu.append((x['evidence_id'],dict(checked_on='2026-10-27',access_mode='existing_static_image_local_review',observed_artifact=x['artifact'],observed_sha256=x['sha256'],archived_artifact=str(N.relative_to(L)),sha256=sha(N),raw_work_path=x['raw_work_path'],raw_sha256=x['sha256'],visual_observed=True,hands_on=False,note='旧图重看及正式归档，沿用E，不新增URL/独立来源')))
props=[]
for eid,c in cu:
 e=next(x for x in oldes if x['evidence_id']==eid);before=e.get('additional_captures',[]);props.append(dict(evidence_id=eid,before=before,after=before+[c]))
dump(W/'existing-evidence-additional-captures.json',props)
rows=[]
def row(title,loc,ev,flows='',status='static_partial_control',gap='仅来源局部，未操作当前版本'):
 rows.append(dict(item_id=f'XYAC-{len(rows)+1:03}',product_id='M-028',item=title,source_locator=loc,evidence_ids=ev,flow_ids=flows,status=status,gap=gap,checked_on='2026-10-27'))
for title in ['拍照搜题','检查作业','答疑讲解','专项练习','作文批改','同步学练','错题本','备考工具','班级功能']:row(title,'官网九大核心功能','E-XYAI-01',status='official_domain_parent',gap='宣传功能域，不是完整机内菜单；性能未验证')
faq=(W/'raw/faq.txt').read_text().splitlines()
for line in faq:
 if line.endswith('？'):row(line,'官方FAQ','E-XYAI-02',status='official_FAQ_item',gap='功能/会员/范围说明，未逐屏操作')
controls={
5:['数学页16个可见入口','年级/教材/单元','题量菜单可见10/20/30/60/100','知识点竞速榜','打印及外部输出入口','手写识别/重写/跳过','草稿清空/撤销/重做/关闭','退出继续/离开确认','今日任务金币','周经验荣誉联赛','完成/答对数/正确率分开','分享/再练'],
6:['单元与判断/选择题','连续读题/朗读本题','错后下一题；解析只正文','题目反馈/草稿'],7:['逐列输入/重置','完成灰/亮状态','跳过/返回'],8:['比较/计算分支','手写比较符号','跳过/草稿'],9:['范围/数量/开始','听题/重播/跳过','语音与对错标记','倒计时内重试仅作者文字'],
12:['课文/范围逐项及全选','在线手写/重播/清写/提交','纸笔上一项/暂停/下一项','自由内容空格分词','每项4汉字/200字词提示','童声/教师声','5/10/15秒间隔','1/2/3次重复','顺序/乱序','正确写法/关闭/知道啦/下一项','完成数/答对数/错题本/重练'],
13:['单元/字卡地图与锁','学/读/练切换','学的字形/图示','读的示范/录音/星级','练的选字/词组缺字','字卡获得/分享/继续'],
14:['检索/年级/课内外','背诵入口/逐步呈字','重背/提示/时长','错字/提示次数/用时/分数','录音播放/再背','诗词PK仅入口'],
15:['课文列表/选择题','朗读/连续读题','跳过/题目反馈；结果缺'],16:['字词学习/每日挑战','描写字形/笔画/拼音','重写/跳过；结果缺'],
19:['计划新学/复习/更换','释义/发音/图片/继续','选择图片','释义选择/提交灰亮','答题反馈后继续','补字母仅正文','错题重练','经验/时间/正确率统计','记忆法标签不证调度算法'],20:['单元/模式/打印','横屏多词输入','重播/清写/下一词；结果缺'],21:['单词/句子/情景模式入口','单词示范/录音','星级/再试；两次规则仅文字','词级回放/色标/星级成绩','下一步句子跟读仅入口']}
for b,title,f in groups:
 eid=byblock[b];flow=f'F-M028-{f:02}' if f else '';row(title,f'作者13小节 block{b}',eid,flow,'source_subsection_parent')
 for t in controls[b]:row(t,f'block{b}; 具体图号见source-notes',eid,flow)
for title,flow in [('免重复注册与关联产品分别注销','F-M028-11'),('资料修改与头像框','F-M028-12'),('撤回系统相机/麦克风权限','F-M028-03'),('客服删除/备份更新时删除','F-M028-11'),('注销双确认与学习记录范围','F-M028-11'),('头像框截图副本','F-M028-12')]:row(title,'旧隐私政策第六节及第97行','E-XYAI-04',flow,'policy_documented_path','未实操；不等于进度迁移、全量导出或恢复')
row('3.145.0 / 2026-10-26','当前商店HTML time datetime','E-XYAI-05',status='version_metadata',gap='不能给历史图定构建；泛化更新不作功能证据')
row('首页xyst政策新链接','首页footer，网页打开内部错误','E-XYAI-01',status='unread_link',gap='未取得新正文；旧政策结论限定2025-08-18旧URL')
wc(B/'xiaoyuan-app-controls-deepening-inventory.csv',rows)
# Keep stable source inventory entries, updating only review status and task links.
ip=L/'inventories/M028-historical-practice-inventory.csv';before=ip.read_bytes();(W/'historical-inventory-before.csv').write_bytes(before);ir=list(csv.DictReader(io.StringIO(before.decode())))
for x,(b,title,f) in zip(ir,groups):
 x['review_status']='text_and_selected_static_partial_task_analysis' if f else 'text_and_static_controls_core_result_unknown';x['related_flow_ids']=f'F-M028-{f:02}' if f else '';x['checked_on']='2026-10-27';x['evidence_id']='E-XYCD-01;'+byblock[b]
wc(ip,ir);dump(W/'historical-inventory-update.json',dict(path=str(ip.relative_to(R)),before_sha256=hashlib.sha256(before).hexdigest(),after_sha256=sha(ip),rows=len(ir)))
case=(W/'case-before.md').read_text().replace('查阅日：2026-10-26。','初查2026-10-26，续查2026-10-27。',1)
case=case.replace('iOS 商店查阅时列 3.143.2；','初查iOS商店列3.143.2；本轮新快照列3.145.0，发布日期2026-10-26；')
case=case.replace('商店列 3.143.2，未实机核对。','初查列3.143.2，本轮快照列3.145.0，均未实机核对。')
case=case.replace('根代理实际看过三张静态拼图','初查根代理实际看过三张静态拼图')
case=case.replace('采用口算、字词听写、单词跟读的局部操作，版本未知。','初查采用口算、字词听写、单词跟读；本轮再读全部13小节并查看选定图像，版本未知。')
case+='''

## 2026-10-27：按题型补控件、重试、结束和资料处理

[完整本轮摘记与38张已看静态图](../artifacts/xiaoyuan-app-controls-deepening/source-notes.md)提供35张新查看图和3张旧图复看；[来源/控件目录](../batches/xiaoyuan-app-controls-deepening-inventory.csv)按官网、FAQ、作者小节、控件与政策分层。旧13小节目录也更新状态，不把来源目录数当功能完成率。**全部历史静态记录，仍无产品实操或连续视觉链**。下表细化/修订上表旧F，不增加重复任务。

| 既有任务 | 本轮补强 | 边界 |
| --- | --- | --- |
| F02 练习 | 口算重写/跳过、草稿清空/撤销/重做、退出继续/离开；竖式显式完成，同步练有下一题；结果可完成但只部分答对 | 题型规则不能互套；题量200仅正文，可见菜单至100；金币、经验、竞速榜分别处理；退出后恢复未知 |
| F05 听写 | 课文/范围、纸笔与在线、自由输入；声音、间隔、次数和顺序；提交前清写与提交后正确写法分开，结果错题重练 | 间隔不是语速；英文只补横屏输入，不能套中文错误规则；完成项数不等于答对数 |
| F06 跟读 | 三种模式入口、单词结果逐项星级/色标/播放；正文两次失败规则 | 句子与情景只有入口；拼图题号不同，非连续两次失败，回放内容未听 |
| F01/03/04 | FAQ/政策与商店范围核对；OS撤权与历史删除分开 | 独练周联赛不能填PK结算；仍缺纸面检查全链、AI失败/恢复与PK退出掉线 |

| 新F（均局部D2） | 进入→作答/控制→反馈→结束 | 缺失分支 |
| --- | --- | --- |
| F-M028-07 听算 | 选范围/题量→听题/重播→语音回答→对错；作者述时限内重试 | 超时、拒麦、误识别、完成与断点保存 |
| F-M028-08 趣味识字 | 单元/道路字卡→学/读/练→示范/录音星级/选字→获得字卡→继续 | 解锁阈值、答错/跳过/退出恢复；未测5–6岁独立使用 |
| F-M028-09 背诗 | 选诗→背诵呈字/提示/重背→错字/提示次数/时长/分数→回放或再背 | 示例1提示99分不证明扣分公式；录音纠错/删除/导出/恢复 |
| F-M028-10 背单词 | 新学/复习计划→释义图音→选图/释义提交→反馈继续→错题重练→统计 | 补字母/重练至全对仅作者报告；首次/重练正确率口径、复习算法与退出保存 |
| F-M028-11 注销 | 政策我→设置→关于→注销→确认→二次确认；列检查/练习/榜/金币记录 | 未执行，未见取消/失败/恢复；关联产品需分别处理 |
| F-M028-12 个人资料副本 | 政策我→头像框→截屏 | 仅所述资料副本路径，不是完整学习数据库导出/可恢复快照，也不能反推全产品绝无其他导出 |

证据新增E-XYAC-01～13均派生于同一作者E-XYCD-01；不是13个独立来源。官网/FAQ/商店/旧政策同URL追加快照，沿用旧E。旧政策正文日期2025-08-18；首页当前xyst政策新链接未读，不宣称其完全相同。后台备份等更新才删除不表示用户能恢复；免重复注册部分服务不表示软件与硬件学习进度互通。

对本项目的助手建议仍待讨论：比较首次答案、重试与提示后的答案，再解释“完成/答对/掌握”；用阶段终点与可理解的错误反馈帮助孩子主动结束。语文选择练习、练字挑战和英文听写仍缺完整反馈/结果，只登记控件，不强升任务深度。下一步的高价值证据是当前构建同题故意答错、提示/重答、主动退出、重启及记录恢复的连续序列。
'''
(L/'cases/M-028-xiaoyuan-ai.md').write_text(case)
dump(W/'build-result.json',dict(new_evidence=len(es),new_flows=len(new),inventory_rows=len(rows),new_images=len(observed),reobserved_images=len(oldims),old_flow_updates=len(updates),additional_captures=len(props)))
print((W/'build-result.json').read_text())
