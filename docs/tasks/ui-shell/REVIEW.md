# ui-shell 第 4 轮复查

复查日期：2026-11-06（Asia/Singapore）。任务基线 `fdff779`，对象 `3578159`。依据复查提示、任务书、总规则、规范及执行报告。工作区旧报告开工时为空，上一轮结论读取自 `HEAD:docs/tasks/ui-shell/REVIEW.md`。

**结论：未通过整体验收；严重 0 项，高 2 项。** 上轮两项高问题的原反例均已修复，反向对照有效；追加探针仍发现尺寸写入及图标宏引用的门禁漏检，详见下文。普通与 ASan/UBSan 全量均为 **136/141**，失败项一致；30 屏对照图重新生成，**0/30** 达到视觉目标。开发报告已披露全量失败及功能、视觉欠项，不认定为伪称整体验收完成。

只改本报告，不改产品源码、永久测试或预算，不提交。生成物只落在忽略的构建和输出目录。

构建最多 `-j4`，ctest 最多 `-j3`；构建和全量测试串行。不启动带窗口的模拟器，不操作硬件、真实凭据、付费接口或远端。

## 高问题

### UI-R4-001（高）：尺寸检查忽略部分实际写入，仍按旧值放行

- 文件和行号：`tools/ui_scale_check.py:32`、`:147`、`:149`、`:156`；相关自检 `tests/ui_scale_check_test.py`。
- 【实现】写入收集只识别以分号结束的简单变量赋值。条件表达式中的赋值、自增、数组元素写入不进入写入事件；检查后续尺寸调用时，仍把初始化值当成有效候选，没有按“无法证明”拒绝。
- 【实测】下方条件赋值和数组改写均返回 `[]`；直接传 `123`、上轮无花括号条件反例均正确报错。另以本机 `cc -Wall -Wextra -Werror -x c -` 编译最小调用探针，两个样例编译、运行均退出 0，实际分别输出 `width=123`、`font=123`。不是不可编译的伪 C 片段。
- 影响：U1 自检通过仍不能证明实际使用的尺寸符合规格；常见 C 写法可以把非法值送入真实尺寸/字号入口。本轮没有证明原版产品正在使用 123 px，不将探针结果写成现有界面故障。
- 修法：补足被支持表达式里的写入识别；遇到暂不支持的写入或可变表，保守将相关值标为未知并拒绝，或明确限制为不可变声明并检查限制。补条件内赋值、数组写入、自增的负例及合法对照，不必继续扩成通用 C 编译器。

复现命令（不写产品文件）：

```sh
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
from unittest.mock import patch
import runpy
check = runpy.run_path('tools/ui_scale_check.py')['check']
cases = {
    'direct': 'lv_obj_set_width(o,123);',
    'old-case': 'int w=123; if(flag) w=104; lv_obj_set_width(o,w);',
    'condition-write': 'int w=104; if ((w=123)) {} lv_obj_set_width(o,w);',
    'array-write': 'int sizes[]={32,44}; sizes[0]=123; ink_ui_font_px(sizes[0],0);',
    'increment': 'int w=104; w++; lv_obj_set_width(o,w);',
}
for name, source in cases.items():
    with patch.object(Path, 'read_text', return_value=source):
        print(name, check(Path('probe.c')))
PY
```

结果：前两项报 `illegal UI dimension 123`；后三项 `[]`。自增探针实际传入 105。以上漏检按同一根因计一项。

### UI-R4-002（高）：普通宏引用的缺失图标仍能通过构建门禁

