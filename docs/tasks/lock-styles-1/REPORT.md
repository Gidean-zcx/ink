# lock-styles-1 报告

（子代理不能写报告文件，由主会话照它交回的原文存进分支。）

# lock-styles-1 报告：Mogo 锁屏样式搬进来了

## 一句话
六个新锁屏样式（票根、拼贴、数字时钟、老黄历、万年历、迷你书库）用 Mogo 原代码画，接在旧的 0～5 后面（编号 6～11），旧表盘一个没动；选择页改成分页列表；全量测试、ASan、S31 编译、gcc 检查都过了。代码行数超预算（胶水 678/500，系统侧约 495/350），固件增量 135.5 KB 在预算内，内部 RAM 多 80 B。

## 验收逐条
| 验收项 | 结果 | 证据 |
| --- | --- | --- |
| 1 原文核对 | 过 | upstream-files.json 63 个文件 SHA-256 一致（1 个有记录的改动），ctest `mogo-lock-provenance`、`mogo-provenance`；两个点阵生成器 `--check` 过（`mogo-lock-host-clock-generator`、`-calendar-generator`）；Mogo 宿主测试接进 ctest 的 6 个全过（`mogo-lock-host-*`）；另 3 个没接，见“没做的” |
| 2 截图 | 基本齐 | `docs/tasks/lock-styles-1/screenshots/`：七个样式两档字号（*-size-1/-size-3，含 wallpaper）；票根两种背景（ticket-size-*、ticket-wallpaper-size-*）；空书库和孩子（ticket-empty、library-empty、library-random-empty、child-ticket/collage/mini-library）；选择页两档含两个子选项（picker-size-*、picker-ticket-size-*、picker-mini-library-size-*）；选择页改前改后并排（picker-before-after.png）；家人空间关着（closed-*）；没字体（no-font）；三个日期的老黄历（almanac-2026-02-17 春节、2023-03-22 闰二月初一、2026-11-22 小雪） |
| 3 老黄历和数据同一份 | 过 | lock-styles-almanac：三天的宜、忌、冲、煞在锁屏快照里和 time.almanac 返回逐字一致；反向对照（喂错一天）测试失败 |
| 4 刷新 | 过 | lock-styles-refresh：分钟变了只刷时钟数字那块（面板上像素变化框 (356,592)-(467,736)，在时钟区内），1 次刷新；同一分钟 0 次；23:59→00:00 票根、老黄历、万年历不按键自己重画；改时区重画；反向对照（整屏刷）测试失败 |
| 5 家人和隐私 | 过 | lock-styles-privacy：空间关着时三种读书样式的屏幕和快照都没有书名、书摘、封面，写“解锁后显示”，下面有时间日期；换成孩子后看不到大人的书；两个反向对照（不按空间开关重画、关着也画书）测试都失败 |
| 6 人和 AI 一样 | 过 | lock-styles-entry：六个新样式各点选一遍、AI settings.set 选一遍，快照里“画面上写着”一致（数字时钟和迷你书库时间会走，不比时间）；锁屏点迷你书库中间那本 → 解锁 → 打开的是这本（MogoReader），和 AI 调 mogo.open 结果一样 |
| 7 并存和升级 | 过 | lock-styles-upgrade：存着 5 的还是“月历”；第 0 项叫“时钟和通知”；新样式在后面 |
| 8 缺东西说清楚 | 基本过 | 没书（“还没有读过的书，读过一本后这里显示票根”/Mogo 自己的“还没有阅读记录”）、卡上没字体（“锁屏样式要用的字体还没放到卡上”）、空间关着且字体没载入（“解锁一次后才能显示这个锁屏样式”）都在默认锁屏底部写原因、快照同一句，从不白屏、不露上一页；时钟没校准、不在范围：代码走 Mogo 的“请校准时间”，Mogo 自己的宿主测试（lock_clock_ui）覆盖了没校准的画面，但模拟器开机时间总是有效，没法做整机测试（见“没做的”） |
| 9 锁屏解锁不变慢 | 过 | scenario-standby-metalio-plus 原样过，预算没改 |
| 10 全量、ASan、S31、gcccheck | 过 | 见上；gcccheck 改过的 12 个文件 + 13 个新编进固件的 Mogo 文件，0 发现（测试里的 CHECK 文案截断已改短） |

