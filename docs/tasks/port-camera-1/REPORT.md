# port-camera-1 执行报告

基线：`aef4331de2f78e5cbfbc7b087d8b81ce1cacde2d`。查阅：2026-11-11。只编译、跑宿主机测试，未连接设备、未发布。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】相机定向测试通过 | 实际 C HAL、实际板级/TCA 驱动，假的 SCCB/DVP；成功、缺席、错误 PID、STOP 分读回退、丢 8 帧保留第 9 帧且不覆盖、超时 -4、掉电/复位 -5、短帧 -6、失败清理及共享线 | `ctest --test-dir build -R '^unit-s31-camera$' --output-on-failure` |
| 1 反向 | 【实测】三种错误全部被抓住 | 不丢预热帧、复位后 P12 未恢复、刷屏中改 P05：各修改一处，都编译成功并因断言失败退出（-6） | `python3 ports/esp32s31/tests/camera_negative.py build` |
| 2 普通版 | 进行中；已有基线失败 | `capability-table-doc` 缺 `calendar.list`；纯基线副本同样失败，其余结果待全轮结束 | 下方构建命令 |
| 2 内存检查 | 未运行 | 待记录 ASan/UBSan 和 S31 宿主机检查 | 待记录 |
| 2 S31 | 未运行 | 待记录镜像大小和警告 | 待记录 |
| 2 gcccheck / UI / 隐私 | 【实测】前两项通过；最终隐私扫描待提交后运行 | `gcccheck: 4 compiled, 0 with findings`；`ui-scale` 通过 | 下方命令 |
| 3 | 【实测】中英文说明页通过 | 启动时模拟无相机，仍可打开 Camera；说明、关闭按钮和 AI 快照一致；实体点按关闭可返回；截图已目视检查无裁切、无遮挡 | `INK_KEEP_EVIDENCE=1 python3 tests/port_camera/run.py build` |
| 4 | 【实现】已列真机首轮步骤 | 见“上机先核对清单”；未把宿主机结果写成真机验证 | 主会话操作 |

## 实现与原厂出处

下文 V 是任务指定的原厂 v2.0.77（档案目录标记 `874538e`）；档案无 Git 元数据，文件 SHA-256 已记在 [NOTICE](../../../ports/esp32s31/vendor/gc2145/NOTICE)。没有修改原厂目录。

| 项目 | 原厂文件与行号 | 本次实现 |
| --- | --- | --- |
| 开机探测失败不提供硬件能力 | `main/boards/metalio-e-ink4-plus/metalio_e_ink4_plus_board.cc:112-123` | `hal_camera_info` 首次在服务启动时探测并保留结果；失败返回 false、清零尺寸 |
| 电源、复位 | `display/epd_board_metalio_eink4_plus.c:146-183`，相对板目录 | 只读取 P05/P12 并要求为高，不上电/断电、不打硬件复位脉冲；复用 metalio_board，不另造扩展口驱动 |
| MCLK、探测 ID | `common/camera/dvp_camera.cc:84-131`，相对 boards | 同一 DVP 控制器先输出 20 MHz，等 10 ms，再探测 0x3C、选页 0、分别读 F0/F1，要求 0x2145 |
| 软件复位、寄存器、取帧 | 同文件 `133-173` | 软件复位和 16 ms 等待来自原表；加载全表、关输出、申请 PSRAM DMA、注册回调、enable/start、开 F2、等待帧 |
| DMA 参数和预热 | 同文件 `178-219` | 640×480 RGB565 高字节在前、8 位、burst=64；只给第 9 次请求输出缓冲，其余用 IDF 备用帧，防下一次 DMA 覆盖照片 |
| 清理、互斥 | 同文件 `54-70`、`232-249` | stop、disable、FE/F2 关输出、del、释放 MCLK、信号量和 PSRAM；显示门覆盖控制器初始化至清理；闪存块读写/擦除使用同一门 |
| SCCB 分读回退 | `common/power/power_i2c.c:44-68` | 重复 START 失败后，等 100 μs、写寄存器、等 100 μs、读一字节 |
| 触摸复位 | `display/epd_board_metalio_eink4_plus.c:615-630` | 原厂主动复位触摸是 P12 低 10 ms、高后等 300 ms；**原厂相机从不调用它**。正常拍照保持 P12 高，无需重新配置触摸 |

