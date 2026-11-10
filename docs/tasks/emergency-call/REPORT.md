# emergency-call 执行报告

基线：`9b0282dd7764f5c99e5a1a9feeb5248888269028`。仅本地分支操作，不连接设备、不拨真实电话、不对外发布。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 锁屏拨 120、挂断仍锁屏 | 【实测】通过 | 真实触摸、密码过期、手动及远端挂断；四个快捷号码均拨出 | 专项命令 V1 |
| 1b 普通号码被拒 | 【实测】通过 | 拨号页、服务、普通拨号能力均拒绝；中英文提示 | V1 |
| 1c 飞行模式临时开射频并恢复 | 【实测】通过（模型） | 呼叫前关、通话中开、结束后 `CFUN=4`；设置全程不变；无 SIM 也提交拨号 | V1 |
| 1d 孩子锁屏拨号 | 【实测】通过 | 孩子实际点按号码、接通、挂断，仍在锁屏 | V1 |
| 1e 快照无家庭数据 | 【实测】通过 | 全屏不透明浮层；快照无底层号码、身份、联系人、记录、密码 | V1 |
| 2a 原有电话、锁屏、飞行模式 | 【实测】通过 | 原有专项原样通过；修正后 16 项回归全过 | `work/emergency-call/focused.log`、`regressions.log` |
| 2b 普通全量 | 【实测】329/330；文档校验待合并 | 最终原话：`99% tests passed, 1 tests failed out of 330`，唯一失败 `capability-table-doc`；568.34 秒，0 跳过 | V2；`work/emergency-call/full-final.log` |
| 2c ASan/UBSan 全量 | 待验证 | — | — |
| 2d S31 编译 | 【实测】通过 | 构建、尺寸及静态栈报告均退出 0；镜像 1,452,928 字节；电话页与通信服务实际编译 | V4；`work/emergency-call/s31.log`、`build-s31/size.txt`、`stack.txt` |
| 2e GCC | 【实测】通过 | `gcccheck: 9 compiled, 0 with findings`；无跳过 | V5；`work/emergency-call/gcccheck.log` |
| 3 原厂依据和上机核对清单 | 【实现】完成 | 原厂文件、行号、假设及只读／替代验证限制见下文 | 只读搜索原厂对应板型和公共模组实现 |

## 没做到的

- 内存检查尚在执行；模拟器结果不代表 NT26 实机验证。现有 `ports/esp32s31/hal_stubs.c:62–72` 的蜂窝 HAL 仍是桩，本任务未扩展实机端口。
- 未在 Linux 宿主运行全量测试；GCC 检查为本机 RISC-V GCC 编译检查。

## 和设计不一致的

- 在已核查的原厂板型和公共模组代码中未找到紧急呼叫实现。保留本项目的 `ATD<number>;` 和 `CFUN=4` 路径；它们是待实机核对的假设，不是原厂已验证功能。

## 改过的已有测试

- `tests/integration/ai_surface.c`：仅在既有能力契约表中登记四个新增能力，`phone.emergency_open` 给 AI，其余三项列为本人操作并注明理由和必填参数；不改断言、旧条目或预算。

## 要用户定的

- AI 是否可直接拨紧急号码：按任务书暂定只能打开拨号页，不能自主拨号。

## 顺手发现

- 【实测】基线提交已合入 sms-multipart-2，满足开工前提；工作区初始干净。

## 新增的机制

- 紧急拨号浮层及短暂的呼叫／射频标记：任务书第 1、2 项要求；复用现有 UI、通信任务、HAL 与通话结束事件。
- `phone.emergency`、`phone.emergency_open`、`phone.emergency_key`、`phone.emergency_close`：任务书第 1、3 项要求；号码验证集中在通信服务，普通拨号走同一入口。
- 未新增任务、线程、队列、配置、持久格式、缓存或后台重试。

## 要改的文档

- 合并时更新生成的能力表，登记 `phone.emergency` 及 AI 限制。
- `capability-table-doc` 会因这四项新增能力和源码行号变化而失败；不能在任务分支改共享生成文档。临时目录已生成并校验 `build/emergency-call-capabilities.md`（510 项定义），原有文档与相关断言均保留。

## 执行记录

