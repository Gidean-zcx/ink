# gallery-3 来源

查阅日期 2026-11-13。三个仓库都是只读浅克隆到 `work/`（不进仓库），按固定提交核对。行号按克隆里的内容核过。

## AgentUI（搬代码）

- 项目：<https://github.com/AlayaElla/MetalioClaw4-AgentUI>，固定提交 `172ec530ff65e11521585fe3b8611daa20a78aa4`。
- 许可：MIT。版权行原文：`Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd.`、`Copyright (c) 2025 Project Contributors`。许可全文在拷贝文件顶上。
- 拷到：`system/apps/gallery/vendor/agentui/camera_gallery_repository_part.h`（只给 `system/apps/gallery/repository.cpp` 包含）。C 端接口在 `system/apps/gallery/repository.h`。
- 原文件和 SHA-256（`shasum -a 256` 核对过，和任务书一致）：
  - `main/display/agent_ui/apps/camera/camera_gallery_repository.cc`：`0a8dcf664db73014902f12aa025630d1dc595e9c04c900ac006a3c5cd26bd29e`
  - `main/display/agent_ui/apps/camera/camera_view.cc`：`859fd456ff060dc5037e9ea1a44fda43977f21fd196b219ffed39a4100c063f4`

### 拷了哪些（整函数，原文照拷）

| 原文 | 行 | 我们用在哪 |
| --- | --- | --- |
| `camera_gallery_repository.cc` 命名空间开头 | 16-17 | 原样保留 `namespace agent_ui::camera { namespace {` |
| `IsJpg` | 24-41 | 信息页“格式”（文件头认不出时按扩展名） |
| `FormatDate` | 64-85 | 最近删除的日期文件夹名、月份键（取前 7 位） |
| `FormatDayLabel` | 87-92 | 单张标题行、网格快照的日期 |
| `FormatTimeLabel` | 94-99 | 信息页的时刻 |
| `FormatSizeLabel` | 101-112 | 信息页的大小 |
| 排序的 lambda | 154-158 | 首页按文件时间从新到旧，时间相同按名字倒序 |
| 匿名命名空间结尾 | 114 | 原样保留 `}  // namespace` |
| `camera_view.cc` 按 `date_key` 分组的循环 | 1109-1120 | “日期”分段的每一行 |

### 改了哪些行（共 4 处）

1. 排序 lambda 的两个参数 `const GalleryPhoto&` 改成 `const auto&`（原 154、155 行）：直接排 PSRAM 里的键数组（`int64_t modified`、`std::string_view name`、下标），不为 1024 张照片各建一个带 6 个 `std::string` 的 `GalleryPhoto`。键数组和排好的副本是两块静态 PSRAM（`INK_PSRAM_BSS`），排序不分配内存。
2. 分组循环包成模板函数 `GroupByDateKey(const Gallery& gallery)`，`state.gallery` 改成参数名 `gallery`（原 1111、1112、1114、1115 行），末尾加 `return groups;`。`groups` 仍是原文的 `std::vector`（有几个月就有几项，用完即放）。
3. `"%d月%d日"`（原 89 行）、`"大小未知"`（原 102 行）包进 `TR()`，英文界面能翻译。
4. 排序 lambda 原来在 `List()` 里、分组循环原来在 `RenderGalleryItems()` 里，各包成一个命名函数（`SortNewestFirst`、`GroupByDateKey`），函数体原样。

本地时间：shim 用我们的 `ink_time_to_local` 填 `std::tm`，不用原文的 `localtime_r`。shim 不包含 `image.h`。

核对脚本：`tests/gallery_3/source_check.py`（CTest `gallery-3-source`）按上表逐行比对原文，只允许上面 4 处改动和包装行；没有克隆时跳过（返回 77，CI 里显示为跳过）。本次【实测】84 行原文，0 处问题。

### 没搬的和证据

- `List()` 的其余部分（116-153、159 行）：118-123 行要 `SdCardManager`，并且只读 `/sdcard/DCIM/Camera`；130-132 行只认 JPG；139 行直接 `stat` 卡上的原始路径，绕过 `ink_fs` 的家人隔离；159 行和 `camera_gallery_repository.h:16` 最多 48 张。我们用文件索引 `ink_fs_index` 枚举，扫完整个索引。
- `Delete`（163-165 行）：直接删文件；我们挪进最近删除。
- `WriteJpeg`（167-209 行）：相机存图，归 camera-3。
- 视图和控制器（`camera_view.cc:44-89`、`929-936`、`1045-1057`、`1062-1088`、`1132-1186`、`1203`，`camera_controller.cc:112-197`）：都是类的成员，离不开 `Theme::Get()`、`ui_components`、`ViewState`、`Dispatch`，只照着做（对照表第 73-78 行）。
- 原仓库这个提交没有测试文件；我们的 `tests/gallery_3/agentui.c`（CTest `gallery-3-agentui`）从 C 通过 shim 调每个搬来的函数。

