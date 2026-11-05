# pairing 执行报告

2026-11-06 第二次集成门禁修复。起点 `939d1e6e`，集成目标 `f1f72ae9`（开工时的 `FETCH_HEAD`）。此前配对开发与第一次集成验证见 `939d1e6e` 中的本报告及[独立复查](REVIEW.md)；旧全量结果不作为本轮验收证据。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| I1 合并最新集成提交 | 已合入【实测】 | 合并提交 `acbc125d`，CMake 和依赖状态自动合并，无文本冲突；集成目标为 HEAD 祖先，原 6 项能力检查完整保留 | `git merge --no-ff --no-commit FETCH_HEAD`；`git merge-base --is-ancestor FETCH_HEAD HEAD` |
| I2 门禁失败修复 | 定向通过【实测／实现】 | 复现对象式字段宏解析失败；增加展开及递归保护，保留重复字段拒绝。生成合并后的 208 项定义（151 能力、14 设置、43 状态），6/6 能力 CTest 通过 | T1 |
| I3 回归与反向对照 | 通过【实测】 | 原 13 项和新增 4 项共 17/17 通过；将旧生成器载入临时模块后，新回归出现 2 个失败子项、1 个错误，真实仓库再次在 CONTROL 处失败；恢复当前模块后通过 | T2 |
| G1 全量与内存检查 | 待执行 | 普通与 ASan/UBSan 全量逐轮串行，不放宽断言、预算或时限 | — |
| G2 能力、分层与 AI | 保持【实现／实测】 | 只修改离线生成器、回归和生成结果；产品源码、能力执行与人工确认路径未改。运行时注册表及真实成员／确认策略检查通过 | T1；文件差异 |
| G3 范围、隐私及提交 | 待执行 | 仅合入集成内容并修复门禁，遵守不 push 约束 | — |

命令中的 `python`、`ctest` 均使用 `$INK_DEPS/build-deps/pyenv/bin/` 下对应程序。日志及临时模块只在忽略的 `work/`、`build/` 和 `build-asan/` 中，不提交。

- T1：按总规则 §5 配置 `build`，额外使用只读 `$INK_DEPS/build/_deps/` 的 SDL2、ImGui 源码，保留既有 `INK_GUI=ON` 和 `INK_HUB_PYTHON`；`cmake --build build -j4`、`SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build --output-on-failure -j3 -R '^capability-table-'` 均退出 0，后者 6/6，2.45 秒。日志 `work/pairing-gate2-configure.log`、`work/pairing-gate2-build.log`、`work/pairing-gate2-targeted.log`。
- T2：`python -B tests/capability_table/test_catalogue.py` 退出 0，17/17；修复前的同命令因 CONTROL 报错。修复后用 `git show acbc125d:tools/capabilities.py` 在临时文件载入旧模块，运行当前 `InitializerMacroTest` 和 `old.load(Path.cwd())`，预期失败均复现；对照脚本核实失败后退出 0，没有改动工作区源码。日志 `work/pairing-gate2-before.log`、`work/pairing-gate2-macro-before.log`、`work/pairing-gate2-regressions.log`、`work/pairing-gate2-reverse.log`。

## 没做到的

- 全量测试、内存检查和最终核查尚未完成，不能据定向结果认定本轮通过。
- 本机是 macOS／Apple clang，未实测 Linux/gcc、真机、真实 TLS/NVS 或手机网页。

## 和设计不一致的

沿用既有模拟器验证边界：真实手机网页、首次开机页面和真机 NVS 映射仍未实现。本轮不改设计迁就代码。

## 改过的已有测试

仅在 `tests/capability_table/test_catalogue.py` 新增 4 项回归：对象宏／别名／续行及函数宏组合的字段与权限、重复字段、未知宏、递归宏。原有 13 项期望逐字保留；没有删除或跳过测试，没有放宽断言、预算或时限。合入的能力测试及原配对、成员隔离测试均保留。

## 要用户定的

没有新产品决定，按用户授权继续修复和验证。

## 顺手发现

门禁前缀下存在 `.merge`、`.cmake`、`.build`、`.ctest`，没有 `.ci`、`.privacy`。门禁构建成功；失败证据集中在能力清单解析阶段。
