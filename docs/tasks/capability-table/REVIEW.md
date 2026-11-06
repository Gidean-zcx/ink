# capability-table 独立复查

复查日期：2026-11-06。开发基线：`1c43c28a`；被查提交：`667e1963cf4d76f91d47cf835944c62e3c539cbe`。依据为 `review-prompt.md`、`backlog.md` 开头及 capability-table 一节、`codex-rules.md`。

**结论：本次复查通过，严重 0、高 0。上一轮高问题 R1 已修到位。** 普通及 ASan/UBSan 全量均为 102/102；旧反例、撤回修复的反向对照和补充实测支持这一结论。未发现需要新增的严重或高问题，不为数量列中低级建议。

工作区旧报告为空，上一轮内容从 `7c393cde:docs/tasks/capability-table/REVIEW.md` 读取。本轮仅修改本报告，不修改产品代码、原有测试或预算，不提交、不推送。构建与全量测试逐轮串行，构建最多 `-j4`、CTest 最多 `-j3`；未启动带窗口的模拟器。

## 逐条验收

| 编号 | 结论 | 本轮独立证据与复核入口 |
| --- | --- | --- |
| C1 从代码生成能力总表 | 通过 | 【实测／实现】198 个定义：115 个调用、27 个事件、14 个设置、42 个状态。所需列、源码链接及关联面齐全；生成检查与提交文件一致。关联面明确是服务上下文，不是每次调用的完整数据流。见 V1。 |
| C2 CTest 校验一致性、描述和风险 | 通过 | 【实测】6 项 capability-table CTest 均通过，含 13 项 Python 回归、142 项运行时元数据及 3976 个外层确认组合；82 个运行时负对照、4 个错误文字负对照均被拒绝。旧生成器不能通过新检查，见 V2。 |
| C3 每个设置项都有能力 | 通过 | 【实现】14 项经 `settings.get/set` 按 id 查找，读取、写入和权限导出逐项核对。【实测】另按总表预期完成全部设置在成人/儿童、AI/触摸下的 112 次真实读写；敏感项等待人工确认，儿童禁写项被拒绝。见补充实测。 |
| C4 用户操作缺口、小改或记录遗留 | 通过任务允许的遗留处理 | 【实现】复核报告列出的返回、锁屏/唤醒、AI 会话、键盘、文本选区、电源菜单、远程点击和占位主页入口；既有能力但按钮绕行也单独记录。没有新增产品入口，也没有把缺口写成完成；相关页面/会话/敏感输入边界确实需要一起处理。 |
| G1 全量及内存检查、预算 | 通过 | 【实测】普通 102/102，42.25 秒；ASan/UBSan 102/102，77.30 秒，构建与测试均退出 0。CTest 无跳过；完整测试日志核查未发现非预期 ASan/UBSan 诊断。未改原有预算、断言、时限或跳过规则。见 V1。 |
| G2 原子能力、按钮/AI 共用入口、页面快照 | 本次符合；基线缺口未消除 | 【实现】没有新增用户操作或页面；生成工具和测试未绕过产品分层。报告仍明确说明 R-061 的既有入口缺口，没有声称系统已经全面共用能力路径。 |
| G3 更新依赖状态 | 通过 | 【实现】`docs/system/ui-dependencies.md:101` 只改对应状态单元格，保留“部分”及原判断。 |
| G4 页面和视觉验收 | 不适用 | 没有界面改动，故不做十屏对照。两轮全量中的 `gui-smoke` 均由既有脚本强制使用 SDL dummy，没有窗口。 |
| G5 范围与总规则 §1、§4 | 通过 | 【实现／实测】相对基线的 12 个文件（含旧复查报告）均属于生成工具、测试、CTest 登记及本任务文档；产品源码、受保护资料和设计合同未改。未碰设备、真实数据、付费接口或远端写入。 |
| 差异、隐私及链接 | 通过 | 【实测】`git diff --check 1c43c28a..HEAD` 退出 0；规定隐私正则 0 匹配；208 个本地文件链接全部存在；变更文件均小于 5 MB。新增 C 导出检查输出、关闭返回值，并在 `vtk_host_call` 内执行；没有新增 `write/read/fgets` 或固定 sleep。 |

## 上一轮 R1：高，已关闭

上一轮将三个相关症状合为一条高问题，本轮继续按一条跟踪，不计入末尾未解决数量。

| 原问题 | 修复位置及本轮结果 |
| --- | --- |
| 把 child_ok 误写成儿童仍须白名单 | `tools/capabilities.py:225` 按实际成员闸门区分直接放行、白名单额外放行和 system 限制；`tests/capability_table/policy.c:87` 起使用真实总线验证。旧 P1 原样执行仍得到：无白名单的儿童改音量到 35、列通知均成功；现在总表与结果一致。 |
| 把设置撤销一律写成无需确认 | `tools/capabilities.py:198` 明确设置撤销重新检查设置项；`tests/capability_table/policy.c:118` 覆盖普通设置、闹钟和权限撤销。旧 P1 中，成人亲自开启并关闭剪贴板权限后，AI 撤销返回 `pending_confirmation`，权限仍关闭；新表述正确。 |
| 未导出、比较设置的 child_may_change | `tests/capability_table/export.c:70` 导出全部 14 项的三个权限布尔字段；`tools/capabilities.py:339` 严格比较类型和值。旧 P2 只把内存中的 `audio.child_max.child_may_change` 改为 true，现在报 `runtime setting metadata differs: audio.child_max`。全部设置的权限翻转、缺字段和错误类型负对照被拒绝。 |

