# port-power-2 执行报告

日期：2026-11-11。基线：`da9f76765f2d461590a870657eb3a0ad1e34152e`。

状态：部分交付，未通过整体验收；C03／RTC 单元与反向对照通过，C02 和完整充电移植未完成。未修改板级文件、系统服务、能力表或共享文档；不操作设备，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| C02 | 未完成【实测】 | 200 ms 时 `EN_CHG=0`，HAL 返回 `10000 ms`，正常 CTest 明确失败；板级提前返回未修 | `build-s31-host/test-s31-vendor-power c02-hal`（退出 1） |
| C03 | 单元通过【实测】；系统交接有遗留 | 3702 mV 放电读 50%，SOC 假值／NACK 不影响结果；单向 5% 步进及 30 s 满电确认通过；撤回曲线改读 SOC 后断言失败 | `taskpolicy -c utility ctest --test-dir build-s31-host -R unit-s31-vendor-power-gauge --output-on-failure`；下列反向对照 |
| P2 | 单元通过【实测】 | STOP 清除、非法月份／小时拒用、VL 拒用、可靠写回清 VL、AF/TF 清除且保留其他位；4 项反向对照命中断言 | `taskpolicy -c utility ctest --test-dir build-s31-host -R unit-s31-vendor-power-rtc --output-on-failure`；下列反向对照 |
| 1a S31 宿主机 | 未全过【实测】 | 原话 `98% tests passed, 1 tests failed out of 55`；54 通过、C02 唯一失败；退出 8，无跳过 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2` |
| 1b 主工程 | 零失败、3 跳过【实测】 | 原话 `100% tests passed out of 391`；388 通过、3 跳过，退出 0，812.57 s；最终重编译后原有充电相关 3/3 再过 | 总规则的普通配置／`cmake --build build -j2`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 1c 内存检查 | C03／RTC 通过，C02 逻辑失败【实测】 | S31 宿主机 ASan+UBSan 全部 55 项，仍 54 通过、C02 唯一失败，退出 8，14.31 s；未发现 sanitizer 错误 | 同宿主机构建，目录换 `build-s31-host-asan`、加 `-DINK_SANITIZE=ON`；同上 ctest，加 `ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` |
| 1d S31 编译 | 通过【实测】 | 退出 0，浅睡 OFF，编译警告／错误 0；镜像静态大小 4373186 B；仅编译，未连接设备 | `INK_S31_LIGHT_SLEEP=OFF taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 1e GCC、来源、预算 | 通过【实测】 | `gcccheck: 4 compiled, 0 with findings`；11 个原文哈希一致，246/250 行 | `taskpolicy -c utility python3 ../../tools/gcccheck.py build-s31-host drivers/chips/bq27220/bq27220.c drivers/chips/pcf8563/pcf8563.c ports/esp32s31/tests/vendor_power.c ports/esp32s31/tests/vendor_board.c`；`python3 ports/esp32s31/tests/power_sources.py --baseline da9f76765f2d461590a870657eb3a0ad1e34152e` |
| 2 | 清单完成【实现】，未上机 | 见文末：截止／终止、再充、百分比、USB、RTC／VL | 本任务禁止操作设备 |

【实测】反向对照命令：`taskpolicy -c utility python3 ports/esp32s31/tests/power_negative.py build-s31-host`，退出 0。`raw-soc`、`rtc-vl`、`rtc-stop`、`rtc-calendar`、`rtc-flags` 五项变体各退出 `-6`，均命中对应断言；正例分别通过。临时变体自动清理，产品文件未改。C02 没有修复，不冒充已有正反对照。

【实测】普通主工程跳过 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，不算通过。按 `vendor-common.md` 第 7 条未另跑主工程 ASan；内存检查覆盖全部 S31 宿主机目标，已核对 BQ／RTC／新测试含 `ESP_PLATFORM=1` 和 `-fsanitize=address,undefined`，不声称验证了真机堆栈或物理内存。

## 没做到的

- 完整 CX25601N 充电／OTG 驱动没有替换。初步 C 适配连同另外两项会超过胶水 250 行，已撤回该适配；现有充电代码没有改动，原厂完整文件仅作副本保存。
- C02 未修：HAL 在禁止改动的板级文件中，需由板级任务把未完成的再充续步放到电量计读取之前。没有以返回假成功、跨芯片耦合、额外定时器、弱符号或宏覆盖绕过限制。
- 未处理未知电量和系统低电策略的交接，条件见“顺手发现”；系统文件不在允许修改范围。
- 尚无本次真机证据；上机核对由主会话执行。仅在 macOS 执行，Linux 运行测试／远端 CI 未跑；GCC 核查及 S31 RISC-V 编译不冒充 Linux 测试。

## 和设计不一致的

