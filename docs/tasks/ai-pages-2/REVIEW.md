# ai-pages-2 第 1 轮复查

日期：2026-11-09。基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`；复查版本：`68b6b2565f2d9a5d2fe85e680cc21b2683153f8a`。

**结论：未达到全部验收。严重 0、高 0、中 5、低 1。** 五页有实际实现，常规场景、成员隔离和四组反向对照有效；补充场景发现错误浮层生命周期、途中断网、首次加载及回执展示的问题。普通和 ASan／UBSan 全量均为 228 项通过、2 项文档失败、3 项环境跳过，未见内存诊断错误。

本轮独立运行命令、查看代码与 12 个界面状态的对照图。只写本文件，不修改产品源码、原测试、预算或共享文档，不提交。编译诊断、合成数据和截图留在被忽略的构建目录；证据限于 macOS／clang 模拟器。

## 验收表

| 项 | 复查结论 | 独立证据 |
| --- | --- | --- |
| 1：断网、超时、额度页出现且能返回 | 部分通过 | 原 `ai-pages` 三个错误场景分别 21、20、30 项断言通过；补测回答途中断网没有 `AiOffline`，通用收起留下隐藏浮层，见 R1-01、R1-02。 |
| 2：当前成员活动分页、换人隔离及反例 | 常规分页与隔离通过；首次加载有缺口 | 十条记录经真实 `activity_flush` 写入，顺序、翻页、儿童不能读取成人编号均通过；固定读取 owner 的反例触发 6 项失败。新成员首次打开记录页不能加载，见 R1-03；返回位置见 R1-06。 |
| 3：显示每次操作回执 | 数据映射通过；可读性及设计动作未完成 | 十种现有状态逐页快照通过；原始数据没有完整回执文案的限制属实。已有对象信息被截断、可撤销操作没有入口，见 R1-04、R1-05。 |
| 4：每页有 AI 快照 | 常规五页通过；可见性不一致 | 原场景使用 3072 字节生产预算，五页结构化快照通过；收起后 AI 仍看到错误页，见 R1-01。新成员读失败只给空列表，见 R1-03。 |
| 5：全量、内存检查、300 行预算 | 规模与运行期内存检查通过；全量未全绿 | 两套构建成功；全量各 228 通过、2 失败、3 跳过。产品新增 185 行、删除 1 行，按两者相加也仅 186 行。 |
| G4：设计对照 | 已独立查看并复算，分数超过目标 | 四页分别 46.679、44.719、46.222、34.283／255，目标 12；回执原稿是组件总览，尺寸不同，不能直接评分。 |

## 问题

### R1-01｜中｜收起错误面板后，隐藏浮层仍占据快照和翻页输入

- **位置：** `system/ui/shell/ai_pages.c:27`、`:64`；关联 `system/ui/shell/ai_panel.c:106`。
- **复现：** 读书时触发离线错误，然后通过已有能力 `ai.panel.hide` 收起，再按下一页键。运行下文探针命令，`REVIEW_CASE=hide`。
- **结果【实测】：** 探针退出 1，20 项通过、2 项失败、完成标记为 1。[收起后的画面](../../../build/review-ai-pages-r1/hide/snap-AfterHiddenNext.png) 已是阅读页，但 `ink_ui_top_name()` 以及 [AI 快照](../../../build/review-ai-pages-r1/hide/AfterHide/snapshot.json) 仍为 `AiOffline`；下一页键被错误浮层吞掉，页码仍为 1。
- **原因【实现】：** 新错误内容在既有 AI 面板里面，又单独注册 `E.overlay`；只在子对象删除时注销。原有 hide 仅隐藏父对象并注销原 `OVERLAY`，不会删除子对象。此处并非需要人为暂停线程的时序问题。
- **影响：** 画面与 AI 所见不一致，收起后不能直接翻页；按 HOME 清除残留或重新提问可以恢复，未发现数据丢失，因此定中。
- **最小修法：** 错误页复用 AI 面板的同一个浮层注册与 hide／reveal／close 生命周期，删除独立的 `E.overlay` 注册；随错误状态更新该浮层的名称和快照。不要逐个关闭入口补清理代码。

### R1-02｜中｜请求途中断网没有进入 AiOffline

- **位置：** `system/ui/shell/ai_pages.c:30`；上游已有分支 `system/services/ai/ai_service.c:263`；原测试条件见 `tests/ai_pages.py:53`。
- **复现：** 保持原 `sim-ai-pages` 二进制与断言，仅令脚本先连接 Wi-Fi、在第 10 秒断网，中枢合成响应等待 60 秒。命令见下文“途中断网”。
- **结果【实测】：** 退出 1，15 项通过、6 项失败、完成标记为 1。[快照](../../../build/review-ai-pages-r1/lost/AiOffline/snapshot.json) 的状态为“网络断了，回答没收全，请再试一次”，顶层仍为旧 `Ai`，没有新页的 `ai.pages.close` 控件。
- **原因【实现】：** 新页只匹配请求前无网的那句文案，漏掉服务已经支持的 `INK_HUB_ERR_LOST`。原用例从启动起就 `net none`，不能覆盖回答期间掉线。
- **最小修法：** 在这一处映射中把现有 LOST 状态文案也归入离线页；保留“回答没收全”的真实含义，不自动重放请求。
- **修法核验【实测】：** 仅在标准输入的临时编译文本中补上该匹配，链接当前产品库为 `sim-lost-mapping`；同一脚本及原断言全部 21 项通过、退出 0。产品文件未改，结果在 `build/review-ai-pages-r1/lost-mapping/`。

### R1-03｜中｜活动集合未预加载时，页面没有加载与恢复路径

- **位置：** `system/ui/shell/ai_pages.c:115`、`:133`、`:140`。
- **复现：** 用既有 `member.add` 和合成的人手确认添加测试孩子，切换后打开记录页，再通过能力总线执行第一条测试操作。运行探针 `REVIEW_CASE=newmember`。
- **结果【实测】：** 退出 1，24 项通过、1 项失败。首次打开显示[“记录暂时读不了，请稍后再打开”](../../../build/review-ai-pages-r1/newmember/snap-NewMemberEmpty.png)。随后数据库确有 1 条记录，但 [当前页快照](../../../build/review-ai-pages-r1/newmember/NewMemberAfterAction/snapshot.json) 仍为 `total:0`、`fields:[]`；退出并重开才显示 1 条。没有损坏存储、耗尽内存或卡住线程。
- **原因【实现】：** 只调用 `ink_db_open_cached_member()`，没有请求已有 BG 能力完成首次加载；而 `list_settle()` 要求 `L.db` 非空，无法从未加载状态恢复。启动时只预加载当时已经存在的成员，不能覆盖之后新加的成员。AI 快照也没有反映屏幕上的读取失败。
- **最小修法：** 缓存未命中时调用已有 `activity.list` 做一次 BG 加载，UI 回执按当前页面／成员校验后重建；沿用现有执行器与能力回调，不建加载线程、缓存或重试机制。加载失败应明确呈现为读失败，不能当作成功读取到 0 条。

### R1-04｜中｜回执的操作对象被截掉，且没有完整查看路径

- **位置：** `system/ui/shell/ai_pages.c:96`、`:99`、`:103`。
- **复现：** 运行探针 `REVIEW_CASE=real`，真实执行 `settings.set {"id":"display.light","value":3}`，再打开该条回执；默认字号即可出现。
- **结果【实测＋实现】：** [实际画面](../../../build/review-ai-pages-r1/real/snap-RealReceipt.png) 只有“做好了／11-02 08:00 · 改一项设置 · setting…”；完整对象 `setting:display.light` 虽在 [快照](../../../build/review-ai-pages-r1/real/RealReceipt/snapshot.json) 中，却没有显示给人。回执行的动作全部 disabled，点它不能展开。原始测试夹具也出现 `synt...`，最大字号还会截断“页面或家人已变，操作已停止”。
- **影响：** 用户无法仅凭回执辨认改了哪个设置；这不是“底层没有保存原回执文案”造成的，已有对象信息本身也被布局丢掉。原测试断言只检查快照中的字符串，不能证明人能读全。
- **最小修法：** 在当前回执页中让状态与已有对象信息完整换行，按主题行高减少每页条数，继续使用 F4；不要新增详情页机制，也不要缩小字号或要求服务新增回执格式。

### R1-05｜中｜已有可撤销回执未接“撤销／改回”入口（开发报告已披露）

- **位置：** `system/ui/shell/ai_pages.c:87`、`:99`、`:107`。已有数据与能力分别见 `system/policy/cap.c:247`、`:1348`。
- **复现：** 同 `REVIEW_CASE=real`：修改亮度后立即看回执，再直接调用现有 `system.undo`，使用原操作的 object 和 revision。
- **结果【实测】：** 原操作返回 `undo_ms=10000`，页面／快照只有“返回”，没有撤销。随后 `system.undo` 成功返回 `UNDONE`，亮度恢复，证明后端能力可用。探针 14 项通过、仅“回执应开放撤销”1 项失败，退出 1。
- **依据：** `AiLog`／`AiReceipts` 画板中的“改回／撤销”及 `docs/product/system-ui-spec.md:30` 的“能撤销的给撤销”。ACTIVITY 已记录 object、revision、undo_ms、when；本次 UI 没读取撤销字段。开发报告承认未接，不定为假实现或严重问题。
- **最小修法：** 对已有撤销窗口内的记录绑定 `system.undo`，传同一 object／revision，按钮和 AI 共用该能力；最终有效性仍由现有撤销能力校验。不要扩建撤销历史或修改活动记录格式。

### R1-06｜低｜从回执返回时丢失活动列表页码

- **位置：** `system/ui/shell/ai_pages.c:69`、`:114`。
- **复现：** 十条记录 → 翻到第 2 页 → 触摸第一条查看回执 → 触摸返回。运行探针 `REVIEW_CASE=visual`。
- **结果【实测】：** 54 项通过、仅此 1 项失败，退出 1。回执显示第 2 页的相应记录，返回后 [快照](../../../build/review-ai-pages-r1/visual/LogReturned/snapshot.json) 变为 `page:1`，选中项也清空。
- **原因【实现】：** 两个页面共用一份 F4 位置，而且每次创建都无条件清零。原测试先回第一页再点记录，所以没有覆盖该回退。
- **最小修法：** 两个视图分别保留既有 F4 position，仅切换成员时清空；不要另外引入导航状态框架。

## 独立测试与反向对照

两套配置的字体、LVGL 和 Python 均来自指定的 `$INK_DEPS`；均保留 `INK_WERROR=ON`，ASan 配置另启用 `INK_SANITIZE=ON`，实际编译参数含 `-fsanitize=address,undefined -fno-omit-frame-pointer`。按总规则配置并重建，构建与全量逐轮执行，全部使用 utility 优先级、最多 `-j2`。

```sh
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2

INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ai_pages.py --build build
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ai_pages.py --build build-asan
```

| 检查 | 退出码与结果 | 证据 |
| --- | --- | --- |
| 普通构建／全量 | 构建 0；CTest 8；228 通过、2 失败、3 跳过；153.62 秒 | `build/review-r1-build.log`、`build/review-r1-ctest.log` |
| ASan／UBSan 构建／全量 | 构建 0；CTest 8；相同计数；253.92 秒；未见 sanitizer 报错 | `build-asan/review-r1-build.log`、`build-asan/review-r1-ctest.log`、`build-asan/Testing/Temporary/LastTest.log` |
| 普通四场景 | 退出 0；21／20／30／85 项断言通过 | `build/ai-pages-akyqrnq0/` |
| 内存检查四场景 | 退出 0；同样全部通过 | `build-asan/ai-pages-rbhbr6by/` |
| 内部 RAM 最低余量 | 两套均为离线 47832、超时 42200、额度 41728、记录 47840 字节；原下限 32768 | 四场景 `metrics.json`；内／外部 failed_allocs 均 0，预算违规均 0 |
| 尺寸检查 | 退出 0，34 个文件、0 违规 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_scale_check.py` |

两项全量失败是能力文档仍为 262 项而源码为 268 项，以及服务设计缺 `ai.pages.close/next/prev/retry` 四项。三个跳过是 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 的既有依赖环境缺失。开发报告如实披露；总规则 §7 又禁止本任务改共享文档，未把失败或跳过计为通过。合并阶段仍须更新指定文档并补齐环境，才能宣称全量通过。

