# ISS 校准报告（自动生成）

由 `tools/iss/calibrate.sh` 生成，请勿手改；方法与解读见 `docs/research/simulator/iss-calibration.md`。
单位：ESP32-S31 内核周期（320 MHz）。raw = 拟合模型、内核 -O2、热代码（理想 I-cache，flash 取指另由 icache.miss 计）；×1.3 = 校准前的悲观系数；建议值 = ×1.3 后向上取整。

## 1. CoreMark 拟合

锚点 3.43 CoreMark/MHz（Espressif material (ESP32-S31 datasheet v0.5: CoreMark 2195.20 dual-core @320 MHz = 6.86/MHz, assumed linear per core), unverified on our board）。

可信参数盒内的全部组合（CoreMark 用乐鑫组件的编译选项 -O3 -fjump-tables -ftree-switch-conversion，代码与数据放内部 SRAM）：

| lat.load_sub | lat.mul | br.mispredict | br.jal_nobtb | CoreMark/MHz | 残差 | CPI | 指令/迭代 |
|---|---|---|---|---|---|---|---|
| 2 | 1 | 2 | 1 | 3.083 | -10.1% | 1.110 | 292217 |
| 2 | 1 | 2 | 3 | 3.072 | -10.4% | 1.114 | 292217 |
| 2 | 2 | 2 | 1 | 3.049 | -11.1% | 1.122 | 292217 |
| 2 | 2 | 2 | 3 | 3.038 | -11.4% | 1.126 | 292217 |
| 2 | 1 | 3 | 1 | 3.031 | -11.6% | 1.129 | 292217 |
| 2 | 1 | 3 | 3 | 3.020 | -12.0% | 1.133 | 292217 |
| 2 | 2 | 3 | 1 | 2.998 | -12.6% | 1.141 | 292217 |
| 2 | 2 | 3 | 3 | 2.988 | -12.9% | 1.145 | 292217 |
| 3 | 1 | 2 | 1 | 2.971 | -13.4% | 1.152 | 292217 |
| 3 | 1 | 2 | 3 | 2.961 | -13.7% | 1.156 | 292217 |
| 3 | 2 | 2 | 1 | 2.940 | -14.3% | 1.164 | 292217 |
| 3 | 2 | 2 | 3 | 2.930 | -14.6% | 1.168 | 292217 |
| 3 | 1 | 3 | 1 | 2.922 | -14.8% | 1.171 | 292217 |
| 3 | 1 | 3 | 3 | 2.912 | -15.1% | 1.175 | 292217 |
| 3 | 2 | 3 | 1 | 2.892 | -15.7% | 1.183 | 292217 |
| 3 | 2 | 3 | 3 | 2.882 | -16.0% | 1.187 | 292217 |

选中：br.jal_nobtb=1, br.mispredict=2, lat.load_sub=2, lat.mul=1 → **3.083 CoreMark/MHz，残差 -10.1%**（偏慢一侧）。

| 构建 / 角点 | CoreMark/MHz | 残差 | CPI | 指令/迭代 |
|---|---|---|---|---|
| idf/fitted | 3.083 | -10.1% | 1.110 | 292217 |
| idf/ideal | 3.422 | -0.2% | 1.000 | 292217 |
| idf/slow | 2.409 | -29.8% | 1.421 | 292217 |
| zbzc/fitted | 3.123 | -8.9% | 1.115 | 287096 |
| zbzc/ideal | 3.471 | +1.2% | 1.004 | 287096 |
| zbzc/slow | 2.429 | -29.2% | 1.434 | 287096 |

idf = IDF 对 S31 的 -march（rv32imafc + zaamo/zalrsc，去掉 GCC 不会生成的 xesploop/xespv）；zbzc = 另加数据手册列出的 Zba/Zbb/Zbs/Zcb/Zcmp。ideal = 理想单发射流水线（无任何停顿）；slow = SiFive U54 类比参数 + 内部 SRAM 访问与 D-cache 命中各多 1 周期。

## 2. PSRAM 拟合

| 角点 | 行填充（周期） | 写回（周期） | 目标 P→S / S→P / P→P（MB/s） | ISS 结果 P→S / S→P / P→P（MB/s） |
|---|---|---|---|---|
| 拟合（ESP32-P4 同等） | 144.8 | 85.0 | 110.3 / 89.0 / 53.4 | 110.2 / 82.1 / 50.6 |
| 慢角（ESP32-S3 同等） | 287.6 | 328.6 | 56.8 / 32.5 / 21.2 | 56.8 / 33.7 / 21.9 |
| 理论（IDF 时序，ideal 角点用） | 96.0 | 85.0 | — | 149.5 / 102.0 / 66.6 |

## 3. 成本类（profile 现有键）

建议值 = raw×1.3 向上取整（预期情形）；悲观值 = max(raw×1.3, slow) 向上取整（同时覆盖慢角）。

