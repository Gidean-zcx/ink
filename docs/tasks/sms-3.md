# sms-3：短信照 Fossify Messages 重写——会话里直接写、每个会话各有草稿、分段和剩余字数、明确没发出的原消息“重新编辑”或“重发”、结果未知只许人自己再发一次、“已发出”和“已送达”分开写；状态区分对照 Lineage；LilyPhone 的短信显示和删除整函数搬进来

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。

**D-140（总规则第 11 节）**：交付计划这一行写的是“照着重写”。调研（`replan-sysapps/SOURCES.md` E02）和本任务书点名了 LilyPhone 的短信代码，这部分按“片段可搬”办，做法和 phone-3 一样：

- **第一步**：把对照表提交成 `docs/tasks/sms-3/对照表.md`，单独一个提交，写代码之前。主会话随本任务书给了一份草稿，可以补行、改行，但每一行都要有决定；改了决定写明理由。
- **LilyPhone 片段拷原文**：下面“搬什么”列的整函数都要拷。`system/apps/sms/vendor/lilyphone/SOURCE.md` 记项目、固定提交、文件的 SHA-256、改了哪几行、为什么。不许看完原文按自己的风格重写一遍。
- **不拷第二份**：phone-3 已经搬进来的 `phone_number_match`（`system/apps/phone/vendor/lilyphone/phone_store.c`）直接链接着用。本任务不改 `system/apps/phone/`。
- **其余照对照表重写**。参照安卓的地方按第 11 节为这台设备改；做完逐行验收，每行配截图或测试名。

## 为什么

用户 2026-11-12 说过三句话：

- “所有的应用和系统应用都是移植的最优秀的三方项目吗 我怎么感觉不太像呢”
- “好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”（D-129）
- “我需要你先把当前的壳和现有已有的应用移植或参考重写搞到成熟可用”（D-131）

2026-11-13 又说：“对 按着实现 参考安卓也行 但是要注意设备特性 不要生搬硬套导致使用体验反而不佳”（D-140）。D-135 定了：电话、短信这类系统应用重写后直接替换旧的。

调研的建议（`docs/tasks/replan-sysapps/APPS.md` A02）：会话、编辑、失败处理照 Fossify 重做，状态区分取 Lineage，嵌入式显示代码取 LilyPhone；通信服务、分段计费提示和成员草稿用我们自己的。参照路径是：没存的号码 → 长中文草稿 → 分段提示 → 发送 → 明确失败 → 原消息重发；另外核对“结果不确定”时不能自动重发。

## 现状（基线 `main` 的 `3baaf0e2`）

这个基线已经合入了 design-lang-2、home-1、lock-1、control-1、settings-1/d137、notify-svc-1、graft-mogo-4、reader-one-1、auto-provision-1、font-pack-1、camera-touch-bus、tap-loss-1，以及 wave 2a（phone-3、memo-3、files-3、gallery-3、music-3、almanac-2、lock-styles-1）。

### `system/apps/sms/sms.c`（466 行，一个文件；最后一次改是 `08eabea2`，在 tap-loss-1 和 phone-3 之前）

**页面**（`:444-447`）：`Sms`（会话列表）、`SmsThread`（会话页）、`SmsCompose`（写新短信）、`ImeLong`（长文编辑）。

**能力**（`:452-463`，共 10 个）：`sms.to`、`sms.pick`、`sms.retry`、`sms.thread`、`sms.compose`、`sms.recipient`、`sms.text`、`sms.long`、`sms.prev`、`sms.next`。短信本身由服务的 `message.send` 发（`:196`）。

**会话列表**（`bind_item`，`:209-222`）：

- 每页 4 行（`SMS_ROWS`，`:19`）。行头写 `"● 名字 · 时:分"`，没有日期，昨天和上个月的同一时刻看不出区别；第二行是最后一条的全文，不收拢换行。
- 最后一条是自己发的、发送中还是没发出，行里都看不出来；没有“草稿：”。
- 未读靠每画一行就把通知收件箱从头走一遍（`unread()`，`:62-70`）。
- “新建”在顶栏（`:411-415`）。没有搜索。

**按号码分会话是逐字比较**（`build_rows`，`:304-305` 用 `strcmp`）。同一个人发来的是 `+8613812345678`，我们回过去的是 `13812345678`，会被分成两个会话。名字也一样逐字比：`ink_contacts_match(peer, true, …)` 是 `strcmp(number, query)`（`system/apps/contacts/contacts.c:53-61`），所以带 `+86` 的来信查不到存成 `138…` 的联系人。

**会话页**（`thread_layout`、`bind_thread`，`:237-299`）：

- 从最新的一条往上排，按页翻，长消息跨页切开（`text_end`，`:198-208`）。隔 5 分钟以上才写时间行（`:317`）。
- 发出的消息画成黑底白字的卡片，收到的画成灰卡片（`ink_ui_card(... INK_SURFACE_BLACK ...)`，`:286`），这是旧设计语言的重框。
- 底部已经有编辑区：“展开”、共用输入框、“发送”（`reply_field`、`:361-374`）。所以“在会话里直接回”现在就有。
- 每条发出的消息下面写服务给的状态原话（`:288`）。

**发送中**：

- 点“发送”后，按钮变忙，提示行写“发送中”（`:194`），会话里看不到这条，要等回执回来（最长 30 秒）才出现。
- 服务在发之前已经把这条存成“结果未知，请先核实”（`comm_service.c:543`）。所以发的时候断电，重开后看到的是“结果未知”，这是对的。

**失败处理**：

- 明确没发出的那条写“发送失败，未发出（点按重发）”（`:243`）。点它走 `sms.retry`，跳到另一页 `SmsCompose`，正文带过去（`:163-166`）。
- 再发一次，服务会新存一条（`comm_service.c:141`：`id` 为 0 就取新编号；`cap_send` 每次都传 0，`:542`），原来那条失败的还留着。会话里就同一段话出现两次，一条失败、一条已发出。
- “结果未知”那条只有原话，没有任何动作。

**草稿**：

- 只有一份，在内存里（`S.draft`，`:31`）。打开另一个会话会清空（`:168`），换家人也清空（`:143`），重开机就没了。列表里不显示草稿。
- 长文页 `ImeLong` 另有一份文本服务的草稿（`draft_ref`，`:32`、`:356-357`）。

**分段提示**（`parts_note`，`:56-61`）：只在超过一条时写“将按 N 条发送”，超过 5 条写“太长了，最多分 5 条发送”。不写这一条还能打几个字。

**删除**：没有删一条、也没有删整个会话的入口和能力。

