# ai-chat 执行报告

基线：`d0cacd1d4b348a8e885904026f3796a10996c646`。首次开发的记录如下；2026-11-09 的复查修复以末节“复查后的修复”为准。仅本地提交，未推送。

[页面实现](../../../system/ui/shell/ai_chat.c)、[场景测试](../../../tests/ai_chat.c)、[快照校验](../../../tests/ai_chat_snapshots.py)。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】流程通过，长回答已展开；像素目标未达 | 两套构建均 105 项断言；文字、语音、等待、四页长回答、实体键打断、两种停止及记录入口。五页及等待页截图位于两套 `test-out/ai-chat/` | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ai-chat$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 五页 F1 JSON 校验；触摸、实体翻页键和 AI 调用同一分页能力；语音统一键能力；旧版本发送拒绝、当前版本发送成功 | `ctest --test-dir build -R '^ai-chat.*$' --output-on-failure -j2`，均加 `taskpolicy -c utility` |
| 3 | 【实测】通过，含反例 | 取消原有 `turn_allowed()` 身份检查：构建退出 0，场景退出 1，6 项隔离断言失败，`test.finished=1`；已恢复并构建 | 见“反向对照” |
| 4 | 【实测】除用户指定的共享文档检查外通过：普通与 ASan／UBSan 全量均 218/219；234 行产品代码 | 唯一失败均为 `capability-table-doc`，按用户补充留给合并主会话。其余无失败、无跳过；无内存检查报错，预算未放宽 | 见“完整验证命令”和“复查后的修复” |

## 没做到的

- 【实现】AI 服务没有历史对话列表接口，也没有重播已有回答的接口；首页显示真实空态，未伪造记录或补改服务协议。没有实现找回旧对话和“听解释”重播；语音轮次的实际 TTS 展示与停止已接通。
- Linux／gcc 和真机尚未运行；本次证据为 macOS／clang 模拟器。
- 【实现】AI 服务原有回答缓冲最多接收 2047 字节，本任务未扩大；超过该缓冲而被服务丢弃的文字，界面无法补回。这里验证的是实际收到的长回答分页。

## 和设计不一致的

- 首页为空态；缺历史列表及答案重播接口，画板里的历史消息、收据和“听解释”没有假做。收据不属于本任务范围。
- 提问框使用已有短输入宿主，上限 383 字节，与 AI 服务接收上限一致；未依赖并行的 `text-long`。问题框比画板矮；键盘、字号、灰色遵循现行主题，原图尚是旧规格。
- 三个面板截图下方是真实提问页，画板背景是读书页；使用合成测试问题与回答，内容和长回答页数不同。长回答扩高已在复查后修复；朗读没有重播按钮。
- 【实测】对照未达到 12/255：`AiHome` 31.105、`AiAsk` 34.627、`AiListenTap` 34.685、`AiLong` 39.469、`AiSpeak` 38.533。对照图、分数及验证过的截图在 `build/ai-chat-compare/`；没有为分数改主题、换字体规格或伪造内容。

## 改过的已有测试

- `tests/integration/apps.c`：注册 AI 系统应用后，精确应用数从 6 改为 7。
- `tests/ui_shell_regressions.c`：页面文字改为 F1 JSON 后，逐字节比较其 JSON 转义串，仍检查完整可见回答及页码；原先直接寻找带换行的裸字符串不再适用。
- 同文件的 Apps 精确内容断言：注册 AI 后“对话记录”出现在工具第一页，相机移到第二页；相应两条断言跟随真实排序，仍检查各页包含和排除的项目。
- `tests/integration/ai_surface.c`：在原有穷举表末尾追加五个新能力，未删旧项。
- `tests/integration/confirm.c`：新增能力使完整目录超过原 32 KiB 缓冲；保持缓冲大小，改用已有分页接口，逐页确认人类确认操作未暴露，并确认查询／撤回仍可见。没有改资源预算。

## 要用户定的

无。

## 顺手发现

无。

## 新增的机制

