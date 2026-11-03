# 本机 Claude Code 接手说明（2026-11-03）

## 为什么交接

- 实物（CloudZao Metalio E-Ink4 Plus，ESP32-S31）2026-11-03 到货，已用 USB 接在用户的 Mac（Apple Silicon）上。
- 之前的工作都在云端会话完成，但云端碰不到 USB 设备，所以项目改由用户本机的 Claude Code 推进。
- 云端的全部成果都在 `main`（提交 3b08a67 及以后），没有未合并的分支，也没有只留在云端的资料。

## 第一天的顺序

### 1. 准备仓库

- 仓库原计划放在 `~/work/ink`。2026-11-03 用户说用当前路径就可以，所以仓库留在用户电脑上原来的工作区目录。没有就 `git clone https://github.com/Gidean-zcx/ink`，有就 `git pull`。
- 用 `gh auth status` 确认能推送；不能就请用户 `gh auth login`。

### 2. 读文档（按顺序）

1. `CLAUDE.md`（自动加载）和 `AGENTS.md`。AGENTS.md 是协作约定；末尾“Codex and OpenCode hybrid routing”只适用于 Codex，与你无关。
2. [docs/background.md](../background.md)：项目全貌、证据边界、构建测试方法、已知问题。
3. [docs/project.md](../project.md)：先看 2026-11-03 和 2026-11-02 的记录。
4. [docs/hardware/README.md](../hardware/README.md)（实机规则、上游配置要点、步骤计划）和 [docs/hardware/phase0-prompt.md](../hardware/phase0-prompt.md)（第 0 步的规则与步骤）。
5. [docs/decisions/README.md](../decisions/README.md)、[docs/system/risks-and-spikes.md](../system/risks-and-spikes.md)。
6. 动到模拟器或系统代码前，再读 [docs/system/platform-and-simulator.md](../system/platform-and-simulator.md) 和 [docs/system/foundation.md](../system/foundation.md)。

### 3. 在 Mac 上构建并跑测试（建议，用来确认本机环境）

依赖：

- CMake、Python 3，Python 包 Pillow；
- Noto Sans CJK 字体：Homebrew 的字体 cask 或 notofonts 发布包都可以；
- 首次配置会从 GitHub 拉取 LVGL v9.5.0。

字体路径默认按 Debian 写的，在 Mac 上要指定：

```bash
cmake -S . -B build -DINK_CJK_FONT=<字体文件> -DINK_CJK_FACE=<简体中文字形在集合里的序号，单个 OTF 填 0>
cmake --build build -j && ctest --test-dir build --output-on-failure    # 应为 31 项（2026-11-03：原 26 项，审计复核 +3，屏幕电源链路 +2）
```

注意：

- 模拟器只在 Linux 上验证过（GitHub Actions 的 Linux CI 是基准）。代码只用了标准 pthread、mmap、clock_gettime，大概率能在 macOS 编过，但没试过。
- 编不过时，小的可移植性修复可以直接做，系统要能迁移到别的平台本来就是 R-035 的要求；但不要为了编过而改变模型行为。确实麻烦就用 Linux 容器跑。
- `tools/iss` 的编译选项里有 `-frounding-math`，Apple clang 可能对它告警。

### 4. 实机第 0 步：只读识别、整片备份、出厂固件日志

- **已于 2026-11-03 完成**，见 [hardware/2026-11-03-phase0.md](../hardware/2026-11-03-phase0.md)。下面保留原步骤。
- 先看实机资料目录里是否已有结果：用户可能已让 Codex 做过。有就核对，不重复备份。
- 没有就由你按 [phase0-prompt.md](../hardware/phase0-prompt.md) 的规则和步骤直接执行。那份提示词原本写给 Codex，规则对你同样适用。
- 产出放在仓库同级目录 `ink-hw/bringup-YYYYMMDD/`，不在仓库里。原计划是 `~/work/ink-hw/`，按用户要求改到当前工作区；不放进仓库目录，因为仓库是公开的，而且 `git clean -fdx` 会删掉被忽略的文件。
- 结束后：
  - 把脱敏摘要写成 `docs/hardware/YYYY-MM-DD-phase0.md`；
  - 更新 [hardware/README.md](../hardware/README.md) 的现状与记录表；
  - 在 `docs/project.md` 加一条记录；
  - 提交并推送。

### 5. 把实测值写回模型