- 文件和行号：`tools/icongen/check.py:102`、`:111`、`:118`、`:148`；真实引用 `system/ui/shell/navigation.c:209`；构建接入 `CMakeLists.txt:338`；运行后果 `system/ui/icons.c:23`。
- 【实现】解析器收集字符串变量、表和 `INK_UI_ICON(...)`，不展开普通 `#define`；解析不到的图标表达式返回空列表，按没有引用处理。生成的名称宏只约束主动使用 `INK_UI_ICON` 的调用，原有字符串 API 仍接受普通宏。
- 【实测】在隔离副本中，将真实 `APP_ICONS` 首项换成 `REVIEW_HEADPHONES`，定义为不存在的 `"missingReviewIcon"`。复制仓库**原样的** `ink_ui_resource_check` CMake 目标，配置及构建检查退出 0，输出 `80 names, 240 SVG files, 0 errors`；该导航源码与当前产品库编译、链接也退出 0。运行 Main 在虚拟时间 **0.158689 s** panic，退出 2：`missing or invalid required icon: icons/missingReviewIcon-line-64.inki`。
- 影响：U4 要求的“用到的图标缺一个就构建失败”仍未成立。当前原版图标名称正确；这里是构建门禁缺口，不是原版正常启动必崩。
- 修法：对所有实际图标引用强制使用可验证的注册 ID，或让检查器解析宏并对无法解析的引用失败。只为数组增加一种推荐写法不足以保证门禁；补真实表经普通宏引用缺失名称的构建负例。

简短复现（检查全部实际源文件，并编译、运行隔离副本；单次串行编译）：

```sh
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
import json, os, runpy, subprocess, sys
root = Path.cwd(); build = root / 'build'
sys.path.insert(0, str(root / 'tests'))
from ui_shell_reverse import compile_variant
p = Path('system/ui/shell/navigation.c')
needle = 'APP_ICONS[] = {INK_UI_ICON(headphones)'
source = p.read_text(); assert source.count(needle) == 1
mutant = '#define REVIEW_HEADPHONES "missingReviewIcon"\n' + source.replace(
    needle, 'APP_ICONS[] = {REVIEW_HEADPHONES', 1)
sources = {f: f.read_text() for d in ('system/ui', 'system/include', 'apps')
           for f in Path(d).rglob('*') if f.suffix in ('.c', '.h')}
sources[p] = mutant
check = runpy.run_path('tools/icongen/check.py')['check_sources']
names = set(json.loads(Path('assets/icons/manifest.json').read_text())['names'])
print('errors:', check(sources, names))  # 实测 []，应拒绝
binary = compile_variant(build, 'r4-macro-icon', mutant, 'ink_system', 'sim-ui-scenes')
print('compile/link: exit 0')
r = subprocess.run([str(binary), '--profile', 'simulator/profiles/metalio-plus.json',
    '--script', 'tests/ui_shell.script', '--hub', 'tests/ui_shell.hub.json',
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
    '--budget', 'tests/ui_scene.budget.json', '--out',
    str(build / 'test-out/review-negative-r4-macro-icon'), '--quiet', '--no-random-outages'],
    env=dict(os.environ, INK_UI_SCENE='Main'), capture_output=True, text=True, timeout=45)
print('Main exit:', r.returncode); print(r.stdout + r.stderr)
PY
```

本轮另外实际构建的完整门禁隔离目录为 `build/review-negative/r4-icon-build/`；其中 `navigation.c` 是副本，其余输入为只读使用的符号链接。可重跑 `cmake --build build/review-negative/r4-icon-build/build --target ink_ui_resource_check -j1` 观察退出 0。未对原版源码进行临时替换。

## 上轮问题复核

命令 B/T、N、V 见下方独立运行记录。已关闭的旧高问题不重复计数；未闭合门禁以本轮新反例计数。

