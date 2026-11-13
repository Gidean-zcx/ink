# files-3：文件管理照 Amaze 和 Fossify File Manager 重写：在文件夹里搜索、排序、多选、选目的地、重名时问怎么办、成功和失败逐个列出；Flipper 的文件类型表、文件菜单、收藏的一致处理和浏览事件拷原文

先读总规则 `docs/tasks/codex-rules.md`，第 9、10、11 节优先；再读 `docs/tasks/vendor-common.md` 第 11 条。

**D-140（总规则第 11 节）**：本任务是“照着重写，Flipper 片段可搬”。
- 第一步：把对照表 `docs/tasks/files-3/对照表.md` 单独提交。草稿附在任务后面，开工时先核对再提交。
- Flipper 那几段（SOURCES.md X01）**必须拷原文**，写在下面“要搬的代码”一节；我们只写垫片和接线。
- 其余照对照表重写。参照安卓的地方，照第 11 节为这台设备改。做完逐行验收。
- 对照表取代原来计划的 `docs/tasks/files-3/SOURCE.md`（逐页对应来源），不再另写一份。

## 为什么

用户 2026-11-12 说：“所有的应用和系统应用都是移植的最优秀的三方项目吗 我怎么感觉不太像呢”；“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”（D-129）；“我需要你先把当前的壳和现有已有的应用移植或参考重写搞到成熟可用”（D-131）。2026-11-13 又说：“对 按着实现 参考安卓也行 但是要注意设备特性 不要生搬硬套导致使用体验反而不佳”（D-140）。

调研见 `docs/tasks/replan-sysapps/APPS.md` A10 和 `CURRENT.md` A10。成熟文件管理器有、我们缺的是：在文件夹里搜索、选排序方式、操作遇到重名时逐个决定怎么办、做完后能看懂的成功和失败列表、解开压缩包。

## 现状（2026-11-13 按 `claude/system-layer` 16e7be5f 核对；design-lang-2、home-1、lock-1、control-1、settings-1、notify-svc-1、transfer-2、usb-disk-1 都已合入）

**应用：`system/apps/files/files.c`，一个文件 478 行**

- **页面**（第 16、457 行）：Files（首页）、FileList、FileActions（文件菜单）、FileMove（放到哪里）、FileName、FileEdit。
- **能力**（第 462-476 行）共 13 个：`files.new/edit/multi/mark/all/browse/select/move/name/preview/next/prev/up`。
- **列表**（`listed`，第 212-246 行）：
  - 先用 `file.list` 取直接子文件夹，再用 `file.index` 逐页（每页 10 条）读**整棵子树**的索引，只留直接子文件。子树里其余的条目读了就丢（第 226 行）。
  - 全部读完才刷新。顺序是：文件夹按卡上的顺序，文件按编号。**没有排序、没有搜索、没有修改时间。**
- **列表行**（`bind_item`，第 169-210 行）：图标（folder/book/headphones/camera，第 175 行）、名字、大小。图片有缩略图（`file.thumb`）。
- **点文件 → FileActions**（`selected`，第 252-291 行）：
  - 页面上有文件名输入框和“改名”（`file.move`）。
  - “打开”按类型分派：text 用 `files.preview`，audio 用 `media.play`，image 用 `photo.open`。其它类型写“这种文件暂时打不开”。
  - 还有拷贝、移动、删除（`file.delete`，有确认卡）。`.txt` 多一个“编辑文本”。
  - **问题**：索引把 `.epub .pdf .mobi .azw` 也算 text（`system/services/file/file_service.c:1597`），所以点电子书的“打开”会预览压缩包的二进制开头。
- **多选**（第 299-308 行）：只能勾文件，文件夹不行。每勾一个就 `ink_ui_redraw()` 整页重建，不符合 D-127（只刷变了的那一块）。
- **目的地**：用自己的 FileMove 页（第 367-379 行）。单个文件走 `file.copy`/`file.move`，多选走 `file.batch`（参数在 `batch_args` 里拼，第 134-139 行）。
  - 原 brief 说用系统文件选择器 F9 选目的地，**这不对**：`ink_file_picker_open(type, done(id))`（`system/include/ink/sys/file_picker.h:9`）只能选文件、返回编号，选不了文件夹。目的地继续用我们自己的页。
