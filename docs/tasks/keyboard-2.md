# keyboard-2：把 Codex 做了大半的新键盘合到今天的主线并收尾——九键读音照 Mogo 拷代码，符号分类补全，上滑出符号，刷新只刷变了的地方

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先，第 11 节最先）和 `docs/tasks/vendor-common.md`（第 11、12 条）。

**D-140（总规则第 11 节）**：交付计划这一行写的是“照着重写”（`docs/tasks/delivery-plan.md:50`）。所以：
- **第一步**：把本任务附带的对照表落成 `docs/tasks/keyboard-2/对照表.md`，**单独提交，提交前不写任何产品代码**（合并 Codex 分支也算代码，要排在对照表提交之后）。
- **照着重写**：fcitx5-android（Kotlin）和 Trime（YAML）在这里编不了，按对照表重写。它们的字符表（长按备选、符号分类）当数据照抄，`SOURCE.md` 写清文件、行号和许可。
- **搬代码**：Mogo 的九键读音列表、九键组合串显示、读音优先筛选、425 个音节表都是同机 C 代码，属于调研写的“可整块移其候选分页/读音过滤及静态布局数据”（`docs/tasks/frameworks-research-1/IME.md:46`、`:65`）。**整函数拷原文**，保留 Apache-2.0 / MIT 文件头，我们只写接线层。确实搬不动的写证据，放进“要用户定”，不许悄悄自己写。
- 做完逐行验收，每行配截图或测试名。报告写对照表做到了几行、哪几行没做。

## 为什么

- 试用时用户说过：“键盘按钮大大小小的很奇怪”“键盘的语音按钮太大太难看了”（`ime-reuse-1`）。
- 2026-11-12 用户说：“外壳有很多我其实并不满意 希望你参考成熟设计完善一下”。
- D-129：输入法这类基础框架“好不好不是看移植工作量小 是看功能成熟度 用户使用体验”；“不需要在乎开源协议”。
- D-131：键盘和输入法要先做到成熟可用。
- D-140：“对 按着实现 参考安卓也行 但是要注意设备特性 不要生搬硬套导致使用体验反而不佳”。

两份调研结论一致（`docs/tasks/frameworks-research-1/IME.md:40-46` 的“键盘界面单独排名”）：
- 键盘界面以 fcitx5-android 最完整，只能照着翻成 LVGL。
- 符号分类补 Trime。
- 九键交互和墨水屏刷新照 Mogo（Mogo 在同一块屏上调过）。

Codex 11-12 晚上做了大半，有 8 项没做完，分支放在单独的克隆里，一直没合进主线。主线这期间又合了 404 个提交，其中 tap-loss-1 改了键盘的点按规则。

## 现状（2026-11-14 按主线 `3baaf0e2` 和 Codex 分支 `2467e6e2` 核对过的代码）

### 主线的键盘：`system/ui/shell/keyboard.c`，707 行

- **状态结构**（`:25-42`）：
  - tap-loss-1 加了 `cand[INK_TEXT_PAGE]`（`:33`，候选按钮上正显示的词）和 `session`（`:34`）。
  - 还没有面板、分类、分页、弹出层这些状态。
- **尺寸**：
  - 键高 `key_height`（`:51-53`）按字号行高乘 `INK_KB_KEY`(96) 或 `INK_KB_DIGIT`(163)。
  - 候选条 `candidates_height`（`:54`）用 `INK_KB_CAND`(101)。
  - design-lang-2 加了 `candidate_band_height`（`:55-58`），至少放得下两行 108 的编辑按钮。
  - 数字键盘比全键盘高，九键和全键盘一样高（`:64-73`）。
- **令牌**：`INK_KB_*` 在 `system/ui/ink/ui_theme.h:98-102`。
- **间距不随字号放大**：`ink_ui_px` 只按画布换算（`system/ui/theme.c:119-124`）；只有 `ink_ui_line_height` 跟着字号走（`:170`）。键高随字号变，间距 `ink_ui_px(INK_KB_GAP)` 不变。
- **点按规则（tap-loss-1）**：
  - `may_edit`（`:83-96`）：触摸要看 `ink_ui_touch_current` 和 `K.session == ink_text_target_session()`，不对就算过期。
  - `press_key`（`:98-129`）：空格在组合中、而且显示的是第一页时，提交按下那一刻显示的第一个候选（`:115-120`，`ink_text_pick_shown`，`text_service.c:469`）。
  - `pick`（`:131-142`）：触摸提交按钮上显示的那个词。
  - 文本服务变了不马上重画，走 `sync_later` → `ink_ui_after_press`（`:560-561`），在 `:705` 登记成 presenter。
- **键表 `map_install`**（`:295-341`）：
  - 九键是“，”“2 ABC”…“9 WXYZ”，键帽上印着数字（`:299-306`）。
  - 数字键盘是 3×3 加两行（`:307-311`）。
  - design-lang-2 把“符号”和“九键/全键”两个键加宽了一档（`INK_KB_FUNCTION + 1`，`:322`、`:325`）。
- **键盘本体**：
  - 按键有边框和圆角，按下时反色（`make_board`，`:363-385`）。这违反 D-31。
  - 键盘出现时请求闪屏清一次（`build` 末尾，`INK_REFRESH_FLASH_UI`）。
  - 符号键表是两页死表，没有分类，没有“最近”，没有长按。
- **盖板键**：上一页、下一页翻的是正文（`overlay_key`，`:249-253`，调 `text.page`）。
- **能力**：只有 `keyboard.key`、`keyboard.pick`、`keyboard.candidates`、`keyboard.edit`（`:697-707`）。

### Codex 分支 `claude/codex-keyboard-2`

