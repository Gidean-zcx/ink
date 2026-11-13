# Mogo calendar 来源

- 项目：Mogo（`mogo-e4p`），本机只读副本 `ink-hw/upstream/oss/mogo-e4p`，固定提交 `864f2f9dc75980a67d2d6b67b17365020b7a50ce`。
- 第一轮（almanac-2 v1，2026-11-12）：把下表 10 个文件逐字节拷进本目录，路径和上游一致；那个提交（`a5740efc`）留在历史里，可以和上游逐字节对比。
- 第二轮（almanac-2 收尾，2026-11-13）：v1 实测 S31 固件多了 181,984 B，超过 60 KB 预算（见 `docs/tasks/almanac-2/REPORT.md`）。这一轮**只改表的存法和读表的几行**，宜忌、神煞、建除、值神、农历、干支、节气的规则和结果一点不动；每处改动在下面“第二轮改了什么”里按文件和行号写清。没有自己写历法。
- 许可：C 模型和生成器 Copyright 2026 chopper1026，`LicenseRef-Kiikoread-Personal-Study-1.0`，全文在 `licenses/kiikoread-Personal-Study-1.0.txt`，根 `LICENSE` 的适用范围一并保留；lunar-javascript 1.7.7 Copyright (c) 2018 6tail，MIT，全文在 `tools/vendor/lunar-javascript/LICENSE`。D-129 不看许可，照原样带上；公开发布时由主会话处理。

## 文件和 SHA-256

| 文件 | 上游原文 SHA-256 | 现在的 SHA-256 | 改了吗 |
| --- | --- | --- | --- |
| `LICENSE` | `8d32fbcfba1f738a1d10f9bf62636137f240e41ff01ede576fcce1dc36626e64` | `8d32fbcfba1f738a1d10f9bf62636137f240e41ff01ede576fcce1dc36626e64` | 一样 |
| `docs/development/guides/CALENDAR.md` | `d22e7ec7858f328533a6f170aa210e01f69374bffba32b5e1845db6c00591d6b` | `d22e7ec7858f328533a6f170aa210e01f69374bffba32b5e1845db6c00591d6b` | 一样 |
| `licenses/kiikoread-Personal-Study-1.0.txt` | `fd3c1789d4c0d4d4fd21d485666a2a7058bad0c74fd3ea875b346dcc444f8e52` | `fd3c1789d4c0d4d4fd21d485666a2a7058bad0c74fd3ea875b346dcc444f8e52` | 一样 |
| `main/assets/calendar_data.h` | `ede8649bf1b60d6975793e076a6b90088771eeb3ba0622c15d921f3a426ec79a` | `27135cb799c33f3392b1ce59bb085ff4e4b42cc35e59e29b5d28eb2d61657ff9` | 第二轮改了 |
| `main/time/calendar.c` | `bcf7019ba50373627cf1f6b67b8e72c78fb85ece7fe2b3ab12fe737e1909841b` | `93c8ec04ebb85866c2e6637f9dad08cdcddf897a9e813ec7b04d4c5b117f8e69` | 第二轮改了 |
| `main/time/calendar.h` | `96cc73da97ff328da55d9bd0bf023802f01bac57e49e70022fbedd3902cce656` | `e77a12862e1844a24503ccfcb4b4ef16c318f93eb9db949b485a1eb9314196aa` | 第二轮改了 |
| `tools/calendar_host_test.py` | `22f6b410ce1c90f57a581cafb1f9d46b973f973ebc0948300723b42066238e20` | `6d2f4095629484921d853bc6d40141ae633f9e8eed9c3454d5e2c5f4e8e89e10` | 第二轮改了 |
| `tools/generate_calendar_data.cjs` | `0e2720092040da6d512ba404e7564b56c89fefe117f223f808ff631da06a6c96` | `6036b95312e4c8095a8d641ac873b4b364e1ac4354b5728e55f39ca7777160c2` | 第二轮改了 |
| `tools/vendor/lunar-javascript/LICENSE` | `d9210caf1844dcf410095cea464b79800aad30dbd49df092076b9f0ddc015404` | `d9210caf1844dcf410095cea464b79800aad30dbd49df092076b9f0ddc015404` | 一样 |
| `tools/vendor/lunar-javascript/lunar.cjs` | `9750324bfe1aa63c146f8c72b1143df924466c11c8a5277d7d9225c541a18aaa` | `9750324bfe1aa63c146f8c72b1143df924466c11c8a5277d7d9225c541a18aaa` | 一样 |

