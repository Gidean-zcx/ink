from pathlib import Path

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'

def replace(path, old, new):
    s = path.read_text()
    assert s.count(old) == 1, (path, old[:80], s.count(old))
    path.write_text(s.replace(old, new))

replace(R / 'README.md',
    '- [第十九期：阅读队列、内容版本与 AI 动作](/home/workspace/reader/docs/briefs/019-reading-queues-and-ai-actions.md)：最新简报；',
    '- [第二十期：儿童纠错、任务完成与可恢复状态](/home/workspace/reader/docs/briefs/020-child-feedback-and-resumable-state.md)：最新简报；儿童实际错答与返回、AI引用定位、设备后台与恢复的证据边界。\n- [第十九期：阅读队列、内容版本与 AI 动作](/home/workspace/reader/docs/briefs/019-reading-queues-and-ai-actions.md)：')
replace(L / 'README.md',
    'Duolingo匿名法语课程及Tolino公开图书馆选择器。',
    'Duolingo匿名法语课程、Tolino公开图书馆选择器及Britannica公开Community Helpers活动。')
replace(L / 'README.md',
    '及[选馆审计](artifacts/tolino-controls-deepening/browser-operation-audit.md)。',
    '、[选馆审计](artifacts/tolino-controls-deepening/browser-operation-audit.md)及[儿童互动审计](artifacts/britannica-controls-deepening/browser-operation-audit.md)。')
replace(L / 'README.md', '最新入口：[第十九期简报]', '前批入口：[第十九期简报]')
with (L / 'README.md').open('a') as f:
    f.write('\n最新入口：[第二十期简报](../../briefs/020-child-feedback-and-resumable-state.md)。Britannica公开活动补错答、回看、讲解、四题回顾与重载；微信读书分历史AI引用、Web帮助和Skill数据口径；Meebook M7补配置与型号日志。文档、有限实际操作和仍缺原件的范围分别保留。\n')

replace(L / 'inventories/README.md',
    '| --- | --- | --- |\n',
    '| --- | --- | --- |\n'
    '| [M049-Britannica指南、FAQ与活动控件](../batches/britannica-controls-deepening-inventory.csv) | 两指南、18个FAQ、两教师checklist及静态/实际控件，共163行 | 分Early/Elementary/高学段；父子、重复和研究缺口分别标记，实际操作只覆盖公开预览与目录失败 |\n'
    '| [M013-微信读书Web、Skill与历史AI](../batches/weread-controls-deepening-inventory.csv) | Web四条帮助、固定Skill接口章节、历史AI及版本/注销范围，共32行 | Web帮助来自首页引用的公开脚本文字；Skill为静态契约，手机完整菜单和当前AI链未知 |\n'
    '| [M057-Meebook M7指南与两版日志](../batches/meebook-controls-deepening-inventory.csv) | 快速指南19项、M7历史日志22项及8项缺口，共49行 | 原PDF未取得、模板型号有疑点；其他型号日志不套用，0新操作或静态图 |\n')

replace(L / 'comparison.md',
    '| 儿童百科的游戏是谁组织和判定的？ | [Britannica F-M049-04/05/06](cases/M-049-britannica.md) 分开内嵌互动、Fundamentals 和教师组织的主题活动；教师积分规则不是内置奖励。 | 区分界面完成、成人反馈和理解表现；不能因有动画或积分就认定存在有效学习闭环。 | G-001、R-019 |',
    '| 儿童百科的游戏是谁组织和判定的？ | [Britannica F-M049-04/05/06](cases/M-049-britannica.md) 分开Fundamentals、公开职业互动和教师活动；公开预览实操有错答重试、回看线索、正确讲解、四题回顾及开放问题。 | 反馈、完成、理解表现和成人规则分别比较；一次活动的机制不推导全产品奖励或学习效果。 | G-001、R-019 |')
replace(L / 'comparison.md',
    '## AI上下文与执行结果\n',
    '## AI上下文与执行结果\n')
# 每组表格紧接标题的表头不同，直接插入已知问题行之前。
anchor = '| 有 AI 接口是否就开放了全部数据？ |'
s = (L / 'comparison.md').read_text()
assert s.count(anchor) == 1
s = s.replace(anchor,
    '| AI给出来源后，能否回到原任务？ | [微信读书 F-M013-01/04](cases/M-013-weread.md) 的2024官方说明给搜索问答到引用片段路径，选文问书另有入口；未实测跳转与返回。 | 分开提问对象、回答、引用锚点、跳转结果和返回位置；历史静态图不当现版闭环。 | G-002、R-007/021 |\n' + anchor)
