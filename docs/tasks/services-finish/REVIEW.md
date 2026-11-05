# 系统服务收尾复查（第 1 轮）

结论：**不通过，1 严重、4 高、1 中。** 常规与 ASan/UBSan 各 86 项既有测试、开发者新增的 13 组回归均通过，但补充实验复现六处功能错误。最严重的是 AI 无需人工确认即可打开自己的剪贴板读取权限、修改儿童规则；S2 和 S6 不能验收。

复查日期：2026-11-05。对象：`6f46486`；用户指定基线：`fdff779`。实现前提交为 `70d3a43`，反向对照使用该版本，避免把此前已有的修复和审计材料混算成本轮开发。本报告覆盖旧内容；结论来自本次独立读代码与执行命令，不沿用开发报告的“通过”。

遵循 `review-prompt.md`：只修改本报告，不修改产品代码、既有测试及设计文档，不提交。构建最多 `-j4`，CTest 最多 `-j3`，构建与全量测试串行；不启动带窗口的模拟器。构建、测试仅产生忽略目录中的运行产物。

## 验收核对

| 编号 | 结论 | 独立证据与命令 |
| --- | --- | --- |
| S1 | 部分通过，不能收尾 | 原 11 高、6 中的指定场景通过，旧代码能反向检出；4 中、1 低如实列后续。升级修复新增 R1-01。见下方 22 项处理表、验证命令 V2/V3、附录 `upgrade`。 |
| S2 | 不通过 | `_meta.asked`、普通设置、确认绑定、密码拒绝、活动查询的现有正反例通过；实际高风险设置、活动满额与连续撤销失败（R1-03/04/06）。V2 的 `policy`；附录 `activity permissions undo`。 |
| S3 | 通过 | `actions` 用实际能力表检验 listen/ask，长度确实大于 768，完整发送；旧实现两项失败。【实现】请求缓冲按长度分配到外部内存。V2/V3 的 `actions`；原预算通过。 |
| S4 | 通过现有测试和内存检查 | V1 两种构建均退出 0、86/86；V2 两种构建 13 组均通过；预算未改。功能遗漏见本报告六项，不能由全绿测试推导产品正确。 |
| S6 | 不通过 | C 路六个范围已补查；原事件/采集隔离修复的正反例通过，但缓冲语音交接、删除失败后的身份复用仍泄漏（R1-02/05）。V1、V2 的 `handoff isolation`，附录 `playback removal`。 |
| S5 | 结束检查已执行，整体不满足完成条件 | 差异、隐私、链接、文件大小及原测试改动已核对，细节见“范围与规则检查”；六项问题尚未修复。复查按要求不提交。 |

## 已确认问题

### R1-01（高）：升级自检的状态提交顺序新增了永久阻塞升级的掉电窗口

- 位置：`system/services/update/update_service.c:1004`、`:1011`、`:1041`、`:928`、`:329`。
- 【实测】附录 `upgrade` 探针在 `resources` 已持久化、`hal_ota_mark_valid()` 尚未执行的边界模拟重启。下一次启动回到 100 版，但恢复记录仍为 200 版的 `resources`；资源写入失败，后续安装被拒绝。实测 `review.rolled_back=1`、`update.resources_failed=1`，两个断言失败。没有切断该边界的控制组成功启动 200 版并写完 12,288 字节资源。【实现】启动只清理 `trying/writing` 的回滚，`resources` 又必须与运行版本一致，因而该状态没有自动收敛路径，会持续拒绝新升级。
- 复现命令：附录命令设置 `INK_REVIEW_CASES=upgrade`；证据 `build/services-review1-b2xtvfuu/cut-next-boot-out/metrics.json`。再设置 `INK_REVIEW_BASELINE=70d3a43`，相同切断时机能正确清除事务并允许后续安装，退出 0（`build/services-review1-mjewn5q_/`），证明是本轮回归。
- 修法：恢复状态必须能区分“提交有效槽之前”和“有效槽之后”；开机按实际运行槽、版本与回滚标志核对并收敛事务，不能把已回滚版本的资源恢复作为所有新升级的永久前置条件。补这两个持久化动作之间断电的多启动回归。

### R1-02（高）：成员切换只封住新的 TTS 帧，已缓冲的私人语音仍播放给下一位