- 任务要求的 AI 页面、F1 声明与五个界面能力；复用已有输入和文本分页。产品没有新增任务、线程、持久格式、缓存、队列、重试或配置项；焦点请求使用现有 UI 执行队列。
- 新增最小服务接口 `ink_ai_stop_speech()`：停止当前轮次播放并丢弃后续帧，保留回答文字；未改协议和轮次推进。
- 测试可执行文件新增一个 PSRAM 栈的场景驱动任务，用来完成任务书要求的模拟器操作；不进入产品。

## 要改的文档

- 合并主会话更新 `docs/tasks/backlog.md`、`docs/system/ui-dependencies.md` 的实际状态，并重新生成 `docs/system/capabilities.md`；按总规则第 7 节，本分支不改这些共享文件。

## 阶段记录

- 【实测】完整普通构建退出 0；初轮问题框在屏幕装载前请求焦点，F2 拒绝打开。已改用现有 UI 执行队列在装载后请求焦点，没有新增任务。
- 【实测】最终普通与 ASan／UBSan 场景均为 97 项断言通过、零失败、`test.finished=1`；内部 RAM 最低余量均为 34,512 字节，高于原下限 32,768 字节；内外部内存分配失败均为 0。
- 【实测】首轮普通全量 212/219；修正面板自动更新误改上下文版本、朗读覆盖错误提示，以及上述已有测试后，8 项定向回归全部通过。最终仅生成能力总表检查未过，按共享文档限制留给主会话更新。
- 【实测】新增“服务器发送完成、扬声器仍有语音排队”用例：旧停止接口有 1 项断言失败、92 项通过、`test.finished=1`，ASan CTest 退出 8。停止目标改用当前轮次的最后播放编号，避免发送结束清空传输编号后停不住；只改一行，没有扩展协议。反例日志及指标在 `work/ai-chat-queued-red.log`、`work/ai-chat-queued-red-metrics.json`。
- 【实测】新增“从提问页回对话记录”用例：仅恢复应用会重开旧提问页，旧实现有 1 项断言失败、96 项通过、`test.finished=1`。现在共用一个记录入口，用既有导航接口关闭该应用的旧页面再打开首页；不累计页面、不改服务轮次。修复后普通场景 97 项断言全部通过。反例日志与指标在 `work/ai-chat-home-red.log`、`work/ai-chat-home-red-metrics.json`。

## 反向对照

【实测】仅临时将 `system/services/ai/ai_service.c` 的 `turn_allowed()` 中 `if (!g_turn_revoked && (!identity.authed ||` 改为 `if (false && !g_turn_revoked && (!identity.authed ||`，测试不动。构建 `sim-ai-chat` 退出 0；用原场景、夹具、预算运行，输出改到 `build/test-out/ai-chat-negative/`，退出 1，6 项断言失败，测试继续运行到 `test.finished=1`。旧面板没有随成员关闭，画面仍有旧会话；快照的另一层归属保护拒绝输出。日志摘要在 `work/ai-chat-negative-summary.json`。已恢复源码并重新构建，未把临时改动提交。

替换上述条件后执行以下命令；完成后恢复条件并重建。没有用崩溃或超时冒充反例。

```bash
taskpolicy -c utility cmake --build build --target sim-ai-chat -j2
taskpolicy -c utility build/sim-ai-chat --profile simulator/profiles/metalio-plus.json --script tests/ai_chat.script --hub tests/ai_chat.hub.json --budget tests/ai_chat.budget.json --assets build/assets --content build/test-content --out build/test-out/ai-chat-negative --quiet --no-random-outages
```

## 规模与架构审视

- 【实测】产品保守计数 202 行新增＋17 行删除＝219 行，连 CMake 的 12 行测试接线也计入；低于 350 行。新页面在 `ai_chat.c`；共用 CMake 只在末尾接线。导航仅修改 AI 图标入口和 Dock 能力绑定；UI 输入仅替换 AI 键的集中转发，没有修改其它并行页面。
- 为什么不能小于 100 行：两页的主题控件、文字输入生命周期、五页 F1 声明、统一实体键和触摸／AI 入口，以及真实音频状态和停止接口缺一不可。只沿用旧面板不能提供对话入口和文字提问；手写纯文本快照更短，但不满足 F1。同一问题只在对应的公共入口修复。
- 服务仍拥有协议、轮次、成员检查及语音传输；页面只通过能力调用，不访问服务内部状态。朗读状态读既有系统状态；输入和结果放 PSRAM。面板自动更新不改变底页的上下文版本，显式答案翻页才更新版本，以保留已有 AI 工具的页码约束。
- 两个新活动在新文件中，三个面板状态复用现有面板；F1 快照的 `page` 保留底页身份，`board` 字段标明当前面板，符合现有上下文读取方式。

