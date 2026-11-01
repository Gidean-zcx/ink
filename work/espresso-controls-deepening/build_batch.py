from pathlib import Path
import json,csv,hashlib,copy,shutil

R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/espresso-controls-deepening';A=L/'artifacts/espresso-controls-deepening';B=L/'batches'
assert not (B/'espresso-controls-deepening-evidence.jsonl').exists(), 'one-time builder'
A.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
caps=json.loads((W/'captures.json').read_text());imgs=json.loads((W/'image-captures.json').read_text());versions=json.loads((W/'store-versions.json').read_text())
for x in imgs:
 assert x['status']==200
 dst=A/(x['name']+'.png');shutil.copyfile(R/x['raw_work_path'],dst)
 x.update(visual_observed=True,observed_on='2026-10-27',archived_artifact=str(dst.relative_to(L)),sha256=sha(dst))
dump(W/'observed-images.json',imgs)
notes='''# Espresso：本轮来源与适用范围

查阅2026-10-27。全文原件在work/espresso-controls-deepening/raw；只保存以下短摘记和必要静态参考，不保存产品音视频或访问账户。没有实际运行Espresso或调用内容API。

## E-ESCD-01：HKU机构指南

[原页](https://libguides.lib.hku.hk/espresso)，页面更新2025-06-13，App构建未知。机构说明自己的学生入口，并提示不要在该注册路径选SSO；不代替其他机构或学生资格条款。四张样例图实际查看：Today短文/时间与红勾、Daily quiz入口、数字卡和播客、For you周长文。图中有勾无勾并存，不证明勾的触发条件、撤销、持久化或最终完成页。没有查看账号创建图，也没有创建账号。

## E-ESCD-02：2017作者亲自使用记录

[原文](https://alearningaday.blog/2017/07/26/delight-to-use-and-the-economist-espresso-thinking-product/)，2017-07-26，作者称图来自当天。正文描述点进文章、滑动继续、完成勾选，七项后出现结束页；图片支持部分列表勾选、文章和结束页三种形态。七项不是当前五篇短文规则，也不应全称七篇文章。没有连续录制，触发阈值、跳读是否算已读、恢复和当前版沿用性未知；作者的速度/喜好与设计评价不是测量结论。静态图保留屏幕时间和样式，不据此猜App构建。

## E-ESCD-03：相邻的Pro内容API，不是Espresso控制接口

[官方开发文档](https://developer.economist.com/docs)，未标更新日，路由v1。API面向Economist Pro合同授权；身份、内容和图片权益分别限定。World in Brief按接入时配置的时区取当日，未发布可回退前日；文档示例有内容ID、首次/更新发布时间。它不能证明Espresso用户可用此接口、能读取个人已读/偏好或能控制App；本轮没有申请Key、认证或执行示例。该来源只支持判断“最新响应不一定是今天”和内容服务/用户状态的边界，不列为Espresso新增功能或任务。

## 旧E-ESD-01的本地复读

复用2026-10-26取得的美区App Store HTML，2026-10-27读取serialized-server-data，提取25条版本记录。3.42.0对应2026-04-16；3.31.0（2024-08-15）把四语言译文称实验功能，3.32.0称改进其显示；3.24.0描述外部进入播客/测验，3.27.0描述Today短视频。3.37.0日期显示和3.40.0分享/无障碍均只是改进声明。25条不是全部历史，未重抓商店或把邻近文本的版本配对当依据。

## 取证限制

三次HTML和七次必要静态图普通GET均200；完整URL/散列见采集记录。检索同时命中咖啡设备、主Economist应用、广告介绍及非操作评论，均未作为Espresso任务依据。economist.com的robots限制没有绕过，旧ACG直开失败路径未重试。当前完整学习/问答、听读/翻译控件、离线/收藏/检索及跨端恢复继续未知。
'''
(A/'source-notes.md').write_text(notes)
visual='''# Espresso静态图核查

观察2026-10-27，根代理用view_image逐张查看下列七图；非App操作、非连续录像，均hands_on=false。图片取得地址、原件和正式散列见对应E及work/espresso-controls-deepening/observed-images.json。

| 图片 | 所见 | 不能推出 |
| --- | --- | --- |
| hku-guide-1.png | Today选中；顶部World in brief卡有相对更新时间/耳机；下方短文时长，部分有红勾 | 未操作卡片，不能认定更新时间与已读存储规则 |
| hku-guide-2.png | 同类短文列表，四项红勾、另一项无勾；下方Daily quiz入口；Today/For you/Settings底栏 | 不是实际滚动连续记录，没有提交测验或验证已读触发 |
| hku-guide-3.png | 数字卡的Read the full story；下方Podcast、耳机和节目卡 | 点击后权限、外链或播放状态未知 |
| hku-guide-4.png | For you选中；周长文卡、听音入口、预计时间及四个位置点，第二点着色 | 圆点不证明手势、四篇持久规则、默认选择或自动播放 |
| historical-finish-1.png | 2017列表，两个勾；Today's agenda与The world in brief分区，设置图标 | 截图未展示全部七项，也不证明重开后勾仍在 |
| historical-finish-2.png | 文章图文和右下Home图标 | 没有实际按Home，返回落点未知 |
| historical-finish-3.png | 红色结束页、That's it、名言、Share the quote与Home | 只有作者正文解释前置阅读过程；未观察分享或结束页触发算法 |

HKU指南更新2025-06-13，图的采集日期/构建未标；2017作者记录与当前商店3.42.0分开。图片不能用于证明手机触控或电子纸显示效果。
'''
(A/'visual-audit.md').write_text(visual)
Es=[]
for num,cap in enumerate(caps,1):
 assert cap['status']==200
 im=[x for x in imgs if x['source_page']==cap['name']]
 e=dict(evidence_id=f'E-ESCD-{num:02}',product_id='M-045',title=['HKU Espresso guide and static examples','Espresso 2017 firsthand finishing flow','Economist Pro Content API — adjacent service boundary'][num-1],url=cap['url'],checked_on='2026-10-27',source_type=['institutional_guide_with_static_images','firsthand_historical_report_with_images','official_developer_documentation'][num-1],access_mode='public_html_get_and_text_read'+('_and_view_image' if im else ''),version_scope=['HKU guide updated2025-06-13; app build unknown','2017-07-26 firsthand screenshots; build unknown; not current','Economist Pro B2B v1; not Espresso user-control API'][num-1],app_build=None,firmware_version=None,source_updated_on=['2025-06-13','2017-07-26',None][num-1],locator=['How to Access; Key Features; four sample screenshots','The Economist Espresso paragraphs and three screenshots','Overview; Entitlements; Images; /v1/the-world-in-brief time zone based delivery'][num-1],note=['Today and For you static controls; completion trigger unknown','Historical article→swipe→checks→end as author report, not actual operation','Pro entitlement and date fallback; excludes personal app-state/control inference'][num-1],hands_on=False,visual_observed=bool(im),archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md'),capture_state='raw_html_text_and_short_note_archived',supersedes=None)
 for key in ['raw_work_path','raw_sha256','text_work_path','text_sha256']:e[key]=cap[key]
 if im:e.update(visual_kind='static_images',visual_audit_path=str((A/'visual-audit.md').relative_to(L)),visual_audit_sha256=sha(A/'visual-audit.md'),rendered_artifacts=[x['archived_artifact'] for x in im],artifact_hashes={x['archived_artifact']:x['sha256'] for x in im},image_urls={x['archived_artifact']:x['url'] for x in im})
 Es.append(e)
(B/'espresso-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in Es))
before=json.loads((W/'coverage-before.json').read_text());proposals=[]
changes={
 'F-M045-01':dict(task='一日简报读完（历史链与现版分开）',depth='D2',evidence_ids='E-ESPR-01;E-ESPR-02;E-ESD-01;E-ESCD-01;E-ESCD-02',known_branches='2017作者点文/滑动/勾选至结束页；2025机构图仅支持Today栏目和勾的外观，当前主链仍未核',gaps='历史已读触发与跳读、当前末篇结束、日期更新/离线及跨端恢复',visual_status='historical_and_institutional_static_images_only'),
 'F-M045-02':dict(evidence_ids='E-ESPR-01;E-ESPR-02;E-ESD-02;E-ESCD-01',known_branches='机构指南Settings改主题；HKU图For you周长文卡/时间/四个位置点；不证明改每日短报',gaps='主题完整选项、确认、更新生效与当前构建；卡片手势和跨端同步',visual_status='institutional_static_images_only'),
 'F-M045-03':dict(evidence_ids='E-ESPR-02;E-ESD-01;E-ESCD-01',known_branches='文章/周长文静态音频入口；本地商店历史3.31四语言实验、3.32译文显示修订；不证明中文音频',gaps='播放/暂停/结束、精确返回与断点；译文入口、范围、回原文位置和当前实验状态',visual_status='official_and_institutional_static_images_only'),
 'F-M045-04':dict(evidence_ids='E-ESPR-04;E-ESPR-03;E-ESCD-01',known_branches='ACG机构账号路径与HKU学生自建账号分开；HKU指引该路径不选SSO；16+资格仍依原优惠条款',gaps='机构/学生权益冲突、登录失败/恢复、服务到期与当前通用流程'),
 'F-M045-07':dict(evidence_ids='E-ESD-02;E-ESD-01;E-ESCD-01',known_branches='机构指南Settings列显示/通知选项；HKU只见Settings底栏；历史通知/无障碍改进声明不证明菜单结果',gaps='字号档位、主题默认/跟随系统、通知权限与保存/跨重启状态',visual_status='settings_entry_static_only')}
for old in before:
 new=copy.deepcopy(old);new.update(changes[old['flow_id']],checked_on='2026-10-27');proposals.append({'flow_id':old['flow_id'],'before':old,'after':new,'reason':'补历史作者使用链、机构静态控件及版本条件；现版主链未知，沿用原任务'})
dump(W/'existing-coverage-updates.json',proposals)
with (B/'espresso-controls-deepening-coverage.csv').open('w',newline='') as f:csv.DictWriter(f,fieldnames=list(before[0])).writeheader()
oldcap=dict(checked_on='2026-10-27',capture_method='local_raw_html_reread_and_embedded_version_extraction',capture_url='https://apps.apple.com/us/app/espresso-world-news-in-a-shot/id896628003',raw_work_path='work/news-app-deepening/espresso-appstore.html',raw_sha256=sha(R/'work/news-app-deepening/espresso-appstore.html'),extracted_work_path='work/espresso-controls-deepening/store-versions.json',extracted_sha256=sha(W/'store-versions.json'),note='Original captured2026-10-26; 25 version entries re-read, not full history or new app operation',archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md'))
dump(W/'additional-captures-updates.json',[{'evidence_id':'E-ESD-01','capture':oldcap}])

rows=[]
def row(scope,typ,title,loc,e,flows,status,gap,url):
 rows.append(dict(entry_id=f'I-ESCD-{len(rows)+1:03}',scope=scope,entry_type=typ,title=title,locator=loc,url=url,evidence_id=e,related_flow_ids=flows,review_status=status,gap=gap,checked_on='2026-10-27'))
hu=caps[0]['url'];his=caps[1]['url'];ap=caps[2]['url'];st=oldcap['capture_url']
for title,flows in [('学生自建账号/非SSO','F-M045-04'),('五短篇/周长文','F-M045-01;F-M045-02'),('短视频声明',''),('编辑选择声明','F-M045-02')]:row('HKU指南','document_domain',title,'How to Access / Key Features','E-ESCD-01',flows,'read_document','完整任务与当前构建未核',hu)
for im,titles,flows in [
 ('hku-guide-1',['Today选中','World in brief卡/更新时间','摘要耳机入口','短文预计时间','短文勾选标记'],'F-M045-01;F-M045-03'),
 ('hku-guide-2',['五短文列表','Daily quiz入口','Today/For you/Settings底栏'],'F-M045-01;F-M045-07'),
 ('hku-guide-3',['数字卡/全文入口','Podcast入口','节目卡/耳机'],'F-M045-03'),
 ('hku-guide-4',['For you选中','周长文卡','周长文音频入口','预计阅读时间','四位置点/第二点着色'],'F-M045-02;F-M045-03')]:
 for title in titles:row('HKU样例图，构建未标','static_control_or_group',title,im+'.png','E-ESCD-01',flows,'viewed_static','未操作；位置/默认/持久规则未知',hu)
for title,loc in [('列表入口与完成勾','historical-finish-1.png'),('文章与Home图标','historical-finish-2.png'),('结束页与名言','historical-finish-3.png'),('分享名言入口','historical-finish-3.png'),('文章间继续/七项终点','正文The Economist Espresso'),('文章间广告报告','正文广告段')]:row('2017作者记录','historical_control_or_report',title,loc,'E-ESCD-02','F-M045-01','read_report_and_viewed_static','非连续过程，当前沿用性未知',his)
for v in versions:row('美区商店本地原件，25条窗口','version_claim',v['version'],v['date'],'E-ESD-01','F-M045-01;F-M045-03;F-M045-07','read_embedded_version','改进/修复是声明；非当前实测或完整历史',st)
for title,loc in [('Pro合同授权边界','Overview / Entitlements'),('图片权益与链接期限','Images'),('本地日与回退前日','Time zone based delivery'),('内容ID/发布时间与个人记录区别','World in Brief example')]:row('相邻Pro内容服务，排除为Espresso功能','scope_boundary',title,loc,'E-ESCD-03','','read_document_exclusion','未调用；不证明Espresso账号/个人状态接口',ap)
for title,flows in [('当前日报末篇与次日恢复','F-M045-01'),('主题保存与生效','F-M045-02'),('音文切换与翻译返回','F-M045-03'),('测验纠错到结算',''),('收藏/搜索/离线入口',''),('显示/通知持久状态','F-M045-07')]:row('待研究，不是已确认功能','research_gap',title,'case gaps','E-ESCD-01;E-ESCD-02',flows,'unresolved','未取得完整链',hu)
with (B/'espresso-controls-deepening-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

case=(W/'case-before.md').read_text()
case=case.replace('- 查阅日：2026-10-26。','- 初查2026-10-26，续查2026-10-27。',1)
case=case.replace('**主要新闻循环仍为 D1；机构指南有局部 D2。**','**当前新闻循环仍缺完整链；F-M045-01仅2017历史子范围补到D2，机构指南有局部D2。**',1)
old='| F-M045-01 | 读完一天简报 | 可访问 Espresso 内容；权限界面未知 | 进入每日内容 → 读全球摘要及短文；准确首页入口、前后篇操作未知 | 有阅读时间提示与有限内容份数；未证明逐篇已读状态 | 可把内容份数作为人工结束边界；最后一篇是否有完成页、返回位置均未知 | E-ESPR-01、02；D1 |'
new='| F-M045-01 | 读完一天简报（历史与当前分开） | 2017作者记录，构建未标；当前权益另核 | 历史点文章→滑动继续→出现完成勾→作者称七项后结束页；2025机构图可见Today与勾 | 图片只支持控件形态；未证实勾的触发、跳读判定、当前末篇反馈 | 历史有结束页，重开/次日/跨端恢复未知；不套当前五篇规则 | E-ESCD-01/02、E-ESPR-01/02；D2仅历史链 |'
assert old in case;case=case.replace(old,new)
case+='''

## 2026-10-27：历史完成链、当前截图与来源边界

本轮新增[逐项台账](../batches/espresso-controls-deepening-inventory.csv)，分机构正文、静态控件、历史使用记录、商店版本窗口、相邻服务边界和研究缺口。不是全App菜单；相邻API明确不计为Espresso功能。原5F沿用编号，没有新增任务；35个字段补充，实际字段数以合并审计为准。

2017-07-26作者称亲自从文章继续阅读、看到勾选，并在七项后到达结束页。根实际查看三图：列表只有部分勾，文章有Home图标，结束页有名言和分享/Home入口。作者的叙述与静态图能支持有条件的历史流程重建，不能证明已读阈值、滑动方向、未读跳过规则或分享/返回结果；更不能推导今天的完成机制。F-M045-01只在此范围升D2。[历史原文](https://alearningaday.blog/2017/07/26/delight-to-use-and-the-economist-espresso-thinking-product/)

HKU指南更新2025-06-13，四张实际查看的样例图补到Today、Daily quiz、数字卡/全文入口、播客与For you周长文；有勾无勾并存，For you有位置点，但没有点击、滑动或提交记录。机构自己的学生入口不选SSO，不与ACG机构账号路径混为一套通用向导。[机构指南](https://libguides.lib.hku.hk/espresso)

旧商店HTML的25条版本说明重新按嵌入数据配对：3.31.0（2024-08-15）四语言为实验，3.32.0提译文显示修订；3.24.0称可从外部直达播客/测验，3.27.0称Today可看短视频。未取得实际链接格式、路由失败或语言切换步骤。3.42.0发布日期为2026-04-16，仅商店版本；这些修复/改进措辞不作为运行结果。

另发现官方Pro内容API文档：World in Brief按接入时区取日，未发布时回退前日。这可提醒后续设计区分请求日期、返回内容日期和更新日期，但**不是Espresso用户的控制接口**，也未说明其个人已读/偏好数据。只读文档，没有认证或调用；不据此把全部新闻和个人状态接入能力写为已具备。[相邻服务文档](https://developer.economist.com/docs)

来源短摘记、原件范围见[本轮记录](../artifacts/espresso-controls-deepening/source-notes.md)；七张图逐张观察见[静态核查](../artifacts/espresso-controls-deepening/visual-audit.md)。前文2026-10-26“未取得完成页”保留为当时的现版缺口；此次只补历史证据，没有操作App或证明当前版本已闭环。对G-001，明确结束可作为候选设计问题；对G-002，文章/一期/媒体/权限/已读状态仍须分别核查。没有将历史七项、学生资格或Pro合同接入变成本项目需求。
'''
# 将临时占位计数替换为实际修订字段数。
fields=sum(sum(old[k]!=new[k] for k in old) for old,new in [(x['before'],x['after']) for x in proposals])
case=case.replace('35个字段补充，实际字段数以合并审计为准',f'{fields}个字段补充')
(L/'cases/M-045-espresso.md').write_text(case)
dump(W/'build-result.json',dict(newE=len(Es),newF=0,oldF=len(proposals),old_fields=fields,oldE_captures=1,inventory_rows=len(rows),static_images=7,hands_on=False))
print((W/'build-result.json').read_text())
