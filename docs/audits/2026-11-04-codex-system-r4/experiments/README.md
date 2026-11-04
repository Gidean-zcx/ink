# 第十三轮复核实验

对象 `99495a8da3903077117f3a181994e3824177d825`；日期 2026-11-04。全部在本机软件模拟器执行，未联网或访问设备。产品与既有测试源码未改动。

## 结果

| 证据 | 内容 |
| --- | --- |
| [baseline_results.json](baseline_results.json) | 普通 57/57、ASan/UBSan 56/56；完整配置/构建/测试命令与四个指定测试的检查数。 |
| [mutation_results.json](mutation_results.json) | 六个负对照，均编译成功、运行完成；每个 exit 1、test.fail=1，且失败断言对应被撤回的修复。 |
| [probe_results.json](probe_results.json) | 三个边界实验各在普通与 sanitizer 构建执行，六次 exit 0，无警告。 |
| [remote-offline-visible.png](remote-offline-visible.png) | 普通 CTest 的 remote_reconnect 场景 23.1276 s 实际绘制帧；已人工确认顶部重连文字不被底部 AI 面板遮住。原路径与 SHA-256 在 baseline_results.json。 |

完整日志、模拟文件系统、指标、二进制在各构建目录的 `audit-r4/` 中；构建日志没有复制到本目录。JSON 是结果摘要，不是所有原始日志。

## 运行方式

先按 [TASK.md](../TASK.md) 构建普通与 sanitizer 目录；本轮额外设置 `FETCHCONTENT_FULLY_DISCONNECTED=ON`，禁止 FetchContent 下载。

```bash
AUDIT_PY=/Users/bytedance/Documents/Workspace/ink/build-deps/pyenv/bin/python
AUDIT_RUN=docs/audits/2026-11-04-codex-system-r4/experiments/run_checks.py
$AUDIT_PY "$AUDIT_RUN" mutations
for audit_build in build-audit build-audit-asan; do
  for audit_probe in ai_close remote_locked comm_deadline; do
    $AUDIT_PY "$AUDIT_RUN" probe --build "$audit_build" --name "$audit_probe"
  done
done
```

`run_checks.py` 读取并复用上一轮 [build_probe.py](../../2026-11-04-codex-system-r3/experiments/build_probe.py)，采用当前构建的编译选项、入口对象和静态库。运行脚本关闭 Python bytecode 写入，避免在上一轮目录生成缓存。每跑完一项立即追加 JSON；重复运行保留历史记录。编译或运行结果不符合预期时驱动脚本返回非零；每个模拟器进程的实际退出码另存为 `exit`。

## 负对照的准确范围

所有变体仅通过编译器 stdin 提供。产品源码及测试文件保持原样；涉及 `#include` 产品源码的测试在内存中展开该文件再替换指定实现。变体输入的 SHA-256 和真实运行命令已记录。展开后的 `#line` 会改变测试后半段的编译诊断行号，判断失败请以断言文本及下列当前源码位置为准。

| 变体 | 仅撤回的行为 | 失败断言（当前源码） |
| --- | --- | --- |
| mut_cell_notify | 未选中蜂窝网络时的可用性通知 | net_auto.c:142，452.010 s 不能自动恢复 LTE |
| mut_ai_capture | turn_over 中的 end_capture(true) | ai_race.c:93，麦克风仍在采集 |
| mut_ai_release | 松键路径对 AI.turn_over 的保护 | ai_race.c:98，错误被覆盖成“没听到，请再按 AI 键说一遍” |
| mut_remote_restore | 恢复反显时改为复制 back | lifecycle.c:629，提前显示未完成 B 的像素 |
| mut_remote_note | 提示改回底部 | lifecycle.c:665，提示底边 1185，侵入 AI 面板覆盖范围 |
| mut_hal_switch | hal_net_use 提前覆盖 N.type | hal_switch.c:42，旧连接没有收到 CLOSED |

最后一项是对新增 HAL 测试的有效性检查；不把该旧实现误列为本轮仍存在的问题。六个负对照仅在普通构建运行，sanitizer 用于未改动产品的正向验证。

## 补充实验的边界

- [ai_close.c](ai_close.c)：正常产品启动，原样包含 AI 服务。测试模拟 RT 层先释放会话再回调拥有者，检查长按时关闭、上传后关闭、旧 sid 回调及旧请求超时；每种构建 34 项检查。上传后提前关闭会丢弃后续语音帧，此实验只验证文字与状态的收尾，不宣称语音完整。
- [remote_locked.c](remote_locked.c)：正常启动，调用真实 `remote.open`，检查锁屏中不开会话、解锁后开一个会话并显示画面，以及锁屏中先关页面再解锁；每种构建 21 项检查。额外电源需求只用于稳定观察锁屏，不验证睡眠或电流。
- [comm_deadline.c](comm_deadline.c)：真实能力总线和模拟模组；信号量人为占用背景执行器 20 s，验证接听/挂断可以等待；另验证排队超过 25 s 只返回一次失败且不再执行；每种构建 23 项检查。未改变模组时序，不把排队延迟解释成 SMS 实网耗时。

补充实验使用 `hal_switch.budget.json` 中通用的完成、失败、panic、warning 门槛，不使用它来声称这些实验就是 HAL 切网测试。脚本里的号码和网络名称均为模拟夹具。
