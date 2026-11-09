# calendar 第 1 轮复查

复查日期：2026-11-09。基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`；被查版本：`1db6dc69b666c31b0e924ec9550be24c9be3841c`。

**结论：尚未达到完整验收。严重 0、高 0、中 3，其中 2 项为本轮新增发现，1 项为开发报告已披露的集成阻塞。** 核心排序、翻月选日、成员隔离及 AI 接线可以复现；全量仍有 3 项失败。

本轮只修改本报告，不改产品源码、仓库测试、共享文档，不提交。执行产物留在忽略的 `build/`、`build-asan/`；反例通过编译器 stdin 生成临时对象，没有改写原文件。

## 验收核对

| 验收 | 结论 | 本轮独立证据 |
| --- | --- | --- |
| 1：接下来按时间列出闹钟、提醒 | 通过（任务允许的两种来源） | C1 两个场景各 221 项断言通过；混合排序、今天及后续日期、关闭项过滤、过期项过滤、F4 翻页均覆盖 |
| 2：月历翻月、选日查看 | 基本功能通过，有中级体验问题 | C1 覆盖触控和 AI、跨年、非法日期、当天列表分页、2026 年 8 月六行月历及重复闹钟；新增问题见 CAL-R1-01、02 |
| 3：成员隔离及反例 | 通过 | C2 正常版本不串数据；仅撤掉时间服务成员过滤后，变体退出 1，213 项通过、6 项失败，失败确实涉及孩子列表出现大人闹钟及被挤出当前页的孩子提醒 |
| 4：两页 AI 快照、按钮与 AI 同能力 | 通过（本轮场景） | C1 使用真实能力总线、UI 任务及 3072 字节生产快照预算；C5 的长标题仍能读取完整语义。六个导航能力均在运行时表中，临时生成目录与运行时匹配 |
| 5：全量、内存检查、300 行预算 | 未通过 | 普通及 ASan/UBSan 构建均退出 0；两轮全量各 233 项，227 通过、3 失败、3 跳过，均退出 8；没有 sanitizer 报错。跳过项已另行补跑通过。含全部 CMake 接线的保守产品行数为 203，未超预算 |

日志：`build/calendar-review-{config,build,ctest}.log`，内存检查同名日志在 `build-asan/`。普通全量耗时 286.17 s，内存检查全量 422.25 s。专用场景内部 RAM 最少空闲 47,840 B、外部 RAM 13,044,544 B，分配失败均为 0；确定性相关测试通过。

没有独立日程后端，只展示闹钟、提醒，符合任务书允许的降级。月历不回放已删除／已结束提醒的历史；这一限制在开发报告中已说明，不能将当前页面当作历史日程存档。没有把模拟器证据当作实机或 Linux 验证。

## 问题

### CAL-R1-01（中，新增）：孩子和特大字号下翻月按钮丢字

- **文件与行号：** `system/apps/calendar/calendar.c:138–140`；现有检查在 `tests/calendar/calendar.c:39–40、95、103`。
- **复现命令：** C5；或切到孩子 → 打开接下来 → 看月历，成人则在设置中选“特大”后打开月历。
- **结果【实测】：** 110% 孩子字号与 120% 成人字号下，“上月／下月”只显示“上／下”。按钮宽 88 px，文字可用宽度为 72 px，而两档完整文字分别需 81、87 px。它们与事项翻页箭头紧贴，丢掉“月”后更难区分两种翻页。C5 对应两个宽度断言失败。
- **证据：** `build/calendar-review-probe/run/snap-ChildCalendar.png`、`snap-ChildSixRows.png`、`snap-LargeFontCalendar.png`。已有测试只在默认字号检查标签对象文本和边界，没有检查孩子页面是否完整画出了文字。
- **最小修法：** 把翻月控件移到标题右侧，复用现有 101 px 图标按钮，底部只留 F4 事项翻页；不缩小孩子的字号、不改公共主题。补一条两档字号下的实际渲染检查。

### CAL-R1-02（中，新增）：分钟变化让整张月历闪屏重建

- **文件与行号：** `system/apps/calendar/calendar.c:181–186`；整页刷新路径为 `system/ui/ui_task.c:457–476`，公共状态栏已有刷新逻辑，见 `system/ui/shell/navigation.c:1498–1512`。
- **复现命令：** C5，在没有事项到期的月历页等待真实 `time.minute` 通知；设置 `CALENDAR_REVIEW_NO_MINUTE=1` 再执行 C5 为对照。
- **结果【实测，模拟器】：** 页面 generation 增加 1；Trace 在 `review.minute.before` / `after` 之间记录一次覆盖原生面板 `1216×684` 的 GC16，约 617.465 ms。仅删除本页分钟订阅的临时变体，generation 增量为 0，只更新状态栏 `62×684`，DU 约 240.012 ms；对应断言通过，两个字号断言仍失败。未用卡住线程制造时序。
- **影响：** 普通分钟更新也重建整棵页面并闪屏，违背界面规范 §5“时钟跳分钟：数字那一小块”。页面内容版本同时失效。此处耗时是模拟器模型值。
- **证据：** `build/calendar-review-probe/run/{metrics,trace}.json`；对照为 `build/calendar-review-minute-control/run/`。
- **最小修法：** 删除本页的 `time.minute` 订阅。时间、时区及闹钟变更仍通过已有 `alarm.revision` 重读；状态栏沿用公共刷新。不新增定时器或缓存。

### CAL-R1-03（中，已披露）：三处集成清单未对齐，验收 5 仍失败

- **文件与行号：** `docs/system/capabilities.md:10`、`tests/integration/ai_surface.c:79–80、204`、`tests/integration/apps.c:146`。
- **复现命令：** C3；最小重跑为 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(capability-table-doc|integration-ai-surface|integration-apps)$'`。
- **结果【实测】：** 能力文档仍列 262 项，当前注册为 268 项；AI 白名单漏了六个 `calendar.*` 能力，并误把 `calendar.select` 当作无参数，产生 7 个断言失败；应用数量仍断言 11，实际为 12，产生 1 个失败。普通与 sanitizer 构建结果一致。
- **判断：** 是未完成的集成阻塞，不是 AI 实际无法调用，也不是隐瞒失败。总规则 §7 禁止本任务更新共享生成文档、任务书限制共享文件修改，开发报告已将其如实留给合并方。
- **最小修法：** 合并方只补六个能力的白名单（`calendar.select` 必填 `day`）、将应用数量改为 12，并重新生成能力文档；保留严格断言，重跑失败项和全量。

