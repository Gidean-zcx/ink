# reuse-crosspoint-1 调研报告

查阅日期：2026-11-11。基线：`6bf4b0014b963558d747c2a4cc2a7bc3e2ca167c`。

**结论：有依据可行的是按模块复用，有条件可行的是把这些模块接成我们的完整系统。** 优先取原厂的 EPUB/图片/阅读位置处理、CrossMux 的统计和小游戏算法、LVGL 的键盘控件。保留我们的能力表、成员数据、文件准入、音频服务和刷新策略。不要整包换成某个固件。

本次为调研与规划，没有改产品代码、测试或构建文件，没有跑全量测试、ASan、S31 编译或产品 gcccheck，没有碰设备。文中的【实测】只指本次只读命令/文档检查，【实现】指源码核查，【文档】指上游说明，【估算】指移植建议和工期；没有把上游运行说明当成我们已上机。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 进行中：许可证已核，模块/71 项/输入法/队列正在整理 | 下面的固定版本、许可边界；最终见全部专题 | C1、C2 |
| 2 | 进行中：仅任务目录新增，最终扫描待执行 | 基线工作区干净，原厂克隆 HEAD 未移动 | C3 |

## 没做到的

- 按任务书，本次不运行全量测试、ASan、S31 编译或产品 gcccheck；不构建、不刷机。
- 未经本项目真机移植验证的模块均不标“在 Plus 上可用”。缺源码、缺许可、需要账号或非官方接口的条目逐项列在对照表。

## 和设计不一致的

- 本次无产品实现变更。后续不能照搬上游的整页黑白清屏、全局文件路径、NVS 账号、直接写服务状态和 Arduino 主循环；这些与 R-061、成员隔离、D-127、D-105 不符。适配范围列在模块表和 AI 接入表。

## 改过的已有测试

无。

## 要用户定的

- 微读、原厂 App/WorkBuddy、坚果云/数据胶囊、ima、网易云和电脑遥控是否接入对应第三方服务。勾选功能不等于同意上传家庭资料或接受无证书校验；账号授权由本人完成。
- 勾选 `lock_ticket` 的备注仍写“锁屏先不做”：保留该功能与队列位置，正式实现前核定时机；不擅自取消。
- 用户 ROM 的授权、老黄历宜忌的权威来源、M4A/M4B 章节来源仍有缺口；不拿不完整替代品声称完成。

## 顺手发现

- 清单中的“没有”不全是当前代码现状：已有农历换算、远程网页、文字选区、文件准入、升级校验等，详见后文。既有功能继续保留。
- CrossMux 固定树支持的是 **Metalio E-Ink4（S3）**，不是 E-Ink4 Plus（S31）；`platformio.ini:787` 起及 README 支持表没有 Plus/S31 目标。
- CrossPoint/CrossMux 的 WebDAV 是服务端；Calibre 入口复用 HTTP 传书配套插件，不是原生 Calibre 智能设备无线协议。

## 新增的机制

无。本次只调研、记录候选和拆任务，没有新增任务线程、状态机、文件格式、缓存或配置项。

## 要改的文档

由主会话合并时统一处理：`docs/tasks/backlog.md` 加后文任务队列并去重 `ui-input-2`；`docs/system/ui-dependencies.md` 修正可复用来源及已有功能；能力落地后生成 `docs/system/capabilities.md`；涉及第三方账户/老黄历/锁屏时机的选择再更新决策。上述共享文件本次均未改。

## 固定来源和许可证（先于功能筛选）

S01～S16 的固定提交与根许可证据沿用 [registry-research-1 的审计](../registry-research-1/source-audit.json)，本次补查子目录、文件头、调用点及依赖，未重新扫描固件清单。S17 起为本次补充。固定提交日期用于比较维护状况，不表示发布质量；S04、S12 的远端 HEAD 已前进，保留已审计版本，不跟随漂移。其余 S01～S18 的公开 HEAD 与固定提交相同。

