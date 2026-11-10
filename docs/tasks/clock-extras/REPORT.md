# clock-extras 执行报告

日期：2026-11-10。基线：`eec5d569be028b539e8778690c239a6947767864`。分支：`claude/codex-clock-extras`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 秒表操作、计次、跨页与模拟时间 | 【实测】通过 | 0.1 秒误差窗口；三次计次、暂停继续、离页、锁屏、换人、99 条及重启清零 | 专项命令见下 |
| 2 世界时钟、夏令时、跨日与成员保存 | 【实测】通过 | 欧美春秋四个切换点、跨年跨日、九选六、增删与真实进程重启 | 同上 |
| 3 局部刷新、能力、快照及中文英文界面 | 【实测】通过 | 每秒更新频率及控件不销毁；AI 开秒表、加城市；两语言截图、文字宽度检查 | 同上 |
| 4 原有回归、全量与内存检查 | 未运行 | 待补 | 待补 |
| 5 S31 与 gcccheck | 未运行 | 待补 | 待补 |

## 没做到的

- 全量、内存检查、S31 及反向对照尚在进行。

## 和设计不一致的

- 新页按 D-117 复用现有主题、胶囊、卡片与分页布局；没有画板。尚未发现和任务设计不一致的行为。

## 改过的已有测试

- `tests/integration/ai_surface.c`：只在 AI 能力清单增加 11 个新能力及必填的 `zone` 参数，原期望未改。
- `tests/capability_table/test_catalogue.py`：新增翻译标记的字面量元数据检查，原断言未改。

## 要用户定的

- 暂无；秒表按现有计时页的成员边界实现，重启不保留。

## 顺手发现

- 暂无。

## 新增的机制

- 成员秒表：任务书第 1 项要求；仅内存记录单调时钟起点、累计时长和 99 个计次，清零同时停止，重启不保留。
- 成员集合 `clock.world`：任务书第 2 项要求；沿用通用数据集合的 JSON 记录、持久化和成员隔离，记录 id 为稳定时区索引加一，正文为 `{"zone":索引}`，上限六条。
- 页面可见时的一个 LVGL 秒级定时器：任务书第 1 项要求；暂停或离页即删除，锁屏不刷新，不持有保持唤醒引用。不新增任务、线程、文件格式、队列或后台重试。

## 要改的文档

- 合并时更新共享的能力表；本任务不修改共享生成文档。

## 复现与里程碑

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/clock_extras.py --build build
```

- 第一里程碑：【实测】创建轮 `exit=0, pass=422, fail=0, finished=1`，重启轮 `exit=0, pass=44, fail=0, finished=1`。日志 `work/targeted.log`。
- 【实现】两个新页面和 11 个能力均在时钟应用内，按钮和 AI 走同一能力；成员集合只读开放给通用数据能力，写操作由城市增删能力负责六条上限。页面快照含可见计次／城市、分页、秒表运行状态及精确毫秒值；孩子沿用计时器权限。
- 【实测＋实现】专项发现并集中修正一处旧根因：手动校时后分钟任务仍沿用旧截止点，改为按新墙钟重排；世界时钟同时订阅已有 `time.changed`，覆盖日期变化但当天分钟数相同的情况。没有给每个城市另加定时器。
- 初次专项因未生成资源而启动失败；资源生成后，ASan 定位到新字段漏填语义角色导致空指针，已补齐并通过上述专项。保留诊断日志 `work/asan-debug.log`，不把这次失败写成内存检查通过。

截图（模拟器界面缓冲，非实机；使用上述命令保留）：

- 秒表中文：`build/clock-extras-evidence/sim-clock-extras/create/clean-stopwatch-zh.png`
- 秒表英文：`build/clock-extras-evidence/sim-clock-extras/create/clean-stopwatch-en.png`
- 世界时钟中文：`build/clock-extras-evidence/sim-clock-extras/create/clean-world-zh.png`
- 世界时钟英文：`build/clock-extras-evidence/sim-clock-extras/create/clean-world-en.png`
- 同目录 `snap-*.png` 为带墨水屏残影的输出；`clean-clock-home-en.png` 核查四个入口同排且文字不截断。首页原有闹钟正文仍为旧中文文案，本任务没有扩展旧页翻译范围。