| 编号、原级别 | 位置 | 本轮结果、复现与处理建议 |
| --- | --- | --- |
| UI-R3-001，高（延续 UI-R2-001） | `tools/ui_scale_check.py:32`、`:147` | 【实测】原无花括号条件赋值反例已拒绝；N 的旧检查器对当前自检产生 25 次失败、0 个错误，证明旧修复可被测到。但新写入反例仍漏检，按 UI-R4-001 继续修，不算完全关闭 U1。 |
| UI-R3-002，高 | `tools/icongen/check.py:102`；`system/ui/shell/navigation.c:209` | 【实测】真实 APP_ICONS 经 `INK_UI_ICON(missingReviewIcon)` 变异：旧检查退出 0、新检查退出 1；真实 CMake 门禁隔离变异退出 2、恢复后退出 0。生成名称表也使合法名编译成功、缺失名编译失败。原反例修到位；普通宏缺口见 UI-R4-002。 |
| UI-R2-002，高 | `system/ui/shell/navigation.c:58`、`:63`、`:330` | 【实现/实测】会话 epoch 与当前成员检查存在，T 成员隔离回归通过。N 撤回旧状态清理产生 5 次失败，单撤迟到回执保护产生 1 次失败。该旧问题修复有效，保留这些回归。 |
| UI-R2-003，中 | `system/ui/shell/ai_panel.c:45`、`:65` | 【实测】完整回答分页回归通过；N 旧分页产生 11 次失败，窄四位页码产生 1 次失败；V 的 Ai 当前页文字未溢出。旧问题修复有效，不把整页主动作欠项混入分页结论。 |
| UI-R2-004，中 | `system/ui/shell/navigation.c:241`、`:478`；`system/services/display/display_service.c` | 【实测/目视】T 主页响应预算仍失败，V 的 Today、Apps、键盘有残影；局部内容仍以清理重建为主。未修到位。应保留可复用对象、限制脏区并核对两段刷新及最终灰阶，不加等待或放宽预算。 |
| UI-R2-005，中 | `system/ui/shell/navigation.c:578`、`:606`；`system/ui/shell/ai_panel.c:159` | 【实现/实测】Apps/通知快照及键盘覆盖改进，N 旧快照产生 7 次失败、旧键盘产生 4 次失败。但字号主要影响示例，声音选项、部分设置和 AI 主动作仍未闭环。部分修复；继续接完整服务/原子能力，按钮与 AI 共用入口。 |
| UI-R2-006，中 | `system/ui/shell/navigation.c:550`；`system/ui/shell/keyboard.c:207`；`system/ui/shell/pinpad.c:39` | 【实测/目视】分段边框、Apps 搜索图标、缩放图标范围、键盘固定区域已改善；N 各撤回产生 24、1、1、2 次失败。其余布局差异见 12 屏记录，仍部分完成。按规范逐屏修正，不以改规格追分。 |

旧中问题只复核既有范围与证据，不继续扩大调查。下列全量失败保留为验收阻塞项；未定位的故障不凭测试名推定数据丢失、永久卡死或成员越权。

## 逐项验收

