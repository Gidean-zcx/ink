# audit14-fix 执行报告

基线：`41d7f251b5d4b2cd4e81764ee3e866a6186663cf`。分支：`claude/codex-audit14-fix`。状态：专项和反向通过，最终全量、内存检查和 S31 验收中；仅本地提交，不推送、不接设备。以下实测均为宿主机或模拟器证据。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 页面声明锁屏权限 | 【实测】正反通过 | lock-camera 不改；合成页面覆盖名单、普通锁屏、未认证空间、AI 拒绝、真实 HOME 和 HOME/BACK 能力；撤回策略、输入、主页处理均失败 | C1、C2；`work/audit14-fix/verified.log`、`reverse.log` |
| 2 能力声明应用归属 | 【实测】正反通过 | 191 项静态声明，跨 14 个应用；ownership 阶段遍历归属，禁用能力逐项核对 AI/触摸拒绝，电话短信保留；恢复旧 action_app 后失败 | C1、C2 |
| 3 批量文件简要回执 | 【实测】正反通过 | 26 项、25 失败、20 文件名和剩余 5 项，另测全成功；固定缓冲 7868 B，原最坏 884768 B；恢复旧回执后失败 | C1、C2 |
| 4 时间页相关事件刷新 | 【实测】正反通过 | 闹钟添加/取消、time.synced 变化不销毁控件；原分钟更新检查保留；撤回过滤后同步状态检查失败 | C1、C2 |
| 5 中枢时间字段加锁 | 【实测】正反通过 | 6 行共享字段访问均在 id_lock 内；模拟中枢核对时间和 RTT；分别撤回读、发送 hello、接收 hello 的锁均失败 | C1、C2 |
| 全量及内存检查 | 待运行 | — | — |
| S31 | 待运行 | — | C5 |
| gcccheck | 【实测】通过 | 原话 `gcccheck: 36 compiled, 0 with findings`，退出 0；动态编译的 thumb_wiring.c 不在编译命令表，工具明确列出未编译 | C6；`work/audit14-fix/gcccheck.log` |
| 隐私及提交范围 | 待最终扫描 | — | 总规则第 6 节 |

原有 lock-camera、kid-apps、files-ops、time-settings 均通过；最终专项含 9 项，原话 `100% tests passed out of 9`（`work/audit14-fix/verified.log`）。反向 9 组均命中指定失败，包装脚本退出 0；功能对照都到达 test.finished，不以崩溃当成功。锁的 3 组对照是源码约束检查。

## 删除的特判

以下为基线行号，可用 `git show 41d7f251:<文件>` 核对。

| 文件和行 | 删除内容 |
| --- | --- |
| `system/policy/cap.c:57`、`:119`、`:123` | 直接问相机、camera/photo/file/app.open 名称匹配、相机退出动作特例 |
| `system/services/input/input_service.c:189` | 按相机状态放行按键；现经现有策略接口读声明，无 UI 头文件依赖 |
| `system/policy/system_actions.c:29` | 主页直接关闭相机；现交给声明中的按键处理 |
| `system/ui/shell/navigation.c:201`、`:1601` | 触摸绑定和返回直接识别相机；现均查声明 |
| `system/ui/app_registry.c:471` | 判断受限状态直接问相机；相机入口本身仍在应用注册层 |
| `system/policy/members.c:384` | action_app 中的 shell.ai/calc/calendar/alarm/reminder 和应用名前缀推断 |

## 没做到的

最终全量、内存检查和 S31 尚待完成。已确认共享能力表需要重新生成；任务明令不修改它。

## 和设计不一致的

没有改规范迁就实现。能力表生成器仍只描述能力级 while_locked，合并时需要补充页面许可、应用名单和 locked_owner_only 的语义。

## 改过的已有测试

- files-ops 只改一条旧回执期望：从失败文件名及 `exists` 错误串，改为失败文件名、失败 3、成功 0、无逐项 error。新合同不再返回逐文件错误串。
- thumb-wiring 的目标值仍为缓存 0，改为等待既有后台清理完成，没有固定 sleep；新拒绝路径不再隐含等待一次媒体调用。
- kid-apps、time-settings 保留旧期望，只新增检查；Python 运行器仅增可选参数。lock-camera 未改。

## 要用户定的

无。

## 顺手发现

- 【实现】闹钟修改发布 alarm.revision，不是 time.*；除要求的闹钟场景，另用 time.synced 验证过滤修复的反向失败。
- 【实现／实测】提前拒绝 file.thumb 暴露旧缓存只在下次读取或 member.changed 时清理，而切换仅发布 member.current 的缺口。为恢复既有隔离检查，复用原清理函数监听切换状态，仅净增 2 行。
- 【实现／实测】相册内部 file.lookup 改用系统来源，外层 photo.open 仍过应用名单，内部仍查文件成员归属，避免禁用文件应用连带阻断相册；AI/触摸直接调 file.* 仍被拒绝。
- 初轮扩大普通锁屏限制的问题已撤回；显式能力标记保留原媒体、确认和后台行为。初轮全量日志在 `work/audit14-fix/ctest-initial.log`。

## 新增的机制

- `ink_activity_t.locked_caps`（ui.h:39）、`ink_ui_overlay_t.activity`（ui.h:59）：任务第 1 项要求的页面许可和覆盖层关联。
- `ink_cap_t.locked_owner_only`（cap.h:106）：29 项静态标记替代普通锁屏下的名称匹配，保留原非人工调用限制；受限页面仍以 activity 名单为准。
- `ink_cap_t.app`（cap.h:89）：任务第 2 项要求；191 项静态归属声明。比域注册更小，无新注册表或运行时前缀解析。
- 调用槽保留本次已获人工锁屏许可，避免动作关闭页面后成功回执被改为 stale；沿用现有调用生命周期。
- 无新任务、线程、状态机、持久格式、队列、缓存、重试或产品配置项。

产品代码增加 275 行、删除 235 行，净增 40 行（预算净增 250 行）。超过 100 行主要是给既有应用能力补静态元数据。看过更小方案：保留前缀违反任务；域注册另需表和查询；一律收紧普通锁屏破坏既有行为。没有改名、搬文件或批量格式化。

## 要改的文档

按总规则不修改共享 `docs/system/capabilities.md`。临时生成结果在 `work/audit14-fix/capabilities.md`；合并时重新生成，并记录页面锁屏声明及能力应用归属。

C1：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(audit14-.*|lock-camera|kid-apps|files-ops|time-settings|thumb-wiring|audit3-b3-06)$'`。

C2：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/audit14_reverse.py --build build --baseline 41d7f251b5d4b2cd4e81764ee3e866a6186663cf`。Git 历史仅用于这个手动入口，不注册到 CTest；临时副本不改跟踪文件。

C3：`INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=hub/src UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。C4 同命令将目录换为 build-asan。两目录按总规则第 5 节配置，均启用 WERROR、编译命令导出、`INK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`；ASan 仅另加 `-DINK_SANITIZE=ON`。构建均为 `taskpolicy -c utility cmake --build <目录> -j2`。

C5：`INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh`，仅编译。

C6：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B "$INK_DEPS/../ink-codex/tools/gcccheck.py" build <基线以来修改的 C 文件>`；列表来自 `git diff --name-only 41d7f251 -- '*.c'`，共 37 个。
