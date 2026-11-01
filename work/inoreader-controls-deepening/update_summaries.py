from pathlib import Path
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library'
p=root/'docs/briefs/010-task-evidence-and-reuse.md';s=p.read_text();a=s.index('| 设计问题 |');b=s.index('\n这些是竞品事实',a)
s=s[:a]+'''| 设计问题 | 当前证据 | 可借鉴什么，仍缺什么 |
| --- | --- | --- |
| “测试规则”是否只预览结果？ | [Inoreader F-M044-02](../research/interaction-library/cases/M-044-inoreader.md)：功能页写preview，帮助却明确Run rule会执行动作，包括邮件和导出；旧概括已修订。 | AI需按真实动作与副作用解释测试；规则部分失败、重复执行和撤回仍无实操证据。 |
| 怎样突出重要新闻，又保留探索空间？ | [Inoreader F-M044-07/08/12/13](../research/interaction-library/cases/M-044-inoreader.md)：区分账号/公共索引搜索、监控、自选栏目与Spotlights着色；着色不隐藏文章或改变排序。 | 来源扩展、纳入、排序和视觉强调分别讨论；公共索引也不是全网，热度不是高价值证明，颜色需适配墨水屏。 |
| 新闻更正以后，原笔记和AI结论还可信吗？ | [Inoreader F-M044-10/11/14](../research/interaction-library/cases/M-044-inoreader.md)：标注保存文本副本，Edited可看修订和Published；未证实摘要会随源更新。 | 区分当时依据、当前版本和重新生成；保留来源关联，不能只覆盖成最新文本。 |
| 中途离开学习后，究竟恢复什么？ | [Quizlet F-M021-07/11/13](../research/interaction-library/cases/M-021-quizlet.md)：卡组Test未完离开不保存；Learn重置清自己的进度；Progress统计能跨端，游戏不计。 | 测试会话、模式进度、答题统计与掌握判断不同。不能把一个“同步学习进度”套用所有模式；Android path文档范围仍有分歧。 |
| AI生成题目后能校订到哪一层？ | [Quizlet F-M021-01/02/12](../research/interaction-library/cases/M-021-quizlet.md)：Smart Assist卡片预览可改/删/增；Study Guide与Practice Test帮助主要给标题/权限等编辑，题干答案校订未证实。 | 原资料、生成草稿、人工校订、发布与作答分别记录；生成完成不代表已经审核或可纠错。 |
| 下载完有声书，就能离线读听切换吗？ | [Audible F-M039-02/13/14](../research/interaction-library/cases/M-039-audible.md)：音频可下载，沉浸文字仍从网络加载，仅部分页缓存；Page Sync是纸书→音频且排除Kids Profile/儿童绘本。 | 音频、文字、配对关系与位置各自准备；现有服务路径不能直接当5–6岁儿童方案，反向切换后的状态仍未测。 |
''' + s[b:]
s=s.replace('接下来仍需补中文学习的一次完整回路、真实听音与中断恢复、AI生成及执行失败、安装升级和备份恢复的实际结果，以及各型号的完整菜单和版本条件。','接下来仍需补当前构建的完整学习回路、新闻过滤/规则的实际结果、真实听音及中断恢复、AI内容校订和执行失败，以及各型号未读目录与恢复分支。')
p.write_text(s)
p=lib/'comparison.md';s=p.read_text();adds={
'学习与反馈':[
'| 同一学习集的各模式都能中途恢复吗？ | [Quizlet F-M021-07/11/13](cases/M-021-quizlet.md) Test未完成离开不保存；Learn重置清该模式进度；Progress跨活动统计却排除游戏。 | 会话断点、练习历史、模式状态和掌握标签分别解释；没有保存不等于从未作答。 | G-001/002、R-015/019/021 |',
'| “每题正确两次”证明长期掌握吗？ | [Quizlet F-M021-07](cases/M-021-quizlet.md) Write/Spell用两次正确结束一轮；错拼还有重输/逐字提示。 | 首答正确、看答案后重输和延后独立回忆分别记录；模式结束条件不能直接变成儿童能力成就。 | G-001、R-014/019 |'],
'AI上下文与执行结果':[
'| AI生成的卡片与试卷都有相同校订能力吗？ | [Quizlet F-M021-01/02/12](cases/M-021-quizlet.md) Smart Assist有卡片草稿编辑；Practice Test所列More只证明标题/可见性等设置，未证明题干答案可改。 | 按产物核校订粒度、原稿关联、版本与再生成；不能将卡片编辑能力推广到全部AI题库。 | G-002、R-016 |'],
'读听与中断恢复':[
'| 音频离线是否就能全文同步高亮？ | [Audible F-M039-13](cases/M-039-audible.md) US沉浸读听文字联网加载，仅部分页缓存；还需对应双格式资格。 | 音频、文字、位置映射和可访问资格分别查询，避免单个下载完成标记掩盖缺失。 | R-018、G-002 |',
'| 扫纸书定位音频能否反查纸书页码？ | [Audible F-M039-14](cases/M-039-audible.md) Page Sync单向纸书→音频，需相机/联网，排除Kids Profile与儿童绘本。 | 识别成功、匹配正确和恢复原播放位置分开；儿童方案需另证，不能继承成人服务的可用范围。 | R-014/018、G-002 |'],
'同步备份与成员数据':[
'| 重下载和重装App会保留相同状态吗？ | [Audible F-M039-10/11](cases/M-039-audible.md) 普通重下载帮助说保留位置，重装帮助却说明下载与Finished标记需重建；Hidden恢复另有路径。 | 下载文件、书库、完成标记、位置和笔记分别核对，不能以某一项保留承诺全部恢复。 | H-004/005、R-018、G-002 |']}
for title,rows in adds.items():
 a=s.index('## '+title);marker='| --- | --- | --- | --- |';i=s.index(marker,a)+len(marker);s=s[:i]+'\n'+'\n'.join(rows)+s[i:]
