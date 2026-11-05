# ui-shell 第 1 轮复查

复查日期：2026-11-05。基线 `fdff779`，复查对象 `8e7f98f`（`claude/codex-ui-shell`）。

**结论：整体验收未通过。严重 0 项，高 4 项，中 5 项。** 新发现的阻塞项包括：确认卡裁掉部分删除说明后仍可执行，逐屏测试没有核对页面身份，尺寸门禁漏检，视觉对照在刷新未完成时取图。开发报告已明确承认全量失败、交互欠项和视觉超标，本轮没有据此认定“未做却伪报全通过”。

本轮只写本文件，不修改产品源码、原测试、预算和任务合同，不提交。普通构建、全量测试、sanitizer 构建和全量测试依次执行；构建使用 `-j4`，ctest 使用 `-j3`，没有启动带窗口的 GUI。临时验证代码只在内存中编译，产物进入已忽略的 `build/`；对照图进入已忽略的 `out/`。依赖和设计稿使用用户给定的 `INK_DEPS`、`INK_DESIGN`。

## 验收表

证据分为【实测】本轮命令结果、【实现】代码核查、【目视】实际打开图片。以下命令均在仓库根目录执行；完整全量命令和新问题的复现代码见文末。

| 项 | 结论 | 本轮证据 | 命令或证据位置 |
| --- | --- | --- | --- |
| U1 | 未通过门禁有效性复查 | 【实测】现有检查绿，但常量变量、字体函数和 `123 * 1` 均绕过；见 002 | `tools/ui_scale_check.py`；附录 A |
| U2 | 组件存在，外观未全部对齐 | 【实测/目视】3 个组件场景通过，打开 Components 原稿和组件概览渲染；滑杆、分段、列表等仍有差异 | 全量内 `ui-scene-Components*`；009 |
| U3 | 本机管线可用；体积超目标，CI 未验证 | 【实测/实现】三字重、规范字号、Outfit 数字子集、回退及 OFL 已接入；缺字反向检查退出 1；字体 7,508,102 字节，超 4 MiB 目标 3,313,798 字节 | 两套构建、`ui-assets`；资源核查段 |
| U4 | 本机管线通过 | 【实测/实现】80 图标、240 SVG、三画法五尺寸和反色资产；检查器模拟缺失 `ai-line.svg` 后退出 1，正常构建通过；本轮没有移动原资源 | 两套构建、`tools/icongen/check.py`、构建资源检查目标 |
| U5 | 部分通过 | 【实测】现有优先级用例通过；确认卡底边上滑、左边返回都被吞，见 005 | 全量内 `ui-shell-priorities`；附录 C |
| U6 | 既有层级和恢复用例通过；交互仍有缺陷 | 【实测】`ui-panel-restore` 的物理面板及帧缓冲恢复检查通过，来电隔离用例通过；确认卡存在 003、005，不能将当前结果泛化为全部浮层验收 | 全量内 `ui-panel-restore`、`ui-shell-priorities` |
| U7 | 未通过 | 【实现/实测】有两阶段实现，但设置/控制中心重建大区域，主页两项配置延迟超预算，见 007 | 全量内 `scenario-home_basic*`；显示服务和导航代码 |
| U8 | 部分实现，未完成 | 【实测/实现】30 个场景可运行，但测试不证明目标屏正确；设置、AI、成员等交互有已承认欠项，见 001、008、009 | 全量内 `ui-scene-*`；30 屏对照 |
| U9 | 未通过完整语义验收 | 【实测/实现】密码遮断和能力表测试通过；快照缺当前设置值、今日卡片等字段；逐屏检查可在错误页面上通过，见 001、008 | `integration-ai-surface`、`integration-text-main`、`ui-scene-*` |
| U10 | 工具可运行；取图不可靠，视觉目标未达 | 【实测】重跑 30 屏，0/30 达到 12/255；Apps/Today 在 display busy 时取图，见 004；本轮目视 20 屏，见 009 | `"$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --design "$INK_DESIGN"` |
| U11 | 未通过全量；当前 RAM 达标 | 【实测】普通和 ASan/UBSan 均 119/124，退出 8；AI 内部 RAM 最低 33,192 ≥ 32,768；旧预算没有上调 | 两轮全量、预算与基线差异检查 |
| U12 | 无窗口验证通过 | 【实测】`gui-smoke` 使用 SDL dummy；主页三屏、控制中心和设置的专项检查通过。001 表明单独的逐屏绿灯不足为证，但专项交互检查提供了补充证据 | 全量内 `gui-smoke`、`ui-shell-priorities` |
| U13 | 本机结束检查已核对；跨平台未验证 | 【实测】两套构建/全量、差异、隐私和文件大小检查完成；没有 Ubuntu 执行证据。复查不提交 | 结束检查段 |

