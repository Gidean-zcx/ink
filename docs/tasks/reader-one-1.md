# reader-one-1：只留一个阅读：嫁接的 Mogo 阅读改名“阅读”，接过旧阅读的应用编号、AI 能力名、“继续读”卡片和三处书；旧阅读退出构建；各家人的进度、书签和排版设置开机自动迁移；缺字体、空书架时页面上写清楚原因，并能用手返回

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1、2、6、9、11 条；第 8 条：不碰设备）。

## 为什么

- **D-139**（用户 2026-11-13：“只保留一个新阅读就可以了”）：新阅读（Mogo 嫁接）是唯一的阅读，名字叫“阅读”，放在首屏常用里。旧阅读 `apps/reader` 退役，代码留在 git 历史里。旧阅读的能力名（`reader.*`）、首页“继续读”卡片、三处书（全家的、传进来的、家人自己的）都交给新阅读；用到旧阅读的测试改走新阅读。这一条取代交付计划里“新旧并存，等用户说换”的安排。
- **D-138**（用户 2026-11-13）：升级要自动。旧数据自动迁移；缺什么，页面上说清楚原因，不能白屏。起因是试用版 21：卡上没有字体，新阅读打开后整页空白，也退不出来（`docs/hardware/2026-11-10-trial.md`“试用版 21”一节）。
- 试用版 21 查出四件事，这次一并修：缺字体时整页空白、没有返回；书架空着时什么也不说；没有用手点的返回键；图标不对。
- **和别的任务的关系**：交付计划原来把 `system/graft/*`、`third_party/mogo/`、`apps/mogo/` 划给 `reader-2`。本任务接过这几处的所有权；`reader-2` 的书架封面、分类、统计、手账、书摘**不在本任务里**。`lock-styles-1`、`music-3` 如果也需要改嫁接接口，只在它们的报告里写缺口，不改这里的文件。

## 来源（现状，都是读代码得到的【实现】）

**新阅读**
- 入口：`apps/mogo/mogo.c`。应用编号是 `mogo`，标题“新阅读”。唯一的界面叫 `MogoReader`。能力 22 个，全叫 `mogo.*`。
- 只有一个后台任务节点 `M.work` 和一个忙标志 `M.busy`：`ink_mogo_start` 在忙的时候直接返回 false，`ink_mogo_input` 在忙的时候返回 false，按键就漏给外壳了。
- 排版和页面接线在 `apps/mogo/port/ink_page_exports.h`（被 `third_party/mogo/main/apps/ink_book_page.c` 包含）。书架只扫 `/me/books`（`ink_mogo_scan` 用 `ink_fs_index`），而且扫书架前要先过 `ink_mogo_setup`，也就是先要能打开字体。
- 偏好、进度、书签都放在家人自己的集合 `mogo.preferences` 里，经 `apps/mogo/port/preferences.c` 模拟 NVS。`nvs_open` 用的是全局的 `ink_mogo_current_owner()`。这个集合**没有**用 `ink_db_register` 登记，所以备份服务把它的 `.snap`/`.log` 当作家人的普通状态文件原样备份（`backup_service.c:395-406`）。
- Mogo 原文的存法：
  - 进度在命名空间 `rp_books`，键是 `b_<fnv(路径)>`，内容是 `book_progress_t`，记文件大小、章节号、章内 UTF-8 偏移、字号、百分比；文件大小对不上就不恢复。最后一本记在键 `last`。
  - 书签在 `rp_marks`，键是 `m_<fnv(路径)>`，内容是 `reader_bookmarks_t`，每本最多 24 条。每条只有（章，偏移，saved_s），**没有摘录**。
  - 排版设置在 `reading/layout`。
- 字体：先找 `/me/fonts/InkSC.ttf`，没有就用 `/family/fonts/InkSC.ttf`，也就是卡上的 `fonts/InkSC.ttf`（`runtime.c:56-60`）。`ink_mogo_builtin` 只是 32 字节的空桩，没有内置字体。
- `ink_mogo_setup` 打开字体失败时返回 false，完成回调就不出图，页面一直是白的。这就是试用版 21 的白屏。
- EPUB：有 NCX/NAV 目录，但**没有加密检测**（`book_epub.c` 里没有 encryption 相关代码）。
- 登记：只在 `system/boot/main.c:98-102` 的 `#ifdef INK_MOGO_SIM` 里登记；主机由 `ink_mogo_build` 给 `ink_boot` 加这个宏，S31 给启动组件加。
- 借自旧阅读构建的两样东西：
  - 解压用的 `tinfl_*` 来自 `ink_reader_vendor`（`mogo.cmake:18`，改名宏是 PUBLIC 传下来的）。S31 上 `miniz.c` 已经挪进 `ink_miniz`，宏是从 `ink_reader_vendor` 复制过去的。
  - `ink_reader_host_call` 来自 `apps/reader/vendor/shim/reader_port.cc`（`runtime.c:113`）。

**嫁接接口** `system/graft/graft.c:147-155`：只许读写 `/me/`，外加只读 `/family/fonts/`。读不了 `/family/books/`，也读不了 `/user/…`。

**外壳的返回** `navigation.c:98-100`：`shell.back` 先调当前界面的 `on_key(INK_KEY_BACK)`，界面不接手才出栈。嫁接界面的 `on_key` 是 `ink_graft_key`，会转给 client 的 `input`。

**旧阅读** `apps/reader/reader.c`，1252 行。
- 能力 20 个：`SHELF_ACTIONS` 6 个、`BOOK_ACTIONS` 14 个（`reader.c:402-414`、`601-624`）。
- 界面四个：`reader`（书架）、`book`、`bookmarks`、`chapters`。
- 书从两处来：
  - `/content/books`，虚拟路径就是 `/family/books`（`file_service.c:232-234`）。
  - `/me/收件箱`（物理路径 `/content/.m/<家人>/收件箱`）。