p.write_text(s)
p=lib/'inventories/README.md';s=p.read_text();m='| --- | --- | --- |';i=s.index(m)+len(m);s=s[:i]+'''\n| [M021-Quizlet学习帮助目录](../batches/quizlet-controls-deepening-inventory.csv) | 4父级及64篇入口，共68行；创建/共享/学习/文件夹 | 只覆盖Studying类目；其他账户、教师等未穷尽；公开文本，无登录或界面观察 |
| [M039-Audible US听读帮助与设置](../batches/audible-controls-deepening-inventory.csv) | 首屏35篇、9补充及9设置，共53行 | Ways to listen仍有Load more未展开；网页工具正文不是原始HTML，未实际听音 |'''+s[i:];p.write_text(s)
p=lib/'backlog.md';s=p.read_text();a=s.index('## 本轮深化后');i=s.index(m,a)+len(m);s=s[:i]+'''\n| Quizlet | 68目录节点；Write/Spell结束/提示、普通Test、AI Practice Test和Progress文档 | 当前完整同题流程、试卷题干答案校订、Test/Practice Test各自恢复、Study path Android分歧、删集联动及未读模式 |
| Audible US手机App | 53帮助/设置项；播放器、Hidden、重装、退还、沉浸文字及Page Sync条件 | 未展开目录/设备、真实网络与音频打断、文字缓存、反向读听位置、隐藏加入入口、跨端并发及离线授权 |'''+s[i:]
s=s.replace('| M-044 Inoreader | 测试范围、近似标题去重、过滤后回找及规则预览；补移动阅读、导入与全文抓取。 |','| M-044 Inoreader | 已明确Rule试跑会执行动作，补源范围/修订/看板/报告；仍需真实过滤回看、级联失败、已读恢复菜单及移动/导入流程。 |')
p.write_text(s)
p=lib/'README.md';s=p.read_text();start=s.index('当前新增[');end=s.index('\n',start);s=s[:start]+'当前新增[Inoreader的来源、新闻修订与规则试跑](cases/M-044-inoreader.md)、[Quizlet各模式的完成与恢复](cases/M-021-quizlet.md)、[Audible离线和读听条件](cases/M-039-audible.md)。同一任务补回原编号，帮助正文、官方单帧与实际操作分开；测试按钮、下载完成和同步声明不替代结果核查。'+s[end:];p.write_text(s)
p=root/'README.md';s=p.read_text().replace('最新简报；AI动作与自动化、安装和备份范围、儿童创作及证据缺口，附设计问题到任务和证据的检索方式。','最新简报；新闻更正与规则试跑、学习恢复、AI试题校订和离线读听，附设计问题到任务和证据的检索方式。');p.write_text(s)
p=lib/'analysis-status.md';s=p.read_text();i=s.index('\n本轮之前的工作属于');s=s[:i].rstrip()+'\n| 新闻、学习模式与读听条件深化（10月27日） | Inoreader33E/10新F/316目录及两静态图；Quizlet17E/3新F/68目录；Audible23E/3新F/53目录；合计补18旧F | 修订Rule预览判断，区分生成题校订、学习状态和文字/音频离线；资料仍为D2及静态图，全部部分覆盖 |\n'+s[i:];p.write_text(s)
p=root/'docs/ideas.md';s=p.read_text()+'\n2026-10-27继续I-021：Inoreader补来源扩展、新闻更正、标注文本副本与报告，并据帮助明确修订Rule“预览”的旧概括：试跑也会实际执行动作。Quizlet分开Test会话、Learn重学、Progress和AI产物可校订范围；Audible分开音频下载、文字加载、纸书定位及内容资格。目录、原件定位、冲突和未知随F/E保存，细节见[第十期简报](/home/workspace/reader/docs/briefs/010-task-evidence-and-reuse.md)。没有新增已确认功能或启动系统设计，5–6岁适配与实际恢复仍待验证。\n';p.write_text(s)
print('Updated research summaries and reuse links')