## 独立执行结果与边界

【实测】普通构建退出 0；全量 119/124，退出 8，用时 31.93 秒。ASan/UBSan 构建退出 0；全量同为 119/124，退出 8，用时 78.62 秒。两套缓存均 `INK_WERROR=ON`，sanitizer 构建 `INK_SANITIZE=ON`。两份 `Testing/Temporary/LastTest.log` 未检出 AddressSanitizer、LeakSanitizer、UBSan 或 `runtime error` 诊断。

| 两套均失败的测试 | 本轮实际结果 | 可以得出的结论及限制 |
| --- | --- | --- |
| `scenario-home_basic-metalio-plus` | 首变化 p95 431.923 > 400 ms；FAST 686.637 > 600 ms；TEXT 1,928.264 > 1,200 ms | 当前实现未达旧延迟预算；见 007 |
| `scenario-home_basic-metalio-plus-psram16` | TEXT p95 1,707.426 > 1,200 ms | 较快内存配置也未达预算 |
| `integration-dictation` | `tests/integration/dictation.c:529`，来电中断后 ASR 帧计数多 1，`test.fail=1` | 不能据此认定通话内容已泄漏；本轮没有区分在途帧和新采集帧，隐私边界仍需时间戳验证 |
| `integration-notify` | `tests/integration/notify.c:657`，提醒入库后立即查提示音计数未增加，`test.fail=1` | 入库与提示音观测顺序不满足原断言；尚不能认定最终永远无声，不能靠宽松等待直接判通过 |
| `scenario-reader_basic-panel-power-faults` | `display.power_failures=1`，预算上限 0 | 固定故障注入下出现未恢复的电源更新失败；本轮未定位到驱动根因 |

现有 `layering`、`sdk-boundary`、确定性、输入/成员相关检查及 UI 新增检查均包含在以上全量中。没有观察到孩子进入成人空间或 AI 代替人确认的成功路径，但这不是对所有状态组合的安全证明。确认卡可见内容与执行对象不一致是本轮另外发现的问题，见 003。

【实测】重新统计资源：字体 7,508,102 字节，图标位图 20,308,480 字节，合计 27,816,582 字节。字体比 4 MiB（4,194,304 字节）目标超 3,313,798 字节，没有调高目标。本轮未验证设备分区能否容纳，也未做实机或目标链接内存验收。

| 当前普通构建场景 | 内部 RAM 最低余量 | DMA 最低余量 | 外部 RAM 最低余量 |
| --- | ---: | ---: | ---: |
| AI ask / Plus | 35,856 | 31,328 | 10,976,112 |
| AI tools / Plus | 34,912 | 31,328 | 10,976,136 |
| AI ask / PQBN | 33,192 | 28,616 | 10,121,280 |
| Home / Plus | 48,816 | 48,752 | 12,361,288 |

这些是模拟器注解模型数据，最低内部余量仅比要求多 424 字节。开发报告的当前值可复现；本轮没有重新构建 `fdff779`，所以不把报告中的基线数值标为本轮独立实测。

## 问题

### UI-R1-001（高）：30 屏测试没有验证实际进入了目标页面

- 文件：`tests/ui_shell_scenes.c:93`、`:125`、`:149`、`:176`。
- 【实测】只在内存中的测试副本让 Settings 的 `prepare` 提前返回，原产品源码、原测试与预算均未改。结果仍退出 0，`test.pass=9`、`test.finished=1`、无警告，而探针确认 `review.page.home=1`。Settings 没打开也通过。
- 复现命令：附录 B 的 `skip-settings` 模式；或运行其生成的 `INK_UI_SCENE=Settings build/review-ui-scenes --profile simulator/profiles/metalio-plus.json --script tests/ui_shell.script --assets build/assets --content build/test-content --budget tests/ui_scene.budget.json --out build/test-out/review-skip-settings --quiet --no-random-outages`。
- 影响：非空快照、全局能力存在和动作字符串已注册，不能证明 U8/U9 的目标页面、字段、权限与实际可达性。
- 修法：逐屏断言页面/浮层身份、关键字段及当前权限下动作，再触发必要按钮核对服务结果。加入撤掉页面入口就失败的反向对照。单有 `ink_cap_find` 不等于当前用户能调用。

### UI-R1-002（高）：尺寸检查漏掉字体尺寸，并可被常量或简单表达式绕过

