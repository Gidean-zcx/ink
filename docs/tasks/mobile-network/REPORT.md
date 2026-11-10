# mobile-network 执行报告

基线：`9b0282dd7764f5c99e5a1a9feeb5248888269028`，已包含 `sms-multipart-2`。本地实现与验证结束，存在下列未完成项；没有推送或操作设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 接入点 | 【实测】通过 | 页面输入并保存，模型读回正确的上下文和认证；切回自动清除手动值；AI 可改非秘密字段，不能代填密码；输入、页面、AI 上下文、能力回执、活动日志不含密码内容，私有集合拒绝 AI 读取 | 下列 `mobile-network` 测试及反向对照 |
| 1 漫游 | 【实测】通过 | 默认关闭；注入 CEREG 漫游后停用数据且不再累计接口字节；允许后恢复，关闭后停用，回到归属网后恢复；通话与短信仍保持注册 | 同上 |
| 1 流量 | 【实测】通过，有分钟采样边界 | 接口接收 300 B、发送 700 B 合计 1000 B；跨本地月界归上月，新月另计 100 B；下一天落盘后再增加 50 B，重启保留 100 B、上月 1000 B；人确认清零后，再次重启仍为零；页面随统计更新 | 同上 |
| 1 儿童权限 | 【实测】通过 | 接入点、漫游、清零的触摸和 AI 调用均拒绝，设置别名也不能绕过 | 同上 |
| 2 原网络、通信、飞行模式 | 【实测】通过 | 最终两套全量中的相应原测试均通过；确定性和资源预算未放宽 | 下列两轮全量命令 |
| 2 普通全量 | 【实测】328/330 通过，2 项文档检查失败 | 原话 `99% tests passed, 2 tests failed out of 330`，805.72 秒；仅 `capability-table-doc`、`services-mid-capability-doc` 失败，原因是禁止本任务修改的生成能力文档过期 | `ctest --test-dir build --output-on-failure -j2` |
| 2 内存检查 | 【实测】328/330 通过，同样 2 项文档检查失败 | 原话 `99% tests passed, 2 tests failed out of 330`，1177.27 秒；全部 330 项均跑过，无 ASan/UBSan 错误，日志扫描命中 0 | `ctest --test-dir build-asan --output-on-failure -j2` |
| 2 S31 | 【实测】编译通过 | 构建退出 0；Flash 1,378,498、DIRAM 98,378、外部 BSS 623,664 字节；不含运行时堆和任务栈 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2 GCC | 【实测】通过 | `gcccheck: 10 compiled, 0 with findings`；S31 桩不在宿主数据库，已随 S31 固件编译 | `gcccheck.py build` 加全部改动的 C 文件 |
| 3 上机边界 | 【实现】遵守 | 本次没有设备、串口、烧录或真实接入点操作；只读核对清单见末尾 | 无设备命令 |
| 4 中英文 | 【实测】新增两页通过；公共确认卡英文未做 | 四张截图已目视复核，路径见下；清零仍复用既有中文确认卡 | `mobile-network` 测试，`INK_KEEP_EVIDENCE=1` |

## 没做到的

- 全量没有全绿：两个生成能力文档检查需合并会话更新共享文档后复验。本任务没有跳过、删除或放宽它们。
- 清零复用的公共确认卡仍为中文；新增移动网络页、接入点编辑页及按钮有中英文。产品新增已达 300 行，未扩展公共确认框架的多语言。
- 月界之间尚未采样的字节按下一次观察到的月份归属，没有逐帧时间戳来精确分月；数值本身来自字节计数，不按时间或速率估算。
- 真机驱动接通、设备验证和运营商计费口径未做。S31 网络和模组仍是基线的桩，新接口明确返回不可用，界面不把桩的零当作实测流量。

## 和设计不一致的

- 【实现】原厂没有这三项设置。本次按标准使用 CID 1、IP、PAP 与数据上下文重连，尚不能当成已核实的 NT26 参数；只接受可打印 ASCII，排除引号和反斜杠。上机前先核对，不能直接写配置。
- 新页按现有主题和 D-117 实现；移动网络入口放在原网络内容之后，原 Wi-Fi 分页和可见动作的断言保持不变。

## 改过的已有测试

- `tests/integration/ai_surface.c`：添加五个 AI 能力、两个本人专用草稿入口及两个状态的预期表。原有项和断言不变。
- `tests/integration/systest.c`：新目录装不下原 8192 字节缓冲，改用已有分页接口逐页检查；缓冲大小和“不能暴露 AT、模组、电源轨”等断言不变。
- 没有修改预算、确定性测试、原网络/通信/飞行模式用例的期望；没有使用 sleep 掩盖问题。

## 要用户定的

- 没有新增阻塞开发的产品决定。按 D-105，接入点和漫游是可逆设置，直接执行；密码只能人在本机输入。清零不能撤销，要求人在设备上确认，AI 不能代确认。
- 真机写接入点可能改变模组非易失配置，必须另得用户明确同意。本次没有这样的操作。

## 顺手发现

- 【实测】零流量也写日检查点会占用后台执行器，拖延大字来电场景的拒接。旧产品源码对照通过；仅取消零检查点通过；仅取消启动读取仍失败。最终只在存在统计数据时保存日检查点。证据：`work/mobile-network/baseline-phone.log`、`phone-probes.log`；来电测试未改。
- 【实测】`selftest-sim-slots` 仍有 `cell` 8000/4096 字节的栈估算告警；基线的 `docs/tasks/sms-multipart-2/REPORT.md:33` 已记录相同告警。本次未提高该预算或隐藏告警。原始指标及扫描在 `build/test-out/selftest-sim-slots/metrics.json`、`work/mobile-network/normal-warnings.json`。
- 【实测】初版启动时直接重设接入点触发主任务 4752/4096 栈估算，改为现有 `exec.bg` 执行后通过；初版失败日志保留在 `work/mobile-network/focused-3.log`，没有增加栈预算。

