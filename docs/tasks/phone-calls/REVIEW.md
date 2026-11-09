# 电话任务复查（第 1 轮）

日期：2026-11-09。基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`；复查版本：`2247f1399f8b2fcdcd70d409ef54f5a62f41108c`。

**结论：发现严重 0、高 1、中 1。建议修复两项流程问题后再验收。** 原有拨出、接听、拒接、持久化、联系人选择、成员隔离和 AI 确认测试可以独立复现；常规及 ASan/UBSan 全量均为 224/225 通过，唯一失败是共享能力表文档过期。开发报告如实披露了这项失败及视觉差距，没有把验收 5 写成全部通过。

本轮只修改本报告，不修改产品或测试源码，不提交。反例及补充流程在系统临时目录编译测试副本，复用本次构建的库、真实页面、服务和模拟 AT 模型；未碰设备、未真实拨号。现有测试产物仍在被忽略的构建目录。临时复查证据目录名为 `ink-phone-review-r1`，下文以 `$PHONE_REVIEW_EVIDENCE` 表示；附录提供不依赖该目录的重建命令。

## 问题

### PC-R1-01（高）：外呼接通后回主页，再打开电话无法回到通话，也没有挂断按钮

- **文件和行号：** `system/apps/phone/phone.c:77–79,162–174`；应用入口 `system/apps/contacts/contacts.c:239`。
- **复现命令：** 附录的最小复现命令。对应断言为 `PC-R1-01: active call must remain reachable`。操作顺序是拨出合成号码并接通 → `system.home` → `shell.launch {"id":"phone"}`。
- **结果【实测】：** 模型仍为 active（`sim_net_call(NULL, 0) == 1`），界面却是 `Phone` 拨号盘，页面快照和可见控件均没有 `call.hangup`。主页也没有 `MainInCall` 画板上的返回入口。人按一次主页后就无法通过电话界面挂断，只能另用 AI/能力调用或由对端结束。无须人为阻塞线程；这是日常导航下的通话控制丢失，定为高。
- **原因【实现】：** 只有瞬时的 `call.dialed` 事件调用 `show(&CALL)`。主页操作清掉应用页面栈后，电话入口按固定的 `Phone` 名称创建拨号盘，没有恢复当前成员正在进行的通话。现有 `outgoing()` / `hangup()` 始终在通话页上连续执行，测不到离开再进入的情况。
- **最小修法：** 在电话应用统一入口，根据已有事件维护的通话状态恢复当前成员仍在进行的通话，继续用既有 `call.hangup`；不逐个主页、联系人等调用方补导航，也不要求为本修复新增主页卡片机制。增加一条“接通 → 主页 → 电话 → 挂断”的回归。
- **图证：** `$PHONE_REVIEW_EVIDENCE/flow/snap-07-MainInCall.png`、`snap-08-Phone-reopened-active.png`。

### PC-R1-02（中）：停留在最近通话页接听来电，挂断后列表和页面快照仍是旧记录

- **文件和行号：** `system/apps/phone/phone.c:50–60,77–79,195–199`；测试覆盖缺口在 `tests/phone_calls/phone_calls.c:45–53`。
- **复现命令：** 同一附录命令。对应断言为 `PC-R1-02: open history must refresh`。先留在有一条记录的 `PhoneRecents`，通过模拟 AT 来电，点接听、挂断，等待数据服务空闲及显示稳定，直接读取原页面。
- **结果【实测】：** 数据集合已有两条，页面及 AI 页面快照仍只有原来的一条；重新调用 `phone.recents` 才出现两条。数据没有丢失，属于可通过重新进入恢复的显示错误，定为中。
- **原因【实现】：** 列表总数只在页面创建时读取，`changed()` 对后续通话事件只重画 `PhoneCall`。现有 `history()` 每次先重新打开最近通话页，所以绕过了这条刷新路径。
- **最小修法：** 在通话记录写入成功的集中点，复用现有 UI 执行器投递一次刷新，只更新仍属于该成员的最近通话页。不要加轮询、sleep 或新的事件框架，也不要仅在早于写入处理的 `call.ended` UI 回调中抢先刷新。回归直接检查原页面，不先重新打开。
- **图证：** `$PHONE_REVIEW_EVIDENCE/flow/snap-09-PhoneRecents-before.png`、`snap-12-PhoneRecents-after.png`、`snap-13-PhoneRecents-reopened.png`。

## 逐条验收

| 验收 | 本轮结论 | 独立证据 |
| --- | --- | --- |
| 1：拨出、接听、拒接、挂断及重启持久化 | 基本链路通过；日常导航和原页刷新有上述缺陷 | 常规、ASan 的 `phone-calls` 均通过；独立原样测试副本五阶段合计 251 个断言通过，新进程读回大人 5 条、孩子 1 条。最小补充复现出现 01、02 两项失败 |
| 2：联系人选号码拨出 | 通过 | 实际联系人编辑页点“选择这个号码”，经 `contacts.select` 回填后真实调用模拟拨号；`contacts` 与 `phone-calls` 均通过 |
| 3：成员隔离和反例 | 通过，反例有效 | 只在临时副本把 `ink_db_open_cached_member(&HISTORY, P.member)` 改为固定 `"owner"`，3 个断言失败；原样副本 251 个断言通过。未运行会修改仓库源码的原 `reverse.py` |
| 4：三页快照、同一能力、AI 确认 | 指定路径通过；01、02 同时影响页面可操作性及数据新鲜度 | 三页快照进入 3072 字节上下文；触摸与 AI 使用 `call.dial` / `call.hangup`；AI 拨号返回待确认、确认前未拨出、AI 自确认被拒、人确认后接通，均独立重跑通过 |
| 5：能力登记、全量、内存、350 行预算 | 部分通过，合并门尚未全绿 | 源码与运行时匹配 224 项能力/事件、16 项设置；产品改动按报告口径为新增 274、删除 2，共 276 行。两套全量均仅 `capability-table-doc` 失败；电话场景无内存分配失败，未放宽预算 |

## 命令与结果

以下命令均在仓库根目录执行，使用题目给定的 `INK_DEPS`、`INK_DESIGN`，构建和测试均限制为 `-j2`、utility 优先级，没有并行跑两轮全量。

```bash
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

