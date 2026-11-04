# 第十三轮小范围复核报告

- 状态：完成。审计日期：2026-11-04。
- 审计对象：`99495a8da3903077117f3a181994e3824177d825`。
- 差异基线：`9cf64adc9ea0a7ce54d1e8f6e05603928e7f17a5`。
- 方法：主代理独立、只读审计；实验仅写入本目录，构建和原始日志留在指定构建目录；未调用子代理或外部模型。

## 1. 一页结论

**本轮范围内未发现严重或高级别问题；AUD13-001～004 均已修复。按 TASK.md 约定的“无严重、高问题”门槛，可以合入 main。** 该判断限于上述提交的本批改动，不代表整套系统、实机或发布验收。

【复现】普通 `INK_WERROR=ON` 构建 **57/57** 测试通过；ASan/UBSan 构建 **56/56** 通过（关闭 GUI，少一项 `gui-smoke`）。四个指定集成测试分别完成 10、114、26、3 项检查，均无失败、无模拟器警告。编译、测试命令均 exit 0。

【复现】**六个负对照均检出对应旧行为**：依次撤回蜂窝通知、AI 停采集、AI 松键保护、前画布反显恢复、顶部提示、HAL 旧连接失效逻辑，现有测试均运行完成并以 exit 1、`test.fail=1` 拒绝退化。仅修改送入编译器的内存副本，未改产品或既有测试文件。

【代码核查】【复现】另对 `on_rt_closed`、锁屏时打开远程页面、接听/挂断 25 s 时限做了三个边界实验，普通与 ASan/UBSan 下均通过。实际重连帧确认：AI 面板打开时，顶部断线提示可见。结果见 [基线](experiments/baseline_results.json)、[负对照](experiments/mutation_results.json)、[补充实验](experiments/probe_results.json)。

**新增问题：严重 0、高 0、中 0、低 0。** `findings.json` 为 `[]`。本轮不重新评判差异以外的既有问题，也不把测试通过解释成覆盖了所有线程交错或网络条件。

## 2. 修复核实

“已修复”针对上轮报告的触发路径和根因。机器可读结果见 [verification.json](verification.json)。

| 编号 | 结论 | 证据 | 说明 |
| --- | --- | --- | --- |
| AUD13-001 | 已修复 | 【代码核查】hal_net_sim.c:332-343、net_service.c:55-58、66-112；【复现】integration-net-auto 26 项通过；mut_cell_notify exit 1、test.fail=1。 | 蜂窝注册完成会唤醒自动选网；去掉未选中蜂窝网络的通知后，同一回归在 452.010 s 无法切到 LTE。 |
| AUD13-002 | 已修复 | 【代码核查】ai_service.c:201-205、239-245、589-607；【复现】integration-ai-race 10 项通过；分别撤回停采集和松键保护的两个负对照均 exit 1、test.fail=1。 | 长按跨过断线检测时停止采集、关闭会话；松键保留失败提示，恢复网络后的下一轮正常。原跨轮会话关闭检查仍通过。 |
| AUD13-003 | 已修复 | 【代码核查】remote.c:139-175；【复现】integration-lifecycle 114 项通过；mut_remote_restore exit 1，前画布像素错误地变为 0（应为 255）。 | 恢复反显仅操作前画布；在部分 B 已解码、LAST 尚未收到时不再提前展示 B。 |
| AUD13-004 | 已修复 | 【代码核查】remote.c:494-503、ai_service.c:103-115；【复现】lifecycle 的提示坐标断言通过；mut_remote_note exit 1；人工检查 remote_reconnect 的 23.1276 s 实际帧。 | 顶部断线提示在 AI 回答面板打开时可见。几何断言能检出旧位置，补充实际绘制帧确认。 |

## 3. 新问题

没有确认的新问题，因此不分配 AUD14 编号。`findings.json` 仅包含新问题，保留空数组，不混入已修复项或待验证推测。

## 4. 改动及测试有效性

### 4.1 蜂窝注册通知与 HAL 切网

【代码核查】`hal_net_sim.c:332-343`：所选网络是 LTE 时沿用 `relink()`；否则只标记链路事件并唤醒事件任务，不直接改写当前网络或现有连接。`net_service.c:55-58、66-112` 会重新查询可用网络，解除“Wi-Fi 已消失、LTE 还在注册”后的静止状态。通知使用原有 ISR/锁内唤醒机制，没有新增阻塞操作。

【复现】`integration-net-auto` 的 `net_auto.c:136-143` 覆盖注册完成晚于 Wi-Fi 消失的顺序；只去掉新增通知后，452.010 s 的 LTE 在线断言失败，其他检查完成。该断言能够发现 AUD13-001 的旧根因。

