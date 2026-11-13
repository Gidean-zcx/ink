# ai-panel-1：AI 面板和 AI 页照豆包、Gemini 重做：在原页底部问，面板写明问的是哪一页；打断照 Siri（按 AI 键马上停、马上听）；“问 AI”进的是能对话的 AI 页，不再是记录列表

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）。

**D-140（总规则第 11 节）**：本任务是“照着重写”。第一步写对照表 `docs/tasks/ai-panel-1/对照表.md`（参照产品每一页、每个操作和状态，外壳看 `docs/tasks/shell-research-1.md` 和交付计划这一行的来源出发）并单独提交；计划和 `replan-sysapps/SOURCES.md` 点名的 C/C++ 代码能直接用的照搬。参照安卓的地方按第 11 节为这台设备改，做完逐行验收。

## 为什么

D-131（用户 2026-11-12）把 AI 面板和 AI 页面列进第一优先的外壳部件；D-129 要求照成熟设计做，不自己发挥。交付计划 3a 一行：“豆包、Gemini 式的底部面板，带当前页面的上下文；打断照 Siri（shell-research-1 第 5 节）”，照着重写，预算 500 行，进试用版 22。中枢这边的语音已经换成小智的流水线（D-136，`hub-voice-1` 已合入：流式识别、中间结果按 `interim_ms` 节流、首句逗号切、流式朗读、100 毫秒内中止、对话历史只留用户听到的部分）。设备这边还没跟上，现在的代码有这些实在的缺口：

- **打断没告诉中枢听到哪**：按 AI 键打断时，`ai_service.c:733` 的 `close_session()` 走 `rt.c:267` 的 `ink_rt_close()`，只发 `{"sid":n}`，没有协议 §3.5 的 `reason` 和 `heard_frames`（`docs/hub/device-protocol.md:214-219`）。中枢 `gateway/server.py:331` 拿不到 `barge_in`，就把整段回答当成用户听完了记进历史，下一句追问的上下文是错的。
- **设置关掉“说话能打断回答”，连按键也打断不了**：`ai_service.c:716` 在朗读时直接 `return`。Siri 的做法是再按一次键就停下、开始听；这个设置只该管“开口打断”（要回声消除，本机 S31 没有，见 `docs/tasks/hub-voice-1/AEC.md`）。
- **“停止朗读”只停声音**：`shell.ai_stop` → `ink_ai_stop_speech()`，中枢照样往下生成，文字还在涨，会话要等回复才关。
- **面板不说问的是哪一页**：`ai_panel.c:105` 一律写“出自这一页”；出错页 `ai_pages.c:17、21、45-46、71` 写死“书还能继续读”“继续读”，在短信、设置里打开也这么说。
- **快照里是内部板名**：`ai_panel.c:37` 的“页面”字段值是 `AiSpeak`、`AiListenTap`、`AiLong`、`AiAsk`，不是看得懂的状态；`ai_chat.c:181-205` 的 `ink_ai_chat_panel_snapshot` 是没人调用的第二份面板快照。
- **旧样式残留**：`ai_panel.c:243` 顶上一根黑色拖拽条（墨水屏不能拖，研究第 5.2 节明确“不只留细拖拽条”“禁止手指拖到哪儿刷到哪儿”）；操作区是胶囊加图标的拼法，不是 design-lang 的三种按钮。
- **“问 AI”进的是记录列表**：主屏常用里的“问 AI”经 `navigation.c:165` 调 `shell.ai_home`，打开的是三行一页的“对话记录”（`ai_chat.c:110-140`，应用名也叫“对话记录”，`:143`）；打字问要再进一个单独的 `AiAsk` 页。打开一条记录（`ai_history.c:60` 的 `ink_ai_show_answer`）只显示回答，问题丢了。

## 来源（照着重写；没有能直接搬的代码）

