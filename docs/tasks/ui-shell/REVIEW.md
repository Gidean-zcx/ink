# ui-shell 第 2 轮复查

复查日期：2026-11-05。基线 `fdff779`，复查对象 `a429a48`。依据：[复查提示](../review-prompt.md)、[任务书](../ui-shell.md)、[总规则](../codex-rules.md)、[开发报告](REPORT.md)及任务指定的设计合同。

**结论：仍不满足整体验收。本轮确认 2 个高、4 个中问题，严重 0 个。** 上轮的确认卡、截图时点、取消手势和滑杆修复有独立测试及反向对照支持；尺寸门禁仍漏检，并新复现了跨成员搜索词残留和 AI 回答截行。普通及 ASan/UBSan 全量均为 **127/132**；30 屏视觉目标 **0/30** 达到。

本轮只修改本文件，不提交，不修改产品源码、永久测试、预算或合同。使用用户指定的 `INK_DEPS`、`INK_DESIGN`，依赖和设计稿只读。构建最多 `-j4`、ctest 最多 `-j3`，构建和全量测试串行；没有启动带窗口的 `ink-sim-gui`。临时验证只在内存及忽略的构建/输出目录产生文件。

证据标记：【实测】本次执行及产物；【实现】代码核查；【目视】本次实际打开的图片。开发报告已明确 U7、U8、U9、U10、U11 未完整通过，本轮不把这些已披露的欠项另判为“伪称整体验收通过”。

## 验收表

| 编号 | 结论 | 独立证据 | 命令/位置 |
| --- | --- | --- | --- |
| U1 | 未通过 | 【实测】现有门禁和自检通过，原反例能检出；简单重新赋值、未初始化后赋值、实际字号数组中插入 123 均漏检 | T、N、附录 A；UI-R2-001 |
| U2 | 部分完成 | 【实测/目视】Components、ComponentsDock、ComponentsPanel 均成功；已看三张图，组件齐备，但分段底边、Dock 圆边及页面组合外观仍有差异 | T；UI-R2-006 |
| U3 | 本机字体管线通过，体积超目标；Linux 未验 | 【实测/实现】三字重、规格字号、数字子集和回退存在；缺字负例明确拒绝生成。字体 7,530,752 字节，超过 4 MiB 目标 3,336,448 字节，目标未改 | B、T、附录 C；资源表 |
| U4 | 本机管线通过 | 【实测/实现】80 图标、240 SVG、1,600 位图；五尺寸、三外观及反白资源。缺少 ai-line.svg 的校验负例失败；CMake 对缺源文件有失败分支 | B、T、附录 C；CMakeLists.txt、tools/icongen/check.py |
| U5 | 本轮既定优先级检查通过 | 【实测】系统边缘、键盘、前景、滑杆及实体键检查通过；旧分派和旧滑杆实现均使回归失败 | T、N |
| U6 | 打断及恢复用例通过，成员残留需修 | 【实测】来电前后底页及确认页物理面板 PNG 一致，帧缓冲恢复断言通过；该结果不覆盖换成员后壳状态清理 | T、N；UI-R2-002 |
| U7 | 未通过 | 【实测/实现】两种主页配置仍超原延迟预算；控制中心和设置重建整块内容，稳定帧仍见残影 | T、V；UI-R2-004 |
| U8 | 30 屏入口通过，完整交互未完成 | 【实测/实现】页面身份和代表性动作已加强；AI 回答丢行、全局字号及部分必需按钮/流程仍缺 | T、V、附录 B；UI-R2-003、005 |
| U9 | 部分完成 | 【实测/实现】当前能力权限、密码隐藏、仅人确认和代表性服务结果有检查；键盘候选/选择、控制中心通知、Apps 可见项的快照不完整，成员搜索词泄漏 | T、N、附录 B；UI-R2-002、005 |
| U10 | 工具及取图修复有效；视觉目标未通过 | 【实测】重新生成全部 30 屏，未用缓存；0/30 ≤ 12/255。实际目视 15 张对照图 | V；逐屏表、UI-R2-006 |
| U11 | 未通过全量；已查 RAM 达标 | 【实测】普通及 sanitizer 均 127/132；AI 内部 RAM 总余量最低 33,192 ≥ 32,768；没有修改旧预算 | B、T；失败/资源表 |
| U12 | 无窗口验证通过 | 【实测】SDL dummy 的 gui-smoke 与主页三屏、控制中心、设置场景通过；没有窗口版验证 | T；tests/simulator/gui_smoke.cmake |
| U13 | 本机结束检查已执行；完整交付未通过 | 【实测】两套构建/全量、反向对照、视觉、范围、隐私及文件大小检查完成；完整基线有第三方空白告警，Ubuntu/设备端未验证 | B、T、N、V；结束检查 |

