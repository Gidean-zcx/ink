# log-1 执行报告

基线 `1450591caecfffdff023b19dc057fad5174a050f`，分支 `claude/codex-log-1`。仅本地编译和宿主机测试，不碰设备、不调付费接口、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 最终全量待跑 | 【实测】日志/解码往返、菜单点击后显示问题编号、快照、订阅槽、存储顺序和索引恢复通过；五项反向对照编译后均被原断言拒绝。首轮 338 项有 10 项失败、3 项跳过；定向复测已消除功能性失败，中枢 3 项补跑通过 | 总规则构建命令；`ctest --test-dir build --output-on-failure -j2`；`python tests/log/negative.py build` |
| 2 | 进行中 | 【实测】宿主机 gcccheck 11 个编译单元 0 findings；最终 S31、端口普通/ASan、隐私扫描待跑 | 见末尾命令 |
| 3 | 已给步骤，设备部分未实测 | 【实测】离线解码、SD 原始导出往返通过；【实现】设备只读拉取与 ELF 解析命令 | 见“用户报问题后 Claude 怎么查” |

【实现】每行含 UTC 秒（未知明确标出）、开机毫秒、开机号、等级、模块。RTC 尾巴 16 KiB；512 KiB 环按 4 KiB 分槽，20 字节 LOG1 头含序号、开机号、长度、CRC32，CRC 覆盖元数据与正文。提前擦下一槽，通常保留 127 条有效记录。30 秒或 8 KiB 批写，后台批次等待现有 500 ms 输入空闲和屏幕空闲；ERROR 唤醒原存储任务，主动记录问题及关机同步请求写入，均服从声音写盘禁令。HAL 启动前保留串口后备，启动后接统一日志。

【实现】能力调用仅记地址、来源、结果、耗时；全能力事件只记名字。记录页面、充电、锁屏、成员编号、网络类型、确认数量、刷新结果。每 5 分钟及低电/锁屏/记录问题采健康数据：两类内存余量、低水位、最大块、任务数、栈余量最少五个任务、供电、刷新统计、输入数。

## 没做到的

- 用户描述参数接受但原文省略，只记 `[private]`。未实现可靠识别人名、短信、聊天、密码的自然语言脱敏；不能算“描述已保存”。Wi-Fi 名及电话也完全省略，没有保留末两位。
- S31 RISC-V SDK 不提供设备端完整符号回溯；启动摘要记任务名、PC、RA，完整回溯须用对应 ELF 离线解码。
- 未逐个改各服务未上报的内部错误；集中覆盖能力失败、事件、LVGL、HAL、显示失败。
- 模拟文件验证 NOR 位清除、回绕、撕裂字节恢复；未接入原 `/user` 闪存耗时和脚本掉电时序。RTC 复位测试保留同一对象，未跨宿主机进程恢复。
- 未上机验证：设备读取、真实 ELF/core 解析、断电/看门狗恢复、TF 卡导出、写闪存对声音的影响。

## 和设计不一致的

- 描述、电话及 Wi-Fi 名完全省略，原因见上。原始转储暂仅成人在设备上导出，AI 不代操作；记录问题对 AI 开放。
- 日志位置按任务定为 font_data 尾部，没有采用原厂 SD 日志路径；没有照搬原厂代码。

## 改过的已有测试

- 电源菜单按钮 3→5，取消索引 2→4；新增实际点击记录问题并检查编号。公共按钮数组允许本测试指定数量，其余仍为 3。
- AI 清单增加 `system.report_problem`（描述可选）及仅本人可用的 `system.export_logs`；原权限断言保留。
- 未删除测试、放宽预算或断言。订阅测试的临时诊断已还原。设计表仅追加任务指定的新能力；示例文档两处源码链接随 include 平移。

## 要用户定的

- 用户描述怎样保留有用上下文并可靠排除家庭隐私；目前省略。
- 是否允许 AI 发起原始核心转储导出；目前仅成人设备入口。

## 顺手发现

- 全局状态观察会占掉框架最后一个可复用槽；改为状态表的集中调用，没有扩容，原测试通过。
- 日志后台批次抢占闪存守卫会影响普通存储等待；已使用现有输入/屏幕空闲条件，存储顺序和索引恢复原测试通过。
- 宿主链接器仍有既存的重复 `ink_port_sim` 库提示，未改无关结构。

