# update-pages 第 1 轮复查

结论：**暂不通过。严重 0，高 1，中 1。** 页面与升级服务的主链路已实现，但正常入口尚未接通，检查失败后的重试也指向了错误操作。

日期：2026-11-09。基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`；被查版本：`c4227291848bb085054a565527356fd9ab857009`。依赖和设计稿使用用户指定的 `$INK_DEPS`、`$INK_DESIGN`，仅运行 macOS 模拟器。只写本复查文件，未修改产品、测试源码或共享文档，未提交。

## 验收逐项核对

| 项目 | 独立结论 | 本轮证据 |
| --- | --- | --- |
| 1 新版本 → 下载 → 准备好 → 安装 → 完成 | **部分通过** | `flow`、`done` 实际通过；但测试先直调 `shell.update`，绕开用户现有入口，见 UP-R1-01。 |
| 2 电量/空间阻止、失败重试、启动恢复 | **部分通过** | 23% 电量与充电恢复、发布状态中的空间不足、坏签名重新下载、自检失败后下一次启动回滚均通过；检查失败的重试有缺口，见 UP-R1-02。空间测试是注入服务已发布状态，不是填满存储卡的端到端验证。 |
| 3 每页 AI 快照 | **通过本任务列出的场景** | 七页均有结构化快照；安装页没有动作，准备页不暴露 `system.upgrade`；AI 安装被拒绝，切到孩子后直接打开更新页被拒绝。七个静态场景使用夹具，另五个阶段验证真实模拟服务流程。 |
| 4 全量与内存检查、300 行预算 | **部分通过** | 普通及 ASan/UBSan 均 232/233，无跳过，唯一失败 `capability-table-doc`；未见 sanitizer 错误。新增产品 C 216 行，加 CMake 6 行，共 **222/300**，删除旧卡片 60 行不抵扣。 |
| G2 能力表 | 运行时一致；共享文档待合并更新 | 独立生成到构建目录：264 项能力、16 项设置与运行时一致，共检查 326 项定义。 |
| G4 视觉 | 七页均超 12/255，已核对差异 | 重新生成并查看十屏，见视觉表。主题放大和灰度调整符合现行规格，不应为降低分数改回旧规格。 |

## 问题

### UP-R1-01 · 高：已发现版本时，正常“系统更新”入口打不开新页面

- **文件和行号**：`system/ui/shell/update_pages.c:184`；现有入口 `system/ui/shell/settings_pages.c:33`；事件只首次发出见 `system/services/update/update_service.c:583`、`:590`。漏测点：`tests/update_pages.c:104`、`:130`。
- **复现命令**：执行下方 P 探针的 `about` 场景。真实服务先发现升级包，再打开“关于和更新”，实际触摸“系统更新”这一行。等价手动路径是上线自动发现新版本后进入设置，或关闭更新页后再次进入。
- **实测结果**：页面显示“有新版本”，点击后仍为 `SetAbout`；等待 5 秒的入口断言失败。`test.pass=15`、`test.fail=1`、`test.finished=1`。随后直调 `shell.update` 能正常打开 `Update`，因此不是服务没发现版本。证据：`build/update-pages-review/about/metrics.json`、`stderr.log`。
- **原因和影响**：新页依赖 `update.available` 才从关于页弹出；同一版本再次检查不会再发这个事件。上线自动检查会让第一次手动进入也遇到，正常操作即可复现，不需要卡住线程。既有流程测试预先直调导航能力，无法发现这个断点。执行报告第 50 行已披露，但它影响主入口，应作为验收缺口，不能仅列“顺手发现”。
- **最小修法**：合并主会话把关于页这一处现有绑定改为 `shell.update`，检查操作留在更新页里；增加从现有入口进入、返回后再进入的测试。当前任务禁止修改共享文件，因此此处由合并主会话处理，不扩展服务事件或新增入口机制。

### UP-R1-02 · 中：检查失败后，“重新下载”无法重试尚未成功的检查

- **文件和行号**：`system/ui/shell/update_pages.c:64`、`:87`、`:159`；下载前置条件见 `system/services/update/update_service.c:803`。
- **复现命令**：执行 P 探针的 `retry` 场景。以无网络、无已知升级包启动，打开更新页，点“检查更新”，等待失败，再点“重新下载”。
- **实测结果**：失败页仅提供 `shell.update_close` 和 `update.download`，没有 `update.check`；此时 `U.offer_code=0`，再次下载因尚未取得版本而失败，仍停在 `UpdateFail`。`test.pass=14`、`test.fail=1`、`test.finished=1`。证据：`build/update-pages-review/retry/metrics.json`、`stderr.log`。此场景未验证恢复联网后的自动检查，不声称联网后永远不能恢复。
- **原因和影响**：任意能力失败都变成同一个失败页，失败页又无条件绑定下载。现有测试只覆盖已经有包的坏签名重试。用户可退出再进入更新页重新检查，故列中级。
- **最小修法**：利用页面已读取的目标版本，尚无可下载版本时把失败页重试绑定回 `update.check`，文案改为“重新检查”；已有包的下载失败仍用 `update.download`。不新增重试任务或状态机。

## 独立验证与证据边界

- 普通构建退出 **0**；全量退出 **8**，232/233，**167.61 秒**。日志 `build/update-pages-review-build.log`、`build/update-pages-review-full.log`。
- ASan/UBSan 构建退出 **0**；全量退出 **8**，232/233，**278.42 秒**。日志 `build-asan/update-pages-review-build.log`、`build-asan/update-pages-review-full.log`。
- 两轮新增场景均 **12 阶段、218 条断言、0 失败**；内部 RAM 最低余量 **37968 字节**。本次使用 `INK_KEEP_EVIDENCE=1` 保留证据。
- 反向对照脚本退出 **0**（表示成功检测到退回旧实现）：基线子测试退出 **1**，**6 条断言失败**且运行到结束；`build/update-pages-reverse/summary.json`。它只验证旧卡片替换后的 `UpdateReady`，不能证明入口或全部失败分支都被覆盖。
- 另补查真实下载中直接点“暂停”，没有先断网：**16 条断言、0 失败**，`build/update-pages-review/pause/metrics.json`。未把下载期间的输入时序列为问题。
- 唯一全量失败是共享能力表从 262 项变为 264 项、文档尚未更新。总规则第 7 节禁止本任务改该文件，执行报告也如实标为部分通过；**这是合并待办，不另计虚报完成或产品缺陷**。主会话更新共享文档后仍需让该检查通过。
- Linux 未实测；未连接设备，签名使用项目既有模拟替身，不能据此声称实机升级已验证。

## 十屏视觉核对

用本轮全量测试产生的稳定面板截图重新生成 `out/ui-compare/`，并查看七个升级画板及 Settings、Notifications、Confirm，共十屏。核查后将本轮生成的对照文件移至忽略目录 `build/update-pages-review/ui-compare/`，避免留下其他未跟踪文件。原七页截图仍在 `build/test-out/update-pages/`。

| 页面 | 1/4 灰度平均差 /255 | 肉眼核对 |
| --- | ---: | --- |
| Update | 34.514 | 字号、返回圆、按钮更大；进度条和说明整体下移，百分比紧贴进度条末端；更新说明卡改为“暂不能查看”。布局位置见 `update_pages.c:114`、`:118`、`:120`。 |
| UpdateReady | 29.362 | 下载圆更大且上移；缺少设计稿下方的更新说明卡，留白较多；“今晚再装”变为“以后再装”。后者与未实现定时安装的边界一致。 |
| UpdateBlocked | 24.358 | 电池圆更大且偏上；正文不再重复当前 23% 电量，只说明 30% 门槛；字距与灰度按现主题。 |
| UpdateFail | 24.804 | “更新包有问题，没有安装”改为泛化的“这次更新没完成”；按钮、状态圆更大，正文层次改用黑色与字号。错误操作的问题见 UP-R1-02。 |
| UpdateInstall | 20.421 | 标题由左对齐变居中；四步之间没有竖线，当前步骤没有实心圆点，主要靠加粗区分；步骤间距增大，另有百分比与两行页脚。见 `update_pages.c:126` 至 `:138`。 |
| UpdateDone | 24.476 | 成功圆、标题整体下移；更新说明换为缺数据提示；底部“好”改“知道了”。 |
| BootRecovery | 24.983 | 状态圆更大、文字整体下移；保留回退版本与“你的东西都在”的说明，未见错字或截断。 |
| Settings | 81.154 | 原有页面：分组明显更高、无搜索圆；关于入口位于更低处，数据与稿中的示例不同。不是本次差异引入。 |
| Notifications | 24.323 | 原有夹具仅一条通知而非稿中的三条；页码条更低，“全部已读”与设计“读未读通知 / 全部清除”不同。不是本次改动。 |
| Confirm | 37.809 | 原有通用确认夹具：底图淡化、文字和按钮更大，合成目标为 `example.com`；操作说明较设计稿泛化。这张图不是升级密码页的完成证据。 |

未见七页标准字号场景的文字重叠或屏外截断。上述字号、灰度差异大部分来自已确认的 1.2 倍主题及 `#888888`，不单独报缺陷；布局与文案差异需要保留在验收记录。执行报告已说明缺更新说明、定时安装、安装前密码步骤和重启后自检阶段显示，复查没有把这些当作已完成。安装步骤的最小视觉调整只需在现有循环内恢复连接线和当前步标记，无需新控件框架。

