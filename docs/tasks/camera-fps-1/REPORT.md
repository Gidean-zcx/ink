# camera-fps-1 执行报告

基线 `56a7b44d5932ecb954d00477eaa0e746fbe359c1`；产品代码提交 `5b0f5fbd`。本地提交，未发布、未操作设备。原厂来源为 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`，查阅日期 2026-11-11。

**未通过全部验收：帧率、严格新增行数预算未达标；S31 退出中拍照语义还有限制。** 普通全量、S31 宿主机、固件编译、内存检查和 GCC 检查完成。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a S31 宿主机及反向对照 | 【实测】通过 | 原话 `100% tests passed out of 30`；三帧只分配一次、一次预热、共享显示互斥、关闭重开、取消和超时清理通过；八项变异均编译成功并以断言退出 -6 | C1、C2；`host-tests-final.log`、`host-negative-final.log` |
| 1b 主工程普通全量 | 【实测】通过，3 项跳过 | 原话 `100% tests passed out of 391`，860.45 秒；388 通过、3 跳过，未修改跳过条件 | C4；`ctest-full.log` |
| 1c S31 固件编译 | 【实测】通过 | 构建、镜像打包、size、stack 全部退出 0；应用镜像 4,354,320 B；无产品编译警告，另有 5 条 SDK 配置提醒及 1 条 bash 补全提醒 | C5；`s31-build.log`、`build-s31/export.log` |
| 2 节奏、模拟帧率、上机清单 | 【实测】未达 2 fps | S31 HAL + 名义 15 fps 假 DVP + PQ-BN 屏幕：6.01 秒内 8 帧，1.3311 fps；默认旧模型 6.001 秒内 2 帧，0.3333 fps。两次基准均退出 1，未放宽断言 | C3；`preview-dvp-final.log`、`preview-model-final.log` |
| 2a UI 反向对照 | 【实测】通过 | 恢复 3 秒定时器、改回普通刷新、去掉显示完成门槛、去掉重探，各自触发对应未变断言 | C3R；`preview-reverse-final.log` |
| M 内存检查 | 【实测】通过 | S31 宿主机 ASan + UBSan，原话 `100% tests passed out of 30`，退出 0；按 vendor-common 不跑主工程 ASan | C6；`asan-tests.log` |
| G GCC | 【实测】通过 | 原话 `gcccheck: 4 compiled, 0 with findings`，退出 0，无跳过；会话适配随两个相机测试编译 | C7；`gcccheck.log` |
| B 代码预算 | 【实测】严格口径未达 | 既有产品文件新增 104、删除 107；C 会话适配另 138 行；非逐字原文产品代码共新增 242、删除 107，净增 135。242 大于 150 | `git diff --numstat 56a7b44d --` 后接三个产品文件及 `session.inc` |
| P 范围和隐私 | 【实测】通过 | 仅任务允许文件及报告；diff 空白检查通过；指定隐私正则 0 匹配，grep 退出 1；未提交构建产物、设计稿或本机绝对路径 | C8；`privacy.log` |

【实现】打开相机在既有 MEDIA 执行器上重探，UI 回调建控制；没有额外线程。首帧立即请求，HAL 仍持有显示互斥；后续沿用现有 LVGL 定时器，每 30 ms 检查 UI/显示是否完成，再经过一次 UI 周期取下一帧。常开的是控制器、MCLK、传感器及缓冲；每次抓完仍停 DMA，让屏幕使用共享资源。Camera 与 Scan 共用应用生命周期；离开应用、普通锁屏、关闭受限相机时清理。

【实现】取景二值化并使用 `INK_REFRESH_FAST`，只更新取景框，提示文字变化时才重绘提示。拍照保存仍是完整 VGA 彩色 PNG，拍后预览及相册仍走灰阶。能力 ID、参数、快照和成员策略保留。C17 首次负探测缓存仍保留，但打开页面重置缓存并重探；C16 原有 586 对寄存器表未改。

【实测】帧率是六秒窗口内成功抓帧数除以模拟器虚拟经过时间；不是主机墙钟或真机帧率。S31 正向基准完成，只有 2 fps 断言失败，其余断言通过，包括重探恢复、四个能力入口、同一缓冲用于拍照及扫码、离页/锁屏全部释放。假 DVP 没有真二维码，不能把扫码入口通过写成真码识别成功。证据目录：`build/camera-preview-k9gjbwsj`、`build/camera-preview-0ivgrr3j`、反向 `build/camera-preview-53147kg1`。

## 没做到的

- 未达到 2 fps；保留 `fps >= 2` 断言。性能处理三轮后停止追加方案，按任务规则交付未达结论。
- 未满足将全部 C 适配计入新增胶水的 150 行上限，见规模说明；没有继续扩大实现。
- 【实现】S31 离页/锁屏会取消在途取帧；已受理的拍照若尚未取得帧，也可能失败。未保留这一情况下旧的“HOME 后继续完成拍照”语义。默认模拟 HAL 没有会话钩子，既有 lock-camera 通过不能证明 S31 的这项兼容性。
- 真机 FPS、刷新时长、帧间控制器与屏幕共存未验证。Linux 原生全量未运行；仅在本机跑测试和 RISC-V GCC 检查。
- 中枢三项因缺少锁定的 `hub/.venv` 跳过：`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`；没有安装任务范围外环境。

## 和设计不一致的

- 原厂 `dvp_camera.cc:133–173` 每次 Grab 分配并停 DMA；按本任务保留会话和缓冲，后续不再预热八帧。原厂清理、超时和可取消等待保留，20 MHz、GPIO、640×480、DMA 参数未自行修改。
- `dvp_camera.h/.cc` 和许可证逐字保留，但实际编译的是 C 适配，未直接编译 C++ 类。来源、哈希和必要改动见[原文与适配说明](../../../ports/esp32s31/vendor/dvp_camera/SOURCE.md)。只读上游 HEAD 核对仍为 `690a023c66862af103a3187464373ee7c5efcd14`。

## 改过的已有测试

- 原有 S31 相机断言全保留，包括首次失败仍缓存。增加显式页面会话重探、连续缓冲和清理断言。
- `camera_negative.py` 原三项变异分别定位 HAL 或 vendor 会话；新增五项变异。已有 UI、翻译和其他测试期望未改。
- 新基准释放断言从“固定释放次数”修正为“所有实际分配均释放且 HAL 不忙”，适应超时重建；2 fps 断言保留。初轮全量两个失败及原因保留在 `ctest-initial.log`：旧中英文取景提示已恢复，帧间空闲窗口已修复；定点两项及最终全量均通过。

## 要用户定的

- 主会话需要选择：扩大构建文件范围，直接编译原厂 C++ 会话；或接受此 C 适配及超出的新增行数。建议前者。本次没有越过文件范围改构建，也没有压行数。
- S31 在途拍照退出后是否继续完成，应与已有“受理后完成”语义统一；本次未继续加请求类型和取消策略。

## 顺手发现

- 默认 `simulator/models/dev_camera.c:122–145` 每次仍写表、预热八帧，没有连续会话钩子；不在本任务允许范围。不能用 S31 桩测速度代表默认模拟相机已加速。
- S31 本次有 5 条 SDK 私有 include 依赖提醒及 bash 3.2 补全提醒，均不来自相机代码；原任务报告中也记录过同类提醒。没有改 SDK。早期 PQ-BN 基准曾出现 Flash/扫描重叠告警；最终正向基准无此告警，只剩帧率断言，未将早期模型告警认定为真机故障。

## 新增的机制

- 保留相机会话、缓冲和取消回调：本任务第 1 项明确要求。
- `hal_camera_session` 可选钩子、现有 MEDIA 执行器上的清理作业、应用前后台回调：用于会话生命周期和 Scan 复用。S31 ELF 的强符号及 `session_update` 调用重定位均核对到实际 S31 HAL；没有链接成默认空操作。
- 帧间完成确认的一个 UI 布尔标志：用于本任务的连续取景调度和输入窗口。没有新增常驻任务、线程、队列、文件格式或用户配置。

## 要改的文档

- `platform/include/ink/hal_camera.h` 仍描述每次装表、预热的单帧模式；需要主会话补充会话复用说明。本任务未改接口头文件。
- 共享需求、想法、决策、能力表和 UI 依赖文档均未修改。

## 规模与更小方案

逐字上游源码 371 行、许可证和测试文档另计；C 适配 138 行没有冒充逐字原文免计。更小的旧 C HAL 补丁不能满足直接复用原厂会话的要求；直接编译 C++ 类需要改未获准的构建文件并适配原厂电源、I2C、显示依赖。本次保留可核对原文和 C 适配，严格新增行数预算未满足。

## 上机核对清单

1. 页面打开有 PID=0x2145；连续取景测至少 20 帧的完成时间和实际可见时间，分别记录 FPS、局部刷新时长，达到 2 fps 才算目标通过。
2. 观察控制器/MCLK 是否被屏幕刷新破坏；取景框持续更新，状态栏和按钮不随每帧重刷，无 DMA/显示冲突、花屏或长时间残影。
3. 临时计数或 heap trace 核对 DMA 缓冲仅首次分配，取景、拍照、扫码复用；离页、普通锁屏和关闭受限相机后全部释放，重开正常。现有日志只有 PID 等信息，不声称已有每帧分配日志。
4. 首次 SCCB 瞬态失败后重新打开页面可恢复；抓帧超时、取帧中离页、拍照中 HOME/锁屏分别核对回执与落盘，尤其复核上面列出的在途拍照语义。
5. 真码识别、彩色照片 VGA 尺寸和灰阶回看、AI 四个入口以及受限相机的成员隔离，由主会话实机复核。

## 复核命令

全部日志在忽略的 `work/camera-fps-1/`。命令前均使用 `taskpolicy -c utility`，构建和 ctest 均为 `-j2`。`python`、`ctest` 使用 `$INK_DEPS/build-deps/pyenv/bin/`。没有启动带可见窗口的模拟器；全量的 gui-smoke 使用 SDL dummy。

- C1：`cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`；`cmake --build build-s31-host -j2`；`ctest --test-dir build-s31-host --output-on-failure -j2`。
- C2：`python ports/esp32s31/tests/camera_negative.py build-s31-host`。
- C3：`INK_KEEP_EVIDENCE=1 python ports/esp32s31/tests/camera_preview.py build`；默认旧模型另加 `--model`。两次均应复现上述非零退出，不注册为伪通过的 ctest。
- C3R：`INK_KEEP_EVIDENCE=1 python ports/esp32s31/tests/camera_preview.py build --reverse`，退出 0。
- C4：按 codex-rules 第 5 节的完整字体、Python、LVGL 配置 `build`，开启 `INK_WERROR=ON`；`cmake --build build -j2`；`ctest --test-dir build --output-on-failure -j2`。
- C5：`INK_UPSTREAM=$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus bash ports/esp32s31/build.sh`，默认 `INK_S31_LIGHT_SLEEP=OFF`。
- C6：C1 目录改为 `build-s31-host-asan`，配置仅加 `-DINK_SANITIZE=ON`，完整构建并 ctest。
- C7：`python $INK_DEPS/../ink-codex/tools/gcccheck.py work/camera-fps-1/gccbuild system/apps/camera/camera.c system/services/camera/camera_service.c ports/esp32s31/tests/camera.c ports/esp32s31/tests/camera_preview.c`。该临时目录的编译表取自普通构建；新 DVP 基准沿用 sim-lock-camera 参数并加相机桩 include 和 `INK_PREVIEW_DVP`。不改产品编译参数来躲警告。
- C8：`git diff --check 56a7b44d..HEAD`；对同一 diff 使用 codex-rules 第 6 节的原样隐私正则。0 匹配、grep 退出 1；最终提交后再核对。
