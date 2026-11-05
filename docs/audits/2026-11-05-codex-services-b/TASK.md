# 审计任务说明（2026-11-05 第十七轮 B 路，交给 Codex）

你是独立的软件质量审计方。对象是本项目自己的家用墨水屏设备的固件和它的模拟器。看的是功能对不对，并发和资源用得对不对，出错时行为对不对，测试是不是真能发现问题，文档说的是不是真的。这一轮分三路，这一路只看**声音和文字**。问题编号 AUD18-2xx。对象：运行时看 `git log -1`。

先读 `AGENTS.md`（末尾的 hybrid routing 不用管）、`docs/system/system-services-design.md` 的 §9、§10、§13、§15.3、§15.3.1、§15.3.2，`docs/product/system-ui-spec.md` 的 §3.6、§7.5、§7.7、§7.12、§8。

范围：

1. **音频**：`system/services/audio/`（五档焦点、`ogg.c`、`media_service.c`、录成文件、路由、`audio.mic`）、`system/services/bt/`、`ports/sim/hal/hal_bt_sim.c`。`ink_audio_focus_acquire/release` 和老接口 `ink_audio_stream_open`、`ink_audio_ring` 的组合；回调在哪个执行器；续播的边界；录音时被来电打断、卡满、卡被拔、U 盘模式，文件是否完整。真机蓝牙协议没有手册，只有接口和模型，这是有意的；如果接口假设了协议，要说。
2. **语音输入**：`system/services/text/voice_input.c`、`system/services/ai/ai_service.c` 里按键和唤醒的部分。打断之后状态是 `finishing`，这时界面、音频焦点、麦克风、实时会话各是什么状态；两个实时会话同时开（听写收尾和 AI 这一轮）；打断后换页、锁屏、再打断；结果和结束的顺序；`quiet_frames` 的计数。
3. **文字**：`system/services/text/`（输入会话、拼音引擎、剪贴板、光标、选区、撤销）、`system/ui/textfield.c`、`system/ui/shell/keyboard.c`。字符和字节换算（多字节字符、组合串、语音写入）；撤销快照的内存上限和内存不足时；点击位置的换算；超长时；词库文件的校验；32 位目标上的溢出。
4. **按键和关机**：音量键（`input_service.c`、`system_actions.c`）：输入任务里读状态、投递作业是否够短，通话和闹钟时；关机图（`system/ui/shell/off_picture.c`、`power_service.c`）：电源任务等 6 s 时别的东西（低电量再次触发、锁屏）、刷新失败时、电量极低时。电源菜单。
5. **内部 RAM 和栈**：启动时不应该有新任务；`exec.bg` 什么时候被启动；`ai_ask-metalio-plus-pqbn` 的 33192 字节是否属实；中枢任务回调、定时器任务、电源任务、输入任务的栈用量。
6. **修复核查**：AUD17-005（[第十六轮报告](../2026-11-05-codex-services-s3/report.md) 的 experiments/record_end_probe.c，设计文档 §15.3.2）、AUD16-002、003（[第十五轮报告](../2026-11-04-codex-services-s2/report.md)）；第十四轮里音频和界面的几条（[报告](../2026-11-04-codex-services/report.md)，比如 `interrupt.c` 的 `apply`）。抽查设计文档里相关的反向对照。

## 规则

- **只读审计**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在本目录**：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`（补充的测试程序和运行说明）。单个文件不超过 3 MB。
- **边做边写，这一条很重要**：运行可能被中途打断。现在就建好 `report.md` 的骨架；每查完一块，立刻把结论写进 `report.md` 和 `findings.json`，不要攒到最后。最后再通读一遍。
- 用普通的集成测试验证想法：`tests/integration/` 里的 `races.c`（在放锁的时刻安排另一个任务）、`people.c`、`update.c`、`dictation.c`、`media.c` 是现成的写法，把产品跑起来，或者把产品源文件原样 include 进来。`docs/audits/2026-11-05-codex-services-s3/experiments/run_probe.py` 可以直接用。
- **不做和密码尝试次数、等待时间有关的实验**：这一块之前查过，这类内容会让运行被外部的内容过滤中断。
- 可以在当前目录构建和跑测试（构建目录 `build-audit/`），不需要联网。不接触设备，不读 `~/Documents/Workspace/ink-hw/` 的实机数据，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。本次不要调用任何子代理或外部模型，你自己完成。

## 构建

```bash
D=<放 build-deps 的主检出目录>
cmake -S . -B build-audit -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT=$D/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$D/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$D/build/_deps/lvgl-src
cmake --build build-audit -j 4
$D/build-deps/pyenv/bin/ctest --test-dir build-audit -j 4 --timeout 400 --output-on-failure
```

内存检查构建：同样的参数再加 `-DINK_SANITIZE=ON`，构建目录 `build-audit-asan/`。macOS 没有 `timeout` 命令。做反向对照时：本机 make 是 3.81，时间戳只到秒，把源文件改回去之后等一秒再构建。另外两路审计同时在跑，所以 `-j 4`。

## 写法

- 报告用中文；代码标识、命令、路径保持原样。每条结论标明证据类型，推断不得写成事实：**【复现】**（附命令与输出摘录）、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。
- 定级：**严重**：正常使用就会丢用户数据、设备起不来、孩子能进大人的空间、AI 能代替人做只有人能做的事；**高**：特定但现实的时序或操作下丢数据、卡死、上述隔离失效，或者测试和文档给了不实的保证；**中**：出错路径上行为不对、资源浪费；**低**：其他。重点是严重和高，不要为了数量去找。
- `report.md`：一页结论；新问题（定级、标题、证据、位置、影响、建议）；修复的核查（逐条：已修复、部分修复或未修复）；结尾写明对象提交、跑了哪些命令、哪些没查。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation","reproduce"}`；`verification.json`：`[{"id","verdict","evidence","note"}]`。

没做的不算问题：正式界面（现在只有占位），中枢真正的协议，正式词库，`ports/esp32s31`，中枢协议 v0.2 要设备做的改动和确认规则 D-105（设计文档 §15.4 列了），配对、备份、无线传输，D-95 的计时。
