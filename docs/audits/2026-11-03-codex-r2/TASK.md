# 复审任务说明（2026-11-03 第二轮，交给 Codex）

你是独立审计方。这是同一天第一轮审计（`docs/audits/2026-11-03-codex/`，24 条，编号 AUD2-001～024）的**复审**。第一轮之后，开发方在分支 `claude/audit2-fixes` 上做了四批修复，处理结果写在 `docs/audits/README.md` 的“2026-11-03 Codex 全系统与模拟器审计：处理”一节。审计对象是当前目录这份检出，`git rev-parse HEAD` 给出的提交，先记下来。

目标有两个：

1. **核实修复**：AUD2-001～023 每一条是否真的修好了、修法对不对、有没有引入回归。不要只看说明，要看代码、跑测试，必要时写补充测试复现。
2. **找新的大问题**：修复本身引入的问题，以及第一轮漏掉的问题。重点是**严重**和**高**（定级同第一轮）。中、低级的问题也可以记，但不要为了数量去找。用户的标准是“直到没有太大问题”，所以请明确给出结论：还有没有严重或高级别的问题。

## 1. 先读

1. `AGENTS.md`（协作约定；末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/audits/2026-11-03-codex/report.md`、`findings.json`（第一轮）和 `docs/audits/README.md` 的处理表（含修复中的新发现 N2-1～N2-4）。
3. `docs/background.md`、`docs/project.md` 最后几十行、`docs/system/simulator-overview.md`、`docs/research/simulator/hardware-parity.md`。
4. 官方项目源码（硬件依据）：`~/Documents/Workspace/ink-hw/upstream/Metalio-E-INK4-Plus/`，固定在提交 `690a023`。这份检出是部分克隆，`git show` 读不到的对象可以直接读工作区文件（工作区就是 690a023）。板级代码在 `main/boards/metalio-e-ink4-plus/` 与 `main/boards/common/`（浅睡流程在 `main/boards/common/power_policy/`）。`~/Documents/Workspace/ink-hw/` 下除 `upstream/` 外的目录是私有实机数据，**不要读**。

## 2. 重点

修复改动最多、最值得复查的地方：

- **浅睡与唤醒**（AUD2-012、014）：`ports/sim/hal_power_sim.c` 的 `hal_power_sleep`、`drivers/board/metalio_board.c` 的睡眠步骤、`ports/sim/hal_input_sim.c` 的按键暂停与恢复、`ports/sim/dev_tca9555.c`、`dev_rtc_accel.c`。对照上游 `power_hw_light_sleep_once` 与 `epd_board_metalio_eink4_plus_ioexp_*`：顺序、引脚电平、方向、唤醒判断、中止路径、唤醒键上报（会不会丢、会不会重复）、P16 的处理（N2-1）、电流记账。
- **音频时钟与蓝牙模式**（AUD2-015）：`ports/sim/hal_audio_sim.c`、`dev_bt_audio.c`、`metalio_board_shared_power_epoch`。
- **系统服务**：`system/ai/ai_service.c`（AUD2-001）、`system/storage/storage_service.c`（AUD2-002）、`system/net/hub_link.c`（AUD2-003）、`platform/include/ink/hal_net.h` 与 `osal.h` 的契约及模拟实现（AUD2-004）：多任务下的同步、锁的先后顺序、会不会互相等待卡死（例如 `hal_ws_free` 等回调、回调等中枢队列）、契约能否在 ESP-IDF 上实现。
- **屏电源失败、摄像头、TCA9555 影子值、内存池**（AUD2-005～011、016、017）。
- **桌面模拟器与摄像头后端**（AUD2-018～021），**CI**（新加的 macOS 任务，AUD2-022），**对照表**（AUD2-023）。
- AUD2-024（分层）开发方留到目录重组，本轮不用复查，除非修复让它更糟。

## 3. 规则

- **只读复审**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。这是本项目自己的嵌入式固件与模拟器的软件质量复审（正确性、并发、资源、文档一致性），不涉及安全测试。
- **产出只写在** `docs/audits/2026-11-03-codex-r2/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充测试程序与运行说明；上一次中断的运行已留下 `experiments/extra_probe.c`、`run.py`，可以接着用）。单个文件不超过 1 MB；构建日志不要放进来。
- **边做边写**：每核实完一组（例如 AUD2-001～004），就把结论写进 `report.md` 和 `verification.json`，不要等到最后一起写；运行可能被中途打断。
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

macOS 没有 `timeout` 命令。桌面窗口的无界面测试用 `SDL_VIDEODRIVER=dummy`。改了文件做前后对比时，换回文件后要 `touch` 一下再构建（同一秒内的修改，构建系统可能看不出来）。

## 5. 写法与格式

- 报告用中文；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- `report.md`：
  1. 一页结论：还有没有严重或高级别的问题；各范围的总体判断。
  2. **修复核实表**：AUD2-001～023 每行一条，`编号 | 结论（已修复 / 部分修复 / 未修复 / 回归）| 证据 | 说明`。
  3. **新问题**：编号 AUD3-001 起，每条含定级、标题、证据、位置、影响、建议。
  4. 结尾写明审计对象提交、跑了哪些命令、哪些没查（以及为什么）。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；另写 `verification.json`：`[{"id":"AUD2-001","verdict":"已修复|部分修复|未修复|回归","evidence_type":"...","note":"..."}]`。
