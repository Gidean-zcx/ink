# phone-calls：电话（phone-sms 的第二块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/phone-calls/REPORT.md。

## 要做的

预算 350 行产品代码：`Phone`（拨号盘）、`PhoneRecents`（通话记录）、`PhoneCall`（通话中）；通话记录存进数据服务，按成员。号码可以从联系人选（`contacts.select`，已合入）。只到模拟器的 AT 模型，不打真电话。

## 从哪里看起

- 这三页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 通信服务 system/services/comm/（拨号、接听、挂断的能力，来电事件）；来电打断已经走通知服务。
- 联系人 system/apps/contacts/contacts.c；页面用 F1 控件语义、F2 短输入框、F4 通用列表。

## 边界

- 不改 4G 模组的固件和配置相关的代码路径；不真拨号。
- AI 拨号照现有的确认规则（D-105）走确认卡，不放松。
- 新代码放 system/apps/phone/；共用的文件（应用目录、CMake、能力注册、启动）只在末尾加行。同时在做的有 sms、settings-a、memo、power-pages，以及在修的 clock-alarm、ai-chat，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 模拟器里拨号 → 通话中 → 挂断，通话记录里有这一条，重启后还在；来电接听和拒接也记下（测试）。
2. 从联系人选号码拨出（测试）。
3. 换了成员看不到别人的通话记录；去掉检查，测试失败（反例）。
4. 每页有 AI 快照；按钮和 AI 走同一个能力；AI 拨号要确认（测试）。
5. 新操作进能力表；全量测试和内存检查通过；产品代码不超过 350 行。