位置：Codex 的单独克隆 `ink-codex/keyboard-2/repo`，基线 `d7bc63ae`，最后一个提交 `2467e6e2`。报告用 `git -C <克隆> show claude/codex-keyboard-2:docs/tasks/keyboard-2/REPORT.md` 读。

**已经做了的**
- 新文件：
  - `keyboard_theme.h`（12 行，`INK_KBD_*`，可以覆盖成 108 键高出对照图）。
  - `keyboard_picker.h`（187 行，被 `keyboard.c` 直接 `#include`）。
- 候选展开成 3×2 网格，可以前后翻页，选了就收起。
- 符号面板：中文、数学、英文三个分类，每页 10 个，有前后翻页；“最近”一行最多 8 个，按家人存在 `keyboard.recent0/1` 两项整数设置里。
- 数字键盘照 fcitx 的 `+ - * /` 两侧排列。
- 字母长按弹出一个备选符号，加一个“收起”。
- 九键键帽去掉了数字；组合栏显示“拼音输入”，不露数字编码。
- 按下不反色，候选和键不套框。
- 用全局 `lv_display_enable_invalidation` 压掉按键在按下、抬手时的无效重画（`before_feedback`/`after_feedback`，加上 `on_key` 里的开关）。
- 候选变了三次、停手 400 ms 后，用 LVGL 计时器整理一次词栏（`settle_candidates`）。
- 新增 6 个能力：`keyboard.page`、`keyboard.panel`、`keyboard.category`、`keyboard.symbol_page`、`keyboard.symbol`、`keyboard.alternates`。
- 截图 64 张；固件增量 6,512 B；产品 C 代码加了 382 行。

**没做完的 8 项**（报告“没做到的”）
1. 九键读音列表和筛选（文本服务没有公开接口，Codex 没去碰）。
2. 第二轮的草稿等待超时，`keyboard-pages` 失败。
3. 刷新面积没做白名单；400 ms 整理只有读代码的证据，没有时间断言。
4. 没做上滑。
5. 分类缺单位、特殊（下面改叫“图形”）、序号。
6. 多指取消长按没做，也没测。
7. 间距不随字号放大。
8. 全量测试最后还剩 `keyboard-pages`、`sms-multipart`、`autotest-cap` 三项失败。

**复核发现的问题**（合并时一起改）
- **自动合并的语义错**：`press_key` 开头加了 `K.first = 0;`，主线空格分支的 `!K.first` 判断因此失效。
- **违反墨水屏规则**：“最近”一行做成了横向滚动（`lv_obj_set_scroll_dir(K.recent, LV_DIR_HOR)`），墨水屏不许滚动；符号翻页没有页码，也没有删除键。
- **按键音标签**：`keyboard.symbol` 限 1 个字，Trime 的“……”“——”被改成了单字符。
- **字号检查**：九键和全键的标签对中文也改成了“9”“26”。任务书原意是放不下才用短标签。
- **测试定位器越界**：`tests/keyboard_matrix.h` 在“找键”的定位函数里偷偷调 `keyboard.panel`、`keyboard.category`、`keyboard.symbol_page` 能力去翻页。这已经超出“只改定位方式”。

### 试合并结果（在临时克隆里做的，仓库没动）

把 `2467e6e2` 合到 `3baaf0e2` 上：
- `keyboard.c` 有 5 处冲突：
  1. 文件头的 ui-scale 注释。
  2. 尺寸函数：主线的 `candidate_band_height`/`editing` 对 Codex 的统一 4 行高度。
  3. 九键/全键键的宽度和标签。
  4. 候选循环：主线的 `K.cand[i] = c[i]` 对 Codex 的 `updated` 标记，两行都要。
  5. 展开时候选区的高度。
- `en.tsv`、`ai_surface.c`、`capabilities.md` 冲突，都是两边在末尾追加。
- presenter 自动保留了主线的 `sync_later`，CMake 自动合进了 `keyboard-pages` 测试。

### 文本服务（ime-engine-1 合入后）

- **九键查词**：
  - 旧引擎：`py_lookup_nine`（`system/services/text/pinyin.c:226-249`）。
  - 谷歌引擎：`nine()`（`google_pinyin.cpp:80-121`），由 `py_google_lookup` 在 `:122` 调。
  - 两边都用 kiikoread 拷来的 `digit`/`prefix`（`vendor/kiikoread/nine_key.h`）。
  - **都没有“优先读音”这个参数**，候选结构 `ink_text_cand_t` 里也没有拼写。
- **九键数字会进正文（新发现的现有问题）**：
  - `flush_composing`（`text_service.c:442-447`）把组合串原样写进正文。
  - 这些路径都会调它：回车（`:549-560`）、切中英（`:480-482`）、切九键/全键（`:483-486`）、`ink_text_commit_utf8`、换输入框。
  - 九键时组合串是数字，打 `94664` 再按回车，正文里就是“94664”。Mogo 明确不许这样：“never display internal T9 codes”，并提示“请先选择词语”。
- **数字和电话框会丢键**：`INK_TEXT_NUMBER` 只收 0-9、`.`、`-`；`INK_TEXT_PHONE` 只收 0-9、`+*#`（`:507-516`）。Codex 的数字键盘在这两种框里还显示 `/`、`=`、`,`、“符号”“26”，这些键点了没反应。

### 其它会受影响的地方

- **直接 `#include` keyboard.c 的测试**：
  - `tests/tap_loss/held.c:23`，其中 `key_target` 写死“footer 从第 9 个键开始”（`:158-160`）。Codex 分支已有 `K.footer_offset`，合并后 `key_target` 改读它。
  - `tests/semantics_shell2/keyboard.c:3`（用 `INK_TEST_SHELL_SOURCE` 宏指向 keyboard.c）、`tests/ime_keyboard1.c:13`、`tests/text_long/run.py`。
  - 所以新代码继续放在被 `keyboard.c` 包含的头文件里，不另起 `.c`。
