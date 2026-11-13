# gallery-3：相册照 Aves 和 Fossify Gallery 重写，AgentUI 图库代码真搬——按日期和相簿找图、单张连续看、看信息；放大看字、照片方向自动摆正、误删能从“最近删除”找回

总规则先读 `docs/tasks/codex-rules.md`，第 9、10、11 节优先。这个任务不碰硬件，图片解码照旧用我们的媒体能力。再读 `docs/tasks/vendor-common.md` 第 11 条。开工前看一遍 `docs/tasks/` 里已有的 `camera-3`、`files-3` 任务文件，以及主线上 `reader-one-1`、`auto-provision-1`、`settings-d137`、`camera-touch-bus` 合入的改动（这四个现在没有任务文件，以主线实际改动为准）；碰到同一个文件只追加，别改别人的行。

**D-140（总规则第 11 节）**：本任务是“照着重写，片段可搬”。

1. **先写对照表，单独提交**：`docs/tasks/gallery-3/对照表.md`，草稿已附在本文件后面。每页、每个操作、每种状态一行，写明照做、按设备改（怎么改、为什么）或不做（为什么）；先提交，再写代码。
2. **AgentUI 必须拷原文**：拷哪些函数、怎么编译，见下面“搬代码”一节。`SOURCE.md` 记项目、固定提交、拷了哪些、改了哪些行、为什么。
3. **其余照对照表重写**。参照安卓的地方按第 11 节为这台设备改：墨水屏、灰阶、触摸、刷新、内存、家人和孩子。
4. **做完逐行验收**：每行配一张截图或一个测试名。

## 为什么

- 用户 2026-11-12：“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”（D-129）；又说要把现有应用“移植或参考重写搞到成熟可用”（D-131）。
- 用户 2026-11-13：“对 按着实现 参考安卓也行 但是要注意设备特性 不要生搬硬套导致使用体验反而不佳”（D-140）。
- 相册在第一优先的现有应用里，计划进试用版 22（`docs/tasks/delivery-plan.md` 第 74 行）。
- D-135：重写的系统应用直接替换旧的，不并存。
- D-138：应用要的东西系统自动准备好，缺什么在屏幕上说清楚，不能白页。

调研（`docs/tasks/replan-sysapps/APPS.md` A08、`CURRENT.md` A08）：

- **现在有的**：3 列 9 格的分页缩略图、单张、删除文件、问 AI。
- **缺的**：放大和平移、旋转、按日期和相簿整理、照片信息、能恢复的删除，照片之间也不能连着看。
- **参照路径**：按日期找到图 → 放大看字 → 回到原位置 → 误删恢复；“问这张图”始终绑定当前家人和文件版本。

## 现状（2026-11-13 按 `claude/system-layer` 的 16e7be5f 读过，行号都核对过）

`system/apps/gallery/gallery.c` 273 行。上次 brief 之后没人改过它（最后一次是 e6c19491）；design-lang-2、home-1、lock-1、control-1、settings-1 都没碰相册。现有能力只有 `gallery.reload`、`gallery.prev`、`gallery.next`、`photo.open`、`photo.ask` 五个；`photo.next`、`photo.prev`、`photo.restore` 等仓库里都还没有。

**列表和单张**

- 列照片：`read_photos`（117-142 行）按索引顺序拼起 `/me` 和 `/family/锁屏拍的照片` 的图片编号，不排序，没有日期也没有相簿。相机把照片存到 `/me/照片`，锁屏时存到 `/family/锁屏拍的照片`（`camera.c:41`、`:87`）。
- 文件时间：`ink_fs_entry_t`（`system/include/ink/sys/file.h:62`）里没有。索引记录里本来就有：`file_service.c:1460` 的 `index_rec_t.mtime`，单位是**纳秒**（`platform/include/ink/hal_storage.h:52`、`:59`）。S31 上是 `st_mtime × 10⁹`（`ports/esp32s31/hal_storage_s31.c:194`、`:228`），模拟器用宿主机的纳秒（`ports/sim/hal/hal_storage_sim.c:606`、`:615`）。
- **要改的那一行**：把条目填给调用方的是 `index_query`（1870 行起），具体在 1896 行 `out->id = rec.id; out->size = st.size;`。旧 brief 写的 `fs_index_for`（1901 行）只是加锁的外壳。
- 单张：`ink_photo_image`（20-32 行）只把图缩到适合屏幕，不能放大、平移、旋转。整图先抖动再交给 LVGL 按最近点缩放（`lv_image_set_antialias(false)`）。看完一张只能返回。相机还在用这个函数（`camera.c:11`、`:46`）。
- 缩略图边长只有 `INK_THUMB_EDGE` 128，放大用不上。文件服务的缩略图缓存只有 4 格（`image.h:12`），一页却有 9 格，翻回上一页要重新解码。这个数不改：`system/ui/file_picker.c:18-20` 的数组和 `tests/services_finish/thumb_wiring.c:123-126` 都依赖它。

**删除、按钮、刷新**

- 删除直接走 `file.delete`（105、174 行），找不回来，要弹确认卡（`file_caps.c:508`，“删除 {path}，不能撤销”）。
- 现成的撤销机制：`file.move` 能撤销（`file_caps.c:323`）；`ink_cap_undoable` 加 `system.undo`（`system/include/ink/sys/cap.h:183-185`）。
- 移动和建目录：`ink_fs_move_if_revision`（`file.h:86`）。`ink_fs_mkdir` 会**自己建缺的上级目录**（`file.h:93`），旧 brief 写的“逐层建”不需要。
- `file.delete_many` 的 `all_path` 只删这个目录下直接的文件，不删子目录（`file_caps.c:495`）。
- “重读照片”大按钮一直在底部（167 行）。
- 右上角的拍照（161 行）、删除（177 行）用的是 `ink_ui_circle`。design-lang-2 以后它只画一个图标、不画圆了（`system/ui/theme.c:333-344`），问题变成**只有图标、没有字**。
- 任何 `file.changed` 都整页重画（`photos_changed`，261-269 行）；codex-rules 第 8 节说过这会丢掉排着队的点按。