- **批量进度**（`batch_progress`，第 439-447 行）：写“已处理 n/m”。失败的写成“未完成：名字”，**不写原因**。
- **首页**（第 402-418 行）：家庭、我的；大人另有其他家人的文件，以及“从手机或电脑传入”和“连电脑”。
- **控件**：返回键用 `ink_ui_circle`（design-lang-2 以后不画圈了），按钮用 `ink_ui_pill`。

**文件服务：`system/services/file/`**

- `file.batch`（`file_caps.c:495`，执行在 217-290 行）：
  - 按编号逐个拷贝或移动。目的地有同名时，`fs_move_unlocked`（`file_service.c:578`）和 `job_begin_unlocked`（`:1006`）都拒绝，返回 `INK_FS_ERR_EXISTS`。
  - 结果里有 `failures`（只有名字，最多 20 个，`BATCH_FAILURE_MAX`，第 25 行），以及 `total`、`succeeded`、`failed`、`more`。
  - **没有“重名怎么办”参数，没有原因，停不下来**：`ink_task_begin_for(..., NULL, NULL)` 没登记停下的回调，而系统已有 `job.cancel`（`system/services/res/task.c:269`）。
- `file.delete_many`（`:497`）：确认卡写的是“删除这些文件 {ids}{all_path}，不能撤销”，把编号直接念出来了。
- **修改时间**：
  - 索引记录本来就存着修改时间（`file_service.c:1460` 的 `index_rec_t.mtime`）。但 `ink_fs_entry_t`（`system/include/ink/sys/file.h:62`）和 `file.index` 的输出里没有。
  - `ink_fs_stat_t`（`file.h:55-59`）和 `file.list` 的输出里也没有，虽然 HAL 的目录项带着 `mtime`（`platform/include/ink/hal_storage.h:59`）。
- **压缩包**：固件里的 miniz（`CMakeLists.txt:335-338`）编译时定义了 `MINIZ_NO_ARCHIVE_APIS` 和 `MINIZ_NO_DEFLATE_APIS`，**只能解压数据流，不能读 zip 目录，也不能压缩**。能读 zip 的是 Mogo 已经在固件里的 `third_party/mogo/main/book/zip_reader.c`（Apache-2.0）：只读，单个条目最多 8 MiB，拒绝加密和 ZIP64。
- **数据库名额**：`system/services/data/datastore.c:44` 的 `MAX_DEFS` 是 32，现在登记了约 28 个。

**测试**

- 相关的测试：`files-app`（`tests/files_app/run.py` 用改源码字符串的方式做反向对照，`files_app.c` 有 88 个检查）、`files-ops`、`integration-files`、`file-index-*`。
- 下列测试按页面名或位置找 Files 页面：`tests/ui_fidelity.py`、`audit3_fix.c`、`textfield_forms.c`、`settings_gaps.c`、`i18n_shell_remaining.c`、`transfer_pages/`、`ui_fix_e/test.c`。
- AI 能力表：`tests/integration/ai_surface.c:51-52`、`:130-131`。

## 成熟来源（已定，不重新选）

- **Amaze**（操作流程第一参照）：`github.com/TeamAmaze/AmazeFileManager`，提交 `5f1aea6829978aff67f97bf6f00ddd9fa9e3723e`，GPL-3.0，**只照着重写，不拷代码**。要看的地方：
  - `asynchronous/asynctasks/movecopy/PreparePasteTask.kt`
    - 109-183：移动到原文件夹报错、空间预检、“检查冲突”。
    - 185-213：找同名。
    - 215-300：重名对话框（改名 / 跳过 / 覆盖，加“全部都这样”；同一文件夹里覆盖不可用）。
    - 302-336：处理“全部都这样”。
    - 338-395：全部跳过时的提示。
  - `filesystem/FilenameHelper.kt:134-175、215-260`：“保留两个”的命名，默认格式 `name (1).ext`。
  - `asynchronous/services/CopyService.java:238-296、367-470`：失败集合。`AbstractProgressiveService.java:328-360`：失败通知和失败列表。
  - `filesystem/compressed/CompressedHelper.java:90-151`：按压缩包类型分派解压器。
  - `ui/dialogs/GeneralDialogCreation.java` 约 220-330 行：删除确认（列出要删的、总大小、“永久删除”勾选）。
  - `README.md:22-34`：功能清单。
