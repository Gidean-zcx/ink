# audit5-fix 执行报告

基线：`8d2a1d9a368bf9d50084dbc1810cea33e1705dde`。仅本地分支提交，不推送。四项定向回归已通过，正在执行全量验收。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B5-01 确认说明 | 定向通过【实测】 | `audit5-b5-01`：true / false / 省略参数的真实待确认说明如实；均取消，不执行删除。单独换回基线 `confirm.c` 后 4 条断言失败 | 下文定向及反向命令，case `confirmation` |
| B5-02 索引持久性 | 定向通过【实测】 | `audit5-b5-02`：成功后 1 μs 断电，新进程恢复；新增文件有编号，统计 8 B；改名原编号仍可用，统计 3 B。单独换回基线文件服务，新建/改名重启分别失败 3 / 2 条 | 下文命令，case `index` |
| B5-03 回执成员 | 定向通过【实测】 | `audit5-b5-03`：孩子日志→主页→独立回执是自己；点孩子日志行仍看孩子；可选成员参数鉴权。单独换回基线 `ai_pages.c` 后 4 条断言失败 | 下文命令，case `receipts` |
| S31 等待换算 | 定向通过【实测】 | `unit-s31-osal-ticks`：相对时长向上取整加 1，保留 0、永久等待和饱和；起点差及 tick 回绕不改变结果；基线实现断言失败 | 下文定向命令 |
| 全量、内存、S31 构建 | 待验证 | 尚未运行 | 待补 |
| 范围、行数、隐私 | 待验证 | 初始工作区干净，HEAD 与基线一致 | `git status --short`、`git rev-parse HEAD` |

## 没做到的

- 全量验收尚在执行。没有运行 Linux 宿主 GCC、远端 CI 或设备；断电均为模拟器断电。

## 和设计不一致的

【实现】未修改设计合同。确认说明仍由统一策略层生成；按钮与 AI 共用回执能力和成员校验。文件持久性集中在失效处理和 `index_edit` 末尾；`index_background` 的批次写盘完全未改。

## 改过的已有测试

- `tests/esp32s31/osal_ticks.c`：1 μs 期望 1→2 ticks，10,001 μs 期望 2→3 ticks；这是任务明确要求的“向上取整后加 1”，不是放宽预算。新增起点差、整 tick、回绕及频率用例。
- `tests/file_index_3/run.py`：两个反向变体的源码匹配串跟随失效调用的新位置和参数更新，断言、预算不变。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

无新增任务、状态机、文件格式或配置项。`durable` 仅区分现有文件操作和不应阻塞的平台回调；后台调度机制不变。

## 要改的文档

- 主会话可在审计汇总同步 B5-01～03 及 S31 换算的修复状态；原审计记录本轮未改。
- `shell.ai_receipts` 的可选 `member` 参数需主会话重生成共享能力清单；未修改生成的 `docs/system/capabilities.md`。

## 复现命令与证据

- 配置沿用任务书字体、Python、LVGL，额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`。中枢测试使用本检出代码：`PYTHONPATH="$PWD/hub"`。
- 定向：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R 'audit5-|unit-s31-osal-ticks'`，4/4 通过，退出 0。证据 `build/audit5-fix/fixed-tests.log`。
- 反向（case 分别为 `confirmation`、`index`、`receipts`）：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audit5_fix/run.py --build build --case CASE --baseline 8d2a1d9a368bf9d50084dbc1810cea33e1705dde`。每次只替换对应产品文件，保持其余修复和最终测试；三次均按业务断言失败退出 1。证据 `build/audit5-fix/reverse-{confirmation,index,receipts}.log`。测试驱动检查实际 `test.fail`，不把模拟器退出 0 误当通过。
- 首次定向执行曾因尚未生成字体/图标未进入业务场景；生成资源后重新运行。此启动错误不计入反向证据。