anchor = '| 分享包会自动跟上集合变化吗？ |'
assert s.count(anchor) == 1
s = s.replace(anchor,
    '| 返回、重做和重载会恢复同一状态吗？ | [Britannica F-M049-05](cases/M-049-britannica.md) 匿名预览Back稳定到前题正确讲解，Start Again及iframe重载回标题。 | 动画中间态、局内导航和持久进度分别核查；不外推订阅账号、关机或其他游戏。 | R-014/019、H-004/005、G-002 |\n' + anchor)
anchor = '## 系统交互与研究方法\n'
assert s.count(anchor) == 1
# 置于该节已有表格中，避免再建重复分类。
pos = s.index(anchor) + len(anchor)
end = s.index('\n| ---', pos)
end = s.index('\n', end + 1)
s = s[:end+1] + '| 配置按键或清后台，是否证明App状态被保留？ | [Meebook M7 F-M057-04/06](cases/M-057-meebook.md) 有历史长按/滚动优化入口和两种后台管理说明，未核App接收与未保存编辑。 | 配置、事件目标、后台处置和持久保存分别查证；型号/固件日志不能代替执行结果。 | R-008/010/018、G-002 |\n' + s[end+1:]
(L / 'comparison.md').write_text(s)

replace(L / 'backlog.md',
    '| Britannica Early／Elementary | 两层级指南；Early 的 11 条章节/子项目录、互动和教师主题方案 | 媒体操作完整链、单题错误/提示/重试、退出恢复及个人保存；教师游戏规则不代替内置奖励 |',
    '| Britannica Early／Elementary | 两层级指南、163行新增台账；Community Helpers公开四题错答/回看/完成/Back与重载，目录429单独记录 | 机构内真实保存、Fundamentals题型、音频开关效果、媒体执行与全文离线；公开预览不套账号版 |\n'
    '| 微信读书 | 32行台账；Web四帮助、2024历史AI问书及3静态图、固定Skill数据范围 | 手机完整目录与当前构建、选文后引用/返回、划线撤销、导入听书恢复及实际接口全量/写回 |\n'
    '| Meebook M7 | 49行转录/型号日志/缺口；六旧任务补条件，没有新增任务 | 原PDF与模板型号疑点、当前菜单、键位优先级、后台编辑/音频保留、书架索引与备份恢复 |')
with (L / 'backlog.md').open('a') as f:
    f.write('\n## 2026-10-27：儿童纠错、引用与可恢复状态续查\n\n'
    '- Britannica：35次网页状态捕获、22张正式实看截图；D4限定Community Helpers预览及目录失败。四题回顾不是第五题；Back稳定讲解与动画过渡分开。没有实际听音，AD/音乐/Credits未逐个操作；订阅平台的保存、学习效果及其他游戏仍未知。目录429后未重试筛选。\n'
    '- 微信读书：两新Web任务为帮助文字重建；F01仅2024历史AI子链升D2。搜索问答的来源定位不自动套到选文分支，当前App仍待核；Skill只读规范不等于已导出/可恢复，README与SKILL版本差异保留。\n'
    '- M7：型号两版日志和快速指南转录补原任务，尚无原PDF或实机链。京东读书记录同步不当全系统同步，任务管理/后台管理不推定相同效果；拒绝访问的资料停止该路径。\n\n'
    '本批11新E、4新F（3 D2、1 D4），17旧F补63字段；14旧E追加采集。三份目录244行包含重叠/缺口；六张静态图与22张网页图分开，全部产品仍部分覆盖。\n')

replace(L / 'analysis-status.md',
    '\n本轮之前的工作属于“有进展”',
    '\n| 儿童纠错、引用与可恢复状态深化（10月27日） | Britannica7E/2F/163目录及35网页状态22正式图；微信读书4E/2F/32目录及三图；M7零新E/F及49目录；17旧F补63字段、14旧E追加 | 新任务3D2/1D4；旧儿童互动仅公开子范围升D4、历史AI子链升D2。订阅保存、当前App/设备及学习效果仍缺，全部部分覆盖 |\n\n本轮之前的工作属于“有进展”')
with (R / 'docs/ideas.md').open('a') as f:
    f.write('\n2026-10-27继续I-021/I-022：Britannica公开互动实际补错答、回看线索、正确解释、四题回顾及返回/重载差别；微信读书拆开历史AI引用、Web帮助与Skill数据口径；M7保留版本日志和原件缺口。详见[第二十期简报](/home/workspace/reader/docs/briefs/020-child-feedback-and-resumable-state.md)。反馈、完成与持久保存分开作为设计问题；学习效果、账户方式和系统架构没有据此定案。所有产品仍部分覆盖。\n')
print('Updated seven indexes and ideas; requirements unchanged.')
