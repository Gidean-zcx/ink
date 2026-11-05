# ui-shell 第 3 轮复查

复查时间：2026-11-05 至 11-06（Asia/Singapore，跨午夜）。任务基线 `fdff779`，复查对象 `27040e3`。依据复查提示、任务书、总规则、界面规范及开发报告；上轮报告从 `HEAD:docs/tasks/ui-shell/REVIEW.md` 读取。

**结论：未通过整体验收。严重 0 项，高 2 项。** 上轮成员隔离问题已修到位；尺寸门禁仍有可复现漏检，另发现图标构建检查漏检应用图标表。普通及 ASan/UBSan 全量均为 **135/140**，30 屏视觉均未达标。开发报告已披露五个失败及功能、视觉欠项，不把已披露欠项认定为伪称整体验收完成。

本次仅覆盖本文件，不修改产品源码、永久测试或预算，不提交。构建与全量测试串行，分别最多 `-j4`、`-j3`；反向副本单个编译执行。未启动窗口模拟器，未操作硬件、真实凭据、付费接口或远端。依赖与设计稿只读；生成物留在忽略的构建及输出目录。

## 高问题

### UI-R3-001（高）：尺寸门禁把无花括号的条件赋值当成必定执行

- 位置：`tools/ui_scale_check.py:81`、`:117`、`:124`；回归覆盖在 `tests/ui_scale_check_test.py`。
- 【实现】`symbols_before` 仅用花括号建立作用域；同一作用域的 `=` 覆盖此前候选值。无花括号的条件赋值被当成无条件赋值，丢掉真实可达的非法值。
- 【实测】下方两个非法条件分支返回 `[]`；直接非法字面量和带花括号的对照均被拒绝。上轮简单赋值、别名、数组反例已修，但 U1 和 UI-R2-001 不能完整关闭。
- 影响：普通 C 条件写法即可让规格外字号/尺寸通过门禁；现有自检全绿仍不能证明所称检查有效。未证明当前产品页面正在使用 123 px，故不是现有运行界面的严重故障。
- 修法：识别单语句条件分支，或在不能证明赋值无条件发生时保守保留候选/拒绝；补上带/不带花括号、条件两路的负例。不必实现通用 C 数据流分析。

复现（仓库根目录，不写源码）：

```sh
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
from unittest.mock import patch
import runpy
check = runpy.run_path('tools/ui_scale_check.py')['check']
cases = {
    'direct': 'lv_obj_set_width(o, 123);',
    'braced': 'int w=123; if(flag) {w=104;} lv_obj_set_width(o,w);',
    'font': 'int px=123; if(flag) px=32; ink_ui_font_px(px, INK_WEIGHT_REGULAR);',
    'if-else': 'int w=104; if(flag) w=123; else w=104; lv_obj_set_width(o,w);',
}
for name, source in cases.items():
    with patch.object(Path, 'read_text', return_value=source):
        print(name, check(Path('probe.c')))
PY
```

结果：`direct`、`braced` 报 `illegal UI dimension 123`；`font`、`if-else` 无错误。预期四者均失败。

### UI-R3-002（高）：应用图标表引用缺失图标时，构建检查仍通过

- 位置：`tools/icongen/check.py:17`；实际引用 `system/ui/shell/navigation.c:199`、`:209`；构建接入 `CMakeLists.txt:338`；运行后果 `system/ui/icons.c:22`。
- 【实现】检查器只识别 `ink_ui_icon/ink_ui_circle` 第二参数直接写字符串的调用。主页 `APP_ICONS`、分组表、转发参数等不在检查范围；构建按 manifest 生成资源，不从这些引用建立依赖或验证名称。
- 【实测】仅在内存中把 `APP_ICONS` 首项 `headphones` 换成 `missingReviewIcon`，门禁仍输出 `80 names, 240 SVG files, 0 errors`，退出 0；隔离副本编译、链接退出 0。运行 Main 在虚拟时间 **0.158689 s** panic：`missing or invalid required icon: icons/missingReviewIcon-line-64.inki`，退出 2。直接字符串调用同一名称的对照被门禁拒绝，退出 1。
- 影响：U4 的“用到的图标缺一个就构建失败”没有覆盖产品实际采用的引用方式，错误留到运行期暴露。当前提交的实际名称正常；这是构建门禁缺口，不能把变异副本的 panic 描述成当前版本正常启动失败。
- 修法：让应用/分组图标引用进入可验证的声明或生成注册表，并接入构建检查；补一个从真实 `APP_ICONS` 注入未知名称的负例，要求构建阶段拒绝。

