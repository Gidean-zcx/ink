# almanac-2 执行报告

基线：`ed4645db28e1339bca7338ca69e42990cf5a6f30`。分支：`claude/codex-almanac-2`。状态：本地交付与核验完成，验收未全通过；未推送、未操作设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 原文与逐日对照 | 【实测】通过 | `--check` 通过；36,525 天全部字段一致 | `ctest --test-dir build -R almanac-oracle --output-on-failure` |
| 2 固定 50 天与反向对照 | 【实测】通过 | 50 天通过；仅篡改 2026-02-17 的宜，原测试退出 1 | `ctest --test-dir build -R 'almanac-(fixtures\|reverse)' --output-on-failure` |
| 3 cnlunar 对照 | 【实测】已列差异 | 50/50 天有差异，完整表见 [CNLUNAR-DIFF.md](CNLUNAR-DIFF.md) | `python -B tests/almanac/run.py --binary build/almanac-model --mode compare --cnlunar "$INK_DEPS/../ink-hw/upstream/oss/cnlunar" --out docs/tasks/almanac-2` |
| 4 现有农历逐日对照 | 【实测】已列差异 | 36,495 天一致，30 天不一致，全部日期见 [LUNAR-DIFF.md](LUNAR-DIFF.md)；没有修改旧实现 | `python -B tests/almanac/run.py --binary build/almanac-model --mode oracle --out docs/tasks/almanac-2` |
| 5 AI、孩子、范围与时区 | 【实测】通过 | 模拟器 216 项检查；AI 与 C 接口全部字段相同，孩子可调、不确认，跨时区切换当天，1999 年如实报不在范围 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build -R almanac-capability --output-on-failure` |
| 6 构建、全量、ASan、S31、资源 | 【实测】部分通过；预算与完整全量未通过 | 普通构建、S31 固件、GCC、本任务普通与 ASan 各 4 个测试、S31 宿主 78 项通过；整轮全量有 2 个失败（备份恢复随后隔离通过），详见下文 | `taskpolicy -c utility ctest --test-dir build --output-on-failure -j2` |

## 没做到的

固件增量的 60 KB 上限未达到：实际新增 **181,984 B（177.72 KiB）**。按任务书“超了停下写清，不砍内容”停止修改产品实现；完整模型和数据保留在本分支，未擅自压缩、裁剪或放宽预算。此后只完成约定核验、测试日志限长和报告。测试结果不等于这一条验收通过。

没有实机测试，S31 上的真实查询耗时待主会话核对。

全量测试发现 `services-mid-capability-doc` 失败：服务设计 §1.3 尚未登记新能力。该设计文件不在本任务允许修改范围，没有越界修改或跳过检查；具体同步补丁见“要改的文档”。

完整全量的 `backup-restore-recovery` 在 240.26 秒被原有 240 秒上限终止；已完成的备份/恢复子场景没有失败断言，最后日志到 `collect-cut PASS`，留下的 `collect-resume` 指标为 `test.pass=276/test.fail=0/test.finished=1`。不能凭这些中间结果认定整个测试通过或断定根因，本次未改备份模块及超时上限。

【实测】随后按同一 240 秒上限隔离复跑原测试，50.99 秒通过（退出 0）。未稳定复现超时，尚不能断定根因；这次隔离通过不替代上面失败的整轮全量结果。

## 和设计不一致的

没有改规则或旧农历。系统时区代替上游固定北京时间，是本任务明确要求的差异，见 [SOURCE.md](../../../system/services/time/vendor/mogo-calendar/SOURCE.md)。预算超限见上，不改任务书迁就实现。

现有服务设计 §1.3 缺少本任务新增的能力，暂未与实现同步；受本任务文件范围限制，只准备补丁，没有修改原设计或放宽检查。

## 改过的已有测试

仅在 `tests/integration/ai_surface.c` 的 `FOR_AI` 表末尾追加 `{"time.almanac", ""}`：该能力给 AI，日期参数全为可选。没有改已有期望、断言或预算；没有在 `NOT_FOR_AI` 重复登记。

## 要用户定的

本次按原文直接编译静态表，增量未达到 60 KB 预算。实际增量为 181,984 B；是否调整预算或另开数据压缩任务，由主会话决定。本次没有默认提高上限。

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
- 【实测】`INK_SANITIZE=ON`（AddressSanitizer + UndefinedBehaviorSanitizer），保留 `INK_WERROR=ON` 和原字体/依赖参数，构建目标 `almanac-model sim-almanac`；`taskpolicy -c utility ctest --test-dir build-asan -R '^almanac-' --output-on-failure -j2` 输出 `100% tests passed out of 4`，9.67 秒，退出 0，没有 sanitizer 报错。四项与普通新测试相同，均未跳过。
- 【实测】`taskpolicy -c utility ctest --test-dir build -R '^backup-restore-recovery$' --output-on-failure -j1`：`100% tests passed out of 1`，51.03 秒，退出 0；没有扩大 240 秒上限。
- 【实测】S31 宿主机独立工程：`taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"`；`taskpolicy -c utility cmake --build build-s31-host -j2`；`taskpolicy -c utility ctest --test-dir build-s31-host --output-on-failure -j2`。输出 `100% tests passed out of 78`，69.54 秒，退出 0。

