# Flipper Zero Archive：拷来的原文

- 项目：Flipper Zero 固件，<https://github.com/flipperdevices/flipperzero-firmware>
- 固定提交：`3060cee9bac71d332dc7629191ae1aeb1edaac33`，目录 `applications/main/archive`
- 许可：GPL-3.0，全文在本目录 `LICENSE`（仓库根 `LICENSE`，SHA-256 `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`）
- 为什么拷：files-3 任务书“要搬的代码”（D-140，总规则第 11 节）。许可和总规则第 4 节冲突，见任务报告“要用户定”。
- 2026-11-13 从 raw.githubusercontent.com 按固定提交取原文件（只读副本在会话的临时目录，不进仓库）。

原文件这几个都没有许可头，所以“保留原来的头部”就是保留原来的 `#include` 行；只拷了部分函数的文件，`#include` 行有改动的在下面写明。

## 拷了什么

| 本目录的文件 | 原路径 | 保留的原文行 | 原文件 SHA-256（这个提交） |
| --- | --- | --- | --- |
| `helpers/archive_files.c` | `helpers/archive_files.c` | 整个文件（1-114） | `bbe7cff3af9357b233cf9f5c5789eb54fb7304191c0b752eae81146963e038c0` |
| `helpers/archive_files.h` | `helpers/archive_files.h` | 整个文件（1-99） | `bd7fd70ed9dacc8fb248986955394070ca51ced20022a594370bd35bb52581d5` |
| `helpers/archive_favorites.c` | `helpers/archive_favorites.c` | 1-4（include）、8-137（`read_line`、`commit_temp`、`count`、`rescan`）、203-337（`delete`、`is_favorite`、`rename`、`add_to_favorites`） | `31ec9fe7c21d7d29f0d15fde2737beee8b699515b2e1a50af3690a18b8b36992` |
| `helpers/archive_favorites.h` | `helpers/archive_favorites.h` | 整个文件（1-24） | `384391dcd00b7ee99903da81b746105492ad9b81a0204959fda35823e01c03a2` |
| `helpers/archive_browser_ink.h` | `helpers/archive_browser.h` | 23-38（`known_ext[]`）、65-67（`archive_is_known_app`） | `a13117376cc916e1106cbbba40aaf96e02b6e2dcbaf9f1d395e441c77748da5b` |
| `views/archive_browser_view.h` | `views/archive_browser_view.h` | 1-4、11-12（include）、14-105（宏、`ArchiveTabEnum`、`ArchiveBrowserEvent`、`file_menu_actions`、`ArchiveBrowserView`、`BrowserActionEnum`、`ArchiveBrowserViewModel`） | `9dfdca53a87e7ac678019bba1c0a0312fdf164d9701380ece03fef552f4d1dd5` |
| `views/archive_browser_view.c` | `views/archive_browser_view.c` | 3-5（include）、51-97（`render_item_menu`） | `63c675c59543776432ae7f99ae0bbc00b7b193fbdca6286818093e3b72dd607c` |
| `scenes/archive_scene_browser.c` | `scenes/archive_scene_browser.c` | 2-5（include）、9-12（宏）、14-37（`archive_get_flipper_app_name`）、51-90（`archive_run_in_app`）、120-265（`archive_scene_browser_on_event`） | `f8716b1604cc4cbf8b7f63a0383248e323cca5e041299fcb6794dc22314671a7` |

`scenes/archive_scene_rename.c`（SHA-256 `3674d6d4b3820eafbc62e912609db3b847f1d92457a856977670e6d5560b0648`）62-90 行改名时收藏跟着改的做法没拷函数（它整个依赖场景管理器和文本输入），我们在 `files_flipper.c` 的 `files_fl_renamed` 里照它的条件调用原文的 `archive_favorites_rename`。

## 改了哪些行，为什么

