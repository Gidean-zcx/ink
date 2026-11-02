# 墨水屏刷新算法研究：资料目录

本目录收录[刷新算法研究报告](../eink-refresh-algorithm.md)背后的全部材料：

- 各阶段当时交付的代码包；
- 最终交付包；
- 结果输出、对比视频与静帧。

研究于 2026-10-28 在公共会话完成，2026-11-02 整理进本仓库。所有内容都是仿真、指令级估算和助手设计，**没有实机测试**。

## 目录

| 路径 | 内容 | 来源 |
| --- | --- | --- |
| [pqbn_v4/](pqbn_v4/) | 最终交付包：v4 内核、等价性与自动模式测试、仿真脚本、RV32 模拟器与基准、全部结果 | 当时交付的 `pqbn_v4_pkg.zip`，只保留源码和结果 |
| [history/01-sim-pq/](history/01-sim-pq/) | 阶段 1：面板模型、PaperBoy 计数器对照、积分器、PQ 定点版、开销基准、视频脚本、`res_*.json` | 当时交付的 `eink_sim_code.zip`，原样解包 |
| [history/02-metalio-analysis/](history/02-metalio-analysis/) | 阶段 2：Plus 波形直流审计、每相开销、延迟与残影仿真、滚动视频脚本 | 当时交付的 `metalio_analysis.zip`，原样解包 |
| [history/03-direct-drive/](history/03-direct-drive/) | 阶段 3：PQ-BN 浮点、整数、64 KB 查表、事件驱动版本，Plus 时序画质结果，以及随后的对比视频脚本 `make_clip2.py` | 当时交付的 `pqbn_direct_drive.zip`，原样解包；另加 `make_clip2.py` |
| [history/04-kernel-iterations/](history/04-kernel-iterations/) | 阶段 4 的中间版本：v4 草稿、v2/v3 的 x86 基准、早期 RV32 基准与比对程序、tile 统计，以及含事件驱动与 v2 行的画质结果 | 工作目录中的源码；v2/v3 内核本体在 `pqbn_v4/` 中 |
| [results/rerun-2026-11-02/](results/rerun-2026-11-02/) | 2026-11-02 用归档代码重跑的阶段 1、2 输出 | 本次整理 |
| [media/](media/) | 三段对比视频（压缩版）、静帧，以及归档版视频的生成脚本 | 本次整理 |

各阶段目录中的 `README.md` 是当时随包交付的说明，保留原文。其中有一处已被更正：阶段 3 README 用“x86 × 10～14”折算 S31 耗时，阶段 4 的指令级估算证明它过于乐观，见报告。

## 媒体文件

| 文件 | 内容 |
| --- | --- |
| [phase1_compare.mp4](media/phase1_compare.mp4) | 阶段 1 对比。PaperBoy 时序，60 场/秒，三个场景：自然视频、阅读/UI、GB 类游戏。面板依次为目标、PaperBoy 计数器 + FS、PaperBoy 计数器 + 蓝噪声、PQ；GB 场景为 PaperBoy 原版、积分器、PQ |
| [phase1_video_gb_stills.png](media/phase1_video_gb_stills.png)、[phase1_text_still.png](media/phase1_text_still.png) | 阶段 1 静帧 |
| [phase2_metalio_scroll.mp4](media/phase2_metalio_scroll.mp4)、[phase2_metalio_scroll.png](media/phase2_metalio_scroll.png) | 阶段 2，Plus 时序（12 ms/相）。列表甩动滚动加点按，比较现架构 DU20、现架构 DU8、异步积分器 |
| [phase3_metalio_algorithms.mp4](media/phase3_metalio_algorithms.mp4) | 阶段 3，Plus 时序六面板对比：目标、现架构灰阶模式、现架构二值 UI、异步积分器 + 蓝噪声、PQ-BN、PQ-ED（理论参考） |
| [phase3_list_scroll.png](media/phase3_list_scroll.png)、[phase3_grid_scroll.png](media/phase3_grid_scroll.png)、[phase3_ai_stream.png](media/phase3_ai_stream.png)、[phase3_page_turn.png](media/phase3_page_turn.png) | 阶段 3 静帧 |

与当时交付的视频相比，归档版有两处不同：

