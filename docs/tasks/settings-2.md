# settings-2：设置各页照 MuditaOS 重写（手机模式和勿扰、SIM 卡和接入点、前光、铃声和振动、自动锁屏），Wi-Fi 照 Brookesia，家人和孩子页、手画页都走 settings-1 的注册表

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。报告写到 `docs/tasks/settings-2/REPORT.md`，来源对照写到 `docs/tasks/settings-2/SOURCE.md`。

**D-140（总规则第 11 节）**：本任务是“照着重写”。第一步写对照表 `docs/tasks/settings-2/对照表.md`（参照产品每一页、每个操作和状态，外壳看 `docs/tasks/shell-research-1.md` 和交付计划这一行的来源出发）并单独提交；计划和 `replan-sysapps/SOURCES.md` 点名的 C/C++ 代码能直接用的照搬（这个任务：Brookesia 的 Wi-Fi 列表）。参照安卓的地方按第 11 节为这台设备改，做完逐行验收。

## 为什么

用户 2026-11-12（D-129）：“移植完整应用嫁接和封装…到我们的系统中 这里不止阅读器 还包括各类系统应用和部分基础框架（例如输入法 通知栏 设置等等）”；D-131：先把外壳和现有应用“搞到成熟可用”，设置是外壳 9 个部件之一。D-137（用户 2026-11-13 “都按推荐做”）：设置首页在默认字号下压到两页，把存储、关于、备份这类不常用的收进一行“系统”；控制中心给孩子一个设置入口，只出现孩子本来就能改的项。D-138：缺什么界面要说清楚原因，不能白屏。

settings-1 已经合入（`4a33d06e`）：首页分组、每行当前值、6 种可用状态、搜索、`settings.search` 和 `settings.open`，全部来自 `system/ui/shell/settings_registry.c` 的 `ITEMS[]`（61 项）。它留下的、归本任务的事（`docs/tasks/settings-1/REPORT.md` “没做到的”第 2、3 项和“要用户定的”两项）：

- 手画的三页（显示和亮度、声音、网络）还自己写行名，和注册表是两份文字；注册表只给它们提供搜索、定位和变灰。
- 首页 7 组 14 行，默认字号 3 页、最大字号 4 页（D-137 要两页）。
- 孩子没有用手进设置的入口（`control_center.c:246-247` 只给大人画齿轮）。

调研结论（`docs/tasks/frameworks-research-1/SETTINGS.md` “手机功能逐项覆盖”和“真正建议搬的内容”）：手机页面按 MuditaOS Pure 逐页翻译（模式和例外、声音和铃声、前光、网络/SIM/APN），“这一页管一个对象、当前值明确”；Wi-Fi 列表和连接页借 ESP-Brookesia；家人、孩子、PIN、AI 快照是我们自己的合同，不借外部应用。

## 开工前先核对（主会话 2026-11-13 补）

1. **归属**：设置里的手机模式页、勿扰页和 `settings.phone_mode` 能力归本任务。`notify-ui-1`（delivery-plan 第 49 行写了“三种手机模式”）只做通知区和状态栏的显示，需要切模式时调用 `settings.phone_mode`，不另做页面、不另存模式。两边任务书由主会话对齐，你发现有冲突写进报告，不自己改对方文件。
2. **settings-d137**：如果它已经派出去并包含第 1 条（首页两页）或第 13 条（孩子进设置），这两条由它做，本任务只把它们当验收项核对；开工时先看主线有没有它的合入记录，没有就按本任务书做。
3. **基线**：先在当前主线上跑一次全量 ctest 和 `ports/esp32s31/build.sh`。settings-1 报告记过基线就失败的项（`cell-status`、`ime-keyboard1`、`gui-smoke`、`power-pages`、`weather`、`games2` 等）和 S31 固件基线编不过（`today_edit.c:258`，归 home-1）。仍失败的写进报告“基线已失败”，不算本任务的失败；`cell-status` 的被测代码这次要改，要单独说清楚前后。
4. **共用单选页**：现在没有“打勾单选行”和单选页公共件（`ui_theme.h` 只有分段、滑杆、开关格、空状态；翻页只在 `ui_list.c` 里）。公共件写在 `settings_pages.c` 里（照 PebbleOS `option_menu.c:26-80`：一列选项、当前打勾、选了走回调），不动 `ui_theme.h`、`ui_list.c`（归 dialogs-1）。翻页用 `ink_ui_list` 现有的翻页条。

