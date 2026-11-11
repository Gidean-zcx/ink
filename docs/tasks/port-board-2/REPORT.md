# port-board-2 执行报告

基线：`281dbe7130514a602c68e4c6de2b7e53c81a5db1`；分支：`claude/codex-port-board-2`。板级适配已实现；**Flash drain 未接存储入口，主工程有可用基线驱动复现的失败，不能认定整任务验收通过。** 不连接设备，不推送。

命令均加 `taskpolicy -c utility`，构建／ctest 最多 `-j2`；ctest 使用 `$INK_DEPS/build-deps/pyenv/bin/ctest`。完整命令和结果见 [EVIDENCE.md](EVIDENCE.md)，原厂提交、SHA-256、适配行见 [SOURCE.md](../../../ports/esp32s31/vendor/board_874538e/SOURCE.md)。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 I2C／B12 | 通过【实现／实测】 | 原厂整文件编入；100 kHz、10 ms stretch、启动恢复脉冲 | `ctest --test-dir build-s31-host -R 'vendor-i2c-(recovery|stretch)'` |
| 1.1 Flash 排空 | 没完成【实现】 | 原厂 drain 原语编入且单测通过，现有存储入口未接它 | `ctest --test-dir build-s31-host -R vendor-i2c-flash`；见“没做到的” |
| 1.1 TCA／B13 | 通过【实现／实测】 | 原厂同锁 STOP 读；power_i2c repeated START 失败后 STOP 回退 | `ctest --test-dir build-s31-host -R 'vendor-i2c-(stop|fallback)'` |
| 1.2 开机清理／C10 | 通过【实现／实测】 | GPIO IRQ／唤醒、UHCI 和 EPD／SD／I2C／前光 hold 清理；boot guard 最早 getter 只触发一次 | `ctest --test-dir build-s31-host -R vendor-board-boot`；入口顺序经代码核对 |
| 1.3 P14／C19 | 通过【实现／实测】 | 单次开机复位；P14 低 1 s，马达 500 ms，恢复后 80 ms；现有蜂窝／BH 通知各一次，epoch／上电时间更新 | `ctest --test-dir build-s31-host -R vendor-board-reset` |
| 1.4 前光／B10 | 通过【实现／实测】 | 原厂初始化与 10%／2×混色；0/50/100、5 个冷暖值核对；系统设置／渐变保留 | `ctest --test-dir build-s31-host -R vendor-board-light` |
| 1.5 触摸／B06、B07 | 通过【实现／实测】 | P12 低 10 ms／高 300 ms；5 次／200 ms ID 就绪读取；横屏轴、双点、空闲 20 ms 补采 | `ctest --test-dir build-s31-host -R 'vendor-board-(reset|touch|retry)'` |
| 1.6 按键 | 通过【实现／实测】 | 原厂逐键 30 ms、BOOT 双边沿、20 ms 恢复采样，读失败保持不可采样；系统 1.5 s 菜单／锁屏／音量代码未改 | `ctest --test-dir build-s31-host -R 'vendor-board-keys|vendor-irq'` |
| 1.6 加速度计／B11 | 通过【实现／实测】 | 原厂配置／mg 换算／静默；0x19→0x18，配置失败仍清 AOI1_SRC、关闭共享 INT | `ctest --test-dir build-s31-host -R 'vendor-board-accel-'` |
| 1.6 马达 | 通过【实现／实测】 | 原厂 20 ms PWM、14/18/20 ms 三档与脉冲定时器；放电脉冲先于普通引擎创建 | `ctest --test-dir build-s31-host -R 'vendor-board-(vibe|reset)'` |
| 1 来源与规模 | 通过【实测】 | 23 份原文 SHA-256 一致；胶水 342/350 行 | `python3 ports/esp32s31/tests/vendor_sources.py --baseline 281dbe71` |
| 2 宿主机／反向对照 | 通过【实测】 | 普通全量 47/47；18 次撤修均编译 0、断言退出 -6 | `ctest --test-dir build-s31-host --output-on-failure -j2`；`python3 ports/esp32s31/tests/vendor_negative.py --out build-s31-host/reverse` |
| 2 主工程普通全量 | 未通过【实测】 | 原话 `99% tests passed, 1 tests failed out of 391`；唯一失败 `integration-races`，基线驱动对照同样失败；3 项 Hub 补跑待记录 | `ctest --test-dir build --output-on-failure -j2`；[基线证据](EVIDENCE.md) |
| 2 S31 编译 | 首轮通过【实测】，最终增量待核 | 完整屏幕输出层编入；编译器 warning 0；最后按键恢复改动需增量核对 | `bash ports/esp32s31/build.sh` |
| 2 内存／GCC | 通过【实测】 | S31 宿主机 ASan＋UBSan 全量 47/47；gcccheck 两组 3＋4 翻译单元均无发现 | 见 [EVIDENCE.md](EVIDENCE.md) |
| 3 上机核对清单 | 已写，未上机【实现】 | 下表给出日志和通过条件 | 本次禁止操作设备 |

## 没做到的

- **未接存储 Flash drain。** 现有入口在范围外 `hal_storage_s31.c`／`hal_display_s31.c`／系统 display service。已编入并测试原厂原语，未另造 IDF hook、包装 API 或假装已集成。需合并主会话在现有 Flash guard 内接锁／解锁；这是验收缺项。
- 主工程全量存在既有驱动对照可复现的通知测试失败，未越界改通知服务或断言。Linux 整套测试未运行；本机 Apple clang、RISC-V GCC 编译检查和 S31 交叉编译不等于 Linux CI。
- 真机的拉伸、INT 实际电平、光效、触感和外围恢复未验证。主工程 ASan 按 `vendor-common.md` 第 7 条不运行，内存检查为 S31 宿主机全量。

