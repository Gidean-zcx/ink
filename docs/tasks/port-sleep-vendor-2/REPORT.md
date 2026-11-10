# port-sleep-vendor-2 执行报告

基线：`2a31195a489bf08e3b7ae850b64b04ab522310a1`。执行日期：2026-11-10。
范围：S31 浅睡执行环境、切片与唤醒路径；只编译、跑宿主机测试，不连接设备。

【实现】最小修复为电源服务任务配置增加 5 行：仅 S31 构建固定到 0 号核、优先级 idle+3；沿用已有 4096 字节内部栈。模拟器仍为不固定核、优先级 4。没有新增睡眠任务。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】S31构建退出0、0条编译warning/error；端口普通与ASan/UBSan均 `100% tests passed out of 4`；新增6项反向对照全部编译0、断言退出−6，原有14项也全部拒绝 | 下方复现命令；`work/s31-build.log`、`work/port{,-asan}-tests.log`、`work/task-negative.log`、`work/port-negative.log` |
| 2 | 进行中 | 【实测】模拟器普通版 `100% tests passed out of 330`，684.77秒；gcccheck共9个编译单元，0 findings；待内存版全量与最终隐私扫描 | 下方命令；`work/sim-tests.log`、`work/gcc-{service,port}.log` |
| 3 | 通过 | 【实现】下方对照表覆盖任务/核/栈/优先级、切片、硬件顺序和等待；7项上机核对步骤含按键唤醒与断电后自动恢复 | 文件与行号见下表 |

## 复现命令与证据

所有构建与测试串行，最多 `-j2`，命令带 `taskpolicy -c utility`。依赖只读；证据在被忽略的work/build目录，不提交产物。`INK_DEPS`、`INK_DESIGN`按用户提供设置。

```bash
taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2
taskpolicy -c utility python3 ports/esp32s31/tests/task_config.py build-s31-host --negative
taskpolicy -c utility python3 ports/esp32s31/tests/negative.py build-s31-host
```

端口内存检查仅改目录为`build-s31-host-asan`并加`-DINK_SANITIZE=ON`，ctest前设置`UBSAN_OPTIONS=halt_on_error=1`。普通版3.14秒，ASan/UBSan版5.93秒，均4/4通过。新配置检查在内存版内也以ASan/UBSan编译执行；覆盖S31配置与模拟器原配置。首次配置检查缺macOS SDK sysroot导致编译失败，已给隔离编译器补入xcrun返回的sysroot后通过，没有改产品代码或放宽断言。

【实测】新增反向对照：不固定核、改核1、改优先级4、外部栈、栈缩成3072、OSAL内外栈能力映射颠倒，各自编译退出0、断言退出−6；S31和模拟器配置的未修改对照均退出0。临时副本退出时清理，无Git历史依赖。原有14项反向对照及3个未修改对照继续全部通过。

模拟器普通与内存版复现配置相同，内存版仅换为`build-asan`、加`-DINK_SANITIZE=ON`：

```bash
export PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" INK_KEEP_EVIDENCE=1
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"
taskpolicy -c utility cmake --build build -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility python3 ../../tools/gcccheck.py build system/services/power/power_service.c
taskpolicy -c utility python3 ../../tools/gcccheck.py build-s31-host \
  ports/esp32s31/hal_power_s31.c ports/esp32s31/tests/logic.c ports/esp32s31/tests/power.c \
  ports/esp32s31/tests/board.c boards/metalio_plus/metalio_board.c \
  boards/metalio_plus/metalio_epd_power.c drivers/chips/tca9555/tca9555.c drivers/chips/tps65185/tps65185.c
```

【实测】普通版全量结果原话：`100% tests passed out of 330`，`Total Test time (real) = 684.77 sec`，没有跳过。gcccheck原话分别为`1 compiled, 0 with findings`、`8 compiled, 0 with findings`，无跳过文件。S31实际编译命令包含`ESP_PLATFORM`，确认本次条件分支已参与目标构建。

## 原厂一步 → 我们一步

