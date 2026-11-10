# notify-manage 任务报告

基线：`eec5d569be028b539e8778690c239a6947767864`。分支：`claude/codex-notify-manage`。状态：进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 逐条删除及重启 | 【实测】专项通过 | 已读／未读逐条删，其他通知与未读数正确，第二次启动不回来 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/notify_manage.py --build build` |
| 1b 短信渠道关闭及恢复 | 【实测】专项通过 | 真实模拟短信事件；关闭后仅短信收件，打开后恢复通知；偏好重启保留 | 同上 |
| 1c AI 删除及渠道设置 | 【实测】专项通过 | AI 和触摸均使用既有 `notify.dismiss`、`notify.set_channel`，页面随结果更新 | 同上 |
| 1d 孩子限制 | 【实测】专项通过 | 孩子不可打开渠道设置或调用渠道修改；可删除自己的通知，看不到成人收件箱 | 同上 |
| 2 回归、全量、内存、S31、gcccheck | 待验证 | — | — |
| 3 中英文页面及快照 | 【实测】通过 | 六张截图及 JSON，含最大字号渠道页；生产 AI 缓冲验证通过 | 同上 |

## 没做到的

全量、内存检查、S31、反向对照尚待完成。未上机。

## 和设计不一致的

暂无。此次按任务书 D-117 使用现有页面和规范设计；通知删除语义、短信收件和来电／闹钟流程均沿用服务。

## 改过的已有测试

暂无。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

- `notify.changed` 变化事件：任务书要求删通知、改渠道后页面和 AI 快照与服务一致。集中在通知服务发出，UI 沿用现有总线订阅；覆盖已读通知被删、未读数不变的情况。没有新任务、状态机、文件格式、后台重试或配置项。

## 要改的文档

- 合并时生成 `docs/system/capabilities.md`：新增 `shell.notify_settings` 导航和 `notify.changed` 事件，`notify.set_channel.child_ok` 改为 false。任务书禁止本分支改生成能力表，因此全量中该文档检查预期会报告过期；将另生成临时目录版本对照运行时。
- 设置缺口清单中的两项可在主会话更新为已接通；共享任务清单和 UI 依赖状态由主会话统一维护。

## 过程与验证记录

- 【实测】工作区初始干净，HEAD 等于指定基线，已在独立任务分支。已读取任务书及总规则；构建和测试最多 `-j2`，使用 `taskpolicy -c utility`。
- 产品代码预算：最多 250 行，待统计。
- 【实测】第一次构建退出 0；专项最终输出 `main: exit=0, pass=203, fail=0, finished=1`、`restart: exit=0, pass=11, fail=0, finished=1`，日志 `work/notify-targeted.log`。
- 【实测】`ui-scale: 50 files, 0 violations`；`gcccheck: 4 compiled, 0 with findings`，无跳过，日志 `work/notify-ui-scale.log`、`work/notify-gcccheck.log`。
- 【实现】删除放在每行右侧独立描边按钮，与打开区域留出间距，整颗按钮满足最小触控尺寸。按钮宽度随译文测量，避免英文挤压；不增加回收站或确认步骤，沿用现有单条删除能力的语义。
- 【实现】渠道入口放设置主目录的声音／语音一组，方便找到且不重复勿扰。按实际登记表枚举系统、短信、电话、闹钟、提醒、下载、AI 七个渠道，不编造独立“系统更新”渠道；更新归现有系统渠道。
- 【实现】成人设置均不向孩子开放，渠道修改能力也收紧为相同规则；孩子自己的通知仍可读和删，数据和偏好按成员隔离。

截图前缀：`build/notify-manage-evidence/sim-notify-manage/main/`。

- 中文：`snap-notifications-zh.png`、`snap-channels-zh.png`、`snap-channels-zh-large.png`。
- 英文：`snap-notifications-en.png`、`snap-channels-en.png`、`snap-channels-en-large.png`。
- 同目录同名无 `snap-` 的 `.json` 为页面 AI 快照。已人工查看英文列表和英文最大字号渠道页。
