# 任务：界面框架和第一批系统壳

报告写到 `docs/tasks/ui-shell/REPORT.md`。

依据（合同）：`docs/product/system-ui-spec.md` 全部，重点：第 0 节词表、第 1 节规格表、第 2 节主屏、第 3 节交互、第 5 节实现提醒和刷新表、第 8 节打断表、第 10 节每屏 AI 表；`docs/system/ui-dependencies.md` §4.2、§4.3、第 7 节。再读 `system/ui/`、`system/ui/shell/`、`apps/home/`、`tools/fontgen/`、`docs/system/system-services-design.md` §9.4.1、§13。

设计稿（只读，不提交）：`$INK_DESIGN/boards/*.dc.html` 是画板源，`$INK_DESIGN/png/<画板名>.png` 是 684×1216 的渲染图，`$INK_DESIGN/states/` 是状态图。规范和设计稿冲突时以规范为准，记进报告。**开工前先看图**，至少看 `Foundation`、`Components`、`Main`、`Today`、`Control`、`Settings`。

可以改的：`system/ui/`、`apps/home/`、`tools/`（字体、图标、对照工具）、`assets/`、`third_party/fonts/`、`tests/`、`CMakeLists.txt`。服务层（`system/services/`、`system/policy/`）只允许小的增量接口，逐个列进报告。服务缺数据时，页面显示规范写的空状态，不许把示例数据写进产品代码；示例数据只放测试夹具。

## 不能违反的设计规则

- 字号、图标、控件高度、间距、圆角**只用规范第 1 节的值**，不许新增（R-064，用户明确提过）。
- 灰只用在不动的东西上（D-61）；默认不反色，只有等网络的东西用 `.busy`（D-31）；普通通知不弹横幅；背后不铺点阵（D-97）；底部面板贴底。
- 墨水屏：没有动画、转圈、闪烁光标、跳动的秒；内容超一屏就分页，不靠滚动；触控区不小于 82 px（例外见规范第 5 节）。不用 LVGL 默认样式。
- 主屏布局已定（D-106 选 A），不要改成别的。

## 里程碑和验收

| 编号 | 要求 | 怎么证明 |
| --- | --- | --- |
| U1 | 主题：规格表一处定义。检查工具 `tools/ui_scale_check.py` 进 ctest：`system/ui`、`apps/home` 里的尺寸字面量不在规格内就失败（文件头可列少量带原因的例外） | ctest；反向对照：加一个非法值，测试失败 |
| U2 | 公共组件：卡片、胶囊、圆形按钮、开关、滑杆、分段、分组列表行、Dock、顶栏、页码条、提示条、底部面板，尺寸和设计稿一致 | 组件总览场景截图 |
| U3 | 字体：规格内的字号，中文 Noto Sans CJK（常规、中等、粗体），数字 Outfit，大字号只放数字子集，缺字回退。Outfit 放 `third_party/fonts/` 带 OFL，CI 也能构建。中等和粗体本机没有：可以下载到自己目录下被忽略的 `build-deps/`，来源和哈希写进报告；CI 用 apt 的 `fonts-noto-cjk`、`fonts-noto-cjk-extra`；缺字重时构建要明确报错。报告写实际占用和 4 MB 目标（SP-05）的差距，不改目标 | 构建产物大小 |
| U4 | 图标管线：从 `Icon`、`IconSheet` 画板提取几何成 `assets/icons/*.svg`，工具转成 28、36、44、64、88 的 line、duo、solid 位图；用到的图标缺一个就构建失败 | 构建 |
| U5 | 系统手势仲裁（规范 §3）：边缘带、键盘和滑杆的输入归属、实体键优先级 | 场景测试，每条优先级一个用例 |
| U6 | 系统浮层：统一层级和打断（规范第 8 节：来电 > 闹钟 > 低电量，AI 和确认卡让路）、关闭后恢复底图、输入归属；替换各处自画顶层 | 场景测试：打断后恢复的画面逐像素一致 |
| U7 | 刷新（规范第 5 节的表）：反馈优先，两段式（先黑白再补灰阶），局部刷新区域正确 | 场景测试和模拟面板记录 |
| U8 | 页面，必须做完这 30 个画板：`Main` `Today` `Apps` `Lock` `LockPage` `Control` `Notifications` `Members` `MembersBack` `Pin` `PinWrong` `ImePinyin` `ImeCandidates` `ImeEdit` `ImePassword` `ImeVoice` `Confirm` `ConfirmDelete` `ConfirmSecret` `AiListen` `AiThink` `Ai` `Settings` `SetDisplay` `SetSound` `SetNetwork` `PowerMenu` `PowerOff` `PhoneIncoming` `ClockRing`。接真实服务和能力，能点；旧的占位外观删掉 | U9、U10 |
| U9 | 每屏 AI：快照和可用动作符合规范第 10 节；新操作都是能力，按钮和 AI 调同一个函数 | 测试遍历这 30 屏，快照不空，动作在能力表里 |
| U10 | 对照工具 `tools/ui_compare.py`：每屏跑模拟器出图，和设计稿拼成对照图到 `out/ui-compare/`（不提交），算 1/4 缩放后的平均灰度差，目标每屏不超过 12/255。超了要在报告说明差在哪；不许为了分数改规格 | 报告里的分数表 |
| U11 | 已有测试全过；AI 场景内部 RAM 最低余量不低于 32768 字节，其他预算不放宽。超了不许改上限，写清原因和缩减办法。预算前后对比写进报告 | ctest |
| U12 | `ink-sim-gui` 能启动，主页能左右滑到三屏，能进控制中心和设置（用不开窗口的场景测试验证，不要启动带窗口的程序；窗口版由复核者看） | 命令或测试 |
| U13 | 总规则第 6 节的结束检查 | 报告 |

其余画板（电话、短信、日历、相机、备忘、文件、首次开机、孩子模式等）这次不做，不要开始。
