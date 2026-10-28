from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/iflytek-planning-reading-deepening'
assert not (W/'summaries-applied.txt').exists(),'one-time'
p=R/'README.md';s=p.read_text();anchor='- [第二十七期：学习状态与证据复用]';i=s.index(anchor);s=s[:i]+'- [第二十八期：阅读计划与交互依据](/home/workspace/reader/docs/briefs/028-reading-plans-and-interaction-evidence.md)：最新简报；任务计划的修改范围、阅读统计、提示与报告，以及宣传视频仍不足以支持的结论。\n'+s[i:];s=s.replace('：最新简报；四条学习机教程','：四条学习机教程');p.write_text(s)
p=L/'README.md';s=p.read_text();needle='## 从哪里查';s=s.replace(needle,'最新一批见[第二十八期：阅读计划与交互依据](/home/workspace/reader/docs/briefs/028-reading-plans-and-interaction-evidence.md)：讯飞语文/阅读/计划、BOOX阅读与笔记、洪恩历史正文和视频边界；保持全部案例部分覆盖。\n\n'+needle,1);p.write_text(s)
p=L/'inventories/README.md';s=p.read_text();needle='| --- | --- | --- |\n';rows='| [M004-BOOX阅读与笔记控件](../batches/boox-reading-controls-deepening-inventory.csv) | 59原文章加68正文标题/控件组，127行；本轮补读10篇 | 4静态帮助图；当前通用帮助与V4.1.2型号基线分开，原59目录另保留历史 |\n| [M029-洪恩可得功能与历史分支](../batches/ihuman-learning-controls-deepening-inventory.csv) | 36来源支持的功能域/分支，按现行声明、历史静态与文字分开 | 不是当前全部菜单；另看官网宣传视频仍无完整错误/恢复链 |\n';s=s.replace(needle,needle+rows,1)
lines=s.splitlines()
for i,line in enumerate(lines):
 if 'M027-help-inventory.csv' in line:lines[i]='| [M027-讯飞帮助目录](M027-help-inventory.csv) | 34功能视频、24问题视频与12通用手册，70项 | 两批8条功能视频抽样；26功能与24问题视频仍未看，旧型号文字与T30目录分开 |'
p.write_text('\n'.join(lines)+'\n')
p=L/'comparison.md';s=p.read_text();needle='## 学习与反馈\n\n| 要解决的问题 | 已有依据与反例 | 本项目要讨论/验证什么 | 关联 |\n| --- | --- | --- | --- |\n';assert needle in s
rows='| AI调计划要区分哪些步骤？ | [讯飞F19/23](cases/M-027-iflytek-learning.md) 区分精读计划和周任务；周任务有排序、增删、确定，家长端按孩子选择。 | 候选/已开启/已修改/已保存分开；已完成记录、提醒与两端冲突未知，不用教程剪辑补同步事务。 | G-001/002、R-016/019 |\n| 提示、拼图和阅读数能否证明学会？ | [讯飞F19–24](cases/M-027-iflytek-learning.md) 诗句提示、发音报告、碎片及百分比分列；[BOOX F17](cases/M-004-boox-neoreader.md) 统计仅本机不同步。 | 记录提示依赖、独立表现、完成与持续记忆；统计的对象/归属/恢复单独确定。 | G-001/002、R-014/019、H-004 |\n';s=s.replace(needle,needle+rows,1);p.write_text(s)
p=L/'backlog.md';lines=p.read_text().splitlines()
for i,line in enumerate(lines):
 if line.startswith('| BOOX Note Air5 C |'):lines[i]='| BOOX Note Air5 C | 59篇通用帮助目录；本轮10正文/68控件组，补书库/排版/统计/标签/文本笔记，4静态图 | V4.1.2本机适用、扫描/删除与恢复、图层锁/隐藏选择、统计保存、后台/TTS及优化配置；通用图不代实机 |'
 if line.startswith('| 洪恩识字 |'):lines[i]='| 洪恩识字 | 当前商店功能声明、旧设置/题面、2020作者正常分支，官网宣传视频抽样；36功能域条目 | 当前同字错答/提示/重试/结算/退出全链；五环节与旧四标签分开，视频仍不能升级D1主任务；旧受限来源不重复绕访 |'
 if line.startswith('| 讯飞 T30 Pro／旧型号补充 |'):lines[i]='| 讯飞 T30 Pro／旧型号补充 | 70项台账；8条教程抽样补数学/英语/语文/读听/计划；旧型号恢复文档 | 余26功能/24问题视频；同题错答/重试/完成重进，计划修改已完成记录、取消和双端冲突；按型号核持久保存/离线 |'
p.write_text('\n'.join(lines)+'\n')
p=L/'analysis-status.md';s=p.read_text();needle='\n本轮之前的工作属于';assert needle in s;s=s.replace(needle,'\n| 阅读计划与证据边界深化（10月27日） | 讯飞四新教程35帧/6D2；BOOX10正文4图/5D2；洪恩历史正文与宣传视频，主链不升级；合计11新D2/20E、4旧任务补证 | 计划修改、统计归属、提示/奖励与保存分别记录；无新产品操作/连续链，全部部分覆盖 |\n'+needle,1);p.write_text(s)
p=R/'docs/ideas.md';p.write_text(p.read_text()+'\n2026-10-27继续I-021/I-022：讯飞补阅读、语文与每周计划控件，BOOX补书库/排版/统计与笔记对象，洪恩历史文字和官网宣传视频仍不足以还原当前教学链。按F/E、原件时间点及未知分支维护可复用依据，见[第二十八期简报](/home/workspace/reader/docs/briefs/028-reading-plans-and-interaction-evidence.md)。全部65组仍部分覆盖，未把奖励/计时当学习效果，未新增已确认需求或进入系统设计。\n')
(W/'summaries-applied.txt').write_text('applied once; do not rerun\n')
print('summary updates applied')
