from pathlib import Path
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library'
def edit(path,old,new):
 p=root/path;s=p.read_text();assert old in s,(path,old);p.write_text(s.replace(old,new))
edit('README.md','尚无连续视觉流程或实机操作记录。','已新增公开样刊网页的有限实际操作记录；尚无手机或电子纸实机操作。')
edit('docs/research/interaction-library/README.md','当前没有产品实机或已登录的 app 操作环境；已读取官方帮助、手册、版本记录，按需静态核查公开源码，并实际查看部分官方图/手册页面。临时 PDF 解析库装在 work/，没有创建产品工具链。文字与静态图不能证明触摸手感、时延、刷新残影或连续流程；D3/D4 仍须有相应观察/操作记录。','当前没有手机/电子纸实机或已登录的原生App操作环境；文档、源码和静态图研究之外，已通过临时浏览器实际操作The Week Junior官网公开样刊，取得三个有限D4任务。环境、操作日志、截图和失败见[网页操作审计](artifacts/week-junior-controls-deepening/browser-operation-audit.md)。PDF解析库与研究浏览器均装在work/，没有创建产品工具链；这些网页结果不能证明触摸手感、时延、刷新残影或原生App行为。')
edit('docs/research/interaction-library/README.md','所有 hands_on 均为 false。','多数记录的hands_on仍为false。公开样刊三条任务/三条证据记true，同时强制写明operation_method=automated_public_browser及审计路径；这表示代理驱动真实网页，不是人在目标硬件上的手感试用。')
p=lib/'batches/README.md';s=p.read_text().replace('D4 需要可复现的亲自操作。','D4需要可复查的实际操作记录，并明确人工设备操作、远程操作或自动化浏览器方法。')
s+='''
公开网页操作新增字段约定：`operation_method=automated_public_browser`、`visual_kind=browser_sequence`，`operation_audit_path`相对资料库根目录，并记录`operation_audit_sha256`。正式审计必须包含环境、逐步动作、可见前后状态、失败和未测范围，保存动作日志、按序截图及散列。`hands_on=true`在此只表示实际执行UI动作，不表示实体设备试用。D4也不等于全分支通过。

静态图证据数排除`visual_kind=browser_sequence`；同一次网页运行支持的多个任务不当多次独立验证。仅在DOM中发现控件、对隐藏控件点击超时，均不能记为已成功操作。
''';p.write_text(s)
p=lib/'inventories/README.md';s=p.read_text().replace('| [M027-help-inventory.csv]','| [M003-PB743K3手册目录](../artifacts/pocketbook-controls-deepening/functional-inventory.csv) | 6.10.2854手册的65条目录，页码与任务映射 | 父子章节及安全/合规也在内，不是65项独立功能；部分仅目录定位，未执行设备操作 |\n| [M027-help-inventory.csv]',1);p.write_text(s)
p=lib/'analysis-status.md';s=p.read_text().replace('\n本轮之前的工作属于','\n| PocketBook功能与数据对象补强 | PB743K3/6.10.2854目录65项；笔记编辑/导出、RSS快照、TTS/有声书与云删除分开 | 导出格式、回导编辑及真实断点仍缺；不套用其他型号HTML格式 |\n| 粉笔来源与历史交互重审 | 补2019 iOS6.8.8静态练习链与2020托管导出帮助；旧错题/收藏两项从D2收紧D1 | 建议文章不证明实际执行；托管租户反链未核，当前交卷/离线主链仍缺 |\n| 首次公开网页实际操作 | M048官网样刊完成有限导航、搜索及书签重访记录；三条D4，14张按序截图、动作日志和失败归档 | 只限桌面匿名网页；原生App仍D1，无手机/电子纸实机；没有逐页读完样刊或验证学习效果 |\n\n本轮之前的工作属于');p.write_text(s)
p=lib/'backlog.md';s=p.read_text().replace('| 讯飞办公本 X2 |','| The Week Junior US | 公开样刊桌面导航/搜索/书签的实际序列，三份活动单页；原生App仍D1 | 搜索结果未跳转原因、移动视口、缩放/音频/离线、进程重启；US原生App完整流程及5–6岁内容适配 |\n| PocketBook PB743K3 | 6.10.2854完整目录65项及匹配RN；Notes编辑、RSS、两类播放器、同步状态/删除 | 真实导出格式和往返编辑；离线/换书/耳机恢复、RSS失败与删源范围；图文默认项及线缆措辞冲突 |\n| 粉笔行测 | 现行渠道版本、FAQ；2019静态快速练习及2020托管导出路径 | 当前答题卡/跳题改选/交卷确认/漏题/超时/断网；托管租户官方反链；错题清理控件与结果未核 |\n| 讯飞办公本 X2 |',1).replace('上述任务尚无连续视觉证据和实机结果；新增静态图不能填补这些缺口。','除明确列出的公开样刊桌面操作外，上述材料仍以文档和静态观察为主；所有产品尚无手机/电子纸实机结果。网页局部操作与新增静态图都不能填补其余平台、功能和分支的缺口。');p.write_text(s)
p=lib/'comparison.md';s=p.read_text()
rows={
'学习与反馈':'| 学习游戏一定要依赖屏幕奖励吗？ | [The Week Junior F-M048-08/09/10](cases/M-048-week-junior.md) 官方活动用观点作品、限时共同揭晓、团队观察形成结束与反馈；均由人组织。 | 研究作品、合作与知识迁移带来的动力；不能把人工规则误当内置评分或证明5–6岁适用。 | G-001、R-019、H-006 |',
'读听与中断恢复':'| 继续阅读应回手动保存点还是最后浏览页？ | [The Week Junior F-M048-07](cases/M-048-week-junior.md) 桌面网页实际标记14后翻回1，再访仍14；空上下文从1开始。 | 手动标记、自动最后位置、账号同步和音频秒点分别定义；本次不证明跨端或原生App行为。 | R-018/021、G-002 |',
'新闻与内容选择':'| 新闻更新后，离线读的是哪一版？ | [PocketBook F-M003-08](cases/M-003-pocketbook.md) 手册将RSS更新写入.hrss，并分最新下载和历史归档。 | 区分订阅源、一次抓取、历史快照与当前页面，供AI报告来源和更新时间；删除传播仍未知。 | R-020/021、G-001/002 |',
'导入笔记与内容关联':'| 本地笔记能改，是否代表导出后也能恢复编辑？ | [PocketBook F-M003-03](cases/M-003-pocketbook.md) 明确Edit comment/Save及导出目标路径，但当前格式与回导流程未闭合。 | 修改数据库条目、生成文件和重新导入分别验收；AI不能仅凭导出成功承诺全部可恢复。 | H-004/005、G-002 |',
'系统交互与研究方法':'| 官方备考建议能证明App的实际规则吗？ | [粉笔 F-M032-04/05](cases/M-032-fenbi.md) 有错题与收藏的建议和入口声明，但缺具体控件/执行后状态，已由D2收紧D1。 | 识别产品行为、教学建议、作者改版提案与历史体验；保留版本冲突，不能从“移出清单”推定自动删除。 | R-004、G-001/002 |'}
for heading,row in rows.items():
 start=s.index('## '+heading); pos=s.index('| --- | --- | --- | --- |',start)+len('| --- | --- | --- | --- |');s=s[:pos]+'\n'+row+s[pos:]