## 和设计不一致的

【实现】按任务保留系统策略：不搬原厂前光 NVS／第二套渐变，不搬按键 3 s 自动关机；按键／加速度计任务接既有 input-hal，不另启动原厂任务。C++ 类作 C 单实例与回调绑定；原文和全部适配行已列 SOURCE.md。保留严格 ID／配置失败状态，未照搬加速度计 Configure 失败仍 ready；触摸只取应用就绪读取阶段，未读 ID2、未进入升级。P14 复位仅在消费者启动前调用，不新增运行时复位能力。以上是接口／策略适配，无本次实机偏差证据。

## 改过的已有测试

无。仅新增对应测试、IDF 桩、来源校验和手动反向对照；未放宽断言、预算、超时或跳过已有用例。

## 要用户定的

触摸固件升级仍待决定。本任务不写触摸固件。Flash 接入范围由合并主会话协调存储／显示任务，推荐接现有 guard，不新增机制。

## 顺手发现

【实测】`integration-races` 在本次和基线驱动对照中，均于 6.757 s 触发 `tests/integration/races.c:120`：`it is in the inbox of who it is for (0)`；两者使用全新输出目录，其余对象、脚本、资源、预算不变。未处理范围外问题。S31 配置有 5 条 SDK 内 Wi-Fi 私有包含目录提示，编译器无 warning；本次未改依赖图。普通宿主机构建有重复 `libink_port_sim.a` 链接提示。

## 要改的文档

合并主会话在共享硬件／端口记录补充原厂提交、C 适配与实机边界；在 backlog 标记 Flash guard 接线缺口和基线通知测试失败。本任务未改共享需求、想法、决策、审计、调研或生成的能力文档。

## 新增的机制

- 原厂马达 pulse／PWM 两个一次性 esp_timer：任务第 6 项明确要求，复用 SDK timer task。
- 无新增任务、队列、配置项或持久数据格式。既有 input-hal 仍为 4096 B、核 0。原厂代码原文 4537 行；编入的绑定／整函数副本共 690 行，其中新增适配非原文行 216；端口／芯片新增物理行 126，胶水合计 **342/350**。测试、文档和原文不计预算。

## 为什么不能更小

任务同时涉及六组板级路径；仅改前光比例或延长 I2C 超时无法覆盖开机残留、STOP 读、P14 放电和输入恢复。比较过直接编译整份 C++（需要改范围外 CMake）和逐模块重写，采用整文件／整函数副本加局部 C 绑定；保留现有 HAL 和执行任务。未另造恢复框架。

## 软总线与 AI 边界

【实现】本次仅替换现有 HAL 后端；服务、能力表、设置和 AI 页面快照接口不变。亮度、冷暖、按键语义与触感仍走现有系统入口，没有新增页面或绕过成员／确认规则。

## 上机核对清单

本次未执行，以下均为复核者在获授权真机上的通过条件；预期数值来自原厂代码，不是实机结果。

| 项目 | 观察与通过条件 |
| --- | --- |
| I2C 拉伸／恢复 | 日志 `MetalioI2C` 100 kHz；电量计超过 4 ms 的拉伸仍能完成，核对配置上限 10 ms；无连续 timeout，键／触摸可用。核对 TCA 指针写后 STOP 再读。Flash 并发验证须先补 guard 接线。 |
| 开机残留 | 冷启／MCU 软重启，先见 `boot: released EPD/SD/I2C/FL gpio holds`；有残留 IRQ 时先清理再注册新驱动，无早期 GPIO/UHCI 异常。 |
| 前光 0/50/100＋冷暖 | GPIO6/7 为 20 kHz；0 两路 0；50 时单路最大 51/1023，100 时最大 102/1023。暖度 0／50／100 分别冷路／两路／暖路；设置渐变正常。 |
| 触摸冷启／软重启 | `TP RST pulse` 后板级 `touch=1`；P12 低 10 ms、高后至少 300 ms。读 ID 短暂失败能重试就绪；没有执行固件升级。 |
| 双点／坐标 | 横竖轴布局均与屏幕一致；两个 ID 分别按下、移动、抬起，盖板键正常，空闲后第一点和短点不丢。 |
| 按键／长按 | BOOT、音量±、电源各一次按下／松开；1.5 s 电源菜单、锁屏、音量正确；恢复后无假沿，恢复读失败保持不可采样。 |
| 加速度计 INT | 核对识别地址 0x19 或 0x18、三轴 mg；活动后 AOI1_SRC 读清，共享 GPIO2 在其它源空闲时回到空闲。初始化失败时仍核对共享 INT 释放、侧键能用。 |
| 马达三档 | 弱／中／强脉宽 35/50/70 ms；弱／中 PWM 导通 14/18 ms、周期 20 ms，强档连续；结束后 P03 低。关闭触感后，开机放电仍连续高 500 ms。 |
| P14／4G／蓝牙恢复 | `BT reset` 低→高仅一次；低至少 1 s、放电 500 ms、恢复后 80 ms，马达／功放先关闭；冷启和 MCU 软重启后 4G 恢复注册，BH 首次播放／录音模式 ACK 正常。保留既有 4G 数据恢复与 BH 通知接口。 |

## 最终检查

最终增量编译、Hub 补跑和最终隐私／范围检查结果待补；前一里程碑的隐私扫描匹配 0 行，原厂克隆 HEAD 保持 `690a023c66862af103a3187464373ee7c5efcd14`。