## 独立执行结果

以下命令从仓库根目录执行，环境变量取用户给定值。B/T 按顺序执行，没有同时运行两轮构建或全量测试。

~~~sh
# B/T：普通构建及全量
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
# B/T：ASan/UBSan 构建及全量
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
# N：串行编译内存副本作反向对照，不修改产品
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_reverse.py
# V：重新运行全部 30 屏，不用 --reuse
"$INK_DEPS/build-deps/pyenv/bin/python" -B tools/ui_compare.py
~~~

- 【实测】两套构建退出 0；两套全量各 127/132，退出 8，用时分别 31.81、74.10 秒。缓存配置均为 `INK_WERROR=ON`，第二套 `INK_SANITIZE=ON`；CJK、Python、LVGL 路径符合指定依赖。此次为已有目录的增量构建，不宣称全新 Linux 环境可重建。
- 【实测】两份 `Testing/Temporary/LastTest.log` 没有 AddressSanitizer、LeakSanitizer、UndefinedBehaviorSanitizer 或 `runtime error:` 诊断；五项失败如下。无 sanitizer 诊断不等于全量通过。
- 【实测】`integration-confirm` 两套均 116 个断言通过；两档案确认、滑杆、上下文回归分别各 65、145、34 个通过。原失败的 `integration-dictation` 两套均 218 个通过；没有进一步证明在途帧/新收音边界，不将偶然转绿写为根因已修。
- 【实测】N 退出 0：旧尺寸检查器在新自检中失败 21 项；撤掉 Settings 准备失败 5 项；待呈现帧正例通过，撤掉等待失败 1 项；旧确认卡失败 18 项，旧说明截断、重叠分页分别失败 2、1 项；旧能力清单失败 1 项；旧手势分派失败 9 项；旧滑杆映射/触摸拦截分别失败 30、60 项；旧上下文/总览分别失败 2、7 项。退出 0 的含义是脚本检出了预期故障。
- 【实测】V 退出 0，说明生成了 30 张已验证稳定帧的对照图；工具退出码不表示 MAE 达标。分数与开发报告一致。

| 两套均失败的测试 | 本次实际结果 | 判断 |
| --- | --- | --- |
| scenario-home_basic-metalio-plus | 首变化 p95 431.873 > 400 ms；FAST 686.587 > 600；TEXT 1928.213 > 1,200 | U7/U11 仍未通过，见 UI-R2-004 |
| scenario-home_basic-metalio-plus-psram16 | TEXT p95 1707.390 > 1,200 ms | 更快内存配置仍超预算 |
| integration-listening | 42 通过、1 失败；tests/integration/listening.c:97，锁定后等待 70 秒，睡眠计数未增长 | 根因未定位，不能从这一条断言归因音频或 UI；不增加等待掩盖 |
| integration-notify | 177 通过、1 失败；tests/integration/notify.c:657，提醒已存在时提示音计数未增长 | 不能据此断言最终永远无声；须核查发布与播放时序 |
| scenario-reader_basic-panel-power-faults | display.power_failures=1，上限为 0 | 固定故障注入仍失败；不修改预算或范围外驱动 |

### 资源

【实测】读取本轮普通测试的 metrics.json。下列“内部”使用预算实际采用的 `mem.internal_total.min_free_bytes`，不是单独内部堆分区的余量；数据来自模拟器注解模型。

| 场景 | 内部最低余量/字节 | DMA 最低余量/字节 | PSRAM 最低余量/字节 |
| --- | ---: | ---: | ---: |
| ai_ask-metalio-plus | 35,856 | 31,328 | 10,976,112 |
| home_basic-metalio-plus | 48,816 | 48,752 | 12,361,288 |
| ai_tools-metalio-plus | 34,912 | 31,328 | 10,976,136 |
| ai_ask-metalio-plus-pqbn | 33,192 | 28,616 | 10,121,304 |