| 来源 | 固定提交（点击源码）／提交日期 UTC | 根许可和文件级核实【实现】 | 第三方与排除边界【实现】 | 本项目取用条件 |
| --- | --- | --- | --- | --- |
| S01 原厂 Plus | [`874538eb54f7`](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)／2026-11-07 | MIT；`main/reader/*`、`screen_keyboard.cc` 无另一许可头，按根许可；JPEG 内嵌 stb 另保留 MIT/公有领域声明 | zlib 为 zlib 许可；ESP-IDF 为 Apache-2.0；不搬整个 emulator/LVGL、字体包或驱动依赖树 | 优先原文取阅读解析、书签、横竖排、WorkBuddy 协议；附根 MIT、来源提交和修改说明 |
| S02 原厂 E-Ink4 | [`d12f481f4a42`](https://github.com/CloudZao/Metalio-E-INK4/tree/d12f481f4a42420634bb56a928f3959a977367d4)／2026-10-13 | MIT；当前公开源码为 S3 板，不等于 Plus | USB HID port 是 Apache-2.0；自带 LVGL/字体等各自许可 | 可作功能参考；该固定树未找到阅读统计、WorkBuddy、BLE 中文键盘的新实现，不按官网版本号补想象 |
| S03 原厂 Claw4 | [`ca3aa3fa027f`](https://github.com/CloudZao/MetalioClaw4/tree/ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950)／2026-08-18 | MIT；P4 平台 | 广播使用 GMF HTTP/音频管线，不是 FM 射频；不搬 P4 板级代码 | 可摘网络电台的节目/播放控制，硬件与输出仍走我们的 HAL |
| S04 read-pico-reader | [`ea371a7d5998`](https://github.com/wegooo-cell/read-pico-reader/tree/ea371a7d5998880265aea09f9d44b26b619a8289)／2026-11-11 | 分范围；`main/book/book_epub.c`、`book_txt.c`、`ui_keyboard.c` 头均为 Apache-2.0 | `components/read_pico_search` 代码 Apache-2.0，pypinyin 数据 MIT；epdiy 是 LGPL-3.0，排除；Personal-Study 部分排除 | 只取有明确宽松许可的文件闭包；保留 Apache 全文、版权/NOTICE 和修改标记 |
| S05 Metalio-S31-kiikoread | [`83942eb45d13`](https://github.com/wegooo-cell/Metalio-S31-kiikoread/tree/83942eb45d13064a47aca7ff04c2faf8fd51f98a)／2026-10-30 | 同 S04；`zip_reader.c`、`ui_ime.c` 也明确 Apache-2.0 | ZIP 用 ROM miniz；宿主机须接独立 miniz/zlib；不能把 ROM 可用当作模拟器可用 | C 版 EPUB/TXT/输入法的备选；不复制受限内容与整套 framebuffer UI |
| S06 AgentUI | [`172ec530ff65`](https://github.com/AlayaElla/MetalioClaw4-AgentUI/tree/172ec530ff65e11521585fe3b8611daa20a78aa4)／2026-10-07 | 根 MIT；`components/codex_remote/*`、`external_apps/examples/radio/main/radio.cc` 无另行许可头 | cJSON/mbedTLS/ESP-IDF 各自许可；外部应用 SDK 与 P4 ELF ABI 不移植 | 可摘 WebSocket 包编解码与广播示例；保留原 MIT；不把配对凭据、终端执行权限交给 AI |
| S07 CloudZao-Projects | [`90150a24d1ed`](https://github.com/Changkai9888/CloudZao-Projects/tree/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8)／2026-11-04 | 根无许可证；`protocols/*` 不能据公开仓库直接复制；`plus-noto-font/` 有独立 MIT | 字体数据另为 OFL-1.1；FreeType 走 FTL，须保留字体保留名和各通知 | 只把字体工具列为可用来源；固件口袋、亮证和未公开小游戏不作为可移植代码 |
| S08 Metalio-WhiteAI | [`153d0967e97e`](https://github.com/446599/Metalio-WhiteAI/tree/153d0967e97e1cf308403252cfc2839d6d65fb08)／2026-09-23 | 根无许可证；输入编辑器没有独立许可头；`main/input/PINYIN_LICENSE` 仅授权其拼音数据 | 第三方字体/图标/esp-ml307 各有许可，不给原创 AI 层补授权 | 原创输入法/MCP 代码暂不取；MIT 拼音数据可用但我们已有更完整的授权词库 |
| S09 retro-go-tab5 | [`4e8f0f2f4e75`](https://github.com/andjiang0083/retro-go-tab5/tree/4e8f0f2f4e75b2102016c433b34ee1f23e17e471)／2026-10-22 | GPL-2.0；沿用前次固定提交 LICENSE 的哈希证据 | 本次树请求不完整；未继续下载源码，也不运行固件 | 只记录，全部排除；GB 另找 MIT 的 Peanut-GB |
| S10 xingzhi-ai | [`55dd6dbb01b2`](https://github.com/WMnologo/xingzhi-ai/tree/55dd6dbb01b2bb50cf09ce4a7faed3a0f2481260)／2025-11-26 | 没有根许可证；固定树有 207 个文件、C/C++ 源文件 0 个 | 有二进制/资源不等于有可复用源码；不下载运行固件 | 没有可复用来源，AI 多厂商切换不从此仓库取 |
| S11 kiikoread-xiugai | [`746ce316faf6`](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/tree/746ce316faf61ce5228eceee618eacb50a02a395)／2026-11-02 | 分范围；`main/music/music_player.c`、`music_tags.c`、`music_lyrics.c` 均 Apache-2.0 | 播放核心依赖 esp_audio_simple_dec、FreeRTOS、NVS、bt_host；解码器分发许可仍须单独锁定 | 可取音乐元数据与播放编排；不带入自己的任务/384 KiB 环形缓冲/NVS 格式；受限部分排除 |
| S12 dpReader | [`9ae58085c480`](https://github.com/dephia31/dpreader31/tree/9ae58085c4805a321dc741055effb53183d0b335)／2026-11-11 | 分范围；实际实现位于 `firmware/shared/main/`，顶层有转引文件；OPDS/EPUB/TXT 头均 Apache-2.0 | epdiy LGPL 不取；FreeType 选 FTL；KOReader 插件不排任务（用户没选） | 可取 C 版 OPDS 1.2 解析；保留原许可，不把全库称为非商用或 Apache |
| S13 TingQuS1 | [`89db82442e65`](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/tree/89db82442e655340e6194db89d4d6c9e64031d93)／2026-11-08 | 根 MIT；`main/reader/book_progress_sync.cc`、局域网同步界面在根许可范围 | `main/weread/*` 为非官方协议/逆向链路，本任务明确不用；KOReader 同步用户明确不要 | 只取局域网传输的非逆向基础及原厂相同模块；微读解码、加解密、抓包样本不入移植包 |
| S14 CodexRemote | [`5f9bcad783ea`](https://github.com/AlayaElla/CodexRemote/tree/5f9bcad783ea3274d01982e0f1826e6549d379ae)／2026-10-07 | 根无 LICENSE，`package.json` 也无 license 字段；原生驱动子目录有许可不授权 JS 主程序 | ws、adm-zip 等第三方许可不能覆盖原创桥接；原项目安装会改本机证书信任 | 当前不能复制桥接主程序；只记录协议/能力缺口，不安装、不连接真实电脑会话 |
| S15 CrossMux | [`a6fd569f7552`](https://github.com/0x1abin/crossmux/tree/a6fd569f7552db4ea1a6550ced7bc06568ae0e61)／2026-11-06 | 根原文为 MIT，并明确完整固件因 wolfSSL 采用 GPLv3；原创统计/小游戏等没有另行许可头 | `licenses/README.md`、`borrowed-code.txt` 区分字体、Expat/miniz MIT、uzlib、LGPL 字体结构；俄/瑞/乌断词数据 LPPL 不取 | 可以摘独立 MIT 模块；不搬固件、wolfSSL/wolfCrypt、LCP/DRM 和受影响的 ContentProtection 路径 |
| S16 CrossPoint develop | [`0333cbbe4649`](https://github.com/crosspoint-reader/crosspoint-reader/tree/0333cbbe4649f80afafe83061bf1afe9fd372fcf)／2026-11-11 | 根 MIT 的适用范围与 S15 相同，GitHub NOASSERTION 不能代替原文 | `platformio.ini` 引入 Arduino-wolfSSL 5.7.2；`Epub.cpp:451` 调 `openProtectedBook`，不能只搬整文件就声称已断开 GPL | 取独立 XML/CSS/位置等代码；EPUB 主线优先原厂，避免把加密/网络闭包一并搬入 |
| S17 CrossInk | [`9914146eeae7`](https://github.com/uxjulia/CrossInk/tree/9914146eeae7b46b300f475a16c32426fc02ec1f)／2026-10-04 | 根 MIT；书签、统计、主题文件无另行许可头 | Arduino/SdFat/uzlib、GfxRenderer；主题图标子模块和字体须另核，不复制资源包 | 可取书签锚点/统计计算，存储改接成员数据；不复用按设备 MAC 标记统计的实现 |
| S18 DaisonChun/crosslink | [`107e172994ad`](https://github.com/DaisonChun/crosslink/tree/107e172994ad2086c414e159795515175681be71)／2026-05-24 | 根 MIT，但 README 明确参考 allocated/crosspoint_chinesetype；不是中文增量已获 MIT 授权的证明 | open-x4-sdk 的 MIT 不能替其应用代码解套；参考源实际为 AGPL-3.0（见下） | 可核对与 MIT 上游相同的文件；来源不清的中文增量不取，等待逐文件历史/授权证明 |
| S19 FreeInk SDK（CrossPoint 锁定） | [`6a208c0c46cc`](https://github.com/Free-Ink/freeink-sdk/tree/6a208c0c46cce99bafa334bc4f171f5e558c5195)／2026-11-10 | 根 MIT；OPDS 原创文件无另一许可头 | Expat/ArduinoJson/JsonSax MIT；`OpdsImplicitAuth.cpp` 明示 wolfSSL 路径，不选隐式认证；FreeType、内容保护另算 | 适合 OPDS 解析与无隐式认证的客户端；保留 SDK MIT，HTTP 接现有安全网络层 |
| S20 FreeInk SDK（CrossMux 锁定） | [`d9a1304cd234`](https://github.com/0x1abin/freeink-sdk/tree/d9a1304cd234b0536136302f629304b609f62b32)／2026-11-03 | 根 MIT；`HidKeymap.cpp` 和 `BleKeyboardHost.cpp` 无另行许可头 | NimBLE-Arduino Apache-2.0、Preferences/FreeRTOS 强耦合；不搬 EpdiyLcd/epdiy | HID 键码解析可直接取；BLE 主机须接 IDF，不能当 Plus 驱动即插即用 |
| S21 open-x4 community-sdk | [`a64a3c29bebc`](https://github.com/open-x4-epaper/community-sdk/tree/a64a3c29bebc59b2ccdfe15492cfc4b5e4c26360)／2026-04-08 | 根 MIT | 主要是 EInkDisplay/InputManager/SDCardManager，只有 8 个 C/C++ 文件；无成熟中文输入引擎 | 仅作为 crosslink 的依赖说明；S31 驱动走原厂，不从 X4 驱动移植 |
| S22 Peanut-GB | [`d0bcca771c83`](https://github.com/deltabeard/Peanut-GB/tree/d0bcca771c83a2638c93a9ae61f3d51f226dc905)／2026-09-21 | 根没有单独 LICENSE；`peanut_gb.h:2` 完整 MIT 授权 | 纯 C 单头；ROM、可选音频实现各自许可；不包含任何游戏 ROM | 可移植 GB 核心，保留头部 MIT；只用自制/明确授权 ROM 验证 |
| S23 dr_libs | [`f12e197fd462`](https://github.com/mackron/dr_libs/tree/f12e197fd462e4c22873fbd70d11b1d2f2c1dc3c)／2026-11-11 | 根和 `dr_mp3.h`、`dr_flac.h`、`dr_wav.h` 给出公有领域或 MIT-0 选择 | 不含 AAC/M4A/M4B 章节解码器；本次默认分支头含未发布版本字样 | 锁定提交后选 MIT-0，保留来源；先做 S31 性能/预算验证，不把桌面性能当实机证据 |
| S24 stb（Vorbis） | [`4e9b368bfc07`](https://github.com/nothings/stb/tree/4e9b368bfc071e97206ea5c6adba68dd0cbf8897)／2026-11-07 | 根及 `stb_vorbis.c` 末尾允许 MIT 或公有领域，选择 MIT | Ogg Vorbis 与现有 Ogg Opus 是两种编码；不能用同一个解码器冒充 | 只取 Vorbis 解码器，不重复引入我们已有的 stb 图像代码 |

### 许可处置要点

1. **MIT 文件可以从 GPL 完整固件中单独抽取，前提是该文件本身及带入的依赖确实有宽松许可。** 不能通过删掉许可证文字、仅不编译某个符号或换个目录消除 GPL 依赖。本次不带 wolfSSL/wolfCrypt、LCP/DRM、ContentProtection，也不取 LGPL 绘图层；TLS 由我们的安全网络实现承接。此为源码筛选结论，落地仍要核对最终链接闭包。
2. CrossPoint/CrossMux 的 `licenses/borrowed-code.txt` 还涉及 EPDiy 字体结构、Unicode 数据、JSZip/pako 等；不复制它们的字体格式、整个网页包、断词数据。取 XML/CSS 解析时保留 Expat 和原项目的通知；图像使用已有解码服务或原厂文件各自声明。
3. kiikoread 系的 Apache 文件不是因为根许可“非商用”就全不可用；反过来，根里留有 Apache 全文也不意味着所有新增文件都是 Apache。仅取表中明确标注的文件，Personal-Study 与来源不清的增量全部排除。
4. allocated 的实际地址是 [Gitee `allocated/crosspoint_chinesetype`](https://gitee.com/allocated/crosspoint_chinesetype)，页面和 README 标 AGPL-3.0；GitHub 同系 [icannotttt/crosspoint-chinesetype](https://github.com/icannotttt/crosspoint-chinesetype) 也标 AGPL-3.0。**不复用其代码。** DaisonChun 的 README 引用了它，故不能仅凭 MIT 根文件就放行其中文增量。
5. 所有正式拷贝都带来源仓库、完整提交、原相对路径、SHA-256、原版权/许可全文；适配修改另记。不提交上游字体包、固件、ROM 或设计稿。本次只提交摘要和哈希，不提交任何上游源码。

### CrossLink 官网追到哪里

【实测】`www.crosslink-reader.top` 本次 DNS 失败；裸域 [crosslink-reader.top](https://crosslink-reader.top) 返回 200。首页脚本 `app/page-38d9c6ba693682cf.js` 明确包含 [Gitee crosslink 发布页](https://gitee.com/daixinchun/crosslink/releases) 与 [xteink-x3 发布页](https://gitee.com/daixinchun/xteink-x3/releases)。公开 Git refs 得到 crosslink HEAD `eaeecd22761dd2d2884cbdb02acd066d4bd71bc4`，但匿名浅取源码要求登录，已停止，没有尝试凭据。

【实现】DaisonChun/crosslink 的固定 README 链向同一官网，团队和名称相符，因而是公开旧版候选源码；其 GitHub HEAD 为 `107e172994ad2086c414e159795515175681be71`，与 Gitee HEAD 不同。**已找到官网指向的发布仓库，未取得其当前源码，也不能把 GitHub 旧分支当作注册表里的 CrossLink2／Pixel 4.7 当前版本。** `capsule`、坚果云、50pt 和当前高清漫画功能不能据此写成已有可搬实现。
