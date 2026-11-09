# memo 任务报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。分支：`claude/codex-memo`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 新建、编辑、删除、撤销、清单触摸勾选与 AI 勾选；另一进程重启核查；1,800 字节正文完整保存 | `tests/memo/run.py --build build --visual` |
| 2 | 【实测】通过 | 注入数据服务拒绝写入，进入真实失败页，正文和长文草稿保留；提示位于编辑区和键盘上方；触摸重试及 AI 保存都落盘 | 同上 |
| 3 | 【实测】通过；反例有效 | 成人／孩子各自存储；故意把两处成员集合打开固定到成人后，空列表及禁止删除断言失败，执行完成标记仍出现 | `tests/memo/run.py --build build --mutant member` |
| 4 | 【实测／实现】通过 | 六页 JSON 快照均放入实际 3,072 字节 AI 上下文，包含真实失败说明；触摸与 AI 调用同一组 `memo.*` 能力；已有 AI surface 全量检查通过 | 定向命令、`integration-ai-surface` |
| 5 | 【实测】两套全量均 224/225；共享文档待同步 | 运行时能力目录一致；普通及 ASan／UBSan 唯一失败均为受保护的生成文档；无内存诊断；产品 288 行 | 完整验证命令 |

## 没做到的

- 普通及 ASan／UBSan 全量均 224/225，没有全绿：唯一失败是总规则第 7 节禁止本任务更新的共享能力文档，留合并主会话生成；未跳过该项或修改检查器。
- 没有实机或 Linux／gcc 运行证据。按任务排除录音页 `MemoRecord` 和离线待整理页 `MemoPending`。
- 上限沿用长文编辑的 4,095 字节；清单最多 32 行，最多 200 条记录（含可撤销删除的数据），集合沿用数据服务默认的 256 KiB 字节上限。没有新增清空历史、独立清单项删除按钮或跨应用提醒／短信建议。

## 和设计不一致的

- 已查看六张原画板和最终实际截图。使用现有主题、F4 与长文键盘，未修改设计。六页均超过 12/255，不能标为视觉通过。
- 列表使用统一分页卡片；详情显示真实文字及编辑／删除／转清单。画板里的录音播放器和 AI 提取事项没有实现，本任务没有录音数据或相应建议服务。
- `MemoEdit` 按规范没有保存按钮；保留长文编辑器及其固定键盘。`MemoSaveFail` 保留编辑器并提供明确重试，未接入“腾出空间”的文件页面。

| 页面 | 1/4 灰度平均差（/255） |
| --- | ---: |
| Memo | 46.208 |
| MemoDetail | 28.821 |
| MemoEdit | 32.268 |
| MemoChecklist | 25.131 |
| MemoUndo | 55.471 |
| MemoSaveFail | 35.612 |

证据：`build/memo-compare/scores.json` 和同目录对照拼图。列表是合成文字记录，没有画板的录音卡及内容密度；清单是 F4 卡片与分页，保留编辑入口，单项添加／删除通过长文编辑逐行完成；时间、电量、当前字号、灰度和键盘采用真实模拟器与现行主题，没有为接近旧画板改变规格。

## 改过的已有测试

- `tests/integration/ai_surface.c` 只在 `FOR_AI` 表末追加十项能力及必填参数，未删除或放宽原有断言。
- `tests/integration/apps.c` 的精确应用数从 7 改为 8，描述补上本任务新增的 `memo`；首轮实际值为 8，仅此断言失败。其余生命周期、权限和资源断言不变，不是提高资源预算。

## 要用户定的

无。

## 顺手发现

- 【实测】首次全量的两项 Apps 快照回归失败：注册备忘后，它同时出现在固定的“记和联系”分组和动态工具列表，挤动了第二页。为解除全量阻塞，在共享 `navigation.c` 的既有电话排除项后只追加一行备忘排除；没有修改原快照断言。这是共用文件尾部追加规则的一处必要例外；首轮两项失败即为撤掉该行时的反向证据。

## 新增的机制

- 任务要求的成员备忘集合 `memo.notes`，JSON schema 1：正文、清单标志、勾选位、可撤销删除标志和已有草稿引用。没有新建持久文件格式。
- 任务要求的删除撤销：同一记录保留内容并标记删除，撤销恢复原编号；复用数据服务落盘。
- 无新增产品任务、线程、状态机、队列、后台重试、配置项或通用框架。

## 要改的文档

- 合并主会话重新生成 `docs/system/capabilities.md`，纳入十项 `memo.*` 能力；本任务只生成忽略目录中的预览。
- 由合并主会话更新 `docs/system/ui-dependencies.md` 和 `docs/tasks/backlog.md` 的备忘状态；按总规则第 7 节，本任务不改共享文档。

## 里程碑与规模

