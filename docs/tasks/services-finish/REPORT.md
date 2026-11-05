# 系统服务收尾报告

2026-11-05，第 3 轮修复进行中。开工基线 `1daec81`，工作区干净。范围限于 REVIEW.md 的严重、高、中问题及成员删除、去重、撤销的同类入口；保留审查原文和下方上轮证据。本轮正向、反向、全量及 ASan/UBSan 结果待补齐，不提前宣称通过。构建最多 -j4，CTest 最多 -j3，构建与全量串行，不启动窗口模拟器。

阶段记录：【实测】R3-01 阅读器延迟保存的六种时序、R3-02 Wi-Fi 保存失败后旧票据、闹钟/提醒状态改变后旧撤销、延迟调用的票据成员归属，均已通过正向回归；替换为 `1daec81` 对应源码后四组均出现功能断言失败。`ed14461` 对应的八组反向也全部编译、链接成功并出现功能断言失败。最终常规全量 86/86、32 组服务回归、低电量保存对照和栈计量已通过；ASan/UBSan 最终验证仍进行中。证据：`build/services-r3-validation.json`、`build/services-r3-reverse-final.log`、`build/services-r3-inherited-reverse.log`。没有改预算、跳过测试或忽略 libc 返回值。

2026-11-05，第 2 轮修复。开工基线 `ed14461`，工作区干净；保留 [REVIEW.md](REVIEW.md) 原文。本轮覆盖其中全部严重、高、中问题，包括继承的四项中级；低级 AUD18-404 保留。已修复本轮八项问题，全部配正向测试和撤掉修复的反向对照；常规与 ASan/UBSan 的全量及服务回归均通过。最后的同类入口补齐已另重建两套并复验：常规全量 37.43 秒，ASan/UBSan 全量 68.56 秒，均 86/86；八组新增回归两套再次通过。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R2-01 严重 | 已修 | 【实现】成员编号先隔离；扫描设备级闹钟/提醒持久集合，清定时与响铃状态并等待落盘；清成员集合、原始 Flash 缓存/预取、成员与应用文件、全局临时缓存、正在运行和已结束的任务。通知、事件、活动日志、PIN 作业、撤销、应用状态和媒体断点校验成员代次。【实测】删除→同编号孩子→重启，以及延迟通知/活动/本地事件、任务取消、旧目录句柄、旧撤销对新文件均覆盖；旧源码失败 | V2/V3 `removal_alarm`；V2 `removal handoff isolation playback` |
| R2-02 高 | 已修 | 【实现】已完成和在途请求均绑定成员代次、请求号、能力、参数与来源；缓存前复查当前成员、空间与策略；本地、tools 和远端代理共用准备入口，异步回执也复查。页面版本属于执行前置条件，正常重试仍返回原回执。【实测】跨成员碰撞、旧成员重发、参数/能力冲突、在途重复、迟到回执、锁空间和正常同成员重试；旧源码失败 | V2/V3 `replay`；V1 `scenario-ai_page_version`、`scenario-ai_tools` |
| R2-03 高 | 已修 | 【实现】数据写入在锁内返回本次版本；记录、通知、Wi-Fi 的逆操作在删除锁内核验版本；文件创建/移动的逆操作与全部文件写入口共用修改锁；校时逆操作核验时钟修改版本。撤销票据另绑成员代次。【实测】人工更新/追加、原子替换、同编号重建、移动后修改、通知已读、人工校时和 Wi-Fi 重连均保留新内容；未修改记录/文件/移动仍可撤销；旧源码失败 | V2/V3 `undo_data`；V2 `undo permissions` |
| R2-04 中 | 已修 | 【实现】逐级创建缺失的父目录后验证空根；读失败、缓存/作业占用仍保留隔离编号。【实测】空白卡删除未使用成员成功；既有卡/Flash/缓存故障四链保留；旧源码失败 | V2/V3 `empty_removal`；V2 `removal files` |
| AUD18-106 中 | 已修 | 【实现】失败版本按版本号持久保存，迁移原 blocked 字段；历史不可读/写失败时保守拒绝；自动提示/选包不再忘掉较早失败版本。显式文件 + retry_failed 仍须真人确认，签名/产品/大小等校验照常。【实测】旧格式→第二次回滚→重启保留两个失败版本，正常版本控制与人工重试规则通过；旧源码失败 | V2/V3 `update_history`；V2 `update upgrade_crash` |
| AUD18-206 中 | 已修 | 【实现】关机卡在 UI 任务实际提交后记录帧号；等待 UI/显示结算且所有面板回调成功，才确认已显示。普通关机失败返回 failed；低电量仍允许紧急断电并如实记未显示。【实测】HAL 拒绝更新时不能以队列空冒充成功，恢复后实际显示并关机；旧源码失败 | V2/V3 `offpanel`；V1 `integration-power-menu`；V2 `shutdown lowbattery` |
| AUD18-207 中 | 已修 | 【实现】长选区窗口从选区起点计算，逐字符维护标签边界到原文位置的映射，包括两侧选择标记与输入中的拼音；UTF-8 按完整字符截取。【实测】跨窗口长选区、右括号后点击、拼音中点击；旧源码三项失败 | V2/V3 `text_window` |
| AUD18-402 中 | 已修 | 【实现】页面快照组合原页面和输入会话，复用 text.state 的序列化；普通、数字输入带内容、目标和版本，密码保留掩码。【实测】三种输入在真实 UI 任务组合读取；旧源码失败 | V2/V3 `keyboard_snapshot`；V1 `integration-text-main` |
| R1-01 高 | 保留修复 | 【实测】正常、有效槽提交前/后断电、提交失败四链 | V2 `upgrade_crash` |
| R1-02 高 | 保留修复 | 【实测】缓冲 TTS/听书交接，同成员锁屏听书 | V2 `playback`；V1 `integration-media` |
| R1-03 高 | 保留修复 | 【实测】1,600 次动作、单成员待记槽满额与恢复 | V2 `activity` |
| R1-04 严重 | 保留修复 | 【实测】真实权限、儿童上限、确认/拒绝/换人和权限撤销 | V2 `permissions policy` |
| R1-05 高 | 保留并补全 | 【实测】卡/Flash/缓存/唯一 PIN 四种故障的多启动清理与重试 | V2 `removal`；R2-01/04 |
| R1-06 中 | 保留并补全 | 【实测】设置连续撤销、过时版本和过期；非设置由 R2-03 补齐 | V2 `undo undo_data` |
| AUD18-101 高 | 保留修复 | 【实测】不可读根不报成功 | V2 `files` |
| AUD18-102 高 | 保留修复 | 【实测】目录和短写持卡，USB 独占不能穿透 | V2 `files` |
| AUD18-103 中 | 保留修复 | 【实测】作业先完成仍正确结束任务 | V2 `files` |
| AUD18-104 高 | 保留修复 | 【实测】拒绝状态写入不切槽，缺状态不标有效 | V2 `update upgrade_crash` |
| AUD18-105 高 | 保留修复 | 【实测】资源恢复事务不被新包覆盖 | V2 `update` |
| AUD18-107 中 | 保留修复 | 【实测】正常包及八类坏包 | V2 `update` |
| AUD18-108 中 | 保留修复 | 【实测】下载固定包身份，两条 offer 变化链 | V2 `update` |
| AUD18-109 高 | 保留修复 | 【实测】遗忘成员集合不破坏独立定时器 | V2 `storage` |
| AUD18-201 高 | 保留修复 | 【实测】拥堵时停止采集/上传 | V2 `audio voice` |
| AUD18-202 高 | 保留修复 | 【实测】OOM 后不能重做旧文本 | V2 `text` |
| AUD18-203 高 | 保留修复 | 【实测】录音结束且持久完成后关机 | V2 `shutdown` |
| AUD18-204 高 | 保留修复 | 【实测】耳机掉线不外放旧媒体 | V2 `audio` |
| AUD18-205 中 | 保留修复 | 【实测】通话结束保留闹钟焦点 | V2 `audio` |
| AUD18-208 中 | 保留修复 | 【实测】AI 不能提交隐藏字段 | V2 `text` |
| AUD18-209 中 | 保留修复 | 【实测】初始 UTF-8 截断保留完整字符 | V2 `text` |
| AUD18-401 高 | 保留修复 | 【实测】字段、内容、光标的会话/版本过期拒绝 | V2 `bindings` |
| S6-01 高 | 保留修复 | 【实测】旧事件缓冲及空间关闭边界 | V2 `handoff replay` |
| S6-02 高 | 保留修复 | 【实测】采集、迟到帧和缓冲播放隔离 | V2 `isolation playback` |
| S4-DET 中 | 保留修复 | 【实测】完整机器字栈计量及确定性比较 | V1；V2 `stack_meter.py` |
| S1/S2/S3/S6 | 通过本轮验证 | 【实测】新增八组、既有十九组；能力列表完整，所有入口仍走能力表/服务，UI 不直接改服务内部状态 | V1/V2/V3 |
| S4 | 通过 | 【实测】两套全量各 86/86，27 组回归、正常低电量保存和栈计量均通过，预算不变 | V1/V2 |
| S5 | 核查通过，本地提交 | 【实测】56 个改动文件均小于 5 MB；本轮与总基线隐私匹配均 0，相对链接有效，本轮 diff --check 为 0 | V4 |

