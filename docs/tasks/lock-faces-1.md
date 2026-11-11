# lock-faces-1：锁屏表盘——几种时钟样式、日历和农历、待办和继续阅读小组件、透明壁纸

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 11、12 条：底层逻辑拿来集成、AI 接入层加在上面、界面照成熟项目的布局用 LVGL 重现）。

用户在功能清单里勾了：锁屏时钟（简约时钟、待机时钟）、锁屏日历和老黄历、老黄历（农历、宜忌）、锁屏待办、锁屏组件（最近阅读）、锁屏和待机壁纸（支持透明 PNG）、锁屏句子。“阅读小票”用户备注“锁屏先不做”，本任务不做。清单见 `docs/tasks/reuse-crosspoint-1/picks.md`。

输入：

- `docs/tasks/reuse-crosspoint-1/REPORT.md` 第 127 行和 219～224 行（锁屏各项的来源和做法），`port-queue.json` 的 L01、L02、L03。
- CrossMux 固定副本（MIT，`$INK_DEPS/../ink-hw/upstream/oss/crossmux`，提交 a6fd569）：`src/activities/apps/standby/SloppyClockFace.cpp`、`ChineseCalendarFace.cpp`、`ChineseAlmanac.cpp`、`StandbyActivity.cpp`。它用自己的 GfxRenderer 画，不是 LVGL：**照搬布局和信息排列，用 LVGL 和我们的主题令牌重现**；历法用我们已有的 `system/services/time/lunar.c`，不搬第二份日期表，不搬它的待机任务和刷新循环。
- 现在的锁屏：`system/ui/lock_mark.c`（`display.lock_style`：0 锁定页、1 分钟时钟、2 壁纸），`system/ui/shell/wallpaper.c`。
- `docs/tasks/ui-audit-2/TASKS.md` UX17 里锁屏那部分（锁定时不要有假的可点外观，S11～S18 中锁屏相关项）。

## 要做的

1. **时钟样式**：在现有分钟时钟之外，照 CrossMux `SloppyClockFace` 的排法加“简约时钟”（大号时间、日期和星期、农历一行、电量），再加一种“待机时钟”（时间偏上，下面留给小组件）。分钟没变不刷新，只刷时间那一块（D-127）。
2. **日历**：照 `ChineseCalendarFace` 的排法做月历表盘：公历日期、农历日名（初一显示月名）、节气、今天突出、年干支和生肖。**宜忌**：CrossMux 的宜忌是按日支循环选词，不是真正的黄历算法，**不显示**，报告写明缺口和可选方案（例如传统建除十二神推算），交主会话问用户。日期变了才重画。
3. **小组件**（可放在待机时钟和日历表盘下方，用户选开关）：
   - 待办：取现有备忘里的待办（`system/apps/memo/` 现有接口，只读），只显示当前锁屏所有者能看的；锁住大人空间或切到孩子时不显示大人的私密待办；
   - 继续阅读：取现有 `reader.last`（书名、第几页），点它解锁后直接继续读。
4. **壁纸**：壁纸样式支持透明 PNG，表盘画在壁纸上面；从现有壁纸设置选图（SD 卡或文件里的图片，走现有存储准入）。壁纸图库内容等用户定，不内置别人的图。
5. **选择**：在 `wallpaper.c` 的锁屏设置页加表盘选择（每种一张小预览）和小组件开关，存在现有设置项里（`display.lock_style` 扩值，新增开关用现有设置服务，不新增文件格式）。
6. **AI 接入**：选表盘、开关小组件都走现有设置能力（人和 AI 同一套）；锁屏页面快照给出表盘类型和可见字段（时间、日期、农历、可公开的小组件内容），私密内容不进快照。
7. 锁屏相机入口、紧急呼叫入口保留；锁定状态下不可点的东西不要画成按钮样子（UX17）。

## 范围和预算

- 只改：`system/ui/lock_mark.c`、`system/ui/shell/wallpaper.c`、新增 `system/ui/lock_faces/`（`CMakeLists.txt` 里加这几个源文件的行）、锁屏相关测试和截图基线、`docs/tasks/lock-faces-1/`。不改 `settings_pages.c`（`ui-fix-b` 在改）、电源和睡眠代码（`port-power-2` 在改驱动）、`textfield.c`、`keyboard.c`、`apps/reader/`。
- 胶水和界面新增不超过 450 行；不新增常驻任务、定时循环、文件格式。

## 验收

1. 每种表盘和小组件在常规、120% 字号、壁纸（含透明 PNG）下的截图（`docs/tasks/lock-faces-1/`，单张不超过 300 KB）。
2. 测试：分钟没变不刷新、跨日重画日历；农历和节气与 `lunar.c` 一致（含闰月一例）；孩子锁屏看不到大人的待办，快照里也没有；继续阅读点了解锁后回到那一页；反向对照。
3. 主工程普通全量 ctest 过（含 `ui-scale`）；S31 编译主会话合并时跑。
4. 报告写清每种表盘照搬了 CrossMux 哪些布局、宜忌缺口和方案、上机核对清单（主会话在真机看各表盘的刷新和残影）。
