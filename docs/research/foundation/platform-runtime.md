# D：ESP32-S31 平台事实与运行时可行性（E-Ink4 Plus）

查阅日期：2026-11-01。上游固定提交 `ad8edf4150a688737c0d7c00cae4413079b262ef`（下称“上游”），链接前缀 `https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4/`。
层次标注：**【官】** 官方文档或数据手册；**【码】** 本次读取的源码和配置（没有编译）；**【估】** 估算，假设随文写出；**【社】** 社区或第三方资料。以下内容均没有实机验证。

## 0. 结论摘要

1. **芯片已量产，IDF 支持仍是 preview。** S31 于 2026-07-27 宣布量产。ESP-IDF v6.1 和 master 都把 `esp32s31` 列在 `PREVIEW_TARGETS`，使用时需要加 `--preview`。官方 FAQ 写明从 v6.1.1 起才算正式支持，但截至查阅日，git 上只有 v6.1 标签。建议跟随上游使用 v6.1，并随 release/v6.1 分支升级。
2. **Plus 的 PSRAM 最可能是 16 MB（推断）。** S31 的 PSRAM 只能封装在芯片内部，芯片手册列出 16 MB 和 32 MB 两档。上游 sdkconfig 只写了 Octal/200 MHz，没有写容量。上游按单次 8 MiB 临时解码预算、外加 2 MiB 余量来设计，与 16 MB 相符。到货后用“设置→关于”（`esp_psram_get_size()`）确认。
3. **真正紧张的是 512 KB 内部 SRAM，PSRAM 反而宽裕。** 上游的 EPDiy 驱动固定占用约 85 KiB 内部 RAM，其中行队列就有 76 KiB。再加上 Wi-Fi/lwIP、I2S DMA 和 64 KiB 的 DMA 预留池，内部 RAM 已经很紧。上游代码里自设了 50 KiB 内部余量告警线。
4. **存储：** 用户数据放在内部 Flash 的 LittleFS 上（掉电安全），采用追加日志加原子替换。书籍、音频等内容放 SD 卡 FAT32，丢失后可以重新生成或下载。首版不引入 SQLite。
5. **v0.4 可安装 app：** 推荐 Lua 5.4/5.5（espressif/lua 组件），界面由系统渲染声明式页面。WAMR 作为强隔离备选，需要打 S31 ABI 补丁并实测。ELF 加载器只用于自家签名的原生模块。
6. **到货前：** QEMU 不支持 S31（IDF 只支持 esp32、c3、s3）。IDF 的 linux 目标是协作式单核模拟，没有时序参考价值。性能只能做参数化建模，到货后再用 SP-01、SP-03 标定。

## 1. ESP32-S31 规格与支持状态

| 项 | 事实 | 层次 |
|---|---|---|
| CPU | HP 双核 32 位 RISC-V（RV32IMAFC+Zc+Zb），最高 320 MHz，5 级流水线；每核一个单精度 FPU；128 位 SIMD（XespV AI/DSP 扩展）**只有一个核支持**；硬件循环 XespLoop；LP 协处理核 RV32IMAC，40 MHz | 【官】数据手册 v0.5 |
| 性能 | CoreMark 2195.20（双核、320 MHz，6.86/MHz）。上游默认 320 MHz，空闲时可降到 80/160/240 MHz | 【官】【码】 |
| 片上存储 | HP SRAM 512 KB；LP SRAM 32 KB（深睡时保持）；ROM 320 KB | 【官】 |
| 缓存 | 手册：L1 I-cache 32 KB、各核私有，L1 D-cache 64 KB 共享，行长 64 B。IDF Kconfig 对 S31 设 `CACHE_L1_ICACHE_SIZE=0x4000`（16 KB）。两者口径不一致，可能是每核 16 KB | 【官】【码】未核实 |
| PSRAM | 封装内 8 线、1.8 V、DDR，最高 250 MHz；手册原文“PSRAM is not pinned out”；芯片型号 ESP32-S31NRV16（16 MB）和 NRV32（32 MB）；模组手册另有 N16R8V（8 MB）；地址空间最大 64 MB；IDF 可选 40/100/200/250 MHz，默认 200 | 【官】【码】 |
| Flash | 片外，最大 256 MB；与 PSRAM 走各自独立的 MSPI，可以并发访问 | 【官】【码】 |
| 可驱动 EPD 的接口 | LCD_CAM（i80 和 RGB 两种 LCD 模式）、PARLIO（TX 最多 16 线，也能做 i80）、DMA2D、PPA、JPEG 编解码。Plus 上的 EPDiy 走 LCD 外设，8 位总线，`bus_speed=20`，AXI-GDMA 要求 64 B 对齐 | 【官】【码】 |
| 音频 | 2 个 I2S（支持 TDM/PDM/PCM），带硬件 ASRC | 【码】soc_caps |
| SD | SDMMC 主机 2 个槽，4 位，支持 UHS-I，可直接 DMA 到 PSRAM。不过上游在 S31 上拒绝 PSRAM 直传，改用 4 KiB 内部中转，原因尚未确认 | 【码】 |
| USB | USB 2.0 HS OTG（UTMI PHY），另有 USB Serial/JTAG | 【官】 |
| 无线 | Wi-Fi 6：仅 2.4 GHz，1T1R，非 AP 模式只支持 20 MHz。BLE 5.4（LE Audio、Mesh 1.1）和经典蓝牙 BR/EDR。802.15.4：Thread 1.4、Zigbee 3.0、Matter | 【官】 |
| 其他 | 千兆 EMAC；CAN-FD×2；60 个 GPIO；QFN80（8×8 mm）；安全启动、XTS-AES 加密 Flash 与 PSRAM、密钥管理器 | 【官】 |
| 电流 | 模组 3.3 V、25 °C 预发布值：Wi-Fi TX 峰值 270–375 mA，RX 110–120 mA。Modem-sleep 下双核 WAITI：320 MHz 时 25/75 mA（外设时钟关/开），40 MHz 时 13/56 mA；双核跑 CoreMark：320 MHz 时 91/147 mA。Light-sleep 和 Deep-sleep 的数值没有取得 | 【官】WROOM-3 v0.7 |
| 勘误与限制 | 只有 v0.0 一个芯片版本。SPI-855：VDD_SPI 为 1.8 V 时无法从 Flash 启动，改用 3.3 V。ECDSA_DS-836：应改用 RSA_DS 安全启动。IDF soc_caps 中 DAC、APM 仍是 TODO | 【官】勘误 v0.2（2026-10-22）【码】 |
| IDF 版本 | v6.1（标签日期 2026-08-25）发布说明写“Added preview support for ESP32-S31”；FAQ 写 v6.1.1 起为正式支持；上游 sdkconfig 由 6.1.0 生成 | 【官】【码】 |