## 最终检查

【实测】按总规则对基线至 HEAD 的完整提交差异扫描本机绝对路径和完整 MAC：零命中（`grep` 退出 1，表示无匹配）；`git diff --check` 通过。检查了差异统计：仅任务指定的代码、构建登记、AI 表、译文、生成能力表和本任务目录；没有构建产物、设计稿、超过 5 MB 的新文件或其它任务文件。

【实测】10 个原文文件与上游固定提交的 Git 对象逐字节一致，cnlunar 包与其指定提交一致；旧 `lunar.c` / `time_service.c` 未变；报告的本地链接有效，待同步设计补丁可正常 `git apply --check`。无产品功能在固件预算失败后继续扩展。

本地提交按里程碑保留；完整运行日志和固件测量产物在忽略目录 `work/almanac-2/`，未推送远端。

---

# 第二轮（收尾，2026-11-13）

上面是第一轮（v1）的原文，没改。下面是第二轮：合到主线 `claude/system-layer`（`d7c508f8`），把表压小，结果一字不变。未推送，没碰设备。

## 一句话结论

- 表压好了：历法表和代码在 S31 固件里只占 **43,856 B**（v1 是 181,984 B），36,525 天全部字段和 lunar-javascript 1.7.7 逐日一致，AI 能力的 JSON 和回执和 v1 逐字一样。
- **但整包固件预算没过**：本任务带进 8 个界面字库里原来没有的汉字（宜忌民俗老黄北南），界面字体多 5,922 B，而 font_data 只剩 1,483 B，于是整套界面粗体 `ui-2-53`（661,420 B）被挤进程序镜像，镜像实际多了 **706,912 B**，应用分区只剩 51,920 B（1%）。按任务书“超预算就停下”，这件事写进“要用户定”；我没有动字库工具、分区表，也没有改 v1 定好的回执文字。

## 验收表（第二轮）