- 离线示例是固件资源 `sample/welcome.txt`，书名“夜邮（示例）”，用固件里的点阵字体显示，**没插卡也能读**。
- 每位家人的数据都在**内部存储**：
  - `/user/m/<家人>/reader.last`：`page=…\nbook=…\noffset=…\nfile=…\nchapter=…`。
  - `/user/m/<家人>/reader.<crc32(key)>.pos`：`offset=…\nchapter=…`。key 的取法：全家的 TXT 是去掉 `.txt` 的文件名；EPUB 是带扩展名的文件名；收件箱是 `inbox/<文件名>`；示例是 `夜邮（示例）`。
  - 书签集合 `reader.bookmarks`（定义在 `bookmarks.cc:9`：家人集合，JSON，`ai_read=true`）：字段 `book`（物理路径）、`id`、`chapter`、`block`、`offset`、`end_*`、`page`、`added_at`、`excerpt`。
  - 每人一份的设置：`reader.font_size`、`line_spacing`、`paragraph_spacing`、`margins`、`footer_mask`，存在 `/user/sys/settings.json`，键名是 `reader.xxx:<家人>`。设置服务会保留本固件没定义的键（`settings.c` 文件头），所以旧阅读退役后这些值还在。
- TXT 的位置是原文件里的字节偏移：旧阅读把 TXT 原样读进来，不解码，所以 GBK 的书在旧阅读里本来就显示不对。目录由 `epub_source.cc` 解码得到。EPUB 的位置是（spine 章，块，块内偏移），块是旧解析器分的，和 Mogo 的 `html_text` 分块不一定一致。
- TXT 的页码是全书页码。
- 卡片已经监听 `storage.card`（`reader.c:1251`）。

**谁在用这些名字**
- 应用编号 `reader`：
  - `today_edit.c:8` 的默认常用（已经是 `reader`），`today_edit.c:80` 的继续读槽位。
  - `shell_home.c:65`（图标特判）、`shell_home.c:379`（分组表 `GROUP_IDS`，里面还有 `mogo`）、`navigation.c:189`。
  - `lock_faces/faces.c:93`：锁屏在 UI 上**同步**调 `.card`。
  - 孩子的应用开关；`members.c` 里孩子的白名单前缀是 `reader.`。
- 中枢：`hub/ink_hub/voice/flow.py:21` 写着 `APP_DOMAINS = {"book": "reader"}`（快照里的 app 是界面名 `book`）。
- 系统：
  - 开机引导：`boot_flow.c:229` 绑定 `reader.open_sample`，文案写着“示例离线也能读”。
  - 锁屏绑定 `reader.continue`。
  - `ai_service.c:921` 的离线意图用 `reader.next_page/prev_page/read_page`。
  - `kid_pages.c:50` 按页面名 `book` 判断。
- 测试和中枢脚本：约 64 个文件用 `reader.*`，只有 3 个用 `mogo.*`。

**固件**：现在的 S31 map（`build-s31/ink-s31.map`）里，旧阅读占 `ink_reader_vendor` 约 136 KB、`reader.c` 约 23 KB、`epub_source.cc` 约 8 KB、`bookmarks.cc` 约 4 KB、`book_bookmarks.cc` 约 3 KB、`ink_reader_port` 314 B，合计约 175 KB【估算】。`ink_miniz`（9 KB）资源包和上传还要用，留着。

## 要做的

### 1. 名字和登记：沿用旧名字，不搬引用

- 新阅读用应用编号 **`reader`**，标题 **TR_NOOP("阅读")**，`.icon = INK_UI_ICON(book)`，`.card` 见第 4 条，`system_app` 和 `child_ok` 照旧。
- 唯一的界面改名为 **`book`**。理由：中枢的 `APP_DOMAINS`、`kid_pages.c:50`、`test_catalogue.py`、`ai_pages`、`reader_epub`、`ui_shell_scenes`，还有能力的 `.activity`，都按 `book` 认。旧的 `reader`、`bookmarks`、`chapters` 这几个界面名不再出现，测试里改成核对快照的“面板=…”。
- 引擎内部的命名空间不变：嫁接客户端 `.app="mogo"`，偏好集合 `mogo.preferences`。这样试用版 21 上已经存下的进度不丢，也不用第二次迁移。
- **为什么沿用 `reader.*` 和 `reader`，不搬所有引用**：
  - AI 和中枢已经认这套名字：工具排序、`sim_hub` 剧本、中枢契约测试、离线意图、对话里学到的叫法。
  - 首页、锁屏、引导页、孩子开关和孩子白名单（`reader.` 前缀）都按这套名字写死。
  - `mogo.*` 从来不是主阅读，只有 3 个测试在用。
  - 改成新名字要动约 60 个文件，用户什么也得不到。
- 统一后**每个动作只有一个名字**，`mogo.*` 能力全部删掉。
- 登记：
  - `system/boot/main.c` 去掉 `ink_appdef_reader` 和 `ink_reader_actions_init`，去掉 `#ifdef INK_MOGO_SIM`，改成无条件登记 `ink_mogo_init()` 和 `ink_appdef_mogo`（符号名可以不改）。
  - `INK_MOGO_SIM` 宏连同 `mogo.cmake:24,33,40` 和 `CMakeLists.txt:1005` 一起删。
- **`mogo.preferences` 不登记**：
  - 它现在已经作为家人的状态文件原样进备份。
  - 一旦登记，`backup_service.c:903` 会把带着这些文件的旧备份整份判为无效：从试用版 21 起用过新阅读的家人，备份都还原不了。
  - 本任务不改备份服务。登记与否和备份格式写进报告“要改的文档／后续”。
  - `backup_restore` 测试里加断言：备份再还原后，新阅读的进度和书签还在。
- **`reader.bookmarks` 保留登记**：在 `apps/mogo/port/migrate.c` 里放一份和旧定义一样的 `ink_db_def_t`（名字、范围、schema、json 不变）。
  - 标题改为“旧阅读书签（迁移用）”，`ai_read=false`，`ai_write=false`。
  - 名额数不变。
  - 作用：还原升级前的备份时，这些记录能回来，并且能再迁移一次（见第 5 条“指纹”）。
  - 开机时 `MUST` 检查返回值。理由写进报告。

### 2. 能力表（`apps/mogo/mogo.c`）：22 个 `reader.*`，按钮、按键、AI 走同一个入口

