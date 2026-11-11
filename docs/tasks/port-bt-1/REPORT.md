# port-bt-1 执行报告

基线：`d8495ea123f0bbefb0640e20d51044e934942f1c`。只编译和宿主机测试，不接触设备。原厂参考：v2.0.77（`874538e`），查阅日期：2026-11-11。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】部分，协议缺口未伪造 | 已通过扫描/去重、CONNECT 建链、连接前模式 2、退回模式 1、模式/连接/扫描/SCO 超时、SCO 共用、并发音频、P14 恢复、已知模式丢失通知；独立配对、芯片端忘记、耳机自行断开仍缺协议 | `build/test-s31-bt`；`python3 ports/esp32s31/tests/bt_negative.py build` |
| 2 | 验证中 | 普通全量进行中；ASan、S31、gcccheck、能力清单、隐私检查待最终记录 | 见复现命令 |
| 3 | 【实现】已列出 | 原厂差异、只读上机核对、全部实际发送的 BH 命令在下方 | 代码及原厂行号见下方 |

## 没做到的

- **完整耳机失联未完成**：V `docs/bh1098-audio-protocol.md:112-114` 明说完整断连事件未定义，`bluetooth_uart.cc:273-335` 也没有相应解析。仅实现 `SET MODE 1`（原厂将连接置 idle）及 P14 掉电的 LOST 通知。没有把 `DISC SCO` 当成耳机失联，也没有编造 `DISCONNECT` 报文。假芯片测的是已知模式报告，不能代替“耳机自行断开”验收。
- **芯片端忘记未完成**：文档没有删除配对命令。`hal_bt_forget` 对当前耳机退回模式 1，既有服务删除主控数据库；BH 自己的连接记录无法保证清除，可能保留回连记录。
- **独立配对未完成**：没有单独配对命令或配对成功报告。`hal_bt_pair` 用一次真实 `CONNECT SUCCESS` 验证可连接，再退回模式 1；不宣称独立绑定、PIN 输入或芯片持久配对已经验证。模式 1 恢复本机音频，也不能冒称已验证无线链路物理断开。
- 真机验证交主会话：版本适配、实际耳机播放/拾音、NT26 通话音频接线和来源、音质、参考声道、耳机关机/离开距离、共享电源恢复均未测。没有猜测麦克风选择或槽位命令，没有实现模式 3/5 或 ESP BLE 翻页器。

## 和设计不一致的

- 【实现】保留原 S31 的按需模式初始化，没有照搬原厂启动强制复位、后台初始化任务和失败后最多 3 次重试。沿用 `port-cell-2` 的断轨/恢复时机，恢复后第一次使用等 BH 上电满 700 ms，再发模式 1 并等 1000 ms 应答。原厂出处：`metalio_audio.cc:24-27,135-183,225-249`、`power_hw.cc:242-265`。
- 【实现】模式 2 也检查 `SET MODE 2`；SCO 检查 `SETUP SCO` / `DISC SCO`（上限沿用模式应答 1000 ms）。原厂设置页仅固定延时发送，未等 SCO 应答；本实现退回音乐时先等 `DISC SCO` 再等 200 ms 发 `AT+PP=1`，间隔至少为原厂的 200 ms。出处：`bluetooth_uart.cc:365-385,439-466`、协议 `116-136`。
- 【估算】CONNECT 的主控等待上限为 15 秒，以留在既有 20 秒能力超时内；原厂没有给出模块连接超时值，不当作芯片参数。
- 【实现】扫描必须进入模式 2，结束/超时退模式 1；已经连接时拒绝扫描，避免破坏现有连接。原厂页面由用户先选模式 2，扫描完仍留模式 2。出处：`bluetooth_screen.cc:654-669`。
- 【实现】模式 2 不交付未经证明的回采参考声道（`ref=NULL`）；麦克风槽位仍沿用原有 I2S 输入，待实机核对。原厂也明确槽位与麦克风路由未定义（协议 `31-33,59-61`）。

## 改过的已有测试

- `audio.c`：原音频断言保留；假串口改为命令触发应答，新增独立 BH 互斥锁和所有权断言、蓝牙流程、超时、模式丢失、P14、SCO、双线程并发。原先“未连接时请求耳机路由失败”的期望保留。
- `audio_negative.py`：模式应答/电源世代的突变位置由音频文件移到共用管理点，预期仍是撤掉修复就失败；其它反向断言不变。
- `cell.c`：原拨号/接听/挂断/拨号音成功断言拆开，并加 SCO 请求状态；增加 `NO CARRIER` 撤销通话请求的断言。假数据保持不变。
- `power.c`：新增 BH 通知记录和顺序断言；恢复失败不得发 BH 恢复通知、保留 P14 时不发 BH 掉电/恢复通知。原有断言不变。未放宽任何预算或超时。

## 要用户定的

- 需要原厂补充“耳机自行断开报文、查询当前模式、主动断开、删除配对”的协议或有人在场的只读抓包证据，才能补齐剩余验收；当前只发送已公开命令。

## 顺手发现

- 既有 `hal_bt_disconnect` / `hal_bt_forget` 返回 `void`，系统服务无法收到切模式失败结果；本任务未扩大 HAL 契约。
- 配对记录可能由 BH 自动回连（协议 `59-61`），原厂也仅显示 `RECONNECT` 文本，未给可靠地址映射；本任务未猜测自动回连身份。

## 新增的机制