- **Fossify File Manager**（搜索、排序、目的地）：`github.com/FossifyOrg/File-Manager`，提交 `1e43648eadfdbae862f1fd2f57ac68b3b7b35357`，GPL-3.0，只照着重写。要看的地方：
  - `activities/MainActivity.kt:191-247`：菜单项（排序、加入和去掉收藏、转到收藏），以及搜索入口。
  - `fragments/ItemsFragment.kt:305-399`：少于 2 个字时提示；在当前目录和子目录里递归找；结果按所在目录分组。
  - `dialogs/ChangeSortingDialog.kt:17-95`：名字、大小、修改时间、扩展名，正序倒序，按数字排，只用于这个文件夹。
  - `adapters/ItemsAdapter.kt`：173-186（选中后能做什么）、509-560（拷贝到、移动到，用的是文件夹选择器）、910-923（删除确认）。
  - `activities/DecompressActivity.kt`：先列出压缩包内容，再解压。
- **Flipper Zero Archive**（C 代码，**拷原文**）：`github.com/flipperdevices/flipperzero-firmware`，提交 `3060cee9bac71d332dc7629191ae1aeb1edaac33`，**GPL-3.0**。行号已按这个提交核对，比 SOURCES.md 写的精确：
  - `applications/main/archive/views/archive_browser_view.c:51-97`：`render_item_menu`，文件菜单怎么组成。
  - `applications/main/archive/helpers/archive_files.c:9-38`：`archive_set_file_type`。
  - `applications/main/archive/helpers/archive_files.c:78-114`：`archive_delete_file`，删除后同步收藏。
  - `applications/main/archive/helpers/archive_browser.h:24-39、64-66`：`known_ext` 表、`archive_is_known_app`。
  - `applications/main/archive/helpers/archive_favorites.c`：收藏的读、删、查、改名、后台清理（`archive_favourites_rescan`，94-137 行）。
  - `applications/main/archive/scenes/archive_scene_browser.c:14-90`：按类型交给对应应用打开。
  - `applications/main/archive/scenes/archive_scene_browser.c:120-265`：`archive_scene_browser_on_event`，浏览和菜单事件。
  - 改名场景里收藏跟着改：`scenes/archive_scene_rename.c:62-90`。

读源码用固定提交的浅克隆，放在自己的临时目录里，不放进仓库。

## 要搬的代码（第 11 节）

**放哪、怎么编译**

- 原文放 `system/apps/files/vendor/flipper/`，照 `system/apps/gomoku/vendor/crossmux` 的做法。
- 每个文件保留原来的头部，旁边放 GPL-3.0 全文 `LICENSE`，再在 `third_party/licenses/catalog.json` 末尾加一条（关于页的许可列表会出现它）。
- 全部按 **C** 编译。原文目录用单独一组编译选项（关掉原文的告警，照 `ink_miniz` 的 `-w`），**不为了迁就告警去改原文**。

**拷哪些**（整文件或整函数）

1. `helpers/archive_files.c` 整个文件，加上 `archive_files.h`。
   - 用到 `archive_set_file_type` 和 `archive_delete_file`。
   - `archive_get_items` 保留，但不接线。
2. `helpers/archive_favorites.c` 中不依赖浏览视图模型的全部函数：`read_line`、`commit_temp`、`count`、`rescan`、`delete`、`is_favorite`、`rename`、`add_to_favorites`。
3. `views/archive_browser_view.c` 里整个 `render_item_menu`，加上 `archive_browser_view.h` 里的 `ArchiveTabEnum`、`ArchiveBrowserEvent`、`ArchiveBrowserViewModel`。
4. `helpers/archive_browser.h` 里的 `known_ext[]` 和 `archive_is_known_app()`，放进 `archive_browser_ink.h`。其余内容依赖 `archive_i.h`，那一大套不拷。
5. `scenes/archive_scene_browser.c` 里整个 `archive_get_flipper_app_name`、`archive_run_in_app`、`archive_scene_browser_on_event`。

**我们写的垫片**（另放文件，算进产品代码预算）