## 完整验证命令

两目录均使用总规则第 5 节的依赖参数，另指定已有的 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，使用仓库锁定的服务器测试依赖；`build-asan` 仅增加 `-DINK_SANITIZE=ON`。不并行运行构建或全量测试，没有启动带窗口的模拟器。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --out build/ai-chat-compare --reuse --scenes AiHome AiAsk AiListenTap AiLong AiSpeak
```

- 【实测】普通构建退出 0；最终全量 CTest 退出 8，218/219，耗时 172.20 秒；唯一失败为生成能力文档。确定性、分层、SDK、原 AI 工具与成员回归均通过。日志 `work/ai-chat-build-final.log`、`work/ai-chat-ctest-final.log`。
- 【实测】服务器测试依赖版本核对：pytest 8.4.2、pytest-asyncio 1.2.0、aiohttp 3.12.15、PyYAML 6.0.3，导入的是当前检出的 `hub/ink_hub`；未安装或升级依赖。
- 【实测】ASan／UBSan 构建退出 0；最终全量 CTest 退出 8，218/219，耗时 315.06 秒；唯一失败相同，无跳过、无 ASan／UBSan 报错。编译参数核对包含 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`。日志 `work/ai-chat-asan-build-final.log`、`work/ai-chat-asan-ctest-final.log`。

## 收尾检查

- 【实测】相对基线仅 18 个任务相关文件；共享文档和其它并行页面没有差异。报告的三个本地链接均存在，`git diff --check` 退出 0；产品、测试和 CMake 与已验证的代码提交一致。
- 【实测】按总规则第 6 节扫描基线到最终差异，本机绝对路径与 MAC 模式零匹配（grep 退出 1）。构建产物、设计稿和本地测试日志均未提交。

## 复查后的修复

2026-11-09，接续复查提交 `fe574e18`。复查列出的严重 0、高 0、中 2，两个中级均已修复；没有单列的低级问题。

| 问题 | 结论与改动 | 测试证据 |
| --- | --- | --- |
| AC-R1-01 | 【实现】仅给 `shell.ai_send` 声明所属页面 `AiAsk`，复用能力关口的页面版本校验 | 【实测】编辑后携带旧版本的 AI／远程发送均返回 `INK_RC_STALE`，保留问题和键盘；当前版本 AI 发送与原键盘提交均到达真实 AI 服务 |
| AC-R1-02 | 【实现】沿用原面板和分页函数，长回答增加两行正文及主题间距；默认面板从 519 增至 657／1216，约 54.0%，正文五行，短回答恢复原高度 | 【实测】两种模拟器档案检查展开比例、正文高度、来源和按钮不重叠、完整翻页逐字节一致、末页边界、返回上一页、流式回答变短及四位页码；真实对话场景遍历四页，触摸、实体键、AI 均走分页能力 |

【实测】普通定向回归 4／4，`ai-chat` 为 105 项断言，两个 `ui-answer-regressions-*` 各 103 项断言，均零失败、`test.finished=1`；五页 F1 校验通过。命令：

```bash
taskpolicy -c utility cmake --build build --target sim-ai-chat sim-ui-regressions -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build \
  -R '^(ai-chat.*|ui-answer-regressions-.*)$' --output-on-failure -j2
```

**反向对照【实测】：** 保持最终测试、夹具和预算不变，分别临时恢复复查提交中的一个产品文件，构建目标后运行原场景；每次结束均恢复修复。没有改断言、放宽预算或增加等待。

| 撤回的修复 | 构建／运行退出码 | 断言结果 |
| --- | --- | --- |
| 仅恢复旧 `ai_chat.c` | 0／1 | 101 通过、4 失败：两种来源的旧版本发送、保留编辑状态及后续键盘提交；`test.finished=1` |
| 仅恢复旧 `ai_panel.c` | 0／1 | 159 通过、4 失败：两份长回答的展开比例和五行正文高度；`test.finished=1` |

