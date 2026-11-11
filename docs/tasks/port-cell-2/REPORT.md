# port-cell-2 执行报告

基线：`21cbcd4765e332e657ad5cd3867165d0924f345e`。2026-11-11 开始；仅编译和宿主机验证，不碰设备，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】定向通过 | 一字节分块重组、短 EOF、4 缓冲耗尽、归还后重对齐、Transmit；真实板级写入记录验证 500/80 ms；4/4 反向对照均编译成功、断言失败 | `build/test-s31-cell`；`python3 ports/esp32s31/tests/uhci_negative.py build` |
| 2 | 普通全量/S31/gcccheck/隐私通过，ASan 待验证 | 【实测】普通全量 369 项：366 通过、3 项环境跳过；S31（浅睡 ON）编过，镜像 3,314,896 字节，编译诊断 0；gcccheck 宿主机 6/6、S31 5/5，无发现；隐私扫描 0 匹配 | 下方“复现命令”及最终记录 |
| 3 | 【实现】已列出 | 下方真机验收步骤交主会话执行，没有冒称真机通过 | 不操作设备 |

## 没做到的

- 真机验证不在本任务执行范围；DMA 吞吐、刷屏并发、来电/短信通知的实际丢失率、浅睡功耗均未测，不把假 UHCI 当成真机结果。

## 和设计不一致的

- 【实现】坏帧头改为逐字节检查校验和，原厂遇到坏头会丢掉当前缓冲余下数据。原因：DMA 溢出清 FIFO 后仍可能接到被截断帧的尾部，必须能在后续数据里找回帧头。帧协议只有短校验和，没有强同步字；这不保证任意损坏字节下绝无误对齐。

## 改过的已有测试

- `cell.c`：假 UART 改成假的四缓冲 UHCI C 接口，原有 AT 白名单、短信/电话、网络和计量断言保留；增加跨缓冲、短 EOF、所有权、溢出恢复、active/idle 停启检查。
- `board.c`：断轨记录从 2 笔变为 5 笔（关功放/马达、放电开关），精确断言马达 500 ms；恢复仍为 2 笔，新增两次 80 ms 和开机 80 ms 断言，未放宽预算。
- `power.c`：只增加恢复电源连续失败时不得通知 4G 恢复的断言；原有停止/恢复通知顺序保留。
- `cell_negative.py`：链接新的独立接收胶水，4 个既有反向对照未变。
- `tests/simulator/selftest.c`：两组切 P14 的事件整体计入新增的 500 ms 放电（600/700 ms 采样、300/301/350/900 ms 故障/按键），所有原断言保留；另给原有 `%u` 参数显式转成 `unsigned`，消除 RISC-V newlib 类型与宿主机类型差异的 gcccheck 警告。
- `member_wake.c`：按键后等明确的 `active`，沿用既有 2 秒上限，避免固定 300 ms 先于恢复电源结束。`accel_use.c`：122 秒后在实际浅睡时触发抬起事件，134 秒截止，原 135 秒后备按键和“抬起必须生效”断言保留。没有抬高测试超时或资源预算。

## 要用户定的

- 暂无。

## 顺手发现

- gcccheck 找到原有自测输出的一处 newlib 类型差异，已最小转型修正（见“改过的已有测试”）；没有额外产品修复。

## 新增的机制

- 原厂 UHCI DMA 缓冲池及其 C 封装，由任务书第 1 项要求；不新增任务。
- 4 指针接收队列将中断缓冲移交现有 I/O 任务，由任务书第 1 项要求。溢出标记只用于组件清 FIFO 后通知解析器丢弃半帧，不是后台重试。
- 两个固定时序宏沿用原厂名称与 500/80 ms 数值，由任务书第 2 项要求；没有新增用户配置或持久格式。

## 里程碑

