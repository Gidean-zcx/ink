# confirm-durable 整改报告

日期：2026-11-09。基线：`5e73902ecf637a74697b4f3f2e52dab6eaf2b35d`；从复查提交 `67052ca9` 继续，开工工作区干净。分支：`claude/codex-confirm-durable`。首次交付记录保留在 Git 历史，独立复查见 [REVIEW.md](REVIEW.md)。

**仍不可合并：R1 未解决，完整验收未通过。** R2～R5 已完成修复或补证；R1 只修复提交失败后的静默丢弃。普通与内存版全量均为 197/207 通过、10 项失败；ASan/UBSan 日志未见错误，但失败场景不算内存验收通过。实现提交：`2c3d57ef`、`c69670cb`、`46fcd9e7`。

## 验收表

命令缩写：`T = taskpolicy -c utility python3 tests/confirm_durable/run.py --build build`。所有反例只改忽略目录中的临时源码，不改产品文件。

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1 高 | 未解决；失败回执已补 | 【实测】注入执行前提交失败时，不执行副作用、发出 failed、持久查询保留失败原因。正常听写/清通知的写盘延误仍在；三种小方案及停止原因见下文 | `T --case failure`（0）；`T --mutant failure --case failure`（1），`build/repair-reverse-failure.log` |
| R2 高 | 已修 | 【实现】确认和自动动作共用 confirm.result；事件携带结果版本和实际发送标记，真实 event_ack 只确认已送出的对应版本；成员过滤跳过的事件不因累计水位被误回收。沿用集合 sort 元数据记 ACK 和原回执的持久性，ACK 元数据写入不会把已落盘 done 降回 unknown；仅删除已 ACK 且不在最近 16 条内的记录。【实测】1200 次自动写入、最近 16 条、错 boot ACK、跳过事件、未 ACK 旧记录、后续 20 次 ACK、重启后的新动作均通过。禁用回收，第 897 次起失败。复查报告附录原文复现器四场景共 1234 检查通过，使用原假中枢自动 ACK 时完成 1200 次写入，重启仍为 17 条（元数据加最近 16 条） | `T --case capacity`（0）；`T --mutant ack --case capacity`、`T --mutant sent --case capacity`（各 1）；附录原文命令（0）；`build/repair-reverse-ack.log`、`build/repair-reverse-sent.log` |
| R3 中 | 已修 | 【实现】回执映射集中在 journal：done/undone/partial 为 done，queued/pending_confirmation 保持 running，原回执保留。【实测】真实 notify.post → system.undo 删除通知且查询 done/undone；恢复旧二分映射后断言失败。queued/partial 的映射仅作代码核查，未另称实测 | `T --case receipts`（0）；`T --mutant receipt --case receipts`（1） |
| R4 中 | 已修 | 【实现】binding 保存 expires_us；accepted 查询继续检查同启动 lease、成员、连接和期限。【实测】10 秒确认期限内 accepted，11 秒后 expired；撤掉期限比较后仍报 accepted，测试失败 | `T --case receipts`（0）；`T --mutant expiry --case receipts`（1） |
| R5 中 | 已补齐执行路径反例 | 【实测】同一个后台执行轮次内，先真实人工同意，再切换成员，旧动作副作用为 0。变异撤掉确认执行前的成员/epoch 检查及执行调用的成员绑定；查询隔离不变，旧动作实际多执行一次，副作用反例失败 | `T --case core`（0）；`T --mutant member --case core`（1），`build/repair-reverse-member.log` |
| 原验收 1 | 专项通过 | 【实测】确认/自动动作各四个断电切点并复用镜像重启；不重用 token、不重放 request_id，无最终持久回执则 unknown | `T`（0）；两套各 21 阶段、4151 检查、0 失败；`build/confirm-durable-fia19o1h/`、`build-asan/confirm-durable-l0jsgndy/` |
| 原验收 2 | 专项通过 | 【实测】原状态覆盖及本轮撤销、到期、ACK 回收、重启场景通过 | 同上 |
| 原验收 3 | 通过 | 【实测】成员、内容版本、执行前提交反例命中副作用或持久标记断言；不是仅以脚本退出非零作证 | `T --mutant member/version/commit --case core`（分别运行）；`build/repair-reverse-*.log` |
| 原验收 4 | 未通过 | 【实现】confirm.status 仍经能力表开放，人工确认入口未向 AI 开放；没有修改 UI/文本/文件/传输服务。产品代码相对基线新增 331、删除 46（增删合计 377），新增超 300 行预算 31 行；不冒称预算通过。【实测】两套构建退出 0；全量各 197/207 通过、10 失败、无跳过，CTest 均退出 8 | 下述收尾命令 |

## 没做到的