反例日志及摘要：`work/ai-chat-fix-negative-*.log`、`work/ai-chat-fix-negative-summary.json`；指标分别在 `build/test-out/ai-chat-negative-stale-send/`、`build/test-out/ai-chat-negative-long-layout/`。场景命令沿用原 `ai-chat` 和 `ui-answer-regressions-metalio-plus` 参数，只改输出目录。

**改过的已有测试：** `tests/ai_chat.c` 增加版本正反例，把页数要求从至少三页加强到至少四页，确保三种翻页来源都实际执行。合成回答从八段增为十六段，分成两个低于既有 512 字节接收上限的增量；未扩大服务缓冲。`tests/ui_shell_regressions.c` 的两份五行夹具改为八行，继续验证多页；四位页码夹具改为连续换行，在同一个 4096 字节上限内继续产生四位页码。原有逐字节完整性、适配、边界和页码断言均保留。

**视觉核对【实测】：** 重新生成五页截图和 `build/ai-chat-compare/` 对照，分数依次为 AiHome 31.105、AiAsk 34.627、AiListenTap 34.951、AiLong 39.741、AiSpeak 38.567／255，仍超过 12。已查看长回答干净帧：五行正文、来源及翻页按钮均完整，面板已展开；底页、合成内容、现行主题和历史／重播接口缺口仍与画板不同，不把像素分数写成通过。

**新增的机制：** 无。发送仅补能力元数据，布局仅调整既有控件，不增加状态、任务、配置或通用框架；协议和轮次逻辑未变。按钮、实体键与 AI 继续经过同一能力关口，五页快照沿用 F1。

**规模【实测】：** 本轮产品差异仅两文件，15 行新增、2 行删除；相对原任务基线保守计入 CMake 后为 216 行新增＋18 行删除＝234 行，低于 350 行。各问题均少于 30 行产品改动。

**全量验证【实测】：** 使用前节完整验证命令和指定依赖重新配置，两目录均为 `INK_WERROR=ON`，内存配置另有 `INK_SANITIZE=ON`；编译参数确含 `-fsanitize=address,undefined -fno-omit-frame-pointer`。逐轮运行，构建与 CTest 均最多 `-j2`、均加 `taskpolicy -c utility`。

| 检查 | 结果 | 日志 |
| --- | --- | --- |
| 普通构建／最终全量 | 构建 0；CTest 8，218／219、238.51 秒、零跳过 | `work/ai-chat-fix-build-all.log`、`work/ai-chat-fix-ctest-final.log` |
| ASan／UBSan 构建／最终全量 | 构建 0；CTest 8，218／219、325.81 秒、零跳过，无内存检查报错 | `work/ai-chat-fix-asan-build.log`、`work/ai-chat-fix-asan-ctest-final.log` |
| 两套场景及预算 | `ai-chat` 均 105 通过；两种档案的回答回归均 103 通过；零失败、`test.finished=1`。`ai-chat` 的内部 RAM 最低余量均 34,512 字节，高于原 32,768 下限；内部／外部分配失败均为 0 | 两套 `test-out/ai-chat/metrics.json` 和 `test-out/ui-answer-*/metrics.json` |

首轮普通全量另外发现 `ui-scale` 不能追踪函数传入的尺寸，已把主题常量计算放回布局函数内部；最终原尺寸检查及其回归均通过，没有加豁免或改检查器。

两轮唯一失败均是此前的 `capability-table-doc`：已注册能力 202 项，生成文档仍为 197 项，按用户补充和总规则第 7 节留给合并主会话重新生成；没有删除或跳过检查。可能因依赖跳过的 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 本轮两套全量均实际运行通过。依赖齐备，未安装或升级：pytest 8.4.2、pytest-asyncio 1.2.0、aiohttp 3.12.15、PyYAML 6.0.3。

**收尾【实测】：** 本轮只改两个产品文件、三个测试文件和本报告，共六个相关文件；共享文档、预算和其它页面未改。`git diff --check` 通过，报告三个本地链接均存在；按总规则扫描原任务基线到最终差异，本机绝对路径和 MAC 模式零匹配。构建产物、截图、设计稿及本地日志未提交；未推送。
