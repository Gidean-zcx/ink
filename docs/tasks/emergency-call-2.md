# emergency-call-2：把已完成的 emergency-call 合到现在的主线上

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/emergency-call-2/REPORT.md。**只编译、跑宿主机测试，不碰设备。**

## 背景

emergency-call（紧急呼叫，D-119）已经做完并通过了它自己的验收，任务书和报告见 `docs/tasks/emergency-call.md` 和它分支里的 `docs/tasks/emergency-call/REPORT.md`。它的完整提交在本仓库的分支 `archive/emergency-call`（克隆里是 `origin/archive/emergency-call`），用 `git log`、`git show`、`git diff` 看，**只读，不要往那个分支提交**。

它基于较早的主线，现在合不进来。2026-11-11 凌晨试合时冲突出在 7 个文件：`assets/i18n/en.tsv`、`system/apps/phone/phone.c`、`system/policy/cap.c`、`system/services/comm/comm_service.c`、`system/ui/lock_mark.c`、`system/ui/shell/member_gate.c`、`tests/integration/ai_surface.c`。最要紧的是 `cap.c` 的锁屏闸门：audit14-fix 已经按审计意见去掉了按名字前缀的特判（相机），改成能力自己的标记加 `ink_ui_locked_activity()`、`ink_ui_locked_allows()`、`locked_owner_only`；emergency-call 是在旧写法上又加了 `phone.`、`call.` 前缀特判和 `opens_emergency()` 例外。

## 要做的

1. 从现在的主线开始，把 emergency-call 的功能重新落进去，语义和它的报告一致：锁屏和成员门上能打开紧急呼叫页，只有人能拨号，AI 只能打开页面，孩子被禁用的电话应用不影响紧急呼叫。
2. 锁屏闸门**不加名字前缀特判**：用能力上的标记表达（沿用 audit14-fix 的机制，需要的话加一个标记，写清含义），`opens_emergency()` 也改成按标记判断。
3. 和 semantics-shell-1 改过的 `lock_mark.c`、`member_gate.c` 对齐：紧急入口要出现在这两个浮层的语义描述里，AI 看得到“打开紧急呼叫页”这个动作。
4. 能力的 `.app` 字段照 audit14-fix 的做法填，但紧急呼叫相关的能力**不挂在电话应用下**，不受孩子应用名单影响，写测试证明。
5. 把 emergency-call 的测试（`tests/emergency_call/`）带过来并通过。

## 验收

1. 模拟器全量测试通过，ASan 版也通过；emergency-call 原有的测试全部通过；新增“孩子禁用电话应用时紧急呼叫照常”和“锁屏闸门没有名字前缀特判”的检查，并做反向对照。
2. 能力清单重新生成（`python3 tools/capabilities.py`），`--check` 通过；S31 能编过，0 警告；gcccheck 通过；做隐私扫描。
3. 报告里写清和 emergency-call 原实现的差别。