| 编号 | 结论 | 独立证据与命令 |
| --- | --- | --- |
| U1 | 未通过 | 【实测】T 原版检查及自检通过、N 原反例有效；UI-R4-001 新反例漏检。 |
| U2 | 部分完成 | 【实测】T 三档组件场景通过，N 组件撤回有效；【目视】V 仍有 Dock、分组行、面板信息层次差异，组件存在不能替代外观验收。 |
| U3 | 本机构建通过，体积及跨平台未完成 | 【实现/实测】B 生成三字重、Outfit 数字子集及缺字回退；缺必要覆盖会明确失败。29 个字体共 7,538,204 字节，比 4 MiB 多 3,343,900。Outfit/OFL、字重来源及固定哈希存在；Ubuntu 未跑，CI apt 仍缺任务指定的 `fonts-noto-cjk-extra`。 |
| U4 | 未通过 | 【实测】B 原版资源检查通过，80 名称、240 SVG、1,600 位图；UI-R4-002 缺失图标经普通宏仍通过真实门禁。 |
| U5 | 本轮回归通过 | 【实测】T 键盘、滑杆、边缘、前台及实体键优先级场景通过；N 旧分派、旧触摸层均被检出。 |
| U6 | 本轮回归通过 | 【实测】T 打断层级、输入归属及关闭后帧缓冲/模拟物理面板恢复通过，N 确认保护及恢复反例有效；不代表所有实机时序已验。 |
| U7 | 未通过 | 【实测/目视】T 主页响应预算超限，V 残影仍在；存在两段提交不等于刷新表及局部刷新完整通过。 |
| U8 | 部分完成 | 【实测】V 30 场景均进入并取图，T 代表动作通过；【实现/目视】声音、设置、AI、密码等完整流程/布局尚未完成。 |
| U9 | 部分完成 | 【实测】T 遍历 30 屏快照、身份、动作与能力表关系，成员隔离及密码保护回归通过；【实现】部分设置及 AI 主动作未齐。已注册动作不能代替完整操作闭环。 |
| U10 | 工具通过，视觉未通过 | 【实测】V 不复用缓存，30 屏新图，0/30 达到 ≤12/255；退出 0 只表示生成成功。 |
| U11 | 未通过 | 【实测】T 两套均 136/141；已查 AI 最低内部余量 33,192 ≥32,768；既有预算未放宽。 |
| U12 | 无窗口验证通过，有重试记录 | 【实测】T 的 SDL dummy `gui-smoke`、主页三屏、控制中心和设置场景通过。普通 GUI smoke 首轮未观测到翻页，使用既有重试后通过；ASan 首轮通过，不称普通版本单次稳定通过。 |
| U13 | 本机结束检查已做，整体未完成 | 【实测】B/T/N/V、范围/隐私检查已复跑；Linux、目标链接/分区与设备未验。本次复查只写 REVIEW，不作开发提交。 |

## 独立运行记录

使用用户指定的 `INK_DEPS`、`INK_DESIGN`。本轮重新配置普通和内存检查目录，再增量构建；macOS 编译器是 Apple Clang，未执行 Linux gcc 构建。第二套配置及编译/链接参数已核对含 AddressSanitizer、UndefinedBehaviorSanitizer 和 WERROR。

```sh
# B/T：以下构建和全量测试依次执行，没有并行启动两轮
cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake -S . -B build-asan -DINK_WERROR=ON -DINK_SANITIZE=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
# N：分别串行运行；变异编译在隔离目录，不替换产品源码
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_r3_reverse.py --components
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_r2_reverse.py
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_reverse.py
# V：重新生成，没有 --reuse
"$INK_DEPS/build-deps/pyenv/bin/python" -B tools/ui_compare.py --design "$INK_DESIGN"
```

- 【实测】两套配置、构建均退出 0。普通 T：136/141，37.52 秒，退出 8；ASan/UBSan T：136/141，73.98 秒，退出 8。
- 【实测】两份 `Testing/Temporary/LastTest.log` 未见 ASan、LeakSanitizer、UBSan 或 runtime error 诊断；这不等于全量测试通过。
- 【实测】N 三次均退出 0，表示脚本要求的正/反向结果成立。除上表外，撤回呈现等待失败 1 项、旧确认卡失败 18 项、旧说明截断失败 2 项、旧动作列表失败 1 项、旧分派失败 9 项、旧滑杆映射失败 30 项、旧触摸层失败 87 项。不是只运行正向测试。
- 【实测/实现】`gui-smoke` 显式使用 `SDL_VIDEODRIVER=dummy`。普通首轮缺 `reader.page_turns >= 1`，既有脚本第二轮通过，总计 32.49 秒；ASan 首轮通过，16.73 秒。非崩溃失败的单次重试在基线已存在，本轮没有增加。见 `tests/simulator/gui_smoke.cmake` 和上述测试日志。

两套全量相同的五个失败（数值取本轮普通产物）：

