# port-boot-1 执行报告

基线：`1450591caecfffdff023b19dc057fad5174a050f`。执行日期：2026-11-10。仅编译与宿主机验证，不连接设备；不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | S31 退出 0，编译警告/错误 0；端口普通/ASan 各 14/14；新反向对照 9/9 拒绝撤回变体 | 下列“复现命令”中的 S31、端口测试和 `boot_negative.py` |
| 2 | 【实测】测试与 gcccheck 通过；待最终隐私扫描 | 普通 334 通过、3 跳过；补跑 4/4（含前置）；ASan 全量 337/337；gcccheck 5 个宿主文件、7 个 S31 文件均 0 发现 | 下列模拟器、gcccheck、隐私命令 |
| 3 | 【实现】静态核对完成；实机边界见清单 | 下表覆盖系统、端口及 IDF 任务；看门狗配置逐项与原厂相等；RTC 和 panic 函数链接区已核对 | `python3 tests/esp32s31/stack_report.py build-s31`、工具链 `objdump -t build-s31/ink-s31.elf` |

## 没做到的

- 真机启动、显示、电源、复位与回出厂必须由复核者上机验证。
- RTC 断电不可靠，上电复位无条件清零；不能统计跨断电的失败。
- 本分支尚无资源映射和 `/user` 的真实实现；完整系统入口已接，但可用界面依赖 port-storage-1 合入。

## 和设计不一致的

- 【实现】原厂没有 panic 关屏钩子。正常 shutdown 复用 `trip()`；panic 时不能用它的板锁、I2C 驱动、任务延时，改用 LCD LL + ROM/LL GPIO，执行相同 PMIC 关电顺序。总线卡死时只能保证尝试关电，不能承诺电压已经撤除。
- 【实现】原厂浅睡保留在 `ON` 配置，试用版默认 `OFF`，这是任务书明确要求的差异。
- `hal_device_info_t` 没有版本字段；遵循现有界面契约，板名走 `hal_device_get_info()`，版本走 `hal_ota_info()` → `update.version`，未修改公共结构。

## 改过的已有测试

- `tests/power.c`：原浅睡断言全部保留，在显式 `ON` 下执行；同一测试新增 `OFF` 分支，检查不睡、不切板电、不重置 USB，且锁屏降频仍为 160 MHz。新增关机先调用停屏的断言。
- `tests/display.c`：原五类检查不变，新增 shutdown/panic 两类与 LCD LL 记录器。
- `tests/negative.py`：显式编译 `ON`，保持原反向对照验证浅睡实现。
- `tests/esp32s31/stack_report.py`：把报告中 power 配置从过时的 3072 更正为源码既有的 4096；没有增加实际栈。

## 要用户定的

- 暂无。

## 顺手发现

- 【实现】真实板信息使通信任务启动，而旧桩 `hal_cell_wait_event()` 返回 `-1`，违反接口的 1=事件/0=超时约定，被调用方当成事件并忙循环。为保证完整启动，只改此桩：按给定超时等待后返回 0；没有接触模组。新增宿主测试和撤回对照。
- 【实现】原厂任务看门狗未启用 panic，单纯饿死 idle 的情况只告警；自动回退统计的是实际发生的异常/看门狗复位，不能据此承诺所有死锁都会自行恢复。
- 【实测】首轮 S31 C/C++ 编译无警告，但 SDK 另报两次应用分区仅剩 1%（骨架单应用仅 1500 KiB）。原厂 `partitions.csv:3–12` 与已确认闪存用途均为两个 5 MiB 应用槽；本任务增加同值的编译描述并切换既有 SDK 分区选项，未改变任何真实分区，也不写设备。最终 S31 编译与正反尺寸检查待补。
- 原厂副本为稀疏/按需检出；读取未下载的音频/UI 对象时 Git 试图补取，被只读沙箱拒绝，副本未写入。后续只用已提供的板级源码和配置，未补全依赖。音频 26 KiB 沿用本项目既有值，未把注释当作本次原厂核查证据。

## 新增的机制

