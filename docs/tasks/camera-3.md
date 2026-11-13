# camera-3：相机照 Fossify Camera 重写：拍前在顶栏调好定时和效果，拍完立刻看到这张、知道存在了哪里，左下角是最近一张照片；取景、最近照片和出错的说法照 AgentUI 的相机代码；锁屏相机和扫码保留

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。

**D-140（总规则第 11 节）**：本任务是“照着重写”。第一步写对照表 `docs/tasks/camera-3/对照表.md`（参照产品每一页、每个操作和状态，应用看 `docs/tasks/replan-sysapps/APPS.md` 的参照路径出发）并单独提交；计划和 `replan-sysapps/SOURCES.md` 点名的 C/C++ 代码能直接用的照搬（这个任务：AgentUI 相机代码）。参照安卓的地方按第 11 节为这台设备改，做完逐行验收。

## 为什么

用户 2026-11-12 说过三句话：
- “好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”（D-129）；
- “我需要你先把当前的壳和现有已有的应用移植或参考重写搞到成熟可用”（D-131，相机在名单里）；
- 外壳预览页“都按推荐推进”（D-135）：系统应用重写后原地替换，不和旧版并存。

调研（`docs/tasks/replan-sysapps/APPS.md` A13，`CURRENT.md` A13）结论：拍前和拍后的控制照 Fossify Camera 重做；取景控件、最近照片和失败呈现，从 AgentUI 相机代码里取能分开的部分。

现在的相机是 `system/apps/camera/camera.c`（246 行）加 `scan.c`（238 行）和 `png.c`（54 行）。已经有的：
- 低频取景，按界面和屏幕空闲来调度，`camera-fps-1` 做的；真机约每秒 0.9 帧，见 `docs/tasks/backlog.md`“相机取景”一条；
- 拍照存成彩色 VGA PNG，按家人存到 `/me/照片/照片-N.png`；
- 锁屏相机只能拍、只能看这次拍的（D-121）；
- 扫码：Wi-Fi、联系人、电话、网址、文字，结果分页，`camera.scan_apply` 要人确认。

和成熟相机比，差这几处（`CURRENT.md` A13 和代码核对）：
1. **拍完看不到结果**：`captured()` 只把状态行换成“已保存到存储卡”。取景定时器下一拍就把刚拍的画面盖掉。没有“这张存在哪、去看看”。
2. **没有最近照片**：左下角是一个画着“相册”图标的圆圈（`camera.c:165-167`），点了打开整个相册，不是刚拍的那张。
3. **拍前什么都调不了**：没有定时拍照，没有效果，没有快门声、按键拍照这些 Fossify 设置页里有的项。
4. **出错一句话打发**：没卡、卡借给电脑当 U 盘、卡满、写不进，全都显示“没能完成，请检查存储卡或重试”（`camera.c:47`、`:122`）。`ink_fs`（`system/include/ink/sys/file.h`）明明分得清 `INK_FS_ERR_NO_CARD`、`_USB`、`_NO_SPACE`、`_IO`。这条违反 D-138（缺什么要说清楚原因）。
5. **还是旧样式**：快门、相册、关闭是三个 `ink_ui_circle`（`camera.c:151`、`:165`、`:168`、`:170`）；取景框是 `ink_ui_card`；“扫码”是写死宽度的胶囊（`:162`）。`docs/tasks/design-lang-1/PAGE-FOLLOWUPS.md:28` 已经点名：相机提示、自绘取景区和扫码分页要单独核对。

## 成熟来源（照着重写，AgentUI 片段可以搬）

两份都只读克隆到 `work/`（目前 `work/` 里没有，需要联网克隆），用下面的固定提交。下面的行号是调研时记的：先对照代码内容确认，对不上以内容为准，并在 `SOURCE.md` 记下实际行号。

