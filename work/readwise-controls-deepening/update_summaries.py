from pathlib import Path
import re
R=Path('/home/workspace/reader'); L=R/'docs/research/interaction-library'
brief='019-reading-queues-and-ai-actions.md'
p=R/'README.md';s=p.read_text();assert brief not in s
s=s.replace('## 项目资料\n','## 项目资料\n\n- [第十九期：阅读队列、内容版本与 AI 动作](/home/workspace/reader/docs/briefs/'+brief+')：最新简报；阅读节奏、正文更新与批注、分享范围和 AI 工具的证据边界。',1)
s=s.replace('：最新简报；词卡实际开关','：词卡实际开关');p.write_text(s)
p=L/'README.md';s=p.read_text().replace('最新入口：[第十八期简报]','前批入口：[第十八期简报]')
s+='\n最新入口：[第十九期简报](../../briefs/'+brief+')。Reader补阅读队列、内容版本、分享与原生/外部AI范围；Kobo补合集、页书签及显示/音频控件；不背单词核版本措辞与入口证据。静态图和文档步骤不当实际执行。\n';p.write_text(s)
p=L/'inventories/README.md';s=p.read_text();assert 'readwise-controls-deepening-inventory.csv' not in s
rows='''| [M015-Reader目录、控件与工作流](../batches/readwise-controls-deepening-inventory.csv) | 37侧栏、190正文标题、28静态控件、4工作流链接及22工具用途，共281行 | 父子项与来源重复；MCP客户端设置仅标题定位，工具用途不是调用schema或实操 |
| [M002-Kobo帮助与控件](../batches/kobo-controls-deepening-inventory.csv) | 74在线指南、28阅读索引、14阅读菜单、5显示控件、4暖光、11音频章节及5补充正文，共141行 | 通用帮助含其他型号；显示Chapter仅截图示例，N428完整固件菜单未知 |
| [M018-不背单词来源覆盖](../batches/bbdc-controls-deepening-inventory.csv) | 15商店功能/版本、13政策入口、3网站导航及3未闭合任务，共34行 | 商店25版本窗口不是全部历史；政策索引不当当前App实操，无新增已还原任务 |
'''
s=s.replace('| --- | --- | --- |\n','| --- | --- | --- |\n'+rows,1);p.write_text(s)
p=L/'comparison.md';s=p.read_text()
ins={
'## 学习与反馈':[
'| 制成问答卡是否就完成了学习闭环？ | [Reader F-M015-19](cases/M-015-readwise.md) 的.qa与问答笔记进入Readwise复习；[不背 F-M018-02](cases/M-018-bbdc.md) 的版本说明只称尝试修复记错按钮。 | 制卡入口、评分、纠错和下次调度分别取证；更新说明不证明修复成功。 | R-014/016/019、G-001/002 |'],
'## 读听与中断恢复':[
'| 收藏以后读、当前阅读队列与自动下一篇有什么区别？ | [Reader F-M015-13/14/25](cases/M-015-readwise.md) 分每日摘要、归档后的下一步和三种书库配置；Shortlist标签视图又不同于位置。 | 内容选择、排队与连续推进分别比较；打开摘要的徽标不当阅读完成。 | R-015/020、G-001/002 |'],
'## 新闻与内容选择':[
'| 更新已保存的文章会发生什么？ | [Reader F-M015-18](cases/M-015-readwise.md) 同URL重存会去重；帮助给出的删除重存会移除关联批注。 | 比较正文版本、批注定位和更新结果；不能由保存成功推断内容最新。 | R-016/020/021、G-002 |',
'| 取消订阅会停止哪一端的邮件？ | [Reader F-M015-22](cases/M-015-readwise.md) Unsubscribe在Reader屏蔽发件人，连转发也受影响，不是退出原邮件名单。 | 动作名称之外记录执行对象、恢复入口和连带影响。 | R-007/020/021、G-002 |'],
'## AI上下文与执行结果':[
'| 原生AI能做的动作是否等于外部连接的工具？ | [Reader F-M015-08/09/26](cases/M-015-readwise.md) 原生权限与确认卡、技能设置、外部工具用途分别记录；外部调用仍D1。 | 分开能力发现、参数约束、授权和执行结果；提示示例不当完整执行链。 | R-007/009/021、G-002 |'],
'## 同步备份与成员数据':[
'| 分享包会自动跟上集合变化吗？ | [Reader F-M015-24](cases/M-015-readwise.md) Bundle需手动刷新，新文档会加入已有接收者账号；删除传播仍未知。 | 内容快照、更新动作与接收端持有的数据分开核查。 | H-004/005、R-021、G-002 |'],
'## 系统交互与研究方法':[
'| 一个刷新选项截图能证明什么？ | [Kobo F-M002-13](cases/M-002-kobo.md) 静态图显示Chapter但没有展开；[Reader F-M015-16](cases/M-015-readwise.md) 应用电子纸选项也不证明面板刷新能力。 | 可见控件、默认值、完整档位和实际效果分别取证；不推导Plus兼容或60fps。 | R-002/010/011、G-002 |']}
for heading,rows in ins.items():
 start=s.index(heading);pos=s.index('| --- | --- | --- | --- |\n',start)+len('| --- | --- | --- | --- |\n');s=s[:pos]+'\n'.join(rows)+'\n'+s[pos:]
