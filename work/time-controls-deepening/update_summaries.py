from pathlib import Path
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library'
def replace(path,old,new):
 s=path.read_text();assert old in s,(path,old[:70]);path.write_text(s.replace(old,new,1))
replace(lib/'cases/M-041-notebooklm.md','Deep Research 后台执行后选报告/来源；退出丢弃未导入结果','Deep Research 后台执行后选报告/来源；帮助写Cancel或缩小退出深研模式会丢弃未导入结果')
replace(lib/'comparison.md','更新：2026-10-26。','更新：2026-10-27；本批来源查阅与网页操作于10月26日。')
replace(lib/'comparison.md','| 看到听书入口，是否能承诺续播？ | [TIME F-M047-04](cases/M-047-time-for-kids.md) 只核读条件式播放器代码；[汉王 F-M008-05](cases/M-008-hanvon.md) 只补到静态设置。 | 启用条件、播放位置和页面生命周期需分别验证；源码和单张图都不代替实际恢复结果。 | R-018、G-002 |','| 暂停、关闭和刷新后的继续是否相同？ | [TIME F-M047-04](cases/M-047-time-for-kids.md) 单篇匿名网页：暂停位置保持，关闭重开与非零刷新对照回起始/.9x；[汉王 F-M008-05](cases/M-008-hanvon.md) 仍仅静态设置。 | 当前播放状态、界面生命周期、持久断点分别核定；TIME未实际听音，不能据此判断声音质量或全部产品行为。 | R-018、G-002 |')
replace(lib/'comparison.md','| 新闻更新后，离线读的是哪一版？ |','| 查询已显示在网址中，是否代表检索成功？ | [TIME F-M047-01](cases/M-047-time-for-kids.md) 无意义查询仍显示普通文章，未见明确空结果提示；本次原因未知。 | 查询已提交、结果匹配、回退内容及无结果反馈分别表达；AI不能仅凭页面跳转声称找到资料。 | R-016/020、G-002 |\n| 新闻更新后，离线读的是哪一版？ |')
replace(lib/'comparison.md','| 地址已到结果页，AI可以说已完成吗？ |','| AI改完的内容还使用原来来源吗？ | [NotebookLM F-M041-11](cases/M-041-notebooklm.md) 桌面帮助明确幻灯片修订不参考sources；待删页可恢复，但提交会生成新套。 | 内容修订、依据重核、版本生成和回滚分开；未操作产品，不把文档描述当生成质量验证。 | R-016/021、G-002 |\n| 地址已到结果页，AI可以说已完成吗？ |')
# Add specific system comparisons within the existing question domain.
s=lib/'comparison.md';body=s.read_text();marker='## 系统交互与研究方法';assert marker in body
pos=body.index('| --- | --- | --- | --- |',body.index(marker))+len('| --- | --- | --- | --- |')
body=body[:pos]+'\n| 关闭优化与恢复默认值会改变同样的数据吗？ | [BOOX F-M004-03](cases/M-004-boox-neoreader.md) 通用帮助区分Master Switch、单App Reset All、全部App重置和原DPI。 | AI要知道动作作用于哪一App/文档/全局、是否清掉配置；本机4.1.2具体菜单与优先级仍待核。 | R-007/010、G-002 |\n| 同名手势是否有统一方向？ | [BOOX F-M004-02/03](cases/M-004-boox-neoreader.md) 两类EinkWise帮助分别写右下和左下默认上滑，均可自定义。 | 按型号、类别、当前配置提供动作；资料不能跨机型拼成默认规则。 | R-009/010、G-002 |'+body[pos:];s.write_text(body)
replace(lib/'backlog.md','更新：2026-10-26。','更新：2026-10-27。')
replace(lib/'backlog.md','| 驾考宝典公开网页 |','| TIME for Kids K–1 | 单篇匿名网页实际启动/暂停、语速、进度、关闭/刷新、英西语及查询；31状态/18正式图 | 实际声音、同步精度、失败重试/离线、词汇与级别切换、查询异常原因、儿童自主使用；不把网页操作当学习效果 |\n| BOOX Note Air5 C | 14节59篇通用帮助目录，面板/手势/显示作用范围、四静态图；跨类别默认手势分开 | 本机4.1.2专用手册、后台各开关优先级、TTS打断、卸载重装与优化配置恢复；通用图不能代实机 |\n| NotebookLM / Gemini Notebook | 19功能域，来源生命周期、卡片进度及六类Studio产物文档链 | Create/FAQ复制冲突、桌面/Android修订差异、人工改题/撤销评分、旧产物随来源更新、生成失败与误删恢复；无账户操作 |\n| 驾考宝典公开网页 |')
replace(lib/'backlog.md','| M-033 驾考宝典 | 补官网正文或官方操作演示；题型、交卷、错题、收藏、地区/题库更新和离线。 |','| M-033 驾考宝典 | 已补匿名网页练习/模拟；继续查空白结算原因、App题型与恢复、登录后的错题/收藏/同步、地区题库更新和离线。 |')
replace(lib/'backlog.md','| M-047 TIME for Kids | 找同一主题适龄文章；词汇帮助、朗读/课堂问题实际入口，避免假定每篇有所有功能。 |','| M-047 TIME for Kids | 已补同篇公开朗读界面和查询；继续核真实声音、词汇/级别切换、故障/离线、儿童独立操作及课堂反馈，不假定每篇均有相同工具。 |')
replace(lib/'inventories/README.md','| [M006-X2手册目录]','| [M004-BOOX公开指南目录](../artifacts/boox-controls-deepening/functional-inventory.csv) | ePaper Tablet PCs分类14节59篇；任务关联与型号限制 | 不是Note Air5 C V4.1.2专用菜单，含其他型号/后续版本；目录存在不代表已观察 |\n| [M041-Studio与帮助功能域](../batches/notebooklm-controls-deepening-inventory.csv) | 官方目录和Create正文对照的19功能域，含数据表与未展开栏目 | 使用功能域/来源章节/F-E/缺口字段；有聚合不是19个独立功能或完整菜单；无实际生成 |\n| [M006-X2手册目录]')
replace(lib/'README.md','已通过临时浏览器操作The Week Junior公开样刊和驾考宝典公开练习／模拟页。','已通过临时浏览器操作The Week Junior公开样刊、驾考宝典公开练习／模拟页及TIME for Kids单篇听读／搜索。')
replace(lib/'README.md','与[练习审计](artifacts/jiakao-controls-deepening/browser-operation-audit.md)。','、[练习审计](artifacts/jiakao-controls-deepening/browser-operation-audit.md)及[听读审计](artifacts/time-controls-deepening/browser-operation-audit.md)。')
replace(lib/'README.md','公开样刊三条任务/三条证据记true，同时强制写明operation_method=automated_public_browser及审计路径','公开网页操作按具体任务记true，数量以覆盖索引为准，同时强制写明operation_method=automated_public_browser及审计路径')
p=lib/'analysis-status.md';s=p.read_text();marker='\n本轮之前的工作属于';pos=s.index(marker);s=s[:pos].rstrip()+'\n| 听读网页、系统控制及AI产物细化（10月27日归档） | TIME两旧任务补有限D4，31状态与18图；BOOX59篇目录/四图与7旧任务；NotebookLM19功能域/6新任务及6旧任务补证 | 未实际听音或使用设备；保留检索异常、手势型号差异、复制文档冲突和跨端修订限制，全部产品仍部分覆盖 |\n'+s[pos:];p.write_text(s)
replace(root/'README.md','最新简报；纠错、录音、学习恢复、云导出和版本修订，以及从问题反查任务与证据的方式。','最新简报；听读恢复、系统设置范围、AI产物修订和版本冲突，以及从设计问题反查任务与证据的方式。')
p=root/'docs/ideas.md';s=p.read_text();marker='每个想法按需要记录以下内容';pos=s.index(marker);s=s[:pos]+'2026-10-27归档前一晚I-021深化：TIME公开文章补实际播放控件、暂停/关闭/刷新、语言和查询，未实际听音；BOOX补通用目录及设置作用范围，NotebookLM补来源与六类产物的生命周期。研究继续区分文档、静态图和实际操作，保留原文冲突与失败结果；同一任务沿用编号，新增证据回写横向问题索引。没有新增已确认功能或启动系统设计。\n\n'+s[pos:];p.write_text(s)
print('Updated synthesis, comparison, inventory, gaps and ideas.')
