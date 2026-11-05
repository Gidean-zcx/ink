# 界面框架与系统壳执行报告

2026-11-06，第 3 轮修复。接续基线 `332cab0`，依据 [REVIEW.md](REVIEW.md) 和[总规则](../codex-rules.md)；原任务基线 `fdff779`。分支 `claude/codex-ui-shell`，仅本地提交。里程碑 `7593e93`、`8b5408a`；补充门禁与最终验证随本报告提交。前轮报告保留在 `27040e3:docs/tasks/ui-shell/REPORT.md`。

**严重 0 项；两项高问题已修。中级补了分段边框、搜索图标、缩放图标边界和键盘固定区域，整组布局仍未完成。普通及 ASan/UBSan 全量均 136/141，退出 8；整体验收未通过。** UI 55 项全部通过；视觉仍为 0/30 达标。未删测试、放宽断言或抬预算。

## 验收表

证据：【实测】本轮命令及产物；【实现】代码核查；【目视】本轮实际打开的图。B/T、N、V 对应下方命令。

| 编号 | 结论 | 证据 | 命令或位置 |
| --- | --- | --- | --- |
| UI-R3-001（高）/ UI-R2-001 | 已修 | 【实测】保留可达候选，覆盖带/不带花括号、两路/嵌套分支、循环、条件复合赋值、别名、预处理赋值及条件宏定义；未知尺寸继续拒绝。25 文件、0 违规。撤回检查器修复，同一自检失败 25 项、无异常 | T、N；`ui_scale_check.py`、`ui_scale_check_test.py` |
| UI-R3-002（高） | 已修 | 【实测】应用/分组/Dock 表、工具结构体、条件选择、别名、注册应用 `.icon` 及跨文件多层转发覆盖，转发不依赖参数叫 `icon`。真实 APP_ICONS 注入未知名称：旧门禁退出 0，新门禁退出 1；复制实际 CMake 检查目标后构建退出 2，恢复名称退出 0；生成名称表编译对照退出 0/1 | B、T、N；`tools/icongen/check.py`、`ui_icon_check_test.py` |
| UI-R2-002（高，前轮已修） | 本轮回归通过 | 【实测】两档案成员隔离各 32 项通过；旧壳失败 5 项，单撤迟到回执守卫失败 1 项 | T、N |
| UI-R2-003（中，前轮已修） | 本轮回归通过 | 【实测】两档案完整答案分页各 113 项通过；旧分页失败 11 项，撤掉四位页码栏修复失败 1 项 | T、N |
| UI-R2-004（中） | 未修 | 【实测/目视】两个主页档案仍超原刷新预算，Apps 稳定帧仍留旧时钟/图标残影；需要跨页对象保留及脏矩形、补灰队列联合验证 | T、V；没做到的 |
| UI-R2-005（中） | 延续部分完成 | 【实测】可见内容/能力回归通过，Apps/通知旧版本失败 7 项；AI 主动作、全局字号、完整声音设置仍未闭环 | T、N；没做到的 |
| UI-R2-006（中） | 本轮部分修复 | 【实测】2/3/4 段、两端选中均留出上下边框和圆角两端；搜索图标不遮文字；缩放时控件与图像同尺寸；候选、折叠、编辑、密码状态保持同一区域。两档案各 185 项滑杆/分段、89 项快照断言通过。分别撤回四处修复失败 24/1/1/2 项 | T、N、V |
| U1 | 本轮门禁通过 | 【实测】条件赋值/宏和原有字号表负例均有效；尺寸集合、例外要求未放宽。检查仍是有界 C 表达式检查 | T、N |
| U2 | 组件回归通过，整体外观未齐 | 【实测/目视】三组件场景通过；分段边框已修，Dock、分组列表和其他页面层次仍偏离设计 | T、V |
| U3 | 本机构建通过，体积/跨平台未完成 | 【实测】609 glyph UI 子集、三字重、Outfit 数字与回退资源通过结构检查；字体 7,538,204 字节，比 4 MiB 超 3,343,900。Ubuntu 未验，CI 的 `fonts-noto-cjk-extra` 欠项仍在 | B、T |
| U4 | 本轮构建门禁通过 | 【实测】80 名称、240 SVG、1,600 位图校验通过；表中名称改用 manifest 生成的 `INK_UI_ICON` 声明，未知名称在检查和编译阶段均拒绝。资源检查是 `ink_system` 的前置依赖 | B、T、N |
| U5 | 本轮回归通过 | 【实测】键盘/滑杆/边缘/前台/实体键优先级通过，旧事件分派及触摸层仍被反向检出 | T、N |
| U6 | 本轮回归通过 | 【实测】打断恢复、帧缓冲/物理面板恢复及成员隔离通过；不代表实机时序验收 | T、N |
| U7 | 未通过 | 【实测/目视】主页刷新延迟和残影仍未解决，跨页局部对象保留未完成 | T、V |
| U8 | 入口通过，完整流程未齐 | 【实测】30 屏身份及代表动作通过；AI、声音、设置、密码等欠项仍保留 | T、V |
| U9 | 部分完成 | 【实测】现有画面/快照、成员及 touch/AI 操作回归通过；本轮是呈现修复，未补齐缺失的服务原子能力 | T、N；UI-R2-005 |
| U10 | 取图通过，视觉未达标 | 【实测】无缓存重取 30 屏；0/30 ≤12/255。Apps 15.673→13.995，SetDisplay 18.817→18.510，仍未达标 | V；下方逐屏表 |
| U11 | 全量未通过，已查 RAM 达标 | 【实测】两套均 136/141，失败项相同；日志无 sanitizer 错误诊断。最低已查 AI 内部 RAM 33,192 ≥32,768，预算未改 | B、T；失败/资源表 |
| U12 | 无窗口验证通过 | 【实测】SDL dummy 的 `gui-smoke`、主页三屏、控制中心及设置场景通过；未启动带窗口的模拟器 | T |
| U13 | 本机结束检查完成，跨平台未验 | 【实测】两套构建/全量、三轮反向、30 屏新图、范围/隐私/返回值/文件检查均已做；仅本地提交，未做 Ubuntu、目标链接/分区或设备验证 | B、T、N、V；顺手发现 |