## Aves（只照着做，BSD-3，不拷代码）

<https://github.com/deckerst/aves>，固定提交 `9d3f1ff19f680db043c9d80831c6f222e0f8220c`，路径在 `lib/` 下。

| 对照表行 | 参照 |
| --- | --- |
| 1、2 | `model/settings/defaults.dart:60-61`（按日期排、按月分节） |
| 3、5、7、15 | `widgets/collection/collection_page.dart:50、143`（滚动）、`151-157`（筛选）、`175-204`（FAB）、`64`（回收站是筛选） |
| 8、9 | `defaults.dart:62-68`（搜索、多选） |
| 10 | `collection_page.dart:206-219`（从单张返回定位） |
| 11、12、13、91 | `widgets/collection/collection_grid.dart:608-657`（加载中、没权限、没有图片） |
| 14 | `defaults.dart:70-78`（缩略图角标） |
| 19-22 | `l10n/app_en.arb:194`（Undated）、`667`（按月）、`791-798`、`812-814`、`829`（相簿） |
| 25 | `widgets/viewer/entry_viewer_page.dart:11-58`、`entry_horizontal_pager.dart` |
| 26、27 | `defaults.dart:85-90`（快捷操作）、`91-95`（叠加层） |
| 29 | `widgets/viewer/visual/entry_page_view.dart:215-322`（捏合） |
| 34 | `widgets/viewer/slideshow_page.dart` |
| 35 | `widgets/viewer/entry_vertical_pager.dart:33-223` |
| 37 | `viewerErrorUnknown`、`viewerErrorDoesNotExist`（`app_en.arb`） |
| 41-48 | `widgets/viewer/info/info_page.dart:1-127`、`basic_section.dart:306-345`（“未知”的写法 308-318） |
| 49 | `defaults.dart:40`（删除确认） |
| 54 | `model/entry/extensions/props.dart:99-103`（还剩几天） |
| 58 | `model/source/trash.dart:15`（30 天）、`22-41`（到期删除），`model/source/media_store_source.dart:150-160`（打开媒体库时删） |
| 59、54 的说法 | `app_en.arb:1024-1025` |

## Fossify Gallery（只照着做，GPL-3，一行 Kotlin 也没拷）

<https://github.com/FossifyOrg/Gallery>，固定提交 `2688360242468f1d591eddf23740c879616bb828`，路径在 `app/src/main/kotlin/org/fossify/gallery/` 下。

| 对照表行 | 参照 | 我们和它不一样的地方 |
| --- | --- | --- |
| 28 | `fragments/PhotoFragment.kt:599-616`、`106`（双击在适合屏幕和 2 倍之间） | 以点按位置为中心；第一次点按不画任何东西 |
| 30 | `PhotoFragment.kt:185-195`（没放大才把手势交给外层） | 放大后加“左移”等写字的按钮，翻页键先看完这张 |
| 33 | `activities/ViewPagerActivity.kt:819-834`（旋转） | 只转这次怎么看，不另存 |
| 38 | `PhotoFragment.kt:398-414`（degreesForRotation、rotateViaMatrix）、`223-238` | Fossify 把 2、4 当成不转，5、7 只转不镜像；我们 8 种都按标准（`system/services/media/image.c` 的 `source_at`） |
| 49、52 | `ViewPagerActivity.kt:1163-1206`（删除前确认）、`1208-1240`（删后换下一张） | 能找回所以不确认（D-105） |
| 50、51、55、57 | `extensions/Activity.kt:346-401`（移进回收站留原路径）、`392、467`（保留修改时间）、`410-486`、`441`（恢复，重名换名）、`488-500`、`508` 起（清空和确认） | 同一张卡上改名，删除日期是文件夹名，不建数据库；重名在扩展名前加“（恢复）” |
| 53 | `activities/MediaActivity.kt:1037-1072`（Toast） | 用留到下一次操作的状态行 |
| 58 | `activities/MainActivity.kt:1640-1658`（一天查一次删 30 天前的） | 打开相册时查一次，不轮询；时间不可信不删 |
