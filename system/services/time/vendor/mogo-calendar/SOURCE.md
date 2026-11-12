# Mogo calendar 来源

查阅与原文拷贝：2026-11-12。

- Mogo：任务书指定固定副本 `mogo-e4p`，提交 `864f2f9dc75980a67d2d6b67b17365020b7a50ce`。
- 仅拷贝本目录下的历法模型、静态表、宿主生成器、原测试、规则说明和对应许可。上游文件逐字节不改，没有删减内容。
- C 模型和生成器：Copyright 2026 chopper1026，`LicenseRef-Kiikoread-Personal-Study-1.0`；全文在 `licenses/kiikoread-Personal-Study-1.0.txt`，根 `LICENSE` 的适用范围一并保留。
- lunar-javascript 1.7.7：Copyright (c) 2018 6tail，MIT；全文在 `tools/vendor/lunar-javascript/LICENSE`。生成表的输入哈希也保留原文。

## 文件 SHA-256

下列路径同时是固定提交中的上游路径和本目录中的相对路径。所有文件的本地哈希与上游一致；修改行数均为 0。

| 文件 | SHA-256 |
| --- | --- |
| `LICENSE` | `8d32fbcfba1f738a1d10f9bf62636137f240e41ff01ede576fcce1dc36626e64` |
| `docs/development/guides/CALENDAR.md` | `d22e7ec7858f328533a6f170aa210e01f69374bffba32b5e1845db6c00591d6b` |
| `licenses/kiikoread-Personal-Study-1.0.txt` | `fd3c1789d4c0d4d4fd21d485666a2a7058bad0c74fd3ea875b346dcc444f8e52` |
| `main/assets/calendar_data.h` | `ede8649bf1b60d6975793e076a6b90088771eeb3ba0622c15d921f3a426ec79a` |
| `main/time/calendar.c` | `bcf7019ba50373627cf1f6b67b8e72c78fb85ece7fe2b3ab12fe737e1909841b` |
| `main/time/calendar.h` | `96cc73da97ff328da55d9bd0bf023802f01bac57e49e70022fbedd3902cce656` |
| `tools/calendar_host_test.py` | `22f6b410ce1c90f57a581cafb1f9d46b973f973ebc0948300723b42066238e20` |
| `tools/generate_calendar_data.cjs` | `0e2720092040da6d512ba404e7564b56c89fefe117f223f808ff631da06a6c96` |
| `tools/vendor/lunar-javascript/LICENSE` | `d9210caf1844dcf410095cea464b79800aad30dbd49df092076b9f0ddc015404` |
| `tools/vendor/lunar-javascript/lunar.cjs` | `9750324bfe1aa63c146f8c72b1143df924466c11c8a5277d7d9225c541a18aaa` |

## 适配差异

- 上游 `main/time/calendar.c:69–74` 的 `calendar_from_utc` 固定 UTC+8，且产品时钟只接受 2024 年起。本项目适配层不调用它：通过现有 `ink_time_to_local` 选系统时区的今天，再把该公历年月日交给原文 `calendar_date`；显式日期同样指系统当地的公历日期，范围 2000–2099。没有重算节气时刻或改变表的规则。
- `docs/development/guides/CALENDAR.md` 的日粒度流派、节气当天整日切月、福神默认 `POSITION_FU_2` 原样沿用；日期换区时可能与北京时间当天的结果不同，这是任务书明确要求的差异。
- 对外 `ink_almanac_t` 逐项提供 `calendar_date_t` 的全部字段，另补三个干支名称、完整农历文字、当天节气（非节气日为空串）；负农历月份表示闰月。指针指向只读常量，调用者不释放、不修改；输出结构由调用者持有。
- `almanac.c` 只接 C 接口和软总线 JSON，结果缓冲在 PSRAM 临时分配；不新增任务、缓存、联网、持久化或定时唤醒。回执仅摘要宜、忌第一项，完整列表在 JSON 中；附民俗说明。
- `tools/calendar_host_test.py` 原文留作方法参考；它依赖完整 Mogo 的 `run_reader_baseline`，没有宣称直接执行。实际移植测试在 `tests/almanac/`：使用相同的 `--check` / `--oracle`，编译并逐日核对我们的 C 接口；固定 50 天和反向对照不依赖 node，没有 node 时只有逐日生成步骤明确跳过。

## 宿主机重生成

在本目录运行 `node tools/generate_calendar_data.cjs --check --oracle oracle.json`。普通固件构建只编译 `main/time/calendar.c` 与静态表，不需要 node，不下载数据、不执行 JavaScript。原测试和规则说明中的路径仍按上游目录结构保留。