最低已查 AI 余量只高于门槛 424 字节。未重新构建基线，开发报告中的基线→当前比较仅核对了当前一侧。

【实测】`build/assets/fonts/*.inkf` 共 29 个、7,530,752 字节；图标 1,600 个、20,308,480 字节；合计 27,839,232 字节。字体比 4 MiB 目标多 3,336,448 字节。没有目标硬件链接、分区容纳或实机峰值证据。Outfit/Noto/NanoSVG 的许可与固定来源记录存在；本轮使用本地资源，没有联网重验来源。

## 问题

### UI-R2-001（高）：尺寸门禁仍漏掉简单赋值和实际静态字号数组

- 文件：`tools/ui_scale_check.py:77`、`:87`、`:153`、`:170`；实际调用 `system/ui/shell/navigation.c:539`。
- 复现：运行附录 A。该脚本只替换检查器读到的内存文本，不修改文件。
- 【实测】直接 `lv_obj_set_width(o,123)` 报错；`int w=104; w=123; lv_obj_set_width(o,w);`、`int w; w=123; ...`、`static const int sizes[]={32,123}; ink_ui_font_px(sizes[index],...);` 均返回空错误列表。把真实 navigation.c 的字号表第一个成员换成 123，仍返回空列表。
- 原因/影响：【实现】符号收集只看声明初值，不跟踪简单赋值；数组候选值及下标表达式未解析，无法求值时放行。原四个反例虽已修，U1 的非法值门禁仍漏掉项目实际采用的写法；现有测试也全绿。
- 修法：补简单赋值和静态尺寸表候选值检查；对不能静态判定的尺寸要求明确的主题类型/枚举或有理由的例外，避免将动态表达式一律视为合法。加入上述负例。这不要求通用 C 数据流分析。

### UI-R2-002（高）：换到儿童成员后仍显示成人搜索词，并进入儿童的 AI 快照

- 文件：`system/ui/shell/navigation.c:42`、`:260`、`:302`、`:330`、`:364`、`:687`；重启入口 `system/ui/app_registry.c:274`。
- 复现：运行附录 B 的 `r2-privacy` 分支；成人通过触摸来源能力搜索合成字符串 `PRIVATE_OWNER_TEST_QUERY`，打开实际成员选择器，再用模拟触摸点击“小”切到儿童。
- 【实测】真实当前成员已是 `kid`；儿童屏幕与 `ink_ui_snapshot` 都含成人搜索词。探针 9 通过、2 失败，进程退出 1；`review.child.query_in_snapshot`、`review.child.query_visible` 均为 1。已打开[实际截图](../../../build/test-out/r2-privacy/snap-child-query.png)核对。
- 原因/影响：【实现】成员变化会重启界面，但 home_destroy 只清对象指针；全局 N.search 和主页位置被下一成员复用。成员身份/权限本身已经切换，泄漏的是上一成员的输入状态；未据此声称儿童解锁了成人数据库或完整空间。现有成员权限回归未覆盖这种壳状态残留。
- 修法：按成员隔离或在成员重启边界清除搜索词、结果提示和选中项；异步结果也绑定原成员，避免回调重新污染新空间。增加“搜索→实际成员选择→儿童屏幕/快照均无旧词”的回归，兼顾正常返回主页时保留本人成员状态。

### UI-R2-003（中）：AI 回答按字符数分页，换行内容会被裁掉且无法翻到

- 文件：`system/ui/shell/ai_panel.c:12`、`:44`、`:101`、`:119`、`:128`。
- 复现：运行附录 B 的 `r2-answer` 分支，用正式 presenter 接口显示“第一行\n第二行\n第三行\n第四行\n第五行”，标为回答完成，再调用与按钮相同的 `ai.panel.next`。
- 【实测】LVGL 计算所需高度 260 px，答案框只有 156 px；屏幕只露出前三行。全文仅 19 个码点，未满固定的 54 字符，下一页动作虽返回 DONE，页码和快照内容都不变，后两行无法读到。探针 5 通过、2 失败，进程退出 1；已打开[实际截图](../../../build/test-out/r2-answer/snap-answer-lines.png)。
- 边界：这是实际面板接口上的确定性合成回答，未调用在线模型；证明呈现内容不可达，不等于持久化记录被删除。
- 修法：按真实字体、宽高和换行计算页界，复用确认卡已有的实际布局思路；给出可用的前后翻页和页数。测试多行短文本、长段落及混合字符，确保完整遍历后文字不缺不重。

