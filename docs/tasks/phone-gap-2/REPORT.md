# phone-gap-2 缺口检查报告

2026-11-10；快照 `4099f390`（分离 HEAD）。以 [11-08 检查](../phone-gap/REPORT.md)（79 项：有 13、部分 56、没有 4、未查到 6）为底更新。只新增本报告，不改代码、测试、文档。（Claude 子代理只读检查；运行环境不让子代理写报告文件，本文由主会话按它的回报保存。）

**证据边界**：本次**没有编译、没有跑测试、没有碰设备**。下文的“有”=【实现】读代码和测试文件确认存在；引用别的任务报告里的“通过”“p50”等写作“××报告【实测，模拟器】”，不是本次实测。所有硬件相关功能真机都未验证；S31 端口的 HAL 全是空实现（`ports/esp32s31/hal_stubs.c`）。路径都相对仓库根目录。

**一句话结论**：模拟器里已接近“没装应用的手机”（79 项中有 38、部分 35、没有 6），主要缺口是几项没设计或没定范围的设置和工具、依赖真实中枢的 AI 后端，以及**整个真机端口**；真机上现在什么都跑不起来。

## 1. 按完成标准（R-068）逐条打分

打分：**A** 模拟器里完整可用；**B** 主体可用、有具体缺项；**C** 只有一部分；**真机** 一栏现在全是“未接”。