## 第二轮改了什么（行号是改后的文件）

为什么都一样：v1 的 181,984 B 里，表结构 45,348 B、表指向的中文串 123,887 B。中文串大是因为每组宜忌、神煞整串存（宜忌 1,231 串 81,777 B，神煞 1,066 串 42,122 B），而里面只有 114 个不同的宜忌词、167 个不同的神煞词。所以改成“词表＋每组一串 1 字节词号”，再去掉三处冗余（下面第 2–4 条）。规则仍然全部来自 lunar-javascript 1.7.7 的 getter，生成器每次都把压缩后的表解回去和原值比一遍。

1. `tools/generate_calendar_data.cjs`（上游 73 行，现在 123 行）
   - 第 16–17 行：新加说明和 `fail()`，自检不过就报错退出。
   - 第 18 行：原来的 `runs` 数组（月干支变化点，上游第 44 行写表）改成只计数，给最后一行的统计用；表本身不再输出（见第 3 条）。
   - 第 24–36 行（上游第 22–23、27–28 行的两个 `intern`）：改成 `wordList()`，每类一张词表、一串词号（每组以 `0xFF` 结尾，相同的组只存一次）、每组一个 `uint16` 起点，并带 `decode()` 用来自检。
   - 第 37–43 行（上游第 24–26 行，宜忌组）：只按 12 个月支×60 日干支生成 720 组（上游是 60 月干支×60 日干支 3,600 组），键 `(月干支%12)*60+日干支`；同时照上游的循环把 3,600 组原值算出来。
   - 第 44–48 行：自检一，折叠后的 720 组和 3,600 组原值逐组相同。
   - 第 49–55 行（上游第 29–31 行，神煞组）：存法同上，键不变，每组生成后马上解回去和原值比。
   - 第 68–71 行（上游第 44 行）：自检二，2000–2099 每一天 `月干支 = (12 + (年-2000)*12 + (月-1) + (日 >= 当月“节”的日子)) % 60` 和 `getMonthInGanZhi()` 一致（也就是 `CALENDAR.md` 说的“节气当天整日按新月份”）。
   - 第 73 行（上游第 46 行）：建除、值神、黄黑道存神煞词表里的 1 字节编号。
   - 第 79–98 行：农历月按起点排好；自检三，相邻两月首尾相接、农历年只在正月（非闰）进位、字段放得下；自检四，拼出来最长的宜、忌、吉神、凶煞（含结尾 0）不超过 `calendar.h` 里四个数组的大小（从 `calendar.h` 读，不会两边各写一个数）；自检五，每类词数小于 255、词号串小于 65,536 B。
   - 第 101–109、112–115 行（上游第 54–55、59–60、63–65 行，输出段）：农历月每条 `{uint16 相对第一个月的起点, uint8 年-1998, int8 月}`，末尾一条哨兵，天数用下一条减本条；`calendar_month_cycles` 不再输出；宜忌、神煞输出词表、词号串和 `uint16` 起点；`calendar_day_roles` 改成 `uint8`。
   - 第 123 行（上游第 73 行）：最后打印的统计跟着改。
   - `--check`（表是不是最新）和 `--oracle <文件>`（逐日对照数据）的含义不变；`--oracle` 每一行仍直接来自 lunar-javascript 的 getter（第 75 行，和上游第 48 行一字不差），和表格式无关。
