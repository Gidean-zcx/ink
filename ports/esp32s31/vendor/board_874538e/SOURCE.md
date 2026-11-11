# 板级原厂代码来源

来源：[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538e)，固定提交 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`（v2.0.77），查阅／拷贝日期 2026-11-11。许可证：[MIT](LICENSE)。没有改动原厂克隆的 HEAD。

## 原文副本

下列文件逐字节拷贝，改动行均为 **0**。只把所需整函数或类方法接到现有 HAL；没有把整份原厂产品程序编入固件。

| 原厂路径（本目录下同路径） | SHA-256 |
| --- | --- |
| `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `main/boards/metalio-e-ink4-plus/bus/metalio_i2c1.c` | `ad1259ef4b810b117b0f8bc5225990a95509c558eb5220df9a57f3a2b8cfaa44` |
| `main/boards/metalio-e-ink4-plus/bus/metalio_i2c1.h` | `de6ef29b3e946004e104f150933d7f295bf2f1499ed4b0c17c9d775a6491bb20` |
| `main/boards/metalio-e-ink4-plus/bus/pca9555.c` | `84c7d6dd5197f00e0fd3a064fadba6690160e55dafd7cac4c9ca9a70df65c159` |
| `main/boards/metalio-e-ink4-plus/bus/pca9555.h` | `a952ad0b4fca46a4a08a6582b78cde7d45f86ad832e07587c1c8716e293632fb` |
| `main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c` | `ca014b04cb9163bbe0c2fef0347a0484d51c51a04c78b0eb40b8232e405a7b22` |
| `main/boards/metalio-e-ink4-plus/display/metalio_touch.c` | `35b6255e20492c342196a0cd08f387296689f36bc7edf2b3a032dedeed8442ce` |
| `main/boards/metalio-e-ink4-plus/display/metalio_touch.h` | `f7a5bfbd7ddcfd8af4fedd2c858650eb7d3faf8660dd52a3f697e52aa98941bd` |
| `main/boards/metalio-e-ink4-plus/input/metalio_keys.cc` | `7286f213284d12474249eac056950b331ffba56b52b1a7992c54183599bedd8d` |
| `main/boards/metalio-e-ink4-plus/input/metalio_keys.h` | `8b7e93ec81e3c8e276a25b8fd3b5b95d4e6f7e4d5196a81ded6a72525a146d68` |
| `main/boards/metalio-e-ink4-plus/input/metalio_vibe.cc` | `704fe17db25302b3cdf83bc0c78bddb7370eced7c7abb33065c455988c2bef0f` |
| `main/boards/metalio-e-ink4-plus/input/metalio_vibe.h` | `f1ca5f6106c392f364d7ad3a68a767eb6053f71b1dbe14c19313ee65c3188de3` |
| `main/boards/metalio-e-ink4-plus/periph/frontlight.cc` | `9ecc59ea946efaf53e59566bda58e0c672d42636f66eedd540597be714f450c2` |
| `main/boards/metalio-e-ink4-plus/periph/frontlight.h` | `2b541bb20aec932f7563b8a148f240f8186f4cc79b746b645b2fd16995ef075c` |
| `main/boards/metalio-e-ink4-plus/periph/metalio_accel.cc` | `66cb7ce24ca8b4329d50379ab582d90561b12be0fee412e77f33bad5ba46d0aa` |
| `main/boards/metalio-e-ink4-plus/config.h` | `b2d39fb67a618488338102d2567fd2d1e3c05e5c18d79872d9b4a092f2a9c15c` |
| `main/boards/common/power/power_i2c.c` | `f174f1d9c635ec7799ec9696004fb11e81f17df405c1b2f73e0ec224547d2a29` |
| `main/boards/common/power/power_i2c.h` | `1712c125dc9c244848d3919d67bfe8980e103698d39ab8cb92eed8efcc8c5bc7` |
| `main/boards/common/sensor/sc7a20h.cc` | `4ff40773dbae5beacbb10264dd26fc1c9db7619829576b4d8c20b38092ad6e9d` |
| `main/boards/common/sensor/sc7a20h.h` | `d399cc0479abb91ed06e530ed3a76f001deae2fbf5894811509991db093653c8` |
| `main/boards/common/power_policy/power_hw.cc` | `16824b1c9f735aa332036d716092a84502b728dd864a4c8a684f8f5c2d2cee0f` |
| `main/boards/metalio-e-ink4-plus/display/ft6336u_upgrade_protocol.h` | `8887c567da42ea0ba0ab85dd7906e56b1db1bf5f167a0307b2e302f2413ae363` |

## 编译入口与适配行

