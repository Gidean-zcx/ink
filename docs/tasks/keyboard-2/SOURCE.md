# keyboard-2 来源

核对日期 2026-11-14。上游源码按固定提交只读取用，没有进仓库；Kotlin、YAML 运行时一行都没拷。

| 项目 | 固定提交 | 许可 | 取法 |
| --- | --- | --- | --- |
| fcitx5-android | `eb5b50216e70484a009933d9519db76005f1faf3` | LGPL-2.1-or-later | 按提交取单个文件到临时目录（下表的 SHA-256 与任务书一致） |
| Trime | `76a9e763b148d747f6245f61d65e02595082f080` | GPL-3.0（D-129：不用在乎许可） | 同上 |
| Mogo | `864f2f9dc75980a67d2d6b67b17365020b7a50ce` | 键盘相关文件头 Apache-2.0 / MIT | 本机只读副本；拷来的代码见 `system/ui/shell/vendor/mogo/SOURCE.md`、`system/services/text/vendor/mogo/SOURCE.md` |

上游文件 SHA-256：`PopupPreset.kt` c9147c5c3f6d631da9122d0b3dc71100feadea27f7b70786259f00a06a03770b；`PickerData.kt` 10b520a80d0274e4849bf6851ce5fd9ca13c4a15ff05c16809f4e090d117703b；`TextKeyboard.kt` 9e08d518a94370bfa631a6c286e7ff34bb4a8f4eb37fa81a0ddd1fbe2eed60e2；`NumberKeyboard.kt` 338163244460df50b21114d3aa87a7fa44d46ba94dd1732030d346bc16e69c6c；`KeyDefPreset.kt` 16a4be9b952b800d5f8c93f4b8f09e714cec9d5f4e69e16c17f20dbad165992c；`PickerWindow.kt` d55b1ccf6c7189f04a49ec214d692136fac700bb5ace944a707bec4586ed2b3b；`PickerPagesAdapter.kt` 74e77528d47c6dc898ba474db634c3944e468b2a7a305534e9f62e56f1c6d7f7；`trime.yaml` 44d29a6ace16ca9f59b3d6bc32b457a8ce41f8b080d94fdbe67e8d89900983f6。

## 每一页对应的来源

| 页面 | 来源（文件:行） | 我们怎么做，哪里没照搬 |
| --- | --- | --- |
| 全键盘 | fcitx `keyboard/TextKeyboard.kt:33-76`、`KeyDefPreset.kt:42-62`（AlphabetKey 的备选、上滑）、`:96-108`（CapsKey）、`:128-143`（Backspace） | 底行多一个九键/全键；功能键同一倍数宽；字母右上角小字深灰写备选（上滑提示）；大写改成点一次、再点锁定、再点取消（没有长按、双击）；没有按下放大气泡（D-31） |
| 长按备选 | fcitx `popup/PopupPreset.kt:12-69`、`popup/PopupKeyboardUi.kt:45-220` | 整张表一次出来，每格 ≥108，最多 3 行，有“收起”；点了才选，不靠拖；只刷弹出层 |
| 九宫格 | Mogo `main/ui/ui_keyboard.c:48`、`:161-192`（拷代码）、`:211-260`、`:274-361` | 读音列 3 格（2 + 更多，或 3）；第一格放“，”不放“选词”；右列删除、重输、“。”，不做“清空”；九键切全键一律提示，不转换 |
| 读音优先 | Mogo `main/ui/ui_ime.c:91-93`（拷代码） | 两个引擎各一行调用 |
| 数字键盘 | fcitx `keyboard/NumberKeyboard.kt:24-55` | 普通框的 123 面板照搬；数字框、电话框只放这个框收的键 |
| 符号面板 | fcitx `picker/PickerData.kt:13`（最近在前）、`picker/PickerWindow.kt:120-122`（先显示第一个符号分类）、`picker/PickerPagesAdapter.kt:77-80`（数字不记）、`picker/PickerPageUi.kt:65-71,104`、`PickerTabsUi.kt`、`PickerPaginationUi.kt`、`PickerLayout.kt:30-40`；Trime `trime.yaml:686-764` | 文字标签换行不滚动；选中的粗体加下划线；按钮翻页加页码；底行 26、◀、页码、▶、123、删除；中文输入先开“中文”，英文和英文框先开“英文”；密码框只有英文表、没有标签和最近 |
| 候选展开 | fcitx `candidates/expanded/GridPagingCandidateViewAdapter.kt:18-48`、`ExpandedCandidateLayout.kt:41-80` | 固定 3×2，底下一行 ◀、页码、▶、删除、键盘；盖板键翻页 |
| 刷新规矩 | Mogo `main/ui/ui_keyboard.c:1-14`、`:408-422`、`:442-483` | 按键本身不刷；候选变三次停手 400 ms 整理一次词栏（按着不整理）；删除连按用系统的 400/100 ms |

