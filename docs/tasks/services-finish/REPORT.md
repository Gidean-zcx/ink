# 系统服务收尾报告

状态：本轮收尾完成。A/B/C2 的 11 高、6 中已修，C 路补查另修 2 高，另最小修复 1 个模拟器验收阻挡项；4 中、1 低按任务要求列后续。指定基线 `fdff779`，开工提交 `70d3a43`，初始工作区干净；保留基线之后已有的审计材料和成员隔离修复，以及用户更新任务和资源限制的提交。只在本地提交，未推送，未接触设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| S1 | 通过；11 高已修，6 中已修；4 中、1 低列后续 | 下表；常规与内存检查新增回归通过，换回 `70d3a43` 源码时相应检查失败 | `python tests/services_finish/run.py files storage update text bindings lowbattery audio voice shutdown`；加 `--baseline 70d3a43` 做反向对照 |
| S2 | 通过；活动记录的持久化边界见下文 | 【实测】常规和 ASan/UBSan `policy` 均为 30 检查通过：请求标记、外来文字、设置撤销、高风险人工确认、密码限制和成员隔离正反例；Wi-Fi 凭据删除手动也确认 | `python tests/services_finish/run.py policy`；再加 `--build build-asan` |
| S3 | 通过 | 【实测】两种构建 `actions` 均为 6 检查通过；真实表大于 768 字节，在 listen/ask 中逐字完整发送；旧源码两项失败；原预算通过 | `python tests/services_finish/run.py actions`；再加 `--build build-asan` |
| S4 | 通过 | 【实测】常规 CTest 86/86，ASan/UBSan CTest 86/86；新增回归各 13 组通过；预算和确定性检查保留原值 | `cmake --build build -j4`；`ctest --test-dir build --output-on-failure -j3`；内存检查换 `build-asan` |
| S6 | 通过补查；另修 2 高，已知边界逐项保留 | 【实测】handoff 15 检查、isolation 15 检查，两种构建均通过；既有成员、通知、确认和应用回归均过；旧源码对照失败 | `python tests/services_finish/run.py handoff isolation`；再加 `--build build-asan`；全量 CTest |
| S5 | 通过，范围例外单列如下 | 【实测】本轮新增隐私匹配 0；159 个相对链接有效，59 个本轮文件无超大文件；除 GUI 阻挡项外均在允许目录；本地提交并核对工作区 | 用户指定的差异隐私检查、`git diff --check`、链接/范围检查、`git status --short` |

所有运行都在模拟器上。【实现】表示代码核查，【实测】表示本次执行；不据此宣称真机验证或 Linux 实测。