端口的 CMake 不在任务允许修改的范围。`hal_i2c_s31.c` 包含 `bus.inc`，`hal_device_s31.c` 包含 `boot_binding.inc`，`s31_board.c` 包含 `light.inc`／`reset_binding.inc`，`hal_input_s31.c` 包含 `input_binding.inc`。芯片驱动包含 `accel.inc`／`touch_ready.inc`。没有新增构建生成器。

C++ 类改为 C 的单实例私有状态、普通函数及回调；方法内硬件值、执行顺序与定时值保留。下表按去掉行首缩进后与原文逐行核对，列出所有新增／改变的非空、非注释行；完整原文在上表，可直接比对。删除的原厂产品策略另列在下表之后。

| 编入的文件 | 新增／改变行 | SHA-256 |
| --- | --- | --- |
| [accel.inc](accel.inc) | 2, 28, 29, 30, 59, 60, 61, 62, 63, 64, 65, 66, 69, 71, 74, 75, 76, 83, 84, 86 | `631f490eea44d71d9a6a129f4f42ba6b1eda291d946e9005d205c539821b19e1` |
| [boot.inc](boot.inc) | 无（仅整函数拷贝） | `9a43211ee47b9074795bedf2d23cf820de7a95ff16510ce2710d936f4eed57b4` |
| [boot_binding.inc](boot_binding.inc) | 2, 3, 4, 5, 6, 12, 13, 15, 16, 17, 19, 20 | `3dd16b8d747fae5f431d7f05805940d3192ef890d38116620ef249aa06c5abab` |
| [bus.inc](bus.inc) | 2, 3 | `bf9f23c356f9ee5c763e08b185667a7e71a74feddc1eaf58da10a456b231f670` |
| [input_binding.inc](input_binding.inc) | 2, 3, 4, 5, 6, 7, 9, 10, 12, 14, 15, 18, 19, 20, 21, 22, 23 | `828c6dc673784b3ba193fb16a6beb8a09ccfd8a7d2a1be45cd2dedbf2b0fe42f` |
| [keys.inc](keys.inc) | 3, 4, 5, 12, 14, 21, 22, 25, 28, 29, 32, 33, 36, 43, 44, 47, 50, 51, 53, 57, 60, 61, 66, 70, 71, 72, 73, 74, 77, 78, 79, 80, 81, 82, 83, 85 | `483bad3e116042a0c4529a96d104069562f847042b72e1f94241200222c60a7f` |
| [light.inc](light.inc) | 2, 3, 5, 14, 15, 20, 29, 43, 52, 62, 63, 67, 68, 69, 71, 73, 74, 83, 84, 87 | `2c35e83eb4427a604b4d113ab3c8b5a988dc6e6413342713347602945d4590ac` |
| [reset.inc](reset.inc) | 2, 60 | `02700754f9670a2869fffc4dd6a70760551e9292b9f45912c64271f82525b585` |
| [reset_binding.inc](reset_binding.inc) | 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 26, 34, 35, 36, 37, 40, 43, 44, 45, 46, 48, 49, 50, 51, 52, 53, 54, 57, 58, 59, 61, 63, 64, 65, 66, 67 | `b08d907b572bd7ad566fe32cf2fa308f906270280ac304902b0aca53b7b00183` |
| [touch_map.inc](touch_map.inc) | 无（仅整函数拷贝） | `039e5f37730fc10addf3dad369356b8709f8e1539180750e2c0219ef3b62b562` |
| [touch_ready.inc](touch_ready.inc) | 2, 3, 4, 7, 8, 9, 13, 15, 16 | `075428a5a45003dee0c58686dfbde875ad4cd9b55ba83b4f215798aa1e1a8245` |
| [vibe.inc](vibe.inc) | 2, 3, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 26, 27, 31, 32, 36, 54, 63, 65, 70, 72, 76, 79, 82, 87, 91, 97, 100, 105, 106, 115, 117, 121, 130, 136, 139, 143, 148, 154, 156, 162 | `b33b2a89ecc68d7408debdbe8000284af94cc3ac629931e2cc13e5f08c836235` |