- 【实测】两次构建退出 0。核对两份 `CMakeCache.txt`：`INK_WERROR=ON`，字体、LVGL、Python 均指向指定依赖，使用已有中枢 Python；内存检查目录另有 `INK_SANITIZE=ON`。
- 【实测】常规全量退出 8，224/225 通过，141.39 秒；ASan/UBSan 全量退出 8，224/225 通过，257.15 秒。没有跳过项，没有额外的 sanitizer 错误。日志分别为系统临时目录中的 `ink-phone-review-ctest.log`、`ink-phone-review-asan-ctest.log`。
- 【实测】电话五阶段分别通过 164、33、14、20、20 个断言。两套 `metrics.json` 的 `mem.internal_total.min_free_bytes` 最低均为 **47,840 字节**，门槛 **32,768 字节**；内部、外部、DMA 分配失败计数均为 0。
- 【实测】能力表核对：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output "$PHONE_REVIEW_EVIDENCE/capabilities.md" --runtime build/test-out/capability-table/capability-inventory.json`，退出 0，285 个定义核对通过。共享文档仍写 213 项，实际 224 项；按总规则第 7 节应由主会话合并后生成，不能把当前全量说成全绿。
- 【实测】反向对照：以原构建的 `flags.make` / `link.txt` 编译原样与单点变异的测试副本，执行 `tests/phone_calls/run.py`。变异版退出 1，失败为“孩子应有 0 条”“孩子应有 1 条”“孩子独立历史”三项；原样版退出 0，251 个断言通过。证据为 `$PHONE_REVIEW_EVIDENCE/negative.log`、`positive.log`。没有变更仓库源码或既有断言。
- 【实测】包含截图的补充流程退出 1，111 个断言通过、上述两项失败；附录更小的复现退出 1，56 个断言通过、同两项失败，`test.finished=1`，没有额外失败。

## 视觉核查

`out/ui-compare/` 不存在。已打开现有 `build/test-out/phone-calls/{Phone,PhoneRecents,PhoneCall}.png` 三张对照图，并在临时目录复用本轮重新跑出的截图，用 `tools/ui_compare.py --reuse` 独立重算，结果一致：

| 页面 | 1/4 灰度平均差 | 结论 |
| --- | ---: | --- |
| Phone | 32.474/255 | 超过 12 |
| PhoneRecents | 52.382/255 | 超过 12 |
| PhoneCall | 25.851/255 | 超过 12 |

对照命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out "$PHONE_REVIEW_EVIDENCE/ui-compare" --scenes Phone PhoneRecents PhoneCall --reuse`。该临时目录的三个场景目录链接到本轮 `build/test-out/phone-calls/` 对应目录；不复制或提交设计稿。

