# 软件质量复审报告（第二轮）

日期：2026-11-03。审计对象：`4309087408aa57e0d4668e5cf13551cb3945438c`。

## 一页结论

**核查进行中，尚不能作出“没有严重或高级别问题”的结论。** 本报告按组更新；下表仅列已经核实的项目，未列项目不代表通过。

范围：AUD2-001～023 的修复与回归，及重点路径中的新问题。AUD2-024 按任务留到目录重组。全部由主代理完成；不调用子代理或外部模型，不改产品源码、原测试、档案及既有文档，不提交、推送或建分支。

## 证据与边界

- 【复现】：本机实际运行，附命令和输出；不等于实机验证。
- 【代码核查】：固定检出的代码控制流、锁与状态、上游对照。
- 【文档核查】：仓库现有说明与代码、测试的一致性。
- 【推断】：尚未运行验证的交错、影响或实机行为。

上游仅读取 `ink-hw/upstream/Metalio-E-INK4-Plus/`，目标为 `690a023`；不读取该目录以外的私有实机数据。不访问在线决策板、远端 Issue/PR 或真实外设：本次任务是固定检出的只读离线复审，亦不修改决策与需求。

## 修复核实表

<!-- verification:start -->
| 编号 | 结论 | 证据 | 说明 |
| --- | --- | --- | --- |
| AUD2-001 | 已修复 | 复现、代码核查 | ai_service.c:132–158、449–451：dismiss 调用幂等 end_capture(true)，清队列并置 drop_frames；松键先清理再判断界面。scenario-ai_dismiss_held-metalio-plus 通过，录音受 ≤2000 ms 门槛约束；关闭后迟到语音场景亦通过。跨轮完整取消仍属原有后续事项 F-5。 |
| AUD2-002 | 已修复 | 复现、代码核查 | storage_service.c:113–155、191–227：过大/OOM 缓存失效；读优先选最新 seq 的排队或在写版本；预取读后有更新则不缓存。storage_cache 探针：落盘 1200 B，读出 16 B 的 NNN，pending=failed=0；systest-services 通过。 |
| AUD2-003 | 已修复 | 代码核查、复现 | hub_link.c:111–119、493–533：状态原子访问，64 位 demand 受锁，发送额度先预留、分配失败退回；连接事件有代次。ws_cb 入队等待已限为 500 ms（132–147），close_ws 不持 tx_lock，未见该链路无限互等。现有 AI/断网场景通过；VTK 不构成真实 SMP 数据竞争验证。 |
| AUD2-004 | 部分修复 | 复现、代码核查 | hal_net.h:64–95、osal.h:165–181 补齐契约；hal_ws_free 等 task_done，普通 timer delete 等在途回调。但 timer 回调可在 delete 等待期间重新 start，删除未再次取消事件；ASan 复现 timer_fire heap-use-after-free，见 AUD3-001。 |
| AUD2-005 | 已修复 | 复现、代码核查 | sim/mem/pools.c:261–269 优先处理 EXTERNAL，EXTERNAL\|DMA 不再强制内池；memory_probe 返回 pool=2（external）。这里只确认能力标志路由，实机 DMA 缓存一致性与对齐仍需设备端验证。 |
| AUD2-006 | 已修复 | 复现、代码核查 | drivers/tca9555/tca9555.c:29–63：输出写成功才提交影子，失败置 stale，下次 RMW 先读回。tca_shadow：失败电机写返回 -2，随后成功写放大器，电机始终为 0；未见失败值被下一次写重放。 |
| AUD2-007 | 已修复 | 代码核查、复现 | metalio_epd_power.c:16–33 的 pin/read/shadow 通过 io_lock；hal_display_sim.c:432 绑定板级唯一锁，长延时和重试不持锁。常规与 ASan 的 sim-selftest、显示场景均通过；本轮未发现绕过板锁的 TCA9555 RMW。 |
| AUD2-008 | 已修复 | 代码核查、复现 | gc2145_start 加载完整 VGA 表；evidence.py 与官方 690a023 逐项比较 586 对寄存器/值完全一致。dev_camera.c:41–90、122–145 校验供电、复位、格式、裁剪、PLL 和逐帧输出；现有完整采集、掉电、超时自测通过。这是 SCCB 与帧时序模型，未声称验证实际 DVP 电信号、图像 ISP 或 20 MHz MCLK。 |
| AUD2-009 | 已修复 | 复现、代码核查 | drivers/nt26/nt26.c 的 UTF-16/UTF-8 转换加入代理对、残缺输入、过长编码与范围校验；nt26_unicode 探针的 D8000041、DC00、004、C080 均返回 -1，现有合法中英文与 emoji 自测通过。 |
| AUD2-010 | 部分修复 | 复现、代码核查 | sim/mem/pools.c:163、231–240、295–306：零容量池确实保持 0，连续分配时 free 与 TLSF 一致；但 free 时无条件回加块头，碎片场景仍高估。100 个 64 B 分配后隔个释放 50 个，reported_free=61936、tlsf_free=61536，多报 400 B。需按实际空闲块求和或正确计入分裂/合并块头；不重复设 AUD3 编号。 |
| AUD2-011 | 已修复 | 复现、代码核查 | sim/mem/pools.c:54–108 将 live 与 tombstone 分开，容量按同时存活项决定，墓碑占多则原容量重散列。100000 次分配/释放后 capacity=4096、live=0、host_bytes=65536、failed_allocs=0，未按历史累计增长。 |
| AUD2-012 | 部分修复 | 复现、代码核查 | 正常初始化后，实体键已从成功的 TCA 寄存器读值去抖，RTC 只通过 P16→INT→GPIO2，input_fault 零事件，board 自测含 RTC/P16 高低电平通过。但 hal_input_sim.c:601–611 把 board_ready=false 当成无模型而退回直接送事件。r2_input_boot_fault 从开机起 100% io-expander 故障，board_present=1、board_ready=0，仍上报 POWER 按下（keys=1）；初始化失败旁路未消除，沿用本编号。 |
| AUD2-013 | 已修复 | 复现、代码核查 | hal_input_sim.c:656 起的触摸轮询每轮服务 expander，不再等抬手；touch_blocks_power 在持续触摸期间收齐 POWER 按下/释放，延迟均约 50.736 ms，keys=2。 |
| AUD2-014 | 部分修复 | 复现、代码核查 | hal_power_sim.c:341–489 与 board 驱动已按上游执行停屏、静音、方向切换、P05→P14 断电、唤醒恢复；board 自测验证真实引脚、RTC、短按与长按无重复，电机开启拒绝睡眠。剩余：恢复 I2C 错误被吞掉（AUD3-002），进入阶段没有与音频启动互斥（AUD3-004），均已复现；电流仍为估计，不是续航证据。 |
| AUD2-015 | 部分修复 | 复现、代码核查 | I2S 的采集与播放已依赖 BH powered/ready/mode；audio_unpowered 500 ms 内 samples=0；board 自测验证复电后重设 mode=1、无时钟时队列超时。新增恢复窗口：ensure_bt_mode 在睡眠切换期间使用旧代次和 ready 时间，启动未计 busy，返回成功但无时钟/零样本（AUD3-004）。 |
| AUD2-016 | 部分修复 | 复现、代码核查 | dev_camera.c:122–145 已逐帧检查 streaming，中途断 P05 返回 -5（board 自测）；初始化中掉电也失败。整次截止时间仍未传给 gc2145_start 的 586 项写入/延时：r2_camera_deadline 的 timeout_ms=0/1/100 实耗 390.383/392.549/399.990 ms 才报 -4。现有 300 ms 自测只断言返回错误，未检查耗时；需在初始化循环中检查剩余预算。 |
| AUD2-017 | 部分修复 | 复现、代码核查 | metalio_epd_power_up 返回失败，hal_display_sim.c:682 拒绝执行波形并回调 ok=false；display_fault 输出 callback_ok=0、panel_ready=0，panel_power_dead 场景通过。波形执行中丢失电源仍更新目标图并 ok=true，见 AUD3-003。 |
| AUD2-018 | 已修复 | 代码核查、复现 | sim/vtk/vtk.c:87、734–739 的 stop_req 已原子化；sim_gui.c:122 在内核 host 回调采集 camera_captures，再通过 Gm 保护的快照给 GUI，gui_main.cpp:816 不再跨线程直读。gui-smoke 及 4 次启动/3 次 exec 重启观察完成；没有执行 TSan，结论限于原报告两个共享字段。 |
| AUD2-019 | 部分修复 | 代码核查 | host_camera_linux.c:103–155 已验证 S_FMT/映射/入队/启动并统一清理，使用 bytesperline，g_run 原子化。残留：31–46 短帧仅提前返回，67–68 仍置 g_have=true，首帧可能含未填充 malloc 区域；硬 DQBUF 错误退出 reader（60）仍显示工作中，重排队错误（72）未处理。原有严重映射越界路径已处理，但不能宣称缓冲/错误路径完整修复；未连接 Linux 摄像头。 |
| AUD2-020 | 部分修复 | 代码核查 | host_camera_mac.m:44–100、115–131 采用串行 ctl 队列与 gen，close 使迟到授权无效并排空帧回调，原生命周期缺口已处理。g_status 仍在 ctl 写、133 的 getter 在 GUI 线程直接读，未建立同一队列/锁/原子关系（gui_main.cpp:816）；原报告的状态数据竞争仍在。未申请摄像头权限或打开真实设备。 |
| AUD2-021 | 已修复 | 复现、代码核查 | gui_main.cpp:109–143、1186–1188：pipe 读端和备份 fd 设 CLOEXEC，exec 前恢复 stdout/stderr。SDL dummy 的 4 次启动（3 次重启）lsof 每次均 pipe_fds=4，无累计泄漏；观察完成后仅 SIGTERM 本探针子进程，见 experiments/gui-restart-results.json。 |
| AUD2-022 | 已修复 | 复现、代码核查 | CMakeLists.txt:9–11 在 FetchContent 前启用 OBJC；完全按 TASK 本地依赖路径，SDL2_DIR-NOTFOUND 的源码 SDL 分支配置/构建 exit=0，38/38 测试通过（含 GUI）。新 macOS CI 已配置，但安装系统 SDL2，未持续覆盖源码 SDL 分支；本轮未联网查看远端 CI 运行。 |
| AUD2-023 | 部分修复 | 文档核查、代码核查 | hardware-parity.md 已分列接线/行为/故障/实机，22 项=9 一致+13 部分，标明 RTC/SRDY/电流等假设；R-034 不再写网络音频未做。但 R-034:88 仍概括 AUD2 已修复，与本轮残留不符；对照表第 3/7 项的故障一致判断须按显示断电和初始化失败旁路下调。第 19 项称睡后重新挂载，hal_power_sim.c:485 只是 sd_remount_us 延时；不能当作实际 SD 状态验证。沿用本编号，不另设文档问题。 |
<!-- verification:end -->