- `shell-research-1` 的 `DESIGN.md` 第 5.1 节（底部面板的职责、退出语义）和第 5.2 节（AI 面板的高度、各状态的呈现和语义，逐条照做），示意图 `ai-panel.svg`；`SOURCES.md` 的 S13（豆包随处唤起、Gemini 的“当前屏幕”上下文入口、Siri HIG 的动作和实体）；`REUSE.md` 第 5 行：几家都没有可移植代码，继续用我们的 `ai_panel.c`、页面语义和能力。合入前的副本在 `$HOME/Documents/Workspace/ink-codex/shell-research-1/repo/docs/tasks/shell-research-1/`。
- **打断照 Siri**：研究第 5 节没有逐条写 Siri 的打断细节。本任务取的是 iOS 上 Siri 的公开行为：回答时再按键马上停、开始听新问题；点“停下”停声音也停回答，已出的文字留着；来电、闹钟照优先级接管；可以打字问，不只能说。`SOURCE.md` 里给每条补 Apple 官方出处（支持页面或 HIG），找不到出处的标【估算】，不要编。
- `docs/tasks/hub-voice-1/REPORT.md`（中枢怎么处理中止和已听历史）、`docs/hub/device-protocol.md` §3.5（`rt.close` 的 `reason`：`barge_in`、`dismiss`、`done`、`cancel`；`heard_frames`）、`docs/tasks/hub-voice-1/AEC.md`（S31 没有 ESP-SR，开口打断暂时只有带语音前端的机型，模拟器配置文件里有，真机没有）。
- `docs/tasks/design-lang-1/PAGE-FOLLOWUPS.md` 第 18 行：面板高度由行高算（新主题下默认 533、展开 673），大字时要按实际内容核对。
- 底部面板容器用 `dialogs-1` 合入的共用底部面板（它按研究第 5.1 节做）。

## 要做的

1. **面板照研究第 5.2 节重排**（`system/ui/shell/ai_panel.c`，原地重写）：用 dialogs-1 的底部面板容器（没有拖拽条；它没有就用 `ink_ui_panel`，报告写明）。从上到下：头部一行（AI 图标不套圈、一句状态、右上“收起”，点按范围 108）；**上下文一行**，用灰字写明问的是什么，取自 `ink_ui_context_snapshot` 的标题，例如“问：树林里的风 · 第 12 页”“问：和妈妈的短信”，主屏写“问：主屏”，取不到就写“没有带上页面内容”，不编；回答正文（默认 3 行，长了展开一次，再长就分页“上一页 1/3 下一页”，不滚动）；底部一行动作，**一个主要按钮**，其余是次要或文字按钮。高度照旧由行高算，不写死 528/660；最大字号下头部和按钮不裁，正文分页。
2. **各状态和按钮**（照研究第 5.2 节的表，按钮只放接通了的）：
   - 在听：状态“在听”；正文显示中枢发来的中间识别结果（`ai.stt` 现在就带累计文字），只刷这一块；按住键时写“松开 AI 键结束”，点按开始时主要按钮是“说完了”；另有“打字问”。
   - 在想 / 回答中 / 朗读中：状态写实际阶段（“正在看《书名》”“正在回答”“正在读给你听”）；主要按钮“停下”。
   - 回答完：正文为主，不重复问题，也不写“回答完成”；主要按钮“继续问”（话筒），次要的有“再听一遍”（只在中枢在线、能重读时出现）和“在 AI 页打开”。
   - 这一轮里 AI 调用过能力（`activity.log` 里本轮开始以后当前家人的新记录）：加一行“这次替你做了 N 件事 ›”，点开 `shell.ai_receipts`。高风险动作照旧交系统确认卡（D-105），确认卡出来时面板不放和它抢的主要按钮。