## 成熟来源（照着重写，不搬代码）

行号是主会话 2026-11-13 用 `gh api` 在固定提交上在线读的【引用】；你在 SOURCE.md 里重新打开核对，差了就写实际行号。本机没有这两个仓库的克隆，需要时只读浅克隆到 `work/`，用完删掉。

- **MuditaOS**，`github.com/mudita/MuditaOS` 提交 `2cd0e4721405c0bf263f311f27d38aadae6ac89f`，目录 `module-apps/application-settings/`：
  - `windows/phone-modes/PhoneModesWindow.cpp:15-86`：三种模式（正常、勿扰、离线），当前那个加粗，勿扰和离线各自点进子页；`DoNotDisturbWindow.cpp:20-74`：顶上一段说明，下面是“锁屏时显示通知”和“收藏联系人的来电能响”两个开关；`OfflineWindow.cpp:25-88`：离线分“飞行模式”和“只收短信”，说明文字随选择变。
  - `windows/network/SimCardsWindow.cpp:20-189`：SIM1、SIM2 两项，在用的卡分三种写法——没连上、被锁住（下面多一行“解锁”）、正常（带号码，下面有“PIN 设置”和“从卡导入联系人”）；`SimPINSettingsWindow.cpp:20-77`：PIN 锁开关、改 PIN；`ApnSettingsWindow.cpp:20-133`、`NewApnWindow.cpp:20-115`：接入点列表和编辑。
  - `windows/display-keypad/DisplayLightWindow.cpp:20-138`：前光开关，开着才出“自动”，不是自动才出亮度格。
  - `windows/apps/PhoneWindow.cpp:16-79`（来电振动、音量、铃声）、`MessagesWindow.cpp:16-83`（短信振动、提示音）；`SoundSelectWindow.cpp:22-69` 加 `models/apps/SoundsModel.cpp:120-195`：铃声列表，当前那个打勾，选中就试听，换选或离开就停。
  - `windows/security/AutolockWindow.cpp:15-71`：自动锁屏是一页单选（1、2、5、10、20 分钟），当前那项打勾。
- **ESP-Brookesia**，`github.com/espressif/esp-brookesia` 提交 `65d66c810ad706c9e2f82fe9177b11816acee546`，目录 `app/brookesia_app_settings/`：
  - `package/res/screens/wifi.json`：Wi-Fi 开关卡（约 33-100 行）、已连接的那一行（约 128-145 行，可断开）、“已存的网络”带翻页（约 146-380 行）、“附近的网络”带刷新（约 396 行起）；`templates/wifi_network_item.json`：每行名字加一行详情（加密、信号），点了连接，已存的能忘掉。
  - `package/res/screens/wifi_connect.json:33-324`：所选网络、密码行、一句状态、“取消”“连接”两个按钮。
  - `src/service/wifi.ipp`：`refresh_wifi_page_state` 208-286（连接中、已连接、刚断开三种写法；Wi-Fi 关着时整块内容藏起来）、`refresh_wifi_lists` 287-431、`start_wifi_scan` 432-565（进页面就扫一次）、`handle_wifi_select_event` 872-920（已存的或不加密的直接连，正连着的那个点了不重连）、`handle_wifi_forget_event` 921-985、`refresh_wifi_connect_state` 1006-1043（没填密码时“连接”不能点）。
- **PebbleOS**，提交 `aa0c27e8330d010e9202f9a706a56002db5861ee`：`pebble/apps/system/settings/option_menu.c:26-80`（单选页：一列选项、当前打勾、选了回调）。本任务所有“几选一”的页面都用一个这样的单选页。
- 注册表写法照 settings-1 报告末尾“注册表写法（给 settings-2）”。

## 现状（读代码得到的，都是要改的地方）