## 新问题

<!-- findings:start -->
### AUD3-001｜高｜定时器删除等待期间允许回调重新启动，删除后事件访问已释放对象

**范围：**B 平台接口 / D 模拟器内核。**证据：**【复现、代码核查】。

**位置：**`ports/sim/osal_sim.c:733`；`ports/sim/osal_sim.c:759`；`ports/sim/osal_sim.c:650`；`platform/include/ink/osal.h:165`。

**核实与复现：**先按 experiments/README.md 的 ASan 配置构建，再运行 python3 docs/audits/2026-11-03-codex-r2/experiments/run.py --asan r2_timer_rearm_delete。回调只做有界 CPU 工作及允许的 API，不阻塞等待。输出：timer delete begins → timer callback rearmed its timer → timer delete returned calls=1 → AddressSanitizer: heap-use-after-free，栈顶 timer_fire，子进程 exit=-6。完整日志在 build-audit/r2-probes/r2_timer_rearm_delete-asan/run.log，摘要在 experiments/results.json。

**影响：**新的静默删除保证仍不成立。回调运行中，另一任务 delete 先 stop 再等待；回调按允许的契约 start 自身，delete 随后释放对象却留下新事件。下一次到期触发宿主堆 use-after-free，可终止模拟器。本轮普通探针退出 0，ASan 才报告错误，说明现有绿色测试不足。

