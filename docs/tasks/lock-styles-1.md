# lock-styles-1：Mogo 的锁屏样式整块搬过来（票根、拼贴、数字时钟、老黄历、万年历、迷你书库），画在 lock-1 的锁屏上，和旧表盘并存

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 6、8、9、11、12 条）。

**D-140（总规则第 11 节）**：本任务是“搬代码”（Mogo 的锁屏样式）。原文整文件或整函数拷进来，`SOURCE.md` 写来源、固定提交、改了哪些行和为什么；为这台设备改的地方（墨水屏刷新、灰阶、108 px、随字号一起放大）也逐条记进 `SOURCE.md`。原文搬不动就写证据、停下，写进“要用户定”，不悄悄换成自己写。

## 派发前提（主会话检查，不满足不派）

- almanac-2 已压缩并合入起步分支。检查 `ink_almanac_date()` 的头文件在不在（路径以合入后为准，原稿写的 `ink/sys/almanac.h` 现在仓库里还没有）以及 `calendar.h`。没合就不派，不要自己带第二份日历数据。
- lock-1 已合入（已满足，`system/ui/lock_mark.c` 在）。
- 主会话把本任务排在 settings-2 之前合并，或接受 `settings_body.c` 那几行的合并冲突（见“范围”）。

## 为什么

用户 2026-11-12 定了方向（D-129）：每个功能选最成熟的项目整块搬，接嫁接接口，套我们的外壳、文字大小和 AI 能力，不跟上游。锁屏调研（`docs/tasks/replan-media/REPORT.md` 第一节）里 Mogo 排第一：票根把封面、进度、阅读数据和书摘连在一起，另有拼贴、时钟、月历和老黄历、壁纸、迷你书库，覆盖最完整，而且有源码、和我们同一块 S31 板子。交付计划 v4（`docs/tasks/delivery-plan.md` 第 3a 节“锁屏样式”一行）：搬代码，胶水 500 行，和旧表盘并存，进试用版 22。旧表盘的自动走时缺口（lock-faces-2 遗留）不再修，由这批样式取代（计划第 8 节）。

已经定下、这次不能动的：锁屏遮住上一页（D-135，lock-1 已合入，`system/ui/lock_mark.c`）；紧急呼叫不用解锁（D-119）；锁屏相机只能拍、只能看这次拍的（D-121）；少刷新（D-127）；升级要自动，缺东西时界面说清原因，不能白屏（D-138）；阅读只留新的那个，能力名 `reader.*` 以后由新阅读接过来（D-139）。

**现状和缺口**（读代码得到的）：

