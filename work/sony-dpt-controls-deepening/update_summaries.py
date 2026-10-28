from pathlib import Path
import re
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
brief='016-visibility-editability-and-recovery.md'
p=R/'README.md';s=p.read_text();assert brief not in s
s=s.replace('## 项目资料\n','## 项目资料\n\n- [第十六期：可见、可改与可恢复](/home/workspace/reader/docs/briefs/'+brief+')：最新简报；临时副本、笔迹与显示、删除路径、跨端连接及恢复条件。',1)
s=s.replace('：最新简报；儿童构数','：儿童构数');p.write_text(s)
p=L/'README.md';s=p.read_text();s=s.replace('最新入口：[第十五期简报]','前批入口：[第十五期简报]');s+='\n最新入口：[第十六期简报](../../briefs/'+brief+')。Sony DPT、QUADERNO和MatePad Paper补文件副本、批注与录音位置、删除/同步/恢复及系统状态；官方目录与当前实机分开，来源日期和地区条件保留。\n';p.write_text(s)
p=L/'inventories/README.md';s=p.read_text();needle='| --- | --- | --- |\n';assert 'sony-dpt-controls-deepening-inventory.csv' not in s
rows='''| [M059-Sony DPT 1.6帮助与设置](../batches/sony-dpt-controls-deepening-inventory.csv) | 73叶入口（72唯一URL）及44设置项，共117行 | 固定RP1/CP1；模板重复、设置父项和故障不是独立完成任务；当前OS及实机未核 |
| [M060-QUADERNO Gen.3C及Mobile帮助](../batches/quaderno-controls-deepening-inventory.csv) | 212书签节点、44设置、9笔键选项、Mobile3类7篇，共275行 | PDF选定76正文页核读；Mobile未标构建，官网公开日与PDF发行月分开；无设备操作 |
| [M061-MatePad Paper来源功能域](../batches/huawei-controls-deepening-inventory.csv) | 25个来源功能域 | 未取得完整官方功能目录；大陆与CA/NZ/LK/BD条件分开，录音图标和裁边单帧不是过程 |
'''
s=s.replace(needle,needle+rows,1);p.write_text(s)
p=L/'comparison.md';s=p.read_text()
insertions={
'## 同步备份与成员数据':[
'| 电脑上打开过，是否已经有备份？ | [Sony DPT F-M059-20/21](cases/M-059-sony-dpt.md) 双击为临时只读副本，关配套App删除；传出文件与画面显示另有入口。 | 内容可见、临时缓存、永久保存和可恢复状态分别核验。 | H-004/005、G-002 |',
'| 回收站能覆盖所有删除路径吗？ | [QUADERNO F-M060-04/08/16](cases/M-060-quaderno.md) PC App及指定同步删除绕过回收站，旧固件修复另有适用范围。 | 记录动作端、同步传播和现存副本；专项修复不当通用恢复。 | H-004/005、R-007、G-002 |'],
'## 导入笔记与内容关联':[
'| 看不见批注或贴纸就说明丢失吗？ | [QUADERNO F-M060-10](cases/M-060-quaderno.md) 接收阅读器可能不显示批注；[华为 F-M061-13](cases/M-061-huawei-paper.md) 特定贴纸与纸色灰度接近会不可见。 | 内容存在、格式支持和显示状态分开，避免把重复导入当默认修复。 | R-016/021、G-002 |',
'| 录音标记是否能定位对应笔迹？ | [华为 F-M061-12](cases/M-061-huawei-paper.md) 区域帮助说明点标记跳音频时间，可移动/删除；没有证明同步笔迹和转写句子。 | 时间锚点、文字锚点与笔迹关联分开，保留来源/区域/构建限制。 | R-018/021、G-002 |'],
'## 系统交互与研究方法':[
'| 返回位置、撤销笔画和恢复笔记页相同吗？ | [Sony DPT F-M059-13/15/17](cases/M-059-sony-dpt.md) 普通划页不入跳转历史，换页清撤销，删笔记页不可还原。 | 根据动作类型查询仍有效的返回和恢复能力，不按同名按钮承诺。 | R-007/010/021、G-002 |',
'| 手机切到后台后传入还能继续吗？ | [QUADERNO F-M060-09](cases/M-060-quaderno.md) 共用帮助明确切App断开Wi-Fi直连；待传列表不是已经传入。 | 连接状态、队列状态和目标设备实际文件分别确认；传中断开结果仍未知。 | R-016、G-002 |']}
for heading,rows in insertions.items():
 start=s.index(heading);pos=s.index('| --- | --- | --- | --- |\n',start)+len('| --- | --- | --- | --- |\n')
 s=s[:pos]+'\n'.join(rows)+'\n'+s[pos:]
# Fix stray appended rows that blank lines had detached from their four-column tables.
s=re.sub(r'(?m)(^\|[^\n]*\|)\n\n+(?=\|)',r'\1\n',s);p.write_text(s)
p=L/'backlog.md';s=p.read_text();s+='''
## 2026-10-27：办公电子纸状态与恢复续查

- Sony DPT-RP1/CP1：固定1.6目录与设置，已补文件/模板/表单/笔迹/临时副本/连接与复位。仍缺当前OS兼容、首次配对每屏、原文改名或删除后的side-note关系、同步删改并发、备份回导及真实笔触；旧Reset警告不能当实测损失范围。
- QUADERNO Gen.3C：已补Mobile7篇、标记/颜色检索、关联笔记、扫描与专项修复。继续查Gen.2逐项差异、脚踏/模板完整链、手机中断与同名文件回执、恢复副本完整性。官网公开日与PDF内发行月已拆开，QHG00001连接措辞冲突保留。
- MatePad Paper：已补笔记录音、同步排查、跨端文件、PDF显示与系统手势。仍未取得完整官方指南目录；区域Wi-Fi4+64帮助不替代大陆当前构建。需查离开录音保存、笔迹/音频联动、源PDF改写、同步删除/冲突、账户与整机备份。

新目录含重复、父项和故障；无新增设备或App操作。上述任务继续沿用F/E编号，不能以已建案例或文档条目数判定全功能完成。
''';p.write_text(s)
p=L/'analysis-status.md';s=p.read_text();marker='本轮之前的工作属于';pos=s.index(marker)
row='| 办公电子纸文件、笔迹与恢复深化（10月27日） | Sony DPT34E/17F/117目录，QUADERNO9E/8F/275目录，华为13E/7F/25功能域；补18旧F的66字段，14旧E追加采集，5旧E日期口径7字段修订 | 无新增实操；临时副本、编辑状态、灰度呈现与恢复路径分开；全部仍部分覆盖 |'
s=s[:pos].rstrip()+'\n'+row+'\n\n'+s[pos:]
s=re.sub(r'(?m)(^\|[^\n]*\|)\n\n+(?=\|)',r'\1\n',s);p.write_text(s)
p=R/'docs/ideas.md';s=p.read_text();s+='\n2026-10-27继续I-021/I-022：Sony DPT、QUADERNO与MatePad Paper深化文件副本、笔迹与录音位置、显示/编辑条件、跨端连接及分层恢复；修正官网公开日与PDF发行日混写，保留地区和构建范围。证据与任务补回原编号，见[第十六期简报](/home/workspace/reader/docs/briefs/'+brief+')。这些是后续设计参考，未新增已确认功能、备份制度或启动全系统设计。\n';p.write_text(s)
print('Updated README, library entry, inventories, comparison, backlog, status and ideas')
