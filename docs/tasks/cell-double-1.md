# cell-double-1：4G 宿主机测试替身不再因机器慢而偶发失败

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3 条）。

现象：`unit-s31-cell*`（`ports/esp32s31/tests/cell.c` 加替身 `cell_host.cc`）在 CI 的 macOS 机器上偶发失败，单独跑（`RUN_SERIAL`）也会，主会话合并检查时机器满载也遇到过：日志依次是 `TX task: ACK timeout`、`timeout waiting for active state`，然后 `cell.c:16` 的 `at_ok` 断言失败。原厂 `uart_eth_modem.cc` 等应答 100 ms（`kAckTimeoutMs`，原厂代码逐字保留，不能改）；替身用真实时钟（`scale = 1`），线程调度一慢就错过应答。本机安静时连跑都过。

## 要做的

1. 先在本机复现：一边用 `ctest -j8` 跑别的测试（或写个忙循环占满 CPU），一边 `--repeat until-fail:30` 跑 `unit-s31-cell*`，记下失败率。
2. 只改替身，不改原厂代码、测试断言和超时：让替身的应答不依赖线程调度时机。可以在 `Transmit` 里同步完成应答的电平和中断回调，可以给替身一个等应答时暂停走动的时钟，也可以用别的办法，但要证明可靠，并写清真机上为什么没有这个时序问题。
3. `unit-s31-cell-negative`、`unit-s31-uhci-negative` 的反向对照仍按预期失败（替身不能把真正的协议错误也吞掉）。

## 范围和预算

- 只改：`ports/esp32s31/tests/cell_host.cc`、`ports/esp32s31/tests/cell_stub/`、`cell_negative.py`、`uhci_negative.py`、相关的 CMake 测试登记、`docs/tasks/cell-double-1/`。不碰别的任务在改的文件。
- 不改 `ports/esp32s31/vendor/**`、产品代码、断言和超时。

## 验收

1. 满载下 `unit-s31-cell*` 连跑 30 次 0 失败（报告写改前的失败率）；八个用例和两个反向对照照旧。
2. 主工程普通全量 ctest 过；S31 宿主机测试过。
