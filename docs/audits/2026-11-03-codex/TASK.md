# 全系统与模拟器审计任务说明（2026-11-03，交给 Codex）

你是独立审计方。对当前目录这份 `ink` 仓库（main 的一份检出）做全面审计，对象是 `git rev-parse HEAD` 给出的提交，先记下来。目标是**找出错误、夸大、遗漏和风险，而不是确认现有结论**。没发现问题的地方，也写明查了什么、怎么查的。

## 1. 先读

1. `AGENTS.md`（协作约定：中文、证据分级、需求与建议分开记录；末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/background.md`、`docs/project.md`（最后几十行是最近的工作）。
3. `docs/system/simulator-overview.md`（模拟系统总览）、`docs/system/platform-and-simulator.md`、`docs/research/simulator/hardware-parity.md`。
4. `docs/requirements.md` 的 R-034、R-042～R-045，`docs/decisions/README.md` 的 D-41～D-44。
5. `docs/audits/README.md`：上一次（2026-11）审计的结论与处理。已修复的问题不要重复报告，除非又坏了（标“回归”）。
6. 官方项目源码（硬件依据）：`~/Documents/Workspace/ink-hw/upstream/Metalio-E-INK4-Plus/`，固定在提交 `690a023`，板级代码在 `main/boards/metalio-e-ink4-plus/` 与 `main/boards/common/`，其余文件用 `git -C <该目录> show 690a023c66862af103a3187464373ee7c5efcd14:<路径>` 读。`~/Documents/Workspace/ink-hw/` 下除 `upstream/` 外的目录是私有实机数据，**不要读**。

## 2. 审计范围

A. **系统与应用**（`system/`、`apps/`）：正确性、资源（内存、栈、CPU、刷新）是否在 S31 的预算内、并发与锁、错误处理与恢复、电源与待机、存储完整性（掉电）、网络与中枢的健壮性。

B. **平台接口**（`platform/include/ink/`）：OSAL 与 HAL 是否足以写出真机端口（`ports/esp32s31/` 尚未写）；有没有模拟器专有的概念漏进接口或系统代码；接口语义是否写清（上下文、阻塞、线程安全、错误）。

C. **驱动**（`drivers/`）：是否符合芯片手册与官方代码；I2C/UART/AT 出错、超时时是否稳健；共享资源（IO 扩展芯片、GPIO2 线与、共享复位 P12、共享电源 P14）的并发与顺序；能否原样用于真机。

D. **模拟器内核**（`sim/vtk/`、`sim/mem/`、`profiles/`）：确定性、时间与调度模型、Flash 停顿、内存模型、栈估计、档案数值的出处与标注。

E. **硬件模型**（`ports/sim/dev_*.c`、`ports/sim/hal_*_sim.c`）：与官方硬件是否一致（芯片、引脚、地址、接法、时序，对照官方 `config.h` 与驱动）；**模拟器只模拟硬件与环境**的原则（R-043 的补充说明）有没有被违反，即系统该做的流程、策略有没有写进模拟器；估计值与假设是否都标明。

F. **桌面模拟器**（`ports/sim/gui/`、`ports/sim/sim_gui.*`）：窗口线程与模拟线程之间的线程安全、锁顺序、生命周期（启动、停止、崩溃、重启）、资源泄漏、电脑摄像头与麦克风的平台代码。

G. **测试与 CI**（`tests/`、`CMakeLists.txt`、`.github/workflows/`）：测试是否真的检验了它声称的东西；靠运气、对时序敏感、平台相关的测试；预算是否合理；明显缺的测试。

H. **文档**：文档里的数字、结论、路径是否与代码一致；`README.md` 的状态。

I. **代码结构与目录**：现在的目录划分是否符合系统架构（应用 / 系统服务 / 平台接口 / 驱动 / 端口 / 模拟器内核 / 工具 / 文档）；命名、模块边界、头文件可见性、构建目标划分是否合理。给出**建议的目标目录结构**，附“现路径 → 新路径”的对照和理由，以及迁移时要注意的事（构建、测试、文档链接）。用户接下来要按系统架构重组目录。

## 3. 规则

- **只读审计**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在** `docs/audits/2026-11-03-codex/`（新建）：`report.md`、`findings.json`，需要时加 `experiments/`（复现用的脚本、场景、测试驱动，附运行说明）和 `patches/`（修复建议的补丁，不要应用）。单个文件不超过 1 MB。
- 可以在当前目录里构建和跑测试（构建目录放在当前目录下，例如 `build-audit/`），见第 4 节。不需要联网。
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

macOS 没有 `timeout` 命令。桌面窗口的无界面测试用 `SDL_VIDEODRIVER=dummy`。

## 5. 写法与格式

- 报告用中文；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（给出 `文件:行号`）、**【文档核查】**、**【推断】**。
- 定级：
  - **严重**：会在真机上出错、丢数据或损坏硬件，或让模拟器在关键结论上误导（比如资源、时序、接法）；
  - **高**：明显的缺陷或风险，近期就会碰到；
  - **中**：应修，但影响有限或有绕过办法；
  - **低**：整洁、文档、命名一类。
- `report.md`：先写一页结论（各范围的总体判断与最重要的问题），再按范围 A～I 列问题，每条含：编号（AUD2-001 起）、定级、标题、证据、位置、影响、建议。最后一节是第 I 部分的结构评估与建议的目标目录结构。
- `findings.json`：数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`。
- 结尾写明：审计对象提交、跑了哪些命令、哪些没查（以及为什么）。
