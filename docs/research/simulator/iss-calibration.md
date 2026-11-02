# S2-L2：指令级校准（ISS）——ESP32-S31 成本单价

查阅日期 2026-11-02。标注沿用 [performance-modeling.md](performance-modeling.md)：
- 【官】官方文档或数据手册；
- 【码】本次读取的源码与配置（ESP-IDF 取 v6.1 标签，提交 `fff9895c`）；
- 【测】第三方实测；
- 【推】推算或推断；
- 【估】经验估计；
- 【仿】本工具在 ISS 上的测量结果，是模型输出，**不是实机数据**；
- **未核实**：没有拿到一手来源。

工具在 [tools/iss/](../../../tools/iss/README.md)，自动生成的完整数据在 [tools/iss/out/report.md](../../../tools/iss/out/report.md) 与 `tools/iss/out/costs.json`。建议值见 §3；**2026-11-02 已写入 profile**：`metalio-plus.json`（及 upstream-display）用悲观值，`metalio-plus-psram16.json` 用预期值（×1.3）；`text.*`、`heap.op`、`lvgl.*` 暂保留原猜测（见 §3 说明）。

## 0 结论

1. **工具已经可用。** `tools/iss/` 是自写的 C11 指令集模拟器（RV32IMAFC + Zba/Zbb/Zbs/Zcb/Zcmp），时序模型为可配置的单发射顺序流水线，带每核 I-cache、共享 D-cache 和分支预测。`bash tools/iss/calibrate.sh` 约 30 秒跑完：拉取源码、交叉编译、自检、拟合、跑内核、写结果。两次运行（含删掉 `build/` 后重新拉取、重新编译）产物逐字节相同。正确性有三重检查：
   - CoreMark 自带的 CRC 全部正确；
   - 同一个 ISA 自检程序，在主机和 ISS 上的校验和一致（两种 -march）；
   - CTest `iss-selftest` 的 8 个手工编码程序，周期数与手算逐一相等。
2. **CoreMark 锚点（每核 3.43/MHz）用公开工具链复现不了。**
   - 按乐鑫 CoreMark 组件的编译选项，GCC 15.2 每次迭代执行 29.2 万条指令【仿】。
   - 3.43 CoreMark/MHz 等于每次迭代 29.15 万周期，也就是 CPI≈1.00；零停顿的理想单发射流水线也只能给出 3.42。
   - 打开数据手册列出的 Zba/Zbb/Zbs/Zcb/Zcmp，指令只少 1.8%；GCC 不会为 C 代码生成 XespLoop/XespV 指令【测，本机】。
   - 在"5 级、顺序、单发射"的可信参数盒内，离锚点最近的是最快的一角：**3.083 CoreMark/MHz，残差 −10.1%**。取它作为拟合结果，偏差落在偏慢一侧，符合悲观原则。
   - 锚点来源不明：可能用了不同的二进制（编译器、扩展、手调），可能内核并非严格单发射，也可能双核分数并不等于单核的两倍，均**未核实**。到货后第一件事是跑单核 CoreMark，并用它替换锚点（§5）。
3. **PSRAM 时间不能只靠时序推算。**
   - 按 IDF 的 AP 八线驱动时序推算，填一行 64 B 约 300 ns（96 周期）。由此算出的 memcpy，比同为 200 MHz、总线宽一倍的 ESP32-P4 实测还快，不可信。
   - 改为"不快于 P4 实测"来拟合：**行填充 144.8 周期（约 450 ns），写回 85 周期**。结果为 P→S 110 MB/s、S→P 82 MB/s、P→P 51 MB/s【仿】。
   - 慢角按 ESP32-S3 实测拟合（287.6/328.6 周期），与现有 profile 的猜测基本一致。
4. **成本类结果**（拟合模型，-O2，热代码；建议值 = raw × 1.3 向上取整）：
   - PSRAM 拷贝三类降到现猜测的 0.51～0.65 倍。
   - 略有上调：`glyph.blit` 10870（现 9000，+21%）、`epd.convert_px` 7.16（现 6）、`json.byte` 75.2（现 60）、`zlib.inflate_byte` 44（现 40）。
   - `crc32.byte` 17.3（现 8，+116%）。
   - `text.break_char` 98、`text.layout_char` 167、`heap.op` 306，远低于现猜测（400、1500、1500）。但这三项的内核只覆盖最小实现，取值建议见 §3。
5. **编译选项影响不小。**
   - 上游固件用 `-Os`。在 `-Os` 下，文本、JSON、堆操作比 `-O2` 慢 20%～80%，例如 `text.break_char` 为 134 对 75【仿】。
   - 若将来 IDF 为 S31 打开 Zb/Zc，大多数类会快 0%～20%。
6. **最大的不确定性在 PSRAM。**
   - 对 PSRAM 主导的类（拷贝、填充、字形混合），慢角比 raw 高 1.7～2.5 倍，超出了 ×1.3 余量所能覆盖的范围。
   - 在拿到 S31 实测的 memcpy 矩阵之前，建议悲观 profile 用"悲观值"（同时覆盖慢角），预期 profile 用"建议值"（§3）。

## 1 工具与方法

### 1.1 ISS（`tools/iss/src`）

**指令集**
- 支持 RV32I/M/A/F/C，以及 Zicsr、Zifencei、Zba、Zbb、Zbs、Zcb、Zcmp（push、pop、popret、mv）。
- 不支持 D、Zcmt、XespV、XespLoop、特权级和中断；遇到就报错停机。

