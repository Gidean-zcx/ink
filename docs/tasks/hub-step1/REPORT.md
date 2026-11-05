# 家庭中枢第 1 步执行报告

基线：`fdff779f2f2746ab10194e32a00b7dc343365e2c`。分支：`claude/codex-hub-step1`。日期：2026-11-05。只在本地提交，未推送或创建 PR。

本次接续 `8a98acc` 的[第 1 轮复查](REVIEW.md)：R1、R2 均为高，严重和中级均为 0。重启后已核对 `fdf14b8`、工作区和报告，保留原有改动；后续构建最多 `-j4`，CTest 最多 `-j3`，构建／全量测试串行，不启动窗口。R1、R2 已实现并通过定向检查，最终全量与内存检查待跑；以下开发阶段的通过记录不能覆盖复查发现，H2/H3/H6/H7/H14 须以本次重测结果为准。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1 | 已修；待全量复验 | 【实测】Registry 执行后复核；直接调用异步分派；出队、RPC 返回、取消、切成员、断线／重连及听写保护新增 19 项通过。撤回本轮修复的隔离副本中，3 个核心反例均失败，另捕获 1 个未取走 Future 异常 | `hub/.venv/bin/python -m pytest -q hub/tests/isolation/test_delivery.py`；`work/review-reverse-R1.log` |
| R2 | 已修；待全量复验 | 【实测】取消与完成按事务状态处理，取消赢则丢结果；并发取消、外部执行前取消／撤权、执行失败撤权和后续任务继续等新增 9 项通过。旧代码反向对照 3 项失败 | `hub/.venv/bin/python -m pytest -q hub/tests/recovery/test_jobs.py hub/tests/recovery/test_cancel_races.py`；`work/review-reverse-R2.log` |
| H1 | 通过 | 【实测】Python 3.11.15、uv 0.9.5；独立新环境安装、CLI 就绪、healthz/setup、84 项测试通过；目录和中文说明见 [README](../../../hub/README.md) | 下方“新环境复现”命令 |
| H2 | 通过 | 【实测】开发握手、目录、双向工具回执、实时会话、文字/语音控制流符合既有协议；开发握手默认拒绝 | `ctest --test-dir build -R '^hub-contract$' -V` |
| H3 | 通过 | 【实测】pytest **84/84**，无跳过；假设备独立词法计数，含乱序、超限、错误回执、重连、请求及轮次超时 | 同 H2；`python -m pytest -q`（hub 目录） |
| H4 | 通过 | 【实测】契约生成 **142 条**出站 JSON，生产 `ink_json.c` 全部接受，最高 **110** 词法单元；128 边界接受，129 反例拒绝；H10 每次另验 19 条 | `ctest --test-dir build -R '^hub-json-crosscheck$' -V` |
| H5 | 通过 | 【实测】运行当前产品初始化代码导出 **131 条工具、104 个 actions**；生产 768 字节缓冲确实溢出；空数组触发分页回查，定义不丢；caps 按完整信封 ≤110 切页 | `ctest --test-dir build -R '^hub-export-caps$' -V`；H3 对应用例 |
| H6 | 通过 | 【实测】两家各三人的 Scope 隔离、家长关系、权限版本、代次、范围每个字段的反例；异步权限变化；遍历全部已安装调用能力 | H3 的 `tests/isolation` |
| H7 | 通过（合成负载） | 【实测】三个假厂商接口；SQLite 队列重开、租约恢复、持久去重、参数摘要冲突、unknown 不重放、执行中撤权；调度器自动执行恢复的假任务；worker 文件探针通过 | H3 的 `tests/recovery`、`tests/surface` |
| H8 | 本机通过；Docker 未验 | 【实测】预检、端口占用、可信本机入口、一次性设置码、向导进度、拒收密钥、脚本预演及 mock Docker 安装、worker 隔离配置通过；Compose config 已配置到 CI，尚未运行 | H3；`sh -n hub/deploy/install.sh` |
| H9 | 通过 | 【实测】普通、ASan/UBSan 各 **91/91**，含原有 86 项和两项确定性；旧代次/关闭后回调测试通过；关闭桥时，主页、阅读、AI 三场景的 JSON 和 PNG 与基线逐字节一致 | 全量命令；下方“基线对照”命令 |
| H10 | 通过 | 【实测】最终代码连续 **5/5**；每次 1× 实时、设备答案 1 次、AI 翻页 1 次、播放 ≥300 ms；无坏消息/桥丢包；缺依赖或缺样本明确退出 77，不复用旧样本 | `ctest --test-dir build -R '^hub-simulator-e2e$' --repeat until-fail:5 --output-on-failure -V` |
| H11 | 通过 | 【实测，开发机】峰值 RSS **48.23～65.22 MiB**；事件循环 p95 **2.173～2.183 ms**，最大 **23.813 ms**；五次数字与方法已写 [§4.3](../../hub/hub-architecture.md#43-规格) | 同 H10；输出 `measurement.json` |
| H12 | 通过 | 【实现】逐项字段、规则、来源、待定项与默认值、设备改动单列于 [两端约定](../../hub/contracts-step1.md) | 文件和链接检查 |
| H13 | 通过（配置） | 【实测】YAML 可解析，两个既有任务与基线逐项相同；新增 Linux 任务，无密钥，含契约、C 交叉验证、五次端到端及 Compose config；未触发云端 CI | PyYAML 解析与基线 jobs 对照；`git diff fdff779..HEAD -- .github/workflows/sim.yml` |
| H14 | 通过 | 【实测】全量、内存检查、范围/文件/链接/体积检查完成；基线至 HEAD 的指定隐私正则 **0 命中**（grep 退出 1）；只提交相关文件；工作区干净 | `git diff --stat`；总规则 §6 的隐私命令；`git status --short` |

新环境复现（从仓库根目录；`INK_DEPS` 使用任务约定）：

```sh
"$INK_DEPS/build-deps/pyenv/bin/python" -m venv work/hub-tools
work/hub-tools/bin/python -m pip install uv==0.9.5
UV_CACHE_DIR=$PWD/work/uv-cache UV_PROJECT_ENVIRONMENT=$PWD/work/hub-fresh \
  work/hub-tools/bin/uv sync --project hub --frozen --python "$INK_DEPS/build-deps/pyenv/bin/python"
work/hub-fresh/bin/python tests/hub/check_fresh.py --out work/fresh-cli-check
work/hub-fresh/bin/python tests/hub/run_contract.py \
  --caps build/test-out/hub/caps.json --samples work/hub-fresh-messages.jsonl
```

先按 README 构建并导出能力表。普通构建为任务指定的 CMake 参数，另设 `-DINK_HUB_PYTHON="$PWD/hub/.venv/bin/python"`；全量命令为 `cmake --build build -j8` 和 `"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j8`。内存检查同参数换 `build-asan`，仅加 `-DINK_SANITIZE=ON`，仍开 `INK_WERROR`。最终全量分别 91/91，耗时 58.19 s、109.19 s；未改预算或原测试期望。

基线对照：用 `git archive` 将指定基线的构建源码导出到忽略的 `work/baseline-src`，同一编译器和依赖构建 `work/baseline-build/ink-sim`；仅关闭 GUI 构建目标，不改变便携系统的编译参数。然后：

```sh
hub/.venv/bin/python tests/hub/compare_default.py \
  --before work/baseline-build/ink-sim --after build/ink-sim \
  --assets build/assets --content build/test-content --out work/default-comparison
```

`home_basic`、`reader_basic`、`ai_ask` 的 `metrics.json` 和 `final.png` 全部逐字节一致。基线导出和构建未操作原分支或只读依赖目录。

反向对照【实测】：在工作副本或隔离源码副本撤掉相应保护，运行下列定向用例，均退出 1；恢复后 84 项全过。原始日志在忽略的 `work/reverse-*.log`，未提交。

- `contract/test_protocol.py::test_concurrent_reply_does_not_swallow_cancellation`：撤掉 `Connection.request` 的取消状态复核，失败 1 项。
- `isolation/test_async_output.py`：分别撤掉 `valid_output` 权限复核、把单会话取消改回取消所有轮次，各失败 1 项。
- `surface/test_framework.py::test_preflight_resource_probe_can_be_unavailable`：资源探针失败改成抛出，失败 1 项。
- `recovery/test_jobs.py::test_revoked_during_execution_cannot_publish_or_stop_queue`：撤掉执行中撤权处理，失败 2 项。
- `surface/test_deploy.py::test_compose_worker_is_isolated`：tmpfs 参数漏引号被 YAML 拆成三个挂载项时失败；补引号后通过。

## 没做到的

- **没有任何“只在 CI 验过”的结果**：本机无 Docker，镜像构建、真实 Compose config、容器运行及 Linux 云端任务都未执行。CI 步骤已加入，按任务要求未 push，所以尚无云端结果。mock 脚本测试不冒充 Docker 验证。
- `worker` 本步只有隔离容器定义和有界合成哈希探针，尚未测容器峰值；真实内容解析在第 5 步。跨架构镜像未验证。
- 按任务范围不接真实厂商、配对/账号、记忆、内容或真实音频；只有示例向导，不能开启真实数据模式。设备侧新成员代次、D-105、确认结果持久化和 budget_ms 尚未落地，本步使用协议缺省值。

## 和设计不一致的

- `/metrics` 本步返回开发测量 JSON，尚未实现设计 §5.11 的 Prometheus 文本格式。
- 安装骨架使用本地构建和 SSH 本机入口；本步没有公开安装地址、已发布镜像或公网证书申请/续期。正式可信 HTTPS 与配对按第 3 步验证；不改设计迁就实现。
- Schema 校验为明确的常用子集，复杂关键字拒绝；不是完整 JSON Schema 实现。定范围语义模型和多步代理循环仍按第 2a 步建设；本步仅提供框架与单次假工具调用。

## 改过的已有测试

- **无**。原 86 项测试、断言与预算均未修改；只新增测试和构建注册。CI 原 `build-test`、`macos` 的步骤逐项未变。

## 要用户定的

- 本步无阻塞验收的待选项。设计没定的 token 编码、自动动作查询参数、范围语义格式在两端约定中标“待定”，默认拒绝越界/高风险、结果不明不重试。
- 第一次用真实家庭资料前仍需确认 D-86、配置对象存储并演练恢复；本次没有使用真实资料。

## 顺手发现

- 【实测】当前能力清单规模已超过旧设计中 53 个全局名字的例子；没有改设备，兼容路径已由运行时导出与分页测试覆盖。
- 【实测】受限开发环境禁止读取整机内存容量，预检报未知；没有申请额外权限或填估计数字。
- 【实现】架构审视：描述表生成目录与参数定义，应用/模型均走同一 Registry，输出经连接/会话/代次/权限复核；尚未实现的模块不发布可调用能力。没有修改 system、apps、drivers 或设计稿，没有增加设备常驻任务。