【实测】仅在临时模块中撤回生成器修复，不改工作区源码：13 项回归出现 **112 个失败子项、0 个异常错误**；运行时检查因接受错误的 `power.lock_after_s.child_may_change` 而失败，真实策略检查因 `settings.get` 的旧白名单文字而失败。恢复当前模块后 13/13 通过。该反向对照证明新增测试能发现原问题，未通过改产品权限或放宽断言迁就文档。

## 补充实测与证据边界

- 【实测】按提交的总表解析预期，临时编译测试入口并链接已有产品库。全部 14 项设置，在成人/儿童、AI/触摸四种组合下分别读取并回写当前值，共 **112 次真实总线调用**。逐次核对回执、对象、设置值及确认状态；等待确认的请求由模拟人工撤回。该检查验证覆盖和权限，不冒充每个设置的持久化或硬件效果测试；改变实际值的情况另由现有策略测试及旧 P1 覆盖。
- 【实测】按总表解释独立推导预期，调用真实 `gate`：115 项调用 × 成人/儿童 × 触摸/按键/AI × asked=false/true，共 **1380 个组合**均一致。环境为主页、未设 PIN、成员空间开放；书页专属能力被正确拒绝。只测闸门，不执行删除、关机等动作，也不把它替代完整调用测试。PIN 分支另由运行时导出的 C `confirm_for` 组合核对。
- 【实测】预放一份旧 `capability-policy.json`，将策略测试终点缩为 1 ms；旧文件仍在，但程序以 1 退出并报 `BUDGET FAIL test.finished: metric not recorded`。结合 `CMakeLists.txt:414`、`:418` 的 fixture 依赖，未完成的导出不能借旧 JSON 判为通过。
- 【实现】模块划分未被改变；能力、设置、状态、事件分开生成，人工 PIN/确认入口仍不向 AI 开放。报告的“没有能力”与“有能力但按钮绕行”两类缺口均真实存在，任务通过不表示 R-061 的产品级闭环已全部完成。
- 本轮在 macOS clang、Plus 模拟器及合成中枢环境验证；未运行 Linux gcc 或真机。临时探针单进程编译，退出后清理，不在仓库新增测试文件。

## 可复核命令

V1：核对现有 `build` / `build-asan` 的缓存后使用它们；均为 `INK_WERROR=ON`，后者另启用 `INK_SANITIZE=ON`，字体、Python、LVGL 来自用户指定的 `INK_DEPS`。以下四条逐轮串行执行：

```sh
cmake --build build -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
python3 -B tools/capabilities.py --check --runtime build/test-out/capability-table/capability-inventory.json
python3 -B tests/capability_table/test_catalogue.py
python3 -B tests/capability_table/check_runtime.py build/test-out/capability-table/capability-inventory.json
python3 -B tests/capability_table/check_policy.py build/test-out/capability-policy/capability-policy.json
```

V2：旧生成器与新增回归的反向对照，只写临时文件；V1 完成后在仓库根目录运行。

```sh
python3 -B - <<'PY_REVERSE'
import contextlib, importlib.util, io, runpy, subprocess, sys, tempfile, unittest
from pathlib import Path
root = Path.cwd()
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'before.py'
    path.write_bytes(subprocess.check_output(['git', 'show', '7c393cde:tools/capabilities.py']))
    spec = importlib.util.spec_from_file_location('capabilities', path)
    old = importlib.util.module_from_spec(spec)
    sys.modules['capabilities'] = old
    spec.loader.exec_module(old)
    old.ROOT = root
    ns = runpy.run_path('tests/capability_table/test_catalogue.py', run_name='reverse_tests')
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ns['CatalogueTest'])
    result = unittest.TextTestRunner(stream=io.StringIO()).run(suite)
    print('tests / failed subcases / errors:', result.testsRun, len(result.failures), len(result.errors))
    assert not result.wasSuccessful() and not result.errors
    for script, artifact in (
        ('check_runtime.py', 'capability-table/capability-inventory.json'),
        ('check_policy.py', 'capability-policy/capability-policy.json'),
    ):
        sys.argv = [script, str(root / 'build/test-out' / artifact)]
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                runpy.run_path(str(root / 'tests/capability_table' / script), run_name='__main__')
        except (ValueError, SystemExit) as exc:
            print(script, type(exc).__name__, str(exc))
        else:
            raise AssertionError('old generator unexpectedly accepted: ' + script)
PY_REVERSE
```

V3：上一轮原始 P1、P2 的完整命令保留在 `git show 7c393cde:docs/tasks/capability-table/REVIEW.md` 的 `P1_BEGIN`、`P2_BEGIN` 标记之间。本轮原样执行 P1，退出 0，输出：

```text
REVIEW kid=0 listed=0 allows=1 setting=0 volume=35 notify=0 adult=0 on=1 off=1 undo=3 after=0
```

其中 `0=done`、`3=pending_confirmation`；P2 的错误设置权限现在必须被拒绝，不能再得到成功的运行时匹配结果。

SEVERE=0 HIGH=0