**解码上限和格式**

- 解码上限 4 MB、100 万像素（`image.h:7-9`，检查在 `image.c:29-33`）。手机拍的大照片缩略图和单张都出不来。
- 单张页遇到大于 4 MB 的文件根本不读（`gallery.c:132`），只写“暂时读不了照片”。
- stb 只编了 PNG 和 JPEG（`system/services/media/image_codec.c:5-6`），索引却把 `.bmp`、`.gif`、`.webp` 都算作图片（`file_service.c:1597`），这三种一律解不了。
- JPEG 的 EXIF 方向没人管，竖着拍的照片会躺着显示。

**索引、输入、图标**

- 整台设备的文件索引最多 1024 条，所有类型、所有家人加在一起（`file_service.c:129-130`）。满了返回 `INK_FS_ERR_INDEX_FULL`。
- 触摸：页面的 `on_swipe` 只给方向，不给位移（`system/include/ink/sys/ui.h:45`）。要按拖动距离平移，得用 `ink_ui_capture_touch`（`theme.c:482`）自己接 LVGL 的按下和松开。
- 图标：没有 `rotate`。`plus`、`minus`、`info`、`trash`、`undo`、`folder` 都有。`assets/icons/manifest.json` 有 `sizes`（34、43、53、77、106）、`names` 列表和 `files`（每个 SVG 的 SHA-256）三部分。

**刷新怎么数、C++ 怎么编**

- 刷新能从显示轨迹数出来：`tests/control_center.py` 的 `inside()` 读 GC16、GL16、DU、A2 事件，每条带 x、y、w、h。照它数，不用自己记 `ink_ui_hint_obj`。
- S31 上 `ink_system` 只自动收 `system/*.c`（`ports/esp32s31/components/ink_system/CMakeLists.txt:2`），`.cpp` 要在 `ports/esp32s31/main/CMakeLists.txt:32-35` 里显式加，五子棋就是这么做的。
- 翻译源文件清单有两份，新的带 `TR()` 的文件两边都要加：宿主机 `CMakeLists.txt:477`，S31 `components/ink_system/CMakeLists.txt:33`。

**测试直接用了 gallery.c 的原文**

- 直接 `#include` gallery.c 并读 `G` 的字段（count、slots、box、path、delete_args、message、photo、revision）：`tests/camera_gallery/camera_gallery.c:17`、`tests/ai_history_photo/test.c:5`、`tests/ui_fix_c/gallery.c:2`。
- 按原文字符串改代码做反向对照的：
  - `tests/ai_history_photo/run.py:35-39`：`photo_question_ready` 和 `photo.ask` 那几行；
  - `tests/camera_gallery/run.py:27-28`：`if (!found) c->rc.status = INK_RC_REJECTED;`；
  - `tests/open_app_latency/run.py:62-64`：`static void read_photos(void *arg) {` 和它结尾的 `ink_exec_post(...loaded...)`。
- 按 gallery.c 的原文或路径对照的：`tests/i18n_literals.py:47`（`/family/锁屏拍的照片` 的白名单）、`tests/audit3_fix.py:18`、`tests/small_gaps_2/reverse.py:63`、`tests/media/negative.py:54-55`（对照 `image.c` 的缓存淘汰那一行）。

## 来源

固定提交都在 `docs/tasks/replan-sysapps/SOURCES.md`（G01、F07、E01）。`work/` 里现在没有这三个仓库，执行者自己只读浅克隆到 `work/`，不进仓库。SOURCES.md 记的行号有几处对不上内容，下面是 2026-11-13 按固定提交核对过的（上游行号以执行者克隆后按内容再核一遍为准）；以内容为准，实际行号记进 `SOURCE.md`。

- **Aves**（BSD-3，只照着做）：`github.com/deckerst/aves`，提交 `9d3f1ff19f680db043c9d80831c6f222e0f8220c`，路径都在 `lib/` 下：
  - 集合和筛选：`widgets/collection/collection_page.dart`。回收站本身就是一个筛选（64 行）；从单张返回时定位（206-219 行）。
  - 空状态和加载中：`widgets/collection/collection_grid.dart:608-657`。
  - 默认值：`model/settings/defaults.dart`。按日期排、按月分节（60-61 行）；单张的快捷操作（85-90 行）；叠加层（91-95 行）。
  - 单张：`widgets/viewer/entry_viewer_page.dart:11-58`。横向换张在 `entry_horizontal_pager.dart`，竖向在 `entry_vertical_pager.dart`。
  - 信息页：`widgets/viewer/info/info_page.dart:1-127`；基本项和“未知”的写法在 `basic_section.dart:306-345`。
  - 回收站：留 30 天（`model/source/trash.dart:15`）；到期删除（22-41 行），在打开媒体库时调用（`model/source/media_store_source.dart:150-160`）；还剩几天（`model/entry/extensions/props.dart:99-103`）。
  - 文案：`l10n/app_en.arb` 第 194 行（Undated）、1024-1025 行（30 天）。
