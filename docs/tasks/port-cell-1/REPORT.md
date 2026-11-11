# port-cell-1 执行报告

基线：`858213ff01e10ddbd362c04ac5adf699eedd1478`。执行日期：2026-11-11。仅宿主机测试与编译，未接触设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过（当前定向测试） | 假 UART、AT/payload、URC、收发帧、复位、存储命令拦截；反向对照 3/3 被拒 | `build/test-s31-cell`；`python3 ports/esp32s31/tests/cell_negative.py build` |
| 2 | 验证中 | 宿主机全量首轮 367 项，9 项失败；已修复部分并定向通过，最终普通全量原话：`99% tests passed, 2 tests failed out of 367`，463.57 s；3 项环境跳过；独立 S31 宿主机 28/28 通过；ASan 待补；S31 已编过 | `ctest --test-dir build --output-on-failure -j2` |
| 3 | 【实现】已列出 | 见下文；没有把编译、假模组当成真机验证 | 原厂代码行号及 AT 清单 |

## 没做到的

- 真机验证不在本任务范围。

## 和设计不一致的

- 帧头、序号、XON、SPIC 请求/应答、50 µs MRDY ACK、200 ms 唤醒等待、100 ms ACK 等待和 500 ms空闲来自原厂；原厂 ACK 超时假定已送达，不会重发短信或拨号，本实现相同。
- 使用 IDF UART 环形缓冲和一个 OSAL I/O 任务替代 UHCI DMA、原厂三个任务；直接接 `esp_netif` 的以太网/DHCP 接口，未引入 `iot_eth` 依赖。2 Mbps 吞吐、功耗和唤醒仍待真机验证。
- 不复制原厂 NAT/波特率/USB 配置、模组重启、睡眠参数设置、设备标识查询。NAT 只读检查不符合时停止数据初始化，不修改模组；不做原厂多波特率探测或初始化 AT 自动重试。普通命令超时后等迟到的终止码或复位通知才重新开放 AT，防止错配回复。唤醒等待后 SRDY 仍未就绪会拒发，原厂会强制进入 active 继续发送；这里采取保守行为。
- S31 不执行短信删除，`hal_cell_sms_take` 读后保留副本，与通用 HAL 注释的“读后删除”不一致，这是任务第 6 条明确要求。
- 信号使用原厂 `CSQ`，换算的是 RSSI（`-113 + 2×CSQ`），不是已验证的 RSRP。

## 改过的已有测试

- `wifi.c`：LTE 选择由“退回 none”变为保留 LTE 类型，并增加 LTE 状态、默认网卡切换检查；蜂窝占位已被替换。
- `power.c`：记录蜂窝停止/恢复顺序，断电前停止、恢复电源后恢复，忙时拒绝切电，保留模组电源时不停止；既有音频保护检查保留。
- `stubs.c`：检查仍占位的 WebSocket；原先“不存在模组”的断言由新的真实蜂窝端口测试接替。
- `ui_shell_scenes.c`：选网的“4G”只匹配内容区，避免点中新状态栏；期望仍为 `net.prefer == 2`。
- `ui_cards.c`：开始渲染计数前等首次模组状态，替代固定等待；隐藏卡片零渲染、零提交断言不变。
- `sms_multipart.c`：等两条分片入库再检查缺片时不显示；数量和“不显示”期望不变。
- `audit7_fix/file_boundary.c`：计数清零前先取得并释放同一把文件锁，避免把测量开始前已持锁的操作算入；300 ms 上限不变，本次普通全量已通过该用例，锁预算不变。

## 要用户定的

无。

## 顺手发现

- 【实测】基线产品代码单独编译并替换对应对象后，`ui-scene-ConfirmSecret` 仍报 “snapshot must be a fully presented frame on ConfirmSecret”，与当前同为 6.291 s。未改断言或加 sleep。手动对照将 5 个相关产品源文件从基线提取、单独编译后替换对象；不是完整历史检出。证据在 `build/port-cell-baseline/`，没有注册依赖 Git 历史的测试。
- WebSocket/TLS 仍是既有 S31 占位；本任务接通的是 lwIP IP 网卡，不能据此声称家庭中枢连接已可用。

## 新增的机制

- `nt26-uart` I/O 任务、TX 队列（8 个包指针）、URC 队列（8 条）、串行 AT 互斥和有界握手/等待：任务第 1、2 条要求；栈与大缓冲在 PSRAM。
- `esp_netif` 以太网/DHCP 接口：任务第 4 条要求；网络动作在已有 ESP 事件任务执行，系统选网规则不变。
- AT 有限白名单：任务第 6 条要求；拒绝命令拼接和非短信十六进制 payload。没有新持久格式、配置项、后台重试任务。
- `cell.signal`、`cell.sim_missing`、`cell.config_readonly` 状态：任务第 5、6 条的界面和 AI 可读状态；按钮与 AI 仍走原能力。

