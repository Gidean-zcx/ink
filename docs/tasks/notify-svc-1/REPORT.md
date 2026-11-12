# notify-svc-1 执行报告

基线：`36473cc4933e6fea2f2a78a4fbec16186d3be3f4`。本地分支 `claude/codex-notify-svc-1`；未推送、未操作设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 勿扰、例外、跨午夜 | 通过【实测】 | 工作日/周末、23:30、06:59、07:00、手动优先、联系人/陌生/重复、闹钟、忙时和全天提醒；撤回跨午夜处理后两时刻均失败 | C1；反向对照见下文 |
| 2 家人隔离和重启 | 通过【实测】 | A/B 的时段、手动开关和自定义回复互不覆盖，重启后保留 | C1，seed/main/restart |
| 3 动作、回复、策略 | 指定用例通过【实测】 | 触摸来源的回复直达 `sms.send`，号码和正文准确；AI 无 asked 标记弹短信确认卡；未接来电动作调 `call.dial`；3 个动作、自由回复和真实回执可读。可信 asked 继承未做 | C1、C2 |
| 4 老数据 | 通过【实测】 | schema 1 原始 JSON 记录的正文、分组、标记和旧动作保留；旧全局 `notify.dnd=1` 迁给当前成员，其他成员不继承 | C1 |
| 5 改前改后 | 完成 | 下表；基线为代码核查，本次为模拟器实测。没有页面改动 | 基线源码、C1 |
| 6 全量、内存、S31、体积 | 全量未通过【实测】 | 最终一轮 438 项有 2 项失败、3 项跳过；ASan/UBSan 9/9、S31 宿主机 77/77；S31 编译通过、增量 10,528 字节、无编译警告。315 行产品代码新增 | C2–C6 |

## 没做到的

- 【实现】可信 `_meta.asked` 只存在能力层内部，公共嵌套调用 API 不提供继承入口。带该标记的 hub AI 请求经过本服务后仍会再确认；保留原始 `src`，没有伪造 `CONFIRMED` 或修改范围外的策略层。
- 收藏枚举保留为 3，目前按“无人”处理，重复来电例外仍独立生效；等待 contacts-3 的收藏接口，符合任务的暂留要求。
- 普通全量未全绿：设计目录校验缺 5 个新能力；备份页触控用例失败（复测结果见下文）。3 项 hub 测试因本机缺少其专用环境跳过，未计作通过。
- 本机未运行 Linux，也未做真机验证；编译风险用指定 gcccheck 和 S31 GCC 实编译核查。

## 和设计不一致的

【实现】原文三个文件和许可证逐字节入库，哈希及接口适配见 [SOURCE.md](../../../system/services/notify/vendor/pebble/SOURCE.md)。实际编译复用跨午夜的三个完整函数；偏好文件、cron、事件和手机传输接 ink 的现有接口，Pebble UI 不链接。手动关闭只关闭手动来源，不搬 Pebble 的“临时覆盖计划”行为，遵守本任务“任一来源生效”的规则。

日历忙时已接 `ink_alarm_next` / `ink_alarm_occurrence`。定时复用 `time.minute`、`time.changed` 和 `alarm.revision`，时间服务改动 **0 行**。

## 改过的已有测试

仅在 `tests/integration/ai_surface.c` 的 FOR_AI 表末尾追加 5 个能力。没有修改已有断言、期望、预算、超时或跳过规则。新增 `tests/notify_model.c` / `.py` 覆盖服务、重启及事件不重复发送。

## 要用户定的

可信 asked 继承需主会话在能力层提供受信转发接口；当前采用继续确认的保守行为。未停工等待。

## 顺手发现

【实测】`small-gaps-3-backup` 在最终全量和一次定向复测均失败，指定基线同场景通过。失败为 `tests/small_gaps_3/main.c:91` 的 `touch turns automatic backup off`。两边都记录到 3 次触摸；当前仅 2 次 `action.user.settings.set`，基线为 3 次。触摸到能力调用之间的丢失原因尚未定位；备份页和该测试不在本任务允许修改的范围，未改、未加等待或放宽断言，交主会话处理，不能把它称为基线问题。

宿主机构建有重复静态库链接提示；ASan 构建的原有第三方 cJSON 有 `sprintf` 弃用提示。未改依赖或链接配置。

## 新增的机制

