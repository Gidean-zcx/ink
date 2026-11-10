# notify-manage 任务报告

基线：`eec5d569be028b539e8778690c239a6947767864`。分支：`claude/codex-notify-manage`。状态：进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 逐条删除及重启 | 【实测】专项通过 | 已读／未读逐条删，其他通知与未读数正确，第二次启动不回来 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/notify_manage.py --build build` |
| 1b 短信渠道关闭及恢复 | 【实测】专项通过 | 真实模拟短信事件；关闭后仅短信收件，打开后恢复通知；偏好重启保留 | 同上 |
| 1c AI 删除及渠道设置 | 【实测】专项通过 | AI 和触摸均使用既有 `notify.dismiss`、`notify.set_channel`，页面随结果更新 | 同上 |
| 1d 孩子限制 | 【实测】专项通过 | 孩子不可打开渠道设置或调用渠道修改；可删除自己的通知，看不到成人收件箱 | 同上 |
| 2a 原有通知、短信、电话、日历回归 | 【实测】通过 | 最终普通版相关测试通过，原测试未改 | 全量命令见下 |
| 2b 全量 | 【实测】320/321，未全绿 | 唯一失败为受保护的共享能力表过期；原话见下 | 全量命令见下 |
| 2c 内存检查 | 待验证 | — | — |
| 2d S31 | 【实测】编译通过 | 退出 0，镜像 1,386,144 B，编译警告／错误均为 0；未上机 | `INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2e GCC | 【实测】通过 | `gcccheck: 5 compiled, 0 with findings`，无跳过 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B ../../tools/gcccheck.py build system/ui/shell/navigation.c system/services/notify/notify_caps.c system/services/notify/notify_service.c tests/notify_manage.c tests/integration/ai_surface.c` |
| 3 中英文页面及快照 | 【实测】通过 | 八张截图及 JSON，含最大字号通知／渠道页；满页长正文符合生产 AI 缓冲 | 同 1a |

## 没做到的

共享生成能力表按任务书留给主会话，导致 `capability-table-doc` 失败；不能声称全量全绿。内存检查仍在进行。未上机。

## 和设计不一致的

暂无。此次按任务书 D-117 使用现有页面和规范设计；通知删除语义、短信收件和来电／闹钟流程均沿用服务。

## 改过的已有测试

`tests/integration/ai_surface.c` 的 `FOR_AI` 增加 `shell.notify_settings`，要求新导航能力也被运行时工具目录覆盖；未删除或放宽断言，通知、短信、电话、日历测试原样保留。

## 要用户定的

暂无。

## 顺手发现

根 CMake 中既有 `unit-s31-osal-ticks` 先于 sanitizer 选项定义，ASan 配置下仍不带检查标志；未扩大本任务范围修改。已核对三个产品变更文件和新专项测试均带 `-fsanitize=address,undefined`。

## 新增的机制

- `notify.changed` 变化事件：任务书要求删除后页面和 AI 快照与服务一致。仅在未读数不变时补信号，数值变化仍用原状态通知，避免新通知占用两份离线事件。
- `notify.channels_changed` 变化事件：渠道开关完成后刷新设置；与列表变化分开，只重画受影响的页面。没有新任务、状态机、文件格式、后台重试或配置项。

## 要改的文档

- 合并时生成 `docs/system/capabilities.md`：新增 `shell.notify_settings` 导航、`notify.changed`／`notify.channels_changed` 事件，`notify.set_channel.child_ok` 改为 false。任务书禁止本分支改生成能力表；临时生成的 `work/notify-capabilities.md` 已通过运行时对照：`Runtime matches: 401 capabilities, 28 settings`、`PASS: 478 definitions`。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tools/capabilities.py --output work/notify-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json`。
- 设置缺口清单中的两项可在主会话更新为已接通；共享任务清单和 UI 依赖状态由主会话统一维护。

## 过程与验证记录

