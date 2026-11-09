# ai-chat 第 1 轮复查

日期：2026-11-09。基线：`d0cacd1d4b348a8e885904026f3796a10996c646`；复查版本：`a04a8227d8d8f75d37e918b2e2544347ab94ac2e`。

**结论：未达到全部验收。严重 0、高 0、中 2。** 新确认一项发送请求的页面版本检查缺失；另一项为开发报告已披露的长回答布局缺口。成员隔离、排队语音停止和记录入口的反例均有效。普通及 ASan／UBSan 全量各 218/219，唯一失败都是共享能力文档未更新，没有内存检查报错。

本轮独立读代码、跑命令、看 10 屏对照。依赖使用指定的 `$INK_DEPS`、`$INK_DESIGN`，证据限于 macOS／clang 模拟器。仅新增本报告；产品源码、已有测试和预算未改，未提交。诊断对象文件、独立可执行文件、日志和截图均在忽略的构建目录。

## 验收复核

| 验收项 | 独立结论 | 证据 |
| --- | --- | --- |
| 1：首页、文字／语音提问、等待、长回答翻页、说话中打断及截图 | 【实测】流程通过；长回答布局未完成，见 AC-R1-02 | 普通及内存检查的 `ai-chat` 均 97 项断言通过、零失败、`test.finished=1`。六个状态截图在 `build/test-out/ai-chat/`，含额外等待态 `AiWait`。 |
| 2：五页 AI 快照；按钮、实体键、AI 共用能力 | 【实测＋实现】五份 F1 快照、分页及语音能力接线通过；发现过期发送请求仍执行，见 AC-R1-01 | `ai-chat-snapshots` 校验五页 JSON；场景检查触摸／实体键／AI 的分页计数、真实采集和停止。专用诊断复现旧版本发送作用于新内容。 |
| 3：换成员后清空；去掉检查测试失败 | 【实测】通过，反例成立 | 原场景检查快照、可见控件、面板和新提问框。仅在编译输入中禁用 `turn_allowed()` 的成员检查后，91 项通过、6 项失败、`test.finished=1`，退出 1。 |
| 4：全量与内存检查通过；产品代码不超过 350 行 | 【实测】规模通过；两轮全量均未全绿 | 普通 218/219，149.56 秒；ASan／UBSan 218/219，225.13 秒；均退出 8、零跳过，唯一失败 `capability-table-doc`。产品保守计数 202 行新增＋17 行删除＝219 行，含 CMake 接线。 |

## 问题

### AC-R1-01｜中｜旧页面版本的 AI 发送请求仍发送编辑后的问题

- **文件和行号：** `system/ui/shell/ai_chat.c:85`，发送执行处 `:70`。共用宏 `:82` 没有声明能力所属页面；`system/policy/cap.c:132` 只对具有 `.activity` 的能力核对 `page_gen`。
- **复现步骤：** 在真实 `AiAsk` 输入“先前的问题”，记录 `ink_ui_content_version()`；追加“，后来补充的新内容”，再在 UI 任务调用 `ink_cap_invoke("shell.ai_send", NULL, INK_SRC_AI, old_version, &rc)`。这是普通的编辑后旧请求到达顺序，无需卡住线程。
- **复现命令：** 下列独立诊断程序复用 `tests/ai_chat.c` 的启动、输入与截图助手，只将场景主体替换为上述步骤，经标准输入编译并链接当前未改的产品库。

```bash
taskpolicy -c utility build/ai-chat-review/sim-stale-send \
  --profile simulator/profiles/metalio-plus.json --script tests/ai_chat.script \
  --hub tests/ai_chat.hub.json --assets build/assets --content build/test-content \
  --out build/ai-chat-review/stale-send --quiet --no-random-outages
```

- **结果【实测】：** 旧版本 6、当前版本 7，仍返回 `INK_RC_DONE`（0），等待页显示已发送“先前的问题，后来补充的新内容”。诊断 11 项通过、1 项失败、`test.finished=1`。此专用程序未附预算，进程退出 0；失败证据是 [metrics.json](../../../build/ai-chat-review/stale-send/metrics.json) 的 `review.*`、`test.fail` 和 [快照](../../../build/ai-chat-review/stale-send/ReviewStaleSend/snapshot.json)，不能把进程退出码当成断言通过。
- **影响：** AI 对旧问题准备的发送操作会提交用户后来编辑的内容；快照的页面版本没有在这个实际工具入口得到执行。`tests/ai_chat.c:116`～`:119` 只覆盖立即发送。
- **最小修法：** 仅给 `shell.ai_send` 的能力声明增加 `.activity = "AiAsk"`，复用现有能力关口。无需新增版本缓存或给各调用方补检查。
- **修法核验【实测】：** 只在编译输入中增加该声明，生成 `build/ai-chat-review/sim-stale-send-bound`；同样场景返回 `INK_RC_STALE`（5），12 项通过、零失败，键盘和问题仍在原处。证据在 `build/ai-chat-review/stale-send-bound/`，未将改动写入源码。

