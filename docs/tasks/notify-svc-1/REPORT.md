# notify-svc-1 执行报告

基线：`36473cc4933e6fea2f2a78a4fbec16186d3be3f4`。范围：通知服务；不操作设备、不推送、不修改其他应用。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 勿扰、例外、跨午夜、反向对照 | 进行中 | 待实现和验证 | 待补 |
| 2 成员隔离和重启 | 进行中 | 待实现和验证 | 待补 |
| 3 多动作、快捷回复、策略 | 进行中 | 待实现和验证 | 待补 |
| 4 老通知和老设置 | 进行中 | 待实现和验证 | 待补 |
| 5 改前改后行为表 | 进行中 | 待补 | 待补 |
| 6 构建、测试、体积、接口 | 进行中 | 待运行 | 待补 |

## 没做到的

【实现】原文已入库，已接入跨午夜时段、成员偏好、例外、忙时、多动作和回复转发。正在运行服务验收。收藏枚举保留为 3，当前按不放行处理，等待 contacts-3 提供收藏接口。

【实测，阶段结果】seed 10、main 136、restart 22 条断言全部通过。普通通知、升级恢复、AI 回合结束与新增通知四项定向回归全部通过；ASan/UBSan 9 项全部通过。普通来电即时放行，条件来电仍后台过滤；勿扰状态不变时不再发变化事件；启动时迁移回执放 PSRAM，勿扰查询不复制回复文本，未放宽栈预算。

## 和设计不一致的

【实现】Pebble 的 UI、手机传输、cron 和偏好文件未链接；跨午夜三个完整函数直接编译，其余用现有成员存储、时间和能力总线适配。手动关闭只关闭手动来源，不临时覆盖仍有效的时段（任务要求任一来源生效）。日历忙时已通过现有提醒 API 接入，时间服务改动 0 行。

## 改过的已有测试

仅在 `tests/integration/ai_surface.c` 的 FOR_AI 表末尾追加 5 个新能力；未改任何已有断言、期望、预算或超时。新增服务验收脚本与用例。

## 要用户定的

收藏接口由 contacts-3 后续接；可信 asked 上下文需要能力层提供受信转发入口，当前保守保留确认。没有停工等待。

## 顺手发现

【实现】现有可信 `_meta.asked` 仅保留在能力层内部，公共嵌套调用接口不能继承。带该标记的 hub AI 请求经本服务转发时仍会再确认；本任务不改策略层，也不伪造 `CONFIRMED`。验收要求的触摸直发和 AI 无标记确认路径已实测。

## 新增的机制

仅实现任务书要求的勿扰时段、例外、多动作和快捷回复；复用现有时间服务、后台执行器和成员数据集合。

## 要改的文档

`docs/system/system-services-design.md` §1.3 需要列入 `notify.act / notify.reply / notify.dnd_get / notify.dnd_set / notify.replies_set`。此文件不在任务允许修改的范围；现有 `services-mid-capability-doc` 因此失败，未跳过或改断言。通知页和控制中心接入清单见下文；共享能力目录按任务书明确允许的例外重新生成。

## 接口清单（notify-ui-1 / control-1）

- 设置 `notify.dnd`：原名、原布尔语义；改为每成员保存。旧的全设备值在首次启动归给当前成员，其他成员默认关闭。
- 状态 `notify.dnd_active`（布尔）、`notify.dnd_reason`（`off/manual/schedule/calendar`）、`notify.dnd_until`（UTC 秒，手动为 0）；事件 `notify.dnd_changed`。C 接口 `ink_notify_dnd_state()` 同时返回 `slot`（0–4，其他来源为 -1）。
- `notify.dnd_get`：返回上述状态、`manual`、`calls/repeat/calendar`、`slots` 和 `replies`。`notify.dnd_set`：可只给例外；改时段必须同时给 `slot/days/start/end`。`days` 周日为 bit 0，工作日 62、周末 65；开始/结束用当天分钟数，结束不包含；days=0 禁用该条。
- 来电枚举：0 无人、1 所有人、2 联系人、3 收藏（保留，当前不放行）。`repeat` 放行 15 分钟内同号码再次来电；内存保留最近 8 次来电，按成员区分，重启清空。
- `notify.replies_set(index,text)`：序号 1–5，每人独立，最多 31 个字符。默认“好的 / 是 / 不是 / 给我回个电话 / 我晚点给你回电话”。
- `notify.list` 和 `ink_notify_next` 增加 `actions`（最多 3 个，`label/cap/args`）和 `reply`（`cap/args`）。旧 `action/action_args` 字段与函数保留；旧单动作同时映射进新数组。
- `notify.act(id,index)`：序号 1–3；`notify.reply(id,index)` 或 `notify.reply(id,text)`：快捷回复序号 1–5。回复目标的 args 是静态参数模板，转发时补 `text`。结果保留目标能力的状态、回执和结构化结果；目标能力自己裁决确认和成员权限。
- 短信通知：动作 `sms.thread(peer)`，回复目标 `sms.send(to)`；未接来电：`call.dial(number)`、`sms.compose(number)`。

## 架构审视

【实现】通知服务只持有通知箱和自己的成员偏好，联系人通过公开匹配函数查询，时间通过现有 API 读取，外部动作通过能力总线转发。按钮与 AI 共用同一组能力；新能力已追加到 AI 覆盖表。服务不操作 LVGL，不新增常驻任务或线程。

## 反向对照（手动运行，不依赖 CI 历史）

【实测】临时将 `vendor/pebble/schedule.inc` 的 `if (from < to)` 改为 `if (from != to)`，使跨午夜错误地走普通区间；重编 `sim-notify-model`，以 `INK_NOTIFY_MODEL_PHASE=cross` 运行。退出 1，8 条断言失败，日志明确含 `23:30 matches the overnight period` 和 `06:59 matches the previous day's overnight period`。

【实测】临时撤回 `changed()` 的无变化事件筛选，重编 `sim-configured`，运行 `scenario-ai_turn_end-metalio-plus`。ctest 退出 8，`hub.idle_disconnects: metric not recorded`。两份源码均逐字节恢复；跨午夜提取文件 SHA-256 再次核对一致。修复后的同一场景此前已通过，恢复后还会随最后一轮验证复测。
