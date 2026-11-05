# 补充实验

对象：`a81c1b8ed805c044bcc24a120c14bb8e3a4d011e`。从仓库根目录执行。先按 [TASK.md](../TASK.md) 配置并构建 `build-audit/`；依赖全部使用本机已有文件，不联网。

`run_probe.py` 复用已有 CMake 目标的编译选项、模拟器入口和库，替换测试入口；`--extra` 只链接审计目录中的边界包装。没有修改产品代码、现有测试或 CMake。每次运行使用新的 `build-audit/audit-probes/<名字>/run-*` 目录，`last-run.txt` 指向最新一次。构建日志、模拟 Flash 和测试副本不属于交付文档。

这些是**复现缺陷的实验**：退出 0、`test.finished=1`、`test.fail=0` 表示观察到报告中的缺陷，并非产品修复后通过。实验中的 `CHECK` 刻意断言当前缺陷现象；修复后应改为相反的产品预期。模拟器本身返回 0 还不足以证明实验完成，脚本会另行检查这两个计数。概要保存在 [results.json](results.json)。

## 数据和文件

```bash
python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py \
  docs/audits/2026-11-04-codex-services/experiments/data_probe.c

python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py \
  docs/audits/2026-11-04-codex-services/experiments/file_probe.c --template sim-files
```

- AUD15-001：原样 include 原 datastore 测试和产品源。仅在一次加载已存在日志时注入 `ink_storage_load` 的正常失败返回 `-1`；保存、追加、回放均是真实产品。数据先持久写为 `old`，加载失败后写 `new` 并等待 durable，再丢弃缓存重载，得到 `old`。
- AUD15-004：真实 C 数据接口写入 3000 字节合法 JSON，真实 AI `data.get` 返回 DONE；cJSON 解析返回失败。
- AUD15-002、003：真实文件服务写入、列目录、删除和能力调用，无失败注入。3000 字节中文文本第一页错误返回 `end:true`；删除 `/family` 同时删除被目录列表隐藏的成员文件。

关键输出：

```text
AUDIT failed-load open_success=1 count=0
AUDIT new-write durable_ver=1
AUDIT reload value=old (expected new)
AUDIT data.get status=0 result_bytes=2109 valid_json=0
AUDIT file.read status=0 next_offset=2046 end=1 source_bytes=3000
AUDIT before family delete private_read=0 hidden_in_list=1
AUDIT after family delete job_state=2 private_read=-3 (expected preserved)
```

## 确定性并发

```bash
python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py \
  docs/audits/2026-11-04-codex-services/experiments/concurrency_probe.c \
  --template sim-notify \
  --extra docs/audits/2026-11-04-codex-services/experiments/notify_boundary.c \
  --extra docs/audits/2026-11-04-codex-services/experiments/interrupt_boundary.c
```

启动完整产品，使用 OSAL 信号量安排第二个真实任务。边界文件原样 include 产品源，不改分支、状态转移或返回值；只在解锁/音频调用之前让出执行权，并读取当前成员、打断编号。

1. AUD15-005：`ink_notify_post` 完成收件人检查并解锁后，第二任务切换成员并等通知服务切换完成，再让原任务继续。
2. AUD15-006：原任务准备调用 `ink_audio_ring(true)` 时，第二任务结束同一打断，再恢复原任务。`actual_ring` 记录真正转发给音频服务的最后参数；`interrupt.active` 是真实产品状态。
3. AUD15-007：占住 BG 执行器；主人接受 `/me/audit-confirm.txt` 的删除确认；切换为孩子后放行。文件删除和两位成员的文件状态均由真实服务执行/读取。

```text
AUDIT notify.post result=0 recipient=owner owner_has=0 kid_has=1
AUDIT interrupt id=1 active=0 actual_ring=1 published=alarm
AUDIT confirmed owner deletion owner_stat=0 kid_stat=-3
```

这些交错证明存在可执行的失败路径，不估算自然发生概率，也不替代多核竞争检测。

