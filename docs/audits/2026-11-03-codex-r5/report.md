# 第五轮软件质量复审（2026-11-03）

审计对象：`0dbf9aaa97a354c6d62eeaef2b4dfbfd1e8237f3`（当前检出，detached HEAD）。
执行方式：主代理独立只读审计；不调用子代理或外部模型，不接触设备或私有实机数据，不修改产品源码、既有测试和文档，不提交、不推送。

## 一页结论

**仍有高问题，尚未达到“没有太大问题”。** 本轮确认新问题 **3项：严重0、高2、中1**。这里的“严重0”仅指本轮范围内没有新确认的严重问题，不是对整个产品或实机的无缺陷保证。六项修复核实为 **已修复3项、部分修复3项**。

- **高 · AUD6-001：播放中途断P14，复电后音频仍停滞。** 已接受的3s声音只播120ms，复电10s后不续播、不报失败，继续持有唤醒/写入hold。代码没有这条状态的自动恢复或超时出口；手动取消才释放。适用于默认音频服务路径，是前轮遗漏的问题。
- **高 · AUD6-002：PQ-BN恢复后多余全屏清理。** 故障后的全屏GC16已恢复成功，标志却未清除；下一次128×128更新从正常 **0.329s** 变为 **10.119s**，再次闪整屏。是本轮显示修复引入的回归，限可选PQ-BN档案。
- **中 · AUD6-003：AI语音失败提示被完成事件覆盖。** 16.842s已明确提示声音失败，18.860s又改成“回答完成”，最终界面丢失声音未播的解释。文字回答不受影响，故没有定高。

范围判断：P05一直断开时的“假成功”、音频启动失败时丢掉已收帧、麦克风失败时伪造提问均已在原反例对照中修好；显示恢复及AI语音反馈仍有上述缺口。PQ-BN整板自检现已注册并使用独立预算；GUI实时速度测试本机通过。文档虽已纠正前轮误述，仍有两处完成表述超出实现。连续丢电、重刷期间排队回调、失败取消/切流等补充路径已核实，不能覆盖未测试的所有组合。

【复现】普通Werror构建 **42/42** CTest通过，ASan+UBSan **41/41** 通过，9个补充sanitizer组合无sanitizer错误。测试通过并未覆盖上述功能缺陷。正常刷新、录放音和AI对照没有卡住或新增告警；正常待机解锁可见p50增加约130ms、显示预热失败多2次，最终刷新成功，详见第5组。所有时延均为模拟器虚拟时间。

证据已随各组核查逐步写入；本结论在分组验证完成后补齐。产品源码与既有文件未修改，未提交、未推送。下一步应先处理两个高问题，再回放本轮反例与正常对照；本次只审计，不实施修复。

## 修复核实表

| 编号 | 结论 | 证据 | 说明 |
| --- | --- | --- | --- |
| AUD5-001 | 部分修复 | 【复现、代码核查】 | 原r3_display_p05/r4_p05_preoff两档均不再假成功；恢复后同图可见。连续两次丢电和自动清理期间重叠更新均每请求一次回调、最终图像正确；P05断开时显示服务busy归0，不无限重试。初始化写也计入changed。但PQ-BN用户GC16清理绕过g_redraw收尾，系统强制全屏恢复后的下一次小更新重复清全屏，0.329s→10.119s（AUD6-002）；局部GC16还推迟损坏区补画。 |
| AUD5-002 | 部分修复 | 【复现、代码核查】 | 服务层原丢帧已修：原故障流480ms全部播放；永久启动失败约8.66s后仅一次回调（audio任务），可重入API且释放hold；新流重置等待期。实际AI恢复组与正常组均播放20160ms；永久故障ai.speech_failed=1。但失败UI被稍后的回答完成覆盖（AUD6-003），端到端提示尚不完整。另有中途失去时钟永久停滞的新高问题AUD6-001，区别于原启动失败。 |
| AUD5-003 | 已修复 | 【复现、代码核查】 | 原 r4_ai_failed：mic_unavailable=1、采音0ms、ai.ask/ai.listen/answers均0，不再伪造提问；ai_service.c:463–489保存本次mic_failed并在松键返回。新增正式场景ai_mic_unavailable：失败后再按可正常录音780ms、ai.listen=1、answers=1，预算通过；示例分支仅无麦克风设备可达。 |
| AUD5-004 | 已修复 | 【复现、代码核查】 | CMakeLists.txt:327–331已注册selftest-board-wiring-pqbn，独立预算min=69。本轮完整Werror CTest 42/42通过；EPDiy整板70项、PQ-BN69项，test.fail=0、budget_violations=0、power.off=1。 |
| AUD2-023 | 部分修复 | 【文档核查、代码核查、复现】 | 此前“两档都过”“音频原已处理”“所有输出写”已改；R-034及硬件表8一致/14部分统一，初始化输出写现被记录，独立PQ-BN预算通过。但audits/README.md:24的“不清记录”与metalio_epd_power.c:49–52先清P05 changed再读取不符；“任何一次丢电后下一次先重刷”未覆盖用户GC16组合（AUD6-002）。需按当前实现及本轮证据收窄完成表述。 |
| N4-1 | 已修复 | 【复现、代码核查】 | tests/simulator/gui_smoke.cmake:10–12使用--speed 1、16s退出；本机dummy后端gui-smoke通过（16.94s），touches=2、page_turns=1、warnings=[]并生成window.png。仅证明本轮运行，不能证明任意宿主长时间卡顿下绝不偶发。 |