- **显示和亮度**（`settings_body.c:357-404`）：滑杆标题“亮度”“暖光”、分组标题“字号”是手写字；自动锁屏是一个按钮，每点一下跳到下一档（398 行“自动锁屏 %lld 分钟”），看不到有哪几档。同一个设置在“锁屏和壁纸”页（`settings_family.c:297`）又有一行，点一下在 5 和 10 分钟之间来回跳。设备没有光线传感器（`platform/include/ink/` 里没有），MuditaOS 的“自动”这里做不了。
- **声音**（`settings_body.c:405-469`）：铃声行“%s · 点按换音试听”（464 行），点一下换下一种，没有列表、看不到有几种；“喇叭”那一行点了是 `bt.disconnect`（423 行）。快照（829 行）写原始数字“铃声：0清音、1双音”，不是页面上写的字。没有短信提示音这个设置（`audio_service.c:818-828` 只有来电和闹钟铃声）。clock-3 已经说定闹钟铃声在设置里统一选（`docs/tasks/clock-3.md` “共同要求”的“来源”一条）。
- **勿扰和手机模式**：没有页面。勿扰只有一个开关；notify-svc-1 已经有时段和例外的能力 `notify.dnd_get`、`notify.dnd_set`（`system/services/notify/notify_model.c:176-215`：5 个时段、来电例外 0 都不响 / 1 所有人 / 2 联系人 / 3 收藏、15 分钟内重复来电、日历忙时），可是界面上没有入口，只有测试在调。`notify.dnd_set` 只能写入时段，不能关掉一个（187-188 行总是 `used=1, enabled=1`）。飞行模式（`net.airplane`）在网络页。
- **网络和 Wi-Fi**（`settings_body.c:489-620`）：一页里堆了飞行模式、Wi-Fi 开关、“找附近的 Wi-Fi”按钮（要手点才扫）、每次只显示 2 个网络加“上一组/下一组”（551、567 行）、密码框插在页中间、上网方式、忘掉、已存的网络入口、“设置设备”、SIM 卡分段、移动网络入口。已存的网络是另一页（`KnownNetworks`，470-505 行）。连不上时 `wifi.c` 只把英文原因写进 `rc.detail`（`system/services/net/wifi.c:38-41`、196 行，`fail()` 只写 detail），页面只会说“没能完成，可以再试一次”，分不出密码不对、网络不在附近、等太久。
- **移动网络**（`mobile_network.c`，118 行）：四行文字加四个按钮排成两列；流量写原始字节数“%lld B”（29 行）；没有两张卡各自的状态，SIM 卡选择在网络页。
- **S31 上 SIM 卡那一行永远是灰的**：`ports/esp32s31/hal_cellular_s31.c:52` 先把 `config_readonly` 设成 true，后面再没改回；D-128 放开切卡（`bf0ad40c`）时没动它。于是设备上 `cell.config_readonly` 一直是 1，SIM 卡分段被禁用、搜不到（`settings_registry.c:76` 的 `sim_free`）。真实情况是：S31 能切卡（D-128，换装 12 验证过），不能写接入点（同文件 70 行 `hal_cell_apn` 返回 -1）。现在一个字段管了两件事。`ports/esp32s31/tests/cell.c:125` 断言 S31 上它是 true。
- **SIM PIN**：完全没有。HAL（`platform/include/ink/hal_cellular.h:19-26`）没有“卡锁着”的字段；S31 每次读状态都发了只读的 `AT+CPIN?`（`hal_cellular_s31.c:58-59`），只用来判断有没有卡；模拟器把 `AT+CPIN?` 的任何错误都当成没插卡（`ports/sim/hal/hal_cellular_sim.c:25`，应答在 `ports/sim/hal/hal_net_sim.c:972-975`）。原厂代码也只查 `+CPIN: READY`（`ink-hw/upstream/Metalio-E-INK4-Plus` 的 `main/boards/common/board/uart_eth_modem.cc:1697-1698`），没有解锁流程。AT 允许列表（D-128）只放行切卡的三条命令。
- **家人和孩子**（`settings_family.c`）：行名、行值都是页面自己拼的，不在注册表；“每天能用多久”点一下在 30 分钟、1、2 小时、不限之间跳（58 行）；禁用时段要在输入框里打“21:00-07:00”（256 行，`.max = 11`）；家人页底下又有一个“孩子的设置”按钮（304 行），和列表里的同名入口（242 行）重复；设置一变就整页重画（172 行 `changed`）。
- **日期和时间**（`time_settings.c`）：`time.auto` 等设置一变就整页重画（210 行），这是交付计划第 8 节列给本任务的已知问题。日期时间选择器 `ink_ui_datetime_picker`（`system/ui/theme.c:34-51`）已经和日历共用，但只能五格（年月日时分）一起画、位置写死在页面顶上，画不了“只有时、分”的时段。
- **蓝牙**（`bluetooth_page.c`）：用 `ink_ui_list`，行名也是自己写的。

