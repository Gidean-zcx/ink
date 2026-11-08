# 无线传文件任务报告

基线：`cb69b93820fccc36caf28b3d5ead8d0e3878fac5`。工作分支：`claude/codex-wireless-transfer`。

## 复查后的修复

2026-11-09，接续 `9a149202`，处理本轮复查的两项中级问题；严重、高、低均无待修条目。以下为本轮记录，原报告保留为上一轮证据。

【实测】任务书验收 1～4 本轮均通过；测试环境为 macOS clang，Linux/gcc 仍未实测。

| 项目 | 结论与证据 |
| --- | --- |
| WT-R1-01 | 【实测】专项通过。16 MiB 连续上传在虚拟时间超过 120 秒后仍返回 `201 文件已收到`；逐字节核对内容、目录和稳定编号。授权期限仅约束尚未使用的授权；接收仍受原有 10 秒无进展超时约束。 |
| WT-R1-02 | 【实测】专项通过。非法文件名与错误授权码均发送 2 MiB 请求体，分别读到完整 `400`、`403` 中文回执；拒绝后停发也会超时。先解析合法长度，发送回执后沿用原接收循环消费剩余内容，读完或断开后收尾；不再依赖关闭 socket 时的 64 KiB 排空上限。 |
| 回归覆盖 | 【实测】21 个客户端阶段、104 项设备检查通过，包含原有中断、空间不足、非法名称、授权过期、U 盘互斥、成员隔离及 AI 能力调用。日志：`work/wireless-transfer/recheck-focused.log`。 |
| 反向对照 | 【实测】七项均检出，命令退出 0、各变体客户端退出 1。撤回 WT-R1-01 与 WT-R1-02 分别复现 `BrokenPipeError`；保留能力注册、授权、超时、NUL、U 盘的反向检查。在忽略目录编译、链接成功后才运行，产品源码不变。日志：`work/wireless-transfer/recheck-reverse.log`。 |
| 普通全量 | 【实测】Werror 构建退出 0；207/207 通过，零失败、零跳过，CTest 退出 0，144.75 秒。中枢合同包含 125 项 pytest 用例；原有能力文档及三项跳过测试全部通过。日志：`work/wireless-transfer/recheck-build.log`、`work/wireless-transfer/recheck-ctest.log`。 |
| 内存检查 | 【实测】ASan/UBSan 加 Werror 构建退出 0；207/207 通过，零失败、零跳过，CTest 退出 0，236.72 秒。无线专项仍为 21 个客户端阶段、104 项设备检查；完整测试日志无 AddressSanitizer、LeakSanitizer 或未定义行为错误。日志：`work/wireless-transfer/recheck-asan-build.log`、`work/wireless-transfer/recheck-asan-ctest.log`、`build-asan/Testing/Temporary/LastTest.log`。 |
| 规模、机制与架构 | 【实现】本轮产品代码仅 `system/services/file/transfer.c`，新增 26 行、删除 16 行；相对任务基线累计 315 行（保守包含 CMake 测试注册），低于 400 行。只多保留当前连接的响应指针，复用接收缓冲、剩余长度、执行器及超时，无新增任务、状态机、配置或重试机制；HAL 和原子能力入口不变。 |

原有全量问题的处理：

- `capability-table-doc`：生成文档缺少基线已有的 `file.index`、`file.lookup`、`file.index_state` 及本任务的三项 `transfer.*`。按本轮明确要求“全量测试全过”，使用原工具同步 `docs/system/capabilities.md`，这是对旧任务规则 §7 延后生成要求的最小调整；没有修改检查规则。
- 三项中枢测试跳过：构建 Python 缺少 `pytest==8.4.2`、`pytest-asyncio==1.2.0`、`aiohttp==3.12.15`、`PyYAML==6.0.3`，导致协议样本也未生成。依赖目录已有 Python 3.11.15 的锁定环境，四项版本已实测核对；选择 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，执行测试时加 `PYTHONPATH="$PWD/hub"`，并核对实际导入本分支代码。没有安装包、修改依赖或改变跳过规则，见 `work/wireless-transfer/recheck-dependencies.log`。

本轮已有测试的调整：非法名称、空间不足的最终状态改为等待接收清理结束后断言，断言内容不变；非法名称、错误授权码从仅检查 HTTP 状态加强为核对完整中文正文。只有新增 16 MiB 阶段的客户端等待上限取 300 秒虚拟时间，其他阶段仍为 60 秒；场景结束从 400 秒延到 600 秒以容纳新上传，CTest 超时及产品超时不变。首次修复后的专项暴露了原断言在收到早期回执后立即检查清理状态的问题，修正等待点后通过；没有用 sleep 掩盖时序问题。

配置沿用 REVIEW 的完整命令，两版均增加上述 `INK_HUB_PYTHON`；ASan 只另加 `-DINK_SANITIZE=ON`，保留 `-DINK_WERROR=ON`。命令从仓库根执行，环境变量按用户给定值：

```bash
export PYTHONPATH="$PWD/hub"
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/wireless_transfer/reverse.py --build build
```

【实测】改动文件检查通过，生成能力表的 256 个相对链接均存在。任务基线至最终提交的隐私扫描有 1 处匹配，是既有 REVIEW 中扫描命令自身的正则文本，未发现隐私数据；本轮修复基线 `9a149202` 至最终提交扫描零匹配（grep 退出 1）。`git diff --check` 通过；范围及预算日志：`work/wireless-transfer/recheck-scope.log`。未更改已有 REVIEW、策略、界面或文件编号实现。