**建议：**为 timer 加入受同一锁保护的 deleting 状态，拒绝删除期间重新 start，并在回调退出后再次取消事件、移除 pending，最后释放；补覆盖回调自重启与外部删除的交错测试，明确自身删除和重启的契约。

### AUD3-002｜高｜唤醒恢复的 I2C 错误被忽略，故障消失后按键方向仍未恢复

**范围：**B 平台接口 / C 板级睡眠。**证据：**【复现、代码核查】。

**位置：**`ports/sim/hal_power_sim.c:361`；`drivers/board/metalio_board.c:91`；`ports/sim/hal_input_sim.c:575`。

**核实与复现：**运行 experiments/run.py r2_wake_read_fault r2_wake_control。故障组在睡眠后 400 ms 按 POWER、401–650 ms 注入 io-expander 错误、450 ms 松键；返回后 config=00,80 keys=1；总线恢复后再按 VOL，仍 keys=1。无故障对照为 config=c0,e3 keys=2，VOL 后 keys=4。两组 exit=0；摘要见 experiments/results.json。

**影响：**在真实按键唤醒时短暂注入总线错误，hal_power_sleep 仍报告 POWER 唤醒并恢复输入任务，但 TCA9555 保持睡眠配置 00,80。总线恢复后音量键仍被配置为输出而完全无事件；POWER 只上报按下，释放丢失。正常运行路径没有重试恢复，单次暂态故障可留下持续输入失效；恢复供电步骤也只计数不处理错误。