对设计的影响：
- 编解码、DSP 类任务要绑到有 SIMD 的那个核。esp_audio_codec 2.6.2 正是因为启用 PIE 汇编会把任务自动迁到 core 1，所以默认关闭了 S31 的汇编库。
- 编译参数为 `-march=rv32imafcb…_xesploop_xespv -mabi=ilp32f`【码 soc/project_include.cmake】。任何第三方运行时都必须使用 ilp32f ABI。

## 2. PSRAM 容量与第一版内存预算

**上游配置事实【码】**（来自 `sdkconfig.defaults.esp32s31` 和 `sdkconfig`）：
- `SPIRAM_MODE_OCT`、`SPEED_200M`，容量不写，由运行时探测。
- `MALLOC_ALWAYSINTERNAL=16`：大于 16 B 的 malloc 都优先分到 PSRAM。
- `MALLOC_RESERVE_INTERNAL=65536`：启动时划出 64 KiB 内部 DMA 池。这部分 DMA/INTERNAL 分配仍可使用，但普通 malloc 只把它当作最后兜底。
- 还开启了 `TRY_ALLOCATE_WIFI_LWIP`、`MBEDTLS_EXTERNAL_MEM_ALLOC`、`FREERTOS_TASK_CREATE_ALLOW_EXT_MEM` 和 `PMP_IDRAM_SPLIT`；PSRAM XiP 关闭。

**两条限制【官】【码】：**
- Flash 写入期间缓存关闭，PSRAM 也随之不可访问。因此写 Flash 的任务（NVS、OTA）必须使用内部栈。
- 上游用 `EpdFlashGuard` 把整段波形输出和 Flash 写入串行化。

**容量推断：** 芯片手册只列出 16 MB 和 32 MB 两档；上游渐进式 JPEG 设定“单次 8 MiB 临时预算、保留 2 MiB 空闲”（`progressive_jpeg.cc`）。两者合起来，判断为 16 MB 的可能性最大。

预算假设：PSRAM 16 MB，内部 SRAM 512 KB。