复现（普通构建已完成；仅生成忽略目录中的副本，串行编译）：

```sh
"$INK_DEPS/build-deps/pyenv/bin/python" -B - <<'PY'
from pathlib import Path
from unittest.mock import patch
import os, runpy, subprocess, sys
root = Path.cwd()
sys.path.insert(0, str(root / 'tests'))
from ui_shell_reverse import compile_variant
p = (root / 'system/ui/shell/navigation.c').resolve()
original = Path.read_text
needle = 'static const char *const APP_ICONS[] = {"headphones"'
source = p.read_text()
assert source.count(needle) == 1
mutant = source.replace(needle,
    'static const char *const APP_ICONS[] = {"missingReviewIcon"', 1)
def fake_read(self, *a, **k):
    return mutant if self.resolve() == p else original(self, *a, **k)
with patch.object(Path, 'read_text', fake_read):
    try: runpy.run_path('tools/icongen/check.py', run_name='__main__')
    except SystemExit as e: print('gate exit:', int(e.code))
build = root / 'build'
binary = compile_variant(build, 'r3-missing-app-icon', mutant,
                         'ink_system', 'sim-ui-scenes')
print('compile/link: exit 0')
args = [str(binary), '--profile', 'simulator/profiles/metalio-plus.json',
        '--script', 'tests/ui_shell.script', '--hub', 'tests/ui_shell.hub.json',
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
        '--budget', 'tests/ui_scene.budget.json', '--out',
        str(build / 'test-out/review-negative-r3-missing-app-icon'),
        '--quiet', '--no-random-outages']
r = subprocess.run(args, env=dict(os.environ, INK_UI_SCENE='Main'),
                   capture_output=True, text=True, timeout=45)
print('run exit:', r.returncode)
print(r.stdout + r.stderr)
PY
```

## 上轮问题复核

| 上轮编号 | 本轮结论 | 独立证据 |
| --- | --- | --- |
| UI-R2-001（高） | 部分修复，不能关闭 | 简单赋值、数组及旧反例已拒绝；旧检查器在当前自检中失败 20 项。但 UI-R3-001 仍漏检，不重复计数 |
| UI-R2-002（高） | 已修 | 两档案 privacy 回归通过；实际切换儿童后对象/快照不含成人搜索词。恢复旧壳失败 5 个断言；仅撤掉晚到 Wi-Fi 回执会话守卫失败 1 个。核查了成员会话、搜索/密码回调及异步回执守卫 |
| UI-R2-003（中） | 已修 | 实际字体和宽高分页，逐页重组不丢字；旧分页失败 11 个断言，撤回四位页码栏修复失败 1 个；新 Ai 图可见分页按钮和页数 |
| UI-R2-004（中） | 未修 | T 的两项主页刷新预算仍失败；V 的 Today/Apps 稳定帧仍有旧主屏残影。位置 `navigation.c:244`、`:478` 的内容重建及显示服务刷新路径。需保留对象并按脏矩形核验反馈/补灰，不能靠延时或抬预算关闭 |
| UI-R2-005（中） | 部分修复 | Apps/通知旧快照失败 7 个断言，旧键盘快照失败 2 个；新快照和服务动作回归通过。`navigation.c:578` 的字号主要落在示例、`:606` 的声音行仍无完整操作，AI 主动作仍欠缺；用 T 的上下文回归及 V 的 Ai/SetSound 复现。需补能力和服务闭环，再验 touch/AI 一致性 |
| UI-R2-006（中） | 部分修复 | 旧闹钟视图/动作分别失败 2/5 个断言，撤掉滑杆图标失败 3 个；词语及小睡 10 分钟已修。`navigation.c:535` 附近设置布局、`keyboard.c:214` 附近键盘、`member_gate.c:68` 附近密码视图等仍欠整体对齐；运行 V 复现下表差异，按规范逐屏修正组件与真实流程 |