## 分组核查记录

每完成一组，在本节追加命令、证据和结论，并同步 `verification.json`。

### 1. 基线构建、正式回归、麦克风失败及测试配置（已核实）

- 【复现】任务给定离线配置与 `cmake --build build-audit -j 8` 均 exit=0；指定 CTest `--test-dir build-audit -j 8 --timeout 300 --output-on-failure` exit=0，**42/42**（21.71s）。日志仅留构建目录。整板故障注入有预期告警，不能将全套表述为零告警。
- 【复现】`python3 docs/audits/2026-11-03-codex-r5/experiments/run.py r3_display_p05 r4_p05_preoff r4_service_failed r4_service_normal r4_ai_failed r4_tca_init_change` exit=0。复用第四轮探针，在本轮运行器记录结果，不运行会改写历史 `results.json` 的旧运行器。
- 【复现】原 AI 失败探针：`ai.mic_unavailable=1`、`audio.mic.captured_ms=0`，`hub.requests.ai.ask`、`hub.requests.ai.listen`、`ai.answers` 均未出现（按0）；`warnings=[]`。正式恢复场景采音780ms、一次listen、一次回答。故障期间不再把固定问题当成用户输入。
- 【代码核查】`ai_service.c:463–489` 以 `mic_failed` 区分失败会话；故障提示在UI任务更新，无麦克风设备才使用标明“示例”的问题。
- 【复现】整板自检 EPDiy `test.pass=70`、PQ-BN `test.pass=69`，两者 `budget_violations=0`、无CHECK失败；独立预算与当前分支执行数一致。
- 【复现】GUI dummy场景16.94s通过，`input.touches=2`、`reader.page_turns=1`、截图存在，`warnings=[]`。实时速度修复与其目的相符；未模拟任意长度宿主卡顿或核验远端CI。
- 【复现】ASan+UBSan配置（`INK_SANITIZE=ON, INK_GUI=OFF`）、构建及同参数CTest均exit=0，**41/41**（9.10s）。本轮不宣称执行TSan或LeakSanitizer。
- 本组确认原反例改善；复合状态与正常对照的最终判断见下面各组。

### 2. 音频等待、放弃和流切换（已核实）

- 【复现】原 `r4_service_failed` 从上一轮0ms变为480ms全部播完，与正常组480ms一致；失败前队列不再被耗尽，`audio.output_open_failures=4`，无警告。
- 【复现】`r5_service_timeout`：约8.66s后一次失败回调，`owner=audio`；回调内重入公开音频API完成，无锁死。写入从pending=1恢复0，失败后能睡眠（cause=1）。
- 【复现】`r5_service_replace`：旧流等待约6s后换新流，新流等待3s未被旧截止时间误杀；复电后300ms全部播放，失败通知0次，写入落盘并可睡眠。
- 【代码核查】`audio_service.c:299–310` 在释放AU锁后调用失败监听；AI监听 `ai_service.c:253–259` 读取AI状态后向UI队列投递。未发现这条新回调的AU→AI反向持锁链；不等于真实SMP无数据竞争证明。
- 【复现】另发现已开始播放后的恢复缺口AUD6-001，立即记录如下。

### 3. 屏幕P05、丢电重刷和系统显示服务（已核实）