3. **打断和停下照 Siri**（`system/services/ai/ai_service.c`、`system/services/hub/rt.c`，小改）：
   - 回答、朗读时按 AI 键：**不论“说话能打断回答”开没开**，都马上停声音、开始听新问题。`ai_service.c:716` 改成只有开口打断（`INK_LISTEN_BARGE`）才看这个设置。
   - 上一轮会话关的时候，带上协议 §3.5 的字段：按键或开口打断是 `barge_in`，点“停下”或关掉面板是 `dismiss`，正常结束是 `done`；`heard_frames` 取 `ink_audio_stream_progress(tts, &played, NULL)` 的已播帧数。`rt.c` 加一个带原因的关闭（例如 `ink_rt_close_reason(sid, reason, heard_frames)`），原来的 `ink_rt_close` 照旧可用，不改别的调用方。
   - “停下”：停声音，也停这一轮的回答（语音一轮关会话、带 `dismiss`；打字问的一轮没有会话，设备丢掉后面的 `ai.delta`）；已出的文字留着，状态写“已停下”。打字问在中枢那边停不下，写进报告，作为协议缺口，不改中枢。
   - “收起”只收起（`ai.panel.hide`，继续处理），HOME/返回照旧结束这一轮（`ink_ai_dismiss`）；来电、闹钟、换家人、锁屏照现有规则收尾，面板和上下文不带给下一个人。
4. **打字问**：面板里点“打字问”，用共用输入框和系统键盘在面板里问。**上下文仍然是面板下面那一页**，不能变成输入页自己；面板上移到键盘上方，还是进 AI 页但带上原页的快照和名称，二选一，报告写理由。发送照旧走 `shell.ai_send`。
5. **AI 页**（`system/ui/shell/ai_chat.c`，照豆包、Gemini 应用的主页重写）：`shell.ai_home`（主屏“问 AI”、面板“在 AI 页打开”都调它）打开一页对话：顶上标题“问 AI”，右边文字按钮“记录”；中间是最近的对话（问题中粗，回答正文，按页翻，不滚动）；底部一行共用输入框“问点什么…”加话筒按钮（108）。从面板进来时显示刚才这轮的问和答。没有对话时只用一行共用空状态：“按住 AI 键说话，或者在下面打字”，不放推荐问题。原来的 `AiAsk` 单独输入页并进来，删掉，不留两套。应用名改成“问 AI”。
6. **记录页**：原来的对话列表退到“记录”二级页（照旧分页、删除、清空，按家人分开）；打开一条记录在 AI 页显示问题和回答（`ai.history.open` 要带上问题）。“AI 替你做过的事”和“做事的结果”（`ai_pages.c` 的 `AiLog`、`AiReceipts`）只换新样式，能力不变。
7. **失败和缺东西时说清楚（D-138）**（`system/ui/shell/ai_pages.c`）：出错页不再写死“书还能继续读”“继续读”。说明写实际的原因和还能做什么，关闭按钮写“收起”或回到哪一页（取上下文名称）。没加入家庭（`pairing.status`）就说“还没连上家庭服务器”，并给出现有的加入家庭入口，没有就只说明，不新造页面；没有话筒、语音通道开不了、离线、额度用完、超时，各有一句原因和出路；孩子的出路写“请家长……”。任何状态都不能是空白面板。
8. **快照和能力**：面板快照的“页面”字段改成看得懂的状态（在听、在想、回答中、朗读中、回答完、已停下、出错），新增“上下文”字段（问的是哪一页，带对象和版本）；删掉没人调用的 `ink_ai_chat_panel_snapshot`。每个按钮都是一条能力，人点和 AI 调走同一条（现有的 `ai.panel.*`、`shell.ai_*`；新增的例如“在 AI 页打开”“打字问”“停下”，停下可以就用 `shell.ai_stop` 加新语义）。输入框、密码框照旧：密码和 `ai_hidden` 的内容不进上下文，AI 不能代输。

## 共同要求（页面任务都一样）

