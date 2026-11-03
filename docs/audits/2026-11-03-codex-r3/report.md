# 第三轮软件质量复审（2026-11-03）

审计对象：`95587e5a5f641fa4fd44c313f366e69f244faeb1`（当前 detached HEAD）。审计已完成；核实表及证据按组增量写入，最后补齐本结论。

## 一页结论

**仍有严重和高级别问题，尚未达到“没有太大问题”的标准。** 本轮确认 **1 项严重、3 项高**，都在当前未修改的产品实现上复现；普通测试全过不能替代这些故障交错的验收。

| 编号 | 定级 | 最重要的结果 |
| --- | --- | --- |
| AUD4-001 | 严重 | 波形期间失电、完成前复电，显示仍可报成功；默认 EPDiy 漏掉短时掉轨，两条管线都漏掉 VCOM 瞬断。模拟器给出错误的关键供电结论。 |
| AUD4-002 | 高 | 唤醒恢复遇总线故障后，持续触摸让后台恢复一直等到抬手，期间按键配置不恢复、音量输入丢失。 |
| AUD4-003 | 高 | 唤醒尚待后台恢复供电时允许录音启动；返回成功，电源后来恢复仍无时钟、零样本。 |
| AUD4-004 | 高 | 开机板初始化失败后，按键能恢复，但音频初始化缺失；后续录音仍可假成功并持续零样本。 |

【复现、代码核查】任务指定 **12 项：7 项已修复、5 项部分修复**，未把旧问题重复计作新问题。定时器删除、内存统计、摄像头截止时间、开机按键绕过失败、macOS 状态原子化、CI 源码构建覆盖和目录分层达到本次核实范围；显示、唤醒、音频的恢复交错仍不完整。Linux 摄像头残留空帧/错误帧有效性问题，硬件对照文档仍有过满表述。

【复现】当前 Werror 构建 **40/40**；ASan+UBSan 构建 **39/39**；补充探针 **27 组配置**（其中 5 组带 sanitizer、3 组 PQ-BN）。目录重组前后分别 **37/37**，39 份指标对象和 410 张 PNG 相同，12 个包含边界编译检查通过。**未发现纯目录重组造成行为回归的证据**，不能把后来故障修复的遗漏归到搬目录本身。

本结论限定于模拟器与本机构建；没有实机测试、Linux 运行、真实摄像头采集或 ESP-IDF 端口验证。严重问题的定级依据第一轮“模拟器在关键结论上误导”，不声称已证实实机损坏。优先处理 AUD4-001～004 后，再用对应探针及正常路径对照复审。

## 修复核实表