| 项目 | 本轮结果 | 判断与处理边界 |
| --- | --- | --- |
| `scenario-home_basic-metalio-plus` | 首变化 p95 431.851 >400 ms；FAST 686.565 >600；TEXT 1928.191 >1,200 | UI 刷新欠项仍在；应减少重建/刷新开销，不能提高预算。 |
| `scenario-home_basic-metalio-plus-psram16` | TEXT p95 1707.372 >1,200 ms | 较快内存配置仍失败。 |
| `integration-listening` | `tests/integration/listening.c:97`，锁后等待 70 秒，睡眠计数未增长；42 通过、1 失败 | 根因未定位，不推定所有音频场景永久阻止睡眠；保留断言调查睡眠入口。 |
| `integration-notify` | `tests/integration/notify.c:657`，提醒已入库时声音计数未增长；177 通过、1 失败 | 未区分事件观测顺序与最终播放失败，不宣称永远无声；保留断言核查发布/播放时序。 |
| `scenario-reader_basic-panel-power-faults` | `display.power_failures=1`，允许 0 | 故障注入下失败保留，未改驱动或预算。 |

本轮普通产物资源独立核对：

| 场景 | 内部最低余量/字节 | DMA 最低余量/字节 | PSRAM 最低余量/字节 |
| --- | ---: | ---: | ---: |
| `ai_ask-metalio-plus` | 35,856 | 31,328 | 10,974,840 |
| `ai_ask-metalio-plus-pqbn` | 33,192 | 28,616 | 10,120,064 |
| `ai_tools-metalio-plus` | 34,912 | 31,328 | 10,974,960 |
| `home_basic-metalio-plus` | 48,816 | 48,752 | 12,361,168 |

内部字段为 `mem.internal_total.min_free_bytes`，最低已查 AI 仅高于门槛 **424 字节**。29 个字体 **7,538,204** 字节，1,600 个图标 **20,308,480** 字节，合计 **27,846,684** 字节。字体超过 4 MiB 目标 **3,343,900** 字节，目标未改。这里只独立确认当前产物；开发报告的修前数值未重建基线复测。模拟器内存模型及本机构建大小不替代目标链接/分区与实机测量。

## 视觉复核

开工已看 Foundation、Components、Main、Today、Control、Settings 原稿。本轮打开以下 **12 张新生成的三栏对照图**（左设计、中实现、右差分）；其余 18 屏运行并核对分数，不称全部目视。真实空状态、夹具中的示例数据、范围外阅读底页不作为假实现问题。

| 已目视页面 | 差值 /255 | 明显差异或本轮修复结果 |
| --- | ---: | --- |
| Main | 15.345 | 布局 A 保留；时钟字形/宽度、页码点位置不同，Dock 多出圆形淡边。 |
| Today | 24.541 | 日期及“继续听/今日报纸”已补；仍有旧 08:00、日期、图标文字残影，卡片稍下移且边线偏重。 |
| Apps | 13.995 | 搜索图标已补且不遮文字；旧时钟和图标残影明显，分组标题层次及图标位置不同。 |
| Control | 23.177 | 滑杆图标存在；网络行变为描边胶囊，缺状态图标/箭头层次；底部把手变为“收起”胶囊。 |
| Settings | 16.222 | 缺右上搜索；家庭卡只有居中姓名，缺头像/家人数层次；分组分隔线及列表图标边线不齐。 |
| SetDisplay | 18.510 | 分段边框已完整；“屏幕”分组变两个胶囊，缺“有残影时用”；暖光图标样式不同，字号主要作用于示例。 |
| SetSound | 20.973 | 勿扰脱离提示分组；提示音/振动仍为“还没有可选设置”，铃声只读；图标及行分隔线不同。 |
| ImeCandidates | 27.015 | 键盘已固定在同一区域；候选仍非设计的五列三行网格，分页/收起分布不同，有旧键盘残影。 |
| PinWrong | 18.619 | 缺姓名、四位密码槽、忘记密码；底行为删除/0/确定，设计为取消/0/退格，取消路径未对齐。 |
| Ai | 37.348 | 当前页文字未溢出，页码 1/2；主动作仍为“继续问”，缺“听解释/更多”及来源书本图标。 |
| ConfirmDelete | 26.084 | 删除动作词正确；仍用 AI 图标，面板更高、缩略图/说明层次合并。文件名属于测试夹具。 |
| ClockRing | 19.637 | 标题、时间、日期已分离；“再睡 10 分钟/这次不再响”正确，数字比例及图标位置与画板不同。 |

