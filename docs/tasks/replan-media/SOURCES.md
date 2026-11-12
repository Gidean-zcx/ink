# 来源与定位

查阅日期：2026-11-12。以下行号来自本次实际读取的固定文本；仓库完整哈希、文件SHA-256/行数见[source-evidence.json](source-evidence.json)。仅核查与结论相关的代码段，没有逐行审计整个仓库，也没有运行上游代码。27个既有本地源码文件另与固定Git对象逐字节核对一致；计算文件哈希不代表源码与发布固件一致。Mogo等项目存在混合或源码可见许可；本次按用户要求比较代码可得性，不以许可证作排名门槛。

## S01

**Mogo**；`chopper1026/mogo-e4p@864f2f9dc75980a67d2d6b67b17365020b7a50ce`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [main/book/book_ticket.c:268–321](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/book/book_ticket.c#L268-L321) | 从持久进度和真实书摘绘制票根，两种背景。 |
| [main/lock/lock_clock.c:25–74](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/lock/lock_clock.c#L25-L74) | RTC/系统时间、分钟身份与变化更新。 |
| [main/apps/app_components.c:26–86](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/apps/app_components.c#L26-L86) | 两个首页插槽的阅读/迷你书库/天气/手账选项。 |
| [main/apps/app_dashboard.c:134–145](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/apps/app_dashboard.c#L134-L145) | 迷你书库有界动画；文件头第14行说明五书设计。 |
| [tools/generate_calendar_data.cjs:9–54](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/tools/generate_calendar_data.cjs#L9-L54) | 实际调用lunar-javascript生成月历/宜忌/神煞表。 |
| [main/time/calendar.c:31–75](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/main/time/calendar.c#L31-L75) | 离线日期与节气/民俗表查询。 |
| [docs/development/guides/CALENDAR.md:9–14](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/docs/development/guides/CALENDAR.md#L9-L14) | 北京时间、产品时间范围、锁屏生成，待机跨日不自动唤醒。 |
| [docs/development/guides/LOCK_CLOCK.md:6–20](https://github.com/chopper1026/mogo-e4p/blob/864f2f9dc75980a67d2d6b67b17365020b7a50ce/docs/development/guides/LOCK_CLOCK.md#L6-L20) | 分钟更新及待机/密码边界；实机耗电仍未知。 |

## S02

**CrossMux**；`0x1abin/crossmux@a6fd569f7552db4ea1a6550ced7bc06568ae0e61`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [src/activities/apps/standby/StandbyActivity.cpp:55–66](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/standby/StandbyActivity.cpp#L55-L66) | 时钟/中文日历两种face，中文日历仅竖屏。 |
| [src/activities/apps/standby/StandbyActivity.cpp:392–420](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/standby/StandbyActivity.cpp#L392-L420) | 按下次更新时间浅睡及唤醒处理。 |
| [src/activities/apps/standby/ChineseAlmanac.cpp:44–79](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/standby/ChineseAlmanac.cpp#L44-L79) | 宜忌为12地支固定词池。 |
| [src/activities/apps/standby/ChineseAlmanac.cpp:669–676](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/standby/ChineseAlmanac.cpp#L669-L676) | 宜忌索引只取日地支。 |
| [src/activities/apps/gomoku/GomokuGameActivity.cpp:55–76](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/gomoku/GomokuGameActivity.cpp#L55-L76) | 恢复棋局和结束状态。 |
| [src/activities/apps/gomoku/GomokuGameActivity.cpp:324–425](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/gomoku/GomokuGameActivity.cpp#L324-L425) | AI落子、胜负/满盘、悔棋。 |
| [src/activities/apps/gomoku/GomokuStore.cpp:26–55](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/gomoku/GomokuStore.cpp#L26-L55) | 存档格式与难度等字段。 |

## S03

**CrossInk**；`uxjulia/CrossInk@9914146eeae7b46b300f475a16c32426fc02ec1f`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [src/components/themes/dashboard/DashboardTheme.cpp:175–218](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/components/themes/dashboard/DashboardTheme.cpp#L175-L218) | 剩余阅读时间/按日阅读速度推测完成日期。 |
| [src/components/themes/dashboard/DashboardTheme.cpp:271–360](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/components/themes/dashboard/DashboardTheme.cpp#L271-L360) | 进度、阅读时间、页速及书籍统计。 |
| [src/components/themes/dashboard/DashboardTheme.cpp:432–445](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/components/themes/dashboard/DashboardTheme.cpp#L432-L445) | 连续阅读天数呈现。 |

## S04

**原厂Plus**；`CloudZao/Metalio-E-INK4-Plus@874538eb54f7f7eb593cd40cd1ba6dc8497226e7`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [main/display/screen/standby_screen/standby_screen.cc:73–92](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/standby_screen/standby_screen.cc#L73-L92) | Classic/Wallpaper两种待机视图。 |
| [main/display/screen/standby_screen/standby_classic.cc:16–95](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/standby_screen/standby_classic.cc#L16-L95) | 日期/天气/待办入口。 |
| [main/display/screen/standby_screen/standby_wallpaper.cc:34–104](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/standby_screen/standby_wallpaper.cc#L34-L104) | 图像alpha与DecodeImageFileOverlay。 |
| [main/display/screen/home_screen/home_screen.cc:96–135](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/home_screen/home_screen.cc#L96-L135) | 分页/当前页，编译期应用数量。 |
| [main/display/screen/home_screen/home_apps_slash.cc:29–63](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/home_screen/home_apps_slash.cc#L29-L63) | 固定应用表生成页面。 |
| [main/display/screen/bluetooth_screen/bluetooth_uart.cc:273–276](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/bluetooth_screen/bluetooth_uart.cc#L273-L276) | RECONNECT分支仅转发状态，不能证明三次重试/外放续播。 |

## S05

**kiikoread Plus**；`wegooo-cell/Metalio-S31-kiikoread@83942eb45d13064a47aca7ff04c2faf8fd51f98a`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [main/book/book_ticket.c:343–398](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_ticket.c#L343-L398) | 票根读取上次书、位置及统计。 |
| [main/book/book_lock_collage.c:684–736](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_lock_collage.c#L684-L736) | 书架封面拼贴及draw/prepare入口。 |
| [main/lock_screen.c:43–74](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/lock_screen.c#L43-L74) | 图片/书签锁屏缓存与样式身份。 |

## S06

**kiikoread改版**；`oouu00/Metalio-S31-kiikoread-xiugai@746ce316faf61ce5228eceee618eacb50a02a395`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [main/apps/app_music.c:124–148](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/apps/app_music.c#L124-L148) | 收藏优先及分层曲库。 |
| [main/apps/app_music.c:221–255](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/apps/app_music.c#L221-L255) | 目录/歌曲分类及收藏。 |
| [main/music/music_player.c:5–17](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_player.c#L5-L17) | 16k双声道、依赖解码器/PSRAM环与任务。 |
| [main/music/music_player.c:51–58](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_player.c#L51-L58) | 保存曲目、模式、位置及播放标志。 |
| [main/music/music_player.c:298–303](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_player.c#L298-L303) | 扫描后缀含M4A/AAC，不含M4B。 |
| [main/music/music_player.c:369–379](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_player.c#L369-L379) | M4A/AAC交给ESP音频简单解码器。 |
| [main/music/music_player.c:561–609](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_player.c#L561-L609) | MP3比例落点；其它格式回头解码丢弃。 |
| [main/music/music_player.c:657–673](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_player.c#L657-L673) | 同曲位置恢复及歌词加载。 |
| [components/bt_host/src/bt_host.c:5–18](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/components/bt_host/src/bt_host.c#L5-L18) | 原厂BH模组音频路径，与BLE翻页器分开。 |
| [components/bt_host/src/bt_host.c:166–171](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/components/bt_host/src/bt_host.c#L166-L171) | 主动重连预算5次。 |
| [components/bt_host/src/bt_host.c:1571–1588](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/components/bt_host/src/bt_host.c#L1571-L1588) | 预算耗尽仅记日志，不是三次后外放续播。 |

## S07

**墨集MoJi**；`NinthCode/MoJi@af02c8a6b83c51f24355a23bab0d3b2ef394caea`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [README.md:3–7](https://github.com/NinthCode/MoJi/blob/af02c8a6b83c51f24355a23bab0d3b2ef394caea/README.md#L3-L7) | 自称目标为E-Ink4/S3，不是Plus。 |
| [README.md:46–48](https://github.com/NinthCode/MoJi/blob/af02c8a6b83c51f24355a23bab0d3b2ef394caea/README.md#L46-L48) | 承认基于Launcher。 |
| [boards/metalio-eink4/main_menu.cpp:216–241](https://github.com/NinthCode/MoJi/blob/af02c8a6b83c51f24355a23bab0d3b2ef394caea/boards/metalio-eink4/main_menu.cpp#L216-L241) | 多页循环切换。 |
| [src/market_client.cpp:96–137](https://github.com/NinthCode/MoJi/blob/af02c8a6b83c51f24355a23bab0d3b2ef394caea/src/market_client.cpp#L96-L137) | 文件大小、SHA-256及应用镜像检查。 |
| [docs/APP_DEVELOPMENT.md:5–9](https://github.com/NinthCode/MoJi/blob/af02c8a6b83c51f24355a23bab0d3b2ef394caea/docs/APP_DEVELOPMENT.md#L5-L9) | 每个应用是独立固件，自行初始化硬件。 |
| [docs/ARCHITECTURE.md:5–17](https://github.com/NinthCode/MoJi/blob/af02c8a6b83c51f24355a23bab0d3b2ef394caea/docs/ARCHITECTURE.md#L5-L17) | 动态分区及安装/启动流程。 |
| [src/main.cpp:518–525](https://github.com/NinthCode/MoJi/blob/af02c8a6b83c51f24355a23bab0d3b2ef394caea/src/main.cpp#L518-L525) | 选择factory启动目标、重启返回的代码入口。 |

【实测】MoJi树与S08固定树比较，同路径同Git blob **220项**，逐项路径见source-evidence中的`moji_launcher_lineage`。这证明同一代码线的继承，不证明精确fork日期或共同祖先提交。MoJi当前历史只有初始化提交。

## S08

**Launcher/M5Launcher**；`bmorcelli/Launcher@2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [README.md:12–24](https://github.com/bmorcelli/Launcher/blob/2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba/README.md#L12-L24) | 上游仓库自己的刷写/启动说明。 |
| [README.md:33–62](https://github.com/bmorcelli/Launcher/blob/2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba/README.md#L33-L62) | SD/Web/在线安装和文件管理。 |
| [src/main.cpp:1–35](https://github.com/bmorcelli/Launcher/blob/2f43aa7bd3ef03e770c0a97135cbcfcc79c15aba/src/main.cpp#L1-L35) | 读取实际源文件；具体墨水屏差异由MoJi代码核对。 |

## S09

固件口袋；固定仓库`Changkai9888/CloudZao-Projects@90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8`。

- [firmware-pocket-1.0.7-s3.20-source.zip](https://raw.githubusercontent.com/Changkai9888/CloudZao-Projects/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/firmware-pocket/firmware-pocket-1.0.7-s3.20-source.zip)；1566126字节，142项；SHA-256 `6b58c74b2f9c3b390735a82aaeb158cefe9e056819dd9176e4b52bd1b9b0b7ea`。
  - 包内`firmware-pocket-1.0.7-s3.20-source/main/pocket/pocket_runtime.cc:812–948`：校验、设启动分区、重启切换/返回。
  - 包内`firmware-pocket-1.0.7-s3.20-source/main/pocket/pocket_handoff.cc:1–120`、`main/pocket/pocket_standard.cc:1–100`：读实际交接/标准协议代码。
  - 包内`firmware-pocket-1.0.7-s3.20-source/main/ui/pocket_screen.cc:1–100`：读取管理界面代码。
- [firmware-pocket-plus-1.0.7-plus.16-source.zip](https://raw.githubusercontent.com/Changkai9888/CloudZao-Projects/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/firmware-pocket/firmware-pocket-plus-1.0.7-plus.16-source.zip)；2018663字节，203项；SHA-256 `a197b62e49a55e12d5ec1e225d77670a51fb317ab8f9df0e460459deadb23bba`。
  - 包内`firmware-pocket-plus-1.0.7-plus.16/main/pocket/pocket_runtime.cc:776–912`：校验、设启动分区、重启切换/返回。
  - 包内`firmware-pocket-plus-1.0.7-plus.16/main/pocket/pocket_handoff.cc:1–120`、`main/pocket/pocket_standard.cc:1–100`：读实际交接/标准协议代码。
  - 包内`firmware-pocket-plus-1.0.7-plus.16/main/ui/pocket_screen.cc:1–100`：读取管理界面代码。
  - 包内`firmware-pocket-plus-1.0.7-plus.16/README.md:3–19`：S31、标准/旧通道，指南修订1.1与ABI1.0；`:25–35`构建范围；`:43`容量；`:51`仍需最终BIN真机验证。

只读source.zip，未下载同目录LGPL-relink包；包内二进制项仅锁屏/许可位图（不是固件镜像），未提取。字体包也未提取。203/142为ZIP成员数，不是源代码文件数。源码是否可完全复现未运行验证。

## S10

**AgentUI**；`AlayaElla/MetalioClaw4-AgentUI@172ec530ff65e11521585fe3b8611daa20a78aa4`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [external_apps/examples/radio/main/radio.cc:97–140](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/external_apps/examples/radio/main/radio.cc#L97-L140) | 节目JSON解析。 |
| [external_apps/examples/radio/main/radio.cc:431–455](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/external_apps/examples/radio/main/radio.cc#L431-L455) | 调用宿主媒体启动并处理失败。 |
| [external_apps/examples/voice_recorder/main/voice_recorder.cc:47–91](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/external_apps/examples/voice_recorder/main/voice_recorder.cc#L47-L91) | 录音/保存/取消/回放调用宿主API。 |
| [external_apps/examples/voice_recorder/main/voice_recorder.cc:102–149](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/external_apps/examples/voice_recorder/main/voice_recorder.cc#L102-L149) | 电平/录音状态处理。 |
| [external_apps/examples/voice_recorder/README.md:3–18](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/external_apps/examples/voice_recorder/README.md#L3-L18) | WAV、自动编号、5分钟上限、退出保存及真机待验。 |
| [external_apps/examples/pet_demo/main/pet_demo.cc:9–23](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/external_apps/examples/pet_demo/main/pet_demo.cc#L9-L23) | 动画显示/性能状态，非养成数据。 |
| [external_apps/examples/pet_demo/main/pet_demo.cc:227–279](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/external_apps/examples/pet_demo/main/pet_demo.cc#L227-L279) | 动作播放和帧率统计。 |

## S11

**ESP-ADF稳定v2.x**；`espressif/esp-adf@74e6be93f007faa411dab985a2b7613725506790`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [examples/player/pipeline_sdcard_mp3_control/main/play_sdcard_mp3_control_example.c:48–87](https://github.com/espressif/esp-adf/blob/74e6be93f007faa411dab985a2b7613725506790/examples/player/pipeline_sdcard_mp3_control/main/play_sdcard_mp3_control_example.c#L48-L87) | 真实管线的播放/暂停/继续/切歌。 |
| [examples/player/pipeline_sdcard_mp3_control/main/play_sdcard_mp3_control_example.c:130–170](https://github.com/espressif/esp-adf/blob/74e6be93f007faa411dab985a2b7613725506790/examples/player/pipeline_sdcard_mp3_control/main/play_sdcard_mp3_control_example.c#L130-L170) | SD列表扫描及解码管线。 |
| [examples/player/pipeline_http_mp3/main/play_http_mp3_example.c:77–125](https://github.com/espressif/esp-adf/blob/74e6be93f007faa411dab985a2b7613725506790/examples/player/pipeline_http_mp3/main/play_http_mp3_example.c#L77-L125) | HTTP读取/MP3/I2S管线。 |
| [examples/display/music_player/main/lv_demo_music.c:37–100](https://github.com/espressif/esp-adf/blob/74e6be93f007faa411dab985a2b7613725506790/examples/display/music_player/main/lv_demo_music.c#L37-L100) | 固定模拟曲目及元数据。 |
| [examples/display/music_player/README_CN.md:53–55](https://github.com/espressif/esp-adf/blob/74e6be93f007faa411dab985a2b7613725506790/examples/display/music_player/README_CN.md#L53-L55) | 说明也称模拟音乐，不当成熟曲库。 |

## S12

**ESP-ADF开发v3**；`espressif/esp-adf@12ea304254d8a1efb1d93afa78e82ab3887684c1`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [README_CN.md:5–13](https://github.com/espressif/esp-adf/blob/12ea304254d8a1efb1d93afa78e82ab3887684c1/README_CN.md#L5-L13) | v3开发中且不兼容v2。 |
| [components/esp_player_service/include/esp_player_service_playback.h:214–297](https://github.com/espressif/esp-adf/blob/12ea304254d8a1efb1d93afa78e82ab3887684c1/components/esp_player_service/include/esp_player_service_playback.h#L214-L297) | 按流暂停/继续/定位/倍速接口。 |
| [components/esp_player_service/include/esp_player_service_playback.h:496–568](https://github.com/espressif/esp-adf/blob/12ea304254d8a1efb1d93afa78e82ab3887684c1/components/esp_player_service/include/esp_player_service_playback.h#L496-L568) | 播放列表与循环模式接口。 |
| [components/esp_player_service/src/core/esp_player_service_ctrl.c:1–80](https://github.com/espressif/esp-adf/blob/12ea304254d8a1efb1d93afa78e82ab3887684c1/components/esp_player_service/src/core/esp_player_service_ctrl.c#L1-L80) | 读到接口背后的控制实现；未运行。 |

## S13

**ESP-Brookesia**；`espressif/esp-brookesia@65d66c810ad706c9e2f82fe9177b11816acee546`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [service/media/brookesia_service_audio/src/audio_decoder.cpp:1–65](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/service/media/brookesia_service_audio/src/audio_decoder.cpp#L1-L65) | 音频解码接口实现文件，不是曲库界面。 |
| [service/media/brookesia_service_audio/src/audio_playback_control.cpp:64–117](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/service/media/brookesia_service_audio/src/audio_playback_control.cpp#L64-L117) | 通过HAL播放器接口操作。 |
| [service/media/brookesia_service_audio/src/audio_playback_control.cpp:189–246](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/service/media/brookesia_service_audio/src/audio_playback_control.cpp#L189-L246) | 存储服务的音量/静音状态恢复，不是听书进度证明。 |
| [service/media/brookesia_service_audio/README_CN.md:5–13](https://github.com/espressif/esp-brookesia/blob/65d66c810ad706c9e2f82fe9177b11816acee546/service/media/brookesia_service_audio/README_CN.md#L5-L13) | provider/DataFlow服务定位。 |

## S14

**ESP32-audioI2S**；`schreibfaul1/ESP32-audioI2S@56f27918a8e6d10d56a19ddf0fdd307cdeee8cde`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [README.md:3–16](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/README.md#L3-L16) | 现声明S31/PSRAM与固定48k I2S，不能代表Plus已适配。 |
| [src/Audio.cpp:2490–2529](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/src/Audio.cpp#L2490-L2529) | M4A样本表及明确拒绝ALAC。 |
| [src/Audio.cpp:2562–2583](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/src/Audio.cpp#L2562-L2583) | chpl只报告数量与首个标题，然后跳过整块。 |
| [src/Audio.cpp:2650–2655](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/src/Audio.cpp#L2650-L2655) | chpl入口。 |
| [src/Audio.cpp:664–715](https://github.com/schreibfaul1/ESP32-audioI2S/blob/56f27918a8e6d10d56a19ddf0fdd307cdeee8cde/src/Audio.cpp#L664-L715) | HTTP Range。 |

## S15

**FFmpeg**；`FFmpeg/FFmpeg@02970f067cf06c5c23209d521575b1024898bb29`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [libavformat/mov.c:623–660](https://github.com/FFmpeg/FFmpeg/blob/02970f067cf06c5c23209d521575b1024898bb29/libavformat/mov.c#L623-L660) | Nero chpl逐章创建章节及100ns时间基。 |
| [libavformat/mov.c:10672–10748](https://github.com/FFmpeg/FFmpeg/blob/02970f067cf06c5c23209d521575b1024898bb29/libavformat/mov.c#L10672-L10748) | QuickTime章节轨读取，非同一种章节表示。 |

## S16

**retro-go/gnuboy**；`ducalex/retro-go@4ced120669750ca7228fd0414211430c1d923166`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [retro-core/components/gnuboy/gnuboy.c:211–249](https://github.com/ducalex/retro-go/blob/4ced120669750ca7228fd0414211430c1d923166/retro-core/components/gnuboy/gnuboy.c#L211-L249) | 加载ROM/卡带类型。 |
| [retro-core/components/gnuboy/gnuboy.c:539–581](https://github.com/ducalex/retro-go/blob/4ced120669750ca7228fd0414211430c1d923166/retro-core/components/gnuboy/gnuboy.c#L539-L581) | SRAM/RTC读取。 |
| [retro-core/components/gnuboy/gnuboy.c:903–912](https://github.com/ducalex/retro-go/blob/4ced120669750ca7228fd0414211430c1d923166/retro-core/components/gnuboy/gnuboy.c#L903-L912) | 即时存档/读档入口。 |
| [README.md:24–38](https://github.com/ducalex/retro-go/blob/4ced120669750ca7228fd0414211430c1d923166/README.md#L24-L38) | 菜单、多槽、最近/收藏、封面/预览等产品功能声明。 |

## S17

**Peanut-GB**；`deltabeard/Peanut-GB@d0bcca771c83a2638c93a9ae61f3d51f226dc905`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [peanut_gb.h:3862–3903](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/peanut_gb.h#L3862-L3903) | 核心初始化/逐帧执行接口。 |
| [peanut_gb.h:3937–3984](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/peanut_gb.h#L3937-L3984) | 逐行画面回调、卡带保存大小接口。 |
| [README.md:10–18](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/README.md#L10-L18) | 只支持DMG；明确精度/兼容性限制。 |
| [README.md:34–46](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/README.md#L34-L46) | 需外部APU及逐行绘制限制。 |

## S18

**Ardens**；`tiberiusbrown/Ardens@e811cc6be72c8a802bd3265a84daf4165c177b93`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [src/absim_arduboy.cpp:33–75](https://github.com/tiberiusbrown/Ardens/blob/e811cc6be72c8a802bd3265a84daf4165c177b93/src/absim_arduboy.cpp#L33-L75) | 实际FX内存/存档模型实现。 |
| [src/absim.hpp:916–932](https://github.com/tiberiusbrown/Ardens/blob/e811cc6be72c8a802bd3265a84daf4165c177b93/src/absim.hpp#L916-L932) | 声音及串口仿真状态。 |
| [README.md:3–13](https://github.com/tiberiusbrown/Ardens/blob/e811cc6be72c8a802bd3265a84daf4165c177b93/README.md#L3-L13) | 桌面/浏览器模拟器，不是MCU移植承诺。 |

## S19

**JavaScript-Snake**；`patorjk/JavaScript-Snake@3243086c0badb4b4c6bedfe6d95d5a52a7caaa88`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [src/js/snake.js:351–393](https://github.com/patorjk/JavaScript-Snake/blob/3243086c0badb4b4c6bedfe6d95d5a52a7caaa88/src/js/snake.js#L351-L393) | 方向/按键规则。 |
| [src/js/snake.js:399–475](https://github.com/patorjk/JavaScript-Snake/blob/3243086c0badb4b4c6bedfe6d95d5a52a7caaa88/src/js/snake.js#L399-L475) | 暂停、移动、碰撞。 |
| [src/js/snake.js:482–516](https://github.com/patorjk/JavaScript-Snake/blob/3243086c0badb4b4c6bedfe6d95d5a52a7caaa88/src/js/snake.js#L482-L516) | 进食增长。 |
| [src/js/snake.js:681–720](https://github.com/patorjk/JavaScript-Snake/blob/3243086c0badb4b4c6bedfe6d95d5a52a7caaa88/src/js/snake.js#L681-L720) | 放置食物。 |

## S20

**memory**；`taniarascia/memory@4c991ddb6b7db2f72945b09d9c8e82af6356482b`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [js/script.original.js:51–59](https://github.com/taniarascia/memory/blob/4c991ddb6b7db2f72945b09d9c8e82af6356482b/js/script.original.js#L51-L59) | 复制卡片/洗牌，原随机排序不直接沿用。 |
| [js/script.original.js:85–101](https://github.com/taniarascia/memory/blob/4c991ddb6b7db2f72945b09d9c8e82af6356482b/js/script.original.js#L85-L101) | 配对及翻回。 |
| [js/script.original.js:104–138](https://github.com/taniarascia/memory/blob/4c991ddb6b7db2f72945b09d9c8e82af6356482b/js/script.original.js#L104-L138) | 两次选择和延后翻回。 |

## S21

**公开旧crosslink（血缘排除检查）**；`DaisonChun/crosslink@107e172994ad2086c414e159795515175681be71`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [src/activities/boot_sleep/SleepActivity.cpp:30–66](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/src/activities/boot_sleep/SleepActivity.cpp#L30-L66) | 只有其自身睡眠图实现，不能证明就是Pixel/2社区修改。 |

## S22

**lunar-javascript补查**；`6tail/lunar-javascript@5c92e62c904ba5cc0b62f032796aeef433702268`。

| 文件与行号 | 本次核查事实 |
| --- | --- |
| [lunar.js:1459–1474](https://github.com/6tail/lunar-javascript/blob/5c92e62c904ba5cc0b62f032796aeef433702268/lunar.js#L1459-L1474) | 宜忌计算依赖月干支/日干支，而非只查12地支。 |
| [lunar.js:5150–5195](https://github.com/6tail/lunar-javascript/blob/5c92e62c904ba5cc0b62f032796aeef433702268/lunar.js#L5150-L5195) | 底层宜忌查表入口。 |

## web

公开清单：[匿名固件列表](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all)。按固件ID、板型和版本定位，不能用同名条目跨板补功能。下列短原文取自不同作者的条目/版本说明；摘要与响应哈希见[registry-evidence.json](registry-evidence.json)。它们是作者说明，不是运行证据。

| 号 | 对象／版本 | 短原文与定位 | 来源追查结果 |
| --- | --- | --- | --- |
| W01 | PageRest v0.7.103 | “把喜欢的句子留在锁屏上”；ID `71addc365d3f31a53ed5dacca7855582` | [官网](https://pagerest.gofree.kdns.fr)403；GitHub同名结果为无关WordPress项目；未见相关源码 |
| W02 | Pixel v4.3.0T、历史v4.2.0T/4.2.3T | “新增待办功能和待办锁屏”；S3 ID `4fea71b49b8bf6fa4a22dc1798423be4`；Plus ID `92c82de9caa4249c4270e76bb10290c8` | 历史备注分别载待办、阅读小票及16灰封面；未找到对应修改树，S21不是已证明上游 |
| W03 | Crosslink2 Plus 4.2.7；S3 4.2.9 | “增加png透明锁屏壁纸”；Plus ID `d932f06a5906b6a283c38c37d031a8f3` | [官网](https://www.crosslink-reader.top)DNS失败；版本说明能确认透明PNG，源码未定位 |
| W04 | 小智畅享版 v0.1.0 | “日历或壁纸”；ID `2c220821474d3b922f6aa2367037edc1` | 小智/原厂是候选来源，未取得对应修改；不能补认农历 |
| W05 | ModernClassicOS v0.1.1、v0.1.0 | “16 kHz 单声道 PCM”“最多三次”；ID `759b008fdff7a846affd925f928dee90` | v0.1.0格式/输出，v0.1.1 AAC-LC快跳/索引/章节；名称及ESP32组合检索无相关源码；ADF/audioI2S为替代来源，血缘未证 |
| W06 | Mogo v1.0.10 | “新增迷你书库组件”；ID `5580fff7f2e8b382283557fa37080944` | 作者明确基于[kiikoread Pico](https://github.com/wegooo-cell/read-pico-reader)，本次另找到S01；S01 README链接同一mogo官网。开发HEAD不等于发行版本 |
| W07 | lnkboot v1.0.6、历史v1.0.4-bev | “非官方多固件切换启动器”；ID `abaa819e9eec5e0c390ffb66cf63d7f8` | 同查lnkboot/inkboot；未见相关代码；一个inkboot同名结果无法与作者/板型相连 |
| W08 | 固件口袋 v1.0.8-plus.15 / 1.0.7-s3.20 | “单个应用镜像最大支持13 MiB”；Plus ID `d339eb28f76779495b909d1129ce994f` | 作者给出[源码目录](https://github.com/Changkai9888/CloudZao-Projects/tree/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/firmware-pocket)，S09已展开；未证明商城版本对应 |
| W09 | kiikoread Plus rc110 | “多种锁屏”；按JSON中同名Plus记录定位 | S05固定83942e，不能用rc110标签给旧树补功能 |
| W10 | BoBoOS v0.2.3.1；音乐v0.1.0 | 官网开发文档：“保留”应用数据（原文标识state.json）；市场：“按歌曲、专辑、歌手、文件夹浏览” | [首页](https://bobo.sh.creativone.cn/)、[开发文档](https://bobo.sh.creativone.cn/developer/docs)、[市场](https://bobo.sh.creativone.cn/apps)均HTTP200；有S3/S31原生ELF、MCP和旧SD插件说明，没有可用源码链接；`.crosspoint`仅线索 |
| W11 | 布丁 v0.1.0 test | “测试版哈，有bug”；ID `f4576587b25c738295eb196468c20307` | 作者列本地/网易云/网络电台/录音/电话短信，另注无4G不建议刷；未找到对应源码 |
| W12 | ThingOS v0.1.7 | “原 AI 按键快捷开始录音”；ID `b9909c6fbbb253f105cc65befbb89ee5` | 说明列蓝牙音乐/ima；同名ccrisan/thingos是另一Linux项目，不能当上游 |
| W13 | 开哥录音机 v0.1.0 | “支持锁屏录音”；ID `0946fcb2f2dddbf7021d56a65e3a4338` | 说明列AI键、TF保存和回放；S09未包含此App源码 |
| W14 | P4 gba_full v0.1.0(test)、retro-go-gba v0.1.0 | 前者说明dummy DAC能改善帧率；不是本次性能测试 | 后者只有简短名称/说明；[retro-go-tab5候选](https://github.com/andjiang0083/retro-go-tab5/tree/4e8f0f2f4e75b2102016c433b34ee1f23e17e471)沿用起步资料，二进制身份未证，不能当S31证据 |
| W15 | 开哥五子棋 S3 v0.0.2 / Plus v0.1.0 | “支持人机和双人对战” | 作者口袋目录没有棋App源码；仅用声明做标杆 |
| W16 | 开哥贪吃蛇 v0.1.0 | “一个适合一个人打发时光” | 未给控制/保存/暂停细节；S19为替代规则来源，未证明其血缘 |
| W17 | 黑猩猩测试 v0.1.0 | “音效、通关画面”；ID `2c1297cb3a6c846e7d1345b78039221e` | 说明有艺术化故事免责声明，不能当认知科学效果证据；无源码 |
| W18 | Eink4Boy 商城v0.1.0，正文自称0.3.18 | “不支持 GBC 专用游戏”；ID `b3f2d73a185865867bc5cea29caa18e0` | 正文承认残影仍调；未定位源码。S17/S16是可用核心候选，不证明原始血缘 |
| W19 | ardmos v0.1.0 | “提供 Moji 应用版与独立固件版”；ID `a743751c5a6ad3d2f2d6066a223770cd` | 相关名称搜索仅见无关[billieblaze/ardmos](https://github.com/billieblaze/ardmos)I²C项目，未当本固件；Ardens为替代核心 |
| W20 | 森林里的朋友/点读机 v0.1.0 | 森林：“3-8岁儿童”；点读：“开发示范使用” | 作者称可放口袋；S09不含这些App。没有把3～8岁宣传当完整课程或实测适龄结论 |

BoBo开发文档把两条通道分开：原生包称逐文件校验SHA-256并整体替换、保留应用状态；旧SD插件段明确称设备端不做校验。报告没有把两者合成“所有应用均安全”。本次没有登录、获取令牌、下载ELF或调用任何写接口。

本地对照只读：`system/apps/snake/SOURCE.md`、`system/apps/memory/SOURCE.md`及相应规则代码，证明基线已有移植；没有复跑旧报告测试。其它已有功能状态参考picks-status，不能当成本次实机验证。
