# audio-out-1 执行报告

基线：`36473cc4933e6fea2f2a78a4fbec16186d3be3f4`。**未通过全部验收。** PCM 通路已实现，按总规则“三次未过即停止”结束欠载修改；提交仅在本地，未接触设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 未通过 | 【实测】FLAC→暂停→定位 60 秒→MP3→语音打断恢复→换成员，控制检查完成；完整流程 44 次欠载、660 ms。五段首次完整 PCM 帧均逐样本匹配全新 FIR，频率检查因断续失败 | C2 |
| 2 | 未通过 | 【实测】最终 1 kHz SNR 18.17 dB，低于 60 dB；10 kHz 衰减 88.54 dB，超过 40 dB。抽点反向对照衰减 0.023 dB，40 dB 断言确实失败 | C2、C4 |
| 3 | 已生成 | 【实测】同一合成双声道音乐的 speaker.wav：[改前](before-spectrum.png) 53264 B、[改后](after-spectrum.png) 70569 B，均小于 300 KB；改后图包含最终欠载，未冒充合格输出 | C7 |
| 4 | 通过 | 【实测】原有 integration-media、两项 determinism、music-formats、music-product，加反向对照共 6/6。旧 Opus open/push/finish/abort 四个公开函数与基线逐字一致 | C4 |
| 5 | 部分通过 | 【实测】普通首轮全量 435 通过、1 超时、3 跳过；超时备份定向复跑通过。ASan/UBSan 11/12，音频连续性失败，无内存诊断；S31 宿主机 77/77、固件编译通过且无新编译警告。固件 +8384 B、内部静态 RAM +32 B | C1、C3、C5、C6 |

## 没做到的

- 最终保守模型下仍有欠载。1 kHz 场景在 speaker.wav 第 1.095 秒插入 15 ms 零样本，SNR 降至 18.17 dB；完整流程累计 660 ms。最终不能通过连续性与音质联合验收。
- 三种调度处理：整块转换只升优先级至 12，仍 4 次/60 ms；按 240 帧转换，降至 1 次/15 ms；240 帧加优先级 12，仍 1 次/15 ms。最终保留分段、恢复优先级 7。依据[总规则第 7 节](../codex-rules.md)“同一个测试修了 3 次还不过：停下”，停止该项修改。
- ASan 注册测试遇到首个连续性失败就停止新场景；另外用 C2 的诊断收集模式在 ASan 下遍历全部四个场景，仍记录失败。未做实机试听、烧录、串口或硬件操作。
- 未运行 Linux 全套测试；本机为 macOS，只执行了指定的 GCC 警告检查。

## 和设计不一致的

- 零欠载、1 kHz SNR 和全部测试通过的交付条件未达到；设计、时限和预算没有修改。原厂 I2S 驱动与 HAL 未改。

## 改过的已有测试

- 无。已有断言、超时、预算和音乐素材未改。新增测试与素材的来源、方法见 [测试说明](../../../tests/audio_out/README.md)。诊断模式遇到失败仍返回非零。

## 要用户定的

- 主会话后续确定实机标定与供流整改。现有原采样率 PCM 队列不能缓冲音频任务内的滤波计算成本。
- 当前 HAL 同时初始化 TX/RX；DMA 4→6 会令两向数据区至少增加 `2×2×240×8=7680 B`，尚不含描述符，超过本任务 4 KB 内部增量上限，所以没有扩容。

## 顺手发现

- 【实测】改前音质对照使用同源真实固定点 Opus 后，媒体执行器宿主栈估算为 25248 B，超过原 8192 B。比较命令保留这个预算失败，只收集音质；该替换未标注 Opus 模拟成本，不能用于调度性能比较。
- 【实测】全量原话：`99% tests passed, 1 tests failed out of 439`，1848.29 s。失败是 backup-restore-recovery 的原 240 s 超时；定向复跑 158.71 s 通过。hub-contract、hub-json-crosscheck、hub-simulator-e2e 按原条件跳过。
- 【实现】媒体头文件说明和 media.play 标题仍只写 Ogg Opus，是既有文案遗漏。

## 新增的机制