- 开工时工作区干净，HEAD 与给定基线一致；已先提交报告。
- 【实现】新增页面和能力均在 `system/apps/memo/`；共用 CMake、启动函数、文本头文件与文本服务只在末尾追加接线或接口。唯一服务增量 `ink_text_draft_read` 按成员和页面读取已有草稿，后台无需访问 LVGL，也无需把长正文塞进 384 字节能力参数表。
- 超过 100 行的原因：六页、十个原子能力、成员数据读写、草稿接线和快照均为任务要求。看过的更小方案：仅保存编辑框内容无法提供可靠失败回执；只传正文会撞上能力参数 384 字节限制；应用复制长文草稿格式会形成第二份合同。最终复用现有数据服务、长文服务、F1／F4 和两个既有执行器。
- 【实测】产品保守计数 288 行：应用 267、共用启动／接口／实现／列表去重 17、CMake 4；新增和删除都计，不计测试及文档。未超过 350。
- 【实测】最终定向写入 142、重启 22、失败处理 31 项断言通过，零告警、零预算违规；内部 RAM 总余量最低 47,840 字节，原门槛 32,768 字节未改。六页视觉证据来自独立、仅截一次已稳定帧的真实运行，不改写指标。证据：`build/memo-xwc6zl68/`；日志：`build/memo-final-functional.log`。

## 验证命令

命令均在仓库根目录执行，设置用户给定的 `INK_DEPS`、`INK_DESIGN`。构建与全量测试串行，构建及 CTest 均不超过 `-j2`，均以 `taskpolicy -c utility` 启动；不启动窗口版。

```sh
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/memo/run.py --build build --visual
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/memo-compare --scenes Memo MemoDetail MemoEdit MemoChecklist MemoUndo MemoSaveFail --reuse
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

配置按总规则第 5 节，启用 `INK_WERROR=ON`，字体、Python 与 LVGL 来自只读依赖目录。另指定现有 SDL2／ImGui 源码镜像及依赖齐全的 `INK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`，没有安装或升级依赖。

ASan 配置只改构建目录为 `build-asan` 并加 `INK_SANITIZE=ON`。已实查两份 CMake cache 的字体、依赖、Python 和 Werror 参数一致，且系统对象实际使用 `-fsanitize=address,undefined`。

## 反向对照

- `member`：两处数据库打开故意固定到成人集合；失败 2 项，均是实际跨成员读取／删除，`test.finished=1`。最初尝试把集合改为设备级，会先被缓存接口拒绝，无法抵达成员场景；该尝试不作为隔离证据。
- `failure`：仅撤掉保存失败后进入失败页的分支；最终失败 6 项，覆盖真实页面、提示位置、AI 状态、可见重试及原内容保存，`test.finished=1`。
- `write`：保存时跳过真正的 `ink_db_put`；编译／链接成功，25 项断言失败，`test.finished=1`。最初直接替换成常量使测试注入函数变为未使用，触发 `-Werror`；该编译失败不算反向证据，已修正临时变体并成功执行。
- `undo`：撤销目标改回按最大编号而非最新记录版本选择；两项断言失败，其中一项明确要求最后删除的较小编号，`test.finished=1`。
- 反例只在忽略的构建目录生成临时源码、对象及可执行文件，不改工作区产品代码；未删除、放宽断言或加 sleep 掩盖问题。
- 命令：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/memo/run.py --build build --mutant <member|write|failure|undo>`。各变体编译、链接成功，模拟器退出 1，反向测试退出 1；源码哈希前后相同。日志：`build/memo-reverse-<名称>.log`；最终证据目录依次 `build/memo-6qchsizq/`、`build/memo-j_e93w2o/`、`build/memo-awebaaw8/`、`build/memo-gbnmmb0g/`。

## 完整验证结果

- 【实测】普通构建退出 0；最终全量 CTest 退出 8，224/225、零跳过，285.86 秒。唯一失败 `capability-table-doc`：新增十项能力及源码行号使受保护的生成文档过期，没有修改检查器或跳过该项。日志：`build/memo-final-build.log`、`build/memo-final-ctest.log`。
- 【实测】运行时目录与代码生成预览完全一致：223 项能力、16 项设置；预览含 284 项能力／状态／设置定义。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/memo-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json`，退出 0。
- 【实测】首轮全量 221/225 的三个功能失败均已处理：两个重复工具快照由产品去重修复；应用数量精确断言按新增备忘更新。四项相关检查（含 AI surface）定向和最终全量均通过，未改它们的资源门槛。首轮证据：`build/memo-first-ctest.log`；复查：`build/memo-existing-checks.log`。
- 【实测】ASan／UBSan 配置和构建退出 0；全量 CTest 退出 8，224/225、零跳过，330.41 秒，同样仅 `capability-table-doc` 失败。无 AddressSanitizer 或未定义行为诊断。日志：`build-asan/memo-config.log`、`build-asan/memo-build.log`、`build-asan/memo-ctest.log`。
- 【实测】普通及内存检查中的备忘三阶段均为 142／22／31 项断言，零告警、零预算违规，内部 RAM 总余量最低均 47,840 字节。最终证据：`build/memo-oskz1rxz/`、`build-asan/memo-nacf_vbm/`；汇总：`build/memo-final-summary.json`。没有抬高原来的 32,768 字节预算门槛。

## 结束检查

- 【实测】`git diff --check`、报告结构与本地链接检查通过；相对基线共 12 个相关文件，产品 288 行。受保护的共享文档、历史调研／审计资料及其它并行任务文件没有差异。
- 【实测】按总规则第 6 节对基线至 HEAD 的完整差异扫描，本机路径及 MAC 模式零匹配，grep 退出 1；本次待提交报告同样零匹配。构建产物、截图、临时反例、设计稿均未提交。
- 只在本地分支提交；未推送、未开 PR、未操作设备、未访问真实通信或付费接口。
