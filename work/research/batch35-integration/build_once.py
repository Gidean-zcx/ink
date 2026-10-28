from pathlib import Path
import csv, json, hashlib, copy, shutil
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch35-integration';RW=R/'work/research/batch35-readwise';XY=R/'work/research/batch35-xiaoyuan'
assert not (W/'applied.json').exists(), 'already applied'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def wc(p,data):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(data[0]));w.writeheader();w.writerows(data)
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
E={x['evidence_id']:x for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())};F={x['flow_id']:x for x in rows(L/'coverage.csv')}
assert len(E)==1502 and len(F)==888
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
before={}
def save(p):
    if str(p) not in before:
        q=W/'before'/p.relative_to(R);q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(p.read_bytes());before[str(p)]=sha(p)
for p in [L/'coverage.csv',L/'evidence.jsonl',L/'catalog.csv',L/'case-index.md']:save(p)
commit='2d1ce9627c611d24f510dfc2e05a123fa509d2f6'
tree=json.loads((RW/'canonical-tree.json').read_text());blobs={t['path']:t['sha'] for t in tree['tree'] if t['type']=='blob'}
sources=[
('README.md','仓库目录','Available Skills / Usage','固定版本列11份工作流与工具说明；安装和运行示例未执行。',''),
('skills/readwise-mcp/SKILL.md','MCP工具说明','Setup; Reader Tools; Readwise Tools','区分文档与高亮；查询分页/字段选择、逐字匹配HTML高亮、每次最多50文档移动、异步导出轮询；仅示例契约。','F-M015-26'),
('skills/readwise-cli/SKILL.md','CLI工具说明','Discovering Commands; Reader Commands; Readwise Commands','帮助发现命令、JSON结果与刷新缓存；文件/高亮对象分开。示例seen筛选与feed工作流文字存在冲突，未调用验证。','F-M015-26'),
('skills/triage/SKILL.md','逐篇筛选','Setup; Process; Handling Responses','每批10篇逐篇给阅读/稍后/归档；read/done/great等回应均可触发归档。没有持久会话恢复定义。','F-M015-27'),
('skills/feed-catchup/SKILL.md','订阅追读','Fetch Unseen Feed Items; Present; Handle Responses','分页凑20篇未打开内容，选5篇推荐；next/seen批量标记当前20篇已见。正文同时出现下一批10与20，保留冲突。','F-M015-28'),
('skills/quiz/SKILL.md','阅读测验','Pick a Document; Prepare; Quiz; Wrap Up','最多两次候选查询，按篇幅3/4/5题；逐题等待回答、给等级和解释后再提问，末尾总评。没有题库写入或稳定评分准则。','F-M015-29'),
('skills/build-persona/SKILL.md','读者画像','Quick Pass; Write Persona; Deep Pass','从高亮/四类位置抽样生成本地reader_persona.md，用户反馈可修订；内容缺席推成反兴趣是推断，非显式偏好。','F-M015-30'),
('skills/reader-recap/SKILL.md','阅读回顾','Fetch Recent Documents; Get Highlights; Classify; Write','按更新日期取archive/later并去重，依据批注整理回顾与行动项；未标注内容被定义为未参与，不能当真实阅读结论。','F-M015-31'),
('skills/book-review/SKILL.md','书评草稿','Phases 1–7; Error Handling','消歧选书，少于10条高亮先询问；关联库内与外部材料后生成草稿并写回Reader，写入失败则提供Markdown和部分产物。','F-M015-32'),
('skills/highlight-graph/SKILL.md','高亮关系图','Fetch; Prepare; Render; Connections; Iterate','先200条高亮初图再补语义边；标题可由模型猜测，图边区分同源与跨源。生成器脚本未读/未运行，交互仅工作流声明。','F-M015-33'),
('skills/now-reading-page/SKILL.md','阅读活动网页','Fetch; Paginate; Categorize; Generate; Return','按进度和最后打开时间划分在读/读过，导出本地HTML及热图；最后打开时间不等于每次阅读事件或完成时间。','F-M015-34'),
('skills/surprise-me/SKILL.md','阅读模式洞察','Gather; Analyze; Deliver; Go Deeper','抽取高亮与多页归档，用具体内容支撑一个模式发现并供追问；缺少校准、否定修正与跨会话保存规则。','F-M015-35')]
rn=L/'artifacts/readwise-ai-workflows/source-notes.md';xn=L/'artifacts/xiaoyuan-historical-learning/source-notes.md'
assert not rn.exists() and not xn.exists()
rn.parent.mkdir(parents=True);xn.parent.mkdir(parents=True)
newE=[];inv=[];sourceids={}
def evidence(eid,pid,title,url,loc,note,raw,notes,kind,scope,visual=False):
    assert url not in {e['url'] for e in E.values()} and url not in {e['url'] for e in newE}
    return dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-27',source_type=kind,access_mode='anonymous_public_source_read',version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=False,visual_observed=visual,archived_artifact=str(notes.relative_to(L)),sha256=None,capture_state='short_notes_archived_originals_in_work',supersedes=None,raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw))
