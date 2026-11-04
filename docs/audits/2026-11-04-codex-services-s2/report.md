# 系统服务第二阶段独立审计

审计日期：2026-11-04。范围：`a81c1b8..HEAD`，依本目录 `TASK.md` 执行。审计进行中；以下未完成项目不表示通过。

## 1. 一页结论

待完成代码核查、运行验证与反向对照后汇总。

## 2. 新问题

### AUD16-001【高】成员表读取失败时退回无 PIN 的默认成人，策略关口随之放行

- 证据：【复现】member_probe.c + member_load_boundary.c：write 后同镜像 control / AUDIT_MEMBER_LOAD_FAIL=1；control has_pin=1 space_open=0 file.list status=4；故障 has_pin=0 space_open=1 file.list status=0。
- 位置：【代码核查】`system/policy/members.c:148`、`system/policy/members.c:150`、`system/policy/members.c:171`、`system/policy/members.c:109`
- 影响：已有 PIN 的设备在成员表读取失败后仍启动默认 owner（has_pin=false）；成人空间开放，AI 可以读取 /me。实验只使一次日志加载返回 INK_STORAGE_LOAD_FAILED，同一持久镜像的对照启动保持锁定。
- 建议：区分首次开机无成员表与已有成员表加载失败；失败时关闭成人空间并进入恢复状态，不能使用无 PIN 默认身份继续提供私人数据。

### AUD16-002【中】暂停后停止或切换成员，会把听书位置回写成该流起点

- 证据：【复现】media_probe.c（真实能力调用，无故障注入）：paused_ms=4080 reopened_ms=0。
- 位置：【代码核查】`system/services/audio/media_service.c:80`、`system/services/audio/media_service.c:262`、`system/services/audio/media_service.c:347`、`system/services/audio/media_service.c:411`
- 影响：暂停已经保存正确位置，但 close_stream 将 M.stream 清零；随后 stop 使用 heard_ms 得到 M.base_ms，覆盖正确书签。普通暂停→停止→重新打开从头开始。
- 建议：无活动流时使用 M.st.position_ms；停止、换文件、切成员和暂停后 seek 应共享一致的位置快照。

### AUD16-003【高】录音没有绑定成员生命周期，换人后仍将新成员声音写进原成员文件

- 证据：【复现】media_probe.c：current=kid recording=1；record_done current=kid path=/me/rec/switch.opus ms=4020 child_stat=-3。
- 位置：【代码核查】`system/services/audio/media_service.c:411`、`system/services/audio/media_service.c:442`、`system/services/audio/media_service.c:473`、`system/services/audio/media_service.c:730`
- 影响：member.current 的处理只停止播放，没有停止录音。owner 发起录音后切 kid，2.2 秒后仍在采集；完成事件发送给当前成员，路径仍是 /me，而实际文件属于 owner。新成员可停止旧录音但不能访问该回执指向的文件。
- 建议：录音保存不可变成员和会话代次；切换成员时先结束采集、排空并封闭 Ogg，再完成成员交接；结果和事件按原成员路由。

### AUD16-004【高】重启后写资源只验头部，不校验将写入的段内容

- 证据：【复现】update_probe.c + update_boundary.c，AUDIT_CORRUPT_RESOURCE=1：validated_before=0 section_check_now=7 writes=1 first_byte=165 state_cleared=1 path_cleared=1。
- 位置：【代码核查】`system/services/update/update_service.c:805`、`system/services/update/update_service.c:810`、`system/services/update/update_service.c:821`、`system/services/update/update_service.c:839`
- 影响：安装前验过的 TF 包不等于重启后仍是同一内容。保持签名头不变，只改资源段，check_sections 已判损坏，但 rest_job 仍把改坏的字节写入无 A/B 的资源区并清恢复标记。该实验没有绕过签名或伪造有效签名。
- 建议：重启后重新验证产品、目标版本、完整段表和全部待写资源哈希；持卡覆盖校验到写入，写入时继续校验并绑定不可变包身份；资源未可靠写完不得清恢复标记。

### AUD16-005【高】资源 HAL 写失败被忽略，错误标为完成且失去恢复入口

