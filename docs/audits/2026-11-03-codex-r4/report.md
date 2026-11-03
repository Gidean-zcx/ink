# 第四轮软件质量复审（2026-11-03）

审计对象：`89b112620730902f0fa1594e83ffb146b08749be`（当前检出，detached HEAD）。

本报告随核查分组增量写入，最终结论于 2026-11-03 完成。仅交付审计材料，未修改产品、原测试或历史文档。

## 一页结论

**仍有严重和高级别问题，尚未达到“直到没有太大问题”的标准。** 本轮核实 8 项：5 项已修复、3 项部分修复；记录新问题 4 项：严重 1、高 2、中 1。

【复现、代码核查】最关键的剩余问题是 **P05 持续断电时两条显示管线仍可回调成功**；PQ-BN 复电后重交相同目标还可能保持空白（AUD5-001）。音频 HAL 已正确返回启动失败，但播放服务会继续消费并丢掉整段已接收数据（AUD5-002）；AI 会把失败的录音会话变成固定问题并显示回答（AUD5-003）。这些都有确定性模拟器反例，不能由现有套件通过推定已解决。

| 范围 | 总体判断与证据边界 |
| --- | --- |
| 显示与供电 | 【复现、代码核查】瞬态 PWRUP/VCOM 丢电、每请求一次回调的原问题修复有效；持续 P05 断电后的重试和复电重画仍有严重缺口。初始化输出写未计入 changed，作为覆盖限制记录。 |
| 输入、睡眠恢复、GPIO2 | 【复现、代码核查】持续触摸时后台恢复、1s 封顶退避、GPIO2 低电平重查通过针对性验证；正常转动无额外重查，三次睡眠后按键、显示、音频可用。 |
| 音频 | 【复现、代码核查】无供电启动会失败并释放资源，晚初始化首次录音可用；上层播放与 AI 各有一项高等级错误处理缺口。 |
| Linux 摄像头 | 【代码核查】空帧、错误帧和队列硬错误处理已补齐；本机 macOS，未编译或运行 Linux 后端。 |
| 测试与正常路径 | 【复现】普通 Werror 40/40、ASan/UBSan 39/39 通过；54 组补充/历史观测覆盖本轮反例。正常两档最终图像相同、无新增失败或告警，PQ-BN 波形增加约 0.12～0.16ms。原 PQ-BN 整板命令因 65/66 数量预算不匹配 exit=1（AUD5-004）。 |
| 文档与硬件依据 | 【文档核查、代码核查】R-034、对照表的部分边界说明已修正；审计处理表对显示、音频和两档验证的完成表述仍超出证据。P05 供电结论限于现有模型契约，不能外推实机。 |
| 其他系统范围 | 【复现】现有内核、存储、蜂窝等纳入完整回归；本轮未重新独立审计前三轮的全部问题，也未做真实并行、长期运行或实机验证。 |

结构化结果见 [verification.json](verification.json) 和 [findings.json](findings.json)。复现命令及证据索引见 [实验说明](experiments/README.md)；下文给出逐项证据、影响和建议。

## 修复核实表