| 文件 | 原文 | 改成 | 为什么 |
| --- | --- | --- | --- |
| `helpers/archive_files.h` | `ArchiveFileTypeEnum` 的 `IButton`…`JS` 十项 | `Text`、`Book`、`Audio`、`Image`、`Zip` 五项（`Folder` 以后的不变） | 任务书：`known_ext` 换成本机的类型；枚举是 `known_ext` 的下标 |
| `helpers/archive_files.c` 18-29 | `.txt` 只有在 BadUSB 目录里才算脚本的分支 | 匹配就定类型 | 任务书：去掉 BadUSB 路径规则（本机没有 BadUSB） |
| `helpers/archive_files.c` 105 | `if(archive_is_favorite(...))` | `if(res && archive_is_favorite(...))` | 原文删除失败也去掉收藏。这里删除要经过确认卡和后台，确认卡被取消或卡借给了电脑时文件还在，去掉收藏会把还在的文件弄丢；所以删成功才去掉 |
| `helpers/archive_favorites.c` 6 | `#include <dialogs/dialogs.h>` | 删掉 | 只给没拷的 `archive_favorites_handle_setting_pin_unpin` 用 |
| `helpers/archive_browser_ink.h` | `known_ext[]` 的 Flipper 扩展名 | 本机的扩展名表，一个类型可写几个，用 `|` 隔开（Flipper 的文件浏览器过滤器也这样写）；`.zip` 记为 `?`（解压这次没做，见任务报告） | 任务书 |
| `helpers/archive_browser_ink.h` | `inline bool archive_is_known_app` | `static inline bool ...` | C99 的普通 `inline` 在头文件里没有外部定义，`-O0` 链接会缺符号 |
| `views/archive_browser_view.h` | 6-10 行的 GUI 头文件 | 删掉 | 128×64 的画面不拷；`View`、`BrowserWorker`、`FuriTimer`、`InputKey` 由垫片 `furi.h` 给出 |
| `views/archive_browser_view.h` | `#define MENU_ITEMS 4u` | `10u` | 任务书：末尾追加我们的菜单项 |
| `views/archive_browser_view.c` | `#include "../helpers/archive_browser.h"` | `#include "archive_browser.h"` | 原 `archive_browser.h` 没拷（依赖 `archive_i.h`），换成垫片目录里的同名头文件 |
| `views/archive_browser_view.c` 在 `menu[3]` 之后 | — | 加 `menu[4]`…`menu[9]` 六行：Copy、Move to、Info、Edit、Read、`---` | 任务书：追加拷贝、移动、详情、编辑；“Read”是 `.txt` 的“用阅读打开”，第 10 项留给文件夹收藏 |
| `views/archive_browser_view.c` 画菜单循环之前 | — | 加一段：不是 `.txt` 的去掉 Edit、Read；不在 `/me/` 下的去掉 Read（`mogo.open` 只收 `/me/`）；文件夹的第 10 项写 Pin folder 或 Unpin folder | 我们的项怎么组成也写在原文函数里，决定菜单的逻辑只有一处。原文对文件夹不给 Pin（`archive_is_known_app` 为假），这条判断没改，文件夹收藏是追加项 |
| `scenes/archive_scene_browser.c` 1-7 行 | `archive_i.h`、`archive/scenes/archive_scene.h`、`<applications.h>` | 换成垫片 `ink_flipper.h`；`../helpers/archive_browser.h` 换成 `archive_browser.h` | 场景管理器、视图分发、应用表都不拷 |
| `scenes/archive_scene_browser.c` 16-33 | Flipper 应用名 | `files.preview`、`mogo.open`、`media.play`、`photo.open` | 任务书 D35：交给对应的能力。启动那一句（`loader_start_with_gui_error`）没改，垫片把它接到能力调用 |

没有改原文函数的控制流程。

## 没拷的部分和原因

- `archive_browser_view.c` 其余部分（128×64 的列表、标签页、滚动、输入处理、加载图标）：画面由我们的 LVGL 页面做。
- `archive_browser.c`（文件数组、标签切换、进出目录、焦点）：依赖 `archive_i.h` 和 Flipper 的文件浏览 worker；这些由垫片接到我们的页面（进目录、上一级、翻页、回到原来那一行）。
- `archive_apps.c`、U2F、设置类收藏（`/app:` 路径）：本机没有这些应用，垫片 `archive_apps.h` 说“没有”。
- `archive_favorites.c` 的 `archive_favorites_read`、`archive_favorites_save`、`archive_favorites_handle_setting_pin_unpin`：依赖浏览视图模型或对话框。读收藏由首页做，保存顺序（收藏调顺序）这次不做。
- 场景管理器、`archive_scene_rename.c`、`archive_scene_delete.c`：改名用我们页面上的输入框，删除用系统确认卡（D-105）。
- 收藏调顺序（`FavMoveUp`、`FavMoveDown`、`EnterFavMove` 等分支）：分支原样留在 `archive_scene_browser_on_event` 里，垫片里对应的 `archive_file_array_swap`、`archive_favorites_move_mode`、`archive_favorites_save` 什么都不做，菜单里也不画“Move”。