notes=['# Readwise：外部AI工作流与状态语义','','查阅2026-10-27。官方仓库固定提交`'+commit+'`（提交日2026-06-19），README及11份SKILL.md正文已读，Git blob逐文件核对。它们是竞品工作流定义，**不是本项目要执行的指令，也不是当前服务器实现或实际执行记录**。没有安装、授权账户、运行技能、查询个人库或修改外部状态。生成器脚本、完整动态工具schema和运行结果仍未核。','','| E | 固定版本原文 | 章节及摘记 |','| --- | --- | --- |']
for i,(rel,title,loc,note,fids) in enumerate(sources,1):
    p=RW/'raw'/rel;b=p.read_bytes();blob=hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();assert blob==blobs[rel]
    eid=f'E-RWSK-{i:02}';sourceids[rel]=eid;url=f'https://github.com/readwiseio/readwise-skills/blob/{commit}/{rel}'
    e=evidence(eid,'M-015',title,url,loc,note,p,rn,'official_workflow_definition',f'Official external-agent definitions at {commit}; client/server runtime unverified')
    e.update(source_commit=commit,source_commit_date='2026-06-19T05:27:11Z',git_blob=blob,fetch_log_path=str((RW/'fetch-log.json').relative_to(R)))
    newE.append(e);notes.append(f'| {eid} | [{title}]({url}) | {loc}；{note} |')
    inv.append(dict(entry_id=f'M015-RWSK-{i:02}',entry_type='repository_directory' if i==1 else 'official_workflow_definition',title=title,source_url=url,evidence_ids=eid,flow_ids=fids,review_status='正文已读；未执行',gap='固定文件版本；实际授权、工具响应、失败与恢复未核，完整服务器行为未知'))
