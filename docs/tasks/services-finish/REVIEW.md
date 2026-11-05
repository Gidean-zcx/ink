# 系统服务收尾复查（第 2 轮）

结论：**不通过，1 严重、2 高、1 中。** 第 1 轮六项修复的指定回归均通过，换回修前代码能检出旧问题；但成员删除仍遗漏设备级私人数据，去重缓存缺成员边界，记录/文件撤销会删除后来的人工作业。S2、S6 仍不能验收。

复查日期：2026-11-05。对象 `7f24726`；指定基线 `fdff779`；第 1 轮后的差异 `d74a261..7f24726`。原收尾实现的起点为 `70d3a43`，只在核对原 A/B/C2 修复及继承资料时另用这一提交。本报告覆盖旧内容，证据均为本次独立命令或源码核查。

只修改本报告，不改产品源码、既有测试、设计文档，不提交。构建最多 `-j4`，CTest 最多 `-j3`，各轮串行；未启动带窗口的模拟器。编译探针与运行产物仅留在忽略的构建目录。没有调用子代理、接触设备、真实密钥、付费接口或外部发布。

## 验收核对

| 编号 | 结论 | 独立证据与命令 |
| --- | --- | --- |
| S1 | 原问题的修复场景通过，整体未收尾 | 原 11 高、6 中的对应回归通过，旧代码反向能检出；4 中、1 低仍如实列未做。第 1 轮六组修复正反例也成立。删除边界仍有 R2-01/04，详见逐项表及 V2/V3/V5。 |
| S2 | 不通过 | 实际权限设置、儿童规则、普通设置、`_meta.asked`、密码与确认限制、活动满额及设置撤销通过；非设置类撤销仍绕过新内容的保护（R2-03）。V2 `policy permissions activity undo`；V5 `undo_data`。 |
| S3 | 通过 | `actions` 在实际能力表长度大于 768 的前提下逐字检查 listen/ask 的完整列表，两套构建各 6 检查通过；旧代码两项失败。【实现】缓冲按能力表长度分配到 PSRAM，不以空数组冒充成功。V2/V3，V1 预算。 |
| S4 | 通过现有测试和内存检查 | 两套构建退出 0；常规 86/86（25.75 秒），ASan/UBSan 86/86（70.63 秒）；两套各 19 组回归与栈计量检查通过。预算未改。四项功能遗漏不能由测试全绿排除。V1/V2/V4。 |
| S6 | 不通过 | 采集、缓冲播放、事件队列与失败清理标记的原场景通过；正常删除后的闹钟/提醒串人，以及缓存回执绕过成员检查仍可复现（R2-01/02）。V2 `handoff isolation playback removal`；V5。 |
| S5 | 检查已执行，尚不具备完成条件 | 范围、已有测试改动、隐私正则、文件大小、链接和差异已核对；新增阻挡问题未修。复查按要求不提交。见“范围与规则检查”。 |

## 已确认问题

### R2-01（严重）：删除成员没清设备级闹钟和提醒，复用编号的孩子继承成人内容

- 位置：`system/policy/members.c:562`、`:572`、`:581`；`system/services/time/time_service.c:44`、`:554`。测试缺口：`tests/services_finish/removal.c:87`、`:93`、`:95`。
- 【实测】正常新增成人 `oldadult`，通过真实能力创建闹钟 `SYNTH_ALARM`、提醒 `SYNTH_REMINDER`，并写入一个合成个人文件。另一成人正常确认删除，回执为 `done`；随后同编号新增孩子也为 `done`。孩子通过 `alarm.list`、`reminder.list` 直接读到原成人的标题和时间；持久化并重启后仍可读。没有注入 I/O 错误、内存不足或线程阻塞。不同编号孩子的列表对照不含成人标题。
- 复现命令：V5，设置 `INK_REVIEW_CASES=removal_alarm`；加 `INK_REVIEW_BUILD=build-asan` 可复现内存检查版本。两种构建均为首次启动 2 个失败、重启后 2 个失败。最终常规证据 `build/services-review2-x6od6xu0/removal_alarm-prepare/metrics.json`、同目录 `removal_alarm-inspect/metrics.json`；内存检查 `build-asan/services-review2-5m3g81en/`。
- 【实现】删除事务只遗忘成员集合并清成员 Flash/卡目录；`time.alarms` 是 **设备级**集合，内存表和持久记录都留着，查询又只比较可复用的 `owner` 字符串。`removal` 回归只验证文件和一个人工注册的成员集合，没有创建真实闹钟/提醒，不能证明所有旧数据已清。
- 影响与修法：正常成员管理流程即可让孩子读取旧成人的私人对象，按复查口径定严重。将设备级集合中归属于该成员的记录、定时任务和迟到事件纳入可恢复清理，清理持久完成后才释放编号；对可复用身份增加代次校验，防止旧对象重新归属。补真实服务对象的“删除→复用→重启”回归。

