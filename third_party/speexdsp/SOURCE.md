# SpeexDSP 重采样器

来源：https://github.com/xiph/speexdsp ，发布标签 `SpeexDSP-1.2.1`，提交 `1b28a0f61bc31162979e1f26f3981fc3637095c8`，查阅日期 2026-11-12。

仅复制重采样器和必需头文件；以下文件保持原文，BSD-3-Clause，许可证见 LICENSE。

| 本地文件 | 上游路径 | SHA-256 |
| --- | --- | --- |
| `resample.c` | `libspeexdsp/resample.c` | `c28fabfc082d0e7634eb678e2e3a2bc091148bbb8324e3d06669c9a9faf6793a` |
| `arch.h` | `libspeexdsp/arch.h` | `102f6a14a95f8ae0bcfc69270f3a9e3fbba08b63bba688ca524f71e3faa48c23` |
| `os_support.h` | `libspeexdsp/os_support.h` | `5ab9c979e8ac1f72c7441d24e39e7840828dd9356aac49a31eabeddb808bf1d3` |
| `include/speex/speex_resampler.h` | `include/speex/speex_resampler.h` | `7e439ec0dd30c32216b3ced17135f8992e5aaf53389d3f5996a7d900c453e65f` |
| `include/speex/speexdsp_types.h` | `include/speex/speexdsp_types.h` | `4f97187c057fd8493c787f2526752859cf57007d50fef8d2e03c9b79b2fed490` |
| `LICENSE` | `COPYING` | `2654a4264b2bfe298dedc508748d140111840c315cc8eb646a3a68c13fa75b01` |

本地适配：`os_support_custom.h` 将三种分配操作接到 OSAL 的 PSRAM；`include/speex/speexdsp_config_types.h` 是使用 stdint.h 的本地固定宽度类型头；CMake 固定浮点、标量实现，模拟器与 S31 同源。

audio-out-2：新增本地 `include/speex/ink_quality.h`，由生产端和独立参考测试共享所选质量常量；上表原文保持不变。Q0～Q10 的合成音调和宿主耗时对比见 `tests/audio_out/quality.c`，S31 估算及取舍见 `docs/tasks/audio-out-2/REPORT.md`。