| 编号 | 结论 | 证据 | 说明 |
| --- | --- | --- | --- |
| AUD3-001 | 已修复 | 【复现、代码核查、推断】 | ports/sim/osal/osal_sim.c:734–747、771–786：同一 VTK 锁内置 deleting、取消事件和 pending，等待正在运行的回调后再次 disarm。普通与 ASan 的 r2_timer_rearm_delete 均 exit=0，重启返回 OSAL_INVALID(-3)，calls=1，未出现 UAF。platform/include/ink/osal.h:165–181 的同步删除有条件可移植：设备端需要包装层的删除标记、在途计数/完成通知和自身删除延后回收，不能把底层原生删除直接视为此保证；现无 ESP-IDF 端口，本轮不声称设备端已经实现/验证。 |
| AUD3-002 | 部分修复 | 【复现、代码核查】 | hal_power_sim.c:370–390 和 hal_input_sim.c:582–658 补上同步 3 次重试及后台恢复。r2_wake_read_fault/control 均恢复 c0,e3、POWER 2 事件、VOL 后共 4 事件。持续触摸会使后台恢复饿死（AUD4-002）；供电尚未恢复就对新外设工作放行（AUD4-003）。永久总线故障下重试持续但有间隔，不阻塞普通应用任务；后台最大间隔实际为 800 ms（50<<4），并非文档所说 1 s。 |
| AUD3-003 | 部分修复 | 【复现、代码核查】 | hal_display_sim.c:520–531、744–751、1223–1231 修复持续失电：r2_display_cut 回调 ok=0。四刷新探针在 PQ-BN 上每个 ID 各失败回调一次，pending 归零；复电后新请求成功。仍不能识别波形内失电后复电，且 VCOM 瞬断两条管线均可误报成功，见严重问题 AUD4-001。 |
| AUD3-004 | 部分修复 | 【复现、代码核查】 | hal_audio_sim.c:359–396、404–494 与 hal_power_sim.c:434–449 的租约覆盖分配/启动/停止，原 entry/late/race 三个交错均修好（样本 9600/18960/9600、mode=clock=1）。分配失败与正常停止有释放路径；未见原交错死锁。但异步恢复 rails 期间仍允许启动，成功后持续零样本（AUD4-003）；启动本身也不传播 ensure_bt_mode 的未就绪。补充 r3_audio_oom：录音/播放分配失败均 -1，释放后睡眠 cause=1；r3_play_sleep_entry：mode=clock=1、写入 2400 样本、停止后可睡眠，未见这两条路径泄漏租约。 |
| AUD2-010 | 已修复 | 【复现、代码核查】 | simulator/memory/pools.c:130–149、219–236、355–363：公开 free/largest_free 按 TLSF 空闲块遍历，内部/DMA 低水位逐次分配更新。build-audit/pools-test exit=0；碎片案例 reported=actual=61536（第二轮相差 400 B），零池为 0，归属表 100000 次循环仍为 4096。外部池的历史低水位仍在两次采样间估算，不把它当逐次精确水位。 |
| AUD2-012 | 已修复 | 【复现、代码核查】 | hal_input_sim.c:663–685 以 sim_board_present 区分模型不存在与板初始化失败，不再在失败时绕过寄存器。r2_input_boot_fault：present=1 ready=0 keys=0；现有 selftest-expander-dead 和补充 r3_boot_audio 均证明总线恢复后按键 2 个且 board_ready=1。延迟初始化对音频的遗漏另记 AUD4-004，不把“键能恢复”升级为整板全部可恢复。 |
| AUD2-016 | 已修复 | 【复现、代码核查】 | drivers/chips/gc2145/gc2145.c:23–35、simulator/models/dev_camera.c:122–145 将截止时间传入完整设置表，并检查延时预算。r2_camera_deadline：0/1/100 ms 预算实耗 1.443/1.443/100.352 ms，均返回 -4；原先约 400 ms 无界超支消除。仍容许在途总线事务/调度粒度的尾差；不能解释为严格硬实时截止。整板自检包含正常采集和帧中断电，常规/ASan 套件均通过。 |
| AUD2-019 | 部分修复 | 【代码核查】 | simulator/desktop/gui/host_camera_linux.c:31–84、155–165：非零短帧不发布、DQBUF/QBUF 硬错误清 g_have 并发布错误状态、帧缓冲 calloc，原报告主要路径已处理。但 72 行仍把 bytesused=0 换成映射容量，空载荷可通过 whole_frame 并作为新帧发布；没有使用 V4L2_BUF_FLAG_ERROR 过滤错误帧。剩余为中级输入有效性问题；本机只作静态核查，未运行 Linux 或真实摄像头。 |
| AUD2-020 | 已修复 | 【代码核查、复现】 | simulator/desktop/gui/host_camera_mac.m:14、60–100、123、134：g_status 的所有读写统一为 C11 原子指针，指向静态字符串；既有 ctl 串行化和 gen 仍保留。macOS Werror 构建及 gui-smoke 通过。未申请摄像头权限、未运行 AVFoundation 实际采集，不据此证明所有媒体生命周期。 |
| AUD2-022 | 已修复 | 【文档核查、代码核查、复现】 | .github/workflows/sim.yml:61 的 macOS 配置明确 CMAKE_DISABLE_FIND_PACKAGE_SDL2=ON；CMakeLists.txt:8–11 提前启用 OBJC，235–249 拉固定 SDL2 release-2.32.10。按 TASK 的已缓存源码依赖完成本机配置/Werror 构建/40 项测试（exit=0），其中 gui-smoke 通过。未联网查看远端 CI，此处核实的是配置覆盖与本地等价源码分支。 |
| AUD2-023 | 部分修复 | 【文档核查、代码核查、复现】 | requirements.md:88 的 R-034 已改为按审计记录逐项处理；hardware-parity.md:15、32 已准确区分 EPDiy 提交模型和 SD 仅延时。其 16、20、28 行仍将掉电检测/唤醒后台恢复/复电重设模式写得过满；本轮 AUD4-001～004 复现反例，屏电源和实体键的故障一致性需重新降级。PQ-BN 的 PG 查询仅在更新可完成时（hal_display_sim.c:1173），不是每相固定查询，处理表“相与相之间”应注明此边界。 |
| AUD2-024 | 已修复 | 【复现、代码核查、文档核查】 | CMakeLists.txt:53–85、145–174 分出 platform/drivers/board/system/apps/boot，system/apps 无驱动、板或模拟器 include 传播；refresh_policy 与 jsmn 已 PRIVATE。layout_check.py 的 12 个正负编译边界检查全部符合预期。4309087→dc085cb 的 85 个变动 C/C++/ObjC 源/头对照，除注释、board 包含名和 GUI 默认档案路径迁移外代码一致，无新增/删除遗失。独立导出并构建两份历史快照，各 37/37 headless 测试通过，39 份 metrics.json 与 410 张 PNG 逐字节/结构相同。模型/HAL 与 sim.h 仍合在 ink_port_sim 是明确记录的边界，不宣称已完成模型内部拆分。 |

