# factory-pages：恢复出厂设置的两页

总规则先读 docs/tasks/codex-rules.md，第 7 节优先；docs/tasks/backlog.md 开头的 G1–G5 也要满足（G4：页面任务先看画板）。报告写到 docs/tasks/factory-pages/REPORT.md。

## 要做的

预算 200 行产品代码。后端已经有 `system.factory_reset`（system/policy/system_actions.c，家长密码确认）；设置里“关于”页的“清空这台设备”现在直接调它。

- `FactoryReset`：按画板写清楚“会清除”（家人、设置和 Wi-Fi；这里的备忘、录音和照片）和“不会动”（家庭服务器上的备份；存储卡上的书和音频），以及“也可以一起清空”存储卡上的书和音频（一个开关）。显示上次备份的时间，以及之后有没有没备份的东西（来自备份服务）。按钮“先备份”（走 `backup.now`）和“继续”（走 `system.factory_reset`，照它现有的家长密码确认）。
- “一起清空存储卡”要后端支持：给 `system.factory_reset` 加一个可选参数，选了才清卡上的书和音频；不选时卡上内容一点不动。
- `FactoryDone`：“已清空”，按钮“知道了”后进开机流程（boot-flow 已有）。
- “关于”页的“清空这台设备”改成打开 `FactoryReset`，不再直接调后端。

## 从哪里看起

- 画板 `$INK_DESIGN/png/` 里的 FactoryReset、FactoryDone；后端 system/policy/system_actions.c 的 `factory_reset`；备份状态 `backup.status`；开机流程 system/ui/shell/boot_flow.c；关于页 system/ui/shell/settings_pages.c 的 `ABOUT_ROWS`。

## 边界

- 只在模拟器里测；不碰任何真机数据。
- 孩子进不了这页，也调不了后端（现有 `child_ok = false` 保留）。
- 同时在做的有 privacy-records（也改 settings_pages.c，但只动 PRIVACY_ROWS）、kid-3、file-index-3、fw-f13、port-skeleton；settings_pages.c 里你只改 ABOUT_ROWS 那一行。

## 验收

1. 不选“一起清空”时，清空后家人、设置、备忘、照片没了，卡上的书和音频还在（逐个文件核对）；选了时卡上的书和音频也没了（测试）。
2. 家长密码没输对不清空；孩子调用被拒（测试，反例）。
3. “先备份”真的发起备份，页面上的上次备份时间跟着变；有没备份的东西时页面如实说（测试）。
4. 清空后进 FactoryDone，“知道了”进开机流程（测试）；两页有截图和 AI 快照。
5. 全量测试和内存检查通过；产品代码不超过 200 行。