反向脚本均实际执行，不以脚本存在代替证据。第一轮反向也重跑：待呈现帧的正向对照通过，撤掉稳定取图等待失败；旧确认卡、旧说明截断、旧事件分派、旧滑杆映射/触摸层及旧上下文均被检出。未发现确认由 AI 代替人完成的新路径。

## U1～U13 验收

B/T/N/V 对应下节命令；【实测】为本次运行，【实现】为本次代码核查，【目视】为本次实际打开的图。

| 编号 | 结论 | 本次证据与边界 |
| --- | --- | --- |
| U1 | 未通过 | 【实测】常规门禁、自检通过；新增反向探针漏检，见 UI-R3-001 |
| U2 | 部分完成 | 【实测】组件场景通过；【目视】Dock、分段、分组行、面板高度仍不齐，组件存在不能通过外观验收 |
| U3 | 本机构建通过，体积/跨平台未完成 | 【实测】29 字体 7,538,204 字节，比 4 MiB 多 3,343,900；三字重、数字子集及回退资源生成。Outfit/OFL、NanoSVG 许可及固定哈希存在。Ubuntu 未跑；CI apt 尚未包含任务指定的 `fonts-noto-cjk-extra`，CMake 的缺失字重下载回退不等于指定 CI 路径已验 |
| U4 | 未通过 | 【实测】原版资源检查通过，80 名称、240 SVG、1,600 位图；真实表引用缺失未在构建期拦截，见 UI-R3-002 |
| U5 | 本轮回归通过 | 【实测】键盘、滑杆、系统边缘、前台、实体键优先级通过，旧分派/触摸层反向确实失败 |
| U6 | 本轮回归通过 | 【实测】打断层级、输入归属、帧缓冲及物理面板恢复通过；成员隔离和迟到回执回归有效。不代表全部实机时序已验 |
| U7 | 未通过 | 【实测/目视】主页响应预算和残影仍失败，局部重建与两段刷新完整性未完成 |
| U8 | 部分完成 | 【实测】30 场景成功进入并稳定取图；【实现/目视】声音、AI、设置、密码等完整交互/布局仍欠验收，入口通过不等于页面完成 |
| U9 | 部分完成 | 【实测】30 屏身份、快照/能力表关联及代表动作通过；新增 Apps/通知/键盘内容覆盖有效。完整 AI 主动作和部分设置原子能力仍未齐 |
| U10 | 工具通过，视觉未通过 | 【实测】V 不复用缓存，30 屏新图；0/30 达到 ≤12/255。退出 0 仅表示生成成功 |
| U11 | 未通过 | 【实测】两套均 135/140。已核对 AI 最低内部余量 33,192 ≥32,768；预算未放宽 |
| U12 | 无窗口验证通过 | 【实测】SDL dummy 的 `gui-smoke`、主页三屏、控制中心、设置场景通过；未启动窗口版 |
| U13 | 本机检查已做，整体未完成 | 【实测】构建、全量、内存检查、反向、视觉、范围/隐私已复跑；Linux、目标链接/分区、设备端未验。本次复查不作开发提交 |

## 独立运行记录