| 名字 | 参数（schema） | 合并了 | 行为 |
|---|---|---|---|
| `reader.open_book` | `title`（必填，和旧 schema 一样），`index`（≥0，书架序号），`path`（可选） | 旧 open_book、`mogo.open`、`mogo.shelf_open` | 匹配规则：书名两头子串匹配书架上的书名；EPUB 用书里的书名（`book_epub_metadata`），其他用去掉扩展名的文件名。给了 `path` 就按路径开，路径只许 `/me/…`、`/family/books/…` 或示例。阅读没开着时先 `ink_app_open("reader", …)`，这次调用一直挂着（`ink_cap_defer`，挂在会话上，由会话第一次出图时完成），等第一页交给屏幕再回 DONE，和旧阅读“DONE 就是书已打开”一样。打不开回 FAILED，`say` 用旧阅读那几句：不是 EPUB/TXT、读不到、内存不足、文件损坏；“加密”见下面的说明。`reader_epub` 测试按“损坏”认 |
| `reader.continue` | — | 旧 | 在后台读最后一本的路径，查文件还在不在。在就照上面打开，DONE 时 `say`“接着读《…》”；不在或卡拔了回 FAILED，**不打开应用** |
| `reader.list_books` | — | 旧 | 只读，不打开应用；`say`“书架上有：…”，结果 JSON `[{index,title,place,percent}]` |
| `reader.open_sample` | — | 旧 | 打开示例 `asset:sample/welcome.txt`，书名“夜邮（示例）”，见第 3 条 |
| `reader.next_page` / `reader.prev_page` | — | 旧、`mogo.next/previous`（正文时） | `.activity="book"` |
| `reader.list_next` / `reader.list_prev` | — | 旧、`mogo.next/previous`（书架、目录、书签列表时） | 列表翻页 |
| `reader.goto_page` | `page`（≥1，章内页） | 旧 | 章内第几页（TXT 以前是全书页码，写进“和旧阅读不一样的地方”） |
| `reader.read_page` | — | 旧 | `say` 是本页开头，全文在快照里 |
| `reader.chapters` | — | 旧 | 结果 JSON `{title,chapter_count,fallback,entries:[{title,chapter,entry}]}` |
| `reader.show_chapters` | — | 旧、`mogo.toc` | 打开目录 |
| `reader.goto_chapter` | `chapter` 或 `entry`（都 ≥1） | 旧、`mogo.goto_chapter` | `entry` 是目录序号，也就是 Mogo 的 navigation 位置；`chapter` 是原书章号 |
| `reader.toc_percent` | `value` 0–100 | `mogo.toc_percent` | 新名字，目录按百分比跳 |
| `reader.bookmarks` | `open`（布尔） | 旧、`mogo.bookmarks` | `open:true` 打开列表；否则回 JSON `{bookmarks:[{id,chapter,percent,added_at,label,excerpt}]}`。`id` 是**稳定编号**，用这条书签的 `saved_s`（同一页只能有一条，已去重；为 0 时用 1）；删掉别的书签它不变。`excerpt` 从书里现读这个位置开头最多 30 字节，读不到就是空串 |
| `reader.add_bookmark` | — | 旧、`mogo.add_bookmark` | 本页已经有书签时什么也不做，回 DONE |
| `reader.remove_bookmark` | `id`（可省，省了就删本页的） | 旧 | 在 `ink_page_exports.h` 里补删除：按 `id` 删 `reader_bookmarks_t` 里的一条 |
| `reader.goto_bookmark` | `id`（必填） | 旧、`mogo.goto_bookmark` | 按稳定编号跳 |
| `reader.get_layout` / `reader.set_layout` | `font_size` 18–90、`line_spacing` 110–150、`paragraph_spacing` 0–100、`margins` 0–96、`weight` −32–32、`indent` 0–3、`header` 0/1、`footer` 0/1 | 旧、8 个 `mogo.set_*` | 名字和字段名不变，取值范围换成 Mogo 的；`footer_mask` 换成 `header` 和 `footer`。改完重排，位置不丢 |
| `reader.menu` | `open`（布尔），`view` ∈ `menu`、`settings`、`shelf` | 旧、`mogo.tools/settings/shelf/reader` | `open:false` 回到正文；旧的 `page`/`chapter` 输入框没了，用 `goto_page`/`goto_chapter` |
| `reader.back` | — | 新 | 退一步：面板、目录、书签、设置、工具 → 正文（没开书就到书架）；正文 → 书架；书架或说明页 → 调 `shell.back` 离开应用。不会循环，见下面 |

- **返回不能绕成圈**：
  - `ink_mogo_input` 收到 `INK_KEY_BACK` 时，在书架和说明页这些根页面**返回 false**，由外壳出栈。
  - 只有不在根页面时，才经 `ink_cap_call` 调 `reader.back`，并返回 true。
  - 于是 `reader.back` 在根页面调 `shell.back` 时，`on_key` 不接手，不会再绕回 `reader.back`。
  - 正在排版（`M.busy`）时按 BACK：不在根页面就吞掉（返回 true，不做事）；在根页面照常交给外壳。
  - 测试要断言：每条路径上 `reader.back` 最多只被调一次。
- **加密的 EPUB**：Mogo 不会判断。在胶水里用现成的 `zip_reader` 查有没有 `META-INF/encryption.xml`（约 10 行，计入预算），有就回“加密”那句。如果做不到，写进报告缺口，**不要**在 `reader_epub` 里删掉或放宽这条检查。
- effect 照旧阅读：写书签、改排版是 `LOCAL_WRITE`，其他是 `NAVIGATE` 或 `READ`。全部 `child_ok`。孩子白名单靠 `reader.` 前缀，所以新名字自动包括在内。
- `ink_mogo_page_hit` 和 `ink_mogo_input` 只能返回上表的名字。点按、长按、翻页键一律经 `ink_cap_call` 调能力。Mogo 目录自带的返回区（`book_toc.c:79` 的 `BOOK_TOC_BACK`）也映射到 `reader.back`。
- `tests/integration/ai_surface.c`：只在 `FOR_AI` 表**末尾**追加 `{"reader.back", NULL}` 和 `{"reader.toc_percent", "value"}`，删掉 `mogo.*`（如果有）。`NOT_FOR_AI` 不动。
- 用 `python3 tools/capabilities.py` 重新生成 `docs/system/capabilities.md`。在最新主线上变基之后再生成一次，避免和别的任务冲突。

### 3. 三处书和示例（数据流）