### AC-R1-02｜中｜长回答仍使用短面板高度，未实现展开布局（已披露）

- **文件和行号：** `system/ui/shell/ai_panel.c:13`、`:164`、`:172`；`system/ui/shell/ai_chat.c:100` 已把多页回答标为 `AiLong`，但没有相应布局变化。
- **复现命令：** 先运行下文的 `ai-chat` 定向测试，再执行视觉对照命令。

```bash
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py \
  --build build --design "$INK_DESIGN" --out build/ai-chat-compare --reuse \
  --scenes AiHome AiAsk AiListenTap AiLong AiSpeak
```

- **结果【实测＋实现】：** [AiLong 对照图](../../../build/ai-chat-review/ui-compare/AiLong.png) 仍只有三行正文的高度。按当前主题默认值计算，面板高 519／1216，约占屏幕 42.7%；多页回答也未变化。规范第 3.1 节和画板要求长回答约占 54%。本场景分成四页，分数 39.469/255，目标 12/255。
- **影响：** 长回答没有利用设计预留的阅读空间，需要更多次翻页。这不是旧画板字号未乘 1.2 能解释的差异；现行主题下仍是短回答比例。开发报告已如实注明，本项不按“假通过”定严重。
- **最小修法：** 在现有面板中调整长回答的正文行数和面板高度，按主题间距组合接近 54% 的布局，继续使用现有分页函数；不另建页面框架，不缩小字号迁就原高度。

## 10 屏视觉核对

使用本轮刚复跑的 `ai-chat` 和 `ui-scene-*` 截图，按原工具校验截图计数并重算分数。对照图先生成至要求的 `out/ui-compare/`，检查后移至 [build/ai-chat-review/ui-compare/](../../../build/ai-chat-review/ui-compare/)，没有遗留未忽略的图片目录。

| 屏幕 | 独立分数 /255 | 看到的差异及归属 |
| --- | ---: | --- |
| AiHome | 31.105 | 实际为真实空态，缺画板的历史和回执；服务接口缺失／回执不属本任务，报告已说明。输入入口为居中描边胶囊，画板是灰底、左对齐提示。 |
| AiAsk | 34.627 | 提问框明显较矮，下方留出较大空白；焦点双边框来自现有短输入框。键盘话筒在左侧，“大写”“删除”替代图标，发送键未做黑色主按钮；键盘为已有组件。 |
| AiListenTap | 34.685 | “正在听 · 停一下自动结束”正确；底页是提问框，画板是读书页。普通面板略高，符合现行主题放大方向。截图有浅色旧字残影，干净帧没有旧回答，未据此认定数据泄露。 |
| AiLong | 39.469 | 应扩高的面板仍约 42.7%，见 AC-R1-02。翻页与“对话记录”在同一行；画板翻页独占一行，下方另有“听解释／更多”。重播接口缺失已披露。 |
| AiSpeak | 38.533 | 有真实朗读状态、正文和可用的“停下”；缺画板停止按钮中的方块图标，底页仍是提问页。未发现文字压住停止按钮。 |
| Main | 54.194 | 时钟、图标与 Dock 更大更深，属于现行主题与旧画板的差异；“还没有读到一半的书”为夹具空态。此次仅更换 Dock AI 的能力绑定。 |
| Apps | 43.141 | 使用分页，工具区当前页出现“对话记录”；画板所有工具同屏。由既有分页和新增应用排序造成，已有测试的包含／排除断言相应更新。 |
| AiListen | 38.008 | 读书底页是不同章节；按住提示“正在听 · 松开结束”正确。尚未出转写时为空，未伪造示例问题。 |
| AiThink | 37.972 | “正在看这一页”和真实问题都有显示；底页章节、字号和面板位置与旧画板不同。 |
| Ai | 43.711 | 普通面板使用三行正文及页码条，底部入口是“对话记录”，没有画板的“听解释／更多”；重播缺口已披露。 |

十屏分数均超目标。灰度、主题放大、空态和测试文本不同也会抬高像素差，不能只凭分数定严重程度。未见新增文案错字或“家庭中枢／PIN”等禁用词；合成问题前缀只在测试夹具中。

## 独立测试与反向对照

核对两套 CMake 缓存：字体、LVGL、Python 均来自指定依赖；`INK_WERROR=ON`，内存配置另有 `INK_SANITIZE=ON`；服务器测试使用已有 `INK_HUB_PYTHON`。编译参数确有 `-fsanitize=address,undefined -fno-omit-frame-pointer`，未安装或升级依赖。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2