p.write_text(s)
p=root/'docs/briefs/010-task-evidence-and-reuse.md';s=p.read_text().replace('当前已补入操作手册、版本记录、作者体验和部分静态界面，但**仍没有逐一研究完每个产品的每个功能，也没有连续操作或实机验证**。','当前已补入操作手册、版本记录、作者体验、静态界面，以及首次公开网页实际操作记录。但**仍没有逐一研究完每个产品的每个功能，也没有手机或电子纸实机验证**。')
rows='''| 保存位置是否跟随最后翻到的页面？ | [The Week Junior公开样刊](../research/interaction-library/cases/M-048-week-junior.md) 实际标记14页、回封面、重访又到14；空上下文从1开始。 | 手动书签与最后浏览点不同。14张按序截图、操作日志和失败见[网页审计](../research/interaction-library/artifacts/week-junior-controls-deepening/browser-operation-audit.md)；仅代表桌面网页。 |
| 找到官方文章就能确定App行为吗？ | [粉笔](../research/interaction-library/cases/M-032-fenbi.md) 两项旧流程收紧为功能声明；补2019静态练习及2020托管导出材料，注明版本与来源归属限制。 | 教学建议、历史界面与当前规则分别引用；没有把“重做后移出”说成已验证的自动清理。 |
| 儿童新闻怎样接到主动学习？ | [The Week Junior活动](../research/interaction-library/cases/M-048-week-junior.md) 补观点写作、共同读书问答和线索寻物。 | 可以研究有限作品、共同揭晓和观察记录；它们是人工组织的活动，原产品面向8–14岁，5–6岁改编仍待验证。 |
| 导出和离线各自保存什么？ | [PocketBook](../research/interaction-library/cases/M-003-pocketbook.md) 补笔记编辑/导出、RSS快照和云条目/本地文件区别，并列65项手册目录。 | 有导出路径不等于能回导编辑，有书封不等于文件已落地；这些状态需要分别供系统AI查询。 |
'''
s=s.replace('| 孩子答错后该如何继续？ |',rows+'| 孩子答错后该如何继续？ |',1)
s=s.replace('资料中的星星、按钮或奖励文案本身不能回答这些问题，也不能证明学习效果。','资料中的星星、按钮或奖励文案本身不能回答这些问题，也不能证明学习效果。\n\n新增的网页记录还保留了失败：隐藏控件定位超时、侧栏拦截上一页点击、搜索结果点击后仍留原页。成功与失败都附观察条件；找到了控件不等于完成了操作，D4也不等于全流程通过。正式设计可以据此提出验证任务，而不是直接复制界面。')
p.write_text(s)
p=root/'docs/ideas.md';s=p.read_text();mark='\n每个想法按需要记录以下内容，不要求用户填写表单：';assert mark in s
s=s.replace(mark,'\n2026-10-26继续核查I-021：首次通过临时研究浏览器实际操作The Week Junior公开样刊，归档导航/搜索/手动位置恢复的有限任务、按序截图、动作日志及失败。原生App、手机/电子纸实机与学习成效仍未验证。另补PocketBook型号目录/数据对象，审查粉笔资料后将错题和收藏两项收紧为功能声明。修订依据继续合回稳定F/E编号和第十期简报；这些取证方法和产品启发未升级为新需求。\n'+mark)
p.write_text(s)
print('Updated research summaries and preserved requirements unchanged')
