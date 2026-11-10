# port-display-1 执行报告

基线：`d56cde2ad18d2394a01a851a427a0878cc1bfca9`。只编译和宿主机验证，未接触设备；没有推送或对外发布。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 三种 S31 完整构建都退出 0。本机副本及自动下载的编译警告均为 0；最终镜像分别比基线增加 38,896 B、38,928 B。离线提示“没有屏幕输出层”，镜像与基线相同 | `INK_UPSTREAM="$INK_UPSTREAM" taskpolicy -c utility bash ports/esp32s31/build.sh`；自动下载和离线命令见下 |
| 2 | 【实测】通过 | 端口普通、ASan/UBSan 均 7/7；反向对照 7/7 拒绝变异，基线通过 | `taskpolicy -c utility ctest --test-dir build-s31-host --output-on-failure -j2`；`taskpolicy -c utility python3 ports/esp32s31/tests/display_negative.py build-s31-host` |
| 3 | 【实测】通过 | 普通及 ASan/UBSan 全量各 322 项，初始跳过的 3 项均另行补齐；gcccheck 共 3 个编译单元、0 findings；隐私扫描 0 命中 | 总规则第 5、6 节；gcccheck 与补跑命令见下 |
| 4 | 【实现】已记录 | LGPL 义务、原厂行号、上机保护验证顺序及复原方法见后文；无实机结论 | 原厂源码核查；未连接设备 |

## 没做到的

- 真机验证不在本次授权内。LCD/DMA、I2C 和供电在宿主机里是记录调用的替身，不能证明电气断电、真实回调时刻、刷新速度或双核送行调度。
- 温度接口保留提示语义；PQ-BN 仍用已有 25℃标定，没有新增温度波形。面板只声明 GC16、GL16；DU/A2 请求回退为 GL16，并在回调中报告。没有新增异步区域并发：`async_updates=false`，单次刷新忙时返回 0，上层已有串行提交路径。

## 和设计不一致的

- 【实现】只链接原厂 `output_lcd/lcd_driver.c` 和 `output_common/line_queue.c`。不链接原厂 `epdiy/highlevel/metalio_epd`、波形表及 `bus/`；本项目已有 PQ-BN 算法和供电驱动，重复链接会引入第二套总线和扩展器锁存。
- 【实现】供电继续用 `hal_i2c_s31.c` → `tca9555/tps65185` → `metalio_epd_power.c`，与输入、休眠共用 `metalio_board` 的同一实例和锁。原厂重试失败后重启；已有本项目驱动返回错误，本端口断电并锁定失败，后续提交返回 0，重启后才重新初始化。
- 【实现】扫描、预填与空扫描沿用 `tools/hw-bench/main/panel_lab.c` 的 128 行队列、192 行预填、12 ms 相位、200 ms 扫描超时、2 s 独立保护；少行先送空行并丢掉迟到行，随后本次刷新失败并断电。
- 【实现】`pending` 在预热、保电期间也返回 1，较 HAL 注释里的“更新数”保守：既有 S31 睡眠入口只检查它是否非零，必须等屏电操作收尾才能报告空闲。严格更新计数与睡眠握手的统一留给 `port-sleep-vendor` 整合；本次未修改 HAL 合同。断电等待中到达的预唤醒在最终发布 `pending` 时重新读取，避免被旧快照覆盖。

## 改过的已有测试

- 无；未改变已有测试的期望。

## 要用户定的

- 暂无。

## 顺手发现

- 【实测】普通及 ASan 主机构建出现 `ld: warning: ignoring duplicate libraries: 'libink_port_sim.a'`；本任务没有修改相关链接配置，未处理。S31 编译没有此警告。

## 新增的机制

- 显示任务一个：HAL 合同要求在平台任务执行完成回调；单个请求槽，调用者返回前完成区域像素复制。
- 双核送行任务各一个、原厂行队列各一个：照任务书指定的 lab 传输方式。
- 保护任务一个：任务书第 3 条要求；扫描超时走同一断电入口，保护任务独立检查扫描心跳。先停 LCD、拉低 XOE/MODE，再经已有 PMIC 供电驱动关电。
- 没有新增持久格式、配置项、通用层或后台重试。

## 实现规模

