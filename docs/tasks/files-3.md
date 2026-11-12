# files-3：文件管理照 Amaze 和 Fossify File Manager 重写——目录里搜索、排序、多选、选目的地、重名怎么办、成功失败逐项列出；文件类型和长按菜单参考 Flipper 的 C 代码

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。

## 为什么

用户 2026-11-12：“所有的应用和系统应用都是移植的最优秀的三方项目吗 我怎么感觉不太像呢”；“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”（D-129）；“我需要你先把当前的壳和现有已有的应用移植或参考重写搞到成熟可用”（D-131）。

调研（`docs/tasks/replan-sysapps/APPS.md` A10、`CURRENT.md` A10）：现在的文件（`system/apps/files/files.c`）有类型和大小、分页、缩略图、详情、改名、新建文本、多选、批量复制移动删除、能编辑文本；缺的是成熟文件管理器的目录里搜索、可选的排序、操作遇到重名时逐项怎么处理、做完之后可读的成功和失败列表、压缩包解开。

## 成熟来源（照着重写，片段可搬）

- **Amaze**（操作流程第一参照），`github.com/TeamAmaze/AmazeFileManager` 提交 `5f1aea6829978aff67f97bf6f00ddd9fa9e3723e`（只读克隆到 `work/`）：`app/src/main/java/com/amaze/filemanager/asynchronous/services/CopyService.java:238-296`（复制移动的结果和失败集合）、`filesystem/compressed/CompressedHelper.java:90-151`（压缩包类型到解压器的分派）、`README.md:22-34`（功能清单）。重名时的“替换、跳过、保留两个、以后都这样”对话在同一仓库，照它翻。
- **Fossify File Manager**（查找和排序），`github.com/FossifyOrg/File-Manager` 提交 `1e43648eadfdbae862f1fd2f57ac68b3b7b35357`：`app/src/main/kotlin/org/fossify/filemanager/activities/MainActivity.kt:191-247`（搜索和排序入口）、`fragments/ItemsFragment.kt:305-399`（目录里递归搜索和结果排序）。
- **Flipper Zero**（C 代码可参考），`github.com/flipperdevices/flipperzero-firmware` 提交 `3060cee9bac71d332dc7629191ae1aeb1edaac33`：`applications/main/archive/views/archive_browser_view.c:54-112`（文件类型、当前选择、长按菜单）、`helpers/archive_files.c:80-114`（删除、改名、收藏的一致处理）、`scenes/archive_scene_browser.c:1-120`（浏览和菜单事件）。

## 要做的

1. **浏览**：当前位置写清（路径可点，回上一级）；每个文件一行——类型图标（不套圆圈）、名字、大小或项数、修改时间；图片照旧有缩略图；分页照旧。
2. **排序**（照 Fossify）：名字、时间、大小、类型，正序倒序；每位家人记住自己的选择（存在现有的按家人设置里）。
3. **搜索**（照 Fossify `ItemsFragment:305-399`）：在当前目录及子目录里按名字搜，结果写出所在目录，点结果进到那个目录并选中它；能用现有的文件索引（`file.index`）就用，不另建索引。
4. **长按菜单**（参考 Flipper）：按文件类型给合适的动作——打开、改名、复制、移动、删除、详情；文本多一个“编辑”；图片多一个“问 AI”（现有能力）。
5. **多选和批量**（照 Amaze）：多选 → 复制或移动 → 用系统文件选择器（F9，`system/ui/file_picker.c`，只调用）选目的地 → 遇到重名逐个问“替换、跳过、保留两个（自动改名）”，可勾“以后都这样” → 进度照现有的 `file.batch_progress` → 做完给一页结果：成功几个、失败几个，失败的逐项写原因（空间不够、名字不合法、被占用……）→ 回到原来的位置和选择。文件服务的 `file.batch` 现在没有“重名怎么办”的参数：在服务里**增量**加一个，列进报告。
6. **删除**照现有规则（`file.delete`、`file.delete_many` 是找不回来的删除，照旧弹确认，写清动词“删除这 3 个文件”）。
7. **解压**（照 Amaze 的分派）：`.zip` 用固件里已有的 miniz 解到同名目录，结果页同上。时间不够就不做，写进“没做到的”。

