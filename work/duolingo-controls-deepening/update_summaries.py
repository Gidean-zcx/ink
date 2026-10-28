from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
p=L/'comparison.md';s=p.read_text().replace('有限网页操作于10月26日','有限网页操作于10月26～27日')
rows={
'学习与反馈':[
'| 如何区别错答、提示后答对与暂时不能听？ | [Duolingo F-M031-17](cases/M-031-duolingo-adventures.md) 实际错答扣心，悬停词义后可作答；不能听时提示15分钟后恢复并给文字题。 | 三类状态分别记录；没等到恢复，不把提示当定时机制已验证。网页Hearts与Energy博文分开。 | G-001/002、R-014/019/021 |',
'| 连续学习和刷分是否都代表增长？ | [Duolingo F-M031-08～11](cases/M-031-duolingo-adventures.md) 天数、任务、XP和个人纪录分开；TSLW为厂商代理指标。 | 成人与儿童分别研究坚持、独立回忆、迁移和探索收益；5–6岁不照搬公开社交竞争。 | G-001、R-019 |'],
'读听与中断恢复':[
'| 移除游戏载体后，是暂停还是回检查点？ | [Tonie F-M038-13](cases/M-038-tonie.md) quiz/故事/Discover有不同恢复粒度；地区帮助措辞保留差异。 | AI先说明将恢复哪一轮/哪一检查点；音频位置和游戏进程不能共用含糊的“继续”。 | R-018、G-002 |'],
'AI上下文与执行结果':[
'| 导入的数量少于提交数量，算完成吗？ | [墨墨 F-M017-07](cases/M-017-maimemo.md) add_words有实际成功数；建立文本词本和词ID入计划不同，学习读写需要自动同步。 | 分别核资料组织、入计划与可学习状态，报告部分成功和未确认部分；本批只读契约。 | R-016、G-002 |',
'| 点完最后一题能否报告学习完成？ | [Duolingo F-M031-17/18](cases/M-031-duolingo-adventures.md) 实际操作未见结算而回基础选择；取消/确认退出另有记录。 | 页面跳转、动作结束与业务完成分开；异常结果也要能供设计复用。 | G-001/002、R-021 |'],
'同步备份与成员数据':[
'| 家庭共享是共享权益、档案还是内容编辑权？ | [Duolingo F-M031-14](cases/M-031-duolingo-adventures.md) 进出套餐不改个人进度；[Tonie F-M038-15/16/18](cases/M-038-tonie.md) 编辑权可逐对象，解散家庭会永久删录音。 | 身份、权益、成员权限和内容归属分开；实体移交不等于数据迁移，不提前决定账户制度。 | H-004/005、G-002 |'],
'导入笔记与内容关联':[
'| 导出学习资料是不是备份学习系统？ | [墨墨 F-M017-09](cases/M-017-maimemo.md) PDF需本地保存且有内容排除；[Duolingo F-M031-13](cases/M-031-duolingo-adventures.md) 数据副本未说明回导。 | 明确导出对象、版本、内容缺失与可回导性；不能用文件生成证明刷机后恢复。 | H-004/005、R-016 |'],
'系统交互与研究方法':[
'| 闹钟已设置，是否一定会响？ | [Tonie F-M038-14](cases/M-038-tonie.md) 日出闹钟有年龄模式/充电及播放状态条件；未实际计时。 | AI应解释生效条件与冲突状态；设置成功和实际执行记录分别核。 | G-002、H-003 |']}
for heading,add in rows.items():
 marker='## '+heading+'\n';start=s.index(marker);pos=s.index('| --- | --- | --- | --- |',start)+len('| --- | --- | --- | --- |');s=s[:pos]+'\n'+'\n'.join(add)+s[pos:]
p.write_text(s)
p=L/'backlog.md';s=p.read_text();pos=s.index('| Quizlet |');s=s[:pos]+'''| Duolingo主App / Adventures | 三组17帮助+6文章；普通课57状态、14图，错答/提示/听力暂缓/退出实际观察 | 最后回基础选择未见结算的原因；课末纠错/奖励；当前原生App各模式、听力恢复、完整设置和其他学科；Adventures仍缺独立全回路 |
| 墨墨软件 | 37帮助节点+24 API方法；选词、自评、内容固定、PDF及静态契约 | 误评撤销、当前拼写反馈/跨日恢复；PDF默认回退与故障扣次边界；API错误/幂等/权限/部分成功，未调用 |
| Toniebox 1/2 与Tonieplay | 150目录行；下载、按类型恢复、日出闹钟、家庭角色及对象归属 | 具体游戏判分/选择/奖励和完整恢复；检查点地区措辞、实际响铃/停铃/断网；逐代角色权限及云删除后的离线缓存 |
'''+s[pos:];p.write_text(s)
p=L/'analysis-status.md';s=p.read_text();needle='\n本轮之前的工作属于';pos=s.index(needle);s=s[:pos]+'| 学习动力、公开课与家庭内容深化（10月27日） | Duolingo27E/14新F/26目录、57实际网页状态与14图；墨墨13E/2新F/61目录；Tonie24E/7新F/150目录；补8旧F | 3条有限D4含未观察结算分支；API只是契约，游戏恢复和角色条件按版本分开，全部仍部分覆盖 |\n'+s[pos:];p.write_text(s)
p=R/'docs/ideas.md';s=p.read_text()+'\n2026-10-27继续I-021/I-022：M-031从Adventures补到主学习应用，实际匿名课程观察错答、词义提示、听力暂缓和退出，最后未观察结算而回基础选择的结果保留。墨墨区分文本词本、词ID入计划、学习读写前提和PDF副本；Tonie按游戏类型核恢复粒度、闹钟条件及对象权限。来源、动作、失败和未知合回F/E，见[第十一期简报](/home/workspace/reader/docs/briefs/011-learning-actions-and-recovery.md)。没有新增已确认功能、账户方案或开始系统设计。\n';p.write_text(s)
print('Updated comparison, backlog, audit and ideas; no requirement changes')
