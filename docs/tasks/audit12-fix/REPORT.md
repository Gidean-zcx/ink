# audit12-fix 报告

（Claude 子代理完成；运行环境不让子代理写报告文件，主会话按它的回报整理保存。）

检出 `ink-codex/audit12-fix/repo`，分支 `claude/sub-audit12-fix`，基线 `bda23afa`。审计报告见 `docs/audits/batch12/report.md`。

## 验收表

| 编号 | 结论 | 证据 |
| --- | --- | --- |
| 中 1 | 已修 | 【实测】`audit12-m1`（17 项）；去掉转调后 5 项失败（AI 改设置立刻生效，回执是“Wi-Fi：关”） |
| 中 2 | 已修 | 【实测】`audit12-m2`（孩子成员，26 项）；撤掉孩子分支 4 项失败，只撤掉按钮的回执 2 项失败 |
| 中 3 | 已修 | 【实测】`audit12-m3`（18 项）；改回 `hal_cell_radio(on, 30000)` 后 4 项失败：后台作业要 1499 ms，拨号要 10010 ms |
| 中 4 | 已修 | 【实测】中枢 `test_text_answer_uses_delta_and_caps_bytes`（改过）和 `test_over_long_answer_ends_with_the_device_note`（新增）；改回旧切法这 2 条失败；中枢全部 `136 passed` |
| 低 5 | 已修 | 读代码核对：`apps_walk` 三处调用都在界面任务上，不重入；全量通过 |
| 低 6 | 已修 | 【实测】`ai-answer-long` 新加“32 KB 全是换行时显示‘回答无法完整显示’”；build-asan 里撤掉上限判断报 `global-buffer-overflow ai_panel.c in paginate` |
| 低 7 | 已修 | 【实测】`audit12-l7`：开书约 8 秒后写一次、翻回同一页不写、换页写、离开时没变化不写；撤掉修改 5 项失败 |

全量 `100% tests passed out of 311`；build-asan 19 项 `100% tests passed out of 19`；S31 退出 0；gcccheck 12 个文件 0 处（gccscan、rscan 只有已知误报）；能力表 `--check` 通过（446）。产品代码：C 加 136、删 48（净增 88）；中枢 Python 加 14、删 5。

## 做法

1. **中 1**：`ink_setting_def_t` 加字段 `via`（这项设置只能经由这个能力改，开关类，参数 `{"enabled":v}`）。`settings.set` 遇到带 `via` 的设置、来源不是 SYSTEM 时，连同来源、成员、页面版本转给那个能力，回执原样交回；服务内部以 SYSTEM 改设置不受影响。`net.airplane` 转 `net.set_airplane`，`wifi.enabled` 转新加的 `wifi.set_enabled`（共用处理：AI 或远程的调用会断网时先回执、2 秒后生效）。`settings.list` 写出 `"via"`，能力表显示“转给 …”。网络页的 Wi-Fi 开关仍绑 `settings.set`，经转调后和 AI 同一个能力。
2. **中 2**：孩子在飞行模式下的电话、短信面板写“请家长关闭飞行模式”，只有“知道了”；AI 断网页写“请家长关闭飞行模式。书还能继续读。”，只留“继续读”。新加 `ink_shell_refused(rc)`：闸门拒绝时在同一个面板上说原因（叠层 `Refused`，标题“没有做”，原因如“请家长来操作”“今天的时间用完了”“请先解锁”）；`ink_shell_bind` 绑定的所有按钮和飞行模式面板的按钮都接上。只认闸门那几条固定原因。
3. **中 3**：`radio()` 只发打开射频的指令（`hal_cell_radio(on, 0)`）就放锁返回；关飞行模式路上的换卡也不等注册（`hal_cell_select_sim(want, 0)`）；注册由 2 秒一次的定时去看（最多 15 次，共 30 秒），每次只短暂拿一下 AT 锁。**和任务书不同**：已有轮询的通信任务空闲时一次睡 60 秒，后台排进来的重试要很久才注意到，所以改用定时（新增 `reg_at`、`reg_job`）。
4. **中 4**：`wire.py` 的 `ANSWER_BYTES` 改成 32768 减去设备提示和结尾 0；新加 `answer()` 超长时在整字处截断并附上和设备同一句提示；`flow.py` 文字用 `answer()`，朗读另设 `SPEECH_BYTES = 1800` 不变；协议文档第 75 行已改。
5. **低 5**：`items`、`shown` 改 `static`，注明只在界面任务、不重入。
6. **低 6**：`starts` 上限改 `PAGES_MAX` = 4096（不是 1024：已有测试 `ui_shell_regressions.c` 要求 4095 个换行时能翻到 1000 页以上），占 8 KB PSRAM（原来 64 KB）。
7. **低 7**：`last_save` 内容没变就不写（计数 `reader.last_saved`）；开书时不再同步写，改为启动原有的保存定时器；关书时调一次 `save_cb`。

## 改过的已有测试

- `tests/airplane_mode.c`：AI 用 `settings.set` 关 Wi-Fi 后，期望回执是 `wifi.set_enabled` 的“已关闭 Wi-Fi”，并且等回执发出后才真关（任务书指定）。
- `hub/tests/contract/test_protocol.py`：原来断言回答不超过 1800 字节，现在 8 KB 的回答要整段送到。
- `tests/integration/ai_surface.c`：登记 `{"wifi.set_enabled","enabled"}`。
- `tests/ai_answer_long/ai_answer_long.c`：新增全是换行的检查。

## 顺手发现

- 通信任务空闲时睡 60 秒，后台排进来的换卡重试可能晚 60 秒才执行（原有问题）。
- 模拟器里卡拔出后要等下次开射频才认回来。
- 测试打印的长字符串截在半个汉字时 `metrics.json` 不是合法 UTF-8，旧的 run.py 遇到失败时可能自己先崩。

## 要改的文档

已改 `docs/hub/device-protocol.md` 第 75 行、`docs/system/system-services-design.md` §1.3（加 `wifi.set_enabled`）、`docs/system/capabilities.md`。待改：`docs/hub/contracts-step1.md` 第 14 行“回答总计 ≤1800 字节”。
