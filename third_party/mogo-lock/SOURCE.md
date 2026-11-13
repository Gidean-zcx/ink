# Mogo 锁屏样式（lock-styles-1，D-140 搬代码）

来源：Mogo，`ink-hw/upstream/oss/mogo-e4p`，固定提交 `864f2f9dc75980a67d2d6b67b17365020b7a50ce`（v1.1.0，和 `third_party/mogo/` 同一个提交）。
许可照原样带上：`LICENSE`（Apache-2.0）、`licenses/`（Apache-2.0 全文、`LicenseRef-Kiikoread-Personal-Study-1.0`）；时钟数字的 Chakra Petch 和日历数字的 Libre Bodoni 是 SIL OFL 1.1（`main/assets/clock-font/OFL.txt`、`tools/vendor/libre-bodoni/OFL.txt`）；只给宿主测试用的 `test-support/main/assets/builtin.ttf` 是 Noto 子集（`test-support/main/assets/OFL-Noto.txt`）。用户说过不看许可证（D-129），各文件头的许可照旧。

每个文件的 SHA-256 在 `upstream-files.json`；`tests/mogo_lock_host/provenance.py`（ctest `mogo-lock-provenance`）逐个核对，改过的必须在这里写清。

## 拷了哪些、各自干什么

| 原文 | 本仓库用来 | 编进固件 |
| --- | --- | --- |
| `main/book/book_ticket.c/.h` | 票根（`book_ticket_draw`，268–321 行画票面；两种背景） | 是 |
| `main/book/book_lock_collage.c/.h` | 拼贴（`book_lock_collage_prepare` / `_cache_commit`） | 是（改 1 行，见下） |
| `main/ui/ui_lock_clock.c/.h`、`main/assets/clock_digits.h` | 数字时钟（Mogo 叫“简约时钟”） | 是 |
| `main/ui/ui_calendar.c/.h`、`main/assets/calendar_digits.h` | 老黄历（单日）和万年历（月格），`ui_calendar_draw(…, monthly)` | 是 |
| `main/ui/ui_mini_library.c/.h`、`main/book/book_mini_library.c/.h`、`main/services/home_library.c/.h` | 迷你书库（最多五本，“最近 / 随机”） | 是 |
| `main/ui/ui_image_dither.c/.h` | 票根、拼贴里封面的 16 级抖动 | 是 |
| `main/ui/ui_wallpaper.c/.h`、`ui_wallpaper_crop.h` | 和我们的壁纸画法对比用（结论：留我们的，见报告） | 否 |
| `tools/generate_clock_digits.py`、`generate_calendar_digits.py` 和两套字体源 | 点阵生成器，ctest 里 `--check` | 否 |
| `docs/development/guides/LOCK_CLOCK.md`、`CALENDAR.md`、`MINI_LIBRARY.md` | 原说明 | 否 |
| `tools/*_host_test.py`、`wallpaper_crop_host_test.c` | Mogo 自己的宿主测试，见“测试” | 否 |
| `test-support/…` | 只给宿主测试用的 Mogo 原文件（UI 工具包、epdiy、导航、内置字体、桩头文件、`book_lock_collage.c` 原样一份） | 否 |

阅读数据模块（票根、拼贴、迷你书库都要读）属于阅读，原样整文件加在 `third_party/mogo/main/book/`：`book_cover.c`、`book_cover_auto.c/.h`、`book_excerpt.c/.h`、`book_title.c/.h`、`book_metadata.c/.h`，追加在 `third_party/mogo/upstream-files.json` 末尾。

## 改了原文的地方（只有一处）

- `main/book/book_lock_collage.c` 第 47 行 `LOCK_FRAME_BYTES`：`684u * 1216u / 2u` → `684u * 1216u`。Mogo 的画布一字节两个像素（4bpp），我们的锁屏画布一字节一个像素（L8，`apps/mogo/port/drawing.c` 的 `epd_draw_pixel` 就是这样写的）。不改的话整帧缓存只存上半屏，再锁屏读回来下半屏是白的（`tests/lock_styles.py` 的 styles 情况会查）。Mogo 自己的拼贴宿主测试按 4bpp 写，用 `test-support/` 里的原样那份跑。
- `ui_calendar.c` 开头 `memset(fb,255,UI_LOCK_WIDTH*UI_LOCK_HEIGHT/2)` 同样是按 4bpp 清半屏，没改：胶水每次画之前已经把整张 L8 画布清白，这一行无害。

## 没改原文、在胶水里按设备换掉的（apps/mogo_lock）

编译时这批文件先强制包含 `apps/mogo/port/runtime.h`（阅读的垫片），再强制包含 `apps/mogo_lock/port/lock_port.h`；`apps/mogo_lock/port/` 在包含路径最前，同名头文件用 `#include_next` 接上阅读的垫片再补缺的。

