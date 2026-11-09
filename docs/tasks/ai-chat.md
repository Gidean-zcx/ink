# ai-chat：AI 对话页面（ai-pages 的第一块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/ai-chat/REPORT.md。

## 要做的

backlog ai-pages 一节的前五页，预算 350 行产品代码：`AiHome`（AI 首页）、`AiAsk`（提问和等待）、`AiListenTap`（按住说话、点按听）、`AiLong`（长回答翻页）、`AiSpeak`（AI 在说话，可以打断）。接现有的 AI 服务和音频服务。

## 从哪里看起

- 这五页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题（system/ui/theme.c）和比例为准。
- 现有的 AI 面板代码（在 system/ui/ 下搜 ai 面板相关的文件）；AI 服务 system/services/ai/ai_service.c（`ink_ai_key`、`ink_ai_tap`、对话轮次）。
- 页面语义和快照用 F1，长回答翻页用 F4 的通用列表或现有的文本分页；text-long 在同时做，不要等它。报告在 docs/tasks/ui-semantics、ui-list-paging。

## 边界

- 只做这五页。离线、超时、额度、记录、回执和各种确认页是后面的任务。
- 不改 AI 服务的协议和轮次逻辑；缺的写进报告。
- 新页面放在新文件里；共用的文件（应用目录、CMake、能力注册）只在末尾加行。同时在做的还有 contacts、clock-alarm、ui-file-picker、thumb-wiring、text-long、boot-flow，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 在模拟器里走一遍：首页 → 提问（文字、语音各一次）→ 等待 → 长回答翻页 → 说话中打断（场景测试，每页截图留在构建目录）。
2. 每页都有 AI 快照；按钮、实体键和 AI 走同一个能力（测试）。
3. 换了成员，对话页清空，看不到上一个人的对话；去掉检查，测试失败（反例）。
4. 全量测试和内存检查通过；产品代码不超过 350 行。