**时序模型（单发射、顺序）**
- 一条指令最早在"上一条的发射时刻 + 上一条的占用周期"之后发射。
- 记分牌记下每个寄存器的就绪时刻（发射 + 结果延迟 + 存储器惩罚）。由此产生的 RAW 停顿按产生者的类别归账，例如 load-use、mul-use。
- 除法、浮点除和开方、CSR 写、Zcmp push/pop 按占用周期阻塞流水线。
- 取指每进入一个新的 cache 行，访问一次 I-cache。这只针对放在 flash/PSRAM 的代码。
- 数据访问：
  - 内部 SRAM 为固定延迟；
  - flash/PSRAM 经过 D-cache：组相联、LRU、写回 + 写分配、缺失阻塞、没有预取、没有关键字优先；
  - 替换脏行时另加写回时间；
  - 跨 4 字节边界的访问加 1 周期；跨 cache 行的访问查两行。
- 分支预测：
  - 结构：2 位 BHT + 直接映射 BTB + RAS；
  - 条件分支只有在 BHT 预测跳转、且 BTB 命中时，才按跳转来预测；
  - 预测正确的跳转加 `br.taken_correct`，方向或目标错了加 `br.mispredict`，JAL 未命中 BTB 时加 `br.jal_nobtb`。
- 时间用 1/256 周期的定点整数。拟合出的小数惩罚也逐位确定。

**度量接口**
- 被测程序用 `ecall` 调用 `iss_begin(name)` 和 `iss_end2(units, aux)`。ISS 记录区段内的周期、指令数、各类停顿、分支统计和 cache 统计。
- `iss_cache()` 可以写回或失效 cache，并可选择把写回时间计入区段（流式写 PSRAM 时使用）。
- 这些调用本身不计时。

**其他**
- ELF32 加载（PT_LOAD）、按函数统计周期（`-p N`）、JSON 报告。
- 速度约 50 MIPS（CoreMark 跑 50 次迭代用 0.27 s）。

**验证**
- CoreMark 性能参数下，seedcrc 0xe9f5、crclist 0xe714、crcmatrix 0x1fd7、crcstate 0x8e3a 全部正确，两种 -march 都通过。
- `kernels/isatest.c` 覆盖 M、Zba/Zbb/Zbs、几乎全部 F 指令和 FP CSR；主机与 ISS 的校验和一致（`a050e1df`）。
- `iss-selftest` 有 8 个手工编码程序，周期数与手算逐一相等。覆盖的内容：循环、分支预测、load-use、乘除法、压缩指令、D-cache 缺失与脏行替换、I-cache、小数惩罚。
- 在 ASan/UBSan 下跑 CoreMark、全部内核和自检，没有报告；`-Werror` 编译通过。

### 1.2 编译配置（与设备构建对齐）

| 项 | 取值 | 依据 |
|---|---|---|
| -march | `rv32imafc_zicsr_zifencei_zaamo_zalrsc`，`-mabi=ilp32f -mtune=esp-base` | 【码】IDF `components/soc/project_include.cmake`：S31 与 P4 的基础 -march 相同，再按 soc_caps 追加扩展。S31 的 `soc_caps.h` 有 HWLOOP 和 PIE，没有 `SOC_CPU_HAS_ZB_EXTENSIONS`/`ZC_EXTENSIONS`。所以设备上的完整 -march 是 `…_xesploop_xespv`，不含 Zb/Zc |
| 去掉 xesploop、xespv | — | 【测，本机】GCC esp-15.2.0 对简单计数循环和整个 CoreMark 都不生成 `esp.*` 指令；PIE 按任务要求不建模 |
| Zb/Zc 变体 | 再加 `_zba_zbb_zbs_zcb_zcmp` | 【官】数据手册列出 Zc（Zcb/Zcmp/Zcmt）与 Zb，但 IDF 尚未为 S31 打开，只作敏感性分析 |
| 内核 | `-O2 -fno-jump-tables -fno-tree-switch-conversion -ffunction-sections -fdata-sections -fstrict-volatile-bitfields`；代码在 flash（XIP），数据在 SRAM 或 PSRAM | 任务要求 -O2；其余几项是 IDF 的全局默认【码】 |
| -Os 变体 | 同上，换成 `-Os` | 【码】上游 Plus 的 `sdkconfig.defaults`：`CONFIG_COMPILER_OPTIMIZATION_SIZE=y` |
| CoreMark | `-O3 -fjump-tables -ftree-switch-conversion`；代码和只读数据放 IRAM；数据静态分配；单上下文；50 次迭代 | 【码】espressif/idf-extra-components 的 `coremark/CMakeLists.txt`、`linker.lf`（`* (noflash)`）和 `core_portme.h.in`（MEM_STATIC） |
| libc | 工具链自带的 newlib（`libc.a`） | 设备上，IDF 链接 ROM 里的 memcpy/memset/str*【码 `esp32s31.rom.libc.ld`】，默认 libc 是 picolibc【码 `esp_libc/Kconfig`】。差异见 §4 |

另一个编译细节：`-mtune=esp-base` 声明非对齐访问不慢，于是 GCC 15 在 -O2 下会把定长 memcpy 内联成字循环；-Os 则调用库函数。实际代码大多在运行时才知道长度，最终调用库（设备上是 ROM）里的 memcpy。所以拷贝类内核经 volatile 函数指针调用 memcpy/memset，两种优化级别下测的是同一段代码。

### 1.3 时序参数

拟合结果完整列在 `tools/iss/out/esp32s31.fitted.cfg`，基础假设和出处注释在 `tools/iss/timing/esp32s31.cfg`。

