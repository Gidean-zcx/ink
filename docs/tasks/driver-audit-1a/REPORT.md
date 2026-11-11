# driver-audit-1a 审计报告

日期：2026-11-11。状态：审计完成，待复核；风险未修复、未真机验证。

基线：`87bf48655ee4820837b0f54a43c8f5aed37278c2`。原厂对照：`CloudZao/Metalio-E-INK4-Plus@874538eb54f7f7eb593cd40cd1ba6dc8497226e7`（v2.0.77）。只读检查，不修改产品代码、测试或构建文件，不接触设备。

【实现】表示代码核查；【实测】仅表示本次宿主命令结果；【估算】表示风险推断或工作量，均不代表真机验证。原厂路径以该提交仓库根目录为起点，我们的路径以本仓库根目录为起点。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 完成【实现】 | C01–C14、W01–W06、B01–B07 共 27 行，覆盖全部指定文件/流程；双方出处及证据边界见表 | 下文 E1/E2/E3：`git show`、`nl -ba`、逐字节比较 |
| 2 | 完成【实现＋估算】 | 10 项排序风险、P0/P1 拷贝清单、只读/脱敏/宿主测试/资源与工期评估；C08 单独分析 | 下文 E2/E3：双方控制流检查、`wc -l` |
| 3 | 待最终提交核对 | 预期仅新增本 REPORT；下文 E4/E5 记录结果 | `git diff --name-status 87bf4865..HEAD`、`git diff --check`、总规则隐私命令 |

## 逐模块对照

路径缩写：`C/` = 原厂 `main/boards/common/board/`，`P/` = 原厂 `main/boards/common/power_policy/`，`S/` = 我们 `ports/esp32s31/`。表中行为均为【实现】，后果为【估算】，未另标注的差异没有本次真机证据。

### 4G 模组

