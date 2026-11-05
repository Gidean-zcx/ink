# 审计任务说明（2026-11-05 第十六轮，交给 Codex）

你是独立的软件质量审计方。对象是本项目自己的家用墨水屏设备的固件和它的模拟器。看的是：功能对不对，并发和资源用得对不对，出错和断电时行为对不对，测试是不是真的能发现问题，文档说的是不是真的，还有模块划分和 AI 原生。

上一轮（第十五轮）被运行环境中途打断，只写出了 8 条，都已经处理（[报告](../2026-11-04-codex-services-s2/report.md)，处理见设计文档 §15.3.1）。那一轮的实验在 `../2026-11-04-codex-services-s2/experiments/`，可以直接用。这一轮把没做完的做完，再加上之后新增的代码。

目标：

1. **第十五轮 8 条的修复**（AUD16-001～008，提交 58247ba）：逐条看修到点上没有，有没有带来新问题。
2. **第十四轮的修复**（AUD15，提交 42f1f22，[报告](../2026-11-04-codex-services/report.md)）：上一轮没写完，逐条给结论：已修复、部分修复或未修复。
3. **第 2 阶段的代码**，上一轮没查完的部分：`system/services/text/`、`system/ui/textfield.c`、`system/ui/shell/keyboard.c`；`system/services/audio/`、`system/services/bt/`、`ports/sim/hal/hal_bt_sim.c`；`system/services/net/wifi.c`；`system/policy/members.c`、`system/policy/cap.c`、`system/policy/confirm.c`、`system/ui/shell/pinpad.c`、`member_gate.c`、`confirm_card.c`；`system/services/update/update_service.c`、`ports/sim/hal/hal_ota_sim.c`、`tools/inkfw/make_inkfw.py`、`system/ui/shell/update_card.c`。
4. **第十五轮之后新增的**（`88f107c..HEAD`）：
   - 语音输入 `system/services/text/voice_input.c`（§9.5，被来电、闹钟、AI 键打断时留字）
   - 电源菜单、音量键、关机图：`system/ui/shell/power_menu.c`、`off_picture.c`、`system/services/input/input_service.c`、`system/services/power/power_service.c`、`system/policy/system_actions.c`
   - 恢复出厂：只写记号然后重启，下次开机最先清（§11.5）
   - 麦克风状态 `audio.mic`（§10.3）、保存阶段 `data.status`（§15.4）
   - 任务列表 `system/services/res/task.c`（§13.1）
   - 文字编辑：光标、选区、撤销（§9.4.1）
   - 界面用词（D-98）
5. **测试是否真能发现它声称发现的问题**：设计文档 §15.2、§15.3、§15.3.1 写了反向对照。抽查几条：在 `build-audit/` 的副本里把修复撤掉，看对应的检查是否真失败。
6. **文档和预算的说法是否属实**：`docs/system/system-services-design.md`（§9～§15）、`docs/system/ui-dependencies.md` §4.1 和 §7。
7. **架构和 AI 原生**（第 5 节）。

重点是**严重**和**高**；中、低级的也可以记，不要为了数量去找。定级：**严重**：正常使用就会丢用户数据、设备起不来、孩子能进大人的空间、AI 能代替人输密码或确认；**高**：特定但现实的时序或操作下丢数据、卡死、上述隔离失效，或者测试和文档给了不实的保证；**中**：出错路径上行为不对、资源浪费；**低**：其他。

作者自己不放心的地方，不限于这些：

- **成员表读不出来**：`table_unread` 这个状态在每条路径上是否一致；`pin_job` 重读时别的任务在读成员表会怎样；重读成功后当前成员和已打开的东西是否一致。
- **恢复出厂**：`ink_factory_reset_resume` 放在启动最前面，它依赖的东西那时是否都准备好了（`ink_fs_wipe_personal`、`ink_storage_run`）；`reset.card` 等卡时每 5 s 看一次，锁屏后还有没有动静；保留 `update.json` 是否够；数据有没有别的副本没清（存储缓存、`/cache`、卡上别的目录）。
- **语音输入**：打断之后状态是 `finishing`，这时界面、音频焦点、麦克风、实时会话各是什么状态；两个实时会话同时开（听写收尾和 AI 这一轮）；打断后换页、锁屏、再打断；结果和结束的顺序；`quiet_frames` 的计数。
- **事件按成员发**：`send_events` 里哪些事件该带 `member` 而没带（通知之外：文件、录音、提醒、闹钟、任务）；设备端的订阅者是否也有同样的问题。
- **录音**：换人时结束的流程，和被来电打断、卡满同时发生时 `why` 和文件是否完整。
- **文字编辑**：字符和字节换算（多字节字符、组合串、语音写入）；撤销快照的内存上限和内存不足时的行为；输入框占位外观里点击位置的换算；`ink_text_commit_utf8` 超长时；密码框。
- **任务列表**：文件作业和任务的生命周期是否总能配对（提交失败、卡拔了、U 盘模式、换人）；`job.cancel` 和结束同时发生；`for_everyone` 的判断。
- **关机图**：电源任务等 6 s 时别的东西（看门狗、低电量再次触发、锁屏）；锁屏时屏幕还能不能刷；刷新失败时；电量极低时要不要跳过。
- **音量键**：输入任务里读状态、投递作业是否够短；通话时、闹钟响时、孩子的音量上限。
- **升级**：`.inkfw` 解析对损坏文件的健壮性（`read_header` 的段表、偏移和大小、段重叠、超出文件）；状态文件在每个断电点是否收敛；`blocked_code` 只记一个版本够不够；资源区写到一半又有新版本。
- **持卡**（`ink_fs_card_hold`）：持有者卡住时 U 盘模式怎样；和 `FS.nopen` 的配对。
- **音频焦点**：`ink_audio_focus_acquire/release` 和老接口 `ink_audio_stream_open`、`ink_audio_ring` 的组合；回调在哪个执行器；续播的边界；卡被拔。
- **内部 RAM 和栈**：启动时不应该有新任务；`exec.bg` 什么时候被启动；`ai_ask-metalio-plus-pqbn` 的 33192 字节是否属实；中枢任务回调、定时器任务、电源任务、输入任务的栈用量。

