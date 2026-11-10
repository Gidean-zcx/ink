# batch14 复查报告

主会话（Claude）复查，2026-11-10 晚上。用户这天起不再用 astra 审计，由 Claude 审。范围：batch14 合入的 `boot-db-stall`、`time-settings`、`reminder-edit`、`port-hal-2`、`files-ops`、`ai-speech-long`、`lock-camera`、`kid-apps`，以及没合并的 `sms-multipart` 第一版。每个任务合并前读过报告和产品代码；合并检查跑了全量测试、S31 编译、gcccheck、gccscan、rscan 和隐私扫描。

## 发现

1. **中：锁屏相机模式靠能力名特判，写进了通用的权限检查（lock-camera）。**
   - `system/policy/cap.c` 的 `gate()` 用前缀 `camera.`、`photo.`、`file.`、`app.open` 判断锁屏时能不能调；`on_its_page()` 也特判 `ink_camera_restricted()`；
   - `system/services/input/input_service.c` 的 `power_filter()` 和 `system/policy/system_actions.c` 的 `go_home()` 直接问相机的状态。
   这样策略层知道了相机这个应用，以后再加别的锁屏功能就得继续加特判。和 AI 原生的拆分要求不符：能力和页面应该自己声明规矩，策略层只执行。修法：把“锁屏时可用的受限页面”做成页面（activity）的声明，带上它允许的能力，策略层和输入服务只查这个声明，不认名字。
2. **中：孩子的应用名单靠能力名前缀判断归属，名字对不上的能力漏网（kid-apps）。** `system/policy/members.c` 的 `action_app()` 用“能力名以应用 id 开头”加四条特例（`shell.ai_`、`calc.`、`calendar.`、`alarm.`/`reminder.`）判断能力属于哪个应用，判断不出来就放行。漏网的例子：
   - 相册的 `photo.open`：家长关掉相册以后，孩子仍能经 AI 调；
   - 文件服务的 `file.*`：文件应用叫 `files`，关掉文件以后，孩子仍能经 AI 列文件、读文件。
   修法：能力自己声明属于哪个应用（能力表加字段，或者由应用注册时登记它用到的能力域），名单检查只看声明；没有声明的应用能力，要有测试列出来。
3. **中：批量文件操作的回执把每个文件都列出来（files-ops）。** `system/services/file/file_caps.c` 的 `cap_batch()`：
   - 回执里每个文件写一项 `{path, error}`，最多 1024 项；
   - 缓冲按“项数 × (128×6+96)”申请，最坏约 885 KB 的 PSRAM，实际 JSON 也能到几十 KB，整段交给 AI 和中枢。
   修法：回执只给总数、成功数、失败数，加上失败的文件名（最多 20 个）；缓冲照这个算。
4. **低：时间设置页对 `time.` 下所有变化都整页重画（time-settings）。** `system/ui/shell/time_settings.c` 的 `changed()`：除了分钟只更新文字，`time.synced`、`time.zone`，以及闹钟改动带来的变化都会 `ink_ui_redraw()`，不符合规则第 8 节（整页重画会丢掉排队的点按）。修法：只对影响这一页的设置（`time.auto`、`time.zone_auto`、`time.zone`、`member.idle_return_s`）重画。
5. **低：`ink_hub_server_time` 读 64 位字段不加锁（time-settings）。** 打开“自动设置日期和时间”时，`apply_auto` 在后台执行器上调 `on_online`，读 `H.server_ms`、`H.hello_sent_us`。中枢任务同时在重连时会写这两个字段，RV32 上 64 位读写不是一次完成的，可能读到一半旧一半新的值；后果是一次错误的对时，下一次 hello 会纠正。修法：在 `id_lock` 里读写。
6. **低：两个日期时间选择**（时间设置的 `SetDateTime`、提醒编辑页）：已交给 `small-gaps-2` 统一。
7. **低：中枢的 hello 还不发 `time_zone`**，“自动设置时区”会一直等：已记进 backlog。
8. **低：port-hal-2 和原厂做法不一致**（浅睡补丁、USB 恢复、调频方式、BOOT 键唤醒、扩展芯片睡眠配置）：已交给 `port-sleep-vendor`。
9. **已处理：`sms-multipart` 第一版没合并。** 它把参数表和确认卡说明放大到 1152 字节，撞了 `exec.bg` 的栈预算（`memo-record`），也破坏了确认卡的规矩（`integration-confirm` 失败 10 条）。返工任务是 `sms-multipart-2`。
10. **已处理：Linux CI 上电源任务的栈估算超了**（3192/3072，accel-use 以后）。照原厂浅睡任务的 4096 改了（`1483b977`）。

## 看过没问题的

- ai-speech-long：每来一帧续期、10 分钟总上限；先在本地收尾再发结果，符合规则第 8 节；协议文档同步了。
- reminder-edit：保存走原来的提醒提供者和成员检查，没新造存储。
- time-settings：手动设时间时标记为没对过时；自动时区只认表里的名字，不拿中枢主机的时区冒充。
- files-ops：草稿断电不丢；批量删除走确认；孩子碰不到大人的文件。

SEVERE=0 HIGH=0 MEDIUM=3 LOW=5（另有 2 项已处理）
