# confirm-durable 执行报告

基线：`5e73902ecf637a74697b4f3f2e52dab6eaf2b35d`。分支：`claude/codex-confirm-durable`。结论：**未通过完整验收，不可合并**；最终全量与内存检查进行中。没有远端写入。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 专项通过，待最终复跑 | 【实测】确认和自动动作各四个断电切点，复用闪存镜像重启；旧 token 和自动 request_id 不重放。执行结果未落盘时是 unknown | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/confirm_durable/run.py --build build`（0）；证据 `build/confirm-durable-mmq21fmr/` |
| 2 | 专项通过，待最终复跑 | 【实测】pending、accepted、rejected、expired、done、failed、member_changed、unknown；保留超过 16 条。中枢请求不会作为设备能力执行 | 同上 |
| 3 | 专项及反向通过 | 【实测】移除成员结果隔离、页面版本关口、执行前落盘时，三轮分别退出 1；未放宽旧断言 | 上述命令分别加 `--mutant member`、`--mutant version`、`--mutant commit`；日志 `build/reverse-member.log`、`build/reverse-version.log`、`build/reverse-commit.log` |
| 4 | 待验证 | 能力注册、全量测试、内存检查、300 行预算 | 待补 |

## 没做到的

- 【实测】本次新增回归：`integration-dictation`、`integration-notify`、`integration-people`。从指定基线分别编译原始 cap/confirm 两个对象并优先链接，三项均通过（218、179、112 个检查），证据 `build/baseline-compare/results.json`。因此不能归为基线故障。
- 【实测】听写与通知的执行前落盘遇到显示守卫，存储等待最高约 10 s / 7.1 s，超过本实现 5 s 等待而未执行；成员流程在错误 PIN 后的“奶奶仍存在”断言失败，原因未完全定位。执行器恢复、初始化依赖、延后关闭卡片三轮处理后仍不通过，按总规则 §7 停止扩改，不放宽旧测试时间或预算。
- 最后一次专项测试发现自身只等副作用计数、未等回执写盘即查询；已改为等待同一个 10 s 上限内出现持久 done，未降低断言，待最终复跑。
- 内存检查尚未完成。
- 没有执行 Linux gcc 或实机验证。

## 和设计不一致的

- 【实现】结果保留在 `confirm.results`，不自动清理；尚未接入 event_ack 后回收。容量耗尽时拒绝新动作，不覆盖未确认接收的结果。
- 【实现】重启不恢复待执行动作；pending 返回 expired、没有最终回执的 running 返回 unknown。中枢卡片的 accepted 在重启、换人或断线后失效。
- 【实现】顶层参数按键排序后摘要；嵌套 JSON 保留已有序列化，语义相同但写法不同会保守拒绝复用旧 request_id。中枢目标版本由中枢随 digest 绑定并在执行前检查，设备不执行中枢能力。

## 改过的已有测试

- `tests/integration/ai_surface.c` 只增加 `confirm.status` 的能力目录期望，参数为可选；既有能力断言、预算和等待上限均未放宽。

## 要用户定的

- 保守实现用数据记录的 sort 元数据在启动时预留 65,536 个 token；每次启动超额、编号耗尽或无法落盘时拒绝继续。没有新增配置项。

## 顺手发现

- 基线自带的 member-quiesce 报告已记录六项全量失败，本任务未擅自修其实现。独立重链基线的 `integration-confirm` 同样缺少 member_changed，`integration-races` 同样未完成；其他四项按旧报告列为已有问题，未重新构建完整基线。

## 新增的机制

- 任务书要求：共用结果集合、跨重启 token 预留、执行前 running 记录、只读状态查询和中枢确认方法；复用 `ink_db_*`、现有执行器和确认队列。
- 没有新增任务、线程、独立持久文件格式、后台重试、缓存或配置项。
- 为什么不能更小：只在执行后记回执，挡不住回执写入前断电；只补内存查询，重启即丢。采用一个共用集合与中枢入口，避免逐服务修补。未选择单独写文件或增加工作线程。

## 要改的文档

- 合并时用 `python3 tools/capabilities.py` 更新 `docs/system/capabilities.md`；待合并版本在 `build/capabilities.pending.md`。总规则 §7 禁止本任务修改生成能力文档，因此 `capability-table-doc` 仍会失败。
- 已在系统服务设计 §1.3 增列既定的 confirm.status，不改变确认规则；主会话更新 backlog、ui-dependencies 和系统服务设计中的完成状态。协议补充 request_id 查询和自动回执中的 confirm_token。

## 里程碑

- 已读取总规则和任务书，确认工作区干净、当前提交只比指定基线多任务书。
- 已实现持久结果、查询、独立确认方法和自动动作追踪；完成专项正反例。修正过执行器位置，最终只有落盘等待在 BG，原能力仍在原执行器运行。
