# almanac-2（收尾）：Mogo 离线老黄历合到主线，固件增量从 181,984 B 压到 60 KB 以内，任何一天的结果都不变

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 1、2、6、7、9、11、12 条；第 8 条不碰设备）。

**D-140（总规则第 11 节）**：本任务是“搬代码”（Mogo 的离线历法模型）。原文整文件拷进来（第一版已经逐字节拷好，提交在 v1 分支里），`SOURCE.md` 写项目、固定提交、拷了哪些文件、改了哪些行和为什么，原文的测试一起带上；我们只写接线层（时间服务里的 C 接口、`time.almanac` 能力）。这次为了体积要改的，是**生成器输出的表格式和 `calendar.c` 里读表的几行**，规则和结果一点不动；每一处改动都按文件和行号记进 `SOURCE.md`。原文搬不动、或者压完还是超预算，就写证据、停下，写进“要用户定”，不悄悄换成自己写的历法。

## 派发前提（主会话检查）

- 没有前置任务。起步分支用当前主线 `claude/system-layer`（不早于 `d7c508f8`；`main` 的 `cfd9c26c` 已经在里面）。
- 第一版的成果在单独的克隆 `$INK_DEPS/../ink-codex/almanac-2/repo`，分支 `claude/codex-almanac-2`（基线 `ed4645db`，5 个提交 `fbfe7998`…`684a0bdc`，报告在该分支的 `docs/tasks/almanac-2/REPORT.md`，用 `git -C <那个克隆> show claude/codex-almanac-2:<路径>` 读）。
- 派之前 `df -h /System/Volumes/Data`，留够 10 GB；第一版克隆的编译产物已经清过，S31 构建要重新编。

## 为什么

用户 2026-11-12 让做老黄历宜忌，D-129 定了用最成熟的整块搬：Mogo 锁屏的“老黄历”用 lunar-javascript 的默认规则离线生成 C 静态表，固件里不跑 JavaScript。它同时给锁屏样式（`lock-styles-1`，已在等）和新日历（`calendar-3`）用。第一版功能和核对都做完了，但固件多了 181,984 B（预算 60 KB），按任务书停了没合；`docs/tasks/backlog.md` 记着“黄历（almanac-2）没合：大头是 12 万字节中文宜忌文字，先压缩再合”。D-140 把“先补完 almanac-2”列为第一批要做的事。

## 现状（读代码和 v1 分支得到的，2026-11-13）

**v1 做到了什么**（v1 报告【实测】）：10 个原文文件拷进 `system/services/time/vendor/mogo-calendar/`，和上游固定提交逐字节一致；`system/services/time/almanac.c`（101 行）、`system/include/ink/sys/almanac.h`（30 行）、`time_caps.c` 登记 5 行；测试 `tests/almanac/`（`model.c`、`run.py`、`reverse.py`、`device.c/.py`、`fixtures.json`、`generate_fixtures.cjs`、`scenario.script`），ctest 里有 `almanac-fixtures`、`almanac-oracle`（要 node，没有就返回 77 跳过）、`almanac-reverse`、`almanac-capability`。36,525 天全部字段和 lunar-javascript 一致；50 天固定样本、反向对照、能力调用（AI、孩子、跨时区、范围外）都过；cnlunar 差异表 `CNLUNAR-DIFF.md`，旧农历差异表 `LUNAR-DIFF.md`（2097-08-07～09-05 共 30 天不同）。

**v1 没过的三条**：
1. 固件增量 181,984 B（基线镜像 4,316,128 B → 4,498,112 B）。其中表结构 45,348 B、表指向的字符串 123,887 B，全在 `.flash.rodata`。
2. `services-mid-capability-doc` 失败：`docs/system/system-services-design.md` §1.3“时间”一行没有 `time.almanac`。v1 带的 `SERVICE-DESIGN.patch` 在当前主线**已经打不上**（`git apply --check` 失败：那一行现在在第 74 行，上下文变了），但这一行的内容没变，手工加即可。
3. 整轮全量里 `backup-restore-recovery` 一次超过它原有的 240 s 上限（`CMakeLists.txt:740`），隔离复跑 51 s 通过。不是本任务的代码路径，也不在 backlog 里。

