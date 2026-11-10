# mobile-network-2 执行报告

基线：`951bdfc15f0997a77cd9ad960b056eee10fb3ef8`；恢复来源：只读分支 `origin/archive/mobile-network`（`9132e6ef`）。状态：最终验证中。只在本地编译和跑宿主机测试，未操作设备、推送或开 PR。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 功能 | 【实测】通过 | 原主流程、重启、清零全保留；四阶段 216/47/24/53，共 340 条检查；密码隔离、漫游、字节、跨月、落盘和儿童权限通过 | `ctest --test-dir build -R '^mobile-network$' --output-on-failure` |
| 1 无模组 | 【实测】通过 | 中英文说明、禁用修改、拒绝编辑捷径、不伪造流量、仍能返回；同时保留了已有接入点数据 | 同上；六张截图见下 |
| 1 反向 | 【实测】7 组通过 | 接入点命令、密码来源、漫游策略、字节累计、每日检查点、无模组说明、原 64 槽容量均命中预期失败 | `python -B tests/mobile_network/reverse.py --build build` |
| 1 普通全量 | 【实测】345/346，未全绿 | 原话 `99% tests passed, 1 tests failed out of 346`，376.97 秒；仅基线已有的中枢端到端失败，全部用例已运行 | 下列普通全量命令 |
| 1 内存全量 | 待最终记录 | ASan + UBSan 全量待运行 | 下列内存全量命令 |
| 2 能力清单 | 【实测】通过 | 已重新生成，`PASS: 525 definitions, docs/system/capabilities.md`；运行时清单和服务表检查也通过 | `python -B tools/capabilities.py --check` |
| 2 S31 | 待记录 | 只编译，不接设备 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2 GCC | 【实测】通过 | `gcccheck: 11 compiled, 0 with findings`；本机交叉 GCC 检查，不冒充 Linux CI | 下列 gcccheck 命令 |
| 2 隐私 | 【实测】阶段扫描无命中 | 总规则 §6 的原扫描命令，grep 退出 1；最终提交后再核对 | `git diff <基线>..HEAD` 后接总规则原扫描 |
| 3 差别 | 【实现】已逐项记录 | 下节；没有改接入点、漫游、统计、凭据、确认和儿童权限的业务语义 | `git diff origin/archive/mobile-network -- <相关文件>` |

## 没做到的

- 普通全量未全绿；内存全量及 S31 结果待回填。
- 【实测】中枢端到端 `hub-simulator-e2e` 在指定基线也失败：`tool_calls=0`，成员夹具约 5.06 秒就绪，晚于原脚本的 3 秒点按。没有调整等待、放宽断言或改中枢产品代码。基线对照见“顺手发现”。
- 沿用原报告边界：按分钟观察字节，未采样的月界字节归下次观察月份；每日检查点后的当天流量掉电可能丢失。公共清零确认卡仍为中文。
- S31 蜂窝驱动和真实字节来源未接通，不能当成真机流量验证。没有做任何上机操作。
- 未运行 Linux CI；本次证据是 macOS 的 `-Werror` 构建、交叉 GCC 和 S31 编译。

## 和设计不一致的

- 沿用原任务已披露的差异：CID 1、IP、PAP、数据上下文重连及可打印 ASCII 限制尚未核实 NT26 固件支持；不是已验证的模组配置。
- 新设置页按既有主题和 D-117 实现。无模组提示依据接口不可用，不把缺失接口的零或无效时钟冒充实际流量。
- 沿用原页的儿童修改按钮禁用方式；界面规范 §0 第 8 条建议隐藏不可用动作，此处没有另改原任务的界面语义。

## 改过的已有测试

- `tests/integration/ai_surface.c`：加入五个 AI 能力、两个本人草稿入口、两个状态；保留主线日志和 Wi-Fi 例外。另把基线失配的 `sms.send` 必填参数从 `to` 改为 `to,text`，与已有生产 schema 一致，检查更严格。
- `tests/integration/systest.c`：沿用归档的目录分页检查（8192 字节缓冲不变）；设置整表输出的测试缓冲由 2048 改为 4096 字节，容纳新增设置；检查内容和资源预算不变。
- `tests/hub/export_caps.c`：改用已有分页接口逐页导出完整清单，原 65536/16384/768 字节缓冲及小缓冲溢出对照保留，输出格式不变。
- `ports/esp32s31/tests/stubs.c`：只添加真实 S31 桩返回不可用的断言；普通和 ASan/UBSan 宿主构建均退出 0（`stubs.log`、`stubs-asan.log`）。
- mobile-network 的三个原阶段和五组原反向对照保留；只增加无模组、系统归属、持久日志检查。没有删测试、跳过用例、加 sleep、放宽资源预算或失败断言。

## 要用户定的

- 本次没有新增产品决定。按 D-105 接入点和漫游直接执行；密码只准本人本机输入；清零不可撤销，仍由本人确认，AI 不能代确认。
- 真机设置接入点可能写模组非易失配置，另需用户明确授权；当前只读核对清单见末尾。

## 顺手发现

