# 键盘截图

均为模拟器生成的干净整页图（不叠加残影模型、无调试框），684 × 1216。默认字号为 1，最大字号为 3。`after-108` 仅在测试编译时覆盖键高，产品默认仍为 96。64 张图的大小和 SHA-256 见 [screenshots.json](screenshots.json)。

| 页面 | 改前默认 | 96 默认 | 108 默认 | 改前最大 | 96 最大 | 108 最大 |
| --- | --- | --- | --- | --- | --- | --- |
| 中文全键 | [图](before/Pinyin-normal.png) | [图](after-96/Pinyin-normal.png) | [图](after-108/Pinyin-normal.png) | [图](before/Pinyin-large.png) | [图](after-96/Pinyin-large.png) | [图](after-108/Pinyin-large.png) |
| 英文全键 | [图](before/English-normal.png) | [图](after-96/English-normal.png) | [图](after-108/English-normal.png) | [图](before/English-large.png) | [图](after-96/English-large.png) | [图](after-108/English-large.png) |
| 九键 | [图](before/Nine-normal.png) | [图](after-96/Nine-normal.png) | [图](after-108/Nine-normal.png) | [图](before/Nine-large.png) | [图](after-96/Nine-large.png) | [图](after-108/Nine-large.png) |
| 数字 | [图](before/Number-normal.png) | [图](after-96/Number-normal.png) | [图](after-108/Number-normal.png) | [图](before/Number-large.png) | [图](after-96/Number-large.png) | [图](after-108/Number-large.png) |
| 符号 | [图](before/Symbols-normal.png) | [图](after-96/Symbols-normal.png) | [图](after-108/Symbols-normal.png) | [图](before/Symbols-large.png) | [图](after-96/Symbols-large.png) | [图](after-108/Symbols-large.png) |
| 候选展开 | [图](before/Expanded-normal.png) | [图](after-96/Expanded-normal.png) | [图](after-108/Expanded-normal.png) | [图](before/Expanded-large.png) | [图](after-96/Expanded-large.png) | [图](after-108/Expanded-large.png) |

基线展开图使用 `ni`，改后展开图使用 `nihao`；用于检查布局。

| 操作 | 点按 | AI |
| --- | --- | --- |
| nihao | [图](steps/Touch-01-nihao.png) | [图](steps/AI-01-nihao.png) |
| 展开 | [图](steps/Touch-02-grid.png) | [图](steps/AI-02-grid.png) |
| 第二页 | [图](steps/Touch-03-page2.png) | [图](steps/AI-03-page2.png) |
| 选词收起 | [图](steps/Touch-04-picked.png) | [图](steps/AI-04-picked.png) |
| 符号 | [图](steps/Touch-05-symbols.png) | [图](steps/AI-05-symbols.png) |
| 数学 | [图](steps/Touch-06-math.png) | [图](steps/AI-06-math.png) |
| 符号翻页 | [图](steps/Touch-07-math2.png) | [图](steps/AI-07-math2.png) |
| 最近符号 | [图](steps/Touch-08-recent.png) | [图](steps/AI-08-recent.png) |
| 返回字母 | [图](steps/Touch-09-letters.png) | [图](steps/AI-09-letters.png) |
| 电话输入类型 | [图](steps/Touch-10-phone.png) | [图](steps/AI-10-phone.png) |
| 长按备选 | [图](steps/Touch-11-alternate.png) | [图](steps/AI-11-alternate.png) |
| 选备选 | [图](steps/Touch-12-alternate-picked.png) | [图](steps/AI-12-alternate-picked.png) |
| 九键编码输入 | [图](steps/Touch-13-nine.png) | [图](steps/AI-13-nine.png) |
| 选中国 | [图](steps/Touch-14-china.png) | [图](steps/AI-14-china.png) |

电话图使用备忘页面承载真实文本服务的 `INK_TEXT_PHONE` 合成会话；没有打开实际电话应用。

复现（`PY=$INK_DEPS/build-deps/pyenv/bin/python`，命令前均使用 `taskpolicy -c utility env INK_KEEP_EVIDENCE=1`）：

```sh
$PY tests/ime_keyboard1.py --build build
$PY tests/ime_keyboard1.py --build build --pages
$PY tests/ime_keyboard1.py --build build --baseline-pictures
$PY tests/ime_keyboard1.py --build build --key-height108
$PY tests/ime_keyboard1.py --build build --refresh-reverse
```

基线截图是手动模式，依赖给定基线提交；普通 ctest 不依赖 Git 历史。完整结论见 [REPORT.md](REPORT.md)。