| 编号 | 结论 | 证据 |
| --- | --- | --- |
| 1 结果一字不变 | 【实测】通过 | `almanac-oracle`：`--check` 通过、36,525 天全字段一致（本机有 node，真跑，没跳过）；`almanac-fixtures` 50 天通过（`fixtures.json` 没改）；`almanac-mogo-host`（Mogo 原文测试）通过；`almanac-reverse` 通过；手工反向见下；能力输出 7 行（含完整 JSON）和 v1 逐字节相同（`cmp` 无差异） |
| 2 体积 ≤ 61,440 B | 【实测】**没过** | 镜像 4,484,048 → 5,190,960 B，+706,912 B。其中界面粗体挤进镜像 +663,056 B，表和代码 +43,856 B。见“体积” |
| 3 合并 | 【实测】通过 | 冲突按任务书解（见“合并”）；`services-mid-capability-doc` 通过；`capabilities.md` 构建后用 `tools/capabilities.py` 重新生成（651 条） |
| 4 全量 | 【实测】455 个里 454 过、1 个失败 | 失败的是 `autotest-cap`（相机脚本“回到主屏”），主线原本就失败，和本任务无关，见“顺手发现”；`backup-restore-recovery` 这次 38.46 s 通过 |
| 5 ASan、S31 宿主、S31 构建、gcccheck | 【实测】通过 | ASan `almanac-*` 5/5；S31 宿主工程 78/78；`build.sh` 通过，日志有 `factory screen output layer @690a023…`、没有“没有屏幕输出层”，编译器警告前后都是 0（CMake 组件提示前后都是 6 条）；gcccheck 6 个文件 0 处问题 |
| 6 耗时、栈、胶水、刷屏 | 【实测】见“其它数字” | 宿主查一天约 350 ns；S31 栈 `calendar_date` 64 B；胶水 160 行；没有页面，刷屏 0 次 |
| 7 改过的已有测试 | 只有任务书列的三处 | 见“改过的已有测试” |

## 合并怎么解的

- `git fetch <v1 克隆> claude/codex-almanac-2:almanac-2-v1`，`git merge --no-ff`（`f8ff0b78`）。v1 拷原文的提交 `a5740efc` 留在历史里。
- 冲突 3 处（`CMakeLists.txt`、`tests/integration/ai_surface.c`、`assets/i18n/en.tsv`）加 `docs/system/capabilities.md`：
  - `CMakeLists.txt`：保留主线的链接行（带 `ink_resampler`），在它前面加 v1 的 `target_include_directories(ink_system PRIVATE system/services/time/vendor/mogo-calendar/main)`。
  - `ai_surface.c`：主线 FOR_AI 表全留，settings-1 那行之后加 `{"time.almanac", ""},`。
  - `en.tsv`：两边都留，本任务 9 行在末尾，没有重复键。
  - `capabilities.md`：取主线版本，构建后重新生成（`6b82af20`）。
  - `ports/.../ink_system/CMakeLists.txt` 自动合并。
- 服务设计 §1.3“时间”行只改 AI 能力那一格，在 `time.lunar`、之后加 `time.almanac`（用 v1 补丁里的原话）。v1 的 `SERVICE-DESIGN.patch` 已经打不上，留在目录里只作记录。
- 合完先编、先跑：`almanac-*` 4 个和 `services-mid-capability-doc` 全过，能力输出存下来当“改前”。

## 压缩怎么做的（规则不动）

文件和行号在 [SOURCE.md](../../../system/services/time/vendor/mogo-calendar/SOURCE.md)。要点：

- 宜忌：114 个词一张词表，每组一串 1 字节词号（`0xFF` 结尾，同样的组只存一次），每组一个 `uint16` 起点；3,600 组折成 720 组，键 `(月干支%12)*60+日干支`。
- 神煞：167 个词一张词表，存法同上，720 组的键不变；建除、值神、黄黑道存 1 字节编号。
- 月干支：不存 1,201 条变化点，由当月“节”的日子算。
- 农历月：每条 4 B（相对起点、年-1998、月），末尾一条哨兵，天数用下一条减。
- 生成器每次生成都自检：折叠后 720 组等于 3,600 组原值；36,525 天月干支公式等于 `getMonthInGanZhi()`；农历月首尾相接、农历年只在正月进位；词数 < 255、词号串 < 65,536 B；最长的宜、忌、吉神、凶煞（223、118、101、94 B 含结尾）放得进 `calendar.h` 的 232、128、112、104（大小从 `calendar.h` 里读）。
- `calendar.h` 四个列表字段改成 `char` 数组；`ink_almanac_t` 跟着改，`almanac.c` 用 `_Static_assert` 卡住两边一样大。`time.almanac` 把 Mogo 的结果、我们的结果和 JSON 放在一块临时 PSRAM 里，查完就还。
- 数据只在 `main/assets/calendar_data.h`：“磑”不在 GB2312，放进 `.c` 会让界面字库构建失败。

