# 第九轮软件质量复审

审计日期：2026-11-04。审计对象：`31642df9fb101e79041d03b1edb215174c6dc116`。

状态：已完成。本文件先建立骨架，随后按 A～E 核查组增量写入，并同步 `verification.json`；最后汇总本节结论。

## 一页结论

**在本轮复审范围内，除已知 F-6 外，没有发现仍需按严重或高定级的问题。** 原 AUD9-002 的千倍漏账已消除；仍有 **1 条中级问题 AUD10-001**：跨 4096 边界只跳 1～2 相时，维护成本最多已复现少计约 **0.12 ms**。没有证据显示它会翻转本轮预算或引发音频欠载。因此，按本轮已覆盖范围已达到“没有太大问题”，不等于没有缺陷、历史事项全部关闭或可以据此作实机/发布验收。

- **AUD9-001 已修复**：原 23 组和新增 552 组，在普通及 ASan/UBSan 下均通过；跨 4096、65536、uint32 边界，混合状态与随后驱动输出一致，扫行数正确。极大步长的 12 组采用明确验证过的周期参考，未冒充数十亿相逐次执行。
- **AUD9-002 部分修复，剩余降为中级**：255 相白屏维护的 slow 成本约 58.1～58.6 ms，当前记 58.5～58.7 ms，并实际进入 `epd` 任务 CPU 账；灰屏采用悲观白屏成本。窄范围短跳残留见 AUD10-001。
- **AUD2-023 部分修复**：测试数、F-6 理由、维护状态主文案已改；还需补充短跳成本限定，并纠正 FLASH_UI 被称作 GC16 闪刷、背景“内核只做拆分”等表述。这些不另列重复问题编号。
- **正常与故障路径未见新增功能失败**：Werror **43/43**、ASan/UBSan **42/42**；22 个补充组合、12 组前后对照。默认 EPDiy 6 组全部指标一致；PQ-BN 阅读动作中位数 908.041→943.515 ms，预算通过。4/60/600 s 静止后刷新及音频并发完成，18 s 语音完整、欠载 0，6 个 WAV 的 352000 样本逐个一致。10/12 组最终 PNG 相同，两组 PQ-BN 的少量灰阶差异如实列于 C2。

证据分为【复现】【代码核查】【文档核查】【推断】，细节、命令和限制见下文。[修复核实记录](verification.json)、[新问题](findings.json)、[实验说明](experiments/README.md)。F-6 继续待用户决定，本轮没有修改产品或替用户作该决定。

## 修复核实表

| 编号 | 结论 | 证据 | 说明 |
| --- | --- | --- | --- |
| AUD9-001 | 已修复 | 【复现、代码核查】 | 原 23 组及扩展 552 组，普通/ASan/UBSan 均通过；状态、后续驱动、扫行数一致。 |
| AUD9-002 | 部分修复 | 【复现（指令级模型）、代码核查】 | 千倍漏账已消除；跨 4096 的 1～2 相短跳仍少计约 0.05～0.12 ms，残留定中，见 AUD10-001；正常场景与并发未见新增失败。 |
| AUD2-023 | 部分修复 | 【文档核查、代码核查、复现】 | 前轮测试数、F-6 理由、维护成本主结论已改；仍需限定 AUD10-001，并修正 FLASH_UI 被称为闪刷及背景“只做拆分”等文案。 |

## 分组核查记录

### A. 基线、构建与现有测试

【复现】`git rev-parse HEAD` 为上述提交；开始时 `git status --short` 仅显示本轮审计目录未跟踪。

【复现】`python3 docs/audits/2026-11-03-codex-r9/experiments/suites.py current`：离线配置、Werror 构建、CTest 全部 exit=0，**43/43**，CTest 25.558 s；同脚本 `asan`：ASan/UBSan、无 GUI，全部 exit=0，**42/42**，CTest 8.969 s。含 `unit-pqbn-clock`、两档案整板自检、阅读、AI、待机、存储和显示故障预算；常规构建含无界面 GUI 冒烟。命令、退出码与日志位置见 [suite-steps.json](experiments/suite-steps.json)。