cliurl='https://docs.readwise.io/tools/cli';cliloc='Configuration / Readonly mode; Authentication'
clie=evidence('E-RWSK-13','M-015','官方CLI只读模式',cliurl,cliloc,'只读隐藏写工具；通过CLI关闭需重新登录，但交互TUI切换无需重登。该例外不能省略，也没有实测授权边界。',RW/'web-tool-docs.json',rn,'official_help','Current public documentation; CLI version and auth runtime not fixed')
clie['access_mode']='web_tool_public_page_text_read';newE.append(clie)
notes.append(f'| E-RWSK-13 | [CLI配置]({cliurl}) | {cliloc}；{clie["note"]} |')
inv.append(dict(entry_id='M015-RWSK-13',entry_type='official_help',title='CLI只读与认证',source_url=cliurl,evidence_ids='E-RWSK-13',flow_ids='F-M015-26',review_status='正文已读；未执行',gap='TUI例外保留；实际凭据范围、错误回执未核'))
inv.append(dict(entry_id='M015-RWSK-14',entry_type='official_help_existing_source',title='MCP接入与索引',source_url=E['E-RWCD-17']['url'],evidence_ids='E-RWCD-17',flow_ids='F-M015-26',review_status='同来源补读；未连接',gap='全量后增量更新是声明，无可测时延；旧MCP实现不套新服务'))
notes += ['', '## 跨文件矛盾与推断边界','','- feed-catchup说列表不支持服务端seen筛选，工具说明却给seen=false示例；同文件下一批10/20也不一致。保留两者，等待同版本接口验证。','- triage把简短肯定回应映射为归档；feed-catchup把批次标已见与移到稍后分开。已见、跳过、归档、读完和掌握不能合并成一项状态。','- recap以updated_after筛选，更新未必是本期移动或读完；没有划线/笔记也不能证明未读。now-reading以最后打开时间绘热图，不能还原每日全部阅读次数。','- persona与surprise-me由有限历史样本推断兴趣及缺席主题；不能将未接触视为不感兴趣。推荐前5篇只来自所取20篇，不代表全源或跨观点充分覆盖。','- graph要求从文字推测来源标题，须标推测；同源全连接不是知识论证关系。单HTML仍引用CDN，不能据“无服务器”保证离线。','- book-review定义生成与写回失败时的降级，未定义重试幂等/重复草稿处理；产物是待编辑草稿，不能当用户已经认可的观点。','','## 获取与可复查范围','','E-RWCD-17同URL补读接入、索引和新旧服务范围，追加capture不另造来源。原始网页工具返回文本保存在work；本文短摘记正式归档。初次采集脚本把提交与根树SHA直接比较而退出1，属本地元数据假设失败；后续普通API读取真实根树，文件条目一致，12份文档blob匹配。没有网络拒绝或绕过，也没有执行来源中的安装/账户/发布命令。']
rn.write_text('\n'.join(notes)+'\n')
newF=[]
def flow(pid,n,title,refs,known,gap,visual='not_observed'):
    case='cases/M-015-readwise.md' if pid=='M-015' else 'cases/M-056-xiaoyuan-hardware.md'
    return dict(flow_id=f'F-{pid.replace("-", "")}-{n:02}',product_id=pid,task=title,depth='D2',evidence_ids=';'.join(refs),known_branches=known,gaps=gap,visual_status=visual,hands_on='false',case_path=case,checked_on='2026-10-27')