### R2-02（高）：去重缓存先于成员检查返回，孩子拿到成人的旧回执，自己的写入还被吞掉

- 位置：`system/policy/cap.c:317`、`:342`、`:556`、`:563`；成员绑定直到 `:575` 才设置，策略校验在 `:521`。真实 tools 入口在 `:1317` 传入同一 request_id/member 路径。
- 【实测】成人用 `synthetic-request-1` 创建 `/me/SYNTHETIC_ADULT_PRIVATE`。切到孩子后，以相同请求号创建孩子的 `/me/child.txt`，实际回执却是成人的 `file:/me/SYNTHETIC_ADULT_PRIVATE`、`done`，再读孩子文件为 `failed`。不同请求号的孩子写入成功。同成员同请求号重试也正常，排除了普通写入/重试失效。
- 【实测】补充“旧成员请求重发”对照：换人后明确绑定 `member=owner`，新请求号正确返回 `stale`；同一旧请求号仍返回成人的私人回执。这条路径不依赖两个成员碰巧生成相同请求号，也说明即使调用者显式携带成员，缓存路径仍绕过校验。
- 复现命令：V5 `INK_REVIEW_CASES=replay`。两种构建各 3 个失败；常规证据 `build/services-review2-x6od6xu0/replay-run/metrics.json`；内存检查 `build-asan/services-review2-5m3g81en/replay-run/metrics.json`。实验观察实际能力回调，未声称连接过真实中枢。
- 修法：去重键至少绑定成员身份/代次与请求号，并核对能力及参数身份；返回缓存前仍检查当前成员和空间状态，处理在途请求时保持同样边界。不能只在普通执行分支检查成员。增加跨成员碰撞、迟到重试和关空间后的缓存读取测试。

### R2-03（高）：记录和文件的旧撤销只匹配撤销票据，不校验当前内容，直接删掉后来的人工修改

- 位置：`system/policy/cap.c:1099`、`:1112`；`system/services/data/data_caps.c:215`、`:221`；`system/services/file/file_caps.c:179`、`:184`。
- 【实测】AI 创建记录，真人随后用 `data.put` 更新同一记录为 `SYNTHETIC_HUMAN_EDITS`，再用 AI 原回执的 **object 和 revision** 调 `system.undo`，得到 `undone`，随后 `data.get` 已找不到记录。文件同样：AI 创建 `draft.txt`，人追加 `SYNTHETIC_HUMAN_APPEND`，原撤销直接删除整个文件。两条链均无人工确认卡；未被修改的控制记录可正常撤销。
- 复现命令：V5 `INK_REVIEW_CASES=undo_data`。两种构建各 4 个失败；证据为各 V5 主目录中的 `undo_data-run/metrics.json`。
- 【实现】新加的 `revision` 只筛选 `UNDO` 中的旧票据。仅 `settings.set` 的逆操作带当前版本校验并保留调用来源；记录和文件的逆操作仍以 `INK_SRC_CONFIRMED` 调不可逆删除，未验证对象在创建后是否改变。第 1 轮的设置撤销修复有效，但不能推广为所有撤销安全。
- 影响与修法：在 AI 与人先后编辑的现实顺序下，旧请求可不可恢复地删除人的新内容，定高。为这些逆操作绑定对象身份、成员代次及修改版本，并在删除锁内验证；目标改变后应拒绝旧逆操作或重新请求人工确认。不能把“曾经可撤销”永久当成对当前内容的删除授权。

### R2-04（中）：空白卡没有个人目录的父目录时，删除未使用的成员反复失败

- 位置：`system/services/file/file_service.c:470`、`:473`、`:924`；`system/policy/members.c:568`、`:575`。
- 【实测】使用可读写的空白模拟卡，新增一个从未存个人文件的孩子，正常确认删除得到 `failed`，再次确认重试仍失败，名单中保留 `removing:true`。让当前成人写入一个无关的 `/me/control.txt` 后，同一删除重试立即成功。没有拔卡、USB 独占或故障注入。
- 复现命令：V5 `INK_REVIEW_CASES=empty_removal`。两种构建各 2 个失败；最终常规证据 `build/services-review2-w4ydnpyx/`，内存检查见 `build-asan/services-review2-empty-removal-final.log`。
- 【实现】`wipe_tree` 对不存在的成员根尝试 `mkdir` 再检查，但只创建末级 `/content/.m/unused`，尚不存在的 `.m` 父目录使这一步失败。回执要求插好卡无法解决实际原因。
- 修法：可靠地区分不存在与读取失败，或逐级建立并验证必要父目录后检查空根；保留真实 I/O 错误时的隔离标记。补空卡、无个人目录的删除正例，不能撤掉失败保留编号的保护。