- 任务书第 1～3、6 项要求的 PCM 入口、FIR 历史及一块输入的分段游标，复用原声音队列、音频任务和输出所有者。无新增任务、线程、独立状态机、配置、持久格式或能力；人和 AI 仍调用现有媒体能力。

## 要改的文档

- 共享文档保持不动；后续媒体设计、接口与能力说明需同步 PCM 通路、支持格式及本次未通过的连续性条件。

## 来源与实现

- 【实现】MP3/WAV/FLAC/Vorbis 解码结果直接交新入口，删除抽点和 Opus 重编码。Ogg Opus 保留原入口。PCM 以源采样率混为单声道，在音频任务中用同一份 SpeexDSP Q4 浮点标量代码转换至 16 kHz，每次最多输出 240 帧；沿用音量、焦点、成员校验及 HAL。新流重建 FIR，停止释放历史及输入，正常 EOF 补零排尽 FIR。打断测试使用原模拟器 Opus 桩的语音优先级流，未连真实中枢朗读。
- 【实测】SpeexDSP 1.2.1，提交 `1b28a0f61bc31162979e1f26f3981fc3637095c8`，6/6 原文哈希一致，BSD-3-Clause；[SOURCE.md](../../../third_party/speexdsp/SOURCE.md) 列出文件与本地分配适配。
- 【实现】Q4 基长 64，在 44.1→16 kHz 时为 176 tap；最终 10 kHz 衰减达标。S31 构建配置 `CONFIG_SOC_CPU_HAS_FPU=1`，采用浮点，临时 4 KB 数组落在既有 26624 B 的 PSRAM 音频栈中，没有增栈。Q4 的完整播放仍未达标。
- 【实现】原厂 `874538e:main/boards/common/audio/bt_audio_codec.cc:66-98` 为 slave/stereo32，`:138-163` 把同一样本复制到两槽；`main/boards/metalio-e-ink4-plus/periph/metalio_audio.cc:94-110,304-312` 没有耳机左右独立发送证据。未证明 mode 2 分离左右，按任务书保持单声道。
- 【实现】MyPaperS31 `325b538:main/audio/audio_radio_s31.cpp:370-430` 是转换后输出；`docs/S31_RADIO_PLAYBACK.md:5-9,71` 给出优先级/DMA 和另一算法约 1.26 ms/块的记录，不能当作本实现实测。当前媒体执行器 4、UI 5、音频 7；DMA 4×240 帧，60 ms。

## 资源与测量

【实测】同一 SDK/配置的独立基线与当前固件：bin `4268288→4276672 B`，增量 **8384 B（8.19 KiB）**；DRAM bss `44360→44392 B`，增量 **32 B**；data、IRAM、静态 PSRAM 均无增量。两次 build.sh 均返回 0，编译 warning/error 均为 0；没有新增 DMA 或任务。

【实现】新增动态分配都请求 PSRAM。S31 队列元数据增加 2048 B；44.1 kHz 媒体最多 35 块单声道源 PCM，申请 185220 B；宿主 FIR 状态请求峰值 4328 B。三者合计 191596 B 是未扣旧 Opus 编码器/工作区的申请字节上界，分配器开销未计。相同 1 kHz 模拟工况总 PSRAM 峰值比真实 Opus 基线少 8008 B，但有欠载、队列未必填满，不能当最坏值或实机结果。新运行时内部分配调用为 0；内部实际运行时余量未上机测。

【实测】宿主 CPU 时间，44.1 kHz 双声道，解码、混音和按 240 帧输出的 Q4 重采样，不含打开/初始化及模拟器调度等待（ms/音频秒）：

| 格式 | 解码 | 重采样 | 合计 | 最大 60 ms 块 CPU 时间 |
| --- | ---: | ---: | ---: | ---: |
| WAV | 0.0503 | 3.0690 | 3.1193 | 0.3070 |
| FLAC | 0.2990 | 3.2760 | 3.5750 | 0.3060 |
| MP3 | 0.9630 | 3.3693 | 4.3323 | 0.3090 |
| Vorbis | 0.6250 | 3.3293 | 3.9543 | 0.3240 |

【估算】320 MHz、3200 cycles/输出采样，对应 FIR 约 160 ms/音频秒；假设解码为宿主耗时的 20～50 倍，总计约 161～208 ms/音频秒（单核 16～21%），另有缓存与抢占开销。未用实机或 ISS 标定，不把这个估算冒充实测，也未调低成本令测试过关。

