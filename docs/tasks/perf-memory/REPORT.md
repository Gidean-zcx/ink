# perf-memory 执行报告

基线：`c680b4b52454df342a79251d8a40bb3cd60243ec`。日期：2026-11-09。分支：`claude/codex-perf-memory`。以下没有实机结论。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 完成 | 【实测，模拟器】前后各 264/264 通过，1,057 份运行逐一对照；全部数据见 [MEASUREMENTS.md](MEASUREMENTS.md)，堆、静态与栈分开列于下文 | V1、V2、V4 |
| 2 | 完成 | 最紧正常产品场景 AI 工具调用：内部最低余量 33,272 → 37,376 B，增加 4,104 B；PSRAM 峰值 4,509,040 → 4,513,144 B。故意耗尽内存的自检不改善，见下表 | V3、V4 |
| 3 | 部分完成，保守保持栈大小 | 【实测，S31 交叉编译】124 个可移植单元编过，提供函数帧与已知调用链；缺少 S31 端口，不能取得完整任务调用链或 `idf.py size`，不据此缩栈或加预算 | V5 |
| 4 | 完成 | 产品代码增 3 行、删 1 行，共 4 行；普通版、ASan／UBSan 各 264/264 通过，均含确定性测试；无预算放宽 | V1、V4、V6、V8 |

## 没做到的

- 基线和只读依赖检出均没有 `ports/esp32s31` 及端口骨架报告，无法执行要求的端口 `idf.py size`。下文是 S31 编译对象的可写数据清单，不含 LVGL、IDF、真实 HAL／驱动和 IRAM 代码；也未经过链接裁剪，不能相加推断最终 SRAM 余量。
- 没有 Linux/GCC 主机全量测试、实机栈高水位、真实外设或 PSRAM 栈性能验证。S31 对象用 Espressif GCC，不等于 Linux 测试。
- 本次改动位于模拟器显示 HAL 的任务创建处；真机端口落地时须采用同一栈分配策略再验证。没有声称已经节省实机 RAM。
- 未缩减任何缓存、队列、DMA 环或栈大小。静态数据迁移不能改变当前模拟器堆指标，未将其记作收益；没有遗留无收益的试改。

## 和设计不一致的

- 未发现本次改动改变能力、界面、刷新策略或平台接口合同。现有 `OSAL_TASK_STACK_EXTERNAL` 是同一驱动的 feeder 已在使用的策略；协调任务不是 ISR，不调用内部 Flash API，完成回调只向显示服务投递消息。LCD DMA 缓冲、行队列和波形表保持原分配。
- 【架构审视】这次只改变端口任务栈的内存域，系统／应用边界和软总线不变；没有新增用户动作或数据，原能力、AI 快照和调用入口均由既有全量测试覆盖。

## 改过的已有测试

- 仅在 `tests/scenarios/ai_tools.budget.json` 新增内部最低余量 **至少 35,840 B** 的断言；没有删除、放宽既有期望或抬高上限。基线二进制配新断言失败，改后通过（V3）。

## 要用户定的

无新增产品决定。缺少目标端口的证据按“未验证”保留，不以调高预算代替。

## 顺手发现

- 本次更多场景里 `main` 主机栈估算最高 **4,000/4,096 B**，比审计举例的 3,952 B 更紧；`ui` 为 9,664/10,240 B。仍是宿主估算，不是实机越界实证。
- 1,758 张 PNG 哈希中 1,753 张一致。无线传输三张差异位于地址／二维码，两轮实际临时端口不同；一张最终图差异仅在状态栏，已查看。该专项使用真实本地 socket 和 `--realtime=20`。列表分页一张最终图不同：改前／改后分别用全新输出目录再跑，均 1,441 条检查通过且 PNG 哈希一致（V7），不把复用测试目录的差异算成产品回归，也不宣称全量截图逐像素一致。
- 静态表中最大的 `files.F` 已在 PSRAM；PSRAM 大户主要是显示、页面与图像。未找到可以在不改变行为／容量约定的条件下删除的重复缓存。

## 新增的机制

无新任务、状态机、持久格式、配置项、缓存或抽象层。两个测量脚本只在宿主命令行读取报告／交叉编译，不进入产品。只复用既有 OSAL 栈标志。

## 要改的文档

- 真机端口合入后，应在其报告中补 `idf.py size`、完整调用链和实机高水位，再决定是否缩减存储栈；显示协调栈的位置应与这里一致。未修改共享文档、需求、想法、决策或历史审计。
- 与 perf-latency 可能交叠的文件只有 `ports/sim/hal/hal_display_sim.c`：仅 `sim_display_init()` 中创建 `epd` 任务的配置和一行注释；没有改刷新计算、时序或执行函数。

## 验证记录