- **其它测试用到的键码和能力**：
  - 键码：1 符号、2 大写、3 中英、4 九键/全键、8 删除、10 回车（5、6 在主线没人用，本任务用作切数字、重输）。用的地方有 `settings_small.c:130`、`memo_3.c:267`、`ui_fix_d/memo.c:66`、`boot_flow.c`、`sms.c:356`、`search.c:137`。
  - 用 `keyboard_matrix.h` 的测试：`weather.c`、`ui_shell_scenes.c`、`ui_input_host.c`、`integration/textinput.c`、`dictation.c`、`i18n_shell*.c`。
- **设备自动测试**：`tools/device/autotest/memo.script` 按百分比坐标点键；`tests/device_autotest/run.py:39-40` 要求备忘场景正好 11 次触摸。
- **刷新**：
  - 刷新调度把相交的请求合并成外接矩形（`system/services/display/refresh_policy.c:1-13` 第 1 条）。所以按键时的一次无效重画哪怕像素没变，也可能把候选条和按下的键并成一大块。这就是要压掉按下重画的原因。
  - 同类的局部关失效区，在 `system/apps/sms/sms.c:271-295`、`:336-342` 有先例，开和关都在同一个函数里。
- **字形**：界面字库只收 `.c` 里出现的汉字和翻译表里的字（`tools/fontgen/ui_fonts.py:32-46`）；别的符号退回 sans-44 缩放字形，那套字形的字集是 ascii、punct、pinyin、gb2312。“特殊”的“殊”不在界面字集里，所以分类叫“图形”；“单位”“序号”“重输”“最近”都已经在。
- **输入服务**：把任何一根手指的按下都当成新的一次按下（`system/services/input/input_service.c:101-150` 不看 `touch.id`），只在 `note_held`（`:241-251`）里按手指记了位。模拟器有两个触点（`simulator/profiles/metalio-plus.json` 的 `touch_points: 2`）。
- **D-138**：键盘不需要卡上的任何文件。拼音词库卡上没有时用资源包里的，资源包也没有时用程序里的小表（`pinyin.c:1-8`），不会出现空白。

## 成熟来源（固定提交；源码浅克隆到自己的临时目录读，不进仓库）

fcitx5-android 和 Trime 不在 `ink-hw/upstream/oss/` 里，按固定提交浅克隆到临时目录只读；Mogo 在本机已有副本。

### fcitx5-android（照着重写，字符表当数据抄）

`github.com/fcitx5-android/fcitx5-android`，提交 `eb5b50216e70484a009933d9519db76005f1faf3`，LGPL-2.1-or-later。下面路径都在 `app/src/main/java/org/fcitx/fcitx5/android/input/` 下：

- **键盘**：
  - `keyboard/TextKeyboard.kt:33-76`：全键盘，每个字母带一个备选符号。
  - `keyboard/KeyDefPreset.kt`：
    - `:42-62` AlphabetKey：点按出字母，上滑出备选，长按弹备选表。
    - `:96-108` CapsKey：点一下大写一次，长按或双击锁定。
    - `:128-143` BackspaceKey：按住连删。
    - `:157-193` CommaKey 的长按菜单：表情、快捷短语、Unicode。
    - `:195-205` LanguageKey。
    - `:207-220` SpaceKey 的长按。
    - `:222-243` ReturnKey。
  - `keyboard/SwipeSymbolDirection.kt:10-17`：上滑方向。
  - `keyboard/NumberKeyboard.kt:24-55`：数字键盘。
  - `keyboard/KeyboardWindow.kt:75`、`:119-134`：字母、数字、符号之间切换，记住上次的符号布局。
- **长按备选**：`popup/PopupPreset.kt:12-69`（拉丁字母小写和大写两张表，SHA-256 `c9147c5c…a770b`）；`popup/PopupKeyboardUi.kt:45-220`（弹出层的行为）。
- **符号面板**：
  - `picker/PickerData.kt`（SHA-256 `10b520a8…7703b`）：`:13` 最近用过的分类，`:16-26` 数字和半角标点，`:60-70` 序号，`:96` 起是表情，`:263` 起是颜文字。
  - `picker/PickerWindow.kt:120-122`：打开时先显示第一个符号分类，不显示“最近”。
  - `picker/PickerPagesAdapter.kt:77-80`：数字不记进“最近”。
  - `picker/PickerPageUi.kt:65-71`（每页多少个）、`:104`（页内删除键）。
  - `picker/PickerTabsUi.kt:34-135`、`picker/PickerPaginationUi.kt:34-68`：分类标签和分页指示。
  - `picker/PickerLayout.kt:30-40`：面板底行。
- **候选**：
  - `candidates/expanded/GridPagingCandidateViewAdapter.kt:18-48`：网格按字宽排。
  - `candidates/expanded/ExpandedCandidateLayout.kt:41-80`：展开后右侧一列是上一页、下一页、删除、回车。
  - `candidates/expanded/CandidateTabActionsAdapter.kt:33-60`：候选的长按动作。
- **候选条和编辑**：`bar/KawaiiBarStateMachine.kt:17-46`（空闲、候选、标题三种状态）；`bar/ui/idle/ButtonsBarUi.kt:31-50`（撤销、重做、移光标、剪贴板、更多）。

### Trime（照着重写，分类字符当数据抄）