- 文件：`tools/ui_scale_check.py:12`、`:25`、`:33`。
- 【实测】`lv_obj_set_width(o, 123);` 正确失败；`const int width=123; lv_obj_set_width(o,width);`、`ink_ui_font_px(123,INK_WEIGHT_REGULAR);`、`lv_obj_set_width(o,123 * 1);` 均零违规。
- 复现命令：附录 A，直接向原 `check()` 提供内存片段，不改源码。
- 影响：U1 的非法尺寸门禁不成立；乘除两侧的数字被直接删除，字体选择函数完全没进入扫描。原“加入直接字面量后失败”的反向测试只覆盖一种写法。
- 修法：覆盖尺寸函数及简单常量表达式/声明，按字号、图标、圆角等参数类别检查，保留有限且有理由的例外；将上述漏检写法纳入检查器自身的负例。

### UI-R1-003（高）：确认说明只显示两行，未读到完整对象仍可执行

- 文件：`system/ui/shell/confirm_card.c:91`、`:95`、`:50`、`:100`；合同 `docs/product/system-ui-spec.md:285`。
- 【实测】通过真实确认服务发起仅计数、无删除副作用的测试能力，说明为“测试文件甲 / 测试文件乙 / 测试文件丙 / 这三个文件都找不回来”四行。正文高度固定 104 px，四行需 208 px；截图只见甲、乙，第三个对象和不可恢复提示被裁掉。点击“确认”后测试动作执行一次。
- 复现命令：附录 C；产物 `build/test-out/review-functional/snap-long-confirm.png` 和 `metrics.json`。结果 `review.confirm.height=104`、`required_height=208`、`effects=1`，相应两条断言失败。
- 影响：高风险确认没有呈现完整的待执行对象及后果，违背“内容长时翻完才能确认”。本轮用无副作用测试动作验证，没有真的删除用户数据，也没有声称已复现数据丢失。
- 修法：按可显示行数分页，所有必读页看完前禁用确认，并将阅读状态绑定本次确认 token/内容版本。最后一个对象及不可恢复后果必须可见；不要用缩小字号、截断关键说明替代。增加多对象、长名称、换页和内容更新的回归检查。

### UI-R1-004（高）：视觉对照在刷新尚未结束时拍照，不能可靠衡量完成态

- 文件：`tests/ui_shell_scenes.c:123`、`:173`、`:178`；`tools/ui_compare.py:34`。
- 【实测】给 `take_snapshot` 加只读 busy 探针，其他行为不变：Apps、Today 的 `review.snapshot.busy=1`，测试仍退出 0。Apps 的 `snap-scene.png` 与稳定后的 `final.png` 有 86,403 个不同像素，Today 有 9,156 个。ImeCandidates 为 settled，两个文件无像素差异。
- 复现命令：附录 B 的 `snapshot` 模式；对照原 `out/ui-compare/Apps/snap-scene.png` 与 `final.png`。
- 影响：工具优先使用固定延迟后截到的半成品；Apps 图标灰底在补灰完成后才出现。原 Apps MAE 16.755，稳定图按同算法为 15.673；Today 从 24.718 到 24.656。两者仍超标，但不能把所有差异都当成布局错误，现有分数也不能证明最终视觉状态。
- 修法：稳定状态场景等待 `!ink_ui_busy() && !ink_display_busy()`，有界超时失败；AI 听取/思考等中间状态先用可控 fixture 固定所需状态，再等待该帧呈现。不要只增加 sleep，也不要一律改取最终图，因为 PIN 和 AI 场景之后还会发生状态变化。

### UI-R1-005（中）：确认卡吞掉系统返回和收起手势

- 文件：`system/ui/ui_task.c:640`、`:642`；`system/ui/shell/confirm_card.c:64`；合同 `docs/product/system-ui-spec.md:94`、`:109`。
- 【实测】确认卡弹出且刷新完成后，底边 `(342,1200)→(342,900)` 上滑及左边 `(10,850)→(300,850)` 右滑，两次都仍在 `confirm`。HOME 键可以取消，证明该卡有取消路径，只是边缘手势在分派前被 `level >= 1` 直接丢弃。
- 复现命令：附录 C；`bottom edge did not cancel confirmation`、`left edge did not cancel confirmation` 两条失败。
- 影响：不符合“底边/HOME 先关浮层、返回取消确认”的合同；没有 HOME 键的设备尤其依赖手势。
- 修法：让最上层浮层接收并决定返回/收起手势，确认卡处理为取消；来电等仍按自身规则独占，禁止传到底页。为各层级的手势与实体键分别补测试。