- 文件：读走 `runtime.h`（`ink_graft_file_open`）；写（`fopen("wb")`、`fwrite`、`mkdir`、`rename`、`remove`）在 `lock_port.c` 里一样走 `ink_graft_file_open` / 当前家人的文件服务。Mogo 的缓存根 `/sdcard/.readpico` 换成当前家人的 `/me/mogo`（卡上不许点开头的名字），拼贴整帧缓存在 `/me/mogo/locks/collage.bin`，封面缓存在 `/me/mogo/covers/`。`opendir/readdir` 列当前家人的目录；没有 `/me/books` 当作空书架，不当作 Mogo 的“卡读不了”。
- 时间：`book_ticket.c` 的 `local_day` 固定按北京时间（+8 h）算天。这批文件的 `time()` 换成“UTC 加上（系统时区偏移 − 8 h）”，Mogo 的算式落在系统时区的那一天上；钟没校准时是 0（Mogo 自己的“不编造今日数据”）。不读 RTC、PMU。
- 字：Mogo 的 `ui_text*` / `ui_text_fixed*` / `ui_text_title_vc` 在 `ui_kit.h` 里改名接到 `lock_port.c`，字号一律经过 `ink_mogo_scaled`（跟系统“文字大小”走，孩子按 110%）；Mogo 自己的截字、换行、缩小规矩照旧。两套数字点阵保持原尺寸。`book_cover_auto.c` 用的内置 UI 字体（`ui_font_*`）我们没有：换成卡上字体的覆盖率，写回它自己的 4 位画布。
- 设置：`app_settings_ticket_background()` 返回新设置“票根背景”（封面 → COVER，壁纸 → TRANSPARENT：画布先铺好已选壁纸，没有壁纸就是白底；Mogo 的“留着锁屏前那一页”不做，D-135）；迷你书库的“最近 / 随机”来自新设置 `display.lock_library`；`app_settings_device_name()` 是当前家人的名字（拼贴写“某某 的书架”）；对比度 100、不快刷、书目录 `/me/books`。
- 封面队列：Mogo 的 `book_cover_jobs_*` 是解码任务，这里在同一个后台执行器上当场做一张。解码一律用我们的 `ink_image_decode`（`book_image_grayscale`），取样照 Mogo 用最近点。
- 书库：`book_store_*` 只有一个根 `/me/books`；`board_sd`、`boot_state`、`device_power`、MOBI 元数据是空实现（这台设备不读 MOBI）。
- 电量图标：`ui_nav_battery_draw` 读电源服务的 `battery.level`、`battery.charging`。Mogo 的 Lucide 图标没带：空书库的“打开的书”画成两页框，“换一批”的图标不画（锁屏上不放“换一批”，胶水把那块涂白）。

## 不搬的（换成我们的胶水或已有的）

| 原文 | 为什么不搬 |
| --- | --- |
| `main/lock/lock_clock.c`、`lock_calendar.c` | 读 RTC、驱动刷新；时间来自时间服务，刷新由 `lock_faces/drawn.c` 只交变了的矩形 |
| `main/lock/lock_screen.c`、`lock_pin*` | 密码由 lock-1 的输密码页和家人门槛负责 |
| `main/power/sleep.c` 的样式分派和睡眠 | 锁屏分派在 `lock_mark.c` / `faces.c`，睡眠是电源服务的 |
| `main/time/calendar.c`、`calendar_data.h` | almanac-2 已带（`system/services/time/vendor/mogo-calendar`），只从 `ink_almanac_date()` 取 |
| Mogo 的图片解码 | 用我们的 `ink_image_decode` |
| `main/book/book_lock_bookmark.c` | 不在本任务名单里，又和 D-135 冲突：写进报告“要用户定” |

## 测试

ctest 里跑的（原文不改，`tests/mogo_lock_host/run.py` 用链接拼出 Mogo 的目录形状，`run_reader_baseline.py` 只换成本机编译器）：`wallpaper_crop_host_test.c`、`mini_library_host_test.py`、`home_library_host_test.py`、`lock_collage_host_test.py`、`book_excerpt_host_test.py`、`lock_clock_ui_host_test.py`、两个生成器 `--check`。

拷来但没接进 ctest 的：`calendar_ui_host_test.py`、`ticket_background_ui_host_test.py`、`mini_library_ui_host_test.py`。它们要 Mogo 的整套界面测试工具（阅读日志、文件页、设置页 161 KB、内置汉字点阵 188 KB、阅读场景图等，约 600 KB 原文件），本任务没带；这三张画面在模拟器里有截图和快照断言（`tests/lock_styles.c`）。

不适用、没拷的：`lock_screen`、`lock_pin`、`auto_lock`、`lock_sleep_trace`、`lock_style` 的测试管 Mogo 的密码、睡眠和设置存储；`lock_clock_host_test.py`、`wallpaper_cache_host_test.py` 依赖 RTC 和 `/sdcard` 缓存。