| 标准条目 | 模拟器 | 模拟器里现在能做到（证据） | 还缺什么 |
| --- | --- | --- | --- |
| 框架和组件／能写应用 | B | C 应用注册、生命周期、能力、确认、控件语义（F1）、短框和长文（F2/text-long）、列表分页（F4）、卡片（F5）、活动和任务（F6）、搜索（F7）、主页编辑（F8）、文件选择器（F9）、后台打开集合和退订（fw-gaps）；写法说明 `docs/system/app-framework.md` 和可跑示例 `examples/jot`（`tests/jot`、`tests/fw_gaps`） | 11 个系统壳文件和 `apps/home` 仍手写快照（A05）；“全部”页工具组最多列 16 个应用（`system/ui/shell/navigation.c` L466 `count < 16`），应用表上限 32（`system/ui/app_registry.c` L27）；生成页面／脚本／应用包安装更新卸载只有测量原型（`docs/tasks/script-s31/REPORT.md`） |
| 基础通信：Wi-Fi | A | 扫描、连接、记住、忘记、自动选网（`system/services/net/`，`tests/integration/net_auto.c`，`tests/settings_gaps.c`） | 没连上的已存网络管理没有画板（已在待问清单）；真机 `hal_wifi_*` 空 |
| 基础通信：蓝牙 | B | 蓝牙服务、耳机模型、SetBluetooth 页（`system/ui/shell/bluetooth_page.c`，`tests/bluetooth.c`） | BH1098G 扫描配对协议不知道，真机 `hal_bt_available` 为假；音箱、翻页器收起待定；扫描没有取消接口（set-bluetooth 报告） |
| 基础通信：4G 数据 | B | 选网（自动／Wi-Fi／4G）、在线选 SIM、NT26 AT 模型（`system/services/comm/comm_service.c`，`tests/simulator/selftest.c`） | 飞行模式、APN、漫游、流量统计、热点都没有（C04、C05）；真机 4G 数据通路（PPP 还是模组协议栈）未查到 |
| 通话 | B | 拨号、来电打断、接听挂断、静音、通话键盘、耳机切换、通话记录落盘、从联系人拨（`system/apps/phone/phone.c`，`tests/phone_calls`；phone-2 报告） | 通话页分钟数不走（phone-calls 报告）；通话声音是否经本机音频芯片未知、通话录音不可用（`call.record` 只返回原因）；`AT+CMUT`/`AT+VTS` 未按 NT26 手册核对 |
| 短信 | B | 会话落盘（`sms.messages`，`comm_service.c` L44）、写短信、长文编辑页、收件人从联系人选、失败／未知回执（`system/apps/sms/sms.c`，`tests/sms`） | 上限 118 汉字（`comm_service.c` L254、L394）；**收到的分段长短信不拼接**（`drivers/chips/nt26/nt26.c` 只用文本模式 `AT+CMGR`，未查到分段处理）；超过 70 字的发送靠模组分段，未核 |
| AI | B（设备端）／C（整体） | 按键问、叫名字、打断、长答分页、离线／超时／额度页、AiLog、回执、确认页（`ai_service.c`，`system/ui/shell/ai_chat.c`、`ai_pages.c`、`confirm_card.c`；`tests/ai_chat.c`、`tests/ai_pages.c`） | 回答缓冲 2048 字节，超出丢弃（`ai_service.c` L45）；无对话历史和重播；记忆、运行记录无后端；**模型、听写、合成都是替身**（`hub/ink_hub/models/fake.py`、`speech/fake.py`） |
| 系统设置 | B | 网络、蓝牙、显示、声音（铃声、按键音、振动、勿扰）、语音、家人、孩子、锁屏壁纸、存储、隐私、记录、关于和更新、恢复出厂（`settings_pages.c`、`settings_family.c`、`navigation.c`；`tests/settings_b.c`、`tests/sound_clock.c`、`tests/kid_2.c`） | 无页面：日期时间、时区、通知渠道、重新验证时长（`docs/tasks/settings-gaps/INVENTORY.md`）、语言；写“暂不能”：叫它的名字、说话能打断回答、打字接话、设备详情、开源许可（`settings_pages.c` L18–L22、L35–L36），自动备份（`backup_pages.c` L83） |
| 系统应用 | B | 按 `docs/product/system-apps.md`：设置、电话、联系人、短信、AI、备忘与清单、日历与提醒、时钟闹钟和计时、相机、相册、文件与传输、远程画面都已注册有页面（`system/apps/*`，`system/boot/main.c` L102–L125）；输入法正式词库、简拼、学词（ime-lexicon、ime-quality 报告） | `MemoRecord`／`MemoPending` 没做；秒表、世界时钟没有；扫码没有；提醒手动加删、新建文件夹、写文件、批量选择没有入口；导入书等中枢；计算器、天气在“不进首版”（待按新标准定） |
| 性能流畅 | B（模拟器） | 点按到第一帧约 234 ms、翻页可读约 601 ms、开机到主页约 1.56 s（perf-latency 报告【实测，模拟器】）；内部 RAM 余量对比（perf-memory）；后台写盘让路输入（storage-order） | 打开应用约 0.84～1.11 s（设置 1.11 s、文件 1.00 s，同报告），时间去处未拆；模拟器时延不能换算真机；栈只有主机估算 |
| 硬件驱动 | 模拟器 A／真机 未接 | 10 颗芯片驱动和板级接线有寄存器级模型和自检（`drivers/chips/*`，`boards/metalio_plus/`，`tests/simulator/selftest.c`；hardware-parity 一致 8、部分 14） | S31 端口只能编译链接（`ports/esp32s31/README.md`），HAL 全返回不可用；BH1098G 协议、NT26 通话音频、P16 电平仍未知 |
| 硬件用得充分合理 | B | 前光冷暖、振动、按键音、拍照存卡、RTC 闹钟、电量计、充电、两路 SIM、存储卡、U 盘、无线传输都已接入 | **加速度计系统没用**（`system/` 无 `hal_accel_*` 调用）；S31 自带蓝牙未评估（D-63）；锁屏相机入口收起待定 |

## 2. 79 项更新

**现在：有 38、部分 35、没有 6、未查到 0。** 11-08 已是“有”的 13 项不重抄（A02 A03 A07 B10 B19 C02 C03 C08 C09 C10 C11 C13 C14）。证据为【实现】；硬件项真机均未验。