S31 链接后的表（`measure_flash.py`）：

| 表 | 字节 |
| --- | --- |
| `calendar_yiji_codes`（宜忌词号） | 12,508 |
| `calendar_folk_codes`（神煞词号） | 6,915 |
| `calendar_lunar_months` | 5,060 |
| `calendar_yiji_pairs`、`calendar_spirit_pairs` | 各 2,880 |
| `calendar_term_days` | 2,400 |
| `calendar_folk_words`、`calendar_yiji_words`（指针表） | 668、456 |
| 其余（建除、干支名、节气名、方位等） | 1,132 |
| 表结构合计 | 34,899 |
| 表指向的中文串（地址并集） | 3,020 |
| 表和串合计 | 37,919（v1 是 169,235） |
| 代码（`calendar.c`、`almanac.c` 留在固件里的函数） | 4,250 |

## 体积（前后各跑一次原来的 `ports/esp32s31/build.sh`）

| 项 | 改前（主线加任务书和对照表，`85fcc4ed`） | 改后（`f676283e`） | 差 |
| --- | --- | --- | --- |
| app 镜像（预算口径） | 4,484,048 B | 5,190,960 B | **+706,912 B** |
| 其中镜像里的 embedded 资源 | 778,158 B | 1,441,214 B | +663,056 B（`ui-2-53.inkf` 被挤进来） |
| 去掉字体挤占后的增量 | — | — | **+43,856 B**（≤ 61,440） |
| `idf.py size` Flash `.text` | 2,769,382 | 2,773,860 | +4,478 |
| `idf.py size` Flash `.rodata` | 1,584,848 | 2,287,288 | +702,440（减去字体 663,056 = 表和串 +39,384） |
| 内部 RAM（DIRAM `.bss` / `.data`） | 48,592 / 34,592 | 48,592 / 34,592 | 0 |
| PSRAM `.bss` | 803,752 | 803,752 | 0 |
| font_data | 4,717,109 / 4,718,592（余 1,483） | 4,061,507（`ui-2-53` 放不下被移走） | 界面字体需要 4,723,031，超 4,439 B |
| 应用分区余量 | 758,832 B（14%） | 51,920 B（1%，构建提示“快满了”） | — |

两种口径对得上：`idf.py size` 的 Flash 合计 +706,918 B，镜像 +706,912 B，差 6 B，没有对齐填充的问题。

**为什么字体会超**：`tools/fontgen/ui_fonts.py` 把 `system/services` 等目录下所有 `.c`（连注释）和全部译文里的汉字收进界面中号、粗号字集。用它同样的规则比较改前改后的源码，本任务新带进 8 个原来没有的字：

| 字 | 出自 |
| --- | --- |
| 宜、忌 | `almanac.c` 的回执格式、`en.tsv` 的键 |
| 民、俗 | `almanac.c`（“这些是民俗说法”）、`time_caps.c` 的能力标题、`en.tsv`、`calendar.c` 第 4 行注释 |
| 老、黄 | `time_caps.c` 的能力标题“老黄历（…）” |
| 北、南 | `calendar.c` 原文的财神方位、煞方位两个小表 |

8 个字在两套界面字体里共 +5,922 B（约 740 B 一个），font_data 只剩 1,483 B，最多再放 1 个多字。`asset_pack.py` 按文件名顺序装 font_data，装不下的那一套（`ui-2-53`）和它后面的全部进程序镜像。v1 当时只多 513 B，是因为那时主线上已有其中一部分字。