- 产品 C 新增 303 行，构建/CI 新增 45 行；共新增 348、删除 2 行（净增 346），不含测试和文档，低于 350 行预算。超过 100 行的原因：原来的八个桩接口需要区域复制、PQ-BN 相位执行、原厂送行、任务回调和独立断电才能形成完整输出路径。更小的 `epd_hl_update` 方案会使用原厂波形，违背任务书的 PQ-BN 要求；把原厂整套板级编进来会重复控制 I2C、扩展芯片和 GPIO2 中断。因此只接送行层，其余复用已有代码。

## 构建与验证命令

环境沿用任务提供的 `INK_DEPS`；本机副本模式还需将 `INK_UPSTREAM` 指向原厂只读副本。所有构建最多 `-j2`，所有 ctest 最多 `-j2`，均通过 `taskpolicy -c utility` 运行。日志位于忽略的 `work/port-display-1/`，构建产物未提交。

```sh
# 本机副本
INK_UPSTREAM="$INK_UPSTREAM" taskpolicy -c utility bash ports/esp32s31/build.sh
# 离线：在自动下载前、缓存尚不存在时运行；用不可连通的本机代理阻断下载
env -u INK_UPSTREAM HTTPS_PROXY=http://127.0.0.1:9 HTTP_PROXY=http://127.0.0.1:9 ALL_PROXY=http://127.0.0.1:9 taskpolicy -c utility bash ports/esp32s31/build.sh
# 无 INK_UPSTREAM，自动获取到 build-s31/metalio-upstream 并核对完整 SHA
env -u INK_UPSTREAM taskpolicy -c utility bash ports/esp32s31/build.sh
```

【实测】基线和离线镜像均为 1,395,936 B；最终本机副本镜像 1,434,832 B；最终自动下载镜像 1,434,864 B。静态 DIRAM 从 99,114 B 增至 105,906 B（+6,792 B），PSRAM 静态 BSS 均为 569,304 B；动态分配和任务栈不包含在此表中。

CI 改动：`.github/workflows/s31-port.yml` 先跑端口宿主机测试和显示反向对照；S31 构建未设置 `INK_UPSTREAM`，走公开仓库固定提交下载；随后要求真实显示 HAL 对象存在，防止联网失败后悄悄只编空实现。没有触发或声称远端 CI 已运行。

【实测】核查 S31 的 `compile_commands.json`：原厂 `bus/` 编译单元为 0；本项目的 `hal_i2c_s31.c`、`tca9555.c`、`tps65185.c`、`metalio_epd_power.c` 各 1 个，共用已有驱动。

【实测】普通全量原话：`100% tests passed out of 322`，`Total Test time (real) = 977.97 sec`。ASan/UBSan 全量原话：`100% tests passed out of 322`，`Total Test time (real) = 1327.70 sec`。两轮起初均有 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 因通用 Python 缺依赖而跳过；改用依赖目录现成的锁定中枢环境补跑，普通轮含前置导出 `4/4` 通过（80.83 s），ASan 轮同为 `4/4` 通过（82.36 s），均无跳过。没有删除或改动测试的跳过条件。

端口独立测试覆盖输入、电源，以及显示的区域/步长/奇偶半字节拼接、灰度换算、调用方复用缓冲、GC16、平台线程回调、超时断电、独立保护、迟到行丢弃、断电中的预唤醒。普通轮 `100% tests passed out of 7`（0.57 s）；ASan/UBSan 轮同为 `7/7`（6.40 s）。反向对照均先编译成功，撤掉对应实现后退出 `-6`，原实现退出 0；不依赖 Git 历史。

gcccheck：`ports/esp32s31/tests/display.c` 为 `1 compiled, 0 with findings`；S31 的 `hal_display_s31.c`、`hal_stubs.c` 为 `2 compiled, 0 with findings`，均退出 0，无跳过。命令：`taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31-host ports/esp32s31/tests/display.c`；S31 一轮将构建目录换成 `build-s31`，文件换成上述两个端口源文件。

【实测】按总规则第 6 节扫描基线以来的完整提交差异，0 命中（grep 退出 1）；工作差异也为 0 命中。`git diff --check` 通过。差异仅含显示接入、构建/CI、相关测试和本报告，没有构建产物或上游源码。