`github.com/osfans/trime`，提交 `76a9e763b148d747f6245f61d65e02595082f080`，GPL-3.0（D-129 说不用在乎许可，以它为准）。文件 `app/src/main/assets/shared/trime.yaml`（SHA-256 `44d29a6a…f83f6`）：
- `:686-706` 符号面板的结构：每屏行数、固定的“返回、空格、删除、回车”按键条、分类列表。
- `:710-712` 常用（history）。
- `:720-723` 数学。
- `:724-756` 中文。
- `:757-760` 特殊（我们叫“图形”）。
- `:761-764` 单位。
- `:920-929` 切键盘的几个键。

### Mogo（九键交互和刷新规矩；整函数拷原文）

`github.com/chopper1026/mogo-e4p`，提交 `864f2f9dc75980a67d2d6b67b17365020b7a50ce`。本机只读副本在 `$INK_DEPS/../ink-hw/upstream/oss/mogo-e4p`；`third_party/mogo/SOURCE.md` 固定的也是这个提交。

- `main/ui/ui_keyboard.c`（SHA-256 `2d95bf1f7953464fc96ad23cc73102217691aef1708a5edf1d3c07758021c5aa`，文件头 SPDX Apache-2.0）：
  - `:1-14` 用户修订过的规矩：只画变化的区域；九键只打中文；数字只在数字页打；删除按住连删，抬手、移开、多指或会话结束就停；候选变三次、停手 400 ms 才整理词栏；同音字能一直翻页。
  - `:48` `groups[]` 字母组。
  - `:161-177` `syllables()`：读音列表，长的在前，每页最多 3 个读音（函数里写死 `count < 3`），“更多”能翻遍所有读音（点“更多”时 `s_syllable_offset` 加 3，到头回 0，`:335-337`）。
  - `:178-192` `composition_text()`：认出的读音写字母，没认出的数字写成 `[ghi]` 字母组。
  - `:211-215` 读音列：3 个读音加一个“更多”；`:216-260` 九键布局；`:274-361` 点按处理（九键切全键时把数字转成拼音，转不了就提示；回车时还有组合串就提示“请先选择词语，再保存”）。
  - `:362-440` 按下反馈、连删时间（500 ms 后每 120 ms 删一次，不补发积压的）、停手整理。
  - `:442-483` 只重画变化的区域。
- `main/ui/ui_ime.c`（SHA-256 `76b8a548bbca9fbbd2c747d681fc2d242b4ae23bda3f49996fc6ae0e0bb1af80`，Apache-2.0）：
  - `:47-57` `digit()`、`ui_ime_digits()`。
  - `:91-93` `preferred_ok()`：第一个音节只认选中的读音。
- `components/book_search/book_search.c:164-167` `book_search_syllable()`（SHA-256 `6d1390b8…a2fd`，Apache-2.0）。
- `components/book_search/search_table.h`（SHA-256 `4966b633afdc2b93bedb4b291b33a0a86310c69442ba6e9d8e99818e6394cb0b`，文件头 MIT，由 pypinyin 0.55.0 生成）：`:1-6` 文件头；`:6170-6313` 的 `search_syllable_offsets` 和 `search_syllables`，共 425 个音节，约 2.6 KB。**只拷这两个数组**，不拷 665 KB 的整个文件。同目录 `LICENSE.pypinyin`（SHA-256 `1e6c9001…764c`）一起拷。

## 搬代码（第 11 节）：拷什么、放哪、怎么编

只拷整函数和整数组，函数体不改；不一样的地方靠垫片宏接到我们的状态上。

1. **`system/ui/shell/vendor/mogo/mogo_nine.h`**（新文件，C，被 `keyboard.c` 经 `keyboard_*.h` 包含）
   - 内容：`ui_keyboard.c:1-14` 的文件头原文，`:48 groups[]`，`:161-177 syllables()`，`:178-192 composition_text()`；`ui_ime.c:47-57 digit()`、`ui_ime_digits()`。
   - 垫片（写在拷贝之前，单独一段，注明是 ink 写的）：
     - `s_raw` → `ink_text_composing()`
     - `s_chinese`、`s_nine` → 当前是中文九键
     - `s_syllable_offset` → `K.reading_offset`（类型必须是 `size_t`，和原文一致，免得多出符号比较警告）
     - `s_best_roman`、`s_best_consume` → 选中的读音和它的长度（没选时为空，就全显示成字母组，这是原文自带的分支）
     - `UI_IME_RAW_MAX` → 48
   - 拷来的 `static` 函数和数组必须都被用到，不许因为拷贝多出 `-Wunused` 警告。
2. **`system/ui/shell/vendor/mogo/mogo_syllables.h`**：`search_table.h:1-6` 文件头（MIT，含 SPDX-FileCopyrightText 行），`:6170-6313` 两个数组，加上 `book_search.c:164-167 book_search_syllable()`（带它的 Apache-2.0 文件头）。旁边放 `LICENSE`（Apache-2.0，从 `third_party/mogo/licenses/Apache-2.0.txt` 复制）和 `LICENSE.pypinyin`。
3. **`system/services/text/vendor/mogo/preferred.h`**：`ui_ime.c:91-93 preferred_ok()` 原文和文件头。
   - 在 `pinyin.c` 的 `py_lookup_nine` 里，`prefix(composing, roman, true)` 之前加一行调用。
   - 在 `google_pinyin.cpp` 的 `nine()` 里，`prefix(digits + pos, part, true)` 之前加一行调用（`at` 参数传 `pos`，原文的意思是“不是第一个音节就放行”，`pos == 0` 就是第一个音节）。
   - `google_pinyin.cpp` 是 C++，包含 `.h` 里的 `static bool` 函数可以直接编；这个头文件只用 `string.h`，不需要 `extern "C"`。调用 `ink_text_preferred_reading()` 走 `textinput.h`（已有 `extern "C"`）。