原厂寄存器表按原文字保存，仅将 `<cstdint>` 和数组的 C++ 类型限定改成 C；值、顺序、注释不改。Apache-2.0 和原厂 MIT 版权/许可证随表保存。非正常拍照的外部掉电/复位只报 -5，不擅自拉起共享电源或复位线。

## 引脚和扩展口所有者

依据 V `main/boards/metalio-e-ink4-plus/config.h:72-103`；下表是【实现】核查，尚非真机测量。

| 引脚 / 位 | 相机用途 | 其他写入者或共享者 |
| --- | --- | --- |
| GPIO46 / 47 / 48 / 49 | D0 / D1 / D2 / D3 输入 | DVP 控制器；当前端口无其他功能使用 |
| GPIO50 / 51 / 52 / 53 | D4 / D5 / D6 / D7 输入 | DVP 控制器；当前端口无其他功能使用 |
| GPIO54 | PCLK 输入 | DVP 控制器 |
| GPIO55 | 20 MHz MCLK 输出 | DVP 控制器；会话结束 `gpio_reset_pin` 释放 |
| GPIO56 | VSYNC 输入 | DVP 控制器 |
| GPIO57 | HREF 输入 | DVP 控制器 |
| GPIO0 / GPIO1 | SCCB SDA / SCL，共享 I²C1 | `hal_i2c_s31` 维护唯一总线；TCA9555、TPS65185、FT6336U、充电/电量计、RTC、加速度计共用；IDF 同步传输按总线串行 |
| TCA P05（Port0 bit5） | 摄像头和屏幕共享电源，须为高 | board_init 拉高；睡眠 cut_rails 拉低、restore_rails 拉高。相机只读；采集 busy 阻止浅睡 |
| TCA P12（Port1 bit2） | 相机、触摸、4G 共享复位，须为高 | board_init 与 modem 启动保证高；睡眠 mask/restore 保留。相机只读；低电平会返回 -5 |
| TCA P00/P01/P02 | 不直接用于相机；屏幕高压控制 | 显示驱动/看门狗和睡眠；取得相机显示门前先等波形完成，再关闭高压并撤销驱动中的看门狗计时，P05 保留 |
| TCA P14（Port1 bit4） | 相机不使用 | 4G/蓝牙音频/功放及睡眠管理；相机不修改 |

GPIO5 是触摸中断，GPIO2 是 TCA/加速度计共享中断，相机不重新配置它们。相机读取 TCA 通过现有板锁，屏幕、触摸与 SCCB 共用同一 I²C HAL。

## 和设计不一致的

- 【实现】HAL 头注释写 DMA 直接填调用者缓冲，但现有相机服务用 `ink_mem_alloc(INK_MEM_PSRAM)`，其 16 字节头部不能保证 IDF 相机要求的缓存行对齐。本端口照原厂用 `esp_cam_ctlr_alloc_buffer` 得到对齐 PSRAM 帧，停 DMA 后复制 614400 字节给调用者；多一帧 PSRAM 和一次复制，没有修改通用分配器或服务预算。IDF 另有一帧备用 DMA，共 1228800 字节临时像素缓冲，服务自己的帧另计。
- 【实现】沿用本端口 I²C 的 50 ms 单次传输上限，原厂相机传 100 ms；回退的 100 μs 间隔相同。等待完成按原厂 20 ms 轮询，截止时间覆盖等待显示、配置及采集，失败清理仍必须完成。
- 【实现】原厂 `power_policy` 与共享复位租约映射为现有浅睡 busy 检查和显示互斥；当前代码无运行中拉低 P12 的普通功能，不增加独立复位锁。P05/P12 低时照原厂拒绝，不自动修复电平。
- 【实现】不移植 `dvp_camera_adapter` 的 JPEG、MCP/HTTP 上传：本任务合同是现有 RGB565 HAL，编码、保存和 AI 调用继续由现有服务/能力负责。

## 没做到的

- 真机 SCCB、光学成像、触摸与刷屏恢复未测试，本任务禁止接设备。
- 全量结果尚待收齐；共享能力目录的基线失败不能在本任务里修改。

## 改过的已有测试

- `ports/esp32s31/tests/power.c`：增加相机 busy 假状态及拒绝睡眠断言；不改已有期望。
- `ports/esp32s31/tests/storage.c`：给显示暂停增加记录，并要求实际闪存块操作持门；不改已有期望。
- `ports/esp32s31/tests/display.c`：释放新增互斥量；新增相机持门时波形等待、释放后正常刷屏的确定性检查；不改已有期望。

