# memo 复查修复报告

日期：2026-11-09。任务基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`；本轮从 `96434807` 接续，起始工作区干净，分支 `claude/codex-memo`。原[复查记录](REVIEW.md)保留不改；本报告更新原来的验收结论。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1（高） | 【实测】已修 | 失败提示留在原编辑器，焦点、控件和草稿编号不变；继续输入后离开重开全文保留；重试后新字在离页前自动落盘。两阶段 34／41 项断言通过 | 定向命令；可单跑 `--phase draft_loss`、`--phase retry_edit` |
| R2（中） | 【实测】已修 | 在 `memo.save` 一处按完整行重排勾选位；删除前行后 beta 仍勾选，插入新前行后 beta 跟随移动，新项未勾选。39 项断言通过 | `tests/memo/run.py --build build --phase check_edit` |
| R3（中） | 【实测】已修 | 超过 24 字的清单项显示省略号和中文说明；快照明确 `content_truncated=true`、`read_full=memo.edit`；触摸编辑入口可读完整尾部。28 项断言通过，四行中文另有 21 项通过 | `--phase long_line`、`--phase full_snapshot` |
| R4（中） | 【实测】已修 | 统一销毁入口用已保留的草稿引用补交最终 `memo.save`，不依赖失焦后归零的引用；同批输入并完成后记录为 `BURST_FINAL`。25 项断言通过 | `--phase burst` |
| 1 新建、编辑、撤销、清单、重启 | 【实测】通过 | 原写入／重启 142／22 项通过，1,800 字节正文和孩子私有数据重启保留；清单组合编辑和同批完成由 R2／R4 补齐 | 定向命令 |
| 2 保存失败内容不丢 | 【实测】通过 | 原失败阶段 31 项通过，中文提示位于正文和键盘上方；R1 覆盖继续输入、离开恢复、重试后继续编辑 | 定向命令 |
| 3 成员隔离 | 【实测】通过，反例有效 | 成人、孩子集合独立；固定到成人集合的反例检出读取及删除两项失败，正常执行到结束 | `--mutant member` |
| 4 AI 快照与同源能力 | 【实测／实现】通过 | 六个视图均有快照并装入真实 3,072 字节上下文；失败视图与长项读取入口均明确；按钮和 AI 仍调用相同的十项 `memo.*` | 定向命令、全量 `integration-ai-surface` |
| 5 能力表、全量、内存、规模 | 【实测】文档同步未完成，其余通过 | 普通与 ASan／UBSan 全量均 224/225，只失败共享能力文档；无内存诊断。运行时 223 项能力、16 项设置与生成预览一致；产品累计 320 行，未超过 350 | 完整验证命令、能力预览命令 |

原复查无严重项；其 1 项高和 3 项中均已修复。普通定向与 ASan／UBSan 定向的九阶段均通过，完整内存检查无诊断；共享文档仍需合并主会话同步。

## 没做到的

- 全量没有全绿：两套唯一失败均为 `capability-table-doc`，正式能力文档未纳入备忘能力及新行号。总规则第 7 节禁止本任务改共享文档，需合并主会话生成；未跳过该测试、改检查器或放宽断言。
- 没有 Linux／gcc 或实机运行证据；本任务禁止碰设备。录音 `MemoRecord`、离线待整理 `MemoPending` 按原任务排除。
- 原有上限不变：正文 4,095 字节、清单 32 行、200 条记录（含可撤销删除数据）。勾选按完整行文字及同名行的出现顺序匹配；改成不同文字视为新项，未新增条目编号或独立存储框架。

## 和设计不一致的

【实测】已看失败页、清单原稿及最终实际截图，重新生成六页对照。分数仍全部超过 12/255，不标视觉通过。统一 F4 卡片、现行放大字号、加深灰色及已有长文键盘与旧稿有差异；保留原报告披露的无录音播放器、无 AI 提取事项、未接“腾出空间”文件入口等边界，本轮不扩展功能。

| 页面 | 1/4 灰度平均差（/255） |
| --- | ---: |
| Memo | 46.208 |
| MemoDetail | 28.821 |
| MemoEdit | 32.268 |
| MemoChecklist | 25.131 |
| MemoUndo | 55.471 |
| MemoSaveFail | 35.804 |

失败提示现在直接呈现在 `MemoEdit` 中，不再压入另一活动；快照用 `view=MemoSaveFail` 表达失败视图，动作页和草稿仍归 `MemoEdit`，不修改框架的页面归属检查。提示、重试和键盘都保留；失败提示仍会换行，未遮挡正文。证据：`build/memo-compare/scores.json`、`build/memo-compare/MemoSaveFail/snap-scene.png`。

## 改过的已有测试

- 本轮 `tests/memo/memo.c`：失败断言从“跳入另一活动”改为“原编辑器内的 FAIL 视图”，并额外要求快照明确 `MemoSaveFail`、保留原焦点和草稿编号；AI 保存参数使用实际草稿页面，原有正文、提示位置、落盘断言保留。
- 本轮 `tests/memo/run.py`：新增六个复查阶段、五个反向变体和单场景运行入口；新阶段各用独立镜像，写入／重启继续共用镜像。六个正式视图仍逐个强制核对，新快照场景标签单独处理；未删除原断言、提高预算或加 sleep。
- 上次已改的 `tests/integration/apps.c`（应用数 7 → 8）和 `tests/integration/ai_surface.c`（追加十项能力）本轮不再修改；原因仍是原任务注册了备忘应用。

## 要用户定的

无；未新增产品范围或配置决定。

## 顺手发现

无需要扩展修复的其它问题。共享能力文档过期是已披露的合并事项。

## 新增的机制

- 本轮无新增任务、线程、状态机、队列、缓存、后台重试、配置项或持久文件格式；复用原来的 EDIT／FAIL 状态、长文草稿、数据服务与能力执行器，未新增服务接口。
- 原任务要求的成员集合 `memo.notes`（schema 1）和删除标记撤销继续沿用，未另建清单项存储。

## 要改的文档

- 合并主会话重新生成 `docs/system/capabilities.md`，纳入十项 `memo.*` 与实际源码位置；本任务只生成忽略目录中的预览。
- 合并主会话更新 `docs/system/ui-dependencies.md`、`docs/tasks/backlog.md` 的备忘状态；本轮不改这些共享文件或需求、想法、决策记录。

## 反向对照

【实测】修复前新增的五个问题场景各失败 1 项，四行中文快照场景通过；日志 `build/memo-repair-before.log`。下面九组均在忽略目录内生成替代对象，编译、链接成功后模拟器与运行器退出 1，均有 `test.finished=1`；没有以编译失败冒充反例。源文件哈希前后相同，未改工作区产品源码。

| 变体 | 临时撤掉什么 | 失败断言数 |
| --- | --- | ---: |
| draft-session | 失败改回压入另一活动，重建编辑器 | 2 |
| resume | 重试成功后恢复编辑状态 | 1 |
| checks | 保存时重排勾选位 | 1 |
| long-item | 长清单项的省略号 | 1 |
| final-save | 统一退页时补交保存 | 1 |
| member | 两处成员集合改为固定成人 | 2 |
| write | 跳过真正的备忘写入 | 25 |
| failure | 不呈现失败视图 | 5 |
| undo | 撤销改回按最大编号选择 | 2 |

命令：`tests/memo/run.py --build build --mutant <变体>`，使用下节相同 Python 和 `taskpolicy`。汇总：`build/memo-repair-reverses.json`，逐项日志 `build/memo-repair-reverse-<变体>.log`。

## 规模与架构审视

- 【实测】本轮仅改一份产品文件：新增 52、删除 20 行；相对任务基线累计产品 320 行（应用 299、原共享接线及接口 17、CMake 4），低于 350。产品超过 100 行来自原任务要求的六个视图、十项能力、成员数据与草稿读写；更小的“再建失败草稿”流程已删除，未采用第二份草稿合同或长正文能力参数。
- 【实现】清单重排在后台 `memo.save` 一处完成；统一退页继续走同一能力，UI 不直接改服务内部状态；成员集合和 AI 可读策略未改，LVGL 只在 UI 路径使用。没有需要新增子系统的依据。
- 里程碑：`88ce9c0c` 记录接续范围；`826a5033` 提交修复和定向测试。本轮只作本地提交，不推送、不建 PR、不动设备或调用付费服务。

## 完整验证命令与结果

命令在仓库根目录执行，`INK_DEPS`、`INK_DESIGN` 使用用户指定值。两份既有配置已核对：字体、Python、LVGL／SDL2／ImGui、中枢 Python 一致，均 `INK_WERROR=ON`；内存检查另启用 `INK_SANITIZE=ON`，对象实际使用 `-fsanitize=address,undefined`。未安装或升级依赖；构建与全量串行，最多 `-j2`，没有启动窗口版。

```sh
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/memo/run.py --build build --visual
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/memo/run.py --build build-asan
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/memo-repair-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/memo-compare --scenes Memo MemoDetail MemoEdit MemoChecklist MemoUndo MemoSaveFail --reuse
```

- 【实测】普通构建退出 0，普通全量退出 8：224/225、零跳过，151.10 秒，唯一失败 `capability-table-doc`；日志 `build/memo-repair-build.log`、`build/memo-repair-ctest.log`。
- 【实测】ASan／UBSan 构建及定向退出 0；全量退出 8：224/225、零跳过，295.91 秒，同样只失败 `capability-table-doc`，无 AddressSanitizer 或未定义行为诊断。日志 `build-asan/memo-repair-build.log`、`build-asan/memo-repair-focused.log`、`build-asan/memo-repair-ctest.log`。
- 【实测】两套定向九阶段均为 142／22／31／34／41／39／28／25／21 项通过；普通另产出六视图截图。所有阶段零告警、零预算违规，内部 RAM 最低余量均 47,840 字节，门槛仍为 32,768 字节。证据 `build/memo-visf3m3y/`、`build-asan/memo-921fxj61/`，汇总 `build/memo-repair-summary.json`。
- 【实测】能力预览与运行时一致：223 项能力、16 项设置，共 284 项定义；视觉命令退出 0 仅表示产出有效对照，分数仍未达标。

## 结束检查

- 【实测】`git diff --check`、报告结构、本地链接和证据路径检查通过；`git diff --stat` 确认本轮仅四个相关文件。原复查报告、受保护共享文档、历史调研／审计和其它任务文件均未改。
- 【实测】任务基线是当前 HEAD 的祖先；对基线至 HEAD 的完整差异及待提交报告分别扫描，本机绝对路径和 MAC 模式均零匹配，两次 grep 均退出 1。最终九组反向对照再次通过预期失败检查，源码哈希与当前文件一致。
- 只提交产品、测试和中文报告；构建产物、日志、截图、反例与设计稿均留在忽略目录，未提交。核对结果：`build/memo-repair-end-check.json`。