| 参数 | 拟合模型取值 | 依据 | 性质 |
|---|---|---|---|
| 流水线 | 5 级、顺序、单发射，320 MHz | 【官】数据手册 v0.5 写明 "Five-stage pipeline that supports clock frequency of up to 320 MHz"。手册没写是否单发射（P4 手册同样只写 5 级），单发射是【推】 | 已知 + 假设 |
| `lat.alu` | 1（全旁路） | 类比 SiFive U54："fully bypassed so that most instructions have a one-cycle result latency" | 假设 |
| `lat.load_word` | 2（1 个 load-use 气泡） | 经典 5 级流水线；U54 的字访问为 2 周期 | 假设（类比） |
| `lat.load_sub` | 2 | 拟合，取值范围 {2, 3}；U54 的字节/半字访问为 3 周期 | 拟合 |
| `lat.mul` | 1 | 拟合，取值范围 {1, 2} | 拟合 |
| `occ.div` | 34 | 基 2 迭代、不提前结束，偏慢 | 假设 |
| `br.mispredict` | 2 | 拟合，取值范围 {2, 3}；U54 为 3 | 拟合 |
| `br.taken_correct` | 0 | U54："correctly predicted control-flow instructions result in no penalty" | 假设 |
| `br.jal_nobtb` | 1（译码级重定向） | 拟合，取值范围 {1, 3} | 拟合 |
| BHT/BTB/RAS | 512 / 28 / 6 项 | 【官】S31 有 BHT、BTB、RAS，大小未公开；取 U54 的 512/28/6 | 假设 |
| CSR 读 / CSR 写 | 结果延迟 3 / 冲刷 5 周期 | U54：CSR 写 "five-cycle" 冲刷 | 假设 |
| FPU | add/mul 4、fma 5、杂项 2、div/sqrt 20（阻塞） | 没有公开数据；取 GCC `-mtune=esp-base` 的代价表（乐鑫源码注明 "Copied from rocket"）。本批内核不用浮点，不影响结果 | 假设 |
| I-cache | 每核 16 KiB、2 路、64 B 行 | 【码】IDF `Kconfig.cache`：`CACHE_L1_ICACHE_SIZE` 0x4000，行 64；ROM `cache.h` 最大 32 KB、2 路。【官】手册写 "32 KB … two-way … each CPU has a private instruction cache"，没写明是否每核 32 KB；按 IDF 取 16 KB，偏慢 | 已知 |
| D-cache | 64 KiB、2 路、64 B 行、两核共享、写回 | 【官】数据手册；【码】Kconfig、ROM `cache.h`（4 个 bank） | 已知 |
| 内部 SRAM | 不经 cache，没有额外延迟 | 【码】`cache_ll.h`：`CACHE_LL_LEVEL_INT_MEM 0`，外存只有一级 cache。"没有额外延迟"是假设；S3 实测 IRAM 拷贝 366 MB/s（约 0.66 周期/字节），与模型的 0.64 吻合 | 已知 + 假设 |
| D-cache 命中额外延迟 | 0（慢角取 1） | — | 假设 |
| flash 行填充 | 1150 周期 | 【码】上游用 DIO 80 MHz；【推】0xBB 读命令 8+12+4 个时钟，加 64 B 数据 256 个时钟，约 3.5 µs。与 performance-modeling.md 的 `icache.miss_flash` 相同 | 推算 |
| PSRAM 行填充 / 写回 | 144.8 / 85 周期 | §1.5 拟合 | 拟合 |
| 写分配、阻塞缺失、无预取 | — | 偏慢的假设。S31 的 cache 有 autoload 预取功能【码 `CACHE_LL_CACHE_AUTOLOAD`】，是否默认开启**未核实** | 假设 |

### 1.4 CoreMark 拟合

**做法**
- 在可信参数盒（`lat.load_sub` ∈ {2,3}、`lat.mul` ∈ {1,2}、`br.mispredict` ∈ {2,3}、`br.jal_nobtb` ∈ {1,3}）的 16 个组合上各跑一遍 CoreMark。
- 选离 3.43 最近的组合；若同样近，选更慢的那个。
- 结果在 2.882～3.083 之间，全部低于锚点，所以选中最快的一角。完整网格见 report.md §1。

| 构建 / 角点 | CoreMark/MHz | 残差 | CPI | 指令/迭代 |
|---|---|---|---|---|
| idf / 拟合 | **3.083** | **−10.1%** | 1.110 | 292 217 |
| idf / 理想（零停顿） | 3.422 | −0.2% | 1.000 | 292 217 |
| idf / 慢角 | 2.409 | −29.8% | 1.421 | 292 217 |
| zbzc / 拟合 | 3.123 | −8.9% | 1.115 | 287 096 |
| zbzc / 理想 | 3.471 | +1.2% | 1.004 | 287 096 |
| zbzc / 慢角 | 2.429 | −29.2% | 1.434 | 287 096 |

拟合模型下，每次迭代的停顿构成【仿】：
- 分支 11 764 周期：5589 次误预测 × 2，加 585 次 JAL 未命中 BTB × 1；
- 字 load-use 11 989 周期；
- 字节/半字 load-use 8408 周期；
- 合计让 CPI 从 1 升到 1.11。

要达到 3.43，必须同时消除全部停顿，还得再少约 700 条指令。可能的解释：
- **不同的二进制。** 乐鑫测分时可能用了内部编译器，或 LLVM（IDF 支持 esp-clang 加 `-mcpu=esp32s31`），或其他优化选项、硬件循环、手写汇编。
- **内核不是严格单发射。** 双发射或指令融合与手册里的"五级流水线"并不矛盾。
- **双核分数不是单核的严格两倍。** 2195.20 是双核总分，"每核 3.43"是按线性推出来的。
- 手册 v0.5 是预发布版，数字可能还会变。

P4 手册给出 6.92/MHz（双核），上面的推理对 P4 应该同样成立（**未核实**）。