## 总规则、模块边界与过度设计

- 基线差异仅 11 个任务相关文件；CMake、服务文件均尾部追加，旧升级占位卡替换成启动桥。没有改签名、校验、回滚逻辑，也没有改其他并行任务文件。
- 页面读服务状态，按钮经能力总线执行；只增加 12 行只读元数据接口，无文件 I/O。LVGL 操作处于 UI 执行器；产品未引用模拟器头文件、未新增常驻任务。
- 安装能力保留人工确认及 AI 禁用策略，没有代人确认。大块页面状态放 PSRAM。未新增依赖、预算放宽、测试跳过或用 sleep 掩盖失败；新增的短虚拟等待用于事件注入，60 秒等待之后有必须重启的失败断言。
- 已有测试只更新安装页名称/文案并增强无动作断言，以及追加两项导航能力白名单，未发现放宽。
- **过度设计**：未发现需删除的新任务、持久格式、缓存、重试队列或通用框架。七种页面选择和只读接口与任务相称，无需为规模而删除产品机制；不建议为上述两处问题新增事件、状态机或逐调用方补丁。
- `git diff --check` 退出 0；基线到 HEAD 的路径/硬件标识正则扫描 **0 匹配**。最终版本库工作区只多本 `REVIEW.md`。

## 复现命令

