# mobile-network-2：把已完成的 mobile-network 合到现在的主线上

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/mobile-network-2/REPORT.md。**只编译、跑宿主机测试，不碰设备。**

## 背景

mobile-network（手机网络设置和流量统计）已经做完并通过了它自己的验收，任务书和报告见 `docs/tasks/mobile-network.md` 和它分支里的 `docs/tasks/mobile-network/REPORT.md`。它的完整提交在本仓库的分支 `archive/mobile-network`（克隆里是 `origin/archive/mobile-network`），用 `git log`、`git show`、`git diff` 看，**只读，不要往那个分支提交**。

它基于较早的主线，现在合不进来：2026-11-10 晚试合时冲突在 `CMakeLists.txt`、`assets/i18n/en.tsv`、`system/ui/shell/navigation.c`、`tests/integration/ai_surface.c`。之后主线又合入了 port-boot-1、port-storage-1、port-audio-1、port-wifi-1、log-1、vendor-feature-audit、audit14-fix、port-sleep-vendor-2，`ports/esp32s31/hal_stubs.c` 里蜂窝相关的桩也可能有变化。

## 要做的

1. 从现在的主线开始，把 mobile-network 的功能重新落进去，语义和它的报告一致。
2. 冲突照两边的意图合：`navigation.c` 里两边新加的页面都保留；能力的 `.app` 字段照 audit14-fix 的做法填；新能力进入日志的能力调用记录（log-1 已在 `ink_cap_call` 里统一记录，不用另加）。
3. 端口侧：S31 的蜂窝 HAL 还是桩（4G 模组没接），设置页在“没有模组”时要说清楚，不报错、不卡住，写测试。
4. 把 mobile-network 的测试带过来并通过。

## 验收

1. 模拟器全量测试通过，ASan 版也通过；mobile-network 原有的测试全部通过；新增“没有模组时手机网络页面正常显示说明”的检查，并做反向对照。
2. 能力清单重新生成并 `--check` 通过；S31 能编过，0 警告；gcccheck 通过；做隐私扫描。
3. 报告里写清和 mobile-network 原实现的差别。
