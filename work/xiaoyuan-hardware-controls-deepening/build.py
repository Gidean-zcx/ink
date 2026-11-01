import csv, hashlib, json
from pathlib import Path
R=Path('/home/workspace/reader'); W=R/'work/xiaoyuan-hardware-controls-deepening'; L=R/'docs/research/interaction-library'; B=L/'batches'; A=L/'artifacts/xiaoyuan-hardware-controls-deepening'; A.mkdir(parents=True,exist_ok=True)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def js(p,v): p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
case=L/'cases/M-056-xiaoyuan-hardware.md'; s=(W/'case-before.md').read_text()
s=s.replace('查阅日 2026-10-26。','初查 2026-10-26；本轮复核 2026-10-27。',1)
s=s.replace('本次下载它公开引用的前端文件','前轮下载它公开引用的前端文件',1)
s=s.replace('| 当前 S6 |','| 已取得官网前端的 S6 条目 |')
s=s.replace('以同步学科与练习为主；不能由“全学段”推定适合 5 岁零基础','以同步学科与练习为主；页面发布日、代际和固件未知，不承诺为最新 S6 配置；不能由“全学段”推定适合 5 岁零基础')
s=s.replace('官网当前 `#/S2` 和 `#/S6`','已取得前端的 `#/S2` 和 `#/S6`')
s=s.replace('今天取到的规格','前轮取到的规格').replace('当前商店的截图','前轮商店的截图').replace('同日进一步取证：','2026-10-26 进一步取证：')
s=s.replace('官方 iPhone 列表 1.25.1；本次未安装。','2026-10-27 中国区 iPhone 列表 1.25.1，HTML 标示 2026-09-25；要求 iOS 13.5 或以上，未安装。')
s=s.replace('没有当前教材选择、题型全集、按钮顺序和每道题的判分规则','前端注释说明功能/资源随年级与教材版本不同；没有教材选择动作、题型全集、按钮顺序和每道题判分规则')
s=s.replace('本例不报价，不据销售文案承诺护眼效果或学习提升。','本例不报价，不据销售文案承诺护眼效果或学习提升。官网前端快照的 S6 命名和规格未与任何后续同名代际、彩墨版或固件对应，不能把该快照标成最新整机。')
needle='“AI 精准”在本次主要是官方能力陈述。'
insert='''逐项目录见 [本轮目录台账](../batches/xiaoyuan-hardware-controls-deepening-inventory.csv)。台账分别记录产品文案、既有静态观察、政策中明确的路径和版本日志；它不是机内完整设置目录，也不表示每项已完成操作。已读公开前端的指南/手册/教程检索没有提供同型号帮助入口，不能据此断言机内没有指南。家长 App 的 25 条可读版本记录仅给泛化更新，1.25.1 的修复声明没有具体故障与验证条件，不能用来补硬件恢复行为。

'''
s=s.replace(needle,insert+needle)
s=s.replace('没有一字/一页的提示→纠错→重试→结束→恢复链；孩子自主选级与家长选级未知','没有一字/一页的提示→纠错→重试→结束→恢复链；孩子自主选级与家长选级未知；课程 S1/S2/S3 不能当硬件代际')
s=s.replace('上级入口、保存、到时锁定和解锁未知','上级入口、保存、到时锁定和解锁未知；2026-09-25 的家长 iPhone 1.25.1 日志没有补足这些动作，也未标配对硬件')
s=s.replace('不知统计跨前后台口径、家长权限差异、离线积压和邀请拒绝/撤回；未发送邀请','政策另列“我的→家庭模块”更正家庭信息；这不证明邀请/退出成功状态。不知统计跨前后台口径、家长权限差异、离线积压和邀请拒绝/撤回；未发送邀请')
s=s.replace('从选文件到校验、传输、设备接收、打开/打印、删除的全链缺失；不发文件、不打印','S6 小程序上传与家长 App 政策的文件/题目发送是不同证据范围，不能接成一条已验证链；从校验、传输、设备接收到打开/打印、删除仍缺失；不发文件、不打印')
s=s.replace('仅记录官方文档；未执行。验证码失败、换绑、儿童设备数据是否保留以及重建家庭恢复均未核实','仅记录 2025-08-15 政策路径；未执行。关联产品需分别注销/提出删除要求，后台备份可能延迟到更新时删除；这不是用户恢复承诺。验证码失败、换绑、儿童设备数据是否保留以及重建家庭恢复均未核实')
needle='离线是主要空白：'
insert='''2025-08-15 家长政策 §二允许借同品牌其他已登录产品的手机号共享登录，但 §四（一）4 同时规定各关联产品的账号及数据分别处理，注销和删除要求分别提出。共享登录不能证明 S6/E1 学习状态自动迁移，亦不能将小猿 AI 手机端的数据操作套入本例。§六.3 的“备份更新时删除”是服务端删除时序，未给用户找回进度、撤回注销或换机恢复的入口。

政策 §六.6 提供登录后的个人信息副本请求入口“我→客服中心”；§六.7 说明可能验证身份或要求书面请求，收到反馈并验证身份后十五天内答复。它没有说明副本格式、哪些学习记录可导出、是否能回灌设备或处理时限内必然交付完整副本。此为 F-M056-06 的数据边界，未形成独立的导出/备份恢复任务。

'''
s=s.replace(needle,insert+needle)
s=s.replace('1.25.1 日期显示 9 月 25 日，未单靠该行补写年份。','原快照只记录日期行的 9 月 25 日；本轮同 URL 新 HTML 的 `time datetime="2026-09-25"` 补足年份，并核读 25 条版本记录。保留旧图观察范围；本轮未重看或新增图片。')
s=s.replace('2025-08-15 发布/生效，§二基础功能、§六个人权利。','2025-08-15 发布/生效，§二基础功能、§四（一）4 关联产品账号/数据边界、§六个人权利。')
s=s.replace('02/03/04 的 `visual_observed=true` 仅指已查看的静态图。','02/03/04 的 `visual_observed=true` 仅指前轮已查看的静态图；本轮无新增视觉观察。')
s += '''
2026-10-27 有限补查：复读已保存前端和家长政策，并仅重新 GET 家长 App Store。先后做 20 条有界查询（其中最后 4 条专找 S6/E1 作者实拍/实用记录）；未取得同型号完整操作指南或能核对连续界面的一手正文。两篇什么值得买候选在搜索索引呈现多来源汇编，正常打开超时；一篇知乎自述体验候选返回 403 后停止。它们并非仅因非官方被排除，而是本轮无法核定作者、样机/构建和实际界面，未从摘要补按钮。视频索引仅发现，未观看、下载或算观察；S2、T6、更新同名代际等材料未移植。本例仍为 6 个任务：5 个 D1，家长账号注销局部 D2。

本轮原件定位、来源范围及失败记录见 [独立来源摘记](../artifacts/xiaoyuan-hardware-controls-deepening/source-notes.md)。下一类有用材料是标明 S6/E1 机型与固件的随箱说明书、机内指南或作者原创连续截图，至少呈现同一道题的错误反馈、重试、结束及暂退恢复；仅销售页新增卖点不能填这些缺口。
'''
case.write_text(s)
notes='''# M-056 本轮来源摘记（2026-10-27）

范围固定为已取得官网前端的 S6 条目、彩墨 E1 以及配套家长 App。0 新 E、0 新 F；下面复用旧编号。本轮只读文字，无新静态图观察、视频观看或设备操作。产品文案是 D1；政策注销路径只是局部 D2。目录数量不是完成数量。

## E-XYHW-01：官网前端复读

[原入口](https://www.xiaoyuanyingjian.com/)；原取得文件 [main.12d356ac.chunk.js](https://www.xiaoyuanyingjian.com/static/js/main.12d356ac.chunk.js)。本轮没有重新联网取得此文件。原件 `work/learning-hardware/official-main.js`；SHA-256 `f3e9acdba2af4c5de5cd342a81d0300d57c5748d868c75338b98d1f112d092ec`。解码文本同目录 `official-main-decoded.txt`，定位按 Unicode 字符偏移，不是行号。

约 26367 的年级/教材注释限定资源差异；`Te` 约 42700–44000：S6/E1 规格分列；`rn` 约 90400–91300：出题、批改、错题归纳/讲解、弱项和类题六模块；约 74858/75339 的图片替代文字标出语文听写/英语听力，未新看图；约 108917–109350 列书目、文件阅读及阅读灯；`Rn` 约 109700–110800：小程序上传及 Word/图片/PDF 分类；约 179695：S2/S6 同组件。手册、指南、教程等检索无匹配帮助入口；“帮助”命中为政策/一般文案。页面没有固件或发布日期，不能确认所示 S6 属于最新代际。所有模块缺机内进入、答错、完成和恢复链；没有请求任何埋点或学习接口。其他型号文案不纳入。

## E-XYHW-02：家长商店新快照

[中国区 iPhone 商店](https://apps.apple.com/cn/app/%E5%B0%8F%E7%8C%BF%E6%99%BA%E8%83%BD%E8%AE%BE%E5%A4%87/id6738277050)。2026-10-27 普通匿名 GET，HTTP 200。原件 `work/xiaoyuan-hardware-controls-deepening/parent-store.html`；SHA-256 `eb9a6caaa2efc25caaf05036db03983050e21eef75fcd9e42ec2e5c23921de2d`。

正文版本 1.25.1；对应 HTML `time datetime="2026-09-25"`，嵌入版本详情首项也为 1.25.1；兼容性 iOS 13.5 或以上。25 个版本详情节点的日志仅泛化改善/修复，未指明硬件机型、实际修复缺陷或恢复步骤。不将“修复历史问题”解释成某个故障已解决。新快照未查看截图或采用评论；原 E 的四张静态图观察仍属于前轮。

## E-XYHW-05：家长政策原件复读

[入口](https://megrez.yuanfudao.com/megrez-parent-web/privacys/privacy)引[正文](https://solar-online.fbcontent.cn/megrez-online/megrez-gallery/megrez-resource/1a017eac33f2fb4.html)，2025-08-15 发布/生效。本轮未重新 GET。原件 `work/learning-hardware/parent-privacy-body.html`；SHA-256 `cda62866625f96c6bffbfb25a7c095dbf788e5c1621f47cd118ddc1a161a3b01`。

解析文本 `parent-privacy-body.txt`：75 行共享登录；217 行关联产品独立处理/分别注销；274 行家庭信息修改；277 行备份删除延迟；281 行注销条件及验证码；282–285 行个人信息副本请求、身份核实后十五天答复。共享登录不证明学习数据迁移；服务端备份保留不是找回权限；副本请求不是可回灌的学习备份。130–147 行发送文件/题目及管控是能力和权限描述，不足以拼出硬件执行链。没有请求个人数据、联系客服或实施注销。

## 目录与负结果

[逐功能台账](../../batches/xiaoyuan-hardware-controls-deepening-inventory.csv)区分正文、既有静态记录和未核完整流程。原 E-XYHW-03/04 静态观察只复用旧记录，本轮未新增查看。

本轮 20 条查询见 `work/xiaoyuan-hardware-controls-deepening/discovery.json`。有限补查候选 [什么值得买 A](https://post.smzdm.com/p/anvdke93/) / [B](https://post.smzdm.com/p/anvonl3p/) 的正常打开均超时；[知乎候选](https://zhuanlan.zhihu.com/p/2028848047052826615)返回 403 后停止。索引出现自述与多来源汇编，未取得可核作者/机型/构建的原始界面，不能用摘要建立任务。另有同名新代 S6 新闻索引，未据此更新规格或迁入功能。没有下载音视频、固件或 APK，没有把旧 S2 或 M-028 手机软件套入 S6/E1。
'''
(A/'source-notes.md').write_text(notes)
fields=['inventory_id','product_id','node_type','function_domain','source_locator','evidence_ids','flow_ids','model_scope','body_status','visual_status','applicability','remaining_gap','checked_on']
rows=[]
def inv(domain,loc,e,f,scope,gap,status='body_reread_marketing',node='product_function',app='scope_conditioned_existing_record'):
 rows.append(dict(zip(fields,[f'XYH-{len(rows)+1:02d}','M-056',node,domain,loc,e,f,scope,status,'not_observed',app,gap,'2026-10-27'])))