全部 12 项已核实。

## 分组证据与进度

### 0. 对象与边界

- 【复现】`git rev-parse HEAD` → `95587e5a5f641fa4fd44c313f366e69f244faeb1`。
- 【复现】初始 `git status --short --branch`：detached HEAD，仅本审计目录为未跟踪内容。
- 按 `TASK.md`：单一主代理独立只读审计，不调用子代理或外部模型、不接触设备或私有实机资料、不联网、不提交或推送。
- 报告、JSON 和补充实验源码只写本目录；构建及测试临时产物使用任务明确允许的构建目录。原产品源码、测试、档案与既有文档不修改。
- 本任务只针对固定检出，不同步在线决策板或远端 Issue/PR，不改决策记录。

### 1. 基线、内存、摄像头与 CI（已核实）

- 【复现】普通 Werror 配置和构建 exit=0；CTest **40/40**，11.82 s（含 GUI）。ASan+UBSan、GUI 关闭的配置和构建 exit=0；CTest **39/39**，12.74 s。两者完整日志只在构建目录。
- 【复现】`python3 docs/audits/2026-11-03-codex-r3/experiments/run.py r2_camera_deadline` 和 `build-audit/pools-test` 的观测见核实表及 `experiments/results.json`。探针从第二轮复制，仅改包含路径与链接库；未修改产品。
- 【代码核查】Linux 后端仍把空载荷当成映射缓冲长度，故 AUD2-019 保留部分修复；macOS 原状态字段竞争已消除。

### 2. 定时器生命周期（已核实）

- 【复现】`python3 docs/audits/2026-11-03-codex-r3/experiments/run.py --asan r2_timer_rearm_delete`：重启 `rc=-3`，删除返回后 `calls=1`，exit=0，无 ASan/UBSan 错误。常规重跑同样通过；40/39 项套件均含 timer 删除/重启自检。
- 【代码核查】删除标记、事件撤销与在途等待共用 VTK 锁，回调退出后只清 `TS.running`，不再解引用已自行删除的 timer。
- 【推断】同等契约能用 FreeRTOS/ESP-IDF 包装层实现，但需控制对象存活期及回调任务中的自身删除。官方项目内的 `esp_timer_stop/delete` 调用只证明其使用方式，不能证明 native API 满足这里的同步契约；未找到本任务允许目录内的 IDF timer 实现，也未编译设备端。

### 3. 显示、唤醒与音频交错（已核实）