**被动刷新**（tap-loss-1 之后的规矩）：新短信到达时，`refresh()` → `refresh_thread()` → `bind_thread()` 会马上删掉、重建会话区的子控件（`:86-92`、`:272`），手指按着时不等。列表那条路走 `ink_ui_redraw()`，已经会等。

**快照**（`:422-442`）：会话页给可见消息、翻页、发送参数和分段提示。

### 短信服务 `system/services/comm/comm_service.c`（882 行；本批只归 phone-3，**本任务不改**）

- **集合 `sms.messages`**（`:63-64`）：按家人分开，`ai_read`，`max_records = 512`，**没开 `drop_oldest`**。存满 512 条后写入会被拒（`system/services/data/datastore.c:810-813`），新来的短信就存不下了（探针 `sms.save_failed`，`:150`）。
- **一条记录**是 `{"peer","text","status"}`（`save_message`，`:137-143`），`sort` 等于编号。时间用记录的 `updated_s`，没有已读字段。收到的写 `"收到"`（`:150`）。
- **发送**（`cap_send`，`:525-561`）：
  - 先存“结果未知，请先核实”（`:543`），再发；
  - 回执的 `object` 是 `message:to:<号码>`（`:556`）。能走到这一步，记录就一定已经存在；
  - 最后在同一编号上写最终状态（`:558`）。
- **状态原话**（`send_status`，`:482-490`）：
  - “已发出”；
  - “发送失败，未发出”；
  - “结果未知，请先核实”；
  - 长短信的“共 N 条：前 k 条已发出，后 m 条未发出”，以及带“第 k 条结果未知”的三种。
  - **没有“已送达”**：驱动发短信时不要送达报告。
- **发送入口有两个**：`message.send`（`:724-736`）和 `sms.send`（`:738`），跑的是同一个 `cap_send`。AI 和远程不能用草稿引用发（`send_text`，`:504-522`）。
- **`message.list`** 只数 `"收到"` 的记录（`cap_list`，`:586-605`）。
- **分段**：`ink_comm_sms_parts`（`:310`）只返回条数。规则在驱动里：全是 GSM 7 位字符时一条 160、拼接后每条 153；否则一条 70、拼接后每条 67（`drivers/chips/nt26/nt26.c:260-283`）。最多 5 条，正文最多 1024 字节（`system/include/ink/sys/comm.h:14-15`）。

### 别处依赖短信的地方（都不改，接口不能变）

- **通知服务**（`system/services/notify/notify_service.c:672-687`）：新短信的通知有“打开会话”，调 `sms.thread {peer}`；快捷回复调 **`sms.send {to}`**。未读状态在通知收件箱里，`channel = "message"`，`group` 是来信号码。
- **未接来电的“发短信”**（`notify_service.c:625`）、**电话详情**（`system/apps/phone/phone.c:799`）、**联系人详情**（`contacts.c:250`）：都调 `sms.compose {number}`。
- **确认卡**：`sms.send`、`sms.compose` 显示“发送”（`system/ui/shell/confirm_card.c:66`）。
- **孩子用完时间以后**：`sms.*`、`message.*`、`contacts.*` 仍放行（`system/policy/members.c:920-922`），但 **`system.undo` 不在放行名单里**（撤销能力的 `app` 为空，走不到 `:914` 的应用放行）。`data.list` 只放行 `phone.contacts`、`phone.calls`、`sms.messages`（`system/policy/cap.c:150-154`）。
- **数据集合的登记数快满了**：上限 32（`MAX_DEFS`，`datastore.c:44`），现在已经登记 31 个。本任务**不新建集合**。

### 已经有、可以直接用的

- **号码比对**：phone-3 搬来的 `phone_number_match`（`phone_store.c`，`MATCH_DIGITS` 已改成 11）。`+8613812345678` 和 `13812345678` 算同一个，10086 这类短号要整串相同。它已经编进 `ink_system`（根 `CMakeLists.txt:1810`）。
  - **注意**：同一个文件里的 `contacts_find_by_number`、`contacts_display_name` 读的是一张全局表，由 `phone.c:179-193` 的 `load_contacts` 绑定。短信**不许**重新绑定这张表，不然电话页会查到短信填的表。
- **可撤销删除的写法**：phone-3 的 `phone.delete`、`phone.restore`（`phone.c:891-938`）。删掉的记录原文放进 PSRAM 的撤销区，`ink_cap_undoable` 登记，页面写“撤销”，留到下一次操作（`phone.c:754-758`）。写满时等一下再写：`put_waiting`、`del_waiting`（`:848-855`）。
- **找不回来时先确认**：处理函数自己调 `ink_confirm_hold`（`system/include/ink/sys/confirm.h:45`），`comm_service.c:539` 是先例。
- **剪贴板**：`ink_clip_set`（`system/include/ink/sys/textinput.h:134-141`）。
- **tap-loss-1 的接口**（`system/include/ink/sys/ui.h`）：
  - `ink_ui_redraw`（`:88`）手指按着时自动等；
  - `ink_ui_after_press`（`:92`）给不整页重建的被动更新用；
  - `ink_ui_content_changed`（`:164`）；
  - `ink_ui_touch_current`（`:168`）。
- **列表控件**：`ink_ui_list` 每个格子只能登记一个动作（`system/include/ink/sys/ui_list.h:11-13`）。会话页现在的做法是整页一个格子，自己写一份 `ink_ui_page_spec_t` 来列每条消息的动作（`:274-292`）。
- **共用输入框没有设置文字的接口**（`system/include/ink/sys/textfield.h`）。要清空只能删掉重建，现在 `sms.c:123` 就是这么做的。
- **模拟器**：
  - 明确没发出：先拔卡、注销网络（`sim_net_set_card(0,false,…)`，`tests/sms/sms.c:93-95`）；
  - 结果未知：卡在但没有短信业务（`sim_net_set_card(0,true,false,true)`，`:97`、`:129`）；
  - 长短信只发出前几条：`sim_net_sms_fail_part(n)`（`simulator/sim.h:356`）；
  - 计数：`sim_net_sms_sent`（`:353`）、`sim_net_sent_sms`（`:367`）。
- **`tools/ui_scale_check.py` 默认只查 `system/ui` 和 `apps/home`**（`:339`），不查 `system/apps/`。本任务要另外显式跑 `python3 tools/ui_scale_check.py system/apps/sms`。

## 成熟来源（选型已定，不重新调研）

本机没有 Fossify 和 Lineage 的副本，按固定提交 shallow clone 到自己的临时目录（`git fetch --depth 1 origin <提交>`）。下面的行号都已在固定提交上核对过。

