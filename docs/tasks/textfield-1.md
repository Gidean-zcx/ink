# textfield-1：输入框统一成几种规格，所有页面换上

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md` 第 11 条（界面只在明显冲突处改，用我们的主题令牌和统一组件）。本任务是没派出去的 `ui-input-2.md` 第 2～4 条（输入框部分）；键盘在 `ime-reuse-1` 里做，不在本任务。

用户 2026-11-11 试用：“还有各种输入框 短信各种界面完全就不符合现代UI/UX 设计 大大小小的输入框 我要求每个功能和页面都参考现代UI的设计审计”。

输入：`docs/tasks/ui-audit-2/FORMS.md`（22 个实际输入构造点、每处的问题）、`COMPONENTS.md`（C03～C06、C09、C12 输入框和表单规格）、`PAGES*.md` 里各页的输入框条目。先找成熟做法：LVGL 9.5 自带 `lv_textarea`（单行模式、占位文字、密码模式、光标）就是现成的轮子，我们的 `textfield.c` 如果在它上面重复造了东西（光标、占位、密码掩码），换成它的；只保留我们的能力、文本服务和快照接入。

## 要做的

1. **输入框组件**（`system/ui/textfield.c`、`system/include/ink/sys/textfield.h`）：定四种规格——单行、多行正文、搜索、密码；高度、内边距、字号、边框、圆角、占位文字颜色、聚焦状态、清除按钮、错误提示和字段标签（在框上方，不能只有占位文字当标签）全部出自主题令牌；页面只能选规格，不能另设宽高，宽度跟随容器。**接口保持兼容**（`comm-ux-2` 和 `reader-epub-1` 同时在用它，不能让它们的调用编不过）；旧接口里的自定义尺寸参数可以保留签名但忽略，报告写清。令牌加在 `system/ui/ink/ui_theme.h` 现有 `enum` 里控件尺寸那几行附近（`ime-reuse-1` 在文件末尾加键盘令牌，不要也加在末尾）。
2. **页面换上统一规格**，去掉各自写的尺寸：`system/ui/search.c`、`system/ui/app_packages.c`、`system/ui/shell/mobile_network.c`、`system/ui/shell/ai_chat.c`、`system/ui/shell/settings_family.c`、`system/ui/shell/boot_flow.c`、`system/ui/shell/navigation.c`（Wi-Fi 密码等）、`system/apps/calendar/calendar.c`、`system/apps/memo/memo.c`、`system/apps/remote/remote.c`、`system/apps/weather/weather.c`、`system/apps/clock/clock.c`、`system/apps/files/files.c`。只改输入框和它的标签、间距；这些页面别的问题（ui-audit-2 的 UX 任务）不在本任务。
3. 加一个检查（放进现有界面测试或 ui-scale 测试）：所有输入框的尺寸只来自令牌，同一页同一规格的框高度一致；大字号（120%）时标签和框一起放大、不截断。

## 范围和预算

- 只改上面列的文件、`system/ui/ink/ui_theme.h`（只加输入框令牌）、相关测试和截图基线、`docs/tasks/textfield-1/`。**不改**：`system/apps/sms/`、`contacts/`、`phone/`（`comm-ux-2`）、`apps/reader/`（`reader-epub-1`）、`system/ui/shell/keyboard.c` 和 `system/services/text/`（`ime-reuse-1`）。
- 产品代码净增不超过 300 行；不新增机制。

## 验收

1. 列出的每个页面的输入框符合统一规格，附常规和大字号前后截图（`docs/tasks/textfield-1/`，单张不超过 300 KB）。
2. 主工程普通全量 ctest 过（含 `ui-scale`）；S31 编译主会话合并时跑。
3. 报告写清每个页面改了什么、用没用 `lv_textarea` 现成能力、旧接口参数怎么处理。