## 共同要求（页面任务都一样）

- 来源：功能、页面、状态、出错处理照上面的来源一个不落，不自己发挥；`docs/tasks/files-3/SOURCE.md` 写每一页对应来源的哪个文件和行号，哪些没照搬、为什么（Amaze 的 root、云盘、FTP 插件，Fossify 的隐藏文件、收藏夹，这次不做的逐项列为“没做到的”）。
- 新设计语言（`design-lang-1`）：只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/ui_list.c`、`system/ui/textfield.c` 里的按钮、输入框、列表行、分段、空状态）；能点的不小于 108 px；一屏最多一个主按钮；一种输入框；行间细分隔线、组间留白；图标不套圆圈；没内容的区块不画，空目录才用共用空状态。不写死像素和颜色（`ui-scale` 检查）。旧画板的对照分数（`tools/ui_compare.py`）不作验收。
- 能力：每个用户操作都是能力（`system/include/ink/sys/cap.h`），按钮和 AI 调同一个；页面快照列出当前位置、可见文件、已选的和能做的动作，跟页面一致。现有能力名（`files.*`、`file.*`）尽量不改，改了或删了的逐条列进报告。
- 不变的规矩：家人隔离（只看得到自己能看的空间）、孩子模式、确认卡只给 D-105 的高风险动作（找不回来的删除）、少刷新（D-127：勾选只刷那一行，进度只刷进度那块）、按下反馈 D-31、密码不给 AI 不进日志。
- 文字大小：跟系统“文字大小”一起缩放（`ink_ui_px`），默认和最大一档各出一套截图；放不下就分页或换行，不缩字、不裁按钮。
- 原地重写（分支里替换旧页面，旧页面代码删掉，不留两套）；合不合入、新旧怎么换，等用户看预览后由主会话定。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由），新文案的英文加进 `assets/i18n/en.tsv`，这两个文件都只在末尾追加本任务的行；合并冲突由主会话处理。
- 截图放 `docs/tasks/files-3/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。

## 范围和预算

- 只改：`system/apps/files/`（可以拆成几个文件）、文件服务里 `file.batch` 加“重名怎么办”参数需要的几处（`system/services/file/` 内，列进报告）、文件相关测试和夹具（明显是假的文件名）、CMake 里新文件的那几行、`docs/tasks/files-3/`。
- 不碰：传书（`system/services/file/transfer.c`、`transfer_pages.c`，`transfer-2` 在改）、文件选择器 `system/ui/file_picker.c`（只调用，要改写进报告）、相册、主题和共用控件文件（`design-lang-1`）。
- 产品代码不超过 600 行（删掉的旧代码不抵）；固件增量不超过 20 KB。
- 起步：主会话给的分支（18:30 合并后的主线，再合上 `design-lang-1`）；合并排在 `design-lang-1` 之后。

## 验收

1. **照参照路径走一遍**（`APPS.md` A10，模拟器脚本，每步截图并断言）：在一个有子目录的目录里搜“报告”→ 结果写出各自所在目录 → 点一个进到它的目录并选中 → 回去多选 3 个文件 → 移动 → 选目的地（目的地里已有一个同名文件）→ 问重名时选“保留两个”→ 另一个同名选“跳过”→ 让其中一个因为空间不够失败 → 结果页写“成功 1 个，跳过 1 个，失败 1 个：空间不够”→ 回到原来的目录和位置。同一条路径用能力调用（来源是 AI）走一遍，结果一样。反向对照：去掉重名处理（直接覆盖），“保留两个”的断言必须失败。
2. **排序**：按大小倒序，换家人再换回来，选择还在。
3. **删除**：删 3 个文件弹确认，写着“删除这 3 个文件”；取消就都还在。
4. **家人隔离**：A 的空间 B 看不到、搜不到。
5. **改前改后截图**：目录页、搜索结果、多选、重名对话、结果页、长按菜单，默认和最大文字大小各一套。
6. 主工程普通全量 ctest 过（含 `ui-scale`）；ASan 跑文件相关测试和 `ui-scene-*` 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写固件增量、产品代码行数、`gcccheck` 结果、改过的已有测试（只改定位方式和页面结构，不改业务断言）、改名或删除的能力、文件服务改了什么。