## 要改的文档

- `docs/system/capabilities.md`：由主会话更新生成目录，新增上述三个状态，51 → 54 项；同时更新服务状态引用。按总规则第 7 节不在本分支修改。
- `hal_cell_sms_take` 的 S31 保留副本例外、`CSQ` 的 RSSI 证据边界需要合并时同步平台说明。

## 原厂依据（查阅 2026-11-11）

均相对只读原厂目录 `main/boards/`，版本 `690a023`，源文件哈希及 MIT、Apache-2.0 署名见 `ports/esp32s31/vendor/nt26/`。

| 项目 | 原厂文件与行号 |
| --- | --- |
| 引脚 | `metalio-e-ink4-plus/config.h:157–160` |
| 帧头、序号、校验、SPIC 常量 | `common/board/uart_eth_modem.h:216–265` 及文件末尾常量 |
| MAC 派生 | `common/board/uart_eth_modem.cc:25–26` |
| AT 封帧及终止码 | 同文件 `262–332`、`1420–1454` |
| 网卡/DHCP、关闭 GARP | 同文件 `588–655` |
| 唤醒、ACK 超时、空闲 | 同文件 `739–832`、`1090–1236` |
| 分片与校验、50 µs ACK | 同文件 `973–1086`、`1750–1767` |
| 运行时开启网卡及握手 | 同文件 `1637–1676` |
| 电源恢复后等待 1 秒 | `common/board/nt26_board.cc:30,130–137` |
| 断电前停止、恢复后通知 | `common/board/nt26_board.cc:112–119`；`common/power_policy/power_hw.cc:181–218` |

## 上机先核对清单（本次均未执行）

1. 先核对 USB 唯一控制台、UART2 引脚与速率、P12 为高、P14 保持供电，不能在放音中切 P14。用逻辑分析确认 MRDY/SRDY 极性和 ACK 脉冲，再看帧回应。
2. 只读依次核对 `AT+CGMM`、`AT+CGMR`（型号与版本）、`AT+CPIN?`（卡）、`AT+CEREG?`（注册）、`AT+CSQ`（信号）、`AT+ECNETCFG?`、`AT+ECNETDEVCTL?`（现有 NAT/网卡状态）。不查询或记录设备/订户标识。
3. 短信只读检查 `AT+CMGF?`、`AT+CMGF=?`、`AT+CSMS=?`、`AT+CNMI?`；IMS/VoLTE 先读 `AT+CIREG=?`、`AT+CIREG?`，固件拒绝时记录“不支持”，不得猜写 IMS、频段或保存命令。
4. 原厂没有短信/通话实现，也没有已知模组到音频芯片的声音线；不能把拨号 AT 成功当成双向音频可用。发短信和拨号需主会话先取得用户对费用的明确授权。
5. 真机验证 DHCP、无 Wi-Fi 时路由、回到 Wi-Fi、拔卡、飞行模式、P14 恢复；观察持续 2 Mbps 收包、内部堆、队列满时丢包和锁屏功耗。浅睡默认关闭，不在本任务打开。

## S31 AT 清单（穷尽白名单）

【实现】发送入口统一检查 `s31_modem.c:ink_allowed`。下表含允许但没有自动调用方的诊断命令；本次只在假 UART 上执行，没有向设备发送任何 AT。**允许的写存储命令：0 条。**