| 模块/流程 | 原厂做法（文件:行） | 我们的做法（文件:行） | 差异 | 可能的坑（场景 → 后果） | 严重度 | 建议 |
| --- | --- | --- | --- | --- | --- | --- |
| C01 帧格式、分片重组 | `C/uart_eth_modem.h:201-246` 四字节头；`.cc:958-1085` 跨 DMA 重组、逐帧 ACK；头损坏丢本缓冲余部 | `S/s31_modem.c:130-188,209-226` 同一头、1600 字节上限、跨块拼接；头损坏滑窗重扫 | 我们重写但正常帧等价；损坏恢复不一样 | 丢字节后滑窗可能把负载认作合法头；两者均未实现协议头 continue/flow 位的完整语义，不据此声称本项目支持额外分片协议 | 中 | 直接拷 `uart_eth_modem.*`，保留 HAL；损坏帧行为随原厂，不再另写解析器 |
| C02 MRDY/SRDY、ACK、空闲 | `C/uart_eth_modem.cc:741-832,1089-1231,1761-1808` 事件主任务与独立 TX；低电平活动、50 µs ACK；待激活超时强制活动 | `S/s31_modem.c:205-279` 1 ms 轮询单任务；发送时等待 200 ms 唤醒、100 ms ACK；空闲 500 ms | 我们不一样；背景已给强制发送修复的真机依据，不能视为整个流程等价 | 收满载以太帧时，同一个任务同步 TX，期间不归还 RX；4×1600 字节接收池可能耗尽；优先级从原厂主任务 10/TX 9 降到 5 | 中 | 直接拷原厂主任务/TX 和握手流程，替换轮询重写；不要把已修的“拒发”重报 |
| C03 UHCI 本体、DMA、缓冲归还 | 原厂 `components/78__uart-uhci/src/uart_uhci.cc:168-255,319-397,437-562` 内部 DMA 缓冲、owner 检查、溢出归还后重挂 | `vendor/uart-uhci/` 已直接拷；`S/s31_uhci.cc:10-51` 4 指针队列，`S/s31_cell_rx.c:3-12` 消费后归还，溢出后通知解析器丢半帧 | 本体一样；桥接我们不一样 | 启停 DMA 会重新挂载全部缓冲；必须先排空旧指针。当前 suspend 已停 DMA 再排空，不能重报为未做；原厂 Transmit 也是同步 FIFO，并非 TX DMA | 低 | 保留原文组件；桥接随整驱动替换缩减，保留溢出到 HAL 的可观察错误 |
| C04 中断与闪存访问 | `C/nt26_board.cc:41-46` GPIO ISR 服务 flags=0；`C/uart_eth_modem.cc:1782-1808` 注释禁止强开 IRAM 中断；UHCI 分配 flags 清零 | `S/s31_modem.c:205-208,295-304` GPIO flags=0，ACK 计数在内部静态区；`S/s31_uhci.cc:7-17` 对象/队列入口内部静态区；状态和重组在 PSRAM、普通任务栈外置 | 中断屏蔽策略一样；任务/状态布局不一样 | 不能仅凭 IRAM_ATTR 或原厂 sdkconfig 的 GDMA_ISR_HANDLER_IN_IRAM 推断 Cache off 时 ISR 会运行；也不能把背景核 1 Cache error 归因于此 | 中 | 保留原厂中断分配；以实际 ELF/map 和中断表核对。不要以“全加 IRAM”修复，见后文证据边界 |
| C05 AT 收发、超时、负载 | `C/uart_eth_modem.cc:262-399,1312-1357,1423-1453` 串行 SendAt、发送完成引用计数、字符串回复；普通 SendAt 仅取含最终结果的帧 | `S/s31_modem.c:96-128,307-359,393-443` 行累积 4096 字节、caller cap 检查、超时隔离；另支持 CMGS prompt+PDU | 我们不一样（安全/短信扩展） | 原文 SendAt 不支持 `>` 后正文；盲换会让现有短信失效。跨帧的内容与 OK 分开发回时，原厂普通 SendAt 也可能丢前帧内容 | 高 | 拷底层帧与生命周期，HAL 保留允许列表、超时结果、prompt/PDU 适配；明确不能把原厂 SendAt 当现有 payload API 的等价实现 |
| C06 URC、数据通道掉线 | `C/uart_eth_modem.cc:1474-1520` 处理 CEREG；ECNETDEVCTL 1/0 分支本身为空；`C/nt26_board.cc:199-221` 上层错误恢复 | `S/s31_modem.c:96-107` 仅名单内 URC；`S/hal_cellular_s31.c:104-140` 不处理 ECNETDEVCTL；`S/s31_modem.c:514-517` up 依赖旧 ip/handshake/registered | 我们缺数据通道通知；原厂也未在该 URC 内恢复 | 模组只重启数据通道、注册仍是 1 且尚无 LOST_IP → 旧握手和 IP 继续被当作 up，30 秒注册查询也不能发现真实数据中断 | 高 | 拷原厂基础流程仍需单列“数据通道状态与 DHCP 恢复”缺口；先上机验证，不宣称原文已解决 |
| C07 开机初始化与探测 | `C/uart_eth_modem.cc:1523-1633` AtDetect、NAT/波特率检查、必要时重启模组、CFUN=1、查卡/注册、CEREG=2 | `S/s31_modem.c:281-305,393-439,555-577` 探测速率并保留现有配置，NAT 不为 1 直接退出；`S/hal_cellular_s31.c:51-68` 查卡/注册；`system/services/comm/comm_service.c:378-390` 安排运行时 CFUN | 我们不一样；保护模组配置是既有任务要求 | 从未被原厂配置过的模组不能自动建立 NAT；不开 CEREG=2 时依赖 30 秒轮询；没有 ECSCLKEX 配置时，500 ms MRDY 空闲不能证明模组省电 | 中 | 拷原厂初始化顺序，配置写入必须在内部策略闸口拦截，不能靠只包装公开 HAL；缺配置应结构化报告不可用，不暗中写模组 |
| C08 数据启动、保活重启 | `C/uart_eth_modem.cc:1636-1675` 查询 state=1 直接置 handshake_done，不发 SPIC；否则启动+SPIC，再等 ACK | `S/s31_modem.c:555-588` state=1 同样直接置 handshake，否则启动+SPIC；无网等待恢复 | 一样（共享风险） | 主控重启而模组存活 → 旧通道被视为新握手；背景已观察等不到 DHCP。原因见专项分析，不能靠整份照抄修复 | 高 | 原文替换仍保留此阻塞项；先区分 SPIC 会话、以太帧和 DHCP 到哪一步，未经真机证据不改协议或定强复位策略 |
| C09 网卡、DHCP、DNS | `C/uart_eth_modem.cc:588-669` iot_eth+glue、GET_MAC/LINK、关闭 GARP、清 DNS；原厂 `components/espressif__iot_eth/iot_eth_netif_glue.c:42-108` 生命周期事件 | `S/s31_modem.c:454-508` 自写 esp_netif 驱动，已有 handle/回收回调，按 selected+registered+handshake 连接；不清 DNS | 我们不一样，已有 handle 修复不重报 | 主控/网络切换后缓存 DNS 可能沿用旧网结果；已开但失效的通道没有获取 IP 的截止/恢复动作。不能从“没 iot_eth”直接推断 DHCP 必坏 | 中 | 原文 iot_eth 及 glue 一起拷，保留 HAL 选网、状态和流量口径；不要只拷 modem 类遗漏 mediator |
| C10 睡眠、保网、唤醒 | `C/uart_eth_modem.cc:550-583,674-694` MRDY sleep-select、SRDY 低唤醒及停机引脚复位；`P/README.md:78-89,115-147` 最新待机保 P14、停蜂窝驱动 | `S/s31_modem.c:524-546` keep=false 停 DMA并清状态，resume 后等待 1 秒；`S/hal_power_s31.c:93-100,157-193` keep=true 不停驱动，SRDY 配 ANY_HIGH | 我们不一样；无该差异的真机证据 | 若启用浅睡并 keep_modem，SRDY 空闲为高会立刻唤醒，活动为低反而不触发；保电与停驱动语义也不同。当前浅睡默认 OFF，不能说交付版正在触发 | 高（启用后） | 复用原厂硬件停放/恢复步骤；D-37 锁屏保 4G 在线需求须保留，SRDY 极性先核对而非盲抄原厂停蜂窝策略 |
| C11 P14/P12、断电通知 | `C/nt26_board.cc:90-136,321-341` 先 Stop/作废生命周期，再断电，恢复后等 1 秒重建；`P/power_hw.cc:242-265` BT 复位也通知蜂窝 | `S/hal_power_s31.c:157-193` 睡眠支路先 suspend/后切轨/再 resume；`boards/metalio_plus/metalio_board.c:37-44,93-105` P14 仅增 epoch；蜂窝不检查该 epoch（BH 会检查） | 睡眠支路顺序等价；通用电源通知我们缺 | 将来从 BT 复位或其它入口切 P14，若只调板级 set，蜂窝继续持有旧 UART/注册/握手；不能把这一潜在入口当作当前已存在的 BT reset 功能 | 中 | 拷原厂共享复位与蜂窝 Stop/恢复的调用关系，P12 共享复位不得拿来单独救 4G；不新增自行设计的全局机制 |
| C12 出错后重建 | `C/nt26_board.cc:122-154,218-221,286-341` 错误标记、5 秒节流、Stop/Start 与按需 EnsureNetworkReady | `S/s31_modem.c:81-88,190-199,417-439,500-507` epoch 丢队列、30 秒探测；选网/状态查询再次尝试启动 | 我们不一样 | 数据通道假握手或只有 DHCP 失败时 desynced=false，AT 重探不触发；不断查注册不会重建旧会话 | 高 | 拷原厂可停止/可重建生命周期；注意原厂也没有“state=1 但 DHCP 永久不通”的完整恢复，不能夸大效果 |
| C13 SIM 切换、APN、短信电话 | 原厂 `main/display/screen/settings_screen/settings_network_tab.cc:493-544,470-471` CFUN0→500 ms→SimSlot→500 ms→CFUN1，随后倒计时重启；原厂 modem 类没有现有 PDU/电话业务 HAL | `drivers/chips/nt26/nt26.c:50-95,316-437` 在线切卡/注册与短信电话；`S/hal_cellular_s31.c:70-102` S31 切卡/APN 返回 -1；`S/s31_modem.c:49-75` 拦配置写命令 | 我们不一样（明确的只读约束与 R-044）；不是遗漏了原厂切卡时序 | 不能把模拟器在线切卡已实现说成 S31 可用；复制原厂页面会写模组且重启整机，违背本项目在线切卡要求 | 中 | 保留业务 HAL/能力和只读限制；可直接拷原厂命令片段接 HAL，禁止拷 UI/NVS/整机重启。放开配置写入须另有授权 |
| C14 来电/短信报告前置条件 | `C/uart_eth_modem.cc:1474-1520,1620-1621` 只配置/处理注册类 URC，没有本项目电话/短信业务的等价初始化 | `S/hal_cellular_s31.c:34-35,104-140` 查询 CNMI、不设置；只凭 CLIP 发来电事件，RING 被忽略；`S/s31_modem.c:49-75` 不允许 CLIP/CNMI 设置 | 原厂无等价业务；我们的固件前置条件信息不足 | 若模组默认不报 CLIP、或 CNMI 不将入短信转为 CMTI，单有 RING/存入短信不会驱动现有来电/收信事件；本次不能断言手上固件默认值 | 中 | 保留现有安全限制；先只读核对固件报告能力/默认值及真实 URC，不能假定数据联网成功就代表电话短信可用 |