### UI-R1-006（中）：滑杆在控件范围内无法触达最大档

- 文件：`system/ui/shell/navigation.c:384`、`:391`。
- 【实测/实现】计算使用 `(x-x1)*10/(x2-x1+1)`，故闭区间内最大也只能得到 9。控制中心右端内侧 `(650,208)` 点击后亮度为 90，非 100；这不是 AI 设置能力的限制。
- 复现命令：附录 C；`review.slider.right_edge_value=90`，端点断言失败。
- 影响：亮度、暖光和音量共用此回调；正常点按滑杆的最大档不可达。
- 修法：用包含两端的档位映射及合理取整，保留 clamp；验证 0、10 及中间各档，覆盖点按、拖动和不同比例屏幕。

### UI-R1-007（中）：整块重绘和两阶段调度尚未满足原刷新预算

- 文件：`system/ui/shell/navigation.c:424`、`:455`、`:497`、`:594`；`system/services/display/display_service.c:95`、`:107`、`:151`。
- 【实现】控制中心清空整树后请求 `FLASH_UI`；设置页也清空内容区再绘制。两阶段预览仍选择 GL16，完成后再排灰阶，存在额外更新成本；不能仅凭代码断言这些就是全部延迟来源。
- 【实测】两种 Plus 配置的主页预算失败，数值见失败表。
- 复现命令：`"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^scenario-home_basic-' --output-on-failure -j3`；本轮已由全量复现，没有额外重复全量。
- 影响：U7、U11 未达标；已有实现不等于反馈优先和正确局部刷新的合同已满足。
- 修法：保留对象并只更新变化区域，记录反馈和补灰各自的矩形、排队时间与波形，再缩减补灰范围/冗余更新；必须保留旧预算，不能盲目换 DU（上一帧含灰时有波形约束）。

### UI-R1-008（中）：快照缺当前可见状态，部分必需交互仍未接入

- 文件：`system/ui/shell/navigation.c:359`、`:377`、`:501`、`:519`、`:528`、`:608`；`system/ui/shell/ai_panel.c:126`。
- 【实现】Today 快照只概括继续读，未覆盖其余卡片；Control 快照没有叫名字、展开后暖光等可见状态；设置页共用快照不含当前字号、锁屏设置、Wi-Fi 列表/选择编号。可调用 `settings.set` 和 `shell.wifi_pick` 不能替代这些页面上下文。
- 【实现/目视】字号只改本页样文；自动锁屏按钮固定写 600 秒，不是选择；多项设置行为只读。AI 只接“继续问/下一页”，没有设计的听解释、更多和保存等完整流程。开发报告已列欠项，不是伪报全完成。
- 复现命令：`rg -n 'ink_shell_home_snapshot|control_snapshot|page_snapshot|display.font_size|自动锁屏|继续问|下一页' system/ui/shell/navigation.c system/ui/shell/ai_panel.c`；按 U10 命令查看 `Today`、`Control`、`SetDisplay`、`SetNetwork`、`Ai` 对照图。
- 影响：U8/U9 与“当前可见字段和可调用能力一致”的要求未完成；模型不能只依赖该屏快照理解和操作当前状态。
- 修法：每屏显式序列化当前值、可见项及稳定编号；能力参数和触摸路径共用服务。已有能力先接完整，缺失服务契约单独列待办，不能用可点击外观或全局能力注册代替实现。

### UI-R1-009（中）：多屏结构、控件和文案仍明显偏离设计

- 文件：`system/ui/shell/navigation.c:424`、`:497`；`system/ui/shell/keyboard.c:263`、`:284`、`:302`；`system/ui/shell/ai_panel.c:119`；`system/ui/shell/confirm_card.c:88`、`:100`。
- 复现命令：`"$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --design "$INK_DESIGN"`，打开 `out/ui-compare/<画板>.png`；本轮工具退出 0（只表示生成成功）。
- 【实测/目视】30 屏均超 12/255，本轮实际打开 20 屏，差异见下表。截图时点问题已单列 004，不能用含残影或尚未补灰的图直接判定所有组件缺失。
- 影响：U2/U8/U10 尚未完成，尤其键盘、设置、确认及 AI 的层次与操作含义不够接近合同。
- 修法：先修取图时点，再按公共组件、键盘/面板、页面内容依次对齐；确认按钮应明确动作。遵守现行尺寸与黑白刷新规则，不复制演示资料或采用已过时的尺寸换分数。

## 逐屏图像复查

