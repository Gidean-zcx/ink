# memo-record 报告：录音备忘

（Claude 子代理完成；运行环境不让子代理写报告文件，主会话按它的回报整理保存，数字和结论照原文。）

检出 `ink-codex/memo-record/repo`，分支 `claude/sub-memo-record`，基线 `80ba2eda`。

## 结论

- 编码定 **Opus**（D-112），数字见下。
- “`audio.record` 没做”已过时：媒体服务早有开始、停止、焦点打断、换成员停录、每 5 秒落盘，写 Ogg Opus。这次补上：取消录音、交给中枢转写、录音中按 AI 键的提示和“停止录音”按钮、MemoRecord 和 MemoPending 两页、语音备忘和录音文件的关联、播放、断电后把录了一半的文件收成备忘。
- 产品代码新增 311 行、删 23 行（预算 400）：memo.c +181/−20，media_service.c +114，ai_service.c +9/−1，ai_panel.c +6/−2，ai_chat.c +1。
- 【实测】全量 `100% tests passed out of 304`；build-asan 里受影响的 10 项 `100% tests passed out of 10`（全量交给 CI）；S31 退出 0；gcccheck 7 个文件 0 处（测试里一处 `%.300s` 改成 `%.150s`），gccscan、rscan 无输出。

## 验收表

| 编号 | 结论 | 证据 |
| --- | --- | --- |
| 录 10 秒保存 | 通过 | 【实测】“说一句”进 MemoRecord，10 秒后“记好了”进 MemoPending；卡上有 `/me/memos/<时间>.opus`，Ogg 时长 9.5～12.5 秒，索引里是音频类型，编号和备忘里存的一致（ctest `memo-record`） |
| 取消不留文件 | 通过 | 【实测】录 3 秒“不要了”，文件数、备忘数不变；去掉删除那一行就失败 |
| 录音中来电 | 通过 | 【实测】录 4 秒来电，录音停下；挂断后多出约 4 秒的一条备忘，文件能读 |
| 录音中按 AI 键 | 通过 | 【实测】面板显示“正在录音，先停了再问”和“停止录音”，快照里有 `audio.record_stop`；点一下停录，录到的部分存成备忘 |
| AI 在听时不能开始录音 | 通过 | 【实测】按住 AI 键时调 `memo.record` 失败，提示“AI 正在听或在说，或者正在通话，等它结束再录” |
| 转写后有文字、能播放 | 通过 | 【实测】断网录、联网后 sim_hub 的假转写回来，备忘有文字，页面从 MemoPending 换成 MemoDetail；`memo.play` 后在放、时长对 |
| 换家人看不到 | 通过 | 【实测】切到孩子：语音备忘 0 条、`/me/memos` 索引 0 项；孩子用大人的编号调 `memo.play` 返回 INVALID |
| 录音中断电 | 通过 | 【实测】录 12 秒断电，同一份闪存和卡重启，打开备忘列表后这段录音自动成为一条备忘，Ogg 能读、能放 |

反向对照：`tests/memo_record/reverse.py` 6 个变体（cancel、ai-key、busy-say、words、member、power-cut）撤掉改动都失败，恢复后通过（手动跑，不进 ctest）。

## 编码对比（一分钟录音，`run.py --phase cost`，模拟器成本模型）

| | Opus 24 kbps（现有，60 ms 一帧） | IMA ADPCM 16 kHz 4 bit |
| --- | --- | --- |
| 处理器 | 【模型】编码约 1000 帧 × 240 万周期 ≈ 7.5 秒，约占一个 320 MHz 核的 12.5%～16% | 【估算】约 0.25% |
| 内存 | 【实测】编码器状态 30 KiB 在 PSRAM，audio 任务栈 26 KB 在 PSRAM，内部 RAM 不增加 | 【估算】几 KB |
| 文件 | 【实测】60.06 秒 148,844 字节（模拟器假编码器）；真 Opus 约 180 KB/分钟【估算】 | 【估算】480 KB/分钟 |
| 转写流量 | Opus 帧直接走 `asr.stream` | 约 2.7～3.3 倍，中枢还要新增解码，或设备先转 Opus（仍要带编码器） |