- `furi.h`、`m-array.h`、`storage/storage.h` 三个同名小头文件：
  - `FuriString`：定长缓冲，只实现原文用到的那几个函数。
  - `furi_assert`、`furi_record_*`、`COUNT_OF`。
  - `ARRAY_DEF` 空定义，加上 `files_array_*` 对到我们的条目数组。
- **画布记录器**：`canvas_draw_str` 把菜单项记成一个数组，由我们的 LVGL 页画出来。其余 `canvas_*`、`elements_*` 和图标都是空操作。
- **收藏的存储垫片**：原文把收藏写成卡上的文本文件，一行一条路径，再用临时文件换掉旧文件。我们的路径不能以点开头，也不能放在用户看得见的位置，所以垫片把 `ARCHIVE_FAV_PATH` 和 `ARCHIVE_FAV_TEMP_PATH` 这两个“文件”换成按家人分开的 `files.prefs` 里的一条文本记录（最多 32 行），读、定位、追加、改名、删除都在内存里做。
  - `storage_file_exists` 查的是真实路径，要读卡，所以 `archive_favourites_rescan` 整个放到后台执行器上跑。
- **事件到能力的接线**：`view_dispatcher_send_custom_event` 直接调用 `on_event`。`scene_manager_next_scene` 和 `archive_show_file_menu` 换成推入我们的页面。`archive_run_in_app` 里启动应用的那一句换成对应能力（见对照表 D-35 行）。`view_dispatcher_stop` 换成 `ink_ui_pop`。
- **删除**：确认卡（D-105）和读卡的删除是异步的。所以 `archive_delete_file` 在 `file.delete` 的回执回来后才调用，垫片里的 `storage_common_remove` 返回这次回执的结果。原文的先后顺序不变：删除 → 同步收藏 → 从列表去掉。

**原文要改的行**（逐行记进 SOURCE.md）

- `known_ext` 换成本机的类型：文本、书、音频、图片、压缩包、文件夹、未知。
- 去掉 BadUSB 那条“`.txt` 在 BadUSB 目录里才算脚本”的路径规则。
- `MENU_ITEMS` 改大，`render_item_menu` 末尾追加我们的菜单项：拷贝、移动、详情、编辑、解压。
- 原文在删除失败时也会去掉收藏（`archive_files.c:105-107`）。是否改成“删成功才去掉”由开发判断，改了就记原因。

**停下的规矩**：哪个垫片需要改原文函数的控制流程才能接上，就停下，把证据（文件、行号、为什么）写进“要用户定”。不许悄悄换成自己写。

**SOURCE.md**（`system/apps/files/vendor/flipper/SOURCE.md`）写这些：
- 项目和 URL，固定提交 `3060cee9…`，许可 GPL-3.0。
- 每个拷来的文件：原路径、保留的行号范围、原文件在这个提交的 SHA-256。
- 改了哪些行、为什么。
- 没拷的部分和原因：128×64 的绘制、标签页、应用和 U2F、场景管理器、收藏排序的接线。
- 我们写的垫片清单。
- 原文没有归档模块的测试可以带过来，列出我们为原文函数写的宿主测试。

**许可**：Flipper 是 GPL-3.0。总规则第 4 节写着“第三方资源只用许可清楚的（不用 GPL）”，`vendor-common.md` 第 12 条也写着“GPL 的不用”。第 11 节（D-140）点名要拷 Flipper，并且写明冲突时以第 11 节为准，所以照拷。但要做到三点：放在单独目录、单独提交、合入主线前由主会话把许可后果问用户（见“要用户定”）。自己用不受影响。

## 要做的（逐项对应对照表）

1. **浏览**：
   - 标题写当前文件夹，下一行写完整位置“我的 › 文档 › 报告”，每一级都能点；太长就从左边省略，不横向滚动。
   - 每行：类型图标（不套圆圈）、名字、大小、修改时间（“今天 14:05”“11月12日”）。设备时钟没对过、时间早于 2020 年时写“时间未知”。文件夹写“文件夹”。图片照旧有缩略图。
   - 分页写页码，盖板的上一页、下一页键能翻。
   - 回到上一级时，回到进去之前的那一页、那一行。
   - 文件夹自己的操作在文件夹页顶上的“这个文件夹”按钮里（改名、移动、拷贝、删除、收藏、详情），根目录没有这个按钮。
