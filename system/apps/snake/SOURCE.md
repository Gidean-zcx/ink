# 贪吃蛇规则来源

来源：[patorjk/JavaScript-Snake](https://github.com/patorjk/JavaScript-Snake)，固定提交 `3243086c0badb4b4c6bedfe6d95d5a52a7caaa88`，查阅日期 2026-11-12。使用任务指定的只读副本，没有下载其他版本。

上游只有浏览器 JavaScript，本任务明确允许将规则移植为 C。未复制 DOM 界面、字体、CSS 或图片。许可证原文见 [LICENSE](LICENSE)，仅将 CRLF 行尾转成 LF，文字未改。`engine.c` 保留原作者版权。

| C 逻辑 | 对应上游 `src/js/snake.js` | 适配 |
| --- | --- | --- |
| `ink_snake_start` | 558–603 行 `reset` | 12×12 棋盘，初始三格，朝右；不引入对象池或链表 |
| `ink_snake_turn` | 351–393 行 `setDirection`、`handleArrowKeys` | 保留 0上1右2下3左和禁止反向；每步只接受一次转向，不移植可选预输入队列 |
| `ink_snake_step` 的移动与撞击 | 399–475 行 `go` | 数组移动蛇身，检查墙和自身；尾格即将腾空时可进入 |
| 吃食物、加长、加分 | 482–516 行 `eatFood`、1510–1520 行 `foodEaten` | 每份食物长一格；上游以长度记成绩，本任务分数为吃到的数量（长度减三），排序相同；不移植 Rush 加速模式 |
| `food` | 633–635、681–720 行 `getRandomPosition`、`randomlyPlaceFood` | 从空格中选择；满盘即结束，不用反复随机碰运气 |
| 暂停、继续 | 340–345 行 `setPaused/getPaused`、403–409 行 `go` 的暂停分支 | 页面 LVGL 定时器每 500 ms 一步，离开页面销毁定时器并暂停；重新打开后显式继续 |

任务要求补充的逻辑在 `snake.c`：最高分写当前成员的 `snake.best`；屏幕方向键、翻页键相对左／右转、AI 全部走 `games.snake.*` 能力，受现有名单、时段规则约束；只对变化格声明 A2 快速刷新。没有新增任务或线程。暂停和结束的计时器不执行游戏逻辑。

## 固定输入校验

由执行时的 `SHA-256` 记录在下表。

| 原文件 | SHA-256 |
| --- | --- |
| `src/js/snake.js` | `c666784da7011050f29df91519e68e81e2ef338fdd7eb5e8fa5f37525a280edd` |
| `LICENSE` | `2a1ed7e59a7d4fc0e89401334c54385aef9e690b3cc84a4d88cc754bfd7da050` |
