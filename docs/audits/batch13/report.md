# batch13 独立复查报告

（审计子代理，一轮，检出 `d52861ef`，范围 `61f55c19..d52861ef`；运行环境不让子代理写报告文件，主会话按它的回报保存。）

已跑：全量 316 项里 313 项通过、3 项照旧跳过；10 个改过的 .c 文件 gccscan、rscan 无报告；一次性实验证实了中 1（已恢复）。没跑 ASan 和 S31。

看过没问题：各页面按行数分配的固定数组（settings_pages 有 `ROWS_MAX` 静态断言，boot_flow、weather、ai_pages、今日卡片、蓝牙都在上限内或有边界检查）；`via` 转调没有绕开孩子限制；`memo.record` 沿用调用方来源，D-105 没放松，`memo.voice` 只给应用；译文占位符核对和 `format_arg`；S31 中断写法。

## 发现

1. **中：天气页第 3 天那一行的 AI 快照标题是“更新时间”。** `system/apps/weather/weather.c` 的 `texts[6]` 不够用：第 i 天用 `texts[2+i]`，第 3 天是 `texts[5]`，接着“更新”那一行又覆盖 `texts[5]`，快照里 `day3` 的 label 变成“11月2日 08:00 更新…”，屏幕上是“周X · 小雪”；中枢固定发 3 天，每次都出现。复现：`tests/weather/weather.c` 加 `expect("周")` 即失败。修法：`texts[7]`，“更新”用 `texts[6]`，加 day3 快照检查。
2. **中：待整理的语音备忘先被手写过，转写回来会盖掉手写的字（读代码）。** `system/apps/memo/memo.c`：`memo.edit {id}` 没拦住 pending 的备忘，`memo.save` 后 pending 仍是 true，联网后 `voice()` 用转写文字覆盖正文。修法：pending 的备忘拒绝 `memo.edit` 或转到 PENDING 页；填字时正文不空就接在后面。
3. **中（待证实）：转写一直失败时反复重传，静音的录音永远“待整理”。** `memo.c` 的 transcribed、voiced、sweep：写库持续失败（如卡满）会循环重选同一条、整段重传；转写成功但文字为空时一直 pending，每次打开列表、每次联网都重传。修法：本次开机记下失败过的 id、逐步拉长重试间隔；空文字当完成并填占位说明。
4. **中：后台执行器上仍同步打开成员的数据集合。** `memo.c` 的 mutate（`memo.voice` 走这里）和 play 都在 BG 上用 `ink_db_open_member`（memo-record 报告里这正是让文件列表读不完的写法，当时只改了 sweep）；sweep 里每个录音文件都调 `find_audio` 逐条解析全部记录，复杂度“文件数×记录数”。修法：用 `ink_app_db_open_async` 或先试 cached、不在缓存就回“稍后”；sweep 先收集已有 audio id 再比对。
5. **低：** `system/services/audio/media_service.c` `audio.transcribe` 在中枢不回 `rt.asr.done` 时一直占着（`TX.waited` 只在 `!ink_rt_ready` 时累加，期限到了回复超时但 `TX.c` 不清空，之后每次都回 busy）。修法：`TX.ended` 后无条件计数，或时限到 0 时 `tx_end`。
6. **低：** `media_service.c` 的 `REC.why` 由 `cap_record_stop` 在调用方任务里不加锁写，媒体执行器同时可能写 `max_length`、`member_changed`、`interrupted`，“不要了”可能丢掉、录音被留下。修法：在锁里写，已是 cancelled 时别处不覆盖。
7. **低：** `system/policy/settings.c` 走 `via` 的分支直接返回，不再登记撤销、不处理 `expected_revision`，AI 用 `settings.set` 改飞行模式或 Wi-Fi 不能 `system.undo` 了。修法：转调成功后补登记撤销，或在能力表写明不能撤销。
8. **低：** `memo.c` `memo.voice` 收到不带 `/` 的 path 会崩溃（`strrchr` 返回 NULL 直接给 `strcmp`），只有应用能调。修法：先判空。
9. **低（待证实）：** `system/services/backup/backup_service.c` 的 `mark_failed` 和 PH_SAVED 也是“先写状态、后结束任务”，和 update-flake 同一种写法。修法：先 `ink_task_end` 再改 phase。
10. **低：** S31 串口一行超过 255 字节会悄悄截断（`ports/esp32s31/s31_hal_map.h` `S31_LINE_CAP 256`），NT26 文本模式 UCS2 的 70 字短信是 280 个十六进制字符；现在还没有代码调用。修法：调大上限或截断时返回错误并计数。
11. **低：** `memo.c` 枚举 `RECORD` 和宏 `RECORD` 同名，靠代码顺序才没出问题。修法：宏改名（如 `ID_ARGS`）。

SEVERE=0 HIGH=0 MEDIUM=4 LOW=7