所有命令从仓库根目录执行，环境变量沿用用户提供值；两种构建不并发。已核对现有 CMake cache：`INK_WERROR=ON`，ASan 目录另有 `INK_SANITIZE=ON`，字体、Python、LVGL 均来自指定依赖目录。

```sh
# B / N / A：本轮独立构建和全量
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# R：反向对照
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/update_pages_reverse.py --build build
# C：不改共享文档的能力验证
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/update-pages-review/capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
# V：本轮保留的十屏稳定帧重新计算分数
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/update-pages-review/ui-compare --reuse --scenes Update UpdateReady UpdateBlocked UpdateFail UpdateInstall UpdateDone BootRecovery Settings Notifications Confirm
```

### P：不改源码的复现探针

下列 C 函数只附加到现有 `tests/update_pages.c` 的**内存副本**，复用其启动代码、触摸辅助函数、真实升级服务和编译参数；未注入升级状态。两个问题各产生 1 条失败断言，另含 1 个正常暂停对照。模拟器进程本身退出 0，所以必须读取 `metrics.json` 的 `test.fail`，不能把进程成功误认成断言通过。

```c
static void review_locate(void) {
    x = y = -1; find_label(lv_screen_active()); find_label(lv_layer_sys());
}
static void review_press(const char *label) {
    quiet(); wanted = label; on_ui(review_locate);
    CHECK(x >= 0 && y >= 0, "review visible control: %s", label);
    if (x >= 0) tap(x, y);
}
static void review_record(void *arg) {
    (void)arg;
    fprintf(stderr, "REVIEW mode=%s top=%s state=%s snapshot=%s\n", mode, g_top, U.st.state, g_snap);
}
static void review_run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home(); quiet();
    if (!strcmp(mode, "retry")) {
        CHECK(BY_HAND("shell.update", NULL), "open update page while offline");
        review_press("检查更新");
        CHECK(WAIT_FOR(page("UpdateFail"), 30000), "failed check reaches failure page");
        quiet(); on_ui(read_screen); vtk_host_call(review_record, NULL);
        CHECK(strstr(g_snap, "\"capability\":\"update.check\""), "failed check must offer checking again before any package is known");
        review_press("重新下载");
        CHECK(!U.offer_code && page("UpdateFail"), "download retry cannot recover without an offer");
    } else {
        ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(), 30000), "hub online");
        CHECK(AI("update.check", NULL) && g_rc.status == INK_RC_DONE, "discover real offer");
        CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "available"), 5000), "offer is available");
        quiet();
        if (!strcmp(mode, "about")) {
            CHECK(BY_HAND("shell.about", NULL) && WAIT_FOR(page("SetAbout"), 3000), "open real about page");
            review_press("系统更新");
            CHECK(WAIT_FOR(page("Update"), 5000), "about update row must enter the available update page (actual=%s)", g_top);
            quiet(); on_ui(read_screen); vtk_host_call(review_record, NULL);
            CHECK(BY_HAND("shell.update", NULL) && WAIT_FOR(page("Update"), 3000), "direct capability still opens the page");
        } else if (!strcmp(mode, "pause")) {
            CHECK(BY_HAND("shell.update", NULL), "open update");
            review_press("下载");
            CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "downloading"), 3000), "real download starts");
            wanted = "暂停"; on_ui(review_locate);
            CHECK(x >= 0, "pause exists during active download");
            if (x >= 0) tap(x, y);
            CHECK(WAIT_FOR(!strcmp(state_of("update.state"), "available"), 5000), "pause works without cutting the network first");
        }
    }
    ink_probe_count("test.finished");
}
```

