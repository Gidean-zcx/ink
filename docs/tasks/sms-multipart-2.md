# sms-multipart-2：长短信返工（参数表和确认卡不放大）

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/sms-multipart-2/REPORT.md。原任务书是 docs/tasks/sms-multipart.md，要求不变，下面只说改法。

## 背景

第一版在分支 `claude/sub-sms-multipart`（你的克隆里是 `origin/claude/sub-sms-multipart`），复核结论写在该分支的 `docs/tasks/sms-multipart/REPORT.md` 开头。**收短信那一半是好的**：PDU 模式、拼接头、两种编码、`sms.parts` 断电不丢、乱序和重复、缺段超时。**发短信那一半的做法不对**：为了让最长 5 段的正文放进一次能力调用，它做了三处放大：
- `INK_ARGS_TEXT` 从 416 放大到 1152；
- 确认卡说明 `INK_CONFIRM_DETAIL` 从 160 放大到 1152；
- 事件 JSON 缓冲从 1024 放大到 2560。

后果：代码里有 27 处参数表放在任务栈上，`exec.bg` 的栈估算超过了 8192（`memo-record` 失败）；确认卡原来“说明超长就拒绝、不藏对象”的规矩也被破坏了（`integration-confirm` 失败 10 条）。

## 要做的

预算：在第一版的基础上，产品代码净增不超过 120 行。

1. 先合并 `origin/claude/sub-sms-multipart`，解决和当前 `claude/system-layer` 的冲突。之后收短信那一半不改。
2. **改回去**：`INK_ARGS_TEXT` 恢复 416；确认卡说明恢复 160；事件 JSON 缓冲、确认列表缓冲、确认卡分页数组都恢复原值。和第一版相比，`args.h`、`confirm.h`、`cap.c`、`confirm.c`、`confirm_card.c` 应该没有改动，除非有别的理由，要写清楚。
3. **长正文由通信服务自己保管**：
   - 写短信页发送时，不把整段正文塞进参数表，而是传一个草稿引用。照 files-ops 的做法：文件编辑页调 `file.write` 时“另传草稿引用”，见 `system/apps/files/files.c` 和 `system/services/file/file_caps.c`，正文由文本服务的草稿保存。
   - `comm.sms_send` 接受 `text`（直接给正文，像现在这样，受参数表大小限制）或者草稿引用，两者选一。通信服务把完整正文（最多 `INK_SMS_TEXT_MAX`，5 段）读进自己在 PSRAM 里的缓冲，不放在任务栈上。
   - 确认卡说明不超过 160 字节，例如“发给 <对方>：<开头约 20 字>…（共 M 字，将按 N 条发送）”。单段时不写条数，和第一版一致。
   - AI 只能直接给 `text`，长度受参数表限制（约 2 段）。超过时回执说清原因，什么都不发。AI 能不能发更长的短信，写进报告的“要用户定”。
4. 第一版的测试照新做法改：
   - 写短信页输入 150 字：卡片写“将按 3 条发送”，点“是”以后发出 3 段；
   - AI 给 100 字：卡片写 2 段；
   - AI 超长：被拒，什么都没发；
   - 其余收发测试照原样。

## 验收

1. `integration-confirm`、`memo-record` 照原期望通过（期望不改）；`tests/sms_multipart` 的收发、重启、部分失败、5 段上限都通过。
2. 全量测试和内存检查通过；S31 能编过；gcccheck 通过。
3. 没有新的“may overflow its … stack”警告。报告里列出 `integration-member-wake`、`memo-record` 两个场景里栈估算最高的五个任务，给出第一版前后的对比。
4. 报告写清“上机要核对的”，沿用第一版那一节：原厂固件不收发短信，NT26 对 PDU 模式的支持要上机确认；不许发写模组配置的命令。
