# ime-reuse-1 执行报告

基线 `fa68b45098b745152ea8fb9a30c7a8727d659305`；分支 `claude/codex-ime-reuse-1`；最终产品代码提交 `85b61173`。**实现已提交，验收 2 未全通过。**

以下命令均使用任务书的依赖配置，构建／测试前加 `taskpolicy -c utility`；构建最多 `-j2`，ctest 最多 `-j2`。`$PY` 指 `$INK_DEPS/build-deps/pyenv/bin/python`，`$CTEST` 指同目录的 `ctest`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 六种键表、尺寸与单一输入路径 | 【实测】通过；跨行取整差异见下 | 最终版 2178 条检查、18 帧；native matrix，无 textarea 和默认写入回调 | `$PY tests/ime_keyboard1.py --build build` |
| 1b 九键词组、剩余数字 | 【实测】通过 | `94664` 有“中国”；`946644862` 选“中国”剩 `2` | 同上 |
| 1c 全拼、密码与 AI 隔离 | 【实测】通过 | ime-lexicon、ime-learning、integration-text 三种词库情形及键盘密码检查通过，未放宽原断言 | `$CTEST --test-dir build --output-on-failure -j2 -R '^ime-|^integration-text-'` |
| 1d 按成员记布局、学词 | 【实测】通过 | 成员互不串用；重启 10 条检查通过 | `$PY tests/ime_keyboard1.py --build build` |
| 1e 九键反向对照 | 【实测】通过 | 撤掉匹配后退出 1、失败 3 条；原有三个反向对照仍按预期失败 | `$PY tests/ime_keyboard1.py --build build --reverse` |
| 2a 主工程普通全量 | 【实测】未通过 | 原话：`99% tests passed, 5 tests failed out of 391`；383 通过、5 失败、3 跳过 | `cmake --build build -j2`；`$CTEST --test-dir build --output-on-failure -j2` |
| 2b 字号检查 | 【实测】通过 | 全量中的 ui-scale 通过；最终键盘静态检查 0 违规 | `$PY tools/ui_scale_check.py system/ui/shell/keyboard.c` |
| 2c S31 宿主机 | 【实测】通过 | `100% tests passed out of 51` | `cmake -S ports/esp32s31/tests -B build-s31-host`，构建／ctest `-j2` |
| 2d S31 固件 | 【实测】通过 | 最终代码编译退出 0，C/C++ 编译告警 0；未连接设备 | `INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" bash ports/esp32s31/build.sh` |
| 2e GCC 与内存检查 | 【实测】GCC 通过；ASan 功能测试未全过 | `gcccheck: 12 compiled, 0 with findings`；ASan/UBSan 7/8 通过，听写失败，未出现内存诊断 | `$PY "$INK_DEPS/../ink-codex/tools/gcccheck.py" build <本任务 12 个 C 文件>`；ASan 命令见下 |
| 3 截图、性能和上机清单 | 【实测】宿主机材料齐全；真机待复核 | 24 张 PNG，单张最大 29708 字节；查询结果见下 | 同 1a；[截图校验清单](screenshots.json)、[验证数据](validation.json) |

## 没做到的

