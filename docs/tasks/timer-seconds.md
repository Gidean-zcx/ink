# timer-seconds：计时器和秒表按秒走

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md` 第 11 条。

用户（2026-11-12）：“其他项目没有现成的时钟 app 移植吗，没有的话你就改。”主会话查过已固定的原厂、CrossPoint 一系、kiikoread、CrossMux 副本，没有能移植的计时器或秒表。现在的计时器和秒表是我们自己的时钟应用（`system/apps/clock/clock.c`，能力 `clock.timer*`、`clock.stopwatch*`）；审计 UX02 说能力已支持秒，但选择和显示都取整到分钟。所以改我们的。

## 要做的

1. 计时器能选到秒（例如 30 秒、1 分 30 秒），倒计时显示“分:秒”，一小时以上显示“时:分:秒”；到点照旧响铃。
2. 秒表按秒显示（分段计时也带秒）。
3. 墨水屏刷新：走秒时每秒只局部快刷数字那一块（D-127），页面其余部分不动；锁屏或离开页面时不每秒刷，回来时补上正确的时间。
4. AI 能力沿用现有 `clock.timer_set` 等的参数，加上秒（例如 `seconds`）；页面快照给出剩余或已走的秒数。

## 范围和预算

- 只改：`system/apps/clock/**`、必要时 `system/services/time/` 里计时器的秒参数、相关测试和截图、`assets/i18n/en.tsv`（只加本任务新文案的英文）、生成的 `docs/system/capabilities.md`、`docs/tasks/timer-seconds/`。不碰别的任务在改的文件（`almanac-1` 也在改 `system/services/time/`，只动计时器那几处）。
- 产品代码不超过 120 行；不新增常驻任务（走秒用现有定时机制，离开页面就停）。

## 验收

1. 测试：选 90 秒，倒计时显示和响铃时刻准确；秒表按秒走；走秒时每秒只刷数字区域；离开页面不刷，回来时间正确；AI 设秒和页面一致；反向对照。
2. 主工程普通全量 ctest 过（含 `clock*`、`ui-scale`、`capability-table-doc`）。
3. 截图：计时器选择、倒计时中、秒表，常规和 120%（`docs/tasks/timer-seconds/`，单张不超过 300 KB）。
