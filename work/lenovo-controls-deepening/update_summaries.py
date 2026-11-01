from pathlib import Path
import re
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';brief='017-action-targets-and-lifecycle.md'
p=R/'README.md';s=p.read_text();assert brief not in s
s=s.replace('## 项目资料\n','## 项目资料\n\n- [第十七期：动作作用于哪里，数据留在哪里](/home/workspace/reader/docs/briefs/'+brief+')：最新简报；设备与配套App的控制范围、定时停止、应用生命周期和账号可见性。',1)
s=s.replace('：最新简报；临时副本','：临时副本');p.write_text(s)
p=L/'README.md';s=p.read_text().replace('最新入口：[第十六期简报]','前批入口：[第十六期简报]');s+='\n最新入口：[第十七期简报](../../briefs/'+brief+')。联想Smart Paper、山灵M1 Plus和iBasso DX180补控件/目录、应用生命周期、计时与控制对象；通用App、型号设备和版本声明分别限定。\n';p.write_text(s)
p=L/'inventories/README.md';s=p.read_text();needle='| --- | --- | --- |\n';assert 'lenovo-controls-deepening-inventory.csv' not in s
rows='''| [M062-Lenovo指南与控件](../batches/lenovo-controls-deepening-inventory.csv) | 31印刷目录、91正文控件/组及5FAQ选题，共127行 | 2023指南p3–17功能正文；父子项重叠，FAQ为区域索引，不是现版全菜单 |
| [M063-M1 Plus与Eddict范围](../batches/shanling-controls-deepening-inventory.csv) | 型号指南38、型号评测39、共用App37，共114行 | M1与App分别标对象；示例SCD3.3/UA7不是M1；无新增设备任务或操作 |
| [M064-DX180手册与版本](../batches/ibasso-controls-deepening-inventory.csv) | 13目录、41功能域、37静态菜单、37版本声明及1区域解释，共129行 | 历史手册与1.04/1.05分开；看见菜单不等于进入操作，版本声明不升现版D2 |
'''
s=s.replace(needle,needle+rows,1);p.write_text(s)
p=L/'comparison.md';s=p.read_text()
ins={
'## 同步备份与成员数据':[
'| 文件不在当前列表里，就能诊断丢失吗？ | [Lenovo F-M062-19](cases/M-062-lenovo-paper.md) FAQ把一种突然不可见解释为账号隐藏；重登原账号是该分支的排查。 | 分开身份可见性、实际存储、误删与云恢复；不能用一条排查覆盖所有丢失。 | H-004/005、R-021、G-002 |',
'| 支持安装App，是否就支持完整恢复？ | [DX180 F-M064-13/14](cases/M-064-ibasso.md) 安装/卸载有步骤，数据恢复未明；[Lenovo F-M062-18](cases/M-062-lenovo-paper.md) 定制WPS另有限制。 | 程序包、用户资料和账号授权分别核查生命周期。 | R-008、H-004/005、G-002 |'],
'## 系统交互与研究方法':[
'| 配套App设置是否控制正在连接的设备？ | [山灵 F-M063-08/10](cases/M-063-shanling.md) Eddict播放设置只影响App本地播放，SyncLink是另一控制范围。 | 能力描述带执行设备、运行端与目标对象；通用手册不补型号缺口。 | R-007/009/018、G-002 |',
'| 定时停止究竟停止什么？ | [DX180 F-M064-06](cases/M-064-ibasso.md) 版本资料区分系统关机与Mango停止；[Lenovo F-M062-16](cases/M-062-lenovo-paper.md) 自动锁屏与闲置十天关机也不同。 | 结束动作、触发条件和重新计时分开；当前固件和恢复仍待验证。 | R-018/021、G-002 |',
'| 页面上有撤销，就能恢复删页或合层吗？ | [Lenovo F-M062-11/13](cases/M-062-lenovo-paper.md) 十步撤销、页/层工具和文件回收站分别有依据，作用交集没有说明。 | 对象和可逆范围要查明，不能仅按控件名称推断。 | R-007/021、G-002 |']}
for heading,rows in ins.items():
 start=s.index(heading);pos=s.index('| --- | --- | --- | --- |\n',start)+len('| --- | --- | --- | --- |\n');s=s[:pos]+'\n'.join(rows)+'\n'+s[pos:]
s=re.sub(r'(?m)(^\|[^\n]*\|)\n\n+(?=\|)',r'\1\n',s);p.write_text(s)
p=L/'backlog.md';s=p.read_text();s+='''
## 2026-10-27：动作对象与应用生命周期续查

- Lenovo SP101FU：补2023功能正文、127条目录/控件/FAQ选题及19条累计任务；仍缺层合并/删页撤销、识别校订、录音关联、Trash时限、跳转返回、账号变更/云冲突/过期与应用恢复。区域FAQ索引不能当统一当前固件。
- 山灵M1 Plus：补114条型号/评测/共用App分层目录，Eddict本地播放设置不用于填设备计时/断点。高级型号手册、SyncLink无线冲突/拒权/后台、文件与列表删除、路径失效、计时重置仍缺。三条403路线停止，无新设备操作。
- iBasso DX180：补129条手册/菜单/版本目录、安装/卸载和USB对外充电；保留中文计时疑误、日本区域解释及当前1.05范围。仍缺应用数据回导、权限拒绝/安装失败、USB退出/低电量、Time排序含义、ID3编辑、搜索清空/无结果和现版计时恢复。

本批14新D2、12旧F补44字段；目录不当独立功能数量。没有App或设备操作，触控、显示、听感与实际恢复均不据文字宣告完成。
''';p.write_text(s)
p=L/'analysis-status.md';s=p.read_text();pos=s.index('本轮之前的工作属于')
row='| 动作对象与应用生命周期深化（10月27日） | Lenovo1E/11F/127目录，山灵3E/0新F/114目录，DX1801E/3F/129目录；补12旧F的44字段、8旧E追加采集 | 无新增实操；本地App设置、设备控制、版本声明与恢复范围分开；全部仍部分覆盖 |'
s=s[:pos].rstrip()+'\n'+row+'\n\n'+s[pos:];s=re.sub(r'(?m)(^\|[^\n]*\|)\n\n+(?=\|)',r'\1\n',s);p.write_text(s)
p=R/'docs/ideas.md';s=p.read_text();s+='\n2026-10-27继续I-021/I-022：Lenovo、山灵与iBasso深化动作作用对象、App安装/卸载、账号可见性、控制端/设备端和计时版本；目录含父子项与未闭合分支，原F/E继续沿用。详见[第十七期简报](/home/workspace/reader/docs/briefs/'+brief+')。这些差别作为后续设计依据，未新增已确认需求、账户/备份制度或启动全系统设计。\n';p.write_text(s)
p=R/'docs/briefs'/brief;s=p.read_text().replace('与F-M064-13。','与F-M064-13/14。');p.write_text(s)
print('Updated README, library, inventories, comparison, backlog, status, ideas and brief')
