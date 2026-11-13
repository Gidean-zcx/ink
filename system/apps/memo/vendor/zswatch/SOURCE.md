# ZSWatch 录音页（拷来的原文）

- 项目：ZSWatch，<https://github.com/ZSWatch/ZSWatch>
- 固定提交：`821c340366a0bc55b536f7feb5d45ca3836c44c3`
- 原路径：`app/src/applications/voice_memo/`
- 许可证：GPL-3.0（原项目根目录 `LICENCE`，原样拷在这里；两个源文件的版权和许可文件头原样保留）。

| 文件 | 原文 SHA-256 | 说明 |
| --- | --- | --- |
| `voice_memo_ui.c` | `1a1e25b0ef3269c7b3e24bcd5015545e749b73b62c560c50a884bc81a2575f40` | 拷进来后改了下面列的行 |
| `voice_memo_ui.h` | `cfcba59e24848a84de2952cc1948c5cdaf6cb77776a820abe3f4b68c6728b994` | 原样，没改 |
| `LICENCE` | `8b1ba204bb69a0ade2bfcf65ef294a920f6bb361b317dba43c7ef29d96332b9b` | 原样 |

原样拷入的提交是 `91d61467`（只有这三个文件，内容和上游一致），之后的改动都能用 `git diff 91d61467 -- system/apps/memo/vendor/zswatch/voice_memo_ui.c` 看到。

## 改了哪些行（行号是原文的）

| 原文行 | 改成什么 | 为什么 |
| --- | --- | --- |
| `:65-67` | 删掉 `dot_visible`、`blink_counter` 两个字段 | 墨水屏不闪 |
| `:70-112` | `#if VOICE_MEMO_UI_LIST` 包起来（相对时间、录音键回调） | 列表页不用：一条语音备忘就是备忘列表里的一行 |
| `:122-141` | `#if VOICE_MEMO_UI_LIST && LV_USE_MSGBOX` | 删除确认框：列表页不用，`lv_conf.h` 也没开 msgbox |
| `:143-165` | `#if LV_USE_MSGBOX` | 返回确认框：msgbox 没开；我们录音中返回就等于“记好了” |
| `:167-253` | `#if VOICE_MEMO_UI_LIST` | 列表页和删除按钮 |
| `:258` | 大小改成 `lv_pct(100)` | 录音页占内容区，状态栏留在上面 |
| `:264-271` | 红色圆点换成文字标签“正在记”（`TR`），字体用 28 号对应的标题字 | 不用颜色、不闪，状态写成字 |
| `:273-274` | 删掉闪动计数的初始化 | 同上 |
| `:278` | `"00:00"` 改成 `TR("不到 1 分钟")` | 计时按分钟 |
| `:281` | 位置换成 `ink_ui_px(令牌)`，行高跟系统文字大小走 | 字号放大时不重叠 |
| `:283-290` | `#if LV_USE_BAR` | 一直在变的音量条在墨水屏上要一直刷；`lv_conf.h` 没开 bar |
| `:297` | 剩余时间的位置换成令牌 | 同 `:281` |
| `:299-307` | 停止键换成共用的 `ink_ui_pill`（写“记好了”的主按钮，108 px 高，药丸形），去掉 `LV_RADIUS_CIRCLE` 和红色；加一行 `LV_OBJ_FLAG_CLICKABLE`（共用控件不带回调时不可点） | 新设计语言不用圆圈；按钮写清做什么 |
| `:308-312` | 删掉停止符号标签 | 按钮上已经写着“记好了” |
| `:324` | `create_list_screen()` 用 `#if VOICE_MEMO_UI_LIST` | 列表页不建 |
| `:330-337` | `#if LV_USE_MSGBOX` | msgbox 没开 |
| `:361-364` | `list_screen` 的判断和隐藏用 `#if VOICE_MEMO_UI_LIST` | 列表页不建，否则 `show_recording` 会直接返回 |
| `:370`、`:374`、`:387` | `#if LV_USE_BAR` | 音量条 |
| `:373` | `"00:00"` 改成 `TR("不到 1 分钟")` | 计时按分钟 |
| `:390` | `"Saving..."` 改成 `TR("正在保存录音")` | 中文 |
| `:395-399` | `list_screen` 的几行用 `#if VOICE_MEMO_UI_LIST` | 同上 |
| `:408-412` | 计时从 `mm:ss` 改成“不到 1 分钟”或“%u 分钟” | 每分钟最多刷一次 |
| `:414-419` | 删掉闪动 | 墨水屏不闪 |
| `:421-424` | `"m:ss left"` 改成 `TR("还能录约 %u 分钟")` | 按分钟，中文 |
| `:427-433` | `#if LV_USE_BAR` | 音量条 |
| `:435-519` | `#if VOICE_MEMO_UI_LIST` | 列表页 |
| `:521-543` | `#if LV_USE_MSGBOX` | 返回确认框 |

