# 全面审计任务说明（2026-11，交给 Codex）

本文是交给外部审计代理的完整任务说明，也作为审计的验收标准存档。

- 用户要求：做一次**非常全面**的审计；报告推送到单独分支。
- 背景先读 [项目全貌与背景](../background.md)。

---

## 0. 角色与目标

你是独立审计方。对仓库 `Gidean-zcx/ink` 的 `main` 分支做全面审计，对象是开始审计时 main 的最新提交，用 `git rev-parse HEAD` 记录。范围包括：

- 代码；
- 模拟器是否真实；
- 测试与预算是否有效；
- 文档里的结论与数字；
- 需求、决策与实现的一致性；
- 安全、隐私与合规；
- 仓库卫生。

**目标是找出错误、夸大、遗漏和风险，而不是确认现有结论。** 文档自称经过三次独立评审，这不是降低标准的理由。没有发现问题的地方，也要写明查了什么、怎么查的。

## 1. 先读

按顺序：

1. [AGENTS.md](../../AGENTS.md)：协作约定，包括证据分级、中文、需求与建议分开记录，以及 Codex 子代理路由。照做。
2. [docs/background.md](../background.md)：全貌、证据边界、已知问题（§12）、构建方法（§11）。
3. [docs/project.md](../project.md)、[docs/process.md](../process.md)、[docs/requirements.md](../requirements.md)、[docs/ideas.md](../ideas.md)、[docs/decisions/README.md](../decisions/README.md)。
4. [docs/system/platform-and-simulator.md](../system/platform-and-simulator.md)：模拟器的唯一维护文档，含前两次评审的问题与处理（§5.2、§5.3）。
5. [docs/system/foundation.md](../system/foundation.md)、[srs.md](../system/srs.md)、[architecture.md](../system/architecture.md)、[risks-and-spikes.md](../system/risks-and-spikes.md)。
6. [docs/research/eink-refresh-integration.md](../research/eink-refresh-integration.md)（第三次评审与刷新算法集成）、[docs/research/simulator/iss-calibration.md](../research/simulator/iss-calibration.md)、[docs/research/simulator/performance-modeling.md](../research/simulator/performance-modeling.md)。
7. S0 调研资料（`docs/briefs/`、`docs/research/interaction-library/` 等）是参考库，不必通读。审计 G、I 两部分时按需抽查。

## 2. 规则与边界

**只读审计**

- 不修改 `main`，不改产品代码、文档、测试、档案。
- 不 force push，不合并任何分支，不删除分支。
- 不改仓库设置（包括可见性），不对外发布，不花钱。

**产出的位置**

- 所有产出放在新分支 `audit/codex-full-2026-11`，从审计对象提交拉出，只新增 `docs/audits/2026-11-codex/` 下的文件。
- 可以开一个指向 main 的 PR 供审阅，但不要合并。
- 修复建议写成补丁文件，放在 `patches/`，不要应用。
- 新场景、测试驱动、模糊测试、分析脚本放在 `experiments/`，附运行说明。
- 单个文件不超过 1 MB；日志只截取关键部分。

**写法**

- 报告用中文。代码标识、命令、路径保持原样。
- 每条结论标明证据类型，推断不得写成事实：
  - **【复现】**：实际运行得到，附命令和输出摘录；
  - **【代码核查】**：读代码确认，给出 `文件:行号`；
  - **【文档核查】**；
  - **【推断】**：没有验证的推理。
- 已知问题（`docs/background.md` §12、三次评审已列出的）可以复评，但标“已知”。重点放在新发现。
- 因环境无法运行的部分（无网络、无字体、无交叉编译器等），写明原因和替代的核查方式，不要略过不提。

**子代理**

- 按 AGENTS.md 的路由：`worker` 只做命令执行、环境检查和日志初筛。
- 结论由你亲自核实。

## 3. 环境准备