- **书架扫三处**：`/me/books`（我的书）、`/family/books`（全家的书，只读）、`/me/收件箱`（传进来的）。三处都用 `ink_fs_index`。
- 排序：读过的按最近在前，其余按书名。直接用 Mogo 的 `book_shelf_compare`，配合 `ink_mogo_qsort`。
- 行上只写书名；两处有同名书时，书名后面加“（全家的）”或“（传进来的）”。
- 快照每行写：序号、书名、哪一处、读到百分之几。
- 扫书架单独成一个函数，**不需要字体**，参数里带上家人身份：
  - 开始时用 `ink_mogo_owner(&身份)` 设好全局 owner，不碰阅读会话的书架数组和书源。
  - `list_books`、`continue`、按书名打开都用它；阅读没开着也能跑，在后台（BG）执行，用自己的任务节点（见第 3 条最后的“后台任务”）。
- `system/graft/graft.c`：只读放行改成两个前缀，`/family/fonts/` 和 `/family/books/`。写入照样拒绝。孩子能读全家的书（和旧阅读一样）。
- **示例书**：`runtime.c` 里路径以 `asset:` 开头时，用 `ink_app_asset_read` 读进内存，作为只读文件交给 Mogo 的 `fopen`、`open`、`stat`（约 30 行）。
  - 不往卡上拷，没插卡也能找到这本书。但字体在卡上，所以没卡或没字体时会显示第 6 条的说明，不会白屏。
  - 这是相对旧阅读的**退步**：旧阅读没卡也能读示例。开机引导的“看看示例”和“示例离线也能读”在没卡或没字体时会落到说明页。`boot_flow.c` 不改，写进报告。
  - 书名固定为 TR("夜邮（示例）")。
  - 嵌一个只含示例用字的小字体，让没卡也能读，是另一个选择，放进“要用户定”（写出实测大小）。
- **数据流**：
  1. 触摸、按键或 AI 调 `ink_cap_call(reader.*)`。
  2. UI 任务里 `ink_mogo_action` 校验参数，调 `ink_mogo_start(op, 整数/文字参数, call)`。
  3. BG：先迁移这次要打开的书和 last（第 5 条），再做引擎操作，画到帧缓冲，生成快照、出错原因、卡片信息。
  4. 回到 UI：`ink_graft_present`、出错页、返回键显隐、卡片镜像，最后 `ink_cap_done`。
  5. 整条路只在现有的 UI 和 BG 执行器上跑，不新开任务。
- **后台任务**：
  - 阅读会话（`M.work`）、书架扫描、卡片读取、迁移，各用**自己的** `ink_exec_job_t` 和忙标志；同一个节点在排队时不会重复排。
  - BG 执行器按投递顺序串行执行，所以这几样不会同时碰 Mogo 的全局状态。
  - 碰书源的只有阅读会话和迁移，迁移只在没有嫁接会话（`M.active=false`）时，或者在会话自己的第一步里运行。

### 4. 首页“继续读”卡片（`.card`）

- **卡片内容**：
  - id `reading`，标题 TR("继续读")，文字 `《书名》章名 · 读到 N%`；章名取不到就写“第 N 章”。按字符边界截断，放得进 `text[96]`。
  - 动作 `reader.continue`，`member` 是当前家人，`value` 是百分比（旧的是页码，写进“和旧阅读不一样的地方”），`bookmark=true`。
- **卡片记录**：每次存进度成功后，BG 往 `mogo.preferences` 写一条（命名空间 `ink_card`，键 `last`）：路径、书名、章名、章号、百分比。走现有的 NVS 模拟，不新增存储格式，然后把变化交给 UI 的镜像。
- **不阻塞 UI**：
  - `.card` 在 UI 任务里只读内存镜像，不碰闪存。
  - 镜像要**提前读好**：开机、`member.` 变化、`storage.card` 变化时，投一个 BG 读取任务。这个任务用 `ink_db_open_member` 直接打开 `mogo.preferences`，**不走 NVS 模拟**（NVS 模拟用的是全局 owner），并确认文件还在。读完回到 UI 后调 `ink_app_card_changed("reader")`。
  - 只有镜像还没读好时 `.card` 才返回 0。
  - 锁屏（`lock_faces/faces.c:93`）在 UI 上同步调 `.card`，所以开机和换家人后锁屏的“继续读”要显示正确的书。`lock_faces` 测试里加这条断言。
- **监听**：`storage.card` 和 `member.` 变化时都重新读。书删了或卡拔了，卡片就不出现。backlog 里“停在今日屏时插拔卡不刷新”用移植后的测试核对；没核对过不要宣称修好。
- **首页图标**：`shell_home.c:65` 里给 `reader`/`mogo` 特判 book 图标的那一句删掉，改由 `.icon` 提供；`shell_home.c:379` 的 `GROUP_IDS` 去掉 `mogo`。
- **常用迁移**：试用版 21 存过的常用如果写着 `mogo`，读入时当作 `reader`，已有的不重复（`today_edit.c` 读入处，两行）。

### 5. 旧数据自动迁移（新文件 `apps/mogo/port/migrate.c`）

**什么时候跑**
- 每位家人的空间打开后跑。
- 触发点：开机后家人空间打开时、`member.` 状态变化、`storage.card` 变为在。都在 UI 上确认没有嫁接会话（`M.active=false`），再投一个 BG 任务。
- 阅读每次开会话时，BG 第一步只迁移 last 和这次要打开的那一本（如果还没迁移），不等全部做完。
- **分片**：每个 BG 任务只换算一本书，做完重新投递，不长时间占住共用的 BG（备份、文件索引也在上面排队）。
- 为了家人隔离，只给当前家人迁移，所有读写都带当前家人身份（`ink_member_identity`）。其他家人在第一次切到他们时迁移。

**读旧数据，只读不删，可以退回旧固件。旧数据在内部存储，嫁接接口读不到，所以用系统存储接口：**
1. `reader.last`：`ink_layout_member(…, "reader.last")` 加 `ink_storage_read`。
2. `.pos` 文件：同样用 `ink_layout_member` 加 `ink_storage_read`。key 从这三处得到：
   - `/family/books` 和 `/me/收件箱` 里的文件，按旧规则算 crc32（列目录经嫁接接口）；
   - 示例的 key；
   - `reader.last` 里的 `file`。
3. `reader.bookmarks`：用第 1 条那份定义加 `ink_db_open_member` 打开。
4. `/user/sys/settings.json` 里的 `reader.*:<家人>`：用 `ink_layout_sys` 加 `ink_storage_read` 读（设置服务原子写，读到的是完整文件）。