另外逐屏打开了以下 **14 个真实流程状态**，图在 `$PHONE_REVIEW_EVIDENCE/flow/snap-<编号-名称>.png`。相关参照 `PhoneContacts`、`PhoneIncoming`、`MainInCall` 也直接打开核对；没有把同一屏的差分图凑作另一屏。

| 编号与状态 | 目视结果及与画板的差别 |
| --- | --- |
| 01-Phone-empty | 空拨号盘布局完整。F2 输入框替代原稿居中号码区；数字键为现行 163 规格，比旧稿大，整体上移；删除图标变成“删除”文字圆按钮 |
| 02-Phone-input | 焦点框和静止光标可见；弹出共用字母键盘遮住下方拨号键，未发现输入框被键盘遮挡 |
| 03-PhoneContacts | 原联系人页仍是“联系人”标题加添加按钮，没有画板上的三段导航；这一布局继承基线 |
| 04-ContactEdit | 合成姓名、号码、选择/删除/保存操作均可见，选择按钮实际可用；这是已有联系人编辑页 |
| 05-Phone-selected | 正确回填号码，但已知联系人姓名没有像画板一样出现在号码下方 |
| 06-PhoneCall | 有真实号码和“通话中”，但无姓名字头像、分钟数、挂断文字标签；四个附加按钮由“暂不可用”说明替代，中央留白明显，属于报告已披露差距 |
| 07-MainInCall | 实际是普通主页，缺少画板顶部的通话返回条；与 01 号问题相关，不建议为修复它扩建主页机制 |
| 08-Phone-reopened-active | 通话仍 active，却显示拨号盘；01 号问题的直接图证 |
| 09-PhoneRecents-before | 一条真实记录。无画板的姓名头像和行分隔；时间混入第二行，没有独立靠右；页码条位于约 y=845，原稿靠近底边，间距差异明显 |
| 10-PhoneIncoming | 接听/拒接可见；与画板相比少状态栏、姓名、快捷回复，通用头像代替姓名字头像；属于已有系统来电浮层 |
| 11-PhoneIncoming-answered | 仍显示既有来电浮层，标题“来电”、正文“通话中”、底部挂断；没有切为新 `PhoneCall`，来电号码也不再显示 |
| 12-PhoneRecents-after | 挂断后仍显示一条旧记录；02 号问题图证 |
| 13-PhoneRecents-reopened | 重新进入后正确显示两条，证明记录已写入；原页未刷新而非数据丢失 |
| 14-PhoneCall-ended | 状态变成“通话已结束”，但大挂断按钮仍在且页面动作仍可用，结束态层次没有收束 |

白底、`#888888` 静态灰及 1.2 倍规格符合现行主题，不因与旧画板不一致而要求改回。没有发现新文案错字。以上已披露或基线继承的视觉差距单列保留，不伪装为本轮新发现的严重/高问题，也不要求伪造尚无服务接口的静音、键盘、耳机或录音。

## 品味、规模与架构审视

- **过度设计：** 未发现需要整体删除的新机制。`phone.calls` 成员集合直接满足持久化要求，复用现有数据格式；三个事件补足真实拨出、接通和联系人选择结果；没有新常驻任务、线程、轮询、重试队列或通用框架。两项修复均可在电话模块集中完成，不应新增后台机制或逐个入口打补丁。
- **可删减处：** 结束态仍可用的挂断控件和动作没有实际作用，可在结束态直接移除；无需新增“通话完成”页。不要为追求对照分数把不可用的四个附加功能画成可操作按钮。
- **模块划分：** 通信仍在 comm/HAL，电话模块负责记录和 UI；BG 订阅负责数据写入，UI 订阅和能力负责页面，未在系统应用引入模拟器头文件或设备操作。BG/UI 两份状态分开有明确执行器边界，问题在入口恢复和写入后的刷新接线。
- **AI 原生：** 新操作都进入能力注册，电话记录经成员数据服务可读，三页有快照，确认门未放松。01 使重新进入后的页面缺失当前通话操作，02 使页面快照陈旧，需与触摸路径一起修。
- **范围与历史：** 对基线检查共 14 个开发文件，未夹带 sms、settings-a、memo、power-pages、clock-alarm、ai-chat 等并行任务改动。联系人入口和成功选择处有非末尾增补，已在开发报告说明，属于接线需要。没有修改模组配置/固件路径或共享需求、决策、生成能力文档。
- **测试诚实性：** 联系人既有测试只调整入口并保留原 CRUD/隔离检查；AI 清单追加 8 个操作和 3 个事件，没有删断言、抬预算或加 sleep 掩盖问题。截图等待和既有条件等待用于等待真实状态。隔离反例确实使断言失败；01、02 是新增流程覆盖缺口，不等于已有测试虚构执行。
- 【实测】`git diff --check` 通过；`git diff f0b0c539f3f8a30bca03f663484287d65c76de33..HEAD | rg -n '/Users/|/private/tmp|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}'` 无匹配（退出 1）。仅本机 macOS 复核，没有声称 Linux 或实机通过。

