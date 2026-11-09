# power-pages 第 1 轮复查

复查日期：2026-11-09。结论：主路径与报告中的测试结果可复现；发现中级 1 项、低级 1 项，未发现严重或高等级问题。省电设置语义需收敛，不能把 60/600 秒切换当作已确定的独立省电策略。

- 基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。
- 被复查版本：`0ab0641d560e23cfda9971a566d6d05ef3347b39`；开始时工作区干净。
- 依据：`docs/tasks/review-prompt.md`、`docs/tasks/power-pages.md`、`docs/tasks/codex-rules.md`、开发报告及设计规格。
- 本轮只修改本文件，不修改产品代码、测试或设计，不提交。
- 已核实差异范围：9 个文件；新增 166 行界面代码，公共 C 文件新增 4 行，CMake 新增 6 行，未超过 250 行产品代码预算。

## 验收复核

| 验收项 | 复查结论与证据 | 命令 |
| --- | --- | --- |
| 1：阈值、充电、省电开关、耗尽关机 | 【实测】四场景通过，共 72 条断言；覆盖 15% 通知、5% 打断、60 秒锁屏、充电锁屏/唤醒/拔线、2% 保存及关机。省电开关仍有 PP-R1-01 | C1；C4 |
| 2：每页 AI 快照 | 【实测＋实现】四页快照均可读取；按钮使用 `settings.set`、`interrupt.ack`，没有绕过能力策略。关机区分保存成功/超时；已有 `integration-power-menu` 的关闭空间场景也通过 | C1；C2 |
| 3：全量、内存、代码预算 | 【实测】常规 225/225，180.74 秒；ASan/UBSan 225/225，241.23 秒，均退出 0、无跳过。两套构建均开 Werror，内存构建实际有 `-fsanitize=address,undefined`。【实现】170 行 C，含 CMake 为 176 行，小于 250 | C1；`git diff --numstat <基线>` |
| G1～G5、设计规则 | 【实测＋实现】全量含确定性和分层检查；尺寸检查 0 违规；10 屏人工对照完成，四页分数超目标但开发报告已披露。共享文档待主会话更新符合总规则 §7；没有修改并行任务文件 | C1；C3；差异检查 |

## 问题

### PP-R1-01｜中｜省电开关关闭时静默覆盖原有锁屏时长

- 位置：`system/ui/shell/power_pages.c:74-77`；现有测试 `tests/power_pages.c:50-55` 只覆盖默认 600 秒，并把关闭固定回到 600 秒写入期望。
- 【实测】先通过 `settings.set` 设置 `power.lock_after_s=120`，触发 5% 页面，实际触摸开关两次，得到 `120 → 60 → 600`。检查恢复原值的临时探针退出 1，唯一失败为 `custom timeout lost: expected 120 got 600`；未卡线程、未修改产品代码。
- 影响：用户以为只是开关一次省电，原先的 2 分钟锁屏实际被改成 10 分钟。开发报告已如实披露，因此不算隐瞒、严重数据丢失或高等级问题。
- 最小修法：沿用同一个 `settings.set`，把现有 60/600 两个选择明确呈现为锁屏时长选项，显示当前值；不要用一个没有说明关闭后时长的“省电”开关表达它。独立省电策略仍保留为待定，不新加持久状态或策略框架。
- 复现命令：下文 C4；临时探针与证据在 `build/test-out/power-pages-review/custom-timeout`、`build/test-out/power-pages-review/custom-timeout-BatteryLow/metrics.json`。这是已披露限制的独立验证，不另计“测试覆盖缺口”。

### PP-R1-02｜低｜关机书签把通用卡片标签放成主标题