### 开机数据通道已开的专项结论

【实现】原厂 `C/uart_eth_modem.cc:1637-1645` 查询第四字段为 1 后直接把本地握手位置真；`1664-1669` 等到的是自己刚设的位，不是模组新发的 ACK。随后 `1234-1260` 才创建 iot_eth，`657-663` 发 LINK_UP 并触发 DHCP。我们的 `S/s31_modem.c:569-574` 和 `490-498` 重复了这一假设。双方均未在该路径验证 SPIC 新会话，也没有 DHCP 失败后重建通道的流程。原厂 `1516-1519` 对通道启停 URC 也只是空分支。**这是双方共有的未解决点，不是今天五个已修问题之一的新版本。**

【估算】模组状态 1 只证明模组认为通道开着；主控重启后的序号、SPIC 会话、帧接收与 DHCP 能否继续，源码没有证明。可能卡在旧会话，也可能卡在 L2/DHCP；本次没抓包，不能断言某个原因或给协议参数。背景里“等到模组自行重启才通”支持优先核对会话恢复，不足以证明必须断电。

建议顺序：先保留 P14 观察“查询第四字段→是否收到以太帧→DHCP Discover/Offer/Request/Ack”；再在用户授权的真机验证里比较“通道已开时重新 SPIC”与“有依据的运行时数据通道停止/启动”。原厂只给出启动命令，没有验证停止命令及重新 SPIC 的幂等性，故本报告不自行给出停止参数。**整份拷贝仍须带着该项待验证，不能承诺解决。**P14 共享蓝牙/功放，禁止把它当作无副作用的 4G 重连。

### Wi-Fi

额外原厂缩写：`W/` = `components/esp-wifi-connect/`。`wifi_board.cc` 主要是产品/电源调度，实际 STA 参数在 `W/wifi_station.cc`，不能只比 Board 包装类。

| 模块/流程 | 原厂做法（文件:行） | 我们的做法（文件:行） | 差异 | 可能的坑（场景 → 后果） | 严重度 | 建议 |
| --- | --- | --- | --- | --- | --- | --- |
| W01 初始化、凭据 | `W/wifi_station.cc:225-262` STA、nvs_enable=false；`:51-72,124-148` 自有 NVS 设置 | `S/hal_wifi_s31.c:124-148` STA、nvs_enable=0、WIFI_STORAGE_RAM；`:185-200` 8 项已知网络只在内存 | 基础初始化我们重写但等价；持久层不一样（既有只读约束） | 不应复制原厂 NVS 设置代码覆盖我们的存储/成员边界；当前未发现 GPIO 或凭据落 NVS 的新增问题 | 低 | 直接拷 STA 底层片段，保留 HAL 及系统已知网络存储；不复制原厂 Settings/NVS/UI |
| W02 省电、协议、发射功率 | `W/wifi_station.cc:35-38,413-415` 固定 WIFI_PS_NONE 和 11b/g/n；`include/wifi_station.h:16` Plus 功率请求 48（12 dBm），`.cc:427-430` 启动时设置 | `S/hal_wifi_s31.c:132-149` 没 set_ps/set_protocol/set_max_tx_power，依赖 IDF 默认值；日志写 MIN_MODEM | 我们不一样；没有对该参数差异的真机证据 | 默认 MIN_MODEM 可能增加音频/交互延迟；没有原厂 12 dBm 限幅可能改变并发用电/射频表现。不能用源码估算出实际电流或宣称必然复位 | 中 | 直接拷 ApplyWifiProfile 与 Plus 功率设置，先核对目标板与实测；省电由暂停射频与系统策略配合，不能自己重定射频参数 |
| W03 重连、再次发现已知网络 | `W/wifi_station.cc:20-22,291-333,464-485` 每 AP 重连至多 5 次，空队列 10 秒后重新扫 | `S/hal_wifi_s31.c:44-46,81-89,91-117,185-200` 断线立即轮换凭据重连；除设已知列表/手动扫描，无周期重新扫描 | 我们缺原厂扫描/重连节奏 | 家中 AP 不在时先切 LTE；回到覆盖区后 available 仍假，LTE 下不 rejoin，不会自动发现 AP。始终选 Wi-Fi 时，错误凭据又可能不断重连耗电 | 高 | 拷原厂重连和扫描流程，接 HAL 可用性通知；不在多个服务补“定时扫一下” |
| W04 关 Wi-Fi、飞行模式、睡眠 | `W/wifi_station.cc:124-178,180-210` 关/暂停会 stop scan 与 esp_wifi_stop，恢复重新应用 profile；`P/power_hw.cc:284-309` 调用暂停/恢复 | `S/hal_wifi_s31.c:124-148,162-171` 首次总会 start；选 NONE/LTE 只 disconnect，不 stop；`S/hal_power_s31.c:126-199` 无 Wi-Fi 停放调用 | 我们缺原厂射频停止/恢复 | UI 开关/飞行模式显示断开不等于射频关闭，空闲功耗仍在；浅睡打开后也缺暂停/恢复协调。默认浅睡 OFF 不消除“关闭 Wi-Fi 只断开”的差异 | 高 | 直接拷 PauseForLp/ResumeFromLp/用户禁用中的驱动收尾，接现有系统策略；保留 HAL 可调用性，不引原厂 UI |
| W05 与 4G 切换 | `C/dual_network_board.cc:50-79` 只建一个网络板，切换保存选择后重启整机 | `S/hal_wifi_s31.c:162-171,225-242` 同时存在两 netif，切默认路由并选择蜂窝；`S/s31_modem.c:490-507` 控制蜂窝 L2；`system/services/net/net_service.c:95-109` 自动切换 | 我们不一样（产品明确要求）；未用本次真机验证 | 原厂没有可直接复用的在线双网交接；原文 modem 的 InitIotEth LINK_UP 不认识我们的 selected/roaming。照抄 Board 会失去在线切换，照抄 netif 又可能绕过选网策略 | 中 | 保留系统选网策略与 HAL 路由胶水；复制各自底层生命周期，接好 LINK_UP/DOWN、默认 netif 和旧网清理，不复制重启式切换 |
| W06 SNTP、校时来源 | 原厂 `C/wifi_board.cc:111-168` 无 SNTP；`main/ota.cc:276-299` 使用服务响应时间并回写 RTC；`main`/Wi-Fi 组件的 SNTP 搜索无命中 | `S/hal_wifi_s31.c:60-77,119-144,178-183` 公共 SNTP 在 Wi-Fi/ETH GOT_IP 后启动；`system/services/time/time_service.c:526-549,883` 接回调当自动校时 | 我们不一样；`docs/system/system-services-design.md:340` 明确允许真机加 SNTP，同入口 | 公共 SNTP 的时间与“来自家庭服务器”状态混在一起；UDP 对时受网络可达性影响，不能把“启动 SNTP”当作已同步。反复 start 在本地 IDF 中是 stop/init，非必然崩溃 | 中 | 保留 HAL 时间回调（原厂无等价 SNTP 可拷），仅校正来源标识/产品含义；不能复制 OTA、NVS 或时区偏移处理来替代本项目时间服务 |