| 项目 | 内部 SRAM | PSRAM | 依据 |
|---|---|---|---|
| EPDiy 行队列（2 线程×128 行×304 B） | 76 KiB | — | 【码】render.c 与 line_queue.c；乘积为本次计算 |
| EPDiy bounce 缓冲 2×2.4 KB、1 KB LUT、行掩码与状态 | 约 9 KiB | — | 【码】lcd_driver.c、render.c |
| 高层帧缓冲：前帧、后帧（4bpp 各 406 KiB）和差分帧（2 倍） | — | 1.59 MiB | 【码】highlevel.c |
| 业务额外 4bpp 帧 2–3 张（预渲染上一页/下一页） | — | 0.8–1.2 MiB | 【估】每张 415,872 B |
| LVGL 绘制缓冲（I1 全帧，或 L8 64 行条带） | — | 约 102 KiB | 【码】lv_adapter_epdiy.cc |
| 阅读页预渲染（上游用 L8 整页） | — | 812 KiB/页（改 4bpp 为 406 KiB/页） | 【码】page_text_raster.cc；【估】 |
| Wi-Fi/lwIP：静态 RX 6 个、TX 6 个，每个约 1.6 KB，加驱动状态 | 40–80 KiB | 动态 RX 最多 32 个（优先放 PSRAM） | 【官】Kconfig 与 Wi-Fi 文档；【估】 |
| DMA 预留池 | 64 KiB | — | 【码】 |
| 1 路 TLS | 2–5 KiB | 25–45 KiB | 【官】mbedTLS 文档：默认约 42 KB，动态缓冲约 22 KB；上游输入 16K、输出 4K，放 PSRAM |
| Opus 编码+解码（16 kHz 单声道语音） | 仅 TCB | 约 30–60 KiB 堆，另加 26 KiB 栈 | 【官】codec 表（48 kHz 立体声：解码 26.6 KB，编码 29.4 KB）；【码】audio_service.h |
| MP3 或 AAC 解码器 | — | 28 或 51 KiB，另加约 20 KiB 栈 | 【官】esp_audio_codec |
| I2S DMA（每方向 4 块×240 帧×8 B，双向） | 约 15 KiB | — | 【码】bt_audio_codec.cc 与 audio_codec.h；乘积为本次计算 |
| 音频网络缓冲 | — | 256–512 KiB | 【估】128 kbps 时约 15–30 s |
| UI 字库（fontpack 从 Flash mmap） | 0 | 0 | 【码】 |
| 正文字形缓存（2000 字×150–250 B） | — | 0.3–0.5 MiB | 【估】依据 .ef 文件实测大小 |
| EPUB 当前章节（解压后的 XHTML）与排版索引 | — | 0.5–2 MiB（设上限） | 【估】上游单项读取上限为 8 MiB |
| FATFS 与 SD 内部中转 | 4–8 KiB | 每文件缓存放 PSRAM | 【码】docs/ap-file-transfer.md |
| 必须放内部的 TCB 与栈（写 Flash、睡眠、ISR 相关） | 30–60 KiB | 其余栈放 PSRAM | 【官】外部 RAM 文档；【估】 |
| 安全余量 | 50 KiB 以上 | — | 【码】firmware_memory.h；SRS SR-PL-04 要求 32 KB 以上 |

结论（估）：
- **PSRAM：** 合计约 4–6 MiB，在 16 MB 内很宽裕。
- **内部 SRAM：** 参考 IDF 给出的 ESP32-S3 数据（同为 512 KB SRAM，Wi-Fi 默认配置下约剩 184 KB 可用），S31 启动并初始化 Wi-Fi 后可能剩 180–280 KB。扣除 EPDiy、I2S 和 DMA 池后只剩约 50–150 KB，余量很紧。

优先采取的措施：
- 实测扫描欠载后，把 EPDiy 行队列从 128 行降到 32–64 行。
- Wi-Fi、lwIP、TLS 一律放 PSRAM，同一时刻只保持 1 路 TLS。
- 从第一天起沿用上游的 `FirmwareMemoryCheckpoint` 打点。

## 3. 文本与书籍渲染

**上游已有基础【码】：**
- 电脑端把 PDF/EPUB/MOBI/TXT 转成 `.ebook`。格式为 4 KB 独立 zlib 块，可流式读取，设计峰值 RAM 小于 64 KB（tools/ebook/SPEC.md）。
- 设备端可以流式解析 EPUB（zip_reader、html_content）。
- 正文用 SD 卡上的 `.ef` 位图字库：6009 个字形；25 px、2bpp 时 886,913 B；30 px、2bpp 时 1.19 MB；提供 20–50 px 各档。
- UI 用 fontpack（4.75 MiB，MiSans 25/30 px 加 emoji 和数学符号），从 5 MB 的 `font_data` 分区 mmap，零 RAM。
- 设备端用 FreeType 2.14.3 把 TTF 转成 `.ef`，工作任务栈 48 KB，放 PSRAM。

**字库覆盖面【官/社】：**
- GB 2312：6763 个汉字加 682 个符号。
- 《通用规范汉字表》：8105 字（一级 3500、二级 3000、三级 1605）。
- GBK：21886 个字符。
- 体积估算【估】：按 25 px、2bpp 每字约 147 B 计，8105 字约 1.2 MB，GBK 约 3.2 MB（指单一字号、单一位深）；1bpp 约为 2bpp 的 0.55 倍。

**字体引擎对比：**
- **LVGL tiny_ttf**（stb_truetype）：可以从文件流式读取，但文件要一直保持打开；默认缓存 256 个字形，对一页约 900 字（684 px 宽 ÷ 25 px ≈ 27 字/行 × 34 行）偏小；没有 hinting，小字号质量较差【官 LVGL 9.5】。
- **FreeType**：质量更好。CrossPoint 记录，2bpp 量化必须开 auto-hinter（约 50 KB 代码），否则细笔画会断【社】。