以下命令从本报告提取探针，生成并串行运行三个场景；只向忽略的 `build/update-pages-review/` 写编译及运行产物：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
import json, os, re, shlex, subprocess
root = Path.cwd()
build = root / 'build'
area = build / 'update-pages-review'
area.mkdir(exist_ok=True)
report = (root / 'docs/tasks/update-pages/REVIEW.md').read_text()
probe = re.search(r'```c\n(.*?)\n```', report, re.S)[1]
source = (root / 'tests/update_pages.c').read_text()
source = source.replace('void ink_main(void) {', probe + '\nvoid ink_main(void) {')
source = source.replace('.fn = run,', '.fn = review_run,')
flags = (build / 'CMakeFiles/sim-update-pages.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
link = shlex.split((build / 'CMakeFiles/sim-update-pages.dir/link.txt').read_text())
obj, binary = area / 'probe.o', area / 'probe'
subprocess.run([link[0], *options, '-iquote', str(root / 'tests'), '-x', 'c', '-',
                '-c', '-o', str(obj)], input=source, text=True, check=True, cwd=build)
link = [str(obj) if x.endswith('/tests/update_pages.c.o') else x for x in link]
link[link.index('-o') + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
for mode in ('about', 'retry', 'pause'):
    out = area / mode
    env = dict(os.environ, INK_UPDATE_PAGE_MODE=mode,
               INK_SIM_UPDATE_PKG=str(build / 'test-out/update-pages/good.inkfw'))
    command = [str(binary), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
               '--script', '/dev/stdin', '--assets', str(build / 'assets'),
               '--content', str(build / 'test-content'), '--out', str(out),
               '--quiet', '--no-random-outages']
    script = '@0 net ' + ('none' if mode == 'retry' else 'wifi') + '\n@300s end\n'
    result = subprocess.run(command, input=script, env=env, text=True,
                            capture_output=True, timeout=90)
    (out / 'stderr.log').write_text(result.stderr)
    (out / 'stdout.log').write_text(result.stdout)
    data = json.loads((out / 'metrics.json').read_text())
    m = data['metrics']
    print(mode, 'exit', result.returncode, 'pass', m.get('test.pass'),
          'fail', m.get('test.fail', 0), 'finished', m.get('test.finished'),
          'warnings', data.get('warnings'))
PY
```

SEVERE=0 HIGH=1