以上为本轮补查发现的遗漏；不是将它们都归因于第 1 轮修复引入。R2-01/02 属 S6，R2-03 属 S2 的撤销与删除保护，R2-04 属 R1-05 清理失败处理的边界。

## 第 1 轮问题复核

| 原编号 | 本次结论 | 独立证据 |
| --- | --- | --- |
| R1-01 高 | 指定掉电窗口已修 | `upgrade_crash` 的正常、有效槽提交前/后断电、提交失败四链共 16 次启动通过；旧源码在提交前断电的两次恢复及提交错误分支失败。 |
| R1-02 高 | 指定播放交接已修 | `playback` 12 检查通过，UI/媒体队列忙时旧流均停止；旧源码 3 失败。既有 `integration-media` 保留同成员锁屏听书。 |
| R1-03 高 | 满额及成员间待记槽隔离已修 | `activity` 31 检查通过，1,600 次动作可继续、容量受限、单成员积压不占满其他成员容量；旧源码 24 失败。异步日志掉电可能丢未写条目的限制已披露。 |
| R1-04 严重 | 已修 | `permissions` 26 检查，真实剪贴板权限、儿童音量规则、撤销重新获权、拒绝/换人/无效值均覆盖；旧源码 15 失败。普通设置仍自动执行。 |
| R1-05 高 | 原故障链已修，整体删除隔离仍不完整 | `removal` 卡/Flash/缓存/唯一 PIN 四链共 12 次启动通过；旧源码能检出各链缺陷。但只验成员目录不足，R2-01/04 仍失败。 |
| R1-06 中 | 设置范围已修 | `undo` 13 检查通过，连续修改选最新、版本过时不覆盖人工改动、过期无效；旧源码 4 失败。记录/文件问题另列 R2-03。 |
| S4-DET 中 | 栈计量修复成立 | 两套 `stack_meter` 为 0 失败；换回 `b431412` 为 7 失败。按完整机器字保守计量，没有过滤确定性指标或调高预算。 |

## 原 A/B/C2 逐项核对

V2 正向在两种构建均执行；V3 用开发前的对应服务源码反向检验。下表只说明原问题的指定场景，不抵消本轮四项发现。

| 原编号 | 原等级 | 本次结论与依据 |
| --- | --- | --- |
| AUD18-101 | 高 | `files` 不可读根报错；旧代码误报成功。无父目录的新边界见 R2-04。 |
| AUD18-102 | 高 | `files` 目录及短操作持卡，阻止 USB 独占穿透；旧代码失败。 |
| AUD18-103 | 中 | `files` 作业先完成也能结束任务；旧代码留下 running。 |
| AUD18-104 | 高 | `update` 拒绝状态写入时不切槽，缺状态不标有效；原代码失败；提交阶段另由 `upgrade_crash` 覆盖。 |
| AUD18-105 | 高 | `update/resources-preserved` 不允许新包覆盖待恢复资源；旧代码失败。 |
| AUD18-106 | 中 | 未做；实现仍只记最近失败版本，报告已明确失败历史及人工重试规则待后续。 |
| AUD18-107 | 中 | `update/tables` 正常包通过、八类坏包拒绝；旧代码误收其中四类。 |
| AUD18-108 | 中 | `update/offer-changed`、`offer-kept` 固定下载身份，两链正反例成立。 |
| AUD18-109 | 高 | `storage` 正向 7 检查通过；兼容旧接口后反向检出共享计时节点损坏，独立定时回调未发生，测试未到完成点。链接兼容处理见 V3，未将链接错误算作缺陷复现。 |
| AUD18-201 | 高 | `audio`、`voice` 拥堵时及时停止采集/上传；旧代码失败。 |
| AUD18-202 | 高 | `text` OOM 后丢弃旧 redo；旧代码失败。 |
| AUD18-203 | 高 | `shutdown` 断电前录音结束且数据持久完成；旧代码失败。 |
| AUD18-204 | 高 | `audio` 耳机掉线且队列忙时不外放私有媒体；旧代码失败。 |
| AUD18-205 | 中 | `audio` 通话后保留闹钟焦点；旧代码失败。 |
| AUD18-206 | 中 | 未做；仍缺具体关机帧显示成功的回执，报告保留跨显示/UI 修改。 |
| AUD18-207 | 中 | 未做；文本窗口与选区映射在范围外 `system/ui/textfield.c`，已列后续。 |
| AUD18-208 | 中 | `text` AI 不能提交隐藏字段；旧代码失败。 |
| AUD18-209 | 中 | `text` 初始 UTF-8 截断保留完整字符；旧代码失败。 |
| AUD18-401 | 高 | `bindings` 16 检查覆盖字段、内容、光标的 session/revision 过期；旧代码 8 失败。 |
| AUD18-402 | 中 | 未做；普通/数字键盘与页面的组合语义快照缺口仍明确保留。 |
| AUD18-403 | 高 | `lowbattery` 超时分支两套各 6 检查通过，旧代码 2 失败；另跑成功保存分支两套各 5 检查通过。 |
| AUD18-404 | 低 | 未做；§1.3 能力总表漏项仍在。`integration-ai-surface` 核对的是代码清单和运行时，并不证明文档表完整。 |

