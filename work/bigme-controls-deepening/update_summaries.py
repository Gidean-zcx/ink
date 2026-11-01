from pathlib import Path
import csv,json
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/bigme-controls-deepening'
assert not (W/'summaries-updated.json').exists(),'one-time summaries'
counts={n:len(list(csv.DictReader((L/f'batches/{n}-controls-deepening-inventory.csv').open()))) for n in ['bigme','moaan','baicizhan']}
def replace(p,a,b):
 s=p.read_text();assert s.count(a)==1,(p,a[:35],s.count(a));p.write_text(s.replace(a,b))
def append(p,s):
 with p.open('a') as h:h.write(s)
replace(R/'README.md','- [第二十二期：AI能做什么，退出后留下什么](/home/workspace/reader/docs/briefs/022-ai-actions-and-resume-conditions.md)：最新简报；','- [第二十三期：学完、保存与恢复](/home/workspace/reader/docs/briefs/023-learning-completion-and-records.md)：最新简报；学习完成、记录上传、日历补卡与书库登记的区别，及阅读设置和宣传图的核查。\n- [第二十二期：AI能做什么，退出后留下什么](/home/workspace/reader/docs/briefs/022-ai-actions-and-resume-conditions.md)：')
replace(L/'README.md','最新入口：[第二十二期简报]','前批入口：[第二十二期简报]')
append(L/'README.md','\n最新入口：[第二十三期简报](../../briefs/023-learning-completion-and-records.md)。百词斩补50题公开帮助及版本冲突，墨案补Plus短说明书/历史文件路径，Bigme分系统显示与阅读排版；实际动作结果仍待核。目录和状态差异补回原案例，不按新增行数计算完成率。\n')
rows=(f'| [M011-Bigme手册与宣传来源](../batches/bigme-controls-deepening-inventory.csv) | {counts["bigme"]}条：13章、正文控件、静态帧及范围排除 | PDF仅文字读取，下载403；宣传TTS图非朗读链，80FPS公告排除黑白版 |\n'
 f'| [M054-墨案Plus与历史指南](../batches/moaan-controls-deepening-inventory.csv) | {counts["moaan"]}条：Plus短说明书、历史目录、作者照片及型号边界/缺口 | 2020指南未标型号；文件接收和书架登记分开，照片不当切换效果 |\n'
 f'| [M016-百词斩公开帮助与来源](../batches/baicizhan-controls-deepening-inventory.csv) | {counts["baicizhan"]}条来源目录，含50个去重帮助问题 | QA无日期/构建，与现政策冲突并列；完整学习反馈链仍D1 |\n')
replace(L/'inventories/README.md','| --- | --- | --- |\n','| --- | --- | --- |\n'+rows)
def table(section,rows):
 p=L/'comparison.md';s=p.read_text();i=s.index('## '+section+'\n');j=s.index('\n| ---',i);end=s.index('\n',j+1);p.write_text(s[:end+1]+rows+s[end+1:])
# Actual existing headings are checked before this script is run.
table('导入笔记与内容关联','| 文件已传入为何书架没有？ | [墨案 F-M054-06](cases/M-054-moaan.md) 2020指南区分蓝牙接收和加入多看书架；型号未知。 | 传输、解析、索引、打开及进度分别核验；不直接当Plus现版流程。 | R-016/017、G-002 |\n')
table('读听与中断恢复','| 带TTS字样的图能证明听书控件吗？ | [Bigme F-M011-06/07/12](cases/M-011-bigme.md) 官网GIF实际为音乐、词典、字体宣传帧；朗读链仍D1。 | 先核图像内容与动作对象，音乐播放不能补书籍断点；排版与系统刷新分开。 | R-018、G-002 |\n')
table('同步备份与成员数据','| 学完后是否已经保存到服务器？ | [百词斩 F-M016-01](cases/M-016-baicizhan.md) 未标版本官方QA区分离线资源下载、记录同步和打卡上传，提示清缓存可能丢未同步记录。 | 本地保存、上传中、服务端接收和账号归属；等待10秒建议不是成功回执。 | H-004/005、G-002 |\n')
table('学习与反馈','| 补日历是否等于补活动奖励？ | [百词斩案例](cases/M-016-baicizhan.md) 官方帮助限制补卡对小班/活动的作用。 | 掌握、连续天数、活动资格和奖励分别改变什么；不直接照搬激励。 | G-001、R-019 |\n')
append(L/'backlog.md','''
## 2026-10-27：学习完成、数据保存与控件范围

- 百词斩：取得50个唯一公开帮助问题，补误斩恢复、离线/同步、补卡和收藏出口等路径；QA未标构建，与现政策冲突保留。当前错答→提示→重试→结算、上传回执和恢复后调度仍缺，主链D1。
- 墨案：Plus短说明书与2022两张作者照片有明确范围；2020指南型号仍未知。USB/FTP路径冲突、移动文件位置歧义保留；书库登记后的返回位置、睡眠音频、数据恢复未闭合。
- Bigme：13章及正文控件逐项登记；补文本/PDF排版，系统刷新与书内设置分开。下载PDF403停止，宣传图不补TTS，其他型号80FPS不当本机固件。读听/批注主链及设置持久范围仍缺。

本批没有新产品实操。后续取证优先补以上关键分支，避免重复下载相同宣传页；全部案例仍部分覆盖。
''')
replace(L/'analysis-status.md','\n\n本轮之前的工作属于“有进展”','\n| 学习完成与记录保存深化（10月27日） | 百词斩公开帮助50题与冲突；墨案Plus说明书/静态控件及历史传入分支；Bigme手册目录与排版 | 细化原任务并保留核心链缺口；来源命名、宣传图及跨型号帧率不能替代实际操作，全部部分覆盖 |\n\n本轮之前的工作属于“有进展”')
append(R/'docs/ideas.md','\n2026-10-27继续I-021/I-022：公开帮助、完整短说明书及实际图帧补核，区分学习完成/上传/补卡、文件接收/书架登记、系统刷新/阅读排版。百词斩QA与政策、墨案历史指南型号、Bigme宣传图内容与高刷型号范围均保留冲突和缺口。见[第二十三期简报](/home/workspace/reader/docs/briefs/023-learning-completion-and-records.md)。没有把参考机制升级为需求，R-011高刷目标仍待验证，未进入全系统设计。\n')
(W/'summaries-updated.json').write_text(json.dumps(dict(inventory_counts=counts,requirements_changed=False),ensure_ascii=False,indent=2)+'\n');print(json.dumps(counts))