**对成本数据的含义**
- 如果真实内核接近锚点，那么计算型的类会被高估：最多为理想角点与拟合角点之比，约 11%；再叠加 ×1.3 余量，整体约 1.4 倍。这在安全的一侧。
- 如果真实内核接近慢角（比如每次访存还要多 1 周期），×1.3 余量对计算型的类基本够用：slow/raw 在 1.17～1.41 之间。

### 1.5 PSRAM 拟合

**理论推算**（IDF v6.1 `esp_psram_impl_ap_oct.c`，200 MHz 档）【码】【推】
- 读一行：命令 16 bit（八线 DDR，1 个时钟）+ 地址 32 bit（2 个时钟）+ 读延迟 14 个时钟（`mr0.read_latency = 4`，驱动打印为 `read_latency*2+6`）+ 数据 64 B（32 个时钟）+ CS 建立/保持 4+4（另有 hold delay 3），共约 57～60 个时钟，≈285～300 ns，≈91～96 个 CPU 周期。启用 variable latency 时，若遇到刷新冲突，延迟会加倍。
- 写一行：写延迟约 7 个时钟（`WR_DUMMY_BITLEN 2*(7-1)`），其余同读，共约 50～53 个时钟，≈265 ns，≈85 个周期。

**ESP32-P4 实测**【测】（developer-portal discussion #353）
- 条件：16 线 PSRAM 200 MHz，128 KB L2，标准 memcpy（不用 PIE），8～16 MiB 缓冲。
- 结果：P→S 105.15 MiB/s，S→P 84.86 MiB/s，P→P 50.96 MiB/s；同帖 S→S（128 KB）352 MiB/s。
- 即使用了 PIE，16 MiB 的 P→S 也只有 115.88 MiB/s，远低于 800 MB/s 的总线带宽。可见 CPU 读 PSRAM 主要受延迟限制，每行的实际延迟是纯传输时间的数倍。

**拟合目标："不快于 P4"**
- S31 与 P4 同为 200 MHz，但 S31 是 8 线，每个时钟传的字节数只有 P4 的一半；P4 多一级 L2，S31 没有。前者让 S31 更慢，后者可能让 S31 的缺失路径更短。把最接近的实测兄弟芯片当作速度上限，是悲观的读法【推】。
- 具体做法：先按 P→S 速率解出行填充时间；写回时间保留理论值；再检查 S→P 和 P→P 都不快于 P4，若快了就提高写回时间。

| 角点 | 行填充（周期） | 写回（周期） | 目标 P→S / S→P / P→P（MB/s） | ISS 结果（MB/s）【仿】 |
|---|---|---|---|---|
| 理论（IDF 时序；理想角点使用） | 96 | 85 | — | 149.5 / 102.0 / 66.6 |
| **拟合（不快于 P4）** | **144.8** | **85** | 110.3 / 89.0 / 53.4 | 110.2 / 82.1 / 50.6 |
| 慢角（与 S3 实测相当） | 287.6 | 328.6 | 56.8 / 32.5 / 21.2 | 56.8 / 33.7 / 21.9 |

### 1.6 内核（`tools/iss/kernels`）

每个内核的流程相同：先用固定种子构造输入（不计时）；不计时地跑一遍，让代码进入 I-cache（稳态）；需要流式读写 PSRAM 时先清空 D-cache；然后测一个区段，区段内脏行的写回时间也算进去（这些写回迟早要发生，计入偏悲观，但如实）。

| 区段 | 内容 | 数据位置 | 单位 |
|---|---|---|---|
| `*.copy_byte`、`*.set_byte` | newlib 的 `memcpy`/`memset`。SRAM 用 32 KiB 块；PSRAM 流式读写 256 KiB（D-cache 的 4 倍），从冷 cache 开始。另测已在 cache 中的 4 KiB 拷贝 | 见类名 | 字节 |
| `glyph.blit` | 24 px CJK 字形（24×24 的 A8 位图，8% 为 12×24 半角），由合成笔画加抗锯齿边缘生成，覆盖率约 40%。字形缓存 384 个，放 PSRAM（216 KiB），75% 的字形取自最常用的 96 个。按 30 px 行距排满一页 684×1216 灰度帧缓冲（PSRAM，836 KiB），用 LVGL 式 A8→L8 混合（0 和 255 走快路径，墨色在运行时给出）。另测写入内部 SRAM 的带状缓冲（4 行文字） | PSRAM / SRAM | 字形；第二单位：字形格像素（`glyph.px`） |
| `fill.px` | UI 式的矩形组合（顶栏、列表行、按钮、图标），逐行 memset | PSRAM 帧缓冲 / SRAM 带状缓冲 | 像素 |
| `epd.convert_px` | 8bpp→4bpp，两像素一字节，宽 1216。SRAM→SRAM 版只含计算，对应 `hal_display_sim` 的用法（它另按 0.75×`psram.copy_byte` 计 PSRAM 流量）；另测整帧 PSRAM→PSRAM | SRAM / PSRAM | 像素 |
| `text.break_char` | 合成中文文本，约 6000 字、17 KB，含标点、拉丁词和数字，读入 PSRAM。依次做 UTF-8 解码、取字宽（CJK 24 px，拉丁字母用比例宽度表）、贪心断行（表意字之间可断，拉丁词不拆，遵守避头尾） | PSRAM | 字 |
| `text.layout_char` | 在上一项基础上，加字形缓存哈希查找（2048 槽，PSRAM），并输出带坐标的绘制表。不含光栅化，也不含字体引擎 | PSRAM | 字 |
| `crc32.byte` | 逐字节查表 CRC-32（表在 SRAM），取 SRAM 数据与 PSRAM 数据两者中的较大值 | 两者 | 字节 |
| `json.byte` | cJSON 解析并释放一个 14 KB 的新闻列表响应。文本放 PSRAM；节点经 TLSF 从 PSRAM 池分配，对应上游 `SPIRAM_MALLOC_ALWAYSINTERNAL=16`。另测只解析不释放 | PSRAM | JSON 字节 |
| `zlib.inflate_byte` | miniz 的 `tinfl`（S31 的 ROM 里也有 miniz【码 `esp32s31.rom.ld`】）。输入为文本样本的 zlib 流（8.8 KB 解压到 17 KB，压缩比 1.93），校验 adler32。tinf 作对照 | PSRAM | 输出字节 |
| `heap.op` | TLSF（`third_party/tlsf`）的 malloc/free 混合序列。尺寸分布：60% 为 16–63 B，25% 为 64–511 B，14% 为 0.5–2 KiB，2% 为 4–16 KiB；共 160 个槽。分别用内部 96 KiB 池和 PSRAM 2 MiB 池，取较大值 | SRAM / PSRAM | 次（一次 malloc 或一次 free） |