- **Fossify Messages**（第一参照），`github.com/FossifyOrg/Messages` 提交 `d9510f44b03c82869856d89a0fcd30b880df9688`。路径前缀 `app/src/main/kotlin/org/fossify/messages/`。`activities/ThreadActivity.kt` 的 SHA-256 是 `bc982ebd…4ad7499`，和 `sources.json` 一致。
  - `activities/ThreadActivity.kt`：
    - `:299` 打开会话时读回草稿；`:315-326` 离开时存草稿；`:343-352` `saveDraftMessage`：有字就存，没字就删。
    - `:690-698` `handleItemClick`：点失败提示，原文填回输入框，并记下 `messageToResend`。
    - `:886-901` 一改字就把 `messageToResend` 清掉，另算字数和条数（计数器默认不显示，`helpers/Config.kt:19-20`）。
    - `:1322-1330` 隔 5 分钟以上才写时间行（`MIN_DATE_TIME_DIFF_SECS = 300`，`:2206`）。
    - `:1333-1389` `getThreadItems`：失败的消息下面接 `ThreadError`，发送中的接 `ThreadSending`；只在最后一条已发出的下面接 `ThreadSent`，送达时打两个勾。
    - `:1585-1603` `sendMessage`；`:1672-1694` `sendNormalMessage`，发完马上清空输入框。
    - `:754` `jumpToMessage`、`:837` `SEARCHED_MESSAGE_ID`：从搜索结果点进来，定位到那一条。
    - `:1176-1191` 删整个会话前先确认。
    - 菜单 `res/menu/menu_thread.xml`：打电话、归档、加为联系人、复制号码、会话详情、标为未读、拉黑、删除。
  - `messaging/MessagingUtils.kt:28-80`：**带 `messageId` 重发时是更新原来那条记录，不新增**（`:62-74`）。`:83-120` 是 `sendSmsMessage`。
  - `activities/MainActivity.kt`：`:445-450` 没有会话时的提示；`:521-541` 打满 2 个字就开始搜，搜正文和会话；`:543-612` 结果先列会话再列消息，点了进会话并定位。
  - `adapters/BaseConversationsAdapter.kt:143-195`：列表行写对方、摘要（有草稿就写草稿）、日期，未读用粗体加数字。
  - `adapters/ConversationsAdapter.kt:45-84`：长按多选后的动作。
  - `adapters/ThreadAdapter.kt`：`:131-155` 长按消息后的动作（复制、另存、分享、转发、选字、删除、详情）；`:276-300` 删前确认，可进回收站；`:628-643` 发出、失败、发送中三种提示。
  - `activities/NewConversationActivity.kt:88-136`：输号码或名字，下面列匹配的联系人。
- **Lineage Messaging**（状态对照），`github.com/LineageOS/android_packages_apps_Messaging` 提交 `47eb88d3219eea82905f844baa9240925329ab6f`。`MessageData.java` 的 SHA-256 是 `36ec372b…1c940`，`ComposeMessageView.java` 是 `9372a055…bbc98`。
  - `src/com/android/messaging/datamodel/data/MessageData.java:120-142`：状态分成已发出、已送达、草稿、待发、发送中、重发中、等待重试、失败、紧急号码失败。
  - `src/com/android/messaging/ui/conversation/ConversationMessageView.java`：`:341-375` 每种状态在消息下面写什么；`:425-427` 送达的标记。
  - `src/com/android/messaging/ui/conversation/ComposeMessageView.java`：`:145-228` 编辑区的结构；`:670-689` 计数规则：超过一条，或这一条剩不到 10 个字（`:112`）才显示“剩余 / 条数”。
  - `res/values/strings.xml:108-117`：“Sending…”“Not sent. Tap to try again.”“Resend or delete”等原文。
- **LilyPhone**（搬代码），`github.com/dparnell/lilyphone1` 提交 `de10fc3882292b9be1cfcd14632726590fdef262`。本机只读副本 `ink-hw/upstream/oss/lilyphone1`，`src/ui/ui_phone1.cpp` 的 SHA-256 是 `4515a8a8f64b8ed35dbacf6ddc6e278aa175ca3997e467b872ab01b6ed3240cb`（已核对）。
  - 原任务书写的 `:2807-2851`，实际是交接用的变量、`ui_sms_dispatch` 和 `ui_set_active_number`。
  - 原任务书写的 `:3880-3930`，实际是删除那几个函数，加上反应消息的开头。
  - 下面按实际的函数边界写。

## 搬什么、怎么编进来（第 11 节）

文件名从 `.cpp` 改成 `.c`，按 C 编译。这几段原文本来就是 C 写法，phone-3 已经这样编过。LVGL 8 的调用（比如 `lv_obj_clear_flag`）靠 LVGL 9.5 自带的 `lv_api_map_v8.h` 编过。第一次提交原样拷贝，下一个提交才改，`git diff` 能看出每一处改动。

