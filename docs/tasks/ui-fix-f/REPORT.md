# ui-fix-f 执行报告

基线：`a379ef4323b576dbda6543cf0874037f05ca1056`。分支：`claude/codex-ui-fix-f`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| UX12（E01～E07） | 针对性通过 | 【实测】两档字号各 127 条通过；基线各 8 条失败。域名、输入目标、秘密隔离、重连保留会话、结束后重新打开 | C1 |
| UX20（W01～W06、Q01～Q03） | 针对性通过 | 【实测】天气两档各 154 条通过／基线各 4 条失败；计算器各 52 条通过／基线各 10 条失败。刷新降为次动作，旧数据标明，错误后可逐字修改 | C1 |
| UX22（D01～D06） | 针对性通过 | 【实测】两档各 98 条通过／基线各 10 条失败。开始结束具名、全天响铃可辨、错误和底栏在键盘上方 | C1 |
| 普通／120% 前后截图及反向对照 | 通过 | 【实测】四个页面 × 两档字号 × 前后，共 16 轮均得出预期结论；截图见本目录 | C1 |
| 全量 ctest（含 ui-scale） | 未全绿 | 【实测】原话：`99% tests passed, 4 tests failed out of 413`；406 通过、4 失败、3 跳过，退出 8，1255.17 秒。`ui-scale` 和本任务既有测试通过 | C2 |
| 内存检查 | 构建／验证中 | ASan/UBSan 结果待补 | C3、C4 |
| S31 | 未编固件 | 【实测】普通全量中的 S31 宿主单元／负向编译检查已运行；未进行设备操作 | C2 |
| gcccheck | 通过 | 【实测】`gcccheck: 8 compiled, 0 with findings`；没有跳过；只做 GCC 编译检查，不等于 Linux 运行测试 | C5 |
| 范围、行数、隐私检查 | 未运行 | 仅允许文件；产品代码 ≤300 行 | 待记录 |

## 没做到的

内存检查尚在运行。普通全量四项失败：`capability-table-doc`（本任务新增依赖需同步禁止修改的共享文档）；`integration-ai-surface`、`audit5-b5-03`、`privacy-records`（基线对照同样失败，见“顺手发现”）。`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 三项被 CTest 跳过，不计为通过。首轮全量中途退出 143（原因未确认），C2 是随后完成的一轮。

- E02：已合入输入框的标签／清除／固定键盘继续复用；公共输入类型没有“打开”回车枚举，保留“完成”，没有改公共组件。
- W03／W04／W05：保留已有选城市、搜索、清除及查询保留；W06 今日卡片的简短更新时间未加。
- Q01 运算符字重未改；Q02 既有长式换行保留，没有新加历史或运算模式，也未专项验最长算式的报错组合。
- D01～D03 接下来页分组、无时间直达设置和月历今天描边未改；本任务实施 D04～D06 表单与起止时间。
- 无真实网站或设备测试；未专项验证英文 120% 表单外观。来源只反映打开时的网址，没有新增重定向来源跟踪。

## 和设计不一致的

【实现】E03 审计建议展示网页原字段名；现有 `rt.nav` 只提供输入类型与阻止标记，没有原字段名。因此展示网站和“文字／密码”，不编造字段名。日期输入、短输入组件、能力表和会话期限均沿用现有实现。

## 改过的已有测试

保留已有断言与期望，没有放宽预算。天气增加层级／旧数据检查，网页增加目标／秘密／恢复检查及密码、结束截图，日历增加表单检查。日历截图在默认场景也保存，新表单测试另有 `Form` 场景；不改变原有分钟刷新检查。新计算器测试独立编译，未改公共 CMake。

新增的日历测试曾错误查找占位文字，已改查绑定后的“内容”标签；输入防遮挡改查真实 textarea，排除宿主组件用于留间距的底部 padding。最终同时检查标签、控件及祖先边界，没有去掉不遮挡断言。GCC 发现既有分钟刷新诊断的 `%u` 参数在 RV32 是 `unsigned long`，补显式 `(unsigned)`，期望不变；随后普通 `calendar` 单项再次通过。

## 要用户定的

无产品决策。本任务继续采用已有可逆布局与服务；范围外失败交主会话处理。

## 顺手发现

【实测】C6 将四个改动模块全部换为基线独立对象，其他产品源码与基线相同：`integration-ai-surface` 仍失败 1 条（`gallery.reload` 未登记）；回执场景仍失败 6 条；隐私记录仍失败 3 条，失败与 C2 一致。后两项是 AiLog／回执页面断言，根因未继续定位。对应公共测试与页面不在本任务写入范围，未改。

## 新增的机制

无新增任务、线程、状态机、文件格式、配置项或后台重试。UX22 复用现有 `text.focused` 事件做页面布局订阅，只隐藏表单正文、移动现有内容框与底栏。UX12 手动重连复用现有实时会话和 `ink_hub_demand`。

## 要改的文档

由主会话同步 UX12／UX20／UX22 的审计处理状态。`docs/system/capabilities.md` 的日历应用状态依赖应从 `alarm.revision` 改为 `alarm.revision`、`text.focused`（C2 的生成差异）；本任务明确禁止修改生成目录。公共 CMake 可接入专用计算器呈现测试（本次按任务约束只由 C1 独立编译）。共享需求、决策和能力表未改。

## 复现命令

- C1：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" docs/tasks/ui-fix-f/run.py --build build --reverse --screenshots docs/tasks/ui-fix-f/screenshots`。手动反向模式才读取历史，不作为 ctest 前提。最后日历收紧间距、补祖先边界检查后加 `--case calendar` 复跑同一模式；[结果数据](RESULTS.json)、[截图索引](SCREENSHOTS.md)。
- C2：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。
- C3：按总规则第 5 节的相同参数配置 `build-asan`，加 `-DINK_SANITIZE=ON`；`taskpolicy -c utility cmake --build build-asan -j2`；`UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2`。
- C4：C1 换为 `--build build-asan`，不加 `--reverse`，不覆盖普通截图。
- C5：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" "$INK_DEPS/../ink-codex/tools/gcccheck.py" work/ui-fix-f/gcc system/apps/{remote/remote,weather/weather,calculator/calculator,calendar/calendar}.c tests/{remote_app/remote_app,weather/weather,calendar/calendar,ui_fix_f_calculator}.c`。数据库从普通构建复制，并为专用计算器测试复用 `sim-calculator` 的编译参数；不修改公共 CMake。
- C6：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" docs/tasks/ui-fix-f/check-baseline.py --build build --case surface`；另分别指定 `receipts`、`privacy`。本手动对照依赖历史，不注册为 ctest。

产品代码四个 C 文件共 **132 行**（新增 93、删除 39）；另加 14 行英文，保守合计 **146 行 < 300**。四项各修自己的呈现根因：只改文案无法保留计算器逐字修改，也无法让日历底栏避开键盘；未引入公共框架或改公共输入组件。截图共 100 张、2,283,903 字节，最大 36,102 字节，均小于 300 KB。
