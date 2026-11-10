# 原厂功能与触感反馈审计

基线：`e802ae57a1d20f3ab1f2770e766b3d03274133a3`。查阅日期：2026-11-10。状态：实现与盘点已完成本轮执行，**未达全部验收**；未通过项和覆盖边界见下文。

证据标记：【实测】本次命令结果；【实现】源码核查；【估算】尚未实机验证的判断。原厂仅查阅允许的 `ink-hw/upstream/Metalio-E-INK4-Plus`，不接触设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 热区、实体键、长按/拖动、开关、三档、勿扰/孩子/锁屏 | 专项通过 | 【实测】首次进程 177 项、重启 16 项检查；真实模拟 P03 开/关及 HAL 参数；普通及 ASan/UBSan 专项均通过 | C1 |
| 1b 原有测试与全量 | 未通过 | 【实测】`99% tests passed, 3 tests failed out of 332`，702.47 秒，0 跳过；三项失败见验证结果 | C2、C7 |
| 1c 内存检查 | 没有内存诊断，但整套用例未全绿 | 【实测】`99% tests passed, 2 tests failed out of 332`，644.11 秒，0 跳过；无 ASan/UBSan 错误 | C3 |
| 1d S31 编译 | 通过 | 【实测】构建退出 0，0 条编译 warning/error；本次系统 C 单元均已交叉编译；未烧录 | C4 |
| 1e gcccheck | 通过 | 【实测】`7 compiled, 0 with findings`，没有跳过 | C5 |
| 2 原厂功能盘点 | 完成 | 【实现】覆盖任务书全部项；列原厂与本项目位置、缺口及独立建议；显式源码位置均已检查存在且行号有效 | C6 |
| 3 中英文截图 | 完成 | 【实测】振动开关和强度均在声音第二页，英文 Strength/Medium 完整可见；路径见下文 | C1 |

## 没做到的

- 尚未做实机验证；任务禁止接触设备。
- 【实测】`ai-speech-long` 的 barge 子用例：捕获已开始，但 `audio.now_playing` 尚为 voice，`tests/ai_speech_long/ai_speech_long.c:80` 的即时打断断言失败。用本任务五个 C 单元的基线对象重链后通过；关闭新增输入触感的临时对象也通过。因此是本次接入引出的回归，不能归为基线已知失败。按三次规则停止这一项：换共享定时器后仍失败；调整实体键反馈顺序后仍失败；尝试在编码前发布播放状态仍失败，已撤回无效的音频状态改动。没有修改断言、加等待或放宽预算。
- 热区表上限为 96，满表后不做 LVGL 晚到补震；不保证超出表容量的页面仍有全部控件触感。矩形热区不等于圆角逐像素命中，这是与原厂早震区域相同的证据边界。
- 【实现】当前只为最上层页面/浮层发布热区，没有合并非模态浮层外仍可点的下层控件；例如键盘（`system/ui/shell/keyboard.c:152`，level=-2）外的下层控件。专项覆盖了键和普通控件，没有把此情形报为通过。

## 触感规则与来源

【实现】原厂固定版本为 `690a023c66862af103a3187464373ee7c5efcd14`（MIT）。以下原厂路径均相对该仓库，数字为行号。本机副本是稀疏/按需取对象的检出；未落地的 UI 源码通过公开仓库同一提交下载到 `work/vendor-source/` 核对，未写入只读副本。

| 规则 | 原厂证据 | 本次做法 |
| --- | --- | --- |
| 默认开、默认弱，弱/中/强 = 35/50/70 ms | `main/haptic_feedback.cc:18`、`:23`、`:71` | 沿用 `audio.vibration`；新增 `audio.vibration_level` 枚举 0/1/2，使用同一设置存储 |
| 按下命中热区才早震，不抢 LVGL 锁 | `main/haptic_feedback.cc:295`、`:323`，热区上限在 `:29` | UI 统一发布裁剪后的可点区域；输入服务只读区域和设置，再调现有 HAL，不碰 LVGL，不等待重画或马达结束 |
| 一次按下只震一次 | `main/haptic_feedback.cc:219`、`:279` | 仅首次 DOWN 和实体键按下反馈；MOVE、UP 和长按定时事件不调用马达 |
| 静音、勿扰、孩子模式 | `main/haptic_feedback.cc:272` 只判断触感开关；没有这三种模式的过滤 | 触感与音量、勿扰无关，孩子沿用同一开关和强度，并可通过原有策略改设置；不改变通知和闹钟已有的勿扰判定 |
| 锁屏 | 原厂公共触感 API 没有锁屏判断；待机与本项目锁屏并非同一个界面机制 | 选择只让锁屏实际可操作控件和实体键震，空白及被挡住的下层控件不震；点空白不唤醒 |