- 位置：`system/services/ai/ai_service.c:837`、`:840`、`system/services/audio/audio_service.c:716`；覆盖缺口在 `tests/services_finish/isolation.c:50`。
- 【实测】附录 `playback`：经真实 AI 下行回调喂入合成声音，阻塞 UI 后切到孩子，先等待 200 ms，再测 1 秒，仍外放 1,005 ms。直接中止旧流的控制组同一测量为 0 ms。现有回归只检查迟到帧不能创建新流，不检查队列里已有的声音。
- 复现命令：附录命令设置 `INK_REVIEW_CASES=playback`；证据 `build/services-review1-b2xtvfuu/playback-out/metrics.json` 及同目录的 `playback-control-out/metrics.json`。
- 修法：成员变更的直接路径同时作废旧成员播放流、预缓冲和在途输出，由音频任务独立停播；不能等 UI 执行 dismiss。补“已在播放且 UI 忙”的测试，覆盖媒体队列拥堵的同类边界。

### R1-03（高）：活动记录写满后占住全局待记槽，所有成员的 AI 写操作无法自动恢复

- 位置：`system/policy/cap.c:162`、`:240`、`:242`、`:571`。
- 【实测】附录 `activity` 以合成的普通本地写能力经过真实策略关口，连续接受 1,229 次后已有记录 131,004 字节，触及 128 KiB 配额而无法继续追加，16 个待记槽全部占用；等待 2.2 秒后仍拒绝动作，切到孩子也拒绝。人直接执行普通动作的控制成功。【实现】`activity_flush()` 对永久容量错误无限重试，过期过滤不物理删除记录，也没有归档或用户可用的回收途径；同一全局槽池还供 `INK_SRC_CONFIRMED` 使用，人工确认后的动作也会受阻。
- 复现命令：附录命令设置 `INK_REVIEW_CASES=activity`；证据 `build/services-review1-b2xtvfuu/activity-out/metrics.json`。这是日志配额问题，不是模拟内存耗尽；重启至多腾出临时槽，不能消除持久日志已满的原因。
- 修法：区分暂时写入失败和容量耗尽，给日志实现有界回收/归档和明确的满额处理；待记容量按成员隔离，避免单人的日志阻塞全家和人工确认后的动作。保留拒绝无审计写入的安全性，同时提供可恢复状态；不能只无限重试。

### R1-04（严重）：权限和儿童规则仍按普通设置处理，AI 可以给自己开权限

- 位置：`system/policy/settings.c:194`、`:316`、`:333`，`system/services/text/text_service.c:534`，`system/services/audio/audio_service.c:739`。
- 【实测】附录 `permissions`：真人写入合成私人剪贴板；AI 初次 `clipboard.read` 正确拒绝，随后 AI 调 `settings.set {id:"ai.read_clipboard",value:true}` 直接得到 `done`、无人工确认，再读即得到原文。`audio.child_max` 从 60 调成 100 同样直接 `done`。
- 依据：S2 和 D-105 明确将权限、改孩子的规则留给人工确认。当前只给能力描述标风险，没有给通用 `settings.set` 的具体目标分级；这不是非高风险设置自动执行的授权。属于本轮验收范围内漏掉的既有设置边界，不能把 S2 整体记为通过。
- 复现命令：附录命令设置 `INK_REVIEW_CASES=permissions`；证据 `build/services-review1-b2xtvfuu/permissions-out/metrics.json`（3 个断言失败）。此过程无 IO 故障或特殊时序，必需的人工授权被直接绕过，按复查提示定为严重。
- 修法：按设置项声明权限/儿童规则等风险，在能力入口绑定 id/value 并走人工确认及执行前重校验；AI 不能自行开放自己的数据权限。添加这两个真实设置的正反例，保留普通音量/亮度自动执行。

### R1-05（高）：删除成员忽略个人目录清理失败，复用编号的孩子能读到原成人文件

- 位置：`system/policy/members.c:541`、`:545`、`:547`、`:483`；相关文件服务返回错误在 `system/services/file/file_service.c:925`。
- 【实测】附录 `removal`：正常新增成人并写入个人文件，只让删除时该目录的一次 `hal_fs_opendir()` 失败。真人确认删除得到 `done`，随后可用同一编号新增孩子；孩子通过正常 `file.read /me/private.txt` 读到 `SYNTHETIC_OLD_ADULT_DATA`。不注入 IO 故障的控制组正确清空文件。
- 复现命令：附录命令设置 `INK_REVIEW_CASES=removal`；证据 `build/services-review1-b2xtvfuu/removal-out/metrics.json`、同目录的 `removal-control-out/metrics.json`。
- 原因：文件服务修复后会报告清理失败，但 `cap_remove()` 先删除身份，随后忽略 `ink_fs_forget_member()` 的 IO 错误，既无清理待办也不保留编号；`cap_add()` 直接复用该目录。本项是 S6 补查范围内的既有缺陷。
- 修法：成员删除作为可恢复事务处理，清理未完成时保留墓碑/隔离标记并禁止编号复用；等 Flash、卡与内存集合均清理成功后才宣告完成。增加“删除失败→重启/重试→同编号新增”的隔离回归。

