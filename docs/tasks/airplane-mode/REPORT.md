# airplane-mode 报告

（Claude 子代理完成；运行环境不让子代理写报告文件，主会话按它的回报整理保存，数字和结论照原文。）

检出 `ink-codex/airplane-mode/repo`，分支 `claude/sub-airplane-mode`，基线 `4099f390`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 省电重启保留 | 通过 | 【实测】`tests/airplane_mode.c` 用同一个闪存镜像跑 write、restart、again 三段：restart 段省电仍开，59 秒不锁屏、第 60 秒内锁屏，再关掉；again 段关闭也保留，61 秒不锁屏 | `tests/airplane_mode.py --build build` |
| 1b 飞行模式关掉 4G、Wi-Fi、蓝牙 | 通过 | 【实测】NT26 模型射频关、`CFUN=4`、未注册；没有网络连接；`wifi.enabled`、`bt.enabled` 都是 0 | 同上 |
| 1c 单独再开 Wi-Fi | 通过 | 【实测】网络设置里点 Wi-Fi，回到 Wi-Fi 且连通、`wifi.scan` 可用，4G 射频仍关；单独开蓝牙后 `bt.list` 可用 | 同上 |
| 1d 关闭后恢复 | 通过 | 【实测】打开前 Wi-Fi 开、蓝牙关，飞行中开过蓝牙，关闭后回到 Wi-Fi 开、蓝牙关；4G 重新注册 | 同上 |
| 1e 重启保持 | 通过 | 【实测】重启后飞行模式、Wi-Fi 关、蓝牙关都在；开机马上把 4G 射频再关（模组自己不记）；打电话仍被拒绝；关闭后从文件读回打开前的状态 | 同上 |
| 1f 打电话、发短信、AI 的提示 | 通过 | 【实测】回执“已拒绝”并说明“飞行模式已打开，关闭飞行模式才能…”；拨号页、写短信页弹出面板，有“关闭飞行模式”和“取消”；断网时问 AI，面板显示“飞行模式已打开”和“关闭飞行模式”，点了真能关 | 同上 |
| 1g 快照 | 通过 | 【实测】控制中心快照有 `飞行模式=1（net.set_airplane）`；网络设置快照有飞行模式、Wi-Fi 和动作 | 同上 |
| 1h 能力表 | 通过 | 【实测】新能力 `net.set_airplane`，新设置 `net.airplane`、`wifi.enabled`、`power.saving`；`PASS: 442 definitions` | `capabilities.py --check` |
| 2 原有测试 | 通过 | 【实测】power-pages、bluetooth-settings、phone-calls、sms、ui-scene-Control、ui-scene-SetNetwork、integration-net-auto、integration-lifecycle、ai-pages 等原样通过；最后一轮全量 `99% tests passed, 1 tests failed out of 302`（`services-mid-capability-doc`：服务设计文档 §1.3 缺新能力），补上后这条和其余 6 条能力表测试 `100% tests passed out of 7`，之后没再整轮跑（主会话合并时补跑） | 全量 |
| 2 内存检查 | 通过 | 【实测】build-asan 13 条 `100% tests passed out of 13` | |
| 2 S31 | 通过 | 【实测】退出 0 | |
| 反向对照 | 通过 | 【实测】7 种改坏方式各让测试失败：省电不落盘、不关 4G 射频、不拒绝拨号、AI 打开时不等回执、不从文件恢复、AI 面板不显示提示、Wi-Fi 开关不影响选网 | |

## 做法

产品代码新增约 292 行、删除约 50 行（预算 300，不含测试和模拟器芯片模型）。

