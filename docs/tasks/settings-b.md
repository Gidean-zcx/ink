# settings-b：设置里的成员、孩子、壁纸、记录、AI 记忆（settings-all 的第二块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/settings-b/REPORT.md。

## 要做的

预算 350 行产品代码：

- `SetMembers`：家庭成员列表，添加、改名、删除，进入设置 PIN。
- `SetKid`：孩子的可用应用和可用时段，接已有的儿童策略。
- `Wallpaper`：用文件选择器（F9）选一张图，经图片解码和抖动后设为壁纸。
- `Records`：使用记录，AI 做过什么、设备上发生过什么。
- `AiMemory`：AI 记得的事，能看、能删。

`SetBluetooth` 收起来等用户定，这次不做，接口保留。

## 从哪里看起

- 这些页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 成员策略 system/policy/members.c、members.h；文件选择器 system/ui/file_picker.c；图片 system/services/media/；活动记录在 system/policy/cap.c（ACTIVITY）；AI 记忆找 AI 服务或中枢接口，没有后端就如实显示并写进报告。
- 已合入的 system/ui/shell/settings_pages.c 是写法参考，**不要改它**，新页面放新文件。

## 边界

- 删除成员、清空 AI 记忆这类不能恢复的操作，照现有的确认规则。
- 设置都走已有的 settings 能力，不另开写法。
- 同时在做的有 backup-pages、camera-gallery、search，还有上一批在收尾的 kid-1、ai-pages-2、confirm-pages、update-pages、calendar、files-app-1，不要改它们的文件；共用的文件只在末尾加行。

## 验收

1. 五页都能打开，显示的值来自后端，改动重启后还在（测试）。
2. 孩子成员进不了这些页；去掉检查，测试失败（反例）。
3. 选图设壁纸后，锁屏和主页显示新壁纸（截图留在构建目录）。
4. 每页有 AI 快照；按钮和 AI 走同一个能力（测试）。
5. 全量测试和内存检查通过；产品代码不超过 350 行。