## 新增的机制

- LOG1 格式、CRC32 原始环、RTC 尾巴：任务 1～3 要求。
- 一个 30 秒软件定时器，只唤醒现有存储任务：任务 2、7 要求。没有新任务、线程、队列、缓存、配置开关或通用框架。
- ESP-IDF 转储及任务跟踪配置：任务 3、7 要求，沿用原厂 8 任务/1792 字节转储栈。
- 产品 C/H 净增 **488 行**，产品构建/配置净增 12 行，合计 **500 行**（测试、报告、解码工具另算）。不能缩成串口打印或 `/user` 文件：前者复位丢证据，后者不符指定位置；固定扇区省去可变记录分配和恢复索引。

## 要改的文档

- 合并时统一生成 `docs/system/capabilities.md`；总规则禁止本任务改共享生成文档，因此保留 `capability-table-doc` 差异，不跳过检查。

## 原厂出处与上机先核对清单

查阅日期 2026-11-10，原厂提交 `690a023c6686`。`sdkconfig.defaults:49–51`：闪存转储、8 任务、1792 字节栈；`sdkconfig:2855`：开机检查；`main/boards/common/storage/sd_log_writer.cc:131–153`：检查和读出有效转储。RISC-V 回溯限制见 IDF `components/espcoredump/include/port/riscv/esp_core_dump_summary_port.h:13–19`；RTC SRAM 32 KiB 见 `components/esp_system/ld/esp32s31/memory.ld.in:108–110`。以上为【实现】核查，非上机证据。

上机先核对分区标签/地址/大小、panic/watchdog 后 RTC 保留、转储与 ELF 匹配、导出遇拔卡、播放不写日志、空闲擦除。端口严格检查 font_data 地址与至少 5 MiB 大小，只写/擦尾部 512 KiB；coredump 只读。

## 用户报问题后 Claude 怎么查

1. 记下“问题 #n”和时间；能操作时从电源菜单导出，卡上是 `/content/ink-logs/<UTC秒>-b<开机号>-<毫秒>/logs.bin`、`coredump.bin`。
2. 已人工进入下载模式后，有设备授权的复核者运行：`python tools/device/pull_logs.py --port "$PORT" --elf "$FIRMWARE_ELF" --output "$LOG_OUT"`。本任务未运行设备命令；工具只发两条 `read-flash`，不复位、不写设备，默认输出到仓库外 `../ink-hw/logs/`。
3. 离线：`python tools/device/pull_logs.py --logs "$RAW_DIR/logs.bin" --coredump "$RAW_DIR/coredump.bin" --elf "$FIRMWARE_ELF" --output "$LOG_OUT"`。用装有 esptool/esp-coredump 的 IDF Python；ELF 必须对应故障固件。纯日志解码去掉 `--elf`，无需额外依赖。
4. 在 `logs.txt` 找问题编号，核对 page、UTC/开机毫秒、boot，查看随后的最近 50 个事件和健康快照，再查前后 call/result、页面和刷新失败。复位后搜“上次开机最后的日志”。
5. 用 `coredump.txt` 的崩溃任务、回溯与启动 PC/RA、最后一个无返回能力相互核对。工具失败返回非零并保留原始文件。

## 复现命令与最终结果

普通及 ASan/UBSan 全量按总规则第 5 节配置，命令前 `taskpolicy -c utility`，构建/ctest 均 `-j2`。ASan 目录 `build-asan` 加 `-DINK_SANITIZE=ON`；中枢用 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，测试环境 `PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=hub/src`。最终结果待补。

端口：`cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`，构建/ctest 并行 2；ASan 换 `build-s31-host-asan` 并加 `-DINK_SANITIZE=ON`。S31：`INK_UPSTREAM="$INK_UPSTREAM" taskpolicy -c utility bash ports/esp32s31/build.sh`，仅编译。GCC：`taskpolicy -c utility python "$INK_DEPS/../ink-codex/tools/gcccheck.py" <构建目录> <修改的C文件>`。反向对照：`taskpolicy -c utility python tests/log/negative.py build`。