| 编号 | 结论 | 证据 | 说明 |
| --- | --- | --- | --- |
| AUD4-001 | 部分修复 | 【复现、代码核查】 | 第三轮 r3_display_transient/vcom 在 EPDiy、PQ-BN 均由误报成功改为 ok=0，重新上电后请求成功；multi 各 ID 回调一次。P05 变化能使首次刷新失败，但 P05 保持低时第二次刷新两档均 ok=1、panel_ready=0（AUD5-001）；PQ-BN复电后重交同图仍空白。changed 记录不覆盖初始化写（补充API级探针）；正常外设并行动作、睡眠后刷新未见误判。 |
| AUD4-002 | 已修复 | 【复现、代码核查】 | hal_input_sim.c:788–798 的触摸循环服务 restore_due。原 r3_wake_touch 恢复后 cfg=c0,e3、键共4个；r4_touch_restore 延长故障至1.4s，确认睡眠先返回 cfg=00,80，再在手指仍按住时补 POWER 松开、恢复输入方向与音量按/松，共4个，无需抬手。 |
| AUD4-003 | 部分修复 | 【复现、代码核查】 | 原 HAL 假启动已修复：r3_wake_audio/r4_audio_restore/r4_touch_restore 在 P14 未恢复时录音、播放均 -1、busy=0；恢复后手动重试 cap=play=0、write=2400、samples=9600，停止后能睡眠。hal_audio_sim.c:302–330、365–435 失败分支释放 DMA/租约。但上层仍静默丢播放帧（AUD5-002），AI 将录音失败变成固定问题（AUD5-003），端到端处理未闭环。 |
| AUD4-004 | 已修复 | 【复现、代码核查】 | hal_audio_sim.c:534–538 按 sim_board_present 创建 bt_lock/任务，不再依赖开机 board_ready；r3_boot_audio：ready=0→1、键2个，第一次录音 rc=0、mode=clock=1、40080样本；停止后可睡眠。selftest-expander-dead 在普通及ASan套件通过。 |
| AUD2-019 | 已修复 | 【代码核查】 | simulator/desktop/gui/host_camera_linux.c:32、65–85：bytesused 直接作有效载荷长度，0/短帧不发布；V4L2_BUF_FLAG_ERROR 被过滤；无效帧仍归还 QBUF，DQBUF/QBUF 硬错误清 g_have 并标记停止。既有映射容量与行跨度检查保留。本机为 macOS，本轮未编译/运行 Linux 后端或真实摄像头，不将此前开发方 Linux CI 记录当成本轮运行证据。 |
| AUD2-023 | 部分修复 | 【文档核查、代码核查、复现】 | R-034（requirements.md:88）与对照表已统一为8项一致/14项部分；INT1/INT2未建模、SD仅延时、PQ-BN仅完成时查PG均已明确。但 audits/README.md:23、25 所称屏供电修复完整、音频服务已处理失败被AUD5-001～003反例推翻；“每次写输出”也不覆盖tca9555_init。原预算的PQ-BN整板命令实际exit=1（AUD5-004），不能表述为两档完整验证均通过。 |
| AUD3-002 | 已修复 | 【复现、代码核查】 | hal_input_sim.c:658–690：50ms倍增后以1s封顶，输入恢复在 sample_lock 内，触摸循环同样重试。长故障 r4_touch_restore/audio_restore 由 cfg=00,80 恢复 c0,e3、POWER/VOL共4事件；正常三次睡眠后均能刷新与录音。此行核实唤醒恢复及退避；音频上层问题另列 AUD5-002/003。 |
| N3-3 | 已修复 | 【复现、代码核查】 | hal_input_sim.c:177–197、773–796：GPIO2保持低时单个重查定时器，按去抖时间倍增、1s封顶，高电平后停止并重置。r4_gpio2_accel：加速度计故障5s触发10次重查/10次超时，恢复后 GPIO2=0（不低）、音量2事件；无故障转动 r4_gpio2_normal 重查0次，未见正常空闲忙轮询。 |

全部 8 项已核实。

## 新问题

### AUD5-001（严重）：P05 持续断电后重新上电会清除变化记录，两条刷新管线再次假成功