2. **排序**（照 Fossify）：
   - 排序页有两组分段控件：名字、时间、大小、类型；正序、倒序。点一下就生效，回到列表。
   - 文件夹总在文件前面。名字排序认数字（第2章在第10章前）。中文按字符编码排，不按拼音，写进报告。
   - 每位家人记住自己的选择，存在新的按家人数据集 `files.prefs` 里（收藏也存这里，只占一个名额）。
3. **搜索**（照 Fossify `ItemsFragment:305-399`，按设备改）：
   - 文件夹页有“搜索”按钮，进入搜索页：一个共用输入框，按“搜索”才开始找。
   - 中文 1 个字就能找，英文和数字至少 2 个字符。
   - 在当前文件夹和子文件夹里按名字找，不分大小写。**不另建索引**：用列这个文件夹时 `file.index` 本来就读到的整棵子树（现在读了就丢），留在 PSRAM 里。
   - 文件夹名从文件路径里取，所以空文件夹搜不到，写进报告。
   - 结果每行的第二行写所在位置。点一个结果，进到它的文件夹、翻到它所在那页并标出来（名字加粗，写“找到”）。
   - 没找到用共用空状态。索引还没建好时，写“刚放进来的文件可能要等几秒才搜得到”。
4. **文件菜单**（Flipper `render_item_menu` 原文决定有哪些项）：
   - 点一个文件就进菜单页，第一项是按类型的“打开”：
     - 文本：预览开头。
     - 书（`.epub`，以及 `.txt` 也给）：用阅读打开，走 reader-one-1 以后按路径打开的能力；它没合入时用现在的 `mogo.open`，只认 `/me/` 下的路径。
     - 音频：`media.play`。
     - 图片：`photo.open`。
     - zip：解压。
   - 接着是：收藏或取消收藏、改名（收藏跟着改）、拷贝、移动、删除（确认卡，删完从收藏里去掉）、详情（名字、位置、大小、修改时间、类型；文件夹此时才读一次项数）。`.txt` 多一个“编辑”。
   - 不能用的项不画（Flipper 画成“---”）。
   - 原 brief 写的“图片多一个问 AI”**不做成单独的菜单项**：`photo.ask` 只能在照片页里用（`system/apps/gallery/gallery.c:255` 有 `.activity = "Photo"`），图片“打开”以后照片页本来就有“问 AI”。
5. **多选和批量**（照 Amaze）：
   - 底部“多选”按钮。进入后每行右边出现勾选框，点一行勾或取消，**只刷这一行和“已选 n 个”**。另有全选、拷贝、移动、删除；没选时灰掉，写“先选文件”。
   - 选好后进“放到哪里？”：
     - 从这位家人上次放过的地方开始（记在 `files.prefs`）。
     - 根目录列出收藏、我的、家庭，只列能写的位置。有写着字的“新建文件夹”按钮。
     - 在原文件夹里“放到这里”灰掉，下面写“已经在这个文件夹里”。
   - 点“放到这里”以后：
     1. 先调 `file.batch {"check":true}`，拿回同名清单、总大小和卡上剩余空间。放不下全部时写“需要 12 MB，还剩 8 MB，放得下的先做”，等人点“开始”。
     2. 有同名就一页问一个：写名字，原有的和新的各自大小和时间，“第 1 个，共 2 个”。三个写字按钮“保留两个”“跳过”“替换”，加一个开关“后面的同名都这样”。在原文件夹里拷贝时没有“替换”。返回键取消整批（这时什么都还没做）。
     3. 选了替换的，开始前弹**一次**系统确认卡：“替换 2 个同名文件，原来的找不回来”。
     4. 进度只刷“已处理 3/10”那一行，最多每 2 秒一次。有“停下”按钮（`job.cancel`）。
     5. 做完换到结果页：第一行“成功 1 个，跳过 1 个，失败 1 个”，下面每个失败一行，写名字和原因（空间不够、名字不合法、正在做别的拷贝、找不到了、没有权限……），多了就翻页。全部跳过时写“都跳过了，没有改动”。
     6. 点“完成”回到原来的文件夹和那一页。拷贝的话勾选保留，移动的话清掉已经移走的。
   - 离开页面后，结果在“正在做的事”里看得到（`ink_task`）。
   - **只能勾文件，不能勾文件夹**：批量按文件编号做，文件夹没有编号。文件夹从“这个文件夹”菜单单独操作。