## 每个主要操作刷几次屏
- 锁屏，样式已画好且内容没变：1 次整屏。
- 锁屏，画布有效但分钟变了（数字时钟、迷你书库、空间关着的画面）：1 次整屏（时间那块先空着）+ 1 次只刷时间那块。
- 锁屏，画布还没画好（第一次、换了家人、书变了）：默认锁屏 1 次 + 换成样式 1 次整屏。
- 锁着时分钟变了：数字时钟 1 次局部（只数字）；票根、拼贴、老黄历、万年历 0 次；迷你书库和空间关着的画面 1 次局部（时间文字）。同一分钟 0 次。
- 跨日零点：1 次，只交变了的矩形。
- 迷你书库点旁边一本：1 次局部（选中换了）；点中间那本：解锁，按解锁规则走。
- 选择页选一个样式：1 次（整页重画）；翻页：1 次。

## 没做的
1. Mogo 的 3 个界面宿主测试 `calendar_ui_host_test.py`、`ticket_background_ui_host_test.py`、`mini_library_ui_host_test.py` 拷来了但没接进 ctest：它们要 Mogo 整套界面测试工具（阅读日志、文件页、设置页 161 KB、内置汉字点阵 188 KB、阅读场景图等约 600 KB 原文件）。这三张画面由模拟器截图和快照断言覆盖。
2. 时钟没校准、日期不在 2000～2099 的整机测试：模拟器开机时间总是有效（profile 的 rtc=false 也一样），没法造出来。代码路径照 Mogo（传 NULL → “请校准时间”），快照写“请先校准时间”或“不在范围（2000—2099 年）”。
3. 选择页的真实缩略图：预算不够，没做。
4. 书摘：我们的阅读还没调 Mogo 的 `book_excerpt_observe`，`book_index_cache_*` 也是空实现，所以票根永远没有书摘（不编造）。交给 reader-2。
5. “今日阅读”：阅读还没调 `book_ticket_record`，票面照 Mogo 规矩写“今日阅读 —  ·  日期未同步”（这句字面意思有点误导，见“要用户定”）。

## 和设计不一致的
1. Mogo 的锁屏是整页画面、没有状态栏和按钮。我们要有紧急呼叫（D-119）和相机（D-121），所以整张画面缩到状态栏下面（1108/1216，约 91%，左右各留 30 px），紧急呼叫做成按字宽的小按钮、相机挪进状态栏那一行；新样式底部不放“已锁定，按电源键或 AI 键唤醒”（放不下，快照里还有）。代价：Mogo 的字小一点、略软（24 px → 约 22 px）。截图见 *-size-*.png。
2. 票根“透明”背景（留着锁屏前那一页）不做（D-135），换成“壁纸”背景：画布先铺已选壁纸，没选就是白底。
3. 老黄历的双线框、圆印、题签、万年历的山水和圆点，是 Mogo 的画面内容，照原样保留（和“无粗框、图标不套圆圈”的规矩冲突）；Mogo 自己在画面上写的 “mogo” 字样（老黄历、万年历、票根“mogo / 阅读票根”）也原样保留，见“要用户定”。
4. 票根、拼贴里的封面用 Mogo 自己的 16 级等分抖动；壁纸用我们的（按面板实测亮度）。
5. 选择页的快照：所选样式是一个字段，这一页上看得见的行和上一页/下一页是动作；说明文字不进快照（AI 上下文 3 KB 放不下 12 条）。
6. 迷你书库在锁屏上：不放动画，不放“换一批”（胶水把那块涂白），点旁边的书只换选中，点中间那本才解锁打开。
7. 没有 /me/books 当作空书架，不当作 Mogo 的“卡读不了”。
8. 缓存在 /me/mogo/locks/collage.bin、/me/mogo/covers/：卡上不许点开头的名字，所以在文件页里看得见。
9. sdk-boundary 只查带引号的包含：胶水用尖括号包含 `<lock_faces/faces.h>`、`<ink/sys/almanac.h>`、`<ink/sys/image.h>`，和 apps/mogo/mogo.c 包含 `<ink/ui_theme.h>` 的做法一样。almanac 和 image 都是纯内存接口；如果要求应用只走软总线，老黄历要改成调 time.almanac。

## 壁纸两套画法对比（只留一套）
`docs/tasks/lock-styles-1/wallpaper_compare.py` 用同一张解码后的灰度图，按面板实测亮度模拟显示：照片 wallpaper-photo.png、线稿 wallpaper-lineart.png、带透明的 PNG wallpaper-alpha.png（左我们、右 Mogo）。Mogo 先缩放再在屏幕坐标上抖动，花纹稳定，但它按 16 等分取灰阶，没按这块面板的亮度曲线，中间调整体发白、浅色几乎消失（照片发灰白，透明图的浅色圆边没了）；我们的按面板实测亮度抖动，层次对，缺点是先抖动再由 LVGL 最近点缩放，深色大块上能看出一点纹理。结论：留我们的画法（`ink_image_dither` + LVGL 铺满），Mogo 的 `ui_wallpaper.c` 只拷来不编译。建议以后把我们的抖动挪到缩放之后（要用户定）。