4. **`system/ui/shell/vendor/mogo/SOURCE.md`**（文本服务那份 `preferred.h` 的 `SOURCE.md` 写同样的格式）写：
   - 项目和固定提交；拷了哪几个文件的哪几行，每个上游文件的 SHA-256；
   - 改了什么：只加了垫片宏，函数体没改；
   - 许可范围：这几个文件在 `864f2f9` 的文件头分别是 Apache-2.0 和 MIT。Mogo 的 LICENSE 规定，只有明确标了 `LicenseRef-Kiikoread-Personal-Study-1.0` 的内容才是个人学习许可，这几个文件不是；
   - 原文能带的测试：没有。Mogo 的 `tools/test_search.py` 测的是书名匹配，和这次无关。
5. **fcitx 和 Trime 的数据**：长按备选表（PopupPreset 的拉丁字母部分）、符号分类内容，放进 `system/ui/shell/keyboard_symbols.h`（新文件，纯数据）。`docs/tasks/keyboard-2/SOURCE.md` 逐表写：来自哪个文件哪几行，许可，筛掉了哪些字（没有字形的、重复的）。
6. **不拷**：fcitx 和 Trime 的任何 Kotlin、YAML 运行时；Mogo 的 `ui_ime_candidates_page` 组词核心（我们有谷歌拼音和旧引擎）；Mogo 的整个 `search_table.h`。
7. **接线层写在我们自己的文件里，不算“拷”**：读音格怎么摆、“更多”怎么翻、提示怎么显示，是我们写的接线（见下面九键一节），`SOURCE.md` 写清这几处是自己写的、对应 Mogo 的哪几行。

## 要做的

### 0. 先合并（对照表提交以后）

1. 在主仓库建本地分支（只读取 Codex 克隆，不动它）：
   `git fetch <Codex 克隆> claude/codex-keyboard-2:keyboard-2-codex`