四个负向变体仅编译临时产品副本，原断言和预算不变；都编译成功、运行到 `test.finished=1` 后以断言失败退出 1：

| 变体 | 撤掉什么 | 通过／失败 |
| --- | --- | --- |
| errors | 错误页创建 | 15／6 |
| member | 当前成员绑定，改成固定 owner | 79／6 |
| receipts | 状态映射，全部显示做好了 | 75／10 |
| snapshot | 错误页快照 | 18／3 |

```sh
# REVIEW_MUTANT 分别取 errors、member、receipts、snapshot
REVIEW_MUTANT=member
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" \
  tests/ai_pages.py --build build --mutant "$REVIEW_MUTANT"
```

日志位于 `build/review-ai-pages-r1/{errors,member,receipts,snapshot}.log`。未发现原测试删断言、抬预算或用固定 sleep 掩盖失败；截图前的 5 ms 是等待已安排的截图事件。补充探针最初在“写入后立即抓图”读到了 settle 前状态，已改为先等 UI 处理并完成刷新；该探针自身问题不计产品缺陷。

## 12 屏视觉核对

起始没有仓库根目录的 `out/ui-compare/`；开发产物在 `build/ai-pages-compare/`。本轮用新跑出的图在被忽略目录内生成并逐一打开了 [out/ui-compare](../../../build/review-ai-pages-r1/out/ui-compare/) 的 12 份对照，原稿未修改、未裁切。四个标准页用原工具校验计数并评分，其余是原稿与状态图并排，核对真实的 `test.snapshot.<状态>=1`；它们是本任务五页的不同状态，不宣称有 12 个新页面。