复跑命令（依赖使用用户指定的 `INK_DEPS` / `INK_DESIGN`；构建和全量串行，最多 `-j4` / `-j3`）：

```sh
# B/T：普通配置、构建、全量
cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
# B/T：相同依赖与 WERROR，换目录并开启 sanitizer
cmake -S . -B build-asan -DINK_WERROR=ON -DINK_SANITIZE=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
# N：串行编译忽略目录中的副本，不改产品源码
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_reverse.py
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_r2_reverse.py
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_r3_reverse.py --components
# V：不用 --reuse
"$INK_DEPS/build-deps/pyenv/bin/python" -B tools/ui_compare.py --design "$INK_DESIGN"
```

【实测】两套配置/构建退出 0；最终普通全量 33.09 秒、ASan/UBSan 全量 85.02 秒，均退出 8。第二套编译和链接实际带 `-fsanitize=address,undefined`；两套 `LastTest.log` 与最终全量日志未见 ASan/LeakSanitizer/UBSan 错误诊断，不把它写成全量通过。N 三脚本与 V 均退出 0；N 的 0 表示预期故障被检出，V 的 0 只表示生成成功。

本轮新增一个 ctest 项 `ui-icons-regressions`，原有 140 项保留。原始记录在忽略目录 `work/ui-shell-r3/`，最终全量为 `full-final.log` / `full-asan.log`；反向副本在 `build/review-negative/`，图片在 `out/ui-compare/`。这是 macOS 增量构建，没有重建 Linux 基线。

资源对比（字节；修前内部/DMA 来自本轮 REVIEW，PSRAM 来自前轮执行报告；未重新构建修前全量。本轮来自最终普通全量。内部取 `mem.internal_total.min_free_bytes`，为模拟器模型，非目标链接或实机）：