【目视】已打开 Foundation、Components 原稿，以及 Main、Today、Control、Settings 原稿；下表标“已看”的 20 屏均打开了本轮生成的三栏对照图。其余 10 屏仅运行并核对分数，不称已目视。表中分数是工具当前选取的 `snap-scene.png`，受 004 影响。

真实书名、网络、成员数量和空状态不同不算假实现；阅读底页不属于本任务重画范围，不能把 LockPage/AI 的全部像素差都归责于壳。画板旧 200/240 头像等与现行规格冲突时，以规格为准；电源面板贴底和规范内字号不列为缺陷。

| 屏幕 | MAE /255 | 本轮目视及具体差异 |
| --- | ---: | --- |
| Main | 15.345 | 已看：时钟字形/粗细与原稿不同，日期及 Dock 的视觉重量有差异；真实无进度状态合理 |
| Today | 24.718 | 已看：日期位置缺失；继续读卡的书名/进度层次未建立；截图有旧时钟残影，需先处理 004 |
| Apps | 16.755 | 已看：搜索框变白底描边，缺搜索图标；图标灰底在此快照未出现，稳定图已出现，不能认定组件没画 |
| Lock | 17.630 | 本轮未单独目视 |
| LockPage | 37.596 | 本轮未单独目视；包含范围外阅读底页的差异 |
| Control | 23.454 | 已看：滑杆缺亮度/音量图标；网络行变描边胶囊；底部“收起”胶囊代替短横条；无通知是真实空态 |
| Notifications | 20.829 | 本轮未单独目视 |
| Members | 18.197 | 本轮未单独目视 |
| MembersBack | 20.701 | 已看：返回选人按钮位置/层次不同，缺阅读位置说明；头像按现行规格缩小不作为错误 |
| Pin | 19.939 | 本轮未单独目视 |
| PinWrong | 18.638 | 已看：缺成员名、密码槽及“忘记密码”入口；操作区为“删除/确定”，与取消/删除设计不同；不要求擅自放宽现有 PIN 策略 |
| ImePinyin | 32.722 | 已看：输入区明显更矮；第二行未按设计缩排；“大写/删除”文字代图标，缺中英切换及中文标点键；完成键主次不明显 |
| ImeCandidates | 27.304 | 已看：展开候选后键盘区高度改变，没有设计的候选网格与页码；本屏已 settled，不能用 004 解释这些结构差异 |
| ImeEdit | 33.666 | 已看：方括号代替选区反白；“剪切”替换设计中的“全选”，工具栏形状及键盘布局不同 |
| ImePassword | 29.552 | 本轮未单独目视；遮断检查由原集成测试验证 |
| ImeVoice | 30.010 | 已看：缺独立识别稿卡和大话筒层次，按钮顺序/主次不同，输入区高度不同；连接中与听取中属于 fixture 状态差异 |
| Confirm | 24.931 | 已看：通用说明与对象信息层次不同，按钮统一叫“确认”，没有表明具体发送动作 |
| ConfirmDelete | 26.074 | 已看：沿用 AI 图标；按钮“确认”代替“删除”；本例恰能容纳，长说明裁剪另见 003 |
| ConfirmSecret | 27.869 | 已看：按钮未表达“填写/这次允许”的具体动作，通用提示重复；掩码仍保留 |
| AiListen | 33.584 | 本轮未单独目视 |
| AiThink | 36.640 | 本轮未单独目视 |
| Ai | 37.163 | 已看：正文仅三行窗口，“继续问/下一页”替代“听解释/更多”，来源缺书籍图标；底页差异不全部计入壳缺陷 |
| Settings | 16.520 | 已看：成员卡只显示名字，缺头像/人数/箭头；搜索入口与分组分隔线缺失，部分状态文字固定 |
| SetDisplay | 19.077 | 已看：滑杆缺图标；屏幕分组变两个胶囊，自动锁屏是固定 10 分钟按钮；样文字号遵守现行规范优先 |
| SetSound | 21.176 | 本轮未单独目视 |
| SetNetwork | 35.519 | 已看：Wi-Fi 开关及已知网络分组被扫描按钮/状态文字替代，SIM 信息层次缺失；不要求伪造设计网络名 |
| PowerMenu | 27.167 | 已看：取消按钮是白底描边，说明区缺失；面板贴底、圆按钮用规范尺寸是合理选择，强制关机提示需先有真实服务支持 |
| PowerOff | 16.877 | 本轮未单独目视 |
| PhoneIncoming | 19.966 | 已看：缺设计顶部信息和快捷回复，接听/拒接位置更低；规范尺寸优先，测试号码为合成数据 |
| ClockRing | 19.940 | 已看：时刻、标题位置和层次不同，缺日期/按键说明；“起床”中回退字的粗细不一致 |