**字节花在哪**（读 v1 的 `main/assets/calendar_data.h`【实测】）：Mogo 的表**本来就不是逐日表**，而是按规则查的：`calendar_yiji_pairs[3600][2]`（月干支×日干支）、`calendar_spirit_pairs[720][2]`（月支×日干支）、`calendar_day_roles[144][3]`、`calendar_lunar_months` 1,264 条、`calendar_month_cycles` 1,201 条、`calendar_term_days[100][24]`。真正占地方的是把每一组宜忌、神煞**整串存成中文字符串**：`calendar_yiji` 1,231 串共 81,777 B，`calendar_folk` 1,066 串共 42,122 B，合计约 124 KB，和 v1 测出的 123,887 B 对得上。而这些串里只有 **114 个不同的宜忌词**（858 B）和 **167 个不同的神煞词**（1,178 B），一共只引用 11,277 次和 5,897 次。所以“逐日表”不用重做；要压的是“同一批词反复整串存”。

**表里还能省的三处冗余**（拿已提交的表逐项核对【实测】）：
- 宜忌其实只看**月支**不看月干：3,600 组里，`pair[m*60+d]` 和 `pair[(m%12)*60+d]` 在全部 60×60 组合上 0 处不同，可以折成 720 组（Mogo 的神煞表本来就是 720 组）。
- 月干支可以由当月的“节”那天算出来：`月干支 = (12 + (年-2000)*12 + (月-1) + (日 >= calendar_term_days[年-2000][(月-1)*2])) % 60`，2000–2099 全部 36,525 天和 `calendar_month_cycles` 0 处不同，这张表可以不要（也正好是 `CALENDAR.md` 说的“节气当天整日按新月份”）。
- 农历月首尾相接（每月天数 = 下一月起点 − 本月起点），农历年恰好在正月（非闰）加一；每条可以从 8 B 压到 4 B。

**原型**（宿主机 arm64，在临时目录里做的【实测】）：用这三条加“词表 + 每组一串 1 字节词号”的格式，写了一个读表函数，和原文 `calendar.c` 在 1999–2100 年、月 0–13、日 0–32 的全部组合上逐字段比较：36,525 个有效日期 0 处不同，无效日期判断也一样。目标文件从 182,400 B 降到 43,976 B（宿主机指针 8 B；S31 是 32 位，会更小）。据此 S31 固件增量预计约 45–50 KB【估算】，要以实测为准。

**lunar-javascript 本身**：Mogo 带的是 1.7.7（`tools/vendor/lunar-javascript/lunar.cjs`，SHA-256 `9750324b…`），`ink-hw/upstream/oss/lunar-javascript`（`5c92e62`）是 1.7.8。只用 Mogo 带的 1.7.7，不换版本。它的 `getDayYi`/`getDayJi`（`lunar.cjs:5149–5221`）解析 34,151 B 的十六进制串 `DAY_YI_JI`，`getDayJiShen`/`getDayXiongSha`（`:5222–5271`）解析 17,392 B 的 `DAY_SHEN_SHA`。不选“把这两个 JS 函数和原串改写成 C”：光十六进制原串就有 51 KB，而且那是重写 JS，不是搬代码。保留 Mogo 的做法：宿主机生成器调 lunar-javascript，固件只编静态表。

**字库（硬约束）**：`tools/fontgen/ui_fonts.py:33–37` 会把 `system/services/` 等目录下**所有 `*.c`** 里的汉字收进界面字库（font_data），而且要求这些字都在 GB2312 里，否则构建失败（“UI text outside the reading font”）。宜忌词里有 GB2312 以外的“磑”（“安碓磑”，lunar.cjs:7598），一共 14 处。所以数据必须留在 `.h` 里（现在的 `main/assets/calendar_data.h`），**不能**生成成 `.c`，也不能把词表写进 `almanac.c`。font_data 在 Mac 上只剩约 1.5 KB（backlog“字体分区快满了”），v1 因为 `calendar.c`、`almanac.c` 和译文里的字长了 513 B；超了的话放不下的字体会不压缩地进程序镜像（每套约 +485 KB），那就整个超预算。