- 一个 BH1098 UART1 管理点和互斥锁：任务第 2 项要求音频/蓝牙共用串口与模式，不再由音频直接发命令。
- 复用现有输入任务的非阻塞接收：连接成功唤醒它，已连接时随现有 10 ms 周期最多读取 4 行、锁忙就返回；任务第 1 项要求事件，无新任务、定时器、队列或后台重试。
- 沿用现有共享电源通知，增加 BH 掉电与恢复通知；任务第 4 项要求。没有新持久格式、用户配置项或独立状态机。

## 要改的文档

- 由主会话更新 `docs/system/ui-dependencies.md` 和 vendor-feature-audit 状态：已接入公开的扫描/连接/SCO；完整失联与芯片删除配对仍有原厂协议缺口，不能标全部完成。
- 系统服务设计 §10.4 关于“尚未见串口协议”的证据描述需主会话更新；需求、决策、能力清单及其它共享文档本任务没有修改。

## 过程与证据

- 【实测】工作区干净，当前分支 `claude/codex-port-bt-1`，HEAD 等于指定基线；基线已合入 `port-cell-2`。
- 【实测】定向音频/电源 3/3、音频/蜂窝 2/2 通过。三项反向对照均编译退出 0、测试退出 -6（断言）：`connect-without-mode2`、`disconnect-without-mode1`、`second-uart-owner`。没有改断言让突变通过。
- 【实现】为什么不能更小：任务要求同一串口的扫描/连接、音频/SCO、异步通知与 P14 恢复，超过 100 行不可避免。看过分别在音频和蓝牙加命令的方案，会形成两个串口所有者；照搬原厂页面与多个任务则超范围。当前只移走原有模式初始化，复用现有驱动、输入任务和电源流程。

## S31 会发送的 BH1098 命令

以下均以 CRLF 结束。V 是任务指定的只读 v2.0.77 副本；路径下文省略公共前缀。

| 命令 | 触发者 | 原厂出处 |
| --- | --- | --- |
| `AT+RX=2` → 等 700 ms → `AT+MODE=1` | 首次音频/蓝牙使用、P14 恢复后首次使用、扫描结束/失败、连接失败、断开、忘记当前耳机、切回外放 | `main/display/screen/bluetooth_screen/bluetooth_uart.cc:365-377`；`main/boards/metalio-e-ink4-plus/periph/metalio_audio.cc:94-126,153-161` |
| `AT+TX=1` → 等 700 ms → `AT+MODE=2` | 扫描、配对时的试连接、连接耳机；成功后音频 ready 不重发模式 1 | `bluetooth_uart.cc:378-385` |
| `AT+INQUIRING` | `bt.scan` | `bluetooth_screen.cc:654-669`；应答解析 `bluetooth_uart.cc:72-89,278-305` |
| `AT+CONNECT=<12 位十六进制地址>` | `bt.connect`、`bt.pair` 的试连接；系统的冒号地址在唯一管理点转换 | `bluetooth_screen.cc:504-509`；应答 `bluetooth_uart.cc:309-325` |
| `AT+PP=1` → 等 200 ms → `AT+BTSCO=1` | 耳机已连时开始采音或蜂窝拨号/接听；已在采音/通话时连接耳机 | `bluetooth_uart.cc:439-451` |
| `AT+BTSCO=0` → `DISC SCO` → 等 200 ms → `AT+PP=1` | 耳机已连且采音、通话都结束；挂断或 `NO CARRIER` 撤销通话请求 | `bluetooth_uart.cc:454-466` |

主控连接超时 15 秒不是新命令；不发未知的 PAIR、DISCONNECT、DELETE、模式查询、麦克风路由或模式 3/5 命令。现有可移植驱动仍保留版本查询函数，但本版 S31 正常路径没有调用 `AT+GVER=1`。

## 上机第一步与核对清单（本次未执行）

1. 主会话先用只读诊断路径读取 BH1098：在启用本版模式初始化之前，由唯一 UART1 所有者发 `AT+GVER=1`，读取 `AT+GVER=V...`；同时被动记录 `SET MODE n`。原厂版本查询出处：`bluetooth_uart.cc:92-104,231-240`。原厂没有公开“查询当前模式”命令；没有模式报告就写“当前模式未知”，不能发送模式切换冒充只读查询。
2. **扫描必须用户在场，只拿用户自己的耳机连接**。确认版本后核对模式 2 应答、扫描地址/名称格式、CONNECT 成功与失败；真实地址只留本地临时证据，不提交仓库。
3. 核对耳机播放时功放关闭；断开退模式 1 后恢复外放；通话/采音 SCO 的两个应答、实际麦克风来源、左右槽与 NT26 通话声音路径。未定义的麦克风/槽位参数仍为未知，不自行设值。
4. 由主会话核对耳机关机/出范围的真实原始应答，再补失联处理；核对原厂未定义的取消扫描、独立配对、断开、删除配对、当前模式查询，不能从名称猜命令。
5. 沿既有流程核对 P14 断前停止、恢复成功后通知、700 ms 稳定及模式 1 应答；不在此任务增加硬件复位或重试参数。

## 复现命令

配置采用总规则 §5（`INK_WERROR=ON`），依赖均从 `$INK_DEPS` 只读复用，所有构建/测试使用 `taskpolicy -c utility`；普通/ASan 仅目录与 `INK_SANITIZE=ON` 不同。

```bash
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_S31_LIGHT_SLEEP=ON taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility python3 ports/esp32s31/tests/bt_negative.py build
taskpolicy -c utility python3 tools/capabilities.py --check
```
