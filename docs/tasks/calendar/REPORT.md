# calendar 执行报告

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。工作分支：`claude/codex-calendar`。状态：页面与专用验收已完成，正在全量及内存检查。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 合成的混合事项按时间排序，覆盖今天、明后天、关闭项、过期项及 F4 翻页 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar/run.py --build build --visual` |
| 2 | 【实测】通过 | 触控和 AI 翻月、跨年、选日、当天列表翻页、空状态；6 行月历与未来重复闹钟 | 同上，两个场景各 210 项断言通过；`build/calendar-acceptance.log` |
| 3 | 【实测】通过 | 双向切换大人/孩子，列表和月历均不串；单独编译移除时间服务成员过滤的版本，原断言失败 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar/run.py --build build --negative-member`；`build/calendar-negative.log` |
| 4 | 【实测】通过 | 两页在真实 AI 的 3072 字节预算内，AI 与界面快照一致；实体键、触控和 AI 使用相同能力 | 同第 1 行；`tests/calendar/calendar.c` |
| 5 | 待全量验证 | 首次 `INK_WERROR=ON` 构建通过；产品代码约 203 行，未放宽预算 | 构建与测试结果待补 |

## 没做到的

- 【实现】没有独立日程后端，因此只显示时间服务的闹钟、提醒；没有编造日程或来源。接下来每条显示下一次时间；月历投影当前启用的规则，不保存或回放历史。
- 【实测】`capability-table-doc` 的共享生成文档需要更新：基线检查通过，加入六个新能力后检查失败。按总规则第 7 节不修改共享文档、不跳过测试，留待合并时生成。

## 和设计不一致的

- 【实测】已先看画板，再用 `tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/calendar-compare --scenes Upcoming Calendar --reuse` 比对：Upcoming **30.835/255**，Calendar **36.627/255**，均超过 12。对照图与 `scores.json` 在 `build/calendar-compare/`。
- 主题按现有放大后的规格；采用 F4 白底描边行和页码条，画板是浅灰分组，且字号、位置、日期和夹具内容不同。接下来逐行写日期，未按“明天、周五”加分组标题。
- 月历新增底部上月/下月按钮；6 行月份当天列表每页一条，其余每页两条。没有画板的节气、事项圆点或当天标记；没有“加提醒”的独立表单和提醒详情跳转。接下来的“加一件”复用已有语音 AI 入口。

## 改过的已有测试

- 暂无。

## 要用户定的

- 暂无。

## 顺手发现

- 暂无。

## 新增的机制

- 无新任务、线程、状态机、文件格式、配置项、缓存或队列。使用现有 UI 任务、F4 列表与 `alarm.revision` / `time.minute` 通知。
- 六个导航能力由验收 2、4 要求：`calendar.open`、`calendar.prev`、`calendar.next`、`calendar.month_prev`、`calendar.month_next`、`calendar.select`。
- 小接口 `ink_alarm_occurrence` 复用时间服务现有规则，只读返回选定日期起的下一次发生时间；对应验收 2。服务与头文件只在末尾增量添加，不改变调度或数据格式。

## 为什么不能更小

- 两页需要月历网格、农历文字、日期校验、混合排序、成员切换清理、F4 列表及能力/快照接线。已复用主题、时间转换、调度规则和 F4，没有另建框架。
- 看过更小的方案：只按保存的 `next_s` 筛选月历，会漏掉下一次之后的重复闹钟；因此增加 6 行只读服务接口。自行画列表会违背 F4 要求，未采用。

## 要改的文档

- `docs/system/ui-dependencies.md`、生成的 `docs/system/capabilities.md` 由主会话合并时统一更新，本任务不改。
