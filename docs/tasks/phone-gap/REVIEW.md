# phone-gap 第 1 轮复查

2026-11-08。复查基线：`291da6760931601e4783ebaddabc08e5bcab22eb`；被复查提交：`0f8848451c737f56d302ac67d6e5d90867c340b4`。

结论：**本轮文档复查通过，未发现需要修改本报告的问题。** 这只验收缺口盘点，不代表系统已达到“没装应用的手机”标准，也不代表既有代码通过了本轮运行验证。

## 范围与方法

- 按 `review-prompt.md`、`phone-gap.md` 和 `codex-rules.md` 独立检查，只写本文件，不提交。
- 基线到被复查提交仅新增 `REPORT.md`（288 行）。本任务是文档盘点，按任务书不运行构建、产品测试或界面截图验收。
- 独立核对报告的能力表、源码及测试内容、设计与 backlog；执行文件、引用、范围、空白和隐私检查。表格完整性与设置项做全量静态检查，实现重点核对 13 个“有”及应用入口、服务器、相机、通信、确认和恢复的边界。静态代码证据不等于运行通过，更不等于真机验证。

## 验收核对

| 项目 | 独立核查结果 | 证据 |
| --- | --- | --- |
| P-01 硬件取舍 | 符合 | 已读硬件记录、背景、硬件对照及相关 HAL；区分芯片驱动、模拟端口与尚未建立的整机端口。 |
| P-02 能力及设置逐项状态 | 符合 | 实际统计 79 项：有 13、部分 56、没有 4、未查到 6；源码中的 16 个设置项均在表内。已核对注册入口、实现、空包和测试内容。 |
| P-03 backlog 与工作量 | 符合 | 归属列出现的 29 个任务名均有 backlog 节或独立任务书；新缺口明确写“未覆盖”，一小时任务数标为估算，共用工作不可直接相加。具体工期没有实测保证。 |
| P-04 验证边界与建议顺序 | 符合 | 未把替身服务器、AT 模型或局部真屏测量算作完整产品可用；结尾保留按依赖推进的建议。 |
| P-05 文件、链接及隐私 | 已执行检查，通过 | 仅新增报告 288 行；79 行五列完整；118 个引用文件存在，显式行号未越界，未发现未定义引用；报告 38,145 字节，差异空白和指定隐私扫描通过。 |

## 问题

严重 0、高 0、中 0、低 0。没有发现本次改动将占位实现写成产品完成、伪报测试通过、删减需求或修改范围外文件的情况。

## 关键证据复核

| 报告条目 | 本轮独立核对到的事实 |
| --- | --- |
| A02／A03／A07 | `app_registry.c` 有应用目录、权限、前后台、冻结和保存／恢复路径；`ui_theme.h` 有报告所列基础控件；`datastore.c`／`data_caps.c` 有成员集合、分页、版本和本地持久回执。报告将完整页面恢复、语义合同及跨端回执另列缺口，未把基础接口当成完整应用框架。 |
| A05／A06／A08／A09 | `navigation.c` 仍手写快照，`textfield.c:152` 的复制菜单、`remote.c:424` 的网页点按仍直调；`confirm.c:24`、`:524` 是内存待确认表及现有能力注册，未有持久查询。报告对软总线、AI 接口、成员收拢和持久确认保留“部分”，与现状相符。 |
| B01／B13／B17／B26 | `system/boot/main.c:88` 只注册 home／reader；`navigation.c:1290` 对未实现应用返回不可打开。联系人、备忘、相册不能凭图标算完成，报告没有这样做。设置和 AI 有壳内特殊入口，不能仅凭应用注册表判成没有。 |
| B06／B24／B27 | 中枢模型和语音是 `fake.py`；账号、记忆、远程包明确标后续阶段。备份测试的身份重绑定是合成夹具，报告明确保留真实重绑定、设备资料及恢复协议缺口，没有借历史测试宣称真实家庭服务器可用。 |
| B10／B19 | 剪贴板实现有 AI 授权、敏感内容拒读和换成员清空；通知实现有成员收件箱、渠道、勿扰，界面有翻页路径。对应测试实际包含授权／敏感拒读／换人和通知前后页断言；本轮只读这些断言，未执行测试。 |
| B11／B12／B25 | 通信服务有 AT 调用与 16 条内存短信；相机只留最后一张 PSRAM 图像。报告分别保留通话音频、持久会话、相机存卡及整机验证缺口。 |
| C01～C31 | 16 项 `ink_setting_def_t` 均有对应表项。亮度、暖光、字号、锁屏时长、音量、勿扰、选网和选卡有服务／界面路径；缺入口的设置保持“部分”。飞行、APN、语言等没有用已有设置表冒充完成。 |
| D01～D08 | `ports/` 只有 `sim/`；硬件对照中的行为模型、协议未知项与局部真屏历史记录不能证明整机通过，报告保留了这些限制。 |

