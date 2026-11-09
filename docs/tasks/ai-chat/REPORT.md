# ai-chat 执行报告

基线：`d0cacd1d4b348a8e885904026f3796a10996c646`。状态：内存检查进行中。实现提交：`b6567320`；实体键播放中打断场景：`80938251`。仅本地提交，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】场景通过，视觉待评分 | 五页及等待页截图位于 `build/test-out/ai-chat/` | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ai-chat$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 五页 F1 JSON 校验；触摸、实体翻页键和 AI 调用同一分页能力；语音统一键能力 | `ctest --test-dir build -R '^ai-chat.*$' --output-on-failure -j2`，均加 `taskpolicy -c utility` |
| 3 | 【实测】通过，含反例 | 取消原有 `turn_allowed()` 身份检查：构建退出 0，场景退出 1，6 项隔离断言失败，`test.finished=1`；已恢复并构建 | 见“反向对照” |
| 4 | 【实测】普通全量 218/219，内存检查待完成；219 行产品代码 | 唯一失败是 `capability-table-doc`：按总规则第 7 节，本分支不能改生成的共享能力文档。其它测试无失败、无跳过 | 见“完整验证命令” |

## 没做到的

- 【实现】AI 服务没有历史对话列表接口，也没有重播已有回答的接口；首页显示真实空态，未伪造记录或补改服务协议。没有实现找回旧对话和“听解释”重播；语音轮次的实际 TTS 展示与停止已接通。
- Linux／gcc 和真机尚未运行；本次证据为 macOS／clang 模拟器。
- 【实现】AI 服务原有回答缓冲最多接收 2047 字节，本任务未扩大；超过该缓冲而被服务丢弃的文字，界面无法补回。这里验证的是实际收到的长回答分页。

## 和设计不一致的

- 首页为空态；缺历史列表及答案重播接口，画板里的历史消息、收据和“听解释”没有假做。收据不属于本任务范围。
- 提问框使用已有短输入宿主，上限 383 字节，与 AI 服务接收上限一致；未依赖并行的 `text-long`。问题框比画板矮；键盘、字号、灰色遵循现行主题，原图尚是旧规格。
- 三个面板截图下方是真实提问页，画板背景是读书页；使用合成测试问题与回答，内容和长回答页数不同。面板沿用现有高度，长回答没有扩高到画板的约 54%；朗读没有重播按钮。
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
- 【实测】五页场景通过；内部 RAM 最低余量 34,512 字节，高于原下限 32,768 字节；内外部内存分配失败均为 0。全量与内存检查进行中。
- 【实测】首轮普通全量 212/219；修正面板自动更新误改上下文版本、朗读覆盖错误提示，以及上述已有测试后，8 项定向回归全部通过。生成的能力总表按共享文档限制留给主会话更新，最终全量结果待补。
- 【实测】新增“服务器发送完成、扬声器仍有语音排队”用例：旧停止接口有 1 项断言失败、92 项通过、`test.finished=1`，ASan CTest 退出 8。停止目标改用当前轮次的最后播放编号，避免发送结束清空传输编号后停不住；只改一行，没有扩展协议。反例日志及指标在 `work/ai-chat-queued-red.log`、`work/ai-chat-queued-red-metrics.json`。

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

- 【实测】普通构建退出 0；全量 CTest 退出 8，218/219，耗时 249.91 秒；唯一失败为被禁止本分支更新的生成能力文档。确定性、分层、SDK、原 AI 工具与成员回归均通过。日志 `work/ai-chat-ctest-final.log`。
- 【实测】服务器测试依赖版本核对：pytest 8.4.2、pytest-asyncio 1.2.0、aiohttp 3.12.15、PyYAML 6.0.3，导入的是当前检出的 `hub/ink_hub`；未安装或升级依赖。
- ASan／UBSan 结果待补。
