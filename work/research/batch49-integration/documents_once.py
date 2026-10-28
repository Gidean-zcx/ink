from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
p=L/'cases/M-040-antennapod.md';s=p.read_text();assert 'E-APST-01' not in s
old='在原12F基础上，队列深化补5个原任务分支，新增F13“按已有标签浏览订阅并恢复选择”。本次只补读3篇官方帮助和7个固定提交的源码文件，其中SubscriptionFragment复用本地原件；未读下游方法不作为已完成任务。'
new='前批队列深化建立F13已有标签浏览；第49批沿缺口补14个固定提交文件，并复读七组旧来源，接通筛选、标签、删除和队列的数据路径。F02/03/05/13沿原编号深化，新增F14局部筛选任务；共14个D2，仍无产品运行证据。'
assert old in s;s=s.replace(old,new)
old='继承上述131项，明确本次仅补读其中3篇帮助，另列27个源码分支，共158行；父项、条件及未闭合链分开，不等于158个完整流程。'
new='继承上述131项，前批另列27个源码分支；第49批更新12项并增加4个入口/状态分支，共162行。父项、条件及未闭合链分开，不等于162个完整流程。'
assert old in s;s=s.replace(old,new)
s=s.replace('赋标签仅入口，命名/保存/删除与每播客设置仍有缺口。','第49批已补单/多订阅标签编辑、确认/取消及共同集合；全局重命名/删除与同步仍缺。')
s=s.replace('媒体删/退订/归档分开；撤销UI、物理失败与清理候选预览未闭合。','已补本地源警告及物理失败/提示数边界；撤销、恢复与清理预览仍缺。')
s=s.replace('原显示设置仍D1；补读过滤回调和SQL状态含义，未读各屏幕调用方，不另立完整过滤任务；订阅筛选是另一未读对话框。','F14补全部单集/单播客入口、即时写入、空结果和回读范围；原显示设置及订阅筛选仍未补齐，不能共用确认规则。')
rows={
'F-M040-02':'| F-M040-02 | 把想听内容放进队列 | 已有媒体；批量需已有选择 | 单集长按出现上下文菜单，选Multi-select才启动多选；选择中点击条目/副按钮切换。选中项按列表顺序取出；原Add to queue及下载入队开关保留。 | 批量跳过无媒体/已入队项，发动作后结束选择；提示数来自请求资格计数。 | E-ANTP-02；E-ANTP-06；E-APCD-03；E-APQD-04；E-APQD-05；E-APST-01 | 多选中旋转/数据变化、并发重复入队、DB失败及实际反馈未知。 |',
'F-M040-03':'| F-M040-03 | 混排多个节目并保留先后关系 | 已有队列；固定3.12.3 | 排序选择即请求保存，Random关闭Keep Sorted；锁定/Keep Sorted限制移动。清空先确认，取消只关闭；确认后删队列表并发CLEARED事件更新列表。 | 移顶先反转选中列表再逐项插首，保留传入顺序；移底顺序追加。setQueue以事务重建，但SQL异常可仅日志，随后事件不证明落盘成功。清空分支不调用媒体删除/STOP。 | E-ANTP-03；E-ANTP-04；E-APQD-01；E-APQD-04；E-APQD-05；E-APCD-14；E-APST-07；E-APST-08 | 当前播放服务对清空事件的所有反应、撤回、并发/故障注入及实机拖动未测；不承诺清空后一定续播。 |',
'F-M040-05':'| F-M040-05 | 清下载并区分退订与本地原文件删除 | 所选含下载、本地源或下载中媒体 | 任一所选本地源触发警告，确认才执行整组回调，取消不调用；无本地源直接执行。警告说明会删除设备原文件且不能由本应用重新下载。 | 已下载/本地媒体请求删除，下载中请求取消；计数在提交时产生。物理删除失败可仅日志并继续状态路径；删除当前媒体STOP、本地刷新与可选出队分开。 | E-ANTP-06；E-ANTP-07；E-APCD-03；E-APCD-14；E-APQD-05；E-APST-05；E-APST-07 | 实际取消/失败UI、半文件与原文件残留、批量回滚、撤销及恢复未测；数量提示不等于删净。 |',
'F-M040-13':'| F-M040-13 | 整理订阅标签、按标签浏览并恢复选择 | 单订阅或批量已有选择；固定3.12.3 | 标签菜单打开临时列表；加/删先改显示集合，OK再加入输入框剩余文本并提交，Cancel无提交回调。批量只显示共同标签，保存替换共同集合而保留各自独有标签。 | 每节目分别写入并通知重载；空名/重复名不新增，也不阻止OK提交已有列表。原标签浏览/onPause保存last_tag、归档从root开始及过滤后回退规则保留。 | E-APQD-01；E-APQD-02；E-APCD-13；E-APCD-14；E-APST-04；E-APST-06；E-APST-07；E-APST-14 | 未核全局重命名/删除、跨端同步、并行编辑/部分失败、异常终止与恢复；root仍受现有过滤限制。 |'
}
lines=s.splitlines()
for i,line in enumerate(lines):
 for fid,row in rows.items():
  if line.startswith('| '+fid+' |'):lines[i]=row