- 来源：状态、按钮、出错处理照上面的来源一个不落，不自己发挥；`docs/tasks/ai-panel-1/SOURCE.md` 写每一部分对应来源的哪一节、哪个文件和行号，哪些没照搬、为什么。
- 新设计语言：只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/theme.c`、`system/ui/ui_list.c`、`system/ui/textfield.c` 里的三种按钮、输入框、列表行、分页、空状态，加上 dialogs-1 的底部面板）；能点的不小于 108 px；一个任务区只有一个主要按钮；图标不套圆圈；没内容的区块不画。不写死像素和颜色，`python3 tools/ui_scale_check.py` 过。
- 文字大小：跟系统“文字大小”一起缩放（`ink_ui_px`、行高），默认和最大一档各出一套截图；放不下就分页，不缩字、不裁按钮。
- 刷新（D-127）：只刷变了的那块。中间识别结果只刷正文，状态变了只刷状态行，翻页只刷正文和页码，不为逐字效果多刷；面板出现、收起照现有规则。按下反馈照 D-31。
- 不变的规矩：家人隔离、孩子模式、确认卡只给 D-105 的高风险动作；AI 不代输密码、不代人确认、不代人换家人。
- 内存：面板和页面的状态放 PSRAM（`INK_PSRAM_BSS`），内部内存不涨（`tests/ai_chat.budget.json` 的内部最低空闲不能降）。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由）；新文案的英文加进 `assets/i18n/en.tsv`。这两个文件都只在末尾追加本任务的行，合并冲突由主会话处理。
- 截图放 `docs/tasks/ai-panel-1/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。

## 范围和预算

- 只改：`system/ui/shell/ai_panel.c`、`ai_chat.c`、`ai_chat.h`、`ai_pages.c`、`ai_pages.h`；`system/services/ai/ai_service.c` 和 `ai_history.c` 里打断、停下、带原因关闭、记录带问题的那几处；`system/services/hub/rt.c` 和 `system/include/ink/sys/rt.h` 加一个带原因的关闭；`simulator/environment/sim_hub.c` 记下 `rt.close` 的 `reason` 和 `heard_frames`（只加计数或日志，给测试断言用）；AI 相关测试；`docs/tasks/ai-panel-1/`。可以在 `hub/tests/` 加一条测试，用设备实际发出的 `rt.close` 文本喂中枢，证明历史截到听到的地方；中枢产品代码不改。
- 不碰：`theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c` 和确认卡那组文件（dialogs-1；缺共用控件写进报告）；`navigation.c`（“问 AI”仍调 `shell.ai_home`，不用改它）；`settings_registry.c`（“说话能打断回答”那一行该改成“只管开口打断”、并标出本机不支持，写进报告交给 settings-2）；`system/services/audio/*`（music-3）；`hub/ink_hub/`。
- 产品代码新增和改写合计不超过 500 行（删掉的旧代码不算）；固件增量不超过 12 KB。
- 起步：主会话给的分支（已含 design-lang-1/2、control-1、settings-1、hub-voice-1，**dialogs-1 合入以后**）；合并排在 dialogs-1 之后。

## 验收