- 证据：【复现、代码核查】python3 docs/audits/2026-11-03-codex-r4/experiments/run.py r3_display_p05 r4_p05_preoff；加 --pqbn 再跑。原P05探针：EPDiy 2.985s id=2 ok=1 ready=0；PQ-BN 2.166s id=2 ok=1 ready=0。补充探针开刷前即断P05，连续三次都ok=1；EPDiy无供电仍改变16384像素；PQ-BN复电后id=4仍ok=1、latency_us=0，同图重交后截图仍全白（mean_gray=255、改变像素0）。常规与选定ASan进程exit=0。见image-evidence.json与results.json。
- 位置：`boards/metalio_plus/metalio_epd_power.c:38`；`boards/metalio_plus/metalio_epd_power.c:85`；`boards/metalio_plus/metalio_epd_power.c:112`；`ports/sim/hal/hal_display_sim.c:539`；`ports/sim/hal/hal_display_sim.c:759`；`ports/sim/hal/hal_display_sim.c:1193`；`simulator/models/dev_epd_power.c:140`。
- 影响：P05 低时模型已经认定面板不响应，但 power_on_once 仅开 TPS 三个控制脚，未恢复/验证 P05，最后却清掉 P05 changed；epd_power_good 只查 P07 与 PG。下一次请求因此被当成成功。EPDiy 可提交没有供电的目标图；PQ-BN 在无供电时不推进光学模型却仍回调成功，内部进度却推进到目标，复电后重交同图仍保持全白。关键供电/显示结论仍为假阳性，按首轮标准定严重；不是实机损坏或画质判断。
- 建议：把当前 P05 有效电平纳入上电成功和完成校验；明确由谁恢复共享屏电源，未恢复就返回失败且不能清掉失效证据。增加持续掉电后的多次请求与恢复后重画断言。

### AUD5-002（高）：播放启动失败仍逐帧消费并丢弃整段音频，复电后无法续播且上层无失败通知

- 证据：【复现、代码核查】python3 docs/audits/2026-11-03-codex-r4/experiments/run.py r4_service_failed r4_service_normal。两组stream_open返回1，8次push均rc=0并finish；故障组先断P14，1s后复电，再等2.5s：audio.start_without_chip=9（录音1+播放8）、audio.speaker.played_ms=0、warnings=[]、mode=0。正常组played_ms=480、mic.captured_ms=120，exit均0；指标见构建目录各metrics.json及摘要。
- 位置：`system/services/audio/audio_service.c:220`；`system/services/audio/audio_service.c:245`；`system/services/audio/audio_service.c:280`；`system/services/audio/audio_service.c:297`；`system/include/ink/sys/audio.h:49`。
- 影响：AUD4-003把HAL改为返回失败是正确的，但service_playback先出队解码，open_output失败后把暂存帧直接清零；每个服务周期继续消费下一帧，最后按正常完成清stream。用户已成功提交的TTS/听书数据全部丢失，恢复供电后也不再播，上层没有错误结果。正常对照480ms声音，故障组0ms且无告警。
- 建议：明确可重试和不可恢复的播放失败：保留待播帧并做有界退避/恢复通知，或终止流并向调用方明确报告失败；在输出尚未成功打开时不要消耗整段音频。增加启动失败→恢复→完整播放或明确失败的服务级断言。

### AUD5-003（高）：麦克风启动失败后仍显示正在听，松键会冒充用户发出固定问题

- 证据：【复现、代码核查】python3 docs/audits/2026-11-03-codex-r4/experiments/run.py r4_ai_failed。运行真实product_main/UI/AI/中枢链路，P14关闭时按AI键700ms，随后复电。exit=0；ai.mic_unavailable=1、audio.mic.captured_ms=0、hub.requests.ai.ask=1、ai.answers=1、warnings=[]。固定请求内容由ai_service.c:375和460–466确认；没有使用真实麦克风或外部网络。
- 位置：`system/services/ai/ai_service.c:425`；`system/services/ai/ai_service.c:438`；`system/services/ai/ai_service.c:444`；`system/services/ai/ai_service.c:455`；`system/services/ai/ai_service.c:460`；`system/services/ai/ai_service.c:375`。
- 影响：有麦克风的Plus在暂时无供电、OOM或其他录音启动失败时，只累加mic_unavailable，未显示失败、未记录本次语音未开始。松键落入“无麦克风”的演示分支，显示“你问：这一页讲了什么？”并发送ai.ask，用户真正说的话全丢却得到看似正常的回答。此次HAL失败路径变得可达后，原有上层错误处理缺口直接暴露。
- 建议：为本次按键会话保存录音启动失败状态并提示重试；松键不得把失败语音转换成固定内容。演示问题应由明确的演示入口触发。补完整应用层故障场景，断言零采音时不发送冒充本次输入的ai.ask。

