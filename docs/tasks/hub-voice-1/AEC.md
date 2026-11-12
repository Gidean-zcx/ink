# 边播边听的回声消除

查阅：2026-11-12。仅源码与官方资料核查，没有接设备、测通道或改固件。

**有条件可行**。BH1098 的原厂驱动声明了麦克风＋回采两个输入通道；但不能从这句声明推导实物第二槽一定是正确的播放参考。更直接的阻碍是：原厂 ESP-S31 配置没有 ESP-SR 依赖，现有 AFE 不能直接按 ESP32-S3 的方式开启。

## 小智系怎么做

【实现】本机公开原厂源码固定提交 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`，以下路径均相对 [CloudZao/Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)：

- `main/audio/processors/afe_audio_processor.cc:21`～`:76`：从 codec 的 `input_reference()` 构造 `MR`（麦克风、播放参考），初始化 `AFE_TYPE_VC`、`AFE_MODE_HIGH_PERF`、`AEC_MODE_VOIP_HIGH_PERF`，分配优先用 PSRAM；`CONFIG_USE_DEVICE_AEC` 控制 AEC 初始启用。
- 同文件 `:160`～`:170`、`:200`～`:244`：采集数据交给 AFE `feed`，处理任务从 `fetch` 取净化后的单声道，重新拼成上行帧；VAD 状态变化通过回调通知应用。`:251`～`:265` 是 AEC 开关。
- `main/application.cc:153`：设备 AEC 与服务端 AEC 互斥。`:2177`～`:2200` 的应用状态处理选择本地 AEC、远端 AEC 或关闭；`:1905` 的 `AbortSpeaking` 发中止通知。回声抑制负责让播放声音不被误认成人声，中止仍需应用状态与协议配合。

【上游文档】[乐鑫 AFE](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/README.html)要求输入为按采样交错的 16 位通道；`M` 是麦克风、`R` 是播放参考。初始化后按 `get_feed_chunksize` 喂入，按 `fetch` 读取。这里引用的是 ESP32-S3 文档，不构成 ESP-S31 支持证据。

## 原厂这台设备有什么

【实现】`main/boards/common/audio/bt_audio_codec.cc:47`～`:49` 显式设置 `input_reference_=true`、`input_channels_=2`，注释“麦＋回采”。`:87`～`:105` 是 I2S slave、32 位立体声槽；`:166`～`:189` 按所有输入槽一对一读取，右移 12 位并限幅成 16 位，没有把第二槽提前丢弃。`:145`～`:158` 把单声道播放样本写入左右输出槽。

【实现】板级 `main/boards/metalio-e-ink4-plus/config.h:148`～`:153` 定义输入输出 16 kHz。`docs/bh1098-audio-protocol.md:29`～`:40` 说明 BH 提供 I2S 时钟、模式 1 同时提供本机播放和麦克风输入；不同模块模式的左右槽来源还需匹配固件。该文档没有给出能独立证明第二槽信号来源的测量。

【实现】`main/idf_component.yml:10`～`:13` 仅在 `esp32s3`、`esp32p4` 引入 `esp-sr ~2.2.0`；`main/Kconfig.projbuild:35`～`:50` 默认关闭音频处理和 AEC，明确写“S31 无 esp-sr 时关，走 no_audio_processor”；`main/audio/audio_service.cc:56`～`:60` 也确实选择无处理分支。**有 AFE 源文件不代表 S31 编过或跑过它。**

## 接到我们系统需要什么

1. 由主会话先核对原厂 S31 上可用的 AEC 实现和许可、内存、CPU 占用；不要仅打开 `CONFIG_USE_DEVICE_AEC`。优先直接复用验证过的原厂实现，保留 MIT 声明。
2. 上机先核对 BH 模式 1、蓝牙耳机模式下两槽分别是什么：静音、仅播测试音、仅讲话、播音同时讲话。检查第二槽与播放数据的相关性、延迟、削顶，以及音量改变后的关系。此处需要采集证据，本任务没有做。
3. 在 HAL 后保留麦克风和参考两槽，按原厂缩放、时序喂 AFE；净化后的单声道再送现有 16 kHz/60 ms Opus。测同步漂移、回声残留、双讲时漏字与误打断，再接现有停止播放、`rt.close` 路径。
4. 【估算】只有参考有效、两路对齐、S31 有可运行的算法和足够预算，才适合免按键打断。在这些条件验证前，保守建议继续按键中止；这不是把免按键打断取消为需求。

本任务没有增加设备能力、AFE 任务、音频驱动或通道设置。
