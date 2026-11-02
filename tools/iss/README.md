# tools/iss：指令级校准（Level 2）

用一个确定性的 RV32 指令集模拟器（ISS）跑真实的 RISC-V 代码，量出 ESP32-S31 上各成本类的"每单位周期数"，给主机模拟器 `profiles/*.json` 的 `costs_cycles` 提供有依据的数值。方法、参数依据、结果与局限见 [docs/research/simulator/iss-calibration.md](../../docs/research/simulator/iss-calibration.md)。

## 一条命令

```bash
bash tools/iss/calibrate.sh
```

约 30 秒。产物：

| 文件 | 内容 |
|---|---|
| `out/costs.json` | 每个成本类的 raw（拟合模型）、×1.3、建议值、ideal/slow 角点、-Os 与 Zb/Zc 敏感性，以及全部测量区段 |
| `out/report.md` | 同一批数据的中文表格（CoreMark 拟合网格、PSRAM 拟合、各区段明细） |
| `out/esp32s31.fitted.cfg` | 拟合后的完整时序参数（可直接给 `iss -c` 用） |

两次运行（包括删掉 `build/` 重新拉取、重新编译）产物逐字节相同。

需要：`riscv32-esp-elf-gcc`（ESP-IDF 工具链；用 `RV_GCC=` 指定，否则在 PATH、`~/.espressif/tools` 里找）、`git` 且能访问 GitHub、`python3`、主机 C 编译器。CoreMark、tinf、miniz 在运行时按固定提交拉到 `build/src/`，**不进仓库**。

## 目录

| 路径 | 作用 |
|---|---|
| `src/` | ISS 本体（C11，约 2200 行）：`iss.c` 译码执行与时序、`iss_cfg.c` 参数、`iss_elf.c` ELF 加载与按函数统计、`main.c` 命令行与 JSON 报告 |
| `timing/esp32s31.cfg` | 时序模型的**基础假设**（带出处注释）；拟合只改其中标明 "fitted" 的几项和 PSRAM 时间 |
| `rt/` | 目标端运行时：`crt0.S`、链接脚本（`xip.ld` 代码在 flash，`iram.ld` 全在内部 SRAM）、newlib 桩、度量接口 `iss_rt.h` |
| `coremark/` | CoreMark 的 ISS 移植层（`core_portme.*`），源码运行时拉取 |
| `kernels/` | 校准内核与 ISA 自检程序；`kernels/data/` 是固定的样本输入及其生成脚本 |
| `scripts/calib.py` | CoreMark 拟合、PSRAM 拟合、跑内核、写 `out/` |
| `test/iss_selftest.c` | 手工编码的小程序，检查结果与**精确周期数**；CTest `iss-selftest`，不需要交叉编译器和网络 |

## 单独使用 ISS

```bash
cmake -S . -B build && cmake --build build --target iss     # 或 calibrate.sh 生成的 tools/iss/build/iss
build/iss -c tools/iss/out/esp32s31.fitted.cfg -o report.json -p 20 prog.elf
build/iss --keys            # 全部时序参数、默认值和含义
```

`-s key=value` 覆盖单个参数，`-p N` 打印耗时最多的 N 个函数。被测程序用 `rt/iss_rt.h`：

```c
iss_begin("glyph.blit@psram");   /* 区段开始（本身不计时） */
... 被测代码 ...
iss_cache_drain();               /* 可选：把脏行写回的时间也算进区段 */
iss_end2(glyphs, pixels);        /* 单位数 + 可选的第二单位数 */
```

内存映射照 ESP-IDF 的 S31 `soc.h`：内部 SRAM `0x2F000000`（512 KiB，不经 cache）、flash `0x40000000`（经 I/D cache）、PSRAM `0x50000000`（经 D cache）。`iss_psram_alloc()`/`iss_sram_alloc()` 是两个简单的顺序分配器。

## 模型要点

单发射顺序流水线：寄存器记分牌给出 RAW 停顿（按产生者类别归账）；除法、浮点除/开方、CSR 写、Zcmp push/pop 按占用周期阻塞；BHT（2 位）+ 直接映射 BTB + RAS；每条 32 位指令跨行时两次取指。I-cache 每核私有、D-cache 共享，组相联 LRU、写回 + 写分配、缺失阻塞、无预取、无关键字优先。时间用 1/256 周期的定点数，拟合出的小数惩罚也逐位确定。不建模：特权级、中断、MMU、PIE/XespV、XespLoop、双核争用（详见设计文档 §4）。

## 来源与许可

- ISS、运行时、内核、脚本：本项目自写，未基于 mini-rv32ima 等现有模拟器。
- CoreMark（EEMBC，Apache-2.0 加使用限制）：运行时从 github.com/eembc/coremark `1f483d5` 拉取；`coremark/core_portme.*` 按其 barebones 模板与乐鑫 idf-extra-components 的移植改写，保留 EEMBC 版权说明。
- tinf（Jørgen Ibsen，zlib 许可）：运行时拉取 github.com/jibsen/tinf `57ffa1f`（v1.2.1）。
- miniz（Rich Geldreich 等，MIT）：运行时拉取 github.com/richgel999/miniz `77d0dce`（3.1.2），只编译 `miniz_tinfl.c`。
- cJSON（MIT）与 TLSF（BSD）：用仓库内 `third_party/` 的副本。
- `kernels/data/` 的文本与 JSON 由 `gen_data.py` 用固定种子生成（无版权内容）；zlib 流已提交，结果不依赖主机的 zlib 版本。
- `kernels/k_feed.c`（墨水屏送行的单独测量，不在 `calibrate.sh` 里）含两段上游 MIT 代码（CloudZao/Metalio-E-INK4-Plus 的 `epdiy_riscv_ve_stub.c`），文件头注明版权与许可。
