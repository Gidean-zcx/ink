# port-display-3 执行报告

基线：`281dbe7130514a602c68e4c6de2b7e53c81a5db1`。日期：2026-11-11。仅本地提交，不操作设备，不推送。

状态：输出层、TPS 上下电和保护适配已实现；共享板级 mask/restore 原文迁移未做，不能称整份任务书全部完成。最终主工程全量已通过。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1：874538e 输出层 | 通过【实测】 | 日志 `factory screen output layer @874538eb54f7f7eb593cd40cd1ba6dc8497226e7`；独立克隆 HEAD 相同；共享原厂 HEAD 始终 `690a023c66862af103a3187464373ee7c5efcd14`；编译数据库含 LCD/队列/新电源，各一份，旧电源/TPS 均零份 | `INK_UPSTREAM=… taskpolicy -c utility bash ports/esp32s31/build.sh`；`git -C build-s31/metalio-upstream rev-parse HEAD` |
| 2：S31 宿主全量 | 通过【实测】 | `100% tests passed out of 33` | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2` |
| 2：撤销修复对照 | 通过【实测】 | HAL 19/19 拒绝；暂停入口关电失败：基线中止，撤修复后错误继续执行；电源 4/4、原厂函数 3/3 拒绝；IRAM 源码/配置静态对照 4/4 拒绝。正常对照均成功，变体编译均成功 | 下方三条 `display_*` 手动命令 |
| 2：主工程全量 | 最终通过【实测】 | 最终原话 `100% tests passed out of 391`，467.49 秒、零跳过；三个 Hub 测试及 `integration-races` 均通过。首次失败记录见“顺手发现” | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 2：S31 固件 | 通过【实测】 | `build.sh` 编译退出 0；无警告；未退回显示桩 | `INK_UPSTREAM=… taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 3：上机清单 | 已列出 | 见文末；无真机通过结论 | 本任务不接设备 |
| B01 | 通过【实现／实测】 | `hal_display_s31.c:68,189`：abort 先等旧 IRQ 退出，再清回调、旧完成信号和复用队列；panic 保留寄存器路径；超时后新刷新恢复 | `display_negative.py`；`display_factory.py` |
| B02 | 通过软件核查【实现／实测】 | 原厂 `start_ckv_cycles:340` 加 IRAM；队列指针表 INTERNAL；GDMA 两项配置带入。最终 ELF 核对 19 个 IRQ 相关符号均在 `.iram0.text`，所查正常路径的直接调用位于 IRAM 或 ROM；间接回调核对到 HAL。`gpio_reset_pin` 仅在任务侧 deinit，不在 LCD ISR | `riscv32-esp-elf-objdump -t/-d build-s31/ink-s31.elf`；`display_factory.py` |
| B04 | 通过软件核查【实现／实测】 | 原厂完整时序原文、实际 684 行及 0 dummy bytes；HAL 断言扫描行数。PQ-BN 源码及接口未改，灰阶等效未上机确认 | `display_factory.py`；S31 编译 |
| B05 | 屏幕路径已处理；共享 mask/restore 未完成 | 关电写失败保留 powered/armed，禁止新刷新；guard 可继续保护，不报告控制脚已确认低；相机访问失败，void Flash 暂停入口停止执行。保持原 guard、自检、panic 接口 | `display_power.c`、`display_negative.py`；未做部分见下 |
| B13：TPS | STOP 分隔读通过【实测】 | 原厂寄存器函数原文经 HAL 接线；写寄存器地址和独立读取共同持现有板级锁，读取失败不覆盖输出；TCA 不在本任务修改范围 | `display_power_negative.py` |
| 内存检查 | 通过【实测】 | S31 宿主 ASan + UBSan 全量 33/33；主工程按 `vendor-common.md` 第 7 条不跑 ASan | `cmake -S ports/esp32s31/tests -B build-s31-host-asan -DINK_SANITIZE=ON`；构建 `-j2`、ctest `-j2` |
| gcccheck | 通过【实测】 | 宿主 `2 compiled, 0 with findings`；S31 `4 compiled, 0 with findings` | 下方两条 gcccheck 命令 |
| 收尾 | 通过【实测】 | 基线差分检查通过；21 个文件均在允许范围、均小于 5 MB；本地工作树干净；隐私正则扫描 0 匹配（grep 退出 1） | `git diff --check`、`git diff --stat`、任务规则第 6 条隐私命令 |

复现补充（所有构建/测试命令前均用 `taskpolicy -c utility`，同一时间仅运行一轮，构建和 ctest 均不超过 `-j2`）：

```sh
# 主工程使用 codex-rules.md 第 5 节配置，另加已有依赖路径：
# -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src"
# -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"
# -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-s31-host -j2
python3 ports/esp32s31/tests/display_negative.py build-s31-host
python3 ports/esp32s31/tests/display_power_negative.py build-s31-host
python3 ports/esp32s31/tests/display_factory.py build-s31/metalio-upstream build-s31-host
python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31-host ports/esp32s31/tests/display.c ports/esp32s31/tests/display_power.c
python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31 ports/esp32s31/hal_display_s31.c ports/esp32s31/vendor/metalio_display/power.c build-s31/metalio-upstream/main/boards/metalio-e-ink4-plus/display/output_lcd/lcd_driver.c build-s31/metalio-upstream/main/boards/metalio-e-ink4-plus/display/output_common/line_queue.c
```