## 改过的已有测试
1. `tests/lock_faces.c`：选择页从两列格子变成分页列表，新增 `touch_face()`，找不到就用翻页按钮同一个能力（`shell.lock_faces {"page":n}`）翻到下一页再点；用在 12 个样式行和两个开关上。矩阵里“待办 / 继续读”两条检查原来对 `face >= INK_LOCK_STANDBY` 生效，现在限定 `face < INK_LOCK_DRAWN`（新样式本来就没有这两个小组件）。断言内容、时限都没改。
2. `tests/settings_b.c`：第 0 项名字 “这一页” → “时钟和通知”。
3. `tests/integration/ai_surface.c`：只在合入 almanac-2 时解决冲突（两边各一行都留），不是本任务的改动。

## 新加的测试
`lock-styles-styles/almanac/refresh/privacy/entry/upgrade/empty/nofont`（tests/lock_styles.c + .py）、`lock-styles-reverse`（4 个反向对照）、`mogo-lock-host-*`（8 个：wallpaper_crop、mini_library、home_library、lock_collage、book_excerpt、lock_clock_ui、两个生成器）、`mogo-lock-provenance`。

## 新增的机制
- 锁屏样式客户登记口：`system/ui/lock_faces/faces.h` 的 `ink_lock_client_t` + `ink_lock_face_register()`（固定 2 个位置，名字长度检查），实现在新文件 `drawn.c`：客户在 BG 执行器上画 684×1216 L8 画布（PSRAM），报一段“画面上写着”；UI 任务把画布放在锁屏遮盖层；没画好先出默认锁屏；分钟/日期/时区/家人/样式/阅读进度变了才重画，新旧画布逐行比较，最多 8 个变了的矩形交给 `ink_ui_hint_area`。`lock_mark.c` 通过 `ink_lock_face_ready()`、`ink_lock_face_why()`、`ink_lock_face_hook()` 接上。探针 `lock.drawn.renders/changed/same/whole/areas/x1..y2`。
- 拼贴缓存：`/me/mogo/locks/collage.bin`（当前家人的空间），封面缓存 `/me/mogo/covers/`。
- 两项设置：`display.lock_ticket_bg`（票根背景：封面/壁纸）、`display.lock_library`（迷你书库显示：最近/随机），按家人存、孩子也能改，在 `ink_wallpaper_init` 里登记。
- `shell.lock_faces` 多一个可选参数 `{"page":n}`（翻页按钮和 AI 同一个入口）。

## 给 reader-2 的接口说明
- `book_ticket_record(seconds, turns)`：Mogo 在 app_book.c:4039 记阅读时长和翻页；我们的阅读要在翻页/退出时调，票根才有“今日阅读”。
- `book_progress_set_last_path(path)`：Mogo 开书时调（app_book.c:7867）；我们的阅读没调，胶水暂时用进度里最近读的那本补记（mogo_lock.c `last_book()`），reader-2 接上后可以删。
- `book_excerpt_observe` / `book_excerpt_flush`（app_book.c:1312、1723、4074）和 `book_index_cache_*`（现在是空实现）：接上后票根才有书摘。
- 新加进 third_party/mogo/main/book/ 的阅读数据模块：book_cover.c、book_cover_auto.c/.h、book_excerpt.c/.h、book_title.c/.h、book_metadata.c/.h（upstream-files.json 末尾），合并时和 reader-2 去重。
- 打开书的能力名只在 `apps/mogo_lock/mogo_lock.c` 的 `MOGO_LOCK_OPEN_CAP "mogo.open"` 一处，D-139 改名后改这里。
- `apps/mogo/port/runtime.c` 追加了 `ink_mogo_dimensions_get()`：锁屏画完把阅读页的尺寸和家人放回去（共用 BG 执行器和字体）。建议嫁接接口以后提供“画布客户”（给一块 L8 缓冲、在 BG 画、报快照），把这套保存/恢复、文件和字体的家人检查收进 graft，锁屏样式就不用另开登记口。

## 给 settings-2 的
`settings_body.c` 改了两行：删掉 `LOCK_CHOICES`，`display.lock_style` 直接用 `ink_lock_face_choices`（faces.c 里唯一一份）。设置搜索请加别名：票根、拼贴、数字时钟、老黄历、万年历、迷你书库、票根背景。

