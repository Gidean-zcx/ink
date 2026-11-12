# USB MSC 来源与适配

查阅日期：2026-11-12。没有接触设备。原厂行为以源码为证，本项目效果以宿主机测试和编译为证。

## 原厂

[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538e)，提交 `874538e`（v2.0.77），MIT，完整声明见 [LICENSE-Metalio](LICENSE-Metalio)。

| 文件 | SHA-256 | 采用的部分 |
| --- | --- | --- |
| `main/boards/common/storage/usb_virtual_disk.cc` | `689f76933038936404396e8ce155e87faf7f98672f9ff98a98a5a93bfeaaf755` | 426–483 行：SDMMC 卡交给 MSC，关闭自动挂载、禁止格式化；487–530 行：排空事件和延迟写入后停 USB 任务；570–638 行：删除 MSC 后恢复普通挂载、S31 HS UTMI、USB 任务固定 CPU0 |
| `main/boards/common/storage/SdCardManager.cc` | `a16b8384f7bc5e99c664f8c7051ca4c036aa68894d4c9716722966ab8d5a6559` | 179–245 行：普通挂载卸载后重新初始化 raw SDMMC 卡，保留原厂初始化、错误释放顺序 |
| `main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c` | `ca014b04cb9163bbe0c2fef0347a0484d51c51a04c78b0eb40b8232e405a7b22` | 321–329 行：`epd_board_metalio_eink4_plus_ana_sw_set` 整函数 9 行逐字拷入 HAL；函数 SHA-256 `88c66b1ebd75b56be6e63108d594ad346b873f8cc11dc7baf45104caf27fdb3d`；`ANA_SW_GPIO` 对应原厂 config.h 的 GPIO36 |

`hal_usb_s31.c` 是上述函数流程的 C HAL 适配，没有整文件搬入原厂的 C++ 类、UI、NVS、队列、工作任务、书库缓存和电源策略。逐项适配：

- `InitRawCardForMsc`：原厂 `sd_access::Transition` 换成现有存储锁和打开句柄计数；复用已挂载卡的 host 和内部 DMA 缓冲；原厂引脚保持与 SDMMC 初始化顺序不变。类字段换成本文件的卡句柄，移除产品日志；raw host 直接读写 SDMMC，避免被设备本地的“卡不在”保护挡住。
- `EnsureMscStorage`：系统服务已完成释放文件，HAL 再检查句柄，卸载普通 FAT，MSC 从 USB 归属开始，不再先挂 APP 再立即卸载；`auto_mount_off=1`，不格式化。只读构建拒绝借卡，避免绕过它的只读保护。
- `ParkUsbTask` / `QuiesceUsbTask`：保留断开、排空 TinyUSB 队列、挂起、卸载的顺序和 2 秒上限；调用方串行执行，不搬原厂请求状态机和工作队列。超时保留卡与 USB 资源，本地继续拒绝访问。
- S31 用 `TINYUSB_CONFIG_HIGH_SPEED`，内部 PHY 由 TinyUSB 管理；任务沿用原厂配置：4096 字节、优先级 5、CPU0。默认堆分配的栈在内部 RAM；没有新增产品工作线程。
- `UsbGadgetStart` 672–704 行和 teardown 542–551 行：保留原厂软断开 250 ms、GPIO36 置低、稳定 100 ms、软连接的顺序；停 USB 后等 150 ms 和 100 ms，再把 GPIO36 置高恢复 USJ。这里的等待直接来自原厂，不是为通过测试添加的延时。切换时的自身断连事件不触发系统归还；在 USB 借出阶段用 USB 事件判定电脑状态，避免已切离插口的 USJ 被误判为拔线。
- 电脑弹出改为置归还标记，交给已有 `file_service.c` 的 USB 回调路径处理；不在 TinyUSB 回调内重挂 FAT。USB 事件也只置标记，复用现有每秒一次的 SD 事件，在平台事件任务中通知服务。
- 在同一编译单元包含原有 `hal_storage_s31.c`，共享它的锁、句柄、卡状态；借出期间拦住它的轮询重挂载。没有修改原存储源文件或公开 HAL。
- `main/CMakeLists.txt` 移除单独编译的存储源，加入 USB 源；只在固件目标给旧 USB 占位函数改链接符号，宿主机原有占位测试不变。
- 第一次 S31 编译检出 TinyUSB 和项目的 9 个同名 OSAL 类型/函数冲突；只在包含 TinyUSB 头文件时给这些静态内联名字加前缀，随后立即恢复宏，项目 OSAL 和组件源码均不改。SDMMC 的 C 错误出口共用一次释放，调用顺序未改。

## ESP-IDF 示例和固定组件

本机 ESP-IDF v6.1：`examples/peripherals/usb/device/tusb_msc/main/tusb_msc_main.c` 与 `idf_component.yml`。示例通过组件管理器使用 `esp_tinyusb`，并非 SDK 内含 MSC 实现。选用原厂 `^2.0.0` 范围内、明确支持 S31 的 2.2.0；没有变更既有依赖版本。

| 组件 | 上游提交 | 归档 SHA-256 | 许可 |
| --- | --- | --- | --- |
| [esp_tinyusb 2.2.0](https://components.espressif.com/components/espressif/esp_tinyusb/versions/2.2.0/readme) | `c04934a51fb0f4db4b63b2500af793d1c0abf742` | `9a173414f7a032bb2bae004531232fca2a977886609df85eabe14f3ab10ef081` | [Apache-2.0](LICENSE-esp_tinyusb)，个别头文件自带 MIT |
| [tinyusb 0.19.0~3](https://components.espressif.com/components/espressif/tinyusb/versions/0.19.0~3/readme) | `ab4a1817907ed865b10a712b5e03e5e0a9902df5` | `e081d4176fd38bebfcf841d487b97476deebe73912936653e7fcc941af46f2cf` | [MIT](LICENSE-TinyUSB) |

构建时下载、核验归档并解压到构建目录；组件原文件和许可证保持原文，不提交归档或构建产物。编译的是设备栈、MSC、SDMMC 和原组件所需的支持源；其他 USB 类不开启。组件 Kconfig 作为 main 的 Kconfig 使用，避免在限定文件范围外新增组件清单。唯一符号适配是把 `tinyusb_msc.c` 自带的 `tud_msc_start_stop_cb` 重命名，让 HAL 的弹出回调进入既有系统归还流程；没有改组件源码。