**推荐：**
- 正文用预渲染位图字库（沿用上游的 `.ef`/fontpack 思路）：速度快、行为确定，不占 RAM。
- 生僻字和临时字号由 FreeType 按需渲染，结果进 PSRAM 缓存。
- 由中枢为每本书生成只含所用字形的子集字库。这样能覆盖生僻字，而不必携带整个 GBK。

**EPUB 方案对比：**
- **CrossPoint**（MIT 许可，ESP32-C3 可用 RAM 约 380 KB）：expat SAX 流式解析章节、支持 CSS 子集；按章节排版后，把结果缓存到 SD 的 `.crosspoint/epub_<hash>/sections/N.bin`，记录换行位置和字词坐标；另有 `book.bin` 保存书脊和目录，CJK 断行规则仿照 UAX#14，西文用 Liang 连字符字典。它已支持 Metalio E-Ink 4（S3 版）。整体架构适合借鉴【社】。
- **crengine-ng**：依赖 freetype、harfbuzz、fribidi、libunibreak、utf8proc、zstd、png/jpeg/webp/jxl、fontconfig【社 Gentoo】，属于桌面级依赖，不适合 MCU（推断）。
- **litehtml**：C++/STL，带 gumbo DOM 和 CSS2/3 排版，本身不负责绘制。内存和复杂度都偏高，首版不建议。
- **推荐混合方案：** 中枢把 EPUB 规范化为精简结构（段落、标题、预抖动的图片、子集字库），设备端只做断行和分页。CJK 断行很便宜。分页结果按“书 + 字体 + 字号 + 视口 + 引擎版本”作键缓存到 SD，上游的 `.idx/.cpg/.pos` 就是这种做法。中枢预排版只作为加速手段，不作为离线依赖。

**中文断行：**
- 汉字不用连字符，每个字之间都是可断点，但要遵守行首、行尾禁则和标点悬挂规则（W3C clreq §6.1.1–6.1.3）。
- LVGL 9.5 的 `lv_text_is_a_word` 只把 CJK 字符当作单字词处理，没有禁则表；默认断行字符为 `" ,.;:-_)}"`【码】。阅读器需要自写断行器。

## 4. 音频播放与解码

**esp_audio_codec 2.6.2【官 README/CHANGELOG】：**
- 从 v2.5.0 起支持 S31。
- 解码格式：MP3、AAC、Opus、FLAC、Vorbis、ALAC、AMR、G711、ADPCM、SBC、LC3、G722。
- Simple Decoder 能自动处理 MP3/AAC/WAV/FLAC/M4A/TS 容器，但只支持流式播放，不支持 seek。
- ESP32-S3R8 实测（每项为堆内存 / CPU 占用）：

| 解码 | 堆内存 | CPU |
|---|---|---|
| MP3（44.1 kHz 立体声） | 28 KB | 8.17% |
| AAC-LC | 51.2 KB | 6.75% |
| Opus（48 kHz 立体声） | 26.6 KB | 5.86% |
| FLAC | 89.4 KB | 8.0% |

  解码任务栈约 20 KB。

**S31 上的估算【估】：**
- 320 MHz RISC-V 单核占用估计与 S3 同量级，单路低于 15%。16 kHz 单声道语音会低得多。
- PIE 汇编库可以进一步降低 AAC/Opus 的负载，但会把任务固定在有 SIMD 的核上。

**流式播放：**
- 链路：`esp_http_client`（用 Range 实现跳转）→ PSRAM 环形缓冲 → 解码 → 硬件 ASRC 重采样 → I2S。
- 带宽：音频码率 64–128 kbps，远低于 4G 链路上限（见 §7）。
- esp-gmf 播放器的 `idf_component.yml` 没有给出目标列表，是否支持 S31 未核实。

**关键约束【码】：**
- 上游 I2S 作为从机，由 BH1098 提供时钟，采样率固定为 16 kHz（32 位立体声槽）。
- 这个采样率适合语音和听书，听音乐不理想。BH1098 能否运行在 44.1/48 kHz 未核实，需要尽早确认。

## 5. 可安装 app 的运行时（v0.4）