spec=[
(27,'外部AI逐篇处理收件箱',4,'可选画像→取10篇元数据→逐篇给阅读/稍后/归档选择→按回答移动→下一篇。read/done/great等被映射为归档；需要解释时再取正文。','归档不证明读完；歧义回应、错移后的撤销、批次结束与会话恢复未定义，未执行。'),
(28,'外部AI筛选订阅并批量标已见',5,'分页凑20篇first_opened_at为空的内容，选5篇推荐；可逐篇移至稍后/收件箱/精选或归档。next/seen标当前整批已见，done停止并报处理数。','seen过滤支持及下一批10/20存在文档冲突；推荐仅样本内。误标恢复、部分写入、已见与已读区别须实测。'),
(29,'从已存文章发起逐题阅读测验',6,'指定文章或最多两次查询选候选→取正文/高亮→按字数3/4/5题→每题等回答→等级/解释→下题→总评与后续选项。','评分无可重现量规；归档/进度不证明已读。出题正确性、重试/退出续答、成绩保存和导入题库未规定或验证。'),
(30,'建立并修订供AI使用的读者画像',7,'抽样高亮、四类位置文档与标签；数据少时回退→写本地reader_persona.md→展示摘要→可深查或按用户反馈修订。','历史缺席不能推定排斥新知识；画像仅外部工作目录文件，非设备账户档案。过期/来源标注、删除传播与儿童隔离未定义。'),
(31,'从批注生成近期阅读回顾',8,'默认24小时或指定天数→查询更新过的archive/later→按ID去重→取高亮/笔记→分类并提炼问题、待办和想法；只读。','updated_after不是阅读/移动事件时间；零批注不等于未读。无超过首100项的分页规则，行动项未自动写入任务App。'),
(32,'跨书库生成书评草稿并保存',9,'搜书并消歧→不足10高亮先询问→提取论点与跨源关联→补外部材料→草稿独立质量检查→写回Reader；失败输出Markdown及已有产物。','以高亮为主要输入，非已读整本证明；没有执行写入/预览确认。来源核验、重复重试和草稿撤回未证，观点待用户编辑。'),
(33,'把高亮和跨来源关联生成可探索图',10,'取两页共最多200高亮→准备数据→先输出无语义边HTML→补跨源关联及理由→重建→可加页或按主题重建。','标题可能靠模型推断；同源边不是语义证明。build_graph.py及真实控件未核，CDN依赖、重建选区保持与错误恢复未测。'),
(34,'从书库生成在读页及活动热图',11,'取shortlist/later/new和多页archive；0.05–0.99进度列在读，归档且进度>0.9列读过；按last_opened_at展示，生成本地HTML后可调整。','进度阈值是工作流分类，不是理解证明；最后打开时间不能恢复历史全部事件。来源身份、空值/边界、导出刷新/公开发布未核。'),
(35,'用阅读记录提出可追问的模式洞察',12,'收集高亮、标签、精选与多页归档→寻找重复主题/矛盾/变化→以具体材料支持一个发现→用户可要求深查或相关内容。','观察样本有偏；没有性格/能力推断的可靠性验证、否定修订或画像写回规则。惊喜目标可能影响结论选择，未执行。')]
for n,title,e,known,gap in spec:newF.append(flow('M-015',n,title,[f'E-RWSK-{e:02}'],known,gap))
old=copy.deepcopy(F['F-M015-26']);updated=copy.deepcopy(old)
updated.update(task='外部AI接入、发现读写工具与处理结果（文档链）',depth='D2',evidence_ids=old['evidence_ids']+';E-RWSK-02;E-RWSK-03;E-RWSK-13',known_branches='官方接入后可查询文档/高亮，示例给分页/字段选择、移动最多50项、精确HTML高亮与异步导出轮询。CLI只读隐藏写工具；CLI关闭需重登而TUI切换无需重登。',gaps='已取得工作流与参数示例，仍无完整动态schema、实际OAuth范围、工具回执/部分成功/重试与回滚。索引全量后增量是声明，实际时延未测；外部AI与原生Global权限分开。')
oldF=[dict(flow_id=old['flow_id'],before=old,after=updated,fields_changed=[k for k in old if old[k]!=updated[k]])]
b=E['E-RWCD-17'];a=copy.deepcopy(b);a.setdefault('additional_captures',[]).append(dict(checked_on='2026-10-27',access_mode='web_tool_public_page_text_read',locator='connection/authorization; MCP FAQs indexing and old server',note='补普通连接/授权顺序与初次全量后增量索引；没有登录、读取个人库、测速或以旧服务源码代替新服务。',raw_work_path=str((RW/'web-repository.json').relative_to(R)),raw_sha256=sha(RW/'web-repository.json'),archived_artifact=str(rn.relative_to(L)),sha256=sha(rn)))
oldE=[dict(evidence_id=b['evidence_id'],before=b,after=a)]
# Historical hardware evidence is kept separate from the current S6/E1 tasks.
xs=[('E-XYHL-01','2024年龙年礼盒作者体验','https://post.smzdm.com/p/a3x3nk5d/','original-review','2024-03-08','核心功能：教材同步/错题本/云文件；不足','作者叙述提交→批改→订正→再提交及首次/订正后评价差别，并报告减号误判；三短片抽样未呈现判错/重做。'),('E-XYHL-02','同作者半年更新与听书','https://post.smzdm.com/p/a4x56p6x/','followup-review','2024-09-14','24.08.31更新：教材习题/听书模式/多孩切换','打开互动阅读书籍→右上听书，可调速/拖进度/定时；多孩独立记录为作者声明，切换/删除/恢复路径未知。')]
xnotes=['# 小猿：2024作者样机学习与听书记录','','查阅2026-10-27。两篇均为“狂徒老爹”的原创使用报告，第二篇关联第一篇；不是两个独立测试。作者称2024龙年限定礼盒旗舰款，**本项目未核硬件型号和固件**，不把页面S2商品卡当样机身份，也不套入现款S6/E1。文字含营销/选购意见，仅记录可定位的操作描述与问题，不引用健康或学习提升承诺。','','| E | 原文及日期 | 取得与范围 |','| --- | --- | --- |']
for eid,title,url,raw,date,loc,note in xs:
    e=evidence(eid,'M-056',title,url,loc,note,XY/(raw+'.html'),xn,'first_person_product_report','Same author 2024 Dragon-year gift-set sample; exact model/firmware unknown; NOT current S6/E1',eid.endswith('01'))
    e.update(source_published_on=date,source_author='狂徒老爹',text_work_path=str((XY/(raw+'.txt')).relative_to(R)),text_sha256=sha(XY/(raw+'.txt')),fetch_log_path=str((XY/'browser-read-log.json').relative_to(R)))
    if eid.endswith('01'):e['visual_kind']='sampled_video_frames'
    newE.append(e);xnotes.append(f'| {eid} | [{title}]({url})；{date} | {loc}；{note} |')
