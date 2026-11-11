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
| `main/boards/metalio-e-ink4-plus/display/epd_board.h` | `7c0aa562d0990052fb6af8afc8cfd09b8fc169e4cc71a3d57449ce57d9a34a3a` |
| `main/boards/metalio-e-ink4-plus/display/epd_board_specific.h` | `d13263a116330ee1362184f25fd0dec6acf4270410a541712d55420363429dd0` |
| `main/boards/metalio-e-ink4-plus/display/epd_display.h` | `98147673e6796fe0a02de3b4ded302d8ec8131bddf8ae9698ddbe9b3cacc5f6a` |
| `main/boards/metalio-e-ink4-plus/display/epd_highlevel.h` | `1e70c7d7f7119454a3f8d6ab004ae33a4f37c079805473216ede4d92fb6d2567` |
| `main/boards/metalio-e-ink4-plus/display/epd_internals.h` | `6cd965fe5e6a79d3afce8d98993adb6baff584b447e01f1c811ef3209550242f` |
| `main/boards/metalio-e-ink4-plus/display/epdiy.h` | `195fa20dab233503664e252d2d60cb59a8d1727c8ec72c39ba95b59ec4ad0492` |
| `main/boards/metalio-e-ink4-plus/display/output_common/render_method.h` | `195aee17880b3d9c0f35ada6e2ce7bae37ebd9ffe1d7f037a6ce6dfa275163f8` |
| `main/boards/metalio-e-ink4-plus/display/output_common/white_border.h` | `ad99e229dfc09fe0fe783b2e9f9c9e5c689942b424fe0db761265fa274cb3f1b` |
| `main/boards/metalio-e-ink4-plus/display/output_lcd/epdiy_idf6_compat.h` | `604452703c0f5c6e937e327081f60322521bc555e8452376517fefcd52e7f947` |
| `main/boards/metalio-e-ink4-plus/config.h` | `b2d39fb67a618488338102d2567fd2d1e3c05e5c18d79872d9b4a092f2a9c15c` |
| `main/boards/metalio-e-ink4-plus/bus/metalio_i2c1.c` | `ad1259ef4b810b117b0f8bc5225990a95509c558eb5220df9a57f3a2b8cfaa44` |
| `main/boards/metalio-e-ink4-plus/bus/metalio_i2c1.h` | `de6ef29b3e946004e104f150933d7f295bf2f1499ed4b0c17c9d775a6491bb20` |

## 拷贝与最小适配

- 输出层及传递头：`display_upstream.cmake` 只在构建目录创建独立克隆并检出固定提交。`INK_UPSTREAM` 仅是只读克隆来源，不改变它的 HEAD。编译该独立克隆内的 `lcd_driver.c`、`line_queue.c`，使用同提交全部头。版本缺失立即构建失败，避免静默退回桩。
- [iram.patch](iram.patch) 只有两处产品行：原 `lcd_driver.c:340` 为 `start_ckv_cycles` 加 `IRAM_ATTR`；原 `line_queue.c:26` 指针表 `calloc` 换为 `heap_caps_calloc(..., MALLOC_CAP_INTERNAL)`，供 ISR 安全读取。不改扫描时序、DMA/IPC 合同或 PQ-BN。补丁只施加在独立克隆，重配时先验证已应用或可应用。
- 原 `lcd_driver.c:468` 的 `gpio_reset_pin` 属于 `deinit_ckv_rmt`，只由任务上下文的 `epd_lcd_deinit:1049` 调用。`lcd_isr:471–510` 没有该调用，故不把整个 GPIO 驱动强行放入 IRAM。GDMA 两个开关取原厂 `sdkconfig:2170,2172`，LCD_RGB_ISR_IRAM_SAFE 保持原厂未启用状态。
- [tps65185.c](tps65185.c) 逐个完整拷贝原 `bus/tps65185.c:15–75` 的寄存器访问、probe、VCOM 函数；[tps65185.h](tps65185.h) 原文保留作接口来源，产品仍用已有 ink 头。函数原文保留，通过 `power.c` 内局部宏将 `esp_err_t`、`i2c_port_t` 和错误名函数接到 HAL；不拷贝不使用的 thermistor/kickback/诊断入口。
- [epd_power.inc](epd_power.inc) 完整拷贝原 `display/epd_board_metalio_eink4_plus.c:815–979,986–1001` 的 `epd_board_poweron_once/poweron/poweroff`。全局 TPS 成功标志和端口通过局部宏接到栈上控制状态与已有 `epd_power_t`；VCOM 从已有字段取，REVID/attempts/retries 写回已有字段。移除原厂 LCD 拆装恢复调用（本端口没有拆 LCD，初始化只做一次）；最终自动重启及相应日志改为向 HAL 返回失败，遵守公共规矩第 2 条。其余寄存器、轮询、延时、GPIO 和 mask 顺序保持原文。
- [power.c](power.c) 是现有接口的胶水：原 `set_ctrl_with_tps:785–803` 的 GPIO/mask 接到原 TCA 单实例和锁；原 `tps_enter_sleep:386–408` 的已确认判断与 50/120 ms 顺序接到同一实例。TCA `out/stale` 继续作为唯一确认来源，不另设全局 desired/confirmed。共享供电 P05 的 300 ms 恢复等待及供电保留检测沿用原 ink 安全检查。GPIO PG 读取失败按低处理。关电失败保留 `stale`，HAL 维持 powered/armed 并禁止后续刷新；显示访问门失败，void 暂停接口停止执行，避免在互斥未取得时继续写 Flash。
- TPS STOP 分隔：原 `bus/metalio_i2c1.c:257–262` 的先 transmit、后 receive 顺序接到两个 HAL 调用，整对由已有 board 锁覆盖；失败不覆盖调用者的输出字节。现有 HAL 无事务超时/带超时锁接口，仍使用原 HAL 超时，不能宣称与原厂 1000/100 ms 等价，见任务报告。TCA 的读事务属于 port-board-2。

## 本目录文件 SHA-256

以下不包含本清单自身；原文范围和适配说明如上。

| 文件 | SHA-256 |
| --- | --- |
| `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `epd_power.inc` | `1ae1192a24f9a20fd325415ed98f0c0cfb74cd04ca7b2b34c64750d8a788496a` |
| `iram.patch` | `d4344d92397f8d345bfa3e3ed3140240949ce1e37bbdc6b57077cbfabf91e1b1` |
| `power.c` | `febefa12be3fd4a466f1e2831051cd83e94281513040c8e39cd2adeff3a91f96` |
| `power.h` | `71efb5a537cc1d1ae21937c7493d49efd330ec1ef6b0e760421f9610641898ba` |
| `tps65185.c` | `ba83620dd0b0f743ab1aa361be67acfcd8c5ac6f12162fd847d1bb5a914d79a7` |
| `tps65185.h` | `a42f21fb1aaeff5df91a8803f0db7cf4687ff8624d3acf4b96433b9aa9842880` |