## C 路六项补查与架构审视

| 范围 | 本次结论 |
| --- | --- |
| 模块、组件与内部机制 | 【实现】数据集合、文件、音频、时间、升级及策略基本按职责分层，`layering`、`sdk-boundary` 通过。HAL 资源写入和系统词库加载属于内部机制，不必伪装成用户能力。成员删除跨服务清理不完整，说明仅按物理目录划界不足；应由各服务参与身份生命周期，见 R2-01。 |
| AI 原生、结果、来源与撤销 | 【实现/实测】按钮和 AI 使用能力入口；活动记录、确认绑定和设置分级有实际路径。旧回执与逆操作走了未充分校验的路径（R2-02/03），不能据“都经过总线”推导隔离与授权正确。 |
| 能力开放 | 【实测】`integration-ai-surface` 检查 tools/list，`actions` 检查真实请求表。成员解锁、设 PIN、确认接受/拒绝、两种安装入口六项 OWNER 例外有合理的凭据/人工操作边界；AUD18-404 文档缺漏另列。 |
| 成员、事件和页面隔离 | 【实测/实现】原 `handoff isolation playback` 及通知/确认/应用/成员门回归通过，旧代码可反向检出原边界。删除后设备级记录和回执缓存仍跨成员（R2-01/02）。未把局部通过扩大成全系统隔离保证。 |
| 文档真实性 | 【文档/实现】§7、§15 已披露异步活动日志、未完成能力及第 1 轮失效结论；但 `REPORT.md` 的 S2/S6“通过本轮验证/补查”仍不足以支持任务验收，需纳入本轮新证据。 |
| 第十四至十六轮修复 | 【实测】全量包含 `integration-people-reset`、`integration-races`、`integration-listening` 及 data/files/people/ai-surface/time/apps/notify/confirm 路径，对应成员表读取、旧事件、关空间与已知通知/确认边界。未另做密码尝试次数或等待时间实验，也不宣称穷尽所有时序。 |

## 独立命令与运行证据

环境按用户指定的 `$INK_DEPS`、`$INK_DESIGN`。复用并核查两套现有 CMake 缓存：`INK_WERROR=ON`、指定字体和只读依赖；`build-asan` 另为 `INK_SANITIZE=ON`。`$INK_DESIGN` 未使用，此任务不验 UI 外观，十屏视觉对照不适用。仅既有 `gui-smoke` 用 `SDL_VIDEODRIVER=dummy`，没有窗口。

V1：两次构建退出 0；常规和 ASan/UBSan CTest 均 86/86。日志为两个构建目录中的 `services-review2-build.log`、`services-review2-ctest.log`。

```bash
cmake --build build -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
```

V2：两套各 19 组全部通过，脚本逐个编译、逐个运行。它们未接入 CTest，已单独执行，不能用 86/86 代替。日志为两个构建目录中的 `services-review2-regressions.log`，产物分别为 `build/services-finish-44hro0es/`、`build-asan/services-finish-6x23ih7l/`。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash removal
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --build build-asan files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash removal
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --build build-asan lowbattery
```

保存成功对照的日志为两目录 `services-review2-saved.log`；产物 `build/services-finish-j8yvs_id/`、`build-asan/services-finish-xlz2c7uy/`，各 5 检查通过。

V3：第 1 轮六组反向退出 **1（预期）**，全部能检出旧问题；正常升级控制链通过。证据 `build/services-review2-reverse.log`、`build/services-finish-_2z6awm7/`。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline d74a261 permissions undo playback activity upgrade_crash removal
```

