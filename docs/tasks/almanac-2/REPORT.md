# almanac-2 执行报告

基线：`ed4645db28e1339bca7338ca69e42990cf5a6f30`。分支：`claude/codex-almanac-2`。状态：进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 原文与逐日对照 | 【实测】通过 | `--check` 通过；36,525 天全部字段一致 | `ctest --test-dir build -R almanac-oracle --output-on-failure` |
| 2 固定 50 天与反向对照 | 【实测】通过 | 50 天通过；仅篡改 2026-02-17 的宜，原测试退出 1 | `ctest --test-dir build -R 'almanac-(fixtures\|reverse)' --output-on-failure` |
| 3 cnlunar 对照 | 【实测】已列差异 | 50/50 天有差异，完整表见 [CNLUNAR-DIFF.md](CNLUNAR-DIFF.md) | `python -B tests/almanac/run.py --binary build/almanac-model --mode compare --cnlunar "$INK_DEPS/../ink-hw/upstream/oss/cnlunar" --out docs/tasks/almanac-2` |
| 4 现有农历逐日对照 | 【实测】已列差异 | 36,495 天一致，30 天不一致，全部日期见 [LUNAR-DIFF.md](LUNAR-DIFF.md)；没有修改旧实现 | `python -B tests/almanac/run.py --binary build/almanac-model --mode oracle --out docs/tasks/almanac-2` |
| 5 AI、孩子、范围与时区 | 【实测】通过 | 模拟器 216 项检查；AI 与 C 接口全部字段相同，孩子可调、不确认，跨时区切换当天，1999 年如实报不在范围 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build -R almanac-capability --output-on-failure` |
| 6 构建、全量、ASan、S31、资源 | 【实测】部分通过；预算与全量未通过，内存及 S31 宿主测试进行中 | 普通构建、S31 固件、GCC 和本任务 4 个测试通过；完整全量有 2 个失败，详见下文 | `taskpolicy -c utility ctest --test-dir build --output-on-failure -j2` |

## 没做到的

固件增量的 60 KB 上限未达到：实际新增 **181,984 B（177.72 KiB）**。按任务书“超了停下写清，不砍内容”停止修改实现；完整模型和数据保留在本分支，未擅自压缩、裁剪或放宽预算。仍完成任务书要求的剩余测试和报告。测试结果不等于这一条验收通过。

没有实机测试，S31 上的真实查询耗时待主会话核对。

全量测试发现 `services-mid-capability-doc` 失败：服务设计 §1.3 尚未登记新能力。该设计文件不在本任务允许修改范围，没有越界修改或跳过检查；具体同步补丁见“要改的文档”。

完整全量的 `backup-restore-recovery` 在 240.26 秒被原有 240 秒上限终止；已完成的备份/恢复子场景没有失败断言，最后日志到 `collect-cut PASS`，留下的 `collect-resume` 指标为 `test.pass=276/test.fail=0/test.finished=1`。不能凭这些中间结果认定整个测试通过或断定根因，本次未改备份模块及超时上限。

## 和设计不一致的

没有改规则或旧农历。系统时区代替上游固定北京时间，是本任务明确要求的差异，见 [SOURCE.md](../../../system/services/time/vendor/mogo-calendar/SOURCE.md)。预算超限见上，不改任务书迁就实现。

## 改过的已有测试

仅在 `tests/integration/ai_surface.c` 的 `FOR_AI` 表末尾追加 `{"time.almanac", ""}`：该能力给 AI，日期参数全为可选。没有改已有期望、断言或预算；没有在 `NOT_FOR_AI` 重复登记。

## 要用户定的

完整数据与 60 KB 增量预算不能同时达到。实际增量为 181,984 B；是否调整预算或另开数据压缩任务，由主会话决定。本次没有默认提高上限。

`calendar-3` 是否替换旧农历：2097-08-07 至 2097-09-05 共 30 天不同，参见完整日期表；本次保留旧实现。

## 顺手发现

【实测】`asset_pack` 当前 `font_data` 为 4,718,343 / 4,718,592 B，仅余 249 B；本次相对基线增长 513 B。未改分区、裁剪字体或扩大上限，后续加字需留意该现有限制。

## 新增的机制

任务书要求：离线静态历法表、只读 `time.almanac` 能力。没有新增任务、线程、状态机、持久格式或配置项。

## 要改的文档

已按任务书明确许可生成并提交 `docs/system/capabilities.md`。

主会话需在 [服务设计](../../system/system-services-design.md) §1.3 的“时间”行 AI 能力列补 `time.almanac`，并注明日期可选、当地公历 YYYY-MM-DD、2000–2099、只读、孩子可用。已准备 [SERVICE-DESIGN.patch](SERVICE-DESIGN.patch)，没有应用到共享文档。

【实测】原设计的检查输出 `FOR_AI: checked=148 missing=1` / `FOR_AI: time.almanac`。仅在临时副本应用上述一行补丁，再以原检查器 `tests/services_mid/check_capability_doc.py --design work/almanac-2/system-services-design-proposed.md` 核对，输出 `missing=0` 并通过；没有修改测试、断言或默认检查对象。这是定位证据，不能替代仓库原文件的全量测试结果。

## 接口与证据边界

【实现】给 `lock-styles-1` / `calendar-3`：包含 `ink/sys/almanac.h`，调用 `ink_almanac_date(year, month, day, &out)`；有效公历日期为 2000-01-01～2099-12-31。返回值为 `false` 时输出清零。`ink_almanac_t` 包含 Mogo 全部 24 个原字段，再补年/月/日干支名称、完整农历文字、当天节气；负月份代表闰月，非节气日为空串。结构由调用者持有，字符串指向只读表，不释放、不修改。

【实现】AI / 其它服务走 `time.almanac`，参数 `{ "date": "2026-02-17" }`；`{}` 或不传参数时使用系统时区的今天。只读、离线、孩子可用；成功的 JSON 给完整宜忌等数据，`say` 只摘要第一项并说明民俗来源。日期越界返回 `INK_RC_INVALID` 与“不在范围”，时钟未校准的默认查询返回失败。没有页面变更。

【实现】边界审视（R-061）：原文模型只算数据；适配层使用现有时区、PSRAM 分配和软总线回执；UI 与 AI 共用 C 数据和能力。没有跨成员私有数据、没有新状态或事件，没有绕过策略关口。

【实测】宿主机轮换日期查询 1,000,000 次，平均 531.0 ns/次；这是 Mac 宿主测试，不代表 S31 真机耗时。命令：`taskpolicy -c utility build/almanac-model --benchmark`。

## 改前改后输出

基线 `time_caps.c` 未登记 `time.almanac`【实现】；以下改后输出均来自 `INK_SRC_AI` 经真实能力策略和执行器的模拟器运行【实测】，JSON 的全部字段已逐项与 C 接口比较。

| 调用 | 改前 | 改后 |
| --- | --- | --- |
| 今天，UTC（模拟时钟 2026-02-16 20:00 UTC） | 未提供该能力 | 今天农历腊月廿九，宜祭祀，忌开市。这些是民俗说法。 |
| 同一时刻的今天，上海时区 | 未提供该能力 | 今天农历正月初一，宜祭祀，忌嫁娶。这些是民俗说法。 |
| `2026-02-17` | 未提供该能力 | 这天农历正月初一，宜祭祀，忌嫁娶。这些是民俗说法。 |
| `1999-12-31` | 未提供该能力 | `INK_RC_INVALID`，不在范围（2000—2099 年） |
| 孩子查 `2026-02-17` | 未提供该能力 | 同成人结果，无确认卡 |

## 资源与规模

| 指标 | 【实测】结果 |
| --- | --- |
| S31 应用镜像 | 基线 4,316,128 B → 本任务 4,498,112 B；增量 181,984 B |
| 链接后的表结构 | 45,348 B，全部在 `.flash.rodata` |
| 表指向的字符串地址并集（含 NUL） | 123,887 B，全部在 `.flash.rodata`，包含与其它模块共享的字符串 |
| Flash 表与字符串地址并集 | 169,235 B（165.27 KiB）；是实际占用范围，并非镜像的净增量 |
| 交叉编译对象 `.rodata*` | 173,861 B；链接时合并重复字符串后以上一行 ELF 测量为准 |
| 静态可写内存 | 前后相同：DIRAM `.bss` 44,408 B、`.data` 26,392 B；PSRAM `.bss` 776,264 B。查询 JSON 临时用 4,096 B PSRAM |
| 胶水产品代码 | 140 行：`almanac.c` 101、头文件 30、登记 5、主 CMake 3、S31 CMake 1；少于 250 行。原文、生成表、测试、文档和译文数据不计；新增译文 9 行 |

为什么不能更小：适配必须给出全部原字段、干支/节气文字、可校验的日期参数、时区默认值及完整 JSON/短回执；101 行集中在一个适配文件。看过直接调用 `calendar_from_utc` 和只返回宜忌的更小方案，前者固定北京时区且拒绝 2000–2023 年，后者丢合同字段，均不采用。没有另造历法算法。

Flash 复核：使用 ESP-IDF 的 Python（含 pyelftools）运行 `python -B docs/tasks/almanac-2/measure_flash.py --before work/almanac-2/s31-before.bin`；它按 ELF 指针取字符串并做地址区间并集，检查对象与字符串均在 Flash。基线源码与指定提交一致，首次基线构建只有报告提交，无产品代码改动；前后均运行原 `ports/esp32s31/build.sh`。构建产物留在忽略目录，没有提交。

【实测】`taskpolicy -c utility bash ports/esp32s31/build.sh` 前后两次退出 0，无新增编译器警告。`taskpolicy -c utility python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/services/time/almanac.c system/services/time/time_caps.c system/services/time/vendor/mogo-calendar/main/time/calendar.c tests/almanac/model.c tests/almanac/device.c tests/integration/ai_surface.c` 输出：`gcccheck: 6 compiled, 0 with findings`。

最早一次 S31 配置因本工作区尚无宿主字体资源失败；补齐任务指定依赖中的构建资源后，基线与改后完整脚本均成功。宿主全构建有上游 SDL 的 macOS 废弃 API 警告及既有重复库链接警告；没有修改上游依赖。Linux 全量未运行，GCC 核查不能冒充 Linux CI 结果。

## 上机核对清单

- 本任务未烧录、未开串口。预算问题处理后，由主会话核对设备能力清单和显式 `2026-02-17` 的完整输出，期望与固定夹具一致，成人和孩子均无需确认。
- 读取设备当前时间、系统时区，核对无日期查询的当地日期；未校准时应如实提示校准。
- 真机测量单日查询耗时和查询前后 PSRAM 回收；本次仅有宿主耗时与编译内存布局证据。

## 构建与测试记录

- 【实测】普通 `INK_WERROR=ON` 全构建退出 0；全量原话为 `99% tests passed, 2 tests failed out of 443`，耗时 1822.90 秒，退出 8。失败：`backup-restore-recovery (Timeout)`、`services-mid-capability-doc (Failed)`。
- 【实测】原有 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 跳过（未配置中枢 Python 依赖），`memo-save-busy` 原本禁用；本次没有改这些设置或跳过规则。
- 【实测】最后一次新测试运行：`100% tests passed out of 4`，18.04 秒；覆盖固定 50 天、全量 36,525 天、宜忌反向对照和真实能力调用。没有 node 的单独核查显示 `almanac-oracle (Skipped)` 并输出缺少 node 的原因；固定夹具测试仍可独立运行。
- ASan、S31 宿主机测试：进行中。