### B. 长跳步 AUD9-001

结论：**已修复**。

- 【复现】`python3 docs/audits/2026-11-03-codex-r9/experiments/pq_checks.py`：第八轮原探针的 23 组在普通与 ASan/UBSan 下均编译/运行 exit=0，`differential_failed=0 total=23`。768、9000、65536 相等原反例已消除，后续白转灰规划相同。684 行屏 `from=1,n=1` 返回 2 行，`from=510,n=255` 返回 341 行，均与实际访问一致。见 [pq-results.json](experiments/pq-results.json)。
- 【复现】`python3 docs/audits/2026-11-03-codex-r9/experiments/pq_extended.py`：普通及 ASan/UBSan 各 **552/552**，`extended_failed=0 total=552 direct=540 periodic=12`。使用真实 API 生成混合黑白/灰阶、已完成计划、手动运动与自动退出运动后的静止状态；10 个起点包含 0、4095/4096、65530、`0xffffff00`、`UINT32_MAX`，16 个步长含 0/1/255/256/511/512/513/768/4096/9000/65535/65536/65537。覆盖 8 与 684 行高度；核对 hot/cold、tile_plan/nmax/dirty、band、游标、运动与规划状态，并退出运动、全屏换灰后逐行比较 20 相驱动输出。见 [pq-extended-results.json](experiments/pq-extended-results.json)。
- 【复现、推断】其中 12 个极长步长（`0x7fffffff`、`0x80000000`、`0xfffffffe`、`0xffffffff`）没有实际跑数十亿相：参考先逐相跑至少 65536 相，并实际核对再跑 65536 相后完整静止状态重复，再利用已饱和状态的 16 位时钟周期折叠。它与其余 540 个直接逐相对照区分记录。
- 【代码核查】`drivers/display/pqbn/pqbn_v4.c:507–538` 按起点年龄与每行最后一次扫描相重建时间戳；跨 4096/65536/uint32 边界的减法为无符号模运算，年龄累加先扩为 64 位。运动 tile 不把 cold 误作静止时间戳；`541–554` 短跳按相实际累计行数，长跳返回整屏行数。
- **契约边界**：等价前提是被省去的相没有待规划、未完成驱动或自动运动退出等工作；不能在仍有驱动任务时以 skip 代替 phase。模拟器 `ports/sim/hal/hal_display_sim.c:1044–1051,1428–1433` 的静止判定排除这些情况。本轮另测保留手动运动标记但已无驱动的状态，未发现回归。

### C. 维护成本及正常路径 AUD9-002

结论：**部分修复；原高级问题的主要影响已消除，剩余是中级边界校准问题 AUD10-001**。

#### C1. 指令级成本与计费路径（已完成）

【复现（指令级模型，非实机）】`python3 docs/audits/2026-11-03-codex-r9/experiments/pq_cost.py` 与 `pq_cost_extended.py`：真实当前内核、1216×684 生产内存布局、ESP RISC-V GCC 15.2.0、仓库 fitted/slow 配置，编译及两种 ISS 运行均 exit=0。结果见 [pq-cost-results.json](experiments/pq-cost-results.json)、[pq-cost-extended-results.json](experiments/pq-cost-extended-results.json)。前者保留上一轮探针中 `model_cycles=rows*43` 的旧账输出，下面按当前档案独立计算，未把旧输出误当现价。

| 调用 | 实际扫行 | fitted ms | slow ms | 当前计费 ms（320 MHz） |
| --- | ---: | ---: | ---: | ---: |
| 白屏 `20→275` | 340 | 37.143924 | 58.114810 | 58.516109 |
| 灰屏 `20→275` | 340 | 3.209730 | 4.845635 | 58.516109 |
| 白屏 `4000→4255` | 340 | 37.343854 | 58.401342 | 58.516109 |
| 白屏跨 uint32，255 相 | 341 | 37.459038 | 58.596497 | 58.687825 |
| 白屏 `4095→4096` | 1 | 0.327304 | 0.424781 | 0.304513 |
| 白屏 `4094→4096` | 3 | 0.526552 | 0.693169 | 0.647944 |