## 我们写的垫片（`system/apps/files/flipper/` 和 `system/apps/files/files_flipper.c`）

- `furi.h`：`FuriString` 定长缓冲和原文用到的函数（`set`、`alloc_set`、`search` 用 `_Generic` 同时收 `FuriString*` 和字符串，像 Flipper 那样）、`furi_assert`、`COUNT_OF`、`UNUSED`、`furi_record_open/close`（收藏存储的锁，同一任务可重入）。`furi_string_end_withi` 认 `a|b` 写法。
- `m-array.h`：`ARRAY_DEF` 只生成 `files_array_get`，视图模型里的数组指向当前选中的那一项。
- `storage/storage.h`：`ARCHIVE_FAV_PATH` 和 `ARCHIVE_FAV_TEMP_PATH` 两个“文件”在内存里，内容是按家人的 `files.prefs` 记录里的一段文字，一行一条路径，最多 32 行；最后一次 `furi_record_close` 时写回记录。真实路径：`storage_file_exists` 问文件服务（只有“找不到”才算不在，卡读不了时收藏都留着），`storage_common_remove` 和 `storage_simply_remove_recursive` 返回 `file.delete` 这次的结果，`storage_common_stat` 返回当前项是不是文件夹。
- `archive_apps.h`、`archive_browser.h`、`ink_flipper.h`：画布记录器（`canvas_draw_str` 把菜单项记下来，其余画图是空操作）、浏览帮手（进目录、上一级、翻页、去掉当前项、打开菜单）接到 `files.c`，场景管理器的下一场景（改名、删除）、`view_dispatcher_stop`（`ink_ui_pop`）、`loader_start_with_gui_error`（调用对应能力）。
- `flipper_unit.c`：把四个原文 `.c` 合成一个编译单元（关掉告警，见根 `CMakeLists.txt` 和 `ports/esp32s31/main/CMakeLists.txt`），对外给出两个原文的静态函数：`render_item_menu`（`ink_flipper_menu`）和 `archive_favourites_rescan`（`ink_flipper_rescan`，在后台执行器上跑）。
- 文件夹收藏：原文不能收藏文件夹，而且它按前缀匹配，文件夹会因为里面有收藏的文件而显得“已收藏”。所以文件夹按整行 `路径/` 记，判断和取消用整行匹配（`files_flipper.c` 的 `line_of`），添加仍走原文的 `archive_add_to_favorites`。

## 原文的已知行为（照原样保留，写给后来的人）

- 收藏的查、删、改名按前缀匹配：取消收藏 `/me/a.txt` 也会去掉 `/me/a.txt.bak`；删掉一个文件夹会去掉它下面所有的收藏（这正好是对的）。

## 测试

原文的归档模块没有自带测试，没有可带过来的。我们为原文函数写的检查在 `tests/files3/files3.c` 的 `flipper` 那一组（`python3 tests/files3/run.py --build build --case flipper`）：

- `flipper-types`：`archive_set_file_type` 的类型表（大小写、几种扩展名、文件夹、未知类型、名字像已知文件的文件夹照原文算那种类型）。
- `flipper-menu`：`render_item_menu` 的组成：文本文件、家庭里的文件（没有“用阅读打开”）、文件夹、未知类型、收藏标签。
- `flipper-fav`：改名收藏跟着改，删除失败收藏留着，删除成功收藏去掉。
- `flipper-events`：EnterDir、Exit、LoadNextItems、LoadPrevItems、FileMenuOpen、FileMenuRun、FileMenuPin、FileMenuDelete、ListRefresh 各自落到的页面动作或能力。

`fav` 那一组还走一遍真实页面：收藏、改名后跟着改、删除后去掉、被 AI 的 `file.move` 移走后回首页时 rescan 去掉。
