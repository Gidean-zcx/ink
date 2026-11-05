# 审计任务说明（2026-11-04 第十五轮，交给 Codex）

你是独立审计方，做的是本项目（自己的嵌入式固件与模拟器）的**软件质量审计**：正确性、并发、资源、失败路径、文档一致性，以及架构和 AI 原生。

目标：

1. **第 2 阶段新增的系统服务是否正确**：只看这个范围里新增和改动的代码（对象和范围见第 3 节）：
   - `system/services/text/`（输入会话、拼音引擎、剪贴板）、`system/ui/textfield.c`、`system/ui/shell/keyboard.c`
   - `system/services/audio/`（五档焦点、`ogg.c`、`media_service.c`、录成文件、路由）、`system/services/bt/`、`ports/sim/hal/hal_bt_sim.c`
   - `system/services/net/wifi.c`、`ports/sim/hal/hal_net_sim.c` 的接入点部分
   - `system/policy/members.c`、`system/util/sha256.c`、`system/policy/cap.c` 和 `confirm.c`（空间关闭、家长 PIN、绑定成员、`ask_by_hand`）、`system/ui/shell/pinpad.c`、`member_gate.c`、`confirm_card.c`
   - `system/system_actions`（恢复出厂、重启：`grep -rn system.factory_reset system`）
   - `system/services/update/update_service.c`、`ports/sim/hal/hal_ota_sim.c`、`tools/inkfw/make_inkfw.py`、`system/ui/shell/update_card.c`、`simulator/environment/sim_hub.c` 的升级部分
   - `system/services/ai/ai_service.c` 的唤醒词部分
2. **第十四轮的修复是否修对了**（提交 42f1f22）：[上一轮报告](../2026-11-04-codex-services/report.md)的 11 条问题，逐条看修复有没有修到点上，有没有引入新问题，比如 `interrupt.c` 的 `apply` 现在一个任务持有“应用权”时别的任务的结果还会不会丢。
3. **测试是否真能发现它声称发现的问题**：`tests/integration/{textinput,media,people,update,listening,races}.c`、`tests/integration/update.cmake`。设计文档 §15.2、§15.3 写了反向对照：请抽查几条，把修复撤掉，看对应的检查是否真失败（改动只放在 `build-audit/` 的副本里，产品代码和测试都不动）。
4. **文档和预算的说法是否属实**：`docs/system/system-services-design.md`（§9～§15）、`docs/system/ui-dependencies.md` §4.1。

重点是**严重**和**高**；中、低级的问题也可以记，但不要为了数量去找。定级：**严重**：正常使用就会丢用户数据、权限被绕过（孩子进成人空间、AI 碰到 PIN 或确认）、设备变砖；**高**：特定但现实的时序或操作下丢数据、卡死、权限问题，或者测试和文档给了虚假保证；**中**：失败路径上的错误行为、资源浪费；**低**：其他。

值得特别看的地方（作者自己不放心的，不限于这些）：

- **PIN 和成人空间**：错误计数和等待在所有路径上是否一致（`member.unlock`、切换成员、改 PIN、家长确认）；断电能不能绕过等待；时钟被改能不能绕过；明文 PIN 会不会留在内存、参数、日志、回执里（`ink_args_t` 里？`confirm` 的待确认表里？）；空间关闭时还有什么能出来（通知内容、页面快照、AI 上下文的 `state`、唤醒词）。
- **恢复出厂**：`ink_fs_wipe_personal`、`ink_storage_wipe_user`、`ink_data_forget_all` 的顺序，断电时的半成品；还有什么没清。
- **升级**：`.inkfw` 解析（`read_header`）对恶意或损坏头的防御（整数溢出、段的 offset 和 size、`nsec`、段重叠、段超出文件）；校验之后到写槽之间文件被换掉（TOCTOU，人可以在 U 盘模式改卡）；`check_sections` 用的 `hal_fs_read` 读不全；下载的 base64 解码边界；续传时 `.part` 比 `offer_size` 大或内容不对；`ink_update_boot_check` 的状态机（`writing`、`trying`、`resources`，加上 `rolled_back`）在每个断电点是否都收敛；`blocked_code` 只记一个版本够不够；`hal_ota_*` 的调用任务；`self_job` 的自检是否太宽或太严；`storage.card` 监听和 `usb.changed` 同时触发 `scan_job`。
- **持卡**（`ink_fs_card_hold`）：持有者卡住时 U 盘模式怎样；和 `FS.nopen` 的配对；`pinyin.c` 的路径。
- **音频焦点**：`ink_audio_focus_acquire/release` 和老接口 `ink_audio_stream_open`、`ink_audio_ring` 的组合；回调在哪个执行器；续播“回退 3 s、30 分钟内”的边界；录音时被来电打断文件是否完整（Ogg 页结束）；卡被拔；U 盘模式和正在写的录音文件；记位置写 Flash 的时机是否会打断声音。
- **文本输入**：会话和成员切换；密码框的内容有没有漏到 AI 的页面快照、`text.state`、剪贴板、日志；候选表缓冲；组合串上限；词库文件的校验（偏移、长度）在 32 位目标上有没有溢出（`dict_ok`）。
- **Wi-Fi**：密码在 `ink_args_t`（确认卡待确认表）里停留多久，`confirm.list` 和 `confirm.result` 会不会带出来；`net.wifi` 集合。
- **内部 RAM 和栈**：启动时不应该有新任务；`exec.bg` 什么时候被启动；`ai_ask-metalio-plus-pqbn` 的 33192 字节是否属实；中枢任务回调（`on_check_reply`、`on_chunk`）、定时器任务（`self_timeout`）、电源任务的栈。