- 【代码核查】`simulator/runtime/profile.c:213–214` 独立加载 `skip_row/skip_call`，默认 55000/42500；PQ-BN 档案实际为 54949/42495。`hal_display_sim.c:1098–1102` 用真实扫行数计算 `skip_call + rows * skip_row`，记 `display.pqbn.rest_upkeep_ms`。`row_idle` 仍为 43，未污染空行 executor 成本。
- 【代码核查】这段在 `pq_drive:1289–1305` 获取 CPU boost、成功上电、标记显示工作后执行；在实际 `epd` 任务（优先级 20，`:1638`）调用 `sim_charge_cycles`，不会只加指标不耗虚拟 CPU。`osal_sim.c:92–95` 进入 `vtk_consume_ns`；`vtk.c:847–859` 每 200 µs 设置抢占点，维护期间不是一次冻结所有任务。`epd_plan` 的正常逐相成本另行计入。默认 EPDiy 由 `pq_mode:878` 分支隔离。
- 【推断】把灰屏也按白屏已静止行计费是明确的悲观策略（表中灰屏高估约 12 倍慢角），不会重现上一轮的主要漏账；但固定调用开销没有完全覆盖少量扫行碰上整屏 tile-age clamp 的边界，不能说所有调用都被慢角包住。最大已复现短跳差约 0.12 ms，没有证据表明它会翻转本轮预算或导致音频欠载，不定高。
- 【复现、范围限制】补测 512/513/UINT32_MAX 长跳慢角为 117.940/123.057/121.737 ms；套同一行价为 117.586 ms。当前模拟器 `hal_display_sim.c:1092` 上限 255，未走这些路径；将来放宽上限需重新测长跳的成本，不将这些值描述为当前模拟器漏账。

#### C2. 端到端、长静止与音频并发

【复现】通过 `git archive 38511ec9b490b20210087dc17a9df31eec0ded6d` 在 `build-audit/before-src/` 独立构建修复前版本，没有替换产品文件。其 5 个正常场景全部通过。共完成 **22 个补充探针组合**（当前普通/内存检查/历史对照），全部 exit=0、warnings=[]；测试结果依靠回调、画面、样本和指标核实，不仅依据退出码。命令及摘录见 [results.json](experiments/results.json)，指标见 [metrics-evidence.json](experiments/metrics-evidence.json)。

| 正常场景指标（ms） | 修复前 `38511ec` | 本轮 `31642df` | 判断 |
| --- | ---: | ---: | --- |
| EPDiy 14 次翻页 TEXT p50/p95 | 709.340 / 723.043 | 相同 | 全部指标相同 |
| EPDiy 待机解锁 p50/p95 | 675.493 / 915.493 | 相同 | 全部指标相同 |
| EPDiy AI 首声 p50 | 2443.887 | 相同 | 全部指标相同 |
| PQ-BN 阅读全部动作到可见 p50 | 908.041 | 943.515 | +35.474；与处理表相符 |
| PQ-BN 阅读 TEXT p50/p95 | 908.041 / 1003.672 | 925.849 / 1017.005 | 14 次翻页均完成，预算通过 |
| PQ-BN 阅读 FLASH_UI 意图到可见 | 891.814 | 950.449 | +58.635；该意图在此档案实际为 GL16，不是 GC16 闪刷 |
| PQ-BN AI 首声 p50 | 2436.058 | 相同 | 播放 27495 ms、录音 4500 ms 不变 |