**当前主线上要接的地方**（逐个核对过）：
- `system/services/time/time_caps.c` 从 `ed4645db` 到现在没改过：`TIME_CAPS[]` 在 `:349`，`time.now` 在 `:350`，`time.lunar` 在 `:358`，`ink_time_caps_init` 在 `:418`。主线上还没有 `almanac*`、`vendor/mogo-calendar/`。
- `CMakeLists.txt`：`ink_system` 的源文件列表里 `lunar.c` 在 `:238`、`time_service.c` 在 `:239`；`ink_system` 的链接行现在多了 `ink_resampler`。
- `ports/esp32s31/components/ink_system/CMakeLists.txt:9` 是 `PRIV_INCLUDE_DIRS`。
- 试合并（临时克隆里 `git merge-tree`【实测】）：4 处冲突，全是“两边都在末尾加行”——`CMakeLists.txt`（include 行和新的链接行）、`tests/integration/ai_surface.c`（FOR_AI 表末尾，主线加了 notify 和 settings-1 的行）、`assets/i18n/en.tsv`（末尾）、`docs/system/capabilities.md`（重新生成）。`ports/.../CMakeLists.txt` 自动合并。v1 新加的 9 条译文在当前 `en.tsv` 里都没有重复。
- `lock-styles-1` 写的是“`calendar_date_t` 用 `ink_almanac_date()` 的结果填，类型来自 almanac-2 带进来的 `calendar.h`；压缩后接口变了跟着它走”。`calendar-3` 只通过自己的包装函数调 `ink_almanac_date()`。

## 来源（搬代码）

- **Mogo**：`$INK_DEPS/../ink-hw/upstream/oss/mogo-e4p`，提交 `864f2f9dc75980a67d2d6b67b17365020b7a50ce`。已经拷好的 10 个文件（v1 的 `SOURCE.md` 有每个文件的 SHA-256）：`main/time/calendar.h`、`main/time/calendar.c`、`main/assets/calendar_data.h`、`tools/generate_calendar_data.cjs`、`tools/calendar_host_test.py`、`tools/vendor/lunar-javascript/lunar.cjs` 和同目录 `LICENSE`、`docs/development/guides/CALENDAR.md`、根 `LICENSE`、`licenses/kiikoread-Personal-Study-1.0.txt`。
- **这次要改的原文**（其余文件一字不动）：
  - `tools/generate_calendar_data.cjs`：第 19 行（农历月记录）、24–26 行（宜忌组）、29–31 行（神煞组）、44 行（月干支变化点）、54–66 行（输出段），改成下面“压缩格式”，并加生成时的自检。
  - `main/time/calendar.h`：`calendar_date_t` 里 `yi`、`ji`、`auspicious`、`inauspicious` 四个字段从 `const char *` 改成定长 `char` 数组（其余字段不变）。
  - `main/time/calendar.c`：第 11–12 行（两个表结构的 typedef）、45–53 行（农历月查找和月干支）、54–55 行（宜忌）、63–67 行（神煞、建除、值神），按新表读；其余函数（`civil_serial`、`calendar_from_utc`、`calendar_lunar_text`、`calendar_term`、`calendar_cycle_name`、`calendar_month_name`）不改。
  - `main/assets/calendar_data.h`：由改后的生成器重新生成，不手改。
  - `tools/calendar_host_test.py`：只改 ctypes `Date` 的四个字段类型（跟着 `calendar.h`），再加一个同目录外的垫片让它能跑（见“要做的”第 4 条）。
- **怎么编**：都是 C11，宿主机和 ESP-IDF 直接编，不需要 C++、Arduino、Furi 或 LVGL 垫片；生成器只在宿主机用 node 跑。
- **对照**：cnlunar，`$INK_DEPS/../ink-hw/upstream/oss/cnlunar`，提交 `3f674fe`（Python，MIT），只对照，不搬。
- **许可**：Mogo 的 C 文件和生成器是 `LicenseRef-Kiikoread-Personal-Study-1.0`，lunar-javascript 是 MIT。D-129 不看许可，照原样带上，`SOURCE.md` 写清，公开发布时由主会话处理。

## 要做的