| 成本类 | 单位 | raw | ×1.3 | 建议值 | 悲观值 | 现猜测 | 建议/猜测 | ideal | slow | -Os | Zb/Zc |
|---|---|---|---|---|---|---|---|---|---|---|---|
| sram.copy_byte | byte | 0.641 | 0.833 | 0.840 | 1.150 | 0.900 | 0.93 | 0.641 | 1.141 | 0.641 | 0.641 |
| psram_to_internal.copy_byte | byte | 2.903 | 3.774 | 3.780 | 5.640 | 5.800 | 0.65 | 2.141 | 5.635 | 2.903 | 2.903 |
| internal_to_psram.copy_byte | byte | 3.899 | 5.069 | 5.070 | 9.490 | 10.00 | 0.51 | 3.137 | 9.486 | 3.899 | 3.899 |
| psram.copy_byte | byte | 6.327 | 8.225 | 8.230 | 14.70 | 15.20 | 0.54 | 4.802 | 14.62 | 6.327 | 6.327 |
| glyph.blit | glyph | 8361 | 10869 | 10870 | 13995 | 9000 | 1.21 | 6728 | 13994 | 8617 | 8362 |
| epd.convert_px | px | 5.507 | 7.159 | 7.160 | 7.160 | 6 | 1.19 | 5.505 | 7.008 | 5.508 | 5.507 |
| text.break_char | char | 75.43 | 98.06 | 98.10 | 98.10 | 400 | 0.24 | 63.34 | 88.25 | 134.2 | 69.95 |
| text.layout_char | char | 128.0 | 166.4 | 167 | 182 | 1500 | 0.11 | 112.9 | 181.1 | 152.8 | 122.6 |
| json.byte | byte | 57.83 | 75.18 | 75.20 | 78.90 | 60 | 1.25 | 50.81 | 78.81 | 74.41 | 50.58 |
| zlib.inflate_byte | out byte | 33.79 | 43.92 | 44.00 | 45.70 | 40 | 1.10 | 28.80 | 45.65 | 33.69 | 32.20 |
| crc32.byte | byte | 13.26 | 17.24 | 17.30 | 17.50 | 8 | 2.16 | 11.50 | 17.49 | 13.26 | 12.26 |
| heap.op | op | 235.2 | 305.7 | 306 | 312 | 1500 | 0.20 | 213.2 | 311.2 | 372.8 | 186.9 |
| lvgl.px | — | — | — | — | — | — | — | — | — | — | — |
| littlefs.op | — | — | — | — | — | — | — | — | — | — | — |
| icache.miss | — | — | — | — | — | — | — | — | — | — | — |

- `lvgl.px`：not measured (LVGL itself is not run); proxies: fill.px, glyph.px
- `littlefs.op`：not measured (dominated by flash timing, modeled in hal_storage_sim)
- `icache.miss`：model input, not a measurement: flash.fill (DIO 80 MHz)

## 4. 建议新增的成本类

| 成本类 | 单位 | raw | ×1.3 | 建议值 | 悲观值 | ideal | slow | -Os | Zb/Zc |
|---|---|---|---|---|---|---|---|---|---|
| glyph.px | px | 14.97 | 19.46 | 19.50 | 25.10 | 12.05 | 25.06 | 15.43 | 14.97 |
| fill.px | px | 3.933 | 5.113 | 5.120 | 9.990 | 3.124 | 9.985 | 3.937 | 3.931 |
| psram.set_byte | byte | 3.524 | 4.582 | 4.590 | 8.870 | 2.762 | 8.860 | 3.524 | 3.524 |
| sram.set_byte | byte | 0.267 | 0.347 | 0.350 | 0.520 | 0.266 | 0.517 | 0.267 | 0.267 |

## 5. 全部测量区段（拟合模型，-O2）

