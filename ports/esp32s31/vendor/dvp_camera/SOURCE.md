# GC2145 相机会话

来源：公开 MIT 仓库 [CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus)，提交 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`（v2.0.77）。2026-11-11 从任务指定的只读上游副本读取，没有移动其 HEAD。

| 本地文件 | 上游路径 | SHA-256 |
| --- | --- | --- |
| `dvp_camera.h` | `main/boards/common/camera/dvp_camera.h` | `509e7b5717c4f19353bc1f4d35dbf348555400935170f78381e256963eff16af` |
| `dvp_camera.cc` | `main/boards/common/camera/dvp_camera.cc` | `52bdd6c28d637b9278119ac4af6d5ed4c31650b3a0e1662f2ef9086dc8ae2979` |
| `LICENSE` | `LICENSE` | `0a5a839033bfe18fe75d32b50d9d028912cf876f69ef59c2791aeb2971335d05` |
| `session.inc`（C 适配） | 以下逐项说明；不是原文 | `7346284ba97143a07278053d1a5e852e46759533c02de5e3f1d5de7445ccbfb9` |

前三份文件逐字保留。寄存器表继续使用相邻 `gc2145/` 中已验证一致的 Apache-2.0 版本，不重复拷贝。没有搬入原厂 JPEG 编码、上传、MCP 或页面。

## 实际参与编译的会话

`session.inc` 由 `hal_camera_s31.c` 包含，是 `dvp_camera.cc:50-229` 的 **C 适配**，不是声称逐字不变的 C++ 文件。任务限定只能改 C HAL、不改构建文件和 HAL 头，因此保留完整原文供逐行核对，再作必需的语言与接口转换：

- `Session` 的成员变成 HAL 已有的内部 RAM 上下文 `ink_cam_session`；成员访问与 `this` 回调改用该上下文，方法改为 `ink_cam_*` 静态函数；C++ 引用、初始化、`static_cast`、range-for 改为等价 C 表达式。`InitController` 原参数逐项保留（原文 178–202）；未自行定频率、引脚或 DMA 参数。
- `Check`（原文 72–75）保留先取消、再比较绝对截止时间；每次 `hal_camera_capture` 重新设置截止时间，取消回调读取服务的原子会话标志，并保留既有共享供电/复位检查。`Write`/`Read`（76–83）接现有 HAL SCCB 适配，保留 STOP 分读退路。写寄存器的原始总线函数不重复执行 `Write` 已执行的取消检查。
- `Prepare`（84–132）保留创建控制器/MCLK、10 ms、探测、page 0、分别读取 PID 高低字节及 ID 校验。原厂 `PowerPolicy`、电源租约与 `EpdFlashGuard` 接到已有 power/busy 和显示互斥；不切共享 P05/P12，不引入原厂应用依赖。日志沿用本项目固定文本。
- `Grab`（133–173）完整保留设表、停止传感器输出、PSRAM DMA 缓冲/信号量/回调、enable/start、启流、20 ms 可取消等待、校验及 stop 顺序。按任务明确要求，初始化块仅在 `frame == NULL` 时运行；后续帧复用缓冲和控制器，重置收帧计数、清信号量并将请求数置为预热数，避免再次丢八帧。
- `NextFrame`/`FrameDone`（201–219）仍只提交目标缓冲一次，后续 DMA 使用 IDF 备用缓冲，完整帧在复制期间不会被覆盖。首帧仍丢八帧。每次 Grab 按原文停止 DMA后再交还显示互斥；常开的是控制器/MCLK、传感器和缓冲会话，不让屏幕刷新与相机 DMA 并行。
- 析构（54–70）顺序保留：stop、disable、输出关、del/MCLK复位、free、删除信号量，再释放显示互斥。C 版本显式清零上下文，服务在离开相机应用、关闭受限相机或锁屏时投递清理；错误也清理。清理写寄存器不受已经取消/过期的请求阻止。
- ESP 错误码保持现有 HAL 的负数返回约定；不修改四个既有能力、成员隔离、快照或照片存储协议。

这些是任务要求的持续会话和 HAL/语言适配，没有真机验证。特别是帧间保留控制器后显示输出与 MCLK 的共存，必须按报告的上机清单复核。