2. 从当前主线开工作分支，`git merge --no-ff keyboard-2-codex`。不 rebase，不改 Codex 的提交。
3. 冲突照下面解：
   - **尺寸**：保留主线的 `candidate_band_height` 和 `editing()`。每种输入类型的键盘总高**不许超过主线现在的高度**：全键盘照旧；数字和九键用 Codex 的 4 行。这样同一批其它任务的页面不会被挡。
   - **候选高度**：用 `max(108, 行高换算)`，取 Codex 的写法；展开时的高度按主线的候选带算。
   - **候选循环**：`K.cand[i] = c[i];` 和 Codex 的 `updated` 判断两行都留。
   - **九键/全键键**：中文保留“九键”“全键”，最大字号放不下才换“9”“26”。功能键统一用一个倍数宽：主线的 `+1` 是为长标签加的，标签合适了就去掉；有一个放不下就整排都用 `+1`，同一行等宽。
   - **删掉** Codex 在 `press_key` 开头的 `K.first = 0;`（结尾那句已经有了），恢复主线空格分支的语义。
   - presenter 保持 `sync_later`。
   - `en.tsv`、`ai_surface.c` 两边的行都留，本任务的放在末尾；`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成。
4. **按下不重画**：Codex 用全局 `lv_display_enable_invalidation` 压按下、抬手时的无效重画，不许整个删掉，先写测试证明下面两件事，再决定留不留：
   - 手指按着键时文本服务的候选变了（`tap_held_suggest`，tap-loss-1 的场景），抬手后候选条一定会重画，不会被吞；
   - 换布局（键被删掉）时，失效区开关一定会恢复。
   - 两条有一条不成立，就改成在同一个回调里成对地关、开，照 `sms.c` 的先例，或者换别的办法。
5. 合并后先跑 `ime-keyboard1`、`keyboard-pages`、`tap-held`、`tap-erase`、`semantics-shell2-keyboard`、`ui-scene-Ime*`，把状态写进报告，再做下面的事。

### 1. 九键（照 Mogo，读音部分拷代码）

- **布局**：5 列。
  - 左列是读音：3 格，每格和键一样高。2 个读音加“更多”；读音正好 3 个、没有更多时就放 3 个。
  - 中间 3×3：“，”和 ABC…WXYZ，键帽不印数字。
  - 右列：删除、重输、“。”。
  - 底行：符号、中/英、全键、空格（有组合串时写“选择”）、完成。
- **读音格接线**（`syllables()` 原文每页最多返回 3 个，不改函数体）：
  - 函数返回 3 个且 `more` 为真时，只显示前 2 个，第三格写“更多”；返回 3 个且 `more` 为假时，显示 3 个，没有“更多”格。
  - 点“更多”：`reading_offset` 加上**这一页显示的个数**（2 或 3，不是 Mogo 的固定 3，否则第三个读音会被跳过）；已经到头（`more` 为假而 `reading_offset` 不为 0）时，格子仍写“更多”，点了回到第一组（照 `:335-337`）。
  - 组合串每变一次，`reading_offset` 清零（照 `:60`）。
- **选读音**：
  - 新能力 `keyboard.reading {text}` 选中一个读音，`text` 为空就取消。
  - 新能力 `keyboard.readings_more {}` 翻下一组读音，到头回第一组。
  - 选中的读音格反白显示（这是状态，不是按下反馈）。
- **组合栏**：用拷来的 `composition_text()`，例如选了 zhong 以后显示“zhong [ghi] [mno]…”，不显示数字。
- **提示**（“先选词或按重输”“拼音过长，先选词”）：
  - 写在组合栏位置，一直留到下一次按键才消失；不设计时器，不为让它消失再刷一次屏（第 11 节“提示”一条）。
  - 英文翻译加进 `en.tsv` 末尾。
- **文本服务**（只加这些，不碰别的）：
  - `void ink_text_prefer_reading(const char *roman)` 和 `const char *ink_text_preferred_reading(void)`：照 Mogo，删除和选词时清空，继续按数字键时保留。
  - `void ink_text_clear_composing(void)`：“重输”用，键码 6 走 `keyboard.key`。
  - `flush_composing` 一处改：九键的组合串**任何路径都不写进正文**。回车、切中英、切全键时，键盘照 Mogo 原文提示“先选词或按重输”并且不动正文；换输入框和失去焦点时直接丢弃。
- **九键切全键**：一律提示“先选词或按重输”，不切（和 Mogo 不同：Mogo 在选定读音并且读音吃满组合串时会把数字换成拼音，还会在全键切九键时把字母换成数字。这需要文本服务再加“改写组合串”的接口，超出本任务，列进“要用户定”）。
- **同音字翻页**：在展开网格里翻，上限是文本服务的 60 个候选（`MAX_CANDS`）。Mogo 能翻遍所有同音字，我们最多 60 个，这一条写进报告“和来源不一致”，不改引擎。

### 2. 符号面板（fcitx 的结构，分类内容取 Trime 和 fcitx）

- **分类**：最近、中文（Trime cn）、英文（ASCII 可打印字符）、数学（Trime math）、单位（Trime unit）、图形（Trime symbol）、序号（fcitx ①）。
  - 标签宽至少 108，放不下就换行，不横向滚动。
  - 选中的分类用粗体加下划线标出，不靠颜色。
- **打开时**先显示中文，不先显示“最近”（照 `PickerWindow.kt:120-122`）。
- **“最近”是第一个分类标签，不另占一行**（照 fcitx `PickerData.kt:13`）。
  - 按家人存，最多 8 个，数字不记（照 `PickerPagesAdapter.kt:77-80`）。
  - 继续用 `keyboard.recent0/1` 存编号。符号表只能往后追加，加测试钉住已有编号。
- **每页放多少个**按剩下的高度算。
- **底行**：字母(26)、◀、页码“2/5”、▶、123、删除。最大字号放不下就换行。
- **盖板键**：面板开着时，上一页、下一页翻符号页。
- **多字的符号**：Trime 的“……”“——”“···”照原样一键打出，`keyboard.symbol` 的 `text` 放宽到 4 个字。
- **字形**：每个符号都要能显示（界面字集，或者 sans-44 的 ascii、punct、pinyin、gb2312 字集）。显示不了的在写数据表时去掉，列进 SOURCE.md。
- **有组合串时**：九键组合中点“符号”，照 Mogo 提示“先选词或按重输”；全拼组合中照现在的规矩，字母原样上屏。

### 3. 字母键：长按和上滑（fcitx）

- **长按弹备选**：备选取 `PopupPreset.kt:12-69` 的整张表（数字或标点、大小写、带调字母）。
  - 弹出层每格至少 108，最多 3 行，有“收起”。点了才算选，不靠拖。
  - 弹出层留到选了或者收起才消失。
  - 只刷弹出层那一块。
  - 带调字母（á 等）只有长按这一个入口，第 11 节要求手势做的事要有写着字的按钮：列进“要用户定”，由用户决定要不要加按钮。
- **上滑**：上滑超过半个键高，打出这个键的备选符号（`AlphabetKey` 的 Swipe）。
  - 键的右上角用小字、深灰写出这个符号，这是上滑的提示（照 fcitx 的 AltText）。
  - 上滑和长按都走 `keyboard.symbol {text}`，和点符号面板是同一个入口；这些符号在符号面板里也能点到。
  - 手指滑到别的键上，不能打出别的键（这是 LVGL 键表的默认行为，要压掉）。
  - 做不好就只留长按，写进报告。
- **大写**：点一下大写一个字母；再点一下锁定，键上画出锁定的样子；再点一下取消。不用长按和双击。

### 4. 数字键盘（fcitx）

- **普通文字框里的“123”面板**：照 `NumberKeyboard.kt:24-55` 全套：`+ - * /`、`, = .`、空格、删除、字母、符号、完成。
- **数字框和电话框**：只放文本服务会收的键。
  - 数字框：0-9、`.`、`-`、删除、完成。
  - 电话框：0-9、`+ * #`、删除、完成。
  - 不放“字母”“符号”。点了没反应的键不画出来。

### 5. 候选展开（fcitx 的结构）

- 3 列 × 2 行，每格至少 108，长词换行。
- 右边或底下放 ◀、页码“2/10”、▶、删除、收起（写“键盘”）。
- 盖板键翻候选页。
- 选了以后自动收起。

### 6. 刷新（Mogo 的规矩，本任务收尾）

- **只刷变了的区域**：
  - 打字只刷组合栏、候选条和正文框；九键还刷读音列；九键组合开始、结束时空格键的标签变一次，只刷空格键那一块。
  - 按键本身不刷（按下不变样，D-31）。
  - 换布局才刷整个键盘区；弹出层只刷它自己那块。
  - 键盘出现时不要求闪屏清一次（Codex 改成 FAST，保留）。
- **停手整理**：候选变了 3 次，停手 400 ms 后只刷一次候选条（照 `ui_keyboard_idle_tick`）。
  - **手指按着时不整理**：tap-loss-1 的规矩，用 `ink_ui_after_press`，或者判断 `ink_input_touch_held()`。
  - 换布局、结束输入就取消。
