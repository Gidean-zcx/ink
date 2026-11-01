from pathlib import Path
import csv, hashlib, json, collections

ROOT=Path('.'); W=ROOT/'work/hanvon-controls-deepening'
L=ROOT/'docs/research/interaction-library'; B=L/'batches'; A=L/'artifacts/hanvon-controls-deepening'
A.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
dump=lambda p,x:Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
case=L/'cases/M-008-hanvon.md'
assert sha(case)==json.loads((W/'baseline.json').read_text())['case_sha256'], 'case changed since baseline'
oldE=json.loads((W/'evidence-before.json').read_text()); oldF=json.loads((W/'coverage-before.json').read_text())
captures=[json.loads(x) for x in (W/'captures.jsonl').read_text().splitlines()]
cap={x['key']:x for x in captures}
U={x['evidence_id']:x['url'] for x in oldE}
U.update({'E-HVCD-01':cap['itsjzx']['url'],'E-HVCD-02':cap['sina-review']['url']})

notes='''# M008 二代资料补读：来源与限制

查阅 2026-10-27。只读正文，没有设备操作、浏览器操作、图片观察或媒体下载。二代身份来自标题及正文，固件/应用构建、供稿或商业合作关系未核。作者自称使用不能替代本项目实测，两文均含推荐性语言；不采用性能、续航或准确率结论。

| E | 正文定位 | 本轮采用与限制 |
| --- | --- | --- |
| E-HVCD-01 | [IT手机资讯](https://itsjzx.com/ai/44320)，非洲小野马，2025-05-06；开箱、创作/会议/阅读部分 | 输入键映射和微信绑定前提，正文没有配套菜单序列、失败反馈或恢复步骤。配件中提到快速手册不等于已取得手册。 |
| E-HVCD-02 | [新浪众测](https://zhongce.sina.com.cn/article/view/189952)，木木滚滚gg，2025-05-30 19:26；操控、系统、语音部分 | 灵动条入口清单及作者描述的切换用途；不证明每次切换保存原页面，也不证明后台服务持续运行。 |
| E-HANV-03 | 旧原件 hanv399.txt；“N10 Pro二代”至下一产品标题前 | 本地复读官方能力段；没有新取官网。后面的 Clear6 Turbo 段不属于本机。 |
| E-HND-01 | 旧 hanvon-review.html 提取正文；阅读/分屏/传输段 | 菜单功能名称与保存声明，不补未给出的手势、存储位置、生命周期。旧两张照片本轮未再查看，仍按 2026-10-26 观察记录引用。 |

IT文把笔键的一项称作“手触”；该词没有给出动作语义，不能推翻官方去掉屏幕手触层的说明，也不能补成手指触控开关。新浪笼统描述九键可自定义，IT文区分中央键和其余八键；目前不足以定为同固件映射或证实差异，案例保留分别归因。正文中的 Android 14 是作者报告，不能确定设备构建或更新后的行为。

检索边界：旧官网 web 路径405、旧下载栏目未见本机手册，仅沿用旧采集；没有重新请求这些路径。二代专用全文手册/官方操作教程未取得。代际不明的228页N10pro预览 E-HND-04 继续排除，2024 N10恢复视频也不转用。

本轮普通GET：IT手机资讯和新浪均200且有文章正文；知乎比较文403后停止；什么值得买文章虽200，HTML只有验证码脚本，文本为空，按访问失败停止，未执行验证码/POST/重试。失败正文只留work，不设正向功能证据。精确URL、状态、字节及哈希见work交接和captures.jsonl。

新批没有新增F；原六F均D1。功能台账是已取得资料的栏目/功能/缺口目录，不是完整机内菜单，也不将父栏目与子项重复计作任务。后台、听书中断、导出再编辑、同步删除/恢复及重装后数据都仍待本机版本证据。
'''
(A/'source-notes.md').write_text(notes)