## 附录：两项问题的最小独立复现

在已完成上述常规构建后执行。只往临时目录写测试副本和产物；产品代码仍从当前工作区原样引用。预期模拟器退出 **1**，`metrics.json` 中恰有两条带问题编号的失败，另有 56 条通过。末尾重新打开记录页的断言会通过。

```bash
export PHONE_REVIEW_OUT=$(mktemp -d)
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
import os, shlex, subprocess
root = Path.cwd()
out = Path(os.environ['PHONE_REVIEW_OUT']).resolve()
s = (root / 'tests/phone_calls/phone_calls.c').read_text()
for rel in ('../../system/apps/phone/phone.c', '../../system/boot/main.c', '../integration/whole_product.h'):
    s = s.replace('"' + rel + '"', '"' + str((root / 'tests/phone_calls' / rel).resolve()) + '"')
probe = r'''
static void review_case(void) {
    open_phone("00000000001"); outgoing();
    CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "home"); quiet();
    CHECK(BY_HAND("shell.launch", "{\"id\":\"phone\"}") && g_rc.status == INK_RC_DONE, "reopen phone"); quiet(); inspect();
    CHECK(sim_net_call(NULL, 0) == 1, "AT call is still active");
    CHECK(!strcmp(ink_ui_page_name(), "PhoneCall") && strstr(snap, "call.hangup"), "PC-R1-01: active call must remain reachable");
    CHECK(BY_HAND("call.hangup", NULL) && g_rc.status == INK_RC_DONE, "cleanup");
    CHECK(WAIT_FOR(!H.active && ink_db_idle(), 10000), "outgoing stored"); quiet();
    history(1); incoming(true); inspect();
    CHECK(ink_db_count(P.db) == 2, "both calls stored");
    CHECK(strstr(snap, "\"total\":2") && strstr(snap, "00000000002"), "PC-R1-02: open history must refresh");
    history(2);
}
'''
s = s.replace('static void run(void *arg) {', probe + '\nstatic void run(void *arg) {')
s = s.replace('if (!strcmp(phase, "write")) write_phase();', 'if (!strcmp(phase, "write")) review_case();')
(out / 'probe.c').write_text(s)
flags = []
for line in (root / 'build/CMakeFiles/sim-phone-calls.dir/flags.make').read_text().splitlines():
    if line.startswith(('C_DEFINES = ', 'C_INCLUDES = ', 'C_FLAGS = ')):
        flags += shlex.split(line.split(' = ', 1)[1])
link = shlex.split((root / 'build/CMakeFiles/sim-phone-calls.dir/link.txt').read_text())
subprocess.run(['taskpolicy', '-c', 'utility', link[0], *flags, '-c', str(out / 'probe.c'), '-o', str(out / 'probe.o')], check=True)
link = [str(out / 'probe.o') if x == 'CMakeFiles/sim-phone-calls.dir/tests/phone_calls/phone_calls.c.o'
        else str(out / 'sim-probe') if x == 'sim-phone-calls' else x for x in link]
subprocess.run(['taskpolicy', '-c', 'utility', *link], cwd=root / 'build', check=True)
PY
INK_PHONE_PHASE=write taskpolicy -c utility "$PHONE_REVIEW_OUT/sim-probe" \
  --profile simulator/profiles/metalio-plus.json --script tests/phone_calls/calls.script \
  --budget tests/ui_scene.budget.json --assets build/assets --content build/test-content \
  --flash-image "$PHONE_REVIEW_OUT/user.img" --sd-dir "$PHONE_REVIEW_OUT/sd" \
  --out "$PHONE_REVIEW_OUT/run" --quiet --no-random-outages
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
import json, os
j = json.loads((Path(os.environ['PHONE_REVIEW_OUT']) / 'run/metrics.json').read_text())
print({k: v for k, v in j['metrics'].items() if k.startswith('test.')})
print(j['warnings'])
PY
```

SEVERE=0 HIGH=1
