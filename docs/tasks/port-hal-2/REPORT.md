# port-hal-2 执行报告

基线：`1a5a7355a5ac57371fb3e3eaa5af0b6e99994ce7`。仅编译和宿主机测试，不接设备，不读 ink-hw，不 push。核查日期：2026-11-10。里程碑 2：最终 S31 编译、模拟器普通全量 317 项、端口普通及 ASan/UBSan 单测均通过；模拟器 ASan/UBSan 全量进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】最终 S31 退出 0，0 条 `warning:` / `error:`；普通模拟器全量通过 | 原话：`100% tests passed out of 317`，717.52 秒；日志 `work/port-hal-2-host-tests.log`、`work/port-hal-2-s31-final.log` | `taskpolicy -c utility bash ports/esp32s31/build.sh`；总规则 §5 参数，构建与 ctest `-j2` |
| 2 | 【实测】普通、ASan/UBSan 各 2/2 通过（含双盖板键）；9/9 反向对照失败，两个原实现对照均通过 | `work/port-hal-2-unit-tests.log`、`work/port-hal-2-unit-asan-tests.log`、`work/port-hal-2-negative.log`；`gcccheck: 3 compiled, 0 with findings` | 下方复现命令；`python3 ../../tools/gcccheck.py build-s31-host ports/esp32s31/tests/logic.c ports/esp32s31/tests/power.c ports/esp32s31/hal_power_s31.c`（工具在 ink-codex 的 tools 目录） |
| 3 | 【实现】已列逐函数状态、资料行号和上机清单；隐私扫描待最终提交后执行 | 见下表；没有设备标识 | `git diff --check` 已退出 0 |

## 复现命令与证据

所有构建和测试串行安排，最多 `-j2`，没有启动窗口模拟器。依赖目录只读；`INK_DESIGN` 本任务未使用。

```bash
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2
taskpolicy -c utility python3 ports/esp32s31/tests/negative.py build-s31-host
# 内存检查同上，目录换 build-s31-host-asan，配置加 -DINK_SANITIZE=ON。
```

【实测】反向对照：去抖等待、90° 坐标转换、触摸抬起、运动原因分类、最终取消回调、最终忙检查、避免使用上次唤醒锁存、先恢复按键再恢复电源轨、PMIC 休眠期间不锁输入，分别撤掉后都编译退出 0、断言退出 −6；原实现两项退出 0。变更只在临时副本中，结束自动清理，不依赖历史提交。

## HAL 状态表

下表“已实现”是代码与编译层面，全部未经上机。行号对应本次仓库文件；资料核查而非实机测量。表中简写文件名分别位于 `ports/esp32s31/`、`ports/sim/hal/`、`boards/metalio_plus/`；档案为 `simulator/profiles/metalio-plus.json`。

