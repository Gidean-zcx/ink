from pathlib import Path
import json,csv
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/xiaoyuan-app-controls-deepening'
assert not (W/'summaries-updated.json').exists()
def replace(p,a,b):
 s=p.read_text();assert s.count(a)==1,(p,a[:35],s.count(a));p.write_text(s.replace(a,b))
def append(p,s):
 with p.open('a') as f:f.write(s)
replace(R/'README.md','- [第二十四期：控件范围与交互证据](/home/workspace/reader/docs/briefs/024-controls-scope-and-evidence.md)：最新简报；','- [第二十五期：答完、答对与保存](/home/workspace/reader/docs/briefs/025-practice-feedback-and-recovery.md)：最新简报；逐题型纠错/结束、手写编辑/恢复、资料副本与备份的范围。\n- [第二十四期：控件范围与交互证据](/home/workspace/reader/docs/briefs/024-controls-scope-and-evidence.md)：')
replace(L/'README.md','最新入口：[第二十四期简报]','前批入口：[第二十四期简报]')
append(L/'README.md','\n最新入口：[第二十五期简报](../../briefs/025-practice-feedback-and-recovery.md)。小猿手机软件13类历史练习逐项补控件/错误/结束；AiPaper旧手册逐页目录与编辑/恢复；小猿硬件资料/账号范围核查。静态图、正文报告与实际操作分开，全部案例仍部分覆盖。\n')
rows='| [M028-小猿软件来源与练习控件](../batches/xiaoyuan-app-controls-deepening-inventory.csv) | 112行：9官网域、10FAQ、13历史小节及控件、政策/版本边界 | 35新图+3旧图复看；13小节不是全App目录，历史图不能定现版；旧13项目录同步更新 |\n| [M007-AiPaper历史手册与版本控件](../batches/viwoods-controls-deepening-inventory.csv) | 578行：20父组、108页/版本组、450控件 | 68静态功能页已看，父子项包含；旧手册不是V3.15实测，文图/版本冲突保留 |\n| [M056-小猿硬件来源范围](../batches/xiaoyuan-hardware-controls-deepening-inventory.csv) | 33行：产品模块、旧记录、政策路径及版本范围 | S6前端代际未知；未取得完整硬件菜单，原5D1和注销局部D2不变，无新看图 |\n'
replace(L/'inventories/README.md','| --- | --- | --- |\n','| --- | --- | --- |\n'+rows)
def table(section,rows):
 p=L/'comparison.md';s=p.read_text();i=s.index('## '+section+'\n');j=s.index('\n| ---',i);end=s.index('\n',j+1);p.write_text(s[:end+1]+rows+s[end+1:])
table('学习与反馈','| 何时重试、下一题或算完成？ | [小猿软件F02/05/06/07–10](cases/M-028-xiaoyuan-ai.md) 口算重写、听写提交后查写法、跟读次数分支、背词重练；完成与答对分开。 | 不用统一下一题规则覆盖各题型；首次答案、识别修订、提示/重试与最终结果分别验证，奖励不证明掌握。 | G-001、G-002、R-014/019 |\n')
table('导入笔记与内容关联','| 删除快捷模板会删除笔记吗？ | [AiPaper F19](cases/M-007-viwoods.md) 历史手册区分快捷入口和关联文件夹，删除方向不同。 | AI需要明确动作对象和影响范围；不能把删除入口、目录和内容理解成同一操作。 | G-002、R-018/019 |\n')
table('AI上下文与执行结果','| 阅读页有返回就能恢复AI前选区吗？ | [AiPaper F04/09/18](cases/M-007-viwoods.md) 阅读返回、选区与笔迹撤销分别可定位；AI返回锚点未证。 | 退出、撤销、恢复来源位置分别查；不能由返回图标推断上下文完整保留。 | G-002、R-019 |\n')
# Locate actual heading rather than invent a section.
s=(L/'comparison.md').read_text();heading=next(x[3:] for x in s.splitlines() if x.startswith('## ') and ('恢复' in x or '备份' in x))
table(heading,'| 资料副本与后台备份能否恢复学习记录？ | [小猿软件F11/12](cases/M-028-xiaoyuan-ai.md) 副本条目为头像框截图；[硬件F06](cases/M-056-xiaoyuan-hardware.md) 有副本请求；后台备份删除延迟未提供用户恢复。 | 分开资料截图、学习导出、同步、恢复快照和关联产品账号范围；不将后台留存写成恢复能力。 | R-013、G-002 |\n')
append(L/'backlog.md','''
## 2026-10-27：练习反馈、编辑与恢复范围

- 小猿手机软件：13历史小节及35新图已审，补题型差别、完成/答对、提示统计及账号资料路径。下一类证据需现版同题错误、提示、重试、退出和重启；语文练习/练字/英文听写仍缺核心反馈，PK不能借独练联赛补。
- AiPaper：68历史功能页目录已补，Paper编辑、快捷目录关联、Memo回收分别建任务。旧手册和V3.15密码分期，AI命令5/4条和清缓存/Clear Data冲突保留；AI返回选区、多用户具体权限、跨笔记移页完成与恢复仍缺。
- 小猿硬件：同型号作者材料补查未取得可核正文，S6/E1核心学习链仍5D1；不以手机软件或同名新代替代。备份删除延迟、资料副本请求与可恢复性分开。

本批文档和静态图补证不增加实际产品操作；所有案例部分覆盖，目标继续进行。
''')
replace(L/'analysis-status.md','\n\n本轮之前的工作属于“有进展”','\n| 练习与编辑控件深化（10月27日） | 小猿软件13类历史练习与35新图；AiPaper68功能页与版本差异；小猿硬件账号/副本范围 | 9新D2/27E，修订22旧任务；没有新增实际操作，全部部分覆盖 |\n\n本轮之前的工作属于“有进展”')
append(R/'docs/ideas.md','\n2026-10-27继续I-021/I-022：小猿软件按13历史题型补控件、重试和结束，分开首次/重练与完成/答对；AiPaper手册逐页目录补编辑、关联文件夹和Memo回收，并保留文图/版本冲突；小猿硬件仍缺同型号核心链。资料副本和后台备份留存均不自动当用户恢复能力。见[第二十五期简报](/home/workspace/reader/docs/briefs/025-practice-feedback-and-recovery.md)。这些为研究问题和候选建议，未升级为新需求，未进入全系统设计。\n')
p=W/'applied-reviewed-updates.json';a=json.loads(p.read_text());a['old_evidence_captures']=sum(x['field']=='additional_captures' for x in a['evidence_changes']);a['old_evidence_metadata_fields']=sum(x['field']!='additional_captures' for x in a['evidence_changes']);p.write_text(json.dumps(a,ensure_ascii=False,indent=2)+'\n')
counts={n:len(list(csv.DictReader((L/f'batches/{n}-inventory.csv').open()))) for n in ['xiaoyuan-app-controls-deepening','viwoods-controls-deepening','xiaoyuan-hardware-controls-deepening']}
(W/'summaries-updated.json').write_text(json.dumps(dict(inventory_counts=counts,requirements_changed=False),ensure_ascii=False,indent=2)+'\n');print(json.dumps(counts))
