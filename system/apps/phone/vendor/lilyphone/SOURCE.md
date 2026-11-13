# LilyPhone 片段（phone-3）

## 出处

- 项目：LilyPhone，`github.com/dparnell/lilyphone1`，作者 Daniel Parnell。
- 固定提交：`de10fc3882292b9be1cfcd14632726590fdef262`（本机只读副本 `ink-hw/upstream/oss/lilyphone1`）。
- 许可证：仓库里没有许可证文件，GitHub 上 license 为空，两个源文件也没有版权头。按 D-129 用户原话“不需要在乎开源协议”照搬；这里如实记下，并保留作者和出处。仓库是公开的。

| 原文件 | SHA-256（已核对，和 `replan-sysapps/sources.json` 一致） |
| --- | --- |
| `src/apps/phone_store.cpp` | `026492851b85169a6b7c16fe251b06c55ac336cd090c715f93864847a0e8db0d` |
| `src/ui/ui_phone1.cpp` | `4515a8a8f64b8ed35dbacf6ddc6e278aa175ca3997e467b872ab01b6ed3240cb` |

## 拷了什么（整函数原样拷）

| 放到 | 原文 | 内容 |
| --- | --- | --- |
| `phone_store.c` | `phone_store.cpp:1-9` | 文件头注释 |
| | `:18-19`、`:21`、`:23` | `MATCH_DIGITS`，表 `contact_list`、`contact_num` 两个静态变量 |
| | `:111-135` `phone_number_match` | 两个号码是不是同一个人（任务书写的是 `:111-138`，原文函数实际到 `:135` 结束，后三行是下一个函数的开头，没拷） |
| | `:190-196` `contacts_find_by_number`、`:198-203` `contacts_display_name` | 按号码找联系人、显示名字 |
| `call_card.c` | `ui_phone1.cpp:3262-3263`、`:3265` | 静态变量 `scr8_1_status`、`scr8_1_answer`、`scr8_1_shown_state`（`:3264` 的 `scr8_1_timer` 不拷） |
| | `:3266-3277` `scr8_1_answer_event`、`scr8_1_hangup_event` | 接听、挂断按键 |
| | `:3279-3318` `scr8_1_render` | 通话状态变文字，“接听”只在来电时显示 |
| | `:3334-3383` `create8_1` | 通话页上半部：名字（查名）、号码、状态行、接听/挂断那一排 |

文件名从 `.cpp` 改成 `.c`，按 C 编译；原文本来就是 C 写法。LVGL 8 的调用（`lv_obj_clear_flag`）靠 LVGL 9.5 自带的 `lv_api_map_v8.h`（`lvgl.h` 已包含）编过。

第一次提交 `a95ddf6c` 是原样拷贝，下一个提交才改，`git diff a95ddf6c -- system/apps/phone/vendor/lilyphone/` 能看到下面每一处改动。

## 改了哪几行、为什么（对原文的改动共 27 行）

`phone_store.c`

| 原文 | 改成 | 为什么 |
| --- | --- | --- |
| `#include <Arduino.h>`、`"FS.h"`、`"SPIFFS.h"`、`"phone_store.h"` | `<ctype.h> <string.h> <stdbool.h>`、`"lily_compat.h"` | 没有 Arduino 和 SPIFFS；`phone_store.h` 由替身头文件代替 |
| `#define MATCH_DIGITS 7` | `#define MATCH_DIGITS 11` | 中国手机号 11 位：末 7 位相同会把 138xxxx5678 和 139xxxx5678 当成同一个人。改成 11 后 `+8613812345678` 和 `13812345678` 照样相同，10086 这类短号照旧要整串相同。单测 `phone-3-match-reverse` 用 7 编一份，“138/139”那条必须失败 |

`call_card.c`

