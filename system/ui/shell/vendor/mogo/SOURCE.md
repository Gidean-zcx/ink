# Mogo 九键读音（键盘这一侧）

- 项目：`github.com/chopper1026/mogo-e4p`，固定提交 `864f2f9dc75980a67d2d6b67b17365020b7a50ce`（与 `third_party/mogo/SOURCE.md` 同一个提交），取自本机只读副本 `ink-hw/upstream/oss/mogo-e4p`。
- 只拷整函数和整数组，函数体一个字没改；和我们状态的对接全靠拷贝之前那一段垫片宏（文件里标着“ink shim”，是我们写的）。

| 我们的文件 | 上游文件和行 | 上游 SHA-256 | 许可（上游文件头） |
| --- | --- | --- | --- |
| `mogo_nine.h` | `main/ui/ui_keyboard.c:1-14`（文件头）、`:48`（`groups[]`）、`:161-192`（`syllables()`、`composition_text()`） | `2d95bf1f7953464fc96ad23cc73102217691aef1708a5edf1d3c07758021c5aa` | Apache-2.0 |
| `mogo_nine.h` | `main/ui/ui_ime.c:1-4`（文件头）、`:47-57`（`digit()`、`ui_ime_digits()`） | `76b8a548bbca9fbbd2c747d681fc2d242b4ae23bda3f49996fc6ae0e0bb1af80` | Apache-2.0 |
| `mogo_syllables.h` | `components/book_search/search_table.h:1-6`（文件头）、`:6170-6313`（`search_syllable_offsets`、`search_syllables`，425 个音节） | `4966b633afdc2b93bedb4b291b33a0a86310c69442ba6e9d8e99818e6394cb0b` | MIT（pypinyin 0.55.0 生成，见 `LICENSE.pypinyin`） |
| `mogo_syllables.h` | `components/book_search/book_search.c:1-8`（文件头）、`:164-167`（`book_search_syllable()`） | `6d1390b85bdb90a4917e41fadbf7df699495c67e2995472b0419cae6fbf102fd` | Apache-2.0 |
| `LICENSE.pypinyin` | `components/book_search/LICENSE.pypinyin` 原样 | `1e6c90014b4912815c296ee64bb6f6280af47e6d4c5d80e86232dfc5defe764c` | MIT |
| `LICENSE` | `third_party/mogo/licenses/Apache-2.0.txt` 复制 | `c2ede378097fb1fcdcd128c47880c918663f475eba53e1e2dc59d95b537c8809` | Apache-2.0 全文 |

## 改了什么

- 函数体、数组都没改。垫片宏把 Mogo 的名字接到我们这里：`s_raw` → `ink_text_composing()`，`s_chinese` → 普通输入框且是中文，`s_nine` → 设置 `text.nine_key`，`s_syllable_offset` → `K.reading_offset`（`size_t`，和原文同类型），`s_best_roman` / `s_best_consume` → 选中的读音和它的长度（没选时为空串，`composition_text()` 走原文自带的“全显示字母组”分支），`UI_IME_RAW_MAX` → 48。拷贝之后 `#undef` 掉这些名字。
- 加了两行原型（`ui_ime_digits`、`book_search_syllable`），原文里它们是非 `static` 函数，放进头文件后要原型才不报 `-Wmissing-prototypes`。
- 拷来的 `static` 函数和数组都被用到（`syllables`、`composition_text`、`groups`、`digit`），没有 `-Wunused`。

## 我们自己写的接线（不算拷贝）

在 `system/ui/shell/keyboard.c`：

- 读音列 3 格、和键一样高（对应 Mogo `ui_keyboard.c:211-215` 的 4 格 50 px）：`syllables()` 每页最多给 3 个；还有更多或已翻过页时只显示前 2 个，第三格写“更多”；正好 3 个且在第一组时显示 3 个（`sync_readings`）。
- “更多”（对应 `:335-337`）：`reading_offset` 加上**这一页显示的个数**（2），不是 Mogo 的固定 3，否则第三个读音会被跳过；到头回 0（`reading_action`）。
- 组合串一变 `reading_offset` 清零（对应 `:60` 的 `refresh()`）。
- 读音字号比键小（Mogo 读音 22 px、字母组 30 px，`:213`、`:251`），我们用备注字号。
- 提示“先选词或按重输”“拼音过长，先选词”（对应 `:279-287`、`:300-304`、`:269-271`）写在组合栏位置，下一次按键才消失。

## 许可范围

Mogo 仓库是分范围许可：只有明确标了 `LicenseRef-Kiikoread-Personal-Study-1.0` 的内容才是个人学习许可。上面几个文件在 `864f2f9` 的文件头分别是 Apache-2.0 和 MIT，不是个人学习许可。

## 原文能带的测试

没有。Mogo 的 `tools/test_search.py` 测的是书名匹配，和读音列表无关。我们的测试在 `tests/keyboard2/kb.c`（`nine` 用例：读音顺序、更多翻遍不漏、到头回第一组、选 zhong 后有“中国”）。