区域在 UI 任务内读取，短暂持有输入服务已有的互斥锁复制数字坐标；锁内不调用 LVGL、绘图、I/O 或等待脉冲。两个 96 项坐标表放在 PSRAM。实体键反馈紧接现有边沿分发和提示音入口，包含被策略消费的音量和电源键。

## 和设计不一致的

- 【实现】没有照搬原厂按控件挂回调和晚到 `PRESSED` 的补震：统一在输入服务判定，避免 UI 重复反馈。原厂盖板短按在松手时震（`main/display/lv_adapter_epdiy_touch.cc:74`），本次按任务书明确要求提前到按下时震；锁屏语义按上表选择，不能声称复现了出厂固件的全部运行行为。
- 【实现】原厂开启振动时，设置页还会补一次确认震（`D/screen/settings_screen/settings_haptic_tab.cc:108`）。本次没有这次界面补震：输入反馈只发生在 DOWN，开关关闭时不震；人和 AI 改设置使用同一个 `settings.set`，不额外模拟一次按压。普通电源/音量的原厂 DOWN 路径未找到直接触感调用（`B/input/metalio_keys.cc`、`D/lv_adapter_epdiy.cc:527`），本次所有逻辑键按下统一反馈依据任务书。
- 【实现】强度作用于已有 `ink_audio_haptic()` 的所有调用，包括通知、闹钟；默认脉冲由原来的 HAL 默认 50 ms 改为任务要求的弱档 35 ms。

## 原厂功能与硬件盘点

分类口径：【实现】“已有”指本项目有对应完整的软件路径；“部分”表示只覆盖部分行为、只有驱动/模拟器，或 S31 端口仍是桩；“没有”指没有可用的对应功能。**三类均不代表在本项目真机上验收过。** 下表的建议独立于事实，均为【估算】：按一名熟悉本仓库的开发者估计，不含硬件采购、等待服务账号或完整实机验收，不自动升级为需求。

原厂目录缩写：`B` = `main/boards/metalio-e-ink4-plus/`，`C` = `main/boards/common/`，`D` = `main/display/`。可从[固定提交源码树](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/690a023c66862af103a3187464373ee7c5efcd14)定位。下表本项目路径相对本仓库；冒号后均为行号。