成员数据排查范围（【实现】源码核查，结合上述实测）：

| 存放位置/状态 | 处理 |
| --- | --- |
| /user/m/<成员> 全部集合与原始文件 | 先遗忘 DB 缓存，检查原始存储队列，再清读取缓存/预取及 Flash 目录；待清理期间拒绝原始写入 |
| time.alarms 设备集合 | 按 owner 遍历持久记录，连同内存表、响铃、RTC/唤醒计划清理，持久完成后释放编号 |
| 通知/通知偏好、活动、媒体进度、应用状态 | 成员集合/目录统一删除；延迟通知、活动、应用/媒体保存另验代次 |
| /content/.m/<成员> 与 /app | 清完整成员目录，覆盖应用私有文件、录音等；活跃句柄或文件作业未结束则失败并保留编号 |
| /cache/sys/media-pos 等全局临时缓存 | 现有布局没有逐成员归属，删除成员时清全局可重建缓存；活跃缓存句柄/作业阻止清理 |
| 任务、撤销、去重、事件、PIN 作业 | 任务按所有者/创建者清理并取消；其余暂存内容按代次失效，迟到事件/回执不再可见 |
| 剪贴板、输入、AI/语音/媒体流、应用运行状态 | 保留原换人时撤销/停止机制；应用和媒体延迟持久化增加代次校验；既有 handoff/isolation/playback/text 回归保留 |
| member.table、net.wifi、bt.devices、升级状态/失败历史、家庭共享内容 | 前者仅删除目标身份；网络/蓝牙和升级是设计明确的设备共享数据，无成员所有权；家庭共享内容保留 |

