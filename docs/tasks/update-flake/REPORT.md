# update-flake 报告

（Claude 子代理完成；运行环境不让子代理写报告文件，主会话按它的回报整理保存。）

检出 `ink-codex/update-flake/repo`，分支 `claude/sub-update-flake`，基线 `f3a07561`。

## 结论

**产品问题**：下载停下或失败时，`dl_finish` 先写 `update.state=available`、再记探针、最后才结束任务记录（`ink_task_end`）。后台执行器刚写完状态就被抢走 CPU 时，别人会看到“状态已回到 available，任务列表里还是 running”。下载成功那条路本来就先结束任务、再写 `ready`，只有停下和失败这条路反了。修法：两行对调，和成功那条路一致。

## 时间线（seed 28，本机复现，虚拟时间）

- 4.249 s 下载开始；测试取消，`dl_finish` 在 exec.bg 上运行。
- 5.306249 s exec.bg 只跑了 43 µs，写完 `update.state=available` 就被换下。
- 5.3063～5.3178 s 墨水屏刷新的准备任务 epd_prep0、epd_prep1 各在一个核上跑约 11 ms，优先级低的 exec.bg 停在写状态和结束任务之间。
- 5.317792 s 测试任务（优先级更高）先拿到核，轮询看到 `available`，“it stops”通过，紧接着 `job.list` 仍是 `running, 40%`，失败。
- 5.318671 s exec.bg 才执行 `ink_task_end`，任务变成 cancelled。

从 ab505bba 起才在 CI 上出现，推断是这几次合并改变了 CPU 时间分布，默认种子在 Linux 上正好让一次刷新落进这个空档（没有逐个提交二分）。空档本身是真实的，真机双核同样可能出现。

## 验收

| 编号 | 结论 | 证据 |
| --- | --- | --- |
| 稳定复现的测试 | 做到 | 【实测】`tests/integration/update.c` resume2 阶段：取消前用 `INK_EXEC_DIRECT` 注册 `update.state` 的观察者，在写状态的任务里同步睡 100 ms 一次（模拟被刷新占住两个核）；只加测试不改产品，seed 1、2、3 都在“and the list says so”失败（`"state":"running","percent":40`，seed 1 在 5.370 s，和 CI 同一时刻）；加上修复后通过 |
| 种子 1～30 | 全过 | 【实测】修复前不加新测试 seed 28 原样复现；修复加新测试后 1～30 全部 PASS |
| 全量 | 通过 | 【实测】`100% tests passed out of 311` |
| 内存检查 | 通过 | 【实测】build-asan 里 integration-update `100% tests passed out of 1` |
| S31、gcc | 通过 | 【实测】S31 退出 0；gcccheck 2 个文件 0 处，gccscan、rscan 无输出 |

改动：`system/services/update/update_service.c` 加 2、删 2（挪一行加注释）；`tests/integration/update.c` 加 15。原有检查的期望一字没改；`job.finished` 的订阅方（`activity_card.c`）不读 `update.state`，对调顺序不影响别处。

## 没做到的

没有逐个提交二分；刷新准备任务同时占两个核约 11 ms、饿住 exec.bg 是否合理，没查。

## 顺手发现

原有的“and the list says so (%s)”打印 `g_result` 没限长（这个目标带 `-Wno-format-truncation`，CI 不报）；加打点会挪动时序，这类竞争只能靠 trace 和少量打点看。