- 勿扰 5 个时段、忙时、例外及当前状态：任务 1–3 要求，沿用时间服务，没有新定时器或常驻线程。
- 最近 8 次来电的成员、号码和单调时间：任务 3 的 15 分钟重复来电规则；在 PSRAM，重启清空，号码按原字符串匹配。
- `notify.prefs` 记录 2 的 JSON 字段：任务 1/2/4 的成员偏好；记录 1 的渠道开关保留，没有另建通知库。
- `notify.inbox` schema 2：任务 4 的动作和回复目标，schema 1 记录仍读。
- 偏好首次加载、联系人过滤和动作派发的短作业：任务 1/3/4，复用已有 BG/UI 执行器。UI 派发点把 BG 目标排到自己的执行器，避免两层能力调用叠加后台栈。

## 要改的文档

`docs/system/system-services-design.md` §1.3 需要列入 `notify.act`、`notify.reply`、`notify.dnd_get`、`notify.dnd_set`、`notify.replies_set`。该文件不在允许修改的范围，交主会话统一补齐；`services-mid-capability-doc` 会因缺这 5 项失败。没有删除或放宽校验。任务明确允许的 `docs/system/capabilities.md` 已重新生成。

通知页和控制中心需使用下面的有效状态，手动设置 `notify.dnd` 仍只表示手动来源。

## 接口清单（notify-ui-1 / control-1）

| 接口 | 字段和约定 |
| --- | --- |
| `notify.dnd` 设置 | 布尔，每成员保存；旧全局值归首次升级启动时的当前成员，其他成员默认关 |
| `notify.dnd_active` / `notify.dnd_reason` / `notify.dnd_until` 状态 | 有效勿扰、`off/manual/schedule/calendar`、UTC 结束秒；手动结束为 0 |
| `ink_notify_dnd_state()` | 同上，并给 `slot`：时段 0–4，其他来源 -1；只读当前成员 |
| `notify.dnd_changed` 事件 | 状态或偏好变化；时间推进但状态相同不会重复发事件 |
| `notify.dnd_get` | `active/manual/reason/slot/until_s`，以及 `calls/repeat/calendar/slots/replies`；后两开关返回 0/1 |
| `notify.dnd_set` | 改时段同时给 `slot/days/start/end`；例外可单独给 `calls/repeat/calendar`（写入开关用布尔） |
| 时段 | `slots` 对象键 `0`–`4`；周日 bit 0，工作日 62，周末 65；起止为当天分钟数，结束不包含；days=0 禁用，起止相同不生效 |
| 来电例外 | 0 无人、1 所有人、2 联系人、3 收藏（预留）；`repeat` 独立放行 15 分钟内同号再次来电 |
| `notify.replies_set(index,text)` | 序号 1–5，最多 31 字符，存自己的偏好；get 的 `replies` 对象键为 `0`–`4` |
| 默认回复 | 好的 / 是 / 不是 / 给我回个电话 / 我晚点给你回电话 |
| `notify.list` / `ink_notify_next` | JSON 增加 `actions[{label,cap,args}]`、`reply{cap,args}`；C 视图为 `nactions/actions[]/reply_cap/reply_args`；旧 `action/action_args` 保留 |
| `notify.act(id,index)` | 动作序号 1–3，转发真实结果和回执 |
| `notify.reply(id,index)` 或 `(id,text)` | 快捷回复序号 1–5，或最多 63 字符正文；回复目标 args 为静态参数模板，转发时补 `text`；保留来源，目标能力裁决权限和确认 |
| 短信来源 | `sms.thread(peer)` 动作；`sms.send(to)` 回复目标 |
| 未接来电来源 | `call.dial(number)`、`sms.compose(number)`；删除通知只删通知箱 |

## 改前改后

同一组有效勿扰下比较：基线手动打开；本次周三 23:30 按时段生效，允许联系人及重复来电。

| 输入 | 基线【实现】 | 本次【实测】 | 通知箱 |
| --- | --- | --- | --- |
| 联系人来电 | 不响，来电卡出现 | 响，来电卡出现 | 未接后都保留 |
| 陌生号码第一次 | 不响，来电卡出现 | 不响，来电卡出现 | 未接后都保留 |
| 同一陌生号码 10 分钟后 | 不响 | 响 | 未接后都保留 |
| 闹钟 | 响 | 响 | 错过时保留 |
| 普通短信 | 无声音 | 无声音，带打开会话和回复目标 | 都保留；删通知不删短信 |
| 高优先级通知 | 不响 | 不响 | 都保留 |

【实测】新用例 3 个允许响铃样本，从来电事件到发出响铃请求：min **1.329 ms**、p50 **1.399 ms**、max **4.110 ms**。这是模拟器服务延迟，未测实际扬声器起声。普通未开启勿扰的来电直接响铃，联系人查询始终在 BG。

## 反向对照