## 要做的

所有页面都用 settings-1 的注册表：每一行先在 `ITEMS[]` 里声明（编号、名称、别名、分组、所在页、读写的设置或要调的能力、摘要、可用状态、是否只给大人），页面上的行名、当前值、变灰原因、快照、搜索都从这一份来；手画页用 settings-1 的 `at("编号", 高度)` 和 `for_me("编号")` 包住每一块，行名取 `ink_sr_find(id)->title`，不再写第二份。

1. **首页压到两页（D-137）**：存储、备份、关于和更新收进一行“系统”（点进去是一个列表页，三项都在里面，搜索照样直接找到各项）；“勿扰”那一行换成“手机模式 · 正常 / 勿扰（到 7:00）/ 飞行模式”。默认字号首页不超过 2 页；最大字号写实际页数。（settings-d137 已做则只核对。）
2. **手机模式**（新列表页，照 `PhoneModesWindow`）：三行“正常”“勿扰 ›”“飞行模式 ›”，当前那行打勾（不加粗、不画框）。当前模式从真实状态算：飞行模式开着就是飞行模式；`notify.dnd_active` 为真就是勿扰，并写原因和到几点（手动 / 时段 / 日历忙，来自 `notify.dnd_get` 的 `reason`、`until_s`）；否则正常。点“正常”关掉手动勿扰和飞行模式。这一下要和 AI 走同一个入口：在本任务的文件里登记一个能力（例如 `settings.phone_mode`，参数 `mode`），里面只调现有的 `settings.set notify.dnd` 和 `net.set_airplane`，不另存模式。孩子能选正常和勿扰，飞行模式那行写“家长管理”。
3. **勿扰页**（照 `DoNotDisturbWindow`，接 notify-svc-1 的偏好）：顶上一句说明“勿扰时不响不亮屏，闹钟照常响”；“现在勿扰”开关；“谁的电话能响”（单选页：都不响、所有人、联系人、收藏的联系人，写 `calls`）；“15 分钟内打第二次就响”（`repeat`）；“日历上忙的时候自动勿扰”（`calendar`）；“时段”最多 5 个，每个一行“22:00—次日 7:00 · 周一到周五”，点进去用日期时间选择器的时、分两格选开始和结束，加星期几，能关掉这个时段。都走 `notify.dnd_get`、`notify.dnd_set`，按家人分开（偏好已经是按家人存的）。MuditaOS 的“锁屏时显示通知”我们没有这个设置，不新加，写进报告“要用户定”。
4. **声音和铃声**（照 `PhoneWindow`、`SoundSelectWindow`）：声音页的行名从注册表来；“来电铃声”“闹钟铃声”各点进一个单选页（清音、双音、柔音），当前打勾，点一项就选中并试听（现有设置的 `preview_ring` 会响一次，`audio.preview` 计数），返回后那一行写新名字；振动开关、振动强度（单选页：弱、中、强；振动关着时变灰写原因，settings-1 已有依赖关系）。“声音从哪里出来”保留，“喇叭”那行改成只显示当前，不再点了就断开耳机（断开放到蓝牙页）。短信提示音：服务里没有这个设置，本任务不加，写进报告“要用户定”（加的话在声音服务，归 music-3）。快照写页面上的字，不写原始数字。
5. **前光**（照 `DisplayLightWindow`）：显示页上“亮度”“暖光”两条滑杆，标题和当前值从注册表来，亮度 0 写“关”；“自动”这一项在注册表里声明成本机不支持（`INK_SR_UNSUPPORTED`，原因“这台设备没有光线传感器”），不出现在页面和搜索里。字号、刷新屏幕、今日屏放什么照旧，行名改从注册表来。
6. **自动锁屏**（照 `AutolockWindow`）：一个单选页（1、5、10、30、60 分钟，即现在按钮里那几档，`power.lock_after_s` 范围 60-3600 秒），当前打勾；显示页和“锁屏和壁纸”页都只放一行“自动锁屏 · 5 分钟”，点进同一个单选页，删掉两处来回跳的按钮。只给大人。（`power_pages.c` 里省电页读 `power.lock_after_s`，只读不改，不动。）
7. **Wi-Fi**（照 Brookesia，新的 Wi-Fi 页，替掉网络页里的 Wi-Fi 部分和 `KnownNetworks` 页）：
   - 顶上“Wi-Fi”开关行，写当前状态；关着时下面整块不画，只有一句“Wi-Fi 已关闭”；飞行模式开着时写原因。
   - 连着或正在连的那一行放最上面：“家里的网络 · 正在连接 / 已连接 / 没连上：密码不对”；点进去有“忘掉这个网络”。
   - “已存的网络”一组、“附近的网络”一组（带“刷新”）。进页面自动扫一次，不循环重扫；每行名字下面一行写“已存”“加密”，右边信号格和锁（现在的画法保留）；分页用 `ink_ui_list` 现有翻页条，不再“上一组/下一组”两个一组。
   - 点已存的或不加密的网络直接连；点没存过的加密网络进连接页（照 `wifi_connect.json`）：网络名、密码框（共用输入框，`ai_hidden`）、一句状态、主按钮“连接”（没填密码时不能点）；连不上就留在这页写原因，密码框里的字不丢。
   - 失败原因说人话：在 `system/services/net/wifi.c` 的失败分支把 `rc.say` 写成中文“密码不对”“这个网络不在附近”“等了太久没连上”（`TR()`，英文进 en.tsv），`rc.detail` 照旧。不改 Wi-Fi 服务的其他逻辑。
   - 网络页（`SetNetwork`）只剩一组入口：飞行模式（开关）、Wi-Fi ›、移动网络 ›、蓝牙 ›、上网方式、用手机设置设备（原“设置设备”，`shell.launch` 照旧）。