超出任务书所列范围的两处：`:299-307` 加的 `LV_OBJ_FLAG_CLICKABLE`（共用 `ink_ui_pill` 不传回调时去掉了可点，原文的 `lv_obj_add_event_cb` 才能生效）；`:273-274` 删掉闪动计数初始化（字段已删）。另外原文多处用 `lv_color_white()` 画字（深色表盘），这些行没改，由垫片头文件换成墨色（见下）。

## 五个垫片头文件（`port/`，只对 `voice_memo_ui.c` 和接线层 `memo_voice.c` 两个文件生效）

主工程 `CMakeLists.txt` 和 S31 `ports/esp32s31/components/ink_system/CMakeLists.txt` 都用按文件的 `INCLUDE_DIRECTORIES` 设置；`memo_voice.c` 也要这条路径，因为它包含原文的 `voice_memo_ui.h`（里面引用 `managers/zsw_recording_manager_store.h`）。`memo_voice.c` 不包含 `zephyr/` 下的任何头文件。

| 垫片 | 接到哪里 |
| --- | --- |
| `zephyr/kernel.h` | 标准整数类型和 `ARG_UNUSED` |
| `zephyr/logging/log.h` | `LOG_MODULE_REGISTER` 展开为空，`LOG_DBG/INF/WRN/ERR` 接 `ink_log` |
| `ui/zsw_ui.h` | `zsw_color_red/blue` 是墨色，`gray` 是次要灰，`dark_gray` 是分隔线灰；`lv_color_white()` 换成墨色（原文是深色表盘上的白字，纸色背景上看不见）；`lv_font_montserrat_10/12/14/16/20/28` 换成我们的中文字体 `ink_ui_font_px(INK_FONT_…)`，跟系统文字大小走；包含 `ink/sys/i18n.h`（`TR`）和 `ink/ui_theme.h`（令牌和 `ink_ui_pill`）；定义 `VOICE_MEMO_UI_LIST 0` |
| `zsw_clock.h` | `zsw_clock_get_time` 接 `ink_time_now_s`，`zsw_timeval_to_tm` 接 `ink_time_to_local`（只有关掉的相对时间用到） |
| `managers/zsw_recording_manager_store.h` | `zsw_recording_entry_t`、`VOICE_MEMO_MAX_FILENAME 32`；`ZSW_RECORDING_MAX_FILES` 定为 1，`ui` 结构里文件名数组只占 32 字节 |

## 没拷的

- `voice_memo_app.c`：它是 Zephyr 的应用管理、zbus 和 `k_work` 这一层（原文 `:18-60` 注册应用和监听，`:290-298` `SYS_INIT`），在我们这里对应的就是接线层 `system/apps/memo/memo_voice.c` 本身。照它做的行为：停止后 2 秒内点“说一句”不开始新录音（`:63-66`、`:130-135`）、停止后先显示保存中再等结束事件（`:145-155`）、剩余时间的算法（`:200-201`，换成我们的 24 kbit/s）、录音中返回就停止并保存（`:248-252`）、录音中离开再回来回到录音页（`:281-288`）。
- `voice_memo_shell.c`、`CMakeLists.txt`、`Kconfig`：Zephyr 的命令行和构建，用不上。

## 测试

原项目没有这个界面的测试可带。我们的宿主测试在 `tests/memo_3/memo_3.c` 的 `vendor` 一段（ctest `memo-3`），直接编译这个文件，覆盖录音中、保存中、计时按分钟、`remove` 后对象清空；`python3 tests/memo_3/run.py --build build --mutant blink` 把闪动恢复进 `update_time`，测试必须失败。