### UI-R2-004（中）：整块重绘和刷新时序仍超原预算，稳定帧仍有残影

- 文件：`system/ui/shell/navigation.c:435`、`:466`、`:508`、`:617`；`system/services/display/display_service.c:95`、`:107`、`:151`。
- 复现：执行 T 的普通全量（或单独选择两个 home_basic 测试），再执行 V，打开 Today/Apps 对照图。
- 【实测】主页两配置超预算的具体数值见失败表；在已等待实际刷新完成的 Today/Apps 图片中，仍可见旧主屏的大时间和图标残影。不是本轮取图过早。
- 【实现】控制中心每次 draw_control 都 clean 整层并标记 FLASH_UI，设置页 clean 整块 page_body 后标记 UI；这些更新不满足按控件变动区域局部反馈的要求。两阶段接口已存在，不能只凭 preview/gray 计数就判定每段延迟和矩形都合格；当前证据不足以把全部延迟唯一归因于某一行调度代码。
- 修法：保留对象并按实际变化更新，核对反馈、黑白预览和补灰的脏矩形/队列/波形；对快更新和模态分别验证。补时序及区域证据后在原预算下验收，不抬预算、不用等待遮盖、不把仍含灰的旧页盲换成 DU。

### UI-R2-005（中）：必需动作和可见状态仍未完整开放给 AI

- 文件：`system/ui/shell/navigation.c:364`、`:383`、`:539`、`:558`、`:563`；`system/ui/shell/keyboard.c:117`；`system/ui/shell/ai_panel.c:104`、`:126`。
- 复现：执行 V 查看 Control、SetDisplay、SetSound、Ai、ImeCandidates；代码核查命令：`rg -n 'display.font_size|control_snapshot|static int snapshot|ai.panel|还没有可选设置' system/ui/shell`。
- 【实现/目视】字号设置被保存并改变示例，壳的其他字号未随之变化；Ai 的主动作仍是“继续问/下一页”，规范要求的“听解释/更多→讲细一点、记下来、对话记录”未接入；SetSound 的提示音、振动及铃声仍为只读行。已有亮度/音量/暖光、网络、自动锁屏的补充有效，不能据此认定全部交互完成。
- 【实现】Control 画出具体通知卡，但快照只报未读数量；键盘快照无候选列表及对应序号、无选区，AI 无法据当前画面可靠选词；Apps 实际补充的工具项/分页与只枚举注册应用的快照来源不同。现有“有非空快照/动作已注册/代表性操作成功”不能检出这些语义缺口。
- 修法：在本次 30 屏范围内补合同要求的真实能力与状态；由同一结构生成可见列表和快照，输出稳定 ID、选择状态和当前页可用动作。用有通知、有候选、多页工具项等夹具逐一对照画面/快照及 touch/AI 结果。服务缺数据保持明确空状态，不造示例业务数据，也不扩展到任务排除的其他应用画板。

### UI-R2-006（中）：页面结构、控件组合和文案仍明显偏离设计

- 文件：`system/ui/theme.c:193`、`:218`、`:228`；`system/ui/shell/navigation.c:258`、`:314`、`:435`、`:508`；`system/ui/shell/keyboard.c:178`；`system/ui/shell/interrupt_card.c:51`、`:57`。
- 复现：执行 V，打开 `out/ui-compare/<页面>.png`；下表标“已目视”的 15 屏均在本轮打开核对。组件截图另见 `build/test-out/ui-scene-Components*/snap-scene.png`。
- 【实测/目视】0/30 达到 12/255。具体可见问题包括候选区改变整个键盘区高度、分组列表层次/分隔线缺失，以及 ClockRing 将闹钟标题“起床”放进大数字时间位置，既没有应有的时间/日期层次，又触发大字号中文回退。Main 保留布局 A，ConfirmDelete 的按钮已正确写“删除”，这些不再沿用旧结论。
- 修法：先统一公共组件的边线、内边距和分组层次，再逐屏校对；闹钟按真实时间/标题分字段，中文标题用合同中文字级，明确“再睡 10 分钟/这次不再响”等行为。规范优先于旧图，不为分数添加示意人名/号码/通知或换回已被规范否定的尺寸。
- 此项覆盖外观和信息布局；缺失能力见 UI-R2-005，残影/延迟见 UI-R2-004，不把同一根因重复计数。