1. **照研究第 5.2 节走一遍**（模拟器脚本，每步截图并断言）：在阅读页按住 AI 键 → 面板出现，原页还在，上下文行写着书名和页 → 说话时正文出现中间识别结果 → 松开 → “正在看《书名》”→ 回答出来 → 长回答展开、分页翻到第 2 页 → “再听一遍”→ “在 AI 页打开”，看到同一轮的问和答 → 返回原页，阅读位置不变。同一条路径用能力调用（来源是 AI）再走一遍，每一步的快照和人点的一样。再在短信对话、设置首页各打开一次，上下文行分别写对。
2. **打断**：朗读时按 AI 键 → 声音在 200 毫秒内停（沿用 `ai.barge_to_stop_ms` 一类的探针），马上进入“在听”；模拟器中枢收到上一轮的 `rt.close`，带 `reason=barge_in` 和不为 0 的 `heard_frames`，之后不再播上一轮的任何朗读帧。把“说话能打断回答”关掉再做一遍：按键照样打断，开口不打断（模拟器语音前端配置下）。点“停下”：声音停，文字不再涨，已出的文字留着，`rt.close` 带 `dismiss`。**反向对照**：去掉带原因的关闭，或者恢复 `ai_service.c:716` 的提前返回，对应断言必须失败。中枢测试：用设备实际发出的 `rt.close` 文本，中枢历史只留听到的部分。
3. **打字问**：在阅读页打开面板 → “打字问”→ 输入、发送；中枢收到的 `page.text` 是阅读页，不是输入页。在 AI 页打字问、用话筒问各一次。
4. **失败和缺东西**：离线、没加入家庭、额度用完、超时、没有话筒，各截一张图，都有原因和出路，没有一张写“书还能继续读”（在短信页打开时）；孩子的版本出路写“请家长”。没有一个状态是空白面板。
5. **边界**：面板开在 Wi-Fi 密码页上，中枢收到的上下文里没有密码，AI 调输入能力往密码框写会被拒绝；回答中换家人或锁屏，面板关掉，下一个人看不到上一轮的内容；来电时声音让出来，文字留着。
6. **刷新**：中间识别结果、翻页、状态变化各只刷自己那块（模拟器显示统计断言区域落在面板内对应的控件里）；一轮普通回答的刷新次数写进报告，和改前对比。
7. **改前改后截图**：面板的在听、在想、回答完（短答和分页的长答）、出错，AI 页（空的、有对话的）、记录页，大人和孩子，默认和最大文字大小各一套。
8. 主工程普通全量 ctest 过（含 `ui-scale`、`ai-chat`、`ai-chat-snapshots`、`ai-answer-long`、`ai-speech-long`、`integration-ai-surface`、`integration-ai-race`、`scenario-ai_*`、`ui-scene-*Ai*`）；ASan 跑 AI 相关测试过；中枢 pytest 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写固件增量、产品代码行数、`gcccheck` 结果、改过的每一个已有测试（只改为了这次有意改变的行为，例如快照状态名、按钮文案、`AiAsk` 并进 AI 页，断言不放松），以及给 settings-2 的“说话能打断回答”改法、打字问在中枢那边停不下的协议缺口。真机上的体验（中枢本地识别目前要 17 秒以上，见 hub-voice-1 报告）不算本任务的验收，由主会话上机看。

## 复核修订（起草后的检查，和上文冲突时以这里为准）

No full rewrite is needed. Apply these edits to the draft:

1. Range and budget: change the product-code cap from 500 to about 900 lines (new plus rewritten). If you want to keep 500, move item 7 (error causes) and item 6 (records page) into a follow-up task ai-panel-2. Keep the 12 KB firmware cap, measured against the pre-task build.
2. Item 3: add "read the played frames with ink_audio_stream_progress(tts_opened, &played, NULL) BEFORE stopping speech or closing the session, then pass them to ink_rt_close_reason".
3. Item 1: say the context is taken from ink_ui_context_snapshot, and that the context object comes from the reader (reader-one-1) when the book is open, otherwise the title only.
4. Item 7: say ai-panel-1 only calls whatever join-family entry exists at merge time (auto-provision-1 and settings-d137 may add or change it) and writes no new join logic.
5. Acceptance 2: name the simulator voice-front-end profile scenario used for open-mouth barge-in.
6. Acceptance 5: name the Wi-Fi password page scene used for the context-leak check.
7. Acceptance 6: if the simulator display stats cannot report refresh regions per widget, use the existing refresh-count probe and drop the per-widget assertion.
8. Add a first step: capture the before-screenshots and the before refresh count on the unmodified build.
9. Sources: copy the sections of the shell-research-1 DESIGN.md (5.1, 5.2), SOURCES.md (S13) and REUSE.md (row 5) into docs/tasks/ai-panel-1/SOURCE.md. Treat gateway/server.py:331 as read-only context.
10. Fix the cited line for the "出自这一页" string to ai_panel.c:104-105.
