# 复审任务说明（2026-11-04 第九轮，交给 Codex）

你是独立审计方。这是第九轮。前几轮：第一轮审计 `docs/audits/2026-11-03-codex/`（AUD2-001～024）；第二轮至第八轮复审在 `docs/audits/2026-11-03-codex-r2/` 至 `-r8/`。第八轮核实 4 条：1 条已修复、2 条部分修复、F-6 未修（留待用户决定）；新问题 AUD9-001（中）、AUD9-002（高）。第八轮之后，开发方在分支 `claude/restructure` 上修了这两条，处理表在 `docs/audits/README.md` 的“2026-11-04 第八轮复审：处理”一节。审计对象是当前目录这份检出，`git rev-parse HEAD` 给出的提交，先记下来。

目标有两个：

1. **核实第八轮的处理**：AUD9-001（提交 e3bf181：`pqbn_v4_skip` 的长跳步）、AUD9-002（提交 38266cf：静止维护的 CPU 成本，PQ-BN 档案的 `skip_row`、`skip_call`）、AUD2-023（文档）。AUD-011/F-8 剩下的问题就是这两条，不再单列。每一条是否真的修好了、修法对不对、有没有引入回归（包括正常路径：PQ-BN 档案的翻页、长时间静止后的刷新、与音频的并发；EPDiy 默认档案不应受影响）。不要只看说明，要看代码、跑测试，必要时写补充测试复现。第八轮在 `docs/audits/2026-11-03-codex-r8/experiments/` 留下的程序可以复用（`pq_checks.py`、`pq_cost.py`）。F-6 已知、留待用户决定，不必修，也不计作新问题。
2. **找新的大问题**：修复本身引入的问题，以及前几轮漏掉的问题。重点是**严重**和**高**（定级同第一轮）。中、低级的问题也可以记，但不要为了数量去找。用户的标准是“直到没有太大问题”，所以请明确给出结论：除 F-6 外，还有没有严重或高级别的问题。

## 1. 先读

1. `AGENTS.md`（协作约定；末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/audits/README.md`（各轮处理表与后续事项），`docs/audits/2026-11-03-codex-r8/` 的 `report.md`、`verification.json`、`findings.json`、`experiments/README.md`。
3. `docs/system/repo-layout.md`、`docs/system/simulator-overview.md`、`docs/research/simulator/hardware-parity.md`、`docs/background.md`、`docs/project.md` 最后几十行。
4. 官方项目源码（硬件依据）：`~/Documents/Workspace/ink-hw/upstream/Metalio-E-INK4-Plus/`，固定在提交 `690a023`。这份检出是部分克隆，`git show` 读不到的对象可以直接读工作区文件（工作区就是 690a023）。板级代码在 `main/boards/metalio-e-ink4-plus/` 与 `main/boards/common/`（浅睡流程在 `main/boards/common/power_policy/`）。`~/Documents/Workspace/ink-hw/` 下除 `upstream/` 外的目录是私有实机数据，**不要读**。

## 2. 重点

第八轮之后改动的地方：

- **长跳步**（AUD9-001）：`drivers/display/pqbn/pqbn_v4.c` 的 `skip_long`、`pqbn_v4_skip`；`tests/unit/pqbn_clock_test.c`。看：与逐相运行是否等价（各种起点、步长、4096 与 65536 与 uint32 边界、含运动块和未静止像素的状态），返回的扫行数。
- **维护成本**（AUD9-002）：`simulator/runtime/profile.{h,c}` 的 `skip_row`、`skip_call`，`simulator/profiles/metalio-plus-pqbn.json`，`ports/sim/hal/hal_display_sim.c` 的 `pq_advance_idle`。看：数值与指令级测量是否相符、计入的时机与任务、对刷新时延和音频并发的影响、档案说明。
- **文档**（AUD2-023）：审计记录、对照表、R-034、`docs/background.md` 与代码是否一致。

## 3. 规则

- **只读复审**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。这是本项目自己的嵌入式固件与模拟器的软件质量复审（正确性、并发、资源、文档一致性），不涉及安全测试。
- **产出只写在** `docs/audits/2026-11-03-codex-r9/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充测试程序与运行说明）。单个文件不超过 1 MB；构建日志不要放进来。
- **边做边写**：先建好 `report.md` 的骨架；每核实完一组，就把结论写进 `report.md` 和 `verification.json`，不要等到最后一起写；运行可能被中途打断。最后再写“一页结论”。
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

内存检查构建：同样的参数再加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`，构建目录 `build-audit-asan/`。PQ-BN 档案的整板自检现在是 CTest 的 `selftest-board-wiring-pqbn`。macOS 没有 `timeout` 命令。桌面窗口的无界面测试用 `SDL_VIDEODRIVER=dummy`。改了文件做前后对比时，换回文件后要 `touch` 一下再构建（同一秒内的修改，构建系统可能看不出来）；对比做完要把文件换回原样。

## 5. 写法与格式

- 报告用中文；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- `report.md`：
  1. 一页结论：还有没有严重或高级别的问题；各范围的总体判断。
  2. **修复核实表**：上面目标 1 的每一条一行，`编号 | 结论（已修复 / 部分修复 / 未修复 / 回归）| 证据 | 说明`。
  3. **新问题**：编号 AUD10-001 起，每条含定级、标题、证据、位置、影响、建议。
  4. 结尾写明审计对象提交、跑了哪些命令、哪些没查（以及为什么）。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；另写 `verification.json`：`[{"id":"AUD9-001","verdict":"已修复|部分修复|未修复|回归","evidence_type":"...","note":"..."}]`。
