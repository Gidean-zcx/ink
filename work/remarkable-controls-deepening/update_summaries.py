import csv,json
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/remarkable-controls-deepening'
brief=R/'docs/briefs/012-data-objects-and-state-boundaries.md';assert not brief.exists()
# Apply one final reviewed correction; preserve audit.
p=L/'batches/sony-controls-deepening-coverage.csv';rs=list(csv.DictReader(p.open()));audit=[]
for r in rs:
 if r['flow_id']=='F-M036-13':
  before=r['task'];r['task']='使用USB DAC并尝试恢复后台无声';audit.append(dict(flow_id=r['flow_id'],field='task',before=before,after=r['task']))
with p.open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
p=L/'cases/M-036-sony.md';s=p.read_text();s=s.replace(audit[0]['before'],audit[0]['after']);p.write_text(s)
(W/'post-review-flow-updates.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
brief.write_text('''# 第十二期：副本、索引与恢复状态

2026-10-27。继续落实逐功能交互研究。本批为文档重建及三张手册静态页；没有新增产品实际操作，也没有把竞品做法确定为本项目设计。

## 三项可直接带入后续设计的问题

| 设计时的问题 | 具体交互依据 | 应保留的区分 |
| --- | --- | --- |
| “改好了”会影响哪些地方？ | reMarkable的Convert and share网页不会随设备后续改动更新；只有所有者可编辑，持链接者可查看/下载。Meeting Notes复制日历信息，改笔记不写回日程。[案例F15/16](../research/interaction-library/cases/M-005-remarkable.md) | 原稿、转换文本、分享网页、日程是不同对象。AI需说明目标对象与传播结果；不能把有链接当成双向同步。 |
| “资料导入了”是否就能搜索和学习？ | 欧路先复制词库，再单独启用全文索引并等待完成；加入生词本、正式学习、同步是其他入口。[案例F04/09/12](../research/interaction-library/cases/M-026-eudic.md) | 文件存在、索引就绪、查到释义、进入学习计划分别报告。后台索引有文档依据，杀进程后续建尚无依据。 |
| 音频文件还在，为什么不能继续上次状态？ | Sony NW-A306移卡时手册分别列出播放位置/队列被清除与收藏/歌单登记信息消失；传回电脑保存音乐内容，不包含完整播放器状态。[案例F14/16](../research/interaction-library/cases/M-036-sony.md) | 内容备份与播放状态备份分别验收。重插是否恢复关联、逐书断点与普通音频打断仍需测试。 |

这些细节对应G-002的全局AI，也对应G-001：收集到内容并不等于形成可学习资料，播放或页面进度也不等于知识掌握。

## 纠正和条件

- reMarkable同日帮助对选区转文字分别描述自动替换与光标粘贴；会议列表帮助写未来10小时，新3.29日志写三天。两条证据与入口/版本条件均保留，不替文档补一个未经观察的统一行为。
- Sony播放页明确提示时间条可能不准确，旧“精确寻位”已收窄；USB音频输出与USB-DAC输入的音效条件不同。后台无声后回DAC屏只是官方建议的可能恢复。
- 欧路键盘Command+S保存笔记退出、Command+W放弃退出；这不证明触屏也有同样按钮。旧帮助的Android周期本地备份没有给出恢复流程，不能据此承诺重装后可恢复。

来源、版本和获取方式分别见[reMarkable原件记录](../research/interaction-library/artifacts/remarkable-controls-deepening/source-notes.md)、[Sony原件记录](../research/interaction-library/artifacts/sony-controls-deepening/source-notes.md)、[欧路原件记录](../research/interaction-library/artifacts/eudic-controls-deepening/source-notes.md)。

## 怎样反复使用这些资料

先从[横向问题表](../research/interaction-library/comparison.md)找设计问题，再打开案例的F任务，沿E编号查章节、原始来源、版本与证据方式，最后看该任务的缺口。功能目录单独维护，避免“列出入口”被误认为“研究完功能”。同一任务有新证据时补回原编号，原结论修订保留原因。

本批新增36条证据、16个任务，并补回20个已有任务；新增241条目录记录，含父章节及非功能项，不作为独立功能完成数。全库准确数量见[覆盖索引](../research/interaction-library/case-index.md)。65组产品仍全部为部分覆盖，不能说每个功能已经深入体验。下一步继续补缺失的完整目录、核心使用链及异常恢复；无设备或登录环境的部分明确停留在文档证据，不编造体验。
''')
p=R/'README.md';s=p.read_text();old='- [第十一期：学习动作与恢复粒度]';s=s.replace(old,'- [第十二期：副本、索引与恢复状态](/home/workspace/reader/docs/briefs/012-data-objects-and-state-boundaries.md)：最新简报；reMarkable跨端副本、欧路词库索引、Sony播放范围与介质恢复。\n'+old,1).replace('：最新简报；Duolingo','：Duolingo');p.write_text(s)
p=L/'README.md';s=p.read_text();s=s.replace('本批续补[Duolingo','本批续补[reMarkable的日历/分享副本与恢复](cases/M-005-remarkable.md)、[欧路的取词/索引/解释页](cases/M-026-eudic.md)、[Sony的播放范围/介质/输出状态](cases/M-036-sony.md)，见[第十二期简报](/home/workspace/reader/docs/briefs/012-data-objects-and-state-boundaries.md)。三项均保留文档条件及实际操作缺口。\n\n此前续补[Duolingo',1);p.write_text(s)
p=L/'inventories/README.md';s=p.read_text();anchor='| --- | --- | --- |\n';addition='''| [M005-Paper Pro手册及帮助标题](../batches/remarkable-controls-deepening-inventory.csv) | 90个旧指南章/节、35个可见帮助标题/空分类，共125行 | 手册包含安全/监管，不是完整软件目录；Features仅三个空分类，标题href#未冒充已解析文章 |
| [M026-欧路帮助与章节](../batches/eudic-controls-deepening-inventory.csv) | 22个帮助页及12子章节，共34行 | 只读正文与目录分开；新版日志只得摘要，跨端AI与恢复链未完成 |
| [M036-NW-A306帮助目录](../batches/sony-controls-deepening-inventory.csv) | 英文Help Guide的82篇主题，附同版PDF定位 | 固定5-042-672-11(5)，不等同固件构建或Android全功能；部分正文未完成任务分析 |
''';s=s.replace(anchor,anchor+addition,1);p.write_text(s)
p=L/'comparison.md';s=p.read_text()
def insert(section,row):
 global s
 start=s.index('## '+section);pos=s.index('| --- | --- | --- | --- |',start)+len('| --- | --- | --- | --- |');s=s[:pos]+'\n'+row+s[pos:]
insert('AI上下文与执行结果','| 同一内容在两个界面出现，就会联动吗？ | [reMarkable F-M005-15/16](cases/M-005-remarkable.md) 分享网页不随设备改动更新，会议笔记修改也不回写日历。 | AI需区分副本、原稿、链接和外部记录；报告改了哪个对象及传播结果。 | G-002、R-007/021 |')
insert('读听与中断恢复','| 换卡后“继续播放”需要恢复哪些对象？ | [Sony F-M036-14/16](cases/M-036-sony.md) 移卡影响位置/队列及收藏关联；传回PC保存曲目，不能当完整状态备份。 | 逐书断点、队列、内容介质、索引与收藏分别核；重插卡恢复未证明。 | R-018、H-004/005、G-002 |')
insert('导入笔记与内容关联','| 已复制词库为何还不能全文查找？ | [欧路 F-M026-09/12](cases/M-026-eudic.md) 导入后还要启用索引并等待完成；词库、生词本和学习课本不同。 | 内容导入、索引可用与进入学习计划分别暴露状态；后台不等于中断可恢复。 | R-016/021、G-001/002 |')
insert('系统交互与研究方法','| 同名USB模式或转换入口能否合并规则？ | [Sony F-M036-09/13](cases/M-036-sony.md) 输入/输出音效条件不同；[reMarkable F-M005-07](cases/M-005-remarkable.md) 选区转换文档有自动/手动粘贴冲突。 | 保留模式、入口、版本与不一致，不按功能名称生成统一动作保证。 | R-004/009、G-002 |')
p.write_text(s)
p=L/'backlog.md';s=p.read_text();s+='''
## 2026-10-27：副本、索引与播放器状态续查

- M005已补125条目录记录、7新任务及8旧任务；旧指南不是完整软件目录。继续核选区转字的自动/手动粘贴、会议列表3.29条件、PDF转笔记保真、同步冲突与重置结果。当前没有设备操作，不用帮助页浏览填补。
- M026已补34条目录、3任务及6旧任务；仍缺当前触屏查词→原句保存→练习闭环、索引中断、备份恢复与AI产物校订。登录壳和429只记限制，不换路径重取受限内容。
- M036已补82篇目录、6任务及6旧任务；仍缺逐书续播、普通音频打断、时间点书签、重插卡关联与数据库重建副作用。先确认固件/应用构建，不能把同版手册当实机状态。
''';p.write_text(s)
p=L/'analysis-status.md';s=p.read_text();anchor='\n本轮之前的工作属于';row='\n| 笔记副本、词典索引与播放器介质深化（10月27日） | reMarkable15E/7新F/125目录，欧路3E/3新F/34目录，Sony18E/6新F/82目录；补20旧F的76字段及一项标题收窄 | 3张手册静态页，无新产品实操；保留转换冲突、日志摘要及条件性恢复，全部仍部分覆盖 |\n';s=s.replace(anchor,row+anchor,1);p.write_text(s)
p=R/'docs/ideas.md';s=p.read_text();s+='\n2026-10-27继续I-021/I-022：reMarkable补日历/分享副本、键入与恢复，欧路补跨软件取词/索引/解释页，Sony补播放范围/介质/USB角色。文档冲突、时间条误差和条件性恢复均回写原任务；目录单独登记，见[第十二期简报](/home/workspace/reader/docs/briefs/012-data-objects-and-state-boundaries.md)。新增资料仍是研究依据，没有把接口、备份、账户或设备能力升级为需求，也未启动全系统设计。\n';p.write_text(s)
print('updated brief and 7 research entry points')