- **Fossify Camera**（第一参照）：`github.com/FossifyOrg/Camera`，提交 `a57a2ffbbac1dd42231e3f030f738929ad139c6e`，目录 `app/src/main/kotlin/org/fossify/camera/`：
  - `activities/MainActivity.kt:569-595`：最近照片缩略图怎么更新（交叉渐变在墨水屏上不做，只换那一块）；
  - `activities/MainActivity.kt:990-1032`：定时拍照，包括倒计时、中途取消、到点拍；
  - `activities/MainActivity.kt:160-190`：顶栏的模式和开关怎么排；
  - `implementations/CameraXPreview.kt:225-270`：取景和拍照的生命周期，只对照“离开页面停、回来重开”；
  - 同一仓库的设置页（`SettingsActivity.kt` 和它的布局）：快门声、用音量键拍照等。逐项对照我们的硬件，留下能做的；去掉的写进 `SOURCE.md` 并写明原因。
- **AgentUI 相机**：`github.com/AlayaElla/MetalioClaw4-AgentUI`，提交 `172ec530ff65e11521585fe3b8611daa20a78aa4`，C++ 和 LVGL：
  - `main/display/agent_ui/apps/camera/camera_view.cc:44-89`：效果列表和缩略图；
  - `camera_controller.cc:112-166`：取景和缩略图的生命周期；
  - `camera_gallery_repository.cc:116-177`：枚举照片。

  能分开的显示片段可以搬，C++ 改成 C，保留 MIT 版权声明。它是 P4 的 LCD 大屏，尺寸、实时刷帧、缓存上限（48 张）都不照搬。
- **底层不动**：摄像头驱动、会话、取帧照原厂（codex-rules 第 10 节，`camera-fps-1` 已经接好）。不能从 CameraX 或 P4 的功能推断这台机器有闪光灯、对焦、变焦、前置摄像头或录像。

## 要做的

### 1. 取景页（`Camera`）照 Fossify 的布局重排

从上到下：
- **状态栏。**
- **顶栏**：返回；一排写着当前值的文字按钮，点一下换到下一个值（照 Fossify 顶栏的开关）：“定时 · 关”“效果 · 原图”；再加“扫码”和“设置”。
- **取景区**：只是画面本身，不画粗框。
- **底栏**：左边是最近照片的缩略图；中间是唯一的主按钮“拍照”。

放不下就换行或分两行，不缩字，不裁按钮。三个圆圈全部去掉。

### 2. 拍前设置（照 Fossify 的顶栏和设置页）

- **定时**：值照 Fossify 的源码取。倒计时的数字只刷数字那一块，每秒最多刷一次。倒计时中再按快门，或调 `camera.cancel`，就取消。离开页面、锁屏、换家人也取消。
  - 倒计时在 UI 一侧计时，到点才把取帧保存任务交给 `INK_EXEC_MEDIA`；倒计时期间不占着这个执行器，否则 `camera.cancel` 进不来。数字刷新的写法参考 `tests/timer_seconds.c`（它已经在断言“只有数字区域快刷”）。
- **效果**：照 AgentUI 的效果列表，挑在灰阶屏上看得出差别的几种，比如原图、黑白、高对比文档。
  - 保存的照片按所选效果处理；选“原图”时，照片和现在一样是彩色 VGA PNG；选“黑白”时存下的 PNG 每个像素 R=G=B。
  - 取景区和效果的关系写进 `SOURCE.md`。取景现在是二值化快刷，不要为了效果加帧或改成灰阶刷新。
- **快门声、用翻页键拍照**（对应 Fossify 的音量键拍照）：放在相机自己的“设置”小页里（用 `ink_ui_toggle` 行；设置首页和搜索归 `settings-2`，不在这里做），默认值照 Fossify。
  - 翻页键拍照打开时，在取景页按翻页键（`INK_KEY_NEXT`/`INK_KEY_PREV`）就是 `camera.photo`；扫码页的翻页键照旧翻结果页。
  - 快门声用系统已有的提示音（`ink_audio_tone`，`INK_TONE_KEY` 或 `INK_TONE_DONE` 选一个，写进报告）。不新加声音资源。