| 运行时 | S31 现状 | 体积 / 内存 | 隔离能力 | 与 LVGL 的绑定 |
|---|---|---|---|---|
| ELF loader 2.0.0（2026-10-14） | 目标列表包含 esp32s31，可在 PSRAM 中执行；`PMP_IDRAM_SPLIT=y` 时只能装入 PSRAM【官】 | 文档未给数据 | 无：原生代码可以写任意内存，出错会拖垮整个系统 | 原生 C 直接调用 |
| WAMR | `idf_component.yml` 目标列表不含 s31。ESP-IDF 构建脚本只给 P4 选 `RISCV32_ILP32F`，其余选 ILP32，而 S31 编译用 ilp32f，因此需要补丁。AOT 可执行内存走 `MALLOC_CAP_EXEC`（内部），与 PMP 分割和内部 RAM 紧张都有冲突；解释器模式可行【码】 | Cortex-M4F 实测：fast-interp 约 58.9K，classic 约 56.3K，AOT 运行时约 29.4K，libc-builtin 3.7K【官】 | 线性内存有边界检查；可开指令计量 | 无现成绑定，需要自定义 host API |
| Lua 5.4/5.5 | espressif/lua 5.5.1，在 idf-extra-components 中，标为“Supports all targets”；官方博客实测过 ESP32 和 C3【官】 | 小（未取得官方 KB 数）；可通过自定义分配器给每个 app 限额 | 依靠受限 `_ENV`；`lua_sethook` 计数钩子可限 CPU；只加载文本块（手册：恶意构造的二进制块会使解释器崩溃）【官】 | luavgl（仍在开发，有 PC 模拟器） |
| Berry | 在 ESP32 上经 Tasmota 验证成熟；S31 未见 | 核心代码小于 40 KiB，堆小于 4 KiB（M4 平台） | 与 Lua 类似 | Tasmota 映射覆盖 LVGL 功能 98% 以上（LVGL 8 时期文档） |
| MicroPython | esp32 端口支持列表里没有 S31，推荐 IDF v5.5.5；PMP 分割开启时会关闭 RV32 原生代码发射器【码】 | 较大 | 弱 | lv_micropython |
| PikaPython、JerryScript、Elk、QuickJS | 均无 S31 适配记录 | Pika 最低 4 KB RAM；Jerry 小于 64 KB RAM、小于 200 KB Flash；Elk 约 20 KB Flash；QuickJS 的 hello world 约 210 KiB x86 代码【官】 | QuickJS 可设内存上限，其余隔离都弱 | Pika 有绑定，其余没有 |

**推荐：Lua，app 不直接操作 LVGL，而是返回声明式视图描述，由系统原生渲染器负责墨水屏刷新。**
- 有官方组件，并支持全部目标，移植风险最低。
- 可以通过“每 app 一个分配器 + 指令计数钩子 + 受限 `_ENV` + 只收文本块”实现软隔离，满足资源准入要求。
- 体积小，主机端可以先测试。
- 与 D-08 的声明式页面阶段共用同一份视图格式。
- 若以后需要强隔离或多语言，再评估 WAMR 解释器。ELF loader 只装自家签名的性能模块。

## 6. 存储引擎与掉电安全

**IDF 官方对比【官 file-system-considerations】：**
- FatFS 没有掉电保护，IDF 默认保留两份 FAT 表来缓解。
- LittleFS 自带掉电保护和磨损均衡，RAM 占用固定，也能用于 SD（IDF 5.0 起），但 PC 无法直接读取。
- SPIFFS 已停止维护。
- NVS 适合低频写入的配置；键名最长 15 字符，每个分区最多 254 个命名空间；文档明确说，在电池等超出规格的供电下不能保证一致性，需要自备恢复路径。
- 其他组件：esp_littlefs 1.23.1；FlashDB 提供 KVDB 和 TSDB，有掉电保护、磨损均衡，RAM 占用约为 0，并支持文件模式。

**SQLite：**
- 官方说明：库体积 300–500 KiB；最低可在 4 KiB 栈、约 100 KiB 堆下运行【官】。
- esp32-idf-sqlite3 这个移植版的问题【码】：基于 SQLite 3.25.2（2018 年）；编译选项包含 `SQLITE_NO_SYNC=1`（不刷盘，因此不掉电安全）和 `THREADSAFE=0`；CMake 写死了 Xtensa 专用的 `-mlongcalls`，RISC-V 无法直接编译。
- LittleFS 随机改写大文件很贵（写时复制，见 littlefs DESIGN）。
- 结论：首版不用 SQLite。以后若查询需求增长，先在主机上评估。

**建议布局：**

| 数据 | 存放位置 | 写入方式 |
|---|---|---|
| 设备与系统配置（Wi-Fi、设备 ID） | 内部 Flash NVS（与 IDF 自用分区分开） | 低频 |
| 成员设置 | LittleFS `/sys/m/<id>/settings.cbor` | 先写临时文件，再 rename 原子替换 |
| 阅读和听书进度 | LittleFS 每成员追加日志 | 延迟批量写入，不超过 5 s（SR-PL-08）；定期压缩 |
| 笔记 | LittleFS 追加日志加快照 | 写入后立即同步；压缩时生成新快照 |
| 单词复习（SRS）状态 | LittleFS：卡片表为定长记录文件，复习记录走追加日志 | 每日压缩 |
| 事件日志 | FlashDB TSDB，或环形日志文件 | 有界，满了覆盖最旧记录 |
| 书、音频、各类缓存 | SD 卡 FAT32（兼容 USB 大容量存储模式） | 全部可重建或重新下载 |
| 备份快照 | SD `/backup/<日期>/` 加中枢服务器 | 打包时附校验和 |

## 7. 网络

**协议组件：**
- WebSocket 客户端在 esp-protocols 中（esp_websocket_client 1.8.0，需要 IDF 5.0 以上）。
- MQTT 自 IDF v6.0 起移出核心，改为 espressif/mqtt 组件。
- HTTP/2 可用 sh2lib 1.1.0 加 nghttp 1.69.0（在 idf-extra-components 中），但没有必要。
- 推荐“一路长连 WSS + 下载走 HTTPS”。