我能改的范围里压不下来：就算改掉能力标题（省 老黄）、再动 `calendar.c` 原文那两个方位表（省 北南，而且超出任务书列的可改行），宜忌民俗 4 个字仍要约 2.96 KB，还是比 1,483 B 多；这 4 个字在 v1 定好的回执和 JSON 里（验收要求逐字一致）。所以按任务书停下，写进“要用户定”。表的压缩本身已经做完；第 6 条的再压（农历月 1 字节、7 位词号、查 v1 表外 12.7 KB）对字体没有帮助，没有做。

## 手工反向对照

在临时副本里改生成器（不在仓库里）：

1. 折叠时所有月份都用子月的宜忌（第 39 行 `LunarUtil.JIA_ZI[month]` 改成 `JIA_ZI[0]`）：`node tools/generate_calendar_data.cjs --check` 退出 1，`Error: Calendar self-check: Yi/Ji pair 60 differs after folding`。
2. 折叠键改成只按日干支（第 46 行改成 `pairs[i%60]`）：`--check` 退出 1，同样报 pair 60。
3. 第 1 条再关掉自检一（第 47 行改成 `if (false) fail(...)`）硬生成：只剩 112 组宜忌；`--check` 退出 1，报 `Stale calendar data`；用这张表编出的模型跑 50 天固定样本退出 1，第一处报 2000-02-04 的宜、忌不同。

`almanac-reverse` 照旧：只把 2026-02-17 的宜改成“故意改错”，50 天测试退出 1 并报出这一天。另外，Mogo 原文测试（只改了字段类型的那份）在上游原文 `calendar.c` 上同样全过，说明它本身能当基准。

## 能力输出（改前改后，代替截图）

模拟时钟 2026-02-16 20:00 UTC，`INK_SRC_AI` 经真实能力策略和执行器。7 行输出（含完整 JSON）改前改后逐字节相同：

| 调用 | 回执 |
| --- | --- |
| 今天，UTC | 今天农历腊月廿九，宜祭祀，忌开市。这些是民俗说法。 |
| 今天，上海时区 | 今天农历正月初一，宜祭祀，忌嫁娶。这些是民俗说法。 |
| `2026-02-17` | 这天农历正月初一，宜祭祀，忌嫁娶。这些是民俗说法。 |
| `2000-01-01` | 这天农历冬月廿五，宜破屋，忌诸事不宜。这些是民俗说法。 |
| `2099-12-31` | 这天农历冬月二十，宜开市，忌嫁娶。这些是民俗说法。 |
| `1999-12-31` | `INK_RC_INVALID`，不在范围（2000—2099 年） |
| 孩子查 `2026-02-17` | 同成人，没有确认卡 |

没有页面：刷屏 0 次；默认字号、最大字号、孩子三套截图不适用，用上表代替。

## 对照表逐行（25 行）

照做 13 行（1–7、9–11、13、16、19），按设备改 5 行（8、14、15、17、18），不做 6 行（12 加载态不需要；20–24 归 `lock-styles-1`、`calendar-3`、设置和字库规划），不适用 1 行（25）。做了的 18 行都有测试：1–7、9–11、16、19 由 `almanac-oracle`、`almanac-fixtures`、`almanac-mogo-host` 和生成器自检覆盖；8、13–15、17、18 由 `almanac-capability`、`model.c` 的无效日期用例和 `services-mid-capability-doc` 覆盖。第 19 行的“表只在 `.h`”做到了，但 font_data 的结果见上（超了）。

## 其它数字