1. **Mogo 的锁屏代码一行都还没进仓库。** `third_party/mogo/` 只有阅读用的模块（graft-mogo-1～4）。现状：`book_cover.h`、`book_cover_jobs.h`、`book_shelf_covers.*` 已在；`book_cover.c`、`book_cover_auto.*`、`book_excerpt.*`、`book_title.*`、`book_metadata.*`、`book_ticket.*` 和 `services/home_library.c` 都不在（graft-mogo-4 报告：“原封面解码……没接完”）。
2. **嫁接接口只有“页面”一种客户。** `system/graft/graft.h` 的 `ink_graft_attach` 把客户挂在某个页面的状态栏下面；锁屏是 `lv_layer_sys` 上的遮盖层（`lock_mark.c` 的 `show()`，243 行起）。另外分层是反的：锁屏表盘在 `ink_system`（`system/ui/lock_faces/`），Mogo 的字体和书库引擎 `ink_mogo_native` 链进 `ink_apps`（`apps/mogo/mogo.cmake`），系统层不能往上调应用层。所以要一个很小的“锁屏样式客户”登记口，见“要做的”第 2 条。
3. **样式名单写了两份，第 0 项名字过时。** `system/ui/lock_faces/faces.c:22` 的 `ink_lock_face_choices` 和 `system/ui/shell/settings_body.c:953` 的 `LOCK_CHOICES` 是同一张表抄了两遍；第 0 项还叫“这一页”，可 lock-1 以后它已经是遮住上一页的默认锁屏。
4. **样式选择页不合新设计语言。** `system/ui/shell/wallpaper.c:25` 起的 `ink_lock_face_settings` 是两列带框的假预览，样式从 6 个变成 12 个以后一屏也放不下。
5. **Mogo 的字靠卡上的字体。** 嫁接层画字走 `apps/mogo/port/drawing.c` 的 `ui_text_box` → `ttf_draw_text_px`，字体在卡上（`/me/fonts/InkSC.ttf`，没有就 `/family/fonts/`）。卡上没有字体时字整片不出现，试用版 21 新阅读白屏就是这个原因（D-138）。
6. **老黄历数据要等 almanac-2 合入。** 接口是 `ink_almanac_date()`（Mogo 的 24 个原字段全在里面）。日历数据只从这个接口取，不再拷第二份 `calendar_data.h`。
7. **Mogo 有些前提我们没有，要换掉。** 固定按北京时间算、自己读 RTC 和 PMU（`docs/development/guides/CALENDAR.md`、`LOCK_CLOCK.md`）；日历待机时跨日不刷新；版式固定 684×1216、字号固定；缓存写在 `/sdcard/.readpico`；票根的“透明背景”会留着锁屏前的那一页（和 D-135 冲突）；`lock_screen.c` 有自己的密码循环，独占输入；票根的背景不是参数传的，而是 `book_ticket_draw` 里读 `app_settings_ticket_background()`（`book_ticket.c:286`，`book_ticket_draw` 的第二个参数被丢掉），这个函数我们没有。
8. **迷你书库在 Mogo 里是首页组件，不是锁屏样式**（`docs/development/guides/MINI_LIBRARY.md`、`main/apps/app_components.c:26-86`）。交付计划把它列在锁屏样式里，这次照计划做成锁屏样式；要不要也放到主屏，写进报告请用户定（主屏归 home-1，不在本任务）。

## 来源（搬代码）

Mogo，`$INK_DEPS/../ink-hw/upstream/oss/mogo-e4p`（即 `../ink-hw/upstream/oss/mogo-e4p`），提交 `864f2f9dc75980a67d2d6b67b17365020b7a50ce`（v1.1.0，和 `third_party/mogo/` 同一个提交）。各文件的许可写在文件头（Apache-2.0 和 `LicenseRef-Kiikoread-Personal-Study-1.0` 混着），用户说不看许可证（D-129），照原样带上，`SOURCE.md` 写清。

- **只管画的，整文件拷**：`main/book/book_ticket.c/.h`（票根，268–321 行画票面，两种背景）、`main/book/book_lock_collage.c/.h`（拼贴）、`main/ui/ui_lock_clock.c/.h`（简约时钟的纯绘制）、`main/ui/ui_calendar.c/.h`（老黄历单日版和万年历月格）、`main/ui/ui_mini_library.c/.h`、`main/book/book_mini_library.c/.h`、`main/services/home_library.c/.h`（迷你书库）、`main/ui/ui_wallpaper.c/.h`、`ui_wallpaper_crop.h`、`main/ui/ui_image_dither.c/.h`；生成的点阵 `main/assets/clock_digits.h`（源文件 75,657 B，Chakra Petch，OFL）、`main/assets/calendar_digits.h`（源文件 196,225 B，Libre Bodoni，OFL）和各自的字体许可；生成器 `tools/generate_clock_digits.py`、`tools/generate_calendar_digits.py`（只在宿主机用，`--check`）；说明 `docs/development/guides/LOCK_CLOCK.md`、`CALENDAR.md`、`MINI_LIBRARY.md`。编进固件后的大小按实测写进报告，不要用源文件大小估。
- **阅读数据模块**（票根、拼贴、迷你书库都要读）：`book_cover.c`（`.h` 已在）、`book_cover_auto.c/.h`、`book_excerpt.c/.h`、`book_title.c/.h`、`book_metadata.c/.h`。它们属于阅读，放 `third_party/mogo/main/book/`：先 `git fetch` 看 reader-2 是否已经加了，加了就直接用；没加就原样整文件加进去，在 `third_party/mogo/upstream-files.json` 末尾追加，不改别的行，并在报告里列出加了哪些文件，让主会话合并时和 reader-2 去重。
- **带上它自己的测试**，接进 ctest，照 `tests/graft_mogo/` 的做法：`tools/calendar_ui_host_test.py`、`lock_clock_ui_host_test.py`（只取绘制部分）、`lock_collage_host_test.py`、`ticket_background_ui_host_test.py`、`mini_library_host_test.py`、`mini_library_ui_host_test.py`、`home_library_host_test.py`、`wallpaper_crop_host_test.c`、`book_excerpt_host_test.py`。管密码、睡眠、Mogo 设置存储的测试（`lock_screen`、`lock_pin`、`auto_lock`、`lock_sleep_trace`、`lock_style`，以及 `lock_clock_host_test.py`、`wallpaper_cache_host_test.py` 里依赖 RTC 和 `/sdcard` 缓存的部分）不适用，报告里逐个写原因。
- **不搬，换成我们的胶水**，`SOURCE.md` 写清为什么：`main/lock/lock_clock.c`（读 RTC、驱动刷新）、`main/lock/lock_calendar.c`（读设备时间）、`main/lock/lock_screen.c` 和 `lock_pin*`（密码由 lock-1 的输密码页和家人门槛负责）、`main/power/sleep.c` 里的样式分派和睡眠、`main/time/calendar.c` 和 `calendar_data.h`（almanac-2 已带）、Mogo 自己的图片解码（我们已有 `ink_image_decode`）。
- `main/book/book_lock_bookmark.c`（书签 PNG 叠在锁屏前的画面上）不在本任务的名单里，而且和 D-135 冲突，写进报告“要用户定的”，不要自己认定不需要。