【实现】核查日期 2026-11-10；原厂 [CloudZao/Metalio-E-INK4-Plus，690a023](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/690a023c66862af103a3187464373ee7c5efcd14)。表中原厂路径：`L` = `main/boards/common/power_policy/power_policy_standby_lp.cc`，`H` = 同目录 `power_hw.cc`，`R` = 同目录 `README.md`，`V` = 同目录 `s31_sleep/`，`B` = `main/boards/metalio-e-ink4-plus/display/epd_board_metalio_eink4_plus.c`，`K` = 同板 `input/metalio_keys.cc`。

本项目路径：`S` = [电源服务](../../../system/services/power/power_service.c)，`P` = [电源 HAL](../../../ports/esp32s31/hal_power_s31.c)，`O` = [S31 OSAL](../../../ports/esp32s31/osal.c)，`M` = [板级 IO](../../../boards/metalio_plus/metalio_board.c)，`E` = [屏幕电源](../../../boards/metalio_plus/metalio_epd_power.c)，`I` = [输入 HAL](../../../ports/esp32s31/hal_input_s31.c)。下表是代码核查，不能当作本次实机测量。

| 原厂一步（出处） | 我们一步（出处） | 核查结论 |
| --- | --- | --- |
| `standby_lp` 专职任务，L:551 | 复用已有 `power` 服务任务，S:456 | 任务书允许；没有中转线程、通知队列或新栈 |
| 固定核 0，L:557 | `cfg.core = 0`，S:459；O:104 | 本次修复；最终传到 FreeRTOS 创建调用 |
| 栈 4096 字节、INTERNAL\|8BIT，L:557 | S:47、S:456 默认 flags=0；O:103 | 原来已满足；补配置及能力标志断言 |
| idle+3，低于按键任务 5，L:557、K:211 | S:460 的 idle+3，低于 I:102 的输入任务 24 | 本次修复；保持“输入先处理，睡眠后处理”的相对关系 |
| 最长 60 秒，`power_policy_priv.h:31`、L:268 | S:52、S:267 的 60000000 µs | 一致；每次 HAL 调用重新设置定时器，P:88 |
| 剩余关机时间不足 60 秒则缩短，最少 1 ms，L:227、L:268 | S:279 的闹钟期限缩短；≤1 ms 时保持清醒让到期任务运行 | 既有差异：本项目没有原厂待机累计关机设置；没有擅加该产品策略 |
| 每片前检查待机状态、退出请求、关机期限和按键，L:341、L:360 | S:309 主循环重读状态；S:246 重查占用、刷屏/存储和到期任务 | 保留本项目策略；定时返回继续待机，解锁或忙碌就不再入睡 |
| SD、开机和网络/音频真正空闲，L:196、H:378 | S:251、S:262 的 holds/quiet，P:120、P:122 | 业务门禁保留；SDMMC 停启端口仍未接入 |
| SD 停放，H:394 | P:135 明确缺少 SDMMC park/restore | 未实现，沿用前任务边界，没有伪造 |
| TPS 休眠；P00/P01低→50 ms→P02低→120 ms；已低仅等120 ms，H:398、B:346 | P:136→E:156 | 一致；原有板级测试继续覆盖 |
| 屏总线拉低，H:402 | P:135 | 总线端口仍缺，未实现 |
| 释放加速度计共享 INT 后 mask 按键，H:407 | P:137 先暂停采样，再配置运动并 mask | 保留任务要求的运动唤醒；防止采样先清掉中断锁存 |
| 安全锁存→全输出，B:478 | M:56 | P00～P02/P06/P07低；P03/P04/P05保持；P10/P11/P17高；P12/P14保持；P13/P15/P16低，一致 |
| GPIO2 输入/上拉/无下拉，每20 ms等一次、最多600 ms，H:85 | P:141、P:143 | 一致；arm 后、断电后再检查，不空闲就回退 |
| P17输入→GPIO2 EXT1 ANY_LOW，H:419、H:424 | M:74、P:89、P:147 | 链路一致；GPIO61不作为浅睡唤醒 |
| 原厂不把P16作为唤醒源，R:134 | M:79 先建RTC基线，再启POWER；P:126、`s31_input_logic.h:99` | 按任务保留；`MB_RTC_PENDING`使用已实测的高有效，未改成原厂低有效 |
| 原厂禁用运动唤醒，H:407 | P:138、`s31_input_logic.h:100` | 按任务保留运动唤醒和独立原因分类 |
| 按切片配置定时器，H:429 | P:87，时长来自S:267、S:285 | 时长一致；本项目先配timer再配EXT1，均在断电与入睡之前 |
| 前光0，停modem/音频→P05→P14，H:438、B:292 | P:149→M:92 | 轨顺序一致；既有keep_modem保电；真实modem/音频收尾尚缺 |
| 总线hold，H:446 | P:135 | 缺总线端口，未实现 |
| 补丁浅睡：PSRAM→XTAL、MSPI/MPLL停启、DREG/PVT和模拟初始化，H:452、V/s31_sleep_sdk.cmake:16、V/s31_sleep.cc:46 | P:155→`s31_sleep_bridge.cc:8`→vendor目录同名文件 | 沿用已移植补丁、SDK检查和三个wrap；本次未改 |
| 浅睡返回先锁存P17，再恢复方向，H:455 | P:166→`s31_input_logic.h:97` | 一致；加上RTC/运动原因，已松开的短按仍能识别 |
| 关闭唤醒配置、恢复按键、稳定20 ms，H:461、K:222 | P:171→M:108→P:174 | 一致；输入采样到恢复末尾才放行 |
| 恢复P14→P05、前光保持灭，H:467、B:303 | P:177→M:122 | 供电顺序一致；既有保电分支保留 |
| 非定时唤醒SD初始化后USB恢复，定时跳过，H:474、H:167 | P:178 | SD端口仍缺；当前仅在电源轨恢复后执行USB |
| USB开时钟→pad断开150 ms→PHY默认→接回→每20 ms等、最多800 ms，H:135 | P:100 | 顺序和等待一致；宿主机检查，不代表实机枚举成功 |
| 恢复屏总线，H:478 | P:135 | 缺总线端口，未实现 |
| timer返回不退出待机；再检查后睡下一片，L:303、L:360 | S:298、S:344 | 一致；额外处理电量、RTC清告警和拿起亮屏；不主动恢复USB |
| POWER短按退出，长按关机，L:64、L:79 | I:125交付唤醒，S:54、S:301最多等1秒输入确认解锁 | 保留现有输入/锁屏分工；未经设备验证 |
| 入睡取消后按已执行步骤回退，H:177 | P:109、P:170按标记恢复；已有最多3次、间隔10 ms的恢复尝试 | 沿用既有故障处理，未新增重试；与原厂回退实现不同 |