| 问题 | 等级 | 结论、实现与证据 | 回归 / 反向对照 |
| --- | --- | --- | --- |
| AUD18-101 | 高 | 【实测】目录打开失败曾返回成功、个人文件仍在；现在返回错误，重试后才清空 | `files`：`unreadable root is an error`；旧源码失败 |
| AUD18-102 | 高 | 【实测】打开目录或原子写入时原来还能借出卡；目录、文件和短操作现统一持卡，检查借出和加计数在同一锁内 | `files`：目录与原子写入中借卡均应 busy；旧源码失败 |
| AUD18-103 | 中 | 【实测】让提交返回晚于作业完成，原任务永久运行；现先登记任务再发布，提交失败配对结束；结束回调读完上下文才释放槽位 | `files`：`task also finished`；旧源码失败 |
| AUD18-104 | 高 | 【实测】原来拒绝写状态后仍切槽；现在等借用写入的持久完成，`trying` 写好后才切槽；试运行缺记录不能自检成功 | `update`：两类状态写失败、正常资源恢复、缺记录重启；旧源码对应链失败 |
| AUD18-105 | 高 | 【实测】资源失败后原来可用 app-only 包覆盖事务；现在先拒绝安装，保留原资源恢复入口 | `update/resources-preserved`；旧源码失败 |
| AUD18-106 | 中 | 【实现】未改：仍只记最近一个失败版本；需要失败历史、迁移和人工重试规则 | 列入“没做到的”，未宣称修复 |
| AUD18-107 | 中 | 【实测】已签名的重叠、重复、未知、空段原来通过；现在校验合法类型、唯一性、头后偏移、非空、文件边界及互不重叠 | `update/tables` 的正常包及 8 类坏包；旧源码 4 类被误收 |
| AUD18-108 | 中 | 【实测】原来换 offer 后混用长度和包标识；现在固定下载身份，续传核对旁文件身份，结束核对版本、长度和哈希；旧包不可取时失败，保留部分文件 | `update/offer-changed`、`update/offer-kept`；旧源码两条链失败 |
| AUD18-109 | 高 | 【实测】删成员集合截断共享定时链表；现清零前取消 flush、tell、compact 三个节点，排除在途工作 | `storage`：无关定时器和延迟落盘；旧源码定时器检查失败 |
| AUD18-201 | 高 | 【实测】媒体执行器忙时录到约 3.24 秒，UI 忙时听写继续上传；现在取得高优先级焦点即截断采集，音频任务独立收尾 | `audio`、`voice`；旧源码录音和上传边界失败 |
| AUD18-202 | 高 | 【实测】分配失败后旧 redo 覆盖新文字；现新编辑总使 redo 失效，快照失败同时清掉不可靠的 undo | `text`：OOM 后仍为 ABD、不能重做；旧源码失败 |
| AUD18-203 | 高 | 【实测】原来录音未结束、记录未落盘就断电；现先结束采集、录音及播放，再等数据完成；普通关机未存好则失败，低电量超时明确记录未保存 | `shutdown`：断电钩子检查录音关闭和 durable == last；旧源码两项失败 |
| AUD18-204 | 高 | 【实测】原来耳机断开后外放约 990 ms；现音频任务在切路由前关闭输出，路由发布与旧媒体帧作废一起完成，媒体队列随后保存暂停位置 | `audio`：媒体队列忙时外放增量为 0；旧源码失败 |
| AUD18-205 | 中 | 【实测】CALL 曾删除闹钟焦点，结束后可在响铃下录音；现 ALERT 在 CALL 下暂停保留，通话后恢复 | `audio`：闹钟持有 ALERT 并拒绝录音；旧源码失败 |
| AUD18-206 | 中 | 【实现】未改：关机图仍只等绘图和 busy 清空，没有具体帧成功回执 | 需要显示服务与界面联合修改，列后续 |
| AUD18-207 | 中 | 【实现】未改：选区标记与窗口的映射仍在 `system/ui/textfield.c` | 文件不在任务允许范围，需要界面任务接手 |
| AUD18-208 | 中 | 【实测】隐藏框原来能被 AI submit；现同样拒绝，隐藏框的语音 finish/cancel 也检查来源 | `text`：隐藏框提交不触发回调 |
| AUD18-209 | 中 | 【实测】初始“中文”截为 4 字节曾留下残缺字；现退回完整 UTF-8 边界 | `text`：结果为“中”；旧源码失败 |
| AUD18-401 | 高 | 【实测】旧请求会写进新焦点或覆盖后续人工编辑；现插入、提交、选区、撤销/重做、听写控制统一校验 `text.state` 的 session/revision，变化推进版本 | `bindings`：旧字段、旧内容、旧光标返回 stale，当前版本成功；旧源码失败 |
| AUD18-402 | 中 | 【实现】未改：普通/数字键盘还缺与页面组合的语义快照、候选页、听写提示和提交含义 | 需要 UI 快照组合接口，列后续 |
| AUD18-403 | 高 | 【实测】沿用 203 的修复；5 秒保存超时明确传 battery_unsaved，图与快照不能再说全存好；正常保存仍显示成功 | `lowbattery`：超时与正常两支；旧电源源码反向失败 |
| AUD18-404 | 低 | 【实现】总表仍缺 job.*、文字编辑等；清单测试校验源码表与运行时，没有解析文档 | 只在允许的 §15 说明边界，不改范围外 §1.3 |
| S6-01 | 高 | 【实测】旧事件进入设备发送缓冲后可在换人后抵达；同一成员关闭空间后迟到的事件也曾发出。现在重查成员与空间，有未确认送达事件时取消连接和队列；同连接 RPC 回答证明前序事件已到达后解除标记，保留正常 AI 切换回执 | `handoff`；旧 hub 源码失败；空间关闭修前另有 2 项失败（`build/services-finish-closed-before.log`）；`integration-media` 和 `scenario-ai_tools` 原断言保留 |
| S6-02 | 高 | 【实测】UI 阻塞时换人，听写和 AI 麦克风都继续采集/上传；现直接作废采集及旧帧，不依赖 UI 收尾；迟到听写结果不再提交 | `isolation`：修前两项失败，修后通过；换回旧音频/AI 源码时三个边界检查失败 |
| S4-GUI | 中（阻挡项） | 【实测】加严 UBSan 时 GUI 启动将无效鼠标位置的 -inf 转 int，直接终止。现无效坐标保留最后触点，有效坐标先限幅再转换 | 原 `gui-smoke` 保留输入和断言，`UBSAN_OPTIONS=halt_on_error=1`；旧可执行文件反向失败；修前全量日志 `build-asan/services-finish-gui-before.log` |