- **Fossify Gallery**（GPL-3，**只照着做，一行 Kotlin 也不拷**）：`github.com/FossifyOrg/Gallery`，提交 `2688360242468f1d591eddf23740c879616bb828`，路径都在 `app/src/main/kotlin/org/fossify/gallery/` 下：
  - EXIF 方向：`fragments/PhotoFragment.kt:398-414`（degreesForRotation、rotateViaMatrix）。223-238 行是 content URI 那条路。Fossify 把 2、4（镜像）当成不转，5、7 只转不镜像；我们 8 种都按标准做。
  - 双击放大：`PhotoFragment.kt:599-616` 在适合屏幕和 2 倍之间切换（第 106 行 `DEFAULT_DOUBLE_TAP_ZOOM = 2f`）。
  - 手势分工：`PhotoFragment.kt:185-195`，没放大时才把手势交给外层。
  - 旋转：`activities/ViewPagerActivity.kt:819-834`。
  - 删除前的确认：`ViewPagerActivity.kt:1163-1206`；删除后换到下一张：1208-1240 行。
  - 回收站：移进去时保留原路径（`extensions/Activity.kt:346-401`）；恢复，重名就换名（410-486 行，第 441 行 getAlternativeFile）；清空（488-500 行），清空前确认（508 行起）。
  - 30 天自动清：`activities/MainActivity.kt:1640-1658`，一天查一次，删 30 天以前的。
  - 移进回收站时的提示：`activities/MediaActivity.kt:1037-1072`（Toast）。
- **AgentUI**（MIT，**搬代码**）：`github.com/AlayaElla/MetalioClaw4-AgentUI`，提交 `172ec530ff65e11521585fe3b8611daa20a78aa4`，路径都在 `main/display/agent_ui/apps/camera/` 下。拷哪些见下一节；`camera_view.cc:44-89` 和 `camera_controller.cc:112-166` 看过，不搬，理由在对照表第 76、77 行。

## 搬代码（第 11 节）

**拷什么**：都是整函数，原文不改，只改下面列的几处。

1. `camera_gallery_repository.cc`（原文件 SHA-256 `0a8dcf664db73014902f12aa025630d1dc595e9c04c900ac006a3c5cd26bd29e`）：
   - 24-41 行 `IsJpg`；
   - 64-85 行 `FormatDate`；
   - 87-92 行 `FormatDayLabel`；
   - 94-99 行 `FormatTimeLabel`；
   - 101-112 行 `FormatSizeLabel`；
   - 154-158 行排序用的那个 lambda。
2. `camera_view.cc`（SHA-256 `859fd456ff060dc5037e9ea1a44fda43977f21fd196b219ffed39a4100c063f4`）：1109-1120 行按 `date_key` 把相邻照片分组的循环。

**放在哪、怎么编译**

- 原文放进 `system/apps/gallery/vendor/agentui/camera_gallery_repository_part.h`，顶上是 MIT 版权和许可原文。原文的 `namespace agent_ui::camera { namespace { … } }` 原样保留。这个 `.h` 只给下面的 shim 包含，所以匿名命名空间里的函数能直接调。
- shim 是 `system/apps/gallery/repository.cpp`（C++17）。它用 `extern "C"` 给 C 端几个函数：
  - `ink_gallery_sort(items, n)`：原地排序；
  - `ink_gallery_day_label(utc_s, out, cap)`；
  - `ink_gallery_date_key(utc_s, sep, out, cap)`；
  - `ink_gallery_time_label(utc_s, out, cap)`；
  - `ink_gallery_size_label(bytes, out, cap)`；
  - `ink_gallery_is_jpg(name)`；
  - `ink_gallery_month_groups(items, n, cb, ctx)`。
- 本地时间用我们的 `ink_time_to_local`（`time.h` 有 `extern "C"`，能在 C++ 里用）填 `std::tm`，不用原文的 `localtime_r`。`image.h` 没有 `extern "C"`，shim 不要包含它；要用就在 shim 里自己包一层。
- C 端的条目放在 PSRAM（`INK_PSRAM_BSS`），每项是 `{uint32_t id; int64_t modified; const char *name; uint16_t album; char month[8]; …}`。排序时 shim 把它看成 `std::string_view`，不分配内存。
- 宿主机 `CMakeLists.txt` 和 `ports/esp32s31/main/CMakeLists.txt`（照五子棋的写法）都在末尾加 `repository.cpp`。

**改了哪些行**（都记进 SOURCE.md）

1. lambda 的参数 `const GalleryPhoto&` 改成 `const auto&`：可以直接排 C 数组，不为 1024 张照片各建一个带 6 个 `std::string` 的 `GalleryPhoto`。
2. 分组循环包成模板函数，`state.gallery` 改成参数名 `gallery`。
3. `"%d月%d日"` 和 `"大小未知"` 包进 `TR()`，英文界面能翻译。
4. lambda 和分组循环原来在 `List()`、`RenderGalleryItems()` 里面，现在各包成一个命名函数，函数体原样。

**不搬的和证据**

- `List()` 的其余部分：118-123 行要 SdCardManager，并且只读 `/sdcard/DCIM/Camera`；130-132 行只认 JPG；139 行直接 `stat` 卡上的原始路径，绕过 `ink_fs` 的家人隔离。159 行和 `.h:16` 最多 48 张。
- `Delete`：直接删文件。
- `WriteJpeg`：相机的事。
- 视图和控制器（`camera_view.cc:1045-1057`、`929-936`，`camera_controller.cc:112-197`）：都是类的成员，离不开 `Theme::Get()`、`ui_components`、`ViewState`、`Dispatch`，只照着做。

**SOURCE.md 写什么**：项目链接、固定提交、查阅日期、许可（MIT 版权行原文）、上面每个函数的原行号、原文件的 SHA-256、改过的 4 处和原因、没搬的和证据。

