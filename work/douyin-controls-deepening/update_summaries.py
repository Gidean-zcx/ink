from pathlib import Path
import csv,json
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/douyin-controls-deepening'
assert not (W/'summaries-updated.json').exists(),'one-time summaries'
names=['douyin-controls-deepening','hisense-controls-deepening','baicizhan-device-controls-deepening-2'];counts={n:len(list(csv.DictReader((L/f'batches/{n}-inventory.csv').open()))) for n in names}
def replace(p,a,b):
 s=p.read_text();assert s.count(a)==1,(p,a[:35],s.count(a));p.write_text(s.replace(a,b))
def append(p,s):
 with p.open('a') as h:h.write(s)
replace(R/'README.md','- [第二十三期：学完、保存与恢复](/home/workspace/reader/docs/briefs/023-learning-completion-and-records.md)：最新简报；','- [第二十四期：控件范围与交互证据](/home/workspace/reader/docs/briefs/024-controls-scope-and-evidence.md)：最新简报；推荐控制、儿童时间管理、当前App显示与保存对象，及单词机型号排除。\n- [第二十三期：学完、保存与恢复](/home/workspace/reader/docs/briefs/023-learning-completion-and-records.md)：')
replace(L/'README.md','最新入口：[第二十三期简报]','前批入口：[第二十三期简报]')
append(L/'README.md','\n最新入口：[第二十四期简报](../../briefs/024-controls-scope-and-evidence.md)。抖音补两篇完整图像指南和功能图，海信补机内引导/设置照片，百词斩单词机核帮助型号范围。文档步骤、静态控件与实际结果仍分开；所有案例部分覆盖。\n')
rows='| [M051-抖音指南与控件](../batches/douyin-controls-deepening-inventory.csv) | 105行：两篇完整图像指南、两篇功能文章的章节/控件/频道 | 静态图不是连续操作；夜间限制与年龄/实名条件冲突保留，主观看链D1 |\n| [M055-海信分型号来源与控件](../batches/hisense-controls-deepening-inventory.csv) | 114行：文章目录、五张照片控件、旧线索及型号缺口 | A9/TOUCH/Hi Reader及联名款分别处理；无完整官方手册与实机操作 |\n| [M022-单词机帮助型号核查](../batches/baicizhan-device-controls-deepening-2-inventory.csv) | 45行：17既有范围/历史条目，5类23问另列排除 | 超级单词机未证实适用Mini/Max；不升级原3条D1任务 |\n'
replace(L/'inventories/README.md','| --- | --- | --- |\n','| --- | --- | --- |\n'+rows)
def table(section,rows):
 p=L/'comparison.md';s=p.read_text();i=s.index('## '+section+'\n');j=s.index('\n| ---',i);end=s.index('\n',j+1);p.write_text(s[:end+1]+rows+s[end+1:])
table('新闻与内容选择','| 不感兴趣、拉黑与关键词屏蔽改变同一对象吗？ | [抖音F02/03/10](cases/M-051-douyin.md) 分视频、类别、关键词和账号；图示有确认与永久/短期分支，结果未实测。 | 明确作用范围、期限、撤销和提交回执；偏好命中不等于信息价值或事实正确。 | G-001、G-002 |\n')
table('AI上下文与执行结果','| AI笔记与事实核查卡怎样返回来源？ | [抖音F06/11](cases/M-051-douyin.md) 摘要章节与求真入口分别登记，仅静态图文。 | 原视频位置、可核来源、失败/纠错/修订与保存对象均需明确；AI标签不证明正确。 | G-002、R-019 |\n')
table('导入笔记与内容关联','| 保存截图、收藏文章、录语音是否同一种笔记？ | [Hi Reader F06/07/10](cases/M-055-hisense.md) 机内指南分别列截屏便笺与收藏，另有速记。 | 数据格式、来源关联、管理入口和恢复分别查；不能只因同一快捷栏就假设共享生命周期。 | G-002、R-013/018 |\n')
table('系统交互与研究方法','| 快捷键和刷新模式在哪种状态生效？ | [Hi Reader F09/10及显示目录](cases/M-055-hisense.md) 引导照片限定非锁屏启动和当前App显示模式。 | 锁屏、绑定保存、跨App切换、失败与返回原书需单独测；照片不是切换效果。 | G-002、R-011 |\n')
table('学习与反馈','| 同品牌帮助能补另一款单词机吗？ | [Mini/Max案例](cases/M-022-baicizhan-device.md) dcj2正文为超级单词机，未证型号对应。 | 先匹配型号/版本再引用学习、奖励、离线和同步；不凭详细程度越过适用范围。 | G-001、H-004/005 |\n')
append(L/'backlog.md','''
## 2026-10-27：偏好、监护与保存对象

- 抖音：两篇完整长图与局部功能图已看，补确认、屏蔽期限类型、孩子/家长入口、适龄/频道、拉黑和求真卡。夜间密码例外、未满14岁退出条件与历史图示有冲突；实际回执、撤回、断网、保存和重启仍缺，主观看链D1。
- 海信：五图补Hi Reader非锁屏启动、当前App显示模式、截图便笺和管理入口；A9/TOUCH/Hi Reader及联名款不混。各型号完整帮助、来电/后台音频、同步与恢复待查。
- Mini/Max：公开dcj2帮助5类23问是超级单词机，未找到目标型号对应；排除后原3F仍D1。下次需型号化说明/完整演示，不能再以同一帮助增加虚假覆盖。

本批没有新增实际产品操作；使用指南完整阅读不代表App全功能覆盖。研究与需求分开，目标继续进行。
''')
replace(L/'analysis-status.md','\n\n本轮之前的工作属于“有进展”','\n| 控件范围与静态证据深化（10月27日） | 抖音完整图像指南及冲突；Hi Reader机内引导/设置与分型号范围；Mini/Max帮助排除 | 更新旧任务和来源，7新D2/11E，无新增产品操作；主链和异常缺口保留，全部部分覆盖 |\n\n本轮之前的工作属于“有进展”')
append(R/'docs/ideas.md','\n2026-10-27继续I-021/I-022：逐图细化抖音推荐控制、亲子模式及时间限制，保留图文冲突；Hi Reader明确当前App显示范围并区分截图/文章收藏/速记；百词斩超级单词机帮助未证适用Mini/Max，单列排除。见[第二十四期简报](/home/workspace/reader/docs/briefs/024-controls-scope-and-evidence.md)。设计问题能反查到F/E及图像，但所有案例仍部分覆盖；未把机制分析升级为需求，未进入全系统设计。\n')
(W/'summaries-updated.json').write_text(json.dumps(dict(inventory_counts=counts,requirements_changed=False),ensure_ascii=False,indent=2)+'\n');print(json.dumps(counts))