## 逐屏图像复查

已先看原稿 Foundation、Components、Main、Today、Control、Settings；另看当前三张组件总览。下表的 15 张对照图为本轮实际目视，其余只核查场景通过和分数，不冒称全部目视。所有分数取同一次 V 的 `out/ui-compare/scores.json`，单位为 1/4 灰度 MAE/255，目标均为 ≤12。

设计中的人名、书名、号码、通知及联网结果是示意，真实服务的空状态不算错误；阅读底页/测试输入宿主不在本任务的产品页面范围。规范中的 160 px 大圆等优先于旧图尺寸，不要求恢复旧图 240 px 头像或 176 px 动作圆。

| 页面 | MAE | 本轮目视结论 |
| --- | ---: | --- |
| Main | 15.345 | 已目视：布局 A 保留；时间字形比例不同，页点更小/重，Dock 内部圆边仍可见；没有阅读进度属有效空状态 |
| Today | 24.656 | 已目视：页头缺右侧日期，卡片标题“听/报纸”偏离“继续听/今日报纸”；旧大时间和图标残影明显 |
| Apps | 15.673 | 已目视：搜索框用白底描边、缺放大镜，分组标题层次较重；稳定帧留有上一页残影 |
| Lock | 17.630 | 未逐图目视；已重新生成并核查分数 |
| LockPage | 37.596 | 未逐图目视；已重新生成并核查分数 |
| Control | 23.454 | 已目视：滑杆缺太阳/音量图标，网络行变成描边胶囊，底部多一个大“收起”按钮，层次偏离 |
| Notifications | 20.829 | 未逐图目视；已重新生成并核查分数 |
| Members | 18.197 | 未逐图目视；成员切换另用实际触摸探针验证 |
| MembersBack | 20.701 | 未逐图目视；已重新生成并核查分数 |
| Pin | 19.939 | 未逐图目视；已重新生成并核查分数 |
| PinWrong | 18.638 | 已目视：缺成员名、密码槽和“忘记密码”入口；底部“删除/确定”与取消/删除的布局不同；未要求更改既有 PIN 策略 |
| ImePinyin | 32.647 | 已目视：第二行未错位，“大写/删除”用文字替代图形，缺中英切换和中文逗号/句号；输入宿主本身不作为备忘产品缺陷 |
| ImeCandidates | 27.239 | 已目视：候选面板缩到下部，未保持键盘区高度和候选网格/页数；旧键盘残影仍在。真实候选数量少本身不算缺陷 |
| ImeEdit | 33.666 | 未逐图目视；已重新生成并核查分数 |
| ImePassword | 29.552 | 未逐图目视；密码拒绝路径已有动态测试 |
| ImeVoice | 29.947 | 未逐图目视；已重新生成并核查分数 |
| Confirm | 24.931 | 未逐图目视；确认内容/分页另有本轮回归与反向对照 |
| ConfirmDelete | 26.084 | 已目视：“删除”已改对；仍用 AI 星形而非删除图形，后果说明层次和面板高度不同 |
| ConfirmSecret | 27.869 | 未逐图目视；已重新生成并核查分数 |
| AiListen | 33.584 | 未逐图目视；已重新生成并核查分数 |
| AiThink | 32.902 | 未逐图目视；已重新生成并核查分数 |
| Ai | 37.224 | 已目视：“继续问/下一页”未对应“听解释/更多”，缺来源书形标记；合成回答及阅读底页差异不用于认定产品造假 |
| Settings | 16.222 | 已目视：缺搜索入口；家庭卡只剩居中名字，缺头像和成员信息；分组行缺分隔层次。亮度/音量等当前值已修 |
| SetDisplay | 19.077 | 已目视：滑杆缺图标；“屏幕”分组变成两个胶囊，缺“有残影时用”等辅助层次；分段轮廓与内边距不同 |
| SetSound | 21.176 | 已目视：滑杆无图标；勿扰脱离提示分组；提示音/振动无开关，“当前”文字替代路由勾选状态 |
| SetNetwork | 35.519 | 已目视：缺 Wi-Fi 开关和已知网络分组，以扫描胶囊替代；SIM 部分上移。无联网数据的空状态保留合理 |
| PowerMenu | 27.167 | 未逐图目视；已重新生成并核查分数 |
| PowerOff | 16.877 | 未逐图目视；已重新生成并核查分数 |
| PhoneIncoming | 19.966 | 已目视：缺快捷回复行和完整来电信息层次；合成号码不能冒充联系人，当前 160 px 圆不按旧图认错 |
| ClockRing | 22.367 | 已目视：大时间位置显示像素化“起床”，缺时间/日期；“关闭/稍后”未说明关闭本次及延后时长 |