**测试**：这个提交没有测试文件（查过 `git ls-files`）。我们在 `tests/gallery_3/` 写宿主机测试，从 C 通过 shim 调每个搬来的函数：日期边界、跨年、时间相同的照片排序、0 字节、1 MB 上下的大小。

Aves 和 Fossify 不拷代码；SOURCE.md 里给对照表的每一行写参照的文件和行号。

## 要做的（逐项对应对照表）

### 1. 相册首页：找图，照 Aves 的集合页（对照表第 1-24 行）

- 标题“相册”；右上角一个写着“拍照”的文字按钮（`app.open camera`）。不用图标按钮。
- 下面一个分段控件“全部 · 相簿 · 日期”（`ink_ui_segments`）。

**全部**
- 3 列网格分页（沿用 `ink_ui_list`），新的在前。按文件时间排，用 AgentUI 搬来的比较函数，不靠目录顺序。
- 网格上方一行灰字写这一页是哪个月：“2026年11月”；一页跨两个月就写“11月—10月”。

**相簿**
- 每个有照片的文件夹一行（`ink_ui_list_row`）：名字、张数、最新一张的日期。不画封面。
- “锁屏拍的照片”照旧算一个相簿。
- 最后一行是“最近删除 · n 张”，没有就不画。点一行进入这个相簿的网格。

**日期**
- 每个月一行（“2026年11月 · 23 张”），新的在前，行用搬来的分组循环分出来。
- 文件时间读不到或早于 2020 年的（设备没校时的时候拍的），放进最后一行“日期不详”。

**筛选以后**：网格上方写“相簿：照片 · 23 张”或“2026年9月 · 9 张”，旁边一个“清除”文字按钮。

**缩略图**
- 只为这一页看得见的格子请求；回来的先放着，这一页都到了（或出错）再一起画、一起刷。
- 1.5 秒还没齐就先画已到的，剩下的到齐再刷一次。测试用可控时钟或注入定时器，不靠真实等待，也不改已有测试的等待时间。

**文件时间**
- 在 `ink_fs_entry_t` 末尾**增量**加 `int64_t mtime`，单位纳秒（和 HAL 一样），-1 表示不知道，头文件注释写清。已有字段的顺序不动。
- 在 `index_query`（`file_service.c` 1896 行那一行）从 `e.mtime` 填进去。
- 不另建索引，也不另存时间。

**扫索引**：扫完整个索引，在 PSRAM 里留最新的 `INK_FS_INDEX_MAX`（1024）张。

### 2. 单张：照 Aves 的单张页和 Fossify 的 PhotoFragment（对照表第 25-40 行）

**连着看**
- 没放大时，左右滑、底栏的“上一张”“下一张”文字按钮、盖板的上一页和下一页键、`photo.next`、`photo.prev` 都换到同一筛选里的上一张、下一张。滑动只是快捷方式，不是唯一入口。
- 标题下一行写“名字 · 第 i/n 张 · 11月13日”。不是今年的，日期前加年份。
- 换张时，新的一张解码完才刷，标题行和照片区一起刷一次，不先清空。

**照片区自己渲染**
- 留一份**没抖动**的灰度原图（≤100 万像素，PSRAM）。每次缩放、平移、旋转，只把看得见的那块裁出来、缩放好，放进一块和照片区一样大的缓冲，再调 `ink_image_dither`。先抖动再放大会把抖动的格子放大成方块。
- `photo.h` 里 `ink_photo_image` 的签名和行为不变（相机在用，`camera-3` 只读着用）。
- 报告写原图加照片区缓冲在 4 倍时的内存数。

**放大**
- 双击放大到 2 倍，以点的位置为中心；再双击回到适合屏幕。双击靠两次点按的间隔判断，输入服务不改；间隔是【估算】，列进上机先核对清单。第一次点按不刷屏，**任何缩放下单击都不做别的事**（所以放大后没有“点一下移到中间”）。
- 底栏的“放大”“缩小”在适合屏幕、2 倍、4 倍之间切换。
- 触摸只报一个点（`input.h:35-45`），不做双指捏合，写进“没做到的”。

**平移**
- 放大以后，照片区用 `ink_ui_capture_touch` 自己接按下和松开，按位移平移；拖动过程中不刷，松手后照片区刷一次。
- 放大时底栏多一行“左移”“右移”“上移”“下移”文字按钮（每个不小于 108 px），每按一次挪半屏，刷一次；`photo.pan` 同一个入口。拖动只是快捷方式。底栏随放大状态变化时，和照片区合成一次刷。
- 翻页键先把这张从上到下看完（每次挪半屏），到底再换下一张。
- 没放大时左右滑是换张（Fossify 185-195 行的分工）。

**方向**
- 在 `ink_image_decode`（`system/services/media/image.c`）里统一处理 JPEG 的 EXIF 方向：读 APP1 里的 0x0112 标签，8 种取值都按标准转，包括镜像。
- 先转灰度，再转方向写进新的缓冲，然后放掉 stb 的 RGBA 缓冲。峰值会比现在多一块灰度缓冲（最多约 1 MB），报告写出具体数字。
- 这样缩略图、壁纸、AI 拿到的缩略图都对了，PNG 不受影响。
- EXIF 损坏或越界时当作没有方向，不崩。
- `image.c` 里的缩略图淘汰那一行（`tests/media/negative.py:54` 对照它）不动。

**旋转**：每按一次顺时针转 90 度（`photo.rotate {dir}`），只改这次怎么看，不改文件，写进报告。不做屏幕自动转向（D-122）。

