# audit14-merge-fix：audit14-fix 合进主线后两项测试失败，找原因并修好

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/audit14-merge-fix/REPORT.md。**只编译、跑宿主机测试，不碰设备。**

## 背景

2026-11-11 凌晨把 audit14-fix（第 14 批审计的修复：锁屏相机闸门不再按名字前缀特判，改用能力标记和 `ink_ui_locked_activity()`；能力加 `.app` 字段，孩子的应用名单按它判断；文件批量回执；中枢连接加锁等）合进了主线，提交 3ba30618。它基于较早的主线，之后主线已合入 log-1、port-boot-1、port-storage-1、port-audio-1、port-wifi-1、vendor-feature-audit 等。合并后修掉了几项（009098ac：状态订阅表、短信参数、文档行号），**还剩两项失败**：

1. `boot-flow`（`tests/boot_flow.c` 的 restore 场景）：`restore.apply` 之后，主页活动卡片（`system/ui/shell/activity_card.c`）每 50～160 ms 调一次 `job.list`，界面一直不安静，`BootHello` 截不到稳定画面（“all scene snapshots must be fully presented”）。合并前的 1390e166 这项通过。模拟器的设备日志环在 `build/test-out/boot-flow-restore/logs.bin`，可用 `python3 tools/device/pull_logs.py --logs <logs.bin> --output <目录>` 解码。
2. `hub-simulator-e2e`：`assert hub.metrics.tool_calls == 1` 得到 0。合并前通过。

已排除（单独撤回后仍失败）：`system/policy/members.c`、`system/services/hub/hub_link.c`、文件服务新加的 `member.current` 监听、各应用的 `.app` 字段那一组、界面那一组（ai_*、settings_family、time_settings、input_service、app_registry、navigation）。剩下的嫌疑在 `system/policy/cap.c`、`cap.h`、`ui.h`、`ui_task.c`、`system_actions.c`、`file_caps.c`、`file_service.c` 的其它改动，以及它们和后来合入的改动的配合。

## 要做的

1. 找出两项失败的真正原因（写清证据：哪一行、为什么在合并后才出现）。
2. 修好，保持 audit14-fix 的意图（不回到名字前缀特判，孩子的应用名单照 `.app` 判断），不放宽测试。
3. 能力清单有变化就重新生成（`python3 tools/capabilities.py`）。

## 验收

1. 模拟器全量测试通过（中枢测试要建好中枢环境，见 `hub/README.md`），ASan 版也通过。
2. S31 能编过，0 警告；gcccheck 通过；做隐私扫描。
3. 报告写清原因和修法。