s6='官网前端 S6 条目；代际/固件未知';e1='彩墨 E1；课程/固件未知';pa='家长App；iPhone1.25.1商店；政策2025-08-15未标构建'
for name,gap in [('智能出题','进入教材/课时、题数及推题触发未知'),('AI批改','提交入口、空白/部分答案、手写识别纠正未知'),('错题归纳','归档/订正/移除条件未知'),('错题讲解','提示层级、视频入口和再答是否计独立答对未知'),('薄弱点专练','薄弱判定与生成/结束条件未知'),('举一反三','原题到新题入口、队列与重试规则未知')]:inv(name,'decoded rn 90400–91300','E-XYHW-01','F-M056-01',s6,gap)
for name,loc,gap in [('教材同步练','decoded约64931','年级/教材版本选择与退出恢复未知'),('专项突破','decoded约60645','题型目录、提示与完成门槛未知'),('智慧拍','decoded约74030；视频链接未观看','拍摄/裁剪/识别/失败处理未知'),('文档传输','decoded Rn 109700–110800','小程序入口后选择、接收、打开与删除未知；E1不确定')]:inv(name,loc,'E-XYHW-01','F-M056-05' if name=='文档传输' else 'F-M056-01',s6,gap)
for name,loc,gap in [('语文听写','decoded74858图片alt文字，未新看图','开始/暂停、答题与听写错误反馈未知'),('英语听力','decoded75339图片alt文字，未新看图','音频选择、暂停/续播及离线未知'),('内置书目与文件阅读','decoded108917–109350','目录、打开/定位、标注及阅读恢复未知'),('阅读灯档位','decoded109015','控制入口、档位与生效状态未知')]:
 inv(name,loc,'E-XYHW-01','',s6,gap)