- 宿主查一天：【实测】1,000,000 次平均约 350 ns（`build/almanac-model --benchmark` 三次 346–354 ns）。同一台机器同样编译选项下 v1 是 145–164 ns（v1 报告的 531 ns 是当时满载下测的）。拼词让一次查询慢了约 0.2 µs，可以忽略。
- S31 `-fstack-usage`：`calendar_date` 64 B（加 `calendar_words` 32 B）；`ink_almanac_cap_run` 144 B，调 `almanac_fill` 64 B，Mogo 的结果放在 PSRAM，能力这条路最深约 300 B；对外的 `ink_almanac_date` 672 B（Mogo 的结果约 650 B 放在调用方栈上），固件里现在没人调它（链接时去掉了），给锁屏、日历用时注意。
- 胶水 160 行（预算 250）：`almanac.c` 117、`almanac.h` 34、`time_caps.c` 登记 5、主 `CMakeLists.txt` 产品行 3、S31 `CMakeLists.txt` 1。
- 不新增任务、线程、定时器、缓存、持久数据或配置项；内部 RAM 不变；查一次只用临时 PSRAM（约 5.4 KB，查完释放）。

## 改过的已有测试

只有任务书列的三处，都因为字段变成数组，严格程度不变：

- `tests/almanac/model.c`：无效日期时 `invalid.yi` 改成 `invalid.yi[0]`（原来查指针为空，现在查数组第一个字节为 0）。
- `tests/almanac/reverse.py`：定位行跟着新 `calendar.c` 改成 `out->guardian_type=calendar_folk_words[...]`，注入语句改成 `strcpy(out->yi,"故意改错")`；仍然只改 2026-02-17 的宜，仍然要求 50 天测试失败并报出这一天。
- `system/services/time/vendor/mogo-calendar/tools/calendar_host_test.py` 第 17 行：`Date._fields_` 四个字段改成 `c_char * N`。

新加：`tests/almanac/run_reader_baseline.py`（原文测试要的 `toolchain()` 垫片，没有 node 时退出 77）和 ctest `almanac-mogo-host`。`docs/tasks/almanac-2/measure_flash.py` 跟着新表名改，并加了代码字节数。其余测试一行没改。

## 要用户定

1. **界面字库放不下本任务的 8 个字（这次新出现，挡住了合并）。** 可选：
   - A. 先给 font_data 腾地方（backlog“字体分区快满了”那条的规划，比如界面中号、粗号字集不收注释和只给 AI 看的文字，或者把大号正文字体按 D-138 放卡上）。要改 `tools/fontgen/`，不在本任务范围。腾出约 6 KB，本任务就能原样合。
   - B. 把只给 AI 看的文字（回执、`folk_notice`、能力标题）和 `calendar.c` 里那两个方位小表挪进头文件，界面字库就不收这 8 个字。显示没问题：普通正文用的阅读字库（sans-44）有全部 GB2312；代价是改了原文里任务书没列的几行，而且这些文字离开了 `i18n-literals` 这类只看 `.c` 的检查。
   - C. 接受字体进镜像（+663 KB，应用分区只剩约 51 KB）。不建议。
   - D. 改短回执措辞，只用已有的字：和 v1 定好的回执不一样，“老黄历”也没法叫。
   - 我倾向 A（问题出在大家共用的字库规则，后面锁屏、日历显示宜忌时也会撞上）；急着合的话 B 最省事。
2. `calendar-3` 要不要换掉旧农历（2097-08-07～09-05 共 30 天不同）：v1 已列，本任务没改 `lunar.c`。

## 顺手发现

- `autotest-cap` 在主线上本来就失败：相机脚本 28 s 时检查“回到主屏”不过（`tests/device_autotest/sim.c:32`）。我把工作区切回改前的 `85fcc4ed` 只编 `sim-autotest` 单独跑两次，都是同样的失败；改后也一样。和本任务无关，可能和主线最近的相机取景改动（`01250c93`）有关，交给主会话进 backlog。
- `backup-restore-recovery` 这次整轮全量里 38.46 s 通过，没再超时。
- Mogo 的 `calendar_from_utc`、`calendar_month_name` 在固件里没人调，链接时被去掉了，不占地方。

## 给 lock-styles-1、calendar-3 的接口说明