8. **移动网络、SIM 卡和接入点**（照 `SimCardsWindow`、`ApnSettingsWindow`）：
   - 移动网络页从上到下：“SIM 卡”一组两行“外置卡”“内置卡”，各写各的状态：在用的那张写真实状态（4G 在线 / 还没连上 / 没插卡 / 卡锁着，要输 PIN / 暂不能更改），另一张写“没在用 · 点这里换成这张”（模组同一时间只能读一张卡，不编造另一张的状态）；点另一张就切卡（`settings.set cell.sim_slot`，D-128），切换中写“正在换卡”，结果出来只刷这两行。然后是上网方式、数据漫游（开关）、接入点 ›、本月流量和上个月流量（换算成 KB/MB/GB）、清零流量统计（照旧 `ask_by_hand`）。
   - 接入点页：自动 / 手动两项单选；手动时三个输入框（名称、用户名、密码，密码 `ai_hidden`、不进快照不进日志），主按钮“保存接入点”。设备不支持写接入点时（见下一条），整页写“这台设备暂不能改接入点”，不出输入框。
   - **把“能不能切卡”和“能不能写接入点”分开**：`hal_cell_status_t` 加一个 `apn_readonly`，`config_readonly` 只表示切卡不可用；S31 端口里 `config_readonly` 不再写死 true、`apn_readonly` 为 true（`hal_cell_apn` 还是 -1），模拟器两个都按真实能力给；蜂窝服务多发一个状态 `cell.apn_readonly`，注册表里接入点那项用它，`sim_free` 继续用 `cell.config_readonly`。`ports/esp32s31/tests/cell.c:125` 这条断言是本任务有意改的行为，改成新的两个字段，一样严，列进报告。
   - **PIN**：只做读。HAL 状态加一个“卡锁着”（`AT+CPIN?` 回 `SIM PIN` 或 `SIM PUK`，S31 已经在发这条只读命令），模拟器的 `AT+CPIN?` 应答加一个“卡锁着”的夹具（`hal_net_sim.c:972-975`，加一个测试能切换夹具的钩子），蜂窝服务发状态 `cell.sim_locked`，移动网络页那张卡写“卡锁着，要输 PIN”，下面一行说明“这一版还不能在设备上解锁，可以把卡放进手机关掉 PIN 锁”。**不发** `AT+CPIN=`、`AT+CLCK`、`AT+CPWD`，不扩 AT 允许列表（D-128 只放行切卡的三条）；解锁、改 PIN、开关 PIN 锁写进报告“要用户定”（要用户同意扩允许列表，输错三次会锁卡要 PUK）。