**4G 模组 NT26【官 利尔达】：**
- 全系为 LTE Cat.1 bis。FDD 峰值下行 10 Mbps、上行 5 Mbps；TDD 为 8.96/3.1 Mbps。
- 空闲电流：PF=64 时 3.3–4.1 mA，PF=128 时 0.17–0.27 mA。
- KCN 系列（EC716E）不支持 VoLTE；FCN D 系列可选 VoLTE。

**上游数据通路【码】：**
- 不用 PPP，也不用 AT socket，而是自定义的“UART 以太网”帧协议：4 字节帧头，以太网帧和 AT 命令复用同一条 UART。NT26 板卡配置为 2 Mbaud，驱动默认 3 Mbaud。
- 模组开启 NAT 模式（`AT+ECNETCFG="nat",1,...`），ESP 端得到 iot_eth 网卡，lwIP 和 TLS 都在 ESP 上运行，因此 WebSocket、TLS 代码与 Wi-Fi 下完全相同。
- 用 MRDY/SRDY 握手实现低功耗，模组空闲 3 s 后休眠。
- 备选是 esp_modem 的 PPPoS 加 CMUX，但需要模组底包支持。

**吞吐与时延【估】：**
- 2 Mbaud、8N1 约 200 KB/s，扣掉帧头后实际约 1.2–1.5 Mbps，远低于 Cat.1 bis 的峰值，但对听书和 AI 语音足够。
- Cat.1 往返时延按 50–150 ms 估计（未核实）。

**功耗【码】：**
- 上游开启了 `PM_ENABLE`，但 `FREERTOS_USE_TICKLESS_IDLE` 关闭，所以自动轻睡不生效。
- esp-ml307 的 README 写明低功耗模式需要同时开启这两项。上游还提供了 30/60/120 s 的网络保活时长选项。

## 8. OTA 与 16 MB 分区

**上游现状【码】：**
- 分区：ota_0 和 ota_1 各 5 MB；font_data 5 MB；model 和 resources 各 400K（SPIFFS）；nvs 16K；coredump 64K。
- `BOOTLOADER_APP_ROLLBACK_ENABLE` 没有开启。
- IDF v6.1 文档中回滚默认关闭；master 已改为默认开启，并提供两种确认模式【官】。
- S31 支持防回滚和恢复 bootloader（soc_caps）。

**建议（估，需先用 `idf.py size` 确认镜像大小）：**

| 分区 | 大小 |
|---|---|
| nvs（IDF 自用） | 24K |
| otadata | 8K |
| phy | 4K |
| ota_0 / ota_1 | 各 4.5M |
| font_data（mmap，现有包 4.75 MiB） | 5M |
| sys（LittleFS） | 约 1.7M |
| nvs_app | 64K |
| coredump | 64K |

- 开启回滚，并由 app 决定确认点：启动后自检屏幕、存储和网络通过，再标记镜像有效。
- 字库和资源分区没有空间做 A/B：先下载到 SD，校验哈希，在 `EpdFlashGuard` 保护下写入；app 镜像里内置最小字库作为兜底。
- Flash 写入会让 PSRAM 暂时不可访问。可以评估两种做法（都尚未在 S31 上验证）：SoC 支持的 Flash 自动挂起（上游未开），或从 PSRAM XiP（IDF 说明它能避免用户 Flash 操作时关缓存，但会增加 PSRAM 总线负载）。

## 9. 到货前的主机端开发

**可用工具及其局限：**
- **QEMU：** IDF 的 `qemu_ext.py` 只支持 esp32、esp32c3、esp32s3，对 S31 会直接报“QEMU is not supported”【码】。即使用 C3 跑，也没有 Wi-Fi、I2S、I2C、USB 和 GPIO 矩阵，只有虚拟以太网和虚拟 RGB 帧缓冲，也不是周期精确的【官 esp-toolchain-docs】。最多用来对 RISC-V 构建做功能冒烟测试。
- **IDF linux 目标**（preview）：基于 FreeRTOS POSIX 模拟，同样可在 macOS 上运行。调度是协作式，任务不调用内核 API 就不会被抢占；时间粒度等于一个 tick；只模拟单核。可模拟的组件包括 esp_http_client、esp_tls、mbedtls、lwip、mqtt、nvs_flash、fatfs、spiffs、esp_partition、esp_event、esp_netif、heap、log、json；没有 Wi-Fi、LCD、I2S、SDMMC 驱动，driver 组件只有 mock【官】。
- **LVGL PC 模拟器：** lv_port_pc_vscode，基于 SDL2，`lv_sdl_window_create` 即可建窗口。上游自带一个先例：`tools/ebook/emulator` 是 LVGL 墨水屏阅读模拟器（可编译为 WASM 或 SDL），通过 `esp_compat` 桩与设备共用 BookSession 和 EbookDocument 代码【码】。

