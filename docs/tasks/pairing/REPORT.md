# pairing 执行报告

2026-11-06。本轮基线 `68b6476f`，修复提交 `3b8aa20a`，接续原实现和[独立复查](REVIEW.md)。本轮修复 R1～R3（高）和 R4（中），只使用合成资料及模拟中枢。以下替换上一轮验收结论。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1 未受信任响应清空资料 | 已修复 | 【实现/实测】清空授权仅接受带已保存身份、令牌及匹配固定公钥的 hello 响应，信任随响应入队保存；换连接清除旧对端。地址、固定公钥、卡内 preauth 三种未确认 begin 均不能吊销或清空；重启保留 Flash/卡资料，正常已绑定吊销及 wipe 仍通过 | T1、T2、T6 |
| R2 提交进度丢失卡住 | 已修复 | 【实现/实测】提交中设 10 秒进度期限，失败后短退避，按已保存 nonce/txid 重连续接；accept 分配、排队、超时及远端错误有回调。不重启覆盖单次 accept/complete 丢失、16 个槽位占满、暂时错误和过期；过期不得推进到 token/hello | T1、T2、T6 |
| R3 一次性授权跨公钥泄漏 | 已修复 | 【实现/实测】变更或移除公钥先持久清除旧身份，再写新配置；失败后保持握手阻断。同公钥迁址保留授权；UNPAIRED/PAIRING 换公钥清掉 preauth、nonce、txid 和家庭信息。三处写入各测排队拒绝/完成失败及重启，共 6 类 | T1、T2、T6 |
| R4 AI 控制能力被整体排除 | 已修复 | 【实现/实测】除最终 accept 外，其余 7 个调用均向 AI 开放：公共地址配置、拒绝、重新发起、热点开关直接走同一能力；卡导入需人通过现有确认机制。热点二维码仅返回触摸/按键调用者。验证 AI 能控制但拿不到凭据、不能自行确认，儿童仍被隔离 | T1、T2 |
| P1 状态、配网和身份隔离 | 通过（模拟器） | 【实测】五状态、固定公钥、迁址、认证错误分类、跨公钥事务隔离均通过 | T1 |
| P2 热点、网页模型、配对卡 | 通过（模拟器） | 【实测】模型表单、随机密码、成员隔离、启动导入、可选 Wi-Fi 恢复和删除卡文件通过 | T1 |
| P3 丢失及重启恢复 | 通过（模拟器） | 【实测】恢复矩阵 35 次启动，拒写矩阵 20 次启动；保留原断电恢复场景，新增活连接恢复 | T1 |
| P4 吊销和资料处理 | 通过（模拟器） | 【实测】已绑定吊销、布尔 wipe、重启清理及未授权清空拒绝通过 | T1 |
| G1 全量、内存、预算 | 通过 | 【实测】普通 95/95、47.82 秒；ASan/UBSan 95/95、83.45 秒；均无跳过，预算未改 | T3、T4 |
| G2 原子能力与成员隔离 | 通过（模拟器） | 【实现/实测】8 个调用、1 个事件和脱敏状态；7 个调用可由 AI 发起，最终 accept 仅限人；公开快照及 AI 热点回执无秘密 | T1、T3 |
| G3 依赖状态 | 保持原状态 | 【实现】依赖表已注明服务/模型与页面的边界，本轮不改状态和原判断 | T5 |
| G4 页面验收 | 不适用 | 服务修复；未开发开机页面、未启动窗口 | — |
| G5 范围、隐私及文件检查 | 通过 | 【实测/实现】12 个文件均为本任务服务、接口注释、模拟故障、测试和报告；指定路径/MAC 模式 0 命中，无超 5 MB 文件，报告链接有效 | T5 |

实现：[pairing.c](../../../system/services/hub/pairing.c)、[hub_link.c](../../../system/services/hub/hub_link.c)。仍复用后台执行器、共享服务锁和既有中枢任务；未增加常驻任务或预算。网页仍是模拟器内的模型，没有宿主 HTTP 监听。所有构建最多 `-j4`，全量最多 `-j3`，构建与全量测试串行执行，未启动 GUI。