9. **蓝牙页**：行名从注册表来，“断开耳机”放在已连接那一行里；其余照旧。
10. **家人和孩子**（`settings_family.c`）：
    - 家人页、孩子的设置页里固定的行（我的密码、离开多久要重新验证、孩子的设置、应用名单、每天能用多久、孩子的音量上限、禁用时段）都在注册表里声明，能搜到、能 `settings.open` 定位；按人、按应用生成的行（每位家人、每个应用的开关）照旧动态生成，但组标题和说明用注册表的名字。删掉家人页底下重复的“孩子的设置”按钮。
    - “每天能用多久”改成单选页（不限、30 分钟、1 小时、2 小时，仍走 `member.time_set`）；“孩子的音量上限”改成单选页（现有的 10 一档）。
    - 禁用时段不再打字：每个时段一行“21:00—次日 7:00 · 每天”，点进去和勿扰时段用同一个时、分选择（第 3 条），加星期几，走现有的 `kid.period_set`。
    - 设置一变只刷变了的那一行，不整页重画。
    - “锁屏和壁纸”页：自动锁屏那行按第 6 条；锁屏样式、壁纸这几行的名字从注册表来，`lock_faces/` 不改。
11. **时、分选择**：在 `system/ui/theme.c` 和 `system/ui/ink/ui_theme.h` 给 `ink_ui_datetime_picker` 加一个只画“时、分”两格、可以放在指定位置的用法（不超过 25 行，日历现在的调用不变），勿扰时段和孩子禁用时段都用它。不另写第二个选择器（交付计划第 8 节：日期时间选择器 settings-2 和 calendar-3 共用一个）。
12. **日期和时间页**：行名从注册表来；`time.auto`、`time.zone_auto`、`time.zone`、`member.idle_return_s` 变了只刷那一行（`time_settings.c:210` 现在整页重画）。
13. **孩子进设置（D-137）**：`control_center.c` 的头行给孩子也画设置入口（现在 246 行 `if (!C.kid)`），图标照控制中心现在的画法；孩子点进来是 settings-1 已有的孩子首页（只有孩子能改的项）。只改这一处，列进报告。（settings-d137 已做则只核对。）
14. **能力和快照**：每个新页面、每个单选页的每一项都是能力，手点和 AI 调同一个（`settings.set`、`notify.dnd_set`、`wifi.connect`、`settings.phone_mode` 等）；快照用 settings-1 的 `row_json` 或 `ink_ui_semantics`，写出这一页上看得见的每一行：名称、当前值（和页面上的字一样）、可用状态、能做的动作。现有能力名（`shell.network`、`shell.known_networks`、`shell.wifi_next`、`shell.wifi_prev`、`shell.wifi_pick`、`shell.mobile`、`shell.mobile_edit`、`shell.mobile_save`、`shell.bluetooth*`、`shell.time*`、`shell.family`、`shell.kid_settings`、`family.*`、`shell.wallpaper`、`shell.notify_settings`）尽量保留；改名或删掉的逐条列进报告，并改 `tests/integration/ai_surface.c`。

如果写到预算还没做完，按这个顺序做，没做的写进报告“没做到的”：1、14、7、2、3、4、8、6、5、10、11、12、9、13。（如果主会话决定拆成 settings-2a / 2b，则 2a 做 1、2、3、4、5、6、11、14，2b 做 7、8、9、10、12、13。）

## 共同要求（页面任务都一样）

- 来源：页面结构、状态、出错处理照上面的来源一个不落，不自己发挥；`SOURCE.md` 写每一页对应来源的哪个文件和行号，哪些没照搬、为什么（例如 MuditaOS 的按键焦点改成触摸、离线里“只收短信”我们没有）。
- 新设计语言：只用令牌和共用控件（`system/ui/ink/ui_theme.h`、`system/ui/ui_list.c`、`system/ui/textfield.c` 里的列表行、开关、滑杆、分段、输入框、翻页、空状态；单选行按“开工前先核对”第 4 条写在 `settings_pages.c`）；能点的不小于 108 px；一屏最多一个主按钮；一种输入框；行间细分隔线、组间留白，分组标题只有灰字不画框；图标不套圆圈；没内容的区块不画，整页空才用共用空状态。不写死像素和颜色（`tools/ui_scale_check.py`）。
- 文字大小：跟系统“文字大小”一起缩放（`ink_ui_px`），默认和最大一档各出一套截图；放不下就分页或换行，不缩字、不裁按钮。
- 少刷新（D-127）：值变了只刷那一行；扫到新网络只刷列表那块；切卡只刷两张卡那两行。不要因为一个状态事件整页重画（codex-rules 第 8 节）。
- D-138：每一页读不到数据时写原因（“网络列表暂时读不出来”“流量暂时读不出来”“还没有孩子家人”），给“重试”这一类动作，不留空白页。
- 不变的规矩：家人隔离；孩子只看到、只能改设置服务本来允许孩子改的项（`child_may_change`），其余写“家长管理”不显示值；确认卡只给 D-105 的高风险动作（清零流量统计、忘掉网络照旧）；按下反馈 D-31；Wi-Fi 密码和接入点密码不给 AI、不进快照、不进日志，AI 不能代输（`wifi.c:154-158` 照旧拒绝）。
- 原地重写，旧的行表、旧的按钮删掉，不留两套。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由）；新文案的英文追加到 `assets/i18n/en.tsv`；这两个文件只在末尾追加本任务的行。
- 截图放 `docs/tasks/settings-2/`，单张不超过 300 KB，先看干净的整页图（没有调试框）。