**底栏**
- 写字的工具按钮，图标加字，每个不小于 108 px：上一张、下一张（没有图标可以只写字）、放大（`plus`）、缩小（`minus`）、旋转（新加的 `rotate`）、信息（`info`）、删除（`trash`）。放不下就排两行，不缩字。
- “问 AI 这张图”保留，是次要按钮，不用黑底，不和“删除”挨着。
- `rotate` 图标照 `tools/icongen/README.md` 里 `cloudsun`、`rain` 的先例手画 line、solid、duo 三种，线宽照现有图标。在 `manifest.json` 的 `names` 末尾加 `rotate`，`files` 末尾加三项和它们的 SHA-256；README 记一笔；图标相关测试要全过。

**看不了的照片**
- 大于 4 MB 或超过 100 万像素：只读文件头（最多 128 KB）取宽高，写“这张照片太大（宽×高），设备只能看 100 万像素以内的”。
- WebP、GIF、BMP：写“这种格式（WebP）还看不了”。
- 读不出来：写“这张照片读不出来，文件可能坏了”。

**回到原位置**：返回网格时，停在最后看的那张所在的页，筛选不变，那一格加细边框，下一次操作时去掉。

### 3. 信息页：照 Aves 的信息页（对照表第 41-48 行）

- “信息”打开一页，列这些：名字、相簿（所在文件夹）、拍摄时间、尺寸（摆正以后的宽×高）、大小（搬来的 `FormatSizeLabel`）、格式（扩展名用搬来的 `IsJpg` 判断，再看文件头确认）、第几张。
- **拍摄时间**：JPEG 有 EXIF 拍摄时间（DateTimeOriginal）就用它；没有就写文件时间，并标明是“文件时间”。时刻用 `FormatTimeLabel`。
- **没有的项**照 Aves 的写法：名字、时间、大小写“不详”；尺寸读不到就不画那一行；不留空行。
- **读文件头的接口**：在 `image.h` 里**增量**加一个只读文件头的 `ink_image_probe(data, size, &out)`，给出格式（按魔数判断）、摆正以后的宽高、EXIF 方向、EXIF 拍摄时间。它和解码共用同一段 EXIF 读取代码；只需要前 128 KB。
- 文件头读失败时，信息页照样画能知道的项（名字、相簿、大小、文件时间），尺寸和拍摄时间不画。
- 位置信息不读不显示（没有地图），写进“没做到的”。

### 4. 最近删除：照 Fossify 和 Aves 的回收站（对照表第 49-61 行）

**删除**
- 单张页的“删除”改成 `photo.trash`，只处理当前显示的那张。
- 把照片挪进同一个空间里的“最近删除”：`/me/最近删除/<删除那天>/<原来的相对路径>`。日期文件夹名用搬来的 `FormatDate`，例如 `2026-11-13`。`/family` 下的照片同理，放进 `/family/最近删除/…`。
- 先用 `ink_fs_lookup` 核对文件的路径、大小、时间和显示的那张一样，再取 `ink_fs_revision()`，用 `ink_fs_move_if_revision` 挪。显示的这张已经变了就拒绝（STALE），页内写“这张照片刚刚变了，请重新打开”；挪不动（卡满、读写出错）写原因，照片留在原处。
- 父目录用一次 `ink_fs_mkdir` 建好，第一次用时自动建（D-138）。
- 照片找得回来，所以不弹确认（D-105）。挪完换到下一张，状态行写“已移到最近删除”，旁边一个“撤销”。撤销就是 `photo.restore`，同时用 `ink_cap_undoable` 登记，AI 用 `system.undo` 也能撤。状态行留到下一次操作时一起消失。
- 原来在哪就藏在路径里，不需要新的存储格式。
- 路径超过 `INK_FS_PATH_MAX`（128 字节，中文一字 3 字节，前缀 `/me/最近删除/2026-11-13/` 占 28 字节）放不进去时，写清原因，只给永久删除。
- `最近删除`、`日期不详` 这两个文件夹名追加进 `tests/i18n_literals.py` 的白名单，写清理由（和 `/family/锁屏拍的照片` 一样，是卡上的固定目录）。

**最近删除页**（从“相簿”最后一行进）
- 每张一行：名字、哪天删的、还剩几天；分页。顶上一行写“照片在这里留 30 天，之后自动删除”。空了就用共用空状态写“最近删除里没有照片”。
- 这一页每行只有“恢复”和“永久删除”，不开单张预览（列进“要用户定”）。
- **恢复**：`photo.restore {path}`，挪回原路径。原路径已经有同名文件，就在扩展名前加“（恢复）”“（恢复2）”。恢复失败写原因，照片留在最近删除里。
- **永久删除**：新能力 `photo.delete_forever {path}`，确认卡写“永久删除这张照片（名字），不能恢复”。
- **清空**：新能力 `gallery.empty_trash`，确认卡写“清空最近删除里的全部照片，不能恢复”，按钮旁边写张数；整棵树用 `ink_fs_delete_start` 删。
- 不再用 `file.delete`、`file.delete_many`，因为：`file.delete` 的确认卡会把原始路径显示出来（“删除 /me/最近删除/2026-11-13/照片/a.png，不能撤销”）；`delete_many` 的 `all_path` 不删子目录（`file_caps.c:495`）。

**30 天自动清空**
- 打开相册时查一次，不轮询。
- 只有系统时间可信（`ink_time_valid()`）才查。删除日期早于 30 天的日期文件夹，用现有的后台删除 `ink_fs_delete_start` 一个一个清；有任务在跑（BUSY）就等下次打开。
- 时间不可信时什么都不清。删的时候时间不可信，照片就放进“日期不详”文件夹，永远不自动清。

