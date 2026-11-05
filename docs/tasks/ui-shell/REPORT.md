# 界面框架与系统壳执行报告

2026-11-05，第 2 轮复查修复。接续基线 `024c800`，依据 [REVIEW.md](REVIEW.md)；原任务基线 `fdff779`。分支 `claude/codex-ui-shell`，仅本地提交。实现里程碑为 `4196b8f`、`27d4bd1`、`5bb2008`；四位页码边界修复与本次最终报告一并提交。上轮详细记录保留在 `a429a48:docs/tasks/ui-shell/REPORT.md`。

**严重 0 项；2 个高均已修；中级 003 已修，005、006 部分修复，004 未修。普通及 ASan/UBSan 全量均 135/140，退出 8；整体验收仍未通过。** 未删测试、放宽断言或抬预算。两套内 UI 54 项全部通过；30 屏视觉仍为 0/30 达到目标。

## 验收表

证据：【实测】本次命令与产物；【实现】代码核查；【目视】本次实际打开的图。B/T、N、V 命令见下方。

| 编号 | 结论 | 证据 | 命令或位置 |
| --- | --- | --- | --- |
| UI-R2-001（高） | 已修 | 【实测】补声明后赋值、未初始化赋值、分支、复合赋值、别名冻结、静态数组候选及真实字号表变异。缩放调用外的非法偏移和错误类别的百分比/哨兵也拒绝。无法判定即报错；运行时转发/几何仅可在文件头具名说明原因，不能覆盖已知非法值。25 文件、0 违规；旧检查器在同一自检中失败 20 项 | T、N；`tools/ui_scale_check.py`、`tests/ui_scale_check_test.py` |
| UI-R2-002（高） | 已修 | 【实测】搜索后实际触摸选儿童，检查当前身份、界面对象和 AI 快照无成人词；儿童 Apps 搜索也为空。普通离开/返回保留本人搜索。会话变化清结果、页面位置、网络结果/选择；旧触摸、搜索/密码回调和异步 Wi-Fi 回执检查原会话。受控阻塞后台，让旧扫描切换后才返回，不能污染新成员。旧壳失败 5 项；单撤回执检查失败 1 项 | T、N；`navigation.c`、`ui-privacy-regressions-*` |
| UI-R2-003（中） | 已修 | 【实测】按真实字体、字号、宽高和换行分页；有前后按钮、禁用边界和页数，按钮与 AI 同能力。短多行、长段落和中英混合逐页拼回原文，字节不缺不重；短内容替换会收回页码，关闭后拒绝翻页。另测 4,094 字节多行内容的四位页码单行可读。旧分页失败 11 项，窄页码栏失败 1 项 | T、N；`ai_panel.c`、`ui-answer-regressions-*` |
| UI-R2-004（中） | 未修 | 【实测/目视】两个主页配置仍超原预算，Today/Apps 稳定帧仍有旧时间/图标残影。【实现】控制中心/设置仍重建整块对象；需跨页保留对象并联合验证脏矩形、预览/补灰队列及波形，不是小改。本轮未盲换 DU、加等待或改预算 | T、V；下方失败表 |
| UI-R2-005（中） | 部分修复 | 【实测】Apps 画面与快照共用分组、工具目录和搜索过滤，只列当前工具页及稳定 ID；双页夹具核验 touch/AI 启动同一活动。Control/通知页快照用实际绘制的通知记录，含 ID、正文和已读状态；键盘列实际可见候选及全局序号、光标/选区、展开和听写状态，密码不泄漏。旧 Apps/通知版本失败 7 项，旧键盘失败 2 项；AI 完整主动作、全局字号及声音设置仍未齐 | T、N；`navigation.c`、`keyboard.c` |
| UI-R2-006（中） | 部分修复 | 【实测/目视】闹钟中文标题、实际时间、日期分开，时间用数字字体；按钮明确显示并实际执行“再睡 10 分钟 / 这次不再响”，服务动作索引及电源键小睡路径保留。补亮度/暖光/音量的黑白图标、Today 日期及“继续听/今日报纸”。旧闹钟视图/动作分别失败 2/5 项，旧滑杆缺图标失败 3 项；成套页面/组件对齐未完成 | T、N、V；`interrupt_card.c`、`theme.c` |
| U1 | 本轮门禁通过 | 【实测】简单赋值、静态尺寸表及未知表达式负例通过；仍是有界 C 检查，不声称通用数据流分析 | T、N |
| U2 | 组件存在，外观未齐 | 【实测】三组件场景通过；补滑杆图标，分页栏边界通过；Dock、分段及分组层次仍有差异 | T、V |
| U3 | 本机字体通过，体积超标 | 【实测】609 glyph UI 子集、三字重、规范字号和数字子集构建成功；字体 7,538,204 字节，比 4 MiB 超 3,343,900；Linux 未验 | B、T；资源表 |
| U4 | 本机管线通过 | 【实测】80 图标、240 SVG、1,600 位图沿用固定来源与许可；两套构建及资源校验通过，无新下载 | B、T |
| U5 | 本轮优先级回归通过 | 【实测】键盘、滑杆、系统边缘、前台及实体键优先级回归通过，旧分派/触摸层仍被反向检出 | T、N |
| U6 | 恢复及本轮成员隔离通过 | 【实测】帧缓冲/物理面板打断恢复检查通过；新增真实选人和晚到回执回归通过 | T：`ui-shell-priorities`、`ui-panel-restore`、privacy |
| U7 | 未通过 | 【实测/目视】刷新预算及稳定态残影未解决，局部对象保留未完成 | T、V；UI-R2-004 |
| U8 | 入口通过，完整交互未齐 | 【实测】30 屏身份、代表动作及本轮新增流程通过；AI 更多/朗读等缺口见下方 | T、没做到的 |
| U9 | 本轮快照缺口已补，整体部分完成 | 【实测】有通知、有候选/选区、双页工具及搜索夹具逐项核对画面/快照和 touch/AI 服务结果；未把这些代表用例当全部能力完整性 | T、N；UI-R2-005 |
| U10 | 可靠取图通过，视觉未达 | 【实测】无缓存重新生成 30 屏，全部稳定取图；0/30 ≤12/255。工具退出 0 表示产物生成成功 | V；逐屏表 |
| U11 | 全量未通过，已查 RAM 达标 | 【实测】两套均 135/140，日志未见 ASan/UBSan 诊断；最低已查 AI 内部 RAM 33,192 ≥32,768。原五项失败保留 | B、T；失败/资源表 |
| U12 | 无窗口验证通过 | 【实测】SDL dummy 的 `gui-smoke`、主页三屏、控制中心及设置场景通过；未启动带窗口的模拟器 | T |
| U13 | 本机结束检查完成，跨平台未验 | 【实测】两套构建/全量、两轮反向、30 屏新取图、范围/隐私/文件检查完成，仅本地提交；Ubuntu、目标链接及设备端未验证 | B、T、N、V；顺手发现 |

