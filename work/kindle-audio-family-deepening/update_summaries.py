from pathlib import Path
import json
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/kindle-audio-family-deepening'
assert not (W/'summaries-updated.json').exists(),'one-time update'
def rep(p,a,b):
 s=p.read_text();assert s.count(a)==1,(p,a[:30],s.count(a));p.write_text(s.replace(a,b))
def app(p,s):
 with p.open('a') as f:f.write(s)
rep(R/'README.md','## 项目资料\n\n','## 项目资料\n\n后续设计优先从[按问题查竞品依据](docs/research/interaction-library/design-reference-guide.md)进入，再查具体任务、证据与缺口；不必按时间顺序读完简报。\n\n')
rep(R/'README.md','- [第二十五期：答完、答对与保存](/home/workspace/reader/docs/briefs/025-practice-feedback-and-recovery.md)：最新简报；','- [第二十六期：声音、档案与研究复用](/home/workspace/reader/docs/briefs/026-audio-controls-and-research-reuse.md)：最新简报；音频控制、AI预览/保存、家庭档案与恢复，附按设计问题检索入口。\n- [第二十五期：答完、答对与保存](/home/workspace/reader/docs/briefs/025-practice-feedback-and-recovery.md)：')
rep(L/'README.md','| 入口 | 用途 |\n| --- | --- |\n','| 入口 | 用途 |\n| --- | --- |\n| [按设计问题查证据](design-reference-guide.md) | 读听、学习、新闻、AI动作、恢复与显示的使用入口；说明怎样引用F/E及判断证据是否足够。 |\n')
rep(L/'README.md','最新入口：[第二十五期简报]','前批入口：[第二十五期简报]')
app(L/'README.md','\n最新入口：[第二十六期简报](../../briefs/026-audio-controls-and-research-reuse.md)。Kindle补声音/AI/家长限制与静态控件，Kompakt补核心App对象与保存，Khan补帮助目录/档案/音乐及报告。新增按设计问题检索入口；全部仍部分覆盖，没有新增产品实操。\n')
rows='| [M001-Kindle音频/家庭/系统控件](../batches/kindle-audio-family-deepening-inventory.csv) | 51项正文控件；原58目录项同步回写 | 11幅官方静态图，3新帮助；Scribe儿童档案及AI地区冲突保留，未实操 |\n| [M012-Kompakt核心App帮助](../batches/mudita-app-controls-deepening-inventory.csv) | 原111项的更新快照，补读18篇；46项仍仅目录 | 保留旧目录历史；两份快照不代表222项独立功能；无新增看图 |\n| [M030-Khan帮助目录和控件](../batches/khan-library-controls-deepening-inventory.csv) | 六栏目/八分页112入口含重复，另18正文节点 | 130行不等于完整App功能；2静态图，未验证读听切换或儿童学习效果 |\n'
rep(L/'inventories/README.md','| --- | --- | --- |\n','| --- | --- | --- |\n'+rows)
def table(section,rows):
 p=L/'comparison.md';s=p.read_text();i=s.index('## '+section+'\n');j=s.index('\n| ---',i);end=s.index('\n',j+1);p.write_text(s[:end+1]+rows+s[end+1:])
table('学习与反馈','| 能看到多次尝试就能判断独立掌握吗？ | [Khan F02/04/05/13](cases/M-030-khan-kids.md) 课堂报告可展开历次尝试，但提示、降级阈值及奖励仍缺。 | 分开首次/求助后答案、教师判断与产品掌握标签；中文指南不等于中文课程。 | G-001、R-014/019 |\n')
table('AI上下文与执行结果','| X或Done究竟停止、保存还是放弃？ | [Kindle F03/10](cases/M-001-kindle.md) 便签X保存、AI预览X确认放弃；[Kompakt F14](cases/M-012-mudita.md) 两次Done分别停止和保存。 | 按对象及页面解释动作，核持久结果；关闭回放页面不等于停播，预览不等于已写入。 | G-002、R-007/021 |\n')
p=L/'comparison.md';s=p.read_text();heading=next(x[3:] for x in s.splitlines() if x.startswith('## ') and ('听' in x or '音频' in x))
table(heading,'| 开始可播或朗读开关开启就能恢复故事吗？ | [Kindle F11–14](cases/M-001-kindle.md) Audible下载进度、TTS导航暂停和VoiceView读屏分开；[Khan F06/12](cases/M-030-khan-kids.md) 下一课及背景音乐单独控制。 | 保留内容、成员、时间/文字锚点、暂停原因及离线完整度；不由图标推自动续播。 | R-018、G-002 |\n')
heading=next(x[3:] for x in (L/'comparison.md').read_text().splitlines() if x.startswith('## ') and ('恢复' in x or '备份' in x))
table(heading,'| 家庭账号、儿童档案与报告是不是备份？ | [Kindle F15](cases/M-001-kindle.md) Scribe儿童档案有官方冲突；[Khan F09/10/13](cases/M-030-khan-kids.md) 删档案清进度，报告仅HTML副本。 | 分开家长限制、共享内容、独立进度、可恢复快照；不假定新建同名档案会找回旧数据。 | G-002、H-004/005 |\n')
app(L/'backlog.md','''
## 2026-10-27：声音、档案与交互依据复用

- Kindle：补F10–20，重看11静态图，58目录项回写；AI地区/语言与Scribe儿童档案冲突保留。下一证据需固定固件的读听切换、蓝牙/睡眠/批注打断、生成失败与已插入恢复；公开指南的报刊描述不当现行新闻服务。
- Kompakt：111目录补读18篇，仍46项index_only；录音停止/保存/关回放分开。来电/锁屏/Offline+中断、录音误删、联系人合并/同步与定位等级仍未知，未借Pure或通用Android补齐。
- Khan Kids：112文章入口含重复与18控件分开；教师报告/背景音乐有文档链，全部题型错误—提示—重试仍缺。英文课程、中文家长指南、家庭/教师档案分别限定；读听即时切换、作品删除、同步完成和报告回导仍未知。

用[按设计问题查证据](design-reference-guide.md)进入资料，不以目录、来源和截图数量替代研究完整性；全部65组仍部分覆盖。
''')
rep(L/'analysis-status.md','\n\n本轮之前的工作属于“有进展”','\n| 声音、档案与资料复用深化（10月27日） | Kindle音频/AI/家长限制，Kompakt核心App，Khan帮助目录与报告；新增按设计问题检索入口 | 19新D2/39E，修订17旧F/54字段，13幅静态图；无新实操，全部部分覆盖 |\n\n本轮之前的工作属于“有进展”')
app(R/'docs/ideas.md','\n2026-10-27继续I-021/I-022：Kindle区分有声书/TTS/读屏、AI预览和写入，保留儿童档案及地区冲突；Kompakt补停止/保存/关闭及数据对象；Khan补多次尝试、音乐控制与档案删除。新增按设计问题查F/E的入口，见[第二十六期简报](/home/workspace/reader/docs/briefs/026-audio-controls-and-research-reuse.md)。学习效果、中断恢复和实机仍有缺口；参考未升级为需求，未进入全系统设计。\n')
(W/'summaries-updated.json').write_text(json.dumps(dict(updated=True,requirements_changed=False),indent=2)+'\n')
print('summary links updated')
