from pathlib import Path
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library'
p=root/'docs/briefs/010-task-evidence-and-reuse.md';s=p.read_text();a=s.index('| 设计问题 |');b=s.index('\n这些是竞品事实',a)
s=s[:a]+'''| 设计问题 | 当前证据 | 可借鉴什么，仍缺什么 |
| --- | --- | --- |
| AI生成一个任务，就代表已经完成吗？ | [快捷指令 F-M043-01/07/08](../research/interaction-library/cases/M-043-shortcuts.md)：自然语言生成后仍可编辑、运行；模型Follow Up可先修订响应；变量可查看类型和来源。 | 生成方案、传递资料、实际写入与成功结果分别追踪。取消追问、生成错误、权限拒绝后的部分结果仍未实测。 |
| 同步规则后，另一台设备会自动执行吗？ | [快捷指令 F-M043-10/11](../research/interaction-library/cases/M-043-shortcuts.md)：26指南说个人自动化不跨设备同步；27改为可同步，但另一设备默认停用。 | 配置存在、启用状态、锁屏条件和实际执行分别研究；不能从“同步成功”推断任务已经开始或可接续。 |
| “备份好了”能恢复哪些数据？ | [Mudita F-M012-10](../research/interaction-library/cases/M-012-mudita.md)：Kompakt备份列联系人、通话、消息、闹钟、笔记，Center4.0另加日历；[AntennaPod F-M040-12](../research/interaction-library/cases/M-040-antennapod.md)的数据库与全局设置也分开。 | 先列恢复对象与版本，再讨论整机/按App。第三方App数据、媒体、覆盖/合并、断线失败均不能靠“支持备份”补齐。 |
| 安装、隐藏、删安装包与卸载是一回事吗？ | [Mudita F-M012-08/09](../research/interaction-library/cases/M-012-mudita.md)：删APK不影响已安装App；隐藏只改布局；当前FAQ无内置单应用卸载，恢复出厂清数据但不回退OS。 | AI动作需要准确指向对象和后果；安装兼容、升级覆盖与数据保留仍缺。这些Android流程不证明Plus能执行APK。 |
| 除了做题，孩子能怎样表达所学？ | [Khan F-M030-07/08](../research/interaction-library/cases/M-030-khan-kids.md)：贴纸配口述录音→回放→存入作品集→另行导出相册；数学故事与复述是活动范例。 | 创作、保存、分享和掌握是不同结果。权限拒绝、退出恢复、导出格式与教学效果仍缺；不是自动评分证据。 |
| 看见实体键和游戏入口，能还原学习机制吗？ | [百词斩设备 F-M022-02](../research/interaction-library/cases/M-022-baicizhan-device.md)：BCZ-Ace-Max原作者拆解说明方向、确认、发音、返回与单击斩词用途，未展示动作后状态。 | 仍为D1；答错、误斩撤销、复习变化和宠物奖励链未知。图标、按键说明和宣传卖点不能拼成已验证流程。 |
''' + s[b:]
s=s.replace('接下来仍需补中文学习的一次完整回路、真实听音与中断恢复、AI生成内容的纠错、安装卸载与备份恢复，以及各型号的完整菜单和版本条件。','接下来仍需补中文学习的一次完整回路、真实听音与中断恢复、AI生成及执行失败、安装升级和备份恢复的实际结果，以及各型号的完整菜单和版本条件。')
p.write_text(s)
# Add design questions to existing domains; facts remain in primary cases.
p=lib/'comparison.md';s=p.read_text()
adds={
'AI上下文与执行结果':[
'| 模型响应何时交给下一个动作？ | [快捷指令 F-M043-07/08](cases/M-043-shortcuts.md) Follow Up允许先修订响应；变量可改类型、属性并回溯来源动作。 | 生成、审阅、对象转换和写入分别可见；取消与类型失败仍需操作验证。 | G-002、R-007/009/021 |',
'| 出现通知是否等于已经确认？ | [快捷指令 F-M043-06](cases/M-043-shortcuts.md) Show Notification立即继续；Show Alert可暂停并由Done继续或Cancel停止。 | 区分告知、等待输入、确认和取消；不把通知已发当用户同意或业务完成。 | G-002、R-007 |',
'| AI查到的是全库还是上一动作的结果？ | [快捷指令 F-M043-09](cases/M-043-shortcuts.md) 连续Find可能筛前一步输出，插Nothing才重新获取自身内容；另可All/Any、排序与限量。 | 查询对象、范围、过滤和上限应可检查；零结果和部分结果不应笼统解释为没有资料。 | G-002、R-016/021 |'],
'同步备份与成员数据':[
'| 另一台设备收到自动化规则后会马上执行吗？ | [快捷指令 F-M043-10/11](cases/M-043-shortcuts.md) 26不跨设备同步个人自动化；27另一设备默认停用。 | 配置、启用、运行环境和执行会话分开；保留版本条件，不推定重复执行或自动接续。 | G-002、H-004/005 |',
'| 系统备份是否包含媒体和第三方App数据？ | [Mudita F-M012-10](cases/M-012-mudita.md) Kompakt产品页列五类数据，Center4.0加日历；Pure设置备份不能套用。 | 按数据类别与版本列恢复范围，明确覆盖/合并/失败未知；账户与备份形式保持待定。 | H-004/005、G-002 |'],
'系统交互与研究方法':[
'| “删掉这个App”作用于哪个对象？ | [Mudita F-M012-08/09](cases/M-012-mudita.md) 隐藏图标、删APK、卸载、恢复出厂不同；恢复出厂不回退OS。 | 为AI和用户明确对象、数据后果及恢复范围；Plus运行时仍需另行评估。 | R-007/008、G-002 |',
'| 实体离线开关还能保留语音唤醒和来电吗？ | [Mudita F-M012-05](cases/M-012-mudita.md) Offline+硬件停用GSM/麦克风，软件停用相机/Wi-Fi/蓝牙；1.6.1有重启后PIN例外。 | 专注状态、可达性与唤醒要求存在取舍；不能直接套用开关行为，恢复延迟和音频仍未测。 | R-012/013、G-001 |']}
for title,rows in adds.items():
 start=s.index('## '+title); pos=s.index('| --- | --- | --- | --- |',start)+len('| --- | --- | --- | --- |')
 s=s[:pos]+'\n'+'\n'.join(rows)+s[pos:]