| 适配文件 | 原文函数／段 | 必要改动及边界 |
| --- | --- | --- |
| `bus.inc` | `bus/metalio_i2c1.c/.h`、`common/power/power_i2c.c/.h` 整文件 | C 源通过 include 编入；原文没有任何改行。HAL 0 映射 I2C1；TCA 使用原厂共同持锁 STOP 读；电源／传感器寄存器读接原厂 repeated START→STOP 回退。 |
| `boot.inc`／`boot_binding.inc` | `epd_board_metalio_eink4_plus.c:483–520` | 整函数拷贝；去掉显示层私有的 `s_lcd_bus_held=false`，显示层自己初始化。SD hold 用同一原厂引脚表在 C 中逐项释放；GPIO/UHCI 清理原文保留。最早现有调用点是 boot guard 调 `hal_device_reset_reason`，只执行一次。 |
| `reset.inc`／`reset_binding.inc` | 板级 `.c:263–318,615–632`；`power_hw.cc:242–265` | 整复位函数、整触摸复位函数拷贝；复用现有 board mutex、TCA 状态和 GPIO 定义。P14 恢复写经现有 `metalio_board_set` 更新 epoch／时间戳；短暂释放再取得同一 board mutex，仅用于消费者启动前的开机复位。蜂窝和 BH1098 掉电／恢复通知调用现有接口。失败保留错误，启动停止。 |
| `light.inc` | `frontlight.cc:17–23`、整 `SetupHardware`／`ApplyOutput` | 类签名改为 C 函数；CCT 为现有 HAL 双通道模式，选 PWM；`std::min` 用局部宏。HAL 冷暖千分数还原总亮度／暖度后调用原厂 10% 与 2×混色公式。没有 NVS、原厂前光渐变定时器。 |
| `touch_map.inc` | `metalio_touch.c:85–106` 整 `map_to_logical` | 函数体不改；映射每个触点，盖板键先保留原始坐标，最终走现有旋转与 HAL 双点事件。空闲 20 ms／按住 10 ms 补采接现有 input task。 |
| `touch_ready.inc` | `ft6336u_upgrade_protocol.h:44–64` 的 `Attempt` 应用就绪阶段 | 保留 5 次／200 ms 读 ID、版本等待；改为 HAL 读与 OSAL 延时；不引入 ID2（现有芯片合同没有它），不执行校验固件、bootloader、擦除或写固件的其余步骤。初始化仍保留本项目严格芯片 ID 检查。 |
| `accel.inc` | `common/sensor/sc7a20h.cc:8–35,53–81,93–100,170–180` | 配置／静默／读数整方法拷贝；`constexpr` 仅在本 include 内映射为 `static const`，C 指针／强制转换替换 C++ 引用／cast，读写接现有 HAL。静默原文忽略的布尔结果显式转 void，避免 GCC 的 unused-value 警告，执行顺序不变。保留本项目未知 ID／配置错误不 ready；板级先 0x19 后 0x18，失败时 probe 并调用原厂静默序列，含 AOI1_SRC 读清。 |
| `keys.inc` | `metalio_keys.cc:24,37–51,62–90,100–135,187–200,226–245` | KeySlot 与采样／消抖方法转 C 指针，保持每键 30 ms。删除原厂 3 s 关机、短按策略、队列和任务；稳定状态汇入原有 HAL 事件，保留系统 1.5 s 菜单、锁屏、音量。恢复沿用 20 ms 等待与重新采样，BOOT 使用上电空闲极性。 |
| `vibe.inc` | `metalio_vibe.cc` 整 VibeEngine（除单例访问器、对外 C 包装） | 字段转本文件静态单实例，方法转 C 函数，两个 lambda 转普通回调。原厂 20 ms PWM、14/18/20 ms 三档与 pulse timer 保留；HAL 35/50/70 ms 映射三档。没有新任务，复用 SDK esp_timer task；开机 P14 放电发生在普通马达引擎创建之前，直接置马达 500 ms。 |

## 未接入的范围

- `metalio_i2c1_lock_for_flash` 已原文编入并测试，但**没有接到现有存储入口**。入口位于任务禁止改动的 `hal_storage_s31.c`／`hal_display_s31.c`／系统 display service，不能以一个未调用的 helper 冒充完成。合并主会话需在现有 Flash guard 的范围持有原厂 drain 锁；不要增加新的 Flash hook 或包装 IDF API。
- P14 复位只从开机装配调用一次。没有增加运行时复位能力，也没有移植原厂 UI／NVS／自动重启策略。现有浅睡流程属于其它任务，未替换。
- 完整原厂按键／加速度计任务均不启动；由既有 input-hal（4096 B、核 0）执行，未新增任务或栈。马达的两个定时器来自任务明确要求的原厂引擎。

## 复核

`python3 ports/esp32s31/tests/vendor_sources.py` 校验 22 个原文文件的 SHA-256；可选 `--baseline <提交>` 手动统计适配新增行，普通 ctest 不依赖 Git 历史。测试、反向对照与上机核对边界见 [任务报告](../../../../docs/tasks/port-board-2/REPORT.md)。