相关测试可在 `CMakeLists.txt` 找到直接登记或场景展开；`voice_handsfree` 经 `ink_scenario` 登记，`timesvc_line_low` 经 `foreach(v main rtc_lost line_low)` 登记。没有将字符串检索不到完整文件名误判成未登记。

## 实际执行的文档检查

以下从仓库根目录执行；最终均退出 0：

```sh
taskpolicy -c utility git diff 291da6760931601e4783ebaddabc08e5bcab22eb HEAD --stat
taskpolicy -c utility git diff 291da6760931601e4783ebaddabc08e5bcab22eb HEAD --check
taskpolicy -c utility python3 - <<'PY'
from pathlib import Path
import collections, re, subprocess
p = Path('docs/tasks/phone-gap/REPORT.md')
t = p.read_text()
rows = [x for x in t.splitlines() if re.match(r'\| [ABCD]\d{2} ', x)]
assert len(rows) == len({x.split('|')[1].split()[0] for x in rows}) == 79
assert all(len(x.split('|')) == 7 and all(v.strip() for v in x.split('|')[1:-1]) for x in rows)
states = collections.Counter(next(s for s in ('未查到', '没有', '部分', '有')
    if x.split('|')[2].strip().startswith(s)) for x in rows)
assert states == {'有': 13, '部分': 56, '没有': 4, '未查到': 6}
refs = dict(re.findall(r'^\[([^\]]+)\]:\s*(\S+)', t, re.M))
assert len(refs) == 118
for key, url in refs.items():
    target = p.parent / url.split('#')[0]
    assert target.is_file(), key
    if '#L' in url:
        assert 0 < int(url.split('#L')[1]) <= len(target.read_text().splitlines()), key
for key, line in re.findall(r'\[[^\]]+\]\[([^\]]+)\](?: L(\d+))?', t):
    assert key in refs, key
    if line:
        assert int(line) <= len((p.parent / refs[key].split('#')[0]).read_text().splitlines()), key
keys = []
for src in Path('system').rglob('*.c'):
    for body in re.findall(r'const\s+ink_setting_def_t\s+\w+\s*\[\s*\]\s*=\s*\{(.*?)\};', src.read_text(), re.S):
        keys += re.findall(r'\.id\s*=\s*"([^"]+)"', body)
assert len(keys) == 16 and all(any(key in row for row in rows) for key in keys)
backlog = set(re.findall(r'^## ([a-z][a-z0-9-]+)\s*$', Path('docs/tasks/backlog.md').read_text(), re.M))
tasks = set()
for row in rows:
    cell = re.sub(r'\[([^\]]+)\]\[[^\]]+\]', r'\1', row.split('|')[4])
    tasks.update(re.findall(r'\b[a-z]+(?:-[a-z0-9]+)+\b', cell))
assert all(name in backlog or Path('docs/tasks/' + name + '.md').is_file() for name in tasks)
base = '291da6760931601e4783ebaddabc08e5bcab22eb'
assert subprocess.check_output(['git', 'diff', '--name-only', base, 'HEAD'], text=True).splitlines() == [str(p)]
diff = subprocess.check_output(['git', 'diff', base, 'HEAD'], text=True)
assert not re.search(r'/Users/|/private/tmp|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}', diff)
assert p.stat().st_size < 5 * 1024 * 1024
print('PASS: 79 rows; states 13/56/4/6; 118 references; 16 settings; 29 task names; scope; privacy; size')
PY
```

输出与报告的数量一致。引用目标存在、行号不越界只证明可定位；语义结论另以上面的源码阅读核对，不以链接检查替代实现判断。

## 架构审视与总规则

- 模块边界：报告分开了设备服务、系统界面、平台端口和服务器，不把服务接口等同完整应用，也没有建议界面直接控制硬件。
- AI 原生：A05／A06 明确保留控件语义、统一调用路径的缺口；A08／A09 保留成员及确认边界，未宣称 G-002／R-061 已全面达成。
- 本次差异没有代码、预算或测试变更，因此不存在撤回修复的反向对照对象；未新增 sleep、放宽断言或跳过测试。
- 复查未使用设备、真实密钥、付费接口或外部发布；未读取依赖目录以外的其他工作区。未访问在线决策板或同步共享文档，遵守本任务只写 REVIEW 的范围。

## 过度设计

产品代码改动 0 行，没有新增任务、线程、状态机、文件格式、配置项、框架、依赖或测试。建议仍复用现有服务及已发任务，未知部分先盘点；未发现本任务引入的过度设计，无建议删除的机制。

## 验证限制

未运行产品构建、全量测试、ASan、Linux 检查、界面十屏对照或实机操作：任务书明确本任务只写报告，不改代码、不要求构建和测试，且不是界面实现任务。已有测试被引用为静态覆盖证据；本轮不替它们背书运行结果。复查只写本文件，不提交。

SEVERE=0 HIGH=0