| 能力 | 11-08 | 现在 | 缺口 | 证据 |
| --- | --- | --- | --- | --- |
| A01 整机端口 | 部分 | 部分 | 骨架能编能链，HAL 全空 | `ports/esp32s31/hal_stubs.c`；port-skeleton 报告 |
| A04 列表／分页／文件选择器 | 部分 | 有 | — | `system/ui/ui_list.c`、`file_picker.c`；`tests/ui_list_paging.c`、`tests/ui_file_picker.c` |
| A05 控件语义、自动快照 | 部分 | 部分 | keyboard、confirm_card、interrupt_card、member_gate、member_pages、kid_pages、power_pages、power_menu、off_picture、ai_panel、lock_mark、`apps/home` 仍手写快照 | `system/ui/ui_semantics.c`；`grep '.snapshot ='` |
| A06 人与 AI 共路 | 部分 | 有 | 剩下是“没入口”，归各项 | settings-gaps INVENTORY 全量复核 |
| A08 换成员收拢 | 部分 | 有 | 逐服务检查实现；集中收尾不合入 | backlog member-quiesce 结果 |
| A09 确认持久 | 部分 | 有 | — | `tests/confirm_durable`、`tests/confirm_pages` |
| A10 卡片、活动卡、主页编辑、图标菜单 | 部分 | 有 | — | `activity_card.c`；`navigation.c` IconMenu、TodayEdit；`tests/activity_card.c`、`tests/fw_f8` |
| A11 Tasks 页 | 部分 | 有 | — | `activity_card.c` 注册 `Tasks` |
| A12 全局搜索 | 部分 | 有 | 设置结果只到分组页；文件只到信息页 | `system/ui/search.c`；search 报告 |
| A13 生成页、脚本、应用包 | 部分 | 部分 | 只有测量；无安装／更新／卸载／签名 | script-s31 报告；框架设计 §9 末段 |
| B01 主页、Dock、状态栏、控制中心 | 部分 | 有 | 状态栏无蓝牙／充电（规范未要求，仅建议） | `navigation.c` L252 起 |
| B02 锁屏 | 部分 | 有 | — | `system/ui/lock_mark.c` L34–L44 |
| B03 成员、密码、找回、清空 | 部分 | 部分 | 忘记密码设备上不能重置，要中枢 | `member_pages.c` L178；`factory_pages.c` |
| B04 孩子主页、限时 | 部分 | 有 | — | `kid_pages.c`；`tests/kid_2.c`、`tests/kid_3.c` |
| B05 AI 对话页 | 部分 | 部分 | 2048 字节上限；无历史／重播 | `ai_service.c` L45；ai-chat 报告 |
| B06 真模型、听写、合成 | 部分 | 部分 | 中枢只到第 1 步 | `hub/README.md`；`models/fake.py` |
| B07 对话历史、记忆 | 部分 | 部分 | 对话历史、记忆、运行记录无后端 | `settings_family.c` L120、L171、L221 |
| B08 键盘、候选、听写 | 部分 | 有 | 真听写归 B06 | ime-lexicon、ime-quality 报告 |
| B09 长文本、草稿、选区 | 部分 | 有 | — | `tests/text_long` |
| B11 电话 | 部分 | 部分 | 分钟数不走；录音不可用；真机音频、AT 待核 | `phone.c`；phone-calls、phone-2 报告 |
| B12 短信 | 部分 | 部分 | 分段收不拼接；>70 字发送未核 | `nt26.c` L194–L229；`comm_service.c` L254 |
| B13 联系人 | 没有 | 有 | 每人一个号码 | `contacts.c`；`tests/contacts` |
| B14 闹钟 | 部分 | 有 | — | `clock.c`；`tests/clock_alarm.c` |
| B15 秒表、倒计时、世界时钟 | 未查到 | 部分 | 倒计时有（重启取消）；另两项没有 | `clock.timer_*`；`tests/sound_clock.c` |
| B16 日历、提醒 | 部分 | 部分 | 无独立日程；提醒不能手动加删 | `calendar.c`；INVENTORY `reminder.*` |
| B17 备忘、清单 | 没有 | 部分 | MemoRecord、MemoPending 没做 | `memo.c`；memo 报告 |
| B18 录音及附件 | 部分 | 部分 | 无录音页和备忘关联；编码待定 | INVENTORY `audio.record` |
| B20 文件 | 部分 | 部分 | 新建文件夹、写文件、批量、导入、打开 EPUB/PDF 没有 | `files.c`；files-app-1 报告 |
| B21 编号、媒体索引 | 没有 | 有 | — | `tests/file_index_3`、`tests/index_prune` |
| B22 解码、缩略图 | 部分 | 有 | — | file-index-2、thumb-wiring 报告 |
| B23 U 盘、无线传输 | 部分 | 有 | — | `transfer_pages.c`；`tests/transfer_pages` |
| B24 备份恢复 | 部分 | 部分 | 只看当前家人最新一份；无自动备份；重绑定要中枢 | `backup_pages.c` L83、L170 |
| B25 相机、扫码 | 部分 | 部分 | 拍照存卡有；扫码没有 | `camera.photo`；camera-gallery 报告 |
| B26 相册 | 没有 | 有 | 不验证模型识图 | `gallery.c`；`tests/camera_gallery` |
| B27 远程画面 | 部分 | 部分 | 设备端齐；真实浏览器节点没有 | `remote.c`；`hub/browser_node/README.md` |
| C01 Wi-Fi 设置 | 部分 | 有 | 未连已存网络管理待画板 | `navigation.c` L1251 |
| C04 飞行模式 | 未查到 | 没有 | 能力、设置、页面都没有 | `docs/system/capabilities.md` |
| C05 APN、漫游、流量、热点 | 未查到 | 没有 | 同上 | 同上；`pairing.c` |
| C06 蓝牙设置 | 部分 | 部分 | 芯片协议未知 | `bluetooth_page.c`；`hal_bt.h` |
| C07 音箱、翻页器 | 部分 | 没有 | 收起待用户定 | backlog settings-all |
| C12 锁屏画面、壁纸 | 部分 | 有 | — | `settings_family.c` L219；`wallpaper.c` |
| C15 铃声、按键音、振动、渠道 | 部分 | 部分 | 通知渠道没有 | INVENTORY `notify.set_channel` |
| C16 SetVoice | 部分 | 部分 | 名字、打断“暂不能” | `settings_pages.c` L18–L19 |
| C17 孩子音量上限 | 部分 | 有 | — | SetKid |
| C18 AI 读剪贴板 | 部分 | 有 | — | `settings_pages.c` L44 |
| C19 重新验证时长 | 部分 | 部分 | 无控件，D-95 未定 | INVENTORY |
| C20 家人与孩子权限 | 部分 | 部分 | 未查到可改的应用白名单、禁用时段 | settings-b、kid-2 报告 |
| C21 存储 | 部分 | 有 | — | `tests/storage_usage` |
| C22 电量、省电、充电 | 部分 | 有 | 校准要真机；省电重启不保留（待问） | `power_pages.c`；`tests/power_pages.c` |
| C23 日期时间 | 部分 | 部分 | 无页面（无画板） | INVENTORY `time.set` |
| C24 时区 | 部分 | 部分 | 同上 | INVENTORY `time.zone` |
| C25 语言、区域 | 未查到 | 没有 | — | 能力表无 locale |
| C26 无障碍 | 部分 | 部分 | 无页面朗读、触控辅助 | `theme.c` |
| C27 自动下载更新 | 部分 | 有 | — | ABOUT_ROWS |
| C28 更新、回滚 | 部分 | 部分 | 更新说明不可查；回滚要换引导程序；无中枢固件源 | `update_pages.c` L106、L125 |
| C29 隐私、记忆 | 部分 | 部分 | 记忆、运行记录无后端 | `settings_pages.c` L42–L43 |
| C30 关于、设备信息 | 部分 | 部分 | 设备详情、许可没接 | `settings_pages.c` L35–L36 |
| C31 首次开机、绑定 | 部分 | 部分 | BootHello 示例正文没做；账号要中枢 | `boot_flow.c`；boot-flow 报告 |
| D01 触摸、按键、振动、加速度计 | 部分 | 部分 | 加速度计系统没用 | `audio_service.c` L1226 |
| D02 墨水屏 | 部分 | 部分 | 算法固定（D-110）；整套系统未上屏 | `drivers/display/pqbn/`；phase3 记录 |
| D03 RTC 唤醒 | 部分 | 部分 | P16 电平不符待查 | hardware-parity |
| D04 音频与通话声音 | 部分 | 部分 | 通话路径、耳机协议、采样率未知 | ui-dependencies §3、§6 |
| D05 Flash、卡、USB 安全 | 部分 | 部分 | 真 FAT／USB 未验 | `tests/storage_order`、`data_powercut.cmake` |
| D06 性能 | 部分 | 部分 | 打开应用约 1 s 未分析；真机无数 | perf-latency、perf-memory 报告 |
| D07 紧急、位置、NFC、生物识别、补光 | 未查到 | 没有 | 硬件无后四项证据；紧急呼叫待问 | hardware-parity；system-apps 待确认 |
| D08 计算器、天气、邮件 | 未查到 | 没有 | “不进首版”待按新标准定 | system-apps.md |

