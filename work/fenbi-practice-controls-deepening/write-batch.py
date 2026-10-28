from pathlib import Path
import json,csv,hashlib,shutil
R=Path('.');W=R/'work/fenbi-practice-controls-deepening';L=R/'docs/research/interaction-library';A=L/'artifacts/fenbi-practice-controls-deepening';A.mkdir(exist_ok=True)
B=json.loads((W/'baseline.json').read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();dump=lambda p,x:Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
notes='''# M032 本轮来源摘记（2026-10-27）

本轮只读公开正文。采用三篇历史投稿作范围受限的补证；页面“原创/真实经历”是作者自述，未独立核实操作过程。没有当前官方移动端操作手册；平台、科目、构建不明的材料不能补成现行行测主链。来源日期均为页面标示“更新”，不等于实际体验日期。没有登录、作答或提交。

## E-FBPC-01：2019错题教程

URL：https://jingyan.baidu.com/article/d2b1d102e627605c7e37d4e0.html

作者wenan一个人的文案；页面更新2019-01-12 12:50；系统/构建、考试类别及地区未注。正文方法1—7：方块菜单进入错题、选类别；作者称重答正确会移出，错误可看正确答案/解析，下滑看考点来源，再到笔记入口。未展示移出回执、撤销或笔记保存。本批仅用来补F04的历史叙述，不证实当前自动清理。

只实际查看方法1图片，见独立[静态记录](static-observation-log.md)。方法4图片GET403后停止该图片路线，后续图片未请求。未看到答对/错答状态，正文不能说成视觉观察。

## E-FBPC-02：2022快速练习漏答确认

URL：https://jingyan.baidu.com/article/6dad5075034816e023e36ef8.html

作者爱玩游戏君；页面更新2022-04-12 11:37，自报荣耀30、magic UI2.0.0、粉笔职教v6.16.25；科目/地区未注。方法1—4写快速练习、选答案、交卷；示范未全部作答，出现对话框后选择确定。没有对话框原文、取消结果或提交后的报告。全部图片未看；自报版本未由设备或图片复核。仅与F09已有2019国考材料并列，不串联成同一会话，不外推套卷/当前版本。

## E-FBPC-03：2022行测错题筛选入口

URL：https://jingyan.baidu.com/article/ca00d56c28bc50a89eebcff4.html

页面更新2022-12-21 11:05，标题明确行测；自报小米11、MIUI12.0.0.1、粉笔8.0.1，版本未由官方或图证核实。方法1—3为菜单→我的错题→选择做题时间和题目类型。正文到选择为止，未给筛选后数量、空列表、重答和恢复。图片未看。用于F04及目录的历史入口补证，不以8.0.1推断后续现行版本关系。

## 已有 E-FBCD-08：本地原件复读

URL：https://kefu.easemob.com/v1/tenants/8342/knowledge/entries/html/50542

原件work/fenbi-controls-deepening/kb-50542.html（本次未联网重抓）；2020-07-03署名粉笔，tenant归属反链仍未补齐。正文列练习页选考试后四方块内9个入口：用户报告、练习历史、我的错题、我的笔记、资料缓存、我的收藏、会员周报、我的1对1、我的体系课。本次逐项列入目录；只确认目录，不推导保存、删除或恢复。

## 其余候选的排除范围

本轮共8个新候选，详见work内candidate-register.json。3139083是2019产品分析，“我的分析/难题集锦”位于改进建议，不当现存功能；3331984为2020商业分析，缺实际作答链。Bilibili cv33546391的工具抽取正文与普通GET412分别记录，未继续访问，不能用标题推定按钮。2020下载打印及智能组卷两篇正文可读，但未核考试类别/版本、未看图；前者不能替代错题导出，后者只到交卷动作，未补状态与计时控制。下载站转载、教师/申论和其他考试系统只在搜索筛选时排除。

原件、失败响应及提取文本均在work；正式归档仅此短摘记及一张必要入口图。raw字段指原始HTTP响应字节；失败HTML/图后缀文件不代表成功图片。未取得完整官方帮助目录，本批功能目录是已得资料映射。
'''
(A/'source-notes.md').write_text(notes)
img=W/'raw/c05-wrong2019-img1.jpg';shutil.copyfile(img,A/'historical-menu-entry.jpg')
audit='''# 静态图实际查看记录

2026-10-27，E-FBPC-01，百度经验“粉笔app如何查看我的错题”方法1的第一张图；普通GET200，随后用view_image实际查看。作者页面更新2019-01-12，构建/平台未注明。

观察范围：上方右侧四块菜单图标被红框标注；底栏有练习、课程、发现、我。页面主体被作者模糊，考试类别、地区和题库内容读不清。只能佐证入口图标，不能证明错题反馈、答对自动移出、当前构建或连续操作。

归档：historical-menu-entry.jpg；SHA256：'''+sha(img)+'''

下一张拟看方法4图片返回403（非图片101字节），已停止同图片路线；其余图没有请求、没有观看。没有视频、音频、登录或设备操作。本记录独立于案例正文，避免案例修订影响视觉审计散列。
'''
(A/'static-observation-log.md').write_text(audit)
urls={
1:('粉笔app如何查看我的错题（2019投稿正文与入口图）','https://jingyan.baidu.com/article/d2b1d102e627605c7e37d4e0.html','c05-wrong2019','2019-01-12',None,'历史粉笔手机App；平台/构建/科目/地区未知','方法1—7；页面更新与作者声明'),
2:('粉笔职教快速练习漏答确认（2022投稿正文）','https://jingyan.baidu.com/article/6dad5075034816e023e36ef8.html','c06-quick2022','2022-04-12','6.16.25（作者自报）','2022作者自报荣耀30/magic UI2.0.0；科目/地区未知','工具/原料；方法1—4'),
3:('行测错题按时间与类型选择（2022投稿正文）','https://jingyan.baidu.com/article/ca00d56c28bc50a89eebcff4.html','c03-baidu','2022-12-21','8.0.1（作者自报，未核）','2022作者自报小米11/MIUI12.0.0.1；行测/地区未知','工具/原料；方法1—3')}
es=[]
for n,(title,url,stem,date,build,scope,loc) in urls.items():
 e=dict(evidence_id=f'E-FBPC-{n:02}',product_id='M-032',title=title,url=url,checked_on='2026-10-27',source_type='third_party_firsthand_claimed_tutorial',access_mode='opened_public_html',version_scope=scope,app_build=build,firmware_version=None,source_published_on=None,source_updated_on=date,locator=loc,note=['历史作者叙述；仅入口图实际查看，答对移出/错答解析未作视觉确认；科目不明不能保证当前行测规则。','作者明确示范漏答后有确认；未看图，没有弹窗原文/取消结果/报告，不与2019国考拼成同一会话。','行测时间/题型选择入口；自报8.0.1未官方核定；未看到选择后的列表和重答。'][n-1],hands_on=False,visual_observed=n==1,archived_artifact='artifacts/fenbi-practice-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),raw_work_path=str(W/'raw'/f'{stem}.html'),raw_sha256=sha(W/'raw'/f'{stem}.html'),capture_state='origin_html_and_independent_short_source_note',supersedes=None)
 if n==1:e.update(visual_audit_path='artifacts/fenbi-practice-controls-deepening/static-observation-log.md',visual_audit_sha256=sha(A/'static-observation-log.md'),additional_artifacts=[{'path':'artifacts/fenbi-practice-controls-deepening/historical-menu-entry.jpg','sha256':sha(img),'role':'only_method1_menu_entry_viewed'}])
 es.append(e)
(L/'batches/fenbi-practice-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in es))
fields=list(B['coverage'][0])
with (L/'batches/fenbi-practice-controls-deepening-coverage.csv').open('w',newline='') as f:csv.DictWriter(f,fieldnames=fields,lineterminator='\n').writeheader()
updates=[]
changes_by={
'F-M032-04':dict(task='错题重做与移出（规则分来源）',evidence_ids=B['coverage'][3]['evidence_ids']+';E-FBPC-01;E-FBPC-03',known_branches='官方备考文章仅功能声明；2020目录四方块与2026“我的”入口并列；2019作者称重答正确移出/错误看解析，科目构建未明；2022行测作者描述做题时间/题型选择',gaps='现行控件/执行结果仍未核；历史文字不证当前自动移出，未见回执/撤销/空列表/历史答案口径；2022自报8.0.1未官方核定；清理与同步未知',visual_status='static_reference_only',checked_on='2026-10-27'),
'F-M032-09':dict(task='历史快速练习：2019国考结果解析与2022漏答确认（分材料）',evidence_ids=B['coverage'][8]['evidence_ids']+';E-FBPC-02',known_branches='2019国考iOS6.8.8静态图：答题卡交卷入口、报告及全部/错题解析；2022作者自报6.16.25文字：快速练习未全部答完交卷有确认，科目/地区未知，未看图',gaps='两材料不得拼成同一会话或套当前App；2022无弹窗原文/取消结果/交卷后状态；2019未见改选跳题确认，返回目标未知；超时后台断网恢复均未知',checked_on='2026-10-27')}
for fid,afters in changes_by.items():
 old=next(x for x in B['coverage'] if x['flow_id']==fid);new={**old,**afters}
 source=[]
 for p in (L/'batches').glob('*coverage.csv'):
  if p.name.startswith('fenbi-practice-'):continue
  with p.open() as f:
   for x in csv.DictReader(f):
    if x.get('flow_id')==fid:source.append(str(p))
 assert len(source)==1,(fid,source)
 updates.append(dict(flow_id=fid,source_batch=source[0],changes={k:dict(before=old[k],after=v) for k,v in afters.items() if old[k]!=v},replacement_row=new))
dump(W/'existing-coverage-updates.json',updates)
e=next(x for x in B['evidence'] if x['evidence_id']=='E-FBCD-08');capture=dict(checked_on='2026-10-27',access_mode='local_reread_existing_origin_html_no_network',url=e['url'],locator='标题/2020-07-03署名/9项功能目录',raw_work_path='work/fenbi-controls-deepening/kb-50542.html',raw_sha256=sha('work/fenbi-controls-deepening/kb-50542.html'),archived_artifact='artifacts/fenbi-practice-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),hands_on=False,visual_observed=False,note='仅复读已保存正文并逐项列目录；具体tenant官方反链与各功能结束恢复仍未知')
before=e.get('additional_captures',[])
dump(W/'existing-evidence-additional-captures.json',[dict(evidence_id=e['evidence_id'],source_batch='docs/research/interaction-library/batches/fenbi-controls-deepening-evidence.jsonl',field='additional_captures',before_exists='additional_captures' in e,before=before,after=before+[capture],append=[capture])])
# Function inventory: mapped entries, not a complete official manual or completed flows.
rows=[]
def inv(domain,scope,eids,loc,fids,status,ver,gap,visual='false'):
 rows.append(dict(item_id=f'FBPC-I{len(rows)+1:02}',product_id='M-032',functional_domain=domain,public_material_scope=scope,evidence_ids=eids,source_locator=loc,flow_ids=fids,review_status=status,version_scope=ver,remaining_gap=gap,checked_on='2026-10-27',new_visual_observation=visual))
oldver='2020-07-03历史署名粉笔托管帮助；构建/系统未知；具体tenant官方反链未核'
for name,fids,gap in [('用户报告','','生成条件/空数据/删除影响'),('练习历史','','未交卷是否进入/继续入口/重做覆盖'),('我的错题','F-M032-04','清理确认/误删恢复/统计重算'),('我的笔记','','编辑保存/撤销/同步'),('资料缓存','F-M032-10','只知导出后查看；缓存清理/离线范围'),('我的收藏','F-M032-05','保存回执/去重/取消与同步'),('会员周报','','生成频率/权限过期/历史'),('我的1对1','','课后数据/作业恢复，超行测题库主链'),('我的体系课','','课表/课程完成，超行测题库主链')]:inv(name,'四方块下真实目录项','E-FBCD-08','唯一正文段',fids,'body_read_directory_only',oldver,gap)
inv('错题时间/题型选择','2022行测作者文字入口','E-FBPC-03','方法1—3','F-M032-04','historical_author_text_partial','小米11/MIUI12.0.0.1/8.0.1均作者自报未官方核','筛选结果/空列表/跨日边界')
inv('错题重答反馈','2019作者文字分支','E-FBPC-01','方法3—6','F-M032-04','historical_author_claim_not_current_rule','2019-01-12；科目/系统/构建未知','未看答案状态；移出回执/撤销/重答记录')
inv('四方块入口','仅方法1静态入口图','E-FBPC-01','方法1图片','F-M032-04','static_reference_only','作者更新2019-01-12，科目及构建不可见','主体被模糊；不证明答题反馈','true')
inv('快速练习漏答确认','2022文字：未全部答完交卷','E-FBPC-02','方法1—4','F-M032-09','historical_author_text_partial','荣耀30/magic UI2.0.0/6.16.25作者自报；科目地区未知','弹窗原文/取消/确认后的报告/恢复')
inv('快速练习报告解析','2019旧静态图覆盖','E-FBCD-07','四2.2第二图','F-M032-09','prior_static_observation_not_repeated','iOS6.8.8/国考；2019-06-24','不能用2022文字补成同一会话')
inv('模考报名/补练','官方FAQ正文已读旧证据','E-FENB-03;E-FBCD-03','参加/岗位/错过/往期报告','F-M032-01;F-M032-02','prior_body_mapping','内容记录2024-09-10；无App构建','本轮无新当前交卷/后台/断网证据')
inv('真题选择与解析','北京备考文章','E-FENB-07','地区/年份/解析','F-M032-03','developer_editorial_claim_only','构建未知；北京行测','选项/改选/交卷成功未核')
inv('套卷计时','备考文章声明','E-FENB-08;E-FBCD-06','计时用法','F-M032-06','developer_editorial_claim_only','2026-09-21文章；无构建','暂停/超时/后台保存；不引用快速练习确认代替')
inv('课程离线/PC机考','客户端目录旧快照','E-FENB-01;E-FENB-02;E-FBCD-01;E-FBCD-02','下载页/开发者说明','F-M032-08','prior_developer_claim_mapping','2026-10-26渠道快照；不重新声称当前','题库草稿离线未知')
inv('错题导出/纸面回录','2020托管帮助','E-FBCD-10;E-FBCD-12','导出/资料缓存/扫一扫','F-M032-10','prior_body_mapping','2020旧限额；构建未知','普通套卷下载不能等同错题导出；格式/失败/重复回录未知')
inv('搜题','关键词/图片入口','E-FBCD-11','行测搜题','', 'prior_body_partial','2020-07-06旧帮助','结果/无匹配/识别错误未闭合')
inv('公基结果解析课','独立旧科目','E-FENB-04','结果/解析课','F-M032-07','out_of_primary_exam_scope','事业单位公基；不可迁移行测','本轮不扩科目')
with (L/'batches/fenbi-practice-controls-deepening-inventory.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');wr.writeheader();wr.writerows(rows)
# Case changes preserve all old flows and sources.
s=(W/'case-before.md').read_text();s=s.replace('查阅 2026-10-26。','本轮补查 2026-10-27；渠道版本快照仍为2026-10-26。',1)
s=s.replace('当前完整真题/计时考试仍为 D1 缺口；','当前完整真题/计时考试仍为 D1 缺口；本轮新增历史作者正文未改变10条流程的深度（5D1/5D2）。')
s=s.replace('原备考文章确有入口及功能声明，但不能确定实际控件、执行后状态、清理规则；','原备考文章确有入口及功能声明，但不能确定实际控件、执行后状态、清理规则；历史作者叙述也不保证当前构建；')
s=s.replace('| F-M032-04 错题重做与清理 |', '| F-M032-04 错题重做与清理 |')
s=s.replace('文章声明自动汇集、筛选、重做/错因/移除；仅保留声明，不重构按钮状态','官方文章只作声明；新增2019作者称答对移出、答错看答案/解析；2022行测作者补时间/题型选择。均为独立历史文本，不证明当前执行')
s=s.replace('D1（本轮收紧）；E-FENB-05、06、E-FBCD-04、05、08。清理确认、误删恢复和首次/历次答案口径未知','D1；E-FENB-05、06、E-FBCD-04、05、08、E-FBPC-01、03。只实际看一张主体模糊的菜单入口图；移出回执、清理确认/撤销和历次答案口径未知')
s=s.replace('F-M032-09 历史国考快速练习→结果→解析','F-M032-09 历史快速练习：分版本材料')
s=s.replace('2019 iOS6.8.8；练习页“公务员行测-国考”→快速练习','2019 iOS6.8.8国考图；另有2022作者自报6.16.25文字（科目/地区未注）')
s=s.replace('D2，仅历史静态重建；E-FBCD-07。不是连续操作；未见选项改动、跳题、确认/漏题/超时、退出恢复；不把15写成所有练习固定题量','D2；E-FBCD-07、E-FBPC-02。2019图与2022正文不能拼为同一会话；后者写未全部作答交卷有确认，但未见弹窗原文/取消结果/交卷后状态。改选、超时与恢复未知；15题仅2019样例')
# Keep old diagram row historically accurate while adding explicit independent branch.
s=s.replace('图中报告为15题；解析下方有考点、来源和笔记入口。可见返回箭头，不补目标页或自动保存','2019图报告为15题，可见考点/来源/笔记及返回箭头，返回目标未知。2022正文只到漏答提示后选择确定；未看该文图片')
insert='''## 2026-10-27 历史补证、目录与边界

完整官方操作目录仍未取得。新增[逐项目录](../batches/fenbi-practice-controls-deepening-inventory.csv)共21项，含2020帮助四方块内9个入口、旧任务映射与本轮局部分支，不能按21项宣称已完成21个流程。

- E-FBPC-01：作者wenan一个人的文案，页面更新2019-01-12；科目/系统/构建不明。错题重答规则仅为作者正文，不能保证当前行测自动移出。实际只看方法1入口图；其余反馈图未看。
- E-FBPC-02：作者爱玩游戏君，页面更新2022-04-12；荣耀30、magic UI2.0.0、6.16.25是作者自报。漏答交卷提示仅适用于该文快速练习叙述，科目未知；不迁入北京真题、计时套卷或2019国考图。
- E-FBPC-03：2022-12-21行测错题教程补时间/题型选择；自报8.0.1未经官方核实，不能据此建立版本升级关系；未观察结果列表。

上述日期是页面更新时间，不是我们实操日期或经核定的版本发布日期。原F04仍D1，F09仅在既有D2任务内增加另一历史来源的有限分支。当前改选、漏答确认的具体文案与取消、超时、后台/断网保存，以及清错题恢复仍未核定。

8个新候选已收束：两篇产品分析缺核心控件或包含未实现的建议；Bilibili正文工具可读但普通GET412后停止；2020套卷打印/智能组卷正文与现有错题导出/计时对象不同且范围未核。百度图片首张200并实际查看，下一张403后停止该图片路线，没有换域/改参重试。来源详情与图证边界见[本轮摘记](../artifacts/fenbi-practice-controls-deepening/source-notes.md)及[独立静态记录](../artifacts/fenbi-practice-controls-deepening/static-observation-log.md)。旧E-FBCD-07审计已独立归档，不引用本案例散列，无需迁移。

'''
s=s.replace('## 数据、动力与本项目关系',insert+'## 数据、动力与本项目关系')
s+='''\n本轮新增（查阅2026-10-27）：\n\n- E-FBPC-01：[2019错题作者教程](https://jingyan.baidu.com/article/d2b1d102e627605c7e37d4e0.html)，方法1—7，正文与一张菜单入口图分开使用。\n- E-FBPC-02：[2022快速练习作者教程](https://jingyan.baidu.com/article/6dad5075034816e023e36ef8.html)，工具/原料、方法1—4；仅正文。\n- E-FBPC-03：[2022行测错题筛选作者教程](https://jingyan.baidu.com/article/ca00d56c28bc50a89eebcff4.html)，工具/原料、方法1—3；仅正文，自报版本未核。\n'''
s=s.replace('D1（本轮收紧）','D1（此前收紧）').replace('旧证据保留编号，旧批次不改：','来源按稳定编号保留：').replace('本轮新批均查阅 2026-10-26：','此前补证批次均查阅 2026-10-26：').replace('11493 本轮最后一次直接读取','11493 在2026-10-26最后一次直接读取').replace('本轮核对官方移动下载页','2026-10-26已核对官方移动下载页').replace('本轮复核见 E-FBCD-02','此前复核见 E-FBCD-02').replace('本轮按声明和建议使用','此前按声明和建议使用').replace('本轮可读正文及日期差异','此前可读正文及日期差异').replace('本轮直接正文；09-24/09-18','此前直接正文；09-24/09-18').replace('入口存在历史差异；“重做对后移除”不等于自动删除，更不等于已掌握','入口存在历史差异；历史作者的移出叙述不保证当前自动清理，更不等于已掌握')
(L/'cases/M-032-fenbi.md').write_text(s)
print(json.dumps(dict(new_evidence=len(es),new_flows=0,old_flows=len(updates),field_changes=sum(len(x['changes']) for x in updates),old_e_captures=1,inventory=len(rows),new_static_images=1),ensure_ascii=False))
