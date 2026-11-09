# sms：短信（phone-sms 的第三块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/sms/REPORT.md。

## 要做的

预算 350 行产品代码：`Sms`（会话列表）、`SmsThread`（一个会话）、`SmsCompose`（写短信）。短信会话存进数据服务，按成员（查缺口报告 B12：现在只在内存里留 16 条）；长短信；发送失败和结果未知的回执如实显示。收件人从联系人选（`contacts.select`，已合入）。只到模拟器的 AT 模型，不发真短信。

## 从哪里看起

- 这三页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 通信服务 system/services/comm/（发短信的能力、`message.received` 事件）；通知服务里收到短信的提醒。
- 写短信：短的用 F2 短输入框，长的用 text-long 的长文编辑页（已合入）；列表用 F4。

## 边界

- 不改 4G 模组的固件和配置相关的代码路径；不发真短信。
- AI 发短信照现有的确认规则（D-105）走确认卡，不放松。
- 新代码放 system/apps/sms/；共用的文件只在末尾加行。同时在做的有 phone-calls、settings-a、memo、power-pages，以及在修的 clock-alarm、ai-chat，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 收到的短信进会话，重启后还在；发出的短信显示结果（成功、失败、未知）（测试）。
2. 长短信（超过 70 个汉字）发送和显示正确（测试）。
3. 换了成员看不到别人的短信；去掉检查，测试失败（反例）。
4. 每页有 AI 快照；AI 发短信要确认（测试）。
5. 新操作进能力表；全量测试和内存检查通过；产品代码不超过 350 行。
