# reuse-crosspoint-1 调研报告

查阅日期：2026-11-11。基线：`6bf4b0014b963558d747c2a4cc2a7bc3e2ca167c`。

**结论：有依据可行的是按模块复用，有条件可行的是把这些模块接成我们的完整系统。** 优先取原厂的 EPUB/图片/阅读位置处理、CrossMux 的统计和小游戏算法、LVGL 的键盘控件。保留我们的能力表、成员数据、文件准入、音频服务和刷新策略。不要整包换成某个固件。

本次为调研与规划，没有改产品代码、测试或构建文件，没有跑全量测试、ASan、S31 编译或产品 gcccheck，没有碰设备。文中的【实测】只指本次只读命令/文档检查，【实现】指源码核查，【文档】指上游说明，【估算】指移植建议和工期；没有把上游运行说明当成我们已上机。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 进行中：许可证已核，模块/选择对照/输入法/队列正在整理 | 下面的固定版本、许可边界；最终见全部专题 | C1、C2 |
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

- 【实测】`picks.md` 标题写 71 项，实际为 72 行、72 个唯一 ID。本次覆盖全部 72 个选择；原文件不改。

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

## 模块清单与移植接缝

以下行数是列出文件的实际物理行数（含注释/空行），不是移植要新写的行数。未全文下载的项明确标树中字节数。原项目路径保持不变，点链接即固定版本；完整取回哈希见 `source-evidence.json`。依赖/耦合指把代码接入我们的 C＋LVGL／ESP-IDF／宿主机模拟器时的阻力，不是对源项目质量评分。

统一接法：普通 C++ 解析器可留原文并给 C 调用口，不为“全 C”重写协议。仅文件、内存、时间、日志、渲染接我们的现有接口；模拟任务中的主机操作仍走既有 host-call 规则。产品 UI、线程调度、持久化和原子能力由 ink 负责。仅有原厂同硬件依据的底层仍归现行端口任务，不能由上层复用任务改驱动。