- 开工：已读取总规则和任务书；本报告用于记录各里程碑与可复现证据。
- 里程碑 1：【实测】六个场景通过；原有 `phone-calls`、`airplane-mode`、`lock-camera`、`integration-member-wake` 原样通过。首轮飞行场景失败暴露 HAL 返回 1 表示尚未注册，已按 HAL 契约修复；英文入口截断已调整并复验。未扩大时限或预算。
- 里程碑 2：【实测】修正后回归 `100% tests passed out of 16`（31.63 秒）。首轮发现大字号锁屏条容纳不足、按钮顺序影响既有选人操作、锁屏策略范围过宽及能力契约缺登记，已分别修正。保留普通能力的既有锁屏宽限语义，仅收紧电话能力；原有测试未放宽。源码链接检查通过移动新增辅助函数保留原锚点，未改设计文档。
- 里程碑 3：【实测】最终普通全量 329/330，只有任务规则第 7 节禁止修改的共享生成文档失败。首轮全量为 `95% tests passed, 15 tests failed out of 330`（848.14 秒），其余 14 项均已修正并在最终全量通过。产品变更新增 165 行、删除 17 行，连同构建注册和翻译保守计 182 行，低于 250 行。
- 里程碑 4：【实测】GCC 9 文件零问题、无跳过，S31 完整编译退出 0；两项反向对照按最终源码再跑通过。普通专项最终六场景共 652 条断言，全部通过：`build/emergency-call-vnhxfq5y/`。ASan/UBSan 构建进行中。

## 为什么不能更小

- 预算内实现独立的紧急拨号浮层、号码验证、临时射频、权限和双语。看过直接复用普通电话页的方案：它包含联系人、记录及通话控制，不适合锁屏隔离；复用现有浮层和通信能力，仅增加专用页面与必要标记。
- 系统和应用未引入模拟器依赖，NT26 模型仅接受 AT 命令并呈现硬件状态；按钮同样经能力表调用，AI 只开放页面导航。手动紧急拨号适用 D-119 和 D-105 的本人直接操作规则，沿用现有触摸／按键免确认路径，不扩大普通 AI 通话的确认例外。

## 上机要核对的

- 查阅日期：2026-11-10。只读原厂 `upstream/Metalio-E-INK4-Plus`：`main/boards/common/board/uart_eth_modem.cc:1596–1611` 先 `AT+CFUN=1` 再查 SIM；无 SIM 仍保留 AT 通道。对应板型及公共模组代码没有找到紧急拨号或 `CFUN=4` 实现。
- 【估算】现有驱动的 `ATD110;`、`ATD120;`、`ATD119;`、`ATD112;` 是否支持紧急呼叫；是否另需 NT26 特定命令；无 SIM、尚未注册和受限服务能否拨出；`CFUN=4 → CFUN=1 → CFUN=4` 的实际支持与回执、结束上报方式。模型按支持处理，不能当作实机结论。
- 上机时绝不拨真实紧急号码，只读查询；如需通话替代验证，仅拨用户指定的普通号码，可能收费的操作必须先获用户同意。本次没有设备操作。

## 验证命令与证据

所有构建、测试限 `-j2`，命令前使用 `taskpolicy -c utility`，串行执行构建与测试轮次。配置沿用总规则，额外指定本地 SDL2／ImGui 源码、中枢 Python 环境、导出编译数据库；依赖只读，未升级。

- V1：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^emergency-call$' --output-on-failure -j2`。
- V2：`INK_KEEP_EVIDENCE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。
- V3：按同一配置只换为 `build-asan` 并增加 `-DINK_SANITIZE=ON`；构建后用 V2 命令将测试目录改为 `build-asan`，不减少测试项目。
- V4：`taskpolicy -c utility bash ports/esp32s31/build.sh`（预先设置用户给定的 `INK_DEPS`）。
- V5：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B "$INK_DEPS/../ink-codex/tools/gcccheck.py" build ports/sim/hal/hal_net_sim.c system/apps/phone/phone.c system/policy/cap.c system/services/comm/comm_service.c system/ui/lock_mark.c system/ui/shell/member_gate.c system/ui/shell/member_pages.c tests/emergency_call/emergency_call.c tests/integration/ai_surface.c`。
- 专项日志：`work/emergency-call/emergency-3.log`；截图和指标：`build/emergency-call-371qm5q5/{lock,gate,kid,airplane,nosim,en}/`。已查看中英文拨号页，布局无重叠。
- 反向对照：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/emergency_call/run.py --build build --negative radio` 和 `--negative page`；对应 `negative-radio-final.log`、`negative-page-final.log`。只编译构建目录中的临时副本；撤掉射频开启、撤掉紧急通话 UI 事件处理，都使同一测试失败。脚本退出 0 表示反向对照成立；未改产品文件。

## 差异与隐私检查

- 【实测】基线到当前提交仅 15 个相关文件；共享生成文档、需求、决策、研究、审计资料未改，原测试断言与预算未改。
- 【实测】按总规则给定的隐私扫描命令执行，`grep` 退出 1、0 行匹配（`work/emergency-call/privacy.log`）；未提交构建产物或设计稿。