| 实测项 | 写回哪里 |
| --- | --- |
| PSRAM 容量 | 决定用 8 MB 还是 16 MB 档案（`profiles/metalio-plus.json` / `metalio-plus-psram16.json`） |
| 启动日志里 `heap_init` 列出的内部 RAM 各段 | `profiles/*.json` 的 `memory`（区段、预留） |
| Flash 型号 | 擦写时间、是否支持擦写挂起，影响 D-36、SP-12 |
| 出厂固件日志里的刷新时序（如有） | SP-01，对照 D-35 的送行模型 |

要求：

- 每个数写清出处，例如 `docs/hardware/…` 的哪次记录。
- 改档案后跑全部测试；预算被打破，就如实记录并分析原因，不要放宽门槛来掩盖。
- 同步更新 platform-and-simulator.md 和 background.md 里对应的数字与证据类型（“估计”改成“实测”）。

### 6. 第 1 步：测量固件

**设计**：在 `tools/hw-probe/` 建一个 ESP-IDF 6.1 工程（目标 esp32s31）。

- 只报告硬件信息：芯片、PSRAM、`heap_caps_print_heap_info` 的各段、Flash 型号与大小、复位原因。
- 在真机上跑指令级模拟器用过的同一批校准内核（`tools/iss/kernels/`）。为它们提供 `tools/iss/rt/iss_rt.h` 接口的 ESP-IDF 实现：
  - `iss_begin` / `iss_end` 用 `esp_cpu_get_cycle_count()`；
  - `iss_cache` 对 PSRAM 缓冲用 `esp_cache_msync`；
  - 分配用 `heap_caps_aligned_alloc`。
- 输出每个成本类的周期数，与 `tools/iss/out/costs.json` 逐项对比。重点是墨水屏送行（`k_feed.c`），它决定 D-35 的结论。
- 不初始化 Wi-Fi/蓝牙（会写 phy_init 和 NVS），不碰屏幕电源和 4G。
- 编译选项与上游一致：Flash DIO 80 MHz 16 MB、八线 PSRAM 200 MHz、控制台 UART0 + USB 串口/JTAG。详见 hardware/README.md。

**写入**

- 方式按第 0 步的结果定：USB 接在哪个控制器、安全功能是否关闭、当前从哪个槽启动。
- 建议只写 ota_0 并擦除 otadata；复原时写回原来的 ota_0 与 otadata，或整片恢复。
- **写入前**，先把方案、命令、复原步骤给用户看，得到明确同意。

**环境**：ESP-IDF v6.1 约 1～2 GB。

```bash
git clone -b v6.1 --recursive https://github.com/espressif/esp-idf ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32s31
. ~/esp/esp-idf/export.sh
```

还要 `brew install cmake ninja dfu-util ccache`。

**之后**：结果写进 `docs/hardware/`，更新 [指令级校准报告](../research/simulator/iss-calibration.md)，并按差异修正档案里的成本。

### 7. 之后的路线

- 实机：见 [hardware/README.md](../hardware/README.md) 的步骤表：屏幕点亮与刷新时序、Flash 擦写停顿、音频并发、电流，对应 SP-01、SP-02、SP-12、SP-13、SP-14。
- 系统：`ports/esp32s31` 端口骨架（OSAL 与各 HAL 的设备实现），以及 [background.md §12](../background.md) 的未完成项：
  - PQ-BN“规划跟着扫描走”；
  - 调制解调器服务与短信/来电唤醒；
  - 设备上的确认卡片与成员 PIN；
  - `ports/posix` + SDL 等。
- 界面视觉设计按用户要求放在底座之后。

## 规则

**交流与授权**

- 用中文。
- 确定的事直接做。不确定的给方案和推荐，记入决策记录；可逆的可以按推荐暂定。
- 以下必须等用户明确选择：
  - 采购、花钱，包括 4G 发短信、打电话产生的费用；
  - 拆机或改硬件；
  - 删除历史资料；
  - 对外发布；
  - 任何可能让设备变砖或不可逆的操作。
- 用户 2026-11-02 的指示：“按你的节奏来，先把底层搞好”。
- 需要用户按键、插拔、操作屏幕时，说清楚做什么，等用户回复。

**设备安全**

