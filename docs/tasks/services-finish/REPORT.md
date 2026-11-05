# 系统服务收尾报告

2026-11-05，第 1 轮复查修复。状态：六项修复、常规 86/86 和 19 组回归通过，正在执行 ASan/UBSan 验收。指定差异基线 `fdff779`，本轮开工提交 `d74a261`，初始工作区干净。保留 [独立复查](REVIEW.md) 原文；它的“不通过”是修前结论。本报告替换上轮过满的 S2、S6 结论；上轮详细记录仍在 `d74a261` 的同名文件中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1-01 高 | 已修，待最终内存检查 | 【实现】先持久保存 `committing`，有效槽提交成功后才进入资源阶段；开机按实际版本收敛回滚事务，检查 HAL 提交错误。【实测】正常、提交前断电、提交后断电、提交失败四链，各四次启动；旧源码在提交前断电后仍永久阻塞 | V2 / V3 的 `upgrade_crash` |
| R1-02 高 | 已修，待最终内存检查 | 【实现】换人直接作废播放流、预缓冲，音频任务丢弃在途输出。【实测】UI 忙时缓冲 TTS、媒体队列忙时缓冲听书都不再继续播放；同一成员锁屏听书保留。旧源码失败 | V2 / V3 的 `playback`；V1 的 `integration-media` |
| R1-03 高 | 已修，待最终内存检查 | 【实现】集合仍为 128 KiB，复用 `drop_oldest` 回收最旧记录；每成员最多占 16 个 PSRAM 待记槽，存储积压时保持拒绝无记录写入。【实测】连续 1,600 次动作不锁死；故意占满成人待记槽，孩子的 AI 和确认后动作仍可执行，恢复存储后成人也恢复。旧源码失败 | V2 / V3 的 `activity` |
| R1-04 严重 | 已修，待最终内存检查 | 【实现】权限、儿童音量规则、身份保护设置逐项标记人工确认；绑定 id/value，确认后重查成员和设置限制；直接设置入口和撤销也不绕过。【实测】真实剪贴板、儿童上限的确认/拒绝/换人/无效值，以及普通音量亮度自动执行；旧源码失败。补测“AI 撤销真人关闭权限”曾失败，修后通过 | V2 / V3 的 `permissions`；`policy` |
| R1-05 高 | 已修，待最终内存检查 | 【实现】先持久保存待清理标记，缓存、Flash、卡清理及身份删除全部成功才释放编号；失败回执不再是 done；标记阻止切换、PIN 解锁和复用，重启可查询并重试。【实测】卡目录失败、Flash 失败、缓存被占、待删除成人持唯一 PIN 四链，每链三次启动，最后复用编号的孩子无旧文件/集合；旧源码失败 | V2 / V3 的 `removal`；V1 的 `integration-people` |
| R1-06 中 | 已修，待最终内存检查 | 【实现】撤销选最新记录，可用 revision 绑定指定操作，设置逆操作在修改锁内校验当前版本。【实测】0→10→20 撤销回 10；旧记录、后来人工修改、过期撤销均不覆盖当前值。旧源码失败 | V2 / V3 的 `undo` |
| S1 | 六项复查问题均有修复和正反例；原保留项见后文 | 【实测】先执行独立复查附录，复现六项共 10 个失败，正常升级/停止播放/正常删除控制通过；再以开工源码做反向对照 | V2、V3；`build/services-review1-before.log` |
| S2 | 已补真实设置、满额、连续撤销边界，待最终全量验收 | 【实测】`permissions` 26、`undo` 13、`policy` 30 检查通过；未把确认卡或密码检查交给 AI | V1 / V2 |
| S3 | 保留既有修复 | 【实测】实际能力表完整发送的 `actions` 回归通过；没有新增内部 RAM 缓冲 | V2 的 `actions`；V1 预算 |
| S4 | 内存检查中 | 最新补修后常规 86/86，45.61 秒；19 组全部通过；预算未改 | V1、V2 |
| S6 | 已补播放与删除事务边界，待最终内存检查 | 【实测】`handoff`、`isolation`、`playback`、`removal`；保留原成员、通知、文件、应用的全量断言 | V1 / V2 |
| S5 | 最终检查中 | 差异、隐私、文件大小、链接及提交尚待最后核对 | V4 |