复跑命令如下；本轮构建与全量均串行，构建最多 `-j4`，ctest 最多 `-j3`：

```sh
# B/T：普通配置、构建及全量
cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
# B/T：相同依赖及 WERROR；只更换目录并开启 sanitizer
cmake -S . -B build-asan -DINK_WERROR=ON -DINK_SANITIZE=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
# N：只串行编译忽略目录中的内存副本
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_r2_reverse.py
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_reverse.py
# V：不用 --reuse
"$INK_DEPS/build-deps/pyenv/bin/python" -B tools/ui_compare.py --design "$INK_DESIGN"
```

【实测】两套配置/构建退出 0；最终普通全量 48.18 秒，sanitizer 全量 93.43 秒，均退出 8；`INK_WERROR=ON`，第二套编译/链接实际带 `-fsanitize=address,undefined`。两套 `LastTest.log` 均无 sanitizer 诊断。N 两脚本均退出 0，表示预期故障均被检出；V 退出 0，视觉目标仍未通过。没有把增量构建写成全新 Linux 重建。

【实测】两档案各有成员回归 32、答案分页 113、快照 70、闹钟 15、确认 65、滑杆 148、既有上下文 34 个断言通过；`integration-confirm` 各 116 个通过。新回归合计增加 8 个 ctest 项，原有项均保留。原始日志在 `work/ui-shell-r2/` 和两目录的 `Testing/Temporary/LastTest.log`；回归、反向及图片均留在忽略目录。

