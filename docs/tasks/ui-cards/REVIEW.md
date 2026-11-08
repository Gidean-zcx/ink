# ui-cards 独立复查（第 1 轮／唯一一轮）

日期：2026-11-09。基线：`1bbf1a49fb8d79d513d6260820fb164a89dedef1`；被查版本：`3cd1d134aafbf6874d63660f464930990e885bc7`。

**结论：暂不通过完整验收。严重 0、高 0、中 2。** 成员过滤、单卡同源和触摸／AI 共用能力均可复现，六项反向对照有效；但卡片事件仍造成未节流的状态栏渲染，合法多卡回执也会令 Today 显示假空态。全量有一项文档失败、三项依赖缺失跳过，开发报告已如实披露，不计为伪称完成。

按 `review-prompt.md`、`codex-rules.md` 和任务书复查。仓库正式文件仅写本报告，不改产品、测试、开发报告或共享文档，不提交。构建、截图和隔离探针仅落在已忽略的 `build/`、`build-asan/`、`work/ui-cards-review/`。没有操作设备、付费接口或远端；命令由本轮独立执行。

## 发现

### R1｜中｜卡片事件绕过节流，额外重建状态栏并提交显示

- **位置**：[navigation.c:1498](../../../system/ui/shell/navigation.c#L1498)～1500；其调用的 `refresh()` 在 [1475](../../../system/ui/shell/navigation.c#L1475)～1481 无条件删除、重建状态栏。现有 [ui_cards.c:143](../../../tests/ui_cards.c#L143)～166 只统计 `home.cards_updates`。
- **触发**：Today 被控制中心盖住时，提供者正常调用 `ink_app_card_changed()`；或者可见时连续发布卡片变更。不需要阻塞线程或异常输入。
- **结果【实测】**：下表记录 LVGL 的 `LV_EVENT_REFR_READY` 和 `ink_display_commits()`，二者本轮数值相同。覆盖态四次变化仍有四次重绘／提交；原测试所谓“0 次更新”只表示没有重读卡片。可见态十次变化合并为一次卡片重读，却有十二次重绘／提交。

| 场景 | 卡片重读 | LVGL 重绘／显示提交 |
| --- | ---: | ---: |
| 控制中心覆盖，4 次不改变数据的对照调用 | 0 | 0 |
| 控制中心覆盖，4 次卡片变更 | 0 | 4 |
| 可见，翻回 Today 后 10 次不改变数据的对照调用 | 0 | 1 |
| 可见，同样翻页后 10 次卡片变更 | 1 | 12 |

- **因果对照【实测】**：仅在隔离副本中将卡片事件的 UI 作业改为调用已有 `ink_shell_home_settle()`，覆盖态重绘／提交降为 0，可见态降为 2（含翻页），卡片重读仍为 1。整个探针的模拟 DU 波形次数从 7 降到 2。未改工作区源码；上述计数不等于实机光学刷新或功耗测量。
- **影响与定级**：卡片重读有节流，额外状态栏渲染没有节流，造成无用 UI 和显示工作；测试不能据此声称“不提前重画”。未观察到丢数据、越权或卡死，按中记录。
- **复现命令**：运行下文“探针重建命令”，查看 `render-exact-out/metrics.json` 的 `review.covered_changed.*`、`review.visible_changed.*`。本轮结果在 `work/ui-cards-review/render-results.json`；编译、链接和运行命令在同目录 `render-exact-*.log`、`render-isolate-*.log`。
- **最小修法**：卡片事件只调度已有 settle 路径，不调用通用状态栏 `refresh()`；补实际渲染／显示提交断言。无需新增任务、缓存或重试。

### R2｜中｜合法多卡回执超出解析 token 上限，真实数据被显示为空

- **位置**：[navigation.c:270](../../../system/ui/shell/navigation.c#L270)～271、[292](../../../system/ui/shell/navigation.c#L292)～305、[659](../../../system/ui/shell/navigation.c#L659)。生产端 [app_registry.c:454](../../../system/ui/app_registry.c#L454) 允许每个应用 8 张卡；[json.h:17](../../../system/include/ink/sys/json.h#L17) 的解析上限为 128 tokens。
- **触发**：测试听书提供者返回同成员 8 张短卡，每张含 `action`、`value`；另保留一张经真实时间服务添加的闹钟卡。字段、长度、提供者数量均合法，未改产品或预算。
- **结果【实测】**：7 张听书卡加闹钟，`app.cards` 返回 1,004 字节，Today 快照 935 字节，12 项检查通过。增加至 8 张加闹钟，能力仍成功返回完整 1,135 字节，但整数组共 132 tokens，Today 解析失败；听书和闹钟均画成“还没有…”，快照返回 `-1`。新增检查失败 2 项，程序退出 1。已查看 `count-8-alarm-out/clean-card-count.png`，确认是假空态；回执远未达到既有 2 KiB 上限。
- **影响与定级**：AI 直接读 `app.cards` 有数据，Today 和 AI 页面快照却丢失全部卡片，验收 1 不能无条件通过。当前内置应用尚不自然产生八张听书卡，因此按中记录，不提升为当前正常使用中的高问题。
- **复现命令**：运行下文“探针重建命令”，比较 `count-7-alarm-out/metrics.json` 与 `count-8-alarm-out/metrics.json`。原始结果在 `work/ui-cards-review/count-alarm-results.json`。
- **最小修法**：在 `cards_reply()` 一处按数组元素逐卡解析，再沿用现有五卡选择，使总卡数不受单次 128-token 解析限制；不要抬高全局 JSON 预算、删回执中的卡片或另造数据源。解析失败应明确失败，不能渲染为“没有数据”。

## 逐项验收

| 编号 | 本轮结论 | 独立证据 |
| --- | --- | --- |
| 1 今日页和 AI 同源 | 部分通过 | 原用例比对标题、文本、版本、能力、LVGL 标签和生产规格的 3,072 字节 AI 上下文，104 项通过；多卡反例见 R2。 |
| 2 换成员隔离及反例 | 通过给定场景 | 成人→孩子→成人正常；去成员条件失败 7 项，去应用可见条件失败 1 项。 |
| 3 不可见不更新、可见节流 | 部分通过 | Main、覆盖层和另一活动下卡片重读 0 次，十次变更重读 1 次，解除覆盖补 1 次，无变更无卡片心跳；实际渲染另有 R1。 |
| 4 点击和 AI 同一打开能力 | 给定无参卡片通过 | 真实触摸和 AI 均调用 `test.cards_open`，各打开同一实际测试页面一次；改触摸动作后失败 3 项。这不代表听书等业务应用已经开发完成。 |
| 5 全量、内存、250 行预算 | 未全部满足 | 两种构建成功；普通、ASan/UBSan 各 204 通过、1 失败、3 跳过；产品增删 173 行，预算通过。 |

### 独立构建、全量与内存检查

使用用户指定的 `INK_DEPS`、`INK_DESIGN`。核查两个 `CMakeCache.txt`：源码目录正确，字体、Python、LVGL 指向指定依赖；均 `INK_WERROR=ON`，仅 ASan 构建为 `INK_SANITIZE=ON`。顺序执行下列命令，构建最多三路、CTest 最多两路：

```sh
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】构建退出均 0；CTest 均退出 8，普通耗时 88.47 秒，ASan/UBSan 耗时 186.69 秒。

- 唯一失败 `capability-table-doc`：生成表的 `system/ui/shell/navigation` 状态引用比共享文档多 `app.cards_rev`。总规则 §7 禁止任务分支改此共享表，留待合并会话更新；没有规避检查。
- 跳过 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`：锁定的中枢 Python 依赖／生成消息不可用。未安装或升级依赖，不把跳过计为通过。
- 两份 `Testing/Temporary/LastTest.log` 无 ASan、LeakSanitizer 错误或 UBSan `runtime error`。F1、F2、Today 普通／大字号／儿童场景及阅读确定性均通过。
- 两份 `ui-cards/metrics.json` 均为 `test.pass=104`、`test.finished=1`、`test.cards_burst_updates=1`，无失败／警告；`mem.internal_total.min_free_bytes=47840`，内外部内存分配失败均 0，原门槛未变。
- 未运行 Linux/gcc、实机或交互窗口；全量 `gui-smoke` 沿用 SDL dummy。

### 独立反向对照

用本轮构建的编译参数和链接命令生成隔离对象，链接既有 `sim-ui-cards` 测试对象；没有执行开发者会原地改源码的临时脚本。六例编译／链接均退出 0，模拟器均退出 1、`test.finished=1`，原断言和预算不变。

| 临时撤掉一处逻辑 | 失败检查数 |
| --- | ---: |
| 成员相等过滤 | 7 |
| 应用可见过滤 | 1 |
| 顶层页面可见条件 | 1 |
| 一秒节流 | 2 |
| 小卡同源标签，改为错误文案 | 12 |
| 触摸声明动作，改成 reader 槽位 | 3 |

结果在 `work/ui-cards-review/results.json`，每例的 `negative-*-compile.log`、`*-link.log`、`*-run.log` 记录完整命令与输出。原工作区文件未临时改写。

## 十屏视觉复核

原检出没有 `out/ui-compare/`。本轮使用刚重跑的场景截图，通过既有 `tools/ui_compare.py --reuse` 在 `work/ui-cards-review/out/ui-compare/` 生成设计／实现／差分图，逐张打开以下十屏；另看了多卡失败截图。工具检查了场景完成、无失败及稳定截图标记。

| 页面 | 1/4 灰度 MAE | 人工观察到的明显差异 |
| --- | ---: | --- |
| Today | 48.401 | 卡片更高，字号更大，小卡空态为两行粗体；没有稿中的书名／小字进度／进度条。这里只能展示提供者的真实数据，不能填设计稿样例冒充已实现。 |
| Main | 54.194 | 时钟更大，网格更靠上；继续读为真实空态，Dock 更深、更高。 |
| Apps | 42.961 | 网格和搜索框更大，工具区分两页；稿中同屏的网页／记录／设置不在第一页。 |
| Control | 39.888 | 滑杆、按钮更高；网络信息和两张通知卡变为空态，底部多了“收起”。 |
| Notifications | 25.302 | 一条通知加“全部已读”和文字翻页按钮；稿中三张通知及回复／回拨操作未呈现。 |
| Members | 24.948 | 测试有两位家人，稿中三位；头像位置更低，没有稿底部“加一位家人”。 |
| MembersBack | 21.235 | 主头像尺寸不同；“回到刚才的地方”取代书名，“换一位家人”及孩子入口位置更高。 |
| Settings | 81.373 | 分组更高、灰更深，没有稿中右上角搜索及组内浅分隔线；状态值来自测试环境。 |
| SetDisplay | 47.923 | 字体示例区更矮，刷新／锁屏为两个描边胶囊，稿中是带说明文字的灰色分组。 |
| PowerOff | 18.177 | 实现突出“已关机”，书名页码在其下；稿中以书名为主体，关机提示在底部。 |

**归属与边界**：十屏 MAE 都超过工具目标 12，不能直接记为本轮十个新缺陷。`system-ui-spec.md` §1 明确设计导出未同步 D-107 的 1.2 倍尺寸和新灰度，部分差异符合现行规范；测试样本和设计稿内容也不同。对照基线，F5 未改这些布局尺寸或整体样式。未发现本轮新增错字、词表违规或文字重叠。既有业务／布局缺口留给原页面任务，不要求 F5 重画。

生成命令（只生成忽略目录，复用本轮全量场景）：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PYVIS'
from pathlib import Path
import os, subprocess, sys
root = Path.cwd()
out = root / 'work/ui-cards-review/out/ui-compare'
out.mkdir(parents=True, exist_ok=True)
scenes = 'Main Today Apps Control Notifications Members MembersBack Settings SetDisplay PowerOff'.split()
for scene in scenes:
    target = out / scene
    if not target.exists():
        target.symlink_to(root / 'build/test-out' / ('ui-scene-' + scene), target_is_directory=True)
subprocess.run([sys.executable, '-B', 'tools/ui_compare.py', '--design', os.environ['INK_DESIGN'],
                '--out', str(out), '--reuse', '--scenes', *scenes], check=True)
PYVIS
```

## 品味、规模与总规则审视

【实测】基线差异中，产品及 CMake 新增 148、删除 25，共 173 行，小于 250。未动 F1/F2 实现、确认持久化／无线传输文件、受保护资料或共享文档。`git diff --check` 通过；基线至 HEAD 的本机路径／MAC 正则扫描退出 1，无匹配。

【实现】F1 页面模型、五个固定卡位、可选成员字段和一次性 LVGL 定时器均对应同源、隔离和节流要求；没有重做应用注册，未加常驻任务、线程、持久格式、后台重试或配置项。`N` 在 PSRAM，LVGL 调用在 UI 任务；打开动作经能力总线，没有 UI 直接改服务内部状态。关机书签的三行过滤属于复用新成员合同，报告已解释，未发现无关扩展。

**建议删掉 R1 的多余通用刷新路径**，而非新增调度机制。其余新增机制与 F5 相称，无需为本轮引入通用卡片框架。R2 在回执消费处修，不逐个提供者打补丁。

已有 Today 断言改为五卡 F1 字段和空动作数组，页面身份检查保留；没有删测试或放宽预算。新测试的 `delay_ms` 为虚拟时间观察窗口，未见产品加 sleep 掩盖时序，但渲染计数覆盖需按 R1 修正。阅读提供者离页后没有卡片是基线问题，报告已披露，不记为本轮新发现。

## 探针重建命令

下面代码随报告保留。先运行普通构建，再从仓库根目录执行提取命令。它只在 `work/ui-cards-review/` 生成临时测试源码、对象、程序、日志和截图，原测试及产品源码不变。三个探针顺序运行，R2 的八卡用例预期退出 1。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PYRUN'
from pathlib import Path
report = Path('docs/tasks/ui-cards/REVIEW.md').read_text()
code = report.split('<!-- UI_CARDS_REVIEW_PROBE -->\n```python\n', 1)[1].split('\n```', 1)[0]
exec(compile(code, '<ui-cards-review-probe>', 'exec'))
PYRUN
```

<!-- UI_CARDS_REVIEW_PROBE -->
```python
from pathlib import Path
import json, re, shlex, subprocess
root = Path.cwd()
build = root / 'build'
out = root / 'work/ui-cards-review'
out.mkdir(parents=True, exist_ok=True)
link = shlex.split((build / 'CMakeFiles/sim-ui-cards.dir/link.txt').read_text())
make = (build / 'CMakeFiles/sim-ui-cards.dir/flags.make').read_text()
flags = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    flags += shlex.split(re.search(r'^' + key + r' = (.*)$', make, re.M).group(1))
original = (root / 'tests/ui_cards.c').read_text()
a = original.index('static void run(void *arg) {')
b = original.index('void ink_main(void) {', a)
def run(name, source):
    src = out / (name + '.c')
    obj = src.with_suffix('.o')
    exe = out / name
    src.write_text(source)
    cmd = [link[0], *flags, '-I' + str(root / 'tests'), '-c', str(src), '-o', str(obj)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    (out / (name + '-compile.log')).write_text(shlex.join(cmd) + '\n' + r.stdout + r.stderr)
    assert r.returncode == 0, r.stderr
    cmd = link.copy()
    cmd[cmd.index('-o') + 1] = str(exe)
    cmd[cmd.index('CMakeFiles/sim-ui-cards.dir/tests/ui_cards.c.o')] = str(obj)
    r = subprocess.run(cmd, cwd=build, capture_output=True, text=True)
    (out / (name + '-link.log')).write_text(shlex.join(cmd) + '\n' + r.stdout + r.stderr)
    assert r.returncode == 0, r.stderr
    cmd = [str(exe), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
           '--script', str(root / 'tests/ui_cards.script'), '--budget', str(root / 'tests/ui_scene.budget.json'),
           '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
           '--out', str(out / (name + '-out')), '--quiet', '--no-random-outages']
    r = subprocess.run(cmd, capture_output=True, text=True)
    (out / (name + '-run.log')).write_text(shlex.join(cmd) + '\n' + r.stdout + r.stderr)
    d = json.loads((out / (name + '-out/metrics.json')).read_text())
    print(json.dumps({'case': name, 'exit': r.returncode,
        'metrics': {k: v for k, v in d['metrics'].items() if k.startswith(('review.', 'test.'))},
        'warnings': d.get('warnings', [])}, ensure_ascii=False), flush=True)
render = r'''
static unsigned review_renders;
static void review_rendered(lv_event_t *e) { (void)e; review_renders++; }
static void review_watch(void) { lv_display_add_event_cb(lv_display_get_default(), review_rendered, LV_EVENT_REFR_READY, NULL); }
static void review_noop(void) {}
static void review_measure(const char *name, bool change_it, int count, int gap) {
    unsigned rendered=review_renders, commits=ink_display_commits();
    double reads=sim_metric_value("home.cards_updates");
    for (int i=0;i<count;i++) { on_ui(change_it ? change : review_noop); delay_ms(gap); }
    delay_ms(1100); quiet();
    char key[96];
    snprintf(key,sizeof key,"review.%s.reads",name); sim_metric_set(key,sim_metric_value("home.cards_updates")-reads);
    snprintf(key,sizeof key,"review.%s.renders",name); sim_metric_set(key,review_renders-rendered);
    snprintf(key,sizeof key,"review.%s.commits",name); sim_metric_set(key,ink_display_commits()-commits);
}
static void run(void *arg) {
    (void)arg; delay_ms(1800); quiet(); on_ui(review_watch); quiet();
    review_measure("hidden_control",false,4,40);
    review_measure("hidden_changed",true,4,40);
    CHECK(BY_HAND("home.prev",NULL),"open Today"); quiet();
    on_ui(fresh_today);
    review_measure("visible_control",false,10,20);
    on_ui(fresh_today);
    review_measure("visible_changed",true,10,20);
    CHECK(BY_HAND("shell.control",NULL),"cover Today"); quiet();
    review_measure("covered_control",false,4,40);
    review_measure("covered_changed",true,4,40);
    ink_probe_count("test.finished");
}
'''
run('render-exact', original[:a] + render + original[b:])
count_probe = r'''
static void review_read(void) {
    ink_call_opts_t opts = {.src=INK_SRC_AI,.reply_exec=INK_EXEC_DIRECT,.done=cards_done};
    CHECK(ink_cap_call("app.cards",NULL,&opts)==0,"AI card capability succeeds");
    int count=0;
    for (const char *p=cards;(p=strstr(p,"\"app\":\"listen\""));p++) count++;
    sim_metric_set("review.source_cards",count);
    sim_metric_set("review.source_bytes",strlen(cards));
    CHECK(count==REVIEW_CARD_COUNT,"app.cards contains every bounded synthetic card");
    int n=ink_ui_snapshot(view,sizeof view,NULL);
    sim_metric_set("review.snapshot_bytes",n);
    CHECK(n>0 && strstr(view,"成人合成听书1"),"Today must retain the first card from the successful source");
    CHECK(find_label(lv_screen_active(),"成人合成听书1"),"the visible first card must not become a false empty state");
}
static void run(void *arg) {
    (void)arg; delay_ms(1800); quiet();
    on_ui(add_alarm);
    CHECK(BY_HAND("home.prev",NULL),"open Today"); quiet(); on_ui(review_read);
    capture("card-count"); ink_probe_count("test.finished");
}
'''
s = original[:a] + count_probe + original[b:]
s = s.replace('if (index > 1) return 0;', 'if (index >= REVIEW_CARD_COUNT) return 0;')
s = s.replace('index ? "kid" : "owner"', '"owner"').replace('index ? "孩子" : "成人"', '"成人"')
s = s.replace('"example-listen");', '"example-%d", index);')
s = s.replace('if (index) return 0;', 'if (index >= 0) return 0;')
for n in (7, 8):
    run(f'count-{n}-alarm', f'#define REVIEW_CARD_COUNT {n}\n' + s)
```

SEVERE=0 HIGH=0
