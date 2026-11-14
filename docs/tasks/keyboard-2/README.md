# keyboard-2 截图

模拟器出的干净整页图，684 × 1216，没有调试框。

- `before/`：改前（主线 keyboard.c，Codex 那一轮留下的对照图）。
- `shots-96/`：现在的键盘，键高 96（产品默认）。`KB-<页面>-default / -large / -kid`：默认字号、最大字号、孩子身份。页面：`full` 全键、`nine` 九宫格（有读音列）、`number` 123 面板、`number-field` 数字框、`phone-field` 电话框、`symbols-0..6`（最近、中文、英文、数学、单位、图形、序号各第一页）、`expanded` 候选展开、`popup` 长按 e、`caps-lock` 大写锁定。
- `shots-108/`：同一套，键高 108（只在测试编译时覆盖，产品默认仍是 96；96 还是 108 由用户定。108 的键盘比主线高 48 像素）。
- `steps/`：验收路径，先触摸（`Touch-*`）再 AI 能力调用（`AI-*`），每步一张。

重新生成：`python3 tests/keyboard2/run.py --build build --case shots [--binary sim-keyboard2-108]`，`INK_KEEP_EVIDENCE=1 python3 tests/ime_keyboard1.py --build build --pages`。
