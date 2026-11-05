# 系统服务收尾报告

2026-11-05，第 3 轮修复；开工基线 `1daec81`，工作区干净。保留 [REVIEW.md](REVIEW.md) 原文。本轮修复其中剩余的两项高问题，并补齐撤销核查中实际复现的闹钟/提醒版本和延迟票据成员归属；没有新增严重、高、中遗留项。原报告已修的严重、高、中逐项复测，低级 AUD18-404 保留。常规及 ASan/UBSan 全量各 86/86，32 组服务回归（各 78 次启动）、额外低电量保存对照和栈计量全部通过；四类本轮修复及上轮八组的反向对照在两套构建均成立。代码里程碑已提交 `4c0662f`，最终验收更新单独提交。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R3-01 高 | 已修 | 【实现】阅读器打开时绑定成员代次；定时、持续翻页触发和销毁补存共用保存检查。【实测】正常换人、删除不复用、删除后孩子复用各测销毁/定时两条路径，共 88 检查；普通换人恢复第 3 页，复用恢复第 1 页。旧源码出现文件重写及真实页码回执失败 | V2/V3 `reader_late` |
| R3-02 高 | 已修 | 【实现】仅实际保存成功且版本非零才发 Wi-Fi 删除票据。【实测】真实 8 条容量上限、保存失败、真人腾位后保存、旧撤销、已有网络更新，以及未改网络的正常撤销，共 23 检查；旧源码 4 项失败 | V2/V3 `wifi_undo` |
| R3-02 同类：闹钟/提醒 | 已修 | 【实现】每条定时项有运行期版本，响铃、稍后再响、关闭响铃及重排均使旧票据失效；校验和内存删除共用服务锁，持久记录格式不变。【实测】真人稍后再响、重复提醒已推进、未修改项及表内移动控制，共 21 检查；旧源码 6 项失败 | V2/V3 `undo_schedules` |
| R3-02 同类：票据成员归属 | 已修 | 【实现】生成票据绑定原调用的成员/代次；换人或关空间后拒发；执行逆操作继续携带原代次。【实测】真实总线的延迟测试提供者先写成人集合，孩子集合有相同 id/版本；旧提供者收尾不能取得孩子的删除授权，正常孩子撤销仍可用，共 12 检查；旧源码 3 项失败。该项是边界提供者探针，不冒称业务页面时序实测 | V2/V3 `undo_owner` |
| 成员数据枚举 | 已核对并增加持久回归 | 【实测】五类实际成员集合、原始文件、应用/嵌套私有卡文件、实际媒体断点缓存、其他成人及共享控制，删除/复用 58 检查、重启 22 检查；闹钟、任务和延迟写者另由对应业务回归覆盖 | V2 `member_inventory removal_alarm removal reader_late` |
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
| S1/S2/S3/S6 | 本轮模拟器范围通过 | 【实现】页面保存继续走存储服务；逆操作版本由服务持有，按钮和 AI 走同一能力；原能力开放及凭据屏蔽保留，没有 UI 私改内部状态。【实测】真实能力表、权限、换人和删除回归均通过 | V1/V2/V3 |
| S4 | 通过 | 【实测】常规全量 86/86，32.85 秒；ASan/UBSan 全量 86/86，73.87 秒；两套服务回归、低电量对照、栈计量均通过，预算不变 | V1/V2 |
| S5 | 范围检查通过 | 【实测】16 个相关文件均小于 5 MB；差异隐私匹配 0，空白检查通过；未改预算或受保护文档 | V4 |

成员删除枚举（【实现】检查 `apps/`、`system/` 的全部集合定义、原始存储写入、成员路径、owner/creator 和延迟持有者；以下实测不等同于实机擦除或 RAM 逐字节清零）：

| 数据/持有者 | 清理或失效方式、证据 |
| --- | --- |
| `member.table` 的目标身份、角色、PIN 和尝试/等待状态 | `removing` 持久隔离，数据清完后才删除目标记录并释放编号；PIN 作业核对代次；`removal` 四种故障、多启动重试保留 |
| `activity.log`、`notify.inbox`、`notify.prefs`、`text.user_freq`、`media.positions` 及应用自定义成员集合 | 全部成员目录和集合缓存统一清理，持有引用/写入占用会阻止释放编号；`notify.prefs` 虽未注册也单独枚举；`member_inventory` 写实际集合哨兵并保留另一成人控制，`removal_alarm` 测自定义集合 |
| 阅读位置、应用 `.state` 及其他成员原始 Flash 文件 | 清 Flash、读取缓存和预取；阅读器、应用注册表、媒体延迟保存各验原代次；`reader_late` 覆盖旧页面未关闭的真实链，`member_inventory` 验另一成人原始状态保留 |
| `/content/.m/<成员>/`、应用目录、录音及未完成文件 | 完整递归清理；文件句柄/文件作业忙时不释放编号，旧目录句柄验代次；`member_inventory`、`removal_alarm`、`removal files shutdown` |
| `/cache/`，包括 `sys/media-pos` | 删除可重建缓存，活跃缓存句柄阻止清理；媒体迟到保存另验代次；`member_inventory` 显式写实际媒体断点缓存和普通缓存并跨重启检查 |
| 设备集合 `time.alarms` 中按 owner 的闹钟/提醒 | 清持久记录、内存定时项、响铃并重排 RTC/唤醒；`removal_alarm` 删除、复用、重启，原时间服务全量仍过 |
| 任务、文件作业 | 任务按 owner 和 creator 清理，包含已结束及曾由该成员创建的共享任务；取消活动任务；文件作业和旧句柄拒绝旧代次；`removal_alarm files` |
| 活动待记、通知待写、事件、去重/在途回执、撤销票据 | 绑定成员代次，逻辑失效；本地延迟队列、通知/活动写者、旧票据有 `removal_alarm replay undo_owner`；传输队列换人/关空间断连丢弃有 `handoff` |
| 剪贴板、输入/选区/undo/redo、AI/听写/播放/录音会话、应用页面 | 换人直接撤销采集、清剪贴板，UI/媒体队列收尾；数据库或文件仍占用则删除失败并保留编号；`text bindings voice audio handoff isolation playback` 及全量对应业务 |
| Wi-Fi、蓝牙、设备设置、升级/失败历史、共享书籍及当前无 member owner 的通信暂存 | 现有设计/实现属于设备共享，未擅自改变为个人数据；明确没有把它们谎报成“随成员清除” |

