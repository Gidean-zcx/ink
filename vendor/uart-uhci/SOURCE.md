# UART UHCI 来源与完整性

- 来源：[78/uart-uhci](https://github.com/78/uart-uhci)，版本 0.2.2（Apache-2.0）。
- 取自 [CloudZao/Metalio-E-INK4-Plus v2.0.77，874538e](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538e/components/78__uart-uhci)，查阅日期：2026-11-11。
- 下表原始文件逐字节保留；本项目仅增加此说明与 `LICENSE`，未改组件源码或构建文件。
- `LICENSE` 是该原厂提交 `main/boards/common/camera/GC2145-LICENSE` 内的标准 Apache-2.0 全文，按任务书许可来源复制。

| 文件 | SHA-256 |
| --- | --- |
| `CMakeLists.txt` | `dd02a69efb9bed88442a622c42c2162a5112eaab3fff03fee9f04adda816c671` |
| `README.md` | `1dddb11b6aff39a6dd83bbf9729257c4b29d582d343f382ebac4ce90e2904ad8` |
| `README_zh.md` | `ac543272c7fcc9a1630f62df8b66eb69c8a0feac9773aab3b91d18110c2b3f7f` |
| `idf_component.yml` | `81efccb968f267f65078a6eb09dbe8cea15ffc6046c4f212df55b35f490d6b3f` |
| `include/uart_uhci.h` | `b119d9a9fc0fd6febe898f3ee32223d055634db6b8596b5827c4621abde0b23f` |
| `src/uart_uhci.cc` | `e200c95e4dd6281e72c16bbfd9228d4caa28be50f00e114172a7fd417e056892` |

接收使用原组件 UHCI/GDMA，4 块 × 1600 字节、burst 32；发送按原组件 `Transmit` 以同步 UART FIFO 写出，原组件不使用 TX DMA。
C 接口及缓冲所有权胶水位于 `ports/esp32s31/s31_uhci.*`、`s31_cell_rx.c`。