使用用户指定的 `INK_DEPS`、`INK_DESIGN`。沿用并核查已有配置：均 `INK_WERROR=ON`，第二套 `INK_SANITIZE=ON`，依赖指向指定目录。**本次为 macOS 增量构建，没有重新配置，也没有 Linux 构建。**

```sh
# B/T：按顺序串行，前一项结束才启动下一项
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
# N：串行编译内存中的变异副本
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_r2_reverse.py
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_reverse.py
# V：不使用 --reuse
"$INK_DEPS/build-deps/pyenv/bin/python" -B tools/ui_compare.py
```

- B 两次退出 0。普通 T：135/140，39.57 秒，退出 8；ASan/UBSan T：135/140，75.09 秒，退出 8。UI 前 54 项均通过。
- 两份 `Testing/Temporary/LastTest.log` 未见 ASan、LeakSanitizer、UBSan 或 runtime error 诊断；这不等于全部测试通过。
- N 两次退出 0，表示各自正/反向预期成立。V 退出 0，30 份场景及 `out/ui-compare/scores.md` 重新生成。

两套全量相同的五个失败：

| 项目 | 本次证据 | 判断边界 |
| --- | --- | --- |
| `scenario-home_basic-metalio-plus` | 首变化 p95 431.634 >400 ms；FAST 686.348 >600；TEXT 1927.975 >1,200 | 刷新欠项仍在 |
| `scenario-home_basic-metalio-plus-psram16` | TEXT p95 1707.372 >1,200 ms | 较快内存配置仍失败 |
| `integration-listening` | `tests/integration/listening.c:97`，锁定后等 70 秒睡眠计数未增长；42 通过、1 失败 | 根因未定位，不扩写为所有音频永久阻止睡眠 |
| `integration-notify` | `tests/integration/notify.c:657`，提醒已入库时声音计数未增长；177 通过、1 失败 | 尚不能区分发布/观测顺序与最终播放失败，不断言永远无声 |
| `scenario-reader_basic-panel-power-faults` | `display.power_failures=1`，允许 0 | 注入故障下失败保留，未改驱动或预算 |

本次普通产物资源独立核对：

| 场景 | 内部最低余量/字节 | DMA 最低余量/字节 |
| --- | ---: | ---: |
| `ai_ask-metalio-plus` | 35,856 | 31,328 |
| `ai_ask-metalio-plus-pqbn` | 33,192 | 28,616 |
| `ai_tools-metalio-plus` | 34,912 | 31,328 |
| `home_basic-metalio-plus` | 48,816 | 48,752 |

内部取 `mem.internal_total.min_free_bytes`；最低已查 AI 仅高于门槛 424 字节。字体 7,538,204、图标 20,308,480，合计 27,846,684 字节。模拟器模型和本机构建大小不能替代目标链接/分区或实机测量。开发报告“修前”数值未重建基线复测，本次只独立确认当前数值。

## 视觉复核

本次打开 Foundation、Components、Main、Today、Control、Settings 原稿，并逐张打开以下 **12 张重新生成的三栏对照图**（左设计、中实现、右差分）。其余 18 屏运行并核对分数，不称全部目视。真实空状态、测试合成内容、范围外底页不算假数据问题；规范与画板冲突时按规范，不能为分数引入新规格。

