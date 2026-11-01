from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
def replace(p,old,new):
 s=p.read_text();assert s.count(old)==1,(p,old[:80],s.count(old));p.write_text(s.replace(old,new))
replace(R/'README.md',
 '- [第二十期：儿童纠错、任务完成与可恢复状态](/home/workspace/reader/docs/briefs/020-child-feedback-and-resumable-state.md)：最新简报；',
 '- [第二十一期：读完、少推和清除](/home/workspace/reader/docs/briefs/021-reading-feedback-and-data-reset.md)：最新简报；历史阅读终点、推荐控制与数据重置的不同作用，型号按键与恢复缺口。\n- [第二十期：儿童纠错、任务完成与可恢复状态](/home/workspace/reader/docs/briefs/020-child-feedback-and-resumable-state.md)：')
replace(L/'README.md','最新入口：[第二十期简报]','前批入口：[第二十期简报]')
with (L/'README.md').open('a') as f:f.write('\n最新入口：[第二十一期简报](../../briefs/021-reading-feedback-and-data-reset.md)。Espresso补历史完成链与机构控件；头条拆推荐控制、历史兴趣和个人副本；汉王N10 Pro二代补作者使用报告中的按键与入口。没有新增实操，历史版本、政策声明和设备未知分开。\n')
replace(L/'inventories/README.md','| --- | --- | --- |\n',
 '| --- | --- | --- |\n'
 '| [M045-Espresso来源、控件与历史版本](../batches/espresso-controls-deepening-inventory.csv) | 4机构正文、16静态控件组、6历史记录、25版本、4相邻服务边界及6缺口，共61行 | 2017完成链与2025机构图不套现版；Pro API边界不计为Espresso功能，未取得全App菜单 |\n'
 '| [M052-头条读者功能来源](../batches/toutiao-controls-deepening-inventory.csv) | 推荐/历史/数据出口及来源范围，共22行 | 主要是政策路径；当前页面/长按菜单/恢复未知，创作者目录另列 |\n'
 '| [M008-汉王二代来源与报告控件](../batches/hanvon-controls-deepening-inventory.csv) | 来源5、文章栏目15、控件/入口/能力/条件26及缺口7，共53行 | 作者推荐性文章不是官方手册；全部原任务仍D1，没有新看图或实机操作 |\n')
with (L/'inventories/README.md').open('a') as f:f.write('\n另存[头条创作者帮助目录](../artifacts/toutiao-controls-deepening/creator-help-index.csv)：73个唯一父子节点，只有一篇FAQ读到正文。它用于划清来源范围，不是读者功能目录，也不补读者学习/新闻覆盖。统计目录行时需保留这一类别区别。\n')
def add_table_rows(section,rows):
 p=L/'comparison.md';s=p.read_text();pos=s.index('## '+section+'\n');separator=s.index('\n| ---',pos);end=s.index('\n',separator+1);s=s[:end+1]+rows+s[end+1:];p.write_text(s)
add_table_rows('新闻与内容选择',
 '| 结束页能证明已理解或能跨日恢复吗？ | [Espresso F-M045-01](cases/M-045-espresso.md) 的2017作者记录有勾与末尾结束页；2025机构图只有部分勾，当前触发/恢复未核。 | 内容份数、读完反馈、理解与持久进度分别评价，历史七项不套当前五篇短文。 | G-001、R-020 |\n'
 '| 少推、重置兴趣和扩大视野是否同一控制？ | [头条 F-M052-02/03/04](cases/M-052-toutiao.md) 政策分总开关、位置/搜索/关注特征、标签、丰富度和清兴趣；重置后再积累。 | 分开供给规则、数据用途和历史状态；丰富度不等于质量，减少不等于绝对屏蔽。 | G-001、R-020、G-002 |\n')
add_table_rows('同步备份与成员数据',
 '| 个人信息副本能否恢复新闻App？ | [头条 F-M052-07](cases/M-052-toutiao.md) 申请需邮箱接收下载链接；格式、范围、导回和位置恢复未述。 | 导出、备份、可重建状态分别验证；有记录列表不代表正文/收藏内容和关系齐全。 | H-004/005、G-002 |\n')