## 范围、既有测试和架构审视

【实现】基线差异为 314 个文件，包含用户 `2522cc0` 的规则更新。没有发现改动受保护的需求、想法、决策、既有调研/审计资料，未发现驱动或模拟器产品实现夹带。服务增量是两阶段显示接口/队列、AI 服务委托 UI 呈现及话筒入口、能力服务的动作页区分、UI 公共回调；与任务直接相关。该结论基于差异，不用于证明开发历史从未执行外部操作。

【实现】旧脚本的入口坐标迁到 Dock，home 场景改为真实三屏/阅读往返；文字输入查找层改为 `lv_layer_sys`；话筒通过能力定位；应用测试改验真实应用可见性；AI 白名单扩展；GUI 仍为 dummy。原有预算文件无修改，只新增 3 个 UI 预算 JSON。没有发现删除失败测试或放宽旧断言来制造全绿。

【实现/边界】已有等待和脚本时序确有变化：`standby` 的 1,180→1,420 ms 已在开发报告解释；home 脚本也调整手势和动作时序。本轮未重做其全部时间轨迹因果分析，不能用报告一句话证明都合理。可以确认旧预算仍在执行且主页仍失败。新增 30 屏测试的固定 sleep 已独立验证会截早，按 004 处理。

按 R-061 审视：

- 模块边界：构建分层检查通过，UI 通过公共服务/能力接口接入，未发现应用直接引用模拟器；`navigation.c` 集中导航、页面渲染和快照，后续宜按职责拆分，但本轮不要求范围外重构。
- 原子能力：多数新增按钮共用能力表，密码/确认保留仅人操作的边界；旧文本长按处理仍有直接调用文本/剪贴板路径，不能说全部用户动作已统一迁完。
- AI 原生：快照与能力骨架具备，但 001、008 说明“模型得到当前状态并实际完成同一动作”的验收仍不完整。动作已注册与当前权限下允许执行必须分开验证。

## 结束检查与证据限制

【实测】`git diff --check` 通过；基线至 HEAD 的本机路径/MAC 模式扫描 0 命中，最大变更文件 `Outfit.ttf` 为 110,884 字节，未超过 5 MiB。工作树唯一变化为本复查文件，生成字体、图标、截图、日志和探针可执行文件均在忽略目录，未提交。

【实现/实测】Outfit 附 OFL，第三方来源、版本和哈希在原开发报告/资源清单中可定位；本轮未重新联网下载。中文缺字反向验证使用 Outfit 输入“测”并启用 `--require-coverage`，退出 1 且拒绝生成不完整字体。图标反向验证用 `unittest.mock` 只模拟指定 SVG 不存在，检查器退出 1；本轮没有通过移走原文件重跑负向构建。

未验证：Ubuntu/CI、窗口 GUI、目标硬件、目标资源分区、基线完整重建。dictation 的额外一帧、notify 的观测竞态、故障注入的电源失败仍需进一步定位。本轮未修改这些问题，也未把开发报告中的推测升级为已证实根因。

## 复现命令

普通和 sanitizer 构建/全量按以下顺序执行，不并行启动第二轮。现有构建目录已按总规则配置；本轮核对配置后做增量构建，没有把它称为干净重建。

```bash
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
"$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --design "$INK_DESIGN"
```

全量证据在 `build/Testing/Temporary/LastTest.log`、`build-asan/Testing/Temporary/LastTest.log` 和各自 `test-out/*/metrics.json`；视觉证据在 `out/ui-compare/`。以下复现不修改仓库源码；编译命令从本轮 CMake 生成文件读取，仅适用于该构建已完成的环境。

### 附录 A：尺寸检查反例

```bash
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
from unittest.mock import patch
import runpy
check = runpy.run_path("tools/ui_scale_check.py")["check"]
for source in (
    "lv_obj_set_width(o, 123);",
    "const int width=123; lv_obj_set_width(o,width);",
    "ink_ui_font_px(123,INK_WEIGHT_REGULAR);",
    "lv_obj_set_width(o,123 * 1);",
):
    with patch.object(Path, "read_text", return_value=source):
        print(source, check(Path("probe.c")))
PY
```

预期当前结果：第一条报非法 123，后三条 `[]`，即漏检。

### 附录 B：撤掉 Settings 入口及快照时点探针