i=next(i for i,x in enumerate(lines) if x.startswith('| F-M040-13 |'))
lines.insert(i+1,'| F-M040-14 | 筛选全局或单播客单集并恢复可见范围 | 有可进入的列表；区分全局与feed | 全局菜单/筛选提示→选项变化事件→写偏好、页数回1、重载；单播客按钮/既有筛选提示→选项变化→写该feed过滤字段→事件重载。Confirm只关闭，Reset空集合。 | 全局偏好在SharedPreferences，单源在DB；全局空条件仍默认已订阅，收藏扩到所有源状态，单源查询仍只看该feed。空筛选结果提示可清条件。 | E-APST-02；E-APST-03；E-APST-07；E-APST-09；E-APST-10；E-APST-11；E-APST-12；E-APST-13；E-APQD-06；E-APQD-07；E-APCD-03；E-APCD-14 | 关闭不是撤回保证；重启落盘、并行变化、查询失败实际UI、备份/重装恢复未测；订阅筛选为另一控件。 |')
s='\n'.join(lines)+'\n'
old='ItemFilterDialog在选项变化时即调用onFilterChanged，Confirm仅关闭，Reset传空集再清选中。它是抽象控件，本批未读各屏幕调用方及保存逻辑，故不声称关闭撤回或每处菜单一致。FeedItemFilterQuery用AND组合维度：Unplayed包括New，Paused按position>0判断，Downloaded按下载时间字段判断；这些字段不证明当前暂停或底层音频仍存在。SubscriptionFragment调用的SubscriptionsFilterDialog是另一控件，不能套用上述确认规则。[E-APQD-06、07；E-APCD-13]'
new='第49批已把ItemFilterDialog接到全局和单播客两个调用方：选择时即请求写入，Confirm仅关闭，Reset先传空集。未找到该链在关闭时补偿原值的回调；这仍是源码结论。全局筛选、单源过滤和标签编辑的保存对象/提交时点不同。FeedItemFilterQuery以AND组合条件，Unplayed包括New、Paused依position>0、Downloaded依下载日期；这些字段不证明实时暂停或文件存在。SubscriptionsFilterDialog是另一未读控件，不套此规则。[E-APST-02、03、07、09～13；E-APQD-06、07]'
assert old in s;s=s.replace(old,new)
old='标签帮助只给赋标签入口，本批F13限定为已有标签浏览，不补造命名/保存/删除或跨设备同步链。'
new='第49批补标签编辑的OK/Cancel与共同集合算法；每节目分别请求写入，不能推成全批原子提交，也未补齐全局标签管理或跨设备同步。'
assert old in s;s=s.replace(old,new)
s=s.replace('清空后的撤回、拖动取消、写库失败回滚仍未知。','本批已追清空/移动的数据库方法，实际撤回、拖动取消及故障回滚仍未知。')
s=s.replace('记录UI、状态和媒体文件分别变化；本轮没有执行这些任务。','再分别观察筛选关闭/重启、批量标签一项失败、含本地源删除取消、队列清空时当前播放。记录UI、数据库与媒体文件分别变化；本轮没有执行这些任务。')
s+='\n## 第49批定点源码依据\n\nE-APST-01～14分别覆盖单集选择、全局/单源筛选、标签编辑、本地删除警告、偏好对象、DB读写和确认分派；[短摘记](../artifacts/antennapod-state-controls/source-notes.md)及[逐文件来源/定位/散列](../artifacts/antennapod-state-controls/source-audit.json)可直接回查。七组旧E仅追加复读范围，不把原件重复算独立来源。新增RP-M040-01～08按取消、保存、批量、空结果和文件范围检索。代码已读不等于产品已运行，14任务仍局部覆盖。\n'
p.write_text(s)

