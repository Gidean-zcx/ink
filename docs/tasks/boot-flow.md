# boot-flow：开机和配网页面

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/boot-flow/REPORT.md。

## 要做的

backlog 原文：`FirstBoot` `BootWifi` `BootWifiPass` `BootJoining` `BootJoinFail` `BootClaim` `BootNoHub` `BootConfirm` `BootRestore` `BootHello` `BootRevoked`，接 `pairing` 的状态机。预算 400 行产品代码：页面多，能共用的版式就共用一个。

## 从哪里看起

- 这些页面的画板在 `$INK_DESIGN/png/`，先看图；规格只用 docs/product/system-ui-spec.md 第 1 节的。画板还没跟上界面放大 1.2 倍和新灰度，以代码里的主题（system/ui/theme.c）和比例为准。
- 配对状态机：pairing 任务合入的代码，文件列表在 docs/tasks/pairing/REPORT.md。
- Wi-Fi 密码用 F2 的短输入宿主（ui-input-host）。密码只由本人填（D-111 ④），AI 快照和日志里都不能有明文。
- 网络服务 system/services/net/；模拟器的 Wi-Fi 在 ports/sim/hal/。

## 边界

- 只做页面和接状态机。状态机本身的问题写进报告；一两行能修的小错可以修。
- 每页都有 AI 快照（规范第 10 节），按钮和 AI 走同一个能力。
- 不新建常驻任务；LVGL 只在 UI 任务里用。

## 验收

1. 模拟器里从首次开机走到 `BootHello`：选 Wi-Fi、输密码、加入、认领、确认（场景测试；每页截图留在构建目录）。
2. 失败分支都能到达，也能返回：密码错（`BootJoinFail`）、找不到中枢（`BootNoHub`）、被吊销（`BootRevoked`）、恢复（`BootRestore`）（测试）。
3. 密码不出现在 AI 快照和日志里（测试）。
4. 全量测试和内存检查通过；产品代码不超过 400 行。
