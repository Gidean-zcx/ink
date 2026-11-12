# audio-out-2 执行报告

基线：`a39e9693bb4dea2edd0b5549486d583122dc0a13`。状态：核心实现及定向回归完成，全量与 S31 验证待补。未连接设备，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 定向通过 | 【实测】audio-out 与两种反向对照 2/2 通过；Q4 生产侧滤波，PSRAM 预缓冲 540 ms | C1 |
| 2 | 待验证 | 前后频谱图及合成音乐 WAV | 待补 |
| 3 | 待验证 | 普通全量、内存检查、S31 宿主测试与固件构建 | 待补 |

## 没做到的

- 尚未完成验证；宿主模拟与计算量估算不能冒充 S31 实机结果。

## 和设计不一致的

- 暂无；保留原厂 HAL 及 DMA 4×240 帧。复用原 PSRAM 队列保存处理后的 16 kHz PCM，未另建队列。

## 改过的已有测试

- `bench.c` 的独立 FIR 参考改用共享质量常量，仍为 Q4；所有逐样本断言保留。
- `control.py` 适配生产侧滤波的位置，并增加把滤波放回输出任务的反向对照；使用随测试提交的补丁，不依赖 CI Git 历史。欠载和音质断言未放宽。

## 要用户定的

- 暂无。

## 顺手发现

- Q8：1 kHz 与合成音乐各 1 次/255 ms 欠载；Q7：完整流程 2 次/285 ms 欠载。因此未采用。Q4 定向场景通过；完整测量待归档。

## 新增的机制

- 任务书第 1 项要求的生产侧滤波：在现有媒体执行器调用 PCM 入口时计算；生产端独占 FIR，用一把互斥锁串行化推送、排尾和销毁，输出任务不取得这把锁。
- 任务书第 1 项要求的缓冲：复用 PSRAM 播放队列，媒体上限 34×60 ms，PCM 启播阈值 9×60 ms。无新增任务、线程、队列、状态机、持久格式或用户配置。

## 要改的文档

- 暂无；不修改共享文档。

## 复核命令

主工程配置沿用总规则第 5 节，构建 `taskpolicy -c utility cmake --build build -j2`。

```bash
# C1
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^audio-out' --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audio_out/measure.py --build build
```