| 项目／模块 | 实现文件与规模【实测】 | 依赖和硬件耦合【实现】 | 可复用内容、接入工作与限制【实现／估算】 |
| --- | --- | --- | --- |
| S16 EPUB、CSS、目录 | [lib/Epub/Epub.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Epub/Epub.cpp)（1327 行）<br>[lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp)（3284 行）<br>[lib/Epub/Epub/css/CssParser.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Epub/Epub/css/CssParser.cpp)（1842 行）<br>[lib/Epub/Epub/parsers/TocNavParser.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Epub/Epub/parsers/TocNavParser.cpp)（171 行） | Expat、Arduino Stream、FsFile、BookMetadataCache；解析中耦合，渲染高耦合 | 已有 EPUB2/3、导航、CSS、脚注、图片；摘解析器须隔离 GfxRenderer/缓存格式；整 `Epub.cpp` 还会进入 ContentProtection，主线不搬 |
| S16 TXT | [lib/Txt/Txt.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Txt/Txt.cpp)（235 行） | Arduino/SdFat，低至中 | 可参考流式文本；中文目录优先原厂 `txt_chapter`，不再另造规则 |
| S16 字体、中文断行 | [lib/Epub/Epub/CjkBreaks.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/lib/Epub/Epub/CjkBreaks.cpp)（137 行） | Unicode 字符规则低耦合；字体的 GfxRenderer/EpdFont 高耦合 | 禁则可对照我们已有 `system/text/`；不换整个字体/绘图层，不搬 LGPL 字体数据结构、未核的断词表 |
| S16 词典、压缩词典 | [src/util/Dictionary.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/util/Dictionary.cpp)（1041 行）<br>[src/util/DictZip.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/util/DictZip.cpp)（292 行） | StarDict/dictzip、SdFat/uzlib；文件层中耦合 | 可移植查词格式读取，字典正文另需授权；用户没勾此项，本轮只盘点不排任务 |
| S16 高亮、书摘 | [src/ClippingStore.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/ClippingStore.cpp)（691 行） | C++、SdFat、当前书页定位，中耦合 | 复用选择范围/书摘模型，持久化走成员 DB；触控选区与 AI 都给同一来源锚点，不搬全局目录 |
| S16 传书、WebSocket、设置网页 | [src/network/CrossPointWebServer.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/network/CrossPointWebServer.cpp)（3492 行） | Arduino WebServer、WebSockets 2.7.3、SdFat、gzip网页；高耦合 | 有 HTTP/WS 上传、文件 API、设置 API；保留我们的准入服务，只摘有边界的协议或前端交互；不引入 Arduino 网络栈 |
| S16 WebDAV | [src/network/WebDAVHandler.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/network/WebDAVHandler.cpp)（848 行） | Arduino WebServer，提供 PROPFIND/PUT/DELETE；高耦合 | 这是服务端，不是坚果云客户端；若增功能也必须转调 `file.*`，不能直接暴露存储卡根目录 |
| S16 Calibre | [src/activities/network/CalibreConnectActivity.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/network/CalibreConnectActivity.cpp)（234 行） | HTTP 传书服务、mDNS、WiFi、GfxRenderer；高耦合 | 配套插件上传，不是原生智能设备协议；未勾选，不排新任务；不搬退页重启 Wi-Fi 的流程 |
| S19 OPDS 客户端 | [libs/network/Opds/src/OpdsParser.cpp](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/libs/network/Opds/src/OpdsParser.cpp)（437 行）<br>[libs/network/Opds/src/OpdsClient.cpp](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/libs/network/Opds/src/OpdsClient.cpp)（278 行）<br>[libs/network/Opds/src/Opds2Parser.cpp](https://github.com/Free-Ink/freeink-sdk/blob/6a208c0c46cce99bafa334bc4f171f5e558c5195/libs/network/Opds/src/Opds2Parser.cpp)（602 行） | Expat、JsonSax、ArduinoJson；HTTP 由 OpdsTransport 注入，解析低、认证中 | 比重写 Atom/JSON 成熟；保留解析，Transport 接中枢/安全 HTTP；排除隐式 OAuth 的 wolfSSL 路径，凭据本人填 |
| S16 OTA | [src/network/OtaUpdater.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/network/OtaUpdater.cpp)（243 行） | Arduino Update、HTTP、整机分区，高耦合 | 只参考版本检查/下载状态；不能覆盖我们的镜像验签、存储准入和分区保护；对应底层任务结束后再接 |
| S16 插件、在线目录 | [src/activities/plugins/PluginCatalogActivity.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/plugins/PluginCatalogActivity.cpp)（1982 行）<br>[src/util/PluginEvents.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/util/PluginEvents.cpp)（510 行） | JSON 清单、HTTP、SD 上浏览器 JS、模板替换；中至高 | 是目录/浏览器侧插件，不是受管应用安装；用户勾应用市场，不等于可加载任意 JS 或任意网络能力 |
| S16 英文等输入 | [src/activities/util/KeyboardEntryActivity.cpp](https://github.com/crosspoint-reader/crosspoint-reader/blob/0333cbbe4649f80afafe83061bf1afe9fd372fcf/src/activities/util/KeyboardEntryActivity.cpp)（1034 行） | 自有 GfxRenderer、按钮导航；高耦合 | 未找到中文组词/九宫格拼音引擎；不为了中文界面名称就认定它有中文输入 |
| S17 书签 | [src/BookmarkStore.cpp](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/BookmarkStore.cpp)（949 行） | C++、HalStorage、uzlib、跨版本二进制文件，中耦合 | 锚点/去重成熟；其全局目录和 5 版文件迁移不搬，按成员接现有 DB；原厂轻量模型更适合先起步 |
| S17 阅读统计 | [src/activities/reader/BookReadingStats.cpp](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/activities/reader/BookReadingStats.cpp)（411 行）<br>[src/activities/reader/GlobalReadingStats.cpp](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/activities/reader/GlobalReadingStats.cpp)（406 行）<br>[src/activities/reader/ReadingStatsUtils.cpp](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/activities/reader/ReadingStatsUtils.cpp)（471 行） | HalStorage/HalClock、esp_mac、统计文件；中耦合 | 有每书/全局统计；不复用硬件身份和全局存储，保留当前成员、时间服务与总线事件 |
| S17 主题、书架 | [src/components/themes/dashboard/DashboardTheme.cpp](https://github.com/uxjulia/CrossInk/blob/9914146eeae7b46b300f475a16c32426fc02ec1f/src/components/themes/dashboard/DashboardTheme.cpp)（607 行） | GfxRenderer、BookStatsTracking、RecentBooksStore；高耦合 | 可借覆盖信息和统计组合方式；布局代码无法直接塞进 LVGL，资源许可另核；我们的主页与应用组织保留 |
| S18 中文排版、EPUB | [lib/Epub/Epub/ParsedText.cpp](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/lib/Epub/Epub/ParsedText.cpp)（541 行）<br>[lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp)（1165 行） | C++、GfxRenderer、Expat、Xteink SDK；中至高 | 旧公开树有中文排版增量；参考源 AGPL 导致溯源缺口，本轮不推荐复制该增量，改用原厂/许可明确的 CJK 规则 |
| S18 输入与主题 | [src/activities/util/KeyboardEntryActivity.cpp](https://github.com/DaisonChun/crosslink/blob/107e172994ad2086c414e159795515175681be71/src/activities/util/KeyboardEntryActivity.cpp)（751 行） | 按钮输入、GfxRenderer；高耦合 | 无本次可证实的中文输入引擎；不能代表官网新版支持的云盘、漫画等功能 |
| S15 中文 EPUB、TXT 目录 | [lib/Epub/Epub/CjkBreaks.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/Epub/Epub/CjkBreaks.cpp)（137 行）<br>[lib/Txt/TxtChapterIndex.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/Txt/TxtChapterIndex.cpp)（122 行） | CJK 规则低耦合，TXT 索引依赖 FsFile；中耦合 | 可合并中文规则和章节锚点；不引入整套渲染/页索引文件 |
| S15 阅读统计、热力图、连续打卡 | [src/ReadingStatsStore.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/ReadingStatsStore.cpp)（1181 行）<br>[src/util/ReadingStatsAnalytics.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/util/ReadingStatsAnalytics.cpp)（127 行）<br>[src/activities/apps/reading-stats/ReadingHeatmapActivity.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/reading-stats/ReadingHeatmapActivity.cpp)（565 行） | HalClock、全局 READING_STATS、GfxRenderer；计算低、存储/绘制中至高 | 复用汇总/连续日计算，替换时钟、成员作用域和存储；热力图用 LVGL 静态格子，按变化重画 |
| S15 待机时钟、农历、宜忌 | [src/activities/apps/standby/SloppyClockFace.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/standby/SloppyClockFace.cpp)（79 行）<br>[src/activities/apps/standby/ChineseAlmanac.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/standby/ChineseAlmanac.cpp)（691 行）<br>[src/activities/apps/standby/ChineseCalendarFace.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/standby/ChineseCalendarFace.cpp)（409 行） | 纯历法/字符串低；待机绘制依赖 GfxRenderer 高 | 农历用我们已有代码。宜忌是按日支循环选词池，不是已核权威黄历；可以借显示方式，不能冒充准确算法 |
| S15 五子棋 | [src/activities/apps/gomoku/GomokuBoard.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/gomoku/GomokuBoard.cpp)（160 行）<br>[src/activities/apps/gomoku/GomokuAI.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/gomoku/GomokuAI.cpp)（510 行）<br>[src/activities/apps/gomoku/GomokuGameActivity.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/activities/apps/gomoku/GomokuGameActivity.cpp)（829 行） | 棋盘/胜负算法低；GfxRenderer/存档/输入高 | 直接复用棋盘和 AI，薄接能力/坐标输入、成员存档与 LVGL；没有查到同版本的贪吃蛇/记忆测试独立实现，不能合写“全有” |
| S15 微读 | [lib/WeReadWebApi/src/WeReadClient.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/WeReadWebApi/src/WeReadClient.cpp)（4373 行）<br>[lib/WeReadWebApi/src/WeReadBrowse.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/WeReadWebApi/src/WeReadBrowse.cpp)（785 行）<br>[lib/WeReadWebApi/src/WeReadHttpClient.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/WeReadWebApi/src/WeReadHttpClient.cpp)（696 行）<br>[lib/WeReadWebApi/src/WeReadXhtmlCodec.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/lib/WeReadWebApi/src/WeReadXhtmlCodec.cpp)（1001 行） | 非官方 HTTP、Cookie、JSON、可能的 wolfSSL；高耦合 | 可见登录/书架/下载/进度编排；不搬逆向解码。设备客户端明确不校验证书/主机名，必须走安全 Transport；此项需用户选择且接口本次未实调 |
| S15 书架主题 | [src/components/themes/inx/InxTheme.cpp](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/src/components/themes/inx/InxTheme.cpp)（478 行） | GfxRenderer、封面缓存、设置；高耦合 | 可复用元数据需求和封面取用逻辑；不照搬 UI/缓存机制；按本项目视觉规范重接 |
| S01 EPUB、HTML、ZIP | [main/reader/epub_document.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/epub_document.cc)（418 行）<br>[main/reader/html_content.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/html_content.cc)（308 行）<br>[main/reader/zip_reader.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/zip_reader.cc)（520 行） | C++ STL、zlib、ESP 日志/PSRAM、FILE；中耦合 | 同硬件原厂优先。原文保留，文件与分配接 OSAL/文件服务；提供 C 边界给阅读器。当前 EPUB 类有 spine/正文/封面，不能当它已有完整 NCX/NAV 和 CSS 支持 |
| S01 阅读排版与渲染 | [main/reader/book_session.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/book_session.cc)（3933 行）<br>[main/reader/page_text_raster.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/page_text_raster.cc)（419 行）<br>[main/display/screen/book_screen/reader/book_reader_body.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/book_screen/reader/book_reader_body.cc)（551 行） | BookSession 3933 行，字体/页面类型/工作任务/SD 锁/刷新，高耦合 | 不能在 1～2 小时里整搬这几千行；保留我们的分页/刷新调度，分取正文块与光栅函数，缺 CSS 单列补齐 |
| S01 TXT 编码、目录 | [main/reader/text_encoding.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/text_encoding.cc)（208 行）<br>[main/reader/txt_chapter.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/txt_chapter.cc)（436 行） | UTF/GB18030 数据、纯字符识别，低耦合 | 优先复用章节规则；大 TXT 流式读取可比较 S05，不保留我们 2 MiB 整本装载和 16 本硬上限为最终方案 |
| S01 书签、详情、书架 | [main/reader/book_bookmarks.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/book_bookmarks.cc)（154 行）<br>[main/display/screen/book_screen/shelf/book_detail.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/book_screen/shelf/book_detail.cc)（395 行） | BookSourcePosition、POSIX 文件、LVGL；模型低、持久化/界面中至高 | 直接借稳定位置模型/信息提取；文件格式 BKM1 与原厂全局路径不搬，改为成员 DB；页面仍由我们的能力驱动 |
| S01 图片、漫画页 | [main/reader/image_stream.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/image_stream.cc)（1021 行）<br>[main/display/screen/book_screen/reader/book_page_image.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/book_screen/reader/book_page_image.cc)（349 行） | zlib/JPEG/PNG、PSRAM/硬件 JPEG；中至高 | 复用流式解码和单图页检测；图片过文件/资源准入，L8 输出交显示服务，不带驱动/清屏次数 |
| S01 横竖屏、竖排、自动翻页 | [main/reader/vertical_text.h](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/reader/vertical_text.h)（130 行）<br>[main/display/screen/book_screen/reader/book_reader_orient.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/book_screen/reader/book_reader_orient.cc)（100 行）<br>[main/display/screen/book_screen/reader/book_auto_turn.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/book_screen/reader/book_auto_turn.cc)（106 行） | 纯字形旋转规则低；触控坐标/LVGL 定时器中 | 复用旋转/位置规则，系统层统一变换坐标；自动翻页复用已有调度器，不加常驻线程，锁屏/通话时停止 |
| S01 键盘 | [main/display/screen/screen_keyboard.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/display/screen/screen_keyboard.cc)（437 行） | LVGL、原厂落墨统计、触感反馈；高耦合 | 当前只有英文/符号/更多；不是中文九键。可参考落墨反馈，但控件直接用 LVGL 更小 |
| S01 WorkBuddy | [main/workbuddy/workbuddy_client.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/workbuddy/workbuddy_client.cc)（652 行）<br>[main/workbuddy/workbuddy_voice_stream.cc](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/main/workbuddy/workbuddy_voice_stream.cc)（369 行） | CloudZao 服务、扫码授权、NVS、实时 ASR/WS、音频占用；高耦合 | 可以取协议 DTO/状态解释；保留家庭中枢、成员凭据和音频服务，不能不经选择改接原厂云 |
| S05 C 版 EPUB/TXT 备选 | [main/book/book_epub.c](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_epub.c)（2247 行）<br>[main/book/book_txt.c](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_txt.c)（799 行）<br>[main/book/zip_reader.c](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/zip_reader.c)（437 行） | Apache C、ESP miniz/PSRAM、book_source/ttf_font；中耦合 | 已有 NAV/NCX、编码、目录容量边界；若原厂解析不够则有证据比较这个替代，不同时接两套长期引擎 |
| S05 中文九键/全键盘 | [main/ui/ui_keyboard.c](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_keyboard.c)（509 行）<br>[main/ui/ui_ime.c](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/ui/ui_ime.c)（189 行） | Apache C、read_pico_search/词表、framebuffer、ESP 分配；高耦合 | 确有拼音组词和九键。只取数字映射/候选匹配逻辑，UI 用 LVGL，词库保留我们现有 IKD1；不把另一套编辑器/词库/分配器一起引入 |
| S04 较新 kiikoread 变体 | [main/book/book_epub.c](https://github.com/wegooo-cell/read-pico-reader/blob/ea371a7d5998880265aea09f9d44b26b619a8289/main/book/book_epub.c)（2395 行）<br>[main/book/book_txt.c](https://github.com/wegooo-cell/read-pico-reader/blob/ea371a7d5998880265aea09f9d44b26b619a8289/main/book/book_txt.c)（825 行）<br>[main/ui/ui_keyboard.c](https://github.com/wegooo-cell/read-pico-reader/blob/ea371a7d5998880265aea09f9d44b26b619a8289/main/ui/ui_keyboard.c)（516 行） | 与 S05 同系，更多索引/大书优化；同样有 framebuffer/ESP 依赖 | 作为同文件功能对照，固定 ea371a7d 不追 HEAD；成熟度由现有宿主机测试源码支持，测试本次未执行 |
| S11 音乐、歌词、标签 | [main/music/music_player.c](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_player.c)（1628 行）<br>[main/music/music_tags.c](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_tags.c)（559 行）<br>[main/music/music_lyrics.c](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/blob/746ce316faf61ce5228eceee618eacb50a02a395/main/music/music_lyrics.c)（316 行） | esp_audio_simple_dec、bt_host、FreeRTOS、NVS；元数据低、核心高 | 取标签/歌词与队列编排，接 media_service；不搬另一套音频线程。M4A 标签不等于 M4B 章节支持 |
| S12 C 版 OPDS、位置 | [firmware/shared/main/book/opds_client.c](https://github.com/dephia31/dpreader31/blob/9ae58085c4805a321dc741055effb53183d0b335/firmware/shared/main/book/opds_client.c)（653 行）<br>[firmware/shared/main/book/book_sync_position.c](https://github.com/dephia31/dpreader31/blob/9ae58085c4805a321dc741055effb53183d0b335/firmware/shared/main/book/book_sync_position.c)（122 行） | ESP HTTP、简单 Atom/XML、读取文件；中耦合 | OPDS 1.2 备选，删除其默认公共书源，接用户选择的书库；位置函数仅参考，不排 KOReader 同步 |
| S13 局域网同步、微读排除 | [main/display/screen/lansync_screen/lansync_screen.cc](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/blob/89db82442e655340e6194db89d4d6c9e64031d93/main/display/screen/lansync_screen/lansync_screen.cc)（375 行）<br>[main/weread/weread_ink.cc](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/blob/89db82442e655340e6194db89d4d6c9e64031d93/main/weread/weread_ink.cc)（727 行） | LVGL、原厂传输、逆向协议；中至高 | 前者可摘界面动作和文件接收；后者仅记录不复制。微信读书大纲/书评等不从受排除代码里“顺便带入” |
| S02 原厂 E-Ink4 | [README.md](https://github.com/CloudZao/Metalio-E-INK4/blob/d12f481f4a42420634bb56a928f3959a977367d4/README.md)（703 行） | S3/SPI 屏、旧公开源码；高耦合 | 与 Plus 多数上层同源，但新宣传功能在此固定树未找到，统计优先 CrossMux，WorkBuddy 取 S01 |
| S03 网络电台 | [main/display/screen/radio_screen/radio_screen.cc](https://github.com/CloudZao/MetalioClaw4/blob/ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950/main/display/screen/radio_screen/radio_screen.cc)（1770 行） | ESP GMF、HTTP、证书包和 P4 音频服务；高耦合 | 只复用节目/流控制编排；与 AgentUI radio 都是网络广播，没有 FM 调谐器实现 |
| S06 电脑语音、网络广播、儿童宠物示例 | [components/codex_remote/codex_ws_client.cc](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/components/codex_remote/codex_ws_client.cc)（1293 行）<br>[components/codex_remote/codex_ws_pipeline.cc](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/components/codex_remote/codex_ws_pipeline.cc)（481 行）<br>[external_apps/examples/radio/main/radio.cc](https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/172ec530ff65e11521585fe3b8611daa20a78aa4/external_apps/examples/radio/main/radio.cc)（617 行） | ESP WS、cJSON、mbedTLS、P4 应用 SDK；中至高 | WS 报文可复用；radio 只能做网络电台。pet_demo 有示例源码，不能当已具备儿童识字关卡；不移植 P4 ELF 加载器 |
| S07 字体工具 | [plus-noto-font/components/plus_font/plus_font.cc](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/plus-noto-font/components/plus_font/plus_font.cc)（209 行） | MIT C++、FreeType FTL、Noto OFL；字体输出/ESP 中耦合 | 我们已有字体打包/映射；仅补动态字体/缺字时比较，不能把大字体包复制进 repo |
| S08 输入与 AI MCP | [main/input/text_input.cc](https://github.com/446599/Metalio-WhiteAI/blob/153d0967e97e1cf308403252cfc2839d6d65fb08/main/input/text_input.cc)（154 行）<br>[docs/AI_SYSTEM_MCP.md](https://github.com/446599/Metalio-WhiteAI/blob/153d0967e97e1cf308403252cfc2839d6d65fb08/docs/AI_SYSTEM_MCP.md)（72 行） | C++ 自有编辑器、词表、MCP；中至高 | 确有中文拼音，但原创代码缺许可；不移植，其功能由我们的文本能力/快照承接 |
| S14 桌面桥接 | [src/core/agent-bridge.js](https://github.com/AlayaElla/CodexRemote/blob/5f9bcad783ea3274d01982e0f1826e6549d379ae/src/core/agent-bridge.js)（45 行）<br>[src/core/codex-device-sync.js](https://github.com/AlayaElla/CodexRemote/blob/5f9bcad783ea3274d01982e0f1826e6549d379ae/src/core/codex-device-sync.js)（57 行） | Node/ws、桌面内部状态/IPC、原生音频桥；高耦合 | 缺原创许可且依赖桌面内部接口；仅用于找协议缺口，不复制或注入桌面进程 |
| S22 Game Boy | [peanut_gb.h](https://github.com/deltabeard/Peanut-GB/blob/d0bcca771c83a2638c93a9ae61f3d51f226dc905/peanut_gb.h)（4045 行） | 纯 C 单头、CPU/内存/图像回调；低耦合 | 可保留核心原文，ROM 读取/帧缓冲/输入/时间接 HAL/OSAL；无声音或着色的支持不能靠名称推断 |
| S23 音频解码 | [dr_mp3.h](https://github.com/mackron/dr_libs/blob/f12e197fd462e4c22873fbd70d11b1d2f2c1dc3c/dr_mp3.h)（5223 行）<br>[dr_flac.h](https://github.com/mackron/dr_libs/blob/f12e197fd462e4c22873fbd70d11b1d2f2c1dc3c/dr_flac.h)（13086 行）<br>[dr_wav.h](https://github.com/mackron/dr_libs/blob/f12e197fd462e4c22873fbd70d11b1d2f2c1dc3c/dr_wav.h)（8720 行） | 纯 C、读/seek/分配回调；低至中 | 可直接复用算法，PSRAM/分块流接 media_service，先测 16 MiB/内部 RAM 预算；AAC/M4A 仍缺 |
| S24 Vorbis | [stb_vorbis.c](https://github.com/nothings/stb/blob/4e9b368bfc071e97206ea5c6adba68dd0cbf8897/stb_vorbis.c)（5584 行） | C、读/seek/预分配；中耦合 | 补 Ogg Vorbis，保留现有 Opus；不承诺固定点性能 |
| S25 云盘 WebDAV 客户端补充 | [src/webdav4/client.py](https://github.com/skshetry/webdav4/blob/e656d7d04f7e7ce732dd743666a3cad3314293e1/src/webdav4/client.py)（714 行） | MIT Python、HTTPX/python-dateutil；只在家庭中枢运行 | 补服务器端 WebDAV 下载/目录，不把 Python 放 MCU；用 `retry=False`、证书校验和成员凭据，流式交现有内容导入 |

未在上述固定源码找到的模块，不能由功能列表补齐：MOBI 设备端解析、M4B 章节索引、网易云客户端、数据胶囊客户端、ima 上传、当前 CrossLink 的云盘/漫画增量、现成浏览器引擎、全部儿童认知关卡、贪吃蛇和记忆测试。官方工具中的 MOBI → `.ebook` 是电脑预处理，不等同设备直接开 MOBI；插件/网页目录不等同应用市场。S09 GPL 全排除、S10 无 C/C++ 源码、S21 只提供 X4 低层，均已在许可表给结论，没有漏算成可用模块。

### 输入法和键盘的单独结论

【实现】本地依赖 LVGL `v9.5.0`，源码提交 `85aa60d18b3d5e5588d7b247abf90198f07c8a63`，根 `LICENCE.txt` 为 MIT。`src/widgets/keyboard/lv_keyboard.c` **501 行**，`src/widgets/ime/lv_ime_pinyin.c` **1202 行**。配置文件实际为 `system/ui/lv_conf.h`：没有显式设置这两个开关；LVGL 默认键盘开、拼音关，不能笼统写“两个都没开”。接口已经在依赖中，不必另找键盘库。

| 方案 | 已有能力 | 限制 | 改动量【估算】 |
| --- | --- | --- | --- |
| 完整 `lv_keyboard`＋`lv_ime_pinyin` | 官方支持 K26、K9、K9_NUMBER、候选页、自定义表 | `lv_pinyin_dict_t` 是拼音→连续汉字串；`lv_ime_pinyin.c:843,925` 按每字 3 字节取候选，偏单字，不是 IKD1 词组/频率表。默认回调直接写 textarea，会绕开我们的能力、撤销和密码规则。没有查询回调可直接挂 `py_lookup` | 普通单字输入小于半天；若保留词组、成员学习和语音则 1～2 天，单开宏不能完成 |
| 原厂屏幕键盘 | 英文/符号，针对落墨反馈 | S01 当前没有中文九键；搬它要适配原厂反馈状态，又造一套控件 | 1～2 天，收益比 LVGL 小 |
| kiikoread 键盘＋IME | S05 `ui_keyboard.c` 509 行、`ui_ime.c` 189 行，Apache，真有九键和有界组词 | framebuffer 绘制、另一个词表与 ESP 分配；直接整搬会丢我们的 IKD1/成员词频/文本会话 | 整搬更多；只摘数字映射/候选匹配约半天 |
| **推荐：LVGL 键盘控件＋现有 IKD1 引擎；摘现成九键映射/匹配** | 保留词组、简拼、每人学习、语音、撤销、密码与 AI 入口；复用布局轮子 | 九键要补候选查询，必须先量只读键表匹配成本；不把 LVGL 单字表转换成另一个常驻大词库 | **1～2 天**，拆成下面 I01～I03；估算净新增约 180～300 行胶水并删除手工按键绘制，超预算再拆，不承诺本次已可用 |

推荐具体改动（规划，尚未执行）：

- `system/ui/shell/keyboard.c` 只负责 `lv_keyboard` 的 map/style 与事件桥。移除控件默认的 textarea 写入回调，按键、选词继续经过现有 `keyboard.key`／`keyboard.pick` 和文本服务；不并存两条输入路径。候选栏和语音仍使用现有会话与快照。密码模式只收真实触控/键盘，不向 AI 暴露正文，不允许 AI 代填。
- `system/services/text/pinyin.c`／`text_internal.h` 增加九键匹配入口，原文摘 S05 的字母→数字映射及匹配逻辑（Apache 声明保留）；对 IKD1 的只读键表匹配并返回原有 `py_cand_t`，保留 `consumed`／频率。首个实现可有界遍历现有键表，必须测按键查询预算；若达不到预算，先交测量和更小备选，不自行新增缓存、索引文件或词库格式。
- `system/services/text/text_service.c` 仅选择全拼或九键查询；保留成员词频、剪贴板、撤销和语音，不能把 LVGL 的单字选择当成已接通词组。`system/ui/lv_conf.h` 显式保留键盘；推荐不启用完整 `lv_ime_pinyin` 引擎。词库仍由 `tools/imedict/` 生成 IKD1，从闪存映射或按现有路径装载。
- **先等 `ui-input-2`** 完成现代键盘/文本框规格，取其最新令牌再改；`ui-audit-2` 只审视页面，不并行覆盖这些文件。I01～I03 不重排短信/联系人/电话页面。

没有统计模型整句组句、模糊音等能力的系统仍要如实说明；本次不会用“大模型输入法”掩盖本地引擎差距。

## 用户选择逐项对照（任务称 71 项，源表实际 72 项）

【实测】[picks.md](picks.md) 有 **72 行、72 个唯一 ID**，标题的 71 是计数差异。本表逐个覆盖原文件所有 ID，没有丢一行凑数，也没有增加用户未选项；**Arduboy、KOReader 同步不排队**。每项工期是实现到可验收的粗估，含适配/测试，不是抄文件时间；“更多”表示多于 2 天或来源/账号/底层尚有闸门。队列里的 1～2 小时单元只承诺表中列出的一个成果，不能把它等同整项完工。依赖列是功能先后关系，不表示把用户没勾的功能加进来。

表中许可证按来源表适用；“没有完整/可用来源”不能自动拿同一行的基础模块充数。所有新能力名称仍是建议；本次没有注册或启用服务。

| 编号／功能 | 最合适的来源（项目、模块、文件） | 许可／结论 | 改造量【估算】 | 依赖项 | 还要做什么／边界 | 队列 |
| --- | --- | --- | --- | --- | --- | --- |
| `ai_services` 多家 AI 服务切换 | 无现成四家切换；S01 `main/protocols/websocket_protocol.cc` 仅传输参考；本地 `hub/ink_hub/models/` | MIT；**缺完整来源** | 更多 | 无新增依赖 | 原厂 E-Ink4 公开树未见新切换代码；中枢当前只有 fake provider，不能标成多厂商已完成；接各家官方 SDK 前逐一锁许可 | A01 |
| `antialias` 抗锯齿强度 | S01 `main/reader/page_text_raster.cc`、`book_reader_prefs.cc` | MIT；**部分复用** | 1～2 天 | `typeset` | MIT；874538e 是抗锯齿开关，不是 2.0.78 宣传的强度档位。复用光栅路径；强度曲线缺源码/标定，不改底层屏驱动 | R04 |
| `ap_transfer` 热点传书 | S16 `src/activities/network/CrossPointWebServerActivity.cpp`；原厂 AP 组件待接 | MIT（仅独立文件）；**合并** | 更多 | `wifi_transfer` | MIT/原厂组件另核；只取 AP 与传书编排，不能引 Arduino；必须有关闭/飞行模式恢复和同一文件准入 | T03 |
| `app_push` 手机 App 推送 | S01 `main/display/screen/cloud_screen/download/cloud_download.cc` | MIT；**仅基础来源** | 更多 | `files` | MIT 下载基础可用；完整 App 账号/推送协议未核实；要用户定是否经原厂云，不能默传家庭文件 | O04 |
| `appstore` 应用市场 | S16 `src/activities/plugins/PluginCatalogActivity.cpp`；本地应用包校验/注册 | MIT（仅独立文件）；**合并** | 更多 | 无新增依赖 | MIT 的目录/HTTP 可借；浏览器插件不是我们的受管应用；包身份、签名、卸载、成员权限保留 | A04 |
| `audiobook` 有声书章节跳转 | S11 `main/music/music_tags.c` 仅标签；没有可用的 M4A/M4B 章节模块 | Apache-2.0；**缺完整来源** | 更多 | `music` | Apache 基础不等于章节；需另找允许的 MP4/AAC/章节解析，不能拿逐曲播放替代 | M04 |
| `autorotate` 自动旋转 | S01 `main/display/screen/book_screen/reader/book_reader_orient.cc`；本地加速度方向事件 | MIT；**部分复用** | 1～2 天 | `landscape` | MIT；阅读坐标变换可借，自动方向判定仍需接现有输入；先核界面/触点一致，不动传感器驱动 | R06 |
| `autoturn` 定时自动翻页 | S01 `main/display/screen/book_screen/reader/book_auto_turn.cc` | MIT；**复用** | 小于半天 | `epub` | MIT；复用定时/停止条件，进现有执行器；锁屏、来电、离页和成员变化停止 | R06 |
| `bookmark` 书签 | S01 `main/reader/book_bookmarks.cc`；S17 `src/BookmarkStore.cpp` 参考复杂锚点 | MIT；**合并** | 1～2 天 | `epub` | MIT；优先原厂位置模型，按成员 DB 保存，不带 BKM1/多代 bin 迁移机制 | R07 |
| `bookstyle` 关闭书内样式 | S16 `lib/Epub/Epub/css/CssParser.cpp` | MIT（仅独立文件）；**部分复用** | 1～2 天 | `epub` | MIT；复用样式解析，增加“使用/忽略书内样式”策略，仍按统一正文排版 | R04 |
| `browser` 浏览器 | 本地 `system/apps/remote/remote.c`、`hub/ink_hub/remote/input.py`；这些固件无可用浏览器引擎 | 现有项目实现；**保留并补全** | 更多 | 无新增依赖 | 已有远程页面/点按/输入通路；`hub/browser_node/README.md` 明说尚无浏览器镜像。不能写完整浏览器已有；账号/证件仍由人输 | A03 |
| `bt_keyboard` 蓝牙键盘 | S20 `libs/network/BleKeyboardHost/src/HidKeymap.cpp`、`BleKeyboardHost.cpp` | MIT＋NimBLE Apache-2.0；**部分复用** | 更多 | `keyboard` | MIT＋NimBLE Apache；键码能摘，Arduino 主机不能直接搬；不和音频蓝牙端口任务重改同文件 | M06 |
| `bt_reconnect` 耳机断开后自动重连 | S11 `main/music/music_player.c` 与原厂蓝牙通路；等待 port-bt-audio-2 | Apache-2.0；**合并** | 1～2 天 | `music` | Apache/MIT；同位置续播已有基础，三次重连规则不在这段核心完整实现中；不得新造另一条蓝牙状态机 | M05 |
| `bt_speaker` 当蓝牙音箱 | 原厂蓝牙模式，归 `port-bt-audio-2`；不从 CrossPoint 读屏驱动移植 | 未取得可用授权；**沿用已有任务** | 更多 | 无新增依赖 | 原厂 MIT；S31 本身的 BLE 与外置蓝牙音频模块要分清，验收是接收手机音频；勾选与早期暂定冲突由主会话确认 | 已有任务 |
| `capsule` 数据胶囊网盘传书 | 官网指向 Gitee crosslink，当前源码匿名取回受限；公开旧版未核到该客户端 | 未取得可用授权；**没有可用来源** | 更多 | `files` | 未取得当前功能的许可/源码，不据发布页宣称能搬；账号与远端取书范围要用户定 | O04 |
| `codex_remote` 语音遥控电脑上的 AI | S06 `components/codex_remote/codex_ws_client.cc`；S14 桥接主程序缺许可 | MIT；见许可表；**部分复用** | 更多 | 无新增依赖 | 设备协议 MIT 可摘；桌面端不复制、不注入；凭据/电脑文件访问/危险操作仍过家庭中枢策略 | A02 |
| `comic` 高清插图和漫画模式 | S01 `main/reader/epub_document.cc` 的单图页检测、`image_stream.cc` | MIT；**部分复用** | 更多 | `epub` | MIT；原厂流式图片可起步；不等同当前 CrossLink2 高清漫画增量，需补缩放/翻页/预算验证 | R05 |
| `desk_pages` 多页桌面 | 本地 `system/ui/shell/navigation.c` | 现有项目实现；**保留** | 小于半天 | 无新增依赖 | 项目现有三屏桌面；移植主题会破坏导航/AI 语义，做兼容复查即可 | 保留 |
| `detail` 书籍详情页 | S01 `main/display/screen/book_screen/shelf/book_detail.cc`、`epub_document.cc` | MIT；**合并** | 1～2 天 | `epub` | MIT；抽元数据/进度字段，详情 UI 用我们的列表与快照 | R08 |
| `epub` EPUB 阅读 | S01 `main/reader/epub_document.cc`、`html_content.cc`、`zip_reader.cc`；S16 NAV/CSS 补足 | MIT；MIT（仅独立文件）；**局部替换** | 更多 | `files` | MIT＋zlib/Expat；原厂正文与 ZIP 原文接入；目录、样式和图片分步交付，不能只解析标题就写完成 | R01～R05、R10 |
| `excerpt` 书摘 | S16 `src/ClippingStore.cpp`；S01 书签源位置 | MIT（仅独立文件）；MIT；**合并** | 1～2 天 | `bookmark` | MIT；稳定选区与原文摘录进成员数据，不复制全局 clippings 路径或默认外部同步 | R07 |
| `files` 文件管理 | 本地 `system/apps/files/files.c`、`system/services/file/file_service.c` | 现有项目实现；**保留** | 小于半天 | 无新增依赖 | 现有改名/复制/移动/编辑与准入更完整；源固件直接 SD IO 不替换，增加来源时复用此入口 | 保留 |
| `fm` 收音机 | S06 `external_apps/examples/radio/main/radio.cc` 和 S03 radio 都是网络广播 | MIT；**没有 FM 来源** | 更多 | 无新增依赖 | 源代码没有 FM 调谐/射频功能；硬件是否有 FM 芯片本任务无证据，不用网络电台冒充收音机 | M07（仅核查） |
| `gameboy` Game Boy 模拟器 | S22 `peanut_gb.h` | MIT；**复用** | 更多 | `files` | MIT；GB 核心可取，输入/帧输出接平台；ROM 必须自制或授权，声音与存档另验 | A06 |
| `hidebar` 隐藏状态栏和进度条 | S17 `src/util/ReaderStatusBarConfig.h`；本地阅读页标题/页脚 | MIT；**合并** | 小于半天 | `typeset` | MIT；布局开关可借，实际正文区域与页码重排走同一设置能力 | R11 |
| `ima` 录音上传 ima 知识库 | ThingOS 只有功能说明，未取得公开许可源码 | 未取得可用授权；**没有可用来源** | 更多 | `recorder` | 不猜私有接口；录音外发是家庭资料出境到第三方服务范围，须本人选择和明确目标 | A05 |
| `jianguo` 坚果云网盘传书 | S25 `src/webdav4/client.py`（补充）；S16 WebDAVHandler 不能当客户端 | MIT；MIT（仅独立文件）；**复用到中枢** | 1～2 天 | `files` | MIT；HTTPX/dateutil 依赖另锁定；只下载用户选的目录，TLS 校验、成员凭据、retry=False；设备端仅内容导入 | O02 |
| `journal` 阅读手账 | S15 `src/util/ReadingStatsAnalytics.cpp` 仅每日汇总 | MIT（仅独立文件）；**部分复用** | 1～2 天 | `stats`, `excerpt` | MIT；可复用数据汇总，未找到 PageRest 像素手账源码；手账页面仍需我们按设计接 | R12 |
| `keyboard` 九宫格键盘 | LVGL `lv_keyboard.c`／`lv_ime_pinyin.c`；S05 `main/ui/ui_ime.c` | LVGL MIT；S05 Apache-2.0；**合并** | 1～2 天 | 无新增依赖 | MIT/Apache；推荐控件＋现有词组引擎，九键不全盘重写；先等 ui-input-2 | I01～I03 |
| `kids` 儿童认知和电子宠物 | S06 `external_apps/examples/pet_demo/main/pet_demo.cc` 仅宠物演示；本地孩子模式 | MIT；**部分复用** | 更多 | 无新增依赖 | MIT；可做可逆宠物交互起点，没有现成认知课程/内容授权；不能把 DEMO 当儿童内容已齐 | A07 |
| `landscape` 横屏阅读 | S01 `main/display/screen/book_screen/reader/book_reader_orient.cc` | MIT；**复用** | 1～2 天 | `typeset` | MIT；横屏分页、触控及能力锚点一致；不切换整套显示驱动 | R06 |
| `lang` 多语言 | 本地 `system/text/i18n.c`；S16 `lib/I18n/` 仅参考 | MIT（仅独立文件）；**保留** | 小于半天 | 无新增依赖 | 已有中英文及提取流程；搬包不得把别的语言资源/字体默认全部常驻 | 保留 |
| `light` 前光亮度和冷暖 | 本地前光服务；`port-board-2`、`port-power-2` | 现有项目实现；**沿用已有任务** | 小于半天 | 无新增依赖 | MIT 原厂；已有亮度/暖光能力，不重复写背光驱动 | 已有任务 |
| `lock_calendar` 锁屏：日历和老黄历 | S15 `src/activities/apps/standby/ChineseCalendarFace.cpp`；本地 `lunar.c` | MIT（仅独立文件）；**合并** | 1～2 天 | `lock_clock`, `lunar` | MIT 画面参考；用现有农历服务，不搬另一份日期表和刷新循环 | L02 |
| `lock_clock` 锁屏：时钟 | S15 `src/activities/apps/standby/SloppyClockFace.cpp`；本地 `system/ui/lock_mark.c` | MIT（仅独立文件）；**合并** | 1～2 天 | 无新增依赖 | MIT；当前已有分钟时钟与快照，取样式而不是新增定时任务；分钟没变不刷 | L01 |
| `lock_pin` 锁屏密码 | 本地 `system/policy/members.c`、`system/ui/shell/pinpad.c` | 现有项目实现；**保留** | 小于半天 | 无新增依赖 | 现有成员验证/锁屏，保留密码排除 AI；源固件单机密码不能代替家庭隔离 | 保留 |
| `lock_quote` 锁屏：句子 | S16 `src/ClippingStore.cpp`；本地锁屏插槽 | MIT（仅独立文件）；**合并** | 1～2 天 | `excerpt`, `lock_clock` | MIT；取当前成员明确选的摘录，锁住大人空间时不向孩子/AI泄露私密书摘 | L03 |
| `lock_ticket` 锁屏：阅读小票 | S01 `main/reader/book_home_snapshot.cc`；S15 统计 | MIT；MIT（仅独立文件）；**部分复用** | 1～2 天 | `stats`, `recent`, `excerpt` | MIT；只读组合成阅读小票；备注“锁屏先不做”需定时机，保留需求不擅自删 | L03 |
| `lock_todo` 锁屏：待办 | 本地 `system/apps/memo/memo.c`、`system/ui/lock_mark.c` | 现有项目实现；**保留并扩展** | 小于半天 | `todo`, `lock_clock` | 已有待办与锁屏基础，不复制上游全局待办；内容按当前可见成员 | L03 |
| `lock_widgets` 锁屏组件 | S17 `src/components/themes/dashboard/DashboardTheme.cpp`；本地锁屏 | MIT；**合并** | 1～2 天 | `shelf`, `recent`, `lock_clock` | MIT；借字段组合，画面由现有壳绘制；锁屏只显示许可内容 | L03 |
| `lunar` 老黄历 | 本地 `system/services/time/lunar.c`；S15 `ChineseAlmanac.cpp` 对照 | MIT（仅独立文件）；**保留；宜忌缺权威来源** | 更多 | 无新增依赖 | 已有 1901～2100 农历；S15 宜忌仅按日支选词池，不是准确黄历。核定来源前不能宣称完成“老黄历宜忌” | L02 |
| `minigames` 小游戏 | S15 `src/activities/apps/gomoku/GomokuBoard.cpp`、`GomokuAI.cpp` | MIT（仅独立文件）；**部分复用** | 更多 | 无新增依赖 | MIT；五子棋可直接取；贪吃蛇/记忆测试在所查树未找到独立模块，不用其它未勾的游戏替代 | A08 |
| `mobi` MOBI 格式 | S01 `tools/ebook/epdbook/extractors/mobi.py` 仅电脑预处理 | MIT；**没有设备端来源** | 更多 | `epub` | 原厂文件 MIT 但其 mobi 解包依赖尚未核许可；本次不建议引入。转换文件不是直接打开 MOBI，保留缺口 | O05（仅来源核查） |
| `music` 本地音乐播放器 | S11 `main/music/music_tags.c`、`music_player.c`；S23 dr_mp3/dr_flac/dr_wav；S24 Vorbis | Apache-2.0；MIT-0；MIT；**合并** | 更多 | `files` | Apache/MIT-0/MIT；保留现有媒体服务。MP3/FLAC/WAV/Vorbis 有候选，AAC/M4A 全链路未补齐 | M01～M03 |
| `netease` 网易云音乐 | 布丁功能页未给可用源码，所查项目无成熟授权客户端 | 未取得可用授权；**没有可用来源** | 更多 | `music` | 非官方接口/账号/曲目授权须用户定；未找到就写缺口，不尝试取付费歌曲 | O03 |
| `netradio` 网络电台 | S06 `external_apps/examples/radio/main/radio.cc`；S03 radio_screen | MIT；**复用** | 1～2 天 | `music` | MIT；复用 HTTP 节目/流编排，输出接现有音频，不启用原项目默认广播站或未经选定的第三方源 | M07 |
| `opds` OPDS 书源 | S19 `libs/network/Opds/src/OpdsParser.cpp`、`OpdsClient.cpp`；S12 C 版备选 | MIT（排除隐式认证）；见许可表；**复用** | 1～2 天 | `files`, `epub` | MIT/Apache；解析和受控下载，禁任意公网默认书源；基本认证本人填，排除隐式 wolfSSL 路径 | O01 |
| `ota` 联网升级 | 本地 `system/services/update/update_service.c`；S16 `src/network/OtaUpdater.cpp` 参考 | MIT（仅独立文件）；**保留并补端口** | 更多 | 无新增依赖 | 已有中枢升级校验；源固件整机 Update 不能替换镜像验签/分区保护；原厂恢复空间与写分区要用户定 | T04 |
| `phone_sms` 电话和短信 | 本地 `system/apps/sms/`、`contacts/`、`phone/`；原厂模组原文 | 现有项目实现；**保留并沿用已有任务** | 更多 | 无新增依赖 | comm-ux-2 / port-cell-3 正在处理；不移植 AgentUI 通话界面或另拆一轮 | 已有任务 |
| `recent` 继续阅读、最近阅读 | 本地 `apps/reader/reader.c:75`；S15 `src/RecentBooksStore.cpp` 参考 | MIT（仅独立文件）；**保留** | 小于半天 | 无新增依赖 | 现有成员 reader.last 与继续读应保留；新 EPUB 用稳定锚点兼容已有 TXT 位置 | R10 |
| `recorder` 录音机 | 本地 `system/services/audio/media_service.c` 与语音备忘；S06 voice_recorder 示例 | MIT；**合并** | 1～2 天 | `files` | 原厂 MIT 示例只作按钮编排参考；复用现有 Opus 录音/锁屏所有权，不再开录音线程 | M08 |
| `scrape` 联网补全书籍信息 | 当前 CrossLink Pixel 功能说明；公开旧版未见授权的封面/评分抓取模块 | 未取得可用授权；**没有完整来源** | 更多 | `detail`, `shelf` | 不能把 EPUB 元数据读取当联网补全；只可先接用户选定的公开合法元数据 API，家庭书目外发需选择 | O03 |
| `sd_update` TF 卡升级固件 | 本地升级服务/原厂 `main/ota.cc` 仅基础；未核到可直接用的 TF 卡引导模块 | 现有项目实现；**缺完整来源** | 更多 | `ota`, `files` | 沿用同一镜像校验和安全分区写入，不能复制 lnkboot 私有整机切换器；不刷设备 | T04 |
| `shelf` 封面书架和分类 | S01 `main/display/screen/book_screen/shelf/book_bookshelf.cc`、`book_detail.cc` | MIT；**局部替换** | 更多 | `detail`, `epub` | MIT；换掉当前 16 本 TXT 列表的内容模型，合入封面/分类；主页、成员与能力接口保留 | R08、R10 |
| `stats` 阅读统计 | S15 `src/ReadingStatsStore.cpp`、`src/util/ReadingStatsAnalytics.cpp` | MIT（仅独立文件）；**复用算法** | 1～2 天 | `recent` | MIT；按当前成员统计，时间走现有服务，不复制设备唯一身份或默认外发 | R09、R12 |
| `streak` 连续阅读打卡 | S15 `src/ReadingStatsStore.cpp:611` 起 | MIT（仅独立文件）；**复用算法** | 小于半天 | `stats` | MIT；阈值是上游阅读目标设置，不自动升级为本项目规则；先给数据和可见说明 | R09 |
| `themes` 阅读主题 | S16 `lib/Epub/Epub/css/CssParser.cpp`；S17 阅读状态栏配置 | MIT（仅独立文件）；MIT；**合并** | 1～2 天 | `typeset`, `bookstyle` | MIT；一套排版值的预设，服从我们的字体/灰度/界面规范；不搬整机主题引擎 | R11 |
| `toc` 目录和章节跳转 | S01 `main/reader/txt_chapter.cc`；S16 `lib/Epub/Epub/parsers/TocNavParser.cpp`、`TocNcxParser.cpp` | MIT；MIT（仅独立文件）；**复用** | 1～2 天 | `epub` | MIT＋Expat；TXT 中文章名、EPUB NAV/NCX 与锚点跳转分清；目录不是仅跳页号 | R03 |
| `todo` 待办和今日事项 | 本地 `system/apps/memo/memo.c`、时间提醒服务 | 现有项目实现；**保留** | 小于半天 | 无新增依赖 | 已有清单/提醒；保留原子能力、成员数据，锁屏只是视图 | 保留 |
| `typeset` 阅读排版设置 | S01 `main/reader/text_spacing.h`、`page_text_raster.cc`；S16 CSS 解析 | MIT；MIT（仅独立文件）；**合并** | 更多 | `epub` | MIT；字号/行距/边距/字距/缩进/下划线是阅读设置，不混成系统字号；一处接正文模型 | R04、R11 |
| `usb_disk` USB 传书（当 U 盘） | 本地 `system/services/file/file_service.c` 与 `hal_usb_disk_*`；原厂 TinyUSB 路径待核 | 现有项目实现；**保留并补端口** | 1～2 天 | `files` | 原厂/IDF Apache；S31 HAL 仍是 -1 桩。卡借出/归还、成员锁与文件准入保留；避免两个文件系统同时写卡 | T02 |
| `vertical` 竖排阅读 | S01 `main/reader/vertical_text.h`、`page_text_raster.cc` | MIT；**复用** | 1～2 天 | `typeset` | MIT；复用竖排标点旋转和列排列；不把整屏旋转当古籍竖排 | R06 |
| `vibe` 振动反馈和强度 | 本地触感服务；`port-board-2` | 现有项目实现；**保留** | 小于半天 | 无新增依赖 | 已有触感与强度，不重造驱动；只校验新输入动作经过现有反馈路径 | 已有任务 |
| `wallpaper` 锁屏和待机壁纸 | 本地 `system/ui/shell/wallpaper.c`；S01 `main/reader/image_stream.cc` | MIT；**合并** | 1～2 天 | `files` | MIT；已有壁纸选择/绘制；补库与透明 PNG 合成，按成员授权可见，不换锁屏安全模型 | L01 |
| `weread_notes` 微信读书：划线、想法、书评 | S15 `lib/WeReadWebApi/src/WeReadBrowse.cpp` 只见浏览基础；ThingOS 无授权源 | MIT（仅独立文件）；**缺完整来源** | 更多 | `weread_online` | MIT 基础不代表已有热门划线/想法/书评；TingQu 逆向部分不用；需账号/接口选择 | O03 |
| `weread_online` 微信读书：在线读和搜书 | S15 `lib/WeReadWebApi/src/WeReadClient.cpp`；在线内容完整链路含非官方/解码 | MIT（仅独立文件）；**仅条件候选** | 更多 | `weread_shelf` | 不复制逆向解码；不把离线 EPUB 下载当在线搜索加入书架；完整源码/服务协议尚缺，接口与 TLS 风险要用户定 | O03 |
| `weread_outline` 微信读书：读书大纲 | 听趣的说明，未找到可直接复用且排除逆向后的大纲生成源码 | 未取得可用授权；**没有可用来源** | 更多 | `weread_online` | TingQu 微读逆向链路明确排除；本地 AI 总结当前书不等于微读账号读书大纲 | O03 |
| `weread_progress` 微信读书：进度和时长同步 | S15 `lib/WeReadWebApi/src/WeReadClient.cpp`、`src/activities/apps/weread/webapi/WeReadProgressSyncActivity.cpp` | MIT（仅独立文件）；**条件复用** | 更多 | `weread_shelf`, `stats` | MIT；有进度请求编排，时长与手机一致性未实调；只上传本人选的微读书籍，失败不猜成功 | O03 |
| `weread_shelf` 微信读书：书架同步和离线读 | S15 `lib/WeReadWebApi/src/WeReadClient.cpp`、`WeReadBrowse.cpp` | MIT（仅独立文件）；**条件复用** | 更多 | `epub` | MIT；二维码/书架/下载是候选；非官方 API、无证书校验和逆向内容解码阻止直接取全链路，不能擅自启用 | O03 |
| `weread_write` 微信读书：自己划线写想法 | S16 `src/ClippingStore.cpp` 仅本地书摘；未找到许可明确的微读云端写接口 | MIT（仅独立文件）；**没有完整来源** | 更多 | `excerpt`, `weread_notes` | 不把本地书摘冒充微读划线同步；发送私人想法需用户选服务与范围，不从 ThingOS 功能页推导实现 | O03 |
| `wifi_transfer` Wi-Fi 网页传书 | 本地 `system/services/file/transfer.c`；S16 `src/network/CrossPointWebServer.cpp` | MIT（仅独立文件）；**保留并补端口** | 1～2 天 | `files` | 已有一次授权码/成员 epoch/空间校验；先补真机 HAL，再择取断线/分块上传协议，不整换服务器 | T01 |
| `workbuddy` 电脑助理 WorkBuddy | S01 `main/workbuddy/workbuddy_client.cc`、`workbuddy_voice_stream.cc`、README | MIT；**条件复用** | 更多 | 无新增依赖 | MIT；现有设备侧可复用，依赖 CloudZao 服务＋百度 ASR＋电脑在线；中枢默认不改接它；用户选择后经同一能力网关 | A02 |
