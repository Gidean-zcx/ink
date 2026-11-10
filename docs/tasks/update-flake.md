# update-flake：系统更新测试在 Linux CI 上失败

通用规矩：`ink-codex/briefs/subagent-rules.md`（仓库外，和仓库同级的工作目录）。

## 现象

- Linux CI（GitHub Actions）上 `integration-update` 失败 1 条：`tests/integration/update.c:508` “and the list says so”（取消下载以后 `job.list` 里没有 `"state":"cancelled"`），虚拟时间 5.370 秒；macOS 本机通过。失败从 ab505bba 开始出现（f3a07561 也一样，运行号 42758348196，可以用 `gh run view 42758348196 --repo Gidean-zcx/ink --log-failed` 看日志、`gh run download 42758348196 --repo Gidean-zcx/ink --name sim-reports` 取报告），61f55c19 和之前通过。这之间合入了 port-hal-1（只改 S31 端口）、memo-record（媒体服务的转写、备忘的后台补漏会在 `net.online` 变真时运行）、i18n-1。
- 前一步 “it stops”（等 `update.state` 回到 `available`，最多 5 秒）是通过的，紧接着读 `job.list`。

## 要做的

1. 查清原因，用时间线说明（事件和时刻），不要只给结论：是产品问题（例如取消后任务记录的状态晚于 `update.state` 写入、后台执行器被别的作业占住）还是测试假设（读得太早）。可以换随机种子（`--seed N`）或注入延迟复现；先找到 `update.cmake` 里对应的阶段和命令。
2. 是产品问题就修产品，配一个能稳定复现的测试；是测试假设就按产品的真实合同改测试（等明确的状态或事件，不盲等）。
3. 改完用种子 1～30 跑这一阶段全部通过，再跑全量测试。

## 边界

只碰和这个问题直接相关的代码和测试。

## 验收

报告里有时间线和原因、改了什么、为什么对；种子 1～30 全过；全量测试和内存检查通过；改了产品代码就跑 S31。
