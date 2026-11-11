# 记忆翻牌规则来源

来源：[taniarascia/memory](https://github.com/taniarascia/memory)，固定提交 `4c991ddb6b7db2f72945b09d9c8e82af6356482b`，查阅日期 2026-11-12。使用任务指定的只读副本，没有下载其他版本。

上游只有浏览器 JavaScript，本任务明确允许将规则移植为 C。未复制图片、CSS 或 DOM 界面。许可证原文见 [LICENSE](LICENSE)。`engine.c` 保留原作者版权。

| C 逻辑 | 对应上游 `js/script.original.js` | 适配 |
| --- | --- | --- |
| `ink_memory_start` | 51–59 行 `cardsArray.concat`、洗牌、初始选择 | 八种系统图标各两张，4×4；Fisher–Yates 取代随机比较排序；种子来自 HAL，测试用固定种子 |
| `ink_memory_flip` 的准入和第一／第二张 | 104–127、135 行 click handler | 排除重复、已配对、等待翻回时的第三张；下标代替 DOM target |
| 相等时保留配对 | 85–90 行 `match`、129–132 行匹配判断 | `open=2` 表示配对，立即保留，避免墨水屏多一次刷新 |
| `ink_memory_hide` | 92–101 行 `resetGuesses`、133 行定时调用 | 未配对保持可见 1200 ms，再由页面的 LVGL 定时器翻回 |

任务要求补充的逻辑在 `memory.c`：每选完两张记一步；用 OSAL 单调时钟累计页面内用时；八对为通关；最好成绩先比较步数、同步数比较秒数，存在当前成员的 `memory.best` 集合内。界面与 AI 共用 `games.memory.*`，未翻开的图案不进入状态或页面快照。只重绘变化的卡片和计数文本。

## 固定输入校验

由执行时的 `SHA-256` 记录在下表；没有复制上游图片素材。

| 原文件 | SHA-256 |
| --- | --- |
| `js/script.original.js` | `88572d9d1736ebda498309ddd25553d17c899e1595dfed30003477b2c02d632f` |
| `LICENSE` | `02d1d3220ef0f798cf06595984a4ca4095829aedd18e6ac429ad04f2cfe9cfff` |