- `INK_S31_LIGHT_SLEEP` 编译开关（默认 OFF）：任务书第 2 条。
- RTC 校验记录和一次性 60 秒 `esp_timer`：任务书第 3 条；不新增常驻任务、文件或后台重试。
- shutdown 注册与 panic 链接包装、无调度 GPIO I2C 关电：任务书第 4 条；硬件路径不引入产品能力或页面。
- 含 Git 提交号及 UTC 构建时刻的版本：任务书第 5 条。
- 编译用原厂分区描述：任务书的闪存布局与 S31 零警告验收；沿用 IDF CSV 格式和原厂全部值，不新增分区机制，不提高真实的 5 MiB 上限。

## 要改的文档

- 合并后由主会话更新 `docs/tasks/backlog.md` 的端口状态；不改共享需求、决策或生成能力表。

## 任务与栈核查

【实现】栈单位均为字节，`任意` 表示不绑定核。系统任务配置未改；内部/PSRAM 由 `ports/esp32s31/osal.c:103–105` 映射。下表是静态配置，不能当作实机峰值测量。

| 任务 | 栈 | 优先级 | 核 | 栈位置 | 闪存与核查依据 |
| --- | ---: | ---: | --- | --- | --- |
| main / app_main | 16384 | 1 | 0 | 内部 | 启动回退在这里写 otadata；入口返回后 IDF 回收 |
| osal-test（短任务） | 2048 | 5 | 任意 | PSRAM | 只验证 OSAL，不访问闪存；随后自删除 |
| storage | 4096 | 3 | 任意 | 内部 | `/user` 读写、OTA 工作经此执行 |
| power | 4096 | 4 | 任意 | 内部 | 满足 IDF 睡眠对栈的限制；试用版不睡 |
| display | 3072 | 8 | 任意 | PSRAM | 处理帧缓冲，不直接写闪存 |
| ui | 10240 | 5 | 任意 | PSRAM | `/user` 经存储服务，资源映射由并行端口实现 |
| hub | 6144 | 6 | 任意 | PSRAM | 中枢消息和回调；持久化经存储服务 |
| exec.media | 8192 | 4 | 任意 | PSRAM | 媒体解码；不直接访问内部闪存 |
| exec.bg | 8192 | 1 | 任意 | PSRAM | OTA 写入经 `ink_storage_run`；更新文件在 `/content/.update`（卡） |
| audio（HAL 有输入/输出才建） | 26624 | 7 | 任意 | PSRAM | 编解码；本分支音频桩不建此任务 |
| comm | 8192 | 3 | 任意 | PSRAM | 事件/AT；无驱动时桩等待，数据写入走存储服务 |
| input-hal | 4096 | 24 | 0 | 内部 | I2C、输入，不能在此写闪存 |
| irq | 4096 | 10 | 0 | 内部 | GPIO 中断转任务，不写闪存 |
| epd_scan ×2 | 各 4096 | 23 | 各 0/1 | 内部 | 行生成，无闪存写 |
| epd_guard | 4096 | 24 | 1 | 内部 | 2 秒看护、异常停屏 |
| epd | 4096 | 5，扫描时 24 | 0 | 内部 | 波形、电源和扫描等待 |
| esp_timer（IDF） | 3584 | 22 | 0 | 内部 | 本任务的健康定时器复用此任务 |
| ipc0 / ipc1（IDF） | 各 1024 | 24 | 各 0/1 | 内部 | SDK 跨核调用，含闪存操作 |
| IDLE0 / IDLE1（IDF） | 各 1536 | 0 | 各 0/1 | 内部 | 两核均由 TWDT 检查 |
| Tmr Svc（FreeRTOS） | 2048 | 1 | 任意 | 内部 | SDK 软件定时器服务 |
| prvTaskDeleteWithCapsTask（短任务，IDF） | 1536 | 继承被删任务 | 当前核 | 内部 | OSAL 自检任务自删除时 SDK 临时清理，随后退出 |

【实现】系统任务来源：`system/services/{storage,power,display,hub,audio,comm}` 各服务的 `osal_task_cfg_t`，`system/services/res/exec.c:14–34,72–74`、`system/ui/ui_task.c:901–903`；端口来源：`hal_display_s31.c`、`hal_input_s31.c:102`、`s31_irq.c:16–17,55`。未发现上述 PSRAM 任务直接写内部闪存：`/user` 在内部栈 storage 上执行，OTA 的 `slot_begin/write/finish/valid` 与资源写入也经它执行（`update_service.c:312–340,369,402,426,965,1036`）。读内部闪存也受栈限制，资源端口需使用映射，不能在 PSRAM 任务调用裸分区读。