## 数据表（`system/ui/shell/keyboard_symbols.h`，由 `docs/tasks/keyboard-2/gen_symbols.py` 生成）

去掉的规则：键盘字体 sans-44 的字集（ascii、punct、pinyin、gb2312）画不出的字，以及同一张表里的重复。表只往后追加，“最近”存的编号是（表, 序号）。

| 表 | 来源 | 留下 | 去掉 |
| --- | --- | --- | --- |
| 中文 `SYM_CN` | Trime `trime.yaml:724-756` | 28 个，含“···”“……”“——”（Trime 给“——”的标签是“破折”，我们直接显示符号） | 没有（`{ click: "" }` 是 Trime 的换行，不是字） |
| 英文 `SYM_EN` | fcitx `PickerData.kt:16-26` 里的 ASCII | 40 个 + 末尾补的 `,` `.`（fcitx 把这两个放在键盘上，我们的面板是全部可打印 ASCII，网址框也要用） | 非 ASCII 的（“”·‘’¡¿¥€… 等） |
| 数学 `SYM_MATH` | Trime `trime.yaml:720-723` | 50 个 | 画不出：‱ ∯ ∬ ∭ ∰ ∟ ∣ ∂ ∆ ∉ ⊕ ⊿；重复：∝ ∈ ∧ ∥ ∞ ≌ ∪ ∨ ⊥ ∫ |
| 单位 `SYM_UNIT` | Trime `trime.yaml:761-764` | 5 个：℃ $ ￡ ￥ ￠ | 画不出：¥ € ฿ ㎡ ³ ℉ £ ₠ 上下标数字和符号 ½ ⅓ ¼ ⅔ ¾ ℅；“m”（是“m³”的一半，³ 画不出，单独一个 m 不是单位） |
| 图形 `SYM_SHAPE` | Trime `trime.yaml:757-760`（Trime 叫“特殊”） | 35 个 | 画不出的 120 多个（▽▼▷◁ ♻ ✓✗ ☀☼ ♪♫ ⇦⇧ ↖↗ ㊤ 及全部表情等） |
| 序号 `SYM_ORDINAL` | fcitx `PickerData.kt:60-70` | 42 个 | 画不出：❶-❿ Ⅼ-Ⅿ ⅰ-ⅿ ¹²³⁴ ₁₂₃₄ ½⅓⅔ ㊣；重复：① |
| 长按备选 `POPUP` | fcitx `PopupPreset.kt:12-69`（小写、大写两张） | 每个字母 2-7 个 | 画不出的带调和外文字母：ë ė ę ȩ ḝ ə ÿ ұ ү ӯ ў û î ï į ı ô ö œ ø õ â ä æ ã å ą ß ś š ş ð ğ ł ž ź ż ç ć č ¿ ¡ ñ ń（大写同） |

所有表的每个字都由 `tests/keyboard2/kb.c` 的 `kb_symbol_glyphs` 在模拟器里用键盘字体查过字形。