- 【复现】第四轮反例回放，两档 `r3_display_p05` 的两次请求都失败；`r4_p05_preoff` 的前三次都失败、复电后第四次成功。旧库89b1126重放仍产生原假成功，见 `experiments/results.json`。当前 `r4_tca_init_change` 为 `touched=1/kept=0`，初始化输出写已纳入记录。
- 【复现】`r5_redraw_twice` 连续两次在驱动中断P05：两次失败、第三次成功；每个用户请求回调一次，最终黑块平均灰度EPDiy=0、PQ-BN=0.22，pending=0。自动清理期间提交覆盖原区域的新图与不重叠区域：各回调一次，重叠的两条等清理收尾后完成，最终白/黑区域正确。
- 【复现】`r5_display_service`：P05持续关闭5s，`busy=pending=0`、仅一次失败，无无限重试；复电并提交后，EPDiy和PQ-BN均恢复正确图像并归零。
- 【代码核查】`metalio_epd_power.c:49–52` 先重置本次P05变更观察窗口、再读P05；`104–110` 遇 `EPD_POWER_NO_SCREEN` 不重试；`124–130` 完成校验包含P05。与处理表字面的“不清记录”仍有区别，文档组将明确记录。
- 【复现】新恢复路径与系统原有强制GC16组合出现AUD6-002：已经恢复成功后仍重复全屏清理。正常无故障和已覆盖的自动清理路径不能证明这一组合正确。
- 探针说明：首版区域均值忘记将原生坐标转换到90°逻辑坐标，初次均值无效；已修正为与正式selftest一致的 `ink_native_to_logical`，重新运行所有引用均值的本轮探针。也延长系统恢复等待至20s，避免把PQ-BN约10s全屏清理误判为卡死；未修改产品。

### 4. AI语音失败提示、文档及上游依据（已核实）

- 【复现】运行真实UI/AI/Hub链路，以模拟Wi-Fi保持文字回答可达，录音结束后切P14：永久故障正确产生一次音频失败与一次AI失败提示；4s后恢复的对照，与正常组最终都播放20160ms、采音1050ms、回答1次、无警告。但永久故障的提示被稍后的回答完成覆盖（AUD6-003）。
- 【代码核查】AI提示由audio任务发消息交UI任务执行；`ui_apply` 对同一轮kind=4与kind=1分别直接写状态，没有保存语音失败。ASan观测标签由失败变为完成，普通构建最终PNG也目视确认是“回答完成”。
- 【文档核查】R-034（`requirements.md:88`）与硬件表均明确8项一致/14项部分；INT1/INT2未建模、P05具体供电路径是推断、SD恢复只有延时等边界没有被本轮改动扩大。旧处理表误述已修订；本轮处理表仍有“不清记录”与代码不符，以及GC16恢复组合的完成表述过宽。
- 【代码核查】官方允许检出的提交是 `690a023c66862af103a3187464373ee7c5efcd14`，2026-11-03读取 `main/boards/metalio-e-ink4-plus/config.h:65–100`、`display/epd_board_metalio_eink4_plus.c:154–190,292–311` 及 `main/boards/common/power_policy/power_hw.cc:378–485`。P05屏/摄像头电源、P14共享音频/4G电源、P05→P14切断/P14→P05恢复的顺序有官方代码依据；本轮没有核验实际电路、设备或私有实机材料。

### 5. 正常路径前后对照及sanitizer复核（已核实）

- 【复现】以 `git archive 89b1126` 在 `build-audit/before-src/` 隔离导出、离线编译旧库，未替换当前产品文件。旧版4个正式场景4/4通过（1.52s），与当前同依赖、同配置比较。具体命令见 [experiments/README.md](experiments/README.md)，完整指标差异见 [comparison.json](experiments/comparison.json)。
- 【复现】两档 `r4_normal` / `r4_normal_activity`，旧版与当前的探针输出逐行一致，最终PNG字节一致：每组9/9次刷新回调成功、三次睡眠唤醒、两次采音各9600样本、每次写入2400播放样本；并行组摄像头模型返回614400字节。显示波形延迟、探针端到端等待及音频耗时相同，无告警。底层I²C等待/上电指标存在细小变化，未将“探针输出相同”扩大为全部指标相同。