### R1-06（中）：同一设置连续改两次，撤销选中了第一笔操作

- 位置：`system/policy/settings.c:316`、`system/policy/cap.c:1085`。
- 【实测】附录 `undo`：`display.light` 从 0 改为 10，再改为 20，使用第二次回执的 `setting:display.light` 撤销，实际回到 0，预期为 10。
- 复现命令：附录命令设置 `INK_REVIEW_CASES=undo`；证据 `build/services-review1-b2xtvfuu/undo-out/metrics.json`。
- 原因与修法：两条撤销记录使用同一 object，`cap_undo()` 从槽位 0 起找第一条，且未绑定设置的变更版本。为每次操作提供独立撤销标识或明确选择最新记录，执行前核对当前版本，避免覆盖后来人工修改；补连续修改及过期撤销测试。

## 原 A/B/C2 逐项核对

以下“通过”只表示原问题的指定场景；不抵消上面的新发现。V2 常规/内存检查均执行，V3 换回开发前对应服务源码，原缺陷检查确实失败；没有回退工作区。原报告共 22 项，11 高、6 中修复的场景可复现，4 中、1 低保留。

| 原编号 | 原等级 | 独立结论与证据 |
| --- | --- | --- |
| AUD18-101 | 高 | `files`：无法打开成员根目录返回错误，重试成功；旧源码误报成功。调用者未处理错误另见 R1-05。 |
| AUD18-102 | 高 | `files`：目录与原子短写持卡期间无法借出卡；旧源码可穿透持卡边界。 |
| AUD18-103 | 中 | `files`：强制作业先结束、提交后返回，任务仍正确结束；旧源码永久 running。 |
| AUD18-104 | 高 | `update`：两类状态写入拒绝不切槽，丢失试运行记录不标有效；旧源码失败。自检阶段新增掉电窗口见 R1-01。 |
| AUD18-105 | 高 | `update/resources-preserved`：资源失败后的恢复事务不被 app-only 包覆盖；旧源码失败。 |
| AUD18-106 | 中 | 【实现】仍仅记最后一个失败版本。开发报告明确未做失败历史及人工重试规则，按任务允许保留较大项。 |
| AUD18-107 | 中 | `update/tables`：正常包通过、8 类坏包拒绝；旧源码误收重叠、重复、未知、空段四类。 |
| AUD18-108 | 中 | `update/offer-changed`、`offer-kept`：下载身份与长度固定；旧源码两条链均失败。 |
| AUD18-109 | 高 | `storage`：遗忘集合后无关计时器与延迟落盘正常；旧源码损坏共享计时链。 |
| AUD18-201 | 高 | `audio`、`voice`：媒体/UI 忙时仍停止采集和上传；旧源码多录约 3,240 ms，听写边界也失败。 |
| AUD18-202 | 高 | `text`：快照 OOM 后不以旧 redo 覆盖新文字；旧源码失败。 |
| AUD18-203 | 高 | `shutdown`：断电钩子看到录音已关闭且 durable == last；旧源码两项失败。 |
| AUD18-204 | 高 | `audio`：耳机掉线且媒体队列忙时，旧私有媒体外放增量为 0；旧源码约 990 ms。 |
| AUD18-205 | 中 | `audio`：通话后保留闹钟 ALERT 焦点并拒绝录音；旧源码失败。 |
| AUD18-206 | 中 | 【实现】仍缺具体关机帧成功显示的回执，busy 清空不充分。开发报告明确保留跨显示/界面改动。 |
| AUD18-207 | 中 | 【实现】选区与窗口位置映射未修，位于任务范围外的 `system/ui/textfield.c`；已列后续。 |
| AUD18-208 | 中 | `text`：AI 不可提交隐藏字段；旧源码能触发提交。 |
| AUD18-209 | 中 | `text`：初始文本按完整 UTF-8 边界截断；旧源码留下残缺字。 |
| AUD18-401 | 高 | `bindings`：session/revision 过期的字段、内容、光标请求拒绝，正确目标成功；旧源码 8 项失败。 |
| AUD18-402 | 中 | 【实现】普通/数字键盘与页面组合的语义快照未补；报告未冒充完成，保留后续。 |
| AUD18-403 | 高 | `lowbattery`：超时不再称全已保存；旧源码两项失败。另独立运行 `INK_FINISH_SAVED=1`，两种构建各 5 检查通过，正常保存仍报成功。 |
| AUD18-404 | 低 | 【实现】设计 §1.3 仍漏若干能力；`integration-ai-surface` 对照源码清单与运行时，不验证文档完整性。报告已列明。 |

