# port-camera-1：S31 接上摄像头（GC2145，DVP），照原厂 v2.0.77

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-camera-1/REPORT.md。**只编译、跑宿主机测试，不碰设备。** 真机验证由主会话做。

## 背景

用户 2026-11-11 试用：“拍照也点不进去”。S31 的相机 HAL 是占位（`ports/esp32s31/hal_stubs.c:47-48` 返回没有相机），所以真机上相机打不开。系统侧已做好：`platform/include/ink/hal_camera.h`（契约：`hal_camera_info`、`hal_camera_capture`，640×480 RGB565，丢 8 帧预热，错误码 -1～-6），`system/services/camera/camera_service.c`，相机和相册应用，模拟器的 DVP 和 SCCB 模型 `simulator/models/dev_camera.c`。`vendor-feature-audit` 报告第 64 行把它列为“部分”。

原厂只读副本：`$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus-874538e`（v2.0.77），下文记作 V。**以 v2.0.77 为准**：它的 `dvp_camera.cc/.h` 和 `690a023` 不同，还新加了 `dvp_camera_adapter.cc/.h`。不要动 `ink-hw/upstream/Metalio-E-INK4-Plus`（固定在 `690a023`）。

原厂相关位置：

- V`/main/boards/common/camera/`：`dvp_camera.cc`（338 行）、`dvp_camera.h`、`dvp_camera_adapter.cc/.h`、`gc2145_vga_settings.h`（寄存器表）、`GC2145-LICENSE`（Apache-2.0）。
- V`/main/boards/metalio-e-ink4-plus/config.h:72-79`、`:101`：P05 摄像头和屏幕共用电源；SCCB 地址 0x3C；D0 GPIO46、PCLK 54、MCLK 55、VSYNC 56、HREF 57；MCLK 20 MHz；摄像头复位和触摸复位是同一根线。
- V`/main/boards/metalio-e-ink4-plus/metalio_e_ink4_plus_board.cc:112-123`：开机先 `dvp_camera::Detect`，失败就不提供相机。
- `dvp_camera.cc` 里用到的 `epd_flash_guard`、`power_policy`、`metalio_i2c1`：原厂怎样避开刷屏、共享电源和 I²C 总线冲突，照做。

## 要做的

1. **照原厂移植 DVP 驱动**：在 `ports/esp32s31/hal_camera_s31.c` 用 C 实现 `hal_camera.h`，换掉占位。
   - 探测、上电、复位、SCCB 读 ID、加载寄存器表、预热丢帧、取一帧、停止，顺序和时序与 V 一致，写清每处对应的原厂文件和行号。
   - 寄存器表原样放进来，附 Apache-2.0 许可证和来源；保留原厂 MIT 署名。
2. **共用的线不能出事**：
   - P05 同时给屏幕供电，复位线和触摸共用，I²C 总线和 TCA9555、TPS 共用。原厂怎样避开刷屏、怎样在复位后恢复触摸，就怎样做。
   - 拍照前后屏幕和触摸都要照常工作。
   - 现有的 `metalio_board` 扩展口接口够用就用，不另造。
3. **探测失败**：开机探测不到摄像头时，`hal_camera_info` 返回没有相机。
   - 相机应用打开后显示一页说明（例如“没有找到摄像头”），可以返回。
   - 不能弹一句提示就停在主页，用户 2026-11-11 报过：主页那句“这个应用还没有可打开的内容”一直不消失。那句提示由主会话另改，本任务只管相机应用这边。
4. 报告列出：和原厂 v2.0.77 的差别；S31 上相机用到的每个引脚和扩展口位，以及谁会去碰它们。

预算：产品代码净增不超过 700 行（不含原样放进来的寄存器表）。

## 验收

1. **宿主机测试**：用假的 SCCB 和 DVP（可以复用 `simulator/models/dev_camera.c` 的思路）测：
   - 探测成功或失败；读错 ID；
   - 预热帧丢掉、留下一帧；
   - 超时返回 -4；拍照中途掉电或复位返回 -5；
   - 拍照前后触摸复位线和屏幕电源的状态和原厂一致。
   - 反向对照：不丢预热帧、复位后不恢复触摸、刷屏中途去动 P05，各改一处，测试都要失败。
2. 模拟器全量和 ASan 版通过；S31 编过，产品代码 0 警告，报告镜像大小；gcccheck 通过；ui-scale 0 违规；隐私扫描。
3. 模拟器截图：相机应用在没有摄像头时的说明页，中文和英文各一张，目视检查无裁切、无遮挡。
4. 报告写清真机上第一步怎么验：只读探测（SCCB 读 ID）、拍一张、拍完摸屏和刷屏正常。
