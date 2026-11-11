# calendar-events 执行报告

基线：`a108101cbb81bd4a0b4785d66f0ab3a4fcb32003`。分支：`claude/codex-calendar-events`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 进行中 | 正在核对共用提醒存储、排程、编辑页和成员过滤 | 待记录 |
| 2 | 未运行 | 全量、ASan、能力清单、S31、gcccheck、ui-scale、隐私扫描 | 待记录 |
| 3 | 未运行 | 设计差异及截图 | 待记录 |

## 没做到的

- 暂无结论；只编译、跑宿主机测试，不碰设备。

## 和设计不一致的

- 待核查。

## 改过的已有测试

- 暂无。

## 要用户定的

- 暂无。

## 顺手发现

- 暂无。

## 新增的机制

- 按任务要求扩展既有提醒记录和能力；不另建日程服务、线程或存储集合。

## 要改的文档

- 共享能力清单不直接修改，生成到构建目录并校验；合并时由主会话同步。

## 执行记录

- 【实测】工作区开工干净，HEAD 与指定基线一致；已读总规则、任务书及相关项目资料。
- 构建与测试用 `taskpolicy -c utility`，并行上限 2。环境变量使用任务指定目录，不把本机绝对路径写入提交。
- 【实测】首轮定向通过：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/calendar_events/run.py --build build`，创建 159 项、重启 31 项检查，均零失败。证据：`build/test-out/calendar-events-a0d1bxfw/`；日志：`work/calendar-events/target-test.log`。
- 【实现】既有记录尾部扩展静默、全天、时长字段；旧长度记录按默认响铃读取。静默日程不进入排程唤醒或通知路径，开始后保留；有时长的已响提醒也保留供跨天查询。没有新增存储集合或后台任务。
- 【实现】`calendar.list` 使用本地日期的左闭右开范围，列出重叠日程和提醒并展开重复；结果超过原 4 KiB 缓冲时报错，请求缩小范围。`reminder.add/update` 接受 `ring`、`all_day`、`end`；未传的新提醒默认响铃，更新未传的新字段保留，空结束字符串清除时长。
- 【实测】初次 gcccheck 原话 `gcccheck: 5 compiled, 0 with findings`；ui-scale 原话 `ui-scale: 51 files, 0 violations`。最终检查和反向对照待补齐。