## 视觉复查

`out/ui-compare/` 原本不存在。本轮复用并重算有效场景，证据置于构建目录 `build/calendar-review-compare/`，未向源码目录加入图片。打开原设计两屏，查看了下列 15 个实际状态，其中 10 屏打开了逐屏设计对照图。后续日期、字体及合成事项不同的状态只检查布局，不把其像素差当作额外验收分数。

| 状态／截图名 | 查验结果 |
| --- | --- |
| Upcoming | 30.835/255；没有“明天／周五”日期分组，改成逐行日期与白底描边卡片；没有画板的来源和详情箭头 |
| Calendar | 36.627/255；月历及事项区整体更靠下，新增 F4 页码与翻月控件；底部“加提醒／说一句”入口未实现 |
| EmptyUpcoming | 空文案可见，没有伪造事项；仍占一整行显示 1/1 与不可用翻页箭头 |
| UpcomingPage2 | 单条家长会与 2/2 可见；空白较多，无裁切 |
| SelectedDay5 | 选中态、当天两条事项和 1/2 可见；排序正确 |
| Day5Page2 | 第三条事项与 2/2 可见，无遗漏 |
| EmptyCalendar | 空日期文案可见；没有画板的“今天”提示与事项圆点 |
| December | 2026 年 12 月网格、农历和空状态完整 |
| January | 2027 年 1 月跨年且六行网格未挤出屏幕 |
| SixRows | 2026 年 8 月 31 日可选，六行网格和底部控件在屏内 |
| ChildUpcoming | 只出现孩子合成提醒，110% 字号可读 |
| ChildCalendar | 提醒可读；翻月按钮丢“月”，见 CAL-R1-01 |
| ChildSixRows | 六行网格可见；同一翻月按钮问题 |
| LargeFontCalendar | 120% 字号仍能显示当天两条事项；同一翻月按钮问题 |
| LongTitles | 长标题按单行省略，AI 快照保留完整标题；未发现快照溢出 |