1. **合到主线**（先合再改，这样“原文逐字节一致”的那个提交留在历史里，后面的压缩改动是一个能看清的差异）：
   - 在新工作区从 `claude/system-layer` 开分支，`git fetch <v1 克隆> claude/codex-almanac-2:almanac-2-v1`，`git merge --no-ff almanac-2-v1`。
   - 解冲突：`CMakeLists.txt` 保留主线的链接行（带 `ink_resampler`），再加 v1 的 `target_include_directories(ink_system PRIVATE system/services/time/vendor/mogo-calendar/main)`；`ai_surface.c` 保留主线全部行，在 FOR_AI 表末尾（settings-1 那行之后）加 `{"time.almanac", ""},`；`en.tsv` 两边都留，本任务的 9 行放末尾；`capabilities.md` 取主线版本，构建后用 `python3 tools/capabilities.py` 重新生成。
   - `docs/system/system-services-design.md` §1.3“时间”行（当前第 74 行）的 AI 能力列，在 `time.lunar`、之后手工加 `time.almanac`（date 可选，YYYY-MM-DD；不给就按系统时区查今天；2000–2099；孩子可用；只读民俗数据）。只改这一格。
   - 合完先编一次、跑 `almanac-*` 四个测试和 `services-mid-capability-doc`，确认和 v1 一样再往下改。
2. **压缩数据（只改表的格式，不改规则）**，都在生成器里做，`calendar_data.h` 由它生成：
   - 宜忌：一张词表（114 个词）＋每组一串 1 字节词号（以 `0xFF` 结尾）＋每组的 `uint16` 起点；`calendar_yiji_pairs` 折成 `[720][2]`，键是 `(月干支%12)*60+日干支`。
   - 神煞：同样做成词表＋词号串＋起点，`calendar_spirit_pairs[720][2]` 键不变；建除、值神、黄黑道（`calendar_day_roles[144][3]`）存词表里的 1 字节编号。
   - 月干支：去掉 `calendar_month_cycles`，按上面的公式由 `calendar_term_days` 算。
   - 农历月：每条 `{uint16 相对起点, uint8 年-1998, int8 月}`，末尾加一条哨兵，天数用下一条减本条；二分查找不变。
   - 生成器自检（不满足就报错退出）：折叠后 3,600 组宜忌和折叠前逐组相同；2000–2099 每天的月干支公式和 `getMonthInGanZhi()` 一致；农历月首尾相接、农历年只在正月进位；词数小于 255、词号串小于 65,536 B；拼出来最长的宜、忌、吉神、凶煞不超过 `calendar.h` 里四个数组的大小（原型实测最长分别是 223、118、101、94 B 含结尾，数组可以取 232、128、112、104）。
   - `--check`（表是不是最新）和 `--oracle <文件>`（逐日对照数据）照旧可用；`--oracle` 的每一行仍直接来自 lunar-javascript 的 getter，和表格式无关。
   - 数据只放在 `main/assets/calendar_data.h`，不放进 `.c`、不放进 font_data、不放到卡上。
3. **接口跟着改**：
   - `ink_almanac_t`（`system/include/ink/sys/almanac.h`）的 `yi`、`ji`、`auspicious`、`inauspicious` 改成和 `calendar_date_t` 同样大小的 `char` 数组，`almanac.c` 用 `_Static_assert` 保证两边大小一致；其余字段、函数名、`false` 时清零的约定、`time.almanac` 的参数和返回 JSON 一律不变。
   - `ink_almanac_t` 变成约 700 B：`ink_almanac_cap_run` 不放在栈上，和 JSON 缓冲一起从 PSRAM 一次分配（v1 已经从 PSRAM 拿 4,096 B）。
   - 数组在表达式里照样当 `const char *` 用，Mogo 的 `ui_calendar.c`（`const char *const words[]={d->yi,d->ji}`）不用改；给 `lock-styles-1` 的说明里写清：从 `ink_almanac_t` 填 `calendar_date_t` 时四个字段要拷字符串，不能赋指针。
