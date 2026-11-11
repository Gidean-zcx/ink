# polish-1：键盘和短信的几处小打磨

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md` 第 11 条。

主会话复核合并时看到的几处小问题（用户要求“每个功能和页面都参考现代UI的设计”）：

## 要做的

1. **密码键盘的空白**（`system/ui/shell/keyboard.c`）：输入密码时候选条和组合串那一带不用，现在留着一大块空白（见 `docs/tasks/textfield-1/screenshots/after-100/WifiPassword.png` 键盘上方约 170 像素空白）。照 iOS/Android：密码键盘不显示候选条，键盘整体矮一截，页面相应多出空间；切回普通输入时恢复。`ink_shell_keyboard_area()` 报的高度跟着变，依赖它排版的页面（短输入框上移的位置）照样正确。
2. **短信回车键是“发送”**（`system/apps/sms/sms.c`）：会话页底部回复框和新建短信的正文框，键盘回车键显示“发送”并直接发送（用现有的回车类型，例如 `INK_ENTER_SEND`，查 `text` 服务现有的取值），发送规则、确认、状态显示不变。
3. **会话页的时间标头**（`system/apps/sms/sms.c`）：今天的消息只显示“08:00”，昨天显示“昨天 08:00”，更早显示“11月2日 08:00”，跨年才带年份；相邻两条间隔不到 5 分钟只在第一条上方显示一次时间（不管方向）。

## 范围和预算

- 只改 `system/ui/shell/keyboard.c`、`system/apps/sms/sms.c`、对应测试（`tests/ime_keyboard1.*`、`tests/sms/sms.c`、`tests/comm_ux2/` 里相关断言）和截图基线、`assets/i18n/en.tsv`（只加本任务新文案的英文）、`docs/tasks/polish-1/`。不碰别的任务在改的文件。
- 产品代码合计不超过 150 行；不新增机制。

## 验收

1. 测试：密码键盘高度变小且页面排版正确、切回普通输入恢复；短信回车键文字是“发送”且按下就发送；时间标头按今天、昨天、更早、跨年和 5 分钟合并的规则显示；反向对照。
2. 主工程普通全量 ctest 过（含 `ui-scale`、`ime-*`、`sms*`、`comm-ux2`）。
3. 前后截图（密码键盘、短信会话页，常规和 120%，`docs/tasks/polish-1/`，单张不超过 300 KB）。