| 已目视页面 | 差值 /255 | 明显差异或修复结果 |
| --- | ---: | --- |
| Main | 15.345 | 布局 A 保留；时钟比例、页码点和 Dock 边线仍不同 |
| Today | 24.541 | 日期及“继续听/今日报纸”已补；仍有旧 08:00、日期及图标残影，卡片边线/位置不同 |
| Apps | 15.673 | 搜索框缺搜索图标、标题层次偏重；仍可见旧时钟和图标残影 |
| Control | 23.177 | 滑杆图标已补；网络及“收起”仍是胶囊，分组与底部把手不符 |
| Settings | 16.222 | 缺搜索入口；家庭卡只有居中姓名，缺头像/成员信息；分组行层次和边线不齐 |
| SetDisplay | 18.817 | 图标已补；分段下沿/边框不齐，“屏幕”分组成为两个胶囊，缺残影操作说明 |
| SetSound | 20.973 | 音量图标已补；免打扰分组不同，提示音/振动仍是“还没有可选设置”，铃声只读 |
| ImeCandidates | 27.239 | 展开后面板变矮，候选网格/页码不同；仍有键盘残影。词库候选少本身不算布局错误 |
| PinWrong | 18.619 | 缺姓名、四位密码槽、忘记密码入口；底部为删除/0/确定，设计中的取消路径未对齐 |
| Ai | 37.348 | 分页按钮和页数可见，当前页文字未溢出；主动作仍为“继续问”，缺“听解释/更多”和来源图标 |
| ConfirmDelete | 26.084 | “删除”动作词正确；仍用 AI 图标，缩略图及说明层次/高度不同 |
| ClockRing | 19.637 | 标题、时间、日期已分开；“再睡 10 分钟/这次不再响”正确，数字比例和位置仍不同 |

其余新取图分数；目标全部 ≤12/255，实际没有一屏达标：

| 页面 | 差值 /255 | 页面 | 差值 /255 |
| --- | ---: | --- | ---: |
| Lock | 17.630 | LockPage | 37.596 |
| Notifications | 20.829 | Members | 18.197 |
| MembersBack | 20.701 | Pin | 19.816 |
| ImePinyin | 32.647 | ImeEdit | 33.666 |
| ImePassword | 29.552 | ImeVoice | 29.947 |
| Confirm | 24.931 | ConfirmSecret | 27.869 |
| AiListen | 33.584 | AiThink | 32.902 |
| SetNetwork | 35.519 | PowerMenu | 27.167 |
| PowerOff | 16.877 | PhoneIncoming | 19.966 |

## 范围、已有测试与结束检查

- 【实现】相对 `fdff779` 核查变更清单及关键测试差异。服务/策略增量涉及能力与确认入口、AI/显示适配、通知闹钟动作及相关头文件；UI 经能力调用，未发现本轮修复新增常驻任务或界面直写服务持久状态。未完成的 AI/设置流程仍限制“软总线 + 原子能力”完整性，能力名字已注册不能替代操作闭环验收。
- 【实现】阅读坐标改到 Dock、输入定位改到统一浮层、闹钟文字/索引更新等测试变更有对应实现变化；确认核对实际呈现，超容量说明拒绝。未改旧预算 JSON、未删声音/睡眠失败断言。`standby.script` 的 1,180→1,420 ms 注入调整在历史报告解释为重新命中睡眠入口竞争；原 `power.sleep_cancelled` 断言保留且通过。本次未另做旧时序的基线轨迹重放，不把该解释当作新的独立轨迹证据。
- 【实现】任务书和总规则的变更是明确资源限制及无窗口验证，没有降低产品验收。既有 CI 工作流仍有无数字 `-j`，本次没有执行；后续运行同样须遵守用户当前硬性限制。
- 【实测】任务基线以来 326 个变更文件，无单文件超过 5 MiB；差异中本机路径及 MAC 格式正则 0 命中。修改过的 C/H 去掉注释/字符串后，`write/read/fgets` 调用扫描为 0，未发现新增忽略这些返回值的代码；不能据此宣称 Linux gcc 已验。
- 【实测】`git diff --check fdff779..HEAD` 退出 2，仅 Outfit-OFL、NanoSVG LICENSE 原文空白及 nanosvgrast 缩进告警，未修改第三方原文。本次报告差异检查通过，追踪文件仅本报告有变更。
- 未新增需用户选择事项。未把剩余验收项改称“不需要”，未为继续深挖中级布局或服务故障延长本轮复查。

SEVERE=0 HIGH=2