## 范围和预算

- 只改：`system/ui/shell/settings_body.c`、`settings_pages.c`、`settings_registry.c`、`settings_registry.h`、`settings_family.c`、`time_settings.c`、`bluetooth_page.c`、`mobile_network.c`；设置相关测试和新测试；CMake 里新测试的那几行；`docs/tasks/settings-2/`。
- 只准小改、每处列进报告：
  - `system/ui/shell/control_center.c`：只改第 13 条那一个条件（control-1 已合入）。
  - `system/ui/theme.c`、`system/ui/ink/ui_theme.h`：只加第 11 条的时、分用法，不超过 25 行（这两个文件现在归 dialogs-1，合并排在它后面，冲突由主会话处理）。
  - `system/services/net/wifi.c`：只改第 7 条的失败文案，不超过 6 行。
  - `system/services/notify/notify_model.c`：只给 `notify.dnd_set` 加一个能关掉某个时段的参数（例如 `enabled`），不超过 10 行，带测试（notify/* 归 notify-ui-1，只加接口）。
  - `platform/include/ink/hal_cellular.h`、`ports/esp32s31/hal_cellular_s31.c`、`ports/sim/hal/hal_cellular_sim.c`、`ports/sim/hal/hal_net_sim.c`、`system/services/comm/comm_service.c`、`ports/esp32s31/tests/cell.c`：只做第 8 条的两件事（拆开只读、读出卡锁着）加验收第 3 条要的“Wi-Fi 连接超时”模拟夹具（`hal_net_sim.c`），合计不超过 45 行。`comm_service.c` 归 phone-3：只在状态定义和 `cell_publish`（约 160-180 行、826-833 行）旁边追加，不动通话短信；合并排在 phone-3 之后。
- 不碰：`status_bar.c`、`notification_list.c`（notify-ui-1）；`system/ui/search.c`、`member_*.c`、`pinpad.*`、`kid_pages.c`（shell-sweep-1；“我的密码”“离开多久要重新验证”只是入口行，页面不改）；`system/ui/lock_faces/*`、`wallpaper.c`（lock-styles-1）；`system/services/audio/*`（music-3）；`system/apps/*`；`power_pages.c`（shell-sweep-1）；AT 允许列表和 `s31_modem.c`；`ui_list.c`、`textfield.c`（dialogs-1）；`navigation.c`（确实要在初始化里加一行调用就只加这一行，列进报告）。设置服务（`system/policy/settings.c`）缺小接口时可以增量加，列进报告。
- 不新加设置项、不新加存储格式、不新加线程；手机模式不另存，从现有状态算。
- 产品代码不超过 1800 行（新增行，测试和文档不算；原 700 行按页面清单估得太少，settings-1 的框架和首页就用了 599）；固件增量不超过 40 KB。
- 起步：主会话给的分支（当前主线，已含 settings-1、control-1、design-lang-1）。先做“开工前先核对”第 3 条的基线。

## 验收

1. **照 MuditaOS 走一遍**（模拟器脚本，每步截图并断言，同一条路再用能力调用走一遍，结果一样）：打开设置 → 首页“手机模式 · 正常” → 进去点“勿扰” → 返回首页那行写“勿扰” → 勿扰页把“谁的电话能响”选成“联系人”、开“15 分钟内打第二次就响” → `notify.dnd_get` 读到 `calls=2, repeat=true`，联系人来电响、陌生号码不响、同一陌生号码 15 分钟内（测试时钟快进）第二次响（用现有的来电测试夹具，`tests/notify_model.c`）→ 加一个 22:00—次日 7:00 周一到周五的时段，再关掉它 → 进声音页选“来电铃声”，点“双音”：打勾移过去、`audio.preview` 响一次、返回后那行写“双音”；闹钟铃声同样 → 自动锁屏单选页选 10 分钟，显示页和锁屏壁纸页那行都写 10 分钟 → 搜“暖光”“铃声”“勿扰”“接入点”“PIN”“自动锁屏”，每个都落到对应那一行。
2. **两张 SIM 卡各自的状态**：外置卡在用、内置卡写“没在用”；点内置卡切过去，两行状态对调，只刷这两行；拔掉卡写“没插卡”；“卡锁着”夹具下写“卡锁着，要输 PIN”和说明；切卡不可用时两行变灰写原因；S31 宿主机测试里 `config_readonly` 为假、`apn_readonly` 为真，接入点页写“这台设备暂不能改接入点”。
3. **Wi-Fi**（照 Brookesia）：进页自动扫一次，连着的那个在最上面，已存的标“已存”；点已存的直接连，那一行“正在连接”再“已连接”；没存过的加密网络进连接页，没填密码“连接”不能点；密码错写“密码不对”、网络不在写“这个网络不在附近”、超时写“等了太久没连上”（超时用 `hal_net_sim.c` 新夹具），都留在连接页，输入的字还在；关掉 Wi-Fi 只剩一句“Wi-Fi 已关闭”；没有网络时写一句并有“刷新”。快照和日志里查不到密码；AI 带密码连接照旧被拒。
4. **家人和孩子**：孩子的设置里“每天能用多久”“孩子的音量上限”是单选页；禁用时段用时、分选择，不再打字；换成孩子，从控制中心点设置进来只看到孩子能改的项，搜“密码”只写“家长管理”；孩子在手机模式页点飞行模式不生效，写“家长管理”，AI 去开也被拒。
5. **一份声明**：扩展 settings-1 的 `one_declaration` 检查：手画页和家人页上每一行的名字都等于注册表里那项的名字，每项都能搜到、能打开、能定位；反向对照：在页面里把一行名字改成和注册表不一样，测试必须失败；把一项从注册表删掉而页面还画它，测试必须失败。反向对照照 settings-1 的做法放在手动跑的脚本里，跑完核对源码没变。
6. **首页两页**：默认字号首页页数等于 2（断言）；最大字号写实际页数。
7. **只刷那一行**：显示页改暖光、声音页换铃声、Wi-Fi 状态从连接中变已连接、日期时间页开关自动校时、孩子的设置页改音量上限、移动网络页切卡，各自只刷变了的那一块（模拟器显示统计断言，状态栏那条照 settings-1 单独列出）。
8. **改前改后截图**：网络、Wi-Fi、连接页、移动网络、接入点、手机模式、勿扰、声音、铃声单选页、显示、自动锁屏单选页、家人、孩子的设置、禁用时段、日期和时间、蓝牙、首页、孩子从控制中心进的设置页，默认和最大文字大小各一套（孩子只有一套）。
9. 主工程普通全量 ctest 过（含 `ui-scale`、`settings-1`、`settings-a`、`settings-b`、`settings-gaps`、`settings-small`、`time-settings`、`mobile-network`、`cell-status`、`bluetooth-settings`、`kid-1`～`kid-3`、`kid-apps`、`notify-model`、`control-center`；基线已失败的按“开工前先核对”第 3 条单列，`cell-status` 要说清前后）；ASan 跑设置相关测试和 `ui-scene-*` 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告（基线 `today_edit.c:258` 若仍在，写明并用临时改动量大小、不提交）。notify_model.c、theme.c、comm_service.c 三处在对方分支合入后再跑一次全量。报告写：固件增量、产品代码行数、`gcccheck` 结果、全量结果原话、改过的每一条已有测试（只改定位方式、页面结构，以及第 8 条有意改的 `cell.c:125`，业务断言一样严）、改名或删掉的能力、“要用户定”（SIM 解锁和改 PIN、短信提示音、锁屏时显示通知），以及给以后加设置页的人看的一段“一页怎么从注册表画出来”。