资源（字节；左侧来自第 2 轮复查的 `024c800` 前状态，未重新构建基线；右侧为本次普通全量。内部采用 `mem.internal_total.min_free_bytes`，不是单独堆分区；均为模拟器注解模型，不是目标链接或实机）：

| 场景 | 内部最低余量：修前 → 本轮 | DMA 最低余量：修前 → 本轮 | PSRAM 最低余量：修前 → 本轮 |
| --- | ---: | ---: | ---: |
| ai_ask-metalio-plus | 35,856 →35,856 | 31,328 →31,328 | 10,976,112 →10,974,840 |
| home_basic-metalio-plus | 48,816 →48,816 | 48,752 →48,752 | 12,361,288 →12,361,168 |
| ai_tools-metalio-plus | 34,912 →34,912 | 31,328 →31,328 | 10,976,136 →10,974,960 |
| ai_ask-metalio-plus-pqbn | 33,192 →33,192 | 28,616 →28,616 | 10,121,304 →10,120,064 |

内部最低已查余量仍只高于门槛 424 字节。答案分页索引/工作缓冲和通知绘制记录放 PSRAM，没有新增常驻任务。29 个字体共 7,538,204 字节（比修前多 7,452），图标 20,308,480，合计 27,846,684；未改 4 MiB 字体目标，未验证设备分区容纳。

## 没做到的

| 两套均失败的项 | 本轮实测 | 没修的原因 |
| --- | --- | --- |
| scenario-home_basic-metalio-plus | 首变化 p95 431.634 >400 ms；FAST 686.348 >600；TEXT 1927.975 >1,200 | UI-R2-004：需要跨页面保留对象及逐阶段区域/队列/波形验证，超出小中级修复；不改预算或刷新假设 |
| scenario-home_basic-metalio-plus-psram16 | TEXT p95 1707.372 >1,200 ms | 同上；较快内存配置也未达标 |
| integration-listening | `tests/integration/listening.c:97`，锁定后睡眠计数未增长，42 通过、1 失败 | 听取/音频与电源时序根因未定位，需独立服务排查；未改源文件、断言或增加等待 |
| integration-notify | `tests/integration/notify.c:657`，提醒已入库而提示音计数未增长，177 通过、1 失败 | 本轮闹钟语义回归通过；该既有提醒发布/播放观测顺序仍需单独定位，不能据此断言永远无声，原断言保留 |
| scenario-reader_basic-panel-power-faults | `display.power_failures=1`，上限 0 | 固定故障注入的电源重试根因未定位；驱动不在本任务允许修改范围，未动注入或预算 |

- UI-R2-005 剩余未做：AI 的“听解释/更多→讲细一点、记下来、对话记录”完整服务流程；字号作用到全局内容；声音页提示音、振动、铃声设置的完整接口/交互。需服务与页面契约一起补，不能用只改按钮词假装已可用；本轮只修可独立验证的快照缺口。其他只读入口不因此认定“不需要”。
- UI-R2-006 剩余未做：公共组件边线/内边距/分组，键盘固定高度及候选网格/页码，设置搜索/家庭卡，密码槽/忘记密码，来电快捷回复等成套页面对齐。牵涉多屏布局与真实流程，不是小修，仍欠验收。
- 字体体积未压到目标；未验证 Ubuntu/CI、窗口 GUI、目标链接/分区、实机刷新或 RTC 行为。答案分页覆盖现有呈现缓冲；未新增无限长回答存储。

## 和设计不一致的

【目视】开工看了 Foundation、Components、Main、Today、Control、Settings 原稿；另看 Ai、ClockRing 原稿。最终看了 Today、Apps、Control、SetDisplay、Settings、ImeCandidates、Ai、ClockRing 八张新三栏对照图，以及四张原始场景截图；其余只运行并核对分数，不称全部目视。

Today/Apps 在稳定帧仍有旧主屏残影。Control 的网络/收起仍是胶囊；Settings 的搜索、家庭卡和分隔层次未齐；键盘候选高度与网格仍偏差。AI 答案能完整翻阅，但“继续问”主动作及来源图标仍偏离合同。ClockRing 的标题/时间已分开且动作准确，数字尺寸和位置仍不同。保留主屏布局 A；旧图中超出规范的控件尺寸不照抄。真实空状态、合成回答/号码及范围外阅读底页不拿来伪造业务内容。