- **存在哪**：设置都按家人分开，用 `ink_settings_define` 定义成 `camera.*` 设置项（定时、效果、快门声、翻页键拍照，共 4 项）。
  - 按钮和 AI 都走 `settings.set`，同一个入口。
  - 孩子能改（`child_may_change`）。
  - 先数一下 `INK_SETTINGS_MAX`（`system/include/ink/sys/settings.h`，48）还剩几格；不够就写进报告，不改上限。
  - 设置首页和搜索的登记（`system/ui/shell/settings_registry.c`）不归本任务。写进报告，留给 `settings-2`。
- **锁屏相机不读这些设置**：锁屏下不倒计时、固定原图、没有快门声以外的任何拍前控制（D-121）。

### 3. 拍后查看（Fossify 的拍完确认，加上 AgentUI 的拍摄结果动作）

- **拍完以后**：
  - 取景停下，取景区换成刚存的这张（灰阶抖动，沿用现在的 `ink_photo_image`）；
  - 状态行写“已存到 我的照片 · 照片-12”；
  - 两个动作：主按钮“再拍一张”回到取景（`camera.preview`），“在相册里看”打开这张（`photo.open`，`id` 用回执里的 `file:id`）；
  - 点画面本身也回到取景，和锁屏相机现在的做法一致。
- **出错要说清楚原因**（D-138，不许空白，也不许一句笼统话）：
  - 存储卡的四种分开说：没插卡（`INK_FS_ERR_NO_CARD`）、正借给电脑当 U 盘（`_USB`）、满了（`_NO_SPACE`）、写不进（`_IO`）；
  - 摄像头没出画面单独说；
  - 每种都给下一步（比如“拔下数据线后再拍”），并保留“再试一次”；
  - 相机服务给不出更细的摄像头原因，不改服务，写进报告。
- `/me/照片` 不存在就自动建（现在已有 `ink_fs_mkdir_for`，保留）。
- 文件命名 `照片-N.png` 和“300 张也只开两次文件”的做法不变。

### 4. 最近照片（Fossify 的缩略图，加上 AgentUI 的图库枚举）

- **画什么**：左下角画当前家人 `/me/照片` 里最新一张的缩略图，走 `file.thumb`。点了打开那一张（`photo.open`）。
- **什么时候换**：拍完只换这一块；换家人、从相册删掉这张、插拔卡以后，回到页面要换成对的那张。
- 一张都没有时这一格不画，不放空框，也不写“暂无照片”。

### 5. 锁屏相机（D-121，照旧）

- **能做的**：只有拍照、看这次拍的（`camera.review`）、返回。
- **不出现的**：扫码、设置、定时、效果、“在相册里看”。
- **缩略图**：只显示这次拍的最新一张，点了是 `camera.review`；这次还没拍时不画。
- **照片存放**：照旧存在 `/family/锁屏拍的照片`，归第一个大人。
- **拍完怎么回**：HOME、返回会关掉受限相机，回到锁屏。
- 原样保留 `ink_camera_open_locked`、`ink_camera_close_locked`、`ink_camera_restricted`、`LOCKED_CAPS` 的语义。
- 输密码页上的相机入口是 `ink_shell_camera_button`，定义在 `camera.c`，由 `system/ui/shell/member_pages.c:139` 调用：
  - 签名不改；
  - 外观可以换成新样式（不套圆圈），改了就在报告里告诉 `shell-sweep-1`（锁屏样式任务 `lock-styles-1` 也看一眼）；
  - 不改 `member_*.c`、`lock_mark.c`。

### 6. 扫码页（`Scan`）只换样式和说法，行为不变

- 换成新共用控件，不写死宽度（`camera.c:162` 那种按语言乘倍数的写法去掉）。
- 识别规则不变：quirc、`parse()`；Wi-Fi 密码不显示、不给 AI；`scan_apply` 必须人来点；`can_open` 的条件不变；分页不变。确认卡的样式归 `dialogs-1`，这里不动。
- 没识别出来时说清楚，比如“没有看到二维码，请把码放在框中间再扫一次”，并保留“再扫一次”（`APPS.md` A13：重扫时保留当前目标）。

### 7. 没有摄像头时

用共用空状态说明，加一个返回。现在用的是圆圈关闭按钮（`camera.c:151`）。能力照旧不登记（`camera-absent` 测试的规则）。

### 8. 能力和快照