## 要用户定
1. 新样式的整张画面缩到状态栏下面（约 91%），紧急呼叫和相机挪进状态栏、底部提示不放——这样可以吗？还是要别的放法（比如按钮叠在画面上、画面不缩）。
2. “数字时钟”这个名字（Mogo 叫“简约时钟”，我们已有同名的）、第 0 项叫“时钟和通知”可以吗？
3. 家人空间关着时，读书样式要不要也显示读书内容？现在不显示，写“解锁后显示”（关机图也是这样）。
4. 迷你书库要不要也放到主屏（归 home-1）？
5. `book_lock_bookmark.c`（书签图叠在锁屏前的画面上）和 D-135 冲突，要不要、怎么做？
6. “月历”（旧 5）和“万年历”（新 10）重复，要不要合并或去掉一个？
7. Mogo 画面里自带的 “mogo” 字样要不要去掉（要改原文）？
8. 阅读接上 `book_ticket_record` 之前，票面会写“今日阅读 —  ·  日期未同步”，字面像是时间没对，是否接受到 reader-2 为止？
9. 壁纸：要不要把我们的抖动挪到缩放之后（去掉深色大块上的纹理）？
10. 代码行数超预算：胶水 678/500，系统侧约 495/350；lock_mark.c 改了 39 行（超出“只改分派和字段”）。
11. 内部 RAM 多 80 B（Mogo 原文件的小 .sbss 变量），要不要再想办法挪走。

## 上机核对清单
- 面板上分钟局部刷新是否只动数字那块；灰阶封面（票根、拼贴、迷你书库）有没有残影，整屏换样式那一下是否干净。
- 每分钟唤醒重画（数字时钟、迷你书库）的待机耗电。
- 卡上字体在不在（/me/fonts/InkSC.ttf 或 /family/fonts/InkSC.ttf）；没有时默认锁屏底部是否写原因。
- BG 执行器 8 KB 栈：模拟器估算已降到预算内（胶水的大缓冲挪进 PSRAM），真机用 -fstack-usage 再看一遍 Mogo 的最大帧（ui_calendar 的 almanac_words、book_ticket_draw）。
- PSRAM 峰值约 2.5 MB（两张画布常驻 1.66 MB）。
- 有 PIN 的家人锁屏过一段时间空间关上后，新样式是否换成“解锁后显示”的画面。

## 复核修复（两路审查后的修复轮，修复代理交回的原文）

# lock-styles-1 报告：Mogo 锁屏样式搬进来了（含复核修复）

## 一句话
六个新锁屏样式（票根、拼贴、数字时钟、老黄历、万年历、迷你书库）用 Mogo 的原代码画，接在旧的 0～5 后面（编号 6～11），旧表盘一个没动。选择页改成了分页列表。复核查出的四个问题，三个代码问题都修了，对照表也补上了（见最后一节“复核修复”）。全量测试、S31 编译、gcc 检查都过了。代码行数超预算：胶水约 706/500，系统侧约 535/350。固件比改前多约 136.1 KB，在 160 KB 预算内。内部 RAM 多 80 B，复核没有再增加。

## 验收逐条
| 验收项 | 结果 | 证据 |
| --- | --- | --- |
| 1 原文核对 | 过 | upstream-files.json 里 63 个文件的 SHA-256 都一致（有 1 处改动，已记录）。ctest `mogo-lock-provenance`、`mogo-provenance` 过；两个点阵生成器 `--check` 过（`mogo-lock-host-clock-generator`、`-calendar-generator`）；接进 ctest 的 6 个 Mogo 宿主测试全过（`mogo-lock-host-*`）。另外 3 个没接，见“没做的” |
| 2 截图 | 基本齐 | 都在 `docs/tasks/lock-styles-1/screenshots/`：七个样式各两档字号（*-size-1/-size-3）；票根两种背景；空书库和孩子；选择页两档，含两个子选项；选择页改前改后并排；家人空间关着；没字体；三个日期的老黄历 |
| 3 老黄历和数据是同一份 | 过 | lock-styles-almanac：三天的宜、忌、冲、煞，锁屏快照和 time.almanac 的返回逐字一致；反向对照（喂错一天）测试失败 |
| 4 刷新 | 过 | lock-styles-refresh：数字时钟分钟变了只刷数字那块，1 次；同一分钟 0 次；23:59→00:00 票根、老黄历、万年历不按键自己重画；改时区重画。复核加了一条：迷你书库的分钟只刷时间那块，而且不扫卡。反向对照（整屏刷、每次都扫）都失败 |
| 5 家人和隐私 | 过 | lock-styles-privacy：空间关着时三种读书样式的屏幕和快照里没有书名、书摘、封面，写着“解锁后显示”；孩子看不到大人的书；两个反向对照都失败 |
| 6 人和 AI 一样 | 过 | lock-styles-entry：六个样式点选和 AI settings.set 各选一遍，快照一样；锁屏点迷你书库中间那本 → 解锁 → 打开的就是这本，和 AI 调 mogo.open 结果一样 |
| 7 并存和升级 | 过 | lock-styles-upgrade：存着 5 的还是“月历”；第 0 项叫“时钟和通知” |
| 8 缺东西说清楚 | 基本过 | 没书、卡上没字体、空间关着且字体没载入，三种情况默认锁屏底部都写原因，快照写同一句，不白屏也不露上一页。时钟没校准、日期不在范围：模拟器造不出来，只有 Mogo 自己的宿主测试 |
| 9 锁屏解锁不变慢 | 过 | scenario-standby-metalio-plus 原样过，预算没改 |
| 10 全量、ASan、S31、gcccheck | 过 | 全量 ctest 见“复核修复”；S31 编译通过，没有新警告；gcccheck 0 发现 |
| D-140 对照表 | 已补（晚了） | `docs/tasks/lock-styles-1/对照表.md`，37 行，单独提交（adac18c2）。它写在代码之后，“先写”这一步补不回来 |

