from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
p=L/'inventories/README.md';s=p.read_text();anchor='| --- | --- | --- |\n';new='''| [M001-Scribe 2024指南目录](../batches/kindle-controls-deepening-inventory.csv) | 9父标题、47子项及2正文子项，共58条；主样本与2025+另列 | 在线指南无固定固件；目录含父子项，音频/家庭/AI等仍未展开 |
| [M030-Khan Kids图书馆栏目](../batches/khan-controls-deepening-inventory.csv) | 官方Library帮助的8个顶层标签，逐项F/E与缺口 | 仅Library，不是全App或题型目录；未知构建，尚无App操作 |
| [M040-AntennaPod帮助与全局设置](../artifacts/antennapod-controls-deepening/functional-inventory.csv) | 47条官方帮助索引与3.12.3的11个XML中84个带key节点 | 包含分类父项；运行时显隐、每播客动态设置及全播放器菜单未穷尽 |
''';assert 'M030-Khan' not in s;s=s.replace(anchor,anchor+new,1);p.write_text(s)
p=L/'comparison.md';s=p.read_text().replace('本批来源查阅与网页操作于10月26日','资料续补至10月27日，有限网页操作于10月26日')
entries={
'## 学习与反馈':'| 复述和数学创作是否也能成为学习任务？ | [Khan F-M030-07/08](cases/M-030-khan-kids.md) 贴纸、口述录音、回放、保存和导出是不同动作；官方活动范例未自动判分。 | 研究表达所学、作品回访与成人反馈；保存成功不能当知识掌握，也不据动画证明5–6岁效果。 | G-001、R-014/019 |',
'## 读听与中断恢复':'| “再听十分钟”按什么时间计算？ | [AntennaPod F-M040-08](cases/M-040-antennapod.md) 3.12.3代码按位置事件到达的墙钟差扣时，超过10秒间隔跳过；不是音频位置差。 | 播放时长、真实截止点与中断后的计时分别定义；源码未运行，不能承诺暂停必冻结。 | R-018、G-002 |',
'## 同步备份与成员数据':'| 数据库备份和永久删除各覆盖哪些副本？ | [AntennaPod F-M040-12](cases/M-040-antennapod.md) 导出SQLite而全局偏好另存；[Kindle F-M001-06/07](cases/M-001-kindle.md) 断开云盘不删已导入云库，Alexa附件也另存。 | 为备份、恢复、断开和删除逐个列对象、范围与剩余副本；禁止只用一个“已完成”状态。 | H-004/005、G-002 |',
'## 系统交互与研究方法':'| 控件不见是权限、设置还是型号差异？ | [Khan F-M030-11](cases/M-030-khan-kids.md) 家长/教师不同，教师ON/OFF帮助冲突；[Kindle F-M001-07](cases/M-001-kindle.md) 云盘分支限2025+。 | AI应说明可用条件与隐藏原因；隐藏Home图标不等于系统不能退出，通用帮助不保证本机支持。 | R-009/010/021、G-002 |'}
for heading,row in entries.items():
 start=s.index(heading);pos=s.index('| --- | --- | --- | --- |\n',start)+len('| --- | --- | --- | --- |\n');s=s[:pos]+row+'\n'+s[pos:]
