# log-1 执行报告

基线 `1450591caecfffdff023b19dc057fad5174a050f`，分支 `claude/codex-log-1`。仅本地编译和宿主机测试，不碰设备、不调付费接口、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 未通过全量 | 【实测】当前提交普通版原话：`99% tests passed, 2 tests failed out of 338`（普通 315.76 s，ASan/UBSan 534.33 s，均无跳过、无 sanitizer 报错）：`index-durable` 和禁止本任务更新的共享能力文档。日志/解码、菜单编号、内存预算和存储顺序通过，五项反向对照均拒绝变异；两轮完整检查均为 336/338 通过 | 总规则构建命令；`ctest --test-dir build --output-on-failure -j2`；`python tests/log/negative.py build` |
| 2 | 条件通过；标准 S31 打包未过 | 【实测】端口普通、ASan/UBSan 均 9/9；GCC 宿主 14、端口测试 2、S31 12 个编译单元，均 0 findings、无跳过。原厂 5 MiB 表下 S31 编译及大小检查通过，0 警告；标准骨架表太小。隐私扫描 0 命中 | `ctest --test-dir build-s31-host --output-on-failure -j2`；ASan 换 `build-s31-host-asan`；GCC、S31 命令见末尾 |
| 3 | 已给步骤，设备部分未实测 | 【实测】离线解码、SD 原始导出往返通过；【实现】设备只读拉取与 ELF 解析命令 | 见“用户报问题后 Claude 怎么查” |

【实现】争用时不等待，丢失和覆盖计入 dropped。每行含 UTC 秒（未知明确标出）、开机毫秒、开机号、等级、模块。RTC 尾巴 16 KiB；512 KiB 环按 4 KiB 分槽，20 字节 LOG1 头含序号、开机号、长度、CRC32，CRC 覆盖元数据与正文。提前擦下一槽，通常保留 127 条有效记录。30 秒或 8 KiB 批写，后台批次等待现有 500 ms 输入空闲和屏幕空闲；ERROR 唤醒原存储任务，主动记录问题及关机同步请求写入，均服从声音写盘禁令。HAL 启动前保留串口后备，启动后接统一日志。

【实现】能力调用仅记地址、来源、结果、耗时；全能力事件只记名字。记录页面、充电、锁屏、成员编号、网络类型、确认数量、刷新结果。每 5 分钟及低电/锁屏/记录问题采健康数据：两类内存余量、低水位、最大块、任务数、栈余量最少五个任务、供电、刷新统计、输入数。

## 没做到的

- 用户描述参数接受但原文省略，只记 `[private]`。未实现可靠识别人名、短信、聊天、密码的自然语言脱敏；不能算“描述已保存”。Wi-Fi 名及电话也完全省略，没有保留末两位。
- S31 RISC-V SDK 不提供设备端完整符号回溯；启动摘要记任务名、PC、RA，完整回溯须用对应 ELF 离线解码。
- 未逐个改各服务未上报的内部错误；集中覆盖能力失败、事件、LVGL、HAL、显示失败。
- 首次最终普通版结果原话：`99% tests passed, 4 tests failed out of 338`；语音输入两个场景因新增定时器占用而低于内部 RAM 预算 104 字节，已移除该定时器，当前提交两轮全量的语音场景均通过。
- 普通版全量再次复现 `index-durable`：`tests/index_durable/device.c:132` 的“new file gets indexed before rename”失败（16.940 s）。按[总规则第 7 节](../codex-rules.md)的“三次还不过就停下”要求，不再扩改索引模块，未证明它与基线无关，验收不能算全绿。
- 模拟文件验证 NOR 位清除、回绕、撕裂字节恢复；未接入原 `/user` 闪存耗时和脚本掉电时序。RTC 复位测试保留同一对象，未跨宿主机进程恢复。
- Linux CI 未执行（本机是 macOS，任务禁止推送），只完成 GCC 交叉编译检查，不声称 Linux 运行实测。
- 未上机验证：设备读取、真实 ELF/core 解析、断电/看门狗恢复、TF 卡导出、写闪存对声音的影响。IDF espcoredump/Kconfig 明确提示 PSRAM 任务栈与闪存核心转储存在限制，当前项目允许外部栈，不能承诺此类任务的真实转储完整；RTC 尾巴独立保留。

## 和设计不一致的