下面设置 `INK_REVIEW_MODE=skip-settings` 验证 001；改为 `INK_REVIEW_MODE=snapshot` 验证 004。仅在内存中改变测试副本，串行编译链接与运行。

```bash
INK_REVIEW_MODE=skip-settings "$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
import json, os, shlex, subprocess
root = Path.cwd()
build = root / "build"
mode = os.environ["INK_REVIEW_MODE"]
source = (root / "tests/ui_shell_scenes.c").read_text()
if mode == "skip-settings":
    source = source.replace('static void prepare(void) {',
        'static void prepare(void) {\n    if (!strcmp(scene,"Settings")) return;', 1)
    source = source.replace('static void assert_view(void) {',
        'static void assert_view(void) {\n    ink_probe_count(!strcmp(ink_ui_page_name(),"home") ? "review.page.home" : "review.page.other");', 1)
    stem, scenes = "review-ui-scenes", ["Settings"]
elif mode == "snapshot":
    source = source.replace('sim_display_snapshot((const char *)arg);',
        'ink_probe_count(ink_display_busy() ? "review.snapshot.busy" : "review.snapshot.settled"); sim_display_snapshot((const char *)arg);', 1)
    stem, scenes = "review-snapshot", ["Apps", "Today", "ImeCandidates"]
else:
    raise SystemExit("unknown mode")
args = []
for line in (build / "CMakeFiles/sim-ui-scenes.dir/flags.make").read_text().splitlines():
    if line.startswith(("C_DEFINES =", "C_INCLUDES =", "C_FLAGS =")):
        args += shlex.split(line.split("=", 1)[1])
link = shlex.split((build / "CMakeFiles/sim-ui-scenes.dir/link.txt").read_text())
obj, target = str(build / (stem + ".o")), str(build / stem)
subprocess.run([link[0], *args, "-I" + str(root / "tests"), "-x", "c", "-c", "-", "-o", obj],
               input=source, text=True, check=True)
link = [obj if x == "CMakeFiles/sim-ui-scenes.dir/tests/ui_shell_scenes.c.o"
        else target if x == "sim-ui-scenes" else x for x in link]
subprocess.run(link, cwd=build, check=True)
for scene in scenes:
    name = "review-skip-settings" if mode == "skip-settings" else "review-snapshot-" + scene
    out = build / "test-out" / name
    result = subprocess.run([target, "--profile", "simulator/profiles/metalio-plus.json",
        "--script", "tests/ui_shell.script", "--assets", "build/assets", "--content", "build/test-content",
        "--budget", "tests/ui_scene.budget.json", "--out", str(out), "--quiet", "--no-random-outages"],
        env=dict(os.environ, INK_UI_SCENE=scene))
    data = json.loads((out / "metrics.json").read_text())
    print(scene, "exit", result.returncode, "warnings", data.get("warnings"))
    for value in data.values():
        if isinstance(value, dict):
            selected = {k: v for k, v in value.items() if k.startswith(("review.", "test."))}
            if selected:
                print(selected)
PY
```

### 附录 C：长确认、取消手势与滑杆端点

此测试能力只递增计数，不删除任何文件。以下是本轮实际运行的源码与生成命令，保留失败断言以复现问题。编译链接应退出 0；模拟器报告 `test.fail=5`、`test.pass=15`、`test.finished=1`。

