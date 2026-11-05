import json
from pathlib import Path
out=Path(__file__).resolve().parent.parent
new=[
dict(id='AUD18-206',severity='中',area='关机图 / 刷新回执',
title='面板供电失败仍被记为关机图已经显示',evidence_type=['复现','代码核查'],
locations=['system/ui/shell/off_picture.c:56','system/services/display/display_service.c:115','tests/integration/power_menu.c:154'],
impact='给模拟 PMIC 注入持续 I2C 故障，关机时 display.power_failures=2、ui.off_picture_on_panel=1、late=0。off_picture 仅等绘图作业和 busy 清空，没有关联成功刷新的回执；失败也会清空 busy，旧画面仍可能留在墨水屏。现有正常关机测试只看提交数增长及空闲，不能排除此情形。没有声称验证真实面板。',
recommendation='让关机图绑定具体帧的成功完成结果；区分已绘制、已提交、已显示和失败，在剩余电量及时间允许时有限重试，失败不得计 on_panel。补面板供电和刷新失败用例。',
reproduce='python3 docs/audits/2026-11-05-codex-services-b/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/power_picture_probe.c --template sim-power-menu --sim-arg=--i2c-faults --sim-arg=1.0:pmic；见 experiments/power-picture.log。'),
dict(id='AUD18-207',severity='中',area='文字框 / 点击位置',
title='选区右侧的点击把结束括号也当成正文，插入位置偏后一字',evidence_type=['复现','代码核查'],
locations=['system/ui/textfield.c:51','system/ui/textfield.c:119'],
impact='正文 abcdef 选择 bc，标签为 a[bc]def；真实模拟触摸点在 d 上，光标变成 4 而非 3，输入 X 得 abcdXef。compose_line 加两个选区标记，place_of 只减一个标记。属于编辑行为错误，不是占位 UI 的外观缺口。',
recommendation='保留显示字符到正文位置的完整映射，统一处理两个选区标记、组合串和滚动窗口；增加选区两侧、组合串前后及长选区的点击用例。',
reproduce='python3 docs/audits/2026-11-05-codex-services-b/experiments/run_probe.py docs/audits/2026-11-05-codex-services-b/experiments/text_boundary_probe.c --template sim-textinput；见 experiments/text-boundary.log。'),
dict(id='AUD18-208',severity='中',area='文字能力 / AI 权限',
title='text.submit 漏查 ai_hidden，AI 仍能提交隐藏输入框',evidence_type=['复现','代码核查','文档核查'],
locations=['system/services/text/text_caps.c:81','docs/system/system-services-design.md:588'],
impact='普通文字框设置 ai_hidden=true 后，AI text.insert 正确拒绝，但 text.submit 返回 done，并调用一次 entered 回调。与该类输入框 AI 碰不到的约定不一致。本轮只验证回调被触发，没有把占位回调推断成真实消息发送、付款或空间权限突破。',
recommendation='在所有文字操作能力执行处统一核查输入框类型、ai_hidden 和调用来源，至少让 submit 与 insert/select/undo 的限制一致；核查 voice_finish/cancel 的相同边界。',
reproduce='同 text_boundary_probe.c；输出 hidden_submit status=done entered_delta=1，见 experiments/text-boundary.log。'),
dict(id='AUD18-209',severity='中',area='文字会话 / UTF-8 截断',
title='初始文字按字节直接截断，会留下不完整的 UTF-8 字符',evidence_type=['复现','代码核查'],
locations=['system/services/text/text_service.c:79','system/ui/textfield.c:227'],
impact='以 initial=中文、max=4 建立文字会话，结果为 e4 b8 ad e6，最后一个字只剩首字节，字符光标仍算作 2。后续显示、提交或持久化会接收无效 UTF-8。常规追加路径会回退到完整字符边界，初始值路径没有同样保护。',
recommendation='初始会话和控件缓存复用追加路径的 UTF-8 边界裁剪；对过长初始值明确拒绝或按完整字符截断，添加多字节字和容量交界用例。',
reproduce='text_oom_probe.c 后半段不注入分配失败，直接建立 max=4 的会话；输出 initial_4_bytes hex=e4b8ade6 cursor=2，见 experiments/text-oom.log。')]
path=out/'findings.json'
all_items=json.loads(path.read_text())
ids={x['id'] for x in all_items}
all_items.extend(x for x in new if x['id'] not in ids)
path.write_text(json.dumps(all_items,ensure_ascii=False,indent=2)+'\n')
path=out/'report.md'; report=path.read_text()
sections=[]
for x in new:
    if x['id'] in report: continue
    sections.append('### {id}【{severity}】{title}\n\n'.format(**x)+''.join('【'+t+'】' for t in x['evidence_type'])+x['impact']+'\n\n位置：'+ '；'.join('`'+p+'`' for p in x['locations'])+'。建议：'+x['recommendation']+'\n\n复现：'+x['reproduce']+'\n\n')
report=report.replace('## 修复核查\n',''.join(sections)+'## 修复核查\n')
path.write_text(report)
