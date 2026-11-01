from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch50-integration'
assert not (W/'DOCUMENTS_APPLIED').exists()
bullet='- [第五十期：交互细节与可复查依据](/home/workspace/reader/docs/briefs/050-interaction-depth-and-reuse.md)：补AI笔记/待办目标与撤销、Gen.2笔迹和校准；依据表新增可筛选的证据字段及实际操作审计入口。'
for p in [R/'README.md',L/'README.md']:
    t=p.read_text();lines=t.splitlines();hit=[i for i,s in enumerate(lines) if s.startswith('- [第四十九期：')];assert len(hit)==1
    lines[hit[0]]=bullet;p.write_text('\n'.join(lines)+'\n')
p=R/'docs/briefs/README.md';t=p.read_text();pos=t.index('- [第四十九期：');p.write_text(t[:pos]+bullet+'\n'+t[pos:])
def add(p,t):p.write_text(p.read_text().rstrip()+'\n\n'+t.strip()+'\n')
add(L/'inventories/README.md','第50批[Gemini原目录](../batches/gemini-controls-deepening-inventory.csv)在原85连接器之外增加4个已读控制分支，共133行；校正以前已读但仍标未读的3项，并明确本次笔记/待办子帮助的局部覆盖。[QUADERNO Gen.2目录](../batches/quaderno-gen2-ink-inventory.csv)另列197节点，区分提取、选定页阅读与整章/实操；不混入Gen.3C目录。')
add(L/'backlog.md','''## 第50批后的AI笔记与代际缺口

Gemini需继续核同名对象选择、写入前批准与操作后Undo、重复请求、部分成功、断网/后台及真实数据回读；本次日历页429后停止，Workspace主体未读。Tasks代理工作流、Skills、Schedules仅有检索线索，不当Google Tasks待办的已读扩展。现有Phone/Messages/Device assistance旧读状态已纠正，不需重新算一批新正文。

QUADERNO Gen.2本轮正文只读10页；下次具体范围为p85–92传输/备份/同步，p35–39删除与回收站，p110–113故障。Gen.3C原76页正文记录不变，剩58页包括38–45、88–94、103–105等；须分别查连接前置、传出后批注对象和取消/失败回执。两代机型不能互相代替实测。''')
add(L/'analysis-status.md','第50批补Gemini两篇操作帮助和QUADERNO Gen.2十页选读：3新D2、4旧任务补证、3新E、8新RP；原连接器目录校正13项并补4控制分支，另列Gen.2的197个提取目录节点。RP从15列扩至22列，随行回填任务深度、来源方式及操作审计，所有122行重新关联。没有新增实操；全部65组仍部分覆盖。见[简报50](../../briefs/050-interaction-depth-and-reuse.md)。')
add(R/'docs/ideas.md','2026-10-27继续I-021/I-022：补AI笔记/待办的目标应用、能力边界和撤销范围，以及QUADERNO Gen.2的校准、剪贴和撤销状态。资料库依据表加入可筛选的F/E方式与审计链接，纠正旧目录未读标签，见[第五十期简报](/home/workspace/reader/docs/briefs/050-interaction-depth-and-reuse.md)。这些是供后续设计复查的依据；65组仍部分覆盖，参考机制没有升级为需求，未开始全系统设计。')
(W/'DOCUMENTS_APPLIED').write_text('Applied once.\n')
print('Updated brief entry points, inventories, backlog, analysis status and ideas; requirements unchanged.')