两项正式 MAE 均超过目标 12，但不是仅凭分数定缺陷：当前规范明确把原稿字号／控件放大，并调整灰度，这部分变化有依据。仍存在的日期分组、来源层次、节气、当天标记、事项圆点和底部入口差异已在开发报告说明；本轮没有把这些已披露的未完成项重复算作“虚假通过”。未发现新增错别字。

## 过度设计、边界与 AI 原生

- 没有新增常驻任务、状态机、队列、缓存、配置项或持久格式。两页复用 F4 和时间服务；六个导航能力与任务要求对应。`ink_alarm_occurrence` 的小接口复用已有重复规则，避免应用复制调度算法，边界合理。
- **建议删掉的只有 CAL-R1-02 的分钟订阅。** 公共状态栏已经负责这一事件，没有必要再让整页响应；无需新增刷新框架，也无需逐个调用方补丁。
- 产品源码没有直接写时间服务内部数据；触控、实体键、AI 通过同一能力路径。快照由现有框架检查成员身份，本轮正常切换及成员过滤反例有效。没有新增人工确认或密码代填入口。
- 基线差异共 9 个文件、453 行新增；新页面 187 行，加启动 2 行、头文件 3 行、服务 6 行及全部 CMake 5 行，共 203 行（包含空行及测试接线，保守计数）。没有修改已有测试期望、放宽预算或加等待掩盖失败。
- `git diff --check <基线>..HEAD` 通过；按总规则扫描提交差异，本机路径和 MAC 模式无匹配（`rg` 退出 1）。未发现任务外源码变更、设计图或构建产物被提交。当前本轮非忽略文件变更只有本报告。

## 复现命令与证据

环境使用题目指定的 `INK_DEPS`、`INK_DESIGN`。两套构建按总规则 §5 配置、保留 `INK_WERROR=ON`；ASan 只增加 `INK_SANITIZE=ON`。构建使用 `taskpolicy -c utility cmake --build <目录> -j2`，普通、内存检查依次运行。

C1（正例与正式截图，退出 0）：

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar/run.py --build build --visual
```

日志 `build/calendar-review-visual.log`，本轮原始证据 `build/calendar-jxwntn01/`。

C2（成员反例：runner 退出 0，变体按预期退出 1）：

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar/run.py --build build --negative-member
```

日志 `build/calendar-review-negative.log`，证据 `build/calendar-b0thkgwv/`。

C3（两轮全量均退出 8）：

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

跳过的三个中枢用例，两套构建各补跑一遍，均退出 0：125 个契约测试、JSON 128/129 token 正反例、一次本机假中枢语音与工具回路。以下以普通版为例，内存版把 `build` 换为 `build-asan`；没有更改依赖或测试规则。

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/hub/.venv/bin/python" tests/hub/run_contract.py --caps build/test-out/hub/caps.json --samples build/test-out/hub/messages.jsonl
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/hub/check_samples.py build/hub-json-check build/test-out/hub/messages.jsonl
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/hub/.venv/bin/python" tests/hub/e2e.py --sim build/sim-hub-e2e --parser build/hub-json-check --assets build/assets --content build/test-content --out build/calendar-review-hub-e2e
```

日志分别为两构建目录下 `calendar-review-hub-{contract,crosscheck,e2e}.log`。

C4（正式对照图，退出 0；目标分数未达到）：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/calendar-compare --scenes Upcoming Calendar --reuse
```