V1（每步完成后才执行下一步；配置完整沿用总规则 §5，内存构建只增加 INK_SANITIZE=ON）：

```bash
cmake --build build -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
```

V2（两种构建均运行；第二套在 run.py 后加 --build build-asan）：

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash removal replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py
```

V3（只读取开工源码到忽略的构建目录；八组均须实际运行失败，整体预期退出 1；编译/链接错误不算证据）：

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline ed14461 replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel
```

旧版本兼容只补新增符号或接回旧回调契约，不代替被测逻辑：旧成员仍只有编号，旧时间/任务遗忘仍不做事；旧文件条件入口调用原无条件操作；旧关机回调按原契约允许继续断电；文本点击映射使用旧 place_of 的原公式。未回滚工作区、删断言或把链接失败当反例。原已修项的历史反向证据保留在 REVIEW.md，当前重跑其正向场景。

V4【实测】：`git diff --check ed14461` 退出 0；`git diff --stat ed14461` 与新增文件逐项核对，56 个相关文件均小于 5 MB。按总规则 §6 原隐私正则检查 `fdff779..ed14461`、本轮及完整 `fdff779` 差异，均为 0；没有输出继承敏感内容。完整基线空白检查退出 2，仅有继承审计报告末尾空行一条，开工时相同，按规则未修改该审计文件。新增/改动源码中的 libc write/read/fgets 调用为 0。报告相对链接均有效。证据 `build/services-review3-closure.json`。

【实测】两套 PQBN AI 场景总内部 RAM 最低空闲均 32,880 字节，门槛仍为 32,768，余 112；内部失败分配为 0。未放宽预算、增加任务或关闭确定性比较。

证据均在忽略的 build 与 build-asan 中：services-review3-build.log、services-review3-ctest-final.log、services-review3-regressions-final.log、services-review3-saved-final.log、services-review3-stack-final.log；最后两处总线入口补齐后另重建并重跑全量及八组新增回归，日志 services-review3-new-final.log；反向为 build/services-review3-reverse-final.log。每组 metrics 和 stdout/stderr 的目录由对应日志最后一行给出。

## 没做到的

- AUD18-404 低级能力文档清单保留，用户本次未要求修低级。没有改已有设计文档迁就实现。
- 没有 Linux、真机或真实中枢验证；没有窗口模拟器。新增源码不使用 libc write/read/fgets，未用忽略返回值规避 Linux Werror。

## 和设计不一致的

- 文件撤销采用保守的卡修改序号：其他文件修改、后台作业或 USB 交接也会使旧票据失效；会返回 stale，不会删除后来内容。未提供跨重启撤销，票据及运行代次都只在本次开机有效；持久私有数据靠清理事务保证编号复用安全。
- 失败版本历史有 128 KiB 容量，满额/读取/落盘失败会保守停止自动升级；不会丢掉旧失败版本。显式人工重试仍完整验包，不清除历史。
- 删除遇到占用或 I/O 失败仍需重试，沿用 removing 标记；未加后台常驻清理任务。

## 改过的已有测试

- 本轮未改既有 C 测试的期望、预算和等待阈值；只扩展 services_finish/run.py 的组别、持久启动矩阵、源码替换及兼容宏，新增八组回归与依赖边界探针。
- 新增等待用于明确作业/持久化完成或人为扣住后释放已有回调，不用延长睡眠掩盖竞态。未过滤确定性指标、关闭检查或跳过失败。

## 要用户定的

无；没有产品范围、采购、硬件、付费或发布动作。

## 顺手发现

- 原始 Flash 缓存、全局媒体断点缓存、旧目录句柄及应用/媒体延迟保存属于 R2-01 同类边界，已一起处理；Wi-Fi、通知、校时和移动逆操作属于 R2-03 同类入口，已补版本保护。
- 首轮全量曾检出一次性提醒、同步调用错误回执、锁屏允许入口、换人成功回执及页面重试被错误阻止；均修实现并保留原测试，最终结果以 V1 为准。