## 改时区后重启

每组实验必须使用一份全新的模拟 Flash；两阶段复用该文件：

```bash
mkdir -p build-audit/audit-probes
audit_time_dir=$(mktemp -d "$PWD/build-audit/audit-probes/time-run.XXXXXX")
INK_AUDIT_PHASE=write python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py \
  docs/audits/2026-11-04-codex-services/experiments/time_probe.c \
  --template sim-time --end 5s --flash-image "$audit_time_dir/user.img"
INK_AUDIT_PHASE=read python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py \
  docs/audits/2026-11-04-codex-services/experiments/time_probe.c \
  --template sim-time --end 5s --flash-image "$audit_time_dir/user.img"
```

AUD15-008：第一阶段在上海时区设每天 09:00，改为 UTC，等待数据和设置落盘。第二阶段是新模拟器进程，不重用内存，也不注入断电或 I/O 错误。

```text
AUDIT time before reboot zone=UTC next_local_hour=9 next_s=1793610000
AUDIT time after reboot zone=UTC rule_hour=9 next_local_hour=1 next_s=1793581200
```

## 应用状态和低电量

```bash
python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py \
  docs/audits/2026-11-04-codex-services/experiments/app_probe.c --template sim-apps

python3 docs/audits/2026-11-04-codex-services/experiments/run_probe.py \
  docs/audits/2026-11-04-codex-services/experiments/power_probe.c --template sim-notify --end 90s
```

- AUD15-010：测试应用依约提供 save/restore 回调；用真实 `ink_storage_hold_writes` 暂停写入并占满 8 个队列槽，无产品源包装。关闭应用成功，恢复得到旧状态。`hold_writes` 也是产品音频服务使用的接口。
- AUD15-011：模拟合法后台消费者持有 `INK_NEED_AWAKE`，允许锁屏，再从模拟硬件事件把电量设为 1%；75 秒后检查真实状态。记录结果后释放 hold。没有断言真实电池或设备损坏。

```text
AUDIT app close_result=0 queue_before=8 restored=old-state expected=new-state
AUDIT locked battery before=80 hardware=1 after75s=80 shutdowns=0 state=locked
```

## 反向对照与运行时清单

```bash
python3 docs/audits/2026-11-04-codex-services/experiments/reverse_controls.py
python3 docs/audits/2026-11-04-codex-services/experiments/dump_surface.py
```

`reverse_controls.py` 仅在 `build-audit/audit-probes/reverse-controls/` 生成产品源副本。每个变异有唯一字符串匹配断言，现有集成测试断言与预算不变：

| 变异 | 原测试 | 模拟器/包装脚本退出码 | test.fail |
| --- | --- | --- | --- |
| 快照帧也筛选递增版本 | integration-data | 1 / 1 | 1 |
| 去掉孩子写家庭目录检查 | integration-files | 1 / 1 | 20（含连锁失败） |
| 去掉确认的真人来源检查 | integration-confirm | 1 / 1 | 2 |
| 将 notify.post 设为 OWNER 可见 | integration-ai-surface | 1 / 1 | 1 |
| 删除 data.get 的 input Schema | integration-ai-surface | 0 / 0 | 0 |

全部变异运行都到达 `test.finished=1`。前四条与设计 §15 的主张一致；最后一条证伪 §1.3 的 Schema 保证（AUD15-009）。不是把编译失败当作反向对照成功。详细裁定在 [verification.json](../verification.json)。

`dump_surface.py` 对原 `ai_surface.c` 的唯一观测改动是输出每页实际工具响应；原测试仍完整运行并通过。解析结果在 [surface_inventory.json](surface_inventory.json)：3 页、65 个 call、18 个 event，无重复名字，无 `confirm.accept/reject`。对照数据、文件、时间、通知、确认、应用和系统七行能力表，仅 `system.reboot` 缺失；它列在设计 §13 的后续范围，本次不判为缺陷。页内专用的 reader 翻页能力不会在主页清单显示。
