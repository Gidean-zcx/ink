# confirm-durable 执行报告

基线：`5e73902ecf637a74697b4f3f2e52dab6eaf2b35d`。分支：`claude/codex-confirm-durable`。实现提交：`e3e280ac`。

**未通过完整验收，不可合并。** 专项通过，但引入了三个原本通过的流程回归。按总规则 §7，三轮处理仍未通过后停止扩改，交付代码和失败证据。没有远端写入。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 专项通过 | 【实测】确认与自动动作各四个断电切点，复用闪存镜像重启；旧 token 不复用、不执行，已记录的自动 request_id 不重放。完成回执未落盘返回 unknown，落盘后返回 done。普通、内存版各 17 阶段、343 检查、0 失败；证据 `build/confirm-durable-u701y7b6/`、`build-asan/confirm-durable-3hbumh_k/` | `tests/confirm_durable/run.py --build build`（0）；换 `build-asan`（0），由下述全量命令调用 |
| 2 | 专项通过 | 【实测】pending、accepted、rejected、expired、done、failed、member_changed、unknown；中枢请求校验目标成员，结果回传 digest，不在设备执行中枢能力；保留超过 16 条结果 | 同上 |
| 3 | 部分通过 | 【实测】换成员后旧卡拒绝，内容版本变化不执行；移除成员查询隔离、页面版本关口、执行前落盘，三轮均退出 1。成员反向只覆盖结果隔离，未撤除完整执行路径的成员检查，不能声称已完成这一部分反例 | 上述脚本分别加 `--mutant member`、`--mutant version`、`--mutant commit`；日志 `build/reverse-member.log`、`build/reverse-version.log`、`build/reverse-commit.log` |
| 4 | 不通过 | 【实现】confirm.status 已注册，confirm/request 是独立方法。产品代码新增 287、删除 45；新增口径小于 300，增删合计 332，保守口径超 32 行。【实测】两套构建退出 0，全量各 197/207 通过、10 失败，无跳过；CTest 均退出 8。ASan/UBSan 未检出错误，但失败场景不算覆盖通过 | 下述构建、全量命令；日志 `build/ctest-delivery.log`、`build-asan/ctest-delivery.log` |

## 没做到的

【实测】三项新增回归：从指定基线取出原始 cap/confirm，按当前编译参数编译，优先链接到相同测试对象和未改动的其他库；三项基线均通过。证据 `build/baseline-compare/results.json`，不是仅凭旧报告归因。

| 新增失败 | 当前表现 | 基线对照 |
| --- | --- | --- |
| integration-dictation | 开始听写未进入 listening，后续转写和再次启动共 7 个断言失败；诊断记录有执行前落盘失败 | 218 检查通过，退出 0 |
| integration-notify | 人同意后，清空通知没有在原 1 s 断言内完成；诊断记录有执行前落盘失败 | 179 检查通过，退出 0 |
| integration-people | 错误 PIN 后“奶奶仍存在”的查询断言失败；原因未完全定位 | 112 检查通过，退出 0 |

【实测】诊断中存储等待显示守卫最高约 10 s / 7.1 s，超过当前 5 s 落盘等待。恢复原执行器、处理初始化依赖、延后关闭卡片三轮后，以上失败仍存在；未放宽旧测试时间、预算或断言。没有增加重试来掩盖失败。

- 未补齐执行路径成员检查的完整反向对照（见验收 3）。
- 既有 3 s 自动撤销窗口仍未实现；现有关口继续拒绝相应自动短信、拨号和 undo_ms 请求。本次只追踪现有允许执行的自动能力。
- 自动调用未带 request_id 时仍会分配 token 并在回执中返回 confirm_token，但无法把无标识的重新提交识别为同一请求；中枢不得根据 unknown 自动重试。
- Linux gcc 未执行；没有接触设备。测试和数据均为合成内容。

## 和设计不一致的

- 【实现】结果保留在已有数据服务的 confirm.results 集合，不自动清理；尚未接入 event_ack 后回收。容量耗尽时拒绝新动作，不覆盖未确认接收的结果。
- 【实现】顶层参数按键排序后摘要；嵌套 JSON 保留既有序列化，语义相同但写法不同会保守拒绝复用旧 request_id，未做递归规范化。
- 【实现】中枢目标版本由中枢随 digest 绑定并在执行前检查；设备只校验成员、代次、连接和期限，不掌握中枢页面的当前版本。
- 【实现】重启不恢复待执行动作：pending 返回 expired，无最终回执的 running 返回 unknown；中枢卡片的 accepted 在重启、换人或断线后失效。这是保守失效处理，不代表动作成功。