| 原文（`src/ui/ui_phone1.cpp`，整函数） | 放到 | 用途 | 怎么接 |
| --- | --- | --- | --- |
| `:2825-2843` 注释和 `ui_sms_dispatch` | `system/apps/sms/vendor/lilyphone/sms_view.c` | 发送入口：号码、正文不能空；一次只发一条；先把这条记成“发送中”，再等结果 | `ui_sms_send` 接胶水：建文本草稿、调 `message.send`，返回非零。`sms_add` 接胶水：往 PSRAM 的会话索引里插一条临时的“发送中”。`time(NULL)` 用替身。`ui_send_watch_*` 放在胶水里，回执到了清零 |
| `:2851-2878` `ui_message_snippet` | 同上 | 列表行和搜索结果的一行摘要：换行和连续空白并成一个空格，去掉控制字符 | 原样拷，加 `<ctype.h>` |
| `:2880-2902` `ui_format_stamp_compact`、`:2904-2926` `ui_format_stamp` | 同上 | 列表右边的时间：今天写时分，别的天写日期；会话里的时间行 | `time`、`localtime_r` 用替身，按 `ink_time_to_local` 填 `struct tm`（通知服务搬 PebbleOS 时的 `pbl_localtime_r` 是先例）。两个 `strftime` 格式串改成 `TR("%m月%d日")`、`TR("%m月%d日 %H:%M")` |
| `:3733-3782` `scr13_populate` | 同上 | 会话列表每一行：未读标记、名字或号码、最后一条的摘要和前缀（我、发送中、未发出）、时间 | `sms_thread_count/number/last/unread` 接胶水，读 PSRAM 会话索引。`scr_row_create` 接胶水，把一行的三段字存进行表，`ink_ui_list` 画格子时取。`lv_obj_clean`、`scr_empty_note_create` 是空操作，空状态由 `ink_ui_list` 负责。`ui_reaction_parse` 永远返回 false。`contacts_display_name` 用宏改名接到胶水的 `sms_display_name`（见下） |
| `:3875-3912` `scr13_1_delete_target`、`ui_sms_delete_at`、`scr13_1_do_delete_thread`、`scr13_1_do_delete_message` | 同上 | 删一条、删整个会话；删完会话里没东西了就回列表，否则重画 | 由在 UI 上跑的能力 `sms.delete`、`sms.delete_thread` 调用。`sms_delete(idx)` 接胶水：把这条记录原文放进撤销区，`ink_db_del` 不等待，再从索引里摘掉。`sms_thread_delete` 同样接胶水。`scr_mgr_pop` 接 `ink_ui_pop`；`scr13_1_populate` 接胶水，只重排会话那一块；`sms_thread_msg_count` 读索引 |
| `:4029-4128` `scr13_1_bubble_create` | 同上 | 会话里画一条：收到的靠左，发出的靠右；点它选中 | 原文的点按回调 `scr13_1_bubble_event` 由胶水提供，调 `sms.select`，不弹 `lv_msgbox` |
| `:4284-4316` `scr13_2_send_event` | 同上 | “发送”键：没写收件人、没写内容、上一条还在发、发不出去，各说一句话；交出去以后清空输入框 | `lv_textarea_get_text`、`lv_textarea_set_text` 接胶水：读共用输入框，清空靠重建。`scr13_2_status` 接编辑区上方的提示行。`scr_mgr_pop(false)` 接胶水 `lily_sent()`：写短信页转到会话页，会话页留在原地 |

**替身头文件** `vendor/lilyphone/sms_lily.h`（算胶水）：

- **原文的类型和常量**：`sms_msg_t`，其中 `SMS_TEXT_LEN` 改成 `INK_SMS_TEXT_MAX`；`SMS_DIR_IN/OUT`、`SMS_ST_OK/PENDING/FAILED` 照原文。另加两个我们的状态 `SMS_ST_UNKNOWN`、`SMS_ST_DRAFT`。
- **名字和长度**：`CONTACT_NAME_LEN` 97、`CONTACT_NUMBER_LEN` 25。`ui_active_number` 是胶水里的 PSRAM 缓冲，也就是当前收件人。
- **号码比对**：声明 `phone_number_match`。只是声明，定义在 phone-3 的 `phone_store.c`，链接同一份。
- **字体和颜色**：`FONT_BOLD_SIZE_15` 换成 `ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR)`（正文用常规体；原文是 1 位屏，全用粗体）。`DECKPRO_COLOR_*` 换成 `ink_ui_ink()`、`ink_ui_paper()`。
- **查名字**：`#define contacts_display_name sms_display_name`。胶水先用 `ink_contacts_match(number, true, …)` 查；查不到、号码又以 `+86` 或 `0086` 开头，就去掉前缀再查一次。每次刷新时每个会话只查一次。这样不碰联系人的记录格式（contacts-3 正在改它），也不动电话那张全局表。
- **时间**：`time`、`localtime_r` 的替身。

**原文里必须改的行**（`SOURCE.md` 逐行写原文、改成什么、为什么；改动合计不超过 80 行，删掉的段也算在内，超了先停下说明）：

- `scr13_populate`：
  - `LV_SYMBOL_ENVELOPE` 改成“●”，我们的字体里没有 LVGL 的符号字形；
  - 英文前缀换成 `TR("我：")`、`TR("发送中：")`、`TR("未发出：")`；
  - 加两种前缀“结果未知：”“草稿：”。草稿照 Fossify，用草稿代替摘要；
  - `snippet[48]` 改成 `[96]`。48 字节只放得下 16 个汉字，一行放不满。
- `scr13_1_bubble_create`：
  - 宽 `180` 改成令牌（内容宽的五分之四），x 坐标 `42` 按宽来算；
  - 边框 `1` 改成 `0`、圆角 `8` 改成 `0`：新设计语言不画框；
  - `pad 5` 和行距 `+ 6` 改成令牌；
  - 原文把“时间加状态”放在正文上面一行，改成状态写在正文下面（照 Lineage）。时间行由胶水按 Fossify 的 5 分钟规则另画，所以 `ui_format_stamp` 那一行去掉；
  - **删掉 `:4100-4125` 反应徽章那一段**：那是小圆角框，我们不做反应消息。
- `scr13_2_send_event`：四句英文换成 `TR`；`scr_mgr_pop(false)` 换成 `lily_sent()`。
- `ui_format_stamp*`：两个格式串改成中文。

**不搬**（`SOURCE.md` 写上证据）：

- `scr13_1_populate`（`:4130-4197`）：它用一个可滚动容器放最近 25 条，打开时滚到底（`:4193` `lv_obj_scroll_to_y`），还要把反应消息贴到对应的消息上。墨水屏不滚动（第 11 节），会话页按页排，排页是我们的胶水（沿用现有 `thread_layout` 的算法）。
- 反应消息：`ui_reaction_*`（`:2928-3116`）、`scr13_1_send_reaction`（`:3919-3947`）、反应选择框（`:3949-4018`）。Fossify 和 Lineage 都没有；它认的是英文手机发的“Liked “…””这类句子，还要表情字体。
- `scr13_1_bubble_event`（`:3977-4018`）：弹模态框，再整屏刷新（`:4017`）。改成就地出一排动作按钮，只刷那一块。
- `scr13_1_delete_event`（`:4020-4027`）：删会话前一律确认。D-105 规定能撤销就不问，只有撤销区装不下时才确认。
- `ui_set_active_number`（`:2845-2850`）：它记的是联系人在数组里的位置，我们这里没有这个东西。收件人匹配照现有的建议行。
- `create13`、`create13_1`、`create13_2` 的页面布局：用的是写死的像素、`scr_*` 小控件和自己画的返回键。我们用外壳的顶栏和主题令牌。
- `phone_store.cpp` 的短信日志 `sms_*`（`:286-505`）：那是 SPIFFS 里的一张表。我们的存储是服务的 `sms.messages`。

## 要做的

### 1. 会话列表（照 Fossify `BaseConversationsAdapter`，行的内容用搬来的 `scr13_populate`）