- **R1 的正常确认流程仍失败。** 【实测】开工重跑听写 7 个、通知 1 个、成员 1 个断言失败，`build/repair-before-ctest.log`。临时探针再次测得执行前提交等待满 5 秒，听写和清通知进入失败分支；探针已移除。成员失败对应先前同意的删除延后到 U 盘模式结束后执行，不能据此称 PIN 绕过。
- 按总规则 §7，同一问题试过三种方案后停止：① 提前收卡，听写仍 4 失败、通知/成员仍各 1；② 用现有后台执行器等 pending 提交后再展示，听写和成员通过，但通知仍失败，且 integration-confirm 新增“卡片未按原时限展示”失败；③ 用现有 lazy 标志合并 pending/执行前写入，仍未通过。证据分别为 `build/repair-r1-ctest.log`、`build/repair-r1b-ctest.log`、`build/repair-r1c-ctest.log`。三种方案均未保留。
- 【实现】最终只保留失败回执：写盘失败后仍不执行，复用 confirmed_done 发出 failed 并尝试写入；集合本身拒写时仍可能 unknown。不新增后台重试、不延长等待、不改业务服务。现有显示/存储调度无法靠这三种局部改动满足旧流程时限；没有进一步扩大到存储优先级或新增机制。
- Linux gcc 未执行；没有设备、真实账号或外部服务操作。ASan/UBSan 未见错误，仍因功能失败未达到完整内存验收。

## 和设计不一致的

- R1 使完整正常流程不成立，不能把执行前有 running 记录等同于功能验收通过。
- 产品代码超过 300 行预算，未压格式凑行数。新增 ACK 回收必须区分“收到事件”和“事件被挤掉”，必须把自动回执接到同一路径，必须保护最近 16 条及未 ACK 记录；只调大容量、直接覆盖或只修确认卡都不满足 R2。没有继续做与缺陷无关的缩行重构。为什么不能更小：首次任务的执行前记录、跨重启编号、确认与自动动作的共用查询本身超过 100 行；只在执行后记结果挡不住执行中断电，只做内存查询挡不住重启丢失。本轮复用既有集合和执行器，没有逐个服务补丁。
- 仍沿用首次交付的保守边界：重启不恢复待执行动作；无 request_id 的重新提交无法识别为重放；参数摘要只排序顶层键；中枢负责自身内容版本的执行前检查。3 秒自动撤销窗口仍未建设，相关自动短信、拨号和 undo_ms 请求继续拒绝；本轮没有把这些缺口说成完成。
- 事件已经丢失且未收到对应 ACK 的旧结果仍保留；不会用事件 ring 的覆盖水位冒充接收确认。长期无有效 ACK 仍可能占满集合，这是拒绝覆盖未确认结果的合同边界。

## 改过的已有测试

- `tests/confirm_durable/regression.c`：原来要求第 16 条之外的 accepted 结果在收到 ACK 后仍可查，改为 unknown，理由是 R2 明确允许回收；此前“刚同意查 accepted”的断言保留。增加容量、重启、撤销、到期、提交失败、已同意后换成员的副作用检查；原断电、查询隔离、版本和提交断言保留。
- `tests/confirm_durable/run.py`：member 变异改为撤执行绑定，保留查询隔离；增加对应修复的变异和可选 case。容量阶段使用复查复现器已有的 2000 秒模拟时长；原 core/cut 时限未放宽。模拟器警告可能按字节截断中文，读取 metrics 用 UTF-8 replacement 解析该诊断文本，test.fail/test.finished 的判定不变。
- capacity 夹具屏蔽假中枢自动 ACK，改为显式投递真实 event_ack 入口；正常产品注册和 ACK 处理不受影响。这让“没收到 ACK”的反例成立。新增辅助函数使用独立名称，保持 REVIEW 附录探针可原文编译，不修改复查报告。另在 ACK 边界将一条发送标记清除，模拟成员过滤跳过的事件；其结果在后续累计 ACK 后仍能查询。容量计数上限由 18 改为 19，对应多保留的这一条未接收结果，不是扩大产品预算。
- 本轮没有修改既有 integration 断言、预算、等待上限或测试选择。先前 ai_surface 只增加 confirm.status 目录期望的改动仍在。

## 要用户定的

没有新增产品决定。R1 与超预算保留为未完成项；不自行引入存储优先级、重试或新线程。

## 顺手发现

- `capability-table-doc` 仍需合并会话更新生成能力文档；本任务按总规则 §7 不改共享文档。
- 最终两套均失败：ui-privacy-regressions-metalio-plus、ui-privacy-regressions-desktop-lcd、capability-table-doc、device-events、integration-people、integration-member-wake-stream、integration-dictation、integration-races、integration-confirm、integration-notify。听写/通知/成员仍分别 7/1/1 个断言失败；这三项的基线对照见 REVIEW，属于此前引入的回归。integration-confirm 的单个失败在复查中已验证基线同样失败；其余项没有本轮完整重建基线，不冒称已排除本任务影响。