- 位置：`system/ui/shell/power_pages.c:132-143`。
- 【实测＋实现】`PowerOffEmpty` 对照图主标题是“在读”，真实书名与页码在下方正文末行。原因是直接把 `card.title` 当成书名；阅读服务的该字段实际是“在读”，书名和页码在 `card.text`。这不是“第 1 页和设计稿第 148 页不同”的夹具差异。
- 影响：书签最显眼的信息失去书名，保存提示也比设计稿冗长；数据和页码本身没有丢失。
- 最小修法：直接把现有 `card.text` 作为主要书签文字，保存结果留作说明；没有书签时仍显示“已关机”。不解析示例书名、不新增阅读服务接口。
- 复现：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -R '^power-pages$' -j2`，查看 `build/test-out/power-pages/PowerOffEmpty/snap-scene.png`；本轮拼图保存在 `build/ui-compare-review/PowerOffEmpty.png`。

## 过度设计

【实现】未发现值得单独定级的过度设计。没有新增任务、业务服务、持久格式、配置项或重试队列。模式变量用于四页呈现，继续使用现有状态订阅、打断服务和原子能力。关机等待与旧文件有少量重复，但在“公共文件只作末尾接线”的约束下，不建议为消除这段重复建立通用框架。建议删除的是 PP-R1-01 中把锁屏时长包装成独立省电模式的含混语义；没有需要另删的机制，也不建议新增策略、缓存或成员切换框架。

## 10 屏视觉对照

【实测】使用本轮全量测试留存的稳定帧生成 `out/ui-compare/`，人工打开并检查四张任务页和六张关联页。四张任务页与开发者留存的拼图逐字节相同。检查后将本轮对照目录移到被忽略的 `build/ui-compare-review/`，避免把未跟踪的 `out/` 留作交付改动。

规范 §1 已说明旧画板没有同步 1.2 倍字号、控件与新灰度；这些差异不直接判错，也不能为了达到 12/255 而缩回旧规格。下表分数均由本轮重新计算。

| 页面 | 1/4 灰度平均差 | 人工核查 |
| --- | ---: | --- |
| BatteryLow | 25.431 | 灰色底圆由旧稿浅灰变为 #888888，图标圆、标题和底部按钮更大；新增省电开关。无错字、截断或重叠；开关语义见 PP-R1-01 |
| BatterySaving | 12.413 | 标题与说明更大，次要文字变纯黑；三点静止。默认及最大字号均未重叠 |
| Charging | 18.897 | 圆环与数字更大，百分号没有旧稿那么小；进度弧端为平头，旧稿为圆头；状态栏缺闪电和锁图标。正文仍明确写出充电与锁定，唤醒说明可读 |
| PowerOffEmpty | 17.284 | “在读”抢占书名的主标题位置，见 PP-R1-02；保存提示分成多行，底部明确写“已关机”。真实第 1 页不应改成画板示例第 148 页 |
| Lock | 25.402 | 时钟更大、更靠下；当前夹具没有闹钟，通知显示真实 0 条，没有照抄两条示例卡；没有新增遮挡 |
| LockPage | 42.093 | 阅读器的章节、正文和页码布局与画板不同，正文更大；底部锁定提示可读。阅读器不是本次改动 |
| PowerMenu | 27.871 | 背景按修订后的 D-97 淡化，按钮变大，取消变描边；旧稿的强制关机提示未显示。该页未修改 |
| PowerOff | 18.177 | 既有普通关机画面以“已关机”为上方标题，不是画板的居中书名；本次仍走旧呈现函数 |
| PhoneIncoming | 20.422 | 使用合成号码和通用头像，缺旧稿的姓名头像及两枚快捷回复按钮；本次没有改来电绘制 |
| ClockRing | 35.467 | 图标更大、时间数字较旧稿小；日期时间取测试运行状态，底部按钮/提示变大。该绘制函数未被本次修改 |

后六页用于确认公共打断/关机接线周边的呈现，不把既有视觉差异算作本任务新增缺陷；没有仅凭分数否决或放宽视觉目标。

## 已排除的疑点

- 最大字号 `display.font_size=3`：临时重链接同一场景测试，四页均退出 0；另检查保存标题与说明不相交、充电唤醒说明不越过屏底，均通过。未把最初的换行猜测记成问题。
- 原有测试未被删除或放宽；新增测试的 59/61 秒用于跨越真实自动锁屏阈值，关机轮询沿用既有显示提交等待，不是新增 sleep 掩盖竞态。
- 四页正向断言为 25/17/15/15，内部 RAM 最低余量均为 47,840 字节，预算仍为 32,768。反向接线测试分别退出 1，失败断言为 4/3/4/1。

## 范围、规则与证据边界

- 【实现】对照基线，9 个文件均与任务直接相关；没有服务层改动、并行页面改动、放宽预算或新增依赖。产品文件不引用模拟器，LVGL 绘制由 UI 执行器处理；等待关机画面发生在关机任务中。
- 【实测】`git diff --check <基线>` 通过；按总规则 §6 检索差异中的本机路径和 MAC 字样，0 命中。本轮未操作设备、真实服务或远端仓库，没有提交；最终工作区新增文件只有本报告。构建、图像与临时探针均留在忽略目录。
- 【实现】成员权限仍经现有能力策略处理，快照仍经 UI 归属检查；未发现本次新增的成人空间泄漏或 AI 代确认路径。报告披露的“提醒期间切换家人不重建页面”没有在本轮单独补测，不宣称覆盖了该场景。
- 【实测】本轮为 macOS/clang 模拟器验证，不含 Linux/gcc 或实机。无条件保证持久保存不属于新增页测试：成功关机的实际落盘由已有 `integration-power-menu` 重启后检查补充验证。

## 命令与复现

以下从仓库根目录执行，`INK_DEPS`、`INK_DESIGN` 使用用户给定值。沿用已核对的构建配置，未换依赖。C1 的两套构建/测试顺序运行，同一时间只跑一轮，均为 utility 优先级及 `-j2`。本轮完整输出留在 `build/test-out/power-pages-review/{normal-ctest,asan-ctest,asan-build}.log`。

### C1：构建与全量测试

```sh
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

