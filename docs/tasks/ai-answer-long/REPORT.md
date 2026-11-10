# ai-answer-long 报告：AI 长回答不丢字

（Claude 子代理完成；运行环境不让子代理写报告文件，主会话按它的回报整理保存，数字和结论照原文。）

检出 `ink-codex/ai-answer-long/repo`，分支 `claude/sub-ai-answer-long`，基线 `4099f390`。

## 结论

设备一侧的长回答不再丢字：一轮最多存 32 KB，面板上按页逐字显示，能翻到最后一页；超过上限在整字处截断，末尾写“回答太长，只显示了前面部分。”。**没做到**：朗读超过 45 秒的回答仍会被截断（协议一轮 45 秒，中枢也有限制），见“没做到的”。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 约 6 KB 中文回答（6002 字节，13 段流式到达）页面和快照逐字一致、能翻到最后一页 | 通过 | 【实测】新测试 `ai-answer-long` 用 AI 的下一页能力逐页读快照里的 `answer`，拼起来与中枢脚本发出的全文 `strcmp` 相等；28 页，`页码=28/28` | `ctest -R '^ai-answer-long$'` |
| 1b 超过上限有明确提示 | 通过 | 【实测】37797 字节的回答：最后一页（152/152）以提示结尾；保留部分逐字等于原文开头，切在整字处；快照里有提示；计数 `ai.answer_cut` 为 1 | 同上 |
| 1c 流式到达时页面不乱 | 通过 | 【实测】到达期间取样 8 次以上：停在第 1 页，总页数只增不减，文字始终是全文开头；“边到边分页”和“整篇一次分页”的分页点完全一样 | 同上 |
| 1d 朗读不截断 | 部分通过 | 【实测】语音问同一篇 6 KB 回答，朗读 555 帧（超过下行缓冲 512 帧），中枢按实时速度发：实际播放至少是应有时长的 97%，自动放完，溢出和失败计数都是 0。为在 45 秒内结束，测试用很快的语速（每秒 60 字） | 同上 |
| 2 原有测试 | 通过 | 【实测】`100% tests passed out of 302`（hub-contract、hub-json-crosscheck、hub-simulator-e2e 三项本来就跳过）；新测试内部内存最低剩余 38000 字节，预算 32768 未放宽 | 全量 |
| 2 内存检查 | 通过 | 【实测】`build-asan` 全量编译，选 `ai\|voice\|scenario\|integration-member\|selftest` 共 61 项，`100% tests passed out of 61` | `ctest --test-dir build-asan -R ...` |
| 2 S31 | 通过 | 【实测】`ports/esp32s31/build.sh` 退出 0 | |
| gcc | 通过 | 【实测】gcccheck `3 compiled, 0 with findings`；gccscan、rscan 无发现 | |
| 能力表 | 通过 | 没有新能力；`capabilities.py --check` `PASS: 438 definitions` | |

反向对照：撤掉产品改动，6 KB 逐字一致失败（只收到 1874/6002 字节）、截断提示 5 项失败、语音回答文字检查失败；只把增量分页故意改错，“分页点一致”失败（28 对 37 页）。

## 做法

产品代码新增 37 行、删除 13 行（预算 200）。

- `system/include/ink/sys/ai.h`：`INK_AI_ANSWER_MAX 32768` 和提示文字 `INK_AI_ANSWER_CUT`。32 KB 约 1 万个汉字，远超一次口头回答；缓冲在外部内存（PSRAM），内部内存不变。
- `system/services/ai/ai_service.c`：`ANSWER_CAP` 用上面的值。原来放不下的一段整段丢掉，后面较短的段还可能接上造成乱序；现在先给提示留位置，放不下时在整字处截断并接上提示，这一轮后面的文字不再追加，计数 `ai.answer_cut`；三处清空回答的地方一起复位截断标记。
- `system/ui/shell/ai_panel.c`：面板文字缓冲和页起点数组放大到 32 KB（PSRAM）；改成增量分页（回答变长时从最后一页重排，前面的页不动；原来每来一段都从头量一遍，32 KB 时是平方级开销）；量字时一页不超过 4 KB 的临时缓冲。快照和翻页能力沿用原来的。
- `CMakeLists.txt`：注册 `ai-answer-long`。新增 `tests/ai_answer_long/`（测试、中枢脚本、场景脚本、预算；期望全文从中枢脚本读）。

## 没做到的

1. **朗读超过约 45 秒的回答仍会被截断**：一轮语音预算 45 秒（`ASK_TIMEOUT_US` 和发给中枢的 `budget_ms`），中枢在朗读帧发完后才回答，超时后设备结束这一轮。真实中枢也有限制：`hub/ink_hub/gateway/wire.py` 的 `ANSWER_BYTES = 1800`（文字只发 1.8 KB），`flow.py` 规定朗读超过 450 帧（27 秒）报错，`hub/tests/contract/test_protocol.py` 断言了 1800。要整篇读完，中枢和协议都要改（先回答再发朗读、有活动就延长时限，或按页朗读）。
2. 对话记录页（`AiHome`）现在只有空状态，这次只做了 AI 面板的分页。
3. 朗读期间仍不能翻页（AiSpeak 原有规格）。

## 改过的已有测试

无。

## 顺手发现

- `sim_hub` 默认 2 倍实时速度发朗读帧，超过约 60 秒的朗读会让 512 帧的下行缓冲溢出；真实中枢按实时速度发，新测试用 `tts_speed: 1`。
- `snap-AiLongCut.png` 里快速连续翻页后行首有一点模拟的墨水屏残影，`clean-` 版本里没有。

## 要改的文档

- 已改：`docs/hub/device-protocol.md` 第 30 行、`docs/hub/hub-architecture.md` §1.3（设备一轮 32 KB）。
- 待定：`device-protocol.md` 第 75 行“中枢一轮 ≤ 1.8 KB”是中枢规则，等中枢放宽时一起改；backlog 记下长朗读的限制；`docs/tasks/ai-chat/REPORT.md` 第 20 行“上限 2047 字节”已过时（历史报告）。
