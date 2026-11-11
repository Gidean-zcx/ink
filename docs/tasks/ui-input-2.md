# ui-input-2：键盘和输入框照现代做法重做

总规则先读 docs/tasks/codex-rules.md，再读 `docs/tasks/ui-feedback-4.md`（用户原话和共同设计基准）。

## 要做的

1. **键盘**（`system/ui/shell/keyboard.c`，`ImePinyin` 等状态）：照主流手机输入法（Gboard、iOS、搜狗/微信键盘的全键盘布局）重排。字母键每行等宽、行间对齐；功能键（大写、删除、中英切换、符号、回车）用统一的倍数宽度；空格最宽；**语音键缩小到和功能键一样大**，放在业界常见位置（空格左边或候选条右侧，选一种并说明），不再是一个大按钮；候选条、符号页、密码键盘、数字键盘同一套规格。用户在功能清单里还勾了“九宫格键盘”：本任务只把布局做成可以加九宫格的结构，九宫格本身留到后面。
2. **输入框组件**（`system/ui/textfield.c`、`system/include/ink/sys/textfield.h`）：定下几种规格（单行、多行正文、搜索、密码），高度、内边距、字号、边框、占位文字颜色、聚焦状态、清除按钮、错误提示全部出自主题令牌；页面只能选规格，不能另设宽高（宽度跟随容器）。**接口保持兼容**（`comm-ux-2` 同时在用它）。
3. 把下面这些页面里的输入框全部换成统一规格，去掉各自写的尺寸：`system/ui/search.c`、`app_packages.c`、`system/ui/shell/mobile_network.c`、`ai_chat.c`、`settings_family.c`、`boot_flow.c`、`navigation.c`（Wi-Fi 密码等）、`system/apps/calendar/calendar.c`、`memo/memo.c`、`remote/remote.c`、`weather/weather.c`、`clock/clock.c`、`files/files.c`。**不改** `system/apps/sms/`、`contacts/`、`phone/`（`comm-ux-2` 在改）。
4. 现有的界面测试和截图测试要跟着更新；新增一个检查：输入框和键盘按键的尺寸只来自令牌、同一行键宽一致（可以加在现有的 ui-scale 或界面测试里）。

## 范围和预算

- 只改上面列的文件、`system/ui/ink/ui_theme.h`（加令牌）、相关测试和截图基线。
- 产品代码净增不超过 400 行；不新增机制。

## 验收

1. 键盘各状态和所有列出页面的输入框符合统一规格（附前后截图对比，放在 `docs/tasks/ui-input-2/`）。
2. 主工程普通全量 ctest 过（不用跑 ASan），`ui-scale` 过；S31 编译不用跑（主会话合并时跑）。
3. 报告写清每个页面改了什么、语音键放在哪、为什么。