**写入 Mogo 的数据**：只经 `ink_graft_preferences_open`（通过 NVS 模拟，迁移开始时设好 `ink_mogo_owner`）；打开书只经 `ink_graft_file_open`。

**路径换算**
- `/content/books/x` → `/family/books/x`
- `/content/.m/<家人>/x` → `/me/x`
- `sample/welcome.txt` → `asset:sample/welcome.txt`

**位置换算**（用 Mogo 引擎 `book_open` 打开那本书）
- TXT：
  - 章号 c 取满足 `book_chapter_byte_offset(c) ≤ 旧偏移` 的最后一章，`delta = 旧偏移 − 章起点`。
  - 这一章解码后的 UTF-8 长度等于原文件里这一章的字节数，就是 UTF-8 书，直接用 `delta`；否则按比例换算，再退到 UTF-8 字边界。
- EPUB：
  - 先核对 Mogo 的章是不是和 spine 一一对应（数目一样）；不一样就按章节文件名（href）对上，对不上的记录跳过并计数。
  - 块号按 `book_chapter_load_blocks` 的块起点，加上 min(块内偏移, 块长)。旧解析器的分块和 Mogo 不一定一样，所以是近似的，写进报告。
- 书签换算后，在 ±4 KB 范围内找旧摘录的开头（最多 30 字节，到第一个换行为止）；找到就用找到的位置。旧摘录本身不迁移，Mogo 的书签格式没有这个字段。
- 百分比用 `book_chapter_byte_offset` 和 `book_total_bytes` 算。
- `file_size` 填这本书现在的大小。

**写入**
- 进度：`book_progress_save`，Mogo 已有这条进度就不覆盖。
- 最后一本：`book_progress_set_last_path`，Mogo 已经有就不覆盖。
- 书签：并进 `rp_marks`，按（章，偏移）去重，按 `added_at` 留最新的 24 条（`saved_s` 填 `added_at`），多出的计入“丢弃”。
- 排版：只有旧值和旧默认值不同，并且 Mogo 还没有 `reading/layout` 时才写。对照表：
  - 字号档 0–3 → px 40/48/56/72
  - 行距档 → 110/120/135/150
  - 段距档 → 25/50/75/100
  - 边距档 → 16/24/40/56
  - `footer_mask≠0` → `footer=1`
- 卡片记录（第 4 条）。
- 最后写标记：命名空间 `ink_migr`，键 `reader1`。内容是**旧数据的指纹**：`reader.last` 内容的 crc32、`reader.bookmarks` 的最新版本号和条数、找到的 `.pos` 个数。

**什么时候重跑、失败了怎么办**
- 指纹和标记一样就不跑；不一样（比如还原了升级前的备份）就再跑一次。每一步都是“不覆盖已有”或“合并去重”，重跑结果一样。
- 卡不在、写入失败时不写标记，下次触发再跑。卡不在**不算**“书已经不在”。
- 卡在、书确实不在的旧记录跳过并计数。
- 计数器：`reader.migrated_progress`、`reader.migrated_bookmarks`、`reader.migration_skipped`。日志一行，只写数字，不写书名和路径。

### 6. 缺东西时说清楚，不白屏（D-138）

- `ink_mogo_page_*` 返回原因码：没卡、缺字体（两个路径都不存在）、字体读不出、书打不开（带 Mogo 的 `s_message`）、书架空。
- UI 收到原因码后，在嫁接图上方用 **`ink_ui_empty`**（`ui_theme.h:73`）加第 7 条的返回键画说明页，只刷这一块。快照写面板名、说明全文和可用动作。
- 文案用界面规范第 0 节的词：
  - 没卡：“没有插存储卡。书和阅读字体都放在存储卡上，插好后再打开阅读。”
  - 缺字体：“还缺阅读用的字体，暂时不能显示书。字体文件 InkSC.ttf 应放在存储卡的 fonts 文件夹里；连着电脑装机时会自动放进去。”
  - 字体坏：“阅读字体（存储卡 fonts/InkSC.ttf）读不出来，请连着电脑重新装机补上。”
  - 书架空（大人）：“书架上还没有书。用‘传书’从电脑或手机传进来，会放在‘传进来的’；全家的书放在存储卡的 books 文件夹。”一个按钮“传书”，调 `transfer.open`。
  - 书架空（孩子）：没有按钮，文字是“请家长用‘传书’把书传进来。”
- 字体路径、文件名集中写在 `runtime.c` 一处，留给 `auto-provision-1` 用；本任务不做送文件。

### 7. 用手点的返回键

- 用和 `ink_shell_topbar`（`status_bar.c:170-174`）一样的返回键：`ink_ui_circle(... "back" ..., INK_SURFACE_STATIC)`，放在 (INK_MARGIN, INK_STATUS_BAR+INK_GAP_S)，点按范围 ≥108，跟系统文字大小一起缩放。用 `ink_shell_bind` 绑到 `reader.back`。
- 在书架、目录、书签列表、设置、工具面板和所有说明页上显示。
- 正文页默认不显示：照 Mogo 和 Kindle 的做法，正文不留边框按钮。点中间或上滑出工具面板，返回键随面板一起出现。物理返回键在哪一页都能用。
  - 这和需求“像别的应用一样有返回键”不完全一致，列进“要用户定”：A 正文页隐藏（默认）；B 正文页也常驻。
- Mogo 自己画的页面，左上角这块不能放内容，也不能是点按区（目录的 `BOOK_TOC_BACK` 正好在这里，映射到 `reader.back`）。改完用截图核对；`ink_mogo_page_hit` 在这块返回空串。
- 显示和隐藏只刷返回键那一块（D-127）。

### 8. 快照（页面写清在显示什么）

- 快照以“阅读”开头，不再写“新阅读”（排版中写“阅读 正在排版”）。
- 第一行：`《书名》 · 章名 · 章内页 m/n · N%`。
- 机器字段：章节、页、位置、结束、字号、面板（书架、正文、目录、书签列表、设置、工具、缺字体、没有卡、书架空、打不开）、书签数。
- 正文最多 600 字节，按字符边界截断。
- 书架、书签、目录这几种面板分别列出条目；设置面板列出 8 项当前值。
- 最后一行：`动作：` 加上本页能用的 `reader.*`。中枢排序靠这一行。