- **压缩**：视频按 H.264 CRF 30 重新压缩以控制仓库体积，原视频为 CRF 16/24，噪点细节略有损失。
- **替换测试图**：原视频中 scikit-image 的 `astronaut` 测试图是真人照片，阶段 1 第一个场景和阶段 3 的封面、壁纸都用到了它。归档版用 `data.moon()` 月面图替换，由 [regen_phase1_video.py](media/regen_phase1_video.py)、[regen_phase3_video.py](media/regen_phase3_video.py) 生成。

替换只影响画面内容，报告中的数值仍是原场景的结果。阶段 2 的视频只有文字，是对原视频的直接重压缩。

## 依赖

2026-11-02 重跑时的环境：

| 用途 | 依赖 |
| --- | --- |
| 编译 | gcc 13 |
| 仿真 | Python 3.13，numpy、scipy、scikit-image、Pillow |
| 视频 | ffmpeg |
| 字体 | `/usr/share/fonts/opentype/noto/NotoSansCJK-*.ttc`、DejaVuSans |
| RV32 构建 | clang 18 + ld.lld、llvm-objcopy、llvm-nm（riscv32 目标） |

阶段 2、3 的脚本需要 Plus 仓库固定提交 `ad8edf4150a688737c0d7c00cae4413079b262ef` 下的 `main/boards/metalio-e-ink4-plus/display/waveforms` 目录。脚本只把其中的波形当数据解析，不编译或执行仓库代码。

## 复现

### 最终交付包（`pqbn_v4/`）

```bash
cd pqbn_v4
sim/build.sh && tests/run_tests.sh       # 等价性、不变量、自动切换测试
cd sim && python3 make_scenes.py && python3 run_v4_q.py && python3 run_fling.py && python3 run_auto.py
cd ../rv && ./build.sh                   # 打印内核符号地址和后续命令
mkdir -p scen && python3 gen_scenarios.py scen
AUTO=1 BUDGET=8000 ./rvbench4 k4.bin <old> <v3> <v4> scen 60
```

`<old> <v3> <v4>` 是 `build.sh` 打印的符号地址。

### 阶段 1、2 的重跑

```bash
# 阶段 1：x86 开销
cd history/01-sim-pq && gcc -O2 -o bench bench.c pq_fixed.c -lm && ./bench

# 阶段 2：波形直流审计
python3 -I history/02-metalio-analysis/wf_dc.py <Plus 仓库>/main/boards/metalio-e-ink4-plus/display/waveforms

# 每相开销：用阶段 3 的新版 bench_metalio.c，它含 PQ-BN 行
cd history/03-direct-drive && gcc -O2 -fno-tree-vectorize -o bench_metalio bench_metalio.c pqbn_fixed.c pqbn_lut.c -lm && ./bench_metalio
```

### 写死的路径

以下脚本写死了原工作目录，运行前需要修改：

| 脚本 | 写死的内容 |
| --- | --- |
| `history/02-metalio-analysis/sim_metalio.py` | `/home/claude/eink_sim`（共享面板模型），以及两行写出大体积 `*.pkl` 的语句。2026-11-02 重跑时删去了这两行 |
| `history/03-direct-drive/make_clip2.py` | 面板模型路径、波形目录、输出路径 |
| `media/regen_*.py` | 面板模型路径、波形目录、输出路径 |

这些脚本需要的面板模型可以用 `pqbn_v4/sim/` 中的版本。它是最新版，保留了早期接口。

### 未归档的内容

以下内容都可以由脚本重新生成：

- 场景数组 `*.npy`：每个约 184 MB；
- RV32 场景 `scen/*.bin`：每个约 30 MB；
- 仿真日志 `*.pkl`；
- 编译产物。

本目录的 `.gitignore` 已排除这些文件。

## 2026-11-02 的核对

在仓库外的副本中做了以下核对：

- `pqbn_v4` 构建通过，`tests/run_tests.sh` 的输出与 `results/tests.txt` 逐行一致。
- `rv/build.sh` 构建通过。
- 用 `gen_scenarios.py` 重新生成 7 个全分辨率场景，再运行 `AUTO=1 BUDGET=8000 ./rvbench4`，输出与 `results/rvbench4_auto8k.txt` 逐字节一致。
- 阶段 1、2、3 的基准程序都能从归档目录编译。

阶段 1、2 的重跑输出见 `results/rerun-2026-11-02/`：

- 波形审计与延迟仿真使用固定输入和随机种子，结果可重复；
- x86 开销随机器浮动，只宜看倍数关系；
- 当时这三项的原始输出没有保存，所以无法逐项比对。
