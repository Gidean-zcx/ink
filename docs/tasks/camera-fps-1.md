# camera-fps-1：相机取景太卡，照原厂摄像头会话改成连续取景

先读 `docs/tasks/vendor-common.md`（公共规矩，第 10、11 条）。

用户 2026-11-11 试用：“相机低频好像也太低了 卡的不行”。

主会话查到的原因（`system/apps/camera/camera.c`、`ports/esp32s31/hal_camera_s31.c`）：

- 取景定时器 `lv_timer_create(preview, 3000, NULL)`：**每 3 秒才更新一帧**；
- 每一帧 `camera.preview` → `take` → `ink_cam_grab()`，都重新申请 DMA 帧缓冲、抓一帧、`heap_caps_free` 释放，控制器每次重来；
- 取景图用的是普通界面刷新提示（`INK_REFRESH_UI`）。

## 要做的

1. 照原厂 `main/boards/common/camera/dvp_camera.{h,cc}`（874538e，约 340 行，MIT；`gc2145_vga_settings.h` 已相同，不重抄）的 Prepare / Grab / 清理会话原文，接到 `hal_camera_s31.c` 下面：**取景期间会话常开、连续抓帧**（帧缓冲在取景开始时申请一次，离开相机页或锁屏时释放），拍照用同一会话抓全画质。原厂会话的取消回调、超时照搬。
2. `camera.c` 的取景节奏改成屏幕跟得上的最快速度：上一帧刷完再取下一帧（不要固定 3 秒），目标每秒 2 帧以上（以模拟器量到的为准，真机由主会话量）；取景图用快速刷新提示（`INK_REFRESH_FAST` 或系统里最快的那种，查 `system/ui` 现有的提示），拍照后的预览和相册照常用灰阶。墨水屏少刷新（D-127）照顾：只刷取景框那一块。
3. 审计 C17（首次探测遇瞬态失败会一直记成“没有相机”，双方共有）：照原厂行为保留，但在打开相机页时重新探测一次（不新增后台任务）。
4. 能力 `camera.preview`、`camera.photo`、`camera.capture`、`camera.scan` 保持兼容（AI 照样能调用）；扫码用同一个连续会话。

## 范围和预算

- 只改：`ports/esp32s31/hal_camera_s31.c`、`system/apps/camera/camera.c`、相机服务里取景节奏的那一处、`ports/esp32s31/tests/` 里相机测试和桩、新增 vendor 拷贝目录。
- 胶水新增不超过 150 行。

## 验收

1. S31 宿主机测试全过（相机测试加反向对照：会话常开时不会每帧重新申请缓冲）；主工程普通全量过；S31 编译通过。
2. 报告写清取景节奏、模拟器里量到的每秒帧数、上机核对清单（主会话在真机量每秒帧数和刷新时长）。
