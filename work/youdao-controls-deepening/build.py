from pathlib import Path
import csv,json,hashlib,shutil
R=Path('.');L=R/'docs/research/interaction-library';W=R/'work/youdao-controls-deepening';A=L/'artifacts/youdao-controls-deepening'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rows,fields):
 with p.open('w',newline='') as f:wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(rows)
assert not any('E-YDCT' in p.read_text() for p in (L/'batches').glob('*evidence.jsonl'))
case=L/'cases/M-025-youdao-pen.md';(W/'case-before.md').write_bytes(case.read_bytes())
oldpaths=[L/'batches'/s for s in ['dictionary-evidence.jsonl','dictionary-coverage.csv','dictionary-inventory-evidence.jsonl','youdao-device-deepening-evidence.jsonl','youdao-device-deepening-coverage.csv']]
dump(W/'old-batch-hashes.json',{str(p):sha(p) for p in oldpaths})
oldE={}
for p in (L/'batches').glob('*evidence.jsonl'):
 if p.name=='youdao-controls-deepening-evidence.jsonl':continue
 for line in p.read_text().splitlines():
  if line.strip():x=json.loads(line);oldE[x['evidence_id']]=x
# No new functional evidence or task: deliberate empty JSONL and header-only coverage.
(L/'batches/youdao-controls-deepening-evidence.jsonl').write_text('')
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
csvout(L/'batches/youdao-controls-deepening-coverage.csv',[],fields)
dump(W/'existing-coverage-updates.json',[]);csvout(W/'existing-coverage-updates.csv',[],['flow_id','source_batch','field','before','after','reason'])
notes='''# M025 普通大陆2.0：已有入口与主链缺口复核

日期2026-10-27。本轮 **0新E、0新F、0深度/字段变更**；E-YDCD已被旧批使用，新候选前缀E-YDCT核无碰撞，但没有为凑数分配。五个原F保留。没有取得普通2.0完整原厂手册或更新后的正式教程，故本目录不是手册目录。

## 本地原件复读

- E-YDP-01：历史产品HTML标题明确2.0，菜单“使用说明”跳向1.4.5网页；app.2389c148.js只有功能宣传。产品资源路径v2.11与小程序1.4.5都是前端包版本，不是笔或手机App构建。产品页能列多行、发音、语音、蓝牙耳机和在线更新，但不能补校订/收藏动作。
- E-YDP-02：公开帮助JS只有两个完整问题，分别是笔端登录和手机实时显示；前者成功身份反馈、后者同账号/蓝牙搜索/重连/离开页面5分钟规则已归原F02/F03，没有第三个收藏或复习章节。**本轮未运行JS或调用其桥接接口。**
- E-YDP-05：入口HTML保留使用说明和相关公开资源链接；原生桥接名只作为代码元数据，不证明真实按钮、成功或数据同步语义。
- E-YDCD-02：复读广外既有HTML，标题/正文将纸质图关联普通2.0借用设备；“激活方式”和“使用引导”是两个图注，不是完整手册章目。没有重新访问该校页面或重看图，不添加本轮visual=true。

原HTML/JS均为2026-10-26取得，本轮仅本地复读，路径和SHA见additional-captures建议。旧E-YDP-03/04模板、技术文章及北外公告本轮未重新取得，台账明确为旧证据范围，不能把引用旧案例称为本轮重新读原件。

## 来源探索（与功能目录分开）

[现行有道首页](https://www.youdao.com/)匿名web正文可读，含AI硬件链接；没有从该页取得普通2.0支持目录。[hardware.youdao.com](https://hardware.youdao.com/)回到上述首页；[c.youdao.com根路径](https://c.youdao.com/)返回403后停止该路线，不重取其历史产品路径。首页AI硬件链接 [smart.youdao.com](https://smart.youdao.com/) 被工具报告重定向至smartyoudao.com商品集合且不可重试；未访问该目的页，不据此断言旧型号停服。

支持目录/原厂教程的限定检索只落回旧产品、旧技术文章、跨代或无关材料。旧桌面词典5.0的单词本帮助不是笔2.0行为；专业版/3代/海外手册不迁入；品牌相似域名未核官方，不引用为事实。一条2019普通2.0作者评测链接只尝试正文，工具报大于1MiB，未读取可用正文或图片，不据搜索片段新增事实。详见同批discovery.csv；这些失败/重定向**不是功能证据，也未获得新E**。

## 下一类取证与完成条件

最有价值的是带普通2.0型号/固件的随盒完整说明或原厂连续教程，至少显示：一次扫描的起止与失败→候选/改字或选词→明确收藏完成→词本回取/练习→退出与重新进入；另需同版本导出、离线队列和换机/删后恢复资料。只有首页收藏图标、面板计数或当前App的能力均不足。若后续只有其他代资料，必须另列型号分支，不抹去本案缺口。

本轮无新静态观察、音视频观看/下载、登录、安装、用户接口调用或产品动作。已有五任务和D1/D2边界保持不变，不能把资料检索受阻当成“不支持”。
'''
(A/'source-notes.md').write_text(notes);arel=str((A/'source-notes.md').relative_to(L));ah=sha(A/'source-notes.md')
help_source=L/'artifacts/dictionary/youdao-2-guide-1.4.5-help.js';help_copy=W/'youdao-2-guide-1.4.5-help.js';shutil.copyfile(help_source,help_copy)
raws=[('E-YDP-01','work/dictionary-research/youdao-2.html','产品标题；使用说明跳转；v2.11资源路径'),('E-YDP-01','work/dictionary-research/youdao-app.js','多行/发音/语音/蓝牙/在线更新宣传模块'),('E-YDP-02',str(help_copy),'两个问题：账号登录、查词实时显示；原F02/03已覆盖正常链'),('E-YDP-05','work/dictionary-research/youdao-guide.html','HTML帮助入口/资源清单；原生桥接名不当真实动作'),('E-YDCD-02','work/youdao-device-deepening/gdufs-2.html','普通2.0标题/正文，激活方式和使用引导图注')]
capgroup={}
for eid,p,loc in raws:
 capgroup.setdefault(eid,[]).append(dict(capture_kind='local_reread_of_existing_public_artifact',original_acquisition_date='2026-10-26',checked_on='2026-10-27',network_refetched=False,raw_work_path=p,raw_sha256=sha(R/p),locator=loc,archived_artifact=arel,sha256=ah,hands_on=False,visual_observed=False))