没做的不算问题：系统壳和系统应用的界面（这一轮只有占位），中枢真正的协议，正式词库，`ports/esp32s31`。真机蓝牙协议没有手册，只有接口和模型，这是有意的；如果接口假设了协议，要说。

上一轮之前的代码不必重审，除非这次的改动碰到了它。

## 1. 先读

1. `AGENTS.md`（协作约定；末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/system/system-services-design.md`（本次的设计依据，尤其 §1.3、§9～§15）。
3. `docs/audits/README.md` 开头的表和第十四轮一节；[上一轮报告](../2026-11-04-codex-services/report.md)。

## 2. 规则

- **只读审计**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在** `docs/audits/2026-11-04-codex-services-s2/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充测试程序与运行说明）。单个文件不超过 3 MB。
- **边做边写**：现在就建好 `report.md` 的骨架；每查完一块，就把结论写进 `report.md` 和 `findings.json`，不要等到最后一起写；运行可能被中途打断。最后再通读一遍。
- 用普通的集成测试和单元测试验证想法：`tests/integration/races.c`（在锁的边界上安排另一个任务）、`tests/integration/people.c`、`tests/integration/update.c` 是现成的写法，把产品跑起来，或者把产品源文件原样 include 进来。上一轮的 `experiments/run_probe.py` 可以直接用。
- 可以在当前目录里构建和跑测试（构建目录 `build-audit/`），见第 3 节。不需要联网。
- 不接触设备，不读 `~/Documents/Workspace/ink-hw/` 的实机数据，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。

## 3. 对象、构建与测试

对象：运行时看 `git log -1`（分支 `claude/system-services`）。范围：`a81c1b8..HEAD`。

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

做反向对照时注意：本机的 make 是 3.81，时间戳只到秒。把源文件改回去之后等一秒再构建，否则会用到旧的目标文件（上一轮处理时踩过）。

## 4. 写法与格式

- 报告用中文；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- `report.md`：
  1. 一页结论：有没有严重或高级别的问题；各范围的总体判断。
  2. **新问题**：编号 AUD16-001 起，每条含定级、标题、证据、位置、影响、建议。
  3. **第十四轮修复的核查**：逐条，已修复、部分修复或未修复。
  4. **架构与 AI 原生审视**（第 5 节）。
  5. 结尾写明审计对象提交、跑了哪些命令、哪些没查（以及为什么）。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；`verification.json`：对设计文档 §15 抽查的反向对照和第十四轮修复的核查，`[{"id","verdict","evidence","note"}]`。

## 5. 架构与 AI 原生审视（用户要求，R-055）

单独成节写进报告，发现的问题和别的问题一样编号、定级：

- **模块划分**：模块、组件、子系统的边界是否合理，是否符合“软总线 + 原子能力”（`docs/system/system-architecture.md` §4、§5）；能力、状态、设置、事件、内部机制归类对不对；服务之间有没有绕过总线和权限的路（比如升级、词库直接走 `hal_fs_*`，是否合适）；机制有没有写进界面层。
- **AI 原生**：是不是从第一天就给 AI 用：按钮和 AI 是否走同一条路径；结果是否结构化；事件能否到 AI；页面和系统浮层（键盘、PIN 键盘、安装卡、成员门）AI 是否看得到；AI 的行为有没有来源记录、能否撤销、是否都过策略关口。
- **能力开放**：用户能做的事、能看的数据，AI 是否都能调用、能读到；对照能力总表（设计文档 §1.3）和运行时的 `tools/list` 核对（测试 `integration-ai-surface`）；不开放的是否都有站得住的理由（`member.unlock`、`member.set_pin`、`system.upgrade`、`system.upgrade_from_card`、`confirm.accept`、`confirm.reject`）。