## 3. 缺口分类

### A 现在就能做（不用拍板，模拟器能验；各约一小时【估算】，满足 backlog G1～G5）

| # | 任务名 | 要改的地方 | 验收 | 并行 |
| --- | --- | --- | --- | --- |
| 1 | `ai-answer-long` | `ai_service.c` 的 `ANSWER_CAP` 改 PSRAM 分块／流式；`ai_chat.c`、`ai_panel.c` 分页读 | sim_hub 合成 6 KB 回答逐字一致、可翻页、内部 RAM 余量不降 | 与 4 冲突，先做 |
| 2 | `sms-multipart` | 按 3GPP TS 23.040 收拼接、超 70 字发分段；`nt26.c`、`comm_service.c`、模拟器 AT 模型 | 注入乱序、缺段、超时；会话一条完整；单段不变 | 独立 |
| 3 | `semantics-shell-1` | confirm_card、interrupt_card、power_menu、member_gate、lock_mark、off_picture 迁语义合同 | 快照由语义导出；秘密只给有无；动作核版本；hub 契约不变 | 与 4 可并行（不改 `ui_semantics.c`） |
| 4 | `semantics-shell-2` | keyboard、ai_panel、member_pages、kid_pages、power_pages、`apps/home` | 同上；键盘组合快照字段不变 | 排在 1 后 |
| 5 | `apps-page-limit` | “全部”页工具组分页，去掉 `count < 16`；核 `MAX_APPS`、`app.list` 8 KB、`app.cards` 2 KB | 注册 20 个假应用都能翻到打开；超上限明确失败 | 改 `navigation.c` |
| 6 | `voice-interrupt-setting` | 新设置 `voice.interrupt`，`audio_service.c` 插话判断读它；SetVoice 接上 | 关时不打断、开时 `ai.barge_ins` 计数 | 与 7 串行 |
| 7 | `about-details-licenses` | 设备详情（`hal_device_get_info`、版本、存储、电量；标识字段快照遮蔽）、开源许可列表 | 两页有快照；识别号不进 AI 快照的反例 | 与 6 串行 |
| 8 | `call-minutes` | `phone.c` 订阅分钟，通话页显示分钟数 | 模拟 3 分钟显示 0→3，只在分钟变时刷新 | 独立 |
| 9 | `power-pages-member` | 电量页订阅换家人 | 换人后状态正确 | 独立 |
| 10 | `open-app-latency` | 打开设置／文件／短信约 1 s 的时间拆分，只留有收益改动 | 前后 p50/p95【实测，模拟器】，确定性不变 | 最后单独做 |
| 11 | `qr-eval` | 评估扫码库（如 quirc，ISC）识别率、RAM、S31 增量，不进产品 | 报告数字；选库要用户定则转 B | 独立 |
| 12 | `ui-deps-refresh` | `ui-dependencies.md` §4.2、§4.3 按代码改状态（G3） | 每行有代码或报告对应 | 只改文档 |

