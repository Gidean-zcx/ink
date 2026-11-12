# PCM 输出回归

素材全是原创合成音调，没有真实音乐、声音、成员或设备数据。

- `01.flac`：70 秒，44.1 kHz、s16 双声道。前一分钟左右为 440/660 Hz，之后为 880/1320 Hz，振幅各 12000。
- `02.mp3`：12 秒，44.1 kHz 单声道、330 Hz，振幅 12000。特意用单声道，避免当前 libsndfile 的双声道 MP3 编码素材本身出现明显调制；FLAC 仍完整覆盖双声道输入。
- 1/10 kHz 测量信号和成员切换后的 1600 Hz WAV 在运行时用标准库生成；两个声道相同，振幅各 16000。

| 文件 | SHA-256 |
| --- | --- |
| `fixtures/01.flac` | `d2d20a9c8c14d862503b7c3f587fe72bcdf4e38c37491e1c41c759bb8d57cd0f` |
| `fixtures/02.mp3` | `7f6127dc048cac85bc0b670d827d45e3b29044833570b5c6d9e532ada2eea027` |

`generate.py` 可重新生成（numpy 2.0.2、SoundFile 0.13.1、libsndfile 1.2.2），它们不进入产品或 ctest 依赖。`run.py` 和 `control.py` 只需要 Python 标准库。`plots.py` 仅用于手动导出报告图，需要 numpy、matplotlib 和指定的 CJK 字体。

`audio-out` 使用真实媒体/声音服务及 I2S 模型。检查暂停、定位、下一文件、语音抢占恢复、成员切换及刷新时欠载；对实际 `speaker.wav` 的各阶段检查频率，并将每段首次完整 PCM 帧与独立创建的上游 FIR 输出逐样本比较，验证滤波历史被清除。1 kHz 信噪比使用稳态一秒的 sin/cos/DC 拟合；10 kHz 衰减比较输入与输出交流均方根。`audio-out-alias-reverse` 只在临时对象中把滤波换成抽点，必须触发 40 dB 衰减断言。

手动基线模式才读取 Git 历史，未注册为 ctest。可用 S31 宿主机工程已经编译的相同固定点 Opus，替换模拟器的音质桩；参数保持 VOIP、DTX、24 kbps、复杂度 0。这个基线若触发媒体栈预算警告，命令明确保留失败，只收集音质比较数据，不记成测试通过。

```bash
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^audio-out' --output-on-failure -j2
taskpolicy -c utility build/test-audio-out-bench tests/music/fixtures
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audio_out/control.py --build build --baseline 36473cc4933e6fea2f2a78a4fbec16186d3be3f4 --opus-build build-s31-tests
```

普通通过的测试清理临时证据；设置 `INK_KEEP_EVIDENCE=1` 保留在构建目录。测试不依赖目录排序，不读取宿主时间来安排模拟任务，不接触设备。

`measure.py --build build` 用于在已知欠载后继续收集四个场景的证据，保留失败并返回非零，不代替 ctest。报告频谱使用最终失败状态的输出；不会绕过欠载或放宽音质门槛。
