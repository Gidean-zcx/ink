# calendar 执行报告

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。工作分支：`claude/codex-calendar`。本地执行结束：验收 1～4 通过，5 未通过；未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 合成的混合事项按时间排序，覆盖今天、明后天、关闭项、过期项及 F4 翻页 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar/run.py --build build --visual` |
| 2 | 【实测】通过 | 触控和 AI 翻月、跨年、选日、当天列表翻页、空状态；2026 年 8 月的 6 行月历与未来重复闹钟 | 同上，两个场景各 221 项断言通过；`build/calendar-acceptance.log` |
| 3 | 【实测】通过 | 双向切换大人/孩子，列表和月历均不串；去掉服务成员过滤的变体退出 1、6 项断言失败，反向对照有效 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar/run.py --build build --negative-member`；`build/calendar-negative.log` |
| 4 | 【实测】通过 | 两页在真实 AI 的 3072 字节预算内，AI 与界面快照一致；实体键、触控和 AI 使用相同能力 | 同第 1 行；`tests/calendar/calendar.c` |
| 5 | 【实测】未通过 | 普通及 ASan/UBSan 构建均通过；两轮全量各 233 项：227 通过、3 失败、3 跳过（均补跑通过），各退出 8；无 sanitizer 报错。产品代码连全部 CMake 接线计 203 行，预算未放宽 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`，内存检查将目录换为 `build-asan`；日志为两目录的 `calendar-ctest.log` |

## 没做到的

- 【实现】没有独立日程后端，因此只显示时间服务的闹钟、提醒；没有编造日程或来源。接下来每条显示下一次时间；月历投影当前启用的规则，不保存或回放历史。
- 【实测】`capability-table-doc` 的共享生成文档需要更新：基线检查通过，加入六个新能力后检查失败。按总规则第 7 节不修改共享文档、不跳过测试，留待合并时生成。
- 【实测】`integration-ai-surface` 的共用白名单尚无六个 `calendar.*` 能力，触发 7 项断言；`integration-apps` 的共用目录断言仍要求 11 个应用，实际新增 `upcoming` 后为 12，触发 1 项断言。任务书规定共用文件仅在末尾追加，因此没有改这些文件中部的期望；合并时需追加六项白名单（`calendar.select` 必填 `day`），并将目录数量改为 12。没有隐藏失败或放宽断言。
- 本机只验证 macOS，未在 Linux 执行；没有实机测试。

## 和设计不一致的

- 【实测】已先看画板，再用 `tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/calendar-compare --scenes Upcoming Calendar --reuse` 比对：Upcoming **30.835/255**，Calendar **36.627/255**，均超过 12。对照图与 `scores.json` 在 `build/calendar-compare/`。
- 主题按现有放大后的规格；采用 F4 白底描边行和页码条，画板是浅灰分组，且字号、位置、日期和夹具内容不同。接下来逐行写日期，未按“明天、周五”加分组标题。
- 月历新增底部上月/下月按钮；6 行月份当天列表每页一条，其余每页两条。没有画板的节气、事项圆点或当天标记；没有“加提醒”的独立表单和提醒详情跳转。接下来的“加一件”复用已有语音 AI 入口。

## 改过的已有测试

- 未改基线已有测试的期望。只新增 `tests/calendar/`；复核后补验真正占 6 行的 2026 年 8 月。

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

- `docs/system/ui-dependencies.md`：Upcoming/Calendar 更新为已接时间服务、F4、能力和 AI 快照；日程后端、画板差异及共享清单阻碍如上。
- 生成的 `docs/system/capabilities.md`：主会话合并后运行 `python3 tools/capabilities.py`，纳入六个 `calendar.*` 导航能力。本任务不改共享文档。

## 补充验证

- 【实测】初次全量的 3 个中枢测试因默认 Python 缺依赖跳过。复用只读依赖目录已有的 `hub/.venv/bin/python`，`tests/hub/run_contract.py` 的 125 项通过，`tests/hub/check_samples.py` 的 128/129 token 边界通过，`tests/hub/e2e.py` 的本机假中枢语音/工具回路通过。日志为 `build/calendar-hub-{contract,crosscheck,e2e}.log`，未安装或升级依赖。
- 【实测，模拟器】专用场景的 `mem.internal_total.min_free_bytes` 为 47840，`mem.external.min_free_bytes` 为 13044544；无分配失败。
- 【实测】`tools/capabilities.py --check --output build/calendar-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json` 通过：生成 330 条定义，运行时匹配 268 个能力、16 个设置。生成文件仅放构建目录，未改共享文档。
- 【实测】内存构建按任务规则只换 `build-asan` 并增加 `INK_SANITIZE=ON`，保留 `INK_WERROR=ON`；全量结果与普通构建相同，3 项失败原因见“没做到的”。中枢的 3 项在 ASan 构建也用现成环境补跑通过，日志在 `build-asan/calendar-hub-{contract,crosscheck,e2e}.log`。
- 【实测】补 6 行月历用例后，产品代码没有变化；两个构建都重建 `sim-calendar`。普通视觉场景各 221 项断言通过；`ctest --test-dir build-asan --output-on-failure -j2 -R '^calendar$'` 通过，219 项断言、0 失败，见 `build-asan/calendar-target-test.log`（普通场景多 2 项截图断言）。
- 【实测】最终差异仅含本任务的产品代码、测试、构建接线和本报告；文件均小于 5 MB。`git diff --check` 通过；按总规则第 6 节扫描全部基线差异，无本机路径或 MAC 匹配（`rg` 退出 1）。没有提交构建产物或设计稿。