- 写设备（烧录、擦除、写 NVS、OTA）之前，必须有核对过的整片备份（两次读取一致，且与设备比对通过），并得到用户同意。
- 永不烧 eFuse，不开启 Flash 加密或安全启动。
- 不升级 4G 模组固件，不改模组的非易失配置。
- 屏幕电源芯片 TPS65185 的上电时序与 VCOM 照搬上游代码，先给用户看再试：设错可能损坏面板。
- 每次写入前写好复原步骤。

**隐私**

- 仓库是公开的。备份、原始日志、NVS 内容、TF 卡数据只留在本机：仓库同级的 `ink-hw/`；整片备份的第二份在本机另一个目录。
- 提交前确认没有 MAC、IMEI、ICCID、序列号、Wi-Fi 名称与密码、令牌。
- 只在项目目录里工作：仓库、仓库同级的 `ink-hw/`、第二份备份目录、`~/esp`。不读这台电脑上与项目无关的文件。

**记录**

- 区分证据类型：实测、代码核查、估算、上游文档，引用写出处与查阅日期。
- 三类内容分开记：用户已明确的要求（requirements.md）、用户的想法（ideas.md）、助手的建议与决策（decisions/README.md）。不把建议写成需求。
- 实测数据只记在 `docs/hardware/`；每次工作结束在 `docs/project.md` 记一笔。

**Git**

- 文档小改可以直接推 `main`。
- 代码改动先在分支上跑通 CI 再合入 `main`。
- 不 force push、不改写历史、不删分支。

## 本机会话看不到的东西

- **决策板**（claude.ai 上的页面，用户的选择存在页面数据库里）：本机会话读不到。截至 2026-11-02 的选择已同步到 [decisions/README.md](../decisions/README.md)，D-35、D-40 仍是暂定。用户之后在板上改了选择，请用户告诉你。
- **claude.ai 项目“嵌入式墨水屏AI系统”**的项目文档与记忆：本机看不到，但内容都已在仓库里。
- **Codex 全面审计**：报告会推到分支 `audit/codex-full-2026-11`（目录 `docs/audits/2026-11-codex/`）。到了以后逐条复现核实，再决定修复、记录或不处理，结果写回 [audits/README.md](../audits/README.md)。
- **GitHub Actions**：每次推送 `main`、`claude/**` 分支，以及每个 PR，都会自动跑 Linux 构建、全部测试和 ASan。

## 交给本机 Claude Code 的提示词

原样保留。其中的 `~/work/…` 路径已按上文改用当前工作区。

```
你接手 GitHub 仓库 Gidean-zcx/ink 的开发（一台 AI 原生墨水屏设备的系统与模拟器）。之前的工作在云端会话完成；现在实物（CloudZao Metalio E-Ink4 Plus，ESP32-S31）已通过 USB 接到这台 Mac，改由你在本机推进。

1. 准备：仓库放在 ~/work/ink（没有就 git clone https://github.com/Gidean-zcx/ink ~/work/ink，有就 git pull）。用 gh auth status 确认能推送，不行请我登录。
2. 先读 docs/handoff/local-claude-code.md（交接说明：第一天的任务顺序、规则、环境准备），再按其中的顺序读其它文档。CLAUDE.md 和 AGENTS.md 的协作约定照做；AGENTS.md 末尾的 Codex/OpenCode 路由与你无关。
3. 然后按交接说明开始：
   - 先做实机第 0 步：只读识别、整片备份、出厂固件日志。规则和步骤在 docs/hardware/phase0-prompt.md，由你直接执行；如果 ~/work/ink-hw/ 里已有结果，先核对，不要重复。
   - 把脱敏结果写进 docs/hardware/ 并推送，再把实测值写回模拟器档案。

最要紧的规则（交接说明里有完整版）：
- 用中文跟我交流。
- 写设备（烧录、擦除、写 NVS、OTA）前，必须有核对过的整片备份，并把方案和复原步骤给我看，得到我明确同意。永不烧 eFuse，不开 Flash 加密或安全启动，不改 4G 模组固件或配置。
- 仓库是公开的：备份、原始日志、NVS、TF 卡数据只留在 ~/work/ink-hw。提交前确认没有 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。不读这台电脑上与项目无关的文件。
- 以下要我明确选择：采购、花钱（包括发短信、打电话）、拆机改硬件、删除历史资料、对外发布。其它确定的事直接做，不确定的给方案和推荐。
- 需要我按键、插拔、操作屏幕时，说清楚做什么，等我回复。
```
