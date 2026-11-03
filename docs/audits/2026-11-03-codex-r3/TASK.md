# 复审任务说明（2026-11-03 第三轮，交给 Codex）

你是独立审计方。这是同一天的第三轮：第一轮审计在 `docs/audits/2026-11-03-codex/`（AUD2-001～024），第二轮复审在 `docs/audits/2026-11-03-codex-r2/`（核实 AUD2-001～023：13 条已修复、10 条部分修复；新问题 AUD3-001～004）。第二轮之后，开发方先按系统架构重组了目录（只搬目录、拆构建目标，不改行为，见 `docs/system/repo-layout.md`，其中“旧路径对照”一节给出重组前后的路径），再在分支 `claude/restructure` 上处理第二轮的结果，处理表在 `docs/audits/README.md` 的“2026-11-03 第二轮复审：处理”一节。审计对象是当前目录这份检出，`git rev-parse HEAD` 给出的提交，先记下来。

目标有三个：

1. **核实第二轮的处理**：AUD3-001～004；第二轮判为部分修复的 AUD2-010、012、016、019、020、023（AUD2-004、014、015、017 剩下的问题就是 AUD3-001、002、004、003，不再单列）；AUD2-022 的附注。每一条是否真的修好了、修法对不对、有没有引入回归。不要只看说明，要看代码、跑测试，必要时写补充测试复现。第二轮在 `docs/audits/2026-11-03-codex-r2/experiments/` 留下的程序可以复用，但里面的路径是重组前的，按“旧路径对照”换算。
2. **核实目录重组**（AUD2-024，第一轮留到重组处理）：分层是否由构建目标表达（系统与应用在构建上引用不到驱动和模拟器）、公开头文件是否收窄、重组有没有漏掉文件或改变行为。
3. **找新的大问题**：修复本身引入的问题，以及前两轮漏掉的问题。重点是**严重**和**高**（定级同第一轮）。中、低级的问题也可以记，但不要为了数量去找。用户的标准是“直到没有太大问题”，所以请明确给出结论：还有没有严重或高级别的问题。

## 1. 先读