决定 Opus：内部 RAM 不增加，和 AI 语音共用一套编码、固件不多一个编解码器，文件和流量约为 ADPCM 的三分之一；代价是录音期间约 12～16% 单核占用（成本模型里的估算值，真机要上机确认）。

## 做法

- **媒体服务**：新能力 `audio.record_cancel`（结束并删文件）和 `audio.transcribe {path}`（后台把录音文件的 Opus 包按原样送进中枢的 `asr.stream`，和听写同一个协议，中枢不用改；约 30 秒连不上就失败）；`audio.record` 因 AI 在听、通话或麦克风被占失败时回执带中文提示。
- **AI**：录音中按 AI 键不开新一轮，面板提示并给“停止录音”按钮（快照里同名动作），计数 `ai.mic_unavailable`。
- **备忘应用**：记录加 path、audio（文件编号）、ms、pending 四个字段；内部能力 `memo.voice`（只给设备内部用，AI 看不到）存待转写的备忘或填入转写文字，和原写入走同一处；订阅 `audio.record_done` 把 `/me/memos/` 下非取消的录音存成备忘；一个后台补漏过程（联网、打开列表、存好一条、转写完成时运行）先把没有对应备忘的录音收成备忘，再一次转写一条；`memo.record` 录到 `/me/memos/<时间>.opus`（调用来源原样传下去，AI 发起仍要人确认）；`memo.play {id}` 按文件编号找路径。
- **页面**：MemoRecord（圆环加波形、“正在记”、按分钟更新的说明、“不要了”“记好了”）；MemoPending（播放条、删除、麦克风圆、“录音已保存”）；有录音的 MemoDetail 顶部播放条；列表底部“说一句”加“写一条”。

## 没做到的

- MemoDetail 里“这段话里有 3 件事”（要 AI 拆事项）和“已记下 · 今天 18:42”时间行；有录音的页面标题还是“备忘”（画板是“随口记”）；播放条是静态的（墨水屏不做动画，暂停和跳转可用 `media.*`）。
- AI 发起 `memo.record` 确认后会开始录，但不会自动跳到 MemoRecord 页。
- 补漏只用已加载的备忘集合：开机后从没打开过备忘应用，要等打开或存一条才会接着转写（见顺手发现 1）。
- 通话期间备忘写不进闪存（通话占着闪存写入），挂断后才写好；原来的 `memo.save` 也这样。
- 模拟器里卡的写入直接到宿主机文件，断电测试接近 12 秒；真机最多丢最近约 5 秒（落盘间隔）。
- 转写文字最长 1024 字节（约 340 个汉字），中枢那边一次转写的上限没核实；真机处理器占用。

## 改过的已有测试

- `tests/integration/ai_surface.c`：登记新能力（AI 可用：`memo.record`、`memo.play`、`audio.record_cancel`、`audio.transcribe`；AI 不可用：`memo.voice` 并写原因）。
- `docs/system/system-services-design.md` §1.3 音频行补 `audio.record_cancel`、`audio.transcribe`（services-mid-capability-doc 要求）。

## 顺手发现

1. files-app 测试里，联网时在后台执行器上 `ink_db_open_member(备忘集合)` 会让文件列表一直读不完（确定性失败），补漏改用 `ink_db_open_cached_member` 后通过；根因没查。
2. `ink_rt_send` 在队列满时序号也加一，接收方会看到断档。
3. 测试里 `%.150s` 截断可能截在半个汉字上，metrics.json 就不是合法 UTF-8。
4. `ai_service.c` 里原来“正在录音”的分支现在基本走不到，没删。

## 截图

检出的 `build/test-out/memo-record/main/`：`clean-MemoRecord.png`、`clean-MemoPending.png`、`clean-MemoDetail.png`、`clean-Memo.png`。
