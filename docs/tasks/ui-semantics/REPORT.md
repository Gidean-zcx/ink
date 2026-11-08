# ui-semantics 任务报告

日期：2026-11-08；复查修复：2026-11-09。基线：`ed3d122665cf704bb77a962f1fe38d66f6febe01`。
范围：应用框架 F1；只增加控件语义接口及测试页面，不改已有页面和 AI 服务。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | [接口](../../../system/include/ink/sys/ui_semantics.h)、[实现](../../../system/ui/ui_semantics.c)；[测试页](../../../tests/ui_semantics.c)真正建页、触摸按钮走能力总线 | `ctest --test-dir build -R '^ui-semantics$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 同一声明生成主题组件及 JSON；字段、动作、对象、对象修订号、运行时页面版本均断言；AI 上下文与页面快照相同 | 同上 |
| 3 | 【实测】通过，反向失败符合预期 | 密码、验证码有值／无值；不同秘密长度快照相同；秘密对象引用省略；去掉遮盖后 3 个断言失败，CTest 退出 8 | 同上；见反向对照 |
| 4 | 【实测】通过，反向失败符合预期 | 全局能力也在执行前拒绝旧页面版本，调用次数不变；新版本可以执行；去掉版本比较后 3 个断言失败，CTest 退出 8 | 同上；见反向对照 |
| 5 | 【实测】本机通过 | 修复后普通及 ASan／UBSan 各 206/206，零失败、零跳过；产品保守计数 165 行，小于 300 行 | 见“复查后的修复” |

## 没做到的

- 五项本机验收均已完成。Linux／gcc 未运行，不能以 macOS／clang 结果代替。
- F2 以后的输入宿主、通用渲染器和既有页面迁移按任务边界没有做；按规则未接触真机、未推送或开 PR。

## 和设计不一致的

- 无 F1 范围内的已知差异。完整设计中的输入会话、分页和生成描述校验不属于本任务。

## 改过的已有测试

- `tests/sdk_boundary.cmake`：只在公开 SDK 头文件允许表追加 `ui_semantics`，使应用可以包含本次接口；没有删除检查、改变断言或放宽资源预算。
- `tests/ui_semantics.c`（本轮）：追加权限回归，所有原有 `CHECK` 均保留，期望不变；完成标记移至新增检查之后，未加 sleep 或改预算。

## 要用户定的

- 无。

## 顺手发现

- 首次全量因默认 Python 缺少服务器依赖，有 3 项自动跳过；改用现有锁定依赖环境后重跑，全量 206 项均执行并通过，没有更改依赖版本。

## 新增的机制

- F1 页面声明、快照及动作校验接口：任务书明确要求；复用现有 JSON 写出器、页面版本及能力总线。
- 没有新增产品任务、状态机、持久格式或配置项。测试驱动任务只存在于测试可执行文件。

## 要改的文档

- 主会话可在 backlog 中标记 F1 的实际验收结果，并在应用框架设计的实现状态处关联本报告；本任务不改共享文档。

## 规模与阶段证据（首轮）

- 【实测】完整 `-DINK_WERROR=ON` 构建退出 0；F1 测试退出 0。首轮测试页误传空图标名，改用既有 `memo` 图标后通过，没有修改产品组件。
- 【实测】头文件 61 行、实现 87 行、全部 CMake 新增行 7 行（连测试接线也计入），合计 155 行；未超过 300 行预算。超过 100 行是因为需要公开结构体、完整 JSON 导出和执行前版本校验。更小的手写页面快照无法证明同源；仅依赖能力自己的页面绑定则不能保护全局能力。没有加入绘制层或新版本机制。
- 【实现】划分：语义与版本检查在 UI 框架，业务动作仍由原能力执行；按钮与 AI 共用同一绑定，秘密值没有 AI 读取入口；没有服务内部状态直写。

## 反向对照（首轮）

【实测】逐个临时修改 `system/ui/ui_semantics.c`，每次只构建 `sim-ui-semantics` 并运行验收表中的定向命令，不改测试；两个反向实验后已恢复源码，`git diff --exit-code -- system/ui/ui_semantics.c` 退出 0。

| 临时撤掉什么 | 实际失败 | 命令及输出 |
| --- | --- | --- |
| 秘密字段的有值／无值分支，改为直接写 `f->value` | 密码／空验证码、秘密长度不泄露、已填验证码／空密码 3 项断言失败 | `cmake --build build --target sim-ui-semantics -j3` 退出 0；定向 CTest 退出 8，`test.fail=3`；本地日志 `build/ui-semantics-negative-mask.log` |
| `target->page_version != rc->page_gen` 比较 | 旧版本动作误执行，后续调用次数断言也失败，共 3 项 | 同样构建退出 0；定向 CTest 退出 8，`test.fail=3`；本地日志 `build/ui-semantics-negative-version.log` |

构建与测试命令均加 `taskpolicy -c utility`；CTest 使用 `$INK_DEPS/build-deps/pyenv/bin/ctest`。

## 完整验证命令（首轮）

配置采用总规则第 5 节的依赖参数；`build-asan` 只额外启用 `-DINK_SANITIZE=ON`。两个目录都用已有选项 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 指向现成服务器测试依赖。已核对依赖版本与锁定版本一致，并确认导入的是本检出的 `hub/ink_hub`，没有写入依赖目录。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
export UBSAN_OPTIONS=halt_on_error=1
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

- 【实测】普通全量退出 0，206/206，零失败、零跳过；耗时 109.48 秒，本地日志 `build/ui-semantics-ctest.log`。包含确定性、分层与 SDK 边界检查；GUI 冒烟测试使用 SDL dummy，无窗口。
- 【实测】ASan／UBSan 构建退出 0，全量退出 0，206/206，零失败、零跳过；耗时 216.59 秒，本地日志 `build-asan/ui-semantics-ctest.log`。两套配置均为 `INK_WERROR=ON`；F1 均为 29 项断言通过、`test.finished=1`，内部 RAM 最低余量 47,840 字节（原阈值 32,768）。
- 【实测】基线至当前提交的 `git diff --stat` 仅有 7 个任务相关文件；`git diff --check` 退出 0。按总规则第 6 节原样运行隐私扫描，零匹配（grep 退出 1）。

## 复查后的修复

2026-11-09，接续提交 `3e503e74`，修复提交 `117a6fe0`。复查仅有 US-R1-001（中）需要修复，严重、高、低均无待修项。

- US-R1-001【实测】：已修。公开 `ink_cap_ai_callable()`，只组合既有 `listed()` 和事件排除；语义快照、完整能力目录、分页目录和编号目录共用这一判断。原调用策略、成员模块及 AI 服务未改。
- 回归【实测】：在真实成人／儿童空间分别检查 `confirm.accept`、`confirm.result`、儿童非白名单能力、系统能力和允许能力，并校验隐藏动作的参数未导出、执行仍被拒、没有副作用。原实现 52 项通过、8 项失败、`finished=1`，定向 CTest 退出 8；修复后 60 项通过、零失败，退出 0。日志为 `build/ui-semantics-review-red.log`、`build/ui-semantics-review-red-metrics.json`、`build/ui-semantics-review-green.log`。
- 再次反向对照【实测】：全量完成后，仅在编译输入中将快照筛选还原为 `ink_cap_available()`，确认与 `3e503e74` 的该文件完全相同，复用普通构建的编译／链接参数运行同一测试。编译、链接均退出 0；模拟器退出 1，52 项通过、8 项失败、`finished=1`。产物和结果仅在忽略目录 `work/ui-semantics-review-negative/`（`summary.json`、`run.log`、`sim/metrics.json`）；没有更改产品源码、测试或已验证的可执行文件。`git diff --exit-code HEAD -- system tests` 退出 0。
- 历史跳过原因【实测】：默认构建 Python 缺 `pytest`、`pytest-asyncio`、`aiohttp`、`PyYAML`，会使 `hub-contract`、`hub-simulator-e2e` 跳过，并使依赖消息样本的 `hub-json-crosscheck` 跳过。本轮使用已有 `INK_HUB_PYTHON`；四项版本为 8.4.2、1.2.0、3.12.15、6.0.3，与 `hub/uv.lock` 一致，确认导入本检出的 `hub/ink_hub`，未安装或升级依赖。
- 规模【实测】：本轮产品代码 8 行新增、4 行删除；相对 F1 原始基线保守计 165 行（新增与删除均计入，含 CMake 接线），低于 300 行。没有新增任务、状态机、缓存、格式或配置项；仅提取现有判断，未改已有测试期望与预算，完成标记移至新增儿童检查之后。
- 本轮普通全量【实测】：配置、构建与 CTest 均退出 0，206/206，零失败、零跳过，111.58 秒；`hub-contract`、`hub-simulator-e2e`、`hub-json-crosscheck`、确定性与 SDK 边界均通过。日志 `build/ui-semantics-review-configure.log`、`build/ui-semantics-review-build.log`、`build/ui-semantics-review-ctest.log`。
- 本轮 ASan／UBSan【实测】：配置、构建与 CTest 均退出 0，206/206，零失败、零跳过，201.67 秒；此前跳过的三项也全部执行并通过。已核查编译参数含 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`，设置 `UBSAN_OPTIONS=halt_on_error=1`。日志对应 `build-asan/ui-semantics-review-configure.log`、`build-asan/ui-semantics-review-build.log`、`build-asan/ui-semantics-review-ctest.log`。
- 资源【实测】：两轮 F1 均为 60 项断言通过、零失败、`finished=1`；内部 RAM 最低余量均为 47,840 字节，原阈值 32,768；内外部内存分配失败均为 0。没有残留的失败或跳过测试；Linux／gcc 与真机仍未运行。

本轮配置继续使用总规则第 5 节的完整依赖参数，并指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；内存配置仅多 `-DINK_SANITIZE=ON`。命令如下，构建与测试逐轮串行，未启动窗口：

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

- 收尾【实测】：本轮只改能力头文件、能力目录判断、语义快照、对应测试和本报告 5 个文件；`git diff --check` 退出 0，本报告本地链接均存在。按总规则第 6 节扫描原始基线至本轮的差异，本机绝对路径及 MAC 模式零匹配（grep 退出 1）。未动共享文档、既有页面及其它并行任务的实现，未推送。