- **能力名照旧**：`camera.preview`、`camera.photo`、`camera.review`、`camera.scan`、`camera.scan_apply`、`camera.scan_next`、`camera.scan_prev`。`camera.capture` 在相机服务里，不碰。
- **新增**：只加真正新的动作，比如 `camera.cancel`（取消倒计时）。设置走 `settings.set`。
- **AI 拍照**：`camera.photo` 对人和 AI 一样遵守当前家人的定时设置，回执在存好或取消以后才回。
  - 现在三个能力的超时都是 15000 ms；要够最长定时加上取帧和保存，改了写进报告。
  - 回执要延后，先看现有能力调用怎么做延后回执，不要让倒计时占着执行器。
- **快照写清屏上是什么**：
  - 处在哪个状态：取景、倒计时剩几秒、看刚拍的、出错及原因；
  - 定时和效果的当前值；
  - 最近一张的名字和 `file:id`（锁屏下不写路径和 id，沿用现有规则）；
  - 每个能做的动作，以及它的能力和参数。

## 共同要求（页面任务都一样）

- **来源**：功能、页面、状态、出错处理照上面的来源，一个不落，不自己发挥。`docs/tasks/camera-3/SOURCE.md` 写每一块对应来源的哪个文件和行号，哪些没照搬、为什么（闪光灯、前后切换、录像、照片质量这类，写成“这台机器的硬件或现有功能没有”）。
- **新设计语言**：
  - 只用令牌和共用控件（`system/ui/ink/ui_theme.h`：`ink_ui_pill`、`ink_ui_text_button`、`ink_ui_segments`、`ink_ui_list_row`、`ink_ui_toggle`、`ink_ui_panel`、`ink_ui_empty`、`ink_ui_pagebar`），不再用 `ink_ui_circle`；
  - 能点的不小于 108 px，一屏最多一个主按钮；
  - 不画粗框，图标不套圆圈，没内容的区块不画；
  - 不写死像素和颜色（`tools/ui_scale_check.py`）；
  - 旧画板的对照分数（`tools/ui_compare.py`）不作验收。
- **刷新（D-127）**：只刷变了的那块。
  - 取景帧只刷取景区（沿用现有的 `INK_REFRESH_FAST`）；
  - 倒计时只刷数字，设置值只刷那个按钮，缩略图只刷那一格；
  - 拍后的照片和整页切换用 `INK_REFRESH_UI`。
  - 取景调度（显示空闲才取下一帧、离页停会话）是 `camera-fps-1` 的成果，不改慢，也不另起线程或定时器。
- **不变的规矩**：
  - 家人隔离：换家人时进行中的拍照作废，沿用 `epoch` 检查；
  - 孩子模式；
  - 确认卡只给 D-105 的高风险动作；
  - 按下反馈照 D-31；
  - 密码（Wi-Fi）不给 AI，也不进日志。
- **文字大小**：跟系统“文字大小”一起缩放（`ink_ui_px`），默认和最大一档各出一套截图。
- **原地重写**：旧页面代码删掉，不留两套。可以把 `camera.c` 拆成几个文件，见“测试的牵连”。
- **共享文件**：
  - `docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；
  - 新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由）；
  - 新文案的英文加进 `assets/i18n/en.tsv`；
  - 这两个文件都只在末尾追加本任务的行，合并冲突由主会话处理。
- **截图**放 `docs/tasks/camera-3/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。取景画面是模拟器合成的，要在报告里说明。

## 测试的牵连（先读，别踩）