## 每个主要操作刷几次屏
- **锁屏，样式已画好、内容没变**：整屏 1 次。
- **锁屏，画布有效但分钟变了**（数字时钟、迷你书库、空间关着的画面）：整屏 1 次（时间先空着），再只刷时间那块 1 次。画的时候芯片不睡，所以这一下就在锁屏当时补上。
- **锁屏，画布还没画好**（第一次、换了家人、书变了）：先出默认锁屏 1 次，再整张换成样式 1 次。复核前，第一次锁票根会多画一遍、多换一次整屏，已修好（lock-styles-latest 断言画 1 次、整换 1 次）。
- **刚读完书马上锁屏、读书的保存还在后台排队**：先出旧画面整屏 1 次，保存完以后只把变了的矩形刷 1 次。
- **锁着时分钟变了**：
  - 数字时钟：局部 1 次，只刷数字。
  - 票根、拼贴、老黄历、万年历：0 次。
  - 迷你书库和空间关着的画面：局部 1 次，只刷时间，不碰卡。
  - 同一分钟：0 次。
- **跨日零点**：1 次，只交变了的矩形。
- **迷你书库点旁边一本**：局部 1 次（复核后不再重扫卡）；点中间那本就解锁，按解锁规则走。
- **选择页**：选一个样式 1 次（整页重画）；翻页 1 次。

## 没做的
1. Mogo 的 3 个界面宿主测试没接进 ctest：`calendar_ui_host_test.py`、`ticket_background_ui_host_test.py`、`mini_library_ui_host_test.py`。它们要 Mogo 整套界面测试工具，约 600 KB 原文件。这三张画面由模拟器截图和快照断言覆盖。
2. 时钟没校准、日期不在范围的整机测试：模拟器开机时间总是有效，造不出来。
3. 选择页的真实缩略图：预算不够，没做。
4. 书摘：我们的阅读还没调 `book_excerpt_observe`，所以票根一直没有书摘。交给 reader-2。
5. “今日阅读”：阅读还没调 `book_ticket_record`，票面写“今日阅读 —  ·  日期未同步”。

## 和设计不一致的
1. **画面缩小了**：Mogo 的锁屏是整页画面，没有状态栏和按钮。我们要放紧急呼叫（D-119）和相机（D-121），所以整张画面缩到状态栏下面，约 91%。新样式底部不放“已锁定……”那行提示。
2. **票根没有“透明”背景**（D-135），换成“壁纸”背景。
3. **保留了 Mogo 的画面装饰**：老黄历、万年历的框、圆印、题签、山水，还有画面上的 “mogo” 字样，都照原样留着。
4. **抖动用了两套**：票根、拼贴的封面用 Mogo 的 16 级抖动；壁纸用我们的，按面板实测亮度。
5. **选择页的快照**：选中的样式是一个字段，看得见的行和翻页是动作；每行的说明文字不进快照。
6. **迷你书库在锁屏上**：不放动画，不放“换一批”。点旁边的书只换选中，点中间那本才解锁打开。
7. **空书架**：没有 /me/books 文件夹时当作空书架。
8. **缓存在卡上看得见**：缓存放在 /me/mogo/locks/collage.bin 和 /me/mogo/covers/，在文件页里看得见。
9. **头文件包含方式**：胶水用尖括号包含 `<lock_faces/faces.h>`、`<ink/sys/almanac.h>`、`<ink/sys/image.h>`。

