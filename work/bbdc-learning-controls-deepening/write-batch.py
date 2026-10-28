from pathlib import Path
import json,csv,hashlib,shutil
W=Path('work/bbdc-learning-controls-deepening');L=Path('docs/research/interaction-library');A=L/'artifacts/bbdc-learning-controls-deepening';A.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,x):Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
notes='''# 不背单词历史学习链：来源性质与范围

查阅2026-10-27。6个候选采用3个；无登录、安装、产品动作或音视频播放。当前官方完整主链仍缺，历史文字/图示不替代现行构建。

## E-BBLC-01 — FuzzFace，2021-03-26

https://www.woshipm.com/evaluating/4425622.html

原HTML `raw/c04-review2021.html`；3.1自报设备原文“Pihon6Splus”、iOS13.6.1、2021-03-19；版本表列不背V4.3。3.7.3含作者绘制的学词流程，以及四产品静态拼图；只采用其中不背区域。流程图可定位选错后答案/继续与下一词，不能声称实见判错动画。正文末段为扇贝改进方案，不迁入不背。学习效率及记忆效果评价不采用。

## E-BBLC-02 — 亭伊，2017-07-23

https://agirls.aotter.net/post/51654

原HTML `raw/c05-agirls2017.html`，Learn/拼写两段及相邻图。标题同时列Android/iOS，图中没有型号/构建号。正文称五次，不能由两张拼图核定计数规则。实际看图只确认辨识、释义/例句入口、辨识完成与拼写选择；未听音，未看到拼错或最终结算。不同词的拼写样例不构成同词提示前后对照。

## E-BBLC-03 — 胡鹤仙，页面2023-10-15

https://sspai.com/post/76700

原HTML `raw/c06-sspai2023.html`。“学习内容丰富”是四次答对及重学叙述；“学习奖励”把辨识和拼写完成分开。叙事回顾2022-11-06并称2022-11-18活动将开始；图片地址含2022/11/08。仅标历史经历，不把2023页面日期当实际体验日或构建。首图请求403后停止该图片域，整篇图片未看；没有把作者标为“官方小窍门”的图当已核原厂帮助。

## 未用于升级任务的三个候选

- C01 https://www.woshipm.com/evaluating/3548342.html ，Claire/2020-03-23。原HTML与文本可读；作者对结算分享/掌握的叙述受同页评论质疑，且无构建。其五秒及错词下一组描述不补现行控制。
- C02 https://www.sohu.com/a/579227156_819205 ，搜索有片段，但web.open返回内部400 Timeout fetching。没有另路重抓，不作为已读正文。
- C03 https://pdf.hanspub.org/ae20220700000_69509379.pdf ，王雅萱/2022-07-25，DOI10.12677/ae.2022.127378。PDF6页，第4页/印页2487图3已渲染查看：是作者流程图，非界面。无软件构建/可复核实验序列；四次/清零与效果解释不升为产品实现。PDF及派生页保存在work，未复制整篇到正式资料。

三次/四次/五次之类次数不能跨资料补齐算法；本批只实际取得四次与五次叙述，不采用其它搜索摘要的三次。原官方日志仍只证明其自身记录：例如尝试修复不等于修复已验证。原商店/政策/官网本批不重抓，旧403脚本/知乎、登录入口、微博阻断路线均未重试；B站只有检索线索，无播放或请求正文。

正式仅短摘记、独立静态记录和5张必要历史图。原HTTP文件及散列在work和证据raw字段；没有当前主链成功/备份恢复证据。
'''
(A/'source-notes.md').write_text(notes)
imgs=['c04-version-table.jpg','c04-flow.jpg','c04-study-ui.jpg','c05-first-word.jpg','c05-spelling.jpg']
for n in imgs:shutil.copyfile(W/'raw'/n,A/n)
audit='''# 静态材料实际查看记录

2026-10-27，均用view_image实际查看；无视频、音频、连续操作或产品交互。以下图来自作者文章，不是本项目截图。

| E/原文位置 | 已看文件 | 可见范围与不能推出的结论 |
| --- | --- | --- |
| E-BBLC-01/3.1 | c04-version-table.jpg | 作者对比表中不背V4.3、最近更新2021-03-17；不是官方版本验证。 |
| E-BBLC-01/3.7.3不背单词记忆流程 | c04-flow.jpg | 作者流程图中选择、错误、答案、继续及是否学完的连线；不是App截图。 |
| E-BBLC-01/单词学习主页 | c04-study-ui.jpg | 四App拼图，仅不背右侧crayon四选义/看答案、1/10及图标用于本例；没有选择后的状态。 |
| E-BBLC-02/Learn段相邻图 | c05-first-word.jpg | 同词begrudge的选义/不认识、释义/下一个/看例句，另有例句卡；只静态展示，不声称听到例句。 |
| E-BBLC-02/拼写段相邻图 | c05-spelling.jpg | 辨识完成弹窗含放弃/开始拼写；键盘、灯泡和1/20；左右为不同词样例，不推定按灯泡揭示整个词或正确计分。 |

另查看work内c03-paper2022-p4.png（PDF第4页，印页2487），确认图3为作者流程图，未把它当App界面或升级依据。没有查看PDF其他页图像，仅提取其文字。没有查看少数派图片：第一次请求即403，后续同域图片未请求。

下列SHA256对应原图字节（未剪裁/改图）：
'''
for n in imgs:audit+=f'\n- {n}: `{sha(A/n)}`'
audit+=f'\n\n仅work查看的论文页SHA256：`{sha(W/"raw/c03-paper2022-p4.png")}`。审计独立于案例全文；旧E没有引用案例作为视觉审计。\n'
(A/'static-observation-log.md').write_text(audit)
meta=[('01','FuzzFace历史学词流程与V4.3界面','https://www.woshipm.com/evaluating/4425622.html','c04-review2021.html','2021-03-26','作者自报iOS13.6.1；2021-03-19体验；V4.3仅作者表格','4.3（作者表格）','3.1环境/版本表；3.7.3不背流程及四App学习页图',True,imgs[:3]),('02','亭伊2017辨识完成与拼写入口历史体验','https://agirls.aotter.net/post/51654','c05-agirls2017.html','2017-07-23','标题Android/iOS；配图型号/系统/构建未注；台湾媒体历史样例',None,'Learn/不认识/例句段；拼写段及两张相邻拼图',True,imgs[3:]),('03','胡鹤仙历史学习与拼写完成叙述','https://sspai.com/post/76700','c06-sspai2023.html','2023-10-15','页面2023；正文2022经历；平台/构建未知；仅正文',None,'学习内容丰富；学习奖励；正文2022-11-06/18时间',False,[])]
es=[]
for n,title,url,raw,date,scope,build,loc,vis,im in meta:
 e=dict(evidence_id='E-BBLC-'+n,product_id='M-018',title=title,url=url,checked_on='2026-10-27',source_type='third_party_firsthand_review',access_mode='opened_public_html_and_selected_static_images' if vis else 'opened_public_html_text_only',version_scope=scope,app_build=build,firmware_version=None,source_published_on=date,source_updated_on=None,locator=loc,note='仅历史材料；静态/作者流程与实操分开；不保证当前算法或恢复。' if vis else '作者自称体验；图片403停止未看。页面日期与正文经历不同；不把四次答对当当前规则。',hands_on=False,visual_observed=vis,archived_artifact='artifacts/bbdc-learning-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),raw_work_path=str(W/'raw'/raw),raw_sha256=sha(W/'raw'/raw),capture_state='origin_html_and_independent_source_note',supersedes=None)
 if vis:e.update(visual_audit_path='artifacts/bbdc-learning-controls-deepening/static-observation-log.md',visual_audit_sha256=sha(A/'static-observation-log.md'),additional_artifacts=[dict(path='artifacts/bbdc-learning-controls-deepening/'+n,sha256=sha(A/n)) for n in im])
 es.append(e)
(L/'batches/bbdc-learning-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
b=json.loads((W/'baseline.json').read_text());updates=[]
afters={
'F-M018-01':dict(evidence_ids=b['coverage'][0]['evidence_ids']+';E-BBLC-01;E-BBLC-02;E-BBLC-03',known_branches='官方学后/复习后拼写配置与100词/组边界沿用；2017历史图补不认识揭示/例句入口、辨识完成后放弃或开始拼写；2021V4.3作者流程与2022经历文字分开',gaps='现行逐题回忆/揭示/评价到拼写结算仍未知；听写/听词与原声音频未核；分组中断和跨日队列未知；不同词拼图不证明同词提示前后；四次/五次口径及历史构建不统一；完成不等于掌握',visual_status='static_reference_only'),
'F-M018-02':dict(evidence_ids=b['coverage'][1]['evidence_ids']+';E-BBLC-01;E-BBLC-02;E-BBLC-03',known_branches='原回上一词/只拼错词/提示页声明沿用；5.11.6仅记修复尝试；2021作者图描述选错→答案→继续，2017图定位拼写灯泡，正文称发音提示；历史作者四次/五次说法分开',gaps='现行误判撤销/错词专项仍未核；未见同词拼错→提示→重试→通过；灯泡是否揭词/扣分、返回作用、空集合、队列和退出恢复未知',visual_status='static_reference_only')}
for fid,fields in afters.items():
 old=next(x for x in b['coverage'] if x['flow_id']==fid);source=[]
 for p in (L/'batches').glob('*coverage.csv'):
  if p.name.startswith('bbdc-learning-'):continue
  if any(x.get('flow_id')==fid for x in csv.DictReader(p.open())):source.append(str(p))
 assert len(source)==1
 updates.append(dict(flow_id=fid,source_batch=source[0],changes={k:dict(before=old[k],after=v) for k,v in fields.items()},replacement_row={**old,**fields}))
dump(W/'existing-coverage-updates.json',updates);dump(W/'existing-evidence-additional-captures.json',[])
with (L/'batches/bbdc-learning-controls-deepening-coverage.csv').open('w',newline='') as f:csv.DictWriter(f,fieldnames=list(b['coverage'][0]),lineterminator='\n').writeheader()
rows=[]
def add(kind,domain,entry,eid,loc,fid,version,state,gap,prior):rows.append(dict(item_id=f'M018-L{len(rows)+1:02}',inventory_kind=kind,functional_domain=domain,known_entry=entry,source_locator=loc,evidence_ids=eid,flow_ids=fid,platform_version=version,coverage_state=state,gaps=gap,prior_inventory_ids=prior,checked_on='2026-10-27'))
add('historical_static_and_text','首次辨识','Learn→选义/不认识→释义/例句','E-BBLC-02','Learn段及第一张采用图','F-M018-01','2017；构建未注','historical_partial','当前规则/音频播放未核','M018-I32')
add('historical_static','辨识完成与拼写选择','辨识完成弹窗→放弃/开始拼写','E-BBLC-02','拼写段相邻图','F-M018-01','2017；构建未注','historical_partial','放弃后状态/完成拼写回执/恢复','M018-I04')
add('historical_static_and_text','拼写提示','灯泡','E-BBLC-02','拼写段正文与图','F-M018-02','2017；两个不同词样例','historical_partial','未听音；同词提示前后/扣分/重试未知','M018-I06')
add('author_flow_diagram','选择错答后继续','选错→答案→继续','E-BBLC-01','3.7.3作者流程','F-M018-02','作者自报V4.3/iOS13.6.1','diagram_not_ui_feedback','非动态判错；不映射当前错词专项','M018-I05')
add('historical_static','学习页选项','crayon四选义/看答案','E-BBLC-01','四产品学习页图右侧','F-M018-01','作者自报V4.3','static_display_only','选中后颜色/下一题/保存未知','M018-I32')
add('historical_author_text','答对次数与重学','作者称四次/错后重学','E-BBLC-03','学习内容丰富','F-M018-01;F-M018-02','2023页面含2022经历；构建未知','text_only','未看图；与2017五次口径不统一','M018-I05')
add('historical_author_text','学习与拼写奖励对象','作者区分两个首组完成事项','E-BBLC-03','学习奖励','F-M018-01','2022经历/2023页面','text_only','现行奖励/到账/重复计算未知','M018-I15')
add('unresolved_task','误评撤销','本批无新可核入口','E-BBD-01','既有5.11.6修复尝试','F-M018-02','现行5.11.6旧快照','unresolved','箭头外观不等于撤销记录','M018-I07;M018-I08')
add('unresolved_task','分组退出/恢复','本批未取得','', '6候选范围审查','F-M018-01;F-M018-02','当前构建未核','unresolved','放弃拼写/强退/离线/跨日/重装','M018-I33;M018-I34')
prior_relations={
'M018-L01':('', '旧34项未单列首次辨识；本次只加历史入口'),
'M018-L02':('M018-I04', '关联阶段选择；不补现版设置保存'),
'M018-L03':('M018-I06', '历史提示入口；不证明现版提示规则'),
'M018-L04':('M018-I05', '对照一般错答；不证明现版错词专项'),
'M018-L05':('', '旧34项未单列学习选项页；本次只加历史静态页'),
'M018-L06':('M018-I05', '对照作者重学说法；不确定现版错词集合'),
'M018-L07':('M018-I15;M018-I21', '关联奖励对象；不补现版成就或酷币明细入口'),
'M018-L08':('M018-I07;M018-I08', '旧缺口保持'),
'M018-L09':('M018-I33', '旧中断缺口保持；账户恢复另属I34/F04')}
for row in rows:
 row['prior_inventory_ids'],row['prior_inventory_relation']=prior_relations[row['item_id']]
with (L/'batches/bbdc-learning-controls-deepening-inventory.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');wr.writeheader();wr.writerows(rows)
print('3E 0F; 2oldF/8fields; 0oldEcaptures; 9inventory; 5formalimages + 1paperpageworkonly')