本轮另复制到 `build/calendar-review-compare/` 并执行同一工具，分数完全一致。其余对照图仅把 C5 的状态截图和所属页面原稿并排，未改变截图内容。

C5（以下脚本编译原有测试的内存扩展，输出 15 屏；原产品有 275 通过、3 失败；删除分钟订阅的对照有 276 通过、2 失败。模拟器均退出 1，失败分别对应 CAL-R1-01／02）：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
text = Path('docs/tasks/calendar/REVIEW.md').read_text()
block = text.rsplit('<!-- CALENDAR_REVIEW_PROBE -->', 1)[1]
code = block.split('```python', 1)[1].split('```', 1)[0]
exec(compile(code, '<calendar-review-probe>', 'exec'))
PY
```

对照执行同一命令前设置 `CALENDAR_REVIEW_NO_MINUTE=1`；不要把对照二进制当作已修复的产品。脚本只写忽略的构建产物：

<!-- CALENDAR_REVIEW_PROBE -->
```python
import json, os, shlex, subprocess
from pathlib import Path
root = Path.cwd()
build = root / "build"
no_minute = bool(os.environ.get("CALENDAR_REVIEW_NO_MINUTE"))
out = build / ("calendar-review-minute-control" if no_minute else "calendar-review-probe")
out.mkdir(exist_ok=True)
source = (root / "tests/calendar/calendar.c").read_text()
source = source.replace('if (strcmp(scene, name)) return;', '(void)scene;')
source = source.replace('sim_display_snapshot("scene");', 'sim_display_snapshot(name);')
shots = [
('visible("还没有接下来的事");', 'EmptyUpcoming'),
('expect("\\"page\\":2");', 'UpcomingPage2'),
('absent("今晚读书");\n    call_ok("calendar.next"', 'SelectedDay5'),
('expect("放学取快递");\n    call_ok("calendar.select"', 'Day5Page2'),
('visible("这一天没有提醒和闹钟");', 'EmptyCalendar'),
('press("calendar.month_next", 0); expect("2026年12月1日");', 'December'),
('call_ok("calendar.month_next", NULL); expect("2027年1月1日");', 'January'),
('visible("上月"); visible("下月");', 'SixRows'),
('quiet(); expect("孩子的合成提醒");', 'ChildUpcoming'),
('absent("带作业本");\n    CHECK(ink_member_switch(owner)', 'ChildCalendar'),
]
for needle, name in shots:
    if "\n" in needle:
        left, right = needle.split("\n", 1)
        replacement = left + f' capture("{name}");\n' + right
    else:
        replacement = needle + f' capture("{name}");'
    assert source.count(needle) == 1, (name, source.count(needle))
    source = source.replace(needle, replacement)
extra_functions = r'''
static void review_width(lv_obj_t *obj) {
    const char *cap = ink_shell_bound_cap(obj);
    if (cap && !strcmp(cap, "calendar.month_prev")) {
        lv_obj_t *label = lv_obj_get_child(obj, 0);
        int need = ink_ui_px(ink_ui_text_width("上月", INK_FONT_LIST, INK_WEIGHT_MEDIUM));
        int have = lv_obj_get_width(label);
        ink_probe_value("review.month_text_need_px", need);
        ink_probe_value("review.month_text_have_px", have);
        CHECK(need <= have, "REVIEW month button must show its complete label: need=%d have=%d", need, have);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) review_width(lv_obj_get_child(obj, (int32_t)i));
}
static bool review_minute_changed(void) { int64_t value = 0; return ink_state_get_int(ink_state_find("time.minute"), &value) && value != 361; }
static void review_ui(void) { lv_obj_update_layout(lv_screen_active()); review_width(lv_screen_active()); }
static char review_json[4096];
static void review_snapshot_ui(void) { int n = ink_ui_snapshot(review_json, sizeof review_json, NULL); CHECK(n > 0, "REVIEW expanded snapshot"); }
static void review_print_host(void *arg) { (void)arg; printf("REVIEW_SNAPSHOT %s\n", review_json); }
static void review_snapshot(void) { on_ui(review_snapshot_ui); vtk_host_call(review_print_host, NULL); }
'''
source = source.replace('static void run(void *arg)', extra_functions + '\nstatic void run(void *arg)')
source = source.replace('capture("ChildCalendar");', '''capture("ChildCalendar"); on_ui(review_ui);
    call_ok("calendar.month_prev", NULL); call_ok("calendar.month_prev", NULL); call_ok("calendar.month_prev", NULL);
    capture("ChildSixRows");''')