xnotes += ['', '## 文字支持的任务及其限制','','- 教材同步：选当前课的练习，作答提交后由系统评价；错题可订正并再交，也可查看解析/部分视频。作者称首次全对S、订正全对A+，其他档次没有精确阈值；点数可换课外书。没有连续画面证明误判修正或奖励到账。','- 错题按学科/时间归集，复练可出相似新题；这不证明原题订正与变式独立掌握相同。作者报告减号误判和跨页题图，属于单人样本，频率及申诉/纠正入口未知。','- 后文听书入口和调速/进度/定时为文字步骤；没有音频、暂停/续播、锁屏、离线、换孩子或重启恢复证据。多孩最多5个、各5G云文件空间是历史作者声明，没有验证身份切换和隔离。','','## 实际查看的九个视频时间点','','匿名浏览器普通访问文章HTTP200，媒体只流式加载/暂停/定位，没有保存MP4或听音。对三条内嵌视频分别看起点、中间、末端；这是抽样静态观察，**不是连续过程或产品操作**。以下正式保留每条一个代表帧；其余六帧及实际定位时间/哈希在work的observations.json。','','| 视频 | 已看时间（秒） | 实际画面与未观察项 | 代表帧 |','| --- | --- | --- | --- |']
obs=json.loads((XY/'clip-observations/observations.json').read_text())
for idx,times,file,desc in [(1,'0.00 / 4.00 / 8.61','clip-1-4.00s.png','可见十几减9退位减法讲解卡，未见手写提交/批改'),(2,'0.00 / 4.00 / 10.81','clip-2-4.00s.png','可见语文选择题与选项，未见用户选择、错误反馈或重试'),(3,'0.00 / 4.00 / 12.67','clip-3-4.00s.png','可见拼音诗文、读一读、累计时长及翻页控件；未听音，不证明点读响应')]:
    dest=xn.parent/file;shutil.copyfile(XY/'clip-observations'/file,dest);xnotes.append(f'| {idx} | {times} | {desc} | [查看]({file}) |')