## 上机核对步骤（以后用户在场时用）

本次没有执行以下步骤。必须由后续会话获得用户当次上机授权后执行；现有端口骨架在无屏幕时会返回（`ports/esp32s31/main/main.c:12`），测试程序应走本次已配置的power任务，不能再从app_main或任意外部栈任务直接调用HAL冒充同环境。

1. **先准备恢复，再做睡眠**：核对已有出厂备份与恢复方案；确认只有一个进程接管USB、恢复程序能等待重新连上并自动写回本轮改过的ota_1/otadata，写后校验并停回下载模式（现有`tools/hw-selftest/run.py:379`恢复循环、`:475`恢复入口）。不要改引导程序、分区、NVS、eFuse。核对控制台只走USB，避免UART0的GPIO58/59把日志送入模组（实机记录第42行）。未准备好自动写回就不开始浅睡。
2. **清醒状态检查**：确认运行任务是power、核0、优先级3、栈4096且在内部RAM；显示/存储/音频收尾完成。确认GPIO2空闲高；P16空闲0、告警1、清除回0（`docs/hardware/2026-11-10-selftest.md:98`的第六轮）。保持P16高有效，运动锁存读后GPIO2应释放。
3. **先测电源键可唤醒**：第一片就用原厂60秒定时+GPIO2 EXT1，不另猜更短硬件参数。在入睡后由用户短按POWER，检查浅睡调用返回、原因含POWER、方向先恢复、电源轨P14→P05、USB恢复，PSRAM既有校验和不变、没有重启。短按松手与仍按住各测一次，确认没有假音量键和二次进待机。
4. **再测定时切片**：通过已有诊断或延后读取的记录核对至少3片：每片60000000 µs，timer醒后保持锁屏、不重枚举USB，重查状态后再睡；最后短按POWER读取证据。USB静默不能判断CPU没醒；不以USB消失作为成功证据。
5. **再测RTC、运动和提前到期**：设置已支持的近闹钟，确认其期限缩短切片且事件处理后重新评估；分别触发P16告警和加速度计运动，检查唤醒原因不误报POWER、P16清告警回0。运动按已有拿起亮屏规则判断，平放轻碰不能直接解锁。GPIO61只检查清醒按键功能，不能期待它唤醒浅睡。
6. **核对每一步等待与外围边界**：查看TPS 50/120 ms、GPIO2最多600 ms、键20 ms、USB150+最多800 ms，以及P05/P14顺序。SD和屏总线端口补齐以后，还必须单独核对SD停放/恢复→USB→屏总线和长期循环，才能验收整板；本任务没有完成这些端口。
7. **卡住立即结束本轮并恢复**：先短按POWER；USB不回来不能反复启动刷写进程抢端口。按第七轮已有救援记录，由用户拔线、按住电源15秒、插线开机；15秒仅是该轮操作记录，硬件强制断电阈值仍未知。恢复程序见USB重新出现后自动写回并校验，确认出厂固件回到原状、停在下载模式。睡眠会暂停RTC看门狗，不能靠上轮“保险丝”兜底（实机记录第七轮）。