先用 `grep -rn "apps/camera" tests ports/esp32s31/tests CMakeLists.txt` 再扫一遍，下面是已核对的清单。不少测试直接 `#include` 相机源文件，用里面的静态变量，或者按原文改源码做反向对照：
- `tests/lock_camera/lock_camera.c`：`#include camera.c`，用 `shots`、`taking`；
- `tests/camera_gallery/camera_gallery.c`：`#include camera.c`，用 `view`；
- `tests/port_camera/absent.c`：`#include camera.c`；
- `ports/esp32s31/tests/camera_preview.c`：用 `view`、`INK_CAMERA_APP_SOURCE`；
- `tests/qr_scan/device.c`、`tests/ui_fix_g/scan.c`：`#include scan.c`；
- `tests/lock_camera/run.py`：按原文替换 `bool limited = ink_camera_restricted();`、`ink_power_hold(INK_NEED_AWAKE);`、`ink_ui_drop_app("gallery");`；
- `tests/camera_gallery/run.py`、`tests/port_camera/negative.py`、`ports/esp32s31/tests/camera_preview.py`：同样按原文替换 `camera.c`；
- `tests/qr_scan/run.py`、`tests/ui_fix_g/run.py`：按原文替换 `scan.c`；
- `tests/i18n_literals.py`：按“文件路径 + 字面串”登记了 `/me/照片`、`/family/锁屏拍的照片`、`照片-%u.png`、`%s/照片-%u.png` 的豁免；拆文件后要把登记的路径改成这些字符串实际所在的文件，不许放宽成别的豁免；
- `tests/small_gaps_2/reverse.py`：`icons` 对照里列了 `camera.c`；
- 另外还有 `tests/i18n_apps.c` 里的截图文案（“低频取景”）、`tests/device_autotest`（`action.user.camera.preview >= 5`，`cap.script`、`device.script`）、`tests/audit3_fix.c`、`tests/audit14_locked.c`、`tests/audit14_reverse.py`、`tests/index_cost/device.c`、`tests/design_lang_1.c`、`tests/kid_apps.c`、`tests/lock_cover.c`、`tests/ui_borders_evidence.py`；
- 预算文件：`tests/integration/systest.budget.json`、`tests/simulator/board.budget.json`、`tests/simulator/board.pqbn.budget.json`，固件或内存预算变了要照实记，不许调松来过关；
- CMake：根目录 `CMakeLists.txt` 的 473、1882、1892 行列了相机文件，拆文件要同步改。

规则如下：
- 拆文件或改名时，这些测试只改定位方式（包含哪个文件、替换哪一行原文）。每个反向对照改完必须照样让测试失败：先跑一遍反向，确认失败，并贴出原话。
- 拍完从“接着取景”改成“停在这张”是本任务有意改的行为。相关断言改成同样严格的新断言。
- 每一条改过的已有测试都列进报告，写清“改前 → 改后、为什么不是放宽”。
- 不许为了通过而削弱、跳过测试或改它的时间。
- `camera-fps-1` 留下的 2 fps 基准不在本任务；不放宽它，也不许比现在慢。

## 范围和预算

- **只改**：
  - `system/apps/camera/`（可以拆成几个文件）；
  - 上面列的相机相关测试（只改定位方式和本任务有意改变的断言）；
  - CMake 里相机文件的那几行；
  - `docs/tasks/camera-3/`；
  - 共享文件末尾追加的行。
- **不碰**：
  - 相机服务和 HAL：`system/services/camera/`、`platform/include/ink/hal_camera.h`、`ports/esp32s31/hal_camera_s31.c`、`ports/esp32s31/vendor/dvp_camera/`；
  - 相册：`system/apps/gallery/`，`gallery-3` 同时在做；`photo.h` 和 `photo.open`、`file.thumb` 只读着用，缺接口写进报告；
  - 锁屏和家人页：`system/ui/lock_mark.c`、`system/ui/shell/member_*.c`（`lock-1` 已合，`shell-sweep-1` 在做）；
  - 设置：`system/policy/settings.c`、`system/ui/shell/settings_registry.c`（`settings-2`、`settings-d137` 在做）；
  - 主题和共用控件：`theme.c`、`ui_theme.h`、`ui_list.c`；
  - `system/ui/app_registry.c`。