| 场景 | 内部最低余量：修前→本轮 | DMA 最低余量：修前→本轮 | PSRAM 最低余量：修前→本轮 |
| --- | ---: | ---: | ---: |
| ai_ask-metalio-plus | 35,856→35,856 | 31,328→31,328 | 10,974,840→10,974,840 |
| home_basic-metalio-plus | 48,816→48,816 | 48,752→48,752 | 12,361,168→12,361,168 |
| ai_tools-metalio-plus | 34,912→34,912 | 31,328→31,328 | 10,974,960→10,974,960 |
| ai_ask-metalio-plus-pqbn | 33,192→33,192 | 28,616→28,616 | 10,120,064→10,120,064 |

最低已查 AI 内部余量仅比门槛高 424 字节；ASan 档案该值也为 33,192。29 个字体共 7,538,204 字节，1,600 个图标 20,308,480，合计 27,846,684，与修前相同。没有新增常驻任务、大缓冲或服务接口。

## 没做到的

两套最终全量相同的五个失败，原断言和预算全部保留：

| 项目 | 本轮实测 | 没修的原因 |
| --- | --- | --- |
| scenario-home_basic-metalio-plus | 首变化 p95 431.851 >400 ms；FAST 686.565 >600；TEXT 1928.191 >1,200 | UI-R2-004：涉及跨页对象保留、脏矩形及预览/补灰队列和波形联合验证；超出小型中级修复，不能用延时或改预算关闭 |
| scenario-home_basic-metalio-plus-psram16 | TEXT p95 1707.372 >1,200 ms | 同上，较快内存档案仍不合格 |
| integration-listening | `tests/integration/listening.c:97`，42 通过、1 失败；锁定后 70 秒内睡眠计数未增长 | 声音/电源时序根因未定位；未改听取服务或放宽等待，不扩大结论为所有音频永久阻止睡眠 |
| integration-notify | `tests/integration/notify.c:657`，177 通过、1 失败；提醒入库时声音计数未增长 | 提醒发布/播放观测时序尚未定位，不能据此断言最终永远无声；未改通知服务或原断言 |
| scenario-reader_basic-panel-power-faults | `display.power_failures=1`，上限 0 | 注入故障后的电源重试未定位；驱动不在本任务允许修改范围，未动注入或预算 |

- UI-R2-005 剩余：AI 的“听解释/更多→讲细一点、记下来、对话记录”完整服务流程；字号应用到全局内容；声音页提示音、振动、铃声的完整能力和交互。需要服务、回执与页面契约一起完成，不能靠换文案或假按钮关闭。本轮没有把这些改称“不需要”。
- UI-R2-006 剩余：设置搜索、家庭卡、分组层次、密码槽/忘记密码、候选网格与页码、AI 主动作和其他成套页面对齐。涉及多屏与真实流程，仍未完成；本轮固定区域不等于完整键盘验收。
- 字体未压到 4 MiB；Ubuntu/CI、目标链接/分区、窗口 GUI 及实机刷新未验。CI 仍缺任务指定的 `fonts-noto-cjk-extra`，工作流内的旧无数字并行命令本轮未执行、未修改（任务允许文件范围不含工作流）。

## 和设计不一致的

开工实际打开 Foundation、Components、Main、Today、Control、Settings 原稿。最终逐张打开 Main、Apps、Control、Settings、SetDisplay、ImeCandidates、ImeVoice 七张新三栏对照，以及新 Components 场景；其余 23 屏运行并核对分数，不称全部目视。

【目视】Apps 已有搜索图标，仍有旧时钟/图标残影；SetDisplay 分段的上下边线完整，下面“屏幕”分组仍是两个胶囊；候选展开不再缩短面板，但网格、页码和键盘残影仍不齐；Control 的网络/收起仍是胶囊，Settings 缺搜索与家庭信息层次。AI、密码、确认卡、闹钟等原有差异未因局部修复关闭。保持布局 A，不为分数采用画板中的规格外数值；真实空状态和测试合成内容仍按证据边界解释。

