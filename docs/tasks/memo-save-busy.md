# memo-save-busy：屏幕一直在刷时，保存不再超时（备忘边打字边存）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md` 第 11 条。

现象（见 `docs/tasks/backlog.md` 的【待修】）：备忘一边打字一边保存，屏幕一直在刷新时 `memo.save` 3 秒超时（`result=7`），提示“备忘没存上”。线索：存储服务写内部闪存前取显示的闪存保护（`system/services/storage/storage_service.c` 约 342、356、514 行调用 `ink_display_flash_guard(true)`），取保护要等波形做完、面板空闲（`system/services/display/display_service.c` 约 296 行；模拟器走等确认的分支）。连续打字时新刷新不断进来，保护迟迟拿不到。和性能项“存储等波形做完”同源。

## 要做的

1. 先写能稳定复现的测试（模拟器：边连续打字边保存，原代码超时）。
2. 修根因：保护请求到了以后，当前这一次刷新做完就把保护交出去，后来的刷新排在保护后面（持续打字不能饿死存储）。真机直驱分支（`async_updates`，在相位边界停住）照旧。不放宽 3 秒超时，不跳过保护（闪存写会冻住正在播放的波形）。
3. 写完闪存后，排着的刷新照常补上，最终屏幕和文本一致；打字到显示的耗时不能明显变差（报告给出改前改后的数字）。

## 范围和预算

- 只改：`system/services/display/display_service.c`、必要时 `storage_service.c` 里取保护的那几行、相关测试、`docs/tasks/memo-save-busy/`。不碰别的任务在改的文件。
- 产品代码不超过 80 行；不新增任务或线程。

## 验收

1. 测试：连续打字时保存在超时内完成；保护期间不启动新刷新；保护释放后补刷；反向对照。
2. 主工程普通全量 ctest 过（含 `memo*`、`i18n-memo`、显示和存储相关、`perf-*` 预算）；`ports/esp32s31/build.sh` 编译通过、无新警告。