- **V1【实测】基线**：Werror 构建退出 0；全量 264/264 通过、零跳过，265.92 秒。日志 `work/perf-memory/{configure,configure-hub,build-baseline,ctest-baseline}.log`。
- **V2【实测】采集**：`measure.py` 记录内存指标和 PNG 哈希；原始摘要在 `work/perf-memory/{before,after}.json`。脚本把已有测试临时目录后缀规范化，同一案例取最近一次运行；不会修改被测指标或测试结果。前后各 1,057 例，档案与案例集合完全对应，其中悲观 Plus 档 1,029 例。
- **V3【实测】反向对照**：用未含修复的基线二进制运行新增预算，`ctest -R '^scenario-ai_tools-metalio-plus$'` 退出 8，唯一失败为 `33272 < min 35840`。改后同一测试通过，未改场景输入。日志 `work/perf-memory/reverse.log`。
- **V4【实测】改后**：Werror 构建退出 0；全量 264/264 通过、零跳过，305.17 秒，包括 `determinism`、`determinism-reader`。日志 `work/perf-memory/{build-after,ctest-after,measure-after}.log`。悲观档 1,026 例内部余量增加 4,096～4,112 B；三例故意耗尽 RAM 的自检仍为 3,136 B。PSRAM 峰值增量 0～4,248 B，全部保留在对照表，无预算放宽。
- **V5【实测】S31 编译**：Espressif GCC 15.2.0，S31 ISA／ABI、`-Os -fstack-usage -fcallgraph-info=su`，124/124 单元成功；对象可写数据为内部 9,846 B、外部 386,227 B（覆盖范围有限，不能替代链接尺寸）。`.su`、`.ci`、`objects.json` 留在 `work/perf-memory/s31/`。配置未启用 LTO，`INK_PSRAM_BSS` 按外部 BSS 放置。
- **V6【实测】内存检查**：Werror + ASan／UBSan 构建退出 0，已核对编译和链接含 `-fsanitize=address,undefined`；全量 **264/264 通过**、零跳过，退出 0，用时 **492.98 秒**。完整日志未见 sanitizer 错误。日志 `work/perf-memory/{configure-asan,build-asan,ctest-asan}.log`。
- **V7【实测】列表对照**：仅把改动的 HAL 翻译单元编回原内部栈配置，复制并替换临时 archive 中对应对象，分别链接原版和改后 `sim-ui-list-paging`；都用全新输出目录。两边退出 0，1,441 项断言均过，最终 PNG SHA-256 同为 `6d4b30dc7fe47dfa140dce2511e4e1ed1d1fced2696974e31fd0dcc5c9477610`。未修改工作区产品源码或测试。日志 `work/perf-memory/list-control.log`。
- **V8【实测】交付核查**：`git diff --stat` 只有本任务 6 个文件，产品改动 4 行；`git diff --check` 通过；报告相对链接有效，测量脚本重生成的逐案例表与提交内容完全相同，两个脚本语法检查通过。按任务书第 6 节对基线到 HEAD 的 diff 执行指定隐私扫描：无匹配，`grep` 退出 1（表示未命中）；未提交改动亦核查。未提交原始指标、构建产物、设计稿或本机路径。

所有构建与全量测试依次运行，命令前加 `taskpolicy -c utility`，最多 `-j2`；没有启动窗口、触碰设备、付费调用或写远端。

```sh
# 普通版；内存版换成 build-asan 并额外加 -DINK_SANITIZE=ON。
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2

# 基线全量后采集 before；改后全量后采集 after 并生成逐案例对照。
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" \
  docs/tasks/perf-memory/measure.py build work/perf-memory/before.json
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" \
  docs/tasks/perf-memory/measure.py build work/perf-memory/after.json \
  --before work/perf-memory/before.json --table docs/tasks/perf-memory/MEASUREMENTS.md
# RV_GCC 指向现有的 riscv32-esp-elf-gcc；不安装或升级工具链。
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" \
  docs/tasks/perf-memory/s31_probe.py --build build --gcc "$RV_GCC" --out work/perf-memory/s31
```

## 内部 RAM 最紧的五个场景

【实测，模拟器】单位 B。确定性 a/b 与 selftest 是同一场景，只列 selftest；完整重复运行见对照表。自检刻意继续分配至仅余一小块，因此不以其低余量当作正常产品下限。

| 场景 | 内部最低余量：前 | 后 | 增量 | PSRAM 峰值：前 | 后 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `test-out/selftest-metalio-plus` | 3136 | 3136 | 0 | 2511160 | 2515264 |
| `test-out/ai_tools-metalio-plus` | 33272 | 37376 | 4104 | 4509040 | 4513144 |
| `test-out/device-events/1` | 33320 | 37424 | 4104 | 3273104 | 3277208 |
| `test-out/device-events/2` | 33320 | 37424 | 4104 | 3273712 | 3277816 |
| `test-out/ui-scene-big-AiListen` | 33376 | 37480 | 4104 | 4504056 | 4508160 |