| 区段 | 单位数 | 周期/单位 | 指令/单位 | CPI | PSRAM 缺失/单位 | PSRAM 写回/单位 | 真实 I-cache 时 | flash I-miss | ideal | slow |
|---|---|---|---|---|---|---|---|---|---|---|
| crc32.byte@psram | 262144 | 13.26 | 10.00 | 1.33 | 0.016 | 0.000 | 13.27 | 1 | 11.50 | 17.49 |
| crc32.byte@sram | 32768 | 11.00 | 10.00 | 1.10 | 0.000 | 0.000 | 11.00 | 0 | 10.00 | 13.00 |
| epd.convert_px@psram | 831744 | 9.520 | 5.505 | 1.73 | 0.023 | 0.008 | 9.522 | 1 | 8.375 | 16.14 |
| epd.convert_px@sram | 155648 | 5.507 | 5.505 | 1.00 | 0.000 | 0.000 | 5.514 | 1 | 5.505 | 7.008 |
| fill.px@psram | 486192 | 3.933 | 0.331 | 11.87 | 0.016 | 0.016 | 3.935 | 1 | 3.124 | 9.985 |
| fill.px@sram | 153216 | 0.340 | 0.316 | 1.08 | 0.000 | 0.000 | 0.340 | 0 | 0.331 | 0.596 |
| glyph.blit@psram | 1164 | 8361 | 4761 | 1.76 | 13.88 | 7.970 | 8362 | 1 | 6728 | 13994 |
| glyph.blit@psram#cold | 116 | 8128 | 4730 | 1.72 | 14.28 | 7.957 | 8128 | 11 | — | — |
| glyph.blit@sram | 1160 | 6130 | 4747 | 1.29 | 2.978 | 0.000 | 6131 | 1 | 5033 | 8379 |
| internal_to_psram.copy_byte | 262144 | 3.899 | 0.641 | 6.09 | 0.016 | 0.016 | 3.904 | 1 | 3.137 | 9.486 |
| json.byte | 13882 | 57.83 | 50.05 | 1.16 | 0.003 | 0.001 | 57.91 | 1 | 50.81 | 78.81 |
| json.byte#cold | 13882 | 75.36 | 50.05 | 1.51 | 0.052 | 0.000 | 75.36 | 117 | — | — |
| json.byte@parse_only | 13882 | 47.01 | 40.37 | 1.17 | 0.002 | 0.001 | 47.01 | 0 | 41.03 | 64.25 |
| psram.copy_byte | 262144 | 6.327 | 0.639 | 9.89 | 0.031 | 0.016 | 6.331 | 1 | 4.802 | 14.62 |
| psram.copy_byte@cached4k | 32768 | 0.675 | 0.674 | 1.00 | 0.000 | 0.000 | 0.710 | 1 | 0.674 | 1.186 |
| psram.set_byte | 262144 | 3.524 | 0.266 | 13.26 | 0.016 | 0.016 | 3.529 | 1 | 2.762 | 8.860 |
| psram_to_internal.copy_byte | 262144 | 2.903 | 0.641 | 4.53 | 0.016 | 0.000 | 2.908 | 1 | 2.141 | 5.635 |
| sram.copy_byte | 131072 | 0.641 | 0.641 | 1.00 | 0.000 | 0.000 | 0.650 | 1 | 0.641 | 1.141 |
| sram.set_byte | 131072 | 0.267 | 0.266 | 1.00 | 0.000 | 0.000 | 0.275 | 1 | 0.266 | 0.517 |
| text.break_char | 6025 | 75.43 | 63.34 | 1.19 | 0.000 | 0.000 | 75.43 | 0 | 63.34 | 88.25 |
| text.layout_char | 6025 | 128.0 | 100.6 | 1.27 | 0.091 | 0.041 | 128.0 | 0 | 112.9 | 181.1 |
| text.layout_char#cold | 1514 | 171.4 | 100.5 | 1.71 | 0.285 | 0.000 | 171.4 | 25 | — | — |
| tlsf.op@psram | 20000 | 235.2 | 209.5 | 1.12 | 0.026 | 0.011 | 235.2 | 0 | 213.2 | 311.2 |
| tlsf.op@sram | 20000 | 228.0 | 207.7 | 1.10 | 0.000 | 0.000 | 228.0 | 0 | 207.7 | 296.7 |
| zlib.inflate_byte | 16971 | 33.79 | 26.16 | 1.29 | 0.024 | 0.016 | 33.85 | 1 | 28.80 | 45.65 |
| zlib.inflate_byte@tinf | 16971 | 152.2 | 141.7 | 1.07 | 0.024 | 0.016 | 152.2 | 0 | 144.3 | 199.0 |

稳态区段按理想 I-cache 计（热代码，结果不受本测试程序偶然的代码布局影响）；“真实 I-cache 时”一列是同一区段在16 KiB 两路 I-cache + DIO 80 MHz flash 下的结果，flash I-miss 为该次运行的缺失次数。`#cold` 区段从空 cache 开始（代码经 flash XIP 取指），只作参考。

## 6. 运行信息

- toolchain: `riscv32-esp-elf-gcc (crosstool-NG esp-15.2.0_20251204) 15.2.0`
- march_idf: `rv32imafc_zicsr_zifencei_zaamo_zalrsc`
- march_zbzc: `rv32imafc_zicsr_zifencei_zaamo_zalrsc_zba_zbb_zbs_zcb_zcmp`
- kernel_flags: `-mabi=ilp32f -mtune=esp-base -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -std=gnu17 -O2 -fno-jump-tables -fno-tree-switch-conversion`
- coremark_flags: `-mabi=ilp32f -mtune=esp-base -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -std=gnu17 -O3 -fjump-tables -ftree-switch-conversion, code+data in internal SRAM, ITERATIONS=50`
- coremark: `https://github.com/eembc/coremark @ 1f483d5b8316753a742cbf5590caf5bd0a4e4777`
- tinf: `https://github.com/jibsen/tinf @ 57ffa1f1d5e3dde19011b2127bd26d01689b694b`
- miniz: `https://github.com/richgel999/miniz @ 77d0dce8627735138c51770d1799a1ef48f2117d`
- cjson: `third_party/cjson v1.7.19`
- tlsf: `third_party/tlsf deff9ab509341f264addbd3c8ada533678591905`
- base_config: `tools/iss/timing/esp32s31.cfg`