### BH1098 耳机

额外原厂缩写：`B/` = `main/display/screen/bluetooth_screen/`，`A/` = `main/boards/metalio-e-ink4-plus/periph/`。任务提到的 `docs/bh1098-audio-protocol.md` 在原厂提交中，本仓库没有该文件，以下明确按原厂版本核对。

| 模块/流程 | 原厂做法（文件:行） | 我们的做法（文件:行） | 差异 | 可能的坑（场景 → 后果） | 严重度 | 建议 |
| --- | --- | --- | --- | --- | --- | --- |
| B01 UART、模式、版本命令 | 原厂 `docs/bh1098-audio-protocol.md:23-61,140` UART1 115200/8N1；`B/bluetooth_uart.cc:92-104,365-392` CRLF、700 ms 模式间隔；板级 `config.h:148-160` 引脚和 I2S | `drivers/chips/bh1098g/bh1098g.c:8-38` 相同命令和间隔；`S/s31_board.h:30-34` TX39/RX38；`S/hal_bt_s31.c:53-84` 等 SET MODE 应答；`S/s31_bh1098.h:6-17` 统一 UART1 所有者 | 我们重写但命令/模式时序等价 | 版本函数对固件型号/功能不作保证；当前没有证据把 BH1098G、H 或 mode5/6 当同一能力。未发现 ESP 与 BH 侧 GPIO 号混淆 | 低 | 直接拷模式/版本原函数原文，HAL 所有权和中文结构化结果保留 |
| B02 上电初始化、失败恢复 | `A/metalio_audio.cc:24-27,76-126,135-181` 上电稳 700 ms，模式应答 1 秒，首次失败最多再试 3 次；`:145-150` 开机先共享电源复位；`P/power_hw.cc:242-265` 复位前关蜂窝/音频 | `S/hal_bt_s31.c:74-102` 仅等稳态后发一次 mode1；失败留 mode=0，下一次调用重新尝试；没有原厂开机共享复位 | 我们不一样；无该差异的真机证据 | 开机第一轮命令/应答丢失，首次播放/录音/扫耳机会直接失败；从异常固件状态接管时，也没有原厂复位及有界重试提供的确定起点 | 中 | 拷 `metalio_audio.cc` 初始化/应答/电源通知原文及共享复位函数，接 HAL；复位必须统一协调 4G，不能在 BH 驱动里私切 P14 |
| B03 扫描、连接、配对 | `B/bluetooth_uart.cc:72-89,278-324` 12 hex+名称、INQ COMPLETE、CONNECT SUCCESS/TIMEOUT；原厂 `docs/bh1098-audio-protocol.md:98-118` 无配对/超时完整规范 | `S/hal_bt_s31.c:168-224` 同格式、去重、有界名称，连接等 15 秒；pair=connect 后切 mode1；scan 后也切 mode1 | 基本协议我们重写但等价；15 秒和 pair/scan 收尾我们不一样 | 15 秒是主控自行设定，不能据原厂源码证明能覆盖所有耳机；“配对成功”目前是一次连接成功，不证明独立的芯片配对查询存在 | 中 | 拷原厂扫描/连接协议处理，HAL 保留列表/配对库；超时与自动回连冲突先实机核对，不发明取消扫描/配对命令 |
| B04 主动回连、物理断连 | 原厂 `B/bluetooth_uart.cc:243-275,315-324` 处理多个 mode、回连文本与连接结果；原厂协议 `:59-61,112-114` 明说完整断连事件未定义 | `S/hal_bt_s31.c:46-51,249-262` 后台只用 SET MODE 1 清连接，丢弃 CONNECT SUCCESS、RECONNECT 等其它行；只有同步 connect 路径置 true | 我们缺对已知主动报告的状态汇合；原厂也缺完整断连协议 | 切 mode2 后芯片主动回连或耳机关闭/远离，B.connected 可能与实际链路相反，AI/界面/音频路由继续用旧状态；尚不能指定一个未文档化的断连字符串 | 高（能力证据缺口） | 拷原厂已知应答处理，接服务事件，保留一个 UART 所有者；物理断连报文必须列上机清单，不能捏造“DISCONNECTED”协议 |
| B05 断开、忘记与结果上报 | 原厂协议 `:112-114` 未定义主动断开/芯片删配对；`B/bluetooth_uart.cc:243-249` mode1 应答才更新模式状态 | `S/hal_bt_s31.c:236-241` disconnect 丢 route 返回值、forget 为 void；`system/services/bt/bt_service.c:196-217` 随即报断开/删主机记录；`system/services/audio/audio_service.c:704-728` 另一次路由切换失败会留原路由 | 我们不一样；无失败场景真机证据 | mode1 持续发送/应答失败，蓝牙服务已报断开，音频服务却保留原路由，状态互相矛盾；忘记仅删主机记录，不保证芯片不再回连 | 高 | 复用原厂模式应答事实，HAL/服务保留可报告失败的边界；不把原厂未定义的删配对命令补成假实现，也不直接拷 UI 状态 |
| B06 SCO 通话、录音、音乐 | `B/bluetooth_uart.cc:439-466` PP=1→200 ms→BTSCO=1；反向 BTSCO=0→200 ms→PP=1，不等应答；协议 `:120-136` 同序列且说明语义未全 | `drivers/chips/bh1098g/bh1098g.c:41-45` 同顺序；`S/hal_bt_s31.c:117-142` 额外要求 SETUP/DISC SCO，1 秒；call/capture 共用状态 | 命令顺序等价；强制应答成功条件我们不一样 | 固件切换成功但不发/迟发该文本时，我们会报音频切换失败；手机通话链路与耳机/板载麦具体选路，原厂协议没有证实，不能凭命令说真机已通 | 中 | 直接拷 SCO 原文流程，经 HAL 显式报结果；保留捕获/通话仲裁，先验证应答与实际采音，不自己定新时序 |
| B07 P14、功放、睡眠恢复 | `P/power_hw.cc:242-265` 复位通知蜂窝/音频、放电/稳态后恢复；`A/metalio_audio.cc:225-249` epoch/ready 作废；原厂协议 `:76-84` 低功耗 mode4 未接入 | `S/hal_bt_s31.c:34-36,86-102,111-115,264-280` epoch 检测、耳机时 PA 关、suspend 通知 LOST/resume 作废 UART；`S/hal_power_s31.c:157-193` keep=false 才通知 | 电源感知和 PA 原则等价；待机断 P14 不一样 | keep_modem=false 会连蓝牙一起掉电；不能理解成“只关 4G”。保 P14 不代表 BH 进入 mode4 省电；当前双方都没有完整 mode4 唤醒 | 中 | 拷原厂共享电源/音频通知次序；保留 HAL 的耳机断连事件；不将 BH GPIO20 当 ESP GPIO20，不加未验证的 mode4 流程 |