唯一优化项：`epd` 标签的内部峰值 **4,456 → 360 B**（仅留 TCB），新增 PSRAM 栈 4,096 B。全堆最低余量增加 4,104 B，含转移的 TLSF 块头 8 B；堆头、对齐与各域峰值时刻解释了少数案例的小幅差异。这不是减少总内存，而是把 4 KB 从紧缺内部 RAM 转移至 PSRAM。

## 动态堆标签前 15 项

【实测，模拟器】在全部悲观 Plus 运行中取每个标签的峰值，单位 B；这些峰值出现在不同运行、不同时间，不能相加当作一次运行的总占用。† 为测试任务或故意施压的测试数据。内部列合并普通内部堆与 DMA 预留的标签峰值（两者峰值之和只是上界）。

| 排名 | 内部 RAM 标签 | B | PSRAM 标签 | B |
| ---: | --- | ---: | --- | ---: |
| 1 | `selftest`† | 70184 | `appstest`† | 13123584 |
| 2 | `display-driver` | 52416 | `texttest`† | 13123584 |
| 3 | `sd-bounce` | 12288 | `systest`† | 12818408 |
| 4 | `uart-nic` | 12288 | `backup-test`† | 8405672 |
| 5 | `systest`† | 9416 | `atomic-writer`† | 8393384 |
| 6 | `starts`† | 9224 | `exec.media` | 5251072 |
| 7 | `timetest`† | 9224 | `backup-pages-test`† | 2114216 |
| 8 | `nettest`† | 9128 | `api-writer`† | 2101928 |
| 9 | `datatest`† | 9032 | `display-driver` | 1662976 |
| 10 | `filetest`† | 9032 | `ui` | 1496984 |
| 11 | `i2s-tx` | 7680 | `reader-pages` | 1325952 |
| 12 | `i2s-rx` | 7680 | `display-service` | 831744 |
| 13 | `board`† | 6568 | `selftest`† | 831744 |
| 14 | `tls` | 6144 | `board`† | 831744 |
| 15 | `main` | 5992 | `camera` | 614416 |

排除测试任务主动施压后，常规产品场景内部堆前列为 `display-driver` 52,416 B、`uart-nic` 12,288 B、`main` 5,032 B、`epd` 4,456 B。前三项涉及显示实时缓冲、4G DMA 和启动栈；只移动第四项。`sd-bounce` 和 I2S 是 DMA 用途，未挪走。

`ink_mem_budget` 的产品声明只有相机帧与输入法字典：PSRAM 上限分别为 `frame_bytes`（当前 614,400 B）和 1,048,576 B。这是配额，不是占用。相机的实测标签峰值 614,416 B 含资源管理头；其他动态占用用 `osal_mem_tag`／任务归属统计。未拿配额当实测用量。

## 静态数据前 15 项

【实测，S31 交叉编译对象】仅含 124 个可移植编译单元的可写静态数据；不是链接后的固件尺寸。单位 B。两列各自按大小排序；本次未改静态数据。

| 排名 | 内部静态对象（源文件） | B | PSRAM 静态对象（源文件） | B |
| ---: | --- | ---: | --- | ---: |
| 1 | `DS`（`system/services/display/display_service.c`） | 2336 | `F`（`system/apps/files/files.c`） | 126544 |
| 2 | `M`（`system/policy/members.c`） | 1696 | `AL`（`system/policy/cap.c`） | 24584 |
| 3 | `args.0`（`system/apps/sms/sms.c`） | 768 | `S`（`system/apps/sms/sms.c`） | 19500 |
| 4 | `a.0`（`system/ui/shell/boot_flow.c`） | 616 | `A`（`system/ui/shell/ai_panel.c`） | 16588 |
| 5 | `C`（`system/ui/shell/interrupt_card.c`） | 544 | `cache`（`system/ui/icons.c`） | 13824 |
| 6 | `AU`（`system/services/audio/audio_service.c`） | 440 | `S`（`system/ui/shell/settings_family.c`） | 13400 |
| 7 | `P`（`system/ui/shell/member_pages.c`） | 312 | `M`（`system/apps/memo/memo.c`） | 9788 |
| 8 | `PS`（`system/services/power/power_service.c`） | 256 | `DB`（`system/services/data/datastore.c`） | 8872 |
| 9 | `O`（`system/ui/shell/off_picture.c`） | 240 | `T`（`system/services/time/time_service.c`） | 8296 |
| 10 | `g_url`（`system/apps/remote/remote.c`） | 180 | `C`（`system/apps/calendar/calendar.c`） | 8272 |
| 11 | `IS`（`system/services/input/input_service.c`） | 160 | `L`（`system/ui/shell/ai_pages.c`） | 7896 |
| 12 | `RD`（`apps/reader/reader.c`） | 160 | `SE`（`system/policy/settings.c`） | 7344 |
| 13 | `HUB_CARD`（`system/policy/confirm.c`） | 152 | `ST`（`system/policy/state.c`） | 7216 |
| 14 | `ST`（`system/services/storage/storage_service.c`） | 132 | `N`（`system/ui/shell/navigation.c`） | 6512 |
| 15 | `seen.1`（`system/services/display/display_service.c`） | 128 | `EV`（`system/policy/cap.c`） | 5568 |