- **每页几行**：每行三段字，行高按当前字号算，放得下几行就几行。默认字号 4 行，最大字号少几行，不缩字。
- **每行写什么**：
  - 第一行：未读的写“● ”加粗体，多于一条时名字后面写“(2)”；然后是名字，没存的写号码；右边是时间（今天写时分，别的天写日期）。
  - 第二行：最后一条的摘要，前缀照下面的状态表。有草稿时写“草稿：……”。
- **会话怎么分**：用 `phone_number_match` 比号码，`+86` 和不带 `+86` 的是同一个会话。排序按最后一条的时间；只有草稿的会话按草稿的时间。
- **顶栏**：两个写着字的按钮“搜索”“新建”，代替 Fossify 的浮动按钮和菜单。翻页用共用翻页条，盖板的上一页、下一页键也能翻。
- **点一行**：进会话页，未读的通知标为已读（照现有）。
- **会话索引**放在 PSRAM 里：每条记录的编号、时间、会话号、状态类，每个会话的号码、最后一条、未读、草稿编号。每次刷新读一遍集合就建好，不再每画一行都走一遍游标和通知收件箱。

### 2. 写新短信（`SmsCompose`）

- 页面和现在一样，收件人和正文在同一页：Fossify 是先选收件人、再进会话页写，按设备改成一页，少换一次页就少一次整屏刷新。
- 收件人照旧：输号码或名字，下面出一行最匹配的联系人，旁边有“从联系人选择”。
- 交出去以后转到会话页（现有）。
- 离开时如果写了字，按输入的号码存成那个会话的草稿（照 LilyPhone `exit13_2`）。
- 没写收件人就按“发送”，说“请先写收件人”（照搬来的 `scr13_2_send_event`）。
- `sms.compose {number}` 照旧打开这一页并填好号码。电话、联系人、通知都在调它，行为不变。

### 3. 会话页（照 Fossify `ThreadActivity`，每条用搬来的 `scr13_1_bubble_create` 画）

- **打开时先看、不弹键盘**：编辑区的输入框不带焦点，有草稿的话草稿在框里。点输入框才开始打字（memo-3 学到的：编辑器弹着键盘不算阅读视图）。
- **收发怎么区分**：收到的靠左，铺浅灰底，不画框；发出的靠右，右对齐，没有底色。不画黑底卡片，不画粗框，不靠颜色。
- **时间行**：隔 5 分钟以上才写（Fossify 的规则），用搬来的 `ui_format_stamp`。
- **翻页**：按页翻，打开时在最新的一页。写页码，盖板键能翻。不滚动，没有“回到底部”的浮动按钮。长消息照现在的办法跨页切开。
- **顶栏**：标题写名字，没存的写号码；右边一个写着字的“详情”，打开会话详情页（第 7 项）。
- **底部编辑区**：“展开”（长文页 `ImeLong`，照旧）、共用输入框、“发送”。“发送”是这一屏唯一的主按钮。
- **被动更新**（新短信到了、回执回来了）：
  - 只重排会话区，走 `ink_ui_after_press`，手指按着时等抬起来；
  - 只在页面显示的内容真的变了时才 `ink_ui_content_changed`；
  - 从别的页回来时（`ink_ui_shown_again`），版本变了就重建一次。通知里的快捷回复就是这样补上的。

### 4. 编辑区：草稿和分段提示

- **草稿**（照 Fossify `:315-352`）：
  - **存在哪**：存在 `sms.messages` 里，状态写 `"草稿"`，每个会话（按号码比对）最多一条。Fossify 和 Lineage 也都是这样，把草稿当成一种消息状态存在短信表里（Lineage `MessageData.java:128` 的 `BUGLE_STATUS_OUTGOING_DRAFT`）。这样每位家人自然分开，也不用新登记集合（只剩 1 个空位）。
  - **什么时候写**：离开会话页或写短信页时，在后台写：有字就写，没字或发出去了就删。打字时不写存储。
  - **不受影响的地方**：服务的 `message.list` 只数“收到”的，不受影响。AI 读 `sms.messages` 会看到草稿，D-23 定的就是全部可见。
  - **存不下时**（满了 512 条）：草稿留在内存里，提示行写“草稿没存上：短信已满，删掉一些就能存”。
- **分段提示**：输入框里有字就显示一行：
  - “第 1 条，还能打 23 字”；超过一条写“共 3 条，第 3 条还能打 40 字”；超过 5 条照旧写“太长了，最多分 5 条发送”。
  - 剩几个字怎么算：用公开的 `ink_comm_sms_parts` 二分查找，“正文后面补几个 `a` 条数还不变”。在 GSM 7 位和 UCS2 两种编码下，一个 `a` 都只占 1 个单位，所以算出来就是当前编码下还能打的字数，服务不用改。
  - 和 Fossify、Lineage 不同：它们默认不显示，或者快满了才显示。按设备改成一直显示，因为每条都要计费；这一行和输入框在同一帧里刷。
- **发送**（用搬来的 `ui_sms_dispatch`、`scr13_2_send_event`）：
  - 点“发送”或键盘回车：输入框马上清空（照 Fossify `clearCurrentMessage`），会话最下面立刻出现这一条，下面写“发送中”（Lineage 的 Sending…）。
  - 回执回来后，这一条的状态只局部刷一次。
  - 长正文照现有办法走文本草稿引用（`send_clicked`，`:180-193`）。

### 5. 状态和失败（Lineage 的分法，用文字，不靠颜色）

| 记录里的状态（服务原话） | 会话里这条下面写 | 能做的 | 列表最后一条的前缀 |
| --- | --- | --- | --- |
| 收到 | 不写 | 复制、删除 | 不加 |
| 草稿 | 不进消息区，放回输入框 | — | 草稿： |
| 正在等回执（页面自己知道） | 发送中 | — | 发送中： |
| 已发出 | 只在最后一条自己发的下面写“已发出”（照 Fossify `ThreadSent`） | 复制、删除 | 我： |
| 含“未发出”，不含“结果未知”（包括“共 3 条：前 2 条已发出，后 1 条未发出”） | 服务原话 | 重新编辑、重发、复制、删除 | 未发出： |
| 含“结果未知” | 服务原话，下面加一行“可能已经发到，先问问对方” | 再发一次（可能重复）、复制、删除 | 结果未知： |
| 已送达 | 不会出现：服务没有送达报告 | — | — |

- **重新编辑**（照 Fossify `:690-698`，沿用能力名 `sms.retry`，它的标题本来就是“重新编辑明确未发出的短信”）：
  - 原文填进本会话的输入框，记下要重发的原消息。不再跳到写短信页。
  - 一改字，这个记号就清掉（Fossify `:890`），发出去算新的一条，原来那条照留。