p.write_text(s)
# Functional directory entry points.
p=lib/'inventories/README.md';s=p.read_text();marker='| --- | --- | --- |';pos=s.index(marker)+len(marker)
s=s[:pos]+'''\n| [M043-快捷指令27指南目录](../batches/shortcuts-controls-deepening-inventory.csv) | 19分组及95主题，共114节点；保留父级、原链接和正文覆盖 | 欢迎链接无版本，其余主题为10.0/27；含非功能页，不是全部动态App动作，目录未读项不升级 |
| [M012-Kompakt手册/帮助/版本目录](../artifacts/mudita-controls-deepening/function-inventory.csv) | 手册15项、帮助81篇与1.6附件15功能域，共111行 | 64条只读目录；手册未标OS、附件入口对应1.6.1；有范围重叠，无设备操作 |'''+s[pos:];p.write_text(s)
# Backlog has initial seed questions as historical scope; refine current priorities.
p=lib/'backlog.md';s=p.read_text();s=s.replace('百词斩历史下发片段；墨墨资源、学习记录、按键和无响应分流','百词斩历史下发及BCZ-Ace-Max拆解键义；墨墨资源/记录与故障分流')
lines=s.splitlines(); changes={
'| M-012 Mudita Kompakt |':'| M-012 Mudita Kompakt | 已补K1.6.1/Center4.0安装、更新、备份与111项目录；下一步固定构建核来电打断、Offline+恢复、短信草稿、备份覆盖及未读64项。 |',
'| M-022 百词斩 Mini/Max |':'| M-022 百词斩 Mini/Max | BCZ-Ace-Max独立拆解仅补实体键用途，3F仍D1；下一步型号化完整学习、错误/误斩恢复、宠物奖励、中断及离线同步。 |',
'| M-043 Apple 快捷指令 |':'| M-043 Apple 快捷指令 | 已补27指南114节点及模型/变量/筛选/自动化/分享；下一步Follow Up取消、空结果、权限拒绝后产物、锁屏执行、跨运行存储与未读正文。 |'}
for i,line in enumerate(lines):
 for key,value in changes.items():
  if line.startswith(key):lines[i]=value
p.write_text('\n'.join(lines)+'\n')
# Visible latest entry points.
p=root/'README.md';s=p.read_text();s=s.replace('最新简报；儿童创作与控制、听读计时、备份范围、型号条件及AI产物修订，附设计问题到任务和证据的检索方式。','最新简报；AI动作与自动化、安装和备份范围、儿童创作及证据缺口，附设计问题到任务和证据的检索方式。');p.write_text(s)
p=lib/'README.md';s=p.read_text();needle='当前另有[Kindle型号与内容生命周期]';start=s.index(needle);end=s.index('\n',start)
s=s[:start]+'当前新增[快捷指令的模型、对象与自动化](cases/M-043-shortcuts.md)、[Kompakt安装与恢复](cases/M-012-mudita.md)，并为[百词斩单词机](cases/M-022-baicizhan-device.md)补型号化实体键来源，仍保持D1。逐条目录、已研究任务和未验证分支分别登记；文字帮助、单帧与实际操作分开。'+s[end:];p.write_text(s)
p=lib/'analysis-status.md';s=p.read_text();marker='\n本轮之前的工作属于';pos=s.index(marker)
s=s[:pos]+'| AI动作、自动化与手机恢复深化（10月27日） | 快捷指令25E/6新F/114目录，Kompakt22E/3新F/111目录及2单帧，百词斩设备1E；补回11旧F | 模型/执行、规则/启用、安装包/应用、备份类别分别处理；百词斩仍D1，未增加实机或D4记录，全部部分覆盖 |\n'+s[pos:]
# Keep the new row adjacent to the prior table, no blank separator within it.
s=s.replace('\n\n| AI动作、自动化','\n| AI动作、自动化');p.write_text(s)
p=root/'docs/ideas.md';s=p.read_text();s+='\n2026-10-27续补I-021：快捷指令按26/27分别研究AI生成与模型输出、变量来源、自动化启用与配置同步；Kompakt区分隐藏、删安装包、卸载、恢复出厂和备份类别；百词斩设备只取得实体键说明，完整学习与奖励链仍缺。新增目录、任务和证据已归档至原编号及[第十期简报](/home/workspace/reader/docs/briefs/010-task-evidence-and-reuse.md)。本轮无原生App或设备操作，未把上述参考转为本项目已确认需求、账户制度或详细架构。\n';p.write_text(s)
print('Updated brief, comparison, inventories, backlog, READMEs, status and ideas')