没做的不算问题：系统壳和系统应用的正式界面（现在只有占位），中枢真正的协议，正式词库，`ports/esp32s31`，中枢协议 v0.2 要设备做的改动（设计文档 §15.4 列了），配对、备份、无线传输，D-95 的计时（等用户定）。真机蓝牙协议没有手册，只有接口和模型，这是有意的；如果接口假设了协议，要说。

## 1. 先读

1. `AGENTS.md`（协作约定；末尾的 hybrid routing 是给 Codex 子代理的路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/system/system-services-design.md`（本次的设计依据，尤其 §1.3、§9～§15）。
3. `docs/audits/README.md` 开头的表和第十四、十五轮两节。
4. `docs/product/system-ui-spec.md` §3.6、§7.5、§7.7、§7.11、§7.12、§8（界面对底层的要求）。

## 2. 规则

- **只读审计**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在** `docs/audits/2026-11-05-codex-services-s3/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充的测试程序和运行说明）。单个文件不超过 3 MB。
- **边做边写，这一条很重要**：运行可能被中途打断。现在就建好 `report.md` 的骨架；每查完一块，立刻把结论写进 `report.md` 和 `findings.json`。不要攒到最后。最后再通读一遍。
- 用普通的集成测试和单元测试验证想法：`tests/integration/races.c`（在放锁的时刻安排另一个任务）、`tests/integration/people.c`、`tests/integration/update.c`、`tests/integration/dictation.c` 是现成的写法，把产品跑起来，或者把产品源文件原样 include 进来。上一轮的 `experiments/run_probe.py` 可以直接用。
- 可以在当前目录构建和跑测试（构建目录 `build-audit/`），见第 3 节。不需要联网。
- 不接触设备，不读 `~/Documents/Workspace/ink-hw/` 的实机数据，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。

## 3. 对象、构建与测试

对象：运行时看 `git log -1`（分支 `claude/system-services`）。

依赖已在本机下载好，用下面的路径，不要联网下载：

```bash
D=<放 build-deps 的主检出目录>
cmake -S . -B build-audit -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src
cmake --build build-audit -j 8
$D/build-deps/pyenv/bin/ctest --test-dir build-audit -j 8 --timeout 400 --output-on-failure
```

内存检查构建：同样的参数再加 `-DINK_SANITIZE=ON`，构建目录 `build-audit-asan/`。macOS 没有 `timeout` 命令。

做反向对照时注意：本机的 make 是 3.81，时间戳只到秒。把源文件改回去之后等一秒再构建，否则会用到旧的目标文件。

## 4. 写法与格式

- 报告用中文；代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：**【复现】**（实际运行，附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- `report.md`：
  1. 一页结论：有没有严重或高级别的问题；各范围的总体判断。
  2. **新问题**：编号 AUD17-001 起，每条含定级、标题、证据、位置、影响、建议。
  3. **修复的核查**：AUD16 八条、AUD15 十一条，逐条。
  4. **架构与 AI 原生审视**（第 5 节）。
  5. 结尾写明审计对象提交、跑了哪些命令、哪些没查（以及为什么）。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；`verification.json`：反向对照抽查和修复的核查，`[{"id","verdict","evidence","note"}]`。

## 5. 架构与 AI 原生审视（用户要求，R-061）

单独成节写进报告，发现的问题和别的问题一样编号、定级：

- **模块划分**：模块、组件、子系统的边界是否合理，是否符合“软总线 + 原子能力”（`docs/system/system-architecture.md` §4、§5）；能力、状态、设置、事件、内部机制归类对不对；服务之间有没有不经总线和策略关口的路（比如升级、词库直接走 `hal_fs_*`，是否合适）；机制有没有写进界面层。
- **AI 原生**：是不是从第一天就给 AI 用：按钮和 AI 是否走同一条路径；结果是否结构化；事件能否到 AI；页面和系统浮层（键盘、密码键盘、安装卡、成员门、关机图）AI 是否看得到；AI 的行为有没有来源记录、能否撤销、是否都过策略关口。
- **能力开放**：用户能做的事、能看的数据，AI 是否都能调用、能读到；对照能力总表（设计文档 §1.3）和运行时的 `tools/list` 核对（测试 `integration-ai-surface`）；不开放的是否都有站得住的理由（`member.unlock`、`member.set_pin`、`system.upgrade`、`system.upgrade_from_card`、`confirm.accept`、`confirm.reject`）。