## C 路六项补查

| 范围 | 本次结论 |
| --- | --- |
| 模块与总线 | 【实现】存储介质、数据集合、文件作业、媒体、时间/电源基本按职责分开。升级包与拼音系统资源直接经 HAL 并持卡属于内部机制，不必伪装成 AI 的分页文本能力。V1 的分层检查通过；AI 服务仍有已声明的 UI 耦合例外，不能宣称全系统已完成解耦。 |
| AI 原生、回执与撤销 | 【实现/实测】策略、结构化结果、快照和活动查询已有实际路径；高风险设置未按对象分级（R1-04），活动容量与撤销语义不完整（R1-03/06）。普通键盘、关机帧确认的已知缺口仍保留，不能把“能力存在”当成正确完成。 |
| 能力开放 | 【实测】V1 的 `integration-ai-surface` 检查真实 tools/list，V2 的 `actions` 检查真实请求。成员解锁/设 PIN、确认接受/拒绝、两种安装入口六项 OWNER 例外理由成立；文档清单缺漏见 AUD18-404。 |
| 成员隔离 | 【实测】`handoff`/`isolation` 各 15 检查通过，V3 分别检出旧源码 4/3 个失败，证明事件排队、迟到帧、采集切断的修复有效。但已有 TTS 缓冲与删除残留未被这两组覆盖，R1-02/05 仍失败。通知、确认、计时、应用状态及成员门路径结合代码和 V1 复核；不把通过的局部分支扩大为完整隔离保证。 |
| 文档真实性 | 【文档/实现】§7、§15 已更新请求标记、活动记录和限制；其他保留项在报告中明确。§15.4“已在现有能力落实”及开发报告 S2/S6 的整体通过结论被本次实验推翻，修复后应据实更新；本次只写 REVIEW。 |
| 原修复复核 | 【实测】V1 含 `integration-people-reset`、`integration-races`、`integration-listening`，覆盖 AUD17-003/004、AUD16-001/007/008 所在路径；第十四轮相关 data、files、people、ai-surface、time、apps、notify、confirm 回归也通过。不据此声称穷尽所有时序；未做密码尝试次数或等待时间专门实验。 |

## 独立验证命令与结果

环境使用用户提供的 `$INK_DEPS`、`$INK_DESIGN`；后者未使用（本任务不验界面外观）。两套 CMake 缓存均 `INK_WERROR=ON`，内存检查目录另有 `INK_SANITIZE=ON`。下列构建与测试轮次串行执行；GUI smoke 使用 `SDL_VIDEODRIVER=dummy`，没有打开窗口。

V1：两次构建退出 0；CTest 常规 86/86、35.59 秒，ASan/UBSan 86/86、69.87 秒。日志分别在 `build/`、`build-asan/` 的 `services-finish-review1-build.log`、`services-finish-review1-ctest.log`。

```bash
cmake --build build -j4
$INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
```

V2：下列 13 组在两种构建均退出 0；升级组内含 12 次链式启动。最终产物 `build/services-finish-f8se2hx8/`、`build-asan/services-finish-mrb9ytch/`，汇总日志分别为 `build/services-finish-review1-regressions.log`、`build-asan/services-finish-review1-regressions-strict.log`。ASan/UBSan 另以遇错终止模式复跑确认。这 13 组未接入 CTest，已单独执行，不能只引用 86/86 代替它们。

```bash
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --build build-asan files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions
INK_FINISH_SAVED=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --build build-asan lowbattery
```

低电量成功分支另存于 `build/services-finish-_nygcyh2/`、`build-asan/services-finish-p16qmoo4/`，各 5 检查通过。13 组通过数：files 18、storage 7、text 8、bindings 16、lowbattery 6、audio 17、voice 8、handoff 15、isolation 15、shutdown 6、policy 30、actions 6；update 按启动链判断。