- **全量未全绿**：`i18n-shell-remaining`、`i18n-extras`、`i18n-literals` 需要补三项英文翻译和源码标记；`capability-table-doc` 需要同步新增设置。这些文件不在允许修改范围／被禁止修改，交主会话统一更新。
- **听写回归未修复**：`integration-dictation` 在 AI 隐藏输入框中，点击麦克风后没有进入监听，随后“人可听写／回到键盘”共 3 条断言失败。普通全量、ASan 及独立干净输出目录均复现；按三次止损规则保留失败，不再扩改。不能把它写成通过。
- `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 跳过：缺锁定的 Hub Python 环境，且相应生成消息缺失；未自行安装依赖。
- 没有实机打字或实机时延数据。本次没有烧录、串口操作、花钱或向远端写入。

## 和设计不一致的

【实现／实测】LVGL 按每行剩余宽度分配像素：同一行等宽键相差至多 1 px，跨行字母相差至多 2 px；与 C12 的严格同一基本键宽存在这个差异。没有修改规范迁就实现。

候选条基准高 101，普通功能键 96，数字区 163；高度随系统文字行高变化，十列的横向宽度受屏宽约束。数字／九键的功能底行另用同一个事件桥的 LVGL 键盘承载（两行／一行），仍只走 `keyboard.key`。语音键在候选条最右侧，宽 91、高同普通功能键；图标基准 43。沿用既有入口位置，给候选保留中间空间。

## 改过的已有测试

- 按键定位由子按钮／label 改为 matrix 命中框：`integration/textinput.c`、`integration/dictation.c`、`weather/weather.c`、`ui_input_host.c`、`ui_shell_scenes.c`、`ui_shell_regressions.c`、`semantics_shell2/keyboard.c`。两份英文界面采集器增加读取可见 matrix 键名。功能、隔离和状态断言不变。
- `ime_keyboard1` 增加几何、九键、成员学习及重启检查；图标检查适配 matrix 绘制与可访问键名。截图期望从 6 增至 18，场景从 35 秒延长至 150 秒以容纳新增截图；未加 sleep。
- 原反向对照的替换锚点适配新条件，原失败期望不变；另加撤掉九键匹配的反向对照。`ui_input_host` 的坐标诊断显式转 int，消除 GCC 的 int32_t 格式告警，断言条件不变。

## 要用户定的

没有新增产品决策。待主会话处理上述同步项与听写失败后再验收。

## 顺手发现

听写失败的触控命中和顶层界面尚未被同一记录证明，不能归因给某个服务。下一步应同时记录点击前的顶层页面、实际命中控件与能力回执。未用 sleep、放宽断言或删除测试绕过。

## 新增的机制

- 成员设置 `text.nine_key`：任务第 4 条要求，沿用现有设置文件。
- 无新增任务、状态机、文件格式、缓存、索引或后台重试；不启用 lv_ime_pinyin。

## 要改的文档

- 主会话运行 `python3 tools/capabilities.py` 更新共享的 `docs/system/capabilities.md`，纳入 `text.nine_key` 和键 4 的说明。本任务未改该文件。
- `assets/i18n/en.tsv` 补“九宫格拼音”“全键”“九键”三项，再将源码相应位置标为 TR_NOOP/TR。英文键帽宜用 26／9 等短标签，避免大字号溢出。本任务未改允许范围外的翻译表。

## 规模、来源与验证边界

【实测】按产品文件 git numstat 统计：**+234 / -109，净增 125 行**，低于 350 上限；拷来的 18 行头文件、测试和文档不计。手工键帽／排键删除 73 行，keyboard.c 总删除 107 行。超过 100 行是因为六种键表、现有候选／语音／密码语义、只读九键查询和成员设置都要接通。更小方案已核查：只开 lv_ime_pinyin 会丢词组和学习；保留手排键帽不满足替换要求，均未采用。

【实现／实测】kiikoread 两个完整函数逐字比对通过；来源、固定提交、哈希和 Apache-2.0 许可证见 [SOURCE.md](../../../system/services/text/vendor/kiikoread/SOURCE.md)。仅按 IKD1 原键表只读查询，候选沿用 consumed／freq 和成员学习。`94664` 是 zhong，因此九键允许尚未输完的词组补全；全拼仍按原规则精确匹配。

【实测】S31 的 compile_commands 确认与宿主机共用 `system/ui/lv_conf.h`、`LV_CONF_INCLUDE_SIMPLE`；库中导出 lv_keyboard_create，未导出 lv_ime_pinyin_create。显式宏分别为 1、0。

【实测】ASan 构建使用同一依赖配置，加 `-DINK_SANITIZE=ON`；最终跑 `ime-keyboard1`、`ime-lexicon`、`ime-learning`、`integration-text-main/nocard/baddict`、`audit9-ime-layout`、`integration-dictation` 共 8 项，最后一项失败，其余通过。使用 `ASAN_OPTIONS=detect_leaks=0`（本机不支持 LeakSanitizer）；没有 ASan/UBSan 内存错误。最终九键底行几何在全量后调整，并复跑最终源码的键盘专项、上述 ASan 和 S31 编译；全量结果不标为通过。

GCC 命令检查的 12 个 C 文件为 `system/ui/shell/keyboard.c`、`system/services/text/pinyin.c`、`system/services/text/text_service.c` 与 `ime_keyboard1.c`、`integration/textinput.c`、`integration/dictation.c`、`ui_input_host.c`、`ui_shell_regressions.c`、`i18n_shell.c`、`i18n_shell_remaining.c`、`ui_shell_scenes.c`、`weather/weather.c`。最终 `git diff --check` 通过；按总规则第 6 节扫描提交差异，绝对路径和 MAC 匹配数为 0。无范围外文件或构建产物提交。

## 查询耗时

【实测】最终普通宿主机运行，每种查询 100 次，单位微秒。计时、统计和输出在 vtk_host_call 中，产品不读宿主时间。

| 输入位数 | 全拼均值 | 全拼最大 | 九键均值 | 九键最大 |
| --- | --- | --- | --- | --- |
| 1 | 0.120 | 2.000 | 520.380 | 599.000 |
| 2 | 0.220 | 1.000 | 317.070 | 378.000 |
| 3 | 0.350 | 1.000 | 242.970 | 285.000 |
| 4 | 0.400 | 1.000 | 245.650 | 296.000 |
| 5 | 0.530 | 1.000 | 246.840 | 286.000 |
| 6 | 0.660 | 1.000 | 253.930 | 302.000 |

查询九键前 1～6 位 946644，对照全拼前 1～6 位 zhongg。全拼既有小于 800 ms 的测试是按键到显示的整链路预算，并无独立 CPU 查询上限。九键扫描明显慢于二分全拼；宿主机数据不能保证 S31 映射闪存上的性能，上机仍需测量。没有自行添加缓存、索引文件或词库格式。

## 截图

【实测】无窗口模拟器采集，常规字号 1（100%），大字号 3（120%）；PNG 仅无损压缩。前图用基线键盘，保留当时英文界面；后图用中文界面。基线没有九键，“九键前”展示原全键盘，不能当作已有九键。宿主输入框规格不在本任务。

| 键表 | 常规前 | 常规后 | 大字号前 | 大字号后 |
| --- | --- | --- | --- | --- |
| 拼音 | [before/normal](before-pinyin-normal.png) | [after/normal](after-pinyin-normal.png) | [before/large](before-pinyin-large.png) | [after/large](after-pinyin-large.png) |
| 英文 | [before/normal](before-english-normal.png) | [after/normal](after-english-normal.png) | [before/large](before-english-large.png) | [after/large](after-english-large.png) |
| 符号 | [before/normal](before-symbols-normal.png) | [after/normal](after-symbols-normal.png) | [before/large](before-symbols-large.png) | [after/large](after-symbols-large.png) |
| 数字 | [before/normal](before-number-normal.png) | [after/normal](after-number-normal.png) | [before/large](before-number-large.png) | [after/large](after-number-large.png) |
| 九键 | [before/normal](before-nine-normal.png) | [after/normal](after-nine-normal.png) | [before/large](before-nine-large.png) | [after/large](after-nine-large.png) |
| 密码 | [before/normal](before-password-normal.png) | [after/normal](after-password-normal.png) | [before/large](before-password-large.png) | [after/large](after-password-large.png) |

## 上机核对清单

由主会话执行：普通／大字号分别输入全拼和九键，核对切换及成员重启记忆；用 94664 选“中国”，用 946644862 选词后确认剩 2；核对密码不出候选且 AI 不见正文；检查符号两页、数字键和语音键命中范围。测按键到候选／正文可见的时间，尤其是闪存词库的 1～6 位九键查询，并复查 AI 隐藏输入框的听写失败。没有将宿主机截图或耗时当成实机结论。
