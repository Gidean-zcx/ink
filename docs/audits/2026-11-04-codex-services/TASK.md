# 审计任务说明（2026-11-04 第十四轮，交给 Codex）

你是独立审计方，做的是本项目（自己的嵌入式固件与模拟器）的**软件质量审计**：正确性、并发、资源、失败路径、文档一致性，以及架构和 AI 原生审视（第 5 节）。上一轮（第十三轮）之后合入的是一大块新的系统服务，这是它们的第一次独立审计。对象是 a81c1b8，范围是 `1fbf510..a81c1b8`。

目标：

1. **新系统服务的正确性**：只看这个范围里新增和改动的代码：
   - `system/services/data/`（结构化数据存储：日志加快照、压缩、断电）、`system/services/storage/`（新作业）
   - `system/services/file/`（统一路径和权限、拷贝和删除作业、U 盘模式）、`ports/sim/hal/hal_usb_sim.c`
   - `system/services/time/`（时钟、时区、农历、闹钟和提醒、唤醒）、`system/services/power/power_service.c`（`ink_power_wake_at`、低电量策略）
   - `system/services/notify/`（收件箱、渠道、打断通道）、`system/services/audio/audio_service.c`（`ink_audio_ring`）
   - `system/policy/confirm.c`、`system/policy/cap.c`（确认卡、撤销、结果、事件转发、分页清单）
   - `system/ui/app_registry.c`、`system/ui/ui_task.c`（浮层、顶层钩子、`ink_ui_restart`）、`system/ui/shell/`
   - `system/services/res/svc.c`（共用的锁和定时器）、`system/services/hub/hub_link.c`（tick、上线回调）
2. **测试是否真能发现它声称发现的问题**：`tests/integration/{data,files,timesvc,notify,confirm,apps,ai_surface}.c`、`tests/layering.cmake`。文档里写了反向对照（设计文档 §15）：抽查几条是否属实。
3. **文档和预算的说法是否属实**：`docs/system/system-services-design.md`（§1～§8、§14、§15），`docs/system/ui-dependencies.md`，预算文件。
4. **第 2 阶段还没做的不算问题**（文本输入、音频焦点、媒体、Wi-Fi、成员和 PIN、升级、设计 §9～§13）。但如果第 1 阶段的接口会挡住它们，要说。

上一轮之前的代码不必重审，除非这次的改动碰到了它。重点是**严重**和**高**；中、低级的问题也可以记，但不要为了数量去找。定级：**严重**：正常使用就会崩溃、丢数据、损坏文件或设备；**高**：常见条件下功能错误、资源泄漏、结果不实，或测试与文档给出虚假保证；**中**：少见条件才出问题，或影响有限；**低**：可读性、小的不一致。作者会把没有严重和高当作继续的条件，所以定级要诚实：不要夸大，也不要放过；需要特定交错才出现、影响有限的，按中级或说明触发条件。

值得特别看的地方（作者自己不放心的，不限于这些）：

- 多个任务调同一个服务时的锁和竞争：`ink_notify_post` 和换成员（`inbox()`、`NS.held`）、`interrupt.c` 的 `apply`、`confirm.c` 的 `take` 和执行、`app_registry.c` 的表（界面任务改、别的任务读）。
- 栈：定时器任务（闹钟响的路径）、电源任务（低电量）、存储任务（`done` 回调）、输入任务（电源键钩子）。
- 断电：数据存储、闹钟表、应用状态、低电量关机前的写入。
- 内部 RAM：启动时不应该有新任务；`exec.bg` 什么时候被启动。
- 打断和确认卡在锁屏、换成员、内存紧张时的组合。
- 事件转发 `send_events` 和中枢任务；`ink_cap_emit` 的开销。

## 1. 先读

1. `AGENTS.md`（协作约定；末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/system/system-services-design.md`（本次的设计依据）、`docs/system/ui-dependencies.md`。
3. `docs/audits/README.md` 开头的表和第十三轮一节；`docs/project.md` 最后几十行。

## 2. 规则

- **只读审计**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在** `docs/audits/2026-11-04-codex-services/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充测试程序与运行说明）。单个文件不超过 1 MB；构建日志不要放进来。
- **边做边写**：现在就建好 `report.md` 的骨架；每查完一块，就把结论写进 `report.md` 和 `findings.json`，不要等到最后一起写；运行可能被中途打断。最后再写“一页结论”。
- 用普通的集成测试和单元测试验证想法：`tests/integration/notify.c`、`tests/integration/lifecycle.c` 是现成的写法，把产品跑起来，或者把产品源文件原样 include 进来，在锁边界上安排另一个任务的操作。
- 可以在当前目录里构建和跑测试（构建目录 `build-audit/`），见第 3 节。不需要联网。
- 不接触设备，不读 `~/Documents/Workspace/ink-hw/` 的实机数据，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。

## 3. 构建与测试

依赖已在本机下载好，用下面的路径，不要联网下载：

```bash
D=<放 build-deps 的主检出目录>
cmake -S . -B build-audit -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src
cmake --build build-audit -j 8
$D/build-deps/pyenv/bin/ctest --test-dir build-audit -j 8 --timeout 300 --output-on-failure
```

内存检查构建：同样的参数再加 `-DINK_SANITIZE=ON`，构建目录 `build-audit-asan/`。macOS 没有 `timeout` 命令。

## 4. 写法与格式

- 报告用中文；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- `report.md`：
  1. 一页结论：有没有严重或高级别的问题；各范围的总体判断。
  2. **新问题**：编号 AUD15-001 起，每条含定级、标题、证据、位置、影响、建议。
  3. **架构与 AI 原生审视**（第 5 节）。
  4. 结尾写明审计对象提交、跑了哪些命令、哪些没查（以及为什么）。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；`verification.json`：对设计文档 §15 抽查的反向对照，`[{"id","verdict","evidence","note"}]`。

## 5. 架构与 AI 原生审视（用户要求，R-055）

单独成节写进报告，发现的问题和别的问题一样编号、定级：

- **模块划分**：模块、组件、子系统的边界是否合理，是否符合“软总线 + 原子能力”（`docs/system/system-architecture.md` §4、§5）；能力、状态、设置、事件、内部机制归类对不对；服务之间有没有绕过总线的直接依赖；机制有没有混进界面层。
- **AI 原生**：是不是从第一天就给 AI 用：按钮和 AI 是否走同一条路径；结果是否结构化；事件能否到 AI；页面和系统浮层 AI 是否看得到；AI 的行为有没有来源记录、能否撤销、是否都过策略关口。
- **能力开放**：用户能做的事、能看的数据，AI 是否都能调用、能读到；对照能力总表（设计文档 §1.3）和运行时的 `tools/list` 核对（测试 `integration-ai-surface`）；不开放的是否都有站得住的理由。