- 【实现】只在 S31 硬件路径接原厂 BQ／RTC；模拟器保留原模型。原厂类方法用 C 语法接入既有 C 目标，必要改行及全部哈希见 [SOURCE.md](../../../ports/esp32s31/vendor/power_874538e/SOURCE.md)。
- 【实现】原厂 RTC 的系统时钟设置不搬入，维持现有板级 UTC+8 换算和系统可靠时间策略；不写 NVS。VL 返回失败，使 `time_service.c:863–870` 维持未知时间并请求可靠时间。
- 【实现】HAL 仍保留既有外电／充电状态补判及充电事件，不将原厂电芯曲线冒充本机校准。完整 RTC P16 边沿流程未替换，继续使用现有板级／输入处理。

## 改过的已有测试

- `vendor_board.c` 仅新增默认关闭的 SOC 命令 NACK 桩，原有断言和期望不变；CMake 增加原厂电量、RTC、C02 故障和来源校验。没有放宽或删除测试；C02 作为正常 CTest 用例保留，未修复时会明确失败。

## 要用户定的

- 方案 A（推荐由主会话另行授权）：纳入板级 HAL 入口及构建文件，直接编译原厂 C++、只留 C 桥接，并处理系统未知电量；可减少语法适配，完成集中修复。
- 方案 B（本次保守执行）：保持允许范围和 250 行预算，保留现有充电及板级入口，交付 BQ／RTC 与失败证据；C02／完整充电移植及未知电量交接继续未完成。

## 顺手发现

- 【实现】`power_service.c:190–205` 在低电策略启用时直接把 `percent=-1` 判为低电；原厂采样器开机前 3 s 或读失败会返回未知。`boot/main.c:105` 启动低电策略。【估算】若首次有效采样前启用策略，或运行中读失败，可能触发关机；本次没有真机复现。系统服务不在允许范围，未改；这是 C03 系统交接的遗留，不能将曲线单元通过当作整机验收。

## 新增的机制

- 任务第 2 项要求的原厂电量估算状态／60 点滤波和单例互斥量；复用 OSAL 时间及锁，没有新任务、线程、定时器、持久文件、配置项或 NVS。
- 任务第 3 项要求的原厂 RTC 初始化状态；原厂 STOP／VL／AF/TF 流程接在既有驱动入口。

## 要改的文档

- 合并主会话更新端口来源／硬件对照，标清 BQ 原厂曲线、RTC VL 已拒用，以及 C02／完整充电移植仍未完成；本任务不修改共享索引、需求、决策或生成能力文档。

## 上机核对清单

- 先核对实际电芯允许值及现有配置读数：VREG 4400 mV、终止 60 mA、请求 1000 mA 量化 960 mA；本任务未重新验证充电驱动。
- C02 修复后，再核对再充关闭 200 ms、恢复后 500 ms 观察；电量计持续读失败也必须重新使能。当前版本不满足这条。
- 观察充／放电百分比走势、5% 步进、方向改变和满电持续 30 s；原厂 424860-2000 曲线不等于本机已校准。首次 3 s／读失败的未知电量不得引发误关机。
- 插拔 USB，核对电量显示、充电状态事件和接近零电流时的外电补判；C03 不代表外电判断已校准。
- 核对 RTC 走时、STOP 恢复、VL 未可信标记与可靠时间源恢复。

## 规模与来源

- 【实测】11 个原文文件与 `874538e` Git 对象逐字节一致，原厂 HEAD 仍为 `690a023`。
- 【实测】当前产品新增 70 行，原厂方法必要适配 176 行，胶水共 246/250 行（测试与文档不计）。命令：`python3 ports/esp32s31/tests/power_sources.py --baseline da9f76765f2d461590a870657eb3a0ad1e34152e`。
- 为什么不能更小：完整曲线／单向步进／满电确认与 RTC 初始化／校验／清标志需要保留整方法；更小方案包括继续读原始 SOC、仅加线性百分比、只过滤 VL，均遗漏本任务指定行为。原厂 C++ 直接编译所需构建文件也在禁改范围，故沿用板级既有的 C 适配入口。

## 架构审视

- 【实现】原厂芯片流程位于驱动层，只经 HAL／OSAL 向下；电量、充电状态和时钟继续进入现有系统状态／软总线，电源及设置能力保持原入口，人和 AI 的调用路径未改变。新增的滤波、初始化和寄存器清标志属于内部机制，不另造用户能力。

## 最终差异与隐私

- 【实测】`git diff da9f76765f2d461590a870657eb3a0ad1e34152e..HEAD --stat` 已核对：25 个文件均属指定驱动、电源测试、新 vendor 目录和本报告；无设备数据、设计稿或构建产物，单文件均小于 5 MiB。
- 【实测】按总规则第 6 节的路径／MAC 正则扫描，0 匹配（搜索退出 1 表示无匹配）；`git diff --check` 通过。仅本地提交，未推送、未开 PR。
- 复核证据保存在本地忽略目录 `work/`：普通全量、S31 普通／ASan、五组反向对照、GCC 与固件构建日志。所有可复现测试和来源校验脚本均已提交；无需 Git 历史的校验已注册到 CTest。