- `#include "ink/sys/almanac.h"`，`ink_almanac_date(年, 月, 日, &out)`，范围 2000-01-01～2099-12-31，按调用方给的“当地公历日期”算；返回 `false` 时 `out` 清零。
- Mogo 的 24 个原字段都在；另有年/月/日干支名、完整农历文字 `lunar_text`、当天节气 `solar_term`（不是节气日为空串）；农历月为负数表示闰月。
- `yi`、`ji`、`auspicious`、`inauspicious` 是结构体里的字符串数组（232、128、112、104 B），其余字符串指向 flash 里的常量。结构体约 700 B，`ink_almanac_date` 自己还要约 670 B 栈：别在小栈上放很多个，万年历月格每格查一次时复用一个。
- 从 `ink_almanac_t` 填 Mogo 的 `calendar_date_t` 时，这四个字段要拷字符串，不能赋指针；Mogo `ui_calendar.c` 里 `const char *const words[]={d->yi,d->ji}` 这种用法不用改。
- “磑”（宜忌词“安碓磑”）不在 GB2312，系统界面字库和阅读字库都没有这个字；锁屏或日历要显示时自己处理（Mogo 是往它自己的字库补了这个字），本任务不改字库。
- 宜忌、神煞、方位是民俗说法，显示时带一句“这些是民俗说法”；时钟没校准时不给日期的查询返回失败，画面上照 Mogo 写“请校准时间”。
- 页面上显示宜忌这些字，同样会碰到“要用户定”第 1 条的字库问题。

## 第二轮的构建和测试记录

- 普通构建 `INK_WERROR=ON` 通过。`ctest -R '^almanac-|services-mid-capability-doc|i18n'` 16/16 通过。
- 全量（清掉 `build/test-out`、重新 configure 后，`ctest -j2`）：`99% tests passed, 1 tests failed out of 455`，`Total Test time (real) = 1135.72 sec`；失败 `autotest-cap`（主线原有，见上），`memo-save-busy` 原本禁用。
- ASan（`-DINK_SANITIZE=ON`，目标 `almanac-model sim-almanac`）：`ctest --test-dir build-asan -R '^almanac-'` 5/5 通过，没有 sanitizer 报错。`almanac-mogo-host` 用的是原文测试自己用本机 `cc` 编的库，不带 sanitizer。
- S31 宿主工程 `ports/esp32s31/tests`：78/78 通过。
- `ports/esp32s31/build.sh`（`INK_S31_BUILD=build-s31-own`）改前、改后各一次，都退出 0。
- gcccheck：`almanac.c`、`time_caps.c`、`calendar.c`、`tests/almanac/model.c`、`tests/almanac/device.c`、`tests/integration/ai_surface.c`，`6 compiled, 0 with findings`。
- 结束时删了 `build-asan`、`build-s31-own`、`build-s31-host`，留 `build`。

---

# 复核修复（2026-11-13 晚）

两位复核人各提了一条“挡合并”，说的是同一件事：本任务带进 8 个界面字库里原来没有的汉字，`font_data` 放不下，整套界面粗体 `ui-2-53` 进了程序镜像（+706,912 B，应用分区只剩 1%），在这件事解决前分支不能合。

## 结论

- **属实，我重新核过，数字一个不差。** 本轮没有改产品代码。
- **不在本分支修，原因是正确的修法已经单独立项：** 主会话在 `fe5938e3` 建了任务 [font-pack-1](../font-pack-1.md)（字形无损压缩；`split()` 放不下一个文件时，后面放得下的照样进 `font_data`），提交说明写明“这个先合”，backlog“字体分区快满了”那条也改成指向它。这就是“要用户定”第 1 条里的 A。font-pack-1 的验收要求 `font_data` 至少余 1 MB、再加 50 个没用过的字也全在 `font_data` 里；本任务这 8 个字共 5,922 B，到时放得下，本分支一行不用改。
- **所以合并顺序是：font-pack-1 先进主线 → 本分支合上主线 → 重跑一次 `build.sh`，确认镜像增量回到约 43,856 B（≤ 61,440）、所有字体都在 `font_data` → 再合。** 在那之前本分支不能合，这一点和两位复核人一致。