- 【实测】首轮全量：`97% tests passed, 9 tests failed out of 346`，316.49 秒，包含两个依赖未运行；`work/mobile-network-2/full-tests.log`。修正后相关 10 项回归为 `100% tests passed out of 10`（58.30 秒），见 `regressions.log`。
- 【实测】新订阅耗尽既有表，初版启动报 `member pages failed to start`；64→68 槽后恢复。恢复原限制的反向检查仍失败；没有调高内存/栈预算。
- 【实测】中枢基线对照：九个差异产品编译单元均以指定基线源码重新编译，包括原 64 槽表对应的 state/settings/watch，替换链接后独立运行；其余未变单元和依赖复用当前构建。相同 `tool_calls=0`，退出 1。证据 `work/mobile-network-2/baseline-e2e.log`，复现脚本 `baseline_e2e.py` 在同目录，未注册依赖历史的 CTest。
- 【实现/实测】基线已有五个空行源码锚点阻塞 `app-framework-snippets`，仅修正锚点，正文设计不变；12 段摘录、47 个头文件引用检查通过。服务能力总表增加 D-125 明确要求的手机网络一行，未改旧设计迁就实现。
- 【实测】`selftest-sim-slots` 仍有 `cell` 8000/4096 字节的宿主栈估算警告；归档报告已记录相同警告，本次未放宽或隐藏。

## 新增的机制

- 私有设备集合 `net.mobile`、schema 1：原任务要求保存接入点凭据和两个月流量，复用既有数据服务。
- 设置 `net.roaming`（默认关），状态 `cell.roaming`、`net.mobile_rev`：原任务要求漫游策略和可读页面。
- 沿用原实现的后台作业、分钟采样和网络 QoS。没有新增线程、常驻任务、定时器、状态机、队列、重试或独立文件格式。
- 本次只扩既有订阅表容量；`net.usage.interface` 和页面不可用标记用于任务书明确要求的无模组说明，不是新服务或配置项。

## 要改的文档

- 本次按验收授权重生成 `docs/system/capabilities.md`；补齐服务能力表和失效源码锚点。
- 合并会话可同步 `docs/system/ui-dependencies.md` 的移动网络入口；本任务未改该共享文件、需求、想法、决策或 backlog。

## 与原 mobile-network 实现的差别

- 保留主线已知网络页、键盘翻译、日志权限检查、audit14-fix 及真实 Wi-Fi 端口；S31 只补两个蜂窝桩，未恢复旧 Wi-Fi 桩。
- 七个设置能力显式填 `.app = NULL`，按 audit14-fix 作为系统能力，不作为可被禁用的应用；儿童系统设置限制仍生效。初版误填 `settings` 已由 ownership 回归发现并纠正。
- `net.usage` 增加 `interface` 布尔值，区分缺失接口和时钟无效；无模组中英文说明、按钮禁用、快捷编辑拒绝。
- 状态订阅表从 64 扩为 68；能力清单重新生成；适配增长后的测试清单和设置表。原四个能力仍经 `ink_cap_call` 统一记录调用，实测持久日志有四项调用且无密码。
- 【实现】按 R-061：页面只调原子能力，通信持久化、选网策略、HAL 和模组模型各在原层；AI 和按钮同路，凭据草稿仍只给本人，页面状态可读。

## 命令与证据

配置按总规则 §5，增加 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`、`-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；内存构建只换目录并加 `-DINK_SANITIZE=ON`。所有构建/测试串行分轮，最多 `-j2`，前缀 `taskpolicy -c utility`；未启动有窗口的模拟器。

```sh
export INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub"
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility python3 -B tests/mobile_network/reverse.py --build build
taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility python3 -B "$INK_DEPS/../ink-codex/tools/gcccheck.py" build $(git diff --name-only 951bdfc1..HEAD -- '*.c' ':(exclude)ports/esp32s31/**')
```

- 工作证据目录：`work/mobile-network-2/`；普通最终全量 `full-tests-final.log`，内存 `asan-tests.log`，S31 `build-s31.log`，GCC `gcccheck-final.log`，反向 `reverse.log`，隐私 `privacy-scan.log`。七组反向摘要在 `build/mobile-network-reverse/summary.json`。
- 中英文截图已目视检查，未提交图片：`build/test-out/mobile-network/main/snap-mobile-{zh,en}.png`、`snap-apn-{zh,en}.png`；无模组在 `build/test-out/mobile-network/no-modem/snap-mobile-no-modem-{zh,en}.png`。

## 原厂依据与上机先核对

- 【实现】2026-11-11 只读查阅原厂 `690a023`：`main/boards/common/board/uart_eth_modem.cc:1474` 解析 CEREG，1505 行接受状态 1/5；1282 和 1359 行附近为以太网帧收发。未找到接入点、漫游策略或流量统计；未照搬原厂代码。
- 保留归档的 3GPP TS 27.007 条款出处：§10.1.1 CGDCONT、§10.1.10 CGACT、§10.1.22 CEREG、§10.1.31 CGAUTH。代码/标准描述不等于模组已支持或实机已验证。
- 上机只读核对 `AT+CGDCONT?`、`AT+CEREG?`，先确认 CID/PDP/认证支持及实际以太网接口累计收发字节的位置、重连连续性与运营商计费差别；未经用户授权不发设置命令。

## 规模与最终检查

- 产品 C/H 增加 292、删除 13 行；词表 11 行，产品构建 2 行，产品新增 **305 行**、净增 292 行。四行 CTest 注册计入测试。
- 为什么不能更小：恢复原任务约 300 行实现，另用 5 行覆盖新验收和主线容量适配。只接页面会缺真实计数、持久化和权限；页面直接发 AT 会绕过 HAL 和串行能力，均未采用；没有另建移动网络服务。
- 最终 diff、文件范围、隐私和提交状态待核对。