- 【复现】补充探针 `r3_display_transient`、`r3_display_vcom`、`r3_display_multi` 在两种显示档案运行。四个在途 PQ-BN 请求各失败一次，pending=0，新请求可成功；普通短时掉轨由 PQ-BN 捕获，但 EPDiy 仍假成功，VCOM 瞬断两条管线均假成功。
- 【复现】唤醒短故障对照恢复正常；`r3_wake_touch` 证明持续触摸使恢复延后到抬手。`r3_wake_audio` 与 `r3_boot_audio` 分别证明异步恢复准入和开机延迟初始化会留下零样本。原始摘录在 `experiments/results.json`；详细步骤见下方新问题。
- 【代码核查】锁序：显示电源锁→板锁→I2C；GPIO2 处理读完板寄存器后只原子累加掉电次数，不反向取得显示电源锁。当前 VTK 单任务执行，不能据此证明任意 SMP 上的 `D.rails_on` 裸读写安全。
- 【复现/代码核查】永久总线故障下程序继续跑到 20 s，后台重试无次数上限、间隔封顶 800 ms；这是持续恢复机制而非阻塞死循环。未恢复期间 expander 按键不可用，POWER 合成按下要等读成功才配对释放；未宣称坏总线还能提供正常按键。

### 4. 目录重组与文档（已核实）

- 【复现】`python3 docs/audits/2026-11-03-codex-r3/experiments/layout_check.py`：系统和应用可包含平台/系统接口，不能按公开包含名引入 `sim.h`、`vtk.h`、`tca9555/tca9555.h`、`metalio_plus/metalio_board.h`；12 个编译判定通过。
- 【代码核查】纯重组提交 `dc085cb` 相对 `4309087` 的 85 对变动代码文件，去注释并映射 board 包含名、GUI 默认档案路径后无逻辑差异，所有 rename 均有去向，无新增/删除文件。比较只针对纯重组提交，不混入后续修复行为变化。
- 【复现】`history_check.py` 把两份 Git 快照导出到 `build-audit/history/` 后分别离线构建（Werror、GUI 关闭）、跑 37 项历史测试：均通过；39 份 metrics.json 完整对象相同、410 张 PNG SHA-256 相同，无单边遗漏。历史 GUI 未重编，当前版本 GUI 冒烟已通过。证据在 `experiments/layout-evidence.json`、`history-evidence.json`。
- 【文档核查】R-034 与 SD 延时描述已校正；硬件表仍应按本轮故障反例下调/限定第 3、7、15 项，不能沿用笼统“已补完”作为验收结论。既有历史记录保留当时路径是明确约定，不算漏搬。

## 新问题

### AUD4-001｜严重｜瞬态失电在完成检查前恢复，两条显示管线仍可回调成功

**范围**：显示 HAL / 屏电源模型。**证据**：【复现、代码核查】。

**位置**：`ports/sim/hal/hal_display_sim.c:520`；`ports/sim/hal/hal_display_sim.c:744`；`ports/sim/hal/hal_display_sim.c:1174`；`ports/sim/hal/hal_display_sim.c:1218`；`ports/sim/hal/hal_display_sim.c:1278`；`simulator/models/dev_epd_power.c:75`。

**核实与复现**：运行 python3 docs/audits/2026-11-03-codex-r3/experiments/run.py r3_display_transient r3_display_vcom，再加 --pqbn 运行同名项。默认档 rails cut 在 1.930 s ready=0，2.013 s ready=1 且 playing=1，2.345 s id=1 ok=1；VCOM 组两档均 ready=0→1 后 id=1 ok=1。PQ-BN 普通 rails cut 对照 id=1 ok=0，说明并非所有掉电检测都失效。见 experiments/results.json；截图仅存构建目录。

**影响**：AUD3-003 的持续失电复现已修好，但结束时 PG 正常不等于整个波形供电正常。默认 EPDiy 不使用已观测的 supply_drops 代次，失电后恢复会把完整目标图提交并报告成功；PQ-BN 的普通掉轨可失败，但 VCOM_CTRL 瞬态拉低不改变 P07，PQ-BN 仍在无 VCOM 的时段推进光学模型，最终报告成功。按第一轮定级，这是关键供电/显示结论假阳性（严重），不是实机画质推断。

**建议**：为每次刷新锁存电源有效性失效代次，EPDiy 和 PQ-BN 在每次提交成功前校验同一代次；供电模型应涵盖 rails 与 VCOM 的有效性变化，PQ-BN 无供电时不推进面板。失电一旦发生，即使本次结束前恢复也应失败，并明确重新初始化/重画策略。

### AUD4-002｜高｜持续触摸占住输入循环，扩展芯片后台恢复被无限推迟