- 证据：【复现】update_probe.c + update_boundary.c：原包正常，资源 HAL 返回 -1；writes=1 state_cleared=1 path_cleared=1。
- 位置：【代码核查】`system/services/update/update_service.c:287`、`system/services/update/update_service.c:820`、`system/services/update/update_service.c:827`、`system/services/update/update_service.c:842`
- 影响：hal_ota_write_resource 返回 -1 不被传回；仍增加 done/res_done_section，清 state/path。OTA 包还会被删除。资源分区没有 A/B，失败可能留下损坏字库或资源，却不能在下一启动重试；实机具体表现未测试。
- 建议：把 HAL 和 ink_storage_run 的结果纳入事务；确认整段写入及读回成功后推进标记，失败保留包及待恢复状态，不提前发成功。

### AUD16-006【高】U 盘模式下恢复出厂跳过 TF 私人数据，仍清 Flash 并成功重启

- 证据：【复现】reset_probe.c：usb_active=1 receipt=0；同一 SD 目录检查 private_file_survives=true marker_matches=true，system.factory_resets=1 power.reboot=1。
- 位置：【代码核查】`system/policy/system_actions.c:67`、`system/policy/system_actions.c:70`、`system/services/file/file_service.c:454`
- 影响：卡清理接口不报告完成或失败，恢复出厂无条件继续，USB 借出时 ink_fs_wipe_personal 不清理卡；已回执成功且重启后 /content/.m/owner/private.txt 内容完整保留。原成员表被清除后也没有持久恢复标记，下一位使用者可遇到原私人文件。
- 建议：先检查并独占所有待清区域；卡借出、缺卡或清理失败时拒绝完成或明确保留待清事务。先冻结数据生产者，再排空/取消写入，使用重启可续的 reset 标记，全部完成后才宣告成功。

### AUD16-007【高】通知发事件前的成员复查仍有 TOCTOU，标题会送给新成员 AI

- 证据：【复现】python3 experiments/copy_observers.py；run_probe.py build-audit/observer-copies/late_notify_probe.c --template sim-races --script tests/integration/races.script --budget tests/integration/races.budget.json。产品源码未改变，test.finished=1、test.fail=1、进程退出 1。
- 位置：【代码核查】`system/services/notify/notify_service.c:293`、`system/services/notify/notify_service.c:295`、`system/services/notify/notify_service.c:297`、`tests/integration/races.c:30`
- 影响：原收件箱已绑定 who，但 mine 在最后一次解锁后仍可失效。将现有 races 的换人安排到第五次解锁（最后一次成员复查后），现有“and not told to their AI”断言失败；写入的数据库正确。AUD15-005 为部分修复。
- 建议：给事件绑定不可变成员／会话代次，消费和转发时验证归属；不能只在发事件之前读一次全局成员。把最后检查到转发的时序纳入回归测试。


### AUD16-008【高】成人空间关闭不终止已开始的语音会话，麦克风仍上传

- 证据：【复现】run_probe.py experiments/listen_close_probe.c --template sim-listening --profile simulator/profiles/metalio-plus-handsfree.json --end 20s；AUDIT listen space_open=0 upload_bytes_while_closed=6812；test.finished=1、test.fail=0。
- 位置：【代码核查】`system/services/ai/ai_service.c:738`、`system/services/ai/ai_service.c:791`、`system/services/ai/ai_service.c:800`、`system/services/ai/ai_service.c:810`
- 影响：唤醒词只在开始时检查空间是否开放；AI 只监听 member.current 终止会话，member.authed 关闭只改变门状态。正常唤醒后锁屏，确认 space_open=0 并等 1 s 排空传输后，接下来 2.2 s 中枢仍收到 6812 字节麦克风数据。未声称 PIN 或锁定后的新页面被上传。
- 建议：将语音、录音和会话绑定成员授权代次；member.authed 关闭时同步停止采集、丢弃未发送帧并关闭实时会话，在发帧和发起请求前校验会话归属。

## 3. 第十四轮修复的核查

待逐条核查上一轮 11 项。

## 4. 架构与 AI 原生审视

待核查模块边界、策略入口、能力开放及运行时工具表。

## 5. 测试、反向对照与预算

【复现】普通 Werror 构建与 ASan/UBSan 构建成功；CTest 均 82/82，通过用时分别 42.05 s、87.40 s。补充实验已发现既有测试未覆盖的缺陷，测试全过不等于这些路径正确。具体命令与反向对照继续补录。

## 6. 对象与覆盖边界

只读产品代码、测试及既有文档；审计产出限本目录，构建和变异副本限 `build-audit/`（内存检查另用任务许可的 `build-audit-asan/`）。不访问设备、实机数据、外部模型或网络；不提交、不推送、不建分支。初始工作区存在未跟踪的 `codex-run.log`，不将其作为审计证据、不修改。