## 壁纸两套画法对比（只留一套）
用同一张图对比了照片、线稿、带透明的 PNG 三种。Mogo 按 16 等分取灰，不按这块面板的亮度曲线，中间调发白，浅色几乎没了。我们按面板实测亮度抖动，层次对，缺点是深色大块上有一点纹理。结论：留我们的画法，Mogo 的 `ui_wallpaper.c` 只拷来不编译。

## 改过的已有测试
1. `tests/lock_faces.c`（第一轮）：选择页从两列格子改成分页列表，按格子找样式的位置跟着改了；“待办 / 继续读”两条检查限定在旧样式上。断言内容和时限没改。
2. `tests/settings_b.c`（第一轮）：第 0 项名字从“这一页”改成“时钟和通知”。
3. `tests/lock_styles.c`（复核）：
   - 把 `read_some()` 拆成 `read_book(path)` 加 `read_some()`，原来的用例行为不变。
   - refresh 用例末尾新增迷你书库三条检查：分钟不扫卡、只刷时间、读过书后重新载入。原有断言一条没动。
   - 新增 sleep、latest 两个用例。
4. `tests/lock_styles_reverse.py`（复核）：
   - privacy-draw 的查找文字改成胶水的新写法（要检查的规则不变：关着也画书，测试必须失败）。
   - 新增 sleep、latest、library 三个反向对照。
   - 整个测试的时限从 1500 s 改到 2400 s，因为多了三次重新链接加运行；实际跑了 20 s。
5. `CMakeLists.txt`：lock-styles 的用例列表加 sleep、latest。

## 新加的测试
- `lock-styles-styles/almanac/refresh/privacy/entry/upgrade/empty/nofont`。
- 复核新加：`lock-styles-sleep`、`lock-styles-latest`。
- `lock-styles-reverse`：7 个反向对照。
- `mogo-lock-host-*`（8 个）。
- `mogo-lock-provenance`。

## 新增的机制
- **锁屏样式客户登记口**：`ink_lock_client_t` 加 `ink_lock_face_register()`，实现在 `drawn.c`。复核加了 `ink_lock_face_busy()`：后台还在画或在写缓存时，芯片不睡。`lock_mark.c` 在 `ink_ui_busy` 后面登记它，只加了 1 行。
- **拼贴缓存**：`/me/mogo/locks/collage.bin`；封面缓存 `/me/mogo/covers/`。
- **两项设置**：`display.lock_ticket_bg`（票根背景）、`display.lock_library`（迷你书库显示）。
- **`shell.lock_faces` 加了一个可选参数** `{"page":n}`。

## 给 reader-2 的接口说明
- `book_ticket_record(seconds, turns)`：阅读在翻页和退出时调它，票根才有“今日阅读”。
- **最近一本书**：锁屏不再写 Mogo 的 `last`。`lock_port.h` 把这批文件里的 `book_progress_last_path` 换成每次按进度保存的先后去算。reader-2 开书时照 Mogo 调 `book_progress_set_last_path` 也没关系。但“刚打开、还没存过进度”的书不算最近一本；要算的话，改 `lock_port.c` 的 `ink_mogo_lock_last_path` 这一处。
- `book_excerpt_observe` / `book_excerpt_flush` 和 `book_index_cache_*`：接上以后票根才有书摘。
- **新加进 third_party/mogo/main/book/ 的阅读数据模块**：book_cover.c、book_cover_auto.c/.h、book_excerpt.c/.h、book_title.c/.h、book_metadata.c/.h。合并时要和 reader-2 去重。
- **`mogo.open` 改名**：只改 `apps/mogo_lock/mogo_lock.c` 的 `MOGO_LOCK_OPEN_CAP` 一处。
- **`apps/mogo/port/runtime.c` 追加了 `ink_mogo_dimensions_get()`**。

## 给 settings-2 的
`settings_body.c` 改了两行：删掉 `LOCK_CHOICES`，直接用 `ink_lock_face_choices`。设置搜索请加这些别名：票根、拼贴、数字时钟、老黄历、万年历、迷你书库、票根背景。