| 功能/用法 | 状态 | 原厂出处 | 本项目对应与缺口 | 建议（与事实分开） |
| --- | --- | --- | --- | --- |
| 点按、按键触感及三档强度 | 已有 | `main/haptic_feedback.cc:18`、`:295`；`D/lv_adapter_epdiy_touch.cc:74`、`:132`；`D/screen/vk_key_handler.cc:83` | 本任务的输入服务、音频设置和声音页；S31 已有非阻塞马达脉冲端口 | 已按本任务实现；上机核对实际触感 |
| 马达软 PWM/连续输出 | 部分 | `B/input/metalio_vibe.cc:16`、`:64`、`:81`、`:136` | `hal_haptic_pulse` 仅有时长，无占空比和连续输出接口；不等同于原厂底层 70%/90%/100% 占空比 | 本任务只要求 35/50/70 ms；PWM 另排 0.5～1 天，需用户决定是否做 |
| 冷暖前光混光 | 已有 | `B/periph/frontlight.cc:187`、`:198` | `system/services/display/light.c:13` 将亮度与暖度分配到两路；`ports/esp32s31/s31_board.c:117` 输出 LEDC | 已有路径，亮度和混光曲线待真机验收 |
| 前光亮度渐变 | 没有 | `B/periph/frontlight.cc:24`、`:169`、`:233`（每 5 ms 步进） | `system/services/display/light.c:17`、`ports/esp32s31/s31_board.c:117` 直接改占空比，没有过渡 | 建议沿用已有共享定时器补渐变，约 0.5～1 天；需用户选变化速度，不能擅定参数 |
| BH1098 模式 1（原厂百问通话/本机 AI 音频） | 部分 | `B/periph/metalio_audio.cc:94`、`:110`、`:120` 明确等模式应答 | `drivers/chips/bh1098g/bh1098g.c:18`；`ports/sim/hal/hal_audio_sim.c:325`；S31 `hal_stubs.c:41` 音频仍不可用 | 属于现有音频端口补齐，约 3～5 天；上机前还需核对 ACK、共享供电和 I2S 时序 |
| 蓝牙耳机模式 2、通话路由 | 部分 | `D/screen/bluetooth_screen/bluetooth_uart.cc:253`、`:313` | `system/services/bt/bt_service.c:176`、`audio_service.c` 路由及 `bh1098g.c:41` 已有；S31 `hal_stubs.c:51` 仍返回不可用 | 建议随音频端口补，约 2～4 天；不扩展到已被用户否决的蓝牙音箱/翻页器 |
| 耳机扫描、连接/断开检测 | 部分 | `D/screen/bluetooth_screen/bluetooth_uart.cc:154`、`:185`、`:191`、`:197` | `system/services/bt/bt_service.c:80` 与模拟耳机事件；实际 UART 回报接到 S31 事件尚缺 | 建议随上一项补，约 1～2 天；这是蓝牙连接检测，未找到独立有线耳机检测引脚的依据 |
| 模式 3：设备接收蓝牙音乐、当音箱 | 部分 | `D/screen/bluetooth_screen/bluetooth_uart.cc:261`；原厂 `README_zn.md:570` | `bh1098g.c:20` 有模式 3 命令，但没有可用系统入口、接收音频路径或 S31 实现 | 不在本任务实现；D-120 已排除蓝牙音箱/翻页器，但是否也排除“设备当音箱”未明确；需用户定，约 2～4 天 |
| TF 卡挂载和按文件访问 | 部分 | `B/periph/metalio_sd.cc:73`；原厂板初始化 `B/metalio_e_ink4_plus_board.cc:80` | `system/services/file/file_service.c`、`ports/sim/hal/hal_storage_sim.c`；S31 存储仍为桩（`ports/esp32s31/hal_stubs.c`） | 建议按既有端口计划补 SDMMC，约 2～4 天，避免上层“有文件系统”被当成真卡已可用 |
| TF 卡借给电脑当 U 盘（MSC） | 部分 | `C/storage/usb_virtual_disk.cc:469`、`:837` 的 MSC 存储、挂载切换；接线见原厂 `README_zn.md:232` | `system/services/file/file_service.c:1172`、`file_caps.c:505`、模拟 USB；S31 `hal_stubs.c:179` 返回失败 | 已有成人确认和借出互斥；补实际 USB/SD 所有权切换约 3～5 天，涉及整卡暴露的范围须保持 D-72 规则 |
| DVP 摄像头 GC2145 | 部分 | `C/camera/dvp_camera.cc:92`、`:125`、`:238`；`B/config.h:73` | `simulator/models/dev_camera.c:122` 模拟 DVP 帧和 SCCB，`system/services/camera/camera_service.c:37` 与相机应用已有；S31 `hal_stubs.c:60` 未实现 | 建议优先补板载 DVP，约 3～5 天；先核对实际摄像头装配与共享 RST，不把电脑摄像头测试当作板载验证 |
| 外接 USB UVC 摄像头 | 没有 | `C/camera/usb_uvc_still.cc:23`、`:52`，以及 `uvc_still_camera.cc` | 现有相机 HAL 不提供 UVC 主机/热插拔选路；S31 相机为桩；宿主机取图不是 UVC 端口 | 如需外接相机再做，约 3～5 天；需要用户决定，采购/插接不在本任务内 |
| 待机/锁屏壁纸选择 | 部分 | `D/screen/wallpaper_screen/wallpaper_list.cc:53`；`wallpaper_active.cc:28`；`D/screen/standby_screen/standby_wallpaper.cc:80` | `system/ui/shell/settings_family.c:257` 从相册选；`wallpaper.c:33` 异步加载、成员隔离；没有原厂九宫格壁纸库与旋转/镜像编辑整套界面 | 基本用途已有；若要独立图库和编辑约 2～4 天，需用户选是否补齐交互 |
| 自选关机图片 | 没有 | `D/screen/wallpaper_screen/wallpaper_active.cc:322`、`:350`、`:378` 支持独立 shutdown 选图 | `system/ui/shell/off_picture.c:53` 固定“已关机”文字和书签，未读取壁纸 | 建议先保留明确关机状态，再决定是否允许用户图，约 0.5～1 天；需用户定 |
| 设备端 TTF/OTF 字体导入、转换 | 没有 | `D/font/ttf_convert.cc:101`、`:316`、`:540`；原厂 `README_zn.md:464` | `tools/fontgen/fontgen.py` 在电脑生成字体；`system/text/ink_font.c:9` 读取项目字体格式，无设备端 TTF/OTF 转换；不与电脑预处理混淆 | 若要任意字体，可先做电脑预处理，约 1～2 天；设备端约 4～7 天且需核 RAM/许可，需用户选方案 |
| APP 云推送书籍/壁纸/字体、下载回执 | 部分 | `D/screen/cloud_screen/cloud_fetch.cc:90`；原厂 `README_zn.md:504`、`:507`、`:509` | 自建中枢备份恢复 `system/services/backup/backup_service.c:1628`、无线传文件 `file/transfer.c:220` 已有；没有兼容原厂云端列表/资源分类/回执的接入 | 建议使用自建中枢协议补资源投递，约 3～5 天；是否兼容原厂服务器和账号需用户决定，不调用厂商接口 |
| 本地 AP/Wi-Fi 传文件 | 部分 | 原厂 `README_zn.md:504`；`D/screen/cloud_screen/ap/ap_transfer.cc` | `system/services/file/transfer.c:90`、`:220` 一次授权 HTTP 上传；S31 `hal_stubs.c:166` 的传输监听为桩 | 已有机制可复用；随 Wi-Fi/传输端口补齐，不新增独立传输框架 |
| 运行日志上传 | 没有 | `D/screen/settings_screen/settings_log_upload.cc:64`、`:377` | `system/ui/shell/settings_family.c:224` 明说暂读不到运行记录；无可用运行日志上传能力 | 建议先本地脱敏导出，约 1～2 天；上传对象、内容和授权需用户定，上传端约另 2～3 天 |
| 电量、电压、电流、充电状态 | 部分 | `C/power/bq27220_gauge.cc:160`、`:183`、`:231`，原厂用电压平滑估算显示电量 | `drivers/chips/bq27220/bq27220.c:14` 读芯片 SOC；`ports/esp32s31/s31_board.c:82`，`power_service.c:153` 和电量页已接；算法与原厂不同，未做实机标定 | 先比较原始 SOC 与电压估算，不自动替换；标定约 1～2 天，取决于完整充放电样本 |
| 可选充电电流 | 部分 | 原厂 `README_zn.md:521`；`C/power/cx25601n.cc:322` 初始化 | `drivers/chips/cx25601n/cx25601n.c:20` 可设，`s31_board.c` 固定 1 A；没有用户可调设置 | 需用户决定是否开放，约 0.5～1 天；数值应沿原厂并核电池标签 |
| 充电再充 kick、终止控制 | 没有 | `C/power/cx25601n.cc:135`、`:201`、`:206`，低于目标且空闲/电流小才 kick | `drivers/chips/cx25601n/cx25601n.c:30` 只有充电使能，无原厂 EN_TERM/再充监控 | 建议列端口优先补齐项，约 1～2 天；不先猜阈值或在本任务改充电行为 |
| VREG 设置、周期核对 | 没有 | `C/power/cx25601n.cc:162`、`:177`、`:322`、`:601` | 本项目 CX 驱动没有 VREG 读写，S31 初始化未覆盖原厂整套参数 | 建议与再充一起补，约 1～2 天；原厂 4400 mV 是源码值，不能直接当成已核实的本机电池额定值，需上机前确认 |
| 欠压时切 BATFET 断电 | 部分 | `C/power/cx25601n.cc:186`、`:190`（无输入、有效电压≤3100 mV） | `cx25601n.c:41` 有 ship/BATFET 写寄存器，但电源策略 `power_service.c:159` 按 SOC≤2% 关机，`hal_power_s31.c:187` 走板级电源脉冲；无独立电压保护路径 | 建议补原厂保护条件和顺序，约 1 天；不能将 SOC 关机视为相同保护，电池/输入条件需上机核对 |
| 4G UART 帧头、校验、重组和以太网通道 | 没有 | `C/board/uart_eth_modem.h:196`、`:207`；`.cc:738`、`:978` | `drivers/chips/nt26/nt26.c` 为 AT/短信 PDU/电话层；`ports/sim/hal/hal_cellular_sim.c` 为 AT 环境；S31 UART/蜂窝为桩，未实现四字节帧层和 UHCI/MRDY/SRDY 传输 | 属必需的端口缺口，建议先做固定帧协议，约 3～5 天；不把裸 AT 测试当作真机可通信 |
| SC7A20H 自动旋转跟踪 | 部分 | `B/periph/metalio_accel.cc:48`、`:124`、`:136`；原厂 `README_zn.md:473` | `drivers/chips/sc7a20h`、S31 采样、`power_service.c:207` 拿起和静止省电已有；`ui_task.c` 仅启动时读固定屏幕方向，无阅读自动旋转跟踪 | 若补阅读旋转约 2～4 天；需用户决定，拿起亮屏不等于自动旋转 |
| 触摸盖板虚拟键 | 已有 | `B/config.h:103`；`D/lv_adapter_epdiy_touch.cc:55`、`:82` | `boards/metalio_plus/metalio_touch.c:4`，S31 输入及模拟 FT 路径都转为 HOME/PREV/NEXT；本次纳入触感 | 沿用现有映射，上机再核触点和去抖 |
| 工厂/老化/触摸等自检页面 | 部分 | `D/screen/settings_screen/settings_test/settings_test_tab.cc:20`、`:31`；原厂 `README_zn.md:526` | `tools/hw-selftest/main/selftest.c`、`ports/esp32s31/main/main.c:8` 有编译自检；没有面向用户的工厂/老化控制页 | 如需设备内自检页，约 2～3 天；是否开放给日常用户及 AI 需用户定，不能把工具固件当成系统页面 |
| 电池测试页面 | 部分 | `D/screen/settings_screen/settings_test/settings_test_battery_screen.cc:18`、`:30`、`:41`，含 VREG/充电寄存器与读数核对 | `system/ui/shell/power_pages.c:115` 是日常电量页；自检工具有电流测量，没有原厂那套寄存器诊断页面 | 建议与只读硬件诊断一起做，约 1～2 天；需用户决定页面范围 |
| 系统监控：内存、水位、连续空闲块、任务 | 部分 | `B/periph/metalio_sys_mon.cc:67`、`:81` | 模拟器 `ports/sim/osal/osal_sim.c`、`simulator/runtime/metrics.c` 有预算数据；S31 `hal_stubs.c:182` 的 probe 为空，仅 `:88` 输出日志 | 建议先接只读诊断快照和低频采样，约 1～2 天；频率/保存范围需用户定，不新增常驻任务作为默认做法 |
| SD 循环运行日志/崩溃转存 | 没有 | `C/storage/sd_log_writer.cc:25`、`:41`、`:85`，4 KiB 环形、256 KiB 文件；后段读取 coredump | S31 只有 `hal_log_write`，运行记录页仍占位；无同等 SD 捕获、轮转和崩溃转存 | 建议先做本地有界脱敏日志，约 2～3 天；保存内容、保留期、AI 可读范围需用户定 |