### 架构与 AI 原生审视（R-061）

【实现】系统服务→HAL→S31 驱动的边界基本合理：`system/services/net/wifi.c:255-267`、`net_service.c:201-214`、`bt/bt_service.c:233-245`、`comm/comm_service.c:816` 提供能力/状态，驱动不依赖 UI 或大模型。UART/GDMA、MRDY/SRDY、DHCP、P14 复位属于内部机制，不应开放为绕过策略的原始 AT 或 GPIO 能力。允许列表、日志过滤、成员/凭据权限应留在 HAL 与系统边界。

主要缺口是状态真实性：C06、W03、B04/B05 会把旧链路状态交给能力和页面，现有软总线不能补救驱动未收到/忽略的硬件事实。复制原厂 Board/UI 会重新把机制与页面耦合，建议只接底层协议和生命周期，原能力/事件/结构化错误继续同路服务按钮与 AI。范围内没有理由把原始 UART/AT 直接开放给 AI。

## 最可能出事的前 10 个坑

以下按本块影响与现实触发条件排序；源码可证的是差异/缺口，硬件后果均为【估算】，没有把背景中的五个已修问题重复排名。

1. **高，C08/C12，共享风险**：主控重启、模组通道仍为 1，双方跳过真实 SPIC 握手且没有 DHCP 失败恢复，可能长时间注册成功却不能上网。
2. **高，W04**：关 Wi-Fi/飞行模式或切 LTE 只 disconnect 而没有 stop，设置显示关了，射频驱动仍启动，耗电与关闭语义不一致。
3. **高，W03**：AP 一度消失后切到 LTE，再回到 AP 附近没有新的自动扫描，自动选网长期留在移动数据。
4. **高，C06**：模组只重启数据通道，ECNETDEVCTL 启停通知被忽略，注册与旧 IP 仍让上层误报在线。
5. **高，B05**：耳机断开/忘记时 mode1 持续发送或应答失败，蓝牙服务报断开并清记录，音频路由仍可能保留耳机，状态互相矛盾。
6. **高（协议证据缺口），B04**：BH 主动回连或耳机离线，后台仅认 mode1，连接状态和 AI/界面的耳机路由可能长期过期；完整物理断连协议双方都未给出。
7. **中，B02**：首次 BH 模式命令/应答丢失，当前调用一次失败就返回，首次录音/播放失败，缺少原厂同一轮有界重试。
8. **中，W02**：Wi-Fi 沿用默认省电/发射功率，未复用原厂 Plus 的固定参数，音频延迟和并发供电表现可能与原厂不同。
9. **中，C02**：4G 在单个轮询任务里同步发一帧才继续归还 DMA 缓冲，持续双向数据下比原厂分离主任务/TX 更容易积压或丢帧；不是已经测出的丢包率。
10. **高（当前默认关闭），C10**：若打开浅睡并保 4G，SRDY 空闲高却配置 ANY_HIGH，可能立即醒来，真正低电平请求不能按预期唤醒。

替换时另一个必须先挡住的问题：**原厂内部初始化会绕过只包在 HAL 外面的 AT 允许列表，并直接记录设备标识**。这不是当前驱动已经在写模组，而是“原文直接接上就安全”的错误假设，见下一节。

## 直接拷贝清单与替换评估

### 直接拷贝清单