| 函数 | 状态、实现位置 | 资料出处 | 上机需确认 |
| --- | --- | --- | --- |
| `hal_input_set_sink` | 已实现：回调和上下文成对登记，输入任务调用；`ports/esp32s31/hal_input_s31.c:111` | `platform/include/ink/hal_input.h:52`；`ports/sim/hal/hal_input_sim.c:814` | GPIO2/5 电平、中断与回调线程 |
| `hal_input_get_info` | 已实现：3 个扩展按键、BOOT/AI，成功初始化触摸后加 2 触点与 3 个盖板键 | `boards/metalio_plus/metalio_board.h:25`、`metalio_touch.h:13`；模拟器档案 108–134 行 | 这块板的 BOOT 和盖板布局 |
| `hal_power_request` | 已实现引用计数；独立外设的启停尚随其端口缺失；`hal_power_s31.c:47` | `platform/include/ink/hal_power.h:22`；`hal_power_sim.c:113` | P14 共享电源，后续音频/通信端口如何配合 |
| `hal_power_set_state` | 已实现 ACTIVE 持有最高频率锁，IDLE/LOCKED 释放；没有凭估计写触摸监视寄存器 | 模拟器档案 `metalio-plus.json:268`、287；`hal_power_sim.c:252` | 空闲 160 MHz、活跃 320 MHz 及外设时钟稳定性 |
| `hal_cpu_boost_create` | 已实现内部 RAM 小对象 + IDF `ESP_PM_CPU_FREQ_MAX` 锁；失败返回 NULL | `platform/include/ink/hal_power.h:50`；`hal_power_sim.c:267` | IDF 动态调频锁有效性 |
| `hal_cpu_boost_acquire` | 已实现同一对象重复持有不重复加锁 | `hal_power_sim.c:274` | 多个锁同时持有时不降频 |
| `hal_cpu_boost_release` | 已实现同一对象重复释放不重复解锁 | `hal_power_sim.c:281` | 最后一个锁退出后降频 |
| `hal_power_sleep` | 已实现本批板级顺序、拒绝/取消/回退、唤醒分类；屏总线和 TF 停启仍空，见“没做到的” | `hal_power_sim.c:353`、389、408；`boards/metalio_plus/metalio_board.c:56`、75、85、101、115 | 浅睡电流、GPIO2 锁存、RTC P16、运动与定时同时唤醒 |
| `hal_power_shutdown` | 已实现前光清零、调用 P13 关机脉冲；不是用 CPU 深睡代替整机关机 | `hal_power_sim.c:189`；`metalio_board.h:34`、`metalio_board.c:136` | 电源芯片实际断电、脉冲停止后是否仍返回 |
| `hal_power_reboot` | 已实现 `esp_restart()` | `platform/include/ink/hal_power.h:98` | 启动复位原因与共享外设的状态 |
| `hal_haptic_available` | 已实现扩展芯片与输入任务就绪后为真 | `metalio_board.h:22` | P03 极性和马达实际存在 |
| `hal_haptic_pulse` | 已实现异步 P03 脉冲；非正时长用板级 50 ms；共用输入任务，不另建马达任务 | `metalio_board.h:22`、36；`hal_power_sim.c:136` | 首次反馈 ≤150 ms、实际脉宽、结束电平 |
| `hal_power_battery` | 保留 port-hal-1 的既有实现 | `ports/esp32s31/s31_board.c:82`；port-hal-1 报告 | 电量/充电读数 |
| `hal_light_set` | 保留既有实现，本批接入睡前关前光 | `ports/esp32s31/s31_board.c:117`；port-hal-1 报告 | 前光极性 |

【实现】输入复用 TCA9555、FT6336U、板级盖板区域和 `ink_logical_to_native`。GPIO5 用现有 `s31_irq_watch` 登记，按住每 10 ms 采样，空帧停止采样。电源键和音量键读两次、稳定 20 ms 后送边沿；BOOT/AI 也去抖。原始边沿支持短按、长按持续时间、重叠按键和两个盖板键同时按住；HAL 本来没有“长按/组合键事件”类型，仍由既有输入服务识别（`system/services/input/input_service.c:21`、74、132）。真实端口的时间戳是采样时间，模拟器能知道精确物理时刻；没有冒充端到端实测时延。

【实现】浅睡：屏 PMIC 休眠 → 采样锁 → 加速度计静音或 10 Hz 低功耗运动监测 → 屏蔽扩展芯片 → GPIO2 最多等 30×20 ms 回高 → P17/P16 唤醒输入和 IDF 唤醒源 → 关前光 → P05/P14 断电（可保留 P14）→ 挂起本核调度、再查忙与取消 → IDF 浅睡。屏 PMIC 的等待期间仍能处理按键。IDF 负责睡眠期间停住另一核。醒后先锁存 IDF 原因，再读 P1/运动锁存，恢复按键、等 20 ms、恢复 P14/P05、恢复加速度计、释放输入任务。失败步骤也按已触及的步骤回退；恢复每步最多 3 次，间隔 10 ms，复用模拟器策略。

唤醒源：定时器；GPIO2 的 EXT1 低电平（P17 电源键、P16 RTC、SC7A20H INT1）；BOOT/AI 的 GPIO 低电平；保留模组电源时 SRDY GPIO3 的 EXT1 高电平。后两项的上机有效性未验证，SRDY 极性仍是假设。`esp_sleep_get_wakeup_causes()` 读多源位图，`esp_sleep_get_ext1_wakeup_status()` 读引脚位图，P1 与运动锁存在恢复方向前分类；P16 睡前不为高就不把 RTC 当唤醒输入。运动驱动出处：`drivers/chips/sc7a20h/sc7a20h.c:51`。

## 上机先核对清单