额外按原厂首页和设置目录核对的功能（原厂说明与代码证据分开）：

| 功能组 | 状态 | 原厂出处 | 本项目对应与建议 |
| --- | --- | --- | --- |
| 阅读/书库、目录、书签、听读 | 部分 | 原厂说明 `README_zn.md:434`、`:443`、`:464`；构建入口 `main/CMakeLists.txt:64`、`:89` | `apps/reader/reader.c:236`、`:423` 有阅读和书库；不宣称支持原厂 `.ebook`、漫画、任意字体和全部布局选项；完整格式对照另需约 2～4 天，是否兼容原厂格式需用户定 |
| 语音 AI、设备工具调用 | 部分 | 原厂说明 `README_zn.md:414`、`:433`；代码 `main/mcp_server.cc` | `system/services/ai/ai_service.c`、`system/policy/cap.c` 和自建中枢已有共用能力；协议不是原厂 MCP/语音服务的兼容实现，S31 音频/网络未就绪；沿既有设计补端口 |
| 每日清单 | 部分 | 原厂说明 `README_zn.md:432`，`main/CMakeLists.txt` 的页面源注册 | `system/services/time/time_caps.c:347` 的提醒有本地功能，原厂云清单同步未接；兼容约 2～4 天，需用户选服务，不调用原厂云 |
| WorkBuddy 电脑助手绑定、消息 | 没有 | 原厂说明 `README_zn.md:426`、`:428`；端点定义 `main/cloudzao_endpoints.c` | 现有家庭中枢/远端能力机制不能视为 WorkBuddy 兼容；若要兼容约 4～7 天并依赖账号和服务端，需用户定 |
| OTA 与卡刷 | 部分 | `main/ota.cc`；原厂说明 `README_zn.md:344` | `system/services/update/update_service.c` 有签名校验、下载和安装流程；S31 OTA HAL 仍是桩；建议按现有 R-062 计划补端口，约 2～4 天 |
| 语言、主题、关于 | 部分 | 原厂说明 `README_zn.md:518`、`:519`、`:527` | `system/text/i18n.c`、`system/ui/theme.c`、`system/ui/shell/settings_pages.c`；中英和设备详情已有，未实现原厂五主题；主题兼容需用户选，约 3～5 天 |
| 电源档位、待机/关机、共享电源 | 部分 | `C/power_policy/README.md:25`、`:68`；`power_hw.cc`；S31 补丁目录 | `system/services/power/power_service.c`、`ports/esp32s31/hal_power_s31.c` 已有固定调频、浅睡补丁和板级顺序；不等于 SD/网络/音频等所有实际端口已完成；延续现有上机核查清单 |