| 正式场景 | 旧版 → 当前 | 判断 |
| --- | --- | --- |
| `reader_basic-metalio-plus` | 翻页14次；动作到可见p50=709.340ms、p95=954.239ms均不变 | 【复现】最终PNG相同，未见新增失败/告警或用户时延退化 |
| `standby-metalio-plus` | 解锁到可见p50 **655.503→785.458ms**（+129.955ms，约19.8%）；p95=915.493ms不变；`display.power_failures` **0→2**，PMIC尝试10→12 | 【复现】正常唤醒有代价；三次解锁最终成功、PNG相同、无警告、预算通过；不能声称“没有新增失败或变慢” |
| `ai_ask-metalio-plus` | 两次回答/一次脚本预期失败；采音4500ms不变，播放27120→27135ms；首声p50 2454.884→2443.693ms | 【复现】无欠载、无新增告警，最终PNG相同；不据播放时长宣称音频逐样本相同 |
| `ai_ask-metalio-plus-pqbn` | 两次回答/一次脚本预期失败；采音4500ms、播放27495ms均不变；首声p50 2437.629→2435.868ms | 【复现】无欠载、无新增告警，最终PNG相同 |

- 【代码核查、推断】正常待机的两次新增失败发生在显示预热：`system/services/input/input_service.c:151–153` 先发 `hal_display_wake` 再解锁；`ports/sim/hal/hal_display_sim.c:1509–1515` 对wake请求直接上电，新P05检查能在板级恢复完成前拒绝预热。trace两次 `power-up failed` 在326710059µs/1050700048µs，随后实际刷新成功。该时序能解释失去提前上电的机会；未通过单项撤销实验将129.955ms全部精确归因于此。记录为正常路径代价和后续调度改进点，不另增一个高问题。
- 【复现】普通与ASan+UBSan完整CTest分别42/42、41/41；补充9个sanitizer组合均exit=0，无sanitizer错误，三个新问题在sanitizer下也可复现。累计53个探针组合（33当前普通、9当前sanitizer、11旧版对照）均完整结束；**这不是53项正确性测试通过**，其中包含有意展示错误状态的反例。
- 证据索引：[results.json](experiments/results.json) 保存命令与关键输出，[metrics-evidence.json](experiments/metrics-evidence.json) 保存相关指标，[suite-evidence.json](experiments/suite-evidence.json) 保存正式测试摘要。无故障对照的警告为0；整板故障注入的预期告警另保留，未一概抹除。

## 新问题

### AUD6-001（高）：播放途中P14断电又恢复后，输出永久停滞且无失败通知

- 【复现】`python3 docs/audits/2026-11-03-codex-r5/experiments/run.py r5_service_midcut`，exit=0。50帧（3000ms）入队并finish；播放120ms后断P14，1s后恢复。再等10s仍 `played_ms=120 busy=1 mode=0 clock=0 power=1 failures=0 storage_pending=1`。
- 【代码核查】`audio_service.c:284–285` 对 `AU.out_open` 直接返回成功；`347–349` 写入0样本没有停滞超时，`385–398` 继续保持唤醒、全速和Flash写入hold。`hal_audio_sim.c:257–261` 无时钟不推进DMA；`302–330` 的重新设模式仅由启动调用触发。新加的8s失败通知覆盖不了这个状态。
- 影响：【复现】已接受的声音不再续播，也不告知失败；复电10s后后台写入仍pending。手动 `ink_audio_stop_all()` 后 `busy=0/storage_pending=0`，新流播放完整300ms，证明非整个模拟器死锁。【代码核查、推断】没有外部取消时这条状态没有自动恢复或失败出口，持续持有唤醒；存储会在排队超过30s时强制写（`system/services/storage/storage_service.c:41,311`），因此不能称写入永久阻塞。本轮没有实测等待到30s。
- 建议：检测供电代次变化或持续无播放进展，明确有界重启/失败，处理未播帧并释放hold；增加中途失去时钟的服务级回归。
- 分类：前几轮遗漏的播放中途故障路径；原AUD5-002的“启动失败丢帧”已改善，不把两者混为同一个复现。


### AUD6-002（高）：恢复GC16完成后重刷标志未清除，下一次局刷又清整屏约10秒