- 【复现】`r8_idle --pqbn`：4/60/600/4 s 静止后的局部刷新等待由 320.042/300.001/320.042/300.001 ms 变为 380.042/360.001/380.042/360.001 ms（等待循环有量化）；8/8 回调成功，最终 pending/playing=0。`task.epd.cpu_ms` 从 14.572282 增至 293.414968，维护指标共 8 次、累计约 279.070 ms，说明这笔成本实际进了任务账；不是仅新增一个统计字段。
- 【复现】新 `r9_concurrent` 在 4/60/600 s 显示静止后分别播 6 s 变频、变幅语音，同时全屏 GL16 黑屏、GC16 白屏交替（6 次请求）。三个 6000 ms 音频均完成，6/6 显示回调成功，欠载/流失败为 0，末尾显示、音频、存储全部空闲。全屏纯色是较重负载：黑屏请求约 4.214 s、白屏 GC16 约 8.131～8.144 s，历史对应约 4.156 s、8.127～8.140 s；不能用 UI 场景的 950 ms 数字概括它。
- 【复现、代码核查】`python3 docs/audits/2026-11-03-codex-r9/experiments/audio_integrity.py`：无显示对照、当前两显示档案、历史两档案、当前 PQ-BN sanitizer 共 6 个 WAV，**352000 个样本逐个相同**。其中 288000 个语音样本（18 s），另有两段各 32000 个流间静音样本；HAL 在 `hal_audio_sim.c:108–129` 将每个关输出间隔最多写成 2 s。见 [audio-integrity.json](experiments/audio-integrity.json)。这是模拟 codec 输出完整性，不是声学或真实 Opus 结论。
- 【复现】12 组前后对照均无新增告警或预算违反，默认 EPDiy 的 6 组全部指标相同。最终 PNG 有 10 组字节相同；PQ-BN 阅读改了 26/831744 像素（最大灰阶差 17），AI 改了 71/831744 像素（最大差 34）。没有把时序改变后的 PQ-BN 截图说成完全一致。见 [comparison.json](experiments/comparison.json)、[image-comparison.json](experiments/image-comparison.json)。
- 【复现】`r4_normal_activity` 两档案均 9/9 显示回调成功，三次睡眠唤醒、直接录放音与并发摄像头完成；`r6_audio_display` 两档案播放完整 3000 ms、欠载 0；18 s 正常语音插提示音仍为约 129 ms、最终 18060 ms，没有重现 AUD8-001。
- **探针边界**：首次编译辅助程序因嵌套 `ink_main` 宏重定义失败，改为本目录内的命名包装后通过，产品无改动。初版长静止并发探针在 600 s 自动锁屏后因正常浅睡未及时打印末尾标记，显示/音频已完成；其输出留于 [preliminary-results.json](experiments/preliminary-results.json)，不当作完整通过。最终探针通过公开 `INK_NEED_NO_LOCK` hold 隔离自动锁屏，保留空闲降频；普通、历史和 sanitizer 使用相同设置，全部打印最终空闲状态。

### D. 文档 AUD2-023 与硬件依据

结论：**部分修复，剩余为本节列明的限定与文案，未发现严重或高级文档问题**。

- 【文档核查、复现】`README.md:12`、`docs/system/simulator-overview.md:97,110`、`docs/background.md:289` 已更新为 43，与本轮常规 CTest 一致；无 GUI sanitizer 的 42 不构成冲突。`docs/audits/README.md:28–30,40–41` 明确写出两个 PQ-BN 修复、F-6 待决定，以及 KEY 不受触摸/滑动画面代次影响，前轮对应文案残留已处理。
- 【文档核查、代码核查】`docs/requirements.md:88` 的 R-034 与 `docs/research/simulator/hardware-parity.md:37` 同为 22 项、8 一致/14 部分，明确估计、假设、上游和实机证据边界；旧 9/13、21 项一致的文字属于注明日期的历史处理，不当作当前承诺。没有接触文档引用的私有设备日志。
- 【代码核查】2026-11-04 仅读官方本地检出 `ink-hw/upstream/Metalio-E-INK4-Plus`，HEAD=`690a023c66862af103a3187464373ee7c5efcd14`。`main/boards/metalio-e-ink4-plus/config.h:65–100,144–160` 支持 P05/P12/P14 的共享接线、RTC P16 与外部 I2S 时钟；`main/boards/common/power_policy/power_hw.cc:378–485` 支持浅睡与先恢复按键再恢复电源的顺序。此次两项修复没有修改这些板级规则。
- 【文档核查、复现】`docs/background.md:305` 已由“尚未维护”改为下次驱动时计维护成本，15～60 ms 说明与本轮常见场景相符；档案 `metalio-plus-pqbn.json:120` 明确 1216 像素行、fitted/slow 出处及 255 相上限，未冒充实机。但处理表把 AUD9-002/F-8 无限定标为完成，仍须补上 C1 的短跳边界残留，不另给重复文档问题编号。
- 【文档核查、代码核查、复现】处理表 `docs/audits/README.md:28` 把 892→950 ms 称作“全屏闪刷”；实际是 `ui.action_to_visible_ms.FLASH_UI`，`system/services/display/refresh_policy.c:94–96` 在 async 档案将 FLASH_UI 映射为 GL16，只有 FULL 映射 GC16，本轮阅读场景 `display.pqbn.cleans=0`。数字正确、波形解释不准确；补充真实 GC16 结果见 C2。`docs/background.md:236` 的内核“只做了拆分”也应更新以反映新增 skip API，`:240` 的 908/1002 ms 应注明为历史集成结果或更新。归入本项已有文档一致性问题，不按数量另列新编号。