### 硬件有、双方运行期都没用上的能力

| 能力 | 状态与证据 | 本项目位置 | 建议（估算） |
| --- | --- | --- | --- |
| BQ27220 温度、剩余/满充容量、健康度等遥测 | 没有运行期用途。[TI 官方产品说明](https://www.ti.com/product/BQ27220)列这些能力；原厂 `C/power/bq27220_gauge.cc:11`、`:160` 仅采电压/电流并算显示电量 | `drivers/chips/bq27220/bq27220.h:14` 定义了多个命令，但 `bq27220.c:14` 和 S31 只采 mv/ma/SOC，未向页面/AI 提供这些遥测 | 建议先加只读诊断，约 1～2 天；需用户定。健康度与剩余时间是否可信仍依赖电池模型标定，不能仅凭寄存器存在承诺准确 |
| PCF8563 硬件倒计时 | 没有。[NXP 官方说明](https://www.nxp.com/products/PCF8563)确认 timer；原厂 `C/sensor/pcf8563.cc:190` 仅清 TF/AF，没有设置倒计时寄存器 | `drivers/chips/pcf8563/pcf8563.c` 仅日历/分钟闹钟；系统倒计时运行在主控，不等于用了 RTC timer | 需要短周期低功耗唤醒时再评估，约 1～2 天；需用户定，并先核 RTC_INT/EXT1 共享线的实际状态 |
| PCF8563 CLKOUT 时钟输出 | 芯片有可编程输出（同上 NXP 说明）；双方驱动未配置 CLKOUT。**板上是否引出/接到可用目标信息不足** | 两边 RTC 头文件均无 CLKOUT 接口，原厂 `B/config.h:86` 只标 INT 接线 | 暂不实现；核原理图/接线约半天，若需改硬件必须用户明确选择，不能当成现成可用 GPIO |

充电芯片 OTG（本项目：没有接入）不列为“双方均没有实现”：原厂 `C/power/cx25601n.cc:458` 已实现 OTG 使能。本项目未接入，属于原厂有实现、本项目没有接入的软件能力；若要对外供电还须核板级电源选路，由用户决定，约 1～3 天。

### 架构与 AI 开放性（R-061）

| 对象 | 分类/边界 | AI 和按钮是否走同一路径 |
| --- | --- | --- |
| 振动开关、强度 | 现有设置表；音频服务拥有参数 | 是：`settings.get`/`settings.set`；页面绑定相同能力，有状态值和 35/50/70 ms 映射的中英快照；沿用儿童策略、持久化和撤销 |
| 按下识别、可点区域、脉冲结束 | 输入/平台内部机制；UI 仅提供坐标，不从输入线程调用 LVGL | 不单独开放任意硬件写入能力；用户操作对应的设置已开放，无额外私有 AI 开关 |
| 盘点中未补齐的能力 | 相机/音频/USB 等由服务能力调用 HAL；空 HAL 不算完成 | 已有上层能力不能掩盖端口缺口；后续补遥测、日志、图库和旋转时必须同步提供原子能力、结构化快照和事件，原厂直接 UI→硬件写入不应照搬成旁路 |

### 上机先核对清单

- 脉冲时长已按原厂固定值；实际力度还受原厂底层软 PWM 和本项目全电平驱动差异影响，35 ms 不等于已经保证相同触感。
- 冷暖光极性/混光曲线、音频 ACK 与模式切换、SD/MSC 借出与恢复、DVP 实际装配、NT26 帧和握手，都只核查代码，没有连接设备。
- 原厂 VREG=4400 mV、无输入欠压阈值=3100 mV 是源码事实；电池额定值、读数单位和本板实况未核，不在本任务写入这些参数。
- BQ 扩展遥测可信度和 RTC CLKOUT 接线未知；不据此开启新硬件行为。

## 改过的已有测试

- 无。只新增 `vendor-haptic` 用例和手动反向模式；原有断言、预算、超时和测试期望未改。

## 要用户定的

- 后续范围按盘点表选择：前光渐变、自动旋转、关机选图/独立图库、字体转换、WorkBuddy/原厂云兼容、日志上传、设备当音箱。建议先补现有端口和充电机制，再考虑这些扩展；本轮均未实现或自行否定。
- 字体建议先选电脑预处理，另一个方案是设备端转换；日志建议先选本地脱敏导出，另一个方案是确认范围后上传自建中枢。电池参数和可能涉及新增外设/改线的事项须先明确核对，未擅自写参数或采购。

## 顺手发现

- 原厂 `C/sensor/pcf8563.h:45` 仍描述 INT 经 MOSFET 到 GPIO4，而当前板型 `B/config.h:86` 明确为 P16；本次按当前板型和实际电源路径核对，没有照旧注释猜接线。

## 新增的机制

- 热区表：任务书第一部分明确要求从输入服务判断可点控件；复用已有输入锁，未新增线程、定时器或队列。
- 设置 `audio.vibration_level`：任务书明确要求三档强度；沿用 `settings.get`、`settings.set`、持久化和撤销机制。
- 模拟器马达改用已有共享定时器：替换原有私有任务和队列，依据原厂 `B/input/metalio_vibe.cc:100` 的定时器方式；新增输入触感后原任务使语音场景降到 30688 字节内部空闲，不能保留或抬预算。

## 要改的文档

- `docs/system/capabilities.md`：由主会话重新生成；新强度设置必须纳入设置表。此共享文件按总规则第 7 节不改，`capability-table-doc` 不能据此写成通过。
- `docs/system/ui-dependencies.md`：补声音页强度设置与本表所列实际端口缺口。
- `docs/tasks/backlog.md`：排原厂充电机制、NT26 帧、音频/USB/相机端口、运行诊断等缺口，并单列上述长语音回归；本报告的建议不自动变成需求。

## 规模与验证说明

【实现】相对指定基线，产品代码按增删合计 **137 行**（C/H 133，构建接线 4）；翻译另 15 行，即使计入也共 152 行，低于 200 行。测试和文档另计。为什么不能更小：单独在 LVGL 点击回调震动不能满足按下即反馈；逐控件手工接线又会漏控件。最终集中发布一个有界区域表，复用输入服务、音频设置、平台脉冲和共享定时器，未建立新服务层。看过的更小方案是沿用旧模拟马达任务，但首次按键使两项原有内存预算失败，故移除该任务和队列。

【实测】移除触摸反馈、实体键反馈、开关过滤或强档 70 ms 的四个临时对象均触发对应原断言。触摸 28 项失败、按键 11 项失败、开关 1 项失败、强度 2 项失败，命令和链接均成功。按键反向对照中因等待不存在的脉冲而延长按住电源键，后续流程未到 `test.finished`；已确认直接失败原因，不将它算成完整场景跑完。工作区产品源码未回退。

【实测】启动恢复页回归由热区失效入口额外获得输入锁引出；改成原子清空、仅在真实刷新后发布表后，原有 `boot-flow` 两模式 173/181 项及全部截图核查通过。保留失败前日志和基线对象对照，不修改原测试。

### 最终验证结果

- 普通全量原话：`99% tests passed, 3 tests failed out of 332`；失败为 `capability-table-doc`、`hub-simulator-e2e`、`ai-speech-long`。日志：`work/host-tests-final.log`。中枢项是 `tests/hub/e2e.py:53` 的播放至少 300 ms 断言；相同命令单独复测原话 `100% tests passed out of 1`，17.70 秒（`work/hub-recheck.log`）。不能用这次复测把完整一轮记成全通过，也未确认波动根因。
- ASan/UBSan 全量原话：`99% tests passed, 2 tests failed out of 332`；失败仅为上述文档和长语音项，中枢项本轮通过。日志：`work/asan-tests.log`。两个全量均无跳过；没有 `ERROR: AddressSanitizer`、`UndefinedBehaviorSanitizer:` 或 `runtime error:` 诊断。
- 本次五个产品 C 单元及两个测试 C 单元的 ASan 编译命令均有 `-fsanitize=address,undefined`；运行时使用 `ASAN_OPTIONS=halt_on_error=1`、`UBSAN_OPTIONS=halt_on_error=1`。原有 `unit-s31-osal-ticks` 在根 CMake 设置 sanitizer 前定义，未插桩；未越界修改它，不将其单独称为已做内存插桩检查。
- S31 构建退出 0；`work/s31-build.log` 无编译 `warning:`/`error:`。SDK 仍输出 Kconfig 十六进制写法弃用 NOTE，没有关闭检查。系统编译命令确认 `INK_PSRAM_BSS=EXT_RAM_BSS_ATTR` 生效。只编译、链接和生成体积/栈报告，没有连接设备。
- gcccheck 原话：`gcccheck: 7 compiled, 0 with findings`，无跳过，日志 `work/gcccheck.log`。
- 设置目录在 `work/capabilities.md` 临时生成并校验通过：`PASS: 513 definitions`（434 能力/事件、30 设置、49 状态），含 `audio.vibration_level`。正式共享文件保持不变，故文档同步验收仍失败；不是通过临时副本规避原有断言。
- 长语音和启动恢复的基线对象对照：`work/check-baseline.py`、`work/baseline-tests.log`。仅将本任务五个 C 实现单元换成指定基线的对象后重链同一组测试，并非宣称完整基线构建全绿；两个相关用例均通过。手动模式依赖本地基线历史，未注册进 CTest。
- 反向对照最终日志：`work/haptic-reverse-final.log`；专项截图和指标：`build/test-out/vendor-haptic/fixed/`。私有马达任务移除后，原语音场景最小内部空闲由 30688 提升到 33128 字节，原 32768 字节预算不变。

## 截图路径

构建产物、日志与截图不提交。保留证据时使用 `INK_KEEP_EVIDENCE=1`。

- 中文：[声音第二页](../../../build/test-out/vendor-haptic/fixed/first/clean-vibration-zh.png)。
- 英文：[声音第二页](../../../build/test-out/vendor-haptic/fixed/first/clean-vibration-en.png)。

## 复现命令

按总规则第 5 节提供的字体、Python、LVGL 参数配置主构建；另加 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`、`-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`。ASan 目录参数一致，仅加 `-DINK_SANITIZE=ON`；两套构建都使用 `taskpolicy -c utility cmake --build <目录> -j2`。

```bash
# C1：专项、进程重启、截图；反向对照为手动模式，不依赖 CI 提交历史
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/vendor_haptic.py --build build
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/vendor_haptic.py --build build --reverse
# C2 / C3：两套完整测试，包含原有预算和确定性检查
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# C4：仅编译
taskpolicy -c utility bash ports/esp32s31/build.sh
# C5：真实 GCC 编译检查
taskpolicy -c utility python3 ../../tools/gcccheck.py build system/services/input/input_service.c system/services/audio/audio_service.c system/ui/ui_task.c system/ui/shell/navigation.c ports/sim/hal/hal_power_sim.c tests/vendor_haptic.c tests/vendor_haptic_hal.c
# C6：只读盘点入口；稀疏检出缺少的文件从固定提交公开源码读取
INK_VENDOR="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"
rg -n 'Haptic|PulseMs|kPulseMs' "$INK_VENDOR/main/haptic_feedback.cc"
git -C "$INK_VENDOR" ls-tree HEAD main/display
# C7：中枢端到端复测与共享文档临时副本检查，不修改正式共享文档
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j1 -R '^hub-simulator-e2e$'
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/capabilities.md
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/capabilities.md --check
```

公开下载的 17 个源码文件与固定提交 Git tree 的 blob SHA-1 全部相符（按 Git blob 头计算，未仅比较模型自报的版本）。日志与临时源码在 `work/vendor-source/`。交互式决策板本次通过工具不可访问；没有推测用户选择，也没有修改任务禁止的决策文件。

## 提交检查

【实测】对照基线共 12 个相关文件；产品代码 137 行，翻译 15 行。两个截图本地链接和 80 处显式源码行号检查通过，记录在 `work/final-checks.log`；`git diff --check` 通过。按总规则第 6 节隐私表达式扫描全部差异，0 命中；没有提交绝对路径、真实识别号、构建产物、日志或设计稿。共享需求、决策、能力目录及既有调研/审计文件未改。只做本地提交，未 push、未开 PR。

## 里程碑

- 已核对工作树干净、当前分支独立，HEAD 与指定基线一致。
- 【实测】新增专项首轮修正了设备档案中不存在的返回键测试；修正后首次进程 168 项检查通过、重启 15 项通过。截图视觉检查后缩短英文强度标题，最终新增覆盖下首次进程 177 项、重启 16 项通过。
- 普通、ASan/UBSan、S31 与 gcccheck 均已跑完；保留未通过结论，未推送。