### B 要用户定

已在 project.md 11-10“攒着问用户的”和 backlog 里的只引用：settings-gaps 无画板项（日期时间、时区、通知渠道、逐条删通知、新建文件夹、写文件、提醒编辑删除、文字重做、未连 Wi-Fi 管理、D-95、语音备忘关联）；画板对照（1.2 倍）；“继续读”卡片；计时页截图；省电保留；备忘录音编码；飞行模式、日期时间页、计算器等按不按标准补；紧急呼叫；语言地区；音箱翻页器；D-109；D-40；reader_basic 预算；刷新最后一组改动未上真屏。

建议并入“按不按标准补”那一问：秒表和世界时钟、APN／漫游／流量／热点、天气、扫码库、孩子应用白名单和禁用时段、叫它的名字、打字接话、自动备份和历史备份。

新增：

| 问题 | 为什么非问不可 |
| --- | --- |
| 加速度计做什么用（转向、拿起唤醒、静止省电，或不用） | 标准要求硬件充分合理应用；转向要整屏重排全刷，唤醒影响待机，属产品取舍，无设计 |
| 真机屏幕输出层：用上游 LGPL 的 EPDiy 输出（构建时取，RK-16 履行义务），还是自写 LCD_CAM 输出 | 显示 HAL 前提；测量固件就是从上游克隆编进去的（`tools/hw-bench/main/CMakeLists.txt`）；涉及许可和分发 |
| 无障碍做到什么程度 | “相当于手机”通常含无障碍，规范没有 |