【实测】同输入、同稳态一秒方法的最终输出（sin/cos/DC 拟合 SNR，交流 RMS 衰减）：

| 通路 | 1 kHz SNR | 10 kHz 衰减 |
| --- | ---: | ---: |
| 基线抽点 + 同源真实 Opus 24 kbps/复杂度 0/VOIP/DTX | 15.2231 dB | 0.5902 dB |
| 最终 PCM + Q4（输出含欠载） | 18.1734 dB | 88.5430 dB |

原始数字与最终各阶段游标见 [measurements.json](measurements.json)。图像已目视检查。GCC 检查原话：`gcccheck: 5 compiled, 0 with findings`。手写产品及构建代码新增 141 行、删除 29 行，严格按增删相加为 **170 行**，第三方原文、测试与文档不计。超过 100 行的原因：新入口、共用队列的两类数据、FIR 生命周期、四格式统一接入及双端构建均属任务要求；更小的整块转换与仅提高优先级已试，仍欠载。

## 复核命令

主工程配置按总规则第 5 节；ASan 同参数增加 `-DINK_SANITIZE=ON`，目录改 build-asan。以下均在仓库根目录运行。

```bash
# C1：普通首轮全量（退出 8）；备份定向复跑通过
 taskpolicy -c utility cmake --build build -j2
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(audio-out|audio-out-alias-reverse|backup-restore-recovery)$' --output-on-failure -j2
# C2：最终全部四个场景，保留失败并退出 1；两种构建均运行
 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audio_out/measure.py --build build
 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audio_out/measure.py --build build-asan
# C3：S31 宿主机，77/77
 taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-tests
 taskpolicy -c utility cmake --build build-s31-tests -j2
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-tests --output-on-failure -j2
# C4：最终原有音频测试及抽点反向对照，6/6
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -R '^(audio-out-alias-reverse|integration-media|determinism|determinism-reader|music-formats|music-product)$' -j2
# C5：S31 编译；基线在独立源码目录同配置构建
 INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh
# C6：ASan/UBSan，11/12，唯一失败为 audio-out 连续性；四场景日志无 sanitizer 诊断
 taskpolicy -c utility cmake --build build-asan -j2
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -R '(audio|media|music|determinism|voice|opus|record)' -j2
# C7：真实 Opus 基线音质、性能及图；基线栈预算失败保留
 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audio_out/control.py --build build --baseline 36473cc4933e6fea2f2a78a4fbec16186d3be3f4 --opus-build build-s31-tests
 taskpolicy -c utility build/test-audio-out-bench tests/music/fixtures
 MPLCONFIGDIR=work/mpl-cache taskpolicy -c utility work/measure-env/bin/python tests/audio_out/plots.py --build build --out docs/tasks/audio-out-1 --font "$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf"
# C8：Linux GCC 警告检查
 taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/services/audio/audio_service.c system/services/audio/media_service.c system/services/audio/codec_formats.c tests/audio_out/device.c tests/audio_out/bench.c
```

【实测】最终 ASan 原话：`92% tests passed, 1 tests failed out of 12`，79.71 s；失败为已有记录的欠载。独立遍历四场景的日志中 ASan/UBSan 诊断为 0。未对已知失败再重复全量 439 项。

## 上机核对清单

- 主会话外放和蓝牙耳机各听一首，覆盖 44.1 kHz FLAC、下一首 MP3、暂停、定位一分钟、朗读/铃声打断恢复和成员切换；关注断续及旧声音残留。
- 标定解码和 FIR 的实际每块耗时，对照供流间隔/I2S 日志。当前真机 HAL 没有模拟器同名的短欠载计数，不把无报错或 stall_lost_ms 当作零欠载；耳机左右分离仍需实际证据。

## 最终检查

- 【实测】基线至提交差异共 30 个文件，均在任务许可范围；git diff --stat 已检查，未夹带构建产物、共享文档或设计稿。
- 【实测】任务书指定的隐私正则匹配 0；本报告及新增说明的本地链接全部存在；SpeexDSP 6/6 文件哈希及原文一致。两张 PNG 已目视检查且各小于 300 KB。