原 C 路的六项补查：

| 范围 | 结论与证据 |
| --- | --- |
| 模块、四类总线对象 | 【实现】`data→storage`、媒体/文件任务、时间→RTC/power 的职责合理；`pinyin` 固定系统资源与升级包经 HAL 读取并持卡，是下层机制，不应转成分页文本 `file.read`。内部锁、焦点、Flash 守卫不需单独变成 AI 工具。AI 面板仍是 layering 明示的旧例外，不能据静态扫描断言全系统无 UI 耦合。 |
| AI 原生、快照、回执 | 【实现】动作走能力关口、查询走结构化结果；活动记录与对象撤销已有实测。确认/打断/成员门/安装卡/关机图提供快照并遮 PIN；普通键盘不完整（402），关机成功回执不完整（206）。不是所有动作都有逆操作，事件也不保证必达。 |
| 能力开放 | 【实测】`integration-ai-surface` 检查实际 tools/list 与源码清单。六项 OWNER 例外保留：成员解锁/设 PIN、确认接受/拒绝、两种安装升级。前两类必须由真人执行，升级仍由人启动；AI 可查状态。文档总表缺项见 404。 |
| 成员隔离 | 【实现】`cap` 在接受/执行/回结果绑定成员；`notify` 未跟上时不提供旧箱及渠道修改；`time`/通知/文件/录音/确认事件携成员；`members` 表读错误维持成人关闭，重读成功重启；`app_registry` 状态路径绑定 AP.member，成员门快照无 PIN。额外发现并修复 S6-01、02；C 任务列的应用/成员门文件只读，未扩大 UI 修改。 |
| 文档真实性 | 【实现】§9～15、ui-dependencies §4.1/7 的整体接口可对上；失败历史、显示完成、文本选区位置、普通键盘快照及文档清单仍有上述缺口。真实中枢/硬件、同步、远端 needs、原始来源链与应用调用者身份仍不完整，不作为已实现保证。 |
| 原修复复核 | 【实测】AUD17-003、AUD16-001：`integration-people-reset` e 链；AUD17-004、AUD16-007：`integration-races`/媒体成员事件；AUD16-008：`integration-listening`，并以 S6-02 补 UI 拥堵分支。第十四轮 AUD15-001～011 分别由 data、files/people、ai-surface、races、time-reboot、apps、notify 对应回归复核，常规与内存检查均通过。未做密码尝试次数/等待时间专门实验。 |

首次实现回归：`build/services-finish-ilqcq2j4/` 的 8 组通过，听写另见 `build/services-finish-5jlddnzj/`。反向证据：文件/存储 `build/services-finish-7bsxidu_/`，声音/文字/关机 `build/services-finish-zqxlzyuc/`，耳机 `build/services-finish-vafd3fwe/`，策略/能力表/升级 `build/services-finish-8g1fgxqx/`。这些是忽略目录中的本地运行产物，不提交；可用上述命令重建。

重启后续做：`build/services-finish-xsbhufn9/` 的 bindings、lowbattery、voice、handoff、isolation 全通过；`build/services-finish-h3ncwufu/` 保留 S6-02 修前两个失败。第一次最终常规 CTest 为 85/86，发现连接取消误伤 AI 切换回执；修正因果送达标记后，相关 6 项原测试全部通过（`build/services-finish-handoff-focused.log`），不把这次失败算作最终验收。后续日志放忽略的 build 目录；构建最多 -j4、CTest 最多 -j3，各轮串行，仅运行 dummy 视频驱动的无窗口 GUI smoke。

