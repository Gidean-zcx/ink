# games-2 执行报告

基线：`1b68f7f18197f492431ffdc794082bb77c158c2a`。状态：实现接入完成，正在验证。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 专项通过，11 组反向对照通过 | 【实测】常规 1048、重启 13、120% 754 条检查，均零失败；含离页 AI 翻牌回归，另 11 组撤销行为均预期失败 | `taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/games2/run.py --build build` |
| 2 | 待验证 | 普通全量、内存检查、S31、gcccheck、尺寸 | 待补 |
| 3 | 截图已生成，视觉核对进行中 | 【实测】十二张，13,529～23,398 字节，均低于 300 KB | 专项自动截图；见下表 |

## 没做到的

已实现两游戏、成员成绩、能力、页面快照与局部刷新；专项常规／重启／120% 已通过；最终回归、全量、反向对照待跑完。

## 和设计不一致的

待核对。

## 改过的已有测试

- 应用数 20→22，登记新增的两个应用。
- AI 能力名单增加十个可调用能力；未删断言、未放宽预算。

## 要用户定的

暂无；按任务书实现固定 4×4 翻牌与约 0.5 秒一步的贪吃蛇。

## 顺手发现

- 新测试首轮遗漏等待异步成员数据库打开及触摸转向落到下一步，3 条失败；已等待明确的就绪／步进状态，未加盲等或改产品预算。
- 【实现】核对时发现离页 AI 翻牌也需要重建页面定时器，成功操作统一在能力处理末尾打开页面，增加回归。

## 新增的机制

- `memory.best` 成员集合：任务要求按家人存最好步数与用时；复用现有数据库。
- `snake.best` 成员集合：任务要求按家人存最高分；复用现有数据库。
- 页面 LVGL 定时器：任务要求翻回延迟和贪吃蛇 500 ms 步进；离开即删除，暂停／结束不运行。
- `games.memory.changed`、`games.snake.changed` 成员事件：任务要求 AI 与软总线接入；不新增线程或常驻任务。

## 要改的文档

任务书明确允许生成能力清单，已用原有生成器更新；新增词条只追加本任务用到的 26 条英文。

## 里程碑

- 已核对两个固定提交及 MIT 许可证，函数对应和输入哈希见各游戏的 `SOURCE.md`。
- 【实测】S31 未注册两游戏的基线构建 exit=0，固件 4,868,640 字节；主工程基线构建 exit=0。
- 【实现】规则、UI、成员成绩、能力和测试已接入；产品代码当前约 430 行，未超 600 行。
- 【实测】两个页面 `ui-scale: 2 files, 0 violations`。

- 【实测】GCC 检查覆盖四个新增产品 C 文件、专项驱动、两个登记测试及启动点：`gcccheck: 8 compiled, 0 with findings`。

## 截图

| 场景 | 100% | 120% |
| --- | --- | --- |
| 翻牌 开始 | [memory-start-100.png](memory-start-100.png) | [memory-start-120.png](memory-start-120.png) |
| 翻牌 进行 | [memory-playing-100.png](memory-playing-100.png) | [memory-playing-120.png](memory-playing-120.png) |
| 翻牌 结束 | [memory-end-100.png](memory-end-100.png) | [memory-end-120.png](memory-end-120.png) |
| 贪吃蛇 开始 | [snake-start-100.png](snake-start-100.png) | [snake-start-120.png](snake-start-120.png) |
| 贪吃蛇 进行 | [snake-playing-100.png](snake-playing-100.png) | [snake-playing-120.png](snake-playing-120.png) |
| 贪吃蛇 结束 | [snake-end-100.png](snake-end-100.png) | [snake-end-120.png](snake-end-120.png) |