- **重发**（照 Lineage “Not sent. Tap to try again.”，新能力 `sms.resend`）：把原文原样再发一次。
- **重发以后只剩一条**：
  - Fossify 是更新原来那条记录（`MessagingUtils.kt:62-74`）。我们的服务每次发都新存一条（`comm_service.c:141`、`:542`），**不改服务**。
  - 办法：回执的 `object` 以 `message:to:` 开头，说明新记录已经存在，胶水再删掉原来那条。
  - 只对“发送失败，未发出”（一条也没发出去）这样做。长短信发出了前几条的，原来那条保留，因为对方已经收到了一部分。
  - 服务原地更新的做法写进报告“要服务配合的地方”。
- **结果未知**：
  - **绝不自动重发**，也没有 Lineage 那种“等待重试”。
  - “再发一次（可能重复）”是新能力 `sms.send_again`，只给本人点按和按键（`INK_EXPOSE_OWNER`，处理函数里还要再拒一次 AI 和远程）。按钮上的字已经写清会重复，不另弹确认卡（人自己按的“发送”就直接发，D-105）。
  - 原来那条保留。
- **AI 能做什么**：AI 能调 `sms.retry` 和 `sms.resend`。真正发出去走的是 `message.send`，来源照传，AI 来源照现有策略等人确认。`sms.send_again` AI 调不了。

### 6. 选中一条和删除（Fossify 长按出动作，按设备改成点按）

- **选中**：点一条（`sms.select {id}`）就选中它，下面出一排写着字的按钮：“复制”“删除”。没发出和结果未知的那一排按钮一直显示，不用先选中。再点一次，或点别处，就取消选中。每次只刷会话区这一块。
- **复制**：`sms.copy {id}`，正文放进剪贴板（`ink_clip_set`）。
- **删一条**：`sms.delete {id}`，可以撤销，不问（D-105）。提示行写“已删除 · 撤销”，留到下一次操作。撤销是 `sms.restore`，走 `system.undo`。删完会话里没东西了就回列表（搬来的 `scr13_1_do_delete_message`）。
- **撤销区**放在 PSRAM，64 KB，存删掉的记录原文。一条最多约 3 KB，总装得下。
- **孩子用完时间以后**：策略会挡掉 `system.undo`（`members.c:920-922`）。这时页面不显示“撤销”；删除找不回来了，所以改成先确认（`ink_confirm_hold`）。要不要把撤销加进放行名单，写进“要用户定”。

### 7. 会话详情页（代替 Fossify 的三点菜单 `menu_thread.xml`，新页面 `SmsInfo`）

- 上面写名字和号码。
- 下面一列写着字的按钮：
  - **打电话**（`call.dial`）；
  - **查看联系人**（`contacts.view`），没存的号码写“加为联系人”（`contacts.edit`；号码带不过去，和 phone-3 一样，写进报告交给 contacts-3）；
  - **复制号码**；
  - **删除整个会话**（`sms.delete_thread {peer}`）：整个会话装得进撤销区就直接删，可以撤销；装不下就走确认卡，写“删除和 某某 的全部 N 条短信？删除后找不回来”（照 Fossify `:1176-1191` 和 LilyPhone，只在找不回来时问）。删完回列表（搬来的 `scr13_1_do_delete_thread`）。
- 归档、拉黑、标为未读：不做，见“要用户定”。

### 8. 搜索（照 Fossify `MainActivity.kt:521-612`，新页面 `SmsSearch`）

- 上面是共用搜索框，按“搜索”键（或键盘回车）才搜。Fossify 是边打边搜；墨水屏上每打一个字结果列表就要刷一次。
- 打 1 个字就能搜。Fossify 要 2 个字，但中文一个字就有意义。
- **搜什么**：当前家人的全部短信正文，加上对方的名字和号码。结果最多 50 条，放在 PSRAM。
- **怎么列**：先列名字或号码命中的会话，再按新到旧列命中的消息。每行写对方、时间和一行摘要（用搬来的 `ui_message_snippet`，从命中处前面一点开始截，前面加“…”，不用颜色高亮）。按页翻。没结果时写“没有找到”。
- **点一条**：进会话页，翻到那一条所在的页，并且选中它。`sms.thread` 加一个可选参数 `id`，不传时行为不变，没有动画。

### 9. 能力和快照

- **现有能力名一个不改**。`sms.retry` 的行为从“跳到写短信页”改成“填进本会话的输入框”，在报告里写明。`sms.thread` 加可选参数 `id`。
- **新增的能力**（名字可以调）：

| 能力 | 做什么 | 给 AI |
| --- | --- | --- |
| `sms.select {id}` | 选中一条 | 给 |
| `sms.copy {id}` | 复制一条 | 给 |
| `sms.resend {id}` | 重发明确没发出的那条 | 给（发送照 `message.send` 的确认规矩） |
| `sms.send_again {id}` | 结果未知的那条再发一次 | **不给**（只给本人点按和按键） |
| `sms.delete {id}` | 删一条（可撤销） | 给 |
| `sms.restore {token}` | 撤销删除 | 不给（走 `system.undo`） |
| `sms.delete_thread {peer}` | 删整个会话 | 给（装不下撤销区时要人确认） |
| `sms.info {peer}` | 打开会话详情 | 给 |
| `sms.search {query}` | 搜短信 | 给 |

- 新能力都 `child_ok`，进 `tests/integration/ai_surface.c` 两张表（只在末尾追加）。
- **快照**写清页面上有什么、能做什么：
  - **列表**：每行的对方、摘要、前缀（状态）、时间、未读、草稿、页码。
  - **会话页**：可见的每条消息（方向、时间、正文、状态原话、能做的动作），选中的是哪条，输入框的字和分段提示，“发送”能不能按，翻页。
  - **搜索页**：搜的词、结果、页码。
  - **详情页**：名字、号码、动作。

### 10. 时间不够可以不做（报告里逐项列成“没做到的”，不许写成“不需要”）

“转发”：Fossify `ThreadAdapter.kt:329`。如果做，`sms.compose` 加可选参数 `text`。

## 按设备改（汇总，逐行见对照表）

- **墨水屏**：
  - 不滚动，会话和列表都按页翻，写页码，盖板键能翻；
  - 没有转圈、气泡入场动画、自动滚到底；
  - 打字只刷输入框和分段提示；
  - 发出一条：点“发送”刷一次（输入框清空和“发送中”同一帧），回执回来局刷一次；
  - 新短信只刷会话区或变了的那几行；
  - 选中、删除、撤销各刷一次。
