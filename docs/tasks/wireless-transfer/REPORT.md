# 无线传文件任务报告

基线：`cb69b93820fccc36caf28b3d5ead8d0e3878fac5`。工作分支：`claude/codex-wireless-transfer`。

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