### 9. 旧阅读退出构建

- 删除 `apps/reader/` 整个目录和 `third_party/reader_expat/`（只有旧阅读用它）。git 历史里都还有。`third_party/reader_miniz/` 留着（`ink_miniz`、`hal_assets_*` 和 S31 测试在用）。
- 根 `CMakeLists.txt`：
  - 删掉 329 行的 `apps/reader/reader.c`、332-333 两行、342-345 和 361-364（`sim-reader-epub` 和它的测试，测试改到第 10 条的新位置）、366 行的 `ink_reader_translations`、483 行 i18n 来源里的 `reader.c`。
- **i18n 表**：`apps/mogo/translations.cmake` 现在对 `INK_I18N_TABLES` 里的每张表做 `file(APPEND)`。删掉 `ink_reader_translations` 以后，这些表会指向源码里的 `assets/i18n/*.tsv`，configure 会往仓库文件里追加。所以：
  - 先把每张表复制到 `${CMAKE_BINARY_DIR}/reader-i18n/`，再追加，`INK_I18N_TABLES` 改成指向副本。
  - `reader-i18n-check` 目录的建立（原 `reader_build.cmake:25-31`）也挪进 `apps/mogo/translations.cmake`，`i18n-literals` 照样按这个目录跑。
  - 验收时 `git status` 要看到 `assets/i18n/` 没有被构建改过。
- `apps/mogo/mogo.cmake`：
  - `ink_mogo_native` 改链 `ink_miniz`。
  - 主机已经有这个库，但它的宏和 `ink_reader_vendor` 的不一样（没有 `tinfl_*` 改名）；核对 Mogo 只用 `tinfl_decompress` 核心函数，主机上不会撞名。
  - S31 在 `ports/esp32s31/main/CMakeLists.txt` 里直接写出 `ink_miniz` 的定义：`MINIZ_NO_*`，加上 `tinfl_decompress=ink_tinfl_decompress` 这一组改名，照抄原 `reader_build.cmake:42-45`，不再从 `ink_reader_vendor` 拿。改名是为了避开 ROM 里的同名函数。
- S31 的 `ports/esp32s31/main/CMakeLists.txt`：
  - 删掉 54、57、64-66、69-73、75 行和 `ink_reader_translations`。
  - i18n 表改由 `assets/i18n/*.tsv` 加 `apps/mogo/translations.cmake` 生成；后面 `i18n_gen.py` 用的 `reader_tables` 变量一起改名。
- `ink_reader_host_call` 搬进 `apps/mogo/port/runtime.c` 成为 static：ESP 上直接调用，主机上走 `vtk_host_call`。名字随意。
- 测试内容 `${INK_TEST_CONTENT}` 加一条生成命令：用 `tools/mogo_font.py` 把 `INK_CJK_FONT` 转成 `fonts/InkSC.ttf`，连同 OFL 放进去。
  - 只在 `INK_HAVE_FONTS` 时生成；CI 的 Linux 和 macOS 都已经装了 fonttools（`.github/workflows/sim.yml`）。
  - 旧测试拿阅读当“有内容的页面”用，靠这份字体才能照旧跑。
  - 报告写生成耗时和文件大小。
- 英文翻译：
  - `apps/mogo/en.tsv`：去掉“新阅读”那几行；从旧的 `apps/reader/en.tsv` 搬过来新代码还要用的行（出错提示等）；新文案追加在末尾。
  - `assets/i18n/en.tsv` 只在末尾追加。

### 10. 测试移植（保留原来的意图；只删测已删除内部实现的测试，并写理由）