## 要做的

1. **原文入库**：锁屏专用的绘制、点阵、生成器、测试、说明拷进新的 `third_party/mogo-lock/`，旁边写 `SOURCE.md`（来源提交、每个文件的 SHA-256、改了哪几行和为什么）、`upstream-files.json` 和许可文件。原文尽量不改；必须改的（比如去掉 `sleep.c` 的头文件依赖）逐行记下。
2. **锁屏样式客户登记口**（`system/ui/lock_faces/faces.h`、`faces.c`）：加一张固定大小的表（命名上限加 `_Static_assert`），应用层在初始化时登记一个客户。客户在 BG 执行器上把整张样式画进 PSRAM 里的 684×1216 L8 画布，再报一句“这一张写着什么”（给快照用）；UI 任务把画布当图片放进锁屏遮盖层，就是现在 `ink_lock_face_draw` 画表盘的位置。状态栏、相机按钮、紧急呼叫按钮、底部提示照 `show()` 现有的表盘分支叠在上面。样式还没画好时先显示 lock-1 的默认锁屏，**不显示上一页，也不白屏**。这是任务书要求的机制，`graft.h` 不改；报告里写一段“要不要并进嫁接接口”的建议（嫁接接口归 reader-2）。
3. **应用层胶水**（新目录 `apps/mogo_lock/`，再加一个 cmake 文件，在根 `CMakeLists.txt` 和 `ports/esp32s31/main/CMakeLists.txt` 里 `mogo.cmake` 的旁边各 include 一行）：把 Mogo 的绘制编进去，链 `ink_mogo_native`，复用 `apps/mogo/port/` 的垫片（只读；缺的垫片比如 `epd_draw_rect`、`ttf_draw_text_px_spaced`、`app_settings_ticket_background` 写在自己目录）。画之前用 `ink_mogo_owner` 设成当前家人，用 `ink_mogo_dimensions` 按当前家人的文字大小设好 px，文件一律走 `ink_graft_file_open`。和阅读在同一个 BG 执行器上排队，不新开任务或线程。
4. **六个新样式**（新编号接在现有 0～5 后面，旧编号的意思一个都不变）：
   - **票根**：`book_ticket_draw`，数据是当前家人的读书进度、书摘和阅读时长。背景两种：“封面”（最近读的书的封面铺满，照 Mogo）和“壁纸”（叠在已选的壁纸上，没选壁纸就是白底），由 `app_settings_ticket_background()` 垫片返回新设置“票根背景”的值。Mogo 的“透明”（留着锁屏前的画面）不做，写进“和设计不一致的”。今天读了多久要靠阅读调用 `book_ticket_record(seconds, turns)`：阅读还没调的话，票面照 Mogo 的规矩不编造今天的数据，并把这个调用点写进报告交给 reader-2。
   - **拼贴**：`book_lock_collage_prepare` 和 `book_lock_collage_cache_commit`。Mogo 的整帧缓存改到当前家人的空间（`/me/` 下面，路径写进报告）。书没变时再锁屏，一次刷新就出整张拼贴；书变了，先出默认锁屏，再整张换成拼贴一次。
   - **数字时钟**（Mogo 叫“简约时钟”，我们已经有一个同名的，所以改叫这个，名字写进报告请用户看）：`ui_lock_clock_draw`。时间从时间服务取，按系统时区；电量图标用电源服务的电量和充电状态；分钟变了只刷数字那一块（`ui_lock_clock_area()` 以内，能更小就更小）。
   - **老黄历**、**万年历**：`ui_calendar_draw` 的单日版和月格版，做成两个样式，不另加子选项。`calendar_date_t` 用 `ink_almanac_date()` 的结果填（类型来自 almanac-2 带进来的 `calendar.h`），按系统时区的当天算。时钟没校准、日期不在 2000～2099 的，照 Mogo 显示“请校准时间”或“不在范围”。跨日零点自己重画一次，不用等按键。
   - **迷你书库**：`ui_mini_library_draw` 画最多五本，模式“最近”或“随机”照 Mogo。锁屏上不放动画、不“换一批”（Mogo 锁屏时本来就取消动画）。点一本书：记下这本书，走现有的解锁规则（D-95、家人门槛照旧），解锁后用阅读打开这本书的能力（现在是 `mogo.open`，D-139 改名后跟着改，只改这一处）。这和 AI 解锁后调同一个能力是同一条路，写法照 `faces.c` 里“继续读”的 `read_clicked` 和 `ink_lock_face_resume`。
   - **壁纸**：还是现有的“壁纸”样式（编号 2），不加第二个。用三张图（照片、线稿、带透明的 PNG）对比 Mogo 的裁切和抖动（`ui_wallpaper_draw`、`ui_image_dither`）和我们现在的（`ink_image_dither` 加 LVGL 铺满），哪个好用哪个，只留一套画法，截图和理由写进报告。解码一律用我们的 `ink_image_decode`。