brief='049-selection-confirmation-and-persistence.md'
(R/'docs/briefs'/brief).write_text('''# 第四十九期：选择、确认与真正保存

2026-10-27。本轮沿AntennaPod原有资料的下游缺口，追踪固定3.12.3源码中“入口→选择→回调→写入→列表反馈”的连接。补14个源码文件、复读七组旧来源，深化4个原任务，增加一个筛选任务；没有编译或运行产品，全部仍为文档/源码级依据。

| 看起来相似的操作 | 实际代码规则 | 对产品设计的意义 |
| --- | --- | --- |
| 选完筛选再点确认 | 选项改变就请求保存；确认仅关闭面板 | “关闭”不能默认解释为放弃修改 |
| 标签编辑点确定或取消 | 先改临时列表，确定才提交；批量只替换共同标签，保留各自独有标签 | AI要表达修改范围，不能把批量设置当全量覆盖 |
| 清空队列与删除音频 | 前者删队列表；后者可能删下载，含本地源还需警告原文件影响 | 内容计划、记录和文件需要分别命名 |
| 显示已处理若干条 | 数量来自发请求时的计数，不等待各项完成 | 反馈数量不应自动成为AI的成功结论 |
| 清掉筛选，显示全部 | 全局空条件仍按已订阅源查询；收藏又扩大源状态范围 | “全部”的范围应可解释，零结果不代表没有内容 |
| 同样叫筛选设置 | 全局写偏好，单播客写数据库字段 | 备份、恢复与跨端同步需要核具体对象 |

依据是[固定版本的源码摘记与原文件定位](../research/interaction-library/artifacts/antennapod-state-controls/source-notes.md)，不是实际安装结果。队列写入虽有事务代码，异常/通知链仍不足以保证操作成功；物理文件删除、Android落盘、中断恢复和并行修改还需实际观察。

这些细节与前轮墨墨单词机的阶段按键、确认弹窗形成互补：同样的按钮名称不能决定提交时点或副作用。成人整理新闻/听书队列、家长批量准备儿童内容，以及AI执行“取消”“整理一下”“清掉这些”，都需要明确对象和结果。以上为G-001/G-002下的研究建议，尚未成为系统设计方案。

从[设计参考指南](../research/interaction-library/design-reference-guide.md)可按问题直达新增8条依据，再查[AntennaPod案例](../research/interaction-library/cases/M-040-antennapod.md)及162项来源/分支目录。新增目录项不当作独立功能；全部65组仍部分覆盖，未宣告已完成每款产品所有交互。
''')
latest='- [第四十九期：选择、确认与真正保存](/home/workspace/reader/docs/briefs/'+brief+')：沿固定源码区分即时筛选、临时标签编辑、批量结果、队列与文件删除；新增8条按问题检索的依据。'
for p in [R/'README.md',L/'README.md']:
 lines=p.read_text().splitlines();i=next(i for i,x in enumerate(lines) if x.startswith('- [第四十八期：'));lines[i]=latest;p.write_text('\n'.join(lines)+'\n')