**不出现在别处**：“全部”“相簿”“日期”都不算最近删除里的照片。文件管理和 AI 的 `file.index` 还看得到这个文件夹，`files-3` 的文件页也会把它当普通文件夹显示、允许对里面的文件 `file.delete`；这次不改文件服务的这部分，写进报告。

### 5. 读不了、看不了时说清楚（D-138，不留白页、不留白格）

**整个相册读不了**，用共用空状态 `ink_ui_empty`，每种一句话：
- 没插卡：`INK_FS_ERR_NO_CARD`；
- 卡正借给电脑：`INK_FS_ERR_USB`，写“拔线或在电脑上推出后自动回来”；
- 索引还没建好：`file.index_state` 不是 ready，写“正在整理照片，好了会自动出来”，不转圈；
- 索引记满了：`INK_FS_ERR_INDEX_FULL`，写“文件太多，有些照片没列出来”。

“重读照片”只在读不了时作为空状态的动作出现，平时不出现。卡回来或索引建好后自动重读（订阅状态变化，不轮询）。

**某一张看不了**：网格里那一格画图标加“看不了预览”；单张页写的话见第 2 项。

**文件变了**：只在影响当前列表时才重画受影响的区域，也就是变的是图片、而且落在当前筛选或最近删除里。传进来一本书不重画相册。自己挪进最近删除引起的 `file.changed`，不能把单张页弹回去。

### 6. 能力和快照

**现有能力名照旧**：`gallery.reload`、`gallery.prev`、`gallery.next`、`photo.open {id}`、`photo.ask`。`photo.ask` 的版本检查（e6c19491 的重试和 STALE）一字不改。

**12 个新能力**（名字可以调，调了列进报告；仓库里原来都没有）：
- 浏览：`gallery.show {view: all|albums|months|trash}`、`gallery.find {album | month}`（月份写 `YYYY-MM` 或 `undated`，都不给就是清除筛选）、`photo.next`、`photo.prev`、`photo.info`；
- 缩放和方向：`photo.zoom {step: in|out|fit}`、`photo.pan {dir: left|right|up|down}`（一次挪半屏）、`photo.rotate {dir: left|right}`；
- 删除和恢复：`photo.trash`、`photo.restore {path}`（新能力，用路径，因为挪动以后编号不一定还是原来的；要在报告里写明）；
- 不能恢复的两个：`photo.delete_forever {path}` 和 `gallery.empty_trash`。它们带 `.ask` 文案和 `ask_by_hand`，效果标 `INK_FX_LOCAL_DESTRUCTIVE`，`child_ok` 跟 `file.delete` 一样。

**两条规矩**
- 按钮和 AI 调同一个能力。
- 新能力的 `child_ok` 和 `locked_owner_only` 跟 `photo.open` 一样（锁屏时 AI、远程和应用调用要等解锁；锁屏相机只能看这次拍的，D-121）。

**快照写清看到的是什么**
- 首页：当前分段、筛选、这一页的月份、每张看得见的照片的名字和日期，“重读”只在读不了时出现；
- 单张：名字、相簿、日期、第几张、缩放（适合屏幕、2 倍、4 倍）、转了多少度、能做的动作；
- 信息页：每一项；
- 最近删除：每张的名字、删除日期、还剩几天。

### 7. 原地重写

- 旧页面代码删掉，不留两套。
- 可以拆成网格和相簿、单张和信息、最近删除几个文件，但**这几段原样留在 `gallery.c`**：
  - `photo_question_ready` 和 `gallery_action` 里 `photo.ask`、`photo.open` 的分支；
  - `photo_found` 里 `if (!found) c->rc.status = INK_RC_REJECTED;` 那一行；
  - `read_photos` 的开头和结尾那两行。

  `ai-history-photo`、`camera-gallery`、`open-app-latency` 的反向对照按这些原文找代码（见“现状”最后一段）。实在要动，就同步改那几个脚本、保持一样严，并在报告里逐条列出。
- 拆出去的文件要让直接 `#include gallery.c` 的三个测试还能编译链接。

## 共同要求（页面任务都一样）

**来源**
- 功能、页面、状态、出错处理照对照表，一行不落，不自己发挥。
- `docs/tasks/gallery-3/SOURCE.md` 写清每一部分对应来源的哪个文件和行号，哪些没照搬、为什么。
- 对照表第七节的每一项都列进“没做到的”和“要用户定”。

**设计语言**
- 只用令牌和共用控件：`system/ui/ink/ui_theme.h` 的 `ink_ui_segments`、`ink_ui_list_row`、`ink_ui_empty`、`ink_ui_pagebar`、`ink_ui_text_button`、`ink_ui_pill`，以及 `ui_list.c`。
- 能点的不小于 108 px；一屏最多一个主按钮；没有粗框，没有圆圈；行之间细分隔线，组之间留白；没内容的区块不画，整页空才用共用空状态。
- 不写死像素和颜色（`tools/ui_scale_check.py`）。旧画板的对照分数（`tools/ui_compare.py`）不作验收。

**墨水屏**
- 没有动画、过渡、惯性和转圈。
- 一次操作只刷一次，只刷变了的那块（D-127）；按下的反馈只刷按下的控件（D-31，用 `ink_ui_style_button`）。
- 不靠颜色区分状态。
- 不用会自己消失的提示。

**家人和权限**
- 只看得到自己能看的空间，孩子看不到大人的照片。
- 确认卡只给不能恢复的删除（D-105）。
- 照片不给不该看的人和 AI。

**文字大小**：跟系统“文字大小”一起缩放，默认字号、最大字号各出一套截图，孩子身份再出一套；放不下就分页或换行，不缩字，不裁按钮。

