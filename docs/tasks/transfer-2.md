# transfer-2：传书整块换成 kiikoread 改版——中文网页管理、上传前在浏览器里生成 TXT 目录、批量操作；原厂的网页文件管理器原位替换掉，我们的授权和家人空间照旧

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7、9、11、12 条；第 8 条不碰设备）。

## 为什么

用户 2026-11-12：“例如原厂的传书和阅读器这些我认为是做的比较垃圾的 在所有项目中排最后的”；“好不好不是看移植工作量小 是看功能成熟度 用户使用体验 这些”；“不需要在乎开源协议 我们都是自用 后续也可以开源”（D-129）；D-131：传书是先做到成熟可用的现有应用。

调研（`docs/tasks/replan-reader/REPORT.md` 第 4 节传书排名、第 128 行的建议）：开源里 kiikoread 改版第一——中文网页能管目录和任意文件（改名、复制、新建、删除、批量下载和删除）、上传有进度能取消、**上传 TXT 前在手机或电脑的浏览器里生成目录**、已有的书能批量补目录，入口有网页、热点、U 盘。原厂版排最后。

现状：真机上传书用的是原厂的网页文件管理器（`transfer-s31-1` 搬进来的 `ports/esp32s31/vendor/ap_transfer/`，S31 上由 `ports/esp32s31/hal_transfer_s31.c` 起服务）；授权码、有效期、错三次关闭、按家人限定根目录、回执和 AI 能力都在 `system/services/file/transfer.c`，平台接口是 `platform/include/ink/hal_transfer.h`（`hal_transfer_server_start(policy, &port)`，策略回调由 `transfer.c` 提供）。页面是 `system/ui/shell/transfer_pages.c`。

## 成熟来源（搬代码）

- kiikoread 改版：`$INK_DEPS/../ink-hw/upstream/oss/kiikoread-xiugai`，提交 `746ce31`：
  - `components/read_pico_transfer/read_pico_transfer.c`（4,224 行，`esp_http_server`；路由表在 2500-2522 行和 4149-4167 行：`/`、`/info`、`/upload`、`/file-upload`、`/file`、`/files`、`/books`、`/titles` 等）及同目录的头文件、`transfer_http_time.c`、`transfer_ntp_time.c`；
  - `components/read_pico_transfer/upload.html`（34,483 字节，中文网页；`TOC_RULES` 到 `sendToc` 之间是浏览器里生成目录的函数）；
  - FTP 在同一文件 2156 行起（和网页二选一、匿名登录）。
- 许可：该仓库的 `LICENSE` 写的是 kiikoread 的许可范围。用户说不看许可证（D-129），照原样带上许可文件，`SOURCE.md` 写清许可范围，以后要公开发布时由主会话处理。
- 已有的离线检查：`docs/tasks/replan-reader/check-transfer.cjs`（用合成文件跑网页里的目录函数，7 种编码 7/7 通过），本任务照用。

## 要做的

1. **原文入库**：上面这些文件整文件拷进 `ports/esp32s31/vendor/read_pico_transfer/`，旁边 `SOURCE.md`（来源提交、每个文件 SHA-256、改了哪几行和为什么）和许可文件；网页照 `transfer-s31-1` 对原厂网页的办法压缩后嵌进固件。删掉 `ports/esp32s31/vendor/ap_transfer/`（原位替换，二维码生成 `qrcodegen` 如果只在那里，挪到新目录继续用）。
2. **接我们的规矩**（胶水；原文尽量不改，非改不可的行列进 `SOURCE.md`）：
   - 授权：沿用 `transfer.c` 的一次性授权码、有效期、错三次关闭、绑定家人和回执；网页第一次进入要输授权码（或扫码带上），之后每个请求都校验，授权结束网页立即失效。FTP 要保留就用授权码当密码，不许匿名；做不完就不开 FTP，写进报告。
   - 根目录：只到当前授权家人能写的空间，不是整张卡；所有读写走现有文件服务和存储准入，不直接 `fopen` 写卡；删除照现有文件服务的策略。
   - 网页生成的目录：照原样存（格式写进报告），存放位置在书旁边；我们的阅读器读不读这个目录文件由以后的 `reader-2` 决定，这次不改阅读器。
   - 原版里改系统设置的接口——`/wifi`（网页改 Wi-Fi）、`/clock`（网页对时）、`/signature`——不注册，写进报告“要用户定”（要的话改成调我们的设置能力，按家人和策略走）。`/font-upload`、`/image-upload`、`/music-upload` 当成上传到当前家人对应目录的普通文件上传；装字体这类系统级动作不做，写进报告。
   - 把原版对它自己的 SD 卡、NVS、电源、联网模块的依赖换成我们现有的接口；传输期间按现有电源规则不睡。