- **灰阶**：收发靠左右位置和浅灰底分开，状态全用字；未读用“●”加粗体，不靠颜色。不画黑底卡片、粗框和圆角框。
- **触摸**：所有按钮不小于 108 px。长按换成点按选中，再给一排写着字的按钮；三点菜单换成“详情”页；浮动按钮换成顶栏的“新建”“搜索”。
- **提示**：没有几秒自动消失的提示条。“已删除 · 撤销”和发送结果都留到下一次操作。
- **资源**：会话索引、行表、撤销区（64 KB）、搜索结果都放在 PSRAM（`INK_PSRAM_BSS`），不后台轮询。

## 共同要求

- **来源**：功能、页面、状态、出错处理照对照表一个不落，不自己发挥。不照搬的写理由，比如彩信和附件：模组驱动没有彩信，服务也没有。`docs/tasks/sms-3/SOURCE.md` 写每一页对应来源的哪个文件和行号。
- **设计语言**：
  - 只用令牌和共用控件：`system/ui/ink/ui_theme.h` 里的 `ink_ui_pill`、`ink_ui_text_button`、`ink_ui_list_row`、`ink_ui_empty`、`ink_ui_pagebar`、`ink_ui_group_divider`，`ui_list.c`，`textfield.c`；
  - 一屏最多一个主按钮：会话页、写短信页是“发送”；
  - 行间细分隔线，组间留白；整页空才用共用空状态；
  - 不写死像素和颜色，显式跑 `python3 tools/ui_scale_check.py system/apps/sms`。
- **不变的规矩**：
  - 短信按家人分开，草稿和撤销区也按家人；换家人时清掉内存里的一切；
  - 孩子照 `child_ok`；
  - 确认卡只给 D-105 的高风险动作。人自己按“发送”“重发”“再发一次”就直接发，AI 代发照现有策略；
  - 按下反馈照 D-31；短信正文不进日志。