S31 使用 `INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"`。来源路径、每个原文件和拷贝的 SHA-256、适配内容见 [SOURCE.md](../../../ports/esp32s31/vendor/metalio_display/SOURCE.md)。

规模【实测】：胶水新增 **198 行非空产品代码**，包含 CMake 37、HAL 17、配置 2、电源适配 131、私有头 5、原厂函数内指标/失败返回适配 4、IRAM 补丁 2；不计未改动原文、测试、文档、注释和空行。没有超过 200 行预算。为什么不能更小：只换提交和 abort 顺序无法覆盖 TPS STOP、原厂上下电与失败确认；保留旧电源只补布尔值也不符合直接复用要求。现有 TCA 单实例和锁继续负责共享输出，没有另设一份确认状态。

## 没做到的

- **原厂整套 `ioexp_mask_for_sleep/ioexp_restore_keys` 没有替换。** 它们的当前入口在不允许修改的 `boards/metalio_plus/metalio_board.c`，且涉及两口共享状态。没有再造第二个输出管理器或超过胶水预算。现有入口保持原状，不能说已按原文复用；需主会话与 port-board-2 收口。
- 没有迁入原厂 `bus_hold_low/latch/reinit` 整套睡眠总线流程。当前屏幕只初始化一次，原 `bus_restore` 调用接为空操作；这不是新版睡眠恢复已通过的证据。
- HAL 不能传入原厂锁等待 100 ms、TPS 事务 1000 ms：目前沿用现有板级锁和 HAL 超时。STOP 顺序已做，不把超时语义称为等价。
- 无真机刷新、灰阶、IRAM 断缓存或物理高压断轨实测；“关电确认”仅指已有 TCA 写入/读回状态，不是电压测量。

## 和设计不一致的

- 以上未迁移路径和超时差异仍存在，没有改设计文件掩盖，也没有本次真机证据支持永久保留。
- 原厂失败重启改为 HAL 返回失败，依 `vendor-common.md` 第 2 条保留本项目安全返回策略；错误结果汇总为 `EPD_POWER_RAILS_FAILED`，具体寄存器原因仍有原厂日志。
- 原厂 85 Hz 的高层渲染器没有搬入；本任务保持 PQ-BN 及既有 12 ms 相间契约。最终原生扫描时序需要重新看屏，不能宣称灰阶校准等效。

## 改过的已有测试

- `display.c`：记录真实二值完成信号、LCD active 和 abort 边界，补旧 EOF/信号、未确认关电及后续恢复场景；既有断言未删、未放宽。
- `display_negative.py`：自检撤修复锚点跟随新实现；新增 B01/B05 对照，保留旧对照。测试日志拷贝加 `%.159s`，修 GCC 发现的原测试缓冲截断警告。
- S31 CMake 增加三个显示相关测试项；未修改既有测试预期或预算。

## 要用户定的

- 由主会话明确共享 mask/restore 与睡眠总线恢复的实施归属，推荐交给持有共享板级状态的任务统一接入；本任务不把推荐升级为已完成需求。
- TPS 事务超时按原厂接入需要板级 HAL 配合，推荐保留单一锁所有者、提供原厂对应调用语义。

## 顺手发现

- 首轮原话 `99% tests passed, 1 tests failed out of 391`，683.10 秒，Hub 三项跳过；`integration-races` 在 `tests/integration/races.c:120` 的主人收件箱检查失败，单独复跑通过。该测试及其模拟器/系统/驱动输入相对基线无改动；没有改它或加等待掩盖。使用已有 Hub 环境后的完整复跑 391/391 通过、零跳过，未改测试断言。
- 默认构建 Python 缺 Hub 依赖；已有 `$INK_DEPS/hub/.venv` 有所需四个模块，切换测试解释器，未安装或升级依赖。
- R-061：变化在原有 HAL 下，系统、UI、能力表及 AI 调用路径不变；原始 I2C、扫描和保护仍是内部机制，不新增用户能力。

## 新增的机制

- `CONFIG_GDMA_CTRL_FUNC_IN_IRAM=y`：任务第 3 条/B02 要求，取原厂配置。
- `CONFIG_GDMA_ISR_HANDLER_IN_IRAM=y`：同上。
- 常驻任务、状态机、持久格式、缓存、队列：新增 0；沿用既有扫描任务与 guard。独立构建克隆是任务第 1 条要求。

## 要改的文档

主会话合并时更新 backlog/审计状态，注明 mask/restore 与总线恢复未完、TPS 超时待接入。共享文档、需求、想法、决策和能力生成文件均未修改。

## 上机核对清单

- 确认新构建使用 874538e、浅睡仍关闭；先核对最终 sdkconfig 与 ELF，再接屏。
- 保护自检日志：`rails_up=1 tripped=1 asleep=1`，`after_ms` 约 2 秒；确认 XOE 先低至少 12 µs，不能用 I2C 无响应单独证明断轨。
- 整屏、局部连续刷新、换页，日志 `refresh N ... ms= ... missed=0`；GC16 清洁和 GL16 一遍按原策略工作。
- **用户目测** 16 级灰阶、文字深浅/粗细、白边与首末有效行；比较相同温度和图案。684 行、CKV、SPV 间隔需核对原生时序。
- 超时中止后下一帧正常，无旧帧补数或错误完成；原有三次连续刷新失败保护仍在。
- I2C 写失败时显示保持停止、保护仍武装，日志明确 `power-off unconfirmed`；恢复成功后确认软件状态与控制脚吻合。
