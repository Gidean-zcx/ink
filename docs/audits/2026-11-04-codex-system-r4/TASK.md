# 审计任务说明（2026-11-04 第十三轮，交给 Codex，复核）

你是独立审计方，做的是本项目（自己的嵌入式固件与模拟器）的**软件质量审计**：正确性、并发、资源、失败路径、文档一致性。上一轮（`docs/audits/2026-11-04-codex-system-r3/`，对象 9cf64ad）没有发现严重或高级别的问题，只有 4 条中级（AUD13-001～004）。作者随后修了这 4 条，外加几处小改动（处理记录在 `docs/audits/README.md` 的“第十二轮”一节）。这一轮的对象是 99495a8。

**这是一次小范围复核，不是全面审计**：只看 `git diff 9cf64ad..HEAD`（产品代码、模拟器、测试）。

目标：

1. 核实 AUD13-001～004：逐条判断（已修复 / 部分修复 / 未修复 / 回归）。
2. 这批改动本身有没有引入新的**严重或高**级别问题。范围：`system/services/ai/ai_service.c`（`turn_over`、`on_rt_closed`、松键路径）、`system/apps/remote/remote.c`（`spot_restore`、提示位置、锁屏时打开）、`ports/sim/hal/hal_net_sim.c`（`cell_relink`）、`system/services/comm/comm_service.c`（接听和挂断的时限）。测试（`tests/integration/ai_race.c`、`lifecycle.c`、`net_auto.c`、`hal_switch.c`）是否真能发现它声称发现的问题。

不要重审其它范围。定级：**严重**：正常使用就会崩溃、丢数据、损坏文件或设备；**高**：常见条件下功能错误、资源泄漏、结果不实，或测试与文档给出虚假保证；**中**：少见条件才出问题，或影响有限；**低**：可读性、小的不一致。作者会把没有严重和高当作合入 main 的条件，所以定级要诚实：不要夸大，也不要放过。中、低级的问题也可以记，但不要为了数量去找。

## 1. 先读

`AGENTS.md`（末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）；`docs/audits/README.md` 的“第十二轮”一节；`docs/audits/2026-11-04-codex-system-r3/report.md`。

## 2. 规则

- **只读审计**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在** `docs/audits/2026-11-04-codex-system-r4/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`。单个文件不超过 1 MB；构建日志不要放进来。
- **边做边写**：现在就建好 `report.md` 的骨架，每查完一块就写进去；运行可能被中途打断。最后再写“一页结论”。
- 可以在当前目录里构建和跑测试（构建目录 `build-audit/`）。不需要联网。不接触设备，不读 `~/Documents/Workspace/ink-hw/`，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。

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

- 报告用中文；代码标识、命令、路径保持原样。每条结论标明证据类型：**【复现】**、**【代码核查】**、**【文档核查】**、**【推断】**，推断不得写成事实。
- `report.md`：1. 一页结论：还有没有严重或高级别的问题；能不能合入 main。2. 修复核实：`编号 | 结论 | 证据 | 说明`。3. 新问题：编号 AUD14-001 起，每条含定级、标题、证据、位置、影响、建议。4. 结尾写明审计对象提交、跑了哪些命令、哪些没查。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；`verification.json`：`[{"id","verdict","evidence","note"}]`。