- 描述、电话及 Wi-Fi 名完全省略，原因见上。原始转储暂仅成人在设备上导出，AI 不代操作；记录问题对 AI 开放。
- 日志位置按任务定为 font_data 尾部，没有采用原厂 SD 日志路径；没有照搬原厂代码。
- 标准 S31 脚本仍使用骨架默认 1.5 MiB 表，首次镜像 1,557,200 字节，超出 21,200 字节；当前提交镜像为 1,557,136 字节；未改共享分区源码，仅在忽略的 build-s31 中选原厂 partitions.csv（5 MiB OTA），编译和大小检查通过，余 70%。正式分区接线由并行 port-boot-1 合入；标准构建不能写为通过。

## 改过的已有测试

- 电源菜单按钮 3→5，取消索引 2→4；新增实际点击记录问题并检查编号。公共按钮数组允许本测试指定数量，其余仍为 3。
- AI 清单增加 `system.report_problem`（描述可选）及仅本人可用的 `system.export_logs`；原权限断言保留。
- 未删除测试、放宽预算或断言。订阅测试的临时诊断已还原。设计表仅追加任务指定的新能力；示例文档两处源码链接随 include 平移。

## 要用户定的

- 用户描述怎样保留有用上下文并可靠排除家庭隐私；目前省略。
- 是否允许 AI 发起原始核心转储导出；目前仅成人设备入口。

## 顺手发现

- 全局状态观察会占掉框架最后一个可复用槽；改为状态表的集中调用，没有扩容，原测试通过。
- 新增日志定时器使语音场景的内部 RAM 最低余量降至 32,664 字节；移除后模拟预算实测 32,776 ≥ 32,768 字节，阈值没有修改。
- 日志后台批次抢占闪存守卫会影响普通存储等待；已使用现有输入/屏幕空闲条件，存储顺序原测试通过；索引恢复定向通过后又在全量失败，保留未解决。
- 宿主链接器仍有既存的重复 `ink_port_sim` 库提示，未改无关结构。

## 新增的机制

- LOG1 格式、CRC32 原始环、RTC 尾巴：任务 1～3 要求。
- 批写与健康采样复用现有存储任务的 30 秒超时唤醒：任务 2、7 要求。移除了曾造成内部 RAM 预算回归的定时器，没有新任务、线程、定时器、队列、缓存或配置开关。
- ESP-IDF 转储及任务跟踪配置：任务 3、7 要求，沿用原厂 8 任务/1792 字节转储栈。
- 产品 C/H 净增 **485 行**，产品构建/配置净增 11 行，合计 **496 行**（测试、报告、解码工具另算）。不能缩成串口打印或 `/user` 文件：前者复位丢证据，后者不符指定位置；固定扇区省去可变记录分配和恢复索引。

## 要改的文档

- 合并时统一生成 `docs/system/capabilities.md`；总规则禁止本任务改共享生成文档，因此保留 `capability-table-doc` 差异，不跳过检查。

## 原厂出处与上机先核对清单

查阅日期 2026-11-10，原厂提交 `690a023c6686`。`sdkconfig.defaults:49–51`：闪存转储、8 任务、1792 字节栈；`sdkconfig:2855`：开机检查；`main/boards/common/storage/sd_log_writer.cc:131–153`：检查和读出有效转储。RISC-V 回溯限制见 IDF `components/espcoredump/include/port/riscv/esp_core_dump_summary_port.h:13–19`；ELF 是当前 IDF 的固定写入格式；删除了已不存在、会被忽略的数据格式配置符号。RTC SRAM 32 KiB 见 `components/esp_system/ld/esp32s31/memory.ld.in:108–110`。以上为【实现】核查，非上机证据。

上机先核对分区标签/地址/大小、panic/watchdog 后 RTC 保留、转储与 ELF 匹配、导出遇拔卡、播放不写日志、空闲擦除。端口严格检查 font_data 地址与至少 5 MiB 大小，只写/擦尾部 512 KiB；coredump 只读。

## 用户报问题后 Claude 怎么查

1. 记下“问题 #n”和时间；能操作时从电源菜单导出，卡上是 `/content/ink-logs/<UTC秒>-b<开机号>-<毫秒>/logs.bin`、`coredump.bin`。
2. 已人工进入下载模式后，有设备授权的复核者运行：`python tools/device/pull_logs.py --port "$PORT" --elf "$FIRMWARE_ELF" --output "$LOG_OUT"`。本任务未运行设备命令；工具只发两条 `read-flash`，带 `--no-stub` 避免 RAM stub 上传，不复位、不写设备，默认输出到仓库外 `../ink-hw/logs/`。
3. 离线：`python tools/device/pull_logs.py --logs "$RAW_DIR/logs.bin" --coredump "$RAW_DIR/coredump.bin" --elf "$FIRMWARE_ELF" --output "$LOG_OUT"`。用装有 esptool/esp-coredump 的 IDF Python；ELF 必须对应故障固件。纯日志解码去掉 `--elf`，无需额外依赖。
4. 在 `logs.txt` 找问题编号，核对 page、UTC/开机毫秒、boot，查看随后的最近 50 个事件和健康快照，再查前后 call/result、页面和刷新失败。复位后搜“上次开机最后的日志”。
5. 用 `coredump.txt` 的崩溃任务、回溯与启动 PC/RA、最后一个无返回能力相互核对。工具失败返回非零并保留原始文件。