原 A/B/C2 反向对照：先运行下列命令，`files` 复现 5 个失败；到 `storage` 时发生链接错误，**该错误不算反向证据**。原因是当前成员服务引用新增的 `ink_db_try_forget_member`，旧 datastore 没有这个符号，链接器又抽取当前 datastore，产生重复定义。日志 `build/services-review2-original-reverse.log`。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline 70d3a43 files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions
# 继续其余组，均检出各自缺陷，退出 1
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline 70d3a43 update text bindings lowbattery audio voice handoff isolation shutdown policy actions
```

其余组证据 `build/services-review2-original-reverse-rest.log`、`build/services-finish-51q3331_/`。`storage` 另以附录的只读适配命令补齐：只为旧版本增加**该探针未调用**的新符号，固定返回失败，不修改被测 `ink_db_forget_member`。运行退出 1，独立定时器断言失败且未到完成点；证据 `build/services-review2-storage-reverse.log`、`build/services-finish-daxg428i/`。因此原 13 组均有实际反向运行证据，但旧版通用命令已不能原样跑完，后续应维护跨版本链接兼容性。

V4：栈计量正反例。两个正向退出 0；旧实现退出 1，7 个前缀碰撞均检出。证据为两目录 `services-review2-stack.log`、`build-asan/services-review2-stack-reverse.log`。

```bash
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py --build build-asan
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py --build build-asan --baseline b431412
```

V5：下方报告内探针。常规及严格 ASan/UBSan 均检出四项问题，合计每种构建 **13 个功能断言失败**，脚本退出 **1（发现问题）**，模拟器自身退出 0，无 sanitizer 诊断。没有改动或修补产品实现。控制组包含同成员重试、不同编号的成员与请求、未修改记录的撤销，以及建立个人目录后的删除重试。

- 常规主证据：`build/services-review2-probes-final.log`、`build/services-review2-x6od6xu0/`；空目录探针调整为成功即停止后单独重测，`build/services-review2-empty-removal-final.log`、`build/services-review2-w4ydnpyx/`。
- ASan/UBSan：前三项 `build-asan/services-review2-probes.log`、`build-asan/services-review2-5m3g81en/`；空目录单独最终重测 `build-asan/services-review2-empty-removal-final.log`。
- 初次删除探针误用了超过能力 Schema 长度的标题，且没有建立个人目录，结果未用于 R2-01 的结论；改成合法合成标题并建立正常个人文件后才验证删除与复用，以上是有效证据。

## 范围与规则检查

- 【实测】初始工作区干净，最终只改本报告；本次未提交或推送。`git diff --stat d74a261..HEAD` 为 27 文件；公共头文件两处及栈计量内核为报告已列明的最小接口/验收阻挡项例外，没有发现额外重构或依赖升级。整个收尾差异 `70d3a43..HEAD` 为 78 文件，其中包括任务规则、报告和复查材料。
- 【实测】指定隐私正则扫描 `fdff779..HEAD` 为 4,134 匹配行，`fdff779..70d3a43` 也为 4,134；收尾实现区间和第 1 轮修复区间均为 0。未回显或改动这些继承审计材料。差异文件没有超过 5 MiB 的文件。
- 【实测】`git diff --check 70d3a43..HEAD`、`git diff --check d74a261..HEAD` 均退出 0。完整指定基线 `git diff --check fdff779..HEAD` 退出 2、1,157 条诊断，和继承资料区间一致；不能写成完整差异无空白问题。改动文档的相对文件链接已核对，无失效。汇总 `build/services-review2-closure.json`。
- 【实测】没有改任何 `tests/scenarios/*.budget.json`。两套 PQBN AI 场景 `mem.internal_total.min_free_bytes=33192`，原门槛 32768，余 424 字节；`mem.internal.failed_allocs=0`。证据为各构建目录 `test-out/ai_ask-metalio-plus-pqbn/metrics.json`。
- 【实现】已有测试改动逐条核对：people/media/textinput 用真实人工确认辅助函数设置新增高风险项，原成员、PIN、锁屏、音量与剪贴板断言仍在；whole_product 的目标绑定与人工确认辅助函数没有伪造 `INK_SRC_CONFIRMED`。此前 confirm/systest/time 的普通设置期望变化、files/notify/people 的危险操作确认符合 D-105。未删整项测试、抬预算、放宽确定性比较或以新增 sleep 掩盖竞态；新增等待用于到期、持久化完成或测量交接后的播放。
- 【边界】只验证本机模拟器，没有 Linux、真机、真实中枢证据；D-95、确认结果持久化、配对/同步、三秒外发撤销以及报告保留的 4 中、1 低不冒充已完成。本轮四项不需要新的产品选择即可确定错误。

## 补充实验脚本

单项运行在命令前加 `INK_REVIEW_CASES=replay`（或 `undo_data`、`removal_alarm`、`empty_removal`）；内存检查加 `INK_REVIEW_BUILD=build-asan`。默认跑四组，成员删除组连续启动两次，共五个模拟器进程。脚本读取 `test.fail` 与完成标记，不能只看模拟器自身退出码。

从本报告提取脚本运行，编译与模拟数据仅进入忽略的构建目录：

```bash
$INK_DEPS/build-deps/pyenv/bin/python - <<'PYTHON'
from pathlib import Path
s = Path('docs/tasks/services-finish/REVIEW.md').read_text()
exec(s.split('<!-- REVIEW2_PROBE_BEGIN -->\n```python\n', 1)[1].split('\n```', 1)[0])
PYTHON
```

<!-- REVIEW2_PROBE_BEGIN -->
```python
import json, os, shlex, subprocess, tempfile
from pathlib import Path
root=Path.cwd()
build=(root/os.environ.get('INK_REVIEW_BUILD','build')).resolve()
work=Path(tempfile.mkdtemp(prefix='services-review2-',dir=build))
common='#include "tests/services_finish/product.h"\n'
entry=r'''
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review2-probe",.fn=run,.stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
'''
sources={}
sources['replay']=common+r'''
static const char *requested_member;
static bool with_id(const char *id,const char *args,const char *rid) {
    ink_call_opts_t o={.src=INK_SRC_AI,.request_id=rid,.args_json=args,
        .member=requested_member?requested_member:ink_member_current()->id,.reply_exec=INK_EXEC_DIRECT,.done=rc_done};
    g_done=0;g_result[0]=0;
    return ink_cap_call(id,NULL,&o)==0 && WAIT_FOR(g_done,5000);
}
static void run(void *arg) {
    (void)arg;delay_ms(1800);
    const char *owner="{\"path\":\"/me/SYNTHETIC_ADULT_PRIVATE\",\"text\":\"owner\"}";
    CHECK(with_id("file.write",owner,"synthetic-request-1")&&g_rc.status==INK_RC_DONE,"adult write");
    CHECK(with_id("file.write",owner,"synthetic-request-1")&&g_rc.status==INK_RC_DONE,"same-member retry");
    CHECK(ink_member_switch("kid")==0,"switch to child");
    CHECK(with_id("file.write","{\"path\":\"/me/child.txt\",\"text\":\"child\"}","synthetic-request-1"),"child receipt");
    CHECK(!strstr(g_rc.object,"SYNTHETIC_ADULT_PRIVATE"),"adult receipt leaked: %s status=%s",g_rc.object,ink_rc_name(g_rc.status));
    CHECK(AI("file.read","{\"path\":\"/me/child.txt\"}")&&g_rc.status==INK_RC_DONE&&strstr(g_result,"child"),"acknowledged child file exists: %s",ink_rc_name(g_rc.status));
    CHECK(with_id("file.write","{\"path\":\"/me/control.txt\",\"text\":\"child\"}","synthetic-child-2")&&g_rc.status==INK_RC_DONE&&strstr(g_rc.object,"control.txt"),"distinct id control");
    requested_member="owner";
    CHECK(with_id("file.write",owner,"synthetic-old-fresh")&&g_rc.status==INK_RC_STALE,"uncached old-member request is stale");
    CHECK(with_id("file.write",owner,"synthetic-request-1")&&g_rc.status==INK_RC_STALE&&!strstr(g_rc.object,"SYNTHETIC_ADULT_PRIVATE"),"cached old-member retry bypasses isolation: %s %s",ink_rc_name(g_rc.status),g_rc.object);
    ink_probe_count("test.finished");
}
'''+entry
sources['undo_data']=common+r'''
static const ink_db_def_t NOTE={.name="review2.note",.scope=INK_DB_MEMBER,.schema=1,.json=true,.ai_read=true,.ai_write=true};
static void run(void *arg) {
    (void)arg;delay_ms(1800);
    CHECK(ink_db_register(&NOTE)==0,"collection");
    CHECK(AI("data.put","{\"collection\":\"review2.note\",\"id\":101,\"value\":{\"text\":\"draft\"}}")&&g_rc.status==INK_RC_DONE,"AI draft");
    char undo[160];snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(BY_HAND("data.put","{\"collection\":\"review2.note\",\"id\":101,\"value\":{\"text\":\"SYNTHETIC_HUMAN_EDITS\"}}")&&g_rc.status==INK_RC_DONE,"human edits");
    CHECK(AI("system.undo",undo),"record undo answers");
    CHECK(g_rc.status==INK_RC_STALE||g_rc.status==INK_RC_PENDING_CONFIRMATION,"old inverse deleted newer record: %s",ink_rc_name(g_rc.status));
    CHECK(AI("data.get","{\"collection\":\"review2.note\",\"id\":101}")&&g_rc.status==INK_RC_DONE&&strstr(g_result,"SYNTHETIC_HUMAN_EDITS"),"human record survives: %s",ink_rc_name(g_rc.status));
    CHECK(AI("data.put","{\"collection\":\"review2.note\",\"id\":102,\"value\":{\"text\":\"control\"}}")&&g_rc.status==INK_RC_DONE,"control record");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(AI("system.undo",undo)&&g_rc.status==INK_RC_UNDONE,"unmodified control undo");
    CHECK(AI("file.write","{\"path\":\"/me/draft.txt\",\"text\":\"draft\"}")&&g_rc.status==INK_RC_DONE,"AI file");
    snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(BY_HAND("file.write","{\"path\":\"/me/draft.txt\",\"text\":\"SYNTHETIC_HUMAN_APPEND\",\"append\":true}")&&g_rc.status==INK_RC_DONE,"human append");
    CHECK(AI("system.undo",undo),"file undo answers");
    CHECK(g_rc.status==INK_RC_STALE||g_rc.status==INK_RC_PENDING_CONFIRMATION,"old inverse deleted newer file: %s",ink_rc_name(g_rc.status));
    CHECK(AI("file.read","{\"path\":\"/me/draft.txt\"}")&&g_rc.status==INK_RC_DONE&&strstr(g_result,"SYNTHETIC_HUMAN_APPEND"),"human append survives: %s",ink_rc_name(g_rc.status));
    ink_probe_count("test.finished");
}
'''+entry
sources['removal_alarm']=common+r'''
#include "ink/hal_power.h"
static int results;
static char result_status[24];
static void result(void *ctx,const char *id,const ink_args_t *args) {
    (void)ctx;(void)id;ink_args_get_str(args,"status",result_status,sizeof result_status);results++;
}
static bool confirmed(const char *cap,const char *args) {
    if(!BY_HAND(cap,args)||g_rc.status!=INK_RC_PENDING_CONFIRMATION)return false;
    unsigned token=0;sscanf(g_rc.object,"confirm:%u",&token);
    int before=results;
    return ink_confirm_decide(token,true,INK_SRC_TOUCH)==0&&WAIT_FOR(results>before,5000)&&!strcmp(result_status,"done");
}
static void inspect(void) {
    CHECK(ink_member_switch("oldadult")==0&&ink_member_current()->role==INK_ROLE_CHILD,"reused id is child");
    CHECK(AI("alarm.list",NULL)&&g_rc.status==INK_RC_DONE,"child alarm list");
    CHECK(!strstr(g_result,"SYNTH_ALARM"),"adult alarm leaked: %s",g_result);
    CHECK(AI("reminder.list",NULL)&&g_rc.status==INK_RC_DONE,"child reminder list");
    CHECK(!strstr(g_result,"SYNTH_REMINDER"),"adult reminder leaked: %s",g_result);
}
static void run(void *arg) {
    (void)arg;delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result",result,NULL,INK_EXEC_DIRECT)==0,"listener");
    if(getenv("INK_REVIEW_INSPECT")){inspect();ink_probe_count("test.finished");return;}
    CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"adult\"}"),"add adult");
    CHECK(ink_member_switch("oldadult")==0,"enter adult");
    CHECK(ink_fs_write_atomic("/me/control.txt","SYNTHETIC",9)==0,"personal card file");
    CHECK(AI("alarm.set","{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_ALARM\"}")&&g_rc.status==INK_RC_DONE,"adult alarm");
    CHECK(AI("reminder.add","{\"at\":\"2099-12-01 12:00\",\"title\":\"SYNTH_REMINDER\"}")&&g_rc.status==INK_RC_DONE,"adult reminder");
    CHECK(ink_member_switch("kid")==0,"distinct child control");
    CHECK(AI("alarm.list",NULL)&&!strstr(g_result,"SYNTH_ALARM"),"distinct child cannot see adult alarm");
    CHECK(ink_member_switch("owner")==0,"owner manages family");delay_ms(300);
    CHECK(confirmed("member.remove","{\"id\":\"oldadult\"}"),"normal removal done");
    CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"child\"}"),"reuse id as child");
    inspect();ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle()&&!ink_storage_pending(),4000),"durable checkpoint");
    ink_probe_count("test.finished");hal_power_reboot();
}
'''+entry
sources['empty_removal']=sources['removal_alarm'].split('static void inspect(void)',1)[0]+r'''
static void run(void *arg) {
    (void)arg;delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result",result,NULL,INK_EXEC_DIRECT)==0,"listener");
    CHECK(confirmed("member.add","{\"id\":\"unused\",\"role\":\"child\"}"),"add unused member");
    bool removed=confirmed("member.remove","{\"id\":\"unused\"}");
    CHECK(removed,"empty member removal should succeed: %s",result_status);
    if(!removed) {
        removed=confirmed("member.remove","{\"id\":\"unused\"}");
        CHECK(removed,"retry should not remain blocked by absent parent: %s",result_status);
        if(!removed) {
            CHECK(AI("member.list",NULL)&&strstr(g_result,"\"removing\":true"),"failed removal retains quarantine");
            CHECK(ink_fs_write_atomic("/me/control.txt","SYNTHETIC",9)==0,"unrelated owner file establishes personal root");
            CHECK(confirmed("member.remove","{\"id\":\"unused\"}"),"same retry succeeds once parent exists");
        }
    }
    ink_probe_count("test.finished");
}
'''+entry
flags={}
for line in (build/'CMakeFiles/sim-media.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k,v=line.split(' = ',1);flags[k]=shlex.split(v)
base=shlex.split((build/'CMakeFiles/sim-media.dir/link.txt').read_text())
failed=False
for name in os.environ.get('INK_REVIEW_CASES','replay undo_data removal_alarm empty_removal').split():
    obj=work/(name+'.o');binary=work/name
    subprocess.run([base[0],*flags['C_DEFINES'],*flags['C_INCLUDES'],'-I'+str(root),*flags['C_FLAGS'],'-Wno-unused-function','-x','c','-c','-','-o',str(obj)],input=sources[name],text=True,cwd=build,check=True)
    link=base.copy();index=next(i for i,w in enumerate(link) if '/tests/integration/' in w and w.endswith('.o'))
    link[index]=str(obj);link[link.index('-o')+1]=str(binary);subprocess.run(link,cwd=build,check=True)
    for phase in (('prepare','inspect') if name=='removal_alarm' else ('run',)):
        out=work/(name+'-'+phase)
        cmd=[str(binary),'--profile',str(root/'simulator/profiles/metalio-plus.json'),'--assets',str(build/'assets'),'--content',str(build/'test-content'),'--out',str(out),'--end','25s','--quiet','--no-random-outages']
        if name in ('removal_alarm','empty_removal'):
            sd=work/(name+'-sd');sd.mkdir(exist_ok=True);cmd+=['--flash-image',str(work/(name+'.img')),'--sd-dir',str(sd)]
        run=subprocess.run(cmd,cwd=root,capture_output=True,text=True,timeout=120,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1',**({'INK_REVIEW_INSPECT':'1'} if phase=='inspect' else {})})
        (work/(name+'-'+phase+'.stderr.log')).write_text(run.stderr)
        (work/(name+'-'+phase+'.stdout.log')).write_text(run.stdout)
        metrics=json.loads((out/'metrics.json').read_text());m=metrics['metrics']
        ok=run.returncode==0 and m.get('test.finished')==1 and not m.get('test.fail',0)
        print(name,phase,'PASS' if ok else 'FAIL','exit='+str(run.returncode),'checks='+str(m.get('test.pass',0)),'failures='+str(m.get('test.fail',0)),flush=True)
        for w in metrics.get('warnings',[]):
            if 'CHECK FAILED' in str(w):print(w,flush=True)
        failed|=not ok
print('Evidence directory:',work.relative_to(root),flush=True)
raise SystemExit(int(failed))
```

## 旧 datastore 反向运行的链接适配

该命令在内存中改写测试驱动的单个编译调用，不写测试或产品源码。新增符号仅为满足当前成员服务的链接；返回失败保持保守行为，`storage` 探针仍直接调用旧 `ink_db_forget_member`。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python - <<'PYTHON'
import sys
from pathlib import Path
path = Path('tests/services_finish/run.py').resolve()
s = path.read_text()
old = '        subprocess.run(cmd, cwd=build, check=True)'
new = '''        if name == 'storage' and a.baseline:
            at = cmd.index(str(ROOT / 'tests/services_finish' / source))
            cmd[at:at + 1] = ['-x', 'c', '-']
            cmd += ['-I' + str(ROOT / 'tests/services_finish')]
            compatibility = 'int ink_db_try_forget_member(const char *m) { (void)m; return -1; }\\n'
            body = (ROOT / 'tests/services_finish' / source).read_text()
            subprocess.run(cmd, input=compatibility + body, text=True, cwd=build, check=True)
        else:
            subprocess.run(cmd, cwd=build, check=True)'''
assert s.count(old) == 1
sys.argv = [str(path), '--baseline', '70d3a43', 'storage']
exec(compile(s.replace(old, new), str(path), 'exec'), {'__name__': '__main__', '__file__': str(path)})
PYTHON
```

SEVERE=1 HIGH=2