source = source.replace('    ink_db_flush_all();', '''
    call_ok("settings.set", "{\\"id\\":\\"display.font_size\\",\\"value\\":3}");
    call_ok("calendar.open", NULL); call_ok("calendar.select", "{\\"day\\":5}");
    capture("LargeFontCalendar"); on_ui(review_ui); review_snapshot();
    ink_time_set(local_time(2026, 11, 5, 6, 1) + 55, false); quiet();
    uint32_t generation = ink_ui_generation(); ink_probe_mark("review.minute.before");
    CHECK(WAIT_FOR(review_minute_changed(), 15000), "REVIEW wait for real minute boundary");
    quiet(); ink_probe_mark("review.minute.after"); ink_probe_value("review.minute_page_rebuilds", ink_ui_generation() - generation);
    CHECK(ink_ui_generation() == generation, "REVIEW minute-only change must not rebuild Calendar (delta=%u)", ink_ui_generation() - generation);
    call_ok("shell.back", NULL);
    for (int i = 0; i < 4; i++) add(INK_REMINDER, 5, 7, "合成测试记得带上学校的作业本", false);
    quiet(); on_ui(read_snapshot); capture("LongTitles"); review_snapshot();
    ink_db_flush_all();''')
flags = {}
for line in (build / "CMakeFiles/sim-calendar.dir/flags.make").read_text().splitlines():
    if " = " in line:
        k, v = line.split(" = ", 1); flags[k] = shlex.split(v)
link = shlex.split((build / "CMakeFiles/sim-calendar.dir/link.txt").read_text())
obj = out / "probe.o"
subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
               "-I", str(root / "tests/calendar"), "-x", "c", "-c", "-", "-o", str(obj)],
               input=source, text=True, cwd=build, check=True)
link[link.index("CMakeFiles/sim-calendar.dir/tests/calendar/calendar.c.o")] = str(obj)
binary = out / "sim-calendar-probe"
link[link.index("-o") + 1] = str(binary)
if no_minute:
    cal = (root / "system/apps/calendar/calendar.c").read_text()
    old = ' || ink_state_watch("time.minute", changed, NULL, INK_EXEC_UI)'
    assert cal.count(old) == 1
    pf = {}
    for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines():
        if " = " in line:
            k, v = line.split(" = ", 1); pf[k] = shlex.split(v)
    po = out / "calendar-without-minute.o"
    subprocess.run([link[0], *pf["C_DEFINES"], *pf["C_INCLUDES"], *pf["C_FLAGS"],
                   "-x", "c", "-c", "-", "-o", str(po)],
                   input=cal.replace(old, ""), text=True, cwd=build, check=True)
    link.insert(next(i for i, word in enumerate(link) if word.endswith(".a")), str(po))

subprocess.run(link, cwd=build, check=True)
result = subprocess.run([str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
    "--script", str(root / "tests/calendar/calendar.script"), "--budget", str(root / "tests/ui_scene.budget.json"),
    "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(out / "run"),
    "--quiet", "--no-random-outages", "--trace"], capture_output=True, text=True, timeout=150)
(out / "run.log").write_text(result.stdout + result.stderr)
data = json.loads((out / "run/metrics.json").read_text())
print("probe exit", result.returncode, {k:v for k,v in data["metrics"].items() if k.startswith(("test.", "review."))})
for warning in data.get("warnings", []):
    if "CHECK FAILED" in str(warning): print(warning)

raise SystemExit(result.returncode)
```

SEVERE=0 HIGH=0