稳态区段按理想 I-cache 计（热代码）。这样结果不受本测试程序偶然的代码布局影响：一个装得进 16 KiB I-cache 的热循环，仍可能因为几段代码恰好落在同一组而在两路组内互相挤出。在一次 Zb/Zc 构建里，`json.byte` 就因此多出 13.6 周期/字节。对 -O2 主构建，打开真实 I-cache 时各稳态区段只差 0.1%～1%（report.md §5）。flash 取指的代价由 `#cold` 区段给出参考，在系统模型里应通过 `icache.miss` 单独计。

## 2 结果

### 2.1 现有成本类

周期/单位，320 MHz【仿】。
- 建议值 = raw×1.3 向上取整（预期情形）；
- 悲观值 = max(raw×1.3, 慢角) 向上取整；
- 现猜测取自 `profiles/metalio-plus.json`。

| 成本类 | 单位 | raw | ×1.3 | 建议值 | 悲观值 | 现猜测 | 建议/现猜测 | 理想 | 慢角 | -Os | Zb/Zc |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `sram.copy_byte` | 字节 | 0.641 | 0.833 | 0.84 | 1.15 | 0.9 | 0.93 | 0.641 | 1.141 | 0.641 | 0.641 |
| `psram_to_internal.copy_byte` | 字节 | 2.903 | 3.774 | 3.78 | 5.64 | 5.8 | 0.65 | 2.141 | 5.635 | 2.903 | 2.903 |
| `internal_to_psram.copy_byte` | 字节 | 3.899 | 5.069 | 5.07 | 9.49 | 10.0 | 0.51 | 3.137 | 9.486 | 3.899 | 3.899 |
| `psram.copy_byte` | 字节 | 6.327 | 8.225 | 8.23 | 14.7 | 15.2 | 0.54 | 4.802 | 14.62 | 6.327 | 6.327 |
| `glyph.blit` | 字形 | 8361 | 10869 | 10870 | 13995 | 9000 | 1.21 | 6728 | 13994 | 8617 | 8362 |
| `epd.convert_px` | 像素 | 5.507 | 7.159 | 7.16 | 7.16 | 6 | 1.19 | 5.505 | 7.008 | 5.508 | 5.507 |
| `text.break_char` | 字 | 75.43 | 98.06 | 98.1 | 98.1 | 400 | 0.25 | 63.34 | 88.25 | 134.2 | 69.95 |
| `text.layout_char` | 字 | 128.0 | 166.4 | 167 | 182 | 1500 | 0.11 | 112.9 | 181.1 | 152.8 | 122.6 |
| `json.byte` | 字节 | 57.83 | 75.18 | 75.2 | 78.9 | 60 | 1.25 | 50.81 | 78.81 | 74.41 | 50.58 |
| `zlib.inflate_byte` | 输出字节 | 33.79 | 43.92 | 44.0 | 45.7 | 40 | 1.10 | 28.80 | 45.65 | 33.69 | 32.20 |
| `crc32.byte` | 字节 | 13.26 | 17.24 | 17.3 | 17.5 | 8 | 2.16 | 11.50 | 17.49 | 13.26 | 12.26 |
| `heap.op` | 次 | 235.2 | 305.7 | 306 | 312 | 1500 | 0.20 | 213.2 | 311.2 | 372.8 | 186.9 |
| `lvgl.px` | 像素 | — | — | — | — | 25 | — | 未测（没有运行 LVGL；参考 `fill.px`、`glyph.px`） | | | |
| `littlefs.op` | 次 | — | — | — | — | 60000 | — | 未测（由 flash 时序主导，已在 `hal_storage_sim` 建模） | | | |
| `icache.miss` | 次 | — | — | — | — | 1150 | — | 模型输入，不是测量值（`flash.fill`） | | | |

### 2.2 建议新增的成本类

| 成本类 | 单位 | raw | ×1.3 | 建议值 | 悲观值 | 理想 | 慢角 | -Os | Zb/Zc | 用途 |
|---|---|---|---|---|---|---|---|---|---|---|
| `glyph.px` | 字形格像素 | 14.97 | 19.46 | 19.5 | 25.1 | 12.05 | 25.06 | 15.43 | 14.97 | 字号不是 24 px 时，按像素数计字形混合 |
| `fill.px` | 像素 | 3.933 | 5.113 | 5.12 | 9.99 | 3.124 | 9.985 | 3.937 | 3.931 | 往 PSRAM 帧缓冲填充矩形 |
| `psram.set_byte` | 字节 | 3.524 | 4.582 | 4.59 | 8.87 | 2.762 | 8.860 | 3.524 | 3.524 | `osal_calloc` 目前按 0.5×`psram.copy_byte` 计；测得 memset 与 copy 之比为 0.56，可直接改用本类 |
| `sram.set_byte` | 字节 | 0.267 | 0.347 | 0.35 | 0.52 | 0.266 | 0.517 | 0.267 | 0.267 | 同上，内部 SRAM |

