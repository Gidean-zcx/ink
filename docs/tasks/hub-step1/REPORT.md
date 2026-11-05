# 家庭中枢第 1 步执行报告

基线：`fdff779f2f2746ab10194e32a00b7dc343365e2c`。分支：`claude/codex-hub-step1`。日期：2026-11-05。只在本地提交，未推送或创建 PR。

本轮按[第 1 轮复查](REVIEW.md)修复：严重 0、高 2、中级 0。R1、R2 均已修复，首个修复里程碑为 `fba2adf`，随后补齐 R1 的轮次收尾。强制重启后先核对 Git 状态、`fdf14b8` 和原报告，保留工作区改动继续；构建最多 `-j4`、CTest 最多 `-j3`，构建与全量测试串行。既有 GUI 冒烟使用 `SDL_VIDEODRIVER=dummy`，没有打开窗口。

最终复核补充：R1 的轮次结束回执／错误提示被撤权丢弃时也须正常收尾。新增 2 项先复现失败，修后全部 pytest 为 114/114；独立进程撤回该补丁仍失败 2 项。最终全量、内存检查与五次 E2E 均通过，下表是最终版本结果。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1 | 已修复，本轮通过 | 【实现】Registry 执行前后复核 Scope；直接调用异步分派且与 AI 轮次合计最多 4 个；RPC、语音、听写和发送队列绑定权限／连接／代次，取消使排队消息失效并清理 Future。【实测】新增 21 项覆盖真实本机 WebSocket 撤权、切成员、断线／重连、发送背压、会话结束、回执竞争及轮次收尾；旧代码反向对照 3 项失败、1 个 Future 异常，单独撤去收尾补丁另失败 2 项 | T1、T2、下方反向对照 |
| R2 | 已修复，本轮通过 | 【实现】取消用事务内条件更新；完成和外部执行前检查已取消状态，取消先发生则保留 cancelled、丢弃结果；撤权后的执行与失败清理均不终止调度。【实测】新增 9 项；运行中取消后第二个任务完成，取消任务不重放、不发布结果；反向对照 3 项失败 | T1、T2、下方反向对照 |
| H1 | 沿用复查通过 | 【实现】固定 Python 3.11.15、uv 0.9.5 和依赖锁未改；本轮未重装环境。[复查 T3](REVIEW.md)已有全新环境、CLI、healthz/setup 证据；中文 [README](../../../hub/README.md) 的构建并发已改为 4 | T1、T2 复用已有环境；新环境方法见 README |
| H2 | 本轮通过（合成协议） | 【实测】既有握手、目录、双向工具回执及语音／听写控制流通过；R1 的直接调用不再阻塞连接控制，撤权后无旧结果 | T1、T2 的 hub-contract |
| H3 | 本轮通过 | 【实测】pytest **114/114，0 跳过**，比复查新增 30 项；独立词法预算、乱序、超限、错误、重连及超时均通过 | T1、T2；定向命令见下 |
| H4 | 本轮通过 | 【实测】生产 C 解析器接受 **161 条**契约测试生成消息，最高 **110** 词法单元；128 边界接受，129 反例拒绝；每轮 E2E 另验 19 条、最高 33 | T1、T2 的 hub-json-crosscheck；T3 |
| H5 | 本轮通过 | 【实测】当前生产初始化代码导出 **131 条工具、104 个 actions、overflow=true**；空 actions 回查、完整定义和 caps 分页用例通过 | T1、T2 的 hub-export-caps、hub-contract |
| H6 | 本轮通过（合成身份） | 【实测】原成员／家庭／动作范围反例及新增异步交付反例均通过；撤权和换代后的旧 RPC、直接回执、听写不出队。仍未证明真实成人解锁／账号接入 | T1、T2；tests/isolation |
| H7 | 本轮通过（合成负载） | 【实测】持久队列重开、幂等、unknown 不重放、执行中撤权、运行中取消后继续调度均通过；worker 文件探针通过。容器本身未运行 | T1、T2；tests/recovery、tests/surface |
| H8 | 本机通过；Docker 未验 | 【实测】预检、安装预演、mock Docker、设置码／向导、worker 隔离配置测试通过；本机无 Docker，不把 mock 当作容器证据 | T1、T2 的 hub-contract；`sh -n hub/deploy/install.sh` |
| H9 | 本轮通过 | 【实测】最终普通与 ASan/UBSan 各 **91/91，0 跳过**，分别 **32.61 s、69.65 s**；含原 86 项及两项确定性。本轮未改模拟器或设备源码；桥关闭时三个场景与基线逐字节一致是[前轮证据](REVIEW.md)，本轮未重做该额外对照 | T1、T2 |
| H10 | 本轮通过 | 【实测】最终代码串行 **5/5**，合计 **88.37 s**；每次 1× 实时、一轮语音、一次翻页、播放 ≥300 ms，无坏消息或上行丢包。就绪事件启动，未新增 sleep 或放宽断言 | T3 |
| H11 | 本轮测量完成 | 【实测，开发机】五次主进程峰值 RSS **44.67～57.09 MiB**，循环 p95 **1.194～1.225 ms**，最大 **13.598 ms**；详见下表。设计 [§4.3](../../hub/hub-architecture.md#43-规格) 的历史测量保留原数 | T3；measurement.json |
| H12 | 本轮代码核查通过 | 【实现】[两端约定](../../hub/contracts-step1.md) 保留字段、来源、待定项／默认值和设备改动；本次修复实现其异步交付隔离规则，没有改设计迁就代码 | 文件与链接检查 |
| H13 | 本机配置检查通过；Linux 未验 | 【实测】YAML 可解析，build-test、macos 两个既有 job 与基线完全相同，hub job 存在。本轮未改 CI；尚无 Linux、真实 Compose config 运行结果，未触发云端 CI | PyYAML 解析与 `git show fdff779:.github/workflows/sim.yml` 对照 |
| H14 | 本轮通过 | 【实测】全量、内存检查、差异范围、隐私、链接、体积均核对；本轮 9 个预期文件，14 个本地链接无缺失；基线新增 92 文件均 ≤5 MiB。指定隐私正则 0 命中（grep 退出 1），差异空白检查退出 0；仅本地提交 | `git diff --stat`；总规则 §6 隐私命令；`git diff --check`；`git status --short` |

T1、T2：从仓库根目录执行，使用任务给定的 `INK_DEPS`，两套构建参数一致，T2 仅加 `INK_SANITIZE=ON`。两套均开 `INK_WERROR`，没有重建未损坏的目录。

```sh
cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DINK_HUB_PYTHON="$PWD/hub/.venv/bin/python"
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
```

T2 将上面三行命令的构建目录换为 `build-asan`，配置时另加 `-DINK_SANITIZE=ON`。构建日志在忽略的 `work/review-build.log`、`work/review-build-asan.log`，最终全量日志为 `work/review-ctest-final.log`、`work/review-ctest-asan-final.log`，均退出 0。首轮全量通过后发现轮次收尾问题，补齐代码后重新跑了全部测试；两轮之间未改 C 源码，沿用已验证的构建。

T3：最终五次串行端到端与测量：

```sh
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^hub-simulator-e2e$' \
  --repeat until-fail:5 --output-on-failure -V -j1
```

以下均来自本轮 `work/review-e2e-five-final.log`（忽略、未提交）。每次虚拟时间 17.05 s，`turns=1`、`tool_calls=1`；RSS 为主进程，不含模拟器子进程或容器。

| 次数 | RSS（bytes） | 采样数 | p95（ms） | 最大（ms） | 墙钟（s） |
| --- | --- | --- | --- | --- | --- |
| 1 | 59867136 | 1560 | 1.216917 | 6.294083 | 17.166126 |
| 2 | 46841856 | 1548 | 1.225208 | 10.768583 | 17.070134 |
| 3 | 50987008 | 1550 | 1.225042 | 9.805083 | 17.068119 |
| 4 | 50823168 | 1556 | 1.194209 | 2.511708 | 17.115606 |
| 5 | 47448064 | 1551 | 1.194542 | 13.598333 | 17.061568 |

定向修复测试（无固件能力表依赖）：

```sh
hub/.venv/bin/python -m pytest -q hub/tests/isolation/test_delivery.py
hub/.venv/bin/python -m pytest -q hub/tests/recovery/test_jobs.py hub/tests/recovery/test_cancel_races.py
```

分别新增 21、9 项；后一个命令含原有 9 项，共 18 项。所有交错以 Event、队列或事务同步，没有用 sleep 等待。

反向对照【实测】：`git archive fdf14b8 hub` 导出到忽略的 `work/review-reverse-r1-r2`，复制本轮两个新测试文件到副本的对应路径。用当前 venv，在副本的 `hub` 目录执行 `python -m pytest -q --tb=short -p no:cacheprovider` 加下表用例；先核验导入确实来自旧代码副本。两个独立 pytest 进程均退出 1，正式源码未撤回。日志为 `work/review-reverse-R1.log`、`work/review-reverse-R2.log`。

| 组 | 用例（tests/ 下的文件与节点） | 旧代码结果 |
| --- | --- | --- |
| R1 | isolation/test_delivery.py：`test_registry_rechecks_permissions_after_handler`、`test_direct_call_drops_result_after_revocation`、`test_member_change_discards_queued_turn_command` | 3 failed、1 teardown error；可见旧 Scope 的文字、epoch=0 的翻页命令及 Future exception was never retrieved |
| R2 | recovery/test_cancel_races.py：`test_cancel_running_job_keeps_scheduler_alive[False]`、`test_cancel_and_finish_respect_transaction_order[cancel]`、`test_cancel_before_external_dispatch_prevents_handler` | 3 failed；后续任务停在 queued，完成／外部执行前竞争抛 invalid job transition |

收尾补丁另外反向验证【实测】：独立 Python 进程用 `git show fba2adf:hub/ink_hub/voice/flow.py` 取得旧函数，只在内存替换 `server.run_turn`，再运行 `pytest -q --tb=short -p no:cacheprovider hub/tests/isolation/test_delivery.py -k revoked_turn_tail`。退出 1，2 failed，均为丢弃末尾输出时的 ConnectionError；日志 `work/review-reverse-turn-tail.log`。正式源码未改回旧版。

## 没做到的

- 本轮没有未修复的严重或高问题；复查未列中级问题。
- 本机无 Docker：真实 Compose config、镜像构建／容器运行、容器峰值与跨架构镜像仍未验证。没有仅在 CI 验过的结果；按规则未 push，也未取得 Linux CI 结果。
- 本步仍仅用合成身份、假厂商、替身音频；真实厂商、配对／账号、记忆、内容和实机未接入，不冒充验证。设备对 `_meta`、新成员代次、D-105、持久确认和 budget_ms 的支持仍按后续步骤落实。

## 和设计不一致的

- 沿用原披露：`/metrics` 是开发测量 JSON，尚非设计 §5.11 的 Prometheus 文本；安装骨架是本地构建和 SSH 入口，尚无已发布镜像／公开安装地址／公网证书流程；Schema 只支持明确子集，其余关键字拒绝，多步代理与语义定范围按后续步骤实现。
- 本轮没有修改设计、需求、决策或审计历史来迁就实现。

## 改过的已有测试

- 无。只新增 `hub/tests/isolation/test_delivery.py`、`hub/tests/recovery/test_cancel_races.py`，共 30 项；未修改原 84 项 pytest、原 86 项 CTest、断言、预算、跳过规则或 CI 步骤。

## 要用户定的

- 本次修复无待决项。两端约定原有待定内容及 D-86 的真实数据确认条件保留；没有使用真实资料、付费接口或操作设备。

## 顺手发现

- 【实现】架构审视：Registry 继续统一能力描述、参数和结构化回执；权限在能力执行前后复核，连接／成员／代次在发送关口复核，RPC 与语音共用该路径。用户／AI 的已有能力入口未改，未发布占位能力；没有新增页面或设备常驻任务。
- 【实测】首次从根目录直接运行全部 pytest 时未指定能力表位置，得到 110 通过、2 个环境跳过；随后通过 T1/T2 正确传入生产导出表。最终两套均为 114/114、0 跳过，没有改跳过条件。
- 【实现】README 的示例构建并发由 8 改为 4，与用户重启后更新的总规则一致；保留复查报告中的历史命令，不改旧审查证据。