for name in ['拼音','识字','口算','思维闯关','自然拼读','中英文分级阅读']:
 inv(name,'旧E-XYHW-03功能目录图记录','E-XYHW-03','F-M056-02',e1,'课程入口、单题反馈/重试、完成与暂退恢复未知','prior_static_record_reused')
inv('课程S1/S2/S3分龄','旧E-XYHW-03课程分龄表','E-XYHW-03','F-M056-02',e1,'选择/跳级/回退未知；级别不是硬件型号','prior_static_record_reused')
inv('防挫折设计反馈','旧E-XYHW-04示意图','E-XYHW-04','F-M056-02',e1,'研发回路不能当儿童实时提示/奖励状态','prior_static_record_reused')
for name,f,gap in [('时段与每日时长','F-M056-03','上级入口、保存、生效、到时锁定及离线解锁未知'),('答案管控','F-M056-03','开关示例非默认；未核适用型号、保存或同步'),('学习轨迹','F-M056-04','计时口径、离线积压、答错状态明细未知'),('家庭家长列表','F-M056-04','邀请/退出反馈、角色权限、拒绝与撤回未知')]:inv(name,'旧E-XYHW-02四张静态构图记录','E-XYHW-02',f,pa,gap,'prior_static_record_reused','parent_control')
inv('发送文件和题目打印','政策§二(4)，txt130–143','E-XYHW-05','F-M056-05',pa,'关联设备范围、传输确认、打印反馈及错误恢复未知','policy_capability_declaration','parent_control')
inv('家庭资料更正','政策§六.2，txt274','E-XYHW-05','F-M056-04',pa,'仅家庭模块入口；具体编辑项/保存结果未知','policy_partial_path','account_data')
inv('共享登录与独立注销','政策§二、§四(一)4，txt75/217','E-XYHW-05','F-M056-06',pa,'共享手机号不证明进度跨产品迁移','policy_boundary_read','account_data')
inv('家长账号注销','政策§六.5，txt281','E-XYHW-05','F-M056-06',pa,'失败验证码、儿童设备遗留数据与重建家庭恢复未知','policy_step_chain_read','account_data')
inv('后台备份删除时序','政策§六.3，txt277','E-XYHW-05','F-M056-06',pa,'无用户恢复入口或明确删除完成回执','policy_boundary_read','account_data')
inv('个人信息副本请求','政策§六.6–7，txt282–285','E-XYHW-05','F-M056-06',pa,'须登录；客服请求与验证条件；副本格式/交付/回灌未知','policy_partial_path','account_data')
inv('家长App版本记录','新商店1.25.1；25项TitledParagraph','E-XYHW-02','F-M056-03;F-M056-04',pa,'泛化修复无硬件适配表/具体修复步骤','version_history_generic','version_directory')
with (B/'xiaoyuan-hardware-controls-deepening-inventory.csv').open('w',newline='') as f:
 c=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');c.writeheader();c.writerows(rows)