ClockRing 的数字大小不单凭差分分数判错：规范区分 176 px 时钟与 88 px 大数字，本实现取 88 px。应按规范语义统一这处画板差异，不能为追分新增字号。

其余 18 屏本轮分数如下；全部 30 屏均高于 12/255，最低 Apps 仍为 13.995。原始表见 `out/ui-compare/scores.md`，不提交图及构建产物。

| 页面 | 差值 /255 | 页面 | 差值 /255 |
| --- | ---: | --- | ---: |
| Lock | 17.630 | LockPage | 37.596 |
| Notifications | 20.829 | Members | 18.197 |
| MembersBack | 20.701 | Pin | 19.816 |
| ImePinyin | 32.647 | ImeEdit | 33.666 |
| ImePassword | 29.636 | ImeVoice | 29.947 |
| Confirm | 24.931 | ConfirmSecret | 27.869 |
| AiListen | 33.584 | AiThink | 32.902 |
| SetNetwork | 35.636 | PowerMenu | 27.167 |
| PowerOff | 16.877 | PhoneIncoming | 19.966 |

## 范围、已有测试与结束检查

- 【实现】相对基线服务/策略增量逐个核对：`system/include/ink/sys/display.h`（+2）、`system/include/ink/sys/ui.h`（+6）为显示/UI 接口；`system/policy/cap.c`（+4/-3）为动作页/成员动作；`system/policy/confirm.c`（+35/-17）为完整描述及拒绝截断；`system/services/ai/ai_service.c`（+17/-43）为新呈现层接入；`system/services/display/display_service.c`（+61/-10）为两段队列；`system/services/notify/notify_service.c`（+3/-2）为闹钟文字与 10 分钟操作。未见本轮新增常驻任务。未完成的 AI/设置流程仍限制“软总线 + 原子能力”完整性，能力表非空不能替代操作闭环。
- 【实现】已有测试改动核对：阅读坐标移到 Dock；确认等待实际呈现并检查完整内容，超容量说明拒绝旧截断；闹钟按钮索引随新顺序更新；Apps 去旧计数后期望 5→4，同时新增无权限应用不暴露断言。未发现靠抬预算、删声音/睡眠断言掩盖本轮失败。
- 【实现】`standby.script` 注入时间 1,180→1,420 ms 的历史解释是重新命中睡眠入口竞争，原 `power.sleep_cancelled` 断言保留且通过。本轮未另重放基线轨迹，不将历史解释当作独立时序证据。
- 【实现】任务书/总规则变更明确资源限制及无窗口验证，没有放宽产品验收。既有 `.github/workflows/sim.yml` 仍有无数字 `-j`，本轮未执行，不能以既有脚本为由突破当前硬性限制。
- 【实测】基线以来 **328** 个变更文件；既有 `*.budget.json` 修改数 **0**，变更文件无单个超过 5 MiB。差异扫描本机路径及 MAC 格式 **0 命中**。修改过的 C/H 去掉注释和字符串后，`write`、`read`、`fgets` 调用数 **0**；未发现新增忽略其返回值的代码，不能据此称 Linux gcc 已验证。
- 【实测】`git diff --check fdff779..HEAD` 退出 **2**，告警仅涉及 Outfit-OFL 原文尾空白、NanoSVG LICENSE 的 CRLF/尾空白及 `nanosvgrast.h:782` 缩进。未为消除告警改第三方原文。本报告的差异检查通过，追踪文件仅本报告修改。
- 没做到的仍是上表列明的门禁、刷新、完整页面流程、字体体积、视觉及跨平台验收。未新增需用户选择事项；没有将欠项改称“不需要”，没有修改规范、预算或需求。没有硬件操作、远端写入或提交。

SEVERE=0 HIGH=2
