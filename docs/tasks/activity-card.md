# activity-card：正在进行的事（框架 F6），以及放声音、通话时的主屏、控制中心和锁屏

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/activity-card/REPORT.md。

## 要做的

预算 350 行产品代码：

- 一份“正在进行的事”数据（F6，docs/system/app-framework-design.md 第 462 行）：正在放的声音（`audio.now_playing`、`media.state`）、正在打的电话（`call.connected`、`call.ended` 等）、后台作业（`job.list`、`job.finished`）。只显示服务真有的阶段，不编进度。主屏、控制中心、锁屏共用这一份，不各写一套。
- `ActivityCard`：“正在进行”卡片：标题、一行说明、一个动作（接着放／暂停／回到通话）。
- `MainPlaying`、`ControlPlaying`：放声音时主屏和控制中心带这张卡。
- `MainInCall`：通话中主屏带“回到通话”。
- `LockPlaying`：锁定时声音继续，小标记写“正在听”；暂停要先唤醒（规格第 161 行）。
- `Tasks`：“正在做的事”和“做好了”两组，来自 `job.list`；能取消的给取消（`job.cancel`）。

## 从哪里看起

- 画板 `$INK_DESIGN/png/` 里的 ActivityCard、MainPlaying、ControlPlaying、MainInCall、LockPlaying、Tasks；规格 docs/product/system-ui-spec.md 第 139–161 行（锁屏）和第 1 节的数。
- 主屏和控制中心：system/ui/shell/navigation.c（`ink_shell_home_*`、`shell.control`）；锁屏小标记：system/ui/lock_mark.c；作业：system/services/res/task.c；声音：system/services/audio、media；电话：system/services/comm、system/apps/phone。

## 边界

- 卡片上的动作用已有能力（暂停、接着放、回到通话），不新增播放或通话逻辑。
- 锁屏样式选择（这一页、时钟和待办、壁纸）在并行的 settings-b 里改，**你只在“这一页”的小标记上加“正在听”**，不要改样式选择。
- 同时在做的有 settings-b（修复中）、kid-2、transfer-pages、remote-app、fw-f8、storage-usage；fw-f8 也改 navigation.c 的今日屏和应用页，你只动主屏和控制中心那部分；共用文件只在末尾加函数，现有函数里只加最少的接线调用。

## 验收

1. 开始放声音后，主屏、控制中心出现同一张卡，标题和动作一致；暂停后卡片跟着变；停了卡片消失（测试）。
2. 通话中主屏显示“回到通话”，点了回到通话页；挂断后消失（测试）。
3. 锁定时放声音，小标记写“正在听”，声音不停；锁着时按卡片上的暂停不起作用，唤醒后才行（测试）。
4. `Tasks` 列出真实作业和阶段，取消后作业真的停了（测试，反例：只改显示不调 `job.cancel`，测试失败）；各页有 AI 快照，按钮和 AI 走同一个能力。
5. 全量测试和内存检查通过；产品代码不超过 350 行；截图留在构建目录。