s+='\n本批补证：Khan教师帮助对同名显示开关的ON/OFF相反；Kindle跨端手写帮助仍有范围冲突；AntennaPod帮助默认下载数量与正式标签XML/缺值回退不一致。详见各案例，不据其中一条推定所有安装配置。\n';p.write_text(s)
p=L/'backlog.md';s=p.read_text();a='| 对象 | 已有的文档基础 | 下一步仍缺 |\n| --- | --- | --- |\n';rows='''| Kindle Scribe 2024 / 2025+分支 | 58项目录，书库/合集、笔迹保存/删除/导出与云盘副本条件 | 主样本固件、音频/家庭/AI剩余任务；未同步笔迹、删除恢复、跨端冲突、空间不足；2025+不套2024 |
| Khan Academy Kids | 8个Library标签；录故事/保存/导出、成员、家长/教师控制；两GIF抽样4正式帧 | 构建固定、同题完整纠错/领奖、创作权限与退出恢复；教师开关方向、家长设置范围；抽样不当完整演示 |
| AntennaPod 3.12.3 | 47帮助索引+84设置节点；正式Media3服务、计时、归档/删除及DB恢复静态链 | 真正音频焦点/短暂停、物理删除失败可见反馈、DB失败回滚；动态设置与其余播放器/多队列；无安装测试 |
''';assert 'Khan Academy Kids | 8个Library' not in s;s=s.replace(a,a+rows,1)
s=s.replace('| TIME for Kids | K–1 文章、教师活动；页面所引播放器的条件式源码路径 | 实际服务配置、播放/暂停/关闭、跨页断点；历史 3–4 年级词汇节点不代替 K–1 交互 |','| TIME for Kids 其他级别与活动 | K–1网页操作已补见表首；教师活动与历史3–4词汇节点分开 | 其他级别和词汇实际路径、教学效果；历史节点不能代替当前K–1交互 |')
s=s.replace('除公开样刊及驾考宝典的有限桌面操作外','除公开样刊、驾考宝典及TIME for Kids的有限桌面操作外');p.write_text(s)
p=L/'analysis-status.md';s=p.read_text();a='本轮之前的工作属于';row='| 儿童创作、阅读器型号与音频正式版本深化（10月27日） | Khan新增5任务/8栏目及4摘帧，Kindle补58目录/1任务并限定2025+；AntennaPod固定3.12.3补47帮助+84设置节点；共42E、6新F、21旧F修订 | 全部仍D2文档/静态源码或抽样画面；没有App操作，保留开关、默认值与跨端冲突，不宣告全产品完成 |\n\n';pos=s.index(a);s=s[:pos].rstrip()+'\n'+row+s[pos:];p.write_text(s)
p=L/'README.md';s=p.read_text();a='例如正式设计“听书时问 AI 后继续”';pos=s.index(a);s=s[:pos]+'当前另有[Kindle型号与内容生命周期](cases/M-001-kindle.md)、[Khan创作与家庭控制](cases/M-030-khan-kids.md)、[AntennaPod正式版本的计时与备份](cases/M-040-antennapod.md)补证。功能目录与已研究任务分别登记，静态源码和演示摘帧不当作App实操。\n\n'+s[pos:];p.write_text(s)
p=R/'docs/ideas.md';s=p.read_text();s+='\n2026-10-27 继续落实I-021/I-022：Khan补创作录音、保存/导出、家庭成员与区域控制，保留教师ON/OFF文档冲突；Kindle按2024主样本与2025+分支核云盘及笔迹生命周期；AntennaPod固定正式3.12.3查计时、归档与数据库备份范围。两段官方动画仅离散摘帧，无App操作或学习效果评估。资料按功能目录→F任务→E来源→横向问题组织，新增事实补回旧编号；未将候选机制升级为需求或开始系统设计。\n';p.write_text(s)
p=R/'README.md';s=p.read_text();s=s.replace('最新简报；听读恢复、系统设置范围、AI产物修订和版本冲突，以及从设计问题反查任务与证据的方式。','最新简报；儿童创作与控制、听读计时、备份范围、型号条件及AI产物修订，附设计问题到任务和证据的检索方式。');p.write_text(s)
p=R/'docs/briefs/010-task-evidence-and-reuse.md';s=p.read_text().replace('整理：2026-10-27；本批查阅与网页操作：10月26日。','整理及资料续补：2026-10-27；有限实际网页操作：10月26日。')
start=s.index('| 设计问题 |');end=s.index('\n这些是竞品事实',start)
table='''| 设计问题 | 当前证据 | 可借鉴什么，仍缺什么 |
| --- | --- | --- |
| 除了做题，孩子能怎样表达所学？ | [Khan F-M030-07/08](../research/interaction-library/cases/M-030-khan-kids.md)：贴纸配口述录音→回放→存入作品集→另行导出相册；数学故事与复述是活动范例。 | 创作、保存、分享和掌握是不同结果。权限拒绝、退出恢复、导出格式与教学效果仍缺；不是自动评分证据。 |
| 限制孩子退出，具体限制了什么？ | [Khan F-M030-11](../research/interaction-library/cases/M-030-khan-kids.md)：家长可隐藏课程Home图标；教师控制另有菜单，两个教师帮助的ON/OFF描述冲突。 | 隐藏入口不证明不能退出系统；家长/教师设置也不能混用。未决定本项目采用强制完成机制，实际方向和设置范围待测。 |
| “再听十分钟”与“接着听”如何定义？ | [AntennaPod F-M040-08](../research/interaction-library/cases/M-040-antennapod.md)：正式3.12.3源码按位置事件到达的时间差计时，长间隔跳过；[TIME F-M047-04](../research/interaction-library/cases/M-047-time-for-kids.md)：网页暂停与关闭/刷新恢复不同。 | 截止时间、有效播放、会话与持久断点分别研究。AntennaPod没有运行测试；TIME没有实际听音，均不能证明完整通话打断恢复。 |
| “备份好了”能恢复哪些数据？ | [AntennaPod F-M040-12](../research/interaction-library/cases/M-040-antennapod.md)：代码导出数据库，全局设置另存；[Kindle F-M001-06/07](../research/interaction-library/cases/M-001-kindle.md)：断开云盘不删已导入云库，发给Alexa的附件也另存。 | 为文件、设置、会话及外部副本分别列恢复/删除范围。导入覆盖与失败回滚、未同步笔迹仍需验证。 |
| 同名产品与同名功能能直接合并吗？ | [Kindle](../research/interaction-library/cases/M-001-kindle.md)区分2024指南与2025+云盘/随手记；[BOOX](../research/interaction-library/cases/M-004-boox-neoreader.md)通用帮助不等于本机固件菜单。 | 型号、版本、账户和地区应随能力一起引用。新建58条指南目录不意味着58项功能已经完成研究。 |
| AI修改后的内容仍有原始依据吗？ | [NotebookLM F-M041-11](../research/interaction-library/cases/M-041-notebooklm.md)：桌面帮助称修订幻灯片不参考来源，提交后生成新版；[Kindle F-M001-05](../research/interaction-library/cases/M-001-kindle.md)：校订文字与可搜索手写PDF不同。 | 修改、事实核查、替换和回滚分别判断。尚未实际生成/导出，不将桌面文档或文字修订套到所有产物。 |
'''
s=s[:start]+table+s[end:];p.write_text(s)
print('updated 9 summary/index files; research scope unchanged')