2. `main/time/calendar.h` 第 13、15 行：`yi`、`ji`、`auspicious`、`inauspicious` 从 `const char *` 改成 `char` 数组 `[232]`、`[128]`、`[112]`、`[104]`，因为现在是用词拼出来的，没有现成的整串可以指。字段顺序和其余字段不变。最长的实测是 223、118、101、94 B（含结尾），由生成器自检四保证。
3. `main/time/calendar.c`
   - 第 11 行（上游第 11–12 行）：农历月记录改成 4 B；月干支变化点的 typedef 去掉。
   - 第 14–18 行：新加 `calendar_words()`，按词号把词用空格拼进数组（和上游 `join(' ')` 一样），放不下就停（自检四保证放得下）。
   - 第 47–57 行（上游第 43–53 行）：二分查找照旧，只是比相对起点，天数用下一条（哨兵）算；农历年 = 1998 + 存的值；月干支按上面的公式由 `calendar_term_days` 算，不再查表。
   - 第 58–60 行（上游第 54–55 行）：宜忌按 `(月干支%12)*60+日干支` 查 720 组，拼进数组。
   - 第 68–72 行（上游第 63–67 行）：吉神、凶煞拼进数组；建除、值神、黄黑道从神煞词表取。
   - 其余函数（`civil_serial`、`calendar_from_utc`、`calendar_lunar_text`、`calendar_term`、`calendar_cycle_name`、`calendar_month_name`）一字不改。
4. `main/assets/calendar_data.h`：由改后的生成器重新生成（`node tools/generate_calendar_data.cjs`），不手改；`--check` 保证是最新的。
5. `tools/calendar_host_test.py` 第 17 行：ctypes `Date._fields_` 里这四个字段的类型跟着 `calendar.h` 改成 `c_char * N`，其余一字不改。它从 Mogo 的 `tools/run_reader_baseline.py` 只导入 `toolchain(work)`，那个文件没拷；我们在 `tests/almanac/run_reader_baseline.py` 放了一个垫片，返回本机 C 编译器，没有 node 时退出 77（跳过）并说明原因。ctest `almanac-mogo-host` 用 `PYTHONPATH` 指到垫片目录直接跑原文测试。改之前，同一个原文测试在上游原文 `calendar.c` 上也全过（第二轮核对过）。

靠什么证明结果没变：`almanac-oracle`（`--check` 加 2000–2099 共 36,525 天全部字段和 lunar-javascript 1.7.7 逐日比）、`almanac-mogo-host`（Mogo 原文测试：百年逐日、春节、闰月、2026 年节气、无效日期、北京零点和范围边界）、`almanac-fixtures`（50 天固定样本，`fixtures.json` 没改）、生成器自检一到五；反向对照见报告。

## 数据为什么只放在 `.h`

`tools/fontgen/ui_fonts.py` 会把 `system/services/` 下所有 `*.c` 里的汉字收进界面字库，并要求都在 GB2312 里。宜忌词“安碓磑”的“磑”不在 GB2312，所以表只能放在 `main/assets/calendar_data.h`，不能生成成 `.c`，词表也不能写进 `almanac.c`。表不进 font_data、不放卡上（D-138：固件里什么都有，没有“数据没装上”的状态）。

## 和上游用法的差异（v1 起就有，照旧）

- 上游 `main/time/calendar.c` 的 `calendar_from_utc` 固定 UTC+8，而且只认 2024 年起的产品时钟。我们的接线层（`system/services/time/almanac.c`）不调它：用现有 `ink_time_to_local` 取系统时区的今天，再把公历年月日交给原文 `calendar_date`；给了日期的查询也按系统当地的公历日期，范围 2000–2099。原函数留着、原测试照跑。
- `CALENDAR.md` 的日粒度流派、节气当天整日切月、福神默认 `POSITION_FU_2` 原样沿用；换时区时“今天”可能和北京时间的今天不同，这是有意的（家人不一定在东八区）。
- 对外 `ink_almanac_t` 带 `calendar_date_t` 的全部字段（四个列表同样是数组，`almanac.c` 用 `_Static_assert` 保证大小一致），另带三个干支名、完整农历文字、当天节气（不是节气日为空串）；负农历月份表示闰月。
- `almanac.c` 只接 C 接口和软总线 JSON；查一次的结果和 JSON 一起从 PSRAM 临时分配一块，用完就还；不新增任务、缓存、联网、持久数据或定时唤醒。

## 宿主机重生成

在本目录运行 `node tools/generate_calendar_data.cjs`（重写表）或 `node tools/generate_calendar_data.cjs --check --oracle oracle.json`（核对）。普通固件构建只编 `main/time/calendar.c` 和静态表，不需要 node，不下载数据，不跑 JavaScript。