1. `AGENTS.md`（协作约定；末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/audits/README.md`（三节处理表），`docs/audits/2026-11-03-codex-r2/` 的 `verification.json`、`findings.json`、`report.md`。
3. `docs/system/repo-layout.md`、`docs/system/simulator-overview.md`、`docs/research/simulator/hardware-parity.md`、`docs/background.md`、`docs/project.md` 最后几十行。
4. 官方项目源码（硬件依据）：`~/Documents/Workspace/ink-hw/upstream/Metalio-E-INK4-Plus/`，固定在提交 `690a023`。这份检出是部分克隆，`git show` 读不到的对象可以直接读工作区文件（工作区就是 690a023）。板级代码在 `main/boards/metalio-e-ink4-plus/` 与 `main/boards/common/`（浅睡流程在 `main/boards/common/power_policy/`）。`~/Documents/Workspace/ink-hw/` 下除 `upstream/` 外的目录是私有实机数据，**不要读**。

## 2. 重点

第二轮修复改动的地方：

- **刷新途中屏电源掉电**（AUD3-003）：`ports/sim/hal/hal_display_sim.c` 的 `power_kept`、`execute`、PQ-BN 管线（`pq_drive`、`pq_after_phase`）、`sim_display_supply_low`；`boards/metalio_plus/metalio_epd_power.c` 的 `epd_power_good`；`ports/sim/hal/hal_input_sim.c` 里 GPIO2 中断对 PWR_GOOD 的处理。看锁的先后、每次刷新的回调是否正好一次、PQ 队列里同时有几次刷新时的失败处理、复电后的重新初始化，以及两种显示管线（档案 `simulator/profiles/metalio-plus.json` 与 PQ-BN 档案）是否都对。
- **定时器删除与回调重启**（AUD3-001）：`ports/sim/osal/osal_sim.c` 的 `osal_timer_delete`、`osal_timer_start`、`disarm_locked`；契约见 `platform/include/ink/osal.h`；这个契约能否在 FreeRTOS/ESP-IDF 上照样实现。
- **唤醒恢复与开机设置失败**（AUD3-002、AUD2-012）：`ports/sim/hal/hal_power_sim.c`（`with_retries`、`board_exit`）、`ports/sim/hal/hal_input_sim.c`（`sim_input_sleep_end`、`sim_input_restore_later`、`restore_try`、`keys_resume_locked`、`service_expander_locked`）。按键会不会丢、会不会重复；后台重试与正常中断处理交错时是否一致；重试会不会无休止；总线一直不好时系统能否继续工作。
- **睡眠与音频启动互斥**（AUD3-004）：`ports/sim/hal/hal_audio_sim.c`（`sim_power_lease` 的使用、`ensure_bt_mode`）、`hal_power_sim.c`（`g_admission`、`busy_reason`）。租约是否成对、各条失败路径是否释放、睡眠与音频会不会互相等待卡死。
- **内存池记账**（AUD2-010）：`simulator/memory/pools.c`、`tests/unit/pools_test.c`。
- **摄像头超时**（AUD2-016）：`drivers/chips/gc2145/gc2145.c` 的 `gc2145_start_until`、`simulator/models/dev_camera.c`。
- **电脑摄像头后端**（AUD2-019、020）：`simulator/desktop/gui/host_camera_linux.c`、`host_camera_mac.m`。本机是 macOS，Linux 版本只能做代码核查。
- **CI**（AUD2-022）：`.github/workflows/sim.yml`。
- **文档**（AUD2-023）：对照表、`docs/requirements.md` 的 R-034、审计记录与代码是否一致。
- **目录重组**（AUD2-024）：`CMakeLists.txt` 的分层目标、`docs/system/repo-layout.md`。

## 3. 规则

- **只读复审**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。这是本项目自己的嵌入式固件与模拟器的软件质量复审（正确性、并发、资源、文档一致性），不涉及安全测试。
- **产出只写在** `docs/audits/2026-11-03-codex-r3/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充测试程序与运行说明）。单个文件不超过 1 MB；构建日志不要放进来。
- **边做边写**：先建好 `report.md` 的骨架；每核实完一组（例如 AUD3-001～002），就把结论写进 `report.md` 和 `verification.json`，不要等到最后一起写；运行可能被中途打断。最后再写“一页结论”。
- 可以在当前目录里构建和跑测试（构建目录 `build-audit/`），见第 4 节。不需要联网。
- 不接触设备，不读私有实机数据，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。

## 4. 构建与测试

依赖已在本机下载好，用下面的路径，不要联网下载：

```bash
D=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=$D/build/_deps/imgui-src \
  -DFETCHCONTENT_SOURCE_DIR_SDL2=$D/build/_deps/sdl2-src
cmake --build build-audit -j 8
$D/build-deps/pyenv/bin/ctest --test-dir build-audit -j 8 --timeout 300 --output-on-failure
```

内存检查构建：同样的参数再加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`，构建目录 `build-audit-asan/`。macOS 没有 `timeout` 命令。桌面窗口的无界面测试用 `SDL_VIDEODRIVER=dummy`。改了文件做前后对比时，换回文件后要 `touch` 一下再构建（同一秒内的修改，构建系统可能看不出来）；对比做完要把文件换回原样。

## 5. 写法与格式

- 报告用中文；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- `report.md`：
  1. 一页结论：还有没有严重或高级别的问题；各范围的总体判断。
  2. **修复核实表**：上面目标 1、2 的每一条一行，`编号 | 结论（已修复 / 部分修复 / 未修复 / 回归）| 证据 | 说明`。
  3. **新问题**：编号 AUD4-001 起，每条含定级、标题、证据、位置、影响、建议。
  4. 结尾写明审计对象提交、跑了哪些命令、哪些没查（以及为什么）。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；另写 `verification.json`：`[{"id":"AUD3-001","verdict":"已修复|部分修复|未修复|回归","evidence_type":"...","note":"..."}]`。