证据只覆盖本机模拟器，不代表 Linux、真机或真实中枢。构建和全量测试逐轮串行，构建最多 `-j4`、CTest 最多 `-j3`。仅既有 `gui-smoke` 使用 SDL dummy，没有启动窗口。

原 A/B/C2 处理逐项保留；本轮未重做已通过的修复。下表“通过”指本轮重跑对应回归，原反向证据见独立复查的 V3（`70d3a43`）。

| 编号 | 结论 | 本轮证据/未做原因 |
| --- | --- | --- |
| AUD18-101 高 | 保留修复 | `files`：目录打开失败报错；调用方另由 R1-05 补齐 |
| AUD18-102 高 | 保留修复 | `files`：目录、原子短写统一持卡 |
| AUD18-103 中 | 保留修复 | `files`：作业先完成仍正确结束 |
| AUD18-104 高 | 保留修复并补 R1-01 | `update`、`upgrade_crash`：状态持久化与多启动 |
| AUD18-105 高 | 保留修复 | `update`：资源恢复事务不被新包覆盖 |
| AUD18-106 中 | 没做 | 失败历史、迁移及人工重试规则较大，仍只记最后失败版本 |
| AUD18-107 中 | 保留修复 | `update`：正常包和八类坏包 |
| AUD18-108 中 | 保留修复 | `update`：两种 offer 变化链 |
| AUD18-109 高 | 保留修复 | `storage`：取消遗忘集合的共享定时节点 |
| AUD18-201 高 | 保留修复 | `audio`、`voice`：拥堵时仍停止采集/上传 |
| AUD18-202 高 | 保留修复 | `text`：OOM 后不重做旧文本 |
| AUD18-203 高 | 保留修复 | `shutdown`：录音结束且数据持久后断电 |
| AUD18-204 高 | 保留修复 | `audio`：耳机掉线不外放旧私有媒体 |
| AUD18-205 中 | 保留修复 | `audio`：通话后仍有闹钟焦点 |
| AUD18-206 中 | 没做 | 具体关机帧的成功显示回执需要显示服务与界面联动，busy 清空仍不能证明成功 |
| AUD18-207 中 | 没做 | 选区和窗口映射在任务范围外的 `system/ui/textfield.c`，需要界面任务处理 |
| AUD18-208 中 | 保留修复 | `text`：隐藏字段不可由 AI 提交 |
| AUD18-209 中 | 保留修复 | `text`：初始 UTF-8 按完整边界截断 |
| AUD18-401 高 | 保留修复 | `bindings`：字段/内容/光标过期拒绝 |
| AUD18-402 中 | 没做 | 普通键盘与页面组合快照涉及 UI 接口，属于较大项 |
| AUD18-403 高 | 保留修复 | `lowbattery`：保存超时不报全已保存；成功分支另测 |
| AUD18-404 低 | 保留后续 | §1.3 的能力清单仍漏项，未改范围外章节迁就实现 |
| S6-01 高 | 保留修复 | `handoff`：旧事件发送缓冲及空间关闭边界 |
| S6-02 高 | 保留修复并补 R1-02 | `isolation`、`playback`：采集、迟到帧、已缓冲播放 |

【实现】本轮沿用设置表、策略/确认关口、数据服务及音频任务的分工；按钮、AI 和直接设置接口执行同一风险规则，未让 UI 直接改服务状态。待删除成员由 `member.list.removing` 提供结构化状态，重试仍走 `member.remove` 原子能力。文件清理、槽提交和缓存遗忘是内部机制，不新建无意义的 AI 工具。原 C 路的 UI 耦合、组合快照、协议/同步边界仍保留，不能据六项修复宣称全系统解耦。

复现命令（Python、CTest 均取 `$INK_DEPS/build-deps/pyenv/bin/`；配置沿用总规则的 Werror、字体及只读依赖缓存，`build-asan` 另开 `INK_SANITIZE=ON`）：