V3：反向对照退出 **1（预期）**，13 组均检出旧代码对应问题。日志 `build/services-finish-review1-reverse.log`，产物 `build/services-finish-nya5nhlo/`。这是在忽略目录复制旧源码再链接，未改当前产品文件。

```bash
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline 70d3a43 files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions
```

V4：附录六个补充场景在常规和严格 ASan/UBSan 下各复现 10 个功能断言失败，脚本退出 **1（发现问题）**。正常升级、预先停止旧播放、正常删除三组控制通过；无 sanitizer 诊断。产物分别为 `build/services-review1-b2xtvfuu/`、`build-asan/services-review1-kt9drv4y/`。模拟器退出 0 不代表断言通过，附录脚本另外读取 `test.fail`。

【实测】PQBN AI 场景两种构建的 `mem.internal_total.min_free_bytes=33192`，原门槛 32768，余量 424 字节，内部失败分配为 0。证据为各构建目录的 `test-out/ai_ask-metalio-plus-pqbn/metrics.json`。未把单独动态池的 112 字节余量与总内部内存预算混为一谈，未修改预算。

## 范围与规则检查

- 【实测】初始工作区干净，复查只新增/覆盖本报告；未提交、推送、调用子代理、连接设备或外部付费服务。构建产物留在被忽略目录。没有 Linux、真机或真实中枢的验证；不据模拟器结果宣称这些通过。
- 【实测】`git diff --stat 70d3a43..HEAD` 共 62 文件，其中三份任务/规则文件属于用户提交，开发内容 59 文件。58 个位于允许范围；唯一范围例外 `simulator/desktop/gui/gui_main.cpp` 是加严 UBSan 后的启动阻挡项，修改仅处理无效鼠标坐标，原 smoke 输入/断言保留。UI 仅关机文案一行，设计只改 §7/§15；没有发现夹带重构或依赖升级。
- 【实测】指定隐私正则检查 `fdff779..HEAD` 为 4,134 匹配行；`fdff779..70d3a43` 同为 4,134；本轮 `70d3a43..HEAD` 为 0。不复制这些继承日志中的本机路径，也不修改原审计文件。差异文件均不超过 5 MB，开发改动 Markdown 中 159 个相对文件链接无失效。
- 【实测】`git diff --check 70d3a43..HEAD` 退出 0。完整指定基线差异 `git diff --check fdff779..HEAD` 退出 2，含 1,157 条空白诊断，与开工前完全相同，来自继承的审计日志/报告；不能笼统说完整差异无空白问题。
- 【实现】逐项检查已有测试差异：confirm/systest/time 的普通设置直接执行与撤销，files/notify/people 的危险动作人工确认，people 的 AI 明文密码拒绝，都符合 D-105 对应行为；whole_product 为普通文字调用补 session/revision，显式过期目标测试保留。ai_surface 更新清单，lifecycle/systest 补初始化，GUI smoke 加严 UBSan。没有删除整项测试、放宽预算或发现以新增 sleep 掩盖失败的改动。缺陷是未覆盖的真实场景，尤其 `policy` 用普通测试设置不能证明所有真实设置安全。
- 【文档核查】UI 外观不在此任务范围，十屏视觉对照不适用；D-95 计时和密码尝试策略未触碰。上述六项均为现有实现应满足的行为，无需等待新的产品决定。

## 补充实验脚本

下面的脚本从本报告读取后执行；只用现有构建参数逐个编译 stdin 中的探针，产物在忽略的构建目录，不改产品源文件。默认执行全部六项。单项复现在 Python 命令前加 `INK_REVIEW_CASES=upgrade`（或该项名称）；内存检查加 `INK_REVIEW_BUILD=build-asan`；升级旧代码对照同时加 `INK_REVIEW_CASES=upgrade INK_REVIEW_BASELINE=70d3a43`。脚本对每个模拟器进程启用 `UBSAN_OPTIONS=halt_on_error=1`，每次启动使用新的模拟数据，只有升级链有意共享模拟 Flash/卡。运行方式：

```bash
$INK_DEPS/build-deps/pyenv/bin/python - <<'PY'
from pathlib import Path
s = Path('docs/tasks/services-finish/REVIEW.md').read_text()
exec(s.split('<!-- REVIEW_PROBE_BEGIN -->\n```python\n', 1)[1].split('\n```', 1)[0])
PY
```