### AUD5-004（中）：整板预算要求66项检查，PQ-BN按设计只执行65项，任务要求的原命令必报失败

- 证据：【复现、代码核查、文档核查】INK_SELFTEST_BOARD=1 build-audit/sim-selftest --profile simulator/profiles/metalio-plus-pqbn.json --budget tests/simulator/board.budget.json --out build-audit/r4-board-pqbn --quiet --end 90s --no-random-outages。exit=1；BUDGET FAIL test.pass: 65.000 < min 66.000；test.fail未出现（0），power.off=1。同一默认EPDiy整板CTest test.pass=66、budget_violations=0。
- 位置：`tests/simulator/board.budget.json:4`；`tests/simulator/selftest.c:985`；`CMakeLists.txt:322`；`docs/audits/README.md:23`。
- 影响：PQ-BN按设计跳过仅适用EPDiy的“断电后屏不变”断言，因此所有适用断言通过也只能得到65项。共用min=66产生固定假失败；当前CTest仅注册默认EPDiy整板档，40/40不会发现该预算问题。开发方“两档都过”的记录与任务给定原命令不符。此项是验证配置缺陷，非面板功能故障。
- 建议：为两种档案分别声明应执行的检查数量，或使用不依赖管线分支的完成标志；将PQ-BN整板测试正式注册到CTest并保持原始退出码可见。

## 分组核查记录

### 0. 范围与基线

- 【代码核查】`git rev-parse HEAD`：`89b112620730902f0fa1594e83ffb146b08749be`。
- 初始 `git status --short` 仅有 `?? docs/audits/2026-11-03-codex-r4/`；产品代码无本地修改。
- 本轮独立完成，不调用子代理或外部模型；不联网、不接触设备，不读取 `ink-hw/upstream/` 之外的私有实机数据。
- 仅审计目录写交付文件；构建与运行临时产物使用任务允许的 `build-audit/`、`build-audit-asan/`。

### 1. Linux 摄像头有效帧过滤（已核实）

- 【代码核查】`git diff 95587e5 HEAD -- simulator/desktop/gui/host_camera_linux.c` 与完整后端核查：空帧和错误帧的剩余两处缺口均封闭；正常 YUYV 整帧仍走原转换路径。
- 不运行宿主摄像头，不申请媒体权限。

### 2. 第三轮显示反例回放与供电边界（已核实）

- 【复现】`python3 docs/audits/2026-11-03-codex-r4/experiments/run.py r3_display_transient r3_display_vcom r3_display_multi r3_display_p05`，再加 `--pqbn` 执行相同四项。全部进程 exit=0，**退出码不代表断言通过**。
- 瞬态 PWRUP/VCOM 和多请求失败回调已符合修复目标；持续 P05 断电后的重试仍报成功，见 AUD5-001。输出摘录在 `experiments/results.json`，完整日志仅在构建目录。
- 【复现】普通 Werror 配置、构建和 CTest exit=0，40/40（10.57 s）；测试绿色并未覆盖持续 P05 断电后的重试。

### 3. 输入恢复、GPIO2 与音频启动（已核实）

- 【复现】原第三轮 `r3_wake_touch`、`r3_wake_audio`、`r3_boot_audio`、`r3_audio_oom`、`r3_play_sleep_entry` 已重跑；补充 `r4_touch_restore`、`r4_audio_restore` 刻意让故障长于同步恢复窗口，触摸仍按住时恢复成功，音频先失败、复电后重试成功，失败和停止均未漏睡眠租约。
- 【复现】`r4_gpio2_accel` 与 `r4_gpio2_normal` 对照：5s 加速度计读故障期间10次重查，恢复后键2个；正常转动后无重查。20s 整次运行 I2C busy 分别630.17ms与116.37ms，额外513.80ms主要为10次50ms超时，并非1s持续占满总线。
- 【代码核查】`sample_lock` 串行化采样、睡眠屏蔽与恢复；重查 ISR 只置标志、唤醒任务，不在 ISR 上读总线。`sampling_off` 时不继续安排 GPIO2 重查，恢复后从当前键电平重建状态。
- 【复现】正常与并行外设探针两种显示档案均9次刷新全部成功，含3次睡眠后的首次刷新；功放播放960样本、摄像头返回614400字节，未产生供电掉落误判或告警。音频普通/复电后各9600样本，write=2400，结束busy=0。前后版本耗时对照见第5组。
- 【复现】ASan+UBSan Werror配置、构建与CTest exit=0，39/39（10.73s）；GUI关闭。未运行LeakSanitizer或ThreadSanitizer。