## 要用户定
1. 新样式整张画面缩到状态栏下面（约 91%），紧急呼叫和相机挪进状态栏，底部提示不放。这样可以吗？
2. “数字时钟”这个名字，和第 0 项叫“时钟和通知”，可以吗？
3. 家人空间关着时，读书样式要不要也显示读书内容？
4. 迷你书库要不要也放到主屏？
5. `book_lock_bookmark.c` 和 D-135 冲突，要不要做？
6. “月历”（旧 5）和“万年历”（新 10）重复，要不要合并？
7. Mogo 画面里自带的 “mogo” 字样要不要去掉？
8. 票面上“今日阅读 — · 日期未同步”看着像时间没对，能不能先接受到 reader-2 接上为止？
9. 壁纸：要不要把我们的抖动挪到缩放之后，去掉深色大块上的纹理？
10. 代码行数超预算：胶水约 706/500，系统侧约 535/350；lock_mark.c 一共改了 40 行。
11. 内部 RAM 多 80 B，来自 Mogo 原文件里的小变量。

## 上机核对清单
- 面板上分钟局部刷新是不是只动数字那块；灰阶封面有没有残影。
- 每分钟唤醒重画的待机耗电。复核后迷你书库每分钟不再扫卡，耗电应该比第一轮低。
- **锁屏后芯片要等样式画完才睡**：数字时钟的分钟应该在唤醒那一下就对。看 `power.lock_to_sleep_ms` 和锁屏后第一分钟。
- 卡上的字体在不在；没有时默认锁屏底部是不是写了原因。
- BG 执行器 8 KB 栈够不够：真机用 -fstack-usage 再看一遍。
- PSRAM 峰值约 2.5 MB。

## 复核修复

两位复核人提了四个问题。四个都核实是真的：三个代码问题修了，对照表补了。

### 1. 芯片在后台还在画锁屏样式时就睡了（major）——真的，已修
- **核实**：读代码确认了。`start()` 把画面任务发到 BG 执行器，但电源任务睡前的检查里没有人看 BG。S31 上睡眠会把后台任务冻住，一冻就是 60 秒。结果是默认锁屏或上一分钟的时间要留到下一次唤醒。新测试用 4 秒卡住 BG 来模拟慢的封面解码：没修时锁屏 1579 ms 就睡了，70 秒后样式还没出来。
- **修法**：`drawn.c` 新增 `ink_lock_face_busy()`。画面、拼贴缓存、复查三种后台任务没做完时，它报“忙”。`lock_mark.c` 在 `ink_ui_busy` 后面登记它；这个顺序保证 UI 刚发起的画面不会漏掉。三个标志放在 PSRAM，S31 内部 RAM 回到 177,928 B。
- **测试**：新用例 `lock-styles-sleep`：断言第一次睡眠在锁屏 4000 ms 以后，而且样式已经在屏上。反向对照 sleep 去掉这个检查，测试失败（1579 ms）。

### 2. 票根第一次以后总是同一本书（major）——真的，已修，还多查出一个原因
- **核实**：没错。胶水只在没存过 `last` 时写一次，我们的阅读又从来不写它，所以票根永远停在第一本。
- **修法**：`lock_port.h` 把这批文件里的 `book_progress_last_path` 换成 `ink_mogo_lock_last_path`。它每次按进度保存的先后找最近一本，什么都不写；一条进度都没有时才读存着的 `last`。原文不改，做法已写进 SOURCE.md。旧的 `last_book()` 删掉了。
- **多查出的原因**：去掉那次写入后，第一次锁票根还是画 2 遍、整换 2 次。加日志查到：版本号是在 UI 发任务时取的，而阅读关闭时的保存在 BG 上排在前面，画的时候版本已经变了。
  - 改法：版本号改在 BG 真正开始画时取。
  - 另一个口子：锁屏时直接用旧画布、保存还在排队的话，旧画面会留到下一分钟。现在锁屏后等 BG 把前面的活做完，再看一眼版本，变了才重画，只刷变了的矩形。
- **测试**：新用例 `lock-styles-latest`：
  - 读 epub，锁屏是 epub，第一次锁屏画 1 次、整换 1 次；
  - 读 GBK.txt，锁屏变 GBK；
  - 再读 epub，变回 epub。
  反向对照 latest 恢复成“存一次就不变”，测试失败（仍然 2 次，也不跟着换书）。