xnotes += ['', '原始媒体规范路径（播放器URL的临时查询参数不作为长期引用）：', '', '- https://sqcwsp-vod.smzdm.com/4395211bvodtranscq1500013160/ec9a24221397757885604583788/v.f100030.mp4','- https://sqcwsp-vod.smzdm.com/4395211bvodtranscq1500013160/b207322a1397757885597783675/v.f100030.mp4','- https://sqcwsp-vod.smzdm.com/4395211bvodtranscq1500013160/fac3bd141397757885611914544/v.f100030.mp4','','## 没有取得的材料','','E1候选BV1TCe8zNEcV正常浏览器返回412，随后停止；新闻候选原站403亦停止。不从索引补完整操作。首次本地浏览器启动因缓存路径错误退出1，在任何导航前失败；root改用已存在浏览器后正常访问，两篇文章200。没有安装产品、登录、评论、购买或绕过站点限制。']
xn.write_text('\n'.join(xnotes)+'\n')
for e in newE:
    e['sha256']=sha(L/e['archived_artifact'])
    if e['evidence_id']=='E-XYHL-01':e['artifact_hashes']={str(p.relative_to(L)):sha(p) for p in sorted(xn.parent.glob('*.png'))}
newF += [flow('M-056',7,'历史作者样机：提交、订正与学习评价',['E-XYHL-01'],'作者报告教材同步课内练习→提交批改→错题订正再交/查看解析→分级评价和点数；首次全对S、订正全对A+，错题可归集及练变式。','2024样机型号/固件未核，不套S6/E1；抽帧只见题面。误识别纠正、同题提示/重试/结算、奖励到账、退出保存均未观察。','static_reference_only'),flow('M-056',8,'历史作者样机：打开听书并调整播放',['E-XYHL-02'],'2024后续报告：互动阅读→打开书→右上听书；可调语速、拖进度、设置定时关闭。','仅文字步骤，实际控制/到时停止、音文定位、离线与锁屏、重启/多孩恢复均未核；不套S6/E1或当前服务。')]
assert not set(e['evidence_id'] for e in newE)&set(E)
assert not set(f['flow_id'] for f in newF)&set(F)
prefix='ai-workflows-and-historical-learning';wc(L/f'batches/{prefix}-coverage.csv',newF)
(L/f'batches/{prefix}-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in newE))
wc(L/'batches/readwise-ai-workflows-inventory.csv',inv)
xi=[]
for i,title in enumerate(['教材同步','专项突破','阶段测评','错题本','教辅专区','互动阅读','益智谜题','万能本','云文件','智慧拍','听书模式','多孩切换'],1):
    fid='F-M056-07' if i in (1,4) else 'F-M056-08' if i==11 else ''
    xi.append(dict(entry_id=f'M056-XYHL-{i:02}',entry_type='historical_author_function_domain',title=title,source_url=xs[1][2] if i>=11 else xs[0][2],evidence_ids='E-XYHL-02' if i>=11 else 'E-XYHL-01',flow_ids=fid,review_status='局部文字任务' if fid else '仅名称/声明；未完整拆解',gap='2024作者样机型号/固件未核，不套S6/E1；错误、恢复和实机未核'))
wc(L/'batches/xiaoyuan-historical-learning-inventory.csv',xi)
for kind,changes,key in [('coverage.csv',oldF,'flow_id'),('evidence.jsonl',oldE,'evidence_id')]:
    for p in sorted((L/'batches').glob('*-'+kind)):
        rs=rows(p) if kind.endswith('csv') else [json.loads(x) for x in p.read_text().splitlines()];changed=False
        for i,r in enumerate(rs):
            c=next((v for v in changes if v[key]==r[key]),None)
            if c:
                assert r==c['before'];rs[i]=c['after'];changed=True;c.setdefault('owning_files',[]).append(str(p.relative_to(R)))
        if changed:
            save(p)
            if kind.endswith('csv'):wc(p,rs)
            else:p.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rs))