下表均为本次 V，目标每屏 ≤12/255；没有为分数改变规格。图片在 `out/ui-compare/`：

| 页面 | 1/4 灰度差 /255 |
| --- | ---: |
| Main | 15.345 |
| Today | 24.541 |
| Apps | 15.673 |
| Lock | 17.630 |
| LockPage | 37.596 |
| Control | 23.177 |
| Notifications | 20.829 |
| Members | 18.197 |
| MembersBack | 20.701 |
| Pin | 19.816 |
| PinWrong | 18.619 |
| ImePinyin | 32.647 |
| ImeCandidates | 27.239 |
| ImeEdit | 33.666 |
| ImePassword | 29.552 |
| ImeVoice | 29.947 |
| Confirm | 24.931 |
| ConfirmDelete | 26.084 |
| ConfirmSecret | 27.869 |
| AiListen | 33.584 |
| AiThink | 32.902 |
| Ai | 37.348 |
| Settings | 16.222 |
| SetDisplay | 18.817 |
| SetSound | 20.973 |
| SetNetwork | 35.519 |
| PowerMenu | 27.167 |
| PowerOff | 16.877 |
| PhoneIncoming | 19.966 |
| ClockRing | 19.637 |

## 改过的已有测试

- `tests/ui_scale_check_test.py`：新增负例与合法对照；原先无依据的未知参数会被放行，现须具名运行时理由，因此原合法作用域例增加该理由；另加未知参数必须失败的反例。没有放宽尺寸集合。
- `tests/ui_shell_regressions.c`：新增 privacy、answer、snapshot、alarm 四种流程，两设备档案均执行；滑杆多检查三个图标，确认等旧断言不删。新工具测试按文字定位对应圆形按钮中心，不把旁边不可点击的标题当按钮。
- `tests/integration/notify.c`：闹钟文字期望改为“再睡 10 分钟/这次不再响”；真实闹钟视觉顺序为先小睡再停止，三处触摸索引相应改为停止第二个、小睡第一个。通用合成打断的原顺序不变；所有停止、再响、功耗和声音断言与时限保留，第 657 行失败断言未改。
- `tests/ui_shell_scenes.c`：ClockRing 的触摸目标从“关闭”改为“这次不再响”，仍必须验证真实打断结束。没有改旧脚本、预算或原取图稳定条件。
- 上轮已改测试的理由仍在历史报告 `a429a48:docs/tasks/ui-shell/REPORT.md`；本轮没有改 `listening.c`、`dictation.c`、驱动或模拟器产品源码。

## 要用户定的

没有新增决策。按已给合同完成可逆修复，没有把剩余缺口改称“不需要”或改产品范围，也没有请求额外确认。

## 顺手发现

【实现】按 R-061 审视：导航/呈现仍在 UI，按钮和 AI 走同一能力总线；成员变化经状态订阅隔离会话，界面不写服务内部持久状态。Apps 共用目录、通知复用绘制记录、键盘读取可见候选，减少画面与快照分叉；密码仍不对 AI 开放。唯一服务层小增量是 `system/services/notify/notify_service.c` 的闹钟动作标签和显式 `minutes=10`，无新服务接口、任务、依赖或能力例外。005 的未完流程仍限制整体 AI 原生完整性。

【实测】结束检查：相对 `024c800` 的本轮差异 `git diff --check` 退出 0；本机路径/MAC 正则对本轮提交及最终工作区差异均 0 命中，17 个相关文件，无文件超过 5 MiB。报告相对链接均可解析。修改过的 C 文件去掉注释/字符串后没有 `write/read/fgets` 调用，因此没有新增忽略返回值的写法；不据此宣称已用 Linux gcc 构建。

【实测】完整旧任务基线 `git diff --check fdff779..HEAD` 仍退出 2：Outfit-OFL、NanoSVG LICENSE 的尾空格/CRLF，以及 nanosvgrast 缩进告警；未为消除告警修改第三方原文。本轮构建产物、反向副本、日志、截图均未提交；依赖和设计稿只读。没有硬件、串口、付费接口、真实凭据、远端写入或窗口 GUI 操作。
