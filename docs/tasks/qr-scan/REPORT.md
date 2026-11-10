# qr-scan 执行报告

基线：`eec5d569be028b539e8778690c239a6947767864`。本报告随本地里程碑提交更新；未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 待验证：测试图片、内容处理、失败提示、密码隔离与 AI 调用 | 待补 | 待补 |
| 2 | 待验证：原有测试、全量、内存检查、S31、gcccheck | 待补 | 待补 |
| 3 | 进行中：先查原厂帧格式，再比较识别库、中英文截图 | 待补 | 待补 |

## 选库与硬件依据

【实现】选 quirc（ISC），固定 `f2e4271ae43a15d9263704e07bc86cba5697d74d`，原文、许可、哈希在 `third_party/quirc/`。它是 C，分配能直接接入 OSAL PSRAM；此次只需二维码。zxing-cpp 2.3.0（Apache-2.0）同图更快，但还需 C++ 运行时和分配适配，体积待交叉编译核对。

原厂依据（只读，查阅 2026-11-10）：CloudZao/Metalio-E-INK4-Plus `690a023`，`main/boards/common/camera/dvp_camera.cc:24–28` 定义 640×480、614400 字节、大端 RGB565；`:145–146` 用 `MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA` 分配帧。本项目经原有 `ink_camera_read_rgb` 获取 PSRAM RGB888 后转灰度，不改摄像头参数。

【实测】同一组 11 张 VGA PNG（10 张有效、1 张空白），quirc 与 zxing-cpp 都为有效图 10/10、空白不误识别；包括倾斜 23°、模糊半径 1.2 和小码 3 像素/模块。主机独立基准：quirc 1.361–3.570 ms，zxing 0.560–1.829 ms；这不是虚拟时间或 S31 实测。生成方法见 `tests/qr_scan/README.md`。

乐鑫 esp-code-scanner：核查 ESP-WHO `idfv4.4` 固定 `db6874aad97ee05e59e13475d1b4ac48611aa33d` 的 `components/esp-code-scanner/CMakeLists.txt` 与目录树。只有 ESP32/S2/S3 的 Xtensa 预编译库，没有 S31/RISC-V、macOS/Linux 目标；无法在本次同图比较或链接 S31。仓库根为 ESPRESSIF MIT（限乐鑫产品），组件目录没有独立许可证及完整二进制依赖许可说明，故不作为许可已核清的候选。

上游链接：[quirc](https://github.com/dlbeer/quirc/tree/f2e4271ae43a15d9263704e07bc86cba5697d74d)、[zxing-cpp 2.3.0](https://github.com/zxing-cpp/zxing-cpp/tree/v2.3.0)、[乐鑫构建目标](https://github.com/espressif/esp-who/blob/db6874aad97ee05e59e13475d1b4ac48611aa33d/components/esp-code-scanner/CMakeLists.txt)。

## 没做到的

尚在执行；未接设备，实机识别率与耗时不属于本次实测。

## 和设计不一致的

待核查。

## 改过的已有测试

无；原有测试代码与断言不改。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

任务书要求的扫码模式：新增扫码页、`camera.scan` 与需人确认的 `camera.scan_apply`。复用现有 MEDIA/UI 执行器、确认服务、剪贴板、电话、联系人和 Wi-Fi 能力。未引入常驻任务、队列、缓存、后台重试、配置项或持久格式。

quirc 分配适配：所有堆区接 OSAL PSRAM；解码临时区由栈移到堆。原因是实测原函数栈帧 9280 字节，大于既有 8192 字节媒体栈；不放宽栈预算。

## 要改的文档

完成后由主会话更新共享能力表与界面依赖文档，本任务不修改。

## 验证与规模

主机构建通过；gcccheck 对 `scan.c`、`camera.c`、`tests/qr_scan/device.c` 为 `3 compiled, 0 with findings`。扫码专项 ASan/UBSan 为 `exit 0 assertions 144 failures 0`，证据 `build-asan/qr-scan-7svtrib4`；全量与移栈后的复测待补。产品代码预算 ≤300 行，不含第三方识别库。