**推荐工作流（Mac 上）：**
1. **硬件抽象层：** 定义 `display_epd`、`input`、`audio`、`net`、`storage`、`power`、`clock` 等接口，各做两套实现：`port/esp32s31` 和 `port/host`（SDL 加 POSIX）。刷新调度、数据存储、同步协议、能力目录、Lua 运行时都写成可移植 C/C++。主机上用原生 CMake 编译并跑单元测试；需要验证 FreeRTOS 语义或 IDF 网络栈时，再用 linux 目标。
2. **墨水屏模拟层：** 把 684×1216 量化为 I1 或 4bpp，记录每个像素的当前值和目标值，并模拟以下效果：按模式计算的延迟、GC 类全刷的反相闪烁、残影累积（全刷时清零）、刷新期间区域忙碌并暂停输入。同时记录“输入 → 首次反馈 → 稳定可读 → 落盘”四个时间点。
   延迟参数【估】：每个波形相位约等于一次全屏扫描。按 20 MHz 时钟、每行 304 字节估算，每相约 10–16 ms；上游注释“128 行约 2–3 ms”与此吻合。由此 30 相约 0.3–0.5 s，18–25 相约 0.2–0.4 s，短 DU 约 0.1 s。这些都是可配置参数，到货后用 SP-01 慢动作录像标定。
3. **内存与 CPU 建模：** 在主机上用带能力标志的 malloc 垫片，分别限额“内部堆约 120 KB”和“PSRAM 约 12 MB”，并统计最大连续块。CPU 时间在主机上不可信，只记录工作量（字形数、解码帧数），到货后换算。
4. **中枢与协议：** 本机运行中枢，模拟器走真实的 WSS 和 HTTPS。4G 条件用 macOS 的 Network Link Conditioner 模拟：限速约 1.5 Mbps，往返时延 50–150 ms（估）。
5. **CI：** 每次提交执行 `idf.py --preview set-target esp32s31 && idf.py build && idf.py size`，跟踪静态 IRAM/DRAM 和镜像大小，保证移植工程一直能编译。
6. 如需提前在真机上测 CPU 和内存，可以购买 S31 开发板（官方 FAQ 写明已可购买）。这需要花钱，须由用户决定。

## 10. 未能核实

- 实物 PSRAM 容量（16 MB 是推断）、芯片具体型号，以及与 sdkconfig 对应的实物版本。
- S31 的 Light-sleep、Deep-sleep 电流（数据手册中对应表格未能取得）；I-cache 实际是 16 KB 还是 32 KB。
- v6.1.1 何时发布：截至 2026-11-01，git 上只有 v6.1。
- 内部 SRAM 实际空闲量和 Wi-Fi 实际占用：§2 中的数字依据 S3 的文档数据加估算。
- BH1098 能否运行 44.1/48 kHz；esp-gmf 是否支持 S31；esp_audio_codec 在 S31 上的 CPU 实测。
- WAMR 在 S31 上的 ABI 补丁和 AOT 可执行内存；Lua、Berry 在 S31 上的实测；SQLite 移植能否编译。
- NT26 的具体子型号和固件；UART 以太网实测吞吐；Cat.1 时延；PPP 是否可用。
- EPD 各模式的真实时序与残影；上游在 S31 上禁用 SD 的 PSRAM 直接 DMA 的原因。
- 有几个网页因工具拒绝没有直接读取，已改用对应的 git 源文件或其他来源代替：docs.espressif.com 上的 S31 外部 RAM 页、espressif.com 的 S31 产品页、components.espressif.com 的 esp_audio_codec 页、gitlab 上的 crengine-ng。

## 11. 来源

