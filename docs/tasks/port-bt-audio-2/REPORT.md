# port-bt-audio-2 执行报告

基线：`1e9a16ee08931d0f86a17c2f852db51e1790c42d`。仅本地修改和提交，不连接设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B02 | 通过【实测】 | 丢首次命令/ACK 可恢复；4 次耗尽，同一电源周期不重启；反向对照失败 | `build-port-tests/test-s31-audio B02-write`、`B02-ack`、`B02-limit` |
| B04 | 部分完成【实测＋实现】 | 已知 CONNECT SUCCESS、mode1 和 SCO 汇合到 HAL；未知离线协议、无地址回连无法完成服务同步，见下 | `build-port-tests/test-s31-audio B04` |
| B05 | HAL 部分通过，整链未完成【实测＋实现】 | 有 ACK 才报 LOST；失败保留最后确认状态；void HAL 不能阻止范围外服务无条件改状态/删记录 | `build-port-tests/test-s31-audio B05` |
| C19 | 模式重设通过【实测】 | 合成 BH 保持模式 2、UART 有旧应答且丢首个模式 ACK，首次播放仍确认模式 1；P14 复位留给 port-board-2 | `build-port-tests/test-s31-audio C19` |
| T01 | 普通版通过【实测】 | 42/42；含已有 audio/bt 负向脚本、9 个新增变异，另手动基线 6/6 失败 | 下方验证命令 |
| T02 | 全量命令通过，补跑中枢依赖项中【实测】 | `100% tests passed out of 391`，388 通过，3 个中枢测试因默认 Python 缺依赖跳过；正用共享现成环境补跑 | 下方验证命令 |
| T03 | 待验证 | S31 固件编译、警告、gcccheck、隐私检查 | 待运行 |

## 没做到的

- 未做真机验证，不保证音质、耳机麦克风槽位或手机通话声音已连通。
- B04：原厂 `bluetooth_uart.cc:273-276` 仅展示 RECONNECT 文本，完整物理离线报文在原厂协议第 4 节明确未定义；没有捏造断连指令。HAL CONNECTED 回调不带地址，服务只处理 LOST（`system/services/bt/bt_service.c:81-84`），未知耳机回连无法安全落到已配对成员记录。
- B05：`hal_bt_disconnect/forget` 返回 void；范围外服务在 `bt_service.c:196-217` 不检查结果就清连接/删记录。范围内已确保失败不发 LOST、不虚构路由成功，但服务和配对库的完整原子一致性仍未完成。

## 和设计不一致的

- 原厂完整源码逐字保存在 `vendor/metalio_bh1098/upstream/`；运行的是从整函数移植的 C 适配版，详见 [SOURCE.md](../../../vendor/metalio_bh1098/SOURCE.md)，不宣称整份 C++ 零改动编译。
- 原厂专属 UART/模式任务、Settings/NVS、页面与产品重启策略不搬入；继续使用单 UART 所有者与现有 PSRAM 音频生命周期。原厂参数和命令顺序保持。
- C19 不在音频驱动单独切共享 P14；没有本任务的实机差异证据，完整共享轨复位由 port-board-2 负责。

## 改过的已有测试

1. SCO 缺应答从失败改为发送成功；原厂明确按 200 ms 发命令，不等待应答。保留并新增实际 UART 写失败断言。
2. 初始化耗尽后必须进入新电源周期才能重试；原厂 s_completed_epoch 的语义，失败次数从 1 改为 4。
3. 已确认的主动断开现在发 LOST，后续累计丢链事件从 1/2 改为 2/3。其余生命周期、负向故障断言保留，另补原厂全部 I2S 槽字段检查。
4. 已有 `audio_negative.py` 的回采、BCLK 两个变异点移到实际编译的 codec.inc；保持 14 个故障和 2 个正向控制的结果要求不变。把已有 audio/bt 负向脚本注册到独立 S31 宿主机 ctest，自动导出它们需要的编译命令。

## 要用户定的

建议由合并主会话另开最小接口配合任务：让 BT 服务在 mode1 确认失败时保留状态/配对库，且回连事件能带经过验证的地址；当前任务不改 HAL 头文件和系统服务。另需取得真实离线报文后才能补物理断连。这里不自行扩范围。

## 顺手发现

【实现】`ports/esp32s31/s31_hal_map.h:34-47` 只以 LF 结束行，原厂 OnUartData 接受 CR 或 LF。当前 HAL 能处理拆包和 CRLF，CR 单独结束的模式 ACK 仍不能处理；共享 UART 行接口超范围，未修改。上机先确认真实固件的应答行结束符。

## 新增的机制

