# calculator 报告

（Claude 子代理完成，含一轮界面收尾；运行环境不让子代理写报告文件，主会话按它的回报整理保存，数字和结论照原文。）

检出 `ink-codex/calculator/repo`，分支 `claude/sub-calculator`，基线 `4099f390`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 按键算对 | 过 | 【实测】`tests/calculator/calculator.c` 在真页面上点真按键，每次核对屏上的数字和 AI 快照：0.1+0.2=0.3；2+3×4=14，再 -4= 得 10；1÷3=0.33333333；200+10%=220；50×10%=5；5 正负号 ×3=-15，再正负号得 15；123 退格 = 得 12；5÷0 显示“不能除以 0”；超过 10 位整数显示“超出范围，整数最多 10 位”；出错后按数字重新开始 | `python tests/calculator/run.py --build build`（597 项，0 失败） |
| 1 AI 算同一个式子 | 过 | 【实测】`calc.eval` 算同样的式子结果一致；5/0、超范围、`1+` 返回 INVALID 并说明原因；`calc.key` 按 7*6= 得 42 | 同上 |
| 1 快照 | 过 | 【实测】快照有“算式”“结果”两栏和“给一个算式求值”、等于、删除一位、清除四个动作 | 同上 |
| 1 “全部”页、搜索 | 过 | 【实测】“全部”页找到并打开；`search.query` 搜“计算器”能找到、`search.show` 点结果能打开 | 同上 |
| 1 20 个应用分页 | 过 | 【实测】另注册 20 个测试应用，在“全部”页逐个翻到并打开（3 页），计算器还在 | apps20 场景（301 项，0 失败） |
| 反向对照 | 过 | 【实测】工具组上限改回 16：工具09～20 翻不到（24 项失败）；结果改回打印二进制浮点：0.1+0.2 显示 0.29999999999999999（14 项失败）；应用表容量改回 32：第 20 个测试应用注册失败；撤掉换行布局：“长算式自己换行”失败 | |
| 2 原有测试 | 过 | 【实测】`100% tests passed out of 302` | 全量 `-j3` |
| 2 内存检查 | 过 | 【实测】build-asan 里 calculator、integration-apps、integration-ai-surface、search、ui-shell-priorities、*-regressions-metalio-plus、ui-icons-regressions，`100% tests passed out of 14` | |
| 2 S31 | 过 | 【实测】`ports/esp32s31/build.sh` 退出 0，calculator.c 编进固件 | |
| gcc、能力表、图标 | 过 | 【实测】gcccheck 0 处（测试里一处打印改成 `%.31s`）；gccscan 无；`capabilities.py --check` `PASS: 440 definitions`；`check.py` `icons: 85 names, 255 SVG files, 0 errors` | |

截图（检出的 `build/test-out/calculator/`）：`clean-CalcTyping.png`（输入中，实时结果）、`clean-CalcResult.png`、`clean-CalcLong.png`（两行，在运算符前换行）、`clean-CalcDivZero.png`、`clean-CalcEmpty.png`、`clean-CalcRange.png`；“全部”页在 `build/test-out/calculator-apps20/`。

## 做法

- **算法**：十进制定点，8 位小数，内部以 10⁻⁸ 为单位存 int64，整数最多 10 位；乘法以 10⁹ 为基分段相乘，除法逐位长除，四舍五入，不用浮点，也不需要 128 位整数。百分号照手机习惯：a+b% = a+a×b/100，a×b% = a×b/100，单独 b% = b/100。
- **能力**：`calc.key`（按一个键，只在计算器页可用；20 个按键都经 `ink_shell_bind` 绑到它）和 `calc.eval`（给一个算式求值，只读，随处可调；可写 × ÷ − 和全角符号；页面开着时也显示这道题）。等号键和 `calc.eval` 用同一个求值函数。
- **页面**（没有画板，照规范和现有组件设计）：上面看、下面按，算式和结果右对齐，每按一个键显示到目前为止的结果。算式按规范字号依次缩小（43 两行 → 36 两行 → 31 三行），只在运算符前换行（新 `layout()`，一个数和它的负号不拆开），快照里给 AI 的是不带换行的原文。结果用 106，放不下依次 72、53、43。按键 4 列 5 行，现有胶囊按钮，间距 `INK_GAP_M`，等号黑底。÷ × − + ± = 六个键用 77 号实心图标（新画 times、divide、equals、plusminus，plus、minus 原有），退格用原有的 `del` 图标；“%”“C”保留文字。换了家人会清掉上一个人的算式。
- **原因**：按键字库是按源码里出现的字生成的中黑字库，“×”“÷”“±”会退到常规字重的阅读字库，“−”两个字库都没有，补字也对不齐粗细，所以改用图标。
- **“全部”页**：限制分页的是 `tools()` 里的 `count < 16`，改成按数组容量 48；应用表容量 32 → 40（13 个应用加 20 个测试应用会超过 32）。
- **图标**：`calc` 的 line、solid、duo 照五段路径结构手画（机身、显示框、9 个圆点）。
- 产品代码约 330 行（calculator.c 319 行，main.c、navigation.c、app_registry.c、appreg.h 少量），预算 400。

## 没做到的

- 键高用 125（规范底部主按钮高度），不是数字键的 163：4 列各 163 放不进 608 的可用宽度，5 行各 163 共 815，加上标题栏和 120 高的结果行就没有算式的地方；用 125 时算式区留 132，正好放 31 号字三行。
- 字号设到 120% 时，算式缩到 31 号三行可能略超出预留高度，没测。
- 没有记忆键（M+ 等）和历史记录（任务书没要求）。

## 改过的已有测试

- `tests/integration/apps.c`：应用总数 16 → 17（多了计算器）。
- `tests/integration/ai_surface.c`：能力清单登记 `calc.eval`、`calc.key`（这个测试要求每个新能力都登记）。

## 顺手发现

- 时钟、相机、相册在“全部”页上是默认的网格图标：注册时没给 icon（原来就有）。
- `appreg.h` 注释原来写“Up to 16”，实际上限是 32，这次改成 40。
- “全部”页第 2 页底下空一行，分页规则原来就这样。

## 要改的文档

- `docs/product/system-apps.md`：计算器移出“不进首版”（主会话已改）。
- `docs/system/capabilities.md` 已重新生成（新增 `calc.eval`、`calc.key`，新源文件让后面的锚点整体顺移，diff 大）。
- `docs/system/ui-dependencies.md`、backlog：计算器的状态。