(B/'xiaoyuan-hardware-controls-deepening-evidence.jsonl').write_text('')
with (B/'xiaoyuan-hardware-controls-deepening-coverage.csv').open('w',newline='') as f:
 c=csv.DictWriter(f,fieldnames=list(json.loads((W/'coverage-before.json').read_text())[0]),lineterminator='\n');c.writeheader()
oldf=json.loads((W/'coverage-before.json').read_text()); updates=[]
changes={
'F-M056-01':{'known_branches':'已取得官网前端S6六模块目录；页面代际与固件未核定','gaps':'年级/教材影响资源；题目实际入口、手写识别纠正、提示重试、完成及暂退恢复未知；不能套后续同名S6或旧S2'},
'F-M056-02':{'gaps':'选级提示纠错重试结束恢复未知；课程S1/S2/S3不是硬件代际；本轮无新增静态图观察'},
'F-M056-03':{'known_branches':'前轮静态管控选项及示例；家长iPhone商店1.25.1日期2026-09-25','gaps':'完整入口保存生效超时解锁离线未知；25条泛化版本记录未补具体硬件行为'},
'F-M056-04':{'known_branches':'前轮示例时间线及家长列表；政策列我的到家庭模块更正信息入口','gaps':'数据口径角色权限邀请恢复未知；共享登录不证明学习状态迁移'},
'F-M056-05':{'gaps':'E1适用性文件限制同步打开打印恢复未知；S6小程序上传与家长App政策发送不能拼成已验证全链'},
'F-M056-06':{'known_branches':'注销确认家庭解绑条件与短信验证码；关联产品须分别注销/要求删除；后台备份可延迟到更新时删除','gaps':'验证码失败、重新绑定儿童进度未知；客服副本请求未给格式/回灌；后台备份不是用户找回承诺'}
}
for r in oldf:
 for field,after in {**changes[r['flow_id']],'checked_on':'2026-10-27'}.items():
  if r[field]!=after:updates.append({'flow_id':r['flow_id'],'source_batch':'docs/research/interaction-library/batches/learning-hardware-coverage.csv','field':field,'before':r[field],'after':after,'reason':'复读同源补型号/政策/版本边界；学习主链和深度不变'})
