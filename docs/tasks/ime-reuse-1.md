# ime-reuse-1：键盘换成 LVGL 自带的键盘控件，加九宫格，按键规格统一

先读 `docs/tasks/vendor-common.md`（公共规矩，第 1、2、3、6、7、9～12 条；第 8 条不碰设备），总规则 `docs/tasks/codex-rules.md` 第 9、10 节优先。本任务替代没派出去的 `ui-input-2.md` 第 1 条（键盘）；它的第 2～4 条（输入框规格）另派，不在本任务。

用户 2026-11-11 试用：“键盘按钮大大小小的很奇怪”“键盘的语音按钮太大太难看了”；随后：“键盘能不能复用一下 这种就属于轮子”“输入法 官方仓库有没有现成可用的”。功能清单里勾了“九宫格键盘”。

输入：

- `docs/tasks/reuse-crosspoint-1/REPORT.md` 第 158～177 行“输入法和键盘的单独结论”（推荐：LVGL 键盘控件＋我们现有的 IKD1 拼音引擎；九宫格的数字映射和匹配摘 kiikoread）和“换入代码怎样接上 AI 与软总线”表的“LVGL 键盘”一行。
- `docs/tasks/ui-audit-2/COMPONENTS.md`（C03～C06、C12）、`PAGES.md` 的 I01～I16：键盘的问题清单和规格。
- LVGL 9.5（依赖里已有，MIT）：`src/widgets/keyboard/lv_keyboard.c`、`src/widgets/buttonmatrix/`。
- kiikoread 固定副本（只读）：`$INK_DEPS/../ink-hw/upstream/oss/kiikoread`（提交 83942eb，`main/ui/ui_ime.c` 和 `ui_keyboard.c` 文件头是 Apache-2.0；仓库里个人学习许可的部分不用）。

## 要做的

1. **键盘控件换成 `lv_keyboard`**（`system/ui/shell/keyboard.c`）：按键区用 `lv_keyboard`（底下是 `lv_buttonmatrix`）和自定义键表，删掉我们手工排的按键。键表：拼音全键盘、英文、符号、数字、拼音九宫格、密码。去掉控件默认写 textarea 的回调，按键和选词仍然只走现有的 `keyboard.key`、`keyboard.pick` 和文本服务（撤销、剪贴板、密码规则、AI 入口都不变），**不留两条输入路径**。
2. **按键规格**照主流手机输入法（Gboard、iOS、搜狗、微信键盘）：字母键每行等宽、行间对齐；功能键（大写、删除、中英切换、符号、回车）用统一的倍数宽度；空格最宽；**语音键和功能键一样大**，放空格左边或候选条右侧（选一种，报告说明理由）；候选条、符号页、数字页、九宫格、密码键盘同一套规格。尺寸出自主题令牌（在 `system/ui/ink/ui_theme.h` 末尾加一个“键盘”令牌块，只加不改别的），跟着系统字号一起放大（`docs/` 里“一起放大”的规则）。
3. **九宫格拼音**：从 kiikoread `ui_ime.c` 整函数拷字母→数字映射和前缀匹配（保留 Apache 声明，放 `system/services/text/vendor/kiikoread/`，旁边 `SOURCE.md`），在 `system/services/text/pinyin.c` 加九宫格查询入口，对我们现有的 IKD1 词库（`tools/imedict/` 生成）做只读匹配，返回原来的候选结构，保留词频、简拼和每个成员的学词。先量查询耗时（宿主机测试里给出键入 1～6 位的耗时），达不到现在全拼的预算就交测量和更小的备选，不自己新增缓存、索引文件或词库格式。不启用 LVGL 的 `lv_ime_pinyin` 引擎（它是单字表，会丢词组和学词）。
4. **切换**：键盘上有一个键在全键盘和九宫格之间切换（照搜狗/微信的做法），选择按成员记住（用现有设置服务存，不新增文件格式）。
5. 页面快照和能力不变：快照给可见候选、组合串和允许的动作；密码正文继续掩码，AI 不能代输秘密。

## 范围和预算

- 只改：`system/ui/shell/keyboard.c`、`system/services/text/pinyin.c`、`system/services/text/text_internal.h`、`system/services/text/text_service.c`（只加全拼/九宫格选择那一处）、`system/ui/lv_conf.h`（显式开键盘控件）、`system/ui/ink/ui_theme.h`（只在末尾加键盘令牌块）、新增 `system/services/text/vendor/kiikoread/`、键盘和文本服务的测试、截图基线、`docs/tasks/ime-reuse-1/`。S31 的 LVGL 配置如果走另一套宏，在报告里写清怎么核对的。
- 同时在跑的：短信电话联系人（`comm-ux-2`，用键盘但不改它）、阅读器（`reader-epub-1`）、4G、电源、蓝牙、相机。不碰它们的文件。
- 胶水净增不超过 350 行（拷来的原文不算）；手工排键的代码应删掉，报告写删了多少。

## 验收

1. 宿主机测试：每种键表同一行键宽一致、尺寸只来自令牌（加一个检查，可以放进现有界面测试）；九宫格输入 `94664` 出“中国”这类词组候选、选词后剩余数字保留；全拼原有测试不放宽照样过；密码键盘不出候选、AI 拿不到正文；切换布局按成员记住。反向对照：把九宫格匹配撤掉测试必须失败。
2. 主工程普通全量 ctest 过（含 `ui-scale`）；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。
3. 报告附各键表常规和大字号的前后截图（放 `docs/tasks/ime-reuse-1/`，单张不超过 300 KB），写清语音键位置、九宫格查询耗时、上机核对清单（主会话在真机打字、切九宫格、量按键到出字的时间）。