## 改过的已有测试

- `tests/integration/ai_surface.c`：只增加 confirm.status 的 AI 能力目录期望，输入参数为可选；既有能力断言、预算和等待上限未变。
- 新专项曾错误地只等副作用计数、未等回执写盘便查询；最终改为在原 10 s 上限内等待持久 done，不降低完成要求。最终普通、内存版均通过。

## 要用户定的

没有新增产品决定；以上回归需要修复，当前不建议合并。

【实现】启动时用数据记录的 sort 元数据预留 65,536 个 token；本次启动超额、32 位编号耗尽或无法落盘时拒绝继续。这是可逆的实现选择，没有新增配置项。

## 顺手发现

以下六项在基线 member-quiesce 报告中已失败，保持未修。当前两套结果一致：

| 已有失败项 | 本次证据与边界 |
| --- | --- |
| ui-privacy-regressions-metalio-plus、ui-privacy-regressions-desktop-lcd | 各 5 个换成员及页面隔离断言失败；依据原报告归为已有失败，未重建完整基线 |
| device-events | isolation 场景 9 失败；依据原报告归为已有失败 |
| integration-member-wake-stream | 9 个切换、旧音频与听写链路断言失败；依据原报告归为已有失败 |
| integration-confirm | 当前与独立重链基线同为 113 通过、1 失败：缺少 member_changed 事件 |
| integration-races | 独立重链基线未完成；当前完成，但 3 个取消和通知归属断言失败。失败路径已经变化，不能说与基线完全相同 |

另一个失败是 capability-table-doc：新能力尚未写入禁止本任务修改的生成文档。能力解析、运行注册、策略和 AI 页面契约检查均通过。

## 新增的机制

- 任务书要求的共用结果集合：使用 ink_db_*，只记录绑定与回执，不记录原始参数。
- 任务书要求的跨重启标识：启动时持久预留 token 区间，避免尚未写入的请求在重启后复用编号。
- 任务书要求的执行前 running 记录、confirm.status、独立 confirm/request 与自动请求追踪：复用现有执行器、锁和确认队列。
- 没有新增任务、线程、独立持久文件格式、后台重试、缓存或配置项。

为什么不能更小：只在执行后记回执，挡不住回执写入前断电；只补内存查询，重启即丢。选择一个共用集合和中枢入口，避免逐服务修补；删除了原来的单独完成上下文，复用已有确认记录。没有为压缩增删行数保留无用代码。

## 要改的文档

- 合并前用 `python3 tools/capabilities.py` 更新 docs/system/capabilities.md；待合并版本为 `build/capabilities.pending.md`。总规则 §7 禁止本任务改该文件，故对应测试仍失败。
- 已在系统服务设计 §1.3 增列既定的 confirm.status，没有改变确认规则。主会话更新 backlog、ui-dependencies 与设计中的完成状态；协议补充 request_id 查询、自动回执中的 confirm_token 和上述边界。
- 未修改 requirements、ideas、decisions、backlog、ui-dependencies、生成能力文档，未修改其他并行任务的服务文件。

## 验证命令与收尾

- 普通和内存版均使用总规则的字体、Python、LVGL 参数；内存版另加 `-DINK_SANITIZE=ON`。同时指定已有 `-DINK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`，内存版复用 `$INK_DEPS/build/_deps/` 的 SDL2、ImGui 源码，没有升级依赖。
- 构建：`taskpolicy -c utility cmake --build build -j3`；内存版换 build-asan。两套均退出 0，日志为各目录的 build.log。
- 全量：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存版换 build-asan。顺序执行，各退出 8，分别耗时 98.36 s、217.84 s。
- 内存检查：编译参数含 address、undefined；扫描内存版全量输出、LastTest.log 及专项 17 个日志，共 19 个文件，未发现 sanitizer 错误。不能把未通过的功能场景写成通过。
- `git diff --check` 通过；检查相对基线的 diff --stat，无夹带文件。按总规则的路径和 MAC 表达式扫描提交差异，无命中（grep 退出 1）。
- 已完成计划记录和实现里程碑提交；没有 push、PR、对外发布或设备操作。