### C 要上真机（写设备前要整片备份并经用户同意；这里只列）

可以先写、先编译、不接设备（`ports/esp32s31/build.sh`、CI `s31-port.yml`；每项 1～2 个一小时任务【估算】）：

| HAL | ESP-IDF 接口 | 引脚／参数出处（仓库内） |
| --- | --- | --- |
| `hal_i2c` | `driver/i2c_master.h` | GPIO0/1、100 kHz：hardware-parity 第 4 项；`tools/hw-bench/main/bench.c` L66–L67 |
| 板级芯片 | 复用 `drivers/chips/*`、`boards/metalio_plus/`；GPIO2 `gpio_isr_handler_add` | `metalio_board.h`；parity 第 3、5、7、11～14 项 |
| 共用胶水 | 抽出 `ports/sim/hal/` 不依赖 `sim.h` 的部分（如 `hal_cellular_sim.c`） | 模拟器全量不变即可验证 |
| `hal_uart`、`hal_modem_*` | `driver/uart.h` | UART1 BH1098G、UART2 NT26：parity 第 15、17 项（上游 config.h L146、L153、L157–L160） |
| `hal_fs_*` 内部 | `esp_partition_*` 作 `third_party/littlefs` 块设备；填 `mtime` | `docs/hardware/README.md` 分区；本项目分区另定（写分区=写设备） |
| `hal_fs_*` 卡 | `driver/sdmmc_host.h`、`esp_vfs_fat_sdmmc_mount`，4 线 | parity 第 19 项；`bench.c` L326–L333 |
| `hal_light`、`hal_haptic` | `driver/ledc.h` 20 kHz；P03 | parity 第 9、10 项 |
| `hal_power`、`hal_rtc`、`hal_accel` | `esp_sleep`、`esp_pm`、`esp_restart` | parity 第 8、13、14、22 项 |
| `hal_net`、`hal_ws_*`、`hal_provision`、`hal_transfer` | `esp_wifi`、`esp_netif`、`esp_websocket_client`／`esp_tls`、SoftAP＋`esp_http_server`、lwIP | `docs/hub/device-protocol.md` |
| `hal_usb` | `esp_tinyusb` MSC | `docs/hardware/README.md` |
| `hal_ota` | `esp_ota_ops.h`；签名 mbedTLS／PSA | 回滚要换引导程序，须上机并同意 |
| `hal_audio`、`hal_codec` | `driver/i2s_std.h` 从模式 16 kHz；P04、P14 | parity 第 15、16 项 |
| `hal_camera`、`hal_jpeg` | `esp_driver_cam` DVP（S31 支持待核）；`driver/jpeg_decode.h` | parity 第 20 项；upstream-firmware.md L259 |
| `hal_display` | `esp_lcd`／LCD_CAM；TPS65185 现有驱动 | 先等 B 的输出层问题；VCOM 1200 mV；`panel_e0470a14.h`（D-110 固定） |
| `hal_assets` | `esp_partition_mmap` | font_data 分区 |