| 命令 | 类别 | 谁触发 |
| --- | --- | --- |
| `AT` | 只读探测 | HAL 诊断调用；没有自动循环 |
| `ATI` | 只读信息 | HAL 诊断调用；不自动查询或记录回应 |
| `AT+CGMM` | 只读型号 | HAL 诊断调用 |
| `AT+CGMR` | 只读版本 | HAL 诊断调用 |
| `AT+CPIN?` | 只读卡状态 | comm 的状态刷新 |
| `AT+CEREG?` | 只读注册 | 状态刷新、射频开启后的注册等待 |
| `AT+CSQ` | 只读信号 | 状态刷新 |
| `AT+ECSIMCFG?` | 只读槽位 | 状态刷新 |
| `AT+ECNETCFG?` | 只读现有 NAT 配置 | 系统选中 LTE 后的数据初始化 |
| `AT+ECNETDEVCTL?` | 只读网卡状态 | 同上 |
| `AT+CMGF?` | 只读短信模式 | HAL 诊断调用 |
| `AT+CMGF=?` | 只读支持的短信模式 | HAL 诊断调用 |
| `AT+CSMS=?` | 只读短信能力 | HAL 诊断调用 |
| `AT+CNMI?` | 只读短信通知方式 | HAL 诊断调用；不设置通知配置 |
| `AT+CIREG?` | 只读 IMS 注册 | HAL 诊断调用；取决于固件支持 |
| `AT+CIREG=?` | 只读 IMS 查询能力 | HAL 诊断调用；取决于固件支持 |
| `AT+CLCC` | 只读通话列表 | HAL 诊断调用；异步同名前缀也分发成 URC |
| `AT+CMUT?` | 只读静音状态 | `call.mute` 写后回读 |
| `AT+CFUN=1` | 运行时开启射频 | 飞行模式关闭、既有紧急呼叫流程 |
| `AT+CFUN=4` | 运行时关闭射频 | 飞行模式开启、既有紧急呼叫结束流程 |
| `AT+ECNETDEVCTL=2,1,1` | 运行时开启串口网卡 | 系统选中 LTE、注册成功且现有网卡未开启 |
| `AT+CMGF=0` | 运行时选择 PDU 模式 | 既有 NT26 短信发送/读取驱动；没有保存命令 |
| `AT+CMGS=<n>` + 十六进制 PDU + Ctrl-Z | 运行时、可能计费 | `message.send` / `sms.send` |
| `AT+CMGR=<n>` | 查询短信（模组可能标为已读） | 收到 `+CMTI` 后读取；不删除 |
| `ATD<number>;` | 运行时、可能计费 | `call.dial` / `phone.emergency` |
| `ATA` | 运行时、可能计费 | `call.answer` |
| `AT+CHUP` | 运行时挂断 | `call.hangup` |
| `AT+CMUT=0` | 运行时取消麦克风静音 | `call.mute` |
| `AT+CMUT=1` | 运行时麦克风静音 | `call.mute` |
| `AT+VTS="<key>"` | 运行时通话按键 | `call.dtmf` |

`<n>` 限 1–5 位十进制数字；号码限可选 `+` 和最多 20 位数字；按键限 `0–9*#ABCD`。包含分号拼接、CR/LF、非十六进制短信 payload、保存命令、APN、SIM 写命令、短信删除、频段和 IMS 写命令均被拒。通话结算及音频能力没有实机结论。

## 为什么不能更小

原厂帧协议是有状态的二进制 UART 通道，不能用逐行 AT 替代；还需将异步 Ethernet 发送、串行 AT、URC 和复位归入同一 I/O 所有者。检查过的更小方案：直接复用逐行 `hal_uart_read_line` 不满足帧协议；直接引入原厂 C++ 类会带入 UHCI、额外任务及会写配置的初始化分支。本实现保留一个 I/O 任务、现有 comm 任务的状态刷新和现有 ESP 事件任务，未增加初始化/重试任务。最终产品代码行数与完整检查结果见验收记录。

## 复现命令与当前证据

所有构建与 CTest 均加 `taskpolicy -c utility`，并行上限都是 2；没有运行带窗口模拟器。字体、Python、LVGL 路径按任务总规则使用 `$INK_DEPS`。

- 普通构建：总规则的 CMake 参数（另指定已存在的 SDL2、ImGui 源目录，避免重复下载），`cmake --build build -j2`；`build/build-final.log`。
- 普通全量：`INK_KEEP_EVIDENCE=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；`build/ctest-final.log`。失败为基线截图用例与共享目录过期；环境跳过 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`。
- 独立端口：`cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`；`cmake --build build-s31-host -j2`；`ctest --test-dir build-s31-host --output-on-failure -j2`。原话 `100% tests passed out of 28`，12.31 s；`build/s31-host-ctest.log`。
- S31：设置总规则中的依赖、原厂路径后执行 `bash ports/esp32s31/build.sh`，最终再执行同一脚本核对所有修改（增量构建仍需激活 IDF 环境）；日志 `build/s31-build.log`、`build/s31-final.log`，只编译。USB 唯一控制台配置已在生成配置中核对。
- 截图：`INK_KEEP_EVIDENCE=1 ctest --test-dir build -R '^cell-status$' --output-on-failure`；`build/test-out/cell-status/` 的 `cell-wifi-4g-strong`、`cell-wifi-4g-weak`、`cell-no-sim`、`cell-no-service`、`cell-airplane` 各有 `snap-`（模拟面板）和 `clean-`（无残影布局）PNG。后者只作布局参考，不代表实屏效果。只读配置 UI 使用明确的 S31 元数据夹具，实际 S31 HAL 标志由假 UART 测试核对。
- 手工基线对照不注册进 CTest：`INK_DEPS=... python3 build/port-cell-baseline/compare.py`；`ConfirmSecret` 同样失败，短信与目录计时基线通过。对照文件在构建目录，未提交。
