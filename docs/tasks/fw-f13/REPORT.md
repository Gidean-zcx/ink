# FW-F13 执行报告

基线：`2228df7de99de9fea15fddc22868a0de1caae770`。分支：`claude/codex-fw-f13`。说明、示例及验证已完成；仅本地提交，未推送、未碰设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实现＋实测】通过 | `docs/system/app-framework.md` 的 11 个接口主题，43 处头文件行号；缺失接口标“还没有” | `"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/jot/check_doc.py` |
| 2 | 【实测】通过 | `examples/jot/`；首次写入 158 项、独立进程恢复 59 项检查；同能力、真实触摸、快照与 LVGL、成员隔离、孩子策略、恢复页码 | `"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/jot/run.py --build build` |
| 3 | 【实测】通过 | 10 块 C 代码逐字存在于编译的示例中；链接和行号存在性同时检查 | `"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/jot/check_doc.py` |
| 4 | 【实测】通过 | 普通构建 259/259（389.50 秒），ASan/UBSan 259/259（572.67 秒），均零跳过；示例 C/H 共 228/300 行（测试驱动按总规则 §7 不计）；产品代码 0 行 | 两套构建及全量命令见下；四项反向对照均检出预期失败 |

## 没做到的

验收范围无遗留。没有上机或 Linux 实跑；本机为 macOS/Apple clang。示例只覆盖短笔记，不声称有长文、附件或设置页面；其接口在说明中按真实头文件列出。

## 和设计不一致的

未改设计合同。示例的首次集合读取由宿主在界面任务外准备；说明明确这一接入条件，没有假造异步打开接口。

## 改过的已有测试

无。

## 要用户定的

无。

## 顺手发现

【实现】`cap.h` 中 `child_ok` 的“白名单还能收窄”注释与 `members.c:366` 不同，当前是 true 放行、false 时白名单还能额外放行；`appreg.h` 注释称最多 16 个应用，实际 `app_registry.c:27` 是 32 个；`INK_FX_SYSTEM` 枚举注释不能代表 `cap.c:80` 的实际确认规则。仅在说明中区分，不改框架。

## 新增的机制

- 测试任务 `jot-test`：FW-F13 要求运行示例；沿用现有整机测试驱动方式，只在测试程序创建。
- 示例集合 `jot.entries`：FW-F13 要求按成员保存，JSON 正文复用现有数据库格式。
- 示例恢复载荷 1 字节页码：FW-F13 要求恢复状态，复用现有应用状态文件。
- 无新产品任务、状态机、配置项或框架存储格式。

## 要改的文档

共享文档不改。建议合并主会话在 backlog 的 F13 条目关联本说明与报告。

## 建议

【实现】统一应用主题控件的公开头文件位置；当前示例复用 `system/ui/ink/ui_shell.h`。补状态／设置的退订接口前，页面不要反复注册持有控件指针的订阅。修正上述过时注释。均未改框架。

架构核查：页面只组装控件，业务持久化走数据服务，用户动作走能力软总线；AI 可读页面和集合、按同一能力写入，没有另开绕过策略的业务入口。

## 验证细节

【实测】四个临时示例变体均被相同验收拒绝（退出码 1）：`source` 篡改调用来源，`state` 停存恢复状态，`snapshot` 导出屏幕没有的值，`child` 撤掉孩子保存权限。命令：`"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/jot/run.py --build build --mutant <变体>`。原始文件不变，正常复跑退出 0。

【实测】调试中修正了新增测试的一条页面可用性断言，并移除测试启动包装的 `CHECK` 诊断缓冲：它让主任务宿主栈估算到 4224 字节，超出原 4096 预算；改为返回启动错误后定向测试通过，没有提高栈或预算。首轮全量还有三项缺少家庭服务器测试依赖而跳过，后续复跑使用依赖目录中现成的锁定虚拟环境，并以 `PYTHONPATH="$PWD/hub"` 确认测试当前检出源码。

复现：配置参数沿总规则 §5，两套均额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；内存目录加 `-DINK_SANITIZE=ON`。构建用 `taskpolicy -c utility cmake --build <构建目录> -j2`。完整测试命令如下（Linux 去掉 `taskpolicy -c utility`）：

```sh
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

本机日志在 `work/fw-f13/`（不提交）；正常示例保留证据为 `build/jot-_qqvhzvi/`，含两次进程的指标、日志和模拟闪存。报告不把宿主栈估算或模拟器资源数字当成实机测量。

【实测】最终范围检查仅 9 个相关文件：CMake 测试接线、说明、报告、示例和验收脚本；`system/`、`apps/`、`platform/`、`drivers/`、`boards/`、`ports/`、`simulator/` 及任务列出的共享文档均无修改。`git diff --check` 通过；按总规则对基线到 HEAD 的差异执行隐私扫描，无匹配（grep 退出 1）。构建产物、日志和临时变体均未提交。