- 【实测】工作区初始干净，HEAD 等于指定基线，已在独立任务分支。已读取任务书及总规则；构建和测试最多 `-j2`，使用 `taskpolicy -c utility`。
- 产品代码含构建配置、译表合计 **+114/−22，改动 136 行，净增 92 行**；其中 `.c/.h` 为 +98/−22。低于 250 行预算，测试和报告未计入。
- 【实测】最终专项 `main: exit=0, pass=263, fail=0, finished=1`、`restart: exit=0, pass=11, fail=0, finished=1`；最终 6 项相关回归见 `work/notify-final-focused.log`，检查计数见对应 `metrics.json`。八张截图和 JSON 均存在，最大通知快照 2782 B，小于生产缓冲 3072 B。
- 【实测】`ui-scale: 50 files, 0 violations`；`gcccheck: 4 compiled, 0 with findings`，无跳过，日志 `work/notify-ui-scale.log`、`work/notify-gcccheck.log`。
- 【实现】删除放在每行右侧独立描边按钮，与打开区域留出间距，整颗按钮满足最小触控尺寸。按钮宽度随译文测量，避免英文挤压；不增加回收站或确认步骤，沿用现有单条删除能力的语义。
- 【实现】渠道入口为“设置 → 声音 → 通知”，与提示行为相关；放在已有声音选项之后，主目录和既有控件位置保留，勿扰不重复。按实际登记表枚举系统、短信、电话、闹钟、提醒、下载、AI 七个渠道；更新发现／结果走系统，下载完成走下载（`update_service.c:507`、`:792`、`:939`），没有独立更新渠道。
- 【实现】成人设置均不向孩子开放，渠道修改能力也收紧为相同规则；孩子自己的通知仍可读和删，数据和偏好按成员隔离。

截图前缀：`build/notify-manage-evidence/sim-notify-manage/main/`。

- 中文：`snap-notifications-zh.png`、`snap-notifications-zh-large.png`、`snap-channels-zh.png`、`snap-channels-zh-large.png`。
- 英文：`snap-notifications-en.png`、`snap-notifications-en-large.png`、`snap-channels-en.png`、`snap-channels-en-large.png`。
- 同目录同名无 `snap-` 的 `.json` 为页面 AI 快照。已人工查看英文列表和英文最大字号渠道页。
- 【实测】首轮全量 321 项有 8 项失败（`work/notify-ctest.log`）。除共享能力文档外的 7 项已修正：设置入口挤页、分页旧删除动作、新导航缺运行时目录测试、额外事件挤占离线队列。相关原测试和本任务专项重新构建后 `100% tests passed out of 8`（`work/notify-regressions.log`），原断言均保留。
- 【实测】五组反向对照逐一撤掉接线／权限／刷新／分页清理，均命中指定失败；不改工作区产品代码。`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/notify_manage_reverse.py --build build`，日志 `work/notify-reverse-final.log`。CTest 正常模式不依赖仓库历史。
- 【实现】通知一页最多四条，确保长正文加全部动作装进既有 3072 字节 AI 缓冲；继续复用翻页，不增大缓冲或放宽预算。
- 为什么不能更小：任务涉及列表删除、成人渠道设置、共享变化通知和中英文快照，合计超过 100 行。看过仅补触摸回调的方案，不能覆盖 AI 修改及已读通知删除；新建详情页会多一套导航和生命周期。最终复用列表、设置分页、服务渠道表和总线，两处界面使用既有原子能力。
- 【实测】最终 GCC 和 S31 均退出 0，日志 `work/notify-gcccheck-final.log`、`work/notify-s31-final.log`；S31 未出现 `warning:`／`error:`，镜像仍为 1,386,144 B。界面尺寸复核仍为 `ui-scale: 50 files, 0 violations`（`work/notify-ui-scale-final.log`）。
- 【实测】普通全量原话：`99% tests passed, 1 tests failed out of 321`，`Total Test time (real) = 689.01 sec`；唯一失败 `136 - capability-table-doc (Failed)`，日志 `work/notify-ctest-final.log`。完成后仅新增空列表快照断言，产品代码未再改动；最终相关 6 项再次全过（`work/notify-final-focused.log`，20.15 秒），五组反向对照也再次命中。

全量命令：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。配置按总规则，另加 `CMAKE_EXPORT_COMPILE_COMMANDS=ON`，`INK_HUB_PYTHON` 指向 `$INK_DEPS/hub/.venv/bin/python` 复用锁定的中枢测试依赖；不调整预算。