【估算】全部可选系统任务启用时，PSRAM 栈合计 70656 B；端口及系统常驻内部栈 32768 B，启动期间另有 main 16384 B、自检短任务 2048 B（PSRAM），均不含 IDF 任务/TCB/驱动堆。是否有足够余量及各栈高水位须上机测量。

原厂对照（2026-11-10 查阅，只读源码 `CloudZao/Metalio-E-INK4-Plus @690a023`）：

- `sdkconfig:2431–2439`：main 16384 B、核 0；本端口一致。
- IDF 默认任务：生成的 `build-s31/sdkconfig` 中 esp_timer 3584 B/核 0、IPC 1024 B、idle 1536 B、Tmr Svc 2048 B/优先级 1/不绑核，与原厂 `sdkconfig:2479,2498–2506,2969,2983–2993` 一致。优先级来源为 IDF `esp_system/include/esp_task.h`；自删除清理来源为 IDF `freertos/esp_additions/idf_additions.c:166–190`。网络/音频端口新增的 SDK 任务需在合入后补核查。
- `sdkconfig:2443–2458`：IWDT 800 ms、检查两核；TWDT 20 s、检查两核 idle、panic 关闭；本端口显式一致。
- `main/boards/common/power_policy/power_policy_standby_lp.cc:556–558`：standby_lp 4096 B、优先级 3、核 0、内部；本系统 power 4096 B、优先级 4、不绑核，按系统服务职责保留。
- `main/boards/metalio-e-ink4-plus/display/render.c:54,420–429`：两条 epd_prep 各 4096 B、优先级 24、各绑一核、PSRAM；本端口扫描栈同大小、内部、优先级 23，为已有独立 guard 留最高级。输入/扫描/看护的实机调度余量待测，没有把编译当作实时性能证据。
- `main/boards/metalio-e-ink4-plus/display/metalio_touch.c:35–36,298`、`input/metalio_keys.cc:211`：原厂触摸栈 4096 B、按键栈 3072 B，优先级均 5；本端口输入合并为 4096 B/24，并另有 irq 4096 B/10，沿用已有移植实现。其优先级高于扫描任务，是否造成行队列欠载必须上机验证，未擅自改调度。
- `main/boards/metalio-e-ink4-plus/bus/metalio_i2c1.c:55–83`：GPIO0/1 总线恢复和 5 us 脉冲；`display/epd_board_metalio_eink4_plus.c:929–949` 的关电顺序以及共享 `metalio_epd_power.c:130–140`：XOE 低至少 12 us → VCOM_CTRL/PWRUP 低 → WAKEUP 低。panic 使用相同引脚和顺序，10 ms 延时与原厂 `vTaskDelay(1)`（100 Hz）和既有 `trip()` 一致。

## 上机先核对清单

- 只使用原厂分区表和许可写入区域；核对正在运行 ota_1、ota_0 镜像有效。生成的分区表/bootloader 不用于设备写入。
- 合入存储、资源、日志、音频、Wi-Fi 后，从版本行、自检到首页；检查内部堆余量与每个任务栈高水位。资源首次映射和日志裸闪存写不能落到 PSRAM 栈调用方。
- 用逻辑分析仪/电压测量确认正常关机、软件重启和 panic 时 LCD 停止、XOE/MODE、PMIC 三脚顺序；SDA 卡死只能尽力关电。
- 有人看护时验证三次 60 秒内异常自动回出厂；验证满 60 秒清零和断电重置边界。此任务没有做实机异常注入。
- 默认 OFF 试用版锁屏后 USB 保持、CPU 降频和前光关闭；浅睡 ON 的唤醒与 USB 恢复未验证，不作为试用版验收结果。

## 过程与证据

