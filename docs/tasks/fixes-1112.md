# fixes-1112：四个已知问题一起修（今天晚上的试用版要带上）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`。

按顺序做下面四份任务书。每份一个独立提交，报告写进各自目录（`docs/tasks/<名字>/REPORT.md`）；本目录只写一页总表（每份：做了什么、改了哪些文件、测试结果原话）。

1. `docs/tasks/ime-switch-key.md`：键盘切换中英文后第一个键不丢。
2. `docs/tasks/timer-seconds.md`：计时器和秒表按秒走。
3. `docs/tasks/memo-save-busy.md`：备忘保存的忙碌状态。
4. `docs/tasks/cell-double-1.md`：4G 宿主机测试替身不再因机器慢而偶发失败。

这四份任务书上午派过一次，刚开头就停了（各自克隆里只有计划提交），这次从头做，不用管那些克隆。

- 范围、预算、验收照各自任务书；做完一份先跑它相关的测试，再做下一份。
- 今天傍晚要合并装机：某一份卡住超过 1 小时就先跳过，在总表里写清卡在哪，继续下一份。
- 全部做完后：主工程普通全量 ctest、S31 宿主机测试、`ports/esp32s31/build.sh` 各跑一遍，结果原话写进总表。