**能联网时**

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git python3 python3-pil fonts-noto-cjk
# 可选，用于静态分析与对比编译器：
sudo apt-get install -y clang clang-tools clang-tidy cppcheck
```

首次配置会从 GitHub 拉取 LVGL v9.5.0。

**不能联网时**

在环境设置阶段先执行：

```bash
git clone --depth 1 -b v9.5.0 https://github.com/lvgl/lvgl /opt/lvgl
```

配置时加 `-DFETCHCONTENT_SOURCE_DIR_LVGL=/opt/lvgl`。

**缺少中文字体**（`/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc`）时，大部分系统场景会被跳过，只剩少数测试。必须先装好 `fonts-noto-cjk`，并确认 `ctest -N` 列出 26 项。

**可选：复现指令级校准**

- 需要 ESP-IDF 6.1 的 `riscv32-esp-elf-gcc`。仓库产物用的是 `crosstool-NG esp-15.2.0_20251204`，可从乐鑫 crosstool-NG 的发布页获取，用 `RV_GCC=` 指定路径。
- `calibrate.sh` 还需要从 GitHub 拉取 CoreMark、tinf、miniz。

**记录**：操作系统、编译器与版本、CMake、Python、是否联网、字体是否就位，以及各步耗时。

## 4. 审计范围（必须逐项覆盖，报告里给覆盖表）

### A. 构建与测试的可复现性

1. **基本构建与测试**：干净构建并运行全部 26 项测试；分别用 `-DINK_WERROR=ON`、`-DINK_SANITIZE=ON`（ASan + UBSan）再构建、运行。如果装了 clang，再用 clang 构建一遍。
2. **确定性**：同一场景跑两次，比较 `metrics.json` 与截图是否逐位一致。条件允许时，比较不同编译器、不同优化级别下的结果。指标有差异时，判断它是否影响预算结论。
3. **预算是否有效**：逐个检查 `tests/scenarios/*.budget.json`、`tests/sim/selftest.budget.json`、`tests/scenarios/portability.budget.json`：
   - 哪些门槛不可能失败：`optional: true`、阈值过宽、指标没被记录时是否算通过；
   - 哪些关键行为根本没有门槛。

   对关键预算做**变异测试**：在临时副本里故意引入已知缺陷，确认测试能抓到，结果记入报告，副本不要提交。可选的缺陷包括：
   - 把送行成本调高到欠载；
   - 让 Flash 操作与波形重叠；
   - 把盖板键改回只认一个；
   - 去掉睡前的安静检查；
   - 关闭 PQ-BN 的相时钟推进。
4. **CI**：`.github/workflows/sim.yml` 与本地测试是否一致，在 GitHub 上依赖什么（字体安装、拉取 LVGL）；`s31-size.yml` 能否运行。

### B. 第一级模拟器的正确性与真实性（`sim/`、`ports/sim/`）

1. **虚拟时间内核**（`sim/vtk/vtk.c`）：
   - 是否符合 FreeRTOS 语义：优先级抢占、同级按节拍轮转、节拍取整、互斥量优先级继承、ISR 上下文、定时器、队列/信号量/事件；
   - 双核建模；
   - “接力棒”机制有没有死锁、丢唤醒或饿死；
   - Flash 停顿冻结的实现；
   - 虚拟时间推进的边界条件。

   可以在 `experiments/` 写小程序做针对性验证。
2. **内存模型**（`sim/mem/pools.c`、`ports/sim/osal_sim.c`）：
   - 区段划分与 DMA 预留；
   - 分配顺序是否符合 ESP-IDF 6.1 在 S31 上的堆能力表（给出依据）；
   - 任务控制块、栈、队列、定时器是否计入；
   - 碎片与最大块统计、`osal_mem_stats` 的语义（刚修改过）、泄漏检测。
3. **墨水屏模型**（`ports/sim/hal_display_sim.c`，EPDiy 端口与上游管线两种档案）：
   - 逐行送行、预填与欠载判定、行时间与相长；
   - PMIC 上电与共享 I2C、比对与回写、各波形相数、清屏；
   - 是否与 `platform-and-simulator.md` §2、§5.4 的描述一致；
   - 是否与上游固件（固定提交 `690a023`）一致（能联网时核对）。
4. **PQ-BN 集成**（同一文件的 PQ-BN 部分、`drivers/epd/pqbn_v4.c`、`profiles/metalio-plus-pqbn.json`）：
   - 内核与研究原样的差异，对照 `docs/research/eink-refresh/pqbn_v4/kernel/pqbn_v4.c`；
   - **独立判断**“规划下一相会改写本相仍在推送的 tile，因此不能与扫描重叠”这一结论是否成立；
   - `plan_during_scan` 的语义；
   - 相时钟推进（255 相上限、4096 相钳位）是否与内核的时间戳设计相容；
   - 闪刷清屏状态机、相边界暂停、面板模型；
   - 成本模型与 ISS 拟合值的关系。
5. **Flash 与 LittleFS**（`ports/sim/hal_storage_sim.c`）：
   - ESP-IDF 6.1 关缓存窗口的粒度、NOR 只能 1→0、断电撕裂；
   - 栈在 PSRAM 的任务访问 `/user` 判为死机；
   - 读进 PSRAM 时的中转缓冲；
   - D-36（波形期间零 Flash 操作）怎么统计和执行；
   - 老化镜像。
6. **输入**（`ports/sim/hal_input_sim.c`）：
   - 触摸控制器的检测、轮询、监视模式；
   - 盖板键：每键一个触点，与手指共用 2 个触点，刚修改过；
   - IO 扩展键与去抖、共享 I2C 争用；
   - 睡眠中输入丢失与唤醒键。
7. **网络、音频、编解码、中枢替身**（`hal_net_sim.c`、`hal_audio_sim.c`、`hal_codec_sim.c`、`sim_hub.c`）：链路与 TLS 成本、断网、I2S DMA 与欠载、Opus 替身成本。
8. **电源**（`ports/sim/hal_power_sim.c`）：
   - 各状态电流、浅睡进入与退出、取消回调、唤醒源、按状态统计；
   - 电流全是估算：检查依据与引用是否到位，`platform-and-simulator.md` §5.5 的续航推算算术是否正确。
9. **CPU 成本标注**：
   - 系统代码里的标注是否覆盖真实的计算热点，有没有大段计算没计费；
   - 成本类数值与 `tools/iss/out/costs.json`、各档案是否一致；
   - “悲观档取 ×1.3 与慢角较大者”的约定是否贯彻。

### C. 第二级：指令级模拟器（`tools/iss`）

1. **正确性**：RV32IMAFC 的译码与执行（含压缩指令、浮点、原子），自检覆盖面，与主机结果比对的方法。
2. **时序模型**：
   - 流水线、分支预测、I/D 缓存、PSRAM 延迟的假设与出处（`tools/iss/timing/esp32s31.cfg`）；
   - CoreMark 只拟合到 3.08/MHz（比手册低约 10%）意味着什么；
   - 用 P4/S3 的 PSRAM 实测拟合 S31 是否合理。
3. **复现**：
   - 有交叉编译器时：运行 `tools/iss/calibrate.sh`，核对产物与仓库中的 `tools/iss/out/` 是否逐字节一致；运行 `tools/iss/bench/run_pqbn.sh`，核对集成报告 §2 的表格。研究场景用 `gen_scenarios.py` 生成。
   - 没有交叉编译器时：用 `iss-selftest`、代码核查、现有产物的内部一致性检查代替。
4. **可追溯**：`iss-calibration.md` 中每个数字能否追溯到产物。

### D. 可移植的系统代码（`platform/`、`system/`、`apps/`）

1. **接口设计**：
   - OSAL/HAL 能否在 ESP-IDF（将来的 `ports/esp32s31`）和 Linux（`ports/posix`）上实现；
   - 有没有只在模拟器里成立的假设：依赖确定性、ISR 语义、内存序、阻塞与否、时间源等；
   - 接口注释与实现是否一致。
2. **并发**：
   - 锁的使用、ISR 安全、优先级反转、跨任务数据竞争、回调运行在哪个上下文；
   - 长时间持锁。底座规则是禁止持界面锁等待刷新或 IO。
3. **C 代码安全**：
   - 越界、整数溢出、未初始化、空指针、格式化字符串、资源泄漏、错误路径；
   - 来自中枢的 JSON 是不可信输入，重点查它的解析与校验：`system/net/hub_link.c`、`system/core/ink_json.c`、`system/core/actions.c`。
4. **各服务的逻辑**：
   - 显示服务与刷新策略：模式选择、在途更新、按编号对应完成回调、强制整屏、异步面板；
   - 存储服务：Flash 守卫、等面板安静、暂停驱动、写入合并、断电安全；
   - 输入服务；
   - 界面任务：LVGL 集成、帧提交；
   - 中文排版：禁则、标点悬挂、两端对齐、分页，用样本书和构造的边界文本检验；
   - 阅读器：位置保存与恢复、预排、翻页竞态；
   - 中枢连接：重连、超时、断网；
   - AI 服务：按住说话流程、取消、错误恢复；
   - 动作注册表与策略关口：儿童白名单；需要确认的动作在确认卡片实现之前是否真的不会被执行；
   - 成员：PIN 未实现时的行为；
   - 电源服务：状态机、需求计数、睡前安静检查。
5. **与需求和决策的一致性**：
   - R-039/D-23（AI 可见所有页面数据）与 D-33（只掩码正在输入的凭据）；
   - R-040/D-25（外部副作用需在设备上确认）；
   - D-16（成人 PIN、闲置自动退回）；
   - D-36 在系统代码层面怎么强制执行；
   - D-13（存储分层）。

### E. 测试场景与覆盖

1. **找缺口**：现有场景覆盖了哪些用户路径，缺了哪些高风险路径。
2. **写新场景并实际运行**，放在 `experiments/`，报告每个实验的结果。可选题目：
   - 快速连续翻页；
   - 刷新途中锁屏与唤醒；
   - AI 提问途中断网与重连；
   - 内存紧张；
   - `/user` 写满；
   - 不同时刻断电；
   - 两个手指加盖板键混合输入；
   - 长时间运行（泄漏与碎片）；
   - 在其他设备档案（RK3566 类、桌面 LCD）上跑完整场景。
3. **模糊或随机测试**，配合 ASan/UBSan：场景脚本解析（`ports/sim/script.c`）、档案加载（`sim/profile.c`）、中枢消息解析（`hub_link.c`、`ink_json.c`）、排版输入（任意 UTF-8）。

### F. 静态分析

- 能用的工具都跑一遍：`gcc -fanalyzer`、`scan-build` 或 `clang --analyze`、`clang-tidy`、`cppcheck`。
- 对结果做分诊（真问题 / 误报）。报告给出统计，只把真问题写进发现。

### G. 文档与结论核查

1. **数字核对**：下列文件里的关键数字，逐项用实际运行结果核对，列表给出“声称值 / 复现值 / 是否一致 / 偏差原因”：
   - `README.md`；
   - `docs/background.md` §10；
   - `docs/system/platform-and-simulator.md`（含 §5.x 各表）；
   - `docs/research/eink-refresh-integration.md`；
   - `docs/research/eink-refresh/integration/system-comparison.md`，其中 PQ-BN 各配置按该文件的说明改档案字段复现；
   - `docs/decision-board.html`。
2. **证据分级是否准确**：有没有把估算写成实测、把上游描述写成已验证、把助手建议写成用户需求。对照 `requirements.md`、`ideas.md`、`decisions/README.md` 与 AGENTS.md 的规则检查。
3. **内部一致性**：
   - 同一事实在不同文档中的说法是否一致：硬件参数、决策状态、测试数量、PR 状态等；
   - 有没有过时的表述；
   - 检查全部本地链接有无断链。
4. **外部出处**：硬件规格、上游代码引用（固定提交链接）、厂商数据是否给了出处与查阅日期。能联网时抽查若干条是否属实。

### H. 产品与工程风险

1. **目标与范围**：现有设计能否满足 G-001、G-002 和首版范围（方案 A）。
2. **追溯**：需求 → 设计 → 实现 → 测试，有没有断点。例如 R-013（短信、通话）、R-022/R-023（账户与备份）、R-026（输入法）是否只停留在需求里。
3. **实机验证计划**：风险登记与 SP-00～SP-14 是否完整、顺序是否合理；到货第一周的计划能否执行；还漏了哪些高风险项。
4. **关键技术判断站不站得住**：
   - D-35 送行设计、D-36 Flash 规则；
   - D-37 可接听待机的耗电；
   - D-40 刷新驱动；
   - 内存余量（AI 场景 36 KiB，预算 32 KiB）；
   - LVGL 与排版成本都是估算，结论对它们有多敏感（可调档案参数做敏感性实验）。

### I. 安全、隐私与合规

1. **仓库可见性为 public**：
   - 扫描全部历史中的密钥、令牌、凭据、个人信息，用 gitleaks、trufflehog 或等价脚本；
   - 评估文档中家庭与个人信息的暴露。只列举，不删除。
2. **第三方代码与资料**：
   - `third_party/` 各库及 LVGL v9.5.0 的版本、许可证、已知漏洞；
   - 字体许可证（Noto Sans CJK，OFL）；
   - `docs/research/` 下竞品手册截图、PDF、视频等第三方资料在公开仓库中的版权风险。只评估与列举，不删除。
3. **设计层面的隐私**：
   - D-23 “所有页面对 AI 可见”与儿童数据；
   - 保留期（D-34）；
   - 设备—中枢传输加密；
   - 这些在设计和代码中有没有落实。

### J. 仓库卫生

- 体积与大文件：`docs/` 约 300 MB，历史中有 `work/`；是否需要 Git LFS。
- `.gitignore`、有没有构建产物误入库、无用分支、文件命名。
- 目录结构是否符合 AGENTS.md 的约定。

### K. 架构与 AI 原生审视（用户 2026-11-04 要求，R-055；之后每一轮审计都要有）

单独成节写进报告，发现的问题和别的问题一样编号、定级：

- **模块划分**：模块、组件、子系统的边界是否合理，是否符合“软总线 + 原子能力”（[系统架构 §4、§5](../system/system-architecture.md)）；能力、状态、设置、事件、内部机制归类对不对；机制和界面是否分开；有没有绕过总线的跨模块调用、循环依赖、一个模块做了两件事。
- **AI 原生**：是不是从第一天就给 AI 用：按钮和 AI 是否走同一条路径；结果是否结构化；事件能否到 AI；页面和系统浮层 AI 是否看得到；AI 的行为有没有来源记录、能否撤销、是否都过策略关口。
- **能力开放**：用户能做的事、能看的数据，AI 是否都能调用、能读到；对照[能力总表](../system/system-services-design.md)和运行时的 `tools/list` 核对（测试 `integration-ai-surface`）；不开放的是否写明了理由；有没有不该开放的（内部机制、凭据）被开放了。

## 5. 交付物

分支 `audit/codex-full-2026-11`，目录 `docs/audits/2026-11-codex/`：

| 文件 | 内容 |
| --- | --- |
| `report.md` | 主报告（中文），结构见下 |
| `findings.json` | 每条发现一个对象，字段见下 |
| `experiments/` | 新场景、脚本、模糊测试驱动，附 `README.md` 说明如何运行与结果 |
| `patches/`（可选） | 建议修复的补丁，不应用 |
| `logs/` | 关键日志摘录（小文件） |

**`report.md` 的结构**

1. **摘要**：
   - 整体结论。关键判断用“有依据可行 / 有条件可行 / 信息不足 / 当前约束下不建议”表述；
   - 最重要的 10 条发现；
   - 建议的修复顺序。
2. **审计对象与环境**：提交 SHA、环境、哪些能运行哪些不能、用时、使用的模型与工具。
3. **覆盖表**：A～J 每个编号子项的状态（已查 / 部分 / 未查）、方法、证据位置；“未查”必须写原因。
4. **发现**：按严重度排序。
5. **数字核对表**（G1）。
6. **实验、变异测试、模糊测试、静态分析的记录**（A3、E、F）。
7. **对已知问题的复评**。
8. **未完成的部分与原因**，以及建议的后续审计。

**`findings.json` 的字段**

```json
{
  "id": "AUD-001",
  "title": "一句话",
  "severity": "critical | high | medium | low | info",
  "category": "A | B | ... | J",
  "location": "文件:行号 或 文档章节",
  "evidence_type": "复现 | 代码核查 | 文档核查 | 推断",
  "evidence": "命令与输出摘录、代码引用",
  "impact": "影响什么结论或行为",
  "reproduce": "复现步骤",
  "recommendation": "建议的修复",
  "known": false,
  "confidence": "high | medium | low"
}
```

**严重度**

| 等级 | 含义 |
| --- | --- |
| 严重 | 导致核心结论或设计决定错误，例如关键数字错、模拟器在关键路径上明显偏乐观；或可利用的安全问题、敏感信息泄露 |
| 高 | 真实设备上很可能出错的设计或代码缺陷；测试与预算发现不了的关键回归；重要文档结论与证据不符 |
| 中 | 局部的正确性、健壮性、可移植性问题；文档不一致但不影响主结论 |
| 低 | 可维护性、表述、风格 |
| 信息 | 建议与观察 |

## 6. 质量要求

- 每条发现都必须能核实：给出文件与行号、命令与输出摘录，或者明确标为推断。
- 先复现再下结论；不能复现的写明。
- 区分“模拟器本身的错误”和“模拟器如实暴露出的系统问题”。
- 数字偏差要尽量量化，并说明对结论的影响，不要只说“偏乐观”。
- 不为凑数报告琐碎问题。同类问题合并成一条，列出全部位置。

## 7. 完成后

- 推送分支 `audit/codex-full-2026-11`，在最后的回复里给出提交 SHA、文件清单、发现数量（按严重度）和最重要的 5 条发现。
- 可以开一个指向 main 的 PR 供审阅，但不要合并。
