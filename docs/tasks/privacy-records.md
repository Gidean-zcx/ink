# privacy-records：记录汇总页和隐私页接线

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/privacy-records/REPORT.md。

## 要做的

预算 300 行产品代码：

- `Records` 改成画板上的汇总页（system/ui/shell/settings_family.c 里现在的 Records 自己分页列 AI 记录，和 `AiLog` 重复）：
  - 上面两个分段“我”“孩子”（孩子那段只有大人看得到，列孩子的记录）。
  - 四行：“AI 替你做过的事”（件数，点了进 `AiLog`）、“和 AI 说过的话”（次数，点了进对话记录，也就是 `ai` 应用）、“AI 记得的事”（进 `AiMemory`）、“运行记录”（不含内容；没有后端就如实说）。
  - 一句保留期限说明，用系统里真实的期限（活动记录大人 1 年、孩子 180 天，见 system/policy/cap.c 的 `cap_activity_list`；其它没有的不要编）。
  - “全部删除”：只删真有删除接口的；没有接口的那几类如实说删不了。删除照现有确认规则（删了找不回来，要确认）。
- 大人看孩子的记录：活动记录现在只能列当前成员的。补一个只给大人的读法（例如 `activity.list` 加可选的成员参数，孩子调用时只能是自己），供“孩子”那段和 `AiLog` 用。
- `SetPrivacy`（system/ui/shell/settings_pages.c 的 `PRIVACY_ROWS`）：“查看和删除记录”“孩子的记录”打开 `Records`（孩子的记录直接到“孩子”分段）；“留多久”按画板写真实期限。

## 从哪里看起

- 画板 `$INK_DESIGN/png/` 里的 Records、SetPrivacy、AiLog、AiMemory；规格 docs/product/system-ui-spec.md 第 319 行。
- AiLog 和回执：system/ui/shell/ai_pages.c（`shell.ai_log`）；对话记录：system/ui/shell/ai_chat.c；活动记录：system/policy/cap.c（ACTIVITY 集合、`activity.list`）。

## 边界

- 同时在做的有 kid-3、file-index-3、factory-pages、fw-f13、port-skeleton，不要改它们的文件；共用文件只在末尾加行。
- 不新做记忆后端；AiMemory 维持“暂时看不到”。

## 验收

1. Records 四行的件数和次数来自后端；点每一行进对应的页（测试）。
2. 大人能看孩子的记录；孩子只能看自己的，调用时指定别人的成员被拒（测试，反例）。
3. “全部删除”确认后，真有接口的几类清空，件数归零；没有接口的那几类页面如实说明（测试）。
4. SetPrivacy 两行进 Records；保留期限文字和代码里的期限一致（测试）。
5. 全量测试和内存检查通过；产品代码不超过 300 行；截图留在构建目录。