- **按住删除**：照现在的 LVGL 重复（400 ms 后每 100 ms 一次）。这个时间是全系统一套的，改了会影响所有长按，所以不改成 Mogo 的 500/120，写进对照表。刷新调度只送最新的一帧，不会补发积压的删除。
  - 抬手、移开就停。
  - 第二根手指按下也要停：先用模拟器的第二个触点测。键盘分不出第二根手指时，停下，写证据进“要用户定”，不改输入服务。

### 7. 间距跟字号一起放大

键间距、行间距、候选间距都按 `ink_ui_line_height(INK_FONT_LIST) * 令牌 / INK_LINE_LIST` 算，和键高同一个比例，写成 `keyboard_theme.h` 里的一个函数。`tools/ui_scale_check.py` 要能查到它（文件头写 `ui-scale-runtime` 注释）。

### 8. 修 3 个失败的测试，查清根因

- **`keyboard-pages`**：第二轮在等“初始草稿写好”（`pages_ready` 要求 `ink_text_draft_saved() != 0`），用的是 memo-3 以后的新备忘。【估算】新建的空草稿可能根本不会写，所以永远等不到。先在合并后的代码上复现，找到原因。不许加长等待时间、加 sleep 或者跳过。
- **`autotest-cap`**（memo 场景）：`memo.script` 的坐标跟着新布局重算。坐标从模拟器打印出的真实键中心取，不要猜。还要正好 11 次触摸。
- **`sms-multipart`**：sms-3 这一批在重写短信，不许改它的测试。查清楚是不是键盘挡住了“发送”。是键盘的问题就在键盘这边修；不是，就写进报告交主会话。

### 9. 能力和快照

- **能力**：打字和选词照旧只走 `keyboard.key`、`keyboard.pick` 和文本服务。
  - 保留 Codex 的 6 个能力；新增 `keyboard.reading`、`keyboard.readings_more`；键码 5 切数字、6 重输，写进 `keyboard.key` 的说明。
  - 每个按钮和 AI 调的是同一个能力。
- **快照**写出这些：组合栏显示的内容（九键不写数字）、选中的读音和可见的读音、可见候选和第几页、当前面板、分类和第几页、弹出层的备选、提示文字、现在能做的动作。
- **密码框**：不出候选，快照里没有正文，“最近”不记密码里的字，AI 不能输入。
- **孩子**：所有键盘能力孩子都能用（`child_ok`），“最近”按家人分开。
- 新能力进 `tests/integration/ai_surface.c` 的表（写清必填参数），在末尾追加；新文案的英文加进 `assets/i18n/en.tsv` 末尾，不新增界面字集里没有的汉字。

### 10. 测试定位器

`tests/keyboard_matrix.h` 恢复成“只找可见的键”。去符号页找符号的，另写一个明说的帮手 `ink_test_keyboard_symbol(text)`，用真实点按点分类和翻页，**不在定位函数里偷偷调能力**。`tests/tap_loss/held.c` 的 `key_target` 如果因为 footer 偏移变了要改，只改成读 `K.footer_offset`，不动断言。

## 共同要求（页面任务都一样）

- 来源：页面、状态、出错处理照上面的来源和对照表，不自己发挥。`docs/tasks/keyboard-2/SOURCE.md` 写每一页对应来源的文件和行号，哪些没照搬、为什么。
- 新设计语言：键盘以外的可点控件用共用控件和令牌，不写死像素和颜色。键不套圆圈、不画粗框，主次靠样式不靠大小。不改 `ui_theme.h`（键盘的令牌放 `keyboard_theme.h`）。
- 不变的规矩：
  - 家人隔离（学词、最近符号按家人分开）；孩子模式；
  - 确认卡只给 D-105 的高风险动作（键盘本身没有）；
  - 少刷新（D-127）；按下反馈 D-31（不反色，出字就是反馈）；
  - 密码不给 AI、不进日志，AI 不能代人输密码；
  - tap-loss-1：按住时被动更新等抬手，`ink_ui_content_changed` 只在页面显示的内容变了时才调。
- 文字大小：键盘跟系统“文字大小”一起缩放，键、字、间距用同一个比例。功能键上的字放不下才用短标签，不缩字。
- 原地改，旧的手工符号键表删掉，不留两套。
- 截图放 `docs/tasks/keyboard-2/`，单张不超过 300 KB。先看干净的整页图（没有调试框）。

## 范围和预算

- **只改**：
  - `system/ui/shell/keyboard.c`；
  - `system/ui/shell/keyboard_*.h`（`keyboard_theme.h`、`keyboard_picker.h`、新的 `keyboard_symbols.h`）；
  - `system/ui/shell/vendor/mogo/`（新）；
  - 文本服务里只改这些：`text_service.c` 的 3 个新函数和 `flush_composing`、`textinput.h` 的 3 行声明、`pinyin.c` 和 `google_pinyin.cpp` 各 1 行调用、`system/services/text/vendor/mogo/`（新）；
  - 键盘测试：`tests/ime_keyboard1.*`、`tests/keyboard_matrix.h`、`tests/audit9/ime_layout.c`、`tests/semantics_shell2/keyboard.c`，`tests/tap_loss/held.c` 只改定位；
  - 新测试（对照表里的 `kb_*`）放进 `tests/keyboard2/`（新目录），在 CMake 里登记；
  - `tools/device/autotest/memo.script`；
  - CMake 里本任务那几行，`docs/tasks/keyboard-2/`。
- **不碰**：
  - `system/ui/ink/ui_theme.h`、`system/ui/theme.c`、`textfield.c`、`ui_list.c`；
  - 输入服务；
  - 文本服务里除上面以外的部分（ime-engine-2 后面接着改）；
  - sms、contacts、clock、calendar、计算器等应用和它们的测试。