下面原有小节保留复查前的验收记录；本轮结论以上述复查修复记录为准。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 外部 Python 进程通过真实 TCP/HTTP 上传 64 KiB 二进制文件，核对全部字节；固定落在 `/me/收件箱/`，索引与编号查询成功 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^integration-wireless-transfer$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 18 个客户端阶段、90 项设备检查：断开、两分钟授权过期、十秒接收超时、写前及写中空间不足、非法名称（含编码及原始 NUL）、错误授权码、重放、成员切换与儿童隔离；失败不留下目标或临时文件 | 同上；最终全量测试的 `build/Testing/Temporary/LastTest.log` |
| 3 | 【实测】通过 | U 盘已开启时拒绝传输；授权等待期间即占用存储卡，U 盘返回忙；关闭传输后 U 盘恢复 | 同上 |
| 4 | 未全通过：共享能力文档待合并时生成 | 【实测】Werror 普通及 ASan/UBSan 构建通过；两轮全量均为 203 通过、3 跳过、1 失败（共享能力文档过期），没有 sanitizer 错误；AI 能力清单及确定性测试通过。【实现】按新增行保守计数 305 行，含 CMake 测试注册行，低于 400 行 | `taskpolicy -c utility cmake --build build -j3`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；ASan 换 `build-asan`（两轮 ctest 均退出 8）；`work/wireless-transfer/ctest-final.log`、`work/wireless-transfer/asan-ctest.log` |

## 没做到的

- 普通全量不是全绿：`capability-table-doc` 需要重新生成共享能力文档，任务规则 §7 明确禁止本分支修改该文件。保留失败，不跳过检查；候选生成结果在忽略的 `build/wireless-capabilities.md`。
- 3 项中枢测试（`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`）因既有锁定 Python 环境缺失而跳过，没有安装或更改依赖。
- 本次仅在 macOS clang 上实测；未运行 Linux/gcc，没有触发远端 CI。
- 模拟器仅监听本机环回地址；设备端 HAL、真实 Wi-Fi 与浏览器界面操作尚未实测。真实 socket 不用于估算设备吞吐或功耗。

## 和设计不一致的

- 服务设计 §13 将无线传输列为以后工作；本任务明确要求实现。协议按任务书采用最简单的单文件 HTTP 上传。

## 改过的已有测试

- `tests/integration/ai_surface.c`：在 `FOR_AI` 明确加入 `transfer.start`、`transfer.stop`、`transfer.status`，三项均无必填参数；这是新增能力的完整性预期，未删除或放宽任何断言。原测试先失败 3 项，补入后通过。

## 要用户定的

- 暂无阻塞实现的选择。超时暂取授权 120 秒、接收无进展 10 秒；沿用共享定时器，没有新增配置项。

## 顺手发现

- 【实测】首次专项测试发现拒绝上传时，直接关闭仍有请求体待读的 socket 会使客户端收到连接重置，错误正文可能丢失。已在 HAL 集中修复：先结束发送，再有界排空已到达的请求体；原失败日志在 `work/wireless-transfer/response-reset-before.log`。

## 新增的机制

- 单文件 HTTP 上传及六位一次性数字授权：任务书明确要求；使用既有后台执行器、共享定时器、存储卡占用、`.part` 临时文件及原子改名。
- 单客户端字节流 HAL：外部真实上传及平台分层规则要求；模拟器通过 `vtk_host_call` 执行宿主机 socket 操作，不新增线程、持久格式、后台重试或配置项。

## 验证补充

- 配置沿用任务规则 §5 的字体和 Python 参数，增加本地 SDL2、ImGui 源码覆盖：`-DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"`；ASan 只换构建目录并增加 `-DINK_SANITIZE=ON`，保留 `-DINK_WERROR=ON`。
- 【实测】最终专项（普通、ASan/UBSan）均通过：18 个客户端阶段、90 项设备检查。拒绝非法名称时发送完整 64 KiB 请求体，确认客户端能读到完整错误响应。日志为 `work/wireless-transfer/focused-final.log` 和 `work/wireless-transfer/asan-focused-final.log`。
- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/wireless_transfer/reverse.py --build build` 退出 0；六个临时变体的客户端均退出 1：移除服务注册、授权校验、超时、编码 NUL 检查、U 盘占用、响应排空。最后一个变体复现 `ConnectionResetError`。只替换忽略目录内的源码副本并重新链接，产品源码不变；日志在 `work/wireless-transfer/reverse.log`。
- 【实测】能力文档可生成至忽略目录：`"$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/wireless-capabilities.md`，退出 0。现有 AI 覆盖测试与编译后能力元数据检查均通过。
- 【实现】架构审视：网络收发在 HAL，授权与接收在文件服务，存储和成员权限复用原服务；开关和查询全部经 `transfer.*` 原子能力，人和 AI 走同一实现。没有新增设备端页面；AI 通过 `transfer.status` 获取结构化状态。
- 【实测】隐私扫描按任务规则 §6 的原命令执行：基线到 HEAD 的差异 0 匹配，grep 退出 1（无匹配）。`git diff --check` 通过；差异没有夹带策略、界面、文本、稳定编号实现或共享文档改动。

## 为什么不能更小

- 现有网络 HAL 只有出站 WebSocket，没有监听或接收 HTTP 的接口；仅改文件服务无法从外部真正收文件。新增一个窄的字节流 HAL 和单文件上传实现，全部按后台执行器串行运行。
- 已考虑复用出站 WebSocket，但它要求中枢及额外协议，不符合本任务最简单 HTTP 的边界；直接在系统层调用 socket 会破坏平台分层。U 盘互斥复用现有存储卡占用，没有修改文件编号实现。

## 要改的文档

- 合并时更新服务设计 §13 与能力表、界面依赖中无线传输的状态，再运行 `python3 tools/capabilities.py` 更新生成文档；本分支按任务规则不修改共享文档。
