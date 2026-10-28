from pathlib import Path
import csv,json
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/gemini-controls-deepening'
assert not (W/'summaries-updated.json').exists(),'one-time summaries'
names=['gemini','palma','ireader'];counts={}
for name in names:
 p=L/f'batches/{name}-controls-deepening-inventory.csv';counts[name]=len(list(csv.DictReader(p.open())))
def replace(p,a,b):
 s=p.read_text();assert s.count(a)==1,(str(p),a[:40],s.count(a));p.write_text(s.replace(a,b))
def append(p,s):
 with p.open('a') as f:f.write(s)
replace(R/'README.md','- [第二十一期：读完、少推和清除](/home/workspace/reader/docs/briefs/021-reading-feedback-and-data-reset.md)：最新简报；',
 '- [第二十二期：AI能做什么，退出后留下什么](/home/workspace/reader/docs/briefs/022-ai-actions-and-resume-conditions.md)：最新简报；AI通信/语音/接入的作用范围，阅读设备睡眠与恢复的条件。\n- [第二十一期：读完、少推和清除](/home/workspace/reader/docs/briefs/021-reading-feedback-and-data-reset.md)：')
replace(L/'README.md','最新入口：[第二十一期简报]','前批入口：[第二十一期简报]')
append(L/'README.md','\n最新入口：[第二十二期简报](../../briefs/022-ai-actions-and-resume-conditions.md)。Gemini补通信、Live与自定义接入的文档链；Palma 2补通用手册及固定型号材料的范围；掌阅区分手机与墨水屏版。目录、静态资料和运行结果分别标注，全部仍部分覆盖。\n')
rows=(f'| [M042-Gemini连接器及控制分支](../batches/gemini-controls-deepening-inventory.csv) | 85连接器目录项与44选定帮助控制分支，共{counts["gemini"]}行；保留条件与子帮助链接 | 85项仅目录，不是完整动作；未登录操作，不能据此声称全App覆盖 |\n'
 f'| [M010-Palma 2来源与通用手册](../batches/palma-controls-deepening-inventory.csv) | {counts["palma"]}条来源/章节/控件记录，型号材料与Mobile ePaper V4.0分开 | 通用手册和静态示例不能证明Palma 2当前固件及听书断点 |\n'
 f'| [M014-掌阅来源与版本](../batches/ireader-controls-deepening-inventory.csv) | {counts["ireader"]}条来源/功能记录，手机商店、历史政策与墨水屏FAQ分开 | 手机选区、AI/听书及导入恢复核心链仍D1；未用FAQ替代手机流程 |\n')
replace(L/'inventories/README.md','| --- | --- | --- |\n','| --- | --- | --- |\n'+rows)
def table(section,rows):
 p=L/'comparison.md';s=p.read_text();i=s.index('## '+section+'\n');j=s.index('\n| ---',i);end=s.index('\n',j+1);p.write_text(s[:end+1]+rows+s[end+1:])
table('AI上下文与执行结果',
 '| 能发送消息就能读取消息库吗？ | [Gemini F-M042-07/08](cases/M-042-gemini.md) 分通信路由、发送、通知读取与消息库；连接器和系统权限分别控制。 | 逐对象核读写能力及真实回执；停用一个连接不推导全部数据访问停止。 | G-002、R-007/021 |\n'
 '| Live中断后能撤销已执行动作吗？ | [Gemini F-M042-09/10](cases/M-042-gemini.md) 区分会话/共享，Undo受前后台与目标应用限制。 | 会话、媒体和目标数据分别保存与恢复；取消、撤销和回App编辑不能混称。 | R-012/018、G-002 |\n'
 '| 应用入口消失是否等于撤销接入？ | [Gemini F-M042-11](cases/M-042-gemini.md) 自定义服务有停用、解除授权、移除三个路径；解除可保留目录。 | 安装/显示/授权/数据副本分开研究；MCP服务接入不等于安装Android应用。 | R-008/009、G-002 |\n')
table('读听与中断恢复',
 '| 睡眠后是否继续播放？ | [Palma 2 F-M010-07](cases/M-010-palma.md) 引用通用V4.0手册，Wi-Fi、蓝牙、音频状态分开，仍受具体型号/固件限制。 | 区分设备睡眠、连接、播放器和内容断点；静态Pause是示例，不当默认值。 | R-018、G-002 |\n')
table('导入笔记与内容关联',
 '| 笔记没有显示，能否判断已丢失？ | [掌阅 F-M014-04/05](cases/M-014-ireader.md) 墨水屏版FAQ涉及书架刷新及PDF笔记模式；手机端路径另缺。 | 先区分页面展示、模式、同步与持久数据；刷新成功不证明恢复完成。 | H-004/005、G-002 |\n')
append(L/'backlog.md','''
## 2026-10-27：AI动作、睡眠与接续条件

- Gemini：新增通信、Live会话/动作、自定义接入文档链；85连接器目录另列。实际拨出/送达、确认取消、复合动作部分成功、重试、断网/强退/重启和共享上下文更新仍缺。屏幕自动操作旧设备/地区冲突保留。
- Palma 2：通用Mobile ePaper V4.0原件和静态控件不成为型号固件验证；固定按键设置、锁屏音频、TTS续句、跨版恢复及同步冲突仍待核查。
- 掌阅：手机选区/AI听书/导入恢复缺完整交互。MatePad Paper FAQ、商店版本声明和历史政策保留各自条件；未取得手机流程不等于不支持。

本批没有产品实际操作，详细数量及合并结果见核查记录。参考机制未升级为本项目需求。
''')
replace(L/'analysis-status.md','\n\n本轮之前的工作属于“有进展”',
 '\n| AI动作、睡眠与接续深化（10月27日） | Gemini补5条文档任务、连接器条件目录；Palma 2补通用手册与型号材料；掌阅补既有来源与目录 | 通信读写、会话/媒体/目标数据及接入授权分别限定；手机薄弱任务仍D1，无新增实操，全部部分覆盖 |\n\n本轮之前的工作属于“有进展”')
append(R/'docs/ideas.md','\n2026-10-27继续I-021/I-022：Gemini补通信/通知/完整消息库的边界，Live会话与共享状态、动作撤销和自定义接入生命周期；Palma 2核睡眠后连接/音频的分离条件；掌阅保留手机和墨水屏版差异。详见[第二十二期简报](/home/workspace/reader/docs/briefs/022-ai-actions-and-resume-conditions.md)。记录的是研究依据与候选问题，没有更改既定需求或进入全系统设计。\n')
(W/'summaries-updated.json').write_text(json.dumps(dict(inventory_counts=counts,requirements_changed=False),ensure_ascii=False,indent=2)+'\n')
print(json.dumps(counts))
