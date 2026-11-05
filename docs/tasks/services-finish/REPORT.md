# 系统服务收尾报告

2026-11-05，第 5 轮修复；开工基线 `9b976aa`，工作区干净。依据 [复查报告](REVIEW.md)、[任务书](../services-finish.md)及[总规则](../codex-rules.md)。上一轮报告与证据保留在 Git 历史。本轮进行中，未给出完成或通过结论。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R5-01 高：文件关闭及同类变更事件归属 | 已修，聚焦正反对照完成，待全量 | 【实现】句柄、短操作和作业携带原成员代次；【实测】真实录音、句柄及六类操作换人事件不进入孩子 AI | 待补 |
| R5-02 高：移动/拷贝跨换人拼接路径 | 已修，聚焦正反对照完成，待全量 | 【实现】源/目标、stat、作业和能力提供者共用接受时的上下文；【实测】四类路径、三个暂停阶段及儿童权限均通过 | 待补 |
| S1/S6 同类隔离排查 | 聚焦通过，待全量 | 【实测】任务标题归原成员，关闭释放槽后删除并复用编号不投递旧事件 | 下列聚焦命令 |
| S2/S3 既有策略、能力表 | 待回归 | 不改变产品决定与策略 | 待补 |
| S4 全量、内存检查和资源预算 | 未执行 | 构建最多 -j4、ctest 最多 -j3；构建及全量测试串行 | 待补 |
| S5 提交、隐私和差异检查 | 未执行 | 仅提交本任务相关文件 | 待补 |

【实测】普通构建开启 Werror，新增 `file_events file_paths file_reuse` 共 53 次启动、548 项检查通过；反向使用 `9b976aa` 的文件服务及文件能力源码，其他底层保留当前版本，共 53 次启动、26 个分支触发 58 项功能断言失败（预期退出 1），没有编译/链接失败。初始原复查脚本也分别复现 4 项和 3 项隔离断言失败。聚焦证据目录为 `build/services-finish-njv8osh8/`、`build/services-finish-vbebrptj/`；日志为 `build/services-r5-focused-positive.log`、`build/services-r5-focused-reverse.log`。

```bash
cmake --build build -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py file_events file_paths file_reuse
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline 9b976aa file_events file_paths file_reuse
```

## 没做到的

- 本轮尚未完成，测试结果待记录。
- 只有本机模拟器环境；不接触硬件，不启动带窗口的模拟器。
- 既有低级 AUD18-404 依用户范围保留。

## 和设计不一致的

待核查。

## 改过的已有测试

- run.py 新增三组回归及其矩阵分支，原组别、断言、等待阈值和预算不变。
- 新增真实 HAL 暂停包装及事件、路径、代次回归；不是模拟返回值或伪造业务结果。
- 目录补充回归：27 项检查通过，撤回文件实现后旧虚拟目录、共享目录继承复用身份，以及儿童目录获得成人根项，均触发断言失败。证据在 `build/services-r5-directory-positive.log` 和 `build/services-r5-directory-reverse.log`。

## 要用户定的

无；按保守、可逆方式处理已授权的修复，不进行远端写入或发布。

## 顺手发现

待记录。