<!-- REVIEW_PROBE_BEGIN -->
```python
import importlib.util, json, os, shlex, subprocess, tempfile
from pathlib import Path
root = Path.cwd()
build = (root / os.environ.get('INK_REVIEW_BUILD', 'build')).resolve()
work = Path(tempfile.mkdtemp(prefix='services-review1-', dir=build))
common = '#include "tests/services_finish/product.h"\n'
entry = '''
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="review-probe", .fn=review_run,
        .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1,
        .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "review task");
}
'''
sources = {}
sources['upgrade'] = common + r'''
#include "ink/hal_ota.h"
static int review_mark_valid(void) {
    if (getenv("INK_REVIEW_CUT")) {
        ink_probe_count("review.cut_before_valid");
        hal_power_reboot();
        for (;;) osal_task_delay_us(1000000);
    }
    return hal_ota_mark_valid();
}
#define hal_ota_mark_valid review_mark_valid
#include "system/services/update/update_service.c"
#undef hal_ota_mark_valid
static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    if (getenv("INK_REVIEW_INSTALL")) {
        CHECK(BY_HAND("system.upgrade_from_card", "{\"file\":\"next.inkfw\"}") &&
            g_rc.status == INK_RC_PENDING_CONFIRMATION, "installation asks");
        unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token);
        CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person accepts");
    } else if (!getenv("INK_REVIEW_CUT")) {
        delay_ms(6000);
        ink_probe_value("review.version", U.me.version_code);
        ink_probe_value("review.rolled_back", U.me.rolled_back);
        CHECK(!U.st.state[0], "rollback must leave a recoverable transaction: version=%u state=%s code=%u path=%s",
            U.me.version_code, U.st.state, U.st.code, U.st.path);
        const char *why = not_now();
        CHECK(!why, "future installation must not be permanently blocked: %s", why ? why : "allowed");
    }
    ink_probe_count("test.finished");
}
''' + entry
sources['playback'] = common + r'''
#include "system/services/ai/ai_service.c"
static osal_sem_t *review_in, *review_go;
static ink_exec_job_t review_hold;
static void review_busy(void *arg) {
    (void)arg; osal_sem_give(review_in); osal_sem_take(review_go, OSAL_WAIT_FOREVER);
}
static void review_ask(void) { ink_ai_ask_text("SYNTHETIC_OWNER_REQUEST"); }
static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    on_ui(review_ask);
    review_in = osal_sem_create(0, 1); review_go = osal_sem_create(0, 1);
    ink_exec_post(INK_EXEC_UI, &review_hold, review_busy, NULL);
    CHECK(osal_sem_take(review_in, 2000000) == OSAL_OK, "UI held");
    uint8_t packet[160];
    int n = sim_opus_fake_packet(packet, sizeof packet, 440, 0.2, 120, 1);
    for (int i=0; i<100; i++) on_rt_frame(NULL, AI.stream, INK_RT_VOICE_DOWN, packet, (size_t)n, 0, 0, 0);
    CHECK(WAIT_FOR(sim_audio_played_ms() > 200, 2000), "owner TTS is playing");
    if (getenv("INK_REVIEW_CONTROL")) ink_audio_stream_abort(AI.tts_stream);
    CHECK(ink_member_switch("kid") == 0, "switch to child while UI busy");
    delay_ms(200);
    double before = sim_audio_played_ms();
    delay_ms(1000);
    double extra = sim_audio_played_ms() - before;
    ink_probe_value("review.private_audio_after_handoff_ms", extra);
    CHECK(extra == 0, "owner TTS continues after handoff: %.0f ms", extra);
    osal_sem_give(review_go);
    ink_probe_count("test.finished");
}
''' + entry
sources['activity'] = common + r'''
#include "system/policy/cap.c"
static int review_ran;
static void review_action(ink_call_t *c) { (void)c; review_ran++; }
static const ink_cap_t REVIEW_ACTION = {.id="review.action", .title="测试动作",
    .effect=INK_FX_LOCAL_WRITE, .child_ok=true, .exec=INK_EXEC_DIRECT, .run=review_action};
static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_cap_register(&REVIEW_ACTION, 1) == 0, "register synthetic action");
    int accepted = 0;
    for (int i=0; i<1600; i++) {
        if (!AI("review.action", NULL)) break;
        accepted++; delay_ms(100);
    }
    delay_ms(2200);
    int pending = 0;
    for (int i=0; i<ACTIVITY_PENDING; i++) pending += AL.pending[i].used;
    ink_db_t *db = ink_db_open(&ACTIVITY, "owner");
    ink_db_cursor_t at = {0}; ink_rec_t rec; size_t bytes = 0;
    while (db && ink_db_next(db, &at, &rec, NULL, 0) == 1) bytes += rec.len;
    ink_probe_value("review.activity_bytes", bytes);
    if (db) ink_db_close(db);
    ink_probe_value("review.actions_before_full", accepted);
    ink_probe_value("review.activity_pending", pending);
    CHECK(AI("review.action", NULL), "ordinary AI actions stopped: accepted=%d pending=%d", accepted, pending);
    CHECK(ink_member_switch("kid") == 0, "switch to unaffected member");
    delay_ms(200);
    CHECK(AI("review.action", NULL), "one member's full log blocks the other member too");
    CHECK(BY_HAND("review.action", NULL), "ordinary human action control");
    ink_probe_count("test.finished");
}
''' + entry
sources['permissions'] = common + r'''
static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_clip_set("SYNTHETIC_PRIVATE_CLIPBOARD", false, INK_SRC_TOUCH) == 0, "human clipboard");
    CHECK(AI("clipboard.read", NULL) && g_rc.status == INK_RC_REJECTED, "permission initially denies AI");
    CHECK(AI("settings.set", "{\"id\":\"ai.read_clipboard\",\"value\":true}") &&
        g_rc.status == INK_RC_PENDING_CONFIRMATION, "permission change must require a person, actual=%s", ink_rc_name(g_rc.status));
    CHECK(AI("clipboard.read", NULL) && !strstr(g_result, "SYNTHETIC_PRIVATE_CLIPBOARD"),
        "AI enabled its own permission and read the clipboard: %s", g_result);
    CHECK(AI("settings.set", "{\"id\":\"audio.child_max\",\"value\":100}") &&
        g_rc.status == INK_RC_PENDING_CONFIRMATION, "child rule change must ask, actual=%s", ink_rc_name(g_rc.status));
    ink_probe_count("test.finished");
}
''' + entry
sources['removal'] = common + r'''
static int review_fail_dir;
static hal_dir_t *review_opendir(const char *path) {
    if (review_fail_dir && !strcmp(path, "/content/.m/oldadult")) {
        review_fail_dir = 0;
        ink_probe_count("review.directory_io_failure");
        return NULL;
    }
    return hal_fs_opendir(path);
}
#define hal_fs_opendir review_opendir
#include "system/services/file/file_service.c"
#undef hal_fs_opendir
static int review_results;
static char review_status[24];
static void review_result(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id;
    ink_args_get_str(args, "status", review_status, sizeof review_status);
    review_results++;
}
static bool review_confirm(const char *cap, const char *args) {
    if (!BY_HAND(cap, args) || g_rc.status != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token);
    int before = review_results;
    return ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 &&
        WAIT_FOR(review_results > before, 3000) && !strcmp(review_status, "done");
}
static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result", review_result, NULL, INK_EXEC_DIRECT) == 0, "result listener");
    CHECK(review_confirm("member.add", "{\"id\":\"oldadult\",\"role\":\"adult\"}"), "add old adult with human confirmation");
    CHECK(ink_member_switch("oldadult") == 0, "old adult uses device");
    CHECK(ink_fs_write_atomic("/me/private.txt", "SYNTHETIC_OLD_ADULT_DATA", 24) == 0, "old adult writes private data");
    CHECK(ink_member_switch("owner") == 0, "owner manages family");
    delay_ms(200);
    review_fail_dir = getenv("INK_REVIEW_CONTROL") ? 0 : 1;
    CHECK(review_confirm("member.remove", "{\"id\":\"oldadult\"}"), "removal reported done");
    CHECK(review_confirm("member.add", "{\"id\":\"oldadult\",\"name\":\"SYNTHETIC_NEW_CHILD\",\"role\":\"child\"}"), "reuse id for new child");
    CHECK(ink_member_switch("oldadult") == 0, "new child uses device");
    delay_ms(200);
    CHECK(AI("file.read", "{\"path\":\"/me/private.txt\"}") && !strstr(g_result, "SYNTHETIC_OLD_ADULT_DATA"),
        "new child inherits deleted adult data: status=%s result=%s", ink_rc_name(g_rc.status), g_result);
    ink_probe_count("test.finished");
}
''' + entry
sources['undo'] = common + r'''
static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":10}") && g_rc.status==INK_RC_DONE, "first change");
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":20}") && g_rc.status==INK_RC_DONE, "second change");
    CHECK(AI("system.undo", "{\"object\":\"setting:display.light\"}") && g_rc.status==INK_RC_UNDONE, "undo second change");
    CHECK(ink_setting_get("display.light")==10, "undo must restore immediate previous value 10, actual=%lld",
        (long long)ink_setting_get("display.light"));
    ink_probe_count("test.finished");
}
''' + entry

flags = {}
for line in (build/'CMakeFiles/sim-media.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k,v = line.split(' = ',1); flags[k] = shlex.split(v)
link_template = shlex.split((build/'CMakeFiles/sim-media.dir/link.txt').read_text())
def compile_probe(name):
    obj = work/(name+'.o'); binary = work/name
    cmd = [link_template[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], '-I'+str(root),
           '-I'+str(root/'system/services/text'), '-I'+str(root/'system/policy'),
           *flags['C_FLAGS'], '-Wno-unused-function', '-x','c','-c','-','-o',str(obj)]
    source = sources[name]
    baseline = os.environ.get('INK_REVIEW_BASELINE')
    if baseline and name == 'upgrade':
        old = subprocess.check_output(['git','show',baseline+':system/services/update/update_service.c'],cwd=root,text=True)
        source = source.replace('#include "system/services/update/update_service.c"', old)
    subprocess.run(cmd,input=source,text=True,cwd=build,check=True)
    link = link_template.copy()
    for i,word in enumerate(link):
        if '/tests/integration/' in word and word.endswith('.o'): link[i] = str(obj)
    link[link.index('-o')+1] = str(binary)
    subprocess.run(link,cwd=build,check=True)
    return binary
failed = False
def boot(binary,label,extra=(),env=None,end='200s'):
    global failed
    out = work/(label+'-out')
    cmd = [str(binary),'--profile',str(root/'simulator/profiles/metalio-plus.json'),
           '--assets',str(build/'assets'),'--content',str(build/'test-content'),
           '--out',str(out),'--end',end,'--quiet','--no-random-outages',*map(str,extra)]
    r = subprocess.run(cmd,cwd=root,capture_output=True,text=True,timeout=180,
                       env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1',**(env or {})})
    (work/(label+'.stdout.log')).write_text(r.stdout)
    (work/(label+'.stderr.log')).write_text(r.stderr)
    data = json.loads((out/'metrics.json').read_text())
    m = data['metrics']
    if env and env.get('INK_REVIEW_CUT'):
        complete = m.get('review.cut_before_valid') == 1 and m.get('power.reboot') == 1
    else:
        complete = m.get('test.finished') == 1
    failed |= r.returncode != 0 or not complete or m.get('test.fail',0) != 0
    print(label,'exit='+str(r.returncode),{k:v for k,v in m.items() if k.startswith(('review.','test.','update.','ota.','power.reboot'))},flush=True)
    for warning in data.get('warnings',[]):
        if 'CHECK FAILED' in str(warning): print(warning,flush=True)
    return m
selected = os.environ.get('INK_REVIEW_CASES','upgrade playback activity permissions removal undo').split()
for name in selected:
    binary = compile_probe(name)
    if name == 'upgrade':
        spec = importlib.util.spec_from_file_location('inkfw',root/'tools/inkfw/make_inkfw.py')
        fw = importlib.util.module_from_spec(spec); spec.loader.exec_module(fw)
        for chain in ('control','cut'):
            sd = work/chain/'sd'; (sd/'content/update').mkdir(parents=True)
            (sd/'content/update/next.inkfw').write_bytes(fw.build('2.0.0',200,app_size=8192,font=4096,resources=8192)[0])
            extra = ['--flash-image',work/chain/'user.img','--sd-dir',sd]
            boot(binary,chain+'-install',extra,{'INK_REVIEW_INSTALL':'1'},'20s')
            boot(binary,chain+'-trial',extra,{'INK_REVIEW_CUT':'1'} if chain=='cut' else {},'20s')
            boot(binary,chain+'-next-boot',extra,end='20s')
    else:
        boot(binary,name,end='200s' if name=='activity' else '12s')
        if name in ('playback','removal'): boot(binary,name+'-control',env={'INK_REVIEW_CONTROL':'1'},end='12s')
print('Evidence:',work.relative_to(root))
raise SystemExit(1 if failed else 0)
```
<!-- REVIEW_PROBE_END -->

SEVERE=1 HIGH=4