- **和别的任务的关系**：开工前先看主线有没有合入 `camera-touch-bus`（这个任务目前没有文档，名字看会动相机的触摸或按键绑定）；如果动了 `camera.c`，以主线为准重写，不覆盖它的改动，并在报告里写清合并方式。`dialogs-1` 管确认卡样式，`lock-styles-1` 管锁屏样式，这里都不动。
- **预算**：产品代码不超过 650 行（旧的 delivery-plan 估 400 行，核对后偏小：现有 camera.c+scan.c 就 538 行，要新装下顶栏循环按钮、倒计时和取消、效果、设置小页、拍后查看、缩略图、五种出错、`camera.cancel`、新快照；最终数字由主会话确认，超了要停下来问，不要静默删功能）。删掉的旧代码不抵，从 AgentUI 逐字搬来的片段另计并写明。固件增量不超过 15 KB。
- **起步**：主会话给的分支，即当前主线（已含 `design-lang-1`、`design-lang-2`、`lock-1`、`settings-1`、`control-1`）。

## 验收

1. **照参照路径走一遍**（`APPS.md` A13，模拟器脚本，注入合成画面，每步截图并断言）：
   - 打开相机 → 取景出画面 → 顶栏把定时调到 3 秒 → 按快门 → 倒计时 3、2、1（只刷数字）→ 存好 → 取景区是刚拍的这张，状态行写着存在哪 → 左下角缩略图换成这张 → 点“在相册里看”，打开的正是这一张 → 返回 → “再拍一张”回到取景；
   - 倒计时中再按快门会取消，不存文件；
   - 效果选“黑白”拍一张，存下的 PNG 每个像素 R=G=B；选“原图”照旧是彩色；
   - 扫码：没有码的画面写清原因，有“再扫一次”；Wi-Fi 码不显示密码，快照里也没有。

   同一组操作用能力调用（来源是 AI）再走一遍，结果一样。
2. **出错不空白**：
   - 分别模拟没卡、卡借给电脑、卡满、写失败、摄像头没出画面（沿用 `tests/audit3_fix.c`、`tests/port_camera` 的注入办法），每种都有对应的话和下一步，快照里也是同一句；
   - 没有摄像头时是空状态加返回。
3. **锁屏相机（D-121）**：现有 `lock-camera` 的断言照样过（只能看这次拍的、HOME 回锁屏、孩子和大人的归属、拍照中按 HOME 不会解锁）；锁屏下没有扫码、设置、定时、效果和“在相册里看”；缩略图不会显示更早的照片。反向对照：让缩略图读 `/me/照片`，测试必须失败。
4. **刷新**（照 `tests/timer_seconds.c` 的办法，统计刷新区域和刷新方式）：
   - 倒计时 3 秒只刷数字那块，每秒不超过一次；
   - 拍完缩略图只刷那一格；
   - 改一个设置值只刷那个按钮；
   - 反向对照：倒计时改成整页重画，这条必须失败。
5. **家人隔离**：
   - A 拍的照片不会出现在 B 的缩略图里；
   - A 的定时设置不影响 B；
   - 倒计时中换家人会取消，不存文件。
6. **改前改后截图**：取景、倒计时、拍后查看、出错、设置、扫码结果、锁屏相机、没有摄像头，默认和最大文字大小各一套。
7. **全量和编译**：
   - 主工程普通全量 ctest 过（含 `ui-scale`、`camera-gallery`、`lock-camera`、`qr-scan`、`camera-absent`、`i18n` 相关）；
   - ASan 跑相机相关测试和 `ui-scene-*` 过；
   - S31 宿主机测试过（含 `unit-s31-camera`、`unit-s31-camera-negative`）；
   - `ports/esp32s31/build.sh` 编译通过、无新警告。
8. **报告要写**：
   - 固件增量、产品代码行数、`gcccheck` 结果；
   - 改过的已有测试（逐条）、改名或新增的能力、新增的 `camera.*` 设置项和设置表剩几格；
   - 给 `settings-2` 的登记说明、给 `shell-sweep-1` 的入口按钮说明；
   - 快门声选了哪个提示音；`camera.photo` 超时改成多少；
   - 上机核对清单：真机取景帧率不比现在慢，倒计时、拍后查看、缩略图在墨水屏上的刷新和残影，U 盘模式下拍照的说法。

## 要用户定的（写进报告，不自己定）

- 录像：现在没有这个功能，本任务不做。是否要做，由用户决定。
- 产品代码行数上限从 400 放到 650，是否接受，由主会话在派任务前确认。