6. **删除**：照旧走 `file.delete` 和 `file.delete_many`，找不回来，要弹确认卡。`file.delete_many` 加 `prepare_confirm`，确认卡写成“删除这 3 个文件（报告.txt、草稿.txt……），找不回来”，新文案在 `en.tsv` 末尾追加。
7. **解压**（照 Amaze 的分派，按设备改）：
   - 只做 `.zip`，解到同名文件夹，结果页同第 5 项。
   - 文件服务加 `file.extract {path}`，用固件里已有的 Mogo `zip_reader.c`。单个条目超过 2 MiB、加密或 ZIP64 的记为失败并写原因，其它种类的压缩包写“这种压缩包解不开”。
   - 预算不够就整项不做，写进“没做到的”。
8. **收藏**（Flipper 原文加 Fossify 的入口）：
   - 首页最上面一组“收藏”，每个一行：文件夹点了直接进去，文件点了打开菜单。
   - 读收藏时在后台跑原文的 rescan，被别处删掉或移走的自动去掉。按收藏的先后排列。
9. **文件服务的增量改动**（只加，不改原有字段的意思；逐条写进报告）：
   - **修改时间**：
     - `ink_fs_entry_t` 末尾加 `int64_t mtime`，在 `fs_index_for`（`file_service.c:1901`）里从索引记录填进去。**gallery-3 要加同样的字段、同样的填法**：谁后合入谁对齐，不能出现两份。
     - `cap_index_query` 的输出加 `"mtime"`（秒）。
     - `ink_fs_stat_t` 末尾加 `mtime`，在 `ink_fs_dir_next` 里从目录项填进去，`file.list` 的输出加 `"mtime"`。
   - **`file.batch`**：
     - 加 `check`（布尔，什么都不做，只回报 `conflicts:[{id,name,size,mtime,existing_size,existing_mtime}]`、`bytes`、`free`）。
     - 加 `conflict`（`fail`、`skip`、`keep_both`，不传等于 `fail`，也就是今天的行为），以及 `skip_ids`、`keep_both_ids` 两个按编号单独指定的数组。
     - `keep_both` 在服务里、同一把锁里取“名字 (n).扩展名”。
     - 结果在 `failures`（名字）旁边加同顺序的 `reasons`（`ink_fs_err_name` 的原词），再加 `skipped`、`renamed`。
     - `file.batch_progress` 事件加 `reason`、`skipped`、`task`。
     - 登记停下的回调：当前这一项做完就停，没做的算“没做”。
   - **新能力 `file.batch_replace`**：和 `file.batch` 用同一个处理函数，`replace_ids` 里的先删掉目的地同名文件再放进去。效果是 `INK_FX_LOCAL_DESTRUCTIVE`，`ask_by_hand`，确认卡写数目和前两个名字。理由：替换会让原来的文件找不回来（D-105）。
   - **新能力 `file.extract`**（第 7 项做才加）。
   - `file.delete_many` 的 `prepare_confirm`。

## 共同要求（页面任务都一样）

- **新设计语言**：只用令牌和共用控件（`system/ui/ink/ui_theme.h` 里的 `ink_ui_text_button`、`ink_ui_pill`、`ink_ui_list_row`、`ink_ui_segments`、`ink_ui_toggle`、`ink_ui_empty`、`ink_ui_pagebar`，`system/ui/ui_list.c`，`system/ui/textfield.c`）。
  - 能点的不小于 108 px，一屏最多一个主按钮，只用一种输入框，行间细线、组间留白，图标不套圆圈，没内容的区块不画。
  - 不写死像素和颜色（`ui-scale` 检查）。旧画板的对照分数（`tools/ui_compare.py`）不作验收。
- **墨水屏**（第 11 节）：
  - 不用动画、转圈、惯性滚动，不用一直在变的数字。
  - 一次操作只刷一次，只刷变了的那块，按下反馈只刷按下的那个控件。
  - 不用自动消失的提示条，也不为了让提示消失再刷一次。
  - 不靠颜色区分状态。
  - 不用下拉刷新、浮动按钮、汉堡菜单，不用长按、滑动删除、拖拽。
  - 大块数据（条目、子树、搜索结果）放 PSRAM，不后台轮询。