evidence=[]
for eid,key,pub,author,title,loc in [
 ('E-HVCD-01','itsjzx','2025-05-06','非洲小野马','N10 Pro二代作者使用报告：实体输入与绑定前提','开箱展示；使用体验中的创作、会议、阅读段'),
 ('E-HVCD-02','sina-review','2025-05-30','木木滚滚gg','N10 Pro二代作者使用报告：灵动条与应用入口','使用体验：灵动操控、智能开放系统、语音转写')]:
 c=cap[key]
 evidence.append(dict(evidence_id=eid,product_id='M-008',title=title,url=c['url'],checked_on='2026-10-27',source_type='firsthand_reviewer_report',access_mode='ordinary_anonymous_GET_text_only',version_scope='China Hanvon N10 Pro second generation explicitly named by author; Android 14 author-reported; firmware/app build unknown',app_build=None,firmware_version=None,source_updated_on=None,source_published_on=pub,source_author=author,locator=loc,note='作者自述使用；推荐性文章非官方指南。仅采用正文明确控制/条件，未取得完整结果、异常或恢复链；全D1。',hands_on=False,visual_observed=False,supersedes=None,raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],extracted_work_path=c['extracted_work_path'],extracted_sha256=c['extracted_sha256'],archived_artifact='artifacts/hanvon-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),capture_state='raw_source_in_work_and_short_note_archived'))
(B/'hanvon-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in evidence))
with (B/'hanvon-controls-deepening-coverage.csv').open('w',newline='') as f:
 csv.DictWriter(f,fieldnames=list(oldF[0]),lineterminator='\n').writeheader()

changes={
 'F-M008-01':dict(evidence_ids='E-HANV-03;E-HND-01',known_branches='官方列PDF/图片输出；旧评测正文另列复制、笔记、划线、查找、百科/AI及全局批注保存声明，未给选中/调用步骤。',gaps='原稿/输出副本/全局批注关系，选页/批量、导出位置、格式层与再编辑、撤销、覆盖/取消。'),
 'F-M008-03':dict(evidence_ids='E-HANV-03;E-HVCD-01;E-HVCD-02',known_branches='作者列灵动条语音转写入口及离线转写模式；只有入口/能力描述，未证明离线生成纪要或音频、转写、摘要共同保存。',gaps='开启离线模式的菜单、录音停/继续、锁屏后台、说话人改名、原音回听、AI联网失败、原稿追溯及导出。'),
 'F-M008-04':dict(evidence_ids='E-HANV-03;E-HND-01;E-HVCD-01',known_branches='作者称微信需先绑定、聚合网盘绑定后传输；没有授权页、方向或落点。“传输/同步”不能推定自动合并或备份。',gaps='微信/网盘账号权限、接收目录、导入进度、重名/中断、云离线、删除传播、取消/回滚与付费条件。'),
 'F-M008-06':dict(evidence_ids='E-HND-01;E-HND-03;E-HVCD-01',known_branches='旧静态有类型/位置/方向及取消/确定；评测正文另提分屏笔记用途，没有提供进入动作或执行后状态。',gaps='入口、第二内容选择、实际布局、取消是否丢更改、退出保存/回原页与笔记独立导出未知。')}
updates=[]
for before in oldF:
 if before['flow_id'] not in changes:continue
 after={**before,**changes[before['flow_id']],'checked_on':'2026-10-27'}
 updates.append(dict(flow_id=before['flow_id'],before=before,after=after,changed_fields={k:{'before':before[k],'after':after[k]} for k in before if before[k]!=after[k]}))
dump(W/'old-coverage-updates.json',updates)

additional=[]
for eid,raw,txt,loc in [
 ('E-HANV-03','work/ai-notebooks-research/hanv399.html','work/ai-notebooks-research/hanv399.txt','N10 Pro二代能力段，止于Clear6 Turbo标题'),
 ('E-HND-01','work/child-information-deepening/hanvon-review.html','work/hanvon-controls-deepening/raw/old-review.txt','已存HTML本地提取，阅读/全局批注/分屏/网盘段')]:
 e=next(x for x in oldE if x['evidence_id']==eid)
 capture=dict(checked_on='2026-10-27',access_mode='local_saved_source_reread_no_network',locator=loc,raw_work_path=raw,raw_sha256=sha(raw),extracted_work_path=txt,extracted_sha256=sha(txt),hands_on=False,visual_observed=False,archived_artifact='artifacts/hanvon-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),note='复读旧原件，不新增URL证据；本次未重新查看配图。')
 additional.append(dict(evidence_id=eid,url=e['url'],before_additional_captures=e.get('additional_captures',[]),append_additional_captures=[capture]))
dump(W/'old-evidence-additional-captures.json',additional)

rows=[]
def row(kind,domain,item,eids='',flow='',loc='',status='body_read_D1',gap='',parent='',scope='N10 Pro二代；固件未知'):
 rows.append(dict(inventory_id=f'I-HVCD-{len(rows)+1:03}',row_kind=kind,parent_id=parent,domain=domain,item=item,model_version_scope=scope,source_url=';'.join(U[e] for e in eids.split(';') if e),locator=loc,coverage_status=status,evidence_ids=eids,flow_ids=flow,gap=gap))
row('source_index','官方','产品列表/二代详情','E-HANV-01;E-HANV-02',loc='二代独立标题',status='prior_capture_identity_only',gap='本轮不重取；产品图未读')
row('source_index','官方','型号能力报道','E-HANV-03',loc='N10 Pro二代段',status='local_body_reread_D1',gap='不是手册；其他机型段排除')
row('source_index','官方','程序及驱动栏目','E-HANV-04',loc='旧下载索引',status='prior_capture_only',gap='旧采集未见本机手册；不是手册不存在的证明')
row('source_index','第三方','二代评测正文','E-HND-01',loc='阅读/分屏/网盘段',status='local_body_reread_D1',gap='无构建号，图片本轮未重看')
row('source_index','第三方','N10pro 228页预览','E-HND-04',loc='标题与初始目录',status='excluded_generation_unverified',gap='不能作为二代手册',scope='代际未知，不纳入M008功能')
for label in ['开箱','使用体验','创作部分','会议部分','阅读资源部分','结尾评价']:
 row('source_heading','IT作者栏目',label,'E-HVCD-01',loc=label,status='heading_and_body_read',gap='栏目不是完整功能路径')
for label in ['外观','使用体验','阅读体验','操控','手写','开放系统','语音与纪要','AI助手','结尾评价']:
 row('source_heading','新浪作者栏目',label,'E-HVCD-02',loc=label,status='heading_and_body_read',gap='栏目不是机内菜单')
row('reported_control','系统输入','中央圆键/另外八键','E-HVCD-01',loc='开箱实体键段',gap='短长按映射见case；配置入口/当前固件未知')
row('reported_control','系统输入','电源键','E-HVCD-01',loc='开箱接口段',gap='锁屏不等于后台存活；关机确认/强制重启未知')
row('reported_control','系统输入','B6三个笔键','E-HVCD-01',loc='创作部分',gap='“手触”语义不明；可配置范围与设置入口未知')
row('reported_ui_group','系统导航','灵动条','E-HVCD-02',loc='操控段',gap='子项另列；父子不重复计功能/任务')
parent=rows[-1]['inventory_id']
for item,flow in [('首页',''),('手写笔记','F-M008-01'),('书架','F-M008-01'),('书城',''),('语音转写','F-M008-03'),('应用',''),('自定义键',''),('设置','')]:
 row('reported_ui_entry','系统导航',item,'E-HVCD-02',flow,'操控段',gap='入口名称，不证明切换后的数据保留或子菜单',parent=parent)
row('reported_feature','阅读','复制/笔记/划线/查找/百科/AI','E-HND-01','F-M008-01','阅读段','local_body_reread_D1','选择对象、入口、回原文与保存未知')
row('reported_feature','阅读','全局批注','E-HND-01','F-M008-01','阅读段','local_body_reread_D1','保存声明不等于写回原文件；批注层和导出未知')
row('capability','阅读','PDF OCR及版面调整','E-HANV-03','F-M008-02','型号段','local_body_reread_D1','本机菜单、扫描语言、联网和回原版式未知')
row('prior_static_control','听书','声音/音量/语速/定时/开始','E-HND-02','F-M008-05','旧听书设置照片','prior_static_observation_only','2026-10-26观察；启动/到期/锁屏/续播未看')
row('prior_static_control','分屏','类型/位置/方向/取消/确定','E-HND-03','F-M008-06','旧分屏照片','prior_static_observation_only','2026-10-26观察；结果/退出未看')
row('reported_feature','分屏','分屏笔记用途','E-HVCD-01','F-M008-06','阅读部分',gap='无进入、内容关联、保存及独立导出链')
row('capability','笔记','文档批注输出PDF/图片','E-HANV-03','F-M008-01','型号段','local_body_reread_D1','菜单、页范围、原稿关系、再编辑与错误未知')
row('capability','录音','分角色/离线转写/纪要','E-HANV-03','F-M008-03','型号段','local_body_reread_D1','纪要是否联网、原音与结果对象关系未知')
row('reported_condition','录音','离线转写模式','E-HVCD-01','F-M008-03','会议部分',gap='进入动作、停/继续与保存时刻未给')
row('capability','传输','Wi-Fi/蓝牙/微信/百度网盘/扫描王/数据线','E-HANV-03','F-M008-04','型号段','local_body_reread_D1','通道清单，不是各渠道已验证流程')
row('reported_condition','传输','微信先绑定','E-HVCD-01','F-M008-04','会议部分',gap='授权范围、接收位置/状态、取消和解绑未知')
row('reported_condition','传输','聚合网盘绑定','E-HND-01','F-M008-04','传输段','local_body_reread_D1','账户权限、方向、删除与冲突未知')
row('reported_feature','应用','应用市场与浏览器安装声明','E-HVCD-01',loc='阅读资源部分',gap='只声明渠道；权限、完成反馈、卸载、数据保留未知')
row('capability','AI','多模型与生成式任务','E-HANV-03','F-M008-03','型号段','local_body_reread_D1','全局数据访问、引用、改要求/回写未核')
for domain,item,flow,gap in [
 ('系统','首次设置/当前固件/完整手册','','需要机内关于页及明确二代完整指南'),
 ('阅读','目录/书签/页码跳转/搜索回原页','F-M008-01','需要本机入口、结果、返回及进度保存'),
 ('听书','后台/锁屏/中断续播','F-M008-05','需版本一致的过程与中断状态，不能由锁屏键推断'),
 ('系统','任务管理/后台清理/刷新模式','','未取得本机菜单和作用范围；长按刷新不等于刷新模式选择'),
 ('笔记','导出副本/再编辑/删除撤销','F-M008-01','需格式样本、对象标识、导出路径及恢复动作'),
 ('数据','备份/同步删除/账户切换','F-M008-04','传输不能代替可恢复备份；未知不写不支持'),
 ('恢复','更新/重装/重置后的数据','','未得二代版本日志、备份范围或恢复手册')]:
 row('gap',domain,item,flow=flow,status='not_established',gap=gap)
with (B/'hanvon-controls-deepening-inventory.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');wr.writeheader();wr.writerows(rows)
dump(W/'inventory-summary.json',dict(total=len(rows),by_kind=dict(collections.Counter(x['row_kind'] for x in rows)),new_flows=0,new_evidence=2,updated_old_flows=len(updates),updated_fields=sum(len(x['changed_fields']) for x in updates)))

text=(W/'case-before.md').read_text()
text=text.replace('查阅2026-10-26。固定对象','初查2026-10-26，本轮补读2026-10-27。固定对象',1)
text=text.replace('这里的F行是待深化的任务单位，**操作栏明确空缺**，不得作为操作指南或已验证流。','这里的F行是待深化的任务单位，**操作栏只有能力、入口或局部线索，完整执行链空缺**，不得作为操作指南或已验证流。')
text=text.replace('原稿与输出副本关系未知。 | E-HANV-03 | 选页/批量、导出路径、隐藏层、撤销、覆盖/取消。','原稿与输出副本关系未知；旧评测正文另列复制、笔记、划线、查找、百科/AI及全局批注保存，未说明选择/调用动作。 | E-HANV-03；E-HND-01 | 选页/批量、导出路径、全局批注层、再编辑、撤销、覆盖/取消。')
text=text.replace('官方列分角色及离线转写、自动会议纪要；没有取得逐屏执行链。','官方列分角色及离线转写、自动会议纪要；作者另列灵动条语音转写入口和离线转写模式，仍无逐屏执行链。')
text=text.replace('一起保存或一起删除。 | E-HANV-03 | 说话人修正、暂停/锁屏、断网后AI、原音回听、改指令、导出格式。','一起保存或一起删除，离线转写不证明纪要离线。 | E-HANV-03；E-HVCD-01～02 | 离线模式菜单、说话人修正、暂停/锁屏、断网后AI、原音回听、改指令、导出格式。')
text=text.replace('官方列多种传输与聚合网盘；未取得绑定、选择方向及接收页面序列。','官方列传输与聚合网盘；作者称微信须先绑定、网盘绑定后传输，未给授权、方向或接收页序列。')
text=text.replace('不能自动解释为冲突合并。 | E-HANV-03 | 云离线、重名、版本、删除、取消和回滚；是否需要付费。','不能自动解释为冲突合并或备份。 | E-HANV-03；E-HND-01；E-HVCD-01 | 授权范围、接收目录、云离线、重名/中断、删除传播、取消/回滚与付费。')
text=text.replace('弹窗选择类型、窗口位置和旋转方向，再取消或确定；入口、第二份内容选择、落地布局与退出后状态未观察 | D1，E-HND-03','弹窗可见类型、窗口位置、旋转方向及取消/确定；评测正文另提分屏笔记用途，未给进入动作。第二份内容选择、取消/退出后保存与独立导出未观察 | D1，E-HND-01；E-HND-03；E-HVCD-01')
anchor='## 有界任务记录\n'
insert='''## 2026-10-27 补读：输入与来源目录

本轮取得两篇作者自述使用的二代评测正文，未取得专用完整手册或型号化更新说明。它们含推荐性语言，固件均未知，不能作为官方承诺。没有新增任务：仍为六个D1，以下只补输入线索和已有任务条件。

非洲小野马（2025-05-06）写中央圆键短按返回、长按刷新，另外八键可配置；电源键短按锁屏、长按开关机。笔键一项称“手触”，含义未解释，不能推成重新启用屏幕手指触控。配置页面、关机确认和恢复状态均未给出。[E-HVCD-01]

木木滚滚gg（2025-05-30）列灵动条的首页、手写笔记、书架、书城、语音转写、应用、自定义键及设置入口。正文说可切换功能，但没有退出/保存验证。该文对九键自定义仅作概述；与前文“八键另加中央键”的细分未核同版本，不强行统一。[E-HVCD-02]

[逐项台账](../batches/hanvon-controls-deepening-inventory.csv)分开记录来源栏目、作者报告控件、旧静态控件、官方能力和未核项目；完整机内目录仍未取得，栏目/子项数量不是已完成任务数。来源身份、失败路线和局限见[本轮短摘记](../artifacts/hanvon-controls-deepening/source-notes.md)。

'''
text=text.replace(anchor,insert+anchor)
text=text.replace('全部查于2026-10-26；获取方式与版本未知字段写入批次JSONL。','旧来源初查2026-10-26；本轮于2026-10-27本地复读E-HANV-03、E-HND-01并读取两篇新正文。原两张静态图未在本轮重新观察；获取方式与版本未知字段写入批次JSONL。')
text+='''\n| E-HVCD-01 | [非洲小野马二代使用报告](https://itsjzx.com/ai/44320) | IT手机资讯，2025-05-06；匿名GET200正文，固件未知，非官方手册。 |
| E-HVCD-02 | [木木滚滚gg二代使用报告](https://zhongce.sina.com.cn/article/view/189952) | 新浪众测，2025-05-30；匿名GET200正文，固件未知，非本项目实测。 |
'''
text=text.replace(' |\n\n| E-HVCD-01',' |\n| E-HVCD-01')
text=text.replace('用随附笔与实体键分别触发菜单、返回、刷新，记录哪些区域支持手触。','用随附笔与实体键分别触发菜单、返回、刷新；核对笔键“手触”的实际含义，不预设屏幕能恢复手指触控。')
case.write_text(text)

failures=[dict(key='zhihu-compare',url=cap['zhihu-compare']['url'],http_status=403,result='access_denied_stopped',raw_work_path=cap['zhihu-compare']['raw_work_path'],raw_sha256=cap['zhihu-compare']['raw_sha256']),dict(key='smzdm-user',url=cap['smzdm-user']['url'],http_status=200,result='captcha_only_no_article_stopped',raw_work_path=cap['smzdm-user']['raw_work_path'],raw_sha256=cap['smzdm-user']['raw_sha256'],text_bytes=0,note='本地HTML有TencentCaptcha及/WafCaptcha POST脚本；均未执行。')]
dump(W/'access-failures.json',failures)
formal=[case,B/'hanvon-controls-deepening-evidence.jsonl',B/'hanvon-controls-deepening-coverage.csv',B/'hanvon-controls-deepening-inventory.csv',A/'source-notes.md']
dump(W/'formal-manifest.json',[dict(path=str(p),sha256=sha(p),bytes=p.stat().st_size) for p in formal])
print(json.dumps(json.loads((W/'inventory-summary.json').read_text()),ensure_ascii=False))