## 新增的机制

- `net.mobile` 私有设备集合，schema 1：接入点凭据与两个月计数各一条记录；任务书要求保存和每日落盘，沿用既有数据服务。
- `net.roaming` 布尔设置，默认关闭：任务书要求漫游开关。
- `cell.roaming` 和 `net.mobile_rev` 状态：供网络策略与页面获知变化，任务书要求注册状态和可读页面。
- 接入点启动恢复复用现有后台执行器的一个作业；字节采样复用已有分钟事件；网络操作使用已有网络 QoS。
- 没有新增线程、常驻任务、定时器、状态机、队列、缓存、后台重试或独立文件格式。

## 要改的文档

- 合并会话重新生成 `docs/system/capabilities.md`：增加四个 `net.*` 能力、三个页面入口、一个设置和两个状态。预览已生成至 `work/mobile-network/capabilities.md`，未改共享文件。
- 合并会话按需要同步 `docs/system/ui-dependencies.md` 的移动网络入口；本任务不修改共享依赖文档、需求、想法或决策记录。

## 验证命令与证据

按总规则第 5 节配置，增加 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`、`-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；内存构建仅换目录并增加 `-DINK_SANITIZE=ON`。全程构建和测试最多并发 2，轮次串行，没有启动窗口版模拟器。

```sh
export INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub"
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility python3 -B tests/mobile_network/reverse.py --build build
taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build $(git diff --name-only 9b0282dd7764f5c99e5a1a9feeb5248888269028 -- '*.c')
```

- 普通与内存专项各通过 263 条检查（主流程 208、重启 39、再次重启 16），专项无运行警告。全量日志：`work/mobile-network/full-tests-final.log`、`asan-tests.log`；构建：`build-final.log`、`build-asan.log`；内存错误扫描：`sanitizer-scan.log`，均在同一工作目录。
- 五种反向撤销均触发指定断言失败：接入点命令、密码来源、漫游策略、字节累计、每日落盘。脚本退出 0，结果在 `work/mobile-network/reverse-final.log`、`build/mobile-network-reverse/summary.json`。只编译构建目录副本，注册进 CTest 的测试不依赖 Git 历史。
- S31：`work/mobile-network/build-s31.log`、`build-s31/size.txt`、`build-s31/stack.txt`。已核对 UI 对象包含最终源文件；栈报告缺少间接调用路径，不能替代实机水位。没有执行构建输出建议的烧录命令。
- GCC：`work/mobile-network/gcccheck-final.log`。这是本机 GCC 交叉编译检查，不冒充 Linux CI 实测。

截图路径：

- `build/test-out/mobile-network/main/snap-mobile-zh.png`
- `build/test-out/mobile-network/main/snap-mobile-en.png`
- `build/test-out/mobile-network/main/snap-apn-zh.png`
- `build/test-out/mobile-network/main/snap-apn-en.png`

## 原厂与标准依据、计数来源

- 【实现】查阅 2026-11-10：原厂提交 `690a023`，`main/boards/common/board/uart_eth_modem.cc:1474` 解析 CEREG，1505～1512 行接受状态 1/5；1282～1308、1359～1417 行是以太网帧收发入口。没有找到接入点、漫游策略和流量计数实现；未照搬原厂代码。
- [3GPP TS 27.007 V18.7.0](https://www.etsi.org/deliver/etsi_ts/127000_127099/127007/18.07.00_60/ts_127007v180700p.pdf) §10.1.1（CGDCONT）、§10.1.10（CGACT）、§10.1.22（CEREG 状态 5）、§10.1.31（CGAUTH）；查阅 2026-11-10。标准定义不代表此 NT26 固件已经支持。
- 【实现】`hal_net_lte_bytes` 读取模拟 LTE 接口累计收发字节；`ports/sim/hal/hal_net_sim.c` 在实际模型传输及 `sim_net_lte_frame` 注入处累计，仅 LTE 数据接口计数。模型只传有效载荷，未模拟协议头与运营商账单口径。
- 【实现】按设备时区取月与日，复用分钟事件采样；非零统计每日首个采样同步保存，清零立即保存。重启可能丢上次检查点之后的当天数据。

## 上机先核对清单

- 只读查询 `AT+CGDCONT?`、`AT+CEREG?`；先核对 CID、PDP 类型、默认 APN、注册上报以及固件支持的认证模式。未经用户同意不发送设置命令。
- 核对 4G 以太网接口累计字节的读取点、重连后计数连续性、与运营商计费的差别。S31 当前接口返回不可用。
- 【估算】命令超时沿用既有查询/打开无线电的上限；APN 的 IP/PAP 和只重连数据上下文仍需真机确认。没有自行猜测硬件电气参数。

## 规模与最终检查

- 相对基线，产品 C/H 增加 287、删除 12 行；加词表 11 行、产品构建 2 行，产品新增合计 **300 行**，净增 288 行。四行 CTest 注册计入测试。
- 为什么不能更小：需要覆盖驱动、HAL、模型、持久化、权限和两页界面；已复用通信服务、分钟事件、后台执行器、数据集合和密码框。直接在设置页发 AT 会绕开串行化和原子能力，未采用；没有另建移动网络服务或线程。
- 【实测】`git diff --check` 通过，差异仅含本任务实现、测试、词表和报告；没有构建产物、设计稿、禁改共享文档入库。按总规则第 6 节原命令扫描基线至 HEAD：无命中，grep 退出 1。隐私扫描：`work/mobile-network/privacy-scan.log`。
