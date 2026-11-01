from pathlib import Path

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
brief = '048-learning-controls-and-statistics.md'
latest = '- [第四十八期：学习按键、确认与统计](/home/workspace/reader/docs/briefs/' + brief + ')：补答案揭示、自评、长按确认、回顾副作用和统计口径；原图与文图冲突可直接回查。'
for p in [R / 'README.md', L / 'README.md']:
    lines = p.read_text().splitlines()
    i = next(i for i, s in enumerate(lines) if s.startswith('- [第四十七期：'))
    lines[i] = latest
    p.write_text('\n'.join(lines) + '\n')
p = R / 'docs/briefs/README.md'
s = p.read_text()
assert brief not in s
i = s.index('- [第四十七期：')
p.write_text(s[:i] + latest + '\n' + s[i:])

p = L / 'design-reference-guide.md'
s = p.read_text()
start = s.index('当前56条覆盖')
end = s.index('字段和证据方式', start)
s = s[:start] + '具体覆盖以当前表及F/E为准；它是增量检索入口，不代表全库所有任务，也不能把未收录解释为产品不支持。' + s[end:]
marker = '| --- | --- | --- | --- |\n'
rows = '''| 同一个学习键何时揭示答案、何时提交评价 | [MM-WM01 F02/03](cases/M-023-maimemo-device.md) | [回忆09](artifacts/maimemo-device-controls/image-09.png)、[答案10](artifacts/maimemo-device-controls/image-10.png)、[详情21](artifacts/maimemo-device-controls/image-21.png)；RP-M023-01/03 | 不同阶段的静态示意，非连续过程；真实写入、重复按键与撤销未测 |
| 长按熟知或收藏后，怎样取消和恢复 | [MM-WM01 F03](cases/M-023-maimemo-device.md) | [熟知确认23](artifacts/maimemo-device-controls/image-23.png)及[逐图摘记](artifacts/maimemo-device-controls/source-notes.md)；RP-M023-02 | 有弹窗示意不证明每次出现、取消无写入或撤销能恢复原排期 |
| “只回顾、不计反馈”是否表示所有动作都只读 | [MM-WM01 F02/04](cases/M-023-maimemo-device.md) | [回顾19](artifacts/maimemo-device-controls/image-19.png)与[正文/状态说明](artifacts/maimemo-device-controls/source-notes.md)；RP-M023-08 | 回顾页仍可长按标记；模式声明与具体动作副作用分开 |
| 调学习量、听词定时和查看统计能否直接照文档实现 | [MM-WM01 F02/04/07](cases/M-023-maimemo-device.md) | [总览11](artifacts/maimemo-device-controls/image-11.png)、[听词14](artifacts/maimemo-device-controls/image-14.png)、[统计16](artifacts/maimemo-device-controls/image-16.png)；RP-M023-04/05/07 | 文图方向冲突、定时单位缺失、图表范围未明；统计任务仍D1 |
'''
assert marker in s
s = s.replace(marker, marker + rows, 1)
p.write_text(s)

p = L / 'comparison.md'
s = p.read_text()
rows = '''| 同一输入是否总表示同一学习动作？ | [墨墨设备F02/03](cases/M-023-maimemo-device.md)在答案前、揭示后、详情页提示不同；[手机端F01](cases/M-017-maimemo.md)也有阶段相关手势。 | 分别匹配各平台状态，不合成统一规则；AI执行前读取阶段，儿童能否理解需观察。 | G-001/002、R-014/019/021 |
| “不计反馈”的回顾能否当作无副作用模式？ | [墨墨设备F04/03](cases/M-023-maimemo-device.md)正文说回顾不记反馈，但回顾页允许长按改变熟知/收藏/不想背。 | 浏览、评价、标记和排期分别研究，不能从模式名推断所有动作只读；取消和持久保存未测。 | G-002、R-007/021 |
'''
s = s.replace(marker, marker + rows, 1)
p.write_text(s)