| 测试 | 意图 | 怎么做 |
|---|---|---|
| `tests/reader_epub/`（reader-epub、-reverse、-segments、-toc-save-reverse） | EPUB2/3 书名作者、NAV/NCX 目录、片段锚点、无目录退回、损坏/加密/非 EPUB 的提示、GB18030 TXT 章节、家人和重启、24 本填充书翻页 | 直接调旧 C 接口（`ink_reader_source_*`）的检查，改成经 `reader.chapters` 和快照核对同样的事实。`sim-reader-epub` 改链新阅读，注册挪到 `apps/mogo/mogo.cmake`。reverse 的变异目标改成 `apps/mogo/*`，比如不存目录跳转后的进度，测试必须失败。加密那条见第 2 条。`-segments` 测的是旧引擎超过 2 MB 分段加载：内部实现删了，换成“长章读到末页再翻进下一章”，理由写报告 |
| `tests/reader_settings/`（含 -reverse） | 排版设置、重启保留、120% 截图 | 改用新的 `set_layout` 字段和范围，`menu view=settings`；“返回书架”改为核对快照“面板=书架” |
| `tests/reader_more/`（reader-more、-reverse） | 收件箱里的书上书架；书签加、列、跳、删；重启 | 照写，书签 id 用稳定编号。`reader-bookmark-model`（旧的 C++ `BookBookmarkStore`）删除，理由是内部实现删了；24 条上限、去重、删一条后其他 id 不变的检查放进新的 `tests/reader_one` |
| `tests/reader_continue.{c,py}` | 卡片、点卡片回到原处、家人分开、重启、拔插卡刷新、书删了不出现 | 卡片文字换成新格式；“回到第 5 页”改为核对同一章、同一位置；`top=="reader"` 改为核对“面板=书架”。加一条：回首页后卡片还在（`docs/project.md` 记过“回主页后卡片消失”） |
| `tests/small_gaps_2/reverse.py` | 卡片跟着 `storage.card` 刷新 | 变异目标改成 `apps/mogo/mogo.c` 里的那个监听 |
| `tests/small_gaps_3/main.c`、`reverse.py`（sample） | 引导页“看看示例”走原子能力，真正打开示例 | 能力名不变；reverse 的文件列表改成 `apps/mogo/mogo.c` |
| 场景：`reader_basic`（含 pqbn、psram16、aged-flash-max、panel-power-faults、upstream-pipeline、determinism-reader）、`reader_continuous`、`reader_rapid`、`reader_empty`、`ai_tools`、`ai_offline_intent`、`ai_page_version`、`keys_overlap`、`powercut_*`、`standby`、`pqbn_flash_pause` | 键、滑、点三种翻页，打开耗时，存位置，AI 调用 | 改脚本里的点按坐标和 snap 名，对准新书架上原来那本书。**预算的每个阈值不动**。新阅读发出同名同义的计数器：`reader.page_turns`、`reader.open_ms`、`reader.page_render_ms`、`reader.last_saved`、`reader.restored_page`。`reader.turns_prerendered` 和 `reader.turns_coalesced` 是旧引擎的预渲染和合并：Mogo 有对应机制就照发，没有就删掉这两行并写理由，翻页速度仍由 `ui.action_to_visible_ms` 管着。达不到原阈值就写进报告，不放宽 |
| `perf-latency`、`perf-latency-2` | 打开书、翻页的时延 | 能力名不变；跑通，`reader.page_render_ms` 由新阅读发 |
| `gui-smoke`（`tests/simulator/gui_smoke.cmake`） | 窗口点击打开书架和书、翻页键 | 核对点击点，`reader.page_turns` 照数 |
| `ui-scene-*`（`ui_shell_scenes.c`） | Apps 页“阅读（reader）”、Ai 场景下面是 book 页；第 436 行要求 `reader.open_book` 回 DONE | 断言是子串匹配，大概不用改；“全部应用”里少了“新阅读”，版面会变，截图要重看。改了就逐条列出 |
| `tests/capability_table/test_catalogue.py` | `reader.next_page` 的界面是 `book` | 应该不用改，必须过 |
| 把阅读当内容页用的测试：`ai_pages`、`audit12_fix`、`backup_restore`、`design_lang_1`、`kid_2`、`kid_3`、`lock_cover`、`lock_faces`（含 reverse）、`integration/{apps,member_wake,people,power_menu}`、`power_pages`、`ui_fix_c`、`log`、`device_events`、`services_finish/late_activity_boundary.c`、`i18n_shell*` | 各测各的外壳行为 | 能力名不变，只改依赖旧快照格式、旧文件（`reader.last`）或应用个数的断言。`audit12_fix` 改为核对新的卡片记录和 `reader.last_saved`。`backup_restore` 加第 1 条的断言。`lock_faces` 加第 4 条的断言 |
| `services_finish` 的 `reader_late` 和 `reader_boundary.c` | 旧 `lv_timer` 存档，和页面销毁、家人删除时的竞态 | 内部实现删了，所以删除。删之前确认 `graft-carrier` 已经有“过期会话写入失败、不重建数据”的断言，没有就在 `tests/graft_mogo/` 补一条 |
| `mogo-page`、`mogo-flow`、`graft-carrier`、`tests/graft_mogo_font` | 嫁接页面、流程、字体 | `mogo.*` 换成 `reader.*`，`run.py` 里的字体路径照旧。carrier 加四种情况：读 `/family/books` 成功、写它失败、`/family/其他` 被拒、`/members/…` 被拒 |
| `device_autotest`、`tools/device/autotest/reader.script` | 真机自动翻 20 页 | `reader.txt` 放进 books 文件夹，能力名不变 |
| 中枢：`hub-contract`、`hub-simulator-e2e`（`tests/hub/e2e.py`）、`hub-export-caps`；`simulator/environment/sim_hub.c` | `reader.next_page` 等工具 | 应该不用改，必须过 |
| `i18n-literals`、`i18n_shell*` | 文案都有英文 | 新文案和搬来的旧文案进 `apps/mogo/en.tsv` |

新增 `tests/reader_one/`，注册在 `apps/mogo/mogo.cmake` 里：
- `reader-one-migrate` 和 `-reverse`：写旧数据的办法照 `tests/configured_boot.c`，在阅读登记之前写入 `/user/m/<家人>/` 和 `/user/sys/settings.json`。
- `reader-one-states`：缺字体、没卡、空书架、三处书、返回键（含“每条路径上 `reader.back` 最多调一次”）。
- `reader-one-bookmarks`：24 条上限、去重、稳定 id。

### 11. 截图

放 `docs/tasks/reader-one-1/`，单张不超过 300 KB，默认和最大文字大小各一套：
- 书架（三处书都有）、正文、目录、书签列表、设置、工具面板（带返回键）
- 缺字体、空书架（大人、孩子）
- 首页“继续读”卡片、锁屏“继续读”、全部应用里的图标

先看干净的整页图。

## 范围和预算

**只改**
- `apps/mogo/**`（含新的 `port/migrate.c`）
- `system/graft/graft.c`、`graft.h`（只改只读前缀和注释）
- `system/boot/main.c`
- `system/ui/shell/shell_home.c`（只改第 65 行那一句和第 379 行 `GROUP_IDS` 里的 `mogo`）
- `system/ui/shell/today_edit.c`（读入时 `mogo`→`reader`，2 行）
- `CMakeLists.txt`、`ports/esp32s31/main/CMakeLists.txt`（交付计划的规矩：尽量只在末尾追加；删除旧阅读的行除外，冲突由主会话解决）
- 删除 `apps/reader/**`、`third_party/reader_expat/**`
- 第 10 条列出的测试和场景文件，新的 `tests/reader_one/**`
- `tests/integration/ai_surface.c`、`assets/i18n/en.tsv`（都只在末尾追加）
- 生成的 `docs/system/capabilities.md`（变基后重新生成）
- `docs/tasks/reader-one-1/**`
- `third_party/mogo/main/**` 原则上不改；非改不可的不超过 30 行，逐条写进 `third_party/mogo/SOURCE.md`

**不碰**
- `theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c`
- 状态栏、控制中心、设置各页（`settings-2` 正在改）、锁屏代码（`lock_faces/faces.c` 不改，只加测试断言）、`boot_flow.c`
- `system/services/file/**`（传书照样放进 `/me/收件箱`）、`system/services/backup/**`、`system/services/data/**`、`system/policy/**`
- `hub/**`（名字不变，所以不用改）
- 分区表、设备
- `auto-provision-1` 的送文件
- `docs/requirements.md`、`docs/ideas.md`、`docs/decisions/`、`backlog.md`、`ui-dependencies.md`、`delivery-plan.md`：要改的写进报告“要改的文档”

**产品代码预算**：新增不超过 **800 行**，删除不计，测试、文档、拷来的 Mogo 原文不计。参考分配：

