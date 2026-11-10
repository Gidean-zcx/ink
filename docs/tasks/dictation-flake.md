# dictation-flake：听写测试在 Linux CI 上失败、换随机种子也失败

通用规矩：`ink-codex/briefs/subagent-rules.md`（仓库外，和仓库同级的工作目录）。

## 现象

- Linux CI（GitHub Actions，提交 61f55c19）上 `integration-dictation` 失败 6 条，第一条是 `tests/integration/dictation.c:572` 闹钟那一步的 “over”（被闹钟拿走麦克风后 3 秒内没回到空闲），后面几条是连锁的：“what was said goes in”、“does not start under one”、`start_talking` 里的 “the microphone key is there”“listening”、“heard”。同一提交在 macOS 本机通过。上一次 CI 通过的提交是 27fd3740；这之间合入了 reader-continue、ai-icon、calculator、airplane-mode。
- CI 里这个测试的指标和本机不同：`task.dictatetest.flash_stall_ms` CI 55 毫秒、本机 1.6 毫秒；`net.retransmissions` CI 2、本机 1；`hub.asr_cancelled` CI 11、本机 10，`hub.asr_done` CI 11、本机 12。
- 本机用 `--seed 7` 跑同一个测试也失败，位置不同：`dictation.c:154` 的 “listening (0)”（193.6 秒，按了话筒 5 秒内没进入听写）。种子 2～6、8～11 通过。

命令（在你的构建目录里）：

```bash
build/sim-dictation --profile simulator/profiles/metalio-plus.json --budget tests/integration/dictation.budget.json \
  --script tests/integration/dictation.script --assets build/assets --content build/test-content \
  --out build/test-out/dict-seedN --quiet --no-random-outages --seed N
```

## 要做的

1. 查清两种失败各自的原因：是产品的问题（例如被闹钟打断后等中枢的最终结果没有上限、某种重连或重传下听写状态卡住、话筒键按下被拒的原因），还是测试的时间假设太紧（例如闹钟那一步只等 3 秒，而来电那一步等 10 秒）。在报告里用时间线（事件和时刻）说明，不要只给结论。
2. 是产品问题就修产品，配一个能稳定复现的测试（例如固定一个会触发的种子，或在测试里注入同样的网络重传）。是测试假设的问题，就按产品的真实合同改测试（等到明确的状态或事件，不加盲等，不放宽到失去意义）。
3. 改完用种子 1～40 各跑一遍，全部通过；再跑全量测试。

## 边界

- 只碰听写相关的代码和测试（`system/services/text/voice_input.c`、`tests/integration/dictation.c` 等），不改别的任务的文件。
- 同时在做的有 audit12-fix（飞行模式、AI 面板、阅读器）、sms-multipart、weather、memo-record、i18n-1。

## 验收

1. 报告里有两种失败的时间线和原因、改了什么、为什么这样改是对的。
2. 种子 1～40 全过；全量测试和内存检查通过；S31 能编过（如果改了产品代码）。