必须上机：整套系统首启和屏幕（保护先自测）；栈高水位、PSRAM 栈性能；0 号核输入任务对刷新和 Wi-Fi 的影响、写盘 500 ms／10 s 校准；P16 和 RTC 唤醒；4G 数据通路和 SRDY；NT26 `AT+CMUT`、`AT+VTS`、超 70 字短信、上报格式；BH1098G 模式 2/3、蓝牙协议、44.1/48 kHz；S31 蓝牙与 Wi-Fi 共存；摄像头出帧；真 FAT、USB、断电；电量计校准与电流；触摸和加速度计坐标轴；OTA 回滚与引导程序。

### D 要真实中枢或花钱

| 缺口 | 依赖 |
| --- | --- |
| 真模型、听写、合成、额度（B05、B06） | 中枢第 2 步；D-78、D-79 已选；要服务器（D-74）和密钥 |
| 记忆、对话历史、运行记录（B07、C29） | `hub/ink_hub/memory`、`sync` 空包 |
| 打字接话（C16） | 模型；设备开关可先用 sim_hub |
| 账号绑定、重绑定、设备上重置密码（B03、C31、B24） | `hub/ink_hub/accounts` 空包 |
| 备份上传对账、历史备份 | 中枢同步 |
| 导入书（B20） | 放书协议 |
| 真实网页（B27） | `hub/browser_node` 只有说明 |
| 固件源、更新说明（C28） | `hub/ink_hub/firmware` 空包 |
| 真打电话、发短信、VoLTE | 话费短信费，要用户同意 |
| 天气（如果要） | 数据源（可能收费） |

## 4. 总评

- **模拟器里**：框架、通信控制、电话、短信、联系人、闹钟、日历、备忘、文件、相机相册、设置都已能用。还差几项没画板或没定范围的设置和工具（日期时间、时区、通知渠道、飞行模式、APN、语言、秒表和世界时钟、扫码、备忘录音），AI 长回答和分段短信两处硬伤，剩余页面的语义迁移，以及依赖中枢的 AI 历史、记忆和真模型。
- **真机上**：还差整个端口，HAL 全空，系统起不来。先写可编译的驱动胶水，再按备份和同意规则逐项上机，屏幕输出层先等用户定。

## 5. 本次怎么查的

- 读了 CLAUDE.md、AGENTS.md、11-08 报告、backlog 全文、project.md 11-10、system-apps.md、ui-dependencies.md、capabilities.md（368 项）、app-framework.md、app-framework-design.md §7～§9、settings-gaps INVENTORY、各任务报告“没做到的”，以及表中源码。
- 只用了只读查看命令；没有构建、测试、写设备，没读 ink-hw。
- 没做到：没运行测试核实他人结论；batch10、batch11 审计正文没逐条看；画板不在快照里，设备详情页和许可页有没有画板未查到。

## 主会话补记（2026-11-10）

- 同日用户答了攒着的问题（D-109、D-112～D-116，R-069、R-070）：飞行模式、计算器、省电保留、“继续读”、AI 图标、中英两种语言都已开工或排进下一批。A 类第 1 项 `ai-answer-long` 同时开工，第 5 项并进了 `calculator`。