### C2：反向对照及尺寸检查

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/power_pages_negative.py --build build
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_scale_check.py system/ui/shell/power_pages.c
```

两条命令均退出 0。反向脚本只在构建目录重链接基线初始化，不修改工作区；其四个子进程应全部退出 1。该对照能证明新增页面接线被测试触达，不等于证明每一个潜在错误都能被测试发现。

### C3：重新计算本轮保留的 10 屏对照

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/ui-compare-review --reuse --scenes BatteryLow BatterySaving Charging PowerOffEmpty Lock LockPage PowerMenu PowerOff PhoneIncoming ClockRing
```

### C4：复现自定义锁屏时长被覆盖

只把原测试的 `low()` 场景换成“设 120 秒 → 实际触摸开、关 → 查回原值”，经标准输入编译、链接原产品库；不写测试源码、不改产品。预期退出 1，唯一失败为 `expected 120 got 600`。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
import json, os, re, shlex, subprocess
r = Path.cwd(); b = r / 'build'; w = b / 'test-out/power-pages-review'
w.mkdir(parents=True, exist_ok=True)
s = (r / 'tests/power_pages.c').read_text()
low = r'''static void low(void) {
    CHECK(BY_HAND("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":120}") && g_rc.status == INK_RC_DONE, "set custom two-minute timeout");
    battery(5, false); CHECK(WAIT_FOR(page("BatteryLow"), 3000), "show low battery"); quiet();
    tap(INK_CANVAS_W-INK_MARGIN-INK_CONTROL/2,INK_CANVAS_H-INK_MARGIN-INK_PRIMARY-INK_GAP_L-INK_CONTROL/2);
    CHECK(WAIT_FOR(ink_setting_get("power.lock_after_s")==60,2000), "enable one-minute saving"); quiet();
    tap(INK_CANVAS_W-INK_MARGIN-INK_CONTROL/2,INK_CANVAS_H-INK_MARGIN-INK_PRIMARY-INK_GAP_L-INK_CONTROL/2);
    CHECK(WAIT_FOR(ink_setting_get("power.lock_after_s")!=60,2000), "disable one-minute saving");
    CHECK(ink_setting_get("power.lock_after_s")==120, "custom timeout lost: expected 120 got %lld",(long long)ink_setting_get("power.lock_after_s"));
    ink_probe_count("test.finished");
}
'''
s = s[:s.index('static void low(void) {')] + low + s[s.index('static void charging(void) {'):]
f = (b / 'CMakeFiles/sim-power-pages.dir/flags.make').read_text()
a = [v for n in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS')
     for v in shlex.split(re.search(rf'^{n} = (.*)$', f, re.M)[1])]
link = shlex.split((b / 'CMakeFiles/sim-power-pages.dir/link.txt').read_text())
obj = w / 'custom-timeout.o'; exe = w / 'custom-timeout'
subprocess.run([link[0], *a, '-I', str(r / 'tests'), '-x', 'c', '-c', '-', '-o', str(obj)],
               input=s, text=True, check=True, cwd=b)
link = [str(obj) if x == 'CMakeFiles/sim-power-pages.dir/tests/power_pages.c.o' else x for x in link]
link[link.index('-o') + 1] = str(exe)
subprocess.run(link, check=True, cwd=b)
out = w / 'custom-timeout-BatteryLow'
result = subprocess.run([str(exe), '--profile', str(r / 'simulator/profiles/metalio-plus.json'),
    '--script', str(r / 'tests/power_pages.script'), '--budget', str(r / 'tests/ui_scene.budget.json'),
    '--assets', str(b / 'assets'), '--content', str(b / 'test-content'), '--out', str(out),
    '--quiet', '--no-random-outages'], env=dict(os.environ, INK_POWER_SCENE='BatteryLow'),
    capture_output=True, text=True)
m = json.loads((out / 'metrics.json').read_text())
print('exit=', result.returncode, 'failures=', m['metrics'].get('test.fail', 0), m.get('warnings'))
raise SystemExit(result.returncode)
PY
```

SEVERE=0 HIGH=0