## 没做到的

- 本次不上机；编译及宿主机断言不能证明实际可唤醒或 USB 可恢复。没有运行Linux测试。
- SDMMC/CNNT、屏总线park/hold/restore、真实modem/音频收尾和完整板级诊断重放仍缺，沿用前任务边界；未改任务外端口。

## 和设计不一致的

- 与原厂的保留差异及缺口见对照表。本项目默认10分钟自动锁屏、空闲160 MHz、闹钟和电量策略没有改成原厂180秒/320 MHz/累计30分钟关机；任务仅允许电源服务的任务配置变化。
- 浅睡外围仍保留原有本核调度暂停与最后cancel/busy检查（P:153），原厂H:452直接调用补丁；本次没有删除已有取消保护。
- 原厂入睡拒绝后等待20 ms（L:287），本项目既有服务等待200 ms（S:293）；属于服务策略，按任务的配置范围不改，不能宣称整条路径与原厂完全一致。

## 改过的已有测试

- `power.c`只新增连续60秒定时片和提前到期时长检查；没有修改已有断言、期望、预算或等待。
- `task_config.py`为新增隔离编译检查：从真实服务取任务初始化语句、从真实S31 OSAL取创建函数，断言最终FreeRTOS实参；不依赖Git历史。它不执行完整服务调度，完整行为由模拟器全量覆盖。

## 要用户定的

- 暂无；按任务约束继续可逆工作。

## 顺手发现

- 基线与用户指定一致，工作树干净，沿当前任务分支继续。
- 普通宿主机构建有既有SDL的macOS API弃用及重复库链接提示，未放宽警告；S31编译warning/error均为0。
- 根工程的`unit-s31-osal-ticks`在sanitizer选项之前定义，已有单项不受插桩；本次端口内存版9个C编译单元全部带ASan/UBSan，新增配置隔离程序也带插桩。

## 新增的机制

- 无新增产品机制、任务、状态机、文件格式、设置项、缓存或重试。仅调整任务书要求的S31任务配置。

## 要改的文档

- 主会话合并时同步端口状态：power任务已改核0、idle+3、4096内部栈；整板浅睡与SD/屏总线仍待后续端口和有人在场的实机验证。无需新增能力或AI快照；R-061边界仍为平台内部机制。