| 已查看状态 | 观察 |
| --- | --- |
| AiOffline | 46.679／255。面板比旧稿更高，字号、关闭按钮、主按钮更大，符合现主题放大；正文是不同章节。说明、继续读和重试无重叠。 |
| AiTimeout | 44.719／255。“什么都没有改”改成“没有收到结果，不会自动重试”，与现有服务能证明的事实一致，不应改回无依据承诺。 |
| AiBudget | 46.222／255。成人说明占两行，面板随内容增高；按钮完整。额度供应方及真实每日恢复仍未验证，报告已说明。 |
| AiLog 第 1 页 | 34.283／255。设计的灰色连组变为五张白色描边卡片；没有右侧查看／改回按钮与底部说明，分页条上移。长状态有省略号。 |
| AiReceipts 第 1 页 | 原稿 1448×1360，为多种回执组件总览；产品 684×1216，不能按整屏 MAE 比较。只剩状态和元数据，缺图标及操作按钮，对象显示 `synt...`，见 R1-04、R1-05。 |
| LogEmpty | 诚实空态“还没有 AI 做过的事”；仍显示 1／1 页码条，没有虚构内容。 |
| LogPage2 | 五条旧记录与 2／2 页码一致；旧页没有混入；状态层次仍靠两行文字。 |
| ReceiptsPage2 | 五种状态都在，操作对象仍被省略号截断，不能点开读全。 |
| LogLarge | 最大字号下仍为五行，第二行离下边框很近；长状态截断更明显，未发现挤到相邻卡片。 |
| ReceiptsLarge | “页面或家人已变，操作已停止”被截为“页面或家人已变，操作已…”；对象整段不可见，见 R1-04。 |
| LogChildEmpty | 儿童字号生效，空态可读，无成人记录或合成对象泄露。 |
| NewMemberEmpty | 有家人且存储正常，却显示“记录暂时读不了”，见 R1-03。 |