- 模式初始化的一轮最多 4 次和 completed 电源周期标记：任务书 B02/C19、原厂 MetalioAudio 要求。
- 没有新增任务、状态机、文件格式、缓存、队列或配置项；原厂 SimpleUart 接收任务不启用。
- 产品/适配新增 248 行（含注释和空行，连 C 适配文件全部计入，整文件原文副本除外），未超 250。超过 100 的原因：同时替换模式重试、应答、SCO 和 I2S 全部流程；仅给现有单次命令加循环不能完成原厂代码移植，直接编译原厂产品 C++ 则会引入任务/UI/NVS 并扩展构建范围。

## 要改的文档

由合并主会话同步 B04/B05 边界、C19 与 port-board-2 配合、原厂来源及上机结果到 backlog、ui-dependencies 和硬件对照；本任务不改共享文档。

## 上机核对清单

全部由主会话在用户授权下上机；只保留脱敏结果，不记录真实设备地址/名称。

| 场景 | 看什么 | 通过条件 |
| --- | --- | --- |
| 开机首次录音、播放 | 初始化后的首个请求、`audio.mic`、录音文件与播放声音 | 无需第二次操作，录音非静音，播放可闻；核对 16 kHz、两槽来源和麦克风/回采限幅；真实 ACK 是 CRLF/LF，若 CR 单独结束则记为接口缺口。 |
| 扫描、配对、连接 | 结构化 `bt.connected`、`bt.device`、`audio.route`、`bt.changed` | 扫描项无乱码重复，连接确认后为耳机路由且外放静音，配对记录仍在既有库中。另测芯片主动回连，记录是否可确认地址；未知地址不得假认已配对成员。 |
| 断开、忘记、耳机离线 | mode1 的真实应答、LOST、上述三个状态及配对库 | 收到 mode1 确认后才报 LOST 并回 speaker；忘记只删主机记录。持续丢 ACK 的上层假成功、物理离线报文均是本任务未闭合项，必须单独记录，不能据 HAL 测试判全链通过。 |
| 通话时 SCO | 对方和耳机双向可闻、现有 `BH1098: call audio change failed` 错误日志 | PP→200 ms→SCO 和反向顺序生效，开始/结束采集不抢走仍在通话的 SCO；无板载麦/耳机麦槽位混用。不在本任务发真实电话。 |
| MCU 重启、BH 不断电后首次放音 | 保持 P14 的受控重启、旧应答清理、首次录放与初始化尝试数 | 模式 2 留存后首次录放仍在模式 1 工作；临时丢首 ACK 可在 4 次内恢复；耗尽后不因轮询无界重试。P14 完整复位另随 port-board-2 联合核对。 |

## 里程碑

- 已导入原厂整文件原文、MIT 与 SHA-256，接好 C HAL 适配。
- S31 宿主机最终普通版 42/42 通过；新 9 个变异、已有音频 14 个/蓝牙 3 个反向控制均未放宽断言。手动把基线的三份产品 C 文件接到新测试，B02-write/ack/limit、B04、B05、C19 六项全以断言退出 -6；临时源码和二进制自动清理。主工程普通全量结束，内存检查、S31 构建进行中。

## 架构与 AI 原生审视

【实现】硬件命令留在驱动/HAL，不依赖页面或大模型；既有 `bt.scan/pair/connect/disconnect/forget`、`audio.record` 和媒体能力保持人/AI 共路，成员/锁屏和配对库存储准入仍由服务负责。低层 UART/GPIO 不另开放为绕过策略的能力。B04/B05 的服务状态缺口会同时影响 UI 和 AI，未声称完整覆盖通过。

## 验证命令与证据

所有构建/ctest 都加 `taskpolicy -c utility`，构建与测试均至多 `-j2`，没有同时跑两轮构建/全量测试。字体、Python、LVGL 配置沿用 `codex-rules.md` §5 的 `$INK_DEPS` 路径；只读依赖和设计目录均未改。

```sh
cmake -S ports/esp32s31/tests -B build-port-tests -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
cmake --build build-port-tests -j2
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-port-tests --output-on-failure -j2
"$INK_DEPS/build-deps/pyenv/bin/python" ports/esp32s31/tests/bh_vendor_test.py build-port-tests/test-s31-audio 1e9a16ee08931d0f86a17c2f852db51e1790c42d
cmake --build build -j2
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

基线模式仅手动运行；ctest 的新反向测试不依赖 Git 历史。普通全量输出：`100% tests passed out of 391`，耗时 630.55 s；未运行 186 hub-contract、187 hub-json-crosscheck、188 hub-simulator-e2e，原因分别为缺 pytest/异步/YAML 等依赖、缺生成消息、缺锁定 Python 环境。已有共享虚拟环境可用，补跑结果待填。

主工程编译没有本次产品代码警告；仍有 SDL2 的 macOS 弃用/扩展警告和重复静态库链接警告，未改第三方依赖或链接设置。
