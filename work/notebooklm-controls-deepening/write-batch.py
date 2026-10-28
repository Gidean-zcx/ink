import csv, hashlib, json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'work/notebooklm-controls-deepening'
LIB=ROOT/'docs/research/interaction-library'
CASE=LIB/'cases/M-041-notebooklm.md'
DATE='2026-10-26'
records=json.loads((WORK/'fetch-results.json').read_text())
good=[r for r in records if r.get('status')==200]
assert len(good)==13

metadata=[
 ('官方帮助目录与品牌跳转','Get started / Change your settings / Fix your problem','只证明帮助条目存在；目录标题不单独构成可执行任务。'),
 ('来源格式、导入与失效','Understand source types / Add source / Import Google Drive / Web / YouTube / Audio / Research','补既有来源任务；导入重复去重、人工删除确认和旧产物重算未说明。'),
 ('导入失败与删除恢复 FAQ','My source import failed / Recover deleted notes / Duplicate a notebook','记录受保护PDF及大小上限失败；笔记删除不可恢复；复制结论与创建帮助矛盾。'),
 ('Studio目录、表格导出与复制','Get started / Data Tables / Web notifications / Create a private copy / Allow copies','表格导出保留单独引用页；导出不回同步。复制步骤与FAQ并列，不能推断统一生效。'),
 ('卡片与测验的控制和进度','Study with Flashcards & Quizzes / Flashcard Controls / Quiz Controls','补导航、重练与进度；自评不等同测试证明掌握；未见逐题文本编辑或评分撤销步骤。'),
 ('笔记来源与导出','Create new notes / How notes work as sources / Convert / Export','来源勾选控制参与范围；旧Studio笔记需要转换；Docs副本不回同步。'),
 ('思维导图生成与节点问答','Generate / Interact / Save & share','从聊天chip生成、Studio留存、点节点发问、下载及退出；重生成描述含Delete note。'),
 ('交互报告与文档报告','Generate interactive / Generate document / Customize / Share','报告类型有不同路径；交互报告Add嵌入建议产物，产物亦出现在Studio。'),
 ('信息图的生成、管理与分享','Generate / Manage / Load previous / Share','后台生成、PNG下载和返回；删除使旧分享链接失效。'),
 ('幻灯片生成、修订和导出','Generate / Co-create / Edit / Delete slides / Reorder / Share','修订形成新deck且不参考来源；待删除页可Restore；与Android帮助范围不同。'),
 ('视频生成、制作说明和播放','Generate / Co-create / Manage / Play / Share','Ultra制作说明先审后提交；Proceed后不能继续修订说明；下载格式及恢复未说明。'),
 ('Android移动端差异与离线音频','Device requirements / Audio / Flashcards / Slide Decks / Limited features','Android 10+文档范围；音频离线缓存不等于设备文件导出，跨端延迟建议退出重开。'),
 ('2025-12-23数据表发布','What’s changing / Getting started / Rollout pace','可核发布时间只限定数据表历史发布；不为当前帮助内其他功能提供上线日期。'),
]
evidence=[]
for r,(title,locator,note) in zip(good,metadata):
 evidence.append(dict(evidence_id=r['evidence_id'],product_id='M-041',title=title,url=r['final_url'],requested_url=r['requested_url'],checked_on=DATE,
  source_type='official_release_note' if r['slug']=='tables-release' else 'official_help',access_mode='opened_text',
  version_scope='查阅日英语官方帮助；电脑网页为主，E-NBCD-12单列Android；NotebookLM旧帮助目录跳Gemini Notebook；精确构建及多数条目更新时间未知',
  app_build=None,firmware_version=None,source_updated_on='2025-12-23' if r['slug']=='tables-release' else None,
  locator=locator,note=note,hands_on=False,visual_observed=False,visual_audit=None,archived_artifact=None,sha256=None,
  capture_state='raw_html_in_work_and_short_case_summary',raw_work_path=r['raw_work_path'],raw_sha256=r['raw_sha256'],http_status=200,
  supersedes=None,version_warning='Evergreen正文无构建/更新时间；FAQ复制与创建帮助冲突，桌面与Android修订差异单列；不得倒填历史版本。'))