## 要用户定的

- 无新增产品决定。调用者直接 DMA 的缓冲契约与当前资源分配器之间的差异，留给主会话决定后续是否统一；本次采取原厂对齐帧加复制的可逆做法。

## 顺手发现

- 【实测】指定基线的 `capability-table-doc` 已失败：`calendar.list` 已实现但目录仍为 448 项，应为 449 项。已用 `git archive` 提取纯基线的 system、apps、生成脚本和目录，运行 `python3 tools/capabilities.py --root work/port-camera-baseline --check`，同样退出 1；不是相机改动新增能力。未修改禁止改动的共享目录。

## 新增的机制

- 相机操作互斥、启动探测结果、单次 DVP 会话及完成信号量：任务书第 1 项要求，照原厂；无新增任务/线程。
- 显示/闪存/相机共用互斥门与相机 busy：任务书第 2 项要求，避免刷屏、PSRAM DMA、闪存和浅睡冲突；复用显示暂停入口。
- 无新增持久格式、配置项、缓存、队列、后台重试或通用框架。SCCB 同步回退是原厂现有流程。
- R-061：硬件留在 HAL；按钮与 AI 仍走同一组 camera 能力，无相机页通过原有页面语义接口提供相同状态和返回动作。

## 为什么不能更小

本次产品代码超过 100 行、低于 700 行预算，准确净增待最终统计。看过的更小方案：继续返回占位无法接硬件；直接用旧版 gc2145 表不符合 v2.0.77；直接 DMA 写资源管理器缓冲无法保证对齐；只调用原来的显示暂停空函数无法阻止刷屏。必要部分是 IDF 控制器/ISR 生命周期、原厂寄存器流程和共享资源互斥；没有拆通用框架或逐个调用方加补丁。

## 要改的文档

- 主会话统一更新 `docs/system/capabilities.md`，补基线新增的 `calendar.list`。
- 主会话将 `vendor-feature-audit` 的摄像头状态改为“已实现并编译/宿主机测试，待真机”，补本报告链接。
- 主会话在 `docs/system/ui-dependencies.md` 记无摄像头说明页和实际验证边界；如需更改 HAL 直接 DMA 注释，应与资源分配器方案一并处理。

## 上机先核对清单（仅供主会话）

1. 先只运行启动探测：确认 P05、P12 原本为高；不脉冲、不改共享输出。确认日志 `camera: SCCB PID=0x2145`，不加载 VGA 表、不拍摄；型号不符或未应答时，相机应用仍能打开说明页和返回。
2. 打开相机拍一张：预期 640×480、RGB565 高字节在前，第 9 帧保留，照片成功写入存储卡并可在相册查看；重点核对颜色、朝向、曝光和实际耗时是否落在服务 3 秒期限内。【估算】芯片 15 fps，8 帧预热加保留帧约 0.6 秒，另有 SCCB 和显示等待，不能当实测。
3. 拍完立刻触摸返回、滑动和点按，再刷屏/翻页；确认无触摸失效、花屏或看门狗停屏；再次拍摄，核对控制器和 MCLK 能重复申请释放。确认正常路径 P05/P12 全程保持高，P00/P01/P02 仍由屏幕电源顺序控制。

## 复现命令

依赖环境沿用任务给定的 `INK_DEPS`、`INK_DESIGN`；不把本机绝对路径写入仓库。macOS 前缀 `taskpolicy -c utility`，Linux 去掉此前缀。

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON -DINK_CJK_FONT=$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 -DINK_CJK_MEDIUM_FONT=$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf -DINK_CJK_BOLD_FONT=$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf -DPython3_EXECUTABLE=$INK_DEPS/build-deps/pyenv/bin/python -DFETCHCONTENT_SOURCE_DIR_LVGL=$INK_DEPS/build/_deps/lvgl-src
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/port_camera/run.py build
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python ports/esp32s31/tests/camera_negative.py build
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/port_camera/negative.py build
taskpolicy -c utility python3 ../../tools/gcccheck.py build system/apps/camera/camera.c system/services/camera/camera_service.c ports/esp32s31/tests/camera.c tests/port_camera/absent.c
```

截图留在忽略的 `build/port-camera-absent-*`，不提交构建产物；中英文各为 `snap-camera-absent-zh.png`、`snap-camera-absent-en.png`。