js(W/'existing-coverage-updates.json',updates)
olde=json.loads((W/'evidence-before.json').read_text()); em={x['evidence_id']:x for x in olde}; caps=[]
for eid,raw,mode,loc in [('E-XYHW-01','work/learning-hardware/official-main.js','local_reread_of_existing_public_artifact','Te/rn/Rn/S2-S6路由；解码字符偏移详独立摘记'),('E-XYHW-02','work/xiaoyuan-hardware-controls-deepening/parent-store.html','ordinary_anonymous_GET','time datetime=2026-09-25；1.25.1；25条版本日志；iOS13.5'),('E-XYHW-05','work/learning-hardware/parent-privacy-body.html','local_reread_of_existing_public_artifact','2025-08-15政策§二/§四(一)4/§六.2、3、5、6、7')]:
 e=em[eid]; cap={'capture_url':e['url'],'checked_on':'2026-10-27','original_capture_on':'2026-10-27' if eid.endswith('02') else '2026-10-26','access_mode':mode,'raw_work_path':raw,'raw_sha256':sha(R/raw),'locator':loc,'archived_artifact':'artifacts/xiaoyuan-hardware-controls-deepening/source-notes.md','sha256':sha(A/'source-notes.md'),'hands_on':False,'visual_observed':False,'limitation':'本轮只读文字；旧静态观察不扩展。硬件固件/代际与完整主链未核定。'}
 if eid.endswith('01'):cap['retrieval_request']=e['retrieval_request']
 if eid.endswith('02'):cap.update({'http_status':200,'source_updated_on':'2026-09-25','version_scope':'China iPhone App Store 1.25.1; iOS13.5+; paired hardware unknown'})
 if eid.endswith('05'):cap['resolved_source_url']='https://solar-online.fbcontent.cn/megrez-online/megrez-gallery/megrez-resource/1a017eac33f2fb4.html'
 before=e.get('additional_captures',[]);caps.append({'evidence_id':eid,'source_batch':'docs/research/interaction-library/batches/learning-hardware-evidence.jsonl','field':'additional_captures','before':before,'after':before+[cap],'reason':'同URL复读/新快照；保留原件与旧观察，不新增E'})
js(W/'existing-evidence-additional-captures.json',caps)
efields=[]
for eid,field,after in [('E-XYHW-01','version_scope','China mainland S6/E1 as represented in captured main.12d356ac.chunk.js; page release/generation and firmware unknown'),('E-XYHW-02','source_updated_on','2026-09-25'),('E-XYHW-02','note','Four static developer compositions viewed in prior capture; only one retained formally. Sample values are not defaults. Original date row showed Sep 25 without year; fresh 2026-10-27 HTML time datetime confirms 2026-09-25 for 1.25.1. Twenty-five version notes are generic; no specific hardware repair inferred. No new images or user reviews used.')]:
 efields.append({'evidence_id':eid,'source_batch':'docs/research/interaction-library/batches/learning-hardware-evidence.jsonl','field':field,'before':em[eid].get(field),'after':after,'reason':'保留来源快照代际与版本日期范围'})
js(W/'existing-evidence-field-updates.json',efields)
print(json.dumps({'new_E':0,'new_F':0,'inventory_rows':len(rows),'old_F_fields':len(updates),'old_E_additional_capture_fields':len(caps),'old_E_metadata_fields':len(efields),'case_sha256':sha(case),'source_notes_sha256':sha(A/'source-notes.md')},ensure_ascii=False,indent=2))