主机普通及内存检查按[总规则第 5 节](../codex-rules.md#5-构建和测试)的 CMake 参数执行，另加 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`，并将并行数限制为 2、加 `taskpolicy -c utility`。端口使用[端口 README](../../../ports/esp32s31/README.md)中的独立测试命令；内存检查配置 `build-s31-host-asan`，加 `-DINK_SANITIZE=ON`。中枢补跑的配置和命令（两种构建目录各执行一次）：

```sh
taskpolicy -c utility cmake -S . -B build -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility ctest --test-dir build --output-on-failure -j1 -R '^hub-(contract|json-crosscheck|simulator-e2e)$'
```

## 原厂依据与上机先核对清单

查阅日期：2026-11-10，上游提交 `690a023c66862af103a3187464373ee7c5efcd14`。以下行号均指该提交的 `main/boards/metalio-e-ink4-plus/`。

- 【实现】`config.h:45–62`：并行脚；`display/displays.c:67–84`：1216×684、8 位、20 MHz。
- 【实现】`display/epd_board_metalio_eink4_plus.c:382–395`：LCD 的 CKV=60、前沿=4、LE=9、数据后空时钟=10；`:763–815`：WAKEUP 后 50 ms、UPSEQ0=0xE4、UPSEQ1=0x55、PWRUP 后 50 ms；`:863–898`：ENABLE=0x3F、VCOM、PG 检查；`:929–946`：XOE 先低至少 12 μs、再降 PWRUP/VCOM、一个 tick 后 WAKEUP 低；`:346–365`：SLEEP 等 120 ms。本端口调用既有供电实现，并在 XOE 回调明确等待 12 μs。
- 【实现】先读了原厂 `main/boards/common/power_policy/README.md`。整板浅睡的 LCD 拆卸、总线保持及恢复仍待与 `port-sleep-vendor` 接合；本任务没有宣称整板浅睡已经可用。

首次上机必须另获授权，按以下顺序；本次未执行：

1. 先准备原厂备份和已验证的复原工具，核对目标板版本、引脚、屏型号及 VCOM；不改分区、不写 eFuse、不碰 4G。
2. 使用单独的保护验证入口，禁止启动系统 UI、PQ-BN 和送行任务，XOE/MODE 保持低；仅升电、确认 TPS 应答及 PG=0xFA，然后武装同一保护入口并故意不给心跳。约 2 s 后必须观察到 XOE 保持低、VCOM/PWRUP/WAKEUP 降低、TPS 不再应答；不能只信会浮高的 PWR_GOOD。还须实测 I2C 故障时的响应。任一失败立即断电，禁止后续驱屏。
3. 保护验证通过才运行空行扫描，检查相位时长、行欠载和 GPIO 波形；然后黑白清屏、灰阶楔、区域拼接、回调耗时、预唤醒与断电；最后才接完整系统。
4. 任一步失败，停止刷新并切断设备电源，使用已有、验证过的备份恢复流程还原原厂镜像；重启核对原厂启动与屏幕。不在本任务内提供或执行写设备命令。

## 许可证与来源

- 【实现】原厂仓库总许可证为 MIT，但任务书和 D-124 指定其中 EPDiy 代码按 LGPL 处理；[EPDiy 官方项目](https://github.com/vroland/epdiy) 标注 LGPL-3.0，[组件元数据](https://github.com/vroland/epdiy/blob/main/idf_component.yml) 标注 LGPL-3.0-or-later，不能仅凭原厂总 LICENSE 视为 MIT。原厂源代码只在外部副本或构建目录，不放进本仓库，也不改其版权声明。
- 将来交付静态链接固件，须保留版权和许可告知，附 GPLv3/LGPLv3 文本、所用库的对应源码及修改和构建脚本，提供允许替换库并重新链接的应用目标代码或等效材料；适用时提供安装和运行修改版所需的信息，并允许为调试库修改进行逆向。只写下载链接或只交二进制不满足这些义务。依据：[LGPLv3 第 4 条](https://www.gnu.org/licenses/lgpl-3.0.html)（2026-11-10 查阅）。本次没有对外分发固件。
- 实际链接源码的 SHA-256：`lcd_driver.c` = `eacc7285a04315821a65282757b14c3092721d422b5c2490a70dd4f125366c4e`；`line_queue.c` = `f282e808d441212b66ad7d710e988d9287e962f27d19ffbdf6f489756c3ea881`。版本由完整提交号固定。

## 要改的文档

- 合并时更新共享的 `docs/system/ui-dependencies.md`：S31 屏幕已接原厂传输，但仅编译验证，单次刷新、整板睡眠总线接口待接。不改能力表：显示仍经原有服务和 HAL，未引入绕过能力的用户操作。
- `ports/esp32s31/README.md` 的“屏幕尚未接入”应在汇总端口状态时更新为本报告的证据边界，并补上 `INK_UPSTREAM` 的固定提交下载与离线回退说明。