3. **平台接口**：`hal_transfer.h` 的“服务器开始/停止 + 策略回调”不变（设计在 `transfer-s31-1`），只是 S31 上起的服务器换成新的。宿主机模拟器继续用现有的极简页面测授权和家人规则。
4. **入口**：已连 Wi-Fi 时在本机地址上提供；没有 Wi-Fi 时开热点（热点平台缺口归 `hotspot-1`，它没合入就保持现在的行为，写进报告）；传书页上有“插电脑当 U 盘”的入口（调现有的 `usb.mount` 能力，U 盘本身归 `usb-disk-1`）。
5. **传书页面**（`transfer_pages.c`）：照新设计语言重排——网址、授权码、二维码、连接方式、已经收到的文件列表；授权码和热点密码不进快照、不给 AI。
6. **AI 接入**：`transfer.start`、`transfer.stop`、`transfer.status` 照旧，人和 AI 共用；状态里有连接方式、网址、已收到的文件。

## 共同要求（页面部分）

- 新设计语言（`design-lang-1`）：只用令牌和共用控件（按钮、列表行、空状态）；能点的不小于 108 px；一屏最多一个主按钮；行间细分隔线、组间留白；图标不套圆圈；没收到文件时不画文件列表那块。不写死像素和颜色（`ui-scale` 检查）。旧画板的对照分数不作验收。
- 不变的规矩：家人隔离、孩子模式（孩子能不能开传书照现有规则）、确认卡只给 D-105 的高风险动作、少刷新（D-127：收到一个文件只刷列表新增那一行）、按下反馈 D-31、授权码和密码不给 AI 不进日志。
- 文字大小：跟系统“文字大小”一起缩放，默认和最大一档各出一套截图。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成后提交；新能力进 `tests/integration/ai_surface.c` 的两张表（给 AI 的写清必填参数，不给 AI 的写理由），新文案的英文加进 `assets/i18n/en.tsv`，这两个文件都只在末尾追加本任务的行；合并冲突由主会话处理。

## 范围和预算

- 只改：新增 `ports/esp32s31/vendor/read_pico_transfer/`、删除 `ports/esp32s31/vendor/ap_transfer/`、`ports/esp32s31/hal_transfer_s31.c`、`system/services/file/transfer.c`、`ports/sim/hal/hal_transfer_sim.c`（只在测试需要时）、`system/ui/shell/transfer_pages.c`、`ports/esp32s31/main/CMakeLists.txt` 和 S31 测试 CMake 里传书自己的几行（只在末尾追加或改自己那几行）、传书测试、`docs/tasks/transfer-2/`。
- 不碰：`ports/esp32s31/hal_ws_s31.c`、`hal_usb_s31.c`（`usb-disk-1`）、热点（`hotspot-1`）、文件管理应用（`files-3`）、阅读器（`graft-mogo-1`、旧阅读）、`hal_wifi_s31.c`（要改写进报告）。
- 胶水不超过 500 行（拷来的原文不算）；固件净增不超过 40 KB（删掉原厂那套以后算）。
- 起步：18:30 合并后的主线（含 `hub-link-1`；`usb-disk-1` 合入了就在它上面）。

## 验收

1. **照 kiikoread 改版的用法走一遍**（S31 宿主机测试里替身网络层跑新服务器，照 `transfer-s31-1` 的测试做法；每一步断言）：打开首页要授权码 → 输错 3 次授权关闭 → 新开授权、输对 → 列出当前家人的书目录 → 上传一本 GBK 的 TXT（带网页生成的目录文件）→ 上传中取消一次，卡上不留半个文件 → 再上传成功 → 改名 → 批量删除两本 → 授权到期后任何请求都被拒。另测：请求越出家人空间的路径（`../`、别的家人的目录）被拒；空间不足、文件名不合法有明确错误；没注册的 `/wifi`、`/clock` 返回 404。反向对照：去掉每个请求的授权校验，授权到期那条必须失败。
2. **网页里的目录函数**：`node docs/tasks/replan-reader/check-transfer.cjs`（把读的文件改成你拷进来的 `upload.html`）7 种编码 7/7 通过；没有 node 的环境明确跳过。
3. **原有测试**：`transfer-s31-1` 和 `wireless-transfer` 留下的授权、过期、家人、满盘测试不改断言照过。
4. **改前改后截图**：传书页（等待连接、已收到文件）默认和最大文字大小各一套；网页本身在电脑浏览器里的样子截一张（用 `docs/tasks/replan-reader/serve-transfer.py` 一类的本机预览，不联网），单张不超过 300 KB。
5. 主工程普通全量 ctest 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。报告写固件净增（嵌入的网页压缩前后）、产品代码行数、`gcccheck` 结果、内部内存和 PSRAM 的变化（http 服务器的栈和缓冲放哪），以及上机核对清单（主会话用手机打开网址、输码、上传一本书、取消一次、看设备上收到）。
