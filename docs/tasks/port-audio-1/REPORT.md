# port-audio-1 执行报告

基线：`1450591caecfffdff023b19dc057fad5174a050f`。仅编译与宿主机测试，未连接设备。第二里程碑：实现及端口正反测试完成，全量验证进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 已验证，末次增补待复跑 | 【实测】端口普通版与 ASan+UBSan 均为 `100% tests passed out of 10`；原实现及 9 个音频反向修改全部编过，原实现退出 0，反向修改退出 -6。S31 按原厂分区完整构建成功，0 编译警告，镜像 `0x1b6440`（1,795,136 B），5 MiB 应用分区剩余 66%；默认骨架分区的失败见下文 | `ctest --test-dir build-s31-host --output-on-failure -j2`；目录换 `build-s31-host-asan`；`python3 ports/esp32s31/tests/audio_negative.py build-s31-host`；`bash docs/tasks/port-audio-1/build_factory.sh` |
| 2 | 进行中 | 【实测】`gcccheck: 4 compiled, 0 with findings`；模拟器普通版原话 `100% tests passed out of 337`，耗时 628.59 s，但其中 3 项中枢测试自动跳过（缺测试 Python 依赖），不能算作全过。已找到依赖目录的锁定环境，补跑及 ASan 全量进行中；隐私扫描待跑 | `python3 ../../tools/gcccheck.py build-s31-host ports/esp32s31/hal_audio_s31.c ports/esp32s31/hal_opus_s31.c ports/esp32s31/tests/audio.c ports/esp32s31/tests/opus.c` |
| 3 | 完成代码对照，上机待复核 | 【实现】见下表；原厂代码不等于本机实测 | 下列文件行号 |

## 原厂一步 → 我们一步

原厂来源均为 CloudZao/Metalio-E-INK4-Plus `690a023c66862af103a3187464373ee7c5efcd14`，2026-11-10 查阅；路径相对原厂仓库。

| 原厂一步（文件和行号） | 我们一步 |
| --- | --- |
| `main/boards/metalio-e-ink4-plus/config.h:142–154`：BH1098G，16 kHz，BCLK42/WS43/DOUT44/DIN45，UART1 TX39/RX38 | `hal_audio_s31.c` 配同样 I2S 引脚，复用现有 BH1098G UART 驱动 |
| `config.h:71,100`：P04 控 NS4150，P14 与 4G 共享供电 | 只经 `metalio_board_set` 改 P04；只读 P14 和其供电周期，不复位或切断共享电源 |
| `periph/metalio_audio.cc:26–27,94–125,202–206`：700 ms 上电稳定、RX=2、间隔 700 ms、MODE=1、等 SET MODE 1 应答最多 1 s | 在首次音频请求和 P14 恢复后的首次请求里同步执行；失败返回 -1，不声称设备已就绪 |
| `main/boards/common/audio/bt_audio_codec.cc:66–125`：I2S0 从机、4 DMA 描述符、32 位立体声、Philips、自动清零；`main/audio/audio_codec.h:15`：240 帧 | 使用相同模式；每块 240 帧、总 60 ms；启播前清空全部 TX DMA，避免录音期间重启播放残留声音 |
| `bt_audio_codec.cc:138–163`：16 位单声道扩到左右两路，音量增益为 `(v/100)^2`，先开 PA 再写 | 音量 0～100 和儿童上限继续由已有音频服务统一应用；HAL 以满幅乘 65536 打包，不重复施加音量 |
| `bt_audio_codec.cc:166–185`：双声道输入各右移 12 位，钳位到 ±32767；`main/audio/audio_service.cc:306–311`：先麦克风、后回采 | 按 240 帧拆成麦克风与回采；RX 溢出回调累计丢样；停止等待采集工作结束后才返回 |
| `bt_audio_codec.cc:16–38`：停用、释放通道、关 PA；`periph/metalio_audio.cc:204,308`：初始 PA 关、写声音时开 | 播放停止先选定排空/丢弃，再停 TX、关 PA；录音可继续。最后一方退出释放双向 DMA 与 PSRAM 缓冲 |
| `main/idf_component.yml:8`：`78/esp-opus-encoder ~2.4.1`；包装 2.4.1 依赖 `78/esp-opus ^1.0.5`；`main/audio/audio_service.h:34`：60 ms | 钉死后者 1.0.5 与压缩包 SHA256，直接调同一固定点 C 实现；VOIP、DTX、16 kHz 单声道 60 ms；码率/复杂度沿现有 HAL 参数 |

## 上机先核对清单