| 优先级 | 拷哪些原文 | 接到哪里、保留什么 | 【估算】规模与边界 |
| --- | --- | --- | --- |
| P0 | `C/uart_eth_modem.cc`、`.h`；`components/espressif__iot_eth/` 的两份 `.c`、`include/`、`interface/`、构建/版本文件和许可证 | 接 C HAL 外壳，替换 `S/s31_modem.c` 的协议/握手/网卡重写；`vendor/uart-uhci/` 直接复用已拷版本；保留 `hal_modem_at[_payload]`、URC、cellular HAL、流量/选网/只读策略 | modem 原文 2321 行，iot_eth 源码/头/构建约 680 行；自有接线/安全/短信补口约 200–400 行，撤下当前约 650 行帧/桥接代码。不是改几十行能整换；C08 不会自动消失 |
| P0 | `C/nt26_board.cc` 中 `TearDownModem`、`PreparePowerOff`、`StartOrRestartModem`、电源恢复的原文；`P/power_hw.cc:242-265` 及其板级共享复位实现 | 保留 `S/s31_cell.h` 的 C 边界与现有电源入口；去除对原厂 Board/UI/NVS 的依赖；共用 P14 调用关系，避免 BH 私切轨 | 约 150–300 行原文生命周期片段，约 50–100 行胶水；完整 `Nt26Board` 与原厂 Application/Display 耦合，不能整类无改动落地。与上一项合做，不能重复增加两套恢复任务 |
| P1 | `W/wifi_station.cc` 中 ApplyWifiProfile、STA 初始化、停止/恢复、重连和扫描流程，连同对应头/组件来源声明 | 替换 `S/hal_wifi_s31.c` 的底层重复流程；保留已知网络来自系统服务、RAM 凭据、HAL callbacks、在线双网路由和 SNTP | 约 300–500 行原文片段，50–120 行胶水。原文件直读写 NVS、依赖 SsidManager，整份无改动复制不满足现有存储约束；不能把两种存储都留着 |
| P1 | `A/metalio_audio.cc` 的 ApplyMode/应答观察/电源通知原文；`B/bluetooth_uart.cc` 的模式、扫描、连接应答、SCO 原文；必要的 `main/boards/common/audio/simple_uart.hpp` 接口 | 接 `drivers/chips/bh1098g/`、`S/hal_bt_s31.c`、`s31_bh1098.h`；保留单 UART 所有者、能力/配对库和事件；原厂 UI 调用改为 HAL 结果回调 | 原文约 200–350 行，50–100 行胶水；SimpleUart 会新增串口任务、原厂初始化也有 worker，必须计入资源，不能与现有接收任务并存。完整 bluetooth_uart.cc 强依赖 LVGL/页面，不建议整页一起带入 |
| 保留 | 当前 SNTP→HAL 时间回调、系统 net/bt/comm 能力与 nt26 短信/电话业务 | 原厂没有等价 PDU/通话 HAL，也没有本项目的在线切网、成员/凭据策略 | 这里保留的是产品接口与安全层；原厂现成协议/时序仍原文复用，不改成另一套风格 |

### 整份换成 uart_eth_modem 的可行性

结论：**有条件可行，但无法“只包公开 SendAt、其它一字不动”就满足现有合同**。原厂类不依赖 UI，适合作为协议/收发/生命周期的唯一实现；`nt26_board.*`、Wi-Fi Board 与蓝牙页面则不宜整个搬入系统层。最小可逆顺序是：保留原文副本和来源哈希，先接 HAL 安全/生命周期，再迁移现有测试，最后才安排授权真机验证。本次没有实施替换。

- **依赖与许可【实现】**：根许可证是 MIT；`uart_eth_modem.cc/.h:1-2` 明确是 Terrence 的 **Apache-2.0**，iot_eth 两份源码也为 Apache-2.0，UART UHCI 为 Apache-2.0；Wi-Fi 组件 manifest 为 MIT。不能统称“原厂 MIT”覆盖组件声明。`iot_eth/idf_component.yml` 是 **0.1.0**（来源提交 `40e383f968c13e87e709c9d52a39892eca376e25`），不是注释提到的 1.1；CMake 还依赖 cmake_utilities/package_manager，未取得其固定版本/许可前不能直接下载升级。现有 `S/vendor/nt26/NOTICE` 已保留两种许可；迁移时保留原文版权、完整许可证和修改说明，不加 GPL。
- **AT 安全接缝【实现】**：HAL 外壳继续复用 `S/s31_modem.c:49-75` 的有限命令/参数验证；但原厂初始化在 `1559-1593,1621,1677-1682` 内部直接调用 NAT/USB 配置、改波特率、重启和睡眠参数命令。它们不会经过 HAL 包装。必须在原厂 SendAt/发送边界接入同一个安全闸口，并让被拒的必需配置导致结构化“配置只读/尚不具备条件”，不能沿用原厂对部分失败返回值的忽略。安全接缝是允许的少量本项目检查，协议/时序保留原文；若坚持整个文件逐字节不变，就需要同接口的受控传输适配，而且仍要解决初始化错误传播，工作量不会更小。
- **日志脱敏【实现】**：原厂 `QueryModemInfo`（`1741-1747`）直接输出设备标识；`SendAt:312,389` 超时日志会带命令，iot_eth `iot_eth.c:65-67` 和 glue `:58-59` 输出地址。仅过滤 HAL 返回文本不足以保护日志。建议对复制组件关闭其原始日志输出，在 C 胶水只记固定事件、结果、长度/计数，复用现有敏感命令与位置过滤；禁令要覆盖原厂内部调用。不能只把 INFO 关掉，WARN 的 AT 超时也可能带私人参数。
- **功能补口【实现】**：原厂普通 SendAt 不支持 CMGS prompt/PDU，不提供现有 URC 队列、信号/注册到 HAL、累计 L2 字节接口；netif 私有且 InitIotEth 自动 LINK_UP，也不认识我们的 selected/数据漫游。必须把这些接到原有 HAL，不能偷偷保留第二套帧解析器或绕过系统选网。原厂的字符串回复只有同帧含最终结果才完整，已有跨帧回复累积/超时隔离的验收不能丢。
- **资源【实现＋估算】**：原文 Start 建主任务 4096、TX 3072、初始化 4096 字节栈，稳态至少 7168 字节任务栈，初始化时 11264 字节；当前单个 nt26-uart 是 4096 字节 PSRAM 栈。原文还加 1600 字节内部重组缓冲、32 项事件/32 项 TX 队列与 C++ 同步对象，4×1600 内部 DMA 缓冲则双方本来都有。该 IDF 默认 FreeRTOS 分配走内部 RAM（`components/freertos/heap_idf.c:14-20,43-55`），不能按“复制文件”忽略内部 RAM 增量。TCB、堆峰值及 flash 大小本次未编译测量，不给伪精确总量。
- **工作量【估算】**：熟悉现有 HAL 的一名执行者，原文入库/依赖固定约半天，HAL/安全/负载/选网与生命周期接线约 1.5–3 天，宿主测试接通及反向对照约 1–2 天，合计约 **3–6 个工作日，再加授权真机验证**；不含 C08 根因未知时的额外排查。若连 Wi-Fi/BH 一起换，是另外的工作，不能塞进本次一小时审计。