| 原文 | 改成 | 为什么 |
| --- | --- | --- |
| `scr8_1_hangup_event` 里的 `scr_mgr_pop(false);` | 删掉 | 结果留在通话页，不自动关（墨水屏不为关页多刷一次） |
| `scr8_1_render` 里的 `char buf[40];` | 删掉 | 下面不再按秒格式化，变量没用了（`-Werror`） |
| `"Incoming call"`、`"Calling..."`（两处） | `TR("来电")`、`TR("正在拨号")` | 中文界面，翻译检查 |
| `MODEM_CALL_ACTIVE` 分支的 `secs`、`lv_snprintf("Connected  %02u:%02u")`、`lv_label_set_text(buf)` 四行 | `lv_label_set_text(scr8_1_status, TR("通话中"));` | 墨水屏不跑秒（第 11 节）：时长在胶水的“N 分钟”那一行，每分钟只刷那一块 |
| default 分支 `"Call ended"` | `ui_phone_end_text()` | 结束原因和按秒的结束时长由胶水给（“通话结束 · 1 分 05 秒”“对方忙或拒接”等） |
| `"Unknown caller"` | `TR("未知号码")` | 中文界面 |
| `lv_pct(92)`（三处） | `ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN)` | 主题令牌，和其他页面一样的左右边距 |
| `lv_obj_align(who, LV_ALIGN_TOP_MID, 0, 62)` | `… ink_ui_px(INK_STATUS_BAR + INK_BLOCK)` | 写死的坐标换令牌，躲开状态栏 |
| `lv_obj_set_height(sub, 18)` | `ink_ui_px(ink_ui_line_height(INK_FONT_NOTE))` | 随系统字号放大 |
| `lv_obj_align(sub, …, 0, 126)`、`lv_obj_align(scr8_1_status, LV_ALIGN_CENTER, 0, 20)` | `lv_obj_align_to(…)` 接在上一行下面 | 名字随字号变大会折行，固定坐标会压住 |
| `scr_action_bar_create(parent, 58)` | `INK_PRIMARY` | 主按钮高 108 |
| `LV_SYMBOL_CALL "  Answer"`、`LV_SYMBOL_CLOSE "  Hang up"`、宽 `108` | `TR("接听")`、`TR("挂断")`、`INK_CONTROL` | 我们的字体没有 LVGL 符号字形；文字按钮 |
| `lv_obj_set_height(…, 48)`（两处） | `ink_ui_px(INK_PRIMARY)` | 触摸目标不小于 108 px |
| `scr_back_btn_create(parent, "Call", scr8_btn_event_cb);` | 删掉 | 返回由外壳负责 |

`ui_disp_full_refr()` 那两行没改：替身头文件把它定义成空操作。改文字时 LVGL 只让那个标签失效，局刷自然只刷它。

## 胶水（我们写的，算产品代码）

- `lily_compat.h`：`phone_store.h` 的替身（`contact_t` 长度改成 `phone.contacts` 的 97 和 25，`CONTACTS_MAX` 200），`modem_call_state_t` 和 `ui_phone_*`、`scr_*` 的声明，字体、颜色换成主题令牌。
- `phone_store.c` 末尾的 `lily_contacts_bind`：表放在 `phone.c` 的 PSRAM 里，读联系人只在 `phone.c` 的 `load_contacts` 一个函数里做。
- `call_card.c` 末尾的 `lily_call_card`、`lily_call_card_render`、`lily_call_card_gone`：代替原文的 `entry8_1`、3 秒定时器和屏幕管理器。
- `phone.c` 里的 `ui_phone_get_call_state/number`、`ui_phone_end_text`、`ui_phone_answer/hang_up`（接到 `call.answer`、`call.hangup`）、`scr_action_bar_create`、`scr_bar_btn_create`（`ink_ui_pill`，“挂断”是这一页唯一的主按钮）。

搬来的只是名字、号码、状态文字、接听和挂断这一段。结束原因、按秒的结束时长、“再打一次/完成”、静音/键盘/声音去向/录音四个按钮、通话中分钟刷新，原文都没有，是我们写的胶水。

## 不搬

- `contacts_reorder`、`contacts_add`（`phone_store.cpp:207-251`）：改通讯录是联系人应用的事（contacts-3）。
- SPIFFS 存储、短信表。
- `peri_modem.cpp` 的模组层：只拿它 `:508-510`、`:615-616` 认 `BUSY`/`NO ANSWER` 的做法当证据（两个 HAL 各加了一段识别）。
- 3 秒定时重画：`scr8_1_timer_event`（`:3322`）和 `entry8_1` 里的 `lv_timer_create`：不轮询，只在 `call.*` 事件和每分钟时更新。
- 拨号页那一行名字 `scr8_update_name`（`:3124-3133`）：被 Fossify 的多候选取代。

## 测试

LilyPhone 没有主机测试（`test/` 只有 README）。新写的 `tests/phone_3/match.c`（ctest `phone-3-match`）覆盖 +86、0086、空格和横线、10086 整串相同、8 位座机和带区号不算同一个、138/139 后 8 位相同不算同一个，以及 `contacts_display_name` 用 `+86` 写法查到存成 `138 1234 5678` 的联系人。`phone-3-match-reverse` 用 `MATCH_DIGITS 7` 编同一份测试，“138/139”和“座机”两条失败。
