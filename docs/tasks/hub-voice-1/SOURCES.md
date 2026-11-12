# 来源、复用边界与待准备的模型

查阅日期：2026-11-12。

## 本次迁入的逻辑

小智服务端：[xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server/tree/4b1a54a41c28112bc06b1ab605d2ba723f678ad3)，固定提交 `4b1a54a41c28112bc06b1ab605d2ba723f678ad3`。本任务只读副本在忽略的 `work/xiaozhi-esp32-server/`；产品保留 MIT 声明和 [许可证](../../../hub/ink_hub/speech/XIAOZHI-LICENSE)。没有引入它的模型自动下载、连接管理、线程框架、日志正文或凭据输出。

路径均相对上游 `main/xiaozhi-server/`：

| 原文件 | SHA-256 | 接入位置与差异 |
| --- | --- | --- |
| `core/handle/abortHandle.py` | `0e60679c6e281d4969b0c6a4952e7071962c16f06fe55c7764dbf7753bc272c2` | 网关既有 `end_session` 的取消＋输出 guard；历史按听到的内容裁剪 |
| `core/handle/receiveAudioHandle.py` | `2cf58dc7e96f7ccf673053fd9ebee7302bce62641794c22146cfaae8e82f0949` | `voice/stream.py`：边收边识别、VAD 收尾，不搬账号和意图层 |
| `core/providers/vad/silero.py` | `aa375cd237c262338b391f8826f35bfe9b3c9e4c77a197bfe75490806d825114` | `speech/vad.py`：512 采样窗、64 采样上下文、独立状态、0.5/0.2 双阈值；用音频时长计算静音，不依赖宿主墙钟 |
| `core/providers/asr/doubao_stream.py` | `ba5bfb455ecf961800ca8b66035bd8fd0f3e1b6e29e23b6732b53456e0d25dd0` | `speech/doubao.py`：gzip 二进制头、上行终止帧、结果流；以 aiohttp 替换 websockets |
| `core/providers/asr/aliyun_stream.py` | `f2e58a9c857d2a0937c2778a018e9cfde23ccf8294af3f4a528d3cf803db54f8` | `speech/aliyun.py`：Started 后送音频、SentenceEnd/Completed；不搬凭据刷新 |
| `core/providers/tts/base.py` | `af2006bb76e0bb66a24ae764ac33eb106035d0ea55d239a48caa149750a6f96f` | `voice/stream.py`：首句用逗号提前切；保留原文标点，按本项目合同至少 8 字 |
| `core/providers/tts/aliyun_stream.py` | `16cbc677d28e87f4295d8ab07285b07ca60c5e4c48aa10ad415ec06c7918f0b3` | `speech/aliyun.py`：FlowingSpeechSynthesizer Start/Run/Stop 和二进制 PCM |
| `core/providers/tts/huoshan_double_stream.py` | `c3b93c746001ab3b494e34e88b0344554fc829696aabfc9f873804c509b1ebae` | 借其 `audio_params` 请求结构；豆包朗读改接官方 HTTP Chunked-V3，不宣称逐字复制双向协议 |

静音时长按本项目设计的 0.3～0.6 秒收尾预算暂用 600 ms，小智 Silero 默认值为 1000 ms；没有把这个选择写成实机校准值。

这是有出处的适配和裁剪，未把整个小智 Python 项目作为依赖导入。保留一条可背压、可取消的链，未另外增加模型/朗读队列或设备连接管理。成员、儿童提示和工具策略使用本项目既有路径；没有补造一个额度或内容审核系统。

## 另外核查的官方接口

- [Whisper server](https://github.com/ggml-org/whisper.cpp/tree/d1be6fde11ac6e0407606b4e42fe72d34add8037/examples/server)，本机源码固定 `d1be6fde11ac6e0407606b4e42fe72d34add8037`，1.9.5-dev，MIT。`server.cpp:992`～`:1006` 的 `is_connection_closed` 原生 abort 回调是取消依据；没有修改上游源码。
- [sherpa streaming Paraformer 示例](https://github.com/k2-fsa/sherpa-onnx/blob/master/python-api-examples/streaming-paraformer-asr-microphone.py)，新增适配使用 `OnlineRecognizer.from_paraformer`。小智原来的 sherpa 接法为离线识别，因此没有把它伪装成流式。
- [豆包 ASR](https://docs.volcengine.com/docs/doubao-voice/bidirectional-streaming-automatic-speech-recognition-websocket?lang=zh)、[HTTP Chunked-V3 TTS](https://docs.volcengine.com/docs/doubao-voice/http-chunked-sse-unidirectional-streaming-v3?lang=en)、[阿里流式 TTS](https://help.aliyun.com/en/isi/user-guide/streaming-text-tts-wss)。只用假服务器检查报文、音频分片与取消，不等于真实服务互操作验收。

## 未下载的可选模型

| 文件 | 来源与大小 | 用途与状态 |
| --- | --- | --- |
| `silero_vad.onnx` | [Silero 官方文件](https://github.com/snakers4/silero-vad/blob/master/src/silero_vad/data/silero_vad.onnx)，页面标 2.22 MB，MIT | 替换默认能量判断；需 numpy、onnxruntime，本机均未安装；未下载或跑真实模型 |
| `encoder.int8.onnx`、`decoder.int8.onnx`、`tokens.txt` | [sherpa 官方模型说明](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-paraformer/paraformer-models.html)：`sherpa-onnx-streaming-paraformer-bilingual-zh-en`，文档标 int8 编码器 158M、解码器 68M；词表【估算】小于 0.2 MB，精确大小待主会话下载前查元数据 | 中英本地真正流式识别；需 numpy、sherpa-onnx，本机均未安装；未下载。取得前还需固定模型修订、核对模型许可和哈希 |

没有往仓库加入模型或二进制。Whisper 使用用户已有的 `ggml-large-v3-turbo-q5_0.bin`，574041195 字节，SHA-256 `394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2`；本任务不重新下载。可选模型和运行库要由用户选择后主会话准备，不能把当前的能量判断写成已经验证的 Silero。