- 【实现】已接入未修改的 0.2.2 组件，6 个原始文件逐字节相同；保留原有 I/O 任务，未复制原厂三个任务。共享电源增加放电和稳定时间，恢复失败不通知 4G 恢复。全量及交叉编译正在验证。
- 【实测】首轮普通全量原话：`99% tests passed, 4 tests failed out of 368`，509.09 秒；失败为两项板级浅睡、成员唤醒、抬起亮屏。手动取基线 `metalio_board.c`，用相同编译参数替换板级静态库并重链原测试（不进入 ctest、不依赖 CI 历史），这四项均退出 0，确认与新增电源时序有关；修正事件阶段后定向 7/7 通过，25.35 秒。最终全量待下方补记。
- 为什么不能更小：任务明确要求完整 C++ 组件、C 封装、独立可测缓冲胶水及两个电源路径，产品代码净增超过 100 行。看过保留 UART 环形缓冲的方案，不满足 DMA 验收；照搬原厂三任务会显著扩大改动。当前沿用任务与帧解析，只加必要接口和时序。
- 【实测】S31 浅睡 ON 构建退出 0，原厂输出层仍校验 `690a023`；应用镜像 3,314,896 字节。IDF 静态分类：Flash 3,185,302 字节、DIRAM 159,854 字节、外部 BSS 668,728 字节，不包含运行时堆。完整日志在忽略的 `work/s31-build.log`。gcccheck 两轮分别为 `6 compiled, 0 with findings`、`5 compiled, 0 with findings`。
- 【实测】6 个原厂文件的 Git blob 也与 V 逐字节相同。产品 C/CC/H 净增 123 行，设备 CMake 净增 1 行，合计净增 124 行；不含原样组件、测试和文档，低于 300 行预算。
- 【实测】最终普通全量原话：`100% tests passed out of 369`，870.63 秒，退出 0；366 项执行通过，`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 三项按原有环境门槛跳过。完整记录 `work/ctest-final.log`。按总规则 §6 对基线至 HEAD 扫描，绝对路径/MAC 0 匹配；额外蜂窝标识名称与独立长数字形状扫描均 0 匹配。

## 复现命令

配置按总规则 §5，普通版 `INK_WERROR=ON`；内存版仅换 `build-asan` 并加 `INK_SANITIZE=ON`。两版都复用 `$INK_DEPS/build/_deps/` 中的 LVGL、SDL2、ImGui，只读依赖没有改动。构建使用 `taskpolicy -c utility cmake --build <目录> -j2`。

```bash
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility env UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility env INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" INK_S31_LIGHT_SLEEP=ON bash ports/esp32s31/build.sh
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build boards/metalio_plus/metalio_board.c ports/esp32s31/s31_cell_rx.c ports/esp32s31/tests/cell.c tests/simulator/selftest.c tests/integration/member_wake.c tests/accel_use/accel_use.c
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31 ports/esp32s31/s31_modem.c ports/esp32s31/s31_cell_rx.c ports/esp32s31/s31_uhci.cc ports/esp32s31/hal_power_s31.c boards/metalio_plus/metalio_board.c
taskpolicy -c utility python3 ports/esp32s31/tests/uhci_negative.py build
```

## 要改的文档

- 无共享需求、能力表或决策变更；仅更新本报告、组件来源说明及既有 NT26 版权说明中的过时 UART 描述。

## 和原厂 v2.0.77 的差别

查阅日期：2026-11-11，V 为 `CloudZao/Metalio-E-INK4-Plus@874538e`；以下是【实现】核对，不是上机结论。

- **已一致**：UART2 2 Mbps、只配参数和引脚、RX 上拉；原始组件 0.2.2 的 4 × 1600 缓冲、burst 32、空闲 EOF、owner 回收和 PM 锁；active 开始、idle 停止、发送走 `Transmit`。出处：V `uart_eth_modem.cc:147-165,526-550,795,1170-1233`；本仓 `s31_modem.c:225-291`、`s31_uhci.cc`。TX 与原厂一样是同步 FIFO，组件没有 TX DMA。
- **已一致，未重写**：帧头、序号、SPIC、50 µs MRDY ACK、200 ms 唤醒等待、100 ms ACK 等待、500 ms 空闲，ACK 超时不自动重发。出处：V `uart_eth_modem.cc:739-825,1750-1767`、`.h:379-380`；本仓 `s31_modem.c:162-164,203-264`。停止通知原本就在断轨前：`hal_power_s31.c:156-161`。1 秒模组开机等待原本已有；本次把恢复后的起算点改为恢复通知时刻，与 V `nt26_board.cc:30,130,321-324` 对齐，假时钟显式验证这个截止时间。
- **已补齐**：断 P14 后马达放电 500 ms；P14→80 ms→P05→80 ms→放行恢复，TCA 初始化后 80 ms 才返回给 TPS 初始化。常量同名。恢复失败不通知 4G。出处：V `config.h:81-82`、`power_hw.cc:182-200,242-266`、`epd_board_metalio_eink4_plus.c:270-318,338-352,705-709`；本仓 `metalio_board.c:28,93-105,130-140`、`hal_power_s31.c:187-189`。
- **仍不同（port-cell-1）**：保留一个 OSAL I/O 任务和 1 ms 轮询，没有复制原厂 main/init/TX 三任务、完整工作状态机及动态 SRDY 中断类型切换；通过组件的接收状态和既有活动时间停启。AT、收帧、归还在同一任务，原厂 TX 独立。保留直接 `esp_netif` 的 ETH/DHCP 接入，没有引入 `iot_eth`；重组缓冲仍在 PSRAM，原厂在内部 RAM。原因：沿用本项目分层、任务与内存约束，真实并发性能待验。
- **仍不同（port-cell-1）**：不复制 NAT/波特率/USB 持久配置、模组重启、睡眠参数设置、设备标识查询、多波特率探测和 AT 自动重试；NAT 不符合只读条件就不初始化数据。不发原厂 `AT+CEREG=2`，入站通知依赖模组已有配置。已发 AT 超时或溢出后等迟到终止码/复位才重新开放，防错配和重复付费动作。SRDY 未就绪仍拒发，原厂超时可强制 active。此任务未授权扩大命令白名单或写模组配置。
- **仍不同（port-cell-1）**：TX 队列 8 包（原厂 32）；URC FIFO 8 条，满时拒绝新项，没有补拉。新增队列只承接四个 DMA 缓冲指针，不混入 GPIO/TX 事件。原厂原样组件不改；解析器丢失边界处理和坏头扫描是本项目的胶水差异。
- **仍不同（port-cell-1）**：扩展 `+CME ERROR` / `+CMS ERROR` 诊断保留在 HAL 回复；短信读后不删除；信号为 `CSQ` 换算 RSSI，未冒称 RSRP。这些沿用上次范围，不因本任务扩展。
- **仍不同（电源）**：原厂 v2.0.77 待机只断 P05；本项目按既有 `keep_modem` 策略决定是否断 P14，任务书明确要求这个断轨路径。原厂 `PreparePowerOff` 销毁模组与任务、恢复后可异步重建；本项目保留对象，停止 DMA、清掉未消费缓冲、重置 AT/帧状态，恢复后由既有状态查询带起链路。没有新增恢复任务或重试；也没有新增原厂蓝牙电源复位入口。屏幕总线/SD 的待机停放缺口沿用原状，见 HAL 既有注释。

## 溢出与丢失的后果

【实现】中断只入队缓冲指针或置溢出标记。I/O 任务按序喂解析，每块立即 `ReturnBuffer`。组件在最后一个缓冲归还后清 RX FIFO、重挂链；胶水随后清半帧和半行。坏头逐字节检查，下一个完整帧可重对齐；定向测试覆盖“旧帧尾巴 + 新短帧”。

- AT：已收到的完整回复先处理；丢失时仍等待中的命令返回 `HAL_AT_ERROR`，若已经发出则进入既有失同步保护，禁止自行重发。丢失段若包含终止码，可能一直等到模组复位或电源周期才解除保护。
- URC：丢失通知不会补造，也没有自动补拉；可能漏来电状态或短信到达通知，后续状态查询只能恢复可查询的当前状态。
- 以太网：不完整包丢掉，后续帧继续；TCP 可由协议栈重传，UDP/广播丢包不能由此胶水补回，不能据宿主机测试声称“流量不丢”。

## 分层与资源

【实现】UHCI 和缓冲胶水在 S31 端口，板级时序走 OSAL/I2C HAL；上层蜂窝能力继续使用同一 HAL。DMA 所有权是内部机制，本任务不新增用户动作或能力。组件数据池占 6400 字节内部 DMA 堆，另有描述符和接收队列开销；重组缓冲与现有任务栈继续在 PSRAM。新增峰值内存没有真机测量，未放宽模拟器预算。

## 真机交给主会话怎么验

1. 在获准的测试固件与测试 SIM 上，循环只读 `AT+CSQ` / `AT+CEREG?`，同时由测试环境生成带可对照计数的 URC；持续整屏刷新，核对发送/收到的完整 AT 回复、URC 和错误计数。不要查询或落日志设备/卡标识。
2. LTE 数据链路持续传输可校验的固定测试文件，同时连续整屏刷新；核对文件校验和、IP/链路事件和连接是否中断，检查 DMA 缓冲是否耗尽、恢复后能否继续收包。宿主机假 UHCI 不验证吞吐或 Flash 暂停时的延迟。
3. 由主会话检查 P14 下降前 DMA/4G 已停；下降后马达高 500 ms；上升后至少 80 ms 才到 P05，再至少 80 ms 才恢复屏幕/TPS；4G 恢复通知在电源成功后。确认 active/idle 能释放/重新获取接收 PM 锁。未新增原厂代码之外的电气参数。