【复现】`integration-hal-switch` 直接打开 HAL WebSocket，在 Wi-Fi 仍可用时切 LTE，等待旧连接的 CLOSED，并检查 LTE 在线；它没有启动 hub 服务，因此不会由 hub 主动关闭连接来掩盖 HAL 的遗漏。恢复旧的提前覆盖 `N.type` 行为后，CLOSED 断言失败。正常构建与 sanitizer 各 3 项检查通过，确实补上了上一轮指出的覆盖缺口。

### 4.2 AI 结束与关闭顺序

【代码核查】`ai_service.c:207-214` 先匹配请求/轮次，再在 UI 任务执行 `turn_over()`（201-205、239-245）；停止采集复用幂等 `end_capture(true)`。每次按下键重置 `turn_over`（541），松键先保留终态（589-607），不会再把已失败的一轮改成“正在想”或“没听到”。旧回调取走旧 sid 的临界区没有改变。

【复现】`integration-ai-race` 既有的跨轮竞态检查仍通过；新增部分（87-105）观察失败、采集/会话关闭、松键后的文字以及下一轮回答。两个独立负对照分别只撤回停采集和松键保护，各造成一个准确的断言失败。这不是只检查一个结束计数。

【代码核查】【复现】`on_rt_closed`（505-518）在当前会话且上传未完成时报告失败；上传完成后由 `ai.listen` 的回复/超时决定这一轮的结果。补充 `ai_close` 在两种构建下各通过 **34** 项检查：

- 长按中关闭：`failures=1, capturing=0, session=0`，松键保持错误。
- 旧 sid 的关闭回调不停止新一轮采集。
- 上传完成、回复尚未到达时关闭：不额外报失败，实际回复随后得到 `answers=1, failures=1, rt=0`。
- 故意中止的旧请求在真实超时到来后不覆盖新一轮的回答。

这是组件回调边界实验：测试主动释放 RT 会话并调用拥有者回调，产品 AI 和启动代码保持原样。它验证状态和文字结果，不证明实网的事件发生率或提前关闭后的声音完整性。

### 4.3 远程画布、提示与锁屏

【代码核查】`remote.c:139-175、424-442`：点按和恢复都只在 UI 任务对 `px` 异或；重复点按先恢复旧反显，收到完整更新也先恢复再复制新区域。没有从部分解码的 `back` 恢复像素，也没有引入额外缓冲或新的释放路径。

【复现】`lifecycle.c:624-632` 明确检查 B 的部分像素已经进入 `back`，然后真实执行 `spot_expired`，确认 `px` 仍保留已提交的 A；收到 LAST 后再确认 B 和配额。将恢复改回复制 `back` 后，恢复像素为 0 而应为 255，测试失败。

【代码核查】【复现】提示对齐改为 `LV_ALIGN_TOP_MID`（`remote.c:502`）。`lifecycle.c:508-513、661-665` 检查未隐藏和提示底边低于屏幕 42% 分界；改回底部后测得 `1185 of 1216` 并失败。**该测试检查几何和隐藏标记，没有渲染真实 AI 面板或逐像素判定遮挡**。为避免扩大其保证，本轮另人工检查原 `remote_reconnect` 场景 **23.1276 s** 的实际帧：顶部“连接断了，正在重连…”与底部 AI 回答面板同时可见。截图：[remote-offline-visible.png](experiments/remote-offline-visible.png)。

【代码核查】【复现】锁屏时打开页面的新判断（`remote.c:524-528`）与现有电源监听（405-419）配合。补充 `remote_locked` 在两种构建下各通过 **21** 项检查：实际调用 `remote.open` 后为 `suspended=1, sid=0, rt=0`；解锁后只建立一个会话并收到画面；锁屏中先关闭页面再解锁，没有会话或画布残留。实验为了观察状态额外持有电源需求，未验证实机睡眠和耗电。

### 4.4 接听、挂断的截止时间

【代码核查】`comm_service.c:272-274` 将这两个能力从默认 3 s 改为 25 s，没有改变执行器或 AT 串行锁。`cap.c:330-401` 的超时仍从接纳请求开始计算：排队时超时的操作不再执行；已运行但超时的操作返回“may still finish”，不谎报成功。处理函数和模组驱动的等待行为属于既有实现。

【复现】补充 `comm_deadline` 在两种构建下各通过 **23** 项检查。用信号量占用后台执行器 20 s 后，真实模拟模组上的接听和挂断分别成功一次，回执耗时约 **20040 ms**；另一挂断排队超过 25 s，只在约 **25000 ms** 回失败一次，执行器恢复后没有执行该操作。未用一个改过的 timeout 常量反推结论。

这是队列等待实验，不是实网 SMS 发送时长测试。25 s 是总截止时间，不能保证多项任务排队、模组等待或无线环境不佳时必然接通；本批文档没有作这种保证。未发现此次增加时限引入新的严重或高级别问题。