4. **原文的测试一起带上**：Mogo 的 `tools/calendar_host_test.py` 除了逐日比较，还查春节、闰月、2026 年节气、北京时间零点和范围边界。v1 只留了文件没跑。这次加一个 `tests/almanac/run_reader_baseline.py` 垫片（只提供它要的 `toolchain(work)`，返回宿主机编译器），新增 ctest `almanac-mogo-host` 用 `PYTHONPATH` 指到垫片目录去跑原文测试；没有 node 时返回 77 跳过并打印原因（和 `almanac-oracle` 一样）。原文只改 `Date._fields_` 那一行里四个字段的类型。
5. **体积怎么量**：前后各跑一次原来的 `ports/esp32s31/build.sh`，写 app 镜像差（这是预算口径）；同时用 v1 的 `docs/tasks/almanac-2/measure_flash.py` 和 ESP-IDF 的 `idf.py size` 写各段的差（表、字符串、代码分开）。两种口径差超过 4 KB 要说明原因（多半是镜像里 64 KB 页对齐的填充）。再写 font_data 前后各多少字节、应用分区还剩多少。
6. **还超 60 KB 时按这个顺序再压**，每一步都要 oracle 全过：农历月改成每月 1 字节＋每年一个起点（约再省 3.7 KB）；宜忌词号改成 7 位打包（约 1.5 KB）；查出 v1 里表以外那约 12.7 KB（181,984 − 169,235）是什么（`riscv32-esp-elf-nm --size-sort` 或 map 文件），能去掉就去掉。都做完还超，停下写进“要用户定”（可选项：放宽预算；或者按 D-138 把规则表放到卡上，经 `auto-provision-1` 的清单自动补，缺了在界面上写清）。不删字段、不砍年份范围。
7. **`SOURCE.md` 改写**：项目、固定提交、10 个文件各自的原始 SHA-256 和现在的 SHA-256；改了哪些文件哪几行（上面“这次要改的原文”逐条写），每条写为什么（v1 实测 181,984 B 超 60 KB 预算，规则和结果不变，靠哪项检查证明）；生成器新增的自检；时区和范围的差异照 v1 原样保留（不调 `calendar_from_utc`，按系统时区的当天，2000–2099）；`calendar_host_test.py` 的字段改动和垫片；“磑”不在 GB2312、数据留在 `.h` 的原因。
8. **报告**：在 v1 的 `docs/tasks/almanac-2/REPORT.md` 上续写（保留 v1 的内容，标明哪段是第二轮），写清合并时怎么解的冲突、体积前后、改过的测试。

## 范围和预算

- 只改：`system/services/time/vendor/mogo-calendar/`（上面列的几个文件和 `SOURCE.md`）、`system/services/time/almanac.c`、`system/include/ink/sys/almanac.h`、`system/services/time/time_caps.c` 里登记能力的那几行（随合并进来）、`CMakeLists.txt` 和 `ports/esp32s31/components/ink_system/CMakeLists.txt` 里本任务的行、`tests/almanac/`、`tests/integration/ai_surface.c` 的一行、`assets/i18n/en.tsv` 末尾本任务的行、`docs/system/capabilities.md`（重新生成）、`docs/system/system-services-design.md` §1.3 时间行那一格、`docs/tasks/almanac-2/`。
- 不碰：`system/services/time/lunar.c`、`time_service.c`；`system/apps/clock/`、日历应用（`calendar-3`）；锁屏（`system/ui/lock_faces/*`、`lock_mark.c`，`lock-styles-1`）；`third_party/mogo/`、`apps/mogo/`；`tools/fontgen/`、`tools/asset_pack.py`、分区表；`backup-restore-recovery` 的代码和超时。
- 和同批任务的关系：`lock-styles-1` 等本任务合入才派，读本任务的头文件；`music-3`、`phone-3`、`files-3`、`memo-3`、`gallery-3` 和第一波的 `reader-one-1`、`auto-provision-1`、`settings-d137`、`camera-touch-bus` 只会在 `CMakeLists.txt`、`ai_surface.c`、`en.tsv`、`capabilities.md` 末尾和本任务相撞，冲突由主会话解。font_data 是大家共用的余量：本任务新增的界面汉字只来自 `almanac.c`、`calendar.c` 和译文，报告写清占了多少。
- 预算不变：固件增量不超过 60 KB（61,440 B，实测）；胶水不超过 250 行（v1 是 140 行，原文、生成的表、测试、文档、译文不算）。超了按第 6 条处理，不砍内容。
- 内部内存不增加；查一次只用临时 PSRAM；不新增任务、线程、定时器、缓存、持久数据或配置项。
- D-138：本任务需要的东西全在固件里，卡上不需要任何文件，所以没有“数据没装上”的状态；`calendar-3` 包装函数里那条分支只用于测试。

## 验收

