from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
brief='047-ai-data-flow-and-results.md'
latest='- [第四十七期：AI动作的数据传递与结果](/home/workspace/reader/docs/briefs/'+brief+')：补空输入、列表/分支、批量处理、网页上下文及成功/取消/错误返回；新增10条可检索依据。'
for p in [R/'README.md',L/'README.md']:
 lines=p.read_text().splitlines();i=next(i for i,x in enumerate(lines) if x.startswith('- [第四十六期：'));lines[i]=latest;p.write_text('\n'.join(lines)+'\n')
p=R/'docs/briefs/README.md';s=p.read_text();assert brief not in s;i=s.index('- [第四十六期：');p.write_text(s[:i]+latest+'\n'+s[i:])
add={
'analysis-status.md':'\n第47批继续快捷指令原95主题目录，28主题补27正文；6新D2、6旧F补分支、28新E及10RP。原114节点保持，49主题已读、46主题仅目录、19分组。没有新增图像观察或实际运行，全部65组仍部分覆盖；见[简报47](../../briefs/047-ai-data-flow-and-results.md)。\n',
'backlog.md':'''\n## 第47批后的系统AI动作缺口

Shortcuts列表、菜单/If、循环、网页脚本、URL回调与API现有27正文依据；原目录仍46主题仅目录。下一层应观察空输入/零选择、参数提问取消、空分支与缺字段、批量中断的部分产物、回调丢失/重复和目标改名、限流与重试，以及最终业务对象回读。普通变量与跨运行存储分开；存储动作仍只取得原发布说明，具体保存/覆盖/删除/恢复待补。没有iPhone/iPad实操环境的结果，不由文档构造实际状态。\n''',
'comparison.md':'''\n## AI任务的数据对象与返回（第47批）

[Shortcuts F13～18](cases/M-043-shortcuts.md)与[Readwise工作流](cases/M-015-readwise.md)、[Viwoods预览/发送](cases/M-007-viwoods.md)可按“输入对象→选择范围→执行步骤→返回结果→实际保存”比较。列表标签不等于传递值，分支或循环最后输出不必等于原目标对象，成功回调也需结合业务结果解释。这些是不同产品各自的文档/源码/画面依据，不能合成一个已实测的统一平台。

对主题学习导入，可研究单项预览后批量处理、失败时保留已完成项目及回读核对；对儿童故事续播，临时变量不能替代持久进度。这些是G-001/G-002下的候选研究问题，未确定Plus协议或恢复机制。\n''',
'design-reference-guide.md':'''\n## AI拿到什么、执行到哪里、怎样返回（第47批）

[Shortcuts案例](cases/M-043-shortcuts.md)新增RP-M043-01～10，直接查[正文摘记与逐篇获取审计](artifacts/shortcuts-data-flow/source-notes.md)：

| 设计问题 | RP及证据定位 | 不能省略的限制 |
| --- | --- | --- |
| 没找到输入时要问什么 | RP01/02，E-SCDF-01/02/15 | 没有入口、空输入、暂停补参数不同；取消和前序产物未测 |
| 显示给用户的名称是不是交给AI的对象 | RP03/04，E-SCDF-05/09～11/26 | 字典可显示键而传值，文本拼接可能改变类型 |
| 一次选项会执行哪些动作 | RP05，E-SCDF-12/13 | 菜单只走一支；条件结果来自所走路径最后有输出的动作 |
| 处理十条是否真的导入了十条 | RP06/09，E-SCDF-14/22～25/27/28 | 次数不等于不同内容数量；部分失败、重复与业务回读未知 |
| 看见网页是否能操作它 | RP07，E-SCDF-16～18 | 活动网页上下文、许可和完成回调各自成立；不覆盖全部原生App |
| 取消、错误、没有输出分别说明什么 | RP08/10，E-SCDF-04/06/07/19～21 | 回调/预览不是业务保存证明；取消无输出不等于没有副作用 |

这些依据属于官方文字重建；当前无快捷指令界面观察/实际执行，不能据此确定动画、时延或恢复。数据表中的evidence_basis与persistence_result必须一起读取。\n''',
'batches/README.md':'\n第47批 `shortcuts-data-flow-*` 新增6D2与28篇27官方正文E；6旧F在原持有CSV更新。28原目录行补正文，原114节点不变；10RP，无新增静态图或产品操作。旧26同主题E保留；详见[简报47](../../../briefs/047-ai-data-flow-and-results.md)。\n',
}
for rel,t in add.items():
 p=L/rel;p.write_text(p.read_text().rstrip()+'\n'+t)
p=L/'data-dictionary.md';s=p.read_text();s=s.replace('当前68条增量示例，不是全部任务或完整状态机','以reference-points.csv当前行为准，属于增量依据，不是全部任务或完整状态机');p.write_text(s)
p=L/'inventories/README.md';s=p.read_text();s=s.replace('19分组及95主题，共114节点；保留父级、原链接和正文覆盖','19分组及95主题，共114节点；第47批后49主题已读、46主题仅目录，保留原链接和版本');p.write_text(s)
p=R/'docs/ideas.md';p.write_text(p.read_text().rstrip()+'''\n\n2026-10-27继续I-021/I-022：快捷指令补28篇27正文，将选择对象/选择路径、循环结果/独立数量、活动网页/URL、成功/取消/错误回调及临时变量/持久存储分开。沿用6旧F，新增6任务与10个按问题检索的依据；见[第四十七期简报](/home/workspace/reader/docs/briefs/047-ai-data-flow-and-results.md)。仍无设备执行证据，所有65组部分覆盖；未新增已确认需求或进入全系统设计。\n''')
print('Updated current entrances, topic indices, ideas and brief index.')