add_table_rows('系统交互与研究方法',
 '| 按键和快捷入口如何影响正在做的事？ | [汉王 F-M008-01/06](cases/M-008-hanvon.md) 作者区分中央短返回/长刷新、其他键和灵动条入口；未给离开后的保存/后台结果。 | 核实际输入方式、动作目标、焦点和恢复，不能套普通Android触控或后台规则。 | R-010/018、G-002 |\n')
replace(L/'backlog.md',
 '| Espresso | Settings 部分路径、官方静态文章/音频入口 | 一期完整阅读与听读过程；问答错答反馈、末篇结束；收藏/搜索/离线是否存在及对应操作 |',
 '| Espresso | 2017作者完成链及三图、2025机构四图、25版本窗口，61来源/控件/缺口项 | 当前已读触发/末篇结束、问答纠错、音频/翻译返回、收藏/搜索/离线与次日恢复；Pro API不补个人状态 |\n'
 '| 今日头条 | 22功能来源项，73创作者目录另列；推荐特征、无痕、兴趣重置和个人副本文档 | 当前长按理由/作用对象、频道编辑、关注与收藏续读、来源跳转、误操作/断网；政策差异不当现版UI结果 |')
replace(L/'backlog.md',
 '| 汉王 N10 Pro 二代 | 同机型官方范围、一手评测中两张听书/分屏静态图 | 固件号、机内完整手册；播放与分屏执行前后、取消、退出、原稿关联及中断；代际不明的手册预览已排除 |',
 '| 汉王 N10 Pro 二代 | 官方范围、旧两张听书/分屏图；新增两篇作者正文及53来源/功能/缺口项 | 固件与机内完整手册；笔键“手触”、配置保存、后台听读、导出再编辑和恢复；全部六任务仍D1 |')
with (L/'backlog.md').open('a') as f:f.write('''
## 2026-10-27：完成反馈、推荐控制与数据重置续查

- Espresso：F01只在2017作者链升D2；七张新看静态图不组成连续记录。当前日报/测验/听读恢复仍缺，25条商店日志只是历史窗口。Pro内容API作为相邻服务边界，不冒充Espresso动作或个人记录。
- 头条：五旧任务补口径，一新任务是个人副本申请的政策路径。旧规则“永久”与重新积累、管理指引与当前政策的删除范围差异保留；无痕阅读/搜索、推荐特征/广告与显式关注分开。创作者73目录只有一篇正文，不能补读者全菜单。
- 汉王：两个作者同型号正文补按键/灵动条/绑定条件，0新任务；全部D1。知乎403和另一站验证码壳停止，未取得完整手册，不把“手触”笔键解释成屏幕触控。

本批8新E、1新D2；14旧F补60字段，5旧E追加本地/新采集。三份功能来源台账136行，另有创作者73目录；七张静态图，0新实操。所有产品仍部分覆盖。
''')
replace(L/'analysis-status.md','\n\n本轮之前的工作属于“有进展”',
 '\n| 完成反馈、推荐控制与重置深化（10月27日） | Espresso3E/0新F/61目录及七静态图；头条3E/1F/22功能与73创作者目录；汉王2E/0新F/53目录；14旧F补60字段、5旧E追加 | 旧日报仅2017历史链升D2，当前阅读主链和汉王六任务缺口保留；无新增实操，全部部分覆盖 |\n\n本轮之前的工作属于“有进展”')
with (R/'docs/ideas.md').open('a') as f:f.write('\n2026-10-27继续I-021/I-022：Espresso补历史末篇反馈与不同年代控件；头条区分少推/关特征/清兴趣/下载个人副本，保留政策版本差异；汉王二代补作者报告中的输入入口与恢复缺口。详见[第二十一期简报](/home/workspace/reader/docs/briefs/021-reading-feedback-and-data-reset.md)。一期完成、理解收益、推荐多样性与持久保存仍是分别待验证的问题；参考没有升级为需求或进入系统设计。\n')
print('Updated brief entries, inventories, comparison, backlog, status and ideas; requirements unchanged')
