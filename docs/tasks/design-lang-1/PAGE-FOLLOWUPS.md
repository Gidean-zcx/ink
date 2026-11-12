# 页面任务接续清单

【实现】以下页面文件本任务均未修改。不是这些页面全部写死像素：有的使用主题令牌，但在页面内单独构造容器、圆形和固定分栏，无法随共用控件完整改变。行号以任务基线为准。

| 页面 / 文件位置 | 仍需页面任务处理的内容 |
| --- | --- |
| `system/ui/shell/navigation.c:568` | 主屏页码自行画拉长的胶囊和圆点，未调用共用页码。 |
| `system/ui/shell/navigation.c:737` | 继续读入口显式全圆角；空内容卡片仍由页面决定显示。 |
| `system/ui/shell/navigation.c:777`、`:788` | 今日首卡和两列小卡按剩余高度分配，文本换行后没有重新分配空间。 |
| `system/ui/shell/navigation.c:860` | Dock 显式全圆角外框、五等分布局；共用图标已去圆底，Dock 容器仍要另改。 |
| `system/ui/shell/navigation.c:1011` | 滑杆释放回调按整行绝对横坐标换算 0–10。应改为共享轨道区间换算，并让两端逐格增减；本任务只变外观，不能声称步进已实现。 |
| `system/ui/shell/navigation.c:1062` | 控制中心按页面自己的行构造快捷入口，尚未接入新开关格、两列 16 间距和状态第二行。 |
| `system/ui/shell/navigation.c:2386` | 孩子音量上限仍按旧式满行条定位，需与新滑杆轨道对齐。 |
| `system/ui/shell/settings_pages.c:219`、`:225` | 页脚仍显式全圆角；连续分组仍另画左右侧边的半块，须删除这些页面自绘侧边。分组标题的 64 高灰字也尚未接入。 |
| `system/ui/shell/keyboard.c:345`、`:392` | 原生矩阵逐键描边；十列字母键、七个编辑动作共占一行，不能同时满足每项至少 108 宽。需重新排布，不能靠共用控件扩宽后互相覆盖。 |
| `system/ui/shell/member_gate.c:140` | 锁屏相机入口显式再加描边。 |
| `system/ui/shell/wallpaper.c:33` | 锁屏样式预览页自绘带框缩略图，固定网格须为页脚和大字重新留位。 |
| `system/ui/shell/ai_panel.c:18`、`:114` | 面板高度由主题行高、间距计算；新主题下默认 533、展开 673，须按实际内容核对大字状态。保留模态淡背景和原有刷新逻辑。 |
| `system/apps/sms/sms.c:211`、`:261` | 短信列表项显式要求边框；对话内容窗及底部回复行由页面独立定位。 |
| `system/apps/phone/phone.c:414` | 拨号矩阵、底部入口需检查 108 高后的位置及长文字；来电说明在 `system/ui/shell/interrupt_card.c` 独立排版。 |
| `system/apps/contacts/contacts.c:15`、`:176` | 固定四项容量及自绘列表容器，应检查新行高下完整字段与分页。 |
| `system/apps/memo/memo.c:299`、`:380` | 录音大圆环和清单圆形勾选框显式绘制；新空状态与文字动作需由页面选择接入。 |
| `system/apps/clock/clock.c:106`、`:398` | 闹钟表单与秒表行自行布局；星期文字、底部动作在中英大字下需重新分配空间。 |
| `system/apps/calculator/calculator.c:246` | 固定四列、五排数字键盘，与共用普通按钮的视觉尺寸分开计算。 |
| `system/apps/calendar/calendar.c:116` | 月历固定七列；每日格宽不足 108 是画布约束，需页面选择交互方式。日期字样与空日程说明需避免重叠。 |
| `system/apps/weather/weather.c:209`、`:242` | 预报三行、底部换城市动作独立布局；两行城市结果虽使用共用行，后续须按大字重新分页。 |
| `system/apps/gallery/gallery.c:33` | 相册固定三列九格，空相册框架与页脚由页面自行控制。 |
| `system/apps/camera/camera.c:150`、`system/apps/camera/scan.c:120` | 相机提示、自绘取景区、扫码分页要单独核对；页码箭头子项 0/2 已保留。 |
| `system/apps/files/files.c:184` | 文件项自己重设静态表面与子项；新空状态和大字下的操作栏要继续核对。 |
| `apps/reader/reader.c:859`、`:874` | 阅读字体选项同时使用主题枚举和固定公开 JSON；本任务保留 36 兼容值，界面渲染归一为 38，未改阅读正文合同。 |

放大后的实际裁切、重叠和不足 108 的控件由 `tests/design_lang_1.py` 导出到构建目录；报告附最终统计，裁切断言不放宽。这里只记录接续位置，不推断真机灰阶和触摸表现。