**范围**：输入 HAL / 唤醒恢复。**证据**：【复现、代码核查】。

**位置**：`ports/sim/hal/hal_input_sim.c:646`；`ports/sim/hal/hal_input_sim.c:735`；`ports/sim/hal/hal_input_sim.c:743`；`ports/sim/hal/hal_input_sim.c:753`。

**核实与复现**：python3 docs/audits/2026-11-03-codex-r3/experiments/run.py r3_wake_touch。POWER 400 ms 唤醒，401–650 ms 注入 io-expander 错误，470 ms 开始持续触摸；5.100 s cfg=00,80 keys=1，5.610 s 音量点击后仍 keys=1；松触摸后 5.640 s 补 POWER 释放，6.120 s cfg=c0,e3 keys=2。r2_wake_read_fault 无触摸对照收齐 POWER/VOL 4 个事件。

**影响**：restore_due 只在输入任务的外层循环处理；持续触摸的内层循环只处理 exp_irq/exp_confirm。若唤醒恢复正好需要后台重试，触摸按住多久，按键方向就继续停留睡眠配置多久，POWER 松开与音量输入都无法送达。探针中总线已恢复数秒，但配置仍为 00,80；松开触摸才恢复 c0,e3。不是永久总线故障导致的合理降级。

**建议**：在触摸轮询循环中同样提取并服务 restore_due，或将恢复放到独立的有界工作队列；统一与 sample_lock 的状态迁移，补持续触摸、恢复重试、按键去抖同时发生的回归。

### AUD4-003｜高｜唤醒供电尚待后台恢复时释放准入锁，录音仍假成功并永久缺少时钟

**范围**：电源 HAL / 音频 HAL。**证据**：【复现、代码核查】。

**位置**：`ports/sim/hal/hal_power_sim.c:379`；`ports/sim/hal/hal_power_sim.c:448`；`ports/sim/hal/hal_audio_sim.c:299`；`ports/sim/hal/hal_audio_sim.c:368`；`ports/sim/hal/hal_input_sim.c:648`。

**核实与复现**：python3 docs/audits/2026-11-03-codex-r3/experiments/run.py r3_wake_audio。2.700 s sleep 返回 POWER、power=0；2.720 s capture_start rc=0 mode=0 clock=0；故障 3.140 s 解除，5.230 s power=1 但 samples=0 mode=0 clock=0 busy=1。停止后睡眠 cause=1，说明此复现不是单纯租约泄漏或死锁。

**影响**：睡眠准入租约修复了正常进入阶段的竞争，但恢复失败会把 rails 恢复转交后台后释放 g_admission。此时启动音频，ensure_bt_mode 看到 P14 关闭直接返回，capture_start 仍置 cap_on 并返回 0。后台把电源恢复后没有再次设置 BH 模式，持续零样本；用户不主动 stop/restart 就无法恢复，播放入口具有同样控制流。

**建议**：让准入状态区分已唤醒和外设供电恢复完成；音频启动必须取得有效供电并确认模式就绪，否则有界等待或明确返回失败并释放租约/缓冲。后台复电需通知音频重新设置，补长于同步重试窗口的故障。

### AUD4-004｜高｜扩展芯片延迟初始化后，BH1098G 初始化永远缺席

**范围**：开机恢复 / 音频初始化。**证据**：【复现、代码核查】。

**位置**：`ports/sim/hal/hal_audio_sim.c:300`；`ports/sim/hal/hal_audio_sim.c:511`；`ports/sim/hal/hal_input_sim.c:663`；`simulator/runtime/sim_main.c:141`。

**核实与复现**：python3 docs/audits/2026-11-03-codex-r3/experiments/run.py r3_boot_audio（脚本自动加 --i2c-faults 1:io-expander）。在应用入口解除故障并触发 POWER：board_ready 0→1、按键共 2 个；3.640 s capture_start rc=0 mode=0 clock=0；6.150 s power=1 samples=0 busy=1。

**影响**：sim_audio_init 只在开机时 board_ready 为真才创建 bt_lock/启动模式设置任务；新的输入恢复路径能使 board_ready 从假变真，但不补音频初始化。此后 ensure_bt_mode 每次因 bt_lock 为空立即返回，录音可报成功、共享电源正常，却永远没有 BH 时钟和样本。与 AUD4-003 不同，单纯等 rails 恢复或再次录音也不能补上缺失的初始化对象。