### E. 新问题与其他严重/高风险路径

- 【代码核查】重点检查此次计费增加的可抢占窗口：`pq_drive` 在维护前设置 `D.playing`、持 CPU boost；维护之后首先检查供电是否可信再规划/扫描；Flash pause 在下一轮检查中得到响应。未发现新增的丢电成功回调、未结束驱动就释放占用，或直接冻结 I2S 的路径。
- 【复现】`r7_display_multi`：16 区域丢电、别处清理与受损区域更新交错，补清后恢复，最终 pending/playing=0、redraws=1；补清期间再断电的 `r7_display_recut` 最终 redraws=2 且全部排空。`r7_rails_wake_fault` 故障恢复后 P05/P14 为高、rails_cut=0，下一请求成功。前者及唤醒故障在 ASan/UBSan 下重放结果相同。故障注入中的失败回调是预期，不算正常路径无失败的证据。
- 【复现】整板两档案现有检查、服务失败/恢复、Flash 掉电、输入、内存池、ISS、正常产品场景在 A 的完整套件中通过。结合 22 个补充组合和 C1 指令成本核对，本轮范围内未发现除已知 F-6 外的严重或高级缺陷；唯一新列项为中级 AUD10-001。
- F-6 按任务保留为已知高级待用户决定项，本轮不修、不重复列新问题；也没有据此宣布所有历史 F-1～F-11 都已关闭。

## 新问题

### AUD10-001（中）：跨 4096 边界的短跳维护成本仍未完全覆盖

- **证据**：【复现（指令级模型）、代码核查】，见 C1；真实当前内核 `pqbn_v4_skip(4095,4096)` 返回 1 行，slow=135929.875 cycles（0.424781 ms），当前公式仅 97444 cycles（0.304513 ms），少计 38485.875 cycles（0.120268 ms）。2 相例子少计 0.045225 ms。不是主机耗时，也不是实机数据。
- **位置**：`drivers/display/pqbn/pqbn_v4.c:495–499,548–552`、`ports/sim/hal/hal_display_sim.c:1099`、`simulator/profiles/metalio-plus-pqbn.json:117–118`。
- **影响**：`age_clamp` 在 4096 边界访问全部 6498 个 tile 和 171 个 band，固定每次 42495 周期不足以在只扫 1～3 行时覆盖这笔开销。普通 255 相调用已被当前成本覆盖；本轮没有端到端失败或预算翻转证据。此项是 AUD9-002 的窄范围残留，按影响有限定中，不再延续原千倍漏账的高定级。
- **建议**：独立标定跨边界固定成本，按是否经过 4096 边界附加，或选择足以覆盖短跳边界的 `skip_call`；补测 0/1/2/255 相以及非边界对照。若未来让模拟器调用长跳，再单独标定 `skip_long`。
- **复现**：`python3 docs/audits/2026-11-03-codex-r9/experiments/pq_cost_extended.py`；fitted/slow 编译和运行均 exit=0。

## 命令、范围与限制

### 对象、环境与命令