| 部分 | 行数 |
|---|---|
| 能力表和分派（`mogo.c`，含返回的根页判断、加密检查） | ≤250 |
| 三处书架、原因码、说明页、返回键、快照 | ≤190 |
| 卡片镜像和卡片记录 | ≤70 |
| 迁移（`migrate.c`，含指纹和分片） | ≤220 |
| `runtime.c`：示例书内存读、host_call | ≤50 |
| `graft.c`、`main.c`、`shell_home.c`、`today_edit.c` | ≤20 |

CMake 另计，不超过 80 行。快超了就停下，写清原因。

**新增的机制**只有这几项，都是本任务要求的：
- 迁移标记（带指纹）和卡片记录，都存在现有的 `mogo.preferences` 里；
- `asset:` 只读内存文件；
- 几个后台任务节点。

不加线程、队列、缓存或配置项。

**固件**：S31 `.bin` 应该净减约 150–165 KB【估算】，验收要求至少减 120 KB。报告写实测的 `.bin` 大小、应用分区余量、静态 DIRAM 和 PSRAM 的变化（旧阅读的 PSRAM 静态区也一起释放）。

## 验收

构建命令和资源限制照总规则第 5、8 节（`taskpolicy -c utility`，`-j2`）。

1. **名字统一**：
   - 能力清单里只有 22 个 `reader.*`，没有 `mogo.*`。
   - `git grep -nE '"mogo\.[a-z_]+"' -- apps system tests | grep -v '"mogo\.preferences"'` 没有结果。
   - `git grep -nE 'apps/reader|ink_appdef_reader|ink_reader_vendor|ink_reader_actions_init|reader_expat' -- ':!docs'` 没有结果。
   - `hub-contract`、`hub-simulator-e2e`、`hub-export-caps` 不改一行就通过。
   - 构建之后 `git status` 显示 `assets/i18n/` 没有改动。
2. **迁移**：`ctest --test-dir build -R '^reader-one-migrate' --output-on-failure`
   - 写入两位家人（大人、孩子）的旧数据：`reader.last`；三本书（全家的 UTF-8 TXT、收件箱里的 EPUB、示例）的 `.pos`；5 条书签（其中 1 条的书已删）；非默认排版。
   - 第一次开机后：
     - 大人的卡片是正确的书、章和百分比，锁屏的“继续读”也一样。
     - `reader.continue` 打开后，快照第一行含旧摘录。
     - `reader.bookmarks` 列出 4 条，跳到摘录所在页。
     - 排版是换算后的值，计数器正确。
     - 孩子在切过去之后得到自己的那份，大人看不到孩子的。
   - 再开机一次：数字一样，书签不重复。
   - 把旧书签集合换成另一份（模拟还原升级前的备份）：指纹变了，迁移再跑一次，结果是合并去重后的。
   - 拔卡开机：不写标记，插卡后再跑。
   - 反向对照：关掉迁移，测试必须失败；跑两次迁移，结果必须一样。
   - 旧文件（`/user/m/…`、`settings.json`、`reader.bookmarks`）逐字节没变。
3. **缺东西时**：`ctest -R '^reader-one-states'`
   - 去掉字体打开阅读：快照“面板=缺字体”，含 `InkSC.ttf` 和 `fonts`；截图上文字看得见，不是白屏；点返回键回到首页。反向对照：去掉说明页，必须失败。
   - 拔卡：显示“没有插存储卡”。
   - 书架空：大人有“传书”按钮，按下调 `transfer.open`；孩子没有按钮。
   - 三处书各放一本都出现，并标出来自哪一处。经嫁接接口写 `/family/books` 被拒。
4. **返回**：
   - 书架、目录、书签、设置、工具、说明页，每页点一次返回键、按一次 `BACK`、AI 调一次 `reader.back`，三条路结果一样，每条路径上 `reader.back` 最多调一次（没有循环）。
   - 排版中按 `BACK` 不会离开应用。
   - 正文页没有返回键；点中间出工具面板，面板上有返回键。
   - 点按范围 ≥108。返回键显隐时，显示统计断言只刷它那一块。
5. **图标和位置**：
   - 首页常用和全部应用里，“阅读”用 book 图标，`ui-scene-Apps` 和 `ui-scene-Main` 通过。
   - 常用里存过的 `mogo` 显示成“阅读”，不重复。
6. **卡片**：移植后的 `reader-continue` 和 `small-gaps-2` reverse 通过（含拔插卡刷新、回首页后卡片还在）；`lock-faces` 通过。
7. **备份**：`backup-restore` 通过，并且新阅读的进度和书签在备份再还原后还在。
8. **测试移植**：
   - 第 10 条每一行都有结论。
   - 报告列出**每一个改过的已有测试**：改了什么、为什么仍然一样严；删掉的测试和理由。
   - 场景预算的阈值一个不改，删掉的计数器行单独列出。
9. **全量**：
   - 普通全量 ctest 通过（先清掉 `build/test-out`），包括 `ui-scale`、`i18n-literals`、`ui-icon-*`、`gui-smoke`、全部 `ui-scene-*`、`capability-table` 相关测试。
   - ASan 构建跑这些通过：`ctest -R 'reader|mogo|graft|backup|lock-faces|ui-scene-(Main|Apps|Ai)'`。
   - S31 宿主机测试通过（`cmake -S ports/esp32s31/tests -B build-s31-tests`，再跑 ctest）。
   - `bash ports/esp32s31/build.sh` 通过，没有新警告；`nm` 证明没有 `epub_document`、`XML_ParserCreate` 等旧符号，有 `ink_appdef_mogo` 和 `ink_tinfl_decompress`。
   - 跑 `gcccheck`，并按总规则第 6 节做隐私检查。
10. **报告**写到 `docs/tasks/reader-one-1/REPORT.md`，按总规则第 6 节的结构，另外写：
    - 固件净减多少（实测）、产品代码行数；
    - 迁移的近似规则（GBK、EPUB 章内位置、分块不同，旧摘录不迁移）；
    - 和旧阅读不一样的地方：排版取值范围；书签上限 24 条、id 改成稳定编号；正文页不显示返回键；没有页码输入框；`goto_page` 和快照页码改成章内页码；卡片 `value` 改成百分比；没卡时示例不能读；
    - 给 `auto-provision-1` 的字体路径；
    - “要用户定”：正文页返回键 A/B；要不要嵌示例小字体（写实测大小）；`mogo.preferences` 要不要登记（要的话得改备份服务的旧备份兼容）；
    - 上机核对清单。
