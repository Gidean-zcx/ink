# 长短信返工报告

基线：`1245e4c2c4ade794d2f29ce4050daf799804bcce`。2026-11-10 完成；仅本地提交，不推送、不操作设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】专项通过 | `integration-confirm`、`memo-record`、`sms`、`sms-multipart`、PDU 单测通过；150 字页面发送 3 段、100 字 AI 发送 2 段、超长 AI 拒绝 | `ctest --test-dir build -R '^(sms|sms-multipart|unit-nt26-pdu|integration-confirm|memo-record)$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 普通全量及 ASan/UBSan 均为 `100% tests passed out of 323`，0 跳过；S31 编译成功；gcccheck 13 个文件、0 个问题，容量夹具补编退出 0 | 见“验证命令与证据” |
| 3 | 【实测指标】无新增告警任务；保留既有自检告警，见下文 | 两指定场景无栈告警；`memo-record` 的 `exec.bg` 最高从 8752 降为 7280 字节，预算仍为 8192；【估算】最高五个任务见下表 | `ctest --test-dir build -R '^(integration-member-wake|memo-record)$' --output-on-failure -j2`；扫描全部指标 |
| 4 | 【实现】已核查并列清单；未上机 | 原厂没有短信实现，本次无设备操作、无写模组配置命令 | 只读核查原厂 `uart_eth_modem.cc`，见“上机要核对的” |

## 栈估算对照

【实测指标】同一普通构建，第一版为合并提交 `8fe03ebc`，返工为 `a729c230` 之后的短信代码；两个场景各取用量最高的五个任务，包含测试任务。`memo-record` 对 main/cut/after/retry 各阶段取最大值。以下是主机模型的【估算】，不是实机水位。

| 场景 | 任务 | 第一版估算 / 预算（字节） | 返工估算 / 预算（字节） |
| --- | --- | --- | --- |
| integration-member-wake | `ui` | 8728 / 10240 | 8728 / 10240 |
| integration-member-wake | `waketest` | 4688 / 16384 | 4688 / 16384 |
| integration-member-wake | `comm` | 4488 / 8192 | 4488 / 8192 |
| integration-member-wake | `exec.bg` | 4240 / 8192 | 4240 / 8192 |
| integration-member-wake | `storage` | 3104 / 4096 | 3104 / 4096 |
| memo-record | `ui` | 9664 / 10240 | 9664 / 10240 |
| memo-record | `exec.bg` | 8752 / 8192 | 7280 / 8192 |
| memo-record | `memo-record-test` | 5200 / 16384 | 5200 / 16384 |
| memo-record | `comm` | 4488 / 8192 | 4488 / 8192 |
| memo-record | `exec.media` | 4048 / 8192 | 4048 / 8192 |

每个参数表恢复后少 736 字节，该峰值下降 1472 字节。原始指标：`build/test-out/member-wake/metrics.json`、`build/test-out/memo-record/*/metrics.json`；第一版提取值在 `work/sms-multipart-2/first-*.json`。

【实测】扫描普通构建的 949 份 `metrics.json`，只有既有的 `selftest-sim-slots` 报 `cell` 8000/4096 字节。为区分新旧，临时取基线的 NT26 驱动、头文件和模组模型重新编译同一自检，复现原有 6528/4096 告警；不是故意注入的告警。第一版改为 PDU 后该估算升高，本次返工未改此自检及其驱动/模拟器依赖，未增加告警任务。没有提高该预算或隐藏告警。对照证据：`work/sms-multipart-2/baseline-cell/run/metrics.json`；扫描：`work/sms-multipart-2/stack-scan-final.json`。

## 没做到的

- 既有 `cell` 自检栈告警未修复；它包含模组模型在测试任务上的开销。按本任务边界保留第一版驱动与拼接机制，另记后续处理；实际系统 `comm` 场景估算为 4488/8192 字节。
- 未连接设备、未在 Linux 主机运行全量测试；GCC 检查是在本机用已安装的 RISC-V GCC 编译到汇编。

## 和设计不一致的

- 【实现】`args.h`、`confirm.h`、`cap.c`、`confirm_card.c` 与基线完全相同，恢复 416/160/1024/1200 字节和 161 个分页边界。
- 【实现】`confirm.c` 有必要的小改动：既有确认准备钩子接收调用来源，并可通过回执 `say` 提供 160 字节以内的说明。否则钩子无法阻止 AI 使用草稿，也无法在接近装满的参数表内再塞一份摘要。钩子签名的其它适配只有 DTMF、清空设备及其合成测试；普通确认卡“超长就拒绝”不变。
- 【实现】PDU 接收、编码、分段拼接、去重、超时和 `sms.parts` 均沿用第一版。唯一必要的接收交接调整：组装好的正文先完整存入 `sms.messages`，事件带记录 ID 和最多 354 字节的完整字符预览，避免恢复参数表后丢正文；原有 118 字事件保持完整。AI 可经原有数据能力按 ID 读取全文。
- 【实现】保留原来参数容量内的触摸发送行为；超过该容量的页面草稿发送先确认，再按确认时的摘要执行。草稿确认绑定正文哈希，确认后正文改变则不发送。

## 改过的已有测试

- `tests/integration/confirm.c` 仅适配钩子的来源参数；所有断言、期望不变。`memo-record` 未修改。
- `tests/integration/lifecycle.c`：第一版“336 字可放入参数表、由服务拒绝”改为经 JSON 调用后由 416 字节边界拒绝；仍断言无短信发出。118 字边界的成功断言不变。
- `tests/integration/ai_surface.c`：两个发送别名的必填项从 `to,text` 改为 `to`，因为正文与本机草稿二选一；服务验证缺正文、两者并给、AI 草稿均不可发送。
- `tests/sms_multipart/sms_multipart.c`：长正文发送改走真实文本草稿；AI 确认改为 100 字/2 段，150 字直接正文改为拒绝；页面 150 字增加确认前零发送、确认后 3 段与完整持久化断言。接收、重启、PDU、部分失败、5 段上限的原期望不变。
- `tests/confirm_durable/regression.c`：全量检查暴露容量场景在临近结束时自动锁屏、进入浅睡，2000 秒模拟时限到时仍未完成。仅该场景持有已有的 `INK_NEED_NO_LOCK`，结束释放；不改断言、期望或时限。原始全量为 `99% tests passed, 1 tests failed out of 323`；修正后容量场景 3760 条检查及重启 9 条检查通过。
- 同一容量夹具补编 GCC 时发现 9 处已有的 `uint32_t`/`%u` 类型不匹配；只给这 9 处打印实参加 `unsigned` 转换，触及的长诊断消息限为 150 字节。断言和期望不变，没有改其它测试的格式调用。

## 要用户定的

- AI 是否允许发送超过参数表容量的长短信；本次按任务书仅允许直接 `text`，超限拒绝。

## 顺手发现

- 【实测】第一版对照：`integration-confirm` 10 条失败；`memo-record` 的 `exec.bg` 栈估算 main/retry 8496、after 8752 字节，超过 8192。日志：`work/sms-multipart-2/first-tests.log`。
- 构建依赖的字体 Python 不包含中枢测试依赖；全量测试使用只读依赖目录已有的中枢虚拟环境，不安装或升级依赖。
- 【实测】手动反向检查 `taskpolicy -c utility python3 tests/sms_multipart/rework_negative.py --build build`：只在构建目录的临时副本撤掉页面草稿交接，退出 1、3 条检查失败；脚本自身退出 0 表示反向对照有效，产品源文件不变。保留证据在 `build/test-out/sms-rework-negative/`。
- 【实测】`gcccheck` 最终结果：`13 compiled, 0 with findings`。两文件不在宿主构建数据库中：S31 桩随固件 GCC 编译通过；运行时容量夹具套用 `sim-media` 的编译参数补编，退出 0。证据：`work/sms-multipart-2/gcccheck-final.log`、`gcc-fixture-final.log`。
- 容量夹具补编曾发现 9 处打印类型不匹配（该 RISC-V 工具链的 `uint32_t` 为 `unsigned long`）；适配后普通全量通过，ASan/UBSan 的 `confirm-durable` 又复验为 `100% tests passed out of 1`，26.91 秒。初检及复验日志均保留。
- 【实现】顺手核对发现 `tests/esp32s31/stack_report.py:49` 的电源任务预算仍写 3072，实际服务已改为 4096；本任务不改该报告工具。上表按普通构建实际指标，未使用这项旧元数据。

## 新增的机制

- 页面发送使用现有 `text.drafts`，通信服务在现有 PSRAM 缓冲读全文：任务书要求；没有新文件格式。
- 沿用第一版的 `sms.parts` 集合、拼接分组和 10 分钟缺段超时：原任务书要求分段持久化与超时显示，本次不重做。
- 既有确认准备钩子增加来源参数及短摘要出口：本任务要求 AI 不得使用本机草稿、确认卡保留原容量。
- 本次返工没有新增任务、线程、状态机、队列、缓存、重试或配置项。

## 要改的文档

- 合并时同步能力文档的正文/草稿二选一、AI 直接正文受参数表限制，以及长接收事件的记录 ID/预览说明。按总规则不改共享文档。

## 上机要核对的

- 原厂固件不收发短信；NT26 的 PDU 模式须上机确认。本次只编译与模拟测试，不发送写模组配置的命令。
- 【实现】核查原厂提交 `690a023` 的 `main/boards/common/board/uart_eth_modem.cc`（查阅：2026-11-10）：409～506 行为标识、信号与注册查询；1563～1578 行包含写模组配置的命令。没有找到短信 PDU 收发实现。
- 沿用第一版：核对 `AT+CMGF=0`、`AT+CMGS=<长度>` 与 PDU、`AT+CNMI` 上报方式、`AT+CMGR` 的 PDU 格式、模组是否自行拼段；上机先只读查询，不发真短信。
- 不发送 `AT+ECPCFG`、`AT+ECNETCFG`、`AT+XJCFG` 这类写模组配置的命令。本次也不运行烧录、监视或设备串口命令。

## 验证命令与证据

使用总规则第 5 节的配置，增加 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 和 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；内存构建只换目录并增加 `-DINK_SANITIZE=ON`。所有构建和测试均用 `taskpolicy -c utility`，构建/测试最大并发均为 2，轮次串行。

```sh
export INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub"
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan -R '^confirm-durable$' --output-on-failure -j2
taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility python3 tests/sms_multipart/rework_negative.py --build build
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build $(git diff --name-only 1245e4c2c4ade794d2f29ce4050daf799804bcce -- '*.c')
```

- 内存检查日志：`work/sms-multipart-2/asan-tests.log`，覆盖全部 323 项（包括短信、确认卡、备忘录、重启、确定性、中枢与界面场景）。无 ASan/UBSan 报错。
- 普通全量首次日志：`work/sms-multipart-2/full-tests.log`（容量测试未完成）；最终日志：`work/sms-multipart-2/full-tests-final.log`，原话 `100% tests passed out of 323`，724.36 秒。ASan/UBSan 全量 1419.18 秒，格式适配后的补测见 `asan-confirm-final.log`。
- S31 日志：`work/sms-multipart-2/build-s31.log`；GCC 最终日志：`gcccheck-final.log`、`gcc-fixture-final.log`（同一工作目录）；反向：`work/sms-multipart-2/rework-negative-final.log`。
- 【实测】S31 编译成功，产出 `build-s31/ink-s31.elf`、镜像及 `size.txt`、`stack.txt`。静态 Flash 1323886、DIRAM 98346、外部 RAM BSS 585376 字节；不含运行时堆和任务栈。栈报告只包含已知直接调用，缺少回调等路径，不作为实机安全水位。
- 截图已检查：`build/test-out/sms-multipart/compose/snap-compose-parts.png`、`build/test-out/sms-multipart/rx/snap-incomplete.png`。
- 产品代码相对第一版合并提交 `8fe03ebc`：增加 93、删除 51、净增 **42 行**，低于净增 120 行预算。计数包含 system/drivers/platform/ports/simulator，不含测试、文档。
- 【实测】`git diff --check` 和基线至 HEAD 的差异范围检查通过；按总规则原命令扫描隐私，无命中（grep 退出 1）。没有构建产物、设计稿、禁改共享文档入库；没有改 remote、推送或创建 PR。
