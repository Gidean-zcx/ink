# 审计任务说明（2026-11-04 第十一轮，交给 Codex）

你是独立审计方，做的是本项目（自己的嵌入式固件与模拟器）的**软件质量审计**：正确性、并发、资源、失败路径、文档一致性。上一轮（`docs/audits/2026-11-04-codex-system/`，对象 ebf05c8）确认了 10 条问题，其后作者做了修复（处理记录在 `docs/audits/README.md` 的“第十轮”一节）；上一轮中途被打断，部分范围没有看完。这一轮的对象是 c95f48c。

目标：

1. **核实第十轮的处理**：AUD11-001～010 以及 AUD-007/F-6 逐条判断（已修复 / 部分修复 / 未修复 / 回归）。修复引入的新问题也算。
2. **补完上一轮没看完的范围**：服务接入（`power_service.c`、`net_service.c`、`display/light.c`、`camera/camera_service.c`、`comm/comm_service.c`）、资源管理（`services/res/res.c`）、系统状态与设置（`policy/state.c`、`settings.c`、`watch.c`）、实时会话与打断（`hub/rt.c`、`hub/hub_link.c`、`ai/ai_service.c`、`audio/audio_service.c`）、远程画面与解码（`apps/remote/remote.c`、`util/lz4.c`、`simulator/environment/sim_remote.c`、`ports/sim/hal/hal_jpeg_sim.c`）、参数与 Schema（`util/ink_args.c`）、应用边界（`ink/sys/app.h`、`ui/app.c`、`tests/sdk_boundary.cmake`、`apps/*`）、模拟模型（`ports/sim/hal/hal_net_sim.c`、`hal_afe_sim.c`、`simulator/environment/sim_hub.c`、`simulator/profiles/metalio-plus-handsfree.json`）、内部内存的账（`INK_PSRAM_BSS`、大静态数组、内部 RAM 上的大栈帧；模拟器不把静态数据计入内存池）、文档与预算是否属实（详细设计 §13、`tests/integration/*`、新场景）。
3. 新的执行队列（`system/services/res/exec.c`、`system/ui/ui_task.c` 里的 `ink_ui_kick` 和循环）和调用生命周期（`system/policy/cap.c`）是这一轮改动最大的地方，请专门看：节点的有效期、唤醒会不会丢、`ink_ui_busy` 的覆盖、`unlock_retire` 的归还条件。

重点是**严重**和**高**；中、低级的问题也可以记，但不要为了数量去找。定级：**严重**：正常使用就会崩溃、丢数据、损坏文件或设备；**高**：常见条件下功能错误、资源泄漏、结果不实，或测试与文档给出虚假保证；**中**：少见条件才出问题，或影响有限；**低**：可读性、小的不一致。作者会把没有严重和高当作合入 main 的条件，所以定级要诚实：不要夸大，也不要放过。

## 1. 先读

1. `AGENTS.md`（协作约定；末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/audits/README.md` 开头的表和“第十轮”一节；`docs/audits/2026-11-04-codex-system/report.md`。
3. `docs/system/system-architecture.md`（概要设计 v1.0）、`docs/system/system-detailed-design.md`（详细设计，重点 §3、§4.3、§6、§8、§13）、`docs/decisions/README.md` 的第五、第六节。
4. `docs/system/repo-layout.md`、`docs/project.md` 最后几十行。

## 2. 规则

- **只读审计**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在** `docs/audits/2026-11-04-codex-system-r2/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充测试程序与运行说明）。单个文件不超过 1 MB；构建日志不要放进来。
- **边做边写**：现在就建好 `report.md` 的骨架；每查完一块，就把结论写进 `report.md` 和 `findings.json`，不要等到最后一起写；运行可能被中途打断。最后再写“一页结论”。
- 用普通的集成测试和单元测试验证想法：`tests/integration/lifecycle.c` 是现成的写法，把产品源文件原样 include 进来，在锁边界上安排另一个任务的操作。
- 可以在当前目录里构建和跑测试（构建目录 `build-audit/`），见第 3 节。不需要联网。
- 不接触设备，不读 `~/Documents/Workspace/ink-hw/` 的实机数据，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。

## 3. 构建与测试

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

内存检查构建：同样的参数再加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`，构建目录 `build-audit-asan/`。macOS 没有 `timeout` 命令。

## 4. 写法与格式

- 报告用中文；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- `report.md`：
  1. 一页结论：还有没有严重或高级别的问题；能不能合入 main；各范围的总体判断。
  2. **修复核实**：`编号 | 结论 | 证据 | 说明`。
  3. **新问题**：编号 AUD12-001 起，每条含定级、标题、证据、位置、影响、建议。
  4. 结尾写明审计对象提交、跑了哪些命令、哪些没查（以及为什么）。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；`verification.json`：`[{"id","verdict","evidence","note"}]`。