## 复现命令与最终结果

普通及 ASan/UBSan 全量按总规则第 5 节配置，命令前 `taskpolicy -c utility`，构建/ctest 均 `-j2`。ASan 目录 `build-asan` 加 `-DINK_SANITIZE=ON`；中枢用 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，测试环境 `PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=hub/src`。最终普通版及 ASan/UBSan 版原话均为 `99% tests passed, 2 tests failed out of 338`，退出 8；失败均为能力目录文档过期和 index-durable。普通耗时 315.76 秒，ASan 534.33 秒，均未跳过；没有 AddressSanitizer/UBSan 报错。两种端口构建各 9/9，通过；最终 no-stub 命令测试另在普通/ASan 各重跑 1/1，通过。

端口：`cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`，构建/ctest 并行 2；ASan 换 `build-s31-host-asan` 并加 `-DINK_SANITIZE=ON`。S31：`INK_UPSTREAM="$INK_UPSTREAM" taskpolicy -c utility bash ports/esp32s31/build.sh`，仅编译。GCC：`taskpolicy -c utility python "$INK_DEPS/../ink-codex/tools/gcccheck.py" <构建目录> <修改的C文件>`。反向对照：`taskpolicy -c utility python tests/log/negative.py build`。


S31 额外核查只修改生成状态：复制 `$INK_UPSTREAM/partitions.csv` 到 `build-s31/trial-partitions.csv`；将生成的 `build-s31/sdkconfig` 中 SINGLE_APP_LARGE 关闭、CUSTOM 打开，CUSTOM_FILENAME 指向此副本。加载 `$IDF_PATH/export.sh`，设置 `INK_LVGL_SOURCE_DIR=$INK_DEPS/build/_deps/lvgl-src`、`INK_UPSTREAM`、`IDF_COMPONENT_MANAGER=0`，执行 `taskpolicy -c utility cmake -S ports/esp32s31 -B build-s31 -DIDF_TARGET=esp32s31 -DSDKCONFIG="$PWD/build-s31/sdkconfig"` 后 `taskpolicy -c utility cmake --build build-s31 -j2`。当前源码提交 `c09652a8` 的镜像 `0x17c290` 字节（1,557,136），5 MiB 槽剩余 70%，编译警告 0；RTC noinit 节 16,432 字节。标准 `build.sh` 会恢复默认小分区，仍不能宣称通过。

GCC 实际范围：宿主 14 个编译单元为 `system/boot/main.c`、`system/services/log/log.c`、`system/policy/{cap,state,system_actions}.c`、`system/services/{display/display_service,power/power_service,storage/storage_service}.c`、`system/ui/{shell/power_menu,ui_task}.c`、`ports/sim/hal/{hal_log_sim,hal_power_sim}.c`、`tests/log/log.c`、`tests/integration/power_menu.c`；S31 是其中前 10 个产品单元加 `ports/esp32s31/{hal_log_s31,hal_stubs}.c`；端口测试为 `tests/log/s31.c`、`ports/esp32s31/hal_log_s31.c`。用对应构建目录传给 gcccheck，三组均 0 findings、无跳过。五项反向对照（回绕、CRC、尾巴、问题落盘、隐私）均编译后被功能断言拒绝，变异进程退出 -6，反向脚本退出 0。

最终本地证据在忽略目录 `work/log-1/`：`normal-ctest.log`、`asan-ctest.log`、`port-ctest*.log`、`gcc-*.log`、`negative.log`、`s31-layout-last.log`、`tool-final*.log`。日志/导出测试在仓库外临时目录自动清理；未提交二进制、设计稿或绝对本机路径。

收尾检查：【实测】基线到最终改动的绝对本机路径/MAC 模式扫描 0 命中，Markdown 本地链接检查 0 错误，`git diff --check` 退出 0。`git diff --stat` 仅本任务日志、HAL、集中观测点、菜单、相应测试和说明；未改共享生成目录、需求/想法/决策、已有调研或审计资料，未 push。