1. **结果一字不变**：
   - `almanac-oracle`：`generate_calendar_data.cjs --check` 通过，2000–2099 共 36,525 天全部字段和 lunar-javascript 1.7.7 一致（要 node；本机必须真跑，不能是跳过）。
   - `almanac-fixtures`（50 天固定样本，`fixtures.json` 不改）和 `almanac-mogo-host`（Mogo 原文测试）通过。
   - 反向对照：`almanac-reverse` 照旧（只把 2026-02-17 的宜改错，50 天测试必须失败）；另外手工做一次，把生成器的折叠键改成只按日干支，`--check` 或 oracle 必须失败，命令和输出写进报告。
   - 能力输出：今天（UTC 和上海时区各一次，模拟时钟 2026-02-16 20:00 UTC）、2026-02-17、1999-12-31（不在范围）、孩子查 2026-02-17，返回的 JSON 和一句话回执和 v1 报告里的表逐字一致（这就是“改前改后”，本任务没有页面）。
2. **体积**：S31 app 镜像增量 ≤ 61,440 B，报告写镜像差、各段差（表、字符串、代码）、font_data 前后、应用分区余量；超了按“要做的”第 6 条停下。
3. **合并**：4 处冲突按上面解，`services-mid-capability-doc` 通过，`capabilities.md` 是重新生成的。
4. **全量**：先清掉 `build/test-out` 再跑普通全量 ctest，全部通过。`backup-restore-recovery` 再超时的话：隔离复跑一次，两次结果都写进报告，不改它的超时、不跳过，写进“顺手发现”交给主会话进 backlog。
5. ASan（`INK_SANITIZE=ON`）跑 `almanac-*` 全过；S31 宿主机工程（`ports/esp32s31/tests`）全过；`ports/esp32s31/build.sh` 通过、无新警告；`gcccheck`（`$INK_DEPS/../ink-codex/tools/gcccheck.py`）覆盖 `almanac.c`、`time_caps.c`、`calendar.c`、`tests/almanac/*.c`，0 处问题。
6. 报告还要写：查一天的宿主机耗时（v1 是 531 ns，词号拼串会慢一点）、S31 构建的 `-fstack-usage` 里 `calendar_date` 和 `ink_almanac_cap_run` 的栈用量、胶水行数、刷屏次数（没有页面，0 次）、截图（没有页面，不适用，用第 1 条的输出表代替）。
7. **改过的已有测试**要逐条列出（只有这几处，都因为字段变成数组，严格程度不变）：`tests/almanac/model.c` 里 `invalid.yi` 改成 `invalid.yi[0]`；`tests/almanac/reverse.py` 的定位行和注入语句跟着新的 `calendar.c` 改（仍然只改 2026-02-17 的宜，仍然要求 50 天测试失败并报出这一天）；`tools/calendar_host_test.py` 的 `Date._fields_`。其余测试一行不改。

## 要用户定（目前没有；下面两种情况出现才写）

- 按第 6 条全部压完仍超 60 KB：放宽预算，还是按 D-138 把规则表放到卡上（经 `auto-provision-1`）。
- `calendar-3` 要不要换掉旧农历（2097-08-07～09-05 共 30 天不同）：v1 已列，本任务不改 `lunar.c`，由 `calendar-3` 定。

## 给 lock-styles-1、calendar-3 的接口说明（写进报告）

- `#include "ink/sys/almanac.h"`，`ink_almanac_date(年, 月, 日, &out)`，范围 2000-01-01～2099-12-31，按调用方给的“当地公历日期”算；返回 `false` 时 `out` 清零。
- Mogo 的 24 个原字段都在；另有年/月/日干支名、完整农历文字 `lunar_text`、当天节气 `solar_term`（不是节气日为空串）；农历月为负数表示闰月。
- `yi`、`ji`、`auspicious`、`inauspicious` 是结构体里的字符串数组，其余字符串指向 flash 里的常量。结构体约 700 B，别在小栈上放很多个（万年历月格每格查一次时复用一个）。
- “磑”（宜忌词“安碓磑”）不在 GB2312，系统界面字库和阅读字库都没有这个字；锁屏或日历要显示时自己处理（Mogo 是往它自己的字库补了这个字），本任务不改字库。
- 宜忌、神煞、方位是民俗说法，显示时带一句“这些是民俗说法”；时钟没校准时不给日期的查询返回失败，画面上照 Mogo 写“请校准时间”。