完整新增回归 13 组通过：`build/services-finish-8dwk65a8/`（含升级 12 次链式启动）；完整反向对照 `build/services-finish-v5bldhg0/`，13 组均检出对应旧行为，脚本按预期返回 1（日志 `build/services-finish-reverse.log`）。对照只读取旧源码另行编译，未回滚工作区、未污染产品构建。最终 handoff 还补了同一成员空间关闭的 6 项功能检查，只设置一次合成 PIN 后锁定，不测试猜测次数或等待策略。S2 核对补齐 Wi-Fi 凭据删除的 ask_by_hand，修前 policy 的 3 个检查失败（`build/services-finish-wifi-before.log`），修后通过。

最终证据（均为重启后串行执行）：

| 检查 | 结果 | 本地忽略目录中的日志/产物 |
| --- | --- | --- |
| 常规 Werror 构建、全量 CTest | 退出 0；86/86，26.70 秒 | `build/services-finish-build.log`、`build/services-finish-ctest.log` |
| ASan/UBSan Werror 构建、全量 CTest（UBSan 遇错终止） | 退出 0；86/86，69.16 秒 | `build-asan/services-finish-build.log`、`build-asan/services-finish-ctest.log`；固化遇错终止后两种构建单跑 GUI 均通过（各自 `services-finish-gui-final.log`） |
| 新增 13 组，常规 / 内存检查 | 全通过；升级组含 12 次链式启动 | `build/services-finish-bzckzzmt/`、`build-asan/services-finish-0h4r8hom/`；各目录外的 `services-finish-regressions-final.log` / `services-finish-regressions.log` |
| 低电量正常保存对照，常规 / 内存检查 | 各 5 检查通过，仍显示已存好 | `build/services-finish-f2n8esda/`；`build-asan/services-finish-qgc7tgmu/` |
| 反向对照 | 旧源码 13 组均检出对应问题，退出 1；最终补的迟到事件拒绝检查也失败 | `build/services-finish-reverse-final.log`、`build/services-finish-handoff-reverse-final.log`；GUI 反向见 `build-asan/services-finish-gui-reverse.log` |
| 内部 RAM | PQBN 最低余量 33,192 字节，原门槛 32,768，余 424；内部堆失败分配 0 | `build/test-out/ai_ask-metalio-plus-pqbn/metrics.json`；未改任何 budget 文件 |
| 隐私、链接、范围 | 新增匹配 0，159 链接无失效；59 文件无超大文件，GUI 阻挡项例外单列 | `build/services-finish-closure.json`；差异与文件检查 |

重现命令（Python/CTest 使用 `$INK_DEPS/build-deps/pyenv/bin/` 中的版本；配置参数同总规则，内存检查仅增加 `INK_SANITIZE=ON`；另指定已有 SDL2、ImGui 只读缓存）：

```bash
cmake --build build -j4
$INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --build build-asan files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline 70d3a43 files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions
INK_FINISH_SAVED=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --build build-asan lowbattery
```

结束检查：指定基线到 HEAD 的隐私正则匹配 4,134 行，与开工前 `fdff779..70d3a43` 的 4,134 行相同，均为继承材料；本轮 `70d3a43..HEAD` 匹配 0。没有改写这些原始审计文件。`git diff --check` 通过；59 个本轮文件中，58 个在任务允许目录，1 个 GUI 阻挡项依总规则例外处理。系统 UI 只改关机文案一行，设计只改 §7、§15。总差异中三份任务书的变化来自用户的 `a70d2c8`、`8211275`，未覆盖。实现里程碑为 `928cd7b`、`e7fbd3d`，最终小修和验收随本报告本地提交。

## 没做到的

