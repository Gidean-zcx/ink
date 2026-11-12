# 键盘页面来源

查阅日期：2026-11-12。以下为代码核查【实现】，不是实机验证。参考页面结构、通用键位及 Unicode 符号集合，用 LVGL 实现；未引入上游运行时。

| 页面／行为 | 固定来源与行号 | 本次对应 |
| --- | --- | --- |
| 中英文全键盘、语言与布局切换 | fcitx5-android `eb5b50216e70484a009933d9519db76005f1faf3`，`input/keyboard/TextKeyboard.kt:33–75`、`KeyboardWindow.kt:68–83` | 保留 LVGL 键表和文本服务；功能键用短标签 |
| 数字键盘 | 同上 `NumberKeyboard.kt:24–55` | 三列数字、独立数字页；号码框自动进入 |
| 候选分页网格 | 同上 `candidates/expanded/GridPagingCandidateViewAdapter.kt:18–48`、`PagingCandidateViewAdapter.kt:18–46` | 六格、前后翻页、选中收起；数量和顺序由文本服务决定 |
| 符号分类、分页、返回 | 同上 `picker/PickerTabsUi.kt:34–135`、`PickerPaginationUi.kt:40–76` | 分类标签、显式分页和返回；不做滑动动画 |
| 字母长按符号 | 同上 `keyboard/TextKeyboard.kt:35–65`、`popup/PopupKeyboardUi.kt:35–93` | 同一字母备选符号、小区域弹出、明确取消 |
| 分类内容、最近符号 | Trime `76a9e763b148d747f6245f61d65e02595082f080`，`app/src/main/assets/shared/trime.yaml:686–712,720–759,920–929` | 中文、数学、英文；最近最多八个，现有成员设置存储 |
| 九键、局部更新、长按删除 | Mogo `864f2f9`，`main/ui/ui_keyboard.c:1–14` | 九键只中文，英文全键；内部编码不印在键帽或组合栏；删除使用 LVGL 原生重复及取消 |

fcitx5-android 根许可证 LGPL-2.1-or-later；Trime 为 GPL-3.0；Mogo 键盘文件头为 Apache-2.0。本任务未复制上游实现代码、未链接它们。文件哈希见 sources.json。

## 未照搬与限制

- 表情、剪贴板历史、手写、完整扩展语言符号集：没做。原有复制、剪切、粘贴、撤销和听写保留。
- 数字页沿用项目三列布局，未照 fcitx5-android 的两侧运算键布局。
- 没有上滑选符号；本次只做显式长按选择。
- Mogo 按下加深、下沉不搬，遵守 D-31。没有按键动画，不主动要求整屏刷新。
- 读音选择缺文本服务公开接口，未自行重写引擎；400 ms 候选整理未加入，候选即时显示，清理交现有刷新调度。
- 中文多字符省略号／破折号归一为单字符，保留项目已有中文分号。
- 最近行横向滑动查看其余项；最多存储八个。