### 2.3 数据放 PSRAM 与放 SRAM 的对照（拟合模型，raw）【仿】

| 内核 | 数据在 PSRAM | 数据在 SRAM | 比值 | 说明 |
|---|---|---|---|---|
| `glyph.blit`（周期/字形） | 8361 | 6130 | 1.36 | 每个字形约 13.9 次 PSRAM 缺失、8.0 次写回；SRAM 版只剩读字形缓存的缺失（3.0 次） |
| `fill.px` | 3.933 | 0.340 | 11.6 | memset 本身每像素约 0.3 条指令，PSRAM 下几乎全是缺失与写回 |
| `epd.convert_px` | 9.520 | 5.507 | 1.73 | 每像素 5.5 条指令（字节循环） |
| `crc32.byte` | 13.26 | 11.00 | 1.21 | 每字节 10 条指令，表在 SRAM |
| `heap.op`（TLSF） | 235.2 | 228.0 | 1.03 | 元数据访问局部性好 |
| memcpy 矩阵 | P→S 2.903，S→P 3.899，P→P 6.327 | S→S 0.641 | — | P→P 4 KiB 且已在 cache 中时为 0.675 |
| memset | 3.524 | 0.267 | 13.2 | — |

### 2.4 冷启动（`#cold`，两个 cache 都从空开始，代码经 flash XIP 取指）【仿】

| 区段 | 冷启动 | 稳态 | flash I-cache 缺失 |
|---|---|---|---|
| `json.byte#cold`（14 KB 一次） | 75.4 | 57.8（+30%） | 117 次 |
| `text.layout_char#cold`（约 1500 字） | 171.4 | 128.0（+34%） | 25 次 |
| `glyph.blit@psram#cold`（116 个字形） | 8128 | 8361 | 11 次 |

冷启动的 `glyph.blit` 样本只有首屏 4 行，字形组合与整页不同，数值不可直接比较。

### 2.5 逐类解读

- **PSRAM 拷贝类。**
  - 现值按 S3 实测定，那时 PSRAM 为 80 MHz；S31 是 200 MHz。
  - 拟合值按与 P4 相当来定，约为现值的 0.5～0.65；慢角（悲观值）约等于现值。
  - 这三类直接影响 `epd.submit_copy`、`osal_realloc`、SD 读入等的时间估计。
- **`sram.copy_byte`**：0.641 → 建议 0.84，与现值 0.9 接近。悲观值 1.15 来自"每次 SRAM 访问多 1 周期"的慢角，这与 P4 实测的约 1 周期/字节（P4 的 SRAM 在 L1 之后）量级相同。
- **`glyph.blit`**：
  - raw 8361 周期/字形，其中约 2700 周期是 PSRAM 缺失与写回，4761 条指令（CPI 1.76），每像素 15.0 周期。
  - 建议值 10870，比现值高 21%；悲观值 13995。
  - 一页 1164 个字形，按建议值约 39.5 ms（12.6 M 周期），按悲观值约 51 ms。
- **`epd.convert_px`**：
  - 字节循环每像素 5.5 条指令，CPI 1.0；建议 7.16，现值 6。
  - 若驱动改用 32 位字、一次处理 8 像素，可望降到约 2 周期/像素【推，未测】。
- **`text.*`**：
  - 内核是最小实现，每字约 63 条指令（断行）和 100 条（排版）。
  - 真实排版若经 LVGL 的 `lv_text`/`lv_font` 接口逐字查字体（函数指针、cmap 二分查找、字距），每字开销会高出数倍，**未测**。
  - -Os 下断行慢 78%（134 周期/字），因为 `utf8_next`、`char_width` 这些小函数不再内联。
- **`json.byte`**：58 → 建议 75.2，现值 60 偏乐观。-Os 下为 74.4，乘 1.3 后约 97。
- **`zlib.inflate_byte`**：miniz 34 → 建议 44；tinf 为 152，是 miniz 的 4.5 倍。选用哪个 inflater 关系很大，设备上应优先用 ROM 里的 miniz。
- **`crc32.byte`**：
  - 13.3 → 17.3。逐字节查表每字节约 10 条指令，再加 load-use 停顿，现值 8 偏乐观。
  - 要更快，需要 slicing-by-4/8，或 ROM/硬件 CRC。ROM 里 `esp_rom_crc32_le` 的实现**未核实**。
- **`heap.op`**：
  - 裸 TLSF 每次 malloc 或 free 约 235 周期；-Os 下 373 周期。
  - IDF 的 `heap_caps_malloc` 还要按 caps 选堆、加锁（多核自旋锁），可能还有毒化或跟踪。这部分额外开销**未测**，【估】在数百周期量级。

### 2.6 墨水屏送行的行准备（2026-11-02，第二次独立评审后补测）【仿】

评审把上游 S31 的送行热路径（`epdiy_riscv_ve_stub.c` 的 `calc_epd_input_1ppB_1k_S3_VE_aligned` + `epd_apply_line_mask_VE`，文件自述是“原 Xtensa SIMD 的等价 C 实现”）逐字搬进内核，在本 ISS 上测；本轮复测得到完全相同的数字，并加测了一个优化版。条件：每行 1216 像素，差分图每像素 1 字节放 PSRAM，LUT、掩码、行队列放内部 SRAM，隔行取（一个送行任务的工作方式），`-O2`、IDF 的 S31 编译选项，I-cache 视为命中。