p = L / 'backlog.md'
s = p.read_text()
old = '| M-023 墨墨单词机 | 初始取证遇超时，后已取得官方正文；当前继续核定版本、绑定失败、标记撤销与离线同步的实际结果。 |'
new = '| M-023 墨墨单词机 | 正文及24静态图已读；继续核定固件、日量文图冲突、答案/评价阶段、确认取消后的写入、统计入口与口径、锁屏/定时/音频断点、同步冲突及解绑/复位后的恢复。 |'
assert old in s
s = s.replace(old, new)
old = '| 百词斩 Mini/Max／墨墨 MM-WM01 | 百词斩历史下发及BCZ-Ace-Max拆解键义；墨墨资源/记录与故障分流 | 百词斩型号化手册与宠物规则；墨墨云词本版本疑点、同步冲突、复位/解绑后恢复；手机端日志不代替设备固件日志 |'
new = '| 百词斩 Mini/Max／墨墨 MM-WM01 | 百词斩历史下发及BCZ-Ace-Max键义；墨墨正文和静态阶段提示/确认/听词设置 | 百词斩型号化主链与宠物规则；墨墨文图/版本冲突、确认结果、统计入口、音频及数据恢复；手机端日志不代替设备固件日志 |'
assert old in s
s = s.replace(old, new)
s += '\n第48批对汉王N10 Pro二代、百词斩Mini/Max、不背单词的有限补检仍未获得可闭合主链的同型号/版本资料；见[路线与排除记录](artifacts/batch48-source-route-audit.md)。不重复受限入口，不以搜索摘要、邻近型号或聚合转述升级深度。墨墨设备新41标题目录用于查漏，统计F07仍D1。\n'
p.write_text(s)

p = L / 'inventories/README.md'
s = p.read_text()
needle = '| [M017-墨墨帮助与API方法]'
start = s.index(needle)
end = s.index('\n', start) + 1
row = '| [M023-MM-WM01帮助标题](../batches/maimemo-device-controls-inventory.csv) | 41标题：5父组、36主题，正文已读，24相关图已看/21归档 | 含非交互与未来声明；不是41个功能或完整固件菜单，统计入口和实操仍缺 |\n'
s = s[:end] + row + s[end:]
p.write_text(s)

additions = {
    'analysis-status.md': '\n第48批复读MM-WM01完整正文与24幅静态配图，归档21图；补5旧F、新增统计任务1D1、同源静态E及8RP。41标题含分组与非交互，不计全功能完成；文字/图片冲突和确认后的保存未知保留。汉王、百词斩设备、不背单词有限补检没有升级主链，65组仍部分覆盖。见[简报48](../../briefs/048-learning-controls-and-statistics.md)。\n',
    'batches/README.md': '\n第48批 `maimemo-device-controls-*` 新增统计任务1D1、1个同源静态图组E和41标题目录；5旧F在原持有CSV更新，E-MMDVC-01追加正文采集，8RP。24图已看、21归档，无设备操作；正文/配图矛盾保留。见[简报48](../../../briefs/048-learning-controls-and-statistics.md)。\n',
}
for rel, text in additions.items():
    p = L / rel
    p.write_text(p.read_text().rstrip() + '\n' + text)

for rel in ['M-008-hanvon.md', 'M-022-baicizhan-device.md', 'M-018-bbdc.md']:
    p = L / 'cases' / rel
    p.write_text(p.read_text().rstrip() + '\n\n2026-10-27第48批补检未取得可补齐本案例主链的同型号/版本流程；[查询范围与排除线索](../artifacts/batch48-source-route-audit.md)用于避免后续重复误用。没有新增本产品F/E或提升证据深度，既有缺口继续保留。\n')

p = R / 'docs/ideas.md'
p.write_text(p.read_text().rstrip() + '\n\n2026-10-27继续I-021/I-022：MM-WM01补24幅官方静态配图，区分答案前后按键、详情导航、长按确认、回顾反馈与标记副作用；保留学习量文图冲突、定时单位与统计口径缺口。8条按问题检索依据可回查原图；见[第四十八期简报](/home/workspace/reader/docs/briefs/048-learning-controls-and-statistics.md)。儿童阶段理解和误按恢复、成人连续学习成本及AI动作状态属于待验证建议；全部65组仍部分覆盖，未改变需求或进入全系统设计。\n')
print('Updated current entrances, question tables, inventories, case route notes and ideas.')