# 定向复跑场景与快照
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ai-chat.*$' --output-on-failure -j2
```

构建、全量测试逐轮执行；GUI 冒烟测试由原测试集以 dummy 模式运行，未开窗口。

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| 普通构建／全量 | 构建 0；CTest 8，218/219，149.56 秒，零跳过 | `build/ai-chat-review-build.log`、`build/ai-chat-review-ctest.log` |
| ASan／UBSan 构建／全量 | 构建 0；CTest 8，218/219，225.13 秒，零跳过；无 sanitizer 报错 | `build-asan/ai-chat-review-build.log`、`build-asan/ai-chat-review-ctest.log` |
| 两套 ai-chat 场景 | 均 97 通过、0 失败、`test.finished=1` | 两套 `test-out/ai-chat/metrics.json` |
| 资源 | 两套内部 RAM 最低余量均 34,512 字节，原下限 32,768；内部／外部内存分配失败均为 0 | 同上；预算未改 |
| 唯一全量失败 | `capability-table-doc`：生成能力从 197 项增为 202 项，文档仍是旧值 | 两套全量日志；开发报告已披露，未虚报全绿 |

三个反例只在内存中的编译输入替换一处，沿用原编译／链接参数生成独立二进制；未修改源码、原可执行文件、断言或预算。编译和链接均退出 0。

| 反例 | 唯一替换 | 场景结果 |
| --- | --- | --- |
| 成员检查 | `turn_allowed()` 身份条件前加 `false &&` | 91 通过、6 失败；快照完成性、旧会话可见控件和旧面板关闭断言各失败两次；退出 1，`test.finished=1` |
| 传输结束后停止语音 | 停止目标从 `AI.tts_opened` 改回 `AI.tts_stream` | 96 通过、1 失败；`tests/ai_chat.c:160` 确认仍在播放；退出 1，`test.finished=1` |
| 返回记录入口 | 去掉 `ink_ui_drop_app("ai")` | 96 通过、1 失败；`tests/ai_chat.c:170` 确认没有回首页；退出 1，`test.finished=1` |

可逐个复跑，`CASE` 分别取 `member-negative`、`queued-negative`、`home-negative`：

```bash
CASE=member-negative
taskpolicy -c utility "build/ai-chat-review/sim-$CASE" \
  --profile simulator/profiles/metalio-plus.json --script tests/ai_chat.script \
  --hub tests/ai_chat.hub.json --budget tests/ai_chat.budget.json \
  --assets build/assets --content build/test-content \
  --out "build/ai-chat-review/$CASE" --quiet --no-random-outages
```

日志、指标和汇总在 `build/ai-chat-review/`。反例均正常跑至结束，没有用崩溃或超时冒充失败。原场景的等待主要为条件轮询；固定按键持续时间、截图事件等待和迟到语音观察窗口有具体用途，未发现靠新增 sleep 掩盖产品问题。

## 品味和规模（含过度设计审视）

- 【实测】相对基线共 18 个任务文件。纯产品 C／头文件新增 190 行、删除 17 行；连同 12 行 CMake 接线保守计 219 行，低于 350。未新增第三方资源或大文件，未修改资源预算。
- 【实现】两页及语义声明集中在 `ai_chat.c`，三个状态复用面板和文本分页；语音仍归 AI／音频服务。新增停止接口只有一个服务入口，没有复制轮次状态机。焦点请求用既有 UI 执行队列，音频订阅派发到 UI 任务。
- 【实现】未发现新增产品任务、线程、持久格式、重试、缓存、配置项或通用框架；驱动任务只进测试程序。**没有需要删除的新机制。** AC-R1-01 使用已有能力关口即可修复，不应为它另造检查机制。
- 【实现】按钮、翻页键与 AI 共享能力入口，五页可导出 F1 快照，分层检查通过；快照版本与实际工具入口的约束不一致，是 AC-R1-01 对“软总线＋原子能力、AI 原生”要求的具体缺口。

## 总规则、范围及已知限制

- 已有测试修改已逐项核对：应用数增加一项；Apps 排序断言随真实目录变化；回答改为 JSON 转义后仍检查完整内容；确认目录改为遍历分页，仍排除人工确认能力并检查查询／撤回；AI 能力清单追加五项。未发现删测试、放宽预算或用跳过换通过。
- CMake 为末尾追加，页面在新文件；`navigation.c:627`、`:1405`、`ui_task.c:733` 等接线有原位替换，不是共享文件完全只追加。这是任务边界的字面偏离，但改动集中于 AI，没有改列出的其它并行任务文件；合并时应关注这几处共享接线。
- 全量检查尚未通过的最小收尾：由合并主会话重新生成并核对 `docs/system/capabilities.md`，再通过原检查。总规则第 7 节禁止本分支改该共享文档，故未要求开发分支越界，也不能删检查。
- 历史列表、已有回答重播、超出 AI 服务 2047 字节上限的回答仍未实现／未覆盖，报告已说明。产品未伪造示例历史；离线、额度、回执、确认页不在本任务范围。Linux／gcc 和真机未运行。
- 本轮未访问设备、真实密钥或无关资料，未调用付费服务，未推送或开 PR。基线至复查版本的差异中，本机路径和 MAC 模式扫描零匹配；`git diff --check` 通过。代码差异不足以追溯开发期间全部外部操作，不对此作无证据保证。
- 结束时 `git diff --exit-code HEAD -- system tests CMakeLists.txt` 为 0，工作区仅本报告新增；本轮源码替换全部发生在编译输入中。

SEVERE=0 HIGH=0