## 新增的机制

- 无新任务、线程、状态机、文件格式、配置项、后台重试、缓存或队列。
- R2 要求的 ACK 回收沿用原集合元数据及现有事件 ACK；自动动作共用原 confirm.result，事件增加 result_ver 以免旧 ACK 确认新版本；原事件条目增加实际发送标记，不以被跳过的序号冒充送达；sort 低位区分 ACK 和原回执已落盘，保持查询结果的持久性判断。R4 在原 binding 增加期限。
- 首次任务已有的共用集合、持久 token 区间、执行前记录、confirm.status 与 confirm/request 保留。

## 要改的文档

由合并会话生成 docs/system/capabilities.md，并同步协议里的自动 confirm.result、result_ver、ACK 回收与过期行为；本任务不改生成能力表、backlog、ui-dependencies、requirements、ideas、decisions。

## 验证命令与收尾

- 沿用已核对的 Werror、字体、Python、LVGL 和 INK_HUB_PYTHON 缓存；内存版另开 address/undefined sanitizer，依赖仍来自 INK_DEPS，没有升级。
- 普通构建：`taskpolicy -c utility cmake --build build -j3`。全量：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。
- 内存版顺序执行相同命令，目录换 build-asan。两套构建退出 0，全量均退出 8；普通耗时 116.55 秒、内存版 203.00 秒。日志：`build/repair-final-build.log`、`build/repair-final-ctest.log`、`build-asan/repair-final-build.log`、`build-asan/repair-final-ctest.log`；汇总 `build/repair-final-results.json`。
- 【实测】核对 ASan/UBSan 编译参数，扫描内存版构建、全量、LastTest 和专项 21 个日志，共 24 文件，未检出 sanitizer 错误；汇总 `build/repair-acceptance-summary.json`。
- 【实测】REVIEW 附录“执行”代码块原文退出 0，undo/expiry/capacity/restart-full 四阶段共 1234 检查，0 失败，日志 `build/repair-review-repro.log`。
- 只撤掉 cap_status 中 ACK 已落盘回执的例外判断，经 INK_TEST_CONFIRM_SOURCE 编译并重跑同一脚本，容量场景第 1 次查询退回 unknown，退出 1；日志 `build/repair-reverse-ack-durable.log`。
- 【实测】`git diff --check`、报告文件/链接检查通过。相对指定基线核对 diff --stat，无夹带文件；按总规则的本机路径/MAC 表达式扫描提交差异，无命中（grep 退出 1）。未提交构建产物或本机路径。
- 没有 push、PR、发布、设备操作或远端写入。

## 移植到主线

2026-11-09，基线 `dccc7d89a80e7d3f74f815cc0917e322bda59007`。本节为本次移植结论；上文保留旧分支整改记录，不代表本次验收结果。

- 【实现】只移植 `old-quiesce..old-confirm` 的确认持久化差异，跳过只添加任务书的提交；`REVIEW.md` 原样保留。没有移植 member-quiesce 的实现或测试。
- 【实现】R1：收卡和期限更新在答复时完成；界面答复交给现有 BG 执行器写盘，PIN 回调所在 BG 及其他非界面调用保留完成顺序。写盘等待限于这张确认卡原有的剩余有效期，写盘失败仍返回 failed；提交后统一检查成员、代次、期限和连接，再在原执行器进入能力关口核查内容版本。没有延长既有测试的等待或动作期限。
- 【实现】删去重复的结果包装和绑定读取，复用同一条完成路径；重启有效性直接用已预留的 token 区间判断，不再重复保存 lease。相对本次基线，产品源文件新增 300 行、删除 48 行；另有 3 行 CMake 专项测试注册，不计产品代码。没有通过压缩空白或提高预算达标。
- 【实测】原听写、通知、成员和确认卡断言已恢复通过，断电专项通过。最终普通全量、全部反例和 ASan/UBSan 验证进行中，尚不宣布完成。
- 已有测试的期望变化：`device_events/protocol.c` 的分配期间 A→B→A 检查，仍要求副作用为零；同一连接必须回对应 id 的 stale。若自动结果的私密事件未 ACK，现有中枢会关闭旧连接，此时改为核对连接确已撤销、没有在新连接回复旧 id、当前成员握手恢复。没有移除或放宽执行与隐私检查。专项成员反例改从实际 UI 答复路径排队，避免非 UI 同步答复先执行后才切人的无效夹具。
- 生成能力目录原本因 `confirm.status` 新增而失败；为满足本次明确要求的全量通过，已用 `python3 tools/capabilities.py` 同步生成文件，仅增加该能力及计数。其余共享文档未修改。