```bash
# V1: 每条完成后才执行下一条
cmake --build build -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3

# V2: 全部 19 组；升级原组 12 次启动，新增升级 16 次、删除 12 次启动
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash removal
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --build build-asan files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash removal
INK_FINISH_SAVED=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --build build-asan lowbattery

# V3: 只读替换对应源码，预期退出 1；不回滚工作区
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline d74a261 permissions undo playback activity upgrade_crash removal

# V4: 指定基线的隐私检查仅统计，避免打印继承的敏感内容
# 另检查 git diff --stat、git diff --check、相对链接、文件大小及 git status
```

本地忽略目录证据：修前 `build/services-review1-before.log`；新增权限撤销修前 `build/services-review1-permission-undo-before.log`（5 个失败），修后 `build/services-review1-permission-undo-final.log`（permissions/undo/policy 均通过）。最新常规构建 `build/services-review1-fix-build.log`、全量 `build/services-review1-ctest-final.log`、19 组回归 `build/services-review1-regressions-final.log`（产物 `build/services-finish-s6a1in6u/`）；最终反向 `build/services-review1-reverse-final.log`，六组均检出问题，退出 1（预期），产物 `build/services-finish-i9bv5wg4/`。内存检查及结束检查待补齐。

## 没做到的

- AUD18-106、206、207、402 四项中级和 AUD18-404 低级仍未做，原因逐项列在验收表，未把它们划出产品需求。
- 没有 Linux、真机、真实中枢验证；没有运行窗口模拟器。确认结果持久化、`confirm.status`、配对、同步和三秒对外撤销仍不是本轮交付。
- 活动日志仍异步写入，意外断电可能丢失待记条目；容量先满会早于成人 365/儿童 180 天查询窗口回收，无中枢归档。

## 和设计不一致的

- 上述四项中级及文档能力总表仍有缺口。只更新允许的设计 §7、§15；没有修改规范迁就代码。
- 必要范围例外：`system/include/ink/sys/settings.h` 增加设置风险字段；`system/include/ink/sys/data.h` 增加能报告缓存占用错误的遗忘接口，原 void 接口保留。二者是 R1-04、R1-05 所需的最小声明变更，未改层次或硬件接口。
- 删除在清理失败时保留隔离编号，需要人重试；未做自动清理后台任务。Flash 数据目录按现有布局为扁平目录；遇到异常子目录返回失败并保留隔离，未谎报完成。

## 改过的已有测试

未删测试、放宽断言、增加预算或用等待掩盖竞态。新增等待只用于跨越既定撤销截止时间、测量换人后的播放、或明确持久化/回调完成。

- `people.c` 两处闲置保护设置由直接 done 改为真人确认后取值；保留原闲置时间、PIN 尝试及删除断言。
- `media.c` 两处家长音量上限设置加真实确认；儿童修改拒绝、锁屏听书、音量键断言保留。
- `textinput.c` 两处剪贴板权限设置加真实确认；默认拒绝、敏感剪贴板、到期清理和成员切换断言保留。
- `whole_product.h` 仅加上述测试共用的真人设置确认辅助函数，实际调用确认服务，不伪造 `INK_SRC_CONFIRMED`。
- `run.py` 添加六组回归、多启动矩阵、源码替换和失败日志保存；原 13 组的断言及预算未改。

## 要用户定的

- 本轮六项修复无待定产品选择。D-95 计时继续按原值；未修改需求、想法、决策文件。
- 原四项较大中级及低级清单缺口需后续安排；本轮未扩展到界面设计、采购或发布。

## 顺手发现

- 权限变更的撤销若继承通用 `CONFIRMED` 来源，AI 可撤销真人关闭权限而重新获权。已纳入 R1-04 修复和回归，不另留安全绕过。
- 首次常规全量为 84/86：同一成员锁屏听书被停、删除标记的固定 2 秒等待早于墨水屏约 2.156 秒写入等待。保留原测试，恢复同一成员锁屏听书；删除改为使用调用现有剩余期限，没有延长能力期限或预算。两项定点重测通过，最终仍须全量和内存检查。
