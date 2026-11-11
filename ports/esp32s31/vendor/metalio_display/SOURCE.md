# 屏幕输出和电源原厂来源

来源：[CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)。提交 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`（v2.0.77），查阅 2026-11-11。原厂根目录 MIT，原文许可证见 [LICENSE](LICENSE)。本目录无 GPL 代码。

## 原文件 SHA-256

| 原厂相对路径 | SHA-256 |
| --- | --- |
| `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `sdkconfig` | `11bc85fed2a847e8ff46393ebe9266ab34cac34770681dd8d8348768badffcaa` |
| `main/boards/metalio-e-ink4-plus/bus/tps65185.c` | `3f59831d117878f699d6ef99647a2524fc42aa12184615fa3390032b6515b5b3` |
| `main/boards/metalio-e-ink4-plus/bus/tps65185.h` | `a42f21fb1aaeff5df91a8803f0db7cf4687ff8624d3acf4b96433b9aa9842880` |
| `main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c` | `ca014b04cb9163bbe0c2fef0347a0484d51c51a04c78b0eb40b8232e405a7b22` |
| `main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.h` | `8a40433fdfc504470fbdf5d8b3004d52d1580af8946b3d03dd72125fb3ad5caf` |
| `main/boards/metalio-e-ink4-plus/display/output_lcd/lcd_driver.c` | `acb1a2633be4fb110e934f1dbe9d5bcb94cf744480c5a0ffc8da5ac72c38ce51` |
| `main/boards/metalio-e-ink4-plus/display/output_lcd/lcd_driver.h` | `00a034e86612acec5c80aba11124dd71f1af32f42ed2b2ac47a1236f01ffb2e0` |
| `main/boards/metalio-e-ink4-plus/display/output_common/line_queue.c` | `f282e808d441212b66ad7d710e988d9287e962f27d19ffbdf6f489756c3ea881` |
| `main/boards/metalio-e-ink4-plus/display/output_common/line_queue.h` | `70cd54837772807c88ed5c4e83c788adde5b6bd73dc1a0f3b568427603b581a5` |

## 拷贝与最小适配

- 输出层及传递头：`display_upstream.cmake` 只在构建目录创建独立克隆并检出固定提交。`INK_UPSTREAM` 仅是只读克隆来源，不改变它的 HEAD。编译该独立克隆内的 `lcd_driver.c`、`line_queue.c`，使用同提交全部头。版本缺失立即构建失败，避免静默退回桩。
- [iram.patch](iram.patch) 只有两处产品行：原 `lcd_driver.c:340` 为 `start_ckv_cycles` 加 `IRAM_ATTR`；原 `line_queue.c:26` 指针表 `calloc` 换为 `heap_caps_calloc(..., MALLOC_CAP_INTERNAL)`，供 ISR 安全读取。不改扫描时序、DMA/IPC 合同或 PQ-BN。补丁只施加在独立克隆，重配时先验证已应用或可应用。
- 原 `lcd_driver.c:468` 的 `gpio_reset_pin` 属于 `deinit_ckv_rmt`，只由任务上下文的 `epd_lcd_deinit:1049` 调用。`lcd_isr:471–510` 没有该调用，故不把整个 GPIO 驱动强行放入 IRAM。GDMA 两个开关取原厂 `sdkconfig:2170,2172`，LCD_RGB_ISR_IRAM_SAFE 保持原厂未启用状态。
- [tps65185.c](tps65185.c) 逐个完整拷贝原 `bus/tps65185.c:15–75` 的寄存器访问、probe、VCOM 函数；[tps65185.h](tps65185.h) 原文保留作接口来源，产品仍用已有 ink 头。只机械替换 `esp_err_t`、`i2c_port_t`、错误常量和错误名函数为 HAL 对应项；不拷贝不使用的 thermistor/kickback/诊断入口。
- [epd_power.inc](epd_power.inc) 完整拷贝原 `display/epd_board_metalio_eink4_plus.c:815–979,986–1001` 的 `epd_board_poweron_once/poweron/poweroff`。全局 TPS 成功标志和端口改接栈上控制状态与已有 `epd_power_t`，错误类型机械替换；VCOM 从已有字段取，REVID/attempts/retries 写回已有字段。移除原厂 LCD 拆装恢复调用（本端口没有拆 LCD，初始化只做一次）；最终自动重启改为向 HAL 返回失败，遵守公共规矩第 2 条。其余寄存器、轮询、延时、GPIO 和 mask 顺序保持原文。
- [power.c](power.c) 是现有接口的胶水：原 `set_ctrl_with_tps:785–803` 的 GPIO/mask 接到原 TCA 单实例和锁；原 `tps_enter_sleep:386–408` 的已确认判断与 50/120 ms 顺序接到同一实例。TCA `out/stale` 继续作为唯一确认来源，不另设全局 desired/confirmed。共享供电 P05 的 300 ms 恢复等待及供电保留检测沿用原 ink 安全检查。GPIO PG 读取失败按低处理。关电失败保留 `stale`，HAL 维持 powered/armed；后续显示访问门不得放行。
- TPS STOP 分隔：原 `bus/metalio_i2c1.c:257–262` 的先 transmit、后 receive 顺序接到两个 HAL 调用，整对由已有 board 锁覆盖；失败不覆盖调用者的输出字节。现有 HAL 无事务超时/带超时锁接口，仍使用原 HAL 超时，不能宣称与原厂 1000/100 ms 等价，见任务报告。TCA 的读事务属于 port-board-2。

## 本目录文件 SHA-256

以下不包含本清单自身；原文范围和适配说明如上。

| 文件 | SHA-256 |
| --- | --- |
| `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `epd_power.inc` | `5a58148ca1a6dcf573806f2dec7e7b837f38f27762ad5821c253a7392a6f154e` |
| `iram.patch` | `21ebc5a3365d0ece95dbe7adfcf9b0fe51dd99c2c271b93c145185e63e5e87ed` |
| `power.c` | `f208ffd804b1f2ac11b5ade44ffa5573d884852cc7c1c8d693a5588e4f81066f` |
| `power.h` | `71efb5a537cc1d1ae21937c7493d49efd330ec1ef6b0e760421f9610641898ba` |
| `tps65185.c` | `13686b63924014699f60dda78cff4ae19a601d189fb4cc21fefa279166a48b8e` |
| `tps65185.h` | `a42f21fb1aaeff5df91a8803f0db7cf4687ff8624d3acf4b96433b9aa9842880` |