p=R/'docs/briefs/README.md';s=p.read_text();assert brief not in s;i=s.index('- [第四十八期：');p.write_text(s[:i]+latest+'\n'+s[i:])
p=L/'design-reference-guide.md';s=p.read_text();marker='| --- | --- | --- | --- |\n';rows='''| 点确认之前是否已经修改数据 | [AntennaPod F14/13](cases/M-040-antennapod.md) | [筛选即时写入、标签临时编辑](artifacts/antennapod-state-controls/source-notes.md)；RP-M040-01/02/03 | 固定源码，未运行；相似按钮的提交时点/对象不同，关闭不自动等于撤回 |
| 批量整理是否覆盖每项的独有信息 | [AntennaPod F13](cases/M-040-antennapod.md) | [共同标签算法与取消](artifacts/antennapod-state-controls/source-notes.md)；RP-M040-03 | 每节目分别提交，未证明整批事务、部分失败恢复或同步 |
| 清队列、删音频和处理数量分别说明什么 | [AntennaPod F03/05](cases/M-040-antennapod.md) | [警告、数据库与文件边界](artifacts/antennapod-state-controls/source-notes.md)；RP-M040-04～07 | 本地原件与下载不同；请求数/UI事件不能证明持久保存或删净 |
| “全部内容”和零结果的范围怎样解释 | [AntennaPod F14](cases/M-040-antennapod.md) | [空条件与收藏范围](artifacts/antennapod-state-controls/source-notes.md)；RP-M040-08 | 筛选作用于来源状态及字段，未实际查询，不代表物理文件可用 |
''';s=s.replace(marker,marker+rows,1);p.write_text(s)
p=L/'comparison.md';s=p.read_text();s=s.replace(marker,marker+'''| 同样是确认，何时才写入？ | [AntennaPod F13/14](cases/M-040-antennapod.md)筛选改变即写，标签编辑OK才提交；[MM-WM01 F03](cases/M-023-maimemo-device.md)只有确认弹窗静态图。 | 竞品分别保留证据深度；临时编辑、已请求、已保存和已同步需要不同状态。 | G-002、R-007/021 |
| 批量准备内容怎样保留每项差异？ | [AntennaPod F13](cases/M-040-antennapod.md)标签编辑只替换共同集合，独有标签保留。 | 研究共同/独有值、逐项失败与回读；代码不证明真实原子提交或家庭同步。 | R-016、G-002、H-004 |
''',1);p.write_text(s)
p=L/'backlog.md';s=p.read_text();s+='\n第49批AntennaPod已补筛选两个调用方、标签共同集合与确认/取消、媒体删除警告、队列写入及选中入口。下一项应固定APK/Android验证即时修改后关闭/重启、批量标签部分失败、含本地原件的取消/删除失败及清队列时当前播放；订阅筛选与标签全局管理仍缺，不套用单集筛选规则。\n';p.write_text(s)
p=L/'inventories/README.md';s=p.read_text();s=s.replace('158行','162行（第49批更新12项、增加4个调用方/状态分支）');p.write_text(s)
for rel,t in {
'analysis-status.md':'第49批AntennaPod补14个固定源码文件、七组旧来源复读，4旧F补证、1新D2及8RP。原158节点更新12项并增4项为162；无安装/运行/视觉观察，65组仍部分覆盖。见[简报49](../../briefs/049-selection-confirmation-and-persistence.md)。',
'batches/README.md':'第49批 `antennapod-state-controls-*` 新增1D2/14固定源码E，4旧F与7旧E原位追加；原队列目录158→162，更新12项。8RP，没有产品执行或图像。见[简报49](../../../briefs/049-selection-confirmation-and-persistence.md)。'
}.items():
 p=L/rel;p.write_text(p.read_text().rstrip()+'\n\n'+t+'\n')
p=R/'docs/ideas.md';p.write_text(p.read_text().rstrip()+'\n\n2026-10-27继续I-021/I-022：AntennaPod固定源码补筛选即时写入/标签确认提交、共同标签与独有标签、队列表/媒体文件及请求数量/实际结果的区别。新增8条按问题检索依据，见[第四十九期简报](/home/workspace/reader/docs/briefs/049-selection-confirmation-and-persistence.md)。这些为成人内容整理、儿童内容准备和全局AI操作的候选参考；仍无设备运行，全部65组部分覆盖，未改变已确认需求或开始全系统设计。\n')
print('Updated case, brief and reusable question entrances; original requirements unchanged.')