- **D-138**：文件应用不需要卡上的额外文件。没卡、卡借给电脑（写“拔线或在电脑上推出后自动回来”）、读不了、索引还没建好、超过 1024 个文件（写“只列出前 1024 个”），每种情况用共用空状态写一句话，不白屏。卡回来或索引好了自动重读：订阅 `usb.changed`、`file.index_state`，不轮询。
- **能力**：
  - 每个用户操作都是能力，按钮和 AI 调同一个。新能力：`files.search {query}`、`files.sort {by,desc}`、`files.fav {path}`、`files.folder`（“这个文件夹”菜单）、`files.conflict {choice,all}`、`files.extract {id}`（第 7 项做才加）。
  - 现有能力名不改，改了或删了的逐条列进报告。
  - 页面快照写出：当前位置、排序、搜索词、可见文件（带修改时间）、已选的、菜单项、重名问题（第几个、共几个）、结果（各项计数和失败原因），跟页面上显示的一致。
- **不变的规矩**：家人隔离（只看得到自己能看的空间）；孩子身份（只看到我的和家庭，家庭只读）；确认卡只给 D-105 的高风险动作（找不回来的删除和替换）；按下反馈 D-31；密码不给 AI、不进日志。
- **文字大小**：跟系统“文字大小”一起缩放（`ink_ui_px`）。放不下就分页或换行，不缩字，不裁按钮。
- **原地重写**：在分支里替换旧页面，可以把 `files.c` 拆成几个文件，旧代码删掉，不留两套。合不合入、何时换，等用户看预览后由主会话定。
- **共享文件**：
  - `docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交。
  - 新能力进 `tests/integration/ai_surface.c` 的两张表：给 AI 的写清必填参数，不给 AI 的写理由。
  - 新文案的英文加进 `assets/i18n/en.tsv`。
  - `third_party/licenses/catalog.json` 加一条。
  - 这些文件都只在末尾追加本任务的行，合并冲突由主会话处理。
- **截图**：放 `docs/tasks/files-3/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。默认字号、最大字号、孩子身份各一套。

## 范围和预算

- **只改**：
  - `system/apps/files/`，可以拆文件；新增 `vendor/flipper/`。
  - 文件服务里第 9 项列的几处：`file_caps.c`、`file_service.c`、`file.h` 的两个结构末尾。
  - 文件相关的测试和夹具（文件名一看就是假的）。
  - 根 `CMakeLists.txt` 和 `ports/esp32s31/main/CMakeLists.txt` 里新文件的那几行。
  - 第 9 项要求的共享文件末尾，以及 `docs/tasks/files-3/`。
- **不碰**：
  - 传书：`system/services/file/transfer.c`、`system/ui/shell/transfer_pages.c`。首页那两个按钮照旧调用它们。
  - 文件选择器 `system/ui/file_picker.c`。
  - 相册和 `photo.*`（gallery-3）。
  - 媒体服务（music-3）。
  - `third_party/mogo/`：只调用 `zip_reader.h`。
  - 主题和共用控件文件。
- **预算**：
  - 产品代码不超过 600 行：新写的、改的、垫片和服务改动都算；拷来的 Flipper 原文不算；删掉的旧代码不抵。
  - 超了先砍解压，写进“没做到的”。还超就停下报告，不压缩功能硬塞。
  - 固件增量不超过 20 KB。
- **起步**：主会话给的分支，也就是当前的 `claude/system-layer`，已含 design-lang-2 等。

## 验收

1. **对照表逐行**：每行配截图或测试名。报告写做到了几行、哪几行没做。
2. **照参照路径走一遍**（`APPS.md` A10，模拟器脚本，每一步截图并断言）：
   1. 在一个有子文件夹的文件夹里搜“报告”，结果写出各自所在的位置。
   2. 点一个结果，进到它的文件夹，翻到它那页，并且它被标出来。
   3. 回到原来的地方，多选 3 个文件，选**拷贝**，选一个目的地。目的地里已有 2 个同名文件，而且卡上只剩放得下 1 个文件的空间。
   4. 预检写“放得下的先做”，点“开始”。第一个同名选“保留两个”，第二个选“跳过”。
   5. 结果页写“成功 1 个，跳过 1 个，失败 1 个：空间不够”，并且出现“报告 (1).txt”。
   6. 点“完成”，回到原来的文件夹和那一页，勾选还在。

   再用移动走一遍：重名时选“保留两个”，移走的从列表里消失。同一卡上移动就是改名，不会因为空间失败，所以空间那一步用拷贝。

   同样两条路径用能力调用走一遍（来源是 AI），结果一样。

   反向对照：去掉重名处理（直接覆盖），“保留两个”的断言必须失败。