【实测】临时将 `vendor/pebble/schedule.inc` 的 `if (from < to)` 改为 `if (from != to)`，让跨午夜错误地走普通区间；重编 `sim-notify-model`，用 `INK_NOTIFY_MODEL_PHASE=cross` 运行。退出 **1**、**8 条断言失败**，明确含 `23:30 matches the overnight period` 和 `06:59 matches the previous day's overnight period`。

【实测】临时撤回 `changed()` 的无变化事件筛选，重编 `sim-configured` 后运行 AI 回合结束场景，ctest 退出 **8**，`hub.idle_disconnects: metric not recorded`。指定基线同场景通过；修复版本同场景通过。

两份源码逐字节还原，提取文件 SHA-256 再次核对一致；反向模式未注册到 ctest，不依赖 CI 的 Git 历史。恢复后的独立 cross 模式退出 0、18 条断言通过、无告警；通知模型与通知服务末次定向复测也通过。

## 构建与检查

命令均在仓库根目录，以任务书给定的依赖变量配置。构建最多 `-j2`，测试最多 `-j2`，前缀 `taskpolicy -c utility`。

```sh
# C1：服务验收（含三次启动）
taskpolicy -c utility env INK_KEEP_EVIDENCE=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^notify-model$'
# C2：普通全量
taskpolicy -c utility env INK_KEEP_EVIDENCE=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# C3：ASan + UBSan，配置同普通构建，另加 INK_SANITIZE=ON
taskpolicy -c utility env INK_KEEP_EVIDENCE=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2 -R '^(notify-model|notify-manage|integration-notify|integration-notify-lowbat|integration-races|phone-calls|sms|sms-multipart|emergency-call)$'
# C4：S31 宿主机独立工程
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2
# C5：仅编译固件；本次用同一构建目录先编基线归档、再编当前源码
INK_S31_BUILD="$PWD/build-s31-baseline" taskpolicy -c utility bash ports/esp32s31/build.sh
# C6：指定 GCC 检查工具
taskpolicy -c utility python3 ../../tools/gcccheck.py build system/services/notify/notify_service.c system/services/notify/notify_caps.c system/services/notify/notify_model.c tests/notify_model.c tests/integration/ai_surface.c
```

【实测】C1：seed **10**、main **136**、restart **22** 条断言通过。ASan/UBSan：`100% tests passed out of 9`。S31 宿主机：`100% tests passed out of 77`。gcccheck：`5 compiled, 0 with findings`。

普通最终全量原话：`99% tests passed, 2 tests failed out of 438`，`Total Test time (real) = 1245.94 sec`。失败：`services-mid-capability-doc`、`small-gaps-3-backup`；跳过：`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`。

【实测】全量后追加动作行表的编译期上限校验，重新编译，`notify-model` 与 `integration-notify` 末次定向复测通过。`small-gaps-3-backup` 同次复测仍失败；指定基线单独构建后同场景通过。基线/当前对照用名称匹配测试，不依赖测试编号。

【实测】末次 S31 `build.sh`（以 bash 执行）退出 **0**，编译日志 **0 条 warning/error**。同工具链和默认配置的 `.bin`：基线 **4,268,848** 字节，本次 **4,279,376** 字节，增量 **10,528 字节（10.28125 KiB）**，低于 20 KB；原文未全部链接进入固件。末次 ASan/UBSan 仍为 **9/9**，耗时 **118.21 秒**；gcccheck 仍为 **5 compiled, 0 with findings**。

【实测】最终差分通过 `git diff --check`；任务书规定的路径/硬件标识隐私正则 **0 匹配**；来源三个原文件和许可证再次逐字节核对一致。提交只含任务范围文件，不含构建产物和设计稿。

产品 C/头文件/CMake 新增 **315** 行、删除 **9** 行、净增 **306** 行；原文、测试、文档和翻译不计。小于 400 行预算。超过默认 100 行的原因：本任务同时要求成员偏好、定时/忙时、来电例外、v1/v2 兼容和动作转发。看过的更小做法中，单布尔开关无法覆盖时段和例外，在模组任务查联系人会阻塞，直接嵌套转发超出后台栈预算；现实现复用已有存储、时间、执行器与原文函数。

## 架构审视与上机核对

【实现】通知服务管理自己的通知箱和偏好，来源事件负责构造记录；动作经过同一能力总线，按钮与 AI 共用。联系人使用任务指定的公开匹配函数，未读其集合格式；这是目前对同库应用接口的依赖，未擅自新建联系人服务。新增能力及状态对 AI 开放，没有页面改动。

本任务没有烧录、接串口或发送真实短信电话。主会话上机时需核对跨午夜结束、联系人/陌生/重复来电卡和铃声、成员切换和重启保留、短信回复确认以及旧通知读取。