adds=[]
for eid,caps in capgroup.items():
 before=oldE[eid].get('additional_captures');adds.append(dict(evidence_id=eid,field='additional_captures',before=before,after=(before or [])+caps,reason='本地复读/原件路径保全；不新增事实或任务，不重复编号'))
dump(W/'additional-captures-updates.json',adds)
rows=[]
def item(n,domain,entry,loc,e,f,status,raw,gap,nextsrc):rows.append(dict(item_id=f'M025-I{n:02}',functional_domain=domain,known_entry=entry,source_locator=loc,evidence_ids=e,flow_ids=f,coverage_state=status,raw_work_path=raw,gaps=gap,next_evidence=nextsrc,checked_on='2026-10-27'))
item(1,'型号与帮助关联','历史产品页使用说明菜单','产品HTML标题/跳转','E-YDP-01;E-YDP-05','','local_original_reread','work/dictionary-research/youdao-2.html','仅证历史普通2.0网页关联；没有完整硬件目录或固件号','同型号带版次的完整随盒指南')
item(2,'硬件部件/扫描触发','按压笔头滑扫','技术文章背景；旧机构使用图','E-YDCD-01;E-YDCD-04','F-M025-01','prior_evidence_only','','本轮未重读技术原文/图；扫描抬笔结束/取消仍未知','原厂起止和失败教程')
item(3,'识别结果校订/选词','未取得明确入口','主链缺口','E-YDCD-01','F-M025-01','unresolved','','划词翻译字样不证候选/改字/撤销','结果页连续操作与纠错分支')
item(4,'多行翻译','只有功能模块','产品JS多行模块','E-YDP-01','F-M025-01','claim_only_D1','work/dictionary-research/youdao-app.js','追加/确认/清空和译文回原文未知','普通2.0多行教程')
item(5,'词典选择','产品列专业考试词典','产品JS词库模块','E-YDP-01','F-M025-01','claim_only_D1','work/dictionary-research/youdao-app.js','有词库不代表已知切词典按钮/无命中处理','目标固件词典设置页')
item(6,'发音','英/美音声明；原厂扫描链含TTS','产品JS；旧技术文章','E-YDP-01;E-YDCD-01','F-M025-01','claim_and_prior_documented_fragment','work/dictionary-research/youdao-app.js','重播/切音/离线音库下载未知','结果页及发音设置教程')
item(7,'蓝牙耳机','仅支持声明','产品JS耳机模块','E-YDP-01','','claim_only_D1','work/dictionary-research/youdao-app.js','配对、断开、扬声器回退未知；不能套手机实时显示','耳机配对正式帮助')
item(8,'语音助手','机构正文写长按主按键','广外HTML功能段；北外旧证据','E-YDCD-02;E-YDCD-05','','entry_fragment_only','work/youdao-device-deepening/gdufs-2.html','录音结束/取消/联网失败未知；主按键和功能键对应未核','同型号按键及语音教程')
item(9,'首次激活','纸质图已有开机/激活/配网步骤','旧激活图审计','E-YDCD-02;E-YDCD-03','F-M025-05','prior_static_documented_D2','work/youdao-device-deepening/gdufs-image-4.png','本轮不重看；超时/中断/重置后行为未知','完整激活异常与服务版本说明')
item(10,'账号登录','笔首页头像；手机小程序扫码','function_explain第1问题','E-YDP-02;E-YDP-05','F-M025-02','local_body_reread_D2',str(help_copy),'当前功能未测；过期码/换账号/权限拒绝未知','同版异常帮助')
item(11,'实时释义','同账号；笔账户/实时显示；手机同入口','function_explain第2问题1–4步','E-YDP-02','F-M025-03','local_body_reread_D2',str(help_copy),'只证释义协同，不证词本/进度全量同步','离线缓存/多笔切换资料')
item(12,'实时显示断连','蓝牙及距离检查→重新搜索','function_explain第2问题末段','E-YDP-02','F-M025-03','local_body_reread_D2',str(help_copy),'离开笔实时页5分钟；手机后台/网络切换仍未知','后台/掉线恢复教程')
item(13,'收藏入口与持久保存','旧首页示意有收藏；具体提交入口未知','旧激活图与旧面板计数','E-YDCD-03;E-YDP-03','','prior_icon_and_template_only','work/youdao-device-deepening/gdufs-image-4.png','收藏图标/计数不能证提交成功、原句或去重','结果页收藏→词本回取连续材料')
item(14,'词本练习','没有同型号题型/答错/结束链','主链缺口','E-YDP-03','','unresolved','','不得套桌面App或其他代词典笔','普通2.0练习帮助和错误反馈')
item(15,'学习统计/周词单','旧公开模板有扫词/入本/时长字段','E-YDP-03旧摘记','E-YDP-03','F-M025-04','prior_template_claim_D1','','原模板本轮未复取；计数不是掌握或练习成功','真实统计定义及完整界面/服务状态')
item(16,'提醒/解绑','旧设置模板开关和结果文案','E-YDP-04旧摘记','E-YDP-04','F-M025-04','prior_template_claim_D1','','本轮未复取；权限/送达/解绑数据范围未知','同版提醒、解绑前后数据说明')
item(17,'导出/换机/删后恢复','未取得入口','主链缺口','','','unresolved','','同步字样不能补备份；未证导出对象和恢复粒度','同型号导出格式与恢复文档')
item(18,'离线范围','机构列离线查词/翻译/发音','广外正文；北外旧公告','E-YDCD-02;E-YDCD-05','F-M025-01;F-M025-05','institutional_historical_scope','work/youdao-device-deepening/gdufs-2.html','不证所有词库/语种；首次激活与音库下载条件未知','原厂离线清单/版本表')
item(19,'在线更新/系统恢复','产品只有更新承诺','产品JS系统在线更新','E-YDP-01','','claim_only_D1','work/dictionary-research/youdao-app.js','实际入口/固件历史/中断/回滚未知；不推现服务可用','普通2.0正式版本日志及恢复帮助')
item(20,'手机桥接边界','HTML资源中存在原生桥接名','入口HTML','E-YDP-05','','metadata_only','work/dictionary-research/youdao-guide.html','只读静态资源；没有调用接口，也不将名字升级为操作链','官方面向用户的帮助正文')
csvout(L/'batches/youdao-controls-deepening-inventory.csv',rows,list(rows[0]))
discovery=[
('D01','https://www.youdao.com/','web_open_body','首页可读含AI硬件链接；未取得普通2.0帮助目录','只目录探索，不分配E'),
('D02','https://hardware.youdao.com/','web_open_redirect','跳至www.youdao.com首页；不是型号化支持正文','不视为停服证明'),
('D03','https://c.youdao.com/','web_open_403_stop','根路径403；停止，不重取历史产品路径','未作绕过或重试'),
('D04','https://smart.youdao.com/','web_click_nonretryable_redirect','工具500并报告转向smartyoudao.com/collections/youdao-products不可访问','未访问目的地，不迁入当前/海外能力'),
('D05','https://post.smzdm.com/p/a5klmn0l/','web_open_size_limit','工具报正文超过1048577字节；没有可用正文/新图片观察','不据搜索片段建E/F'),
('D06','https://cidian.youdao.com/5.0/help/deskdict5beta/features/03.html','search_result_excluded','桌面词典5.0单词本帮助，不是普通2.0笔','不迁入复习/同步流程'),
('D07','https://www.sohu.com/a/409798850_120135347','search_result_excluded','专业版体验，不是普通2.0固定范围','不迁入菜单/收藏/听力'),
('D08','','bounded_official_search_no_stronger_source','售后服务/原厂使用视频及dictPen2.0 help检索回旧材料或跨代、无关条目','非穷尽不存在证明；无额外重复检索')]
drows=[dict(discovery_id=i,url=u,access_status=s,result=r,disposition=d,checked_on='2026-10-27') for i,u,s,r,d in discovery]
csvout(L/'batches/youdao-controls-deepening-discovery.csv',drows,list(drows[0]));dump(W/'discovery-log.json',drows)
case.write_text(case.read_text().replace('查阅2026-10-26。范围','查阅2026-10-26；来源目录复核2026-10-27。范围',1)+'''\n\n## 2026-10-27：普通2.0目录复核与未解决环节\n\n本轮没有取得更强的同型号手册或完整教程，保留5个原任务及原D1/D2深度，**0新增E/F、0旧F字段变化**。新增 [20项覆盖台账](../batches/youdao-controls-deepening-inventory.csv)，逐项区分本轮复读原件、旧证据范围、营销声明和未取得入口；它不是完整硬件手册目录，也不新增学习功能结论。\n\n复读旧产品HTML、产品JS、帮助JS、帮助入口HTML及广外既有HTML：可追溯的官方帮助正文仍是登录/实时显示两个问题，现有F02/03已覆盖其正常链。没有通过继续拆原生桥接名、面板计数或首页收藏图标补出校订、收藏、练习、导出或恢复。多行追加、词典切换、英美音切换和耳机断联仍缺同版操作。\n\n来源探索另列 [记录](../batches/youdao-controls-deepening-discovery.csv)：现行官网首页未给普通2.0目录；硬件域回首页，AI硬件链接转向另一商品集合且工具不可访问；c.youdao.com根路径403后停止。一条历史作者评测正文超过工具大小限制，未据搜索片段立事实。失败和重定向不说明产品不支持或服务停运。桌面词典5.0、专业版、3代和未核官方的相似域均不填本型号空白。\n\n没有新增看图或连续操作。原件SHA及本地复读方式见 [短摘记](../artifacts/youdao-controls-deepening/source-notes.md) 与work/youdao-controls-deepening追加采集建议。下一类有效材料应是带普通2.0型号/固件的完整随盒指南或原厂连续教程，覆盖结果校订→明确收藏完成→词本取回/练习→退出/恢复；不能用更多宣传页替代。\n''')
print('newE0 newF0 oldFields0 inventory20 discovery8 oldEvidence',len(adds),'captures',sum(len(v) for v in capgroup.values()))