## 5. 构建、负对照与证据索引

| 验证 | 结果 | 证据 |
| --- | --- | --- |
| 普通配置、编译、CTest | exit 0；57/57，28.39 s | [baseline_results.json](experiments/baseline_results.json) |
| ASan/UBSan 配置、编译、CTest（GUI 关闭） | exit 0；56/56，13.92 s | 同上 |
| ai_race / lifecycle / net_auto / hal_switch | 两种构建均 10 / 114 / 26 / 3 项检查，无失败、无模拟器警告 | 同上 |
| 六个负对照 | 编译成功；每个 exit 1、test.fail=1、test.finished=1 | [mutation_results.json](experiments/mutation_results.json) |
| 三个补充边界实验 × 两种构建 | 六次均 exit 0、test.finished=1、无警告 | [probe_results.json](experiments/probe_results.json) |
| 远程断网提示 | 人工查看真实绘制帧，提示可见 | [截图](experiments/remote-offline-visible.png) |

【复现】sanitizer 的 CTest 原始日志与三个补充实验日志未出现 `AddressSanitizer` 错误、`LeakSanitizer` 报告或 UBSan `runtime error:`。构建日志存在第三方库的 macOS 弃用提醒和链接器重复库提醒；本报告中的“无警告”仅指上述运行结果中的模拟器警告，不代表第三方构建日志完全为空。

【文档核查】上一轮处理表对四项修复及新增 HAL 测试的描述与本轮证据吻合；本批详细设计将断线提示位置改为上方，与实现一致。对测试能力的判断采用上面的限定，不把坐标检查扩大成通用视觉回归。

复现脚本和实验边界见 [experiments/README.md](experiments/README.md)。完整构建日志、二进制和运行目录保留于 `build-audit/`、`build-audit-asan/`，未复制进审计交付目录。

## 6. 审计对象、运行命令和未查范围

对象为 **`9cf64ad..99495a8`**，完整提交号见报告开头。审计覆盖这段差异中的 `ai_service.c`、`remote.c`、`hal_net_sim.c`、`comm_service.c`、CMake 测试注册以及四组集成测试和相关脚本/预算。对 RT、能力总线、网络服务、驱动和电源的读取仅用于解释本批改动的调用边界，没有重启全面审计。

实际执行的核心命令如下；完整展开参数和退出码另见 `experiments/baseline_results.json`：

```bash
AUDIT_DEPS=/Users/bytedance/Documents/Workspace/ink
cmake -S . -B build-audit -DINK_WERROR=ON \
  -DINK_CJK_FONT=$AUDIT_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE=$AUDIT_DEPS/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$AUDIT_DEPS/build/_deps/lvgl-src \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=$AUDIT_DEPS/build/_deps/imgui-src \
  -DFETCHCONTENT_SOURCE_DIR_SDL2=$AUDIT_DEPS/build/_deps/sdl2-src \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build-audit -j 8
$AUDIT_DEPS/build-deps/pyenv/bin/ctest --test-dir build-audit -j 8 --timeout 300 --output-on-failure
```

sanitizer 配置使用相同参数，目录换为 `build-audit-asan` 并加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`；其构建和 CTest 也使用 `-j 8`、`--timeout 300`。所有依赖来自任务指定的本机路径。

```bash
$AUDIT_DEPS/build-deps/pyenv/bin/python docs/audits/2026-11-04-codex-system-r4/experiments/run_checks.py mutations
# 三个补充实验各运行一次；sanitizer 再分别加 --build build-audit-asan
$AUDIT_DEPS/build-deps/pyenv/bin/python docs/audits/2026-11-04-codex-system-r4/experiments/run_checks.py probe --name ai_close
$AUDIT_DEPS/build-deps/pyenv/bin/python docs/audits/2026-11-04-codex-system-r4/experiments/run_checks.py probe --name remote_locked
$AUDIT_DEPS/build-deps/pyenv/bin/python docs/audits/2026-11-04-codex-system-r4/experiments/run_checks.py probe --name comm_deadline
```

另执行 `git rev-parse HEAD 9cf64ad`、`git diff --stat/--name-status 9cf64ad..HEAD`、指定文件差异与行号读取、`git diff --exit-code`、`git diff --cached --exit-code`、JSON/链接/文件大小检查。最初与结束时均为 detached HEAD；没有修改任何既有跟踪文件，没有提交、推送或建分支。

未查：本批差异以外的功能、所有可能的并发交错、实机固件/无线网络/睡眠功耗/硬件时限、其他平台的本轮执行结果。未联网、未读取远端决策板或 Issue/PR，未接触设备或任务禁止的实机工作目录；没有对外发布或实际合入 main。