### 4. 文档、上游依据与整板测试边界（已核实）

- 【文档核查】需求R-034没有继续宣称所有审计项已修复；对照表第3、7、15项已分离模型行为与端口恢复，明确故障锁存尚未建模。这些修订有效；本轮新增反例仍须回写开发方处理表后才能形成一致的验收表述。
- 【代码核查】官方检出 `690a023c66862af103a3187464373ee7c5efcd14`，2026-11-03读取：`main/boards/metalio-e-ink4-plus/config.h:65–100` 定义P05为摄像头/屏幕电源、P14为共享音频/4G电源；`display/epd_board_metalio_eink4_plus.c:292–311` 为P05→P14关闭、P14→P05恢复；`main/boards/common/power_policy/` 的浅睡与按键恢复代码用于顺序对照。未读取私有实机数据。P05具体供哪路电仍是既有模型假设，本轮发现是在**该模型自己的电源契约内**不一致，不以此证明真实电路。
- 【复现】任务给定PQ-BN整板命令跑完55.681s虚拟时间，65项CHECK成功、0项CHECK失败，但`test.pass min=66`导致exit=1，详见AUD5-004。EPDiy原CTest为66项成功。不能把这一预算失败写成硬件模型功能失败，也不能省略失败出口宣称测试通过。
- 【复现/代码核查】`r4_tca_init_change`：直接用驱动初始化API拉低再恢复VCOM，`ready=0→1`，`touched=0`、`kept=1`。`tca9555.c:10–22`的初始化写和`45–48`的方向写没有记changed，故“所有写路径已覆盖”不成立。当前产线调用初始化仅在开机/延迟开机；未复现这条API覆盖缺口导致另一条可达产品故障，不单独升为严重/高。睡眠mask/restore的输出写均走set，正常三次睡眠后刷新通过。

### 5. 修复前后对照与补充恢复证据（已核实）

- 【复现】将`95587e5a5f641fa4fd44c313f366e69f244faeb1`用`git archive`导出到`build-audit/before-src/`，离线Werror、GUI关闭构建到`build-audit/before/`，未切换当前检出。`run.py --before`链接这份旧库；运行器源码`sim_main.c`在两提交之间无变更。13组旧版本对照包括第三轮四条AUD4原反例、正常显示/音频、故障音频上层。
- 【复现】`r4_normal`与`r4_normal_activity`：两档各9次成功刷新，包含连续翻色、3次睡眠后的首刷、正常录音播放；两种版本所有最终PNG字节相同、均无告警。EPDiy探针逐条观测相同、波形时长444012µs；PQ-BN波形增加116～161µs，约0.08%～0.11%，应用等待时间和音频样本数不变。不是声称全部指标逐字节相同：CPU记账、栈估计及若干I2C等待指标有小差异，完整差异在`experiments/comparison.json`。
- 【复现】原8% PMIC总线超时场景在旧/当前版本均按各自预算通过；`display.power_lost_during_waveform`从4降到0（缺省指标按0），支持完成检查重试避免瞬时总线错误被判掉电。不能由此证明所有随机种子和持续总线故障。
- 【复现】`r4_held_supply`两档均4次刷新成功、`display.power_recycled=2`、无告警：保持上电期间，马达/功放位变化不触发误回收；P05和VCOM分别短暂变化后，下一次请求先重新上电。
- 【复现】原第三轮AUD4-002/003/004在旧库上仍分别表现为触摸抬起才恢复、录音rc=0却0样本、晚开机后录音rc=0却0样本；当前库对应原缺陷已消失。音频上层旧库会卡在假启动，当前库转为丢帧或固定提问，证明新的服务层缺口不能用HAL返回正确就视为闭环。
- 【复现】7组ASan/UBSan补充运行与常规观测一致，没有sanitizer报错；包括P05反例、延迟初始化、长故障触摸/音频恢复、服务丢音频与完整AI失败场景。总计54组探针配置，全部进程exit=0，包含34组当前常规、7组当前sanitizer、13组历史对照。**它们是观测实验，不是54个产品通过的断言测试。**