## 上轮问题追踪

| 上轮编号 | 本轮判断 |
| --- | --- |
| UI-R1-001 | 原“未真正进入目标屏”漏洞已修，撤掉 Settings 入口会失败；更广的语义完整性仍见 UI-R2-005 |
| UI-R1-002 | 原反例已修，但简单赋值和真实字号数组仍漏检，续为 UI-R2-001 |
| UI-R1-003 | 已验证原确认卡/说明截断修复；全部必读页呈现前禁确认、AI 不可代翻、超容量拒绝的回归通过，反向会失败 |
| UI-R1-004 | 已验证稳定取图及无缓存回退修复；当前残影不能再解释为旧截图时点问题 |
| UI-R1-005 | 已验证取消和系统手势修复；旧分派失败 |
| UI-R1-006 | 已验证端点及覆盖层触摸修复；旧实现失败 |
| UI-R1-007 | 仍在，见 UI-R2-004 |
| UI-R1-008 | 当前值/网络等部分已修；余项见 UI-R2-005 |
| UI-R1-009 | 仍在，按新截图重新列证，见 UI-R2-006 |

## 范围、既有测试和架构审视

- 【实现】主要职责为 theme/组件、navigation/presenter、统一 overlay、能力表和服务状态；按钮的 `ink_shell_bind` 经能力调用总线，AI 使用同一能力路由。未见新页面绕开服务写内部持久化数据。但 navigation 的成员无关静态状态破坏隔离，快照/画面分别手写也已发生偏差，分别见 UI-R2-002、005。
- 【实现】服务/策略增量包括显示分阶段提交、AI 面板接入及触摸收音、能力列表按当前成员/动作页过滤、确认说明超容量拒绝；未见驱动、硬件、产品模拟器主体或任务排除应用的功能扩张。字体/图标工具及许可证属于任务范围。
- 【实现】核查基线差异：旧预算 JSON 无修改，只有新 UI 预算文件。已有阅读场景改 Dock 入口；Apps 用真实目录断言替代旧计数器；键盘/电源查找改统一系统层；确认的旧“截断仍排队”断言改成明确拒绝并新增执行/呈现检查。这些调整与删除旧壳、修确认缺陷对应，未发现删原失败断言以变绿。
- 【实测/实现】standby 的 1,180→1,420 ms 注入时点已披露；本轮原 `power.sleep_cancelled` 为 1，轨迹的首次休眠准备在约 30.860 s，仍触发原竞争检查，预算没有放宽。未将其当作新增 sleep 掩盖延迟。确认/截图等待有界并检查真实 busy 状态，撤掉等待的反向例会失败。
- 【边界】本轮不修五项全量失败，不扩展到其他画板，不改规范或资源目标；这些是待开发处理的欠项，不需要通过产品决策将其重新定义成“无需实现”。

## 结束检查

【实测】`git diff --check cab5c46..HEAD`、本轮工作区差异检查退出 0。完整基线 `git diff --check fdff779..HEAD` 退出 2：Outfit-OFL.txt 尾空格、NanoSVG LICENSE.txt 的 CRLF/尾空行、nanosvgrast.h:782 缩进告警；未修改第三方原文来消除告警。