- AUD18-106：失败版本历史的持久格式、容量、升级清理和人工重试规则未做。
- AUD18-206：显示完成回执未做；不把 busy 归零当成已显示的可靠证明。
- AUD18-207：文本控件位置映射未做，属于允许范围之外的界面文件。
- AUD18-402：非模态输入面板与页面组合的语义快照未做；AUD18-404 低级文档总表漏项只列后续。
- 没有 Linux 运行环境；不推送、不使用付费接口、没有实机或中枢服务器代码的验证。
- 协议中的确认结果落盘、`confirm.status`、三秒延后发送、配对和同步仍不是本任务交付；不伪装为完成。

## 和设计不一致的

- AUD18-206、207、402 仍与已有设计保证不一致，原因见上。只更新允许的设计 §7、§15，不改其他设计迁就代码。
- 为修 AUD18-203，只对 `system/ui/shell/off_picture.c` 小改低电量未存好时的文案，不改界面外观。
- 范围例外：`simulator/desktop/gui/gui_main.cpp` 原本不在目录清单；加严 UBSan 的既有无窗口 smoke 因其整数转换错误终止，阻挡 S4。按总规则 §2“别的问题记进报告的‘顺手发现’，除非挡路，否则不修”处理，只补鼠标坐标有效性与转换边界检查，测试没有避开原输入或停用 sanitizer。
- 活动记录写入设备成员集合，复用既有数据重试与后台执行器；中枢保留和控制台展示仍需服务器实现。
- 活动记录异步保存；待记槽 16、集合 128 KiB，查询按成人 365 天/儿童 180 天过滤，尚未物理回收过期记录。意外断电前未写好的条目不保证保留，关机这类不返回的动作也没有最终回执记录；不宣称全局持久审计。

## 改过的已有测试

未删除测试、放宽预算、跳过检查或以 sleep 掩盖错误。D-105 改变的旧期望：

- `confirm.c`：无风险系统设置由 pending 改为 done，同时检查无卡片和可撤销；用于检查密码遮盖的人工卡片夹具显式标 `ask_by_hand`。
- `systest.c`：同类设置期望 done、值改变并有撤销；启动补数据与共享计时服务；完整 manifest 缓冲改为 PSRAM，以容纳新增数据能力，Schema 断言保留。
- `files.c`：不可恢复删除即使手动也先确认，再检查实际删除；USB 全卡分享先人工确认，再保留原成功/失败和借卡计数断言；初始化确认服务。
- `notify.c`：批量清空即使手动也先确认，确认前保留原条目，确认后才为空。
- `people.c`：Wi-Fi 明文密码不能由 AI 提供，已存网络可以直接重连；忘记凭据手动也先确认，再保留原实际删除成功、二次删除失败断言；新增家人经人工 PIN 确认后再检查成功、重复和非法编号。
- `timesvc.c`：非高风险校时直接执行并给撤销，撤销恢复现场，再继续原闹钟测试。
- `ai_surface.c`：将 `activity.list`、文本控制的 session/revision 纳入能力清单与 Schema 核查。
- `lifecycle.c`：启动补上活动记录需要的数据与共享计时服务，不改行为断言。
- `whole_product.h`：普通 AI/远端文字测试先取 `text.state`，携当前 session/revision；显式给旧目标的回归保持原参数，密码/隐藏框仍按原断言拒绝。
- `gui_smoke.cmake`：保留原输入、截图和触摸/翻页断言，将 UBSan 诊断改为终止进程，防止默认恢复模式把未定义行为算作通过；仍用 SDL dummy，不开窗口。

## 要用户定的

- D-95 计时保持待定，本任务不改。
- AUD18-106 的失败历史与人工重试规则，以及 AUD18-207 的界面文件扩展范围，留后续安排；本轮不扩大范围。

## 顺手发现

- 基线之后继承的原始审计材料含本机路径；已经分开检查指定基线完整差异与本轮新增差异，结果见结束检查，不改历史审计文件。
- 全量媒体测试揭示设备发送缓冲还需成员收尾，已作为 S6-01 修复；S6-02 另补 UI 忙时的语音边界。没有改中枢代码或放宽隔离断言。
- GUI smoke 的旧整数转换错误会被 UBSan 默认恢复模式掩盖，现已修并将该测试固化为遇错终止；按阻挡项例外处理，未改原输入和断言。
- 构建复用 `$INK_DEPS` 已有 LVGL、SDL2、ImGui 缓存；没有升级依赖。构建中间出现的编译错误已修正，不计作通过证据。