- **数据手册与芯片状态：**
  - [ESP32-S31 数据手册 v0.5](https://documentation.espressif.com/esp32-s31_datasheet_en.pdf)
  - [WROOM-3 模组手册 v0.7](https://documentation.espressif.com/esp32-s31-wroom-3_datasheet_en.pdf)
  - [量产公告](https://www.espressif.com/en/news/ESP32_S31_Mass_Production)
  - [S31 FAQ](https://esp32-s31.espressif.com/en/faq)
  - [勘误](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32s31/esp-chip-errata-en-master-esp32s31.pdf)
  - [v6.1 发布说明（转载）](https://newreleases.io/project/github/espressif/esp-idf/release/v6.1)
- **ESP-IDF 源文件与文档：**
  - [constants.py（v6.1）](https://github.com/espressif/esp-idf/blob/v6.1/tools/idf_py_actions/constants.py)
  - [soc_caps.h](https://github.com/espressif/esp-idf/blob/master/components/soc/esp32s31/include/soc/soc_caps.h)
  - [project_include.cmake](https://github.com/espressif/esp-idf/blob/master/components/soc/project_include.cmake)
  - [external-ram.rst](https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/external-ram.rst)
  - [host-apps.rst](https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/host-apps.rst)
  - [qemu.rst](https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/tools/qemu.rst)
  - [qemu_ext.py](https://github.com/espressif/esp-idf/blob/master/tools/idf_py_actions/qemu_ext.py)
  - [file-system-considerations.rst](https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/file-system-considerations.rst)
  - [mbedtls.rst](https://github.com/espressif/esp-idf/blob/master/docs/en/api-reference/protocols/mbedtls.rst)
  - [ota.rst（v6.1）](https://github.com/espressif/esp-idf/blob/v6.1/docs/en/api-reference/system/ota.rst)
  - [Wi-Fi 性能文档](https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/wifi-driver/wifi-performance-and-power-save.rst)
  - [mqtt.rst](https://github.com/espressif/esp-idf/blob/master/docs/en/api-reference/protocols/mqtt.rst)
  - [lwip.rst](https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/lwip.rst)
  - [QEMU 功能表](https://github.com/espressif/esp-toolchain-docs/blob/main/qemu/README.md)
- **组件：**
  - [esp-protocols](https://github.com/espressif/esp-protocols)
  - [idf-extra-components](https://github.com/espressif/idf-extra-components)
  - [esp_audio_codec](https://github.com/espressif/esp-adf-libs/tree/master/esp_audio_codec)
  - [esp-gmf](https://github.com/espressif/esp-gmf)
  - [elf_loader](https://github.com/espressif/esp-iot-solution/tree/master/components/elf_loader)
- **运行时：**
  - [WAMR](https://github.com/bytecodealliance/wasm-micro-runtime)
  - [WAMR 组件 readme（体积数据）](https://components.espressif.com/components/espressif/wasm-micro-runtime/versions/2.4.0~1/readme)
  - [MicroPython esp32 端口](https://github.com/micropython/micropython/tree/master/ports/esp32)
  - [Berry](https://github.com/berry-lang/berry)
  - [Tasmota Berry LVGL 文档](https://lvgl.io/docs/open/integration/frameworks/tasmota-berry)
  - [Lua 源码与手册](https://github.com/lua/lua)
  - [乐鑫 Lua 博客](https://developer.espressif.com/blog/using-lua-as-esp-idf-component-with-esp32/)
  - [luavgl](https://github.com/XuNeo/luavgl)
  - [PikaPython](https://github.com/pikasTech/PikaPython)
  - [JerryScript](https://github.com/jerryscript-project/jerryscript)
  - [Elk](https://github.com/cesanta/elk)
  - [QuickJS 文档](https://fuchsia.googlesource.com/third_party/quickjs/+/refs/heads/main/doc/quickjs.html)
- **存储：**
  - [SQLite 体积说明](https://www.sqlite.org/docsrc/info/8da1a575d77dc455)
  - [esp32-idf-sqlite3](https://github.com/siara-cc/esp32-idf-sqlite3)
  - [FlashDB](https://github.com/armink/FlashDB)
  - [esp_littlefs](https://github.com/joltwallet/esp_littlefs)
  - [littlefs DESIGN](https://github.com/littlefs-project/littlefs/blob/master/DESIGN.md)
- **阅读与排版：**
  - [CrossPoint Reader](https://github.com/daveallie/crosspoint-reader)
  - [crengine-ng 依赖（Gentoo）](https://packages.gentoo.org/packages/app-text/crengine-ng/dependencies)
  - [litehtml](https://github.com/litehtml/litehtml)
  - [LVGL v9.5 文档](https://github.com/lvgl/lvgl/tree/v9.5.0/docs/src)
  - [lv_port_pc_vscode](https://github.com/lvgl/lv_port_pc_vscode)
  - [W3C clreq](https://www.w3.org/TR/clreq/)
  - [GB 2312](https://en.wikipedia.org/wiki/GB_2312)
  - [GBK](https://en.wikipedia.org/wiki/GBK_(character_encoding))
  - [通用规范汉字表报道](https://www.chinanews.com/cul/2013/08-21/5187039.shtml)
- **4G 模组：** [利尔达 NT26 文档](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/index.html)
- **上游代码（固定提交）：**
  - [README](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/ad8edf4150a688737c0d7c00cae4413079b262ef/README.md)
  - 配置与分区：`sdkconfig`、`sdkconfig.defaults(.esp32s31)`、`partitions/v1/16m.csv`、`main/idf_component.yml`
  - 显示：`main/boards/metalio-e-ink4-plus/display/{highlevel.c, render.c, output_common/line_queue.c, output_lcd/lcd_driver.c}`、`main/display/lv_adapter_epdiy.cc`、`main/display/epd_flash_guard.h`
  - 内存与图片：`main/firmware_memory.h`、`main/reader/jpeg/progressive_jpeg.cc`
  - 4G：`main/boards/common/board/{uart_eth_modem.h/.cc, nt26_board.cc}`
  - 音频：`main/audio/audio_service.h`、`main/boards/common/audio/bt_audio_codec.cc`
  - 工具与文档：`tools/ebook/{SPEC.md, emulator/README.md}`、`tools/epdfont`、`tools/fontpack`、`docs/ap-file-transfer.md`