```bash
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
import shlex, subprocess, json
root=Path.cwd(); build=root/'build'
args=[]
for line in (build/'CMakeFiles/sim-ui-scenes.dir/flags.make').read_text().splitlines():
    if line.startswith(('C_DEFINES =','C_INCLUDES =','C_FLAGS =')): args += shlex.split(line.split('=',1)[1])
source=r'''
#include <stdio.h>
#include <string.h>
#include "ink/sys/confirm.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/settings.h"
#include "ink/sys/members.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
static int effects, accept_x, accept_y, detail_h, required_h;
static void effect(ink_call_t *c) { (void)c; effects++; }
static const char *DETAIL="测试文件甲\n测试文件乙\n测试文件丙\n这三个文件都找不回来";
static const ink_cap_t PROBE={
 .id="test.review_delete", .title="测试删除", .ask="测试文件甲\n测试文件乙\n测试文件丙\n这三个文件都找不回来",
 .effect=INK_FX_LOCAL_DESTRUCTIVE, .child_ok=true, .run=effect
};
static void inspect(lv_obj_t *root) {
 for(uint32_t i=0;i<lv_obj_get_child_count(root);i++){
  lv_obj_t *o=lv_obj_get_child(root,i);
  if(lv_obj_check_type(o,&lv_label_class)){
   const char *s=lv_label_get_text(o);
   if(!strcmp(s,"确认")){lv_area_t a;lv_obj_get_coords(o,&a);accept_x=(a.x1+a.x2)/2;accept_y=(a.y1+a.y2)/2;}
   if(!strcmp(s,DETAIL)){detail_h=lv_obj_get_height(o);required_h=4*lv_obj_get_style_text_font(o,LV_PART_MAIN)->line_height;
      ink_probe_value("review.confirm.height",detail_h); ink_probe_value("review.confirm.required_height",required_h);}
  }
  inspect(o);
 }
}
static void inspect_ui(void){lv_obj_update_layout(lv_layer_sys());inspect(lv_layer_sys());}
static void snap(void *name){sim_display_snapshot(name);}
static void shot(const char *name){vtk_event_at(vtk_now()+SIM_MS(1),snap,(void*)name);delay_ms(5);}
static void quiet(void){CHECK(WAIT_FOR(!ink_ui_busy()&&!ink_display_busy(),10000),"panel settles");}
static void swipe(int x0,int y0,int x1,int y1){hw(HW_PRESS,x0,y0);delay_ms(100);hw(HW_PRESS,x1,y1);delay_ms(100);hw(HW_RELEASE,x1,y1);delay_ms(400);}
static void test_task(void *unused){
 (void)unused;delay_ms(1800);quiet();
 CHECK(AI("test.review_delete",NULL)&&g_rc.status==INK_RC_PENDING_CONFIRMATION,"open confirmation");
 quiet();on_ui(inspect_ui);shot("long-confirm");
 CHECK(detail_h>=required_h,"confirmation clips content: visible=%d needed=%d",detail_h,required_h);
 tap(accept_x,accept_y);quiet();
 ink_probe_value("review.confirm.effects",effects);
 CHECK(effects==0,"unread clipped deletion executed");
 CHECK(AI("test.review_delete",NULL)&&g_rc.status==INK_RC_PENDING_CONFIRMATION,"open confirmation for bottom gesture");
 quiet();swipe(342,1200,342,900);quiet();on_ui(read_screen);
 CHECK(strcmp(g_top,"confirm")!=0,"bottom edge did not cancel confirmation");
 key(HAL_KEY_HOME);quiet();
 CHECK(AI("test.review_delete",NULL)&&g_rc.status==INK_RC_PENDING_CONFIRMATION,"open confirmation for left gesture");
 quiet();swipe(10,850,300,850);quiet();on_ui(read_screen);
 CHECK(strcmp(g_top,"confirm")!=0,"left edge did not cancel confirmation");
 key(HAL_KEY_HOME);quiet();
 CHECK(BY_HAND("shell.control",NULL)&&g_rc.status==INK_RC_DONE,"open control");
 quiet();tap(650,208);quiet();
 ink_probe_value("review.slider.right_edge_value",ink_setting_get("display.light"));
 CHECK(ink_setting_get("display.light")==100,"rightmost slider tap cannot reach 10: %lld",(long long)ink_setting_get("display.light"));
 ink_probe_count("test.finished");
}
void ink_main(void){product_main();ink_cap_register(&PROBE,1);osal_task_cfg_t c={.name="review",.fn=test_task,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};if(osal_task_create(&c,NULL)!=OSAL_OK)osal_panic("review");}
'''
link=shlex.split((build/'CMakeFiles/sim-ui-scenes.dir/link.txt').read_text())
obj=str(build/'review-functional.o'); target=str(build/'review-functional')
subprocess.run([link[0],*args,'-I'+str(root/'tests'),'-x','c','-c','-','-o',obj],input=source,text=True,check=True)
link=[obj if x=='CMakeFiles/sim-ui-scenes.dir/tests/ui_shell_scenes.c.o' else target if x=='sim-ui-scenes' else x for x in link]
subprocess.run(link,cwd=build,check=True)
out=build/'test-out/review-functional'
r=subprocess.run([target,'--profile','simulator/profiles/metalio-plus.json','--script','tests/ui_shell_test.script','--assets','build/assets','--content','build/test-content','--budget','tests/ui_scene.budget.json','--out',str(out),'--quiet','--no-random-outages'])
d=json.loads((out/'metrics.json').read_text())
print('functional_probe_exit=',r.returncode)
print('warnings=',d.get('warnings'))
for v in d.values():
    if isinstance(v,dict): print({x:y for x,y in v.items() if x.startswith(('test.','review.'))})
PY
```

SEVERE=0 HIGH=4
