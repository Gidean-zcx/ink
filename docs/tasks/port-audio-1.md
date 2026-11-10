# port-audio-1：真机上的喇叭和麦克风

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-audio-1/REPORT.md。**只编译、跑宿主机测试，不碰设备**（上机由 Claude 做）。

## 背景

用户要明天早上能刷进去试用的版本（见 port-boot-1 的背景）。S31 端口的声音（`hal_audio_*`）和 Opus（`hal_opus_*`）都还是桩，闹钟、来电铃、录音备忘、读给我听都没声。今晚并行的还有 port-boot-1、port-storage-1、log-1、port-wifi-1，共用端口的几个文件，你只删自己的桩，改动集中。

## 要做的

预算：产品代码净增不超过 300 行。

1. **照原厂接声音**：编解码芯片、I2S 引脚、功放使能（可能经扩展芯片）、采样率、音量范围，全部照原厂代码（`ink-hw/upstream/Metalio-E-INK4-Plus/main/boards/metalio-e-ink4-plus/` 和它用的音频组件），写出处到行号。实现 `hal_audio_play_*`、`hal_audio_capture_*`、`hal_audio_get_info`、`hal_audio_route_set`（只有板载喇叭和麦克风；蓝牙路由返回不支持）。
2. **Opus**：照原厂用的 Opus 组件和参数实现 `hal_opus_*`；组件要联网下载的话，版本钉死、离线时编译给出清楚的提示，写明许可证。
3. **功放和省电**：不放声音时关功放，照原厂顺序，避免开关时的爆音。
4. `hal_afe_*`（唤醒词、回声消除）这次不做，保持“不可用”。

## 验收

1. S31 能编过，0 警告；端口单测普通版和 ASan 版都通过；播放、录音的状态机和功放开关顺序有宿主机单测（IDF 用替身），做反向对照。
2. 模拟器全量测试照旧通过；gcccheck 通过；做隐私扫描。
3. 报告里列一张“原厂一步 → 我们一步”的对照表，和上机时要听什么、看什么。