3. **排序**：按大小倒序，换家人再换回来，选择还在。
4. **删除**：删 3 个文件，确认卡写“删除这 3 个文件”加名字；点取消，3 个都还在。替换走确认卡，点取消就什么都没动。
5. **家人隔离**：A 的空间 B 看不到、搜不到、收藏不到。孩子看不到“其他家人的文件”，在家庭里改不了。
6. **收藏**：收藏一个文件 → 在菜单里改名 → 收藏里是新名字 → 删除 → 收藏里没了 → 再收藏一个，用 AI 的 `file.move` 把它移走 → 回首页时收藏里没了（rescan）。
7. **Flipper 原文**：SOURCE.md 里的哈希和原文件一致；改动行逐行有理由；宿主测试覆盖类型表、菜单组成（文件、文件夹、收藏标签、未知类型）、删除和改名同步收藏、每个浏览事件对应到能力。
8. **刷新次数**（报告写实测，模拟器数刷新区域）：
   - 进文件夹 2 次：先出框架，再刷列表区。
   - 翻页 1 次。勾选 1 次（只刷那一行和计数）。换排序 1 次。出搜索结果 1 次。开菜单 1 次。
   - 每个重名问题 1 次。进度每 2 秒最多 1 次，只刷那一行。结果页 1 次。
9. **截图**：目录页、搜索结果、排序页、多选、放到哪里、重名问题、结果页、文件菜单、文件夹菜单、首页收藏、空状态和出错状态。默认字号、最大字号、孩子身份各一套，加上改前的对照。
10. **构建和测试**：
    - 主工程普通全量 ctest 过（含 `ui-scale`）。先清掉 build/test-out 再跑。
    - ASan 跑文件相关测试和 `ui-scene-*`，过。
    - S31 宿主机测试过。`ports/esp32s31/build.sh` 编译通过，没有新警告。
    - 报告写：固件增量、产品代码行数（不算原文）、`gcccheck` 结果。
    - **改过的已有测试逐条列出**：`files_app/run.py` 的改源码反向对照按新代码重写，意图和严格程度不变；页面定位和结构的改动不改业务断言。
    - 报告还要写：改名或删除的能力、文件服务改了什么。

## 要用户定（开工不等，合入前由主会话问）

1. **Flipper 的许可**：GPL-3.0 和总规则第 4 节“不用 GPL”冲突。另外固件里 Mogo 部分内容是 Kiikoread 个人学习许可，和 GPL 不能一起对外发布。
   - 自己用不受影响。
   - 以后要把固件给别人，有两条路：整个按 GPL 发布（还得先解决 Mogo 那部分），或者删掉 `vendor/flipper/`，把那几处换成对照表里的同样行为（那时算改写）。
   - 默认：照 D-140 拷，隔离、单独提交。
2. **这次不做的参照功能**（默认不做）：
   - 回收站（Amaze；建议等 gallery-3 的“最近删除”做成通用的再接）
   - 压缩（固件没有压缩算法）
   - 最近文件（Fossify）
   - 排序只用于某个文件夹
   - 多选里勾文件夹
   - 收藏调顺序
   - 网格视图
   - 中文按拼音排序

## 和本批其他任务的边界

- **gallery-3**：都要改 `ink_fs_entry_t.mtime` 和 `fs_index_for`，只能有一份，后合入的对齐先合入的。相册的“最近删除”文件夹在文件应用里照常显示，不特殊处理。
- **music-3**：文件应用只调用 `media.play`。能力名改了就跟着改，列进报告。
- **reader-one-1**（可能先合）：用阅读打开，走它的按路径打开能力；它没合入时用 `mogo.open`。
- **auto-provision-1、settings-d137、camera-touch-bus、almanac-2、phone-3、memo-3、lock-styles-1**：没有共同文件。共享文件只在末尾追加。