【实测】基线差异按总规则的本机路径/MAC 模式扫描 0 命中；没有变更文件超过 5 MiB，最大为 Outfit.ttf（110,884 字节）。所有新探针、截图、位图、字体和日志留在忽略目录；本轮唯一受版本控制的改动为本 REVIEW.md。没有硬件、串口、付费接口、真实凭据、远端写入或提交操作。

【边界】本轮没跑 Ubuntu/CI、没运行窗口 GUI、没做设备刷写/实机测量，也没重建基线资源比较；不把这些写成通过。

## 附录 A：尺寸门禁新负例

以下命令本次已执行；输出 direct 有错误，其余四项为空列表。

~~~sh
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
from unittest.mock import patch
import runpy
check = runpy.run_path('tools/ui_scale_check.py')['check']
cases = {
    'direct': 'void f(void) { lv_obj_set_width(o,123); }',
    'reassign': 'void f(void) { int w=104; w=123; lv_obj_set_width(o,w); }',
    'assign': 'void f(void) { int w; w=123; lv_obj_set_width(o,w); }',
    'table': 'void f(void) { static const int sizes[]={32,123}; ink_ui_font_px(sizes[index],INK_WEIGHT_REGULAR); }',
}
for name, source in cases.items():
    with patch.object(Path, 'read_text', return_value=source):
        print(name, check(Path('probe.c')))
p = Path('system/ui/shell/navigation.c')
source = p.read_text().replace('static const int px[] = {INK_FONT_LIST,', 'static const int px[] = {123,', 1)
with patch.object(Path, 'read_text', return_value=source):
    print('real navigation table mutated', check(p))
PY
~~~

## 附录 B：成员残留和 AI 回答裁切探针

以下为本次实际使用的内存编译方法，依赖已构建的普通 build。只在原回归测试的内存副本增加检查，复用原启动、服务、成员、触摸、LVGL 及预算；不写源文件。两个分支各起一个独立模拟器进程，退出 1 是复现缺陷的预期结果；编译和链接均退出 0。