1. 由主会话按上机授权流程开始；本任务没有连接、烧录、监视或写设备。
2. GPIO2/5 空闲高、触发低；确认扩展芯片两口与加速度计都读完后 GPIO2 放开，GPIO5 在抬起后放开；确认 BOOT 是 GPIO61。
3. 20 ms 去抖：抖动不重复、短按有一对边沿、长按持续到抬起；电源/音量重叠按、两个盖板键同时按均不丢。
4. 触摸四角与拖动：FT 原始竖屏 684×1216 映射到原生横屏 1216×684；HOME/PREV/NEXT 的 X=100/200/300、Y≈1600；两触点 ID 稳定，抬起后没有持续读总线。
5. P16 必须先确认板卡接法：仓库记录它可能空闲为低（`docs/research/simulator/hardware-parity.md:41`）；低时只用定时与其他已启用源，不强行改 RTC 接线假设。
6. 浅睡电流待量，分别测保留/切断 P14；补齐屏总线、TF 停启并确认音频/相机/存储都安静后，才核对整板电流。不要用模拟器估算作为验收结果。
7. 核对定时、RTC、电源短按后立即释放、保持按住、运动单源与同时唤醒；波形上确认恢复方向前已读取 P1/INT1_SRC，确认 GPIO2 醒后仍能响应下一次按键。
8. CPU 160/320 MHz 和独立 boost 锁；前光关断；P03 马达低电平结束；P13 15 个 100 ms 高/低脉冲是否实际断电。SRDY 唤醒与 BOOT 唤醒分别验证，不预先算通过。

## 没做到的

- 模拟器 ASan/UBSan 全量检查待完成；端口 ASan/UBSan 2 项已通过。
- 新端口单测用独立 CMake 运行，未接入既有 CI 的根 CTest（不计入 317 项）；遵守只改端口的范围，根 CMake 与工作流未动。Linux 未在本机执行，C 源由真实 GCC 做过检查。
- 屏总线保持低、恢复输出、SDMMC 停启/唤醒重挂尚无对应端口，未编造占位延时；本批只接已有屏 PMIC 与板级电源轨。音频、相机、Flash 的真实忙状态还需未来端口接入；当前可查显示待处理、音频引用计数、功放和马达。调用方仍须先让系统安静。
- 没有上机测中断电平、去抖实际手感、睡眠电流、唤醒读取、时钟或振动时延。
- 恢复按键或电源轨连续三次 I2C 失败后仅记错误；按键方向未恢复时停止读扩展按键，没有再增加后台恢复队列或重试任务。

## 和设计不一致的

- 相比模拟器，输入采样锁在加速度计睡眠配置前获取，并保持到电源轨恢复之后才交回输入任务，避免 GPIO2 运动锁存被中途消费。PMIC 休眠期间不持锁。
- 模拟器为振动另建队列和任务；这里共用输入任务并合并重叠脉冲的截止时间。没有新队列，非重叠脉冲仍各自开/关 P03。
- 无法解释的 GPIO2 唤醒且电源键未启用时返回 0，不把它伪报成 RTC；`hal_power_sim.c:416` 的后备处理会计为 RTC。
- 上述缺少的屏总线/TF 流程未完成；不修改设计文档迁就现状。

## 改过的已有测试

暂无。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

- 输入任务：本任务要求按键去抖、触摸 10 ms 采样、向 sink 分发；栈 4096 字节、0 核、OSAL 最高优先级。马达也在此任务操作，没有再加马达任务。
- 输入采样锁及去抖/触点记录：本任务要求中断输入与睡眠顺序，避免屏蔽引脚时误报按键以及提前清唤醒锁存。
- `CONFIG_PM_ENABLE=y`：本任务要求 CPU 提频锁；自动浅睡保持关闭。没有新文件格式、持久设置、后台队列或缓存。

## 规模与架构审视

产品 `.c/.h` **+454/−25 行（净增 429）**；CMake/sdkconfig 另 +4/−2；测试与文档不计。为什么不能更小：这是替换输入、电源两组 HAL 桩，需去抖、两个触点及盖板键、醒后边沿、马达、IDF CPU 锁、睡眠进入/退出，超过 100 行并非单个小修。看过的更小方案：直接转发 IRQ 无法保证去抖与抬起；定时轮询所有设备会增加空闲耗电；让马达调用方等待会阻塞输入。采用既有芯片驱动与板级顺序，仅把纯逻辑提成可测头文件，独立测试留在端口目录，根 CMake 与系统/应用均未改。

【实现】R-061：没有新增产品动作、页面、服务或能力；现有输入服务和电源服务仍是唯一上层路径。硬件代码与测试限于端口，系统、应用不引用模拟器或 IDF 头；没有额外 AI 专用实现，也不在 HAL 绕过能力与策略。

## 要改的文档

主会话合并时更新 `docs/system/ui-dependencies.md` 与任务 backlog 的 S31 输入、电源状态；生成的能力表无需变更。共享文档本任务未改。