(LIB/'batches/notebooklm-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evidence))

flows=[
 ('08','从思维导图分支追问并保存图','E-NBCD-07','聊天Mind Map入口→Studio产物→展开或点节点进入主题问答→下载或退出；帮助把Delete note列为重生成动作。','下载格式、删除确认/撤销、重生成时旧图状态、来源更新传播未知'),
 ('09','生成报告并嵌入学习产物','E-NBCD-08;E-NBCD-04','Reports分Interactive/Document；交互报告Add嵌入产物且另列Studio；模板Edit后可立即/稍后生成；文档导出Docs，表格导出Sheets。','生成取消/失败重试、排程取消、报告内文本校订、删除恢复及交互嵌入导出形态未知'),
 ('10','将来源整理成带引用的数据表','E-NBCD-04;E-NBCD-13;E-NBCD-12','Studio Data Tables可先铅笔定义语言/行列→生成→More Export to Sheets；新表第一页数据、第二页引用，修改不回同步且权限不继承。','应用内单元格校订、重复行合并、来源变化后更新、删除恢复未知'),
 ('11','修订幻灯片并导出演示文件','E-NBCD-10;E-NBCD-12','桌面Revise集中待改项；待删除页变灰可Restore，Generate revised deck创建新版且修订不参考来源；PDF/PPTX导出，幻灯播放Esc退出。','修订事实准确性、已生成新版的回滚、排程取消、失败恢复未知；Android文档仍限制增删页，不能套桌面'),
 ('12','生成信息图并下载或删除产物','E-NBCD-09','Studio Infographic后台生成→按标题重新打开/缩放→PNG下载或退出；More可删除，旧分享链接随删除失效。','图片内容逐项编辑、误删恢复、失败重试、已下载副本回收未知'),
 ('13','审阅视频制作说明并播放产物','E-NBCD-11;E-NBCD-12','Video Overview生成后按标题Play；Ultra可先Review producer notes集中反馈，Proceed后不能再改说明；Generate later绕过说明；Android左滑可删除。','生成取消、排程取消、断网续播精确位置、误删恢复、下载格式未知'),
]
header=list(next(csv.DictReader((LIB/'batches/ai-coverage.csv').open())).keys())
with (LIB/'batches/notebooklm-controls-deepening-coverage.csv').open('w',newline='') as f:
 writer=csv.DictWriter(f,header,lineterminator='\n');writer.writeheader()
 for n,task,eids,branches,gaps in flows:writer.writerow(dict(flow_id='F-M041-'+n,product_id='M-041',task=task,depth='D2',evidence_ids=eids,known_branches=branches,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-041-notebooklm.md',checked_on=DATE))

old=list(csv.DictReader((LIB/'batches/ai-coverage.csv').open()))
updates={
 '01':('E-NBL-01;E-NBCD-02;E-NBCD-03','保留Fast/Deep Research任务；本地资料Add sources导入。超过500000词、上传200MB或PDF复制保护可失败；YouTube取字幕、音频取转写；重复去重未说明。','重复导入识别、错误页与保留部分数据、全格式保真未知'),
 '02':('E-NBL-01;E-NBCD-02;E-NBCD-04','Drive打开来源可手动sync，帮助亦述自动更新；失权/被删来源仍占额度但不参与新问答/产物，可请求访问或移除。取消勾选只确证范围选择，不等同删除；原Drive文件不受编辑/删除。','手动删除入口/确认/恢复、重复合并、旧产物是否随来源更新未知；创建帮助关于unselect/remove措辞需实核'),
 '04':('E-NBL-02;E-NBCD-05','前后箭头导航、Hint/Explain、自评和结果重练；卡片可重启、打乱、删卡和CSV下载。自评记录不证明掌握；未见题干/答案文本编辑或评分撤销步骤。','逐题文本校订、评分撤销、删卡确认/恢复、自动间隔复习规则未知'),
 '05':('E-NBL-04;E-NBL-07;E-NBCD-05;E-NBCD-12','当前帮助也述离开后继续进度；2026-03-20旧证据保留单卡/单题删除与重练历史范围，本轮该发布页HTTP429未复取。Android同步延迟建议退出重开，不保证冲突合并。','多端冲突优先级、删题恢复、精确断点及重练是否重置历史成绩未知'),
 '06':('E-NBL-05;E-NBCD-12','原英语互动流程保留；Android可后台听，Studio音频Download或左滑下载后在首页Downloaded找回离线音频；离线缓存不能当作设备文件导出。','中文互动、精确续播位置、缓存清理后的恢复及离线互动未知'),
 '07':('E-NBL-06;E-NBCD-06;E-NBCD-03','新笔记属于Sources，勾选后参加问答/生成；旧Studio笔记Convert或批量Convert；笔记More导出新Docs，修改不回同步且权限不继承。FAQ明确已删除笔记不能恢复；移动端只读/选取/问答。','移除引用后可追溯性、删除确认、笔记变更是否更新旧产物未知'),
}
suggestions=[]
for row in old:
 if row['product_id']!='M-041':continue
 n=row['flow_id'].rsplit('-',1)[1]
 if n not in updates:continue
 for field,after in zip(['evidence_ids','known_branches','gaps'],updates[n]):
  suggestions.append(dict(flow_id=row['flow_id'],field=field,before=row[field],after=after,reason='本轮只读官方补证；D2/未实操/未视觉观察状态不变；旧batch与master由root合并'))
with (WORK/'existing-coverage-updates.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,['flow_id','field','before','after','reason'],lineterminator='\n');w.writeheader();w.writerows(suggestions)

inventory=[
 ('来源收集/检索','Sources: types/Add/Research','E-NBCD-02;E-NBCD-03','F-M041-01','D2部分','多数格式仅支持声明；去重和失败恢复未知'),
 ('来源同步/有效性/移除','Sources: Drive/YouTube','E-NBCD-02','F-M041-02','D2部分','人工删除确认/恢复和旧产物刷新未知'),
 ('来源标签/自动分类','Sources: Label & Categorize','E-NBCD-02','','D1','只列操作类别；具体入口、移除标签是否改来源未知'),
 ('聊天/引用','Use chat；旧E-NBL-03','E-NBL-03','F-M041-03','D2既有','矛盾来源与引用准确性未验证'),
 ('卡片/测验/进度','Cards: Study/Controls','E-NBCD-05','F-M041-04;F-M041-05','D2部分','人工改题、撤销自评、间隔复习未知'),
 ('音频/互动/离线','Audio旧帮助；Android: Audio/Limited','E-NBL-05;E-NBCD-12','F-M041-06','D2部分','英语互动；离线缓存与文件导出分开；精确断点未知'),
 ('笔记/作为来源/导出','Notes: Create/Convert/Export；FAQ恢复','E-NBCD-06;E-NBCD-03','F-M041-07','D2部分','FAQ称删除不可恢复；删除确认未知'),
 ('思维导图','MindMap: Generate/Interact/Save','E-NBCD-07','F-M041-08','D2','节点手工改写、重生成保留旧图未知'),
 ('交互/文档报告','Reports: Generate/Customize/Share','E-NBCD-08;E-NBCD-04','F-M041-09','D2','内嵌产物导出、生成取消/恢复未知'),
 ('数据表','Create: Studio Data Tables；2025发布','E-NBCD-04;E-NBCD-13','F-M041-10','D2','单元格校订、来源刷新、删除恢复未知'),
 ('幻灯片/修订','Slides: Generate/Edit/Delete/Reorder/Share','E-NBCD-10;E-NBCD-12','F-M041-11','D2','桌面/Android增删页差异；新版本回滚未知'),
 ('信息图','Infographic: Generate/Manage/Load/Share','E-NBCD-09','F-M041-12','D2','图中内容校订、误删恢复未知'),
 ('视频/制作说明','Video: Generate/Co-create/Play/Share','E-NBCD-11;E-NBCD-12','F-M041-13','D2','渲染取消、断网恢复未知；生成时长非本项目测量'),
 ('完成通知/未读状态','Create: Web notifications','E-NBCD-04','','D2附属分支','任务完成≠已审阅；权限拒绝/通知丢失未知'),
 ('复制笔记本','Create: Private copy；FAQ: Duplicate','E-NBCD-04;E-NBCD-03','','文档冲突','已有操作描述与FAQ不支持结论并存；不新增F'),
 ('分享/导出权限','Create: Share；各产物Share','E-NBCD-04;E-NBCD-08;E-NBCD-09;E-NBCD-10;E-NBCD-11','F-M041-07;F-M041-09;F-M041-10;F-M041-11;F-M041-12;F-M041-13','D2部分','每类分享入口已读；组织策略/撤权后下载副本未知'),
 ('用量/组织与年龄/设置','帮助目录相关条目','E-NBCD-01','','仅索引','未逐页读取套餐额度、组织控制与全部设置'),
 ('连接应用/Gemini/AI Mode','帮助目录相关条目','E-NBCD-01','','仅索引','不把外层Gemini删除动作套Notebook界面；跨产品删除传播待查'),
 ('公共/精选笔记本与统计','目录；Create: Analytics','E-NBCD-01;E-NBCD-04','','D1或未展开','未展开访问/退出；查询次数不是学习效果'),
]
with (LIB/'batches/notebooklm-controls-deepening-inventory.csv').open('w',newline='') as f:
 w=csv.writer(f,lineterminator='\n');w.writerow(['functional_domain','source_section','evidence_ids','flow_ids','coverage','missing_controls_or_branches']);w.writerows(inventory)
print('WROTE 13 E, 6 new F, 19 inventory rows; 18 field proposals for 6 existing F; case written separately')