## 怎么核的

- 字：按 `tools/fontgen/ui_fonts.py` 同样的范围（`system/ui`、`system/apps`、`apps`、`system/services`、`system/policy` 下的 `.c`，加 `assets/i18n/*.tsv`）比较 `d7c508f8` 和 `HEAD`：新增正好 8 个字“俗北南宜忌民老黄”，没有减少的字；出处和第二轮那张表一样（宜忌：`almanac.c`、`en.tsv`；民俗：`almanac.c`、`time_caps.c`、`en.tsv`、`calendar.c` 注释；老黄：`time_caps.c` 能力标题；北南：`calendar.c` 两个方位表）。主线 `fe5938e3` 相对 `d7c508f8` 没有新增字，问题全部来自本任务。
- 宿主构建的字体：9 个字体文件合计 4,722,083 B，比 `font_data` 的 4,718,592 B 多 3,491 B（还没算打包头）；界面字形 971 个（改前 963）；`ui-2-53.inkf` 661,420 B。
- S31：重跑一次原来的 `ports/esp32s31/build.sh`（`INK_S31_BUILD=build-s31-own`），退出 0；日志里 `factory screen output layer @690a023` 出现 2 次，“没有屏幕输出层” 0 次，编译器警告 0 条。打包打印 `font_data 4061507/4718592; embedded 1441214/2097152 bytes`；`ink-s31.bin` 5,190,960 B，`Smallest app partition is 0x500000 bytes. 0xcad0 bytes (1%) free`，即余 51,920 B，构建提示“快满了”。和第二轮报告逐字节相同。

## 为什么不在本分支用别的办法压下去

- **B（把回执、标题、方位表挪进头文件，躲开字库扫描）**：`ui_fonts.py` 注释写明界面字库外的字由正文 `sans-44` 缩放顶上，不会变豆腐块；但宜忌这些字在粗体标题里会变成细体混排，而且只是让扫描看不见，问题原样留给 `lock-styles-1`、`calendar-3`——它们在页面上显示宜忌时还得把这些字加回字库，照样撞上。还要改原文里任务书没列的行。不做。
- **只改能力标题（省“老黄”）或原文方位表（省“北南”）**：剩下宜忌民俗 4 个字约 2.96 KB，仍大于剩余的 1,483 B，解决不了；“老黄历”这个名字也没了。不做。
- **C（接受字体进镜像）、D（改短回执）**：第二轮已说明不建议；D 还会让 v1 定好、验收要求逐字一致的回执变样。
- 第二轮“要用户定”第 1 条保留原文作记录；现在的实际状态是主会话已按 A 排了 font-pack-1。只有用户要求本任务赶在 font-pack-1 之前合，才需要在 B、C、D 里选。

## 本轮测试

- 重新 configure、构建（`INK_WERROR=ON`）通过。
- 相关测试 `ctest -R '^almanac-|services-mid-capability-doc|i18n|ui-font|font'`：`100% tests passed out of 18`。
- 全量（清掉 `build/test-out`、重新 configure，`ctest -j2`）：`99% tests passed, 1 tests failed out of 455`，`Total Test time (real) = 901.08 sec`。失败的是 `autotest-cap`，和第二轮一样是主线原有的相机脚本失败（第二轮已在改前的 `85fcc4ed` 上复现），和本任务无关；`memo-save-busy` 原本禁用。
- gcccheck（`almanac.c`、`time_caps.c`、`calendar.c`、`tests/almanac/model.c`、`tests/almanac/device.c`、`tests/integration/ai_surface.c`）：`6 compiled, 0 with findings`。本轮没有改任何 C 文件，没有改任何测试。
- S31 构建见上；结束时删了 `build-s31-own`（`build-asan` 本轮没建），留 `build`。