s=re.sub(r'(?m)(^\|[^\n]*\|)\n\n+(?=\|)',r'\1\n',s);p.write_text(s)
p=L/'backlog.md';s=p.read_text();old='| Kobo N428 | 导入、阅读导航、书内手写、独立笔记、音频、删除/恢复 | 可读取的型号整册；全刷设置、合集/书签；常规与无障碍分支的实际菜单；断网音频换章、重启续播与导入中断 |'
assert old in s;s=s.replace(old,'| Kobo N428 | 原有导入/笔迹/恢复，补合集创建管理删除、页书签回查及141条目录；两张显示/音频静态图 | 可读取的型号整册；刷新完整档位、合集跨端冲突、书签锚点与删除；固件/无障碍菜单、音频打断和重启续播 |')
s+='''
## 2026-10-27：阅读队列、内容版本与AI动作续查

- Reader：补19篇正文、五张静态图、281条目录及14任务；本批外部AI工具仅D1。当前App各配置的真实状态迁移、取消/失败/部分成功、问答卡后续复习、共享删除传播、批注迁移和离线冲突仍缺。长文返回上篇不能推导精确位置恢复，应用电子纸模式不证明面板效果。
- Kobo：补合集与页书签两任务，显示/导航/听书回原编号。Chapter只是设置截图中的值，蓝牙自动打开Wi-Fi不等于需要互联网；帮助覆盖其他型号，不能称N428完整菜单。旧PDF/EPUB的403路线未重试。
- 不背单词：补34项来源台账及4旧任务的条件说明；完整学习、误评纠正、退出/离线恢复依旧未取得。商店“尝试修复”、100词/组、04:00数据更新不能替代运行结果或队列规则；政策搜索正文与脚本壳分开，账号入口差异保留。

本批16新F（15 D2、1 D1）、16旧F补63字段；29新E、10旧E追加11次采集；七张新看静态图。没有产品操作或设备测试，所有案例仍部分覆盖。
''';p.write_text(s)
p=L/'analysis-status.md';s=p.read_text();pos=s.index('本轮之前的工作属于')
row='| 阅读队列、内容版本与AI动作深化（10月27日） | Reader17E/14F/281目录、Kobo12E/2F/141目录、不背0新E/F及34目录；16旧F补63字段，10旧E追加11次采集 | 无新增实操；外部AI仅声明，版本修复措辞、截图值与当前运行结果分开；全部仍部分覆盖 |'
s=s[:pos].rstrip()+'\n'+row+'\n\n'+s[pos:];s=re.sub(r'(?m)(^\|[^\n]*\|)\n\n+(?=\|)',r'\1\n',s);p.write_text(s)
p=R/'docs/ideas.md';s=p.read_text();s+='\n2026-10-27继续I-021/I-022：Reader补阅读队列、保存快照/更新与批注、邮件屏蔽、分享包及原生/外部AI范围；Kobo补合集/页书签和静态控件；不背区分版本修复措辞、词量分组与政策入口。稳定F/E与目录、横向问题继续关联，详见[第十九期简报](/home/workspace/reader/docs/briefs/'+brief+')。所有产品仍部分覆盖，未将参考机制升级为需求或启动系统设计。\n';p.write_text(s)
print('Updated README, library, inventories, comparison, backlog, status and ideas')