- T1：`cmake --build build -j4`；`"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(pairing-|integration-ai-surface$)' --output-on-failure -j2`，3/3，2.28 秒。日志 `work/pairing-review-build.log`、`work/pairing-review-targeted.log`。
- T2：`"$INK_DEPS/build-deps/pyenv/bin/python" tests/pairing/reverse.py --build build`，18/18，退出 0；生产源码不撤回，只编译忽略目录中的变体。保留原 10 项，新增信任、跨公钥清理、写入顺序、提交期限、accept 错误、AI 控制、凭据回执及导入确认共 8 项。每项均以对应保护断言失败判定；日志 `work/pairing-review-reverse.log`，证据 `build/pairing-reverse-89s610is/`。
- T3：按总规则 §5 重新配置，`cmake --build build -j4`，`"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3`，构建及测试退出 0；95/95，47.82 秒，无跳过。日志 `work/pairing-review-configure.log`、`work/pairing-review-build.log`、`work/pairing-review-full.log`。
- T4：同样配置换 `build-asan` 并加 `-DINK_SANITIZE=ON`，`cmake --build build-asan -j4` 退出 0；`"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3` 退出 0，95/95、83.45 秒，无跳过。日志 `work/pairing-review-asan-configure.log`、`work/pairing-review-asan-build.log`、`work/pairing-review-asan-full.log`。两个目录均 `INK_WERROR=ON`、`INK_GUI=OFF`，内存目录另有 `INK_SANITIZE=ON`；依赖使用只读 `$INK_DEPS`，既有中枢 Python 环境不变。
- T5：`git diff --stat 68b6476f..HEAD`（12 个文件）、`git diff --check 68b6476f..HEAD`（退出 0）；按总规则的 `grep -nE` 路径/MAC 模式扫描差异，0 命中（grep 退出 1）。文件大小和报告链接检查通过。6 个变更 C 文件按各自 CMake 参数加 `-Wsign-compare -Werror=unused-result -fsyntax-only` 均退出 0；新增 POSIX write/read/fgets 调用为 0，HAL 读写长度及关闭返回值已检查。日志 `work/pairing-review-checks.log`；这仅是 clang 核查。
- T6：原样执行 REVIEW.md 末尾复现命令，进程退出 0，7/7 场景完成、`test.fail` 均为 0；卡/Flash 私有文件重启后均仍为 23 字节，无旧 preauth 到新公钥。日志 `work/pairing-review-repro.log`，证据 `build/pairing-review-q9wdi0it/`。

## 没做到的

- 本机没有 Linux/gcc 工具链；未实测 Linux、真机 HAL/TLS/NVS、真实手机网页或真实家庭服务器，不把 clang 或模拟器通过写成这些环境通过。
- R1～R4 无未修项；本报告的“通过”限于上述模拟器和本机工具链。

## 和设计不一致的

- 继续使用模拟器已有内部 Flash 原子存储保存 `pair.config` 与 `pair.identity`，真机 NVS 映射仍待设备端口实现；长期令牌不在存储卡。没有修改设计迁就实现。

## 改过的已有测试

- `tests/pairing/device.c` 的 AI configure 期望由“拒绝”改为“能设置公共地址但没有加入权限”，对应 R4；最终 accept 仍要求人操作。
- `tests/integration/ai_surface.c` 将 configure、reject、restart、import、hotspot_start、hotspot_stop 六项从禁止表移到 AI 能力表；accept 仍留在禁止表。
- 其他原断言、预算和超时未放宽；只追加网络边界、活连接恢复、AI 权限、写入故障及反向对照。过期 accept 新增中间路径断言，防止仅凭最终状态漏报。

## 要用户定的

- 本轮无需新决定。沿用上一轮暂定配对文件 JSON 编码，服务器对接时仍需冻结；没有改需求、想法或决策记录。

## 顺手发现

- 写入顺序与在途连接也影响授权隔离，已作为 R3 同类边界修复；配置失败保持握手阻断，重启只会看到原公钥或已清空授权的新事务。
- 【实测】普通和内存全量的 PQ-BN AI 场景内部总 RAM 最低余量 32880 字节，门槛仍为 32768 字节，预算违规 0；余量仍很紧，没有提高预算上限。