- **省电**：`power.saving` 成为普通布尔设置写进设置文件；`power.set_saving` 走 `ink_setting_set`（按钮、AI、`settings.set` 同一条路）；状态 `power.saving` 保留，含义不变。
- **飞行模式**：新设置 `net.airplane` 和 `wifi.enabled`（默认开），归网络服务。打开时通过各自的设置关 Wi-Fi 和蓝牙，所以之后用原来的开关能单独再开；打开前两者的状态存在 `/user/sys/airplane`（两字节），关闭时照着恢复，重启后也能恢复。选网：飞行模式下不用 4G，Wi-Fi 关着不用 Wi-Fi。
- **4G 射频**：通信服务订阅 `net.airplane`，驱动新增 `nt26_radio`（打开发 `AT+CFUN=4`，关闭发 `AT+CFUN=1` 并等注册），接口新增 `hal_cell_radio`；开机时飞行模式开着就在第一次检查模组时立刻关射频；飞行模式下不切 SIM（切卡会重开射频）；打电话、发短信直接拒绝，短信在存草稿之前就检查。
- **能力 `net.set_airplane`**：按钮和 AI 共用，不弹确认卡，孩子不能用。AI 或远端打开时先给回执，用新增的 `ink_ai_note` 把说明显示在本机 AI 面板上，2 秒后才真正关射频；`ai_service.c` 加了“本轮已经结束就不再显示错误”，断网后面板上的说明不被盖掉。
- **界面**：控制中心“更多”里暖光下面加“飞行模式”胶囊；网络设置最上面加一组开关（飞行模式、Wi-Fi）；状态栏飞行模式下显示飞机，再开 Wi-Fi 时后面跟 Wi-Fi 图标；电话、短信被拒时弹出现成的底部面板（新文件 `system/ui/shell/airplane_card.c`）；AI 断网面板在飞行模式下换成“飞行模式已打开”和“关闭飞行模式”；Wi-Fi 关着时扫描和连接都拒绝并提示。
- **NT26 模型**：支持 `AT+CFUN=4`（关射频、断开通话，`AT+CFUN?` 返回 4，SIM 保持供电，依据 3GPP TS 27.007 §8.2）；`sim_cell_state_t` 加 `airplane` 字段。

## 没做到的

- 紧急呼叫：飞行模式下没有例外（用户后来定了要做，D-119，排在任务 `emergency-call`）。
- `CFUN=4` 没按 NT26/EC718PM 厂家手册核实，按 3GPP 做；真机是否支持、要不要等、重启后是否保留要上机看。
- Wi-Fi 和蓝牙的“射频关”只到系统层：Wi-Fi 是不选它并拒绝扫描连接；蓝牙是断开并拒绝扫描、配对、连接（蓝牙接口没有断电函数，S31 的 Wi-Fi 还是空实现）。真正断电要等真机驱动。
- 开机后到系统第一次发 `CFUN=4` 之间，模组可能已经注册上，这段时间理论上可能收到短信或来电。
- 点了“关闭飞行模式”后，AI 面板要手动关；电话、短信页的提示文字要到重绘时才更新。
- 配网用的设备热点没有和飞行模式联动。
- 网络设置页多了两行开关，SIM 卡那一组挪到第 2 页；没有按画板对布局。

## 改过的已有测试

- `tests/integration/systest.c`：设置表的缓冲 1536 → 2048 字节（多了 3 个设置放不下），期望没改。
- `tests/integration/ai_surface.c`：登记 `{"net.set_airplane", "enabled"}`。

## 新增的机制

- `ink_ai_note`（设备自己在 AI 面板上说话）和“本轮已结束就不再显示错误”的规则；`/user/sys/airplane`；`hal_cell_radio`、`nt26_radio`；被飞行模式拒绝时的统一面板 `ink_shell_airplane_card`（靠回执里固定的说明 `airplane mode is on` 识别）。

## 顺手发现（没改）

- 状态栏判断 `net.type` 时比较的是 `"mobile"`，网络服务发出的是 `"lte"`，所以用 4G 时状态栏显示 Wi-Fi 断开的图标。
- 拨号页的提示文字（y≈961）压在键盘最后一行上。
- rscan 报 `navigation.c` 里 `N.card_objects` 的 snprintf（原有的已知误报）。

## 截图

检出的 `build/test-out/airplane-mode/`：控制中心 `write/clean-control.png`、网络设置 `write/clean-network.png`、状态栏 `write/clean-statusbar.png` 和 `restart/clean-restart.png`、打电话 `write/clean-call.png`、发短信 `write/clean-sms.png`、AI `write/clean-ai.png`（电话和短信面板是正文去重那次小改之前拍的）。

## 要改的文档

- `docs/system/capabilities.md`（已重新生成）；`docs/system/system-services-design.md` §1.3（已加能力和三个设置，另应补一节飞行模式的做法）；`docs/system/ui-dependencies.md` 控制中心“飞行”改已实现；`docs/product/system-ui-spec.md` §3.7 写明三处入口、省电重启保留、紧急呼叫例外待做；硬件调研记 `CFUN=4` 按 3GPP 实现、真机待核。