### 3. 迷你书库每次画都重扫整个书架（major）——真的，已修
- **核实**：没错。`library_load()` 每次都清掉再扫，分钟重画、点选、拼贴的快照都会扫一遍卡。Mogo 自己的规矩是“静止不扫描”（`MINI_LIBRARY.md:39`；`home_library_host_test.py:109`：离开再进入不重扫），胶水把它破坏了。
- **修法**：Mogo 的书目和封面在两次画之间留着。只有换家人、读过书（进度版本）、换“最近/随机”、卡的版本变了时才重扫。卡的版本在载入之后取，免得封面缓存的写入引起第二次重扫；版本是 0（正在写）时当作没变。分钟和点选只用留着的结果，不碰卡。重扫失败时，同一位家人接着用上次的书，不在默认锁屏和样式之间来回换，合 Mogo 的“保留上次完整书匣”（`MINI_LIBRARY.md:12`）。第一次就失败才退回默认锁屏，并写原因。
- **测试**：`lock-styles-refresh` 加了三条：
  - 书库的分钟 `mogo_lock.library_loads` 不增加；
  - 分钟只刷 y 150..300 那块，1～2 次更新；
  - 读过另一本书以后不用等分钟就重新载入，并显示那本书。
  反向对照 library 改成每次都扫，refresh 失败。扫描失败的路径在模拟器里造不出来，没有整机测试（对照表第 28 行如实写了）。

### 4. 没有 D-140 对照表（blocker）——真的，已补
- **核实**：`git ls-files | grep 对照表` 只有 almanac-2 的那份。
- **处理**：写了 `docs/tasks/lock-styles-1/对照表.md`，单独提交（adac18c2）。
  - 37 行：Mogo 的 11 页、13 个操作、13 种状态和规矩各一行，每行写照做 / 按设备改（怎么改、为什么）/ 不做。
  - 31 行配了截图或整机测试；第 28、31 行只有 Mogo 的宿主测试；第 30、32 行没有测试；第 33 行不适用；第 9 行（书签锁屏）没做，等用户定。
  - 表头写明了：这张表在代码之后补写，D-140 要的“先写后做”这一步漏了，补不回来。

### 测试结果
- **全量 ctest**（清空 test-out、重新配置后跑，-j2），原样一行：`100% tests passed out of 487`（共 488 个，`memo-save-busy` 本来就是 Disabled，这次没动它）。总耗时 777.52 s。
- 全量跑完以后，又把三个标志挪进了 PSRAM（只换了存放位置）。之后重跑了相关的 30 个（lock-*、mogo-lock*、scenario-standby、ui-scale、sdk-boundary）：`100% tests passed out of 30`。
- `lock-styles-reverse` 的 7 个反向对照都按要求失败：refresh、almanac、privacy-key、privacy-draw、latest、library、sleep。
- gcccheck：drawn.c、lock_mark.c、mogo_lock.c、lock_port.c、tests/lock_styles.c 共 5 个，0 发现；挪进 PSRAM 后 drawn.c 再查一次，0 发现。

### S31
- `ports/esp32s31/build.sh` 通过，没有新警告。日志里有 “factory screen output layer @690a023”，没有“没有屏幕输出层”。
- `ink-s31.bin` 4,685,840 B（0x478010），比复核前的 4,685,232 B 多 608 B；比任务开始前一共多 139,376 B（约 136.1 KB）。
- app 分区 5,242,880 B，剩 557,040 B（11%）。
- 内部 RAM（DIRAM）177,928 B，和复核前一样。
- build-s31-own 已删除；build-asan 本来就不在（这次没跑 ASan）。

### 行数变化
- 胶水：mogo_lock.c 260→265，lock_port.c 250→270，lock_port.h 34→37。
- 系统侧：drawn.c 264→300，faces.h +3，lock_mark.c +1。

### 提交
- adac18c2 对照表
- 76ed2d7d 三个修复和测试
- 7b86f3a2 标志放进 PSRAM

最后一次提交：`7b86f3a2`（分支 worktree-wf_fa602b56-3c7-29）。

### 相关文件
- .claude/worktrees/wf_fa602b56-3c7-29/system/ui/lock_faces/drawn.c
- .claude/worktrees/wf_fa602b56-3c7-29/system/ui/lock_faces/faces.h
- .claude/worktrees/wf_fa602b56-3c7-29/system/ui/lock_mark.c
- .claude/worktrees/wf_fa602b56-3c7-29/apps/mogo_lock/mogo_lock.c
- .claude/worktrees/wf_fa602b56-3c7-29/apps/mogo_lock/port/lock_port.c
- .claude/worktrees/wf_fa602b56-3c7-29/apps/mogo_lock/port/lock_port.h
- .claude/worktrees/wf_fa602b56-3c7-29/tests/lock_styles.c
- .claude/worktrees/wf_fa602b56-3c7-29/tests/lock_styles_reverse.py
- .claude/worktrees/wf_fa602b56-3c7-29/third_party/mogo-lock/SOURCE.md
- .claude/worktrees/wf_fa602b56-3c7-29/docs/tasks/lock-styles-1/对照表.md