### 宿主机测试怎么接

【实现】现有 `S/tests/cell.c:12-15` 直接 include 我们的 C 实现，`:99-140` 伪模组只走假帧；`S/tests/CMakeLists.txt:1-4,36-39` 目前是 C 工程。`S/tests/audio.c:147-179` 用假串口验证 BH 命令/应答。它们证明软件合同，不证明真实 DMA、中断或模组固件。

后续替换应让**同一份拷来的 C++ modem 原文**编进宿主测试，C HAL 成为被测入口，现有假 UHCI/GPIO/时钟/esp_netif 可保留，补最小 FreeRTOS 队列/事件/线程适配和 iot_eth 链路事件适配；CMake 需启用 CXX。这不是本次新增的测试框架。保留现有拒绝 AT、付费动作不重发、分块回复、URC、暂停前归还 DMA 和电源通知的断言，只因实现被替换而迁移私有字段断言到 HAL/事件。

最小补验情形：state=1 但 DHCP 不来；ECNETDEVCTL 0/1 在普通 AT 中插入；旧回复迟到；配置不符时所有内部配置写被拦；日志无设备/位置/正文；P14 停止后无旧回调；BH 首次丢 ACK、断开失败、已知回连文本；Wi-Fi 关射频与 AP 回来后重发现。每项修复后做撤销修复的反向对照，不能放宽旧断言。宿主仍不能代替 SRDY 电平、供电、电流、真实 I2S 选路和 Cache error 的实机证据。

## 上机先核对清单

本次不执行以下动作；需要原有设备授权和用户在场的部分留给主会话。

1. 保 P14 的主控重启：记录脱敏后的 ECNETDEVCTL 状态、SPIC 是否真正应答、L2 收发和 DHCP 四步到哪一步；确认第 1 项旧通道假设。
2. 在不写模组持久配置的前提下验证数据通道恢复；原厂未给出的停止参数、SPIC 幂等性先取权威资料/真机证据，不能猜。
3. 模组仅数据通道重启时，确认启停 URC、IP LOST/GOT 和系统在线状态如何先后出现；不可用旧 registered 代替数据可用。
4. 核对 SRDY 低=活动/唤醒、高=空闲/ACK，MRDY ACK 50 µs，2 Mbit/s、实际 UART 波特率和唤醒等待；原厂配置在 `config.h:159-162`，不自己定参数。
5. 同时 4G 数据与 Wi-Fi/音频时，测原厂 profile、12 dBm 限幅与当前默认设置的延迟/电源表现；记录而不自行调整射频参数。
6. 关 Wi-Fi/飞行模式确认驱动确实停止；AP 离开再回来确认自动重发现，不能只验页面状态。
7. BH 冷启动、主控重启但 BH 存活、首次丢 ACK：核对原厂 700 ms/1 秒/重试流程及共享复位对蜂窝的影响。
8. 扫描/连接/主动回连/耳机关机离开，获取脱敏文本并验证事件同步；完整断连报文、芯片删配对、15 秒连接预算原厂均未证实。
9. SCO 切换与真音频：确认 SETUP/DISC 报告、耳机麦/板载麦/I2S 槽和手机通话实际选路；只发命令成功不算通话可用。
10. 浅睡仅在未来明确启用后验证：分别 keep_modem=true/false，核对 P14、SRDY、UART/DMA 停放及来电短信需求；当前默认 OFF 不擅自打开。
11. **Cache error**：本地 IDF `fff9895c82d744c7237be8847347bdd1b07c6643` 中 `components/esp_driver_dma/src/gdma.c:1080-1087` 只有通道 `flags.isr_cache_safe` 才分配 IRAM 中断，UHCI `rx_alloc={}` 没置它；`S/s31_irq.c:53-60` 和 modem 都用 GPIO flags=0。故这些代码不支持“GDMA ISR 在 cache off 仍跑”的归因。下一次需实际交付 ELF/map、崩溃 PC/回溯、中断分配 core/flags 与 PSRAM 栈/数据位置；不读私有设备日志、不本次上机，因而不声称已排除所有核 1 缓存访问来源。
12. P14 共享复位须按原厂保持低电平累计 1 秒、马达放电 500 ms，再稳定上电。出处：原厂 `main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c:199,263-318`；其 `331-353` 最新待机仅切 P5，不复用这套共享复位。我们的 `boards/metalio_plus/metalio_board.c:93-105` 只有放电 500 ms，不能把一次浅睡中止后立即恢复当作已满足原厂 1 秒复位要求。
13. 只读查询来电号码报告/CNMI 支持和当前值，核对 RING、CLIP、CMTI 与电话/短信能力链；原厂未提供的初始化不能由联网成功推导。任何实际拨号/短信测试另按花钱规则授权。

## 没做到的

- 按总说明，本次不运行全量测试、ASan 或 S31 构建；未修改代码，不运行 gcccheck。产品代码改动 0 行。
- 未实机复现 C08、Cache error 或其它风险，未抓 DHCP/URC，也未测耗电、射频、栈高水位和音频选路；不把背景中的已修问题当作新发现。
- 原厂未给完整 BH 断连/删配对、重新 SPIC 幂等性或数据通道停止流程；均已列上机先核对清单。cmake_utilities 的实际固定版本/许可尚未核验，本次没有引入依赖。

## 和设计不一致的

- **实现偏离原厂**：C01/C02 的帧与轮询调度、C10 的浅睡/唤醒、W02/W03/W04 的射频参数与生命周期、B02/B04/B05/B06 的初始化/状态处理，没有本次真机证据支持这些差异；按用户第 10 节要求列原文复用建议，本次不改实现。
- **有既定合同的差异**：R-044/D-44 要在线切卡，D-37 要锁屏保 4G 在线；原厂切卡/切网重启、待机停蜂窝不能原样迁入产品策略。S31 切卡/APN 禁写又是 `docs/tasks/port-cell-1.md:21` 的明确限制，不擅自解除，也不认定产品不需要切卡。
- **校时不属于未授权扩展**：`docs/system/system-services-design.md:339-341` 允许真机 SNTP，同入口；当前 `time.synced` 的“家庭服务器”措辞需与实际来源对齐。本文没有修改规范来迁就状态名。