- 正常开机与待机唤醒后分别播放提示音、闹钟、来电铃和朗读；确认 UART 模式应答和 16 kHz 外部 I2S 时钟，听首字是否完整。
- 录一段并播放，确认麦克风声道、回采声道、左右槽顺序、音量和削波；边录边放后分别停一边，另一边应继续。
- 连续播放/暂停、立刻停止、自然播完、录音中重开播放：听是否爆音或重播旧数据；观察 P04 空闲/纯录音为低，P14 不被音频切换。
- 缺少 BH 应答或无外部时钟时确认失败/可停止，查看丢样、堆余量和采集任务栈高水位。声音品质与省电电流本次无实测。

## 没做到的

- 未上机；声音、录音和爆音只能列入上机清单。AFE 依任务保持不可用；蓝牙路由返回 -1。
- `hal_audio_play_delay_us` 为最后一次写入后的 60 ms 保守 DMA 上界，非声学测量；回调时间戳为读取时刻减本块长度，未校正队列滞留时间。
- 未在 Linux 运行；gcccheck 是给定 RISC-V GCC 的编译告警检查，不能代替 Linux CI。
- 本机 ASan 运行时不支持 LeakSanitizer。首轮误设 `ASAN_OPTIONS=detect_leaks=1`，程序初始化即退出；移除该选项后保留 `-fsanitize=address,undefined` 与 `UBSAN_OPTIONS=halt_on_error=1`，正在重跑全量。不能据此声称做过泄漏检查。

## 和设计不一致的

- 原厂启动时复位 P14 并有后台模式重试任务；本端口不复位与 4G 共用的电源，在实际音频请求中确认模式，没有新增后台重试。
- 原厂释放整个 codec 时才关 PA；本端口停止播放即关 PA，录音继续。为防止录音持续时重播旧 DMA，TX 重启先预装静音。
- 本机只读副本缺少 `main/audio/`；从同一固定提交的公开原厂源码补查了 [`audio_codec.h:15`](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/audio/audio_codec.h#L15) 的 240 帧定义及 [`audio_service.cc:47–49`](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/690a023c66862af103a3187464373ee7c5efcd14/main/audio/audio_service.cc#L47) 的 16 kHz 单声道与复杂度 0。
- `hal_codec.h` 注释预想的是 esp_audio_codec / RISC-V 汇编；原厂实际依赖 78/esp-opus，本任务按任务书采用其固定点 C 实现，未加 RISC-V 汇编优化，编码 CPU 成本待上机量。

## 改过的已有测试

- 不修改已有测试断言；只在端口测试工程增加音频、Opus 目标。第三方 Opus 的宿主机构建沿原组件自己的编译参数，产品代码与测试保持 `-Wall -Wextra -Werror`。

## 要用户定的

- 暂无产品决策。

## 顺手发现

- 默认 S31 骨架分区容量 `0x177000`，首次镜像为 `0x1b6440`，溢出 `0x3f440`。任务要求的原厂 OTA 分区为 5 MiB。当前只在构建目录用原厂 CSV 核验，正式默认配置由 port-boot-1 收口。
- 普通模拟器构建有未改目标的 `ld: warning: ignoring duplicate libraries: 'libink_port_sim.a'`；S31 编译没有此警告。本任务未调整根构建链接关系。

## 新增的机制

- 按需采集任务 `audio-capture`（4096 字节 PSRAM 栈）：任务书要求的平台音频回调；停止后退出，不常驻。
- 固定版本 Opus 压缩包下载与哈希检查：任务书第 2 条明确要求。无新配置项、队列、缓存、持久格式或后台重试。
- 播放/采集生命周期及停止同步：任务书第 1、3 条和现有 HAL“停止后不再回调”契约要求；复用既有板级电源周期与资源引用。

## 规模和为什么不能更小

- 产品代码净增 258 行（新增 275、删除 17，含 CMake；不含测试、报告和许可证），低于 300 行预算。
- 必需覆盖模式应答、I2S 双工、PCM 槽转换、采集任务停止同步、功放与缓冲释放，再加 Opus C API。已考虑直接引入原厂 C++ codec，但它依赖 Board、Settings、PowerPolicy 和向量缓冲，改动范围更大；本实现只复用既有 BH1098G、UART、板级电源驱动。

## 要改的文档

- 合并时更新 `ports/esp32s31/README.md` 的音频、Opus 状态及依赖预置方法；默认分区配置与 port-boot-1 一并核对。
- 合并时将 Opus 的 BSD 许可证加入产品许可证清单；完整文本已随端口提交。
- R-061：改动只在硬件 HAL；现有音频服务/软总线能力、设置、成员隔离和 AI 调用入口继续共用，无新增绕过服务的界面或 AI 接口。
