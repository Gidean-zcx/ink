# 电量计与 RTC 原厂来源

来源：[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)，固定提交 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`（v2.0.77），查阅／拷贝日期 2026-11-11。许可证：[MIT](LICENSE)。只读核对 Git 对象，原厂克隆 HEAD 未移动。

## 完整原文副本

下列文件按同路径保存，修改行均为 **0**；`sources.json` 供宿主机校验。充电驱动和板级 RTC 的完整副本仅用于出处与后续接入，不冒充已经编入。

| 原厂路径（本目录下同路径） | SHA-256 |
| --- | --- |
| `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `main/boards/common/power/cx25601n.h` | `6b5f4a44caf0cbf5a5459d49ad99fe598aabb733ecd355c9037194a0113f3412` |
| `main/boards/common/power/cx25601n.cc` | `8af78da4b8e806c406b783a0185f85559f771d5c8196a9cf548a1b89474dee3b` |
| `main/boards/common/power/power_i2c.h` | `1712c125dc9c244848d3919d67bfe8980e103698d39ab8cb92eed8efcc8c5bc7` |
| `main/boards/common/power/power_i2c.c` | `f174f1d9c635ec7799ec9696004fb11e81f17df405c1b2f73e0ec224547d2a29` |
| `main/boards/common/power/bq27220_gauge.h` | `c2b3e5788e523855b3d7fe1237e06e08f2f952014a74a13ec5ad70de9aacaae2` |
| `main/boards/common/power/bq27220_gauge.cc` | `1c36f177d417a3028eeb16a3f501c6689062001b588a1e211d54cf0e1e2cce8a` |
| `main/boards/common/sensor/pcf8563.h` | `803b8b36f7a4e5dd2c35f9bec95a016c8eb5c22afae6f9dbec2ef2e087a9ebf6` |
| `main/boards/common/sensor/pcf8563.cc` | `90dc8b9c33a58de27ff258841e0a7d83da54908e3bf2744dd1f426102ff71e71` |
| `main/boards/metalio-e-ink4-plus/periph/metalio_rtc.h` | `37dd69271f01245ec213a402cd161f7df18e1cba30b14fa222295c252d609e05` |
| `main/boards/metalio-e-ink4-plus/periph/metalio_rtc.cc` | `0a0837c2fa934509fdf7aa097eabcfe2008d74c234db6607b992d430fdacc5b7` |

## 编入的整方法与必要适配

原工程只把这些驱动作为 C 编译，且任务禁止修改端口构建目标及 `s31_board.c`。沿用板级移植已有的 `.inc` 接入方式，不新增构建生成器或 C++ 编译入口。机械替换类作用域、引用、`static_cast` 和单例字段；算法内的曲线、阈值、顺序与时间常数保持原文。

| 编入文件 | 原厂整方法／范围 | 必要改动 |
| --- | --- | --- |
| `gauge.inc` | `common/power/bq27220_gauge.cc:11–303` 全部类方法；`.h:66–105` 状态和常量 | 类变为文件内单实例 C 状态和 `ink_bq_*` 函数，引用变指针，原子快照用编译器原子操作；时间读 OSAL 单调时间，500 ms 探测等待走 OSAL。原 `std::mutex` 由 OSAL mutex 覆盖完整采样和 ResetFilter；把同一次采样的电压／电流交还 HAL，避免重复 I²C；不新增算法、重试策略或线程。 |
| `rtc.inc` | `common/sensor/pcf8563.cc:8–137,190–197`：常量、Begin、寄存器读写、BCD、GetTime、SetTime、ClearIrqFlags | 类变为 C 单实例和 `ink_rtc_*` 函数；引用／强制转换机械替换，读写接 HAL。不编原厂 `mktime`、`settimeofday`、`localtime_r`，保留项目 UTC 日历换算及板级 UTC+8 策略。 |
| `binding.h` | 电源共用 I²C 的 HAL 胶水 | 一设备一 bus 描述；寄存器读写／probe 委托既有 HAL。HAL 下已编入 board_874538e 的完整原厂 `power_i2c.c/.h`，含同锁 repeated START→STOP 回退、100/200 ms 预算；不再复制一套总线重试。写入缓冲上限沿用 RTC 原厂 WriteRegs 的 16 字节。 |

`drivers/chips/bq27220/bq27220.c` 只在 S31 硬件路径把 `soc` 接到原厂曲线结果，原始命令接口保留。`pcf8563.c` 接原厂初始化／读写／清标志，复用 `ApplyRtcToSystem` 的粗校验（原文 149–154 行）；VL 或非法字段返回 HAL 失败，使既有时间服务进入未知时间、请求可靠时间源。可靠写回清 VL。原厂的产品时钟写入及 GPIO/TCA 中断管理留给已有系统／板级，不引入原厂 NVS。

模拟器路径保持原实现，未声称其电芯模型等于原厂曲线。充电 `cx25601n.c`、HAL、系统服务、能力表、事件与 UI 未改。C02 在板级的提前返回尚未消除，见任务报告。

## 所有新增／改变的适配行

以下列出相对完整原文的全部非空、非注释新增／改变行（忽略行首缩进）；整方法删除的外围依赖已在上一节列明。每次改动后以 `power_sources.py --baseline <基线>` 复核预算。

| 文件 | 新增／改变行 | SHA-256 |
| --- | --- | --- |
| [binding.h](binding.h) | 2, 3, 4, 5, 8, 9, 10, 11, 12, 14, 15, 16, 18, 19, 20, 21, 22, 23, 24 | `68aad983df3a811a9ad8681fc25ccbc6a3e6fd8ae851857981f300a99a4373ab` |
| [gauge.inc](gauge.inc) | 2, 4, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 45, 46, 47, 48, 49, 51, 56, 75, 77, 104, 106, 116, 120, 123, 126, 130, 134, 137, 141, 150, 175, 176, 177, 183, 186, 187, 193, 195, 199, 201, 204, 216, 221, 251, 252, 262, 263, 264, 265, 266, 267, 271, 272, 274, 275, 276, 280, 281, 282, 283, 284, 295, 298, 299, 320 | `ad5113cb0f308d8de945c0bddc122af551cd64a9154a03bdb2606e19b79fb4c0` |
| [rtc.inc](rtc.inc) | 2, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 20, 24, 25, 26, 28, 29, 32, 33, 36, 38, 56, 57, 62, 66, 68, 78, 79, 83, 87, 88, 92, 96, 97, 98, 108, 109, 110, 111, 112, 113, 115, 116, 117, 121, 126, 133, 134, 135, 136, 137, 138, 139, 141, 144, 146, 149, 150 | `f84b22a7e4874baae2da84cd50b37310aa764f763d1d232f3c2c08e603e650ae` |

## 证据边界

原厂放电曲线来自 424860-2000 电芯，约 1 A 放电到 3.3 V、以 1870.7 mAh 归一化；充电曲线为 3.3–4.3 V 线性估算。它不是本机标定。本次只在合成寄存器与虚拟时钟验证，未操作设备。

原厂 RTC 只做范围粗检，未增强为完整 BCD、闰日或月份天数校验；不能把本次通过解读为已验证所有损坏寄存器。原厂 STOP 清除失败后仍 ready 的行为保留。

充电驱动 C++→C 全量适配加其 HAL 胶水会使总预算超过 250 行，未纳入产品；没有改构建目标或用弱符号／宏覆盖板级 HAL 来绕过范围。C02 及完整 OTG 移植记为未完成，保留原厂完整副本供后续整块接入。