- **产品代码不超过 750 行**（不算拷来的 Mogo 函数、数据表、测试和文档；拷来的行数单独列）。
  - Codex 已经用了 382 行。
  - 比原来的 600 多，原因是：原任务书说文本服务“不碰”，读音筛选、重输、九键数字不进正文没法只在键盘里做，文本服务的接线约 60 行；读音列、上滑、多格备选、分类换行加页码这几样，Codex 那一轮都没做，约 200 行。快超了就停，写原因。
- **固件增量不超过 25 KB**（Codex 6.4 KB，加上音节表约 2.6 KB、符号和备选数据约 5 KB）。
- 起步：当前主线（`3baaf0e2` 或之后），合入 `keyboard-2-codex`。
- **共享文件**：`docs/system/capabilities.md` 重新生成后提交；`ai_surface.c`、`en.tsv` 只在末尾追加；合并冲突交主会话。

## 和同一批任务的避让

- **sms-3**：拥有 `system/apps/sms/` 和 `tests/sms*`、`tests/sms_multipart/`，本任务一个字都不改。`sms-multipart` 如果是被键盘挡住的，在键盘这边修。
- **contacts-3、clock-3、calendar-3、small-apps-3**：它们的页面按 `ink_shell_keyboard_area_for()` 留位置。这个函数签名不变，每种输入类型的键盘只许比主线矮、不许比主线高。键码 1、2、3、4、8、10 不变。
- **control-1-check、home-1-check、settings-1-check**：不碰键盘；`settings_small.c` 用键码 33，保持能用。
- **ime-engine-2**（不在这一批）：之后接着改文本服务。本任务加的 3 个函数写在 `text_service.c` 的同一段里，报告列出行号，方便它接手。

## 验收

1. **照 fcitx5-android 和 Mogo 的用法走一遍**。模拟器脚本，每一步截图并断言，先用触摸走一遍，再用 AI 的能力调用走一遍，每步结果一样：
   1. 备忘里打 `nihao`，候选条出“你好”；
   2. 点 ▾ 展开，翻到第 2 页，页码写“2/…”；选一个，自动收起；
   3. 切符号，默认是“中文”；选“数学”，翻页，选“≈”；
   4. 切到“最近”，第一格是“≈”；
   5. 回字母；
   6. 电话号码框自动是数字键盘，没有“字母”“符号”和没用的键；
   7. 长按“a”弹出整张备选，选“á”；
   8. 上滑“q”打出“1”；
   9. 九宫格打 `94664`：组合栏没有数字，读音列出现 zhong；选 zhong 后候选第一页有“中国”，选中；
   10. 九宫格再打两个数字，按回车：提示“先选词或按重输”，正文里没有数字；按“重输”，组合串清空。
   - 第二轮之前的等待原因查清楚，不靠加长时间。
2. **规格**：
   - 每种键表同一行键宽一致；
   - 候选格、网格格、分页按钮、分类标签、弹出格的点按范围都不小于 108；键（含底行按钮和读音格）的高度等于键高令牌，96 还是 108 由用户定，两套都出截图，不在这条里要求 108；
   - 默认和最大字号下，间距和键高的比例不变（新断言）；
   - 尺寸只来自令牌（`ui-scale` 检查）。
3. **刷新白名单**：
   - 按 10 个字母，每一次刷新的矩形都在“组合栏 ∪ 候选条 ∪ 正文框”以内；九键加上读音列，以及组合开始、结束那一次的空格键；没有整屏刷新。
   - 切到符号面板，刷新面积是键盘区；切分类、翻页只刷面板以内；弹出层的开和关只刷弹出层那块。
   - 停手整理：变了 3 次以后，停手 400 ms 正好刷一次候选条；连打时不刷；手指按住 1 秒也不刷。
   - 反向对照，下面两条都必须失败：每次按键重画整个键盘；去掉“按下不重画”。
4. **按下时候选变了**（tap-loss-1 的场景）：抬手后候选条已经重画，提交的是按下时看到的词；`tap-held`、`tap-erase` 原样通过。
5. **长按删除**：按住连删，抬手立刻停，移开也停；第二根手指按下也停（做不到的写证据进“要用户定”）。
6. **密码**：不出候选，快照里没有正文，“最近”不记密码里的字，AI 不能输入。
7. **数据**：“最近”的编号钉死（加了新符号以后，老的编号不变）；符号表和备选表里每个字都有字形。
8. **截图**：
   - 全键、九宫格（有读音列）、123 面板、数字框、电话框、符号面板（每个分类第一页）、候选展开、长按弹出，各出默认字号、最大字号、孩子身份三套；
   - 另出键高 108 的一套（96 还是 108 写进“要用户定”）；
   - 改前改后对照。
9. **报告**写这些：
   - 每个主要动作刷几次屏：打一个字、选词、展开、翻页、切面板、切分类、长按弹出、选备选、选读音；
   - 对照表做到了几行、哪几行没做；
   - 产品代码行数，拷来的行数单独列；固件增量；`gcccheck` 结果；
   - 改过的已有测试（逐条写原因；只许改定位方式和本任务有意改的行为，严格程度不能降）；
   - 上机核对清单：按键到出字的时间、长按弹出、上滑、九键读音、停手整理的实际效果。
10. 主工程普通全量 ctest 通过（含 `ui-scale`、`keyboard-pages`、`autotest-cap`，以及其它任务改动以外的 `sms-multipart`）；ASan 跑键盘和输入相关测试、`ui-scene-*`、`tap-*` 通过；S31 宿主机测试通过；`ports/esp32s31/build.sh` 编译通过，没有新警告。
