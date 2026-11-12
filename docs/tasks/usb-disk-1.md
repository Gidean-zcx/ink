# usb-disk-1：插电脑时存储卡当 U 盘（功能清单 `usb_disk`，移植队列 T02）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7 条；第 8 条不碰设备）。

现状：系统层已有“借卡给电脑、归还”的流程（`system/services/file/file_service.c` 和 `hal_usb_disk_*` 接口），模拟器能用；S31 上 USB 这层还是占位，真机插电脑不会出现 U 盘。

## 要做的

1. 先读原厂 `$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e/` 里 USB 大容量存储（TinyUSB MSC）的做法，再看 ESP-IDF v6.1 自带的 TinyUSB MSC 组件；照原厂接法补 S31 的 `hal_usb_disk_*`（新文件 `ports/esp32s31/hal_usb_s31.c`），存储卡用原厂同样的 SDMMC 读写。原厂代码能拷就拷，写 `SOURCE.md`。
2. 沿用系统已有的借卡、归还：借出期间设备本地对卡只读或拒绝写，电脑弹出或拔线后恢复；不新增状态机。
3. 宿主机测试用桩证明：借卡后本地写被拒、归还后恢复、借出时拔线能恢复。

## 范围和预算

- 只改：新增 `ports/esp32s31/hal_usb_s31.c`、`ports/esp32s31/main/CMakeLists.txt` 和 `sdkconfig.defaults` 里 USB 那几行（只开 TinyUSB MSC 需要的，不改 Flash、分区、安全、Wi-Fi、4G 参数）、`ports/esp32s31/tests/` 的新测试、`docs/tasks/usb-disk-1/`。不碰别的任务在改的文件。
- 胶水不超过 250 行（拷来的原文不算）。

## 验收

1. S31 宿主机测试过（含新测试和反向对照）；主工程普通全量 ctest 过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小。
2. 报告写上机核对清单：主会话插 Mac 看到 U 盘、拷文件、弹出后设备上能看到新文件。