## 栈结论

【实测，模拟器】主机列是在全部悲观 Plus 报告中取最大估算，不是 S31 的高水位。S31 列来自本次 `.su/.ci`：单函数帧与已知直接调用链小计，**不包括**未编译的 OSAL／HAL／LVGL／IDF／C 库以及无法静态闭合的回调，不能当作完整任务栈上界。

| 任务 | 配置 B／内存域 | 主机最高估算 B | S31 对照 B | 结论和依据 |
| --- | --- | ---: | --- | --- |
| `esp_timer` | 3584／内部 | 3552 | svc 回调帧 32；ring_cb 已知链 976 + 分发未知 | 保持；缺少完整目标链，不缩减、不调高 |
| `main` | 4096／内部 | 4000 | ink_main 帧 64；已知启动链 3,088 + 外部调用未知 | 保持；缺少完整目标链，不缩减、不调高 |
| `storage` | 4096／内部 | 3472 | storage_task 帧 128；校验写路径小计 784 + HAL 未知 | 保持 4,096；不能凭 128 B 帧忽略 LittleFS 和 HAL 深调用 |
| `exec.media` | 8192／PSRAM | 7552 | exec_task/run 各 16；缩略图已知链 3,056 + 间接调用未知 | 保持；不能把未用到的 zlib 导出函数误算为 PNG 路径 |
| `exec.bg` | 8192／PSRAM | 6376 | exec_task/run 各 16；各工作项及外部调用未闭合 | 保持；缺少完整目标链，不缩减、不调高 |
| `epd` | 4096／内部→PSRAM | 1760 | 无端口，未取得目标帧 | 保持大小，仅迁移；不访问 Flash API，前后全量通过 |
| `epd_prep0` | 3072／PSRAM | 704 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |
| `epd_prep1` | 3072／PSRAM | 512 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |
| `epd_plan`（PQ-BN 档） | 2048／内部 | 368 | 无端口，未取得目标帧 | 保持；补列只在 PQ-BN 档启动的规划任务 |
| `input` | 3072／内部 | 1392 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |
| `i2c_bg` | 2048／内部 | 928 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |
| `sys_evt` | 2560／内部 | 1632 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |
| `uart_nic` | 4096／PSRAM | 272 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |
| `ws` | 4096／PSRAM | 848 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |
| `power` | 3072／内部 | 2688 | power_task 帧 128；睡眠入口未知 | 保持；缺少完整目标链，不缩减、不调高 |
| `display` | 3072／PSRAM | 2112 | service_task 帧 336 | 保持；缺少完整目标链，不缩减、不调高 |
| `comm` | 8192／PSRAM | 5840 | comm_task 帧 96 | 保持；缺少完整目标链，不缩减、不调高 |
| `hub` | 6144／PSRAM | 4240 | hub_task 帧 464 | 保持；缺少完整目标链，不缩减、不调高 |
| `audio` | 26624／PSRAM | 3776 | audio_task 帧 112 | 保持；缺少完整目标链，不缩减、不调高 |
| `audio_in` | 4096／PSRAM | 1072 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |
| `bt_audio` | 2048／PSRAM | 896 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |
| `ui` | 10240／PSRAM | 9664 | ui_task 帧 208；LVGL 未编译 | 保持；缺少完整目标链，不缩减、不调高 |
| `haptic` | 2048／内部 | 1056 | 无端口，未取得目标帧 | 保持；缺少完整目标链，不缩减、不调高 |

【实现＋交叉编译】`ink_json_parse` 的 S31 帧仍有 2,656 B，启动已知链并不小，不能把 `main` 的接近上限完全归咎于 64 位主机。相反，`stb_image.h` 若干未用的 zlib 导出函数有 6,592～6,608 B 帧，但 PNG 实际调用的 `stbi_zlib_decode_malloc_guesssize_headerflag` 已把大结构放堆上，帧只有 48 B；直接把最大 `.su` 数字当成 `exec.media` 的实际调用链会误判。现有证据没有证明哪个任务必须加栈，也不足以接受缩栈。

里程碑提交：`b7fb7776`（建立报告）、`238c221b`（优化、反向对照、全量对照及 S31 分析）；最终验证与隐私核查单独提交。全部只在本地分支提交，未 push。