| 内核 | 拟合模型 周期/行 | 微秒/行（320 MHz） | 慢角 周期/行 | 指令/行 |
|---|---|---|---|---|
| 上游 C（逐字） | 10 680 | 33.4 | 15 451（48.3 µs） | 7 903 |
| 优化 C：四张预移位 256 字节表（`T0[b0]|T1[b1]|T2[b2]|T3[b3]`），掩码并入同一遍，一次写一个字 | 9 253 | 28.9 | 13 662（42.7 µs） | 6 500 |
| 区域外的空操作行（清零 304 字节） | 116 | 0.4 | 196 | 111 |

两条交叉验证：

1. 每行约 2 750 周期是 PSRAM 读入（每行 19 次行缺失），优化只能压指令部分；没有预取（上游在 S31 上关掉了 `Cache_Start_DCache_Preload`）。
2. 上游 `render_lcd.c` 的注释说“64 行预填在并发音频时欠载”，于是改成 192 行。按固定奇偶分行、0 核扣 10% 中断计算：64 行预填不出错要求每行 ≤约 33.7 µs，再加约 5% 音频负载就出错要求每行 >约 32.1 µs；拟合模型的 33.4 µs 正落在这个区间里。因此送行成本取拟合值 × 1.1（端口 31.8 µs，上游复现 33.4 µs），而不是通用的 × 1.3；慢角作为风险界写进 [平台与模拟器 §5.4](../../system/platform-and-simulator.md)。

S31 的 XespV 向量扩展 ISS 不建模；若能用于查表，这一项还能大幅下降，需到货后核实。

## 3 建议写入 profile 的值（2026-11-02 已按本节写入，`text.*`/`heap.op` 除外）

两组值都由 `calibrate.sh` 算出，在 `costs.json` 的 `proposed_costs_cycles`（预期）与 `proposed_costs_cycles_pessimistic`（悲观）里。

**预期情形**（建议用于 `metalio-plus-psram16.json`，即"expected case"）：

```json
"costs_cycles": {
  "glyph.blit": 10870, "epd.convert_px": 7.16, "json.byte": 75.2, "zlib.inflate_byte": 44.0,
  "crc32.byte": 17.3, "psram.copy_byte": 8.23, "psram_to_internal.copy_byte": 3.78,
  "internal_to_psram.copy_byte": 5.07, "sram.copy_byte": 0.84,
  "glyph.px": 19.5, "fill.px": 5.12, "psram.set_byte": 4.59, "sram.set_byte": 0.35
}
```

**悲观情形**（建议用于 `metalio-plus.json`，它是默认的悲观 profile）：

```json
"costs_cycles": {
  "glyph.blit": 13995, "epd.convert_px": 7.16, "json.byte": 78.9, "zlib.inflate_byte": 45.7,
  "crc32.byte": 17.5, "psram.copy_byte": 14.7, "psram_to_internal.copy_byte": 5.64,
  "internal_to_psram.copy_byte": 9.49, "sram.copy_byte": 1.15,
  "glyph.px": 25.1, "fill.px": 9.99, "psram.set_byte": 8.87, "sram.set_byte": 0.52
}
```

两组都不变的键：
- `text.break_char`、`text.layout_char`、`heap.op`：内核只覆盖最小实现。
  - 若排版自研、精简，可分别用 98/167（预期）和 98/182（悲观）。
  - 若用 LVGL 的文本 API，建议保留现值 400/1500，直到测到为止。
  - `heap.op` 建议取 306 + IDF 包装开销（估计数百周期），即 700 左右；或者保留 1500，直到在 IDF 下实测。
- `lvgl.px`、`littlefs.op`：未测，保留现值。
- `icache.miss`：保持 1150。它是 ISS 的输入，不是测量值。

**配套修改建议（交根代理决定）**
- `osal_calloc` 的 `0.5 × psram.copy_byte` 改用 `psram.set_byte`。
- `pinenote-rk3566.json`、`desktop-lcd.json` 不是本工具的目标平台，不要套用这些值。

## 4 局限

- **锚点不可达。** CoreMark 只约束内核 CPI 的总量，分不清各项延迟各占多少。这里用参数盒加理想/慢角来覆盖，但核心并非单发射等结构性差异无法排除。
- **未建模的硬件：**
  - PIE/XespV SIMD（只在一个核上）和 XespLoop 硬件循环；
  - 双核争用：共享 D-cache、PSRAM 带宽；DMA 争用时 CPU 读 PSRAM 带宽下降，S3 实测为 −46%【测】，EPD 扫描期间尤其如此；
  - 中断与上下文切换；
  - cache 的 autoload 预取、关键字优先、写缓冲；
  - ROM 代码经"ROM cache"执行的路径；
  - flash 擦写冻结（已有单独模型）。
- **存储器模型假设：**
  - 写回 + 写分配、缺失阻塞，均偏慢；
  - D-cache 命中与 SRAM 同为单周期；
  - flash 缺失 1150 周期按上游的 DIO 80 MHz 计，换成 QIO 或 120 MHz 会快一倍左右；
  - PSRAM 的参数来自 P4/S3 的类比，不是 S31 实测。
- **分支预测器**的结构与大小是类比 U54 的假设（直接映射 28 项 BTB、512 项 BHT、6 项 RAS）。
- **I-cache。** 稳态值假定代码是热的。真实固件里 16 KiB I-cache 被多个任务分享，额外的缺失应按 `icache.miss` 单独计；系统模拟器目前还没有地方计这一项。
- **软件差异：**
  - GCC 15.2 可能不是乐鑫测 CoreMark 时用的编译器；
  - 设备上 memcpy 等函数在 ROM，默认 libc 是 picolibc，本工具用的是工具链 newlib。两者都是字循环，量级相同，但细节不同；
  - 内核用合成数据（文本、字形、JSON），代表性有限；
  - `text.*` 不含字体引擎和回退字体；`heap.op` 不含 IDF 的锁开销；
  - `epd.convert_px` 用的是朴素字节循环。