**测试**
- 新测试放 `tests/gallery_3/`。
- 夹具是明显假的照片，文件时间由测试代码在宿主机上明确设定（模拟器的文件时间来自宿主机，见 `hal_storage_sim.c:615`，不能靠“现在几点”）；系统时间用 `ink_time_set` 控制。
- 不弱化、不跳过已有测试，也不改它们的等待时间来让它们通过；只在这个任务有意改了行为的地方改已有测试，而且要一样严，每一条都列进报告。
- 预计会改的：
  - `tests/ui_fix_c/gallery.c`：
    - 第 20 行，快照里一直有 `gallery.reload`；
    - 第 35-38 行，删除绑定 `file.delete`，而且在顶栏；
    - 第 58-60 行，删除要确认；
    - 第 62、68 行，平时页内能重读、卡回来后手动重读。

    要保留的：“AI 不是黑底主按钮”；“读不了时页内能重读”（第 43 行）；“读错不冒充空相册”。
  - `tests/camera_gallery/camera_gallery.c` 里相册的部分；`tests/i18n_literals.py` 白名单追加。

**共享文件**
- `docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交。
- 新能力进 `tests/integration/ai_surface.c` 的两张表：给 AI 的写清必填参数，不给 AI 的写理由。
- 新文案的英文加进 `assets/i18n/en.tsv`。
- 这些文件和两处 CMake、两份翻译源文件清单、`assets/icons/manifest.json` 都只在末尾追加本任务的行，合并冲突由主会话处理。`ink_fs_entry_t` 若 `files-3` 等任务也要加字段：先合入的在前，后到的只在末尾追加，已有字段顺序不动。

**截图**：放 `docs/tasks/gallery-3/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。

## 范围和预算

**只改**
- `system/apps/gallery/`：可以拆成几个文件；新加 `repository.cpp` 和 `vendor/agentui/`（原文、许可、`SOURCE.md` 那部分）。
- `system/services/media/image.c` 和 `system/include/ink/sys/image.h`：EXIF 方向、`ink_image_probe`，都是增量。
- `system/include/ink/sys/file.h` 的 `ink_fs_entry_t` 末尾加 `mtime`，以及 `system/services/file/file_service.c` 里 `index_query` 填它的那一行。
- `assets/icons/`：新加 `rotate` 三个样式、`manifest.json` 末尾一项，加 `tools/icongen/README.md` 一句说明。
- 相册相关的测试和假夹具：新的 `tests/gallery_3/`，以及上面列的已有测试里相册的部分。
- `CMakeLists.txt`（新文件、翻译源文件清单）、`ports/esp32s31/main/CMakeLists.txt`（`.cpp`）、`ports/esp32s31/components/ink_system/CMakeLists.txt`（翻译源文件清单）：都只追加。
- `docs/tasks/gallery-3/`。

**不碰**
- `system/apps/camera/`：`camera-3` 在做，`photo.h` 不改签名；`camera-touch-bus` 可能也动相机。
- `system/apps/files/`、`file.batch`、传书：`files-3`、`transfer-2` 的。`files-3` 也在改 `system/services/file/`，可能给 `ink_fs_stat_t` 加修改时间；本任务只动 `ink_fs_entry_t` 和 `index_query` 那一行，单位都用纳秒。
- `theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c`：缺共用控件就写进报告。
- 输入服务：不为捏合或双击改。
- 锁屏和壁纸：`lock_mark.c`、`wallpaper.c`，`lock-styles-1` 在做。`ink_image_dither` 也不改，只调用。
- 文件服务和图片解码只加上面写的几处，别的要改就写进报告。

**预算**
- 产品代码不超过 1600 行：删掉的旧代码不抵；搬来的 AgentUI 原文不算；shim 算。报告里逐文件列行数。
- 固件增量不超过 25 KB，超了写清原因。
- 交付计划第 74 行还写着 500 行，装不下：旧 `gallery.c` 本身就 273 行，这次要新加首页三个分段和筛选、相簿和月份两种列表、单张的缩放平移旋转和自己渲染、信息页、最近删除的挪动、恢复、清空和自动清理、EXIF 读取、12 个新能力和 4 种快照。这里保留上一版定的 1600 行，请主会话改交付计划，或者拆成两个任务：首页、单张、信息算一个，最近删除算另一个。

**起步**：当前主线 `claude/system-layer`，已经有 design-lang-1、design-lang-2、home-1、lock-1、settings-1、control-1、notify-svc-1、graft-mogo-4、audio-out-2。

## 要用户定（第 11 节：少做的功能、搬不动的代码）

1. **AgentUI 只搬纯函数**：枚举、删除、视图和控制器不搬，证据见“搬代码”一节和对照表第 68-78 行。请用户认可。
2. **这次不做的功能**（建议都不做或另开任务）：
   - 搜索、多选：多选交给文件应用；
   - 收藏；
   - 可选排序、列数可调；
   - 幻灯片和屏保：一直在刷屏；
   - 编辑和另存旋转；
   - 隐藏文件夹；
   - 分享、打印：设备上没有分享对象；
   - 复制、移动、改名：文件应用有；
   - 位置和地图、完整元数据、改元数据；
   - 回收站开关；
   - 视频和动图；
   - 设为壁纸：建议本批 lock-styles-1 合入后加一个按钮；
   - 在最近删除里点开单张预览：建议不做，恢复前靠名字和日期认；要做就另加一页。
3. **大图缩小解码**：手机拍的照片大多超过 100 万像素和 4 MB，这一版看不了。建议另开媒体层任务，用 LVGL 自带的 TJpgDec 按 1/2 到 1/8 缩小解码；它边读边解，不用把整个文件读进内存。壁纸、缩略图会一起受益。
4. **预算**：1600 行，还是拆成两个任务（见上）。

