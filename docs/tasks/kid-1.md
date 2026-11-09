# kid-1：换成员和 PIN 页面（kid-member 的第一块）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/kid-1/REPORT.md。

## 要做的

预算 300 行产品代码：`MemberSwitch`（换成员）、`PinWait`（输错后的等待）、`PinForgot`（忘记 PIN）、`PinSet`（设置、修改 PIN），接成员策略（system/policy/members.c：`ink_member_switch`、PIN 的尝试次数和等待、家长 PIN）。

## 从哪里看起

- 这些页的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的，以代码里的主题和比例为准。
- 现有的成员门 system/ui/shell/member_gate.c（解锁页）；system/include/ink/sys/members.h；PIN 规则在 members.c。

## 边界

- PIN 只由本人输入（D-111 ④），AI 快照和日志里不能出现 PIN。
- 不改 PIN 的存储格式和尝试规则；缺的写进报告。
- 新页面放新文件；共用的文件只在末尾加行。同时在做的有 ai-pages-2、confirm-pages、update-pages、calendar、files-app-1，不要改它们的文件。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 换成员：有 PIN 的成员要输 PIN；输对进入，输错计数，到上限进 `PinWait`，显示还要等多久（测试）。
2. `PinSet` 设置和修改 PIN，重启后有效（测试）。
3. `PinForgot` 走家长验证（测试）。
4. PIN 不出现在快照和日志里（测试）。
5. 全量测试和内存检查通过；产品代码不超过 300 行。
