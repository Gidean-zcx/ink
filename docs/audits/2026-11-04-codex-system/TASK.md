# 审计任务说明（2026-11-04 第十轮，交给 Codex）

你是独立审计方。前九轮审的是模拟系统与原有系统服务（`docs/audits/2026-11-03-codex*/`，处理记录在 `docs/audits/README.md`）。这一轮的对象是合并后的代码：上一轮结束时的 main（560d81a）加上新写的**底层系统十步**（`docs/system/system-detailed-design.md`，实现记录在它的 §13）和 **F-6 的修复**。审计对象是当前目录这份检出，`git rev-parse HEAD` 给出的提交（ebf05c8），先记下来。审计通过后这份代码会合入 main，所以请把它当作合入前的把关。

目标有两个：

1. **核实 F-6 的修复**（`docs/audits/README.md` 的 AUD-007 / F-6 一行）：内容版本 `ink_ui_content_version`（换屏和翻页都加一）、工具调用按它判过时、回执带回新版本、请求号去重（做过的回第一张回执，执行中的回“已排队”）；回归场景 `tests/scenarios/ai_page_version.*`。修法对不对、有没有漏掉会改变页面内容的路径、会不会误判过时、有没有引入回归。
2. **审计新的系统层**：正确性、并发、资源、失败路径，以及文档说法是否属实。重点是**严重**和**高**，中、低级的问题也可以记，但不要为了数量去找。定级：**严重**：正常使用就会崩溃、丢数据、卡死，或者测试通过但核心功能其实没工作；**高**：较常见的情况下功能出错、资源泄漏或违反已定约定；**中**：边界情况出错，或者文档与实现不符、会误导后续开发；**低**：小问题。请明确给出结论：**还有没有严重或高级别的问题，这份代码能不能合入 main**。

## 1. 先读

1. `AGENTS.md`（协作约定；末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/system/system-architecture.md`（概要设计 v1.0，已确认）、`docs/system/system-detailed-design.md`（详细设计，重点看 §13 的实现记录，里面写了与设计不同之处和没做的事）、`docs/decisions/README.md` 的第五、六轮决策。
3. `docs/audits/README.md`（各轮处理与后续事项）、`docs/system/repo-layout.md`、`docs/project.md` 最后几十行。

## 2. 重点

新写或大改的代码（`git diff 560d81a..HEAD --stat` 可以看全）：

- **能力与软总线**：`system/policy/cap.c`（调用池的生命周期：`finish`、`answer`、`answered`、`release`、超时、迟到完成、`ink_cap_invoke` 放弃等待、`replay`、事件）、`system/util/ink_args.c`（JSON 边界与 Schema 子集）、`system/services/res/exec.c`。
- **系统状态与设置**：`system/policy/state.c`、`settings.c`、`watch.c`（锁、通知的合并与丢失、定时器、保存）。
- **各服务接入**：`system/services/power/power_service.c`、`system/services/net/net_service.c`、`system/services/display/light.c`、`system/services/camera/camera_service.c`（回收与拍照的互斥）、`system/services/comm/comm_service.c` 与 `ports/sim/hal/hal_cellular_sim.c`。
- **资源管理**：`system/services/res/res.c`（记账、准入、压力分级、回收函数的调用时机与重入、作业的暂停与恢复、释放后使用）。
- **实时会话**：`system/services/hub/rt.c`、`system/services/hub/hub_link.c`（分级发送队列、帧池、后台让路、`ink_hub_keep`、重连后重新登记）、`system/services/ai/ai_service.c`（语音改走会话）。
- **免按键与打断**：`system/services/audio/audio_service.c`（听与录共用麦克风、捕获结束路径、打断时停播与其它音频路径的竞争）、`system/services/ai/ai_service.c`（免按键的一轮、听的策略、离线命令）、`platform/include/ink/hal_afe.h`、`ports/sim/hal/hal_afe_sim.c`。
- **远程画面**：`system/apps/remote/remote.c`（解码与渲染共用画布、配额、会话结束后的迟到帧与待处理区域、再次打开）、`system/util/lz4.c`（解码器不能越界）、`simulator/environment/sim_remote.c`、`ports/sim/hal/hal_jpeg_sim.c`。
- **应用边界**：`system/include/ink/sys/app.h`、`system/ui/app.c`、`tests/sdk_boundary.cmake`、`apps/*`。
- **模拟器的改动是否歪曲了模型**：`ports/sim/hal/hal_net_sim.c`（`sim_net_hub_send_at`、多个网络回调）、`simulator/environment/sim_hub.c`、`simulator/runtime/profile.*`、`simulator/profiles/metalio-plus-handsfree.json`。
- **内部内存的账**：详细设计 §1.2 说内部 RAM 只多控制结构、大表放 PSRAM（`INK_PSRAM_BSS`，`platform/include/ink/osal.h`）；模拟器不把静态数据计入内存池。看有没有别的大静态数组或内部 RAM 上的大栈帧漏掉了（真机上它们会占内部 RAM，底线 SR-PL-04 是 32 KiB）。
- **文档与预算是否属实**：详细设计 §13 的每条说法是否与代码和测试一致；`tests/integration/systest.*` 与新场景（`voice_handsfree`、`voice_listen_policy`、`remote_browse`、`ai_offline_intent`、`ai_page_version`）是否真的检查了它们声称的东西，预算有没有放宽到失去意义。

不必再审前九轮已处理过、这一轮没有改动的部分。

## 3. 规则

- **只读审计**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。这是本项目自己的嵌入式固件与模拟器的软件质量审计（正确性、并发、资源、文档一致性），不涉及安全测试。
- **产出只写在** `docs/audits/2026-11-04-codex-system/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充测试程序与运行说明）。单个文件不超过 1 MB；构建日志不要放进来。
- **边做边写**：先建好 `report.md` 的骨架；每查完一块，就把结论写进 `report.md` 和 `findings.json`，不要等到最后一起写；运行可能被中途打断。最后再写“一页结论”。
- 可以在当前目录里构建和跑测试（构建目录 `build-audit/`），见第 4 节。不需要联网。
- 不接触设备，不读 `~/Documents/Workspace/ink-hw/` 的私有实机数据，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。

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

内存检查构建：同样的参数再加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`，构建目录 `build-audit-asan/`；新的并发与释放路径请用它跑一遍。macOS 没有 `timeout` 命令。改了文件做前后对比时，换回文件后要 `touch` 一下再构建（同一秒内的修改，构建系统可能看不出来）；对比做完要把文件换回原样。

## 5. 写法与格式

- 报告用中文；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- `report.md`：
  1. 一页结论：还有没有严重或高级别的问题；能不能合入 main；各范围的总体判断。
  2. **修复核实**：F-6 一行，`编号 | 结论（已修复 / 部分修复 / 未修复 / 回归）| 证据 | 说明`。
  3. **新问题**：编号 AUD11-001 起，每条含定级、标题、证据、位置、影响、建议。
  4. 结尾写明审计对象提交、跑了哪些命令、哪些没查（以及为什么）。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；另写 `verification.json`：`[{"id":"AUD-007/F-6","verdict":"已修复|部分修复|未修复|回归","evidence_type":"...","note":"..."}]`。