另查看了真实亮度回执和隐藏错误页后的阅读图，用于 R1-01、R1-04、R1-05。未发现新增文案错字；技术对象名直接出现在回执里不利于理解。旧稿的浅灰、字号不能作为缩小现行主题的理由；像素差超标也不直接等于高问题。

标准四页复算命令：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py \
  --build build --design "$INK_DESIGN" --out build/review-ai-pages-r1/out/ui-compare \
  --reuse --scenes AiOffline AiTimeout AiBudget AiLog
```

## 范围、架构与过度设计

- 【实测】相对基线仅 10 个文件，新增产品代码 185 行、删除 1 行；没有改其他并行页面、预算、历史资料、需求或决策。两处已有测试修改与新增应用／能力数量相符，没有放宽原有权限断言。规定隐私扫描零匹配，`git diff <基线>..HEAD --check` 通过。
- 【实现】活动数据沿用能力总线的成员集合，读取限定当前成员；LVGL 留在 UI；大列表缓冲位于 PSRAM；未增加常驻任务、文件格式、配置项、队列或后台重试。额度通过明确合成错误码接线，不是已验证的真实计费功能。
- 【实现】六项新导航能力均注册，触摸／按键／AI 经同一能力路径。主要 AI 原生缺口是隐藏页仍占据快照、读失败不在快照中反映，以及已有撤销能力未进入页面动作。
- 【实现】共享文件“只在文件末尾追加”的边界确有偏离：`ai_panel.c:157`、`ai_service.c:266`、`:359` 是函数内部接线，开发报告已披露；未扩展到无关功能。合并时应人工核对这些共享处，不能声称严格满足追加限制；不建议为字面追加规则引入包装层。
- **可删的重复机制：** 错误内容在已有 AI 面板里，却再次注册一套浮层所有权（R1-01）。虽然复用了现有 API，仍造成双重生命周期；删掉独立 `E.overlay`，由原面板统一管理即可。其余 F4、活动集合和能力表复用合理，没有证据要求新建通用框架。此处不另加问题计数。
- 原始回执细节、真实供应方额度、Linux／gcc 和实机均不在本轮实测证据内。没有把未验证项改写为已完成，也没有触碰设备、付费接口、真实消息或远端发布。

## 补充探针复现命令

[诊断源码](../../../build/review-ai-pages-r1/probe.c) 复用 `tests/ai_pages.c` 的现有启动、触摸和截图助手，仅替换场景主体；使用原 CMake flags 和 link 参数从标准输入编译，链接当前产品库。它不是提交的新测试或产品改动。

```sh
# 分别取 hide、newmember、visual、real；对应 R1-01、R1-03、R1-06、R1-04/05
REVIEW_CASE=hide
INK_AI_PAGES_CASE="$REVIEW_CASE" taskpolicy -c utility build/review-ai-pages-r1/sim-probe \
  --profile simulator/profiles/metalio-plus.json \
  --script "build/review-ai-pages-r1/$REVIEW_CASE/case.script" \
  --hub "build/review-ai-pages-r1/$REVIEW_CASE/hub.json" \
  --budget tests/ui_scene.budget.json --assets build/assets --content build/test-content \
  --out "build/review-ai-pages-r1/$REVIEW_CASE" --quiet --no-random-outages

# R1-02：使用原测试二进制；脚本为 @0 net wifi、@10s net none、@150s end
INK_AI_PAGES_CASE=offline taskpolicy -c utility build/sim-ai-pages \
  --profile simulator/profiles/metalio-plus.json \
  --script build/review-ai-pages-r1/lost/case.script --hub build/review-ai-pages-r1/lost/hub.json \
  --budget tests/ui_scene.budget.json --assets build/assets --content build/test-content \
  --out build/review-ai-pages-r1/lost --quiet --no-random-outages
```

每个目录包含 `run.log`、`metrics.json`、快照 JSON 与截图；以 `test.fail`、完成标记和实际图像共同判定，未把编译失败或未完成场景当作有效反例。

SEVERE=0 HIGH=0