5. **样式名单只留一份**：`settings_body.c` 的 `display.lock_style` 改成直接用 `ink_lock_face_choices`，删掉 `LOCK_CHOICES`（只动 `settings_body.c:953` 起这两三行，该文件归 settings-2，改动列进报告，合并冲突主会话处理）。第 0 项改成说得出它是什么的名字，比如“时钟和通知”，最终用词写进报告。新样式只往后加，已存的值升级后指的还是同一个样式。
6. **选择页按新设计语言重写**（`wallpaper.c` 里的 `ink_lock_face_settings` 和 `LockFaces` 页）：用共用列表行做分页（不滚动），每行写名字加一句说明（比如“票根 · 正在读的书和今天读了多久”），当前选中的打勾。选了票根才出现“背景”一行，选了迷你书库才出现“最近 / 随机”一行（共用分段控件）。去掉带框的假预览；想放真实缩略图的话，用同一套绘制缩小来画，预算放得下才做，放不下写进报告。入口能力 `shell.lock_faces` 和 `settings.set` 照旧。新加两项设置（票根背景、迷你书库模式）和现有的 `display.lock_todo` 一样按家人存，孩子也能改，在 `ink_wallpaper_init` 里一起登记。
7. **刷新**（D-127）：分钟没变就不刷；分钟变了，把新画布和旧画布比一遍，只把变了的矩形交给 `ink_ui_hint_area`；跨日、改时区（`time.changed`）、换家人、换样式才整张重画。锁屏和解锁的速度不能变慢：`power.unlock_to_visible_ms` 现有的预算（`tests/scenarios/standby.budget.json`，max 1200、目标 800）和测试原样要过。
8. **家人和隐私**：带读书内容的样式（票根、拼贴、迷你书库）只画当前家人的书，规则照 `faces.c:30` 的 `visible()`：家人空间关着时，屏幕和快照里都不出现书名、书摘、封面，换成一行“解锁后显示”，下面照样有时间和日期，不会空着。别的家人的书任何时候都不出现。孩子只看到自己的。空间关着时要不要也显示读书内容，写进报告“要用户定的”（关机图 `system/ui/shell/off_picture.c` 现在也是空间关着就不留书签）。
9. **文字大小和字体**：Mogo 画的字一律经过 `ink_mogo_scaled`（`apps/mogo/port/runtime.c`，依赖 `ink_mogo_dimensions` 设好的 px）跟系统“文字大小”走（孩子按 110%，和 `apps/mogo/mogo.c` 一样）。Mogo 固定版式里放不下的，照 Mogo 自己的规矩换行或在整字处加省略号，不能叠字、不能裁半个字。只有两套数字点阵（时钟大数字、日历大日期）保持原尺寸，它们比最大一档的字还大；这一条例外写进报告并附两档截图。卡上字体（`/me/fonts/InkSC.ttf`，没有就 `/family/fonts/`）读不出来时，这张样式不画，退回 lock-1 的默认锁屏，底部一行写原因（比如“锁屏样式要用的字体还没放到卡上”），快照也写这句（D-138）。
10. **快照和 AI**：锁屏快照（`lock_mark.c` 的 `semantic_snapshot`，经 `ink_lock_face_fields`）写清当前是哪个样式，以及画面上写着什么：时间、日期、农历；老黄历的宜、忌、冲煞；票根的书名、进度、书摘；拼贴和迷你书库的书名；退回默认锁屏时写原因。`INK_LOCK_FACE_FIELDS` 按需要加大。选样式、改两项新设置，人点和 AI 调的都是 `settings.set`；不为锁屏样式另加能力，除非绕不开（绕不开就写进“新增的机制”）。`lock_mark.c` 原则上不改；确实要改，只改样式分派和字段那几行，列进报告。