- 【复现】`AUDIT_END=60s python3 docs/audits/2026-11-03-codex-r5/experiments/run.py --pqbn r5_system_redraw r5_system_redraw_control`，均exit=0。真实显示服务：故障后的恢复全屏已完成（12.092s、busy/pending=0、目标黑色），下一次128×128更新却再耗 **10.119s**；正常对照 **0.329s**，多9.79s，约30.8倍。第二次才出现 `display.redraws_after_supply_loss=1`。
- 【代码核查】`refresh_policy.c:109–120,151–153` 将失败后的下一次请求升级为全屏GC16；`hal_display_sim.c:1012–1018` 先进入用户清理，导致 `1272` 的 `g_redraw && PQ.clean_stage == 0` 不成立，`1279` 清标志不执行。`1210–1221` 的用户清理收尾也未清恢复标志。
- 影响：供电恢复后下一次正常动作发生多余全屏闪烁和约10s等待。直接HAL提交另一区域GC16时，还会跳过此前受损区域：`r5_redraw_gc16_other` 在该次成功回调后，原目标黑块均值150.66；下一次GL16才恢复至0.22。该局部GC16情况不是当前系统强制全屏策略的典型路径，作为同根因的HAL边界证据。
- 建议：将成功清理覆盖的范围从恢复记录扣除；不足则合并剩余区域补清；对“失败→系统强制GC16→下一次局刷”增加断言。
- 分类：本轮修复新引入的回归（`39db89f`），影响可选PQ-BN档案；默认EPDiy在本组未出现。

### AUD6-003（中）：语音失败提示随后被回答完成覆盖

- 【复现】`AUDIT_END=40s python3 docs/audits/2026-11-03-codex-r5/experiments/run.py --asan r5_ai_speaker_fail`，exit=0，无sanitizer错误。16.842s/17.850s UI标签为“声音没能播放：扬声器没打开，文字回答在上面”；18.860s为“回答完成 · 按 AI 键继续问，按 HOME 关闭”。最终 `ai.speech_failed=audio.play_failed=1`，只播放过60ms按键提示音，回答声音为0。
- 【代码核查】`ai_service.c:194–200` 的kind=4只改标签，kind=1无条件覆盖；没有本轮语音失败状态供完成分支检查。监听本身在 `253–259` 正常执行。
- 影响：晚看屏的用户失去“声音未播”的解释。此前确实短暂提示过，文字也正常完成，故定中，不称作完全静默丢失或高。
- 建议：将语音失败与文字完成分开保存；最终状态保留播放失败原因，直到新轮次或用户重试。

## 审计范围、命令与限制

- **对象及只读边界**：开始与收尾 `git rev-parse HEAD` 均为 `0dbf9aaa97a354c6d62eeaef2b4dfbfd1e8237f3`；`git diff --exit-code`、`git diff --cached --exit-code` 为0，`git status --short` 仅本轮审计目录未跟踪。修复前为本地 `89b112620730902f0fa1594e83ffb146b08749be` 隔离归档，没有切换/替换当前检出。
- **实际命令**：任务指定的离线 `cmake -S . -B build-audit -DINK_WERROR=ON …`、`cmake --build build-audit -j 8`、`ctest --test-dir build-audit -j 8 --timeout 300 --output-on-failure`；ASan目录加 `-DINK_SANITIZE=ON -DINK_GUI=OFF` 同样配置/构建/CTest；旧版4个场景对照；本轮 `experiments/run.py` 53个组合。配置/构建/正式测试exit均0；所有完整参数、分组命令及日志位置见 [复现说明](experiments/README.md)，每条探针命令见 [results.json](experiments/results.json)。
- **环境与产物**：macOS 15.7.9 arm64、Apple clang17、CMake3.28.3、CTest4.4.4。构建日志、WAV、PNG、trace和Flash镜像只留任务允许的 `build-audit/`、`build-audit-asan/`；报告、JSON及复现源码只写本轮目录，每文件小于1MB。JSON结构、源码行号、链接、只读边界检查见 [artifact-validation.json](experiments/artifact-validation.json)。
- **已看范围**：六项修复对应的板级P05、TCA初始化changed、两条显示管线与刷新策略、音频服务/模拟HAL、AI/UI回调、场景faults与预算/GUI配置，以及直接相关的电源、存储和输入调用路径。硬件依据仅来自任务允许的官方固定检出，引用日期2026-11-03；没有把官方描述或模型输出当成实机证据。
- **未看及原因**：本轮是第四轮处理的复审，没有从头重做四轮历史全部问题或完整工程审计；未运行TSan、LeakSanitizer、真实SMP并行/无限压力/任意宿主卡顿验证，故不保证无并发竞争、泄漏或GUI偶发。ASan构建关闭GUI，GUI仅普通dummy后端验证。探针静态宿主图像缓冲不计目标内存，不能由本轮探针推出资源足够。
- **未接触**：按任务要求未联网、未读远端决策板或Issue/PR、未访问设备、未读 `ink-hw/upstream/` 外的私有实机资料；未核验电路、真实墨水屏波形/功耗、真实摄像头和音频兼容性。未审计新的实体端口，也不据此批准固件上机或发布。没有调用子代理或外部模型，没有更改项目决策记录。