## 命令、覆盖与限制

审计对象始终为 `89b112620730902f0fa1594e83ffb146b08749be`；历史对照为 `95587e5a5f641fa4fd44c313f366e69f244faeb1`，仅导出到构建目录。环境为 macOS 15.7.9 / arm64、Apple Clang 17.0.0、CMake 3.28.3、指定虚拟环境 Python 3.11.15 / CTest 4.4.4。

【复现】已执行命令及结果如下，完整离线参数和场景清单见 [实验说明](experiments/README.md)：

| 命令/组 | 结果 |
| --- | --- |
| `git rev-parse HEAD`、`git status --short`、`git branch --show-current`、`git diff --exit-code` | 对象提交如上；detached HEAD；原有跟踪文件无改动，新增交付均在本轮目录。 |
| `cmake -S . -B build-audit -DINK_WERROR=ON …`；`cmake --build build-audit -j 8`；指定 `ctest --test-dir build-audit -j 8 --timeout 300 --output-on-failure` | 配置、构建、测试均 exit=0；40/40。 |
| 同配置使用 `build-audit-asan/`，增加 `-DINK_SANITIZE=ON -DINK_GUI=OFF`，构建并运行 CTest | 均 exit=0；39/39。 |
| `INK_SELFTEST_BOARD=1 build-audit/sim-selftest --profile simulator/profiles/metalio-plus-pqbn.json --budget tests/simulator/board.budget.json --out build-audit/r4-board-pqbn --quiet --end 90s --no-random-outages` | exit=1；65个适用检查成功，但预算要求66个。 |
| `experiments/run.py` 的54组配置 | 34组当前普通、7组当前ASan/UBSan、13组历史对照；进程均exit=0，反例的功能失败单独判读。 |
| 历史库离线构建及 `ctest --test-dir build-audit/before -R '^scenario-reader_basic-panel-power-faults$' --output-on-failure` | 均exit=0；完成原故障及正常路径前后比较。 |
| `experiments/summarize.py`、`experiments/validate.py` | 提取证据并核对审计包；检查结果见下列机器可读记录，不能解释为产品验收通过。 |

关键输出摘录在 [results.json](experiments/results.json)，指标及告警在 [metrics-evidence.json](experiments/metrics-evidence.json)，前后差异在 [comparison.json](experiments/comparison.json)，P05图像统计在 [image-evidence.json](experiments/image-evidence.json)，整板结果在 [board-results.json](experiments/board-results.json)，交付一致性检查在 [artifact-validation.json](experiments/artifact-validation.json)。完整构建日志、运行日志、PNG只保留在构建目录，未放入审计包。

Linux 摄像头仅代码核查；本轮没有运行实际固件、真实媒体采集、硬件功耗或光学测量，没有做ThreadSanitizer、LeakSanitizer、真实SMP或长期稳定性验证。未重跑历史版本的完整套件或GUI，也不将此前开发方CI记录当作本轮运行证据。正常路径耗时均是模拟器虚拟时间；模型CPU/功耗估算和栈估计不是实机测量。

按本次只读、离线任务范围，不同步交互决策板，不查询远端Issue/PR，不读取私有实机资料；未调用子代理或外部模型，未提交、推送、建分支，未修改产品代码、原测试、档案或历史文档。