~~~sh
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
import subprocess, shlex, json, os
root=Path.cwd(); build=root/'build'; target='sim-ui-regressions'
src=Path('tests/ui_shell_regressions.c').read_text()
probe=r'''
static void r2_snapshot(void *name) { sim_display_snapshot(name); }
static void r2_photo(const char *name) { quiet(); vtk_event_at(vtk_now()+SIM_MS(1),r2_snapshot,(void *)name); delay_ms(5); }
static void r2_privacy(void) {
    CHECK(BY_HAND("shell.search", "{\"query\":\"PRIVATE_OWNER_TEST_QUERY\"}") && g_rc.status==INK_RC_DONE, "owner submits a synthetic app query"); quiet();
    CHECK(BY_HAND("shell.members", NULL) && g_rc.status==INK_RC_DONE, "open the real member picker"); quiet();
    wanted="小"; on_ui(read_view); CHECK(label_x>=0,"find the child's avatar"); tap(label_x,label_y); quiet(); on_ui(read_context);
    CHECK(!strcmp(ink_member_current()->id,"kid"),"the touch switched to the child");
    CHECK(!strstr(context,"PRIVATE_OWNER_TEST_QUERY"),"the child snapshot must not contain the adult query");
    wanted="PRIVATE_OWNER_TEST_QUERY"; on_ui(read_view);
    CHECK(label_x<0,"the child screen must not display the adult query");
    ink_probe_value("review.child.query_in_snapshot",strstr(context,"PRIVATE_OWNER_TEST_QUERY")!=NULL);
    ink_probe_value("review.child.query_visible",label_x>=0);
    r2_photo("child-query");
}
static lv_obj_t *r2_root,*r2_status,*r2_question,*r2_answer;
static const char *r2_words="第一行\n第二行\n第三行\n第四行\n第五行";
static void r2_open_answer(void) {
    ink_shell_ai_open(&r2_root,&r2_status,&r2_question,&r2_answer);
    ink_shell_ai_words(r2_words); ink_shell_ai_status("回答完成");
}
static void r2_check_answer(void) {
    lv_obj_update_layout(r2_answer); lv_point_t size;
    const char *text=lv_label_get_text(r2_answer);
    lv_text_get_size(&size,text,lv_obj_get_style_text_font(r2_answer,0),lv_obj_get_style_text_letter_space(r2_answer,0),lv_obj_get_style_text_line_space(r2_answer,0),lv_obj_get_width(r2_answer),LV_TEXT_FLAG_NONE);
    ink_probe_value("review.answer.required_height",size.y); ink_probe_value("review.answer.height",lv_obj_get_height(r2_answer));
    CHECK(size.y<=lv_obj_get_height(r2_answer),"every displayed answer line must fit: need %d, have %d",size.y,lv_obj_get_height(r2_answer));
}
static void r2_answer_test(void) {
    on_ui(r2_open_answer); quiet(); on_ui(r2_check_answer); r2_photo("answer-lines");
    CHECK(BY_HAND("ai.panel.next",NULL) && g_rc.status==INK_RC_DONE,"request answer next page"); quiet();
    on_ui(read_context); CHECK(!strstr(context,r2_words),"next page must reveal the clipped lines");
}
'''
src=src.replace('static void test_task(void *unused) {',probe+'\nstatic void test_task(void *unused) {',1).replace('if (!strcmp(mode, "slider")) slider_tests();','if (!strcmp(mode, "r2-privacy")) r2_privacy();\n    else if (!strcmp(mode, "r2-answer")) r2_answer_test();\n    else if (!strcmp(mode, "slider")) slider_tests();',1)
flags=[]
for ln in (build/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
    if ln.startswith(('C_DEFINES =','C_INCLUDES =','C_FLAGS =')): flags+=shlex.split(ln.split('=',1)[1])
link=shlex.split((build/f'CMakeFiles/{target}.dir/link.txt').read_text())
out=build/'review-r2';out.mkdir(exist_ok=True)
obj=out/'probes.o'; exe=out/'probes'
cp=subprocess.run([link[0],*flags,'-I'+str(root/'tests'),'-x','c','-c','-','-o',str(obj)],input=src,text=True)
print('compile',cp.returncode)
if cp.returncode: raise SystemExit(cp.returncode)
link=[str(obj) if x==f'CMakeFiles/{target}.dir/tests/ui_shell_regressions.c.o' else str(exe) if x==target else x for x in link]
subprocess.run(link,cwd=build,check=True)
for mode in ['r2-privacy','r2-answer']:
    folder=build/'test-out'/mode
    cp=subprocess.run([str(exe),'--profile','simulator/profiles/metalio-plus.json','--script','tests/ui_shell_regressions.script','--assets','build/assets','--content','build/test-content','--budget','tests/ui_scene.budget.json','--out',str(folder),'--quiet','--no-random-outages'],env=dict(os.environ,INK_UI_REGRESSION=mode),capture_output=True,text=True)
    print(mode,'exit',cp.returncode)
    if not (folder/'metrics.json').exists(): print(cp.stdout,cp.stderr); continue
    m=json.loads((folder/'metrics.json').read_text()); print({k:v for k,v in m['metrics'].items() if k.startswith(('review.','test.'))}); print(m.get('warnings',[]))

PY
~~~

## 附录 C：缺图和缺字校验负例

本次已执行以下等价内存负例：缺图校验退出 1；缺字明确报“refusing an incomplete UI font”，且没有生成输出文件。未删除或替换真实资源。

~~~sh
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
from unittest.mock import patch
import runpy, sys, os
original = Path.exists
with patch.object(Path, 'exists', lambda p: False if p.name == 'ai-line.svg' else original(p)):
    try:
        runpy.run_path('tools/icongen/check.py', run_name='__main__')
    except SystemExit as e:
        print('missing icon exit', int(e.code))
g = runpy.run_path('tools/fontgen/fontgen.py')
fn = g['main']
font = Path(os.environ['INK_DEPS']) / 'build-deps/fonts/NotoSansCJK-Regular.ttc'
args = ['fontgen.py', '--font', str(font), '--face', '2', '--px', '32',
        '--require-coverage', '-o', 'build/review-r2/missing.inkf']
with patch.object(sys, 'argv', args), patch.dict(fn.__globals__, {'charset': lambda spec: [0x10ffff]}):
    try:
        fn()
    except SystemExit as e:
        print(str(e).replace(str(font), '$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc'))
print('artifact exists', Path('build/review-r2/missing.inkf').exists())
PY
~~~

SEVERE=0 HIGH=2