## 验收

1. **照参照路径走一遍**（APPS.md A08，模拟器脚本，每步截图并断言）：
   - **夹具**：三个月份共 25 张，分在两个相簿里，9 月 9 张。另有：一张 EXIF 方向为 6 的竖拍 JPEG；一张 1600×1200 的 JPEG；一张 `.webp`；一张画着小字的 PNG。文件时间由测试设定，小字 PNG 是 9 月里最新的一张，保证它排第 1、下一张是第 2。
   - **路径**：
     1. 打开相册 → “日期” → 点“2026年9月” → 网格里只剩 9 月的。
     2. 点那张有小字的 → 双击放大（两次点按）→ 拖到字的位置。断言照片区的缩放是 2 倍，字的区域在屏幕里。
     3. 回到适合屏幕 → 左滑到下一张，写着第 2/n 张。
     4. 返回，停在刚才那张所在的页。
     5. 再进一张，删除：不弹确认，换到下一张，写“已移到最近删除”。
     6. 相簿 → 最近删除 → 恢复 → 这张回到原路径，也回到原来的相簿和原来的月份（文件时间没变）。
   - **同一条路径用能力调用再走一遍**（来源是 AI），结果和快照一样。
   - **反向对照**，每项都要让测试失败：
     - 去掉 EXIF 方向处理，竖拍照片的宽高断言失败；
     - 删除改回直接走 `file.delete`，“不弹确认、能恢复”失败；
     - 返回时不定位，“回到原位置”失败。
2. **对照表逐行验收**：每一行对应的截图或测试都在，报告写做到了几行、哪几行没做。
3. **搬来的代码**：`agentui_*` 测试全过；`SOURCE.md` 的行号、SHA-256、改过的 4 处和原文对得上（脚本对比原文，改过的行列成例外）。
4. **信息**：
   - 竖拍 JPEG 的信息写 EXIF 拍摄时间、摆正后的宽×高、大小、相簿；
   - PNG 写“文件时间”；
   - 没有的项照对照表第 43 行处理。
5. **方向**：8 种 EXIF 方向的夹具，解码以后宽高和四个角的像素都对；EXIF 损坏、越界、截断时当作没有方向，不崩（ASan 过）。
6. **看不了、读不了**：
   - 1600×1200 那张的格子写“看不了预览”，单张写“太大（1600×1200）……”；
   - `.webp` 写格式看不了；
   - 没卡、卡借给电脑、索引没建好各有一句话，没有白页；
   - 卡回来后自动重读，不用按按钮；
   - 相对路径长到放不进最近删除的夹具，只给永久删除并写清原因。
7. **永久删除和自动清空**：
   - 最近删除里永久删除弹确认卡（文字有“不能恢复”），取消后照片还在；清空也一样。
   - 系统时间往后拨 31 天再打开相册：31 天前删的被清掉，29 天前删的还在；时间不可信时都不清。
   - 反向对照：去掉 `ink_time_valid` 判断，“时间不可信时都不清”这一条要失败。
8. **家人和锁屏**：
   - 孩子只看到自己的照片和自己的最近删除；
   - 孩子按编号开大人的照片被拒（现有断言保留）；
   - 锁屏时，AI、远程和应用调用所有新的相册能力都被拒（`locked_owner_only`）；
   - `ai-history-photo` 的断言一条不改，照样过。
9. **刷新**（D-127）：照 `tests/control_center.py` 的 `inside()` 从显示轨迹数刷新，断言：
   - 网格翻页只刷网格区；缩略图合批，一次翻页最多 1+2 次；
   - 放大、平移（含“左移”等按钮）、旋转只刷照片区（底栏随状态变化时合成同一次），而且只刷一次，拖动过程中不刷；
   - 双击的第一次点按不刷；
   - 删除后换下一张只刷照片区、标题行和状态行，一次；
   - 传进一本书，相册不重画。

   报告列出每个主要操作刷了几次屏：打开相册、翻页、切分段、点开一张、换张、双击放大、平移、旋转、信息、删除、撤销、恢复。
10. **改前改后截图**：全部（带月份行）、相簿、日期、筛选后的网格、单张（适合屏幕和 2 倍）、信息、最近删除、空相册、读不了（没卡）、看不了的格子。默认字号、最大字号、孩子身份各一套；能点的都不小于 108 px（`ui-scale` 过）。
11. **构建和测试**：
    - 主工程普通全量 ctest 过，含 `ui-scale`、`camera-gallery`、`ai-history-photo`、`ui-fix-c`、`open-app-latency`、`media` 相关、`i18n` 相关。
    - ASan 跑 `camera-gallery`、`ai-history-photo`、`ui-fix-c` 里的相册、`gallery-3` 和 `ui-scene-*`，都要过。
    - S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过，没有新警告，`.cpp` 进了固件。
12. **报告要写**：
    - 固件增量；产品代码行数（逐文件，搬来的原文另列）；`gcccheck` 结果；
    - 对照表做到了几行；
    - 改过的已有测试：每条写哪条、为什么改、怎么保持一样严；
    - 新增和改名的能力（包括 `photo.restore` 是新能力、用 `{path}`，不能恢复的两个删除用自己的能力）；
    - 文件服务和图片解码改了什么；解码峰值内存多了多少；4 倍放大的内存占用；每个主要操作的刷新次数；
    - 上机先核对清单：真机 FAT 文件时间的时区、双击间隔；
    - 没做到的：捏合、位置信息、大图缩小解码，以及对照表第七节各项。

对照表见 [gallery-3/对照表.md](gallery-3/对照表.md)。