- **tap-loss-1**：人没做的变化，在手指按着时都要等。整页重建用 `ink_ui_redraw`；局部重排用 `ink_ui_after_press`；只在显示内容变了时才 `ink_ui_content_changed`。
- **D-138**：短信不需要固件以外的文件。字体缺了由系统统一说明，页面不能是空白。冷启动时数据还没读进来，照现在的做法和第一次刷屏合在一起；读失败就写“短信没读出来”并给“重试”，不留空白页。
- **文字大小**：跟系统文字大小一起缩放（`ink_ui_px`）。默认、最大一档、孩子身份各出一套截图。放不下就分页或换行，不缩字，不裁按钮。
- **原地重写**：分支里替换旧页面，旧代码删掉，不留两套（D-135）。
- **共享文件**：
  - `docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成；
  - `tests/integration/ai_surface.c`、`assets/i18n/en.tsv` 只在末尾追加本任务的行；
  - 根 `CMakeLists.txt` 加新文件：`ink_system` 源（`:1848` 附近）和翻译源列表（`:477` 附近，phone-3 的 `call_card.c` 在 `:475` 是先例）；
  - `ports/esp32s31/components/ink_system/CMakeLists.txt:40` 的翻译源也加上；
  - 合并冲突由主会话处理。
- **截图**放 `docs/tasks/sms-3/`，单张不超过 300 KB。先看干净的整页图，不要有调试框。

## 范围和预算

- **只改**：
  - `system/apps/sms/`：可以拆文件；新的 `vendor/lilyphone/` 和它的 `SOURCE.md`；
  - 短信相关测试，以及验收 6 列出的、引用 `sms.c` 的测试；
  - 上面说的 CMake 那几行、`ai_surface.c`、`en.tsv`、`capabilities.md`；
  - `docs/tasks/sms-3/`。
- **不碰**：
  - `system/services/comm/comm_service.c`：只归 phone-3。需要服务改的写进报告。往服务的 `sms.messages` 里写草稿、删记录不算改服务，但要在报告里写清楚；
  - `system/apps/phone/`：只链接 `phone_number_match`；
  - `system/apps/contacts/`（contacts-3）；
  - `system/services/notify/`：`sms.send` 的参数和 `sms.thread {peer}` 的意思都不变；
  - `system/services/text/`、`system/ui/shell/keyboard.c`（keyboard-2）；
  - 主题和共用控件文件；
  - `system/policy/`：撤销要不要放行写进报告；
  - `system/ui/search.c`：全局搜索里搜短信不在本任务。
- **代码量**：
  - 产品代码（胶水加重写，含替身头文件）不超过 **600 行**，删掉的旧代码不抵。比交付计划的 500 多，原因有三条：
    - 本任务比旧页面多了搜索页、选中动作行、三类状态各自的重发规矩、按会话存草稿、撤销区和装不下时的确认、带号码比对的会话索引；
    - 旧 `sms.c` 不含这些就已经 466 行；
    - 同类任务 phone-3 用了约 880 行（预算 500），memo-3 用了 794 行（预算 600）。
  - LilyPhone 原文整函数照拷不算在内。对原文的改动不超过 **80 行**。
  - 超了先停下说明，不许悄悄砍功能。
- **固件和内存**：固件增量不超过 **15 KB**，超了写进报告；内部静态 RAM 不增加，新表都放 PSRAM。
- **起步**：主线 `3baaf0e2`。只做到模拟器的 AT 模型，不发真短信（要花钱），不碰设备。

## 验收

1. **照参照路径走一遍**（模拟器脚本，每步截图并断言）：
   - 写新短信给一个没存的号码，打 150 个汉字。分段提示从“第 1 条，还能打 N 字”变成“共 3 条，第 3 条还能打 M 字”，数字和驱动的 70/67 规则对得上。
   - 离开再回来，草稿还在；列表上写“草稿：……”。另开一个会话再回来，草稿也还在。
   - 让模拟器的模组明确发送失败（先注销网络）：这条下面写“发送失败，未发出”，有“重新编辑”“重发”。
   - 点“重发”：模组正好收到一次发送。成功后会话里只剩一条这段话，状态“已发出”，原来那条没了。
   - 另走“重新编辑”：原文进入输入框，不换页。改一个字再发，原来那条失败的还在。
   - **结果未知**：让模组回“结果未知”（卡在但没有短信业务）。页面写“结果未知，请先核实”，模组只收到一次发送，没有自动重发（断言次数）；人点“再发一次（可能重复）”才再发，原来那条保留。
   - 长短信只发出前两条（`sim_net_sms_fail_part`）：页面写服务原话，原来那条保留。
   - 同一条路径用能力调用走一遍：AI 调 `sms.send_again` 被拒；AI 调 `sms.resend` 要等人确认。
   - **反向对照**：
     - 把结果未知当失败自动重发，发送次数的断言必须失败；
     - 把会话分组改回逐字比较，“+86 和不带 +86 是同一个会话”必须失败；
     - 把“只删一条也没发出的原消息”改成长短信部分发出的也删，“部分发出的保留”必须失败。
2. **号码和名字**：来信 `+8613812345678`、回信 `13812345678` 在同一个会话里。联系人存成 `13812345678` 时，两种写法都显示名字。10086 和 1008611 是两个会话。
3. **搜索**：搜一个词，结果列出含它的消息和名字命中的会话。点一条，进会话、翻到那一页、选中那一条。没结果时写“没有找到”。
4. **删除**：
   - 删一条，再撤销，记录回来；
   - 删整个会话，再撤销，全部回来；
   - 删一个大于撤销区的会话，走确认卡；拒绝了什么都不删；
   - 删掉最后一条后回到列表；
   - 孩子用完时间后删一条：先确认，页面没有“撤销”。
5. **家人隔离**：A 的会话、草稿、搜索结果、撤销，B 都看不到，也撤不回 A 的。
6. **截图**，改前改后并排：
   - 会话列表（有草稿、有未读、最后一条没发出）；
   - 会话页（收到、已发出、发送中、没发出、结果未知、长短信部分发出、选中一条）；
   - 分段提示；搜索结果；会话详情；空列表；读不出来；
   - 默认、最大字号、孩子各一套。
   - 测试断言：所有按钮不小于 108 px、都在屏幕内；页面上没有 `ink_ui_circle`；会话页没有黑底卡片。
7. **刷新次数**：用现有的刷新次数探针（`tests/sms/sms.c` 的 `start_measure`），断言这些动作各刷几次：打一个字、点发送、回执回来、新短信到达、选中、删除和撤销、翻页、搜索。不放宽次数。报告写每个主要操作刷几次屏。
8. **全量**：
   - 主工程普通全量 ctest 过（含 `sms`、`sms-multipart`、`comm-ux2`、`ui-scene-*`、`design-lang-*`、`ui-scale`、`i18n-*`、`integration-*`、`phone-3-*`）；
   - 显式跑 `python3 tools/ui_scale_check.py system/apps/sms` 过；
   - ASan 跑短信相关测试和 `ui-scene-*` 过；
   - S31 宿主机测试过；
   - `ports/esp32s31/build.sh` 编译通过，无新警告。

   **报告写**：固件增量；产品代码行数和对原文的改动行数；`gcccheck` 结果；对照表做到几行、哪几行没做；新增和改了行为的能力（应该没有改名或删除的）；需要短信服务配合的地方（原地重发、512 条上限）；以及**改过的每一个已有测试**。预计会改的：
   - `tests/sms/sms.c`、`run.py`、`reverse.py`：失败那条现在留在会话页，不再跳 `SmsCompose`；状态写在正文下面的位置断言改指新控件；按原文替换的反向对照改指新位置。失败条件不变。
   - `tests/comm_ux2/comm_ux2.c:126`：“失败的消息打开重发”断言的页面从 `SmsCompose` 改成 `SmsThread`，并且输入框里是原文，一样严格。
   - `tests/design_lang_2_reverse.py:30`、`tests/open_app_latency/run.py:16-60`：按原文插桩和替换的，改指新位置。
   - `tests/i18n_apps.c`、`tests/kid_2.c`、`tests/notify_model.c`、`tests/airplane_mode.c`、`tests/design_lang_1.c`：页面名不变，应当照过；结构变了只改定位方式。
   - `tests/integration/ai_surface.c`：只在末尾追加。

   只改定位方式和页面结构，不改业务断言；测试不放宽、不删、不加等待来凑。

## 要用户定（主会话派出前问清，或者写进报告）

1. **“已送达”**：要发短信时请求送达报告（`nt26.c` 里 `submit_pdu` 的标志位），模组要上报送达（`+CDS`，还要改 `AT+CNMI` 的参数），服务还要记下来。这牵涉 CLAUDE.md 的“不改模组配置”，还要改驱动和服务，不在本任务。本任务只写“已发出”，从不写“已送达”。
2. **Fossify 有、本任务不做的**：
   - 定时发送：要服务在后台按时发；应用退到后台会被冻结，做不到；
   - 归档；回收站（用撤销代替）；置顶；标为未读；
   - 拉黑：要服务收到短信时直接丢掉；
   - 彩信和附件：模组驱动和服务都没有彩信；
   - 群发：`message.send` 只收一个 `to`；
   - 选卡：D-128；
   - 分享、选一段字、另存：没有分享面板；选字要长按；
   - 转发：做了就从这里划掉；
   - 全局搜索里搜短信：`system/ui/search.c:84` 只搜联系人和备忘；
   - 加为联系人时带上号码：`contacts.edit` 只收 `id`，交给 contacts-3。
3. **孩子用完时间后不能撤销**：`system.undo` 不在 `members.c:920-922` 的放行名单里，所以这时删短信改成先确认。建议在策略里放行撤销（不归本任务）。
4. **短信存满 512 条后新短信存不下**：服务的集合没开 `drop_oldest`（`comm_service.c:63-64`、`datastore.c:810-813`），草稿也占名额。建议 phone-3 一侧开 `drop_oldest`，或者把上限调大。本任务的删除是现在唯一腾地方的办法。
5. **重发原地更新**：建议 `message.send` 收一个 `replace` 编号，就能照 Fossify 原地更新，不用先存一条再删一条。不急。
6. **行为改动，请用户知道**：
   - “重新编辑”留在会话里，不再跳到写短信页；
   - 状态写在正文下面；
   - 列表里别的天的时间写成“04月09日”，这是搬来的原文格式；
   - 新短信页仍是收件人和正文同一页（Fossify 是两页）。
7. **LilyPhone 没有许可证文件**：按 D-129 照搬，仓库是公开的，在这里记一笔。

## 和本批其他任务的接缝

- **contacts-3**：只经 `ink_contacts_match` 和 `contacts.*` 能力接。contacts-3 承诺签名不变，可能会规整 `+86`；`sms_display_name` 去前缀再查一次的做法和它不冲突。
- **keyboard-2**：只经共用输入框和文本服务接，不碰键盘文件。
- **clock-3、calendar-3、small-apps-3、control-1-check、home-1-check、settings-1-check**：只在 `ai_surface.c`、`en.tsv`、`capabilities.md`、根 CMake 上有末尾追加的重叠。本任务不新登记数据集合，最后 1 个空位留给别人。