- **未测的类：** `lvgl.px`、`littlefs.op`、Opus 解码、TLS 握手、PNG 解码。

## 5 如何重跑

```bash
bash tools/iss/calibrate.sh            # 约 30 s；产物在 tools/iss/out/
cmake -S . -B build && cmake --build build -j && ctest --test-dir build   # 含 iss-selftest
```

- **需要**：`riscv32-esp-elf-gcc`（ESP-IDF 工具链；用 `RV_GCC=` 指定）、`git` 且能访问 GitHub、`python3`、主机 C 编译器。CoreMark、tinf、miniz 按固定提交拉取到 `tools/iss/build/src/`，该目录已被 git 忽略，**不进仓库**。
- **拿到实机数据以后**（对应 performance-modeling.md §5 的"到货第一周基准"）：
  - 单核 CoreMark：把 `scripts/calib.py` 里的 `ANCHOR` 换成实测的 CoreMark/MHz。若实测落在参数盒内，拟合会自动选中对应组合，残差也会变小；若低于 2.88，需要放宽参数盒。
  - memcpy 矩阵：把 `P4_RATES` 换成 S31 实测的 P→S、S→P、P→P 速率，PSRAM 的行填充和写回时间会随之重新拟合。
  - 然后重跑 `calibrate.sh`，再按 §3 的方式更新 profile。验收仍用 performance-modeling.md 的单边标准。
- **改了内核或模型以后**：`out/` 下的三个文件随代码一起提交，审阅时直接看 diff。

## 来源

**ESP32-S31 / P4 官方资料**
- ESP32-S31 数据手册 v0.5（预发布）：https://documentation.espressif.com/esp32-s31_datasheet_en.pdf ——5 级流水线、BHT/BTB/RAS、CoreMark 2195.20（双核，320 MHz）、RV32IMAFC + Zc + Zb、XespV/XespLoop、L1 cache、250 MHz 八线 DDR PSRAM
- ESP32-P4 数据手册：https://documentation.espressif.com/esp32-p4_datasheet_en.html ——5 级流水线、BHT/BTB/RAS、双核 6.92 CoreMark/MHz
- ESP32-P4 TRM：https://documentation.espressif.com/esp32-p4_technical_reference_manual_en.pdf ——只读到目录（§2.12 Branch Prediction/RAS），正文没取到

**ESP-IDF v6.1（提交 `fff9895c`）源码**
- `components/soc/project_include.cmake`（-march）
- `components/soc/esp32s31/include/soc/soc_caps.h`、`soc.h`
- `components/esp_system/port/soc/esp32s31/Kconfig.cache`
- `components/esp_rom/esp32s31/include/esp32s31/rom/cache.h`
- `components/hal/esp32s31/include/hal/cache_ll.h`
- `components/esp_psram/device/esp_psram_impl_ap_oct.c`、`components/esp_psram/esp32s31/Kconfig.spiram`
- `components/esp_rom/esp32s31/ld/esp32s31.rom.libc.ld`、`esp32s31.rom.ld`
- `components/esp_libc/Kconfig`
- 仓库：https://github.com/espressif/esp-idf/tree/v6.1

**乐鑫 CoreMark 组件与编译器**
- CoreMark 组件：https://github.com/espressif/idf-extra-components/tree/master/coremark ——`CMakeLists.txt`、`linker.lf`、`port/core_portme.h.in`
- 乐鑫 GCC 的 esp-base 调优表：https://github.com/espressif/gcc/blob/esp-15.2.0_20251204/gcc/config/riscv/riscv.cc ——`esp_base_tune_info`，注释 "Copied from rocket"

**类比核心（SiFive E51/U54，PolarFire SoC）**
- PolarFire SoC MSS 技术参考手册：https://www.microchip.com/content/dam/mchp/documents/FPGA/ProductDocuments/ReferenceManuals/PolarFire_SoC_FPGA_MSS_Technical_Reference_Manual_VC.pdf ；本次读的是镜像 https://web.pa.msu.edu/people/edmunds/Disco_Kraken/MPFS250T_FPGA/polarFire_MSS_technical_reference_manual_vc_svd_7mar23.pdf ——5 级单发射顺序、全旁路、字/字节访问 2/3 周期、CSR 写冲刷 5 周期、28 项 BTB、512 项 BHT、6 项 RAS、预测正确无惩罚、误预测 3 周期
- 分支预测一节：https://onlinedocs.microchip.com/pr/GUID-0E320577-28E6-4365-9BB8-9E1416A0A6E4-en-US-6/GUID-570D5971-D2FD-47A1-9DF9-6F410542D066.html

**PSRAM 实测**
- ESP32-P4 memcpy 与 PIE：https://github.com/espressif/developer-portal/discussions/353
- ESP32-S3 memcpy：https://github.com/project-x51/esp32-s3-memorycopy （数值转引自 performance-modeling.md）

**上游固件**
- CloudZao/Metalio-E-INK4-Plus：https://github.com/CloudZao/Metalio-E-INK4-Plus ——`sdkconfig.defaults`（`CONFIG_COMPILER_OPTIMIZATION_SIZE=y`）、`sdkconfig.defaults.esp32s31`（八线 PSRAM 200M），flash 为 DIO 80 MHz（转引自 performance-modeling.md）

**运行时拉取的第三方代码**
- CoreMark：https://github.com/eembc/coremark @ `1f483d5`（Apache-2.0 + EEMBC 条款）
- tinf：https://github.com/jibsen/tinf @ `57ffa1f`（v1.2.1，zlib 许可）
- miniz：https://github.com/richgel999/miniz @ `77d0dce`（3.1.2，MIT）