同类入口复核（【实现】）：撤销逐一查设置、数据、通知、Wi-Fi、校时、闹钟、提醒、文件新建和移动的成功条件与版本保护，并检查统一票据生成和执行入口。去重复核本地 `call_with_intent/prepare`、`tools/call` 和远端代理的共用准备/答复入口，保留能力、参数、来源、成员代次及策略检查；`replay` 新加来源碰撞断言，其余断言保持。未扩展到其他功能。

V1：两套均按[总规则 §5](../codex-rules.md#5-构建和测试)完整配置（内存构建仅增加 `INK_SANITIZE=ON`），以下串行执行，均退出 0：

```bash
cmake --build build -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
```

V2：两套均运行，第二套给 `run.py` 和 `stack_meter.py` 加 `--build build-asan`；32 组服务回归、两种低电量分支及栈计量均退出 0；每套 78 次回归启动无功能失败、未完成或 sanitizer 诊断：

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash removal replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel reader_late member_inventory wifi_undo undo_schedules undo_owner
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py
```

V3：两套均运行，内存构建增加 `--build build-asan`；只从 Git 读取旧源码到忽略的构建目录，不回滚工作区；编译/链接成功，模拟器退出 0 并完成测试，因功能断言失败而由脚本退出 1，不能以编译失败充当反例：

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline 1daec81 reader_late wifi_undo undo_schedules undo_owner
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline ed14461 replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel
```

【实测】第一行四组均失败：阅读器正常换人的两组控制仍通过，删除不复用两组各 1 项失败、复用两组各 2 项失败；Wi-Fi 4 项、定时项 6 项、票据归属 3 项失败。第二行八组均出现原预期功能失败；更早已修项保留 REVIEW.md 的历史反向证据，本轮重跑其正向。新增旧时间 API 兼容只转调原增删并忽略新版本参数，不替代业务逻辑；其他兼容沿用上轮。没有删断言、放宽期望或跳过测试。

V4【实测】：`git diff --stat 1daec81..HEAD` 核对为 16 个相关文件；`git diff --check 1daec81..HEAD` 为 0；按总规则原隐私正则扫描差异，匹配 0；改动 C/头文件无 libc `write/read/fgets` 调用，没有用忽略返回值规避 Linux Werror。报告相对文件链接有效，提交范围没有夹带其他文件。证据 `build/services-r3-closure.json`。

证据保存在忽略的 `build/`、`build-asan/`：`services-r3-configure-final.log`、`services-r3-build-final.log`、`services-r3-ctest-final.log`、`services-r3-regressions-final.log`、`services-r3-saved-final.log`、`services-r3-stack-final.log`；逐步退出码/时长见 `build/services-r3-validation.json`。两套反向日志均为 `services-r3-reverse-final.log`、`services-r3-inherited-reverse.log`；内存反向退出码见 `build/services-r3-reverse-asan.json`，每组 metrics 和输出目录在日志末行。

【实测】两套 PQBN AI 场景内部总 RAM 最低空闲均 32,880 字节，原门槛 32,768，仍余 112；内部失败分配均 0。没有抬高预算、增加常驻任务或关闭确定性检查。

## 没做到的

- AUD18-404 是低级能力文档漏项，按用户范围保留。
- 仅 macOS 模拟器；没有 Linux、真机、真实中枢或窗口 UI 验证。全量中的既有 `gui-smoke` 由 CTest 使用 SDL dummy，不启动带窗口模拟器。

## 和设计不一致的

- 保留上轮边界：文件撤销按全卡修改序号保守失效；失败升级历史有 128 KiB 上限，读取/落盘/容量失败保守拒绝；成员清理占用或 I/O 失败仍需重试并保留 removing 编号。
- 撤销票据与本次新增定时项版本都只在本次开机有效，未承诺跨重启撤销；持久定时记录格式未变。没有改设计文档迁就实现。

## 改过的已有测试

- `replay.c` 仅增加“同请求号换来源”断言及测试来源变量；原期望不变。
- `run.py` 新增五组及阅读器六种时序/数据清单多启动矩阵，增加旧阅读器源码替换；`time_boundary.c` 增加旧 API 兼容供反向运行。原组别、断言、预算、确定性比较和等待阈值不变。
- 新增测试显式扣住/释放既有队列以固定时序；没有靠加长 sleep 掩盖竞态。

## 要用户定的

无；没有采购、硬件、付费、远端写入或发布动作。

## 顺手发现

- 两项撤销同类遗漏已随本轮修复并做正反对照，见验收表。其余范围不扩展。
- 原阅读器探针把 `sim_metric_value` 用在分布指标上会读到样本数；新回归直接用真实 `reader.read_page` 回执验证恢复页码，文件重写断言也保留。没有更改模拟器指标含义。