下表是本轮无缓存 V，目标全部 ≤12/255，实际 0/30 达到：

| 页面 | 差值 /255 | 页面 | 差值 /255 |
| --- | ---: | --- | ---: |
| Main | 15.345 | Today | 24.541 |
| Apps | 13.995 | Lock | 17.630 |
| LockPage | 37.596 | Control | 23.177 |
| Notifications | 20.829 | Members | 18.197 |
| MembersBack | 20.701 | Pin | 19.816 |
| PinWrong | 18.619 | ImePinyin | 32.647 |
| ImeCandidates | 27.015 | ImeEdit | 33.666 |
| ImePassword | 29.636 | ImeVoice | 29.947 |
| Confirm | 24.931 | ConfirmDelete | 26.084 |
| ConfirmSecret | 27.869 | AiListen | 33.584 |
| AiThink | 32.902 | Ai | 37.348 |
| Settings | 16.222 | SetDisplay | 18.510 |
| SetSound | 20.973 | SetNetwork | 35.636 |
| PowerMenu | 27.167 | PowerOff | 16.877 |
| PhoneIncoming | 19.966 | ClockRing | 19.637 |

## 改过的已有测试

- `ui_scale_check_test.py` 只增加条件赋值、循环、预处理/条件宏定义的负例及合法对照；原断言、尺寸集合和例外要求保留，没有修改旧期望。
- `ui_shell_regressions.c` 只增加 2/3/4 段实际边界、搜索图标与文本位置、键盘跨状态区域检查；原滑杆、快照、密码隔离和服务动作断言保留，没有修改旧期望。
- 新增 `ui_icon_check_test.py`、`ui_shell_r3_reverse.py`。后者用真实表变异、原检查器及隔离构建/编译作正反对照；组件副本先删旧指标，再要求退出 1、实际 CHECK 失败及任务完成，避免把崩溃或旧结果当成功。
- 本轮未改已有脚本、预算、`listening.c`、`notify.c` 或 `dictation.c`，未删测试、跳过测试或增加等待遮掩失败。前轮测试变更的理由仍在历史报告。

## 要用户定的

无新增决策。按已有授权完成可逆修复，未请求确认，也未修改产品合同或范围。

## 顺手发现

【实现】R-061：新增工作仍在 UI 呈现与宿主构建检查；输入、按钮和 AI 继续走既有能力总线，界面没有直写服务内部持久状态。没有服务/策略层增量接口、常驻任务或第三方依赖。缺失的 AI/设置流程仍限制整体 AI 原生完整性。

【实测】中间版本在原尺寸图标上重复设布局，听写出现“一帧晚到”计数断言；隔离撤回该处后通过。最终只在需要缩放时调整控件布局，两套原听写及免按键听写断言均通过。未查明那一帧的内容，不把计数变化称为已证明的通话内容泄漏，也没有改声音服务或放宽断言。

【实测】结束检查：本轮 14 个相关文件，无文件超过 5 MiB；相对 `332cab0` 的差异检查通过。本轮提交及最终工作区差异、本任务原基线 `fdff779` 以来的差异，本机路径/MAC 正则均 0 命中。修改的 C/H 去掉注释/字符串后，`write/read/fgets` 调用为 0，没有新增忽略返回值的写法；不据此声称 Linux gcc 已验。报告的相对链接可解析。

【实测】完整原任务基线 `git diff --check fdff779..HEAD` 仍有第三方 Outfit-OFL、NanoSVG LICENSE 的原文空白及 nanosvgrast 缩进告警；未为消除告警修改第三方原文。构建物、日志、反向副本、截图及生成名称头文件均未提交。依赖/设计稿只读；无硬件、串口、真实凭据、付费接口、远端写入或带窗口 GUI 操作。