**建议**：按硬件存在性初始化必需的同步对象，并让芯片启动按 ready/电源代次惰性重试；或增加完整的板级恢复通知，避免仅恢复按键就宣称整板可用。为开机总线故障后恢复加入音频端到端检查。

## 命令、覆盖与限制

### 对象、来源与已执行命令

- 最终审计提交仍为 `95587e5a5f641fa4fd44c313f366e69f244faeb1`；纯重组比较为 `4309087 → dc085cb`。
- 2026-11-03 核对本地官方源码提交 `690a023c66862af103a3187464373ee7c5efcd14`，仅读取 TASK 允许的 `ink-hw/upstream/Metalio-E-INK4-Plus/`。重点为 `main/boards/common/power_policy/` 与 `main/boards/metalio-e-ink4-plus/` 的睡眠、供电及按键实现；未读取私有实机资料。
- `git rev-parse HEAD`、`git status --short --branch`、`git diff --name-only`、`git diff --cached --name-only`：核对固定提交与只读范围。
- `cmake -S . -B build-audit -DINK_WERROR=ON …`（TASK 指定离线依赖，另开 `CMAKE_EXPORT_COMPILE_COMMANDS`）；`cmake --build build-audit -j 8`；指定 pyenv 的 `ctest --test-dir build-audit -j 8 --timeout 300 --output-on-failure`。三步均 exit=0，40/40。
- 同样配置 `build-audit-asan`，附加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`；构建和 CTest 均 exit=0，39/39。探针 5 个 sanitizer 场景未见 ASan/UBSan 诊断；禁用了 LeakSanitizer，不声称检测了全部内存泄漏。
- `build-audit/pools-test`：exit=0；`experiments/run.py` 的 27 组场景都正常退出，但含四项缺陷的明确反例。具体模式、参数、期望与实际值见[复现说明](experiments/README.md)和[观测摘要](experiments/results.json)。
- `python3 docs/audits/2026-11-03-codex-r3/experiments/layout_check.py`、`history_check.py`：exit=0，结果见[分层证据](experiments/layout-evidence.json)与[历史等价证据](experiments/history-evidence.json)。
- `python3 docs/audits/2026-11-03-codex-r3/experiments/validate.py`：审计包结构、链接、位置、观测与范围检查；结果见[交付核验](experiments/artifact-validation.json)。这项通过表示证据包一致，不表示产品验收通过。

### 未查与限制

- 本机为 macOS，Linux 摄像头后端仅静态核查；macOS 构建/GUI 冒烟未申请真实摄像头权限，未检查媒体设备切换和实际 AVFoundation 流。
- 不联网，因此未查看远端 CI 运行、在线决策板或 Issue/PR；CI 判断限于工作流配置与本机源码构建分支。没有新建分支、提交或推送。
- FreeRTOS/ESP-IDF 端口尚未实现。同步定时器删除是有条件可移植的接口契约，尚无设备端实现证据；VTK 串行调度也不能替代 SMP/ThreadSanitizer 检查。
- 没有设备功耗、画质、真实时序和原理图校准；仿真截图变化只证明模拟器更新了像素。未将上游具备某功能等同于本项目在目标板验证过该功能。
- 【复现、文档核查】额外 P05 探针显示：P05 拉低时模拟器的 panel_ready 仍为 1、刷新仍成功；上游 `config.h:72` 和 `display/epd_board_metalio_eink4_plus.c:199` 将其称为摄像头/屏幕电源。其实际电气影响未用原理图或实测确认，作为模型覆盖边界保留，未计入四项已确认新问题。
- 本轮按 TASK 聚焦后续修复、目录边界及故障交错，没有重新开展第一轮所有领域的穷举审计；没有发现不等于不存在其他缺陷。

产出仅在本审计目录；运行日志、大型图像、历史快照和构建产物仅在任务允许的 `build-audit/`、`build-audit-asan/`。最终 Git 检查未见已跟踪文件变化，仅本审计目录为未跟踪产出。每个交付文件均小于 1 MB。