assert all(c.get('owning_files') for c in oldF+oldE)
cp=L/'cases/M-015-readwise.md';save(cp);t=cp.read_text()
t=t.replace('外部AI用途保留D1','外部AI现补工作流定义的D2；执行结果未核')
oldline=next(line for line in t.splitlines() if line.startswith('| F-M015-26 '))
t=t.replace(oldline,f'| F-M015-26 {updated["task"]} | {updated["known_branches"]} | {updated["gaps"]} | {updated["evidence_ids"]} |')
t=t.replace('补充边界：F-M015-26仅D1外部AI用途与提示示例；','此前边界（第35批已补文档链）：F-M015-26原仅D1外部AI用途与提示示例；')
t+='\n## 第35批：固定版本的外部AI工作流\n\n官方公开定义现补到查询对象、批量作用范围、对话分支和部分失败出口；全为D2，未运行。原生Ghostreader和外部工作目录中的技能/画像分开。见[固定来源与冲突](../artifacts/readwise-ai-workflows/source-notes.md)、[14项目录](../batches/readwise-ai-workflows-inventory.csv)。原281项帮助台账不因这11个技能文件而变成全功能已读。\n\n| F任务 | 输入、动作与结束 | 缺口 | E |\n| --- | --- | --- | --- |\n'
for f in newF:
    if f['product_id']=='M-015':t+=f'| {f["flow_id"]} {f["task"]} | {f["known_branches"]} | {f["gaps"]} | {f["evidence_ids"]} |\n'
t+='\n用于本项目讨论的推断：个人新闻需区分“给我下一批”与“我确认读过这批”；AI整理历史应使用真实事件及用户反馈，不能把无批注、归档或最后打开时间直接当理解程度。读者画像应允许新兴趣修正旧判断；测验生成、评分和写入练习App是三个结果。具体协议、阈值和界面尚未定。\n';cp.write_text(t)
cp=L/'cases/M-056-xiaoyuan-hardware.md';save(cp);t=cp.read_text()
t=t.replace('仅账号注销的文档步骤达到局部 D2','当前型号仅账号注销的文档步骤达到局部 D2；第35批另列2024历史样机文字任务')
t+='\n## 第35批：历史样机补证，不迁移到S6/E1\n\n找到同一作者2024年3月和9月两篇原创报告，样机自称龙年礼盒旗舰款，具体型号/固件未核。页面商品推荐不能充当样机身份。另查三内嵌短片共九帧，主要为讲解/题面/诗文，未看到同题错答重试；不是连续视觉链或产品实操。见[正文与抽帧摘记](../artifacts/xiaoyuan-historical-learning/source-notes.md)及[历史功能目录](../batches/xiaoyuan-historical-learning-inventory.csv)。\n\n| 任务 | 文字支持的步骤 | 未证明 | E |\n| --- | --- | --- | --- |\n'
for f in newF:
    if f['product_id']=='M-056':t+=f'| {f["flow_id"]} {f["task"]} | {f["known_branches"]} | {f["gaps"]} | {f["evidence_ids"]} |\n'
t+='\n原F-M056-01～06的范围与深度保持；本批两个历史D2不抬高当前学习主链。听书、多孩记录和云文件空间的历史声明不证明跨档案续播或本项目备份能力。值得后续验证的是首次独立答对与订正通过的区分，以及识别错误能否被纠正；这些仍是建议，不是新需求。\n';cp.write_text(t)
dump(W/'changes.json',dict(new_evidence=[e['evidence_id'] for e in newE],new_flows=[f['flow_id'] for f in newF],old_flows=oldF,old_evidence=oldE))
dump(W/'before-manifest.json',before)
dump(W/'applied.json',dict(newE=len(newE),newF=len(newF),oldF=len(oldF),oldE=len(oldE),requirements_sha256=sha(R/'docs/requirements.md')))
print(json.dumps(dict(newE=len(newE),newF=len(newF),oldF=len(oldF),oldE=len(oldE)),ensure_ascii=False))