**建议：**将板级恢复视为可失败的事务：恢复并确认方向与电源后才恢复采样和对外宣告完成；为暂态错误增加有界重试及明确的故障状态/后续恢复机制。读失败时保留需重新采样状态，并配对处理已合成的唤醒按键。

### AUD3-003｜严重｜波形执行中失去屏电源仍绘制完整目标图并回调成功

**范围：**C 显示驱动 / D 器件模型。**证据：**【复现、代码核查】。

**位置：**`ports/sim/hal_display_sim.c:682`；`ports/sim/hal_display_sim.c:690`；`ports/sim/hal_display_sim.c:726`；`ports/sim/dev_epd_power.c:75`。

**核实与复现：**运行 experiments/run.py r2_display_cut，再运行 experiments/evidence.py。探针等待 sim_display_playing 后用 metalio_board_set(0,7,0) 拉低屏 PMIC 控制脚；输出 cut rc=0 ready=0，callback_ok=1 ready=0；前后 PNG 有 4096 个像素变化。日志及图片只在 build-audit/r2-probes/r2_display_cut，摘要见 experiments/driver-evidence.json。

**影响：**AUD2-017 修复了启动阶段重试耗尽，但显示就绪只在波形前检查。波形开始后经板级驱动拉低 PMIC 控制脚，模型已确认 panel_ready=0，执行器仍完成 apply_area 并回调 ok=1。4096 个像素被改变，形成供电丢失时仍可成功刷屏的假阳性；会误导上层故障恢复与显示验收。

**建议：**将电源/复位代次与正在执行的波形关联；执行中失电必须终止或标记失败，不能继续提交完整目标图。供电模型通知显示器状态失效；补启动失败、波形中断电及复电后的重新初始化覆盖。

### AUD3-004｜高｜进入睡眠期间启动音频未被互斥，复电后启动成功却持续无样本

**范围：**B 音频接口 / C 板级睡眠。**证据：**【复现、代码核查】。

**位置：**`ports/sim/hal_audio_sim.c:299`；`ports/sim/hal_audio_sim.c:347`；`ports/sim/hal_audio_sim.c:501`；`ports/sim/hal_power_sim.c:318`；`ports/sim/hal_power_sim.c:427`。

**核实与复现：**运行 experiments/run.py r2_audio_sleep_entry r2_audio_sleep_late r2_audio_sleep_race。entry 组先调用 sleep（1.741 s，audio_busy=0），在其进入延时中才开始采集（1.770 s）；sleep 返回 TIMER，3.630 s capture_start 返回 0 但 mode=0 clock=0，4.980 s samples=0 busy=1。late 组同样失败；10 ms 较早交错对照能恢复 mode=1 并取得 18000 样本。均 exit=0，见 experiments/results.json。

**影响：**睡眠调用时音频空闲，进入序列让出执行权后另一个任务开始采集。启动先等待 BH1098G，却直到等待完成才置 cap_on；睡眠的第二次忙检查仍认为空闲，切断 P14。启动沿用旧代次/就绪时间，模式命令在芯片再次就绪前丢失，仍返回 0 并保留 busy=1，之后没有采样和自动重试。本次是 HAL 交错复现，没有声称在产品 UI 或真实硬件上观察到同一故障。

**建议：**从音频启动第一步直到停止持有共享电源/睡眠准入租约，睡眠转换与新工作开始必须互斥，不能仅对 cap_on/out_on 做事后检查。模式设置后核对 P14 代次是否改变；发生切电时重走完整启动或返回失败并清理资源。添加睡眠进入期间开始录音/播放的有界交错回归。
<!-- findings:end -->

## 执行记录与未查范围

- 已读取本轮任务、项目约定、上轮报告与处理表，固定检出为上述提交。
- 正在按 TASK 的本地依赖路径构建 `build-audit/`，配置和构建日志仅留在构建目录。
- 已有 `experiments/` 探针保留；其旧运行不作为本轮证据，需核对并重新执行。
- 未做实机、Linux 摄像头实际采集、真实服务器、网络服务调用、安全测试；原因是本轮范围及宿主环境限制。

结构化记录：[verification.json](verification.json)、[findings.json](findings.json)。