## 改过的已有测试

无。

## 新增的机制

无。

## 要用户定的

无新增产品取舍。本次推荐 P0 先复用原厂模组与生命周期，保留已有只读、在线切换和锁屏保网约定；是否自动软重启模组/共享复位，应在 C08 真机证据明确后再给方案，当前不暂定有外部影响的自动恢复策略。没有发起确认或等待用户回答。

## 要改的文档

- 后续合并者在 `docs/tasks/backlog.md` 登记 C08 的真机验证、P0/P1 原文替换及 W03/W04、B04/B05 缺口；本任务没有修改它。
- 驱动替换实施后再同步 `docs/system/ui-dependencies.md` 的实机证据边界，以及生成能力表中的可用性/错误结果；本次不改生成文件、需求、想法、决策或已有审计资料。

## 顺手发现

无范围外修复。需要特别保留的既有正确做法：UHCI 已原文复用，DMA 在内部 RAM，RX 在 suspend 前停机后归还；GPIO/GDMA 没有因函数带 IRAM_ATTR 就强开 cache-off 中断；NT26 允许列表与本地许可文件仍保留。本次未把这些已有行为当新漏洞。

## 验证与隐私

### 来源与联网核对

- 【实测，E1】2026-11-11，`git ls-remote https://github.com/CloudZao/Metalio-E-INK4-Plus.git HEAD refs/heads/main refs/heads/master` 退出 0，HEAD 与 main 均为 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`；GitHub REST `repos/CloudZao/Metalio-E-INK4-Plus/commits/main` 也返回此 SHA，提交说明为 v2.0.77，日期为 2026-11-07。**公开主分支没有比本次对照更新的提交**；网页烧录列表的 2.0.78 不能作为可审计源码的替代，未臆造本块新改动。[原厂对照提交](https://github.com/CloudZao/Metalio-E-INK4-Plus/commit/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)
- 【实测，E2】原厂只读克隆开工/核对的 HEAD 均为 `690a023c66862af103a3187464373ee7c5efcd14`，没有移动 HEAD；目标文件通过 `git show 874538e:<路径>` 读取。以下 SHA-256 固定本次主要源码。模组 `.cc/.h` 与现有 `S/vendor/nt26/NOTICE` 的来源哈希一致，不能把“最新对照”误写成这些文件今天有新修复。

| 原厂文件 | SHA-256 |
| --- | --- |
| `C/uart_eth_modem.cc` | `5e9157aa89136be4c4ff3d634fd7d4066b1ea200373b9a60266f603d72c11c26` |
| `C/uart_eth_modem.h` | `00ffb790a900a072e6daeefd0192c361b93d7df13b08669ad4389d2cef47b78d` |
| `components/espressif__iot_eth/iot_eth.c` | `f9df959163f207c0d1c9d463d025fa25eb24bce4d8e99bd551dd907ea5d5cf6f` |
| `components/espressif__iot_eth/iot_eth_netif_glue.c` | `684955d3a35520a974846fdf989b4a5ebf96cb1b375480559659ede106e63880` |
| `W/wifi_station.cc` | `a236bae918d3aa6d54fdd2f50699938cd246ae10d0115390fa146e9f651e928b` |
| `B/bluetooth_uart.cc` | `8e1ad2629b3c37ba2dfe96a676f0cdc178cd23e6de3689c47e302027d3eebc04` |
| 原厂 `docs/bh1098-audio-protocol.md` | `88e0e66e92e885e7c409836804259abc8f0ac1b5472deccd7cb26f579680f856` |

- 【实测，E2】UHCI 六份上游文件（CMakeLists、README、README_zh、manifest、头、源码）与 `vendor/uart-uhci/` 逐字节全同，Python 比较退出 0；哈希与该目录 SOURCE.md 表一致。LICENSE/SOURCE.md 是本项目附加，不混称上游原文件。
- 【实现，E3】手工检查表中控制流，并核查 IDF `esp_wifi.h:698-710` 的默认 MIN_MODEM、`esp_netif_sntp.c:180-189` 的 stop/init、GDMA 分配中断 flags 和 FreeRTOS 内存分配；没有把 sdkconfig/IRAM 注解当实机行为证据。

复核命令示例（只读；`INK_UPSTREAM` 指任务书允许的原厂克隆）：

```sh
export INK_UPSTREAM="$(dirname "$INK_DEPS")/ink-hw/upstream/Metalio-E-INK4-Plus"
git -C "$INK_UPSTREAM" rev-parse HEAD
git -C "$INK_UPSTREAM" show 874538e:main/boards/common/board/uart_eth_modem.cc | nl -ba | sed -n '1636,1686p'
git -C "$INK_UPSTREAM" show 874538e:components/78__uart-uhci/src/uart_uhci.cc | shasum -a 256
shasum -a 256 vendor/uart-uhci/src/uart_uhci.cc
rg -n 'esp_wifi_(start|stop|set_ps|set_protocol|set_max_tx_power)' ports/esp32s31/hal_wifi_s31.c
rg -n 'SET MODE|CONNECT|RECONNECT' ports/esp32s31/hal_bt_s31.c
git diff --name-status 87bf48655ee4820837b0f54a43c8f5aed37278c2..HEAD
git diff --check 87bf48655ee4820837b0f54a43c8f5aed37278c2..HEAD
```

### 收尾检查

- E4：覆盖计数、引用文件/行范围、文内链接、差异范围与空白检查待最终核对。
- E5：按总规则第 6 节扫描提交差异与工作区，隐私结果待最终核对；报告不保存真实设备/网络信息或本机绝对路径。
- 全量测试原话：**未运行（driver-audit-1 总说明明确豁免）**；内存检查：**ASan/UBSan 均未运行**；S31：**未编译**；gcccheck：**未运行（无代码改动）**。这不是通过测试的固件交付；报告完成不等于所列风险已修好。
- 产品代码、测试、构建文件改动 **0 行**；新增任务、线程、状态机、文件格式、配置项 **0**。只新增审计报告，无已有测试期望变更，无反向测试适用项。