- 审计提交：`31642df9fb101e79041d03b1edb215174c6dc116`；历史对照：`38511ec9b490b20210087dc17a9df31eec0ded6d`。开始和结束时 HEAD 相同，产品跟踪文件无差异，仅本轮审计目录未跟踪。
- 本机环境：macOS 15.7.9 (24G830) / arm64，Apple clang 17.0.0，CMake 3.28.3，CTest 4.4.4，Python 3.9.6；交叉编译器 `riscv32-esp-elf-gcc` 15.2.0。版本通过命令核实；所有依赖使用 TASK 指定的本地路径。

| 范围 | 已执行命令 / 入口 | 结果 |
| --- | --- | --- |
| 基线与代码 | `git rev-parse HEAD`、`git status --short`、`git log -5 --oneline`、`git diff 38511ec..HEAD`、`rg` / `nl` / `sed` | 固定检出，重点逐行检查修改及调用链 |
| 构建与 CTest | `python3 docs/audits/2026-11-03-codex-r9/experiments/suites.py current` / `asan` / `before` | 配置/构建/测试全部 exit=0；43/43、42/42、5/5 |
| 原内核反例 | `experiments/pq_checks.py` | 普通/ASan/UBSan 各 23/23 |
| 扩展等价性 | `experiments/pq_extended.py` | 普通/ASan/UBSan 各 552/552，极长步长方法见 B |
| 指令级成本 | `experiments/pq_cost.py`、`experiments/pq_cost_extended.py` | 两次编译及 4 次 ISS 运行 exit=0；确认 AUD10-001 |
| 正常、并发、故障 | `experiments/run.py`，完整 10 条分组命令见 [实验说明](experiments/README.md) | 最终 22 组全部 exit=0，语义证据见 C2/E |
| 样本/指标/图片 | `audio_integrity.py`、`summarize.py`、`image_compare.py` | 6 份 PCM 相同；12 组指标对照、2 组 PNG 差异已记录 |
| 只读边界和产物 | `git diff --exit-code HEAD --`、`git ls-files --others --exclude-standard`、`experiments/validate.py` | 产品文件不变；JSON/证据/相对链接/单文件大小检查见 [artifact-validation.json](experiments/artifact-validation.json) |

表内脚本的完整前缀为 `docs/audits/2026-11-03-codex-r9/`；具体编译/运行参数、退出码与摘录已保留在各结果 JSON，完整构建日志不放审计产出目录。

### 没有查的范围及原因

- **固定检出的软件质量复审**：以这两处修复、调用链、现有套件和关联正常/故障路径为范围，未逐行重审整个仓库或重新验收 F-1～F-11。F-6 按任务排除修复及新问题计数；其余已知设计取舍不因本轮“没有新的严重/高”而自动关闭。
- **没有接触实机、私有数据或联网资料**：遵守 TASK，官方依据仅本地 `upstream/Metalio-E-INK4-Plus` 固定 690a023。没有读取 `ink-hw/` 其他目录，没有验证文档引用的私有日志，也没有重新查手册。此次成本是仓库 ISS 假设内的测量，PSRAM/缓存/SMP 争用及功耗仍需设备验证。
- **没有真实双核竞争、Linux 或真实外设验收**：本机运行的是确定性 VTK 与 macOS 的 ASan/UBSan；没有 TSan、目标固件、真实 LCD/I2S、网络服务、Opus 或声学测试。GUI 仅无界面冒烟，不访问用户摄像头/麦克风。
- **长静止是虚拟时间**：最长 600 s 静止不是实机耐久试验；巨量跳步的周期参考有明确前提，未将折叠参考说成逐相完整枚举。成本模型的新中级残留只在补充 ISS 复现，未声称它在整机上造成了预算失败。
- **没有同步外部决策板、Issue/PR 或远端文档**：本任务限定本检出、只读及本目录产出，因此不执行跨会话的远端同步，也不声称已远端保存。
- **写入边界**：产出仅在本轮审计目录；构建/运行临时产物仅在 TASK 允许的 `build-audit/` 和 `build-audit-asan/`。不修改产品代码、原有测试、档案或现有文档，不提交、不推送、不建分支，不调用子代理或外部模型。