- 【实测】开工工作区干净，HEAD 与指定基线一致；当前分支 `claude/codex-port-boot-1`。
- 【实现】基线已有 `ink_main()` 调用；本次保留并验证启动顺序，在自检和板级初始化前建立保护，缺屏幕从直接返回改为 panic，使保护能计入失败。
- 【实现】新增 RTC 校验、60 秒一次性定时器、第三次异常复位后选择原厂 `ota_0`；没有写出厂应用或其他受保护分区的代码。
- 【实现】浅睡编译开关默认关，关时不执行睡眠准备、切共享电源或 USB 重置；CPU 降频原逻辑保留。
- 【实现】shutdown 走既有 `trip()`；panic 用 IDF 链接包装、LCD LL 和 GPIO I2C，避免在异常中获取板锁/等待任务。原厂没有 panic 关电钩子；这是满足任务书第 4 条的端口实现，待验证电气效果。
- 【实测】模拟器构建完成（退出 0），普通全量测试进行中；Apple 链接器报告既有重复库警告，不能记为 S31 编译警告。
- 【实测】端口普通单测 14/14 通过（9.88 s）；`boot_negative.py` 的 4 个原实现全部通过、9 个撤回变体全部触发断言（编译退出 0，测试退出 -6）。覆盖阈值、RTC 校验、60 秒清零、上电清零、浅睡开关、shutdown、panic 栅极/电源以及无事件桩。
- 【实测】普通模拟器全量原话：`100% tests passed out of 337`、`Total Test time (real) = 850.04 sec`；其中 3 项中枢测试跳过，已找到可用的锁定依赖环境，待补跑。
- 当前产品改动 +222/-14，净增 208 行（含 CMake、配置和构建脚本，不含测试/文档），预算 250 行内。超过 100 行的原因：RTC 保护 50 行、无调度 panic 关电 67 行、真实设备/版本 43 行及必要接线。考虑过直接在 panic 调用 `trip()`，但它依赖锁与调度，会在异常中卡住；只拉低 XOE 又不能关 PMIC，因此采用固定、有限的 GPIO 写序列，没有通用 I2C 框架。
- 【实测】S31 以 `OFF` 编译成功，C/C++ 编译警告和错误均为 0；`ink-s31.bin` 1527344 B（不含并行任务资源包）。IDF size：DIRAM 静态 101024 B/503744 B，静态剩余 402720 B，不等于运行时空闲堆。版本为 `g5b36019b-261110140518`（产品提交 `5b36019b`，UTC 构建时刻）。
- 【实测】`objdump -t`：`ink_boot` 是 `.rtc_noinit` 中 16 B；`__wrap_esp_panic_handler`、`ink_s31_display_panic`、`ink_s31_panel_panic` 和两个 LCD 回调设置函数均在 `.iram0.text`。链接 map 确认 IDF panic 入口引用了包装函数。
- 【实测】端口 ASan+UBSan 14/14 通过（8.94 s，`UBSAN_OPTIONS=halt_on_error=1`）。既有输入/电源/板级反向对照 14/14、显示反向对照 7/7 全部拒绝变体，原实现均通过；未放宽断言和预算。
- 【实测】`ink-codex/tools/gcccheck.py`：宿主 `5 compiled, 0 with findings`；S31 `7 compiled, 0 with findings`；没有跳过文件。
- 【实测】模拟器 ASan+UBSan 全量原话：`100% tests passed out of 337`、`Total Test time (real) = 968.68 sec`；无跳过、无 sanitizer 报告。
- 【实现】架构审视：变更局限端口启动/异常机制；不新增用户动作或页面，不绕过既有能力总线，不新增 AI 私有通道。

## 复现命令

模拟器配置使用总规则 §5 的字体、Python、LVGL 参数，并设 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`。ASan 配置同一组参数，构建目录换 `build-asan`、加 `-DINK_SANITIZE=ON`。SDL2/ImGui 使用 `$INK_DEPS/build/_deps/{sdl2,imgui}-src` 的固定只读源码，不装依赖、不升级。

```bash
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" INK_S31_LIGHT_SLEEP=OFF taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2
# 端口内存检查：目录换 build-s31-host-asan，配置增加 -DINK_SANITIZE=ON。
taskpolicy -c utility python3 ports/esp32s31/tests/boot_negative.py build-s31-host
taskpolicy -c utility python3 ports/esp32s31/tests/negative.py build-s31-host
taskpolicy -c utility python3 ports/esp32s31/tests/display_negative.py build-s31-host
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31-host ports/esp32s31/tests/{boot,panic,power,display,stubs}.c
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31 ports/esp32s31/{s31_boot,s31_panic,hal_device_s31,hal_display_s31,hal_power_s31,hal_stubs}.c ports/esp32s31/main/main.c
# 隐私扫描：使用 codex-rules.md 第 6 节原命令，基线同本报告。
```

日志保存在忽略目录 `work/port-boot-1/`；S31 的尺寸、栈报告和链接 map 在 `build-s31/`，均未提交。
