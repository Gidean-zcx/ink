# batch12 独立复查报告

（审计子代理，一轮约 1 小时，检出 HEAD `61f55c19`；运行环境不让子代理写报告文件，主会话按它的回报保存。）

已跑：全量构建和测试 `100% tests passed out of 307`；17 个改过的 .c 文件 gcccheck 0 处；gccscan、rscan 只有已知的 `navigation.c` `N.card_objects` 误报。上限：能力 338/512，设置 25/48（设置文件约 0.8 KB/2 KB、约 51/128 个词法单元），应用 17/40，`LIST_CAP` 8192 按每个应用 200 字节正好放下 40 个。计算器定点运算边界、阅读位置按成员分开、AI 回答截断长度都核过没问题。

## 发现

1. **中：AI 用 `settings.set` 可以绕过“先回执、再断网”。** `system/services/net/net_service.c` 里 `net.airplane`、`wifi.enabled` 两项设置不是 system、也没有 `ask_by_hand`，AI 调 `settings.set {"id":"net.airplane","value":true}` 立刻生效：Wi-Fi 马上关、回执可能送不到中枢、AI 面板没有说明；`wifi.enabled` 同理（`tests/airplane_mode.c` 里就有 AI 用 `settings.set` 关 Wi-Fi 的例子）。也不符合“按钮和 AI 走同一个能力”。证据：读代码。修法：`settings.set` 遇到这两个 id 转调专门能力，或给设置加“只能经由专门能力改”的标记，或标成 system 让 `settings.set` 拒绝 AI。
2. **中：孩子在飞行模式下看到的“关闭飞行模式”按钮点了没反应。** `system/ui/shell/airplane_card.c`、`system/ui/shell/ai_pages.c`；`net.set_airplane` 是 system 能力、没设 `child_ok`，闸门（`cap.c`）拒绝孩子，而面板的调用没有 `.done`、`ink_shell_bind` 不显示回执，结果静默失败。证据：读代码，现有测试没覆盖孩子。修法：孩子时不给按钮，改说“请家长关闭飞行模式”，或至少显示拒绝原因。
3. **中（影响待证实）：关闭飞行模式时后台执行器和 AT 锁可能被占 30–90 秒。** `system/services/comm/comm_service.c` 的 `radio()` 在单线程后台执行器上持着 `CM.at` 调 `hal_cell_radio(true, 30000)`，`nt26_radio` 轮询到注册或满 30 秒才返回，没卡、没信号时等满，之后还可能 `sim_switch` 再 60 秒；期间 Wi-Fi 扫描连接、短信、设置的 apply、AI 打开飞行模式的延时作业都排队，拨号要拿 AT 锁也被卡。修法：打开射频只发 `AT+CFUN=1` 就返回，注册交给已有的轮询；至少不要持锁等待。
4. **中：接真实中枢时长回答仍在 1.8 KB 处被悄悄截断。** `hub/ink_hub/gateway/wire.py` `ANSWER_BYTES = 1800`、`hub/ink_hub/voice/flow.py` 按它切文字；`docs/hub/device-protocol.md` 第 75 行还写“一轮 ≤ 1.8 KB（设备 2048）”，和第 30 行的“设备 32 KB”矛盾。设备收不到超长部分，也就不会出现截断提示，端到端“不能静默丢字”没成立。修法：中枢文字上限提到 32 KB（朗读另算），或中枢截断时附上同一句提示；改协议文档第 75 行。
5. **低：** `navigation.c` `apps_walk` 里 `items[TOOLS_MAX]`、`shown[TOOLS_MAX]` 从 2×24 项变成 2×48 项，界面任务栈（10 KB，在 PSRAM）多占约 1.2 KB，AI 取快照时走得到；没有界面栈余量探针，待证实。修法：改成 `static` 或加栈余量探针。
6. **低：** `ai_panel.c` 的 `uint16_t starts[INK_AI_ANSWER_MAX + 1]` 约 64 KB PSRAM，实际最多几百页。修法：上限改成例如 1024 页，超过按“无法完整显示”处理。
7. **低：** `apps/reader/reader.c` 每次存阅读位置多写一次 `reader.last`，内容不变也写；开书时在界面任务上同步写一次。修法：没变就跳过，开书那次交给 `save_cb`。

看过没问题的：飞行模式状态落盘和重启、主会话改的状态栏 4G 判断、`ink_ai_note` 走截断后的追加、`power.saving` 能力仍是孩子不能调、`calc.eval` 只读不改页面、`calc.key` 限定在计算器页。

SEVERE=0 HIGH=0 MEDIUM=4 LOW=3