## 共同要求（页面任务都一样）

- 来源：功能、状态、出错处理照 Mogo 原文，不自己发挥；`third_party/mogo-lock/SOURCE.md` 写清每个样式对应原文的哪个文件和行号，哪些没照搬、为什么。
- 新设计语言：选择页、设置行、提示行只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/ui_list.c` 里的列表行、分段、空状态），能点的不小于 108 px，一屏最多一个主按钮，没有粗框，图标不套圆圈，不写死像素和颜色（`tools/ui_scale_check.py`，ctest 里的 `ui-scale` 会查 `system/ui`）。老黄历画面自带的双线框、圆印、题签是 Mogo 的画面内容，照原样保留，写进“和设计不一致的”，附截图给用户看。
- 不变的规矩：家人隔离、孩子模式、确认卡只给 D-105 的高风险动作（这里没有）、按下反馈 D-31、密码不给 AI 也不进日志、LVGL 只在 UI 任务里碰、大缓冲放 PSRAM。
- 旧表盘（编号 1、3、4、5）原样保留，不修它们的缺口；“月历”和“万年历”有重复，写进报告请用户以后定。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交（能力有变化时）；新能力进 `tests/integration/ai_surface.c` 的两张表；新文案的英文加进 `assets/i18n/en.tsv`。这些文件都只在末尾追加本任务的行，合并冲突由主会话处理。
- 截图放 `docs/tasks/lock-styles-1/`，每张不超过 300 KB，先看干净的整页图（没有调试框）。

## 范围和预算

- 只改：新的 `third_party/mogo-lock/`；`third_party/mogo/main/book/` 里新加的阅读数据模块（只能新加整文件）和 `upstream-files.json` 末尾；新的 `apps/mogo_lock/`；`system/ui/lock_faces/faces.c`、`faces.h`（必要时同目录加一个新文件）；`system/ui/shell/wallpaper.c`；`settings_body.c` 里样式名单那两三行（归 settings-2，列进报告）；根 `CMakeLists.txt` 和 `ports/esp32s31/main/CMakeLists.txt` 各一行 include 和新测试的登记；锁屏相关测试；`docs/tasks/lock-styles-1/`。
- 不碰：`system/graft/*`（归 reader-2，缺什么写进报告）；`apps/mogo/` 的现有文件（确实要补一个垫片声明，只追加一行，列进报告）；`third_party/mogo/` 里已有的文件；`system/services/time/*`（almanac-2 的，只读着用）；`settings_family.c`、`settings_registry.c`（settings-2；设置搜索要加“票根、老黄历”这些别名，写进报告交给它）；`shell_home.c`（home-1）；`system/ui/shell/off_picture.c`。
- 产品代码行数分两块计，拷来的原文、生成的点阵、测试和文档都不算：`apps/mogo_lock/` 胶水不超过 **500 行**；系统侧（`faces.c/.h`、`wallpaper.c` 的新增和改动）另外不超过 **350 行**。超了不要停，也不要砍内容，做完在报告里写清各块行数，由主会话定。
- 固件增量不超过 **160 KB**，大头是两套数字点阵（编译后大小实测，原稿估约 85 KB）。超了不要停，也不要砍内容或压缩原表：做完，写清每一块多大（点阵、各样式代码、胶水），由主会话定。内部 RAM 不增加（状态放 PSRAM，照 `faces.c` 的 `INK_PSRAM_BSS`）；写清 PSRAM 峰值（一张 684×1216 的 L8 画布 831,744 B，加上封面工作区）。
- 起步：主会话给的分支（必须已合 almanac-2，见“派发前提”）。almanac-2 压缩后接口要是变了，跟着它的接口走，不要自己再带一份数据。

## 验收

1. **原文核对**：`upstream-files.json` 里的 SHA-256 和固定提交逐个一致；`generate_clock_digits.py --check`、`generate_calendar_digits.py --check` 通过；带来的 Mogo 宿主测试在 ctest 里全过。
2. **截图**（控制数量）：七个样式（六个新的加壁纸）在系统默认和最大文字大小下各一张有内容的大人。票根和迷你书库再加“什么书都没有”和“孩子”两种，票根两种背景都要，空书库和孩子只截默认大小；选择页（含两个子选项）两档各一张；锁屏改前和改后并排。
3. **老黄历和数据是同一份**：2026-02-17（春节）、一个闰月里的日子、一个节气当天，锁屏快照里的宜、忌、冲煞和 `time.almanac` 的返回逐字一致。反向对照：给画面喂错一天，测试必须失败。
4. **刷新**（模拟器的显示统计断言，用 `time.changed` 驱动时钟，照 `tests/lock_faces.c` 的写法）：数字时钟分钟变了，刷新区域在时钟数字那块以内，不是整屏；分钟没变，零刷新；23:59 到 00:00，老黄历、万年历、票根不按键自己重画一次；改时区重画一次。反向对照：去掉“只交变了的矩形”，测试必须失败。
5. **家人和隐私**：空间关着时，票根、拼贴、迷你书库在屏幕和快照里都没有书名、书摘、封面，并显示“解锁后显示”；换家人以后看不到上一位的书；孩子只看到自己的。反向对照：去掉 `visible()` 判断，测试必须失败。
6. **人和 AI 一样**：每个新样式点选择页选一遍、用 `settings.set`（来源是 AI）选一遍，状态和快照一样；在迷你书库点一本书，走现有解锁规则以后打开的就是这本，和 AI 解锁后调同一个能力结果一样。
7. **并存和升级**：编号 0～5 的意思不变（存着 5 的设备升级后还是“月历”）；第 0 项改了名；新样式接在后面。
8. **缺东西时说清楚（D-138）**：没有书、时钟没校准、日期不在范围、卡上没有字体、封面解不出来，每种情况屏幕上都有一句原因或者 Mogo 自己的占位，快照写着同一句，任何情况都不白屏、不露出上一页。
9. **锁屏和解锁不变慢**：现有的待机、解锁时间测试原样通过，预算不改。
10. 主工程普通全量 ctest 过（含 `ui-scale`、`lock-faces`、`lock-cover`）；ASan 跑锁屏相关测试和 Mogo 锁屏宿主测试过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告；`gcccheck` 无发现。报告写：固件增量（分块）、内部 RAM 和 PSRAM、产品代码行数（两块分开）、改过的已有测试（逐条写改了什么、为什么，只改选择页结构和样式数量带来的定位，不改业务断言，不放宽、不跳过、不改时限）、“新增的机制”（样式客户登记口、拼贴缓存、两项新设置各一行）、给 reader-2 的接口说明（`book_ticket_record` 的调用点、新加进 `third_party/mogo/` 的阅读数据模块清单、`mogo.open` 改名后要跟着改的那一处）、上机核对清单（面板上的分钟局部刷新和灰阶封面的残影、每分钟唤醒的待机耗电、卡上字体在不在）。
