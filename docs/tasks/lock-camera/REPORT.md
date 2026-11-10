# lock-camera 任务报告

基线：`ec78378e05f2694b9fa410780ab2bcf9d8de1c99`。状态：实现和回归完成，正在跑最终全量、内存和 S31 检查。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过（模拟器） | lock 110、gate 138、kid 126 条断言；卡上三张公共照片含前一次拍的照片；孩子场景再加一张采集中 HOME 的照片，仍无儿童归属；本次只回看新拍两张 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_camera/run.py --build build` |
| 2 | 【实测】定向、S31、GCC 通过；全量和内存检查待结束 | 原有锁屏、成员门、相机相册回归未修改；见下文 | 见下文命令 |
| 3 | 【实测】通过，逐张看过 | 下表三张为最终布局；另有成员门、密码页截图 | 同第 1 条 |

截图路径相对仓库根目录，未提交构建产物：

| 画面 | 路径 |
| --- | --- |
| 默认锁屏入口 | `build/lock-camera-lj7062pt/lock/snap-lock-entry.png` |
| 受限取景 | `build/lock-camera-lj7062pt/lock/snap-restricted.png` |
| 本次第二张的拍摄确认 | `build/lock-camera-lj7062pt/lock/snap-confirmation.png` |
| 成员门、密码页入口 | `build/lock-camera-lj7062pt/gate/snap-member-entry.png`、`build/lock-camera-lj7062pt/gate/snap-pin-entry.png` |

【实测】最终反向对照：`run.py --build build --negative` 仅撤掉公共存放选择，变异程序退出 1，122 通过、4 失败、finished=1；包装脚本确认归属断言失败后退出 0。原程序与工作树未变，证据 `build/lock-camera-d9zu4d2m/`，日志 `build/lock-negative.log`。

【实测】`taskpolicy -c utility python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build <本次 11 个产品 C 文件及新测试 C 文件>`：`gcccheck: 12 compiled, 0 with findings`，退出 0，日志 `build/lock-gcccheck.log`。

【实测】`INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh`：完整 S31 编译、链接、镜像生成、size、stack 报告均退出 0。镜像 1,342,416 字节，DIRAM 88,180 字节、PSRAM 静态 BSS 552,728 字节；日志 `build/lock-s31.log`，产物 `build-s31/ink-s31.bin`。仅编译，未烧录或运行设备。

## 没做到的

【实现】浅睡时触摸不是当前 HAL 的唤醒源（`hal_power.h`、`hal_input_sim.c`）；睡着后仍须按电源键或 AI 键唤醒。拍摄期间复用 `INK_NEED_AWAKE`，不解锁，退出释放。不改唤醒硬件或驱动，不碰设备。

## 和设计不一致的

【实现】规范 §3.5 仍写“相机没有入口、锁屏不响应触摸”，与本任务 D-121 相冲突。本次按任务实现入口，不改规范迁就代码。受限拍摄沿用现有 Camera 布局，左下角只回看本次照片，点画面恢复取景。

## 改过的已有测试

无；原有测试保持原样。

## 要用户定的

无。按任务授权选独立公共目录 `/family/锁屏拍的照片`，不属于任何家人，解锁后的各成员相册均可见。文件服务的公共写权限由现有成年成员身份执行，只写这个目录，不借其身份访问个人相册。这样无需新增“最后使用的大人”状态或迁移照片。

## 顺手发现

暂无。

## 新增的机制

受限拍摄浮层、本次照片编号列表：任务书第 2、3 条要求；退出即清空，不写新持久格式。

`camera.review` 原子能力：任务书第 5 条要求；只轮看本次拍摄编号。入口、取景和拍照复用原能力。

无新任务、线程、队列、文件格式、配置项；唤醒引用、图片解码和执行器均复用原设施。

产品代码 140 行增加、35 行删除，共 175 行（含头文件；连同 CMake 3 行和翻译 7 行为 185 行，未超 200 行）。超过 100 行的原因：入口要跨锁屏、成员门与密码页；策略、触摸与返回必须共用现有关口；还要保存、回看和解锁相册。看过只加按钮、直接打开普通相机、存给当前成员三种小方案，分别缺触摸路由、泄露旧相册、把照片存给孩子，不能满足任务。

【实测】初版锁屏用了整屏透明根对象，改变了浅睡与唤醒时序。仅恢复基线 `lock_mark.c` 的隔离对照使成员唤醒、拿起亮屏恢复通过；最终把默认入口放进原有提示条，其他区域用现有 `skip_touch` 丢弃。原有成员唤醒、待机预算、拿起亮屏、camera-gallery，加新 lock-camera 共 5 项均原样通过，`100% tests passed out of 5`，23.39 秒，退出 0。未修改原测试。日志 `build/lock-layout-regressions.log`。

【实测】策略初版扩大到所有锁定操作，回归检出了误拦截。最终只限制锁屏的相机/图片/文件与入口，以及受限拍摄页；原来的宽限期媒体、确认、网页和儿童时限回归均已恢复。只改了文档 `app-framework.md` 的两个源码行号链接，正文与测试未改。

【实测】完整保留“电源键或 AI 键唤醒”和“正在听”提示，并按两个次要文字行高留足提示条高度。大小字号锁屏、成员门、电源菜单、设置、正在听、待机、拿起亮屏、camera-gallery、lock-camera、翻译表和文档链接共 13 项原样通过，32.16 秒，退出 0；日志 `build/lock-final-ui-regressions.log`。

## 要改的文档

合并会话需重新生成 `docs/system/capabilities.md`：新增 `camera.review`，`app.open` 与相机能力的锁定标志改变。独立生成 `build/lock-capabilities.md`，461 项定义校验通过。

合并会话需同步界面规范 §3.5、`docs/system/ui-dependencies.md` 的 D-121 状态；本次不修改禁止的共享文档。

【实测】拍摄中退出：固定在采集开始前选择公共归属；恢复到 RGB 采集后才读模式的 `--late-mode` 对照，变异程序 125 通过、1 失败、finished=1，包装脚本退出 0，证据 `build/lock-camera-f18nhp3d/`。正常锁屏与密码页、孩子场景共 374 条断言通过。拍完两张后的 HOME 用真实按键；采集中的退出用同一 `system.home` 能力，避免把未送达的短按当成相机行为。
