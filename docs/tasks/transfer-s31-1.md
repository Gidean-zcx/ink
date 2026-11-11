# transfer-s31-1：Wi-Fi 传书和热点传书——用原厂的网页文件管理器，接我们的授权和成员空间

先读 `docs/tasks/vendor-common.md`（公共规矩，第 1～3、6、7、9～12 条；第 8 条不碰设备），总规则 `docs/tasks/codex-rules.md` 第 9、10 节优先。

用户 2026-11-11：“这些功能我希望你能尽量走移植复用 不要重复造轮子 改造和适配我们的系统”。传书在用户的系统优先级里排第二（阅读 > 传书 > 在线书源 > 锁屏 > 声音蓝牙 > AI）。功能清单见 `docs/tasks/reuse-crosspoint-1/picks.md`。

现状：`system/services/file/transfer.c`（219 行）是一个极简 HTTP 服务：一个授权码、一个成员、一次一个文件，传进 `/me/收件箱/`，网页只有一个上传框；S31 上 `hal_transfer_*` 还是桩（`ports/esp32s31/hal_stubs.c`），所以真机上传书不能用。

原厂 874538e 有成熟的同硬件实现（MIT）：`main/display/screen/cloud_screen/ap/` 下 `ap_transfer.{h,cc}`（1371 行，`esp_http_server`：文件列表、存储空间、下载、新建文件夹、删除、改名、移动、WebSocket 上传）、`fileman.html`（1251 行网页文件管理器，`fileman.en.json` 英文）、`ap_request_worker.{h,cc}`、`ap_qr.{h,cc}` 和 `qrcodegen.{c,h}`（Nayuki，MIT），热点的开法在 `main/boards/common/board/wifi_board.cc` 和双网板级代码里。

## 要做的

1. **原文入库**：上面这些文件整文件拷进 `ports/esp32s31/vendor/ap_transfer/`，旁边 `SOURCE.md` 和许可证；网页照原厂的办法压缩后嵌进固件。
2. **接我们的规矩**（胶水，原厂文件尽量不改，非改不可的行在 `SOURCE.md` 列出）：
   - 授权：沿用 `transfer.c` 的一次性授权码、有效期、错三次关闭、成员绑定和回执；网页第一次进入要输授权码（或扫码带上），之后每个请求都校验，授权结束网页立即失效；
   - 根目录：不是整张卡，而是当前授权成员能写的空间（现有文件服务的成员路径；默认落在收件箱，书放进书架目录由现有导入规则决定）；所有读写走现有文件服务和存储准入，不能绕过去直接用 `fopen` 写卡；删除照现有文件服务的策略（能进回收站就进回收站）；
   - 把原厂对 `SdCardManager`、`sd_paths`、`book_library`、`power_policy`、`wifi_station`、双网板级的依赖换成我们现有接口；传输期间按现有电源规则不睡；
   - 联网：已连 Wi-Fi 时在本机地址上提供；没有 Wi-Fi 时照原厂开热点（`192.168.4.1`），用户关掉传书或授权到期时关热点、恢复原来的 Wi-Fi 状态；飞行模式下不开热点。热点名称和密码不进日志。
3. **平台接口**：`hal_transfer.h` 现在是逐字节的 socket 接口。设计一个最小改法让原厂服务器在 S31 上跑、宿主机模拟器仍能用现有极简页面测试授权和成员规则（例如在 `hal_transfer.h` 加“文件管理服务器开始/停止”，策略回调由 `transfer.c` 提供）；系统层的授权、成员、回执、AI 能力只有一份。报告写清选了哪种、为什么。
4. **二维码**：用原厂 `qrcodegen` 生成网址加授权码的二维码，给传书页面一个画二维码的函数。`system/ui/shell/transfer_pages.c` 正被 `ui-fix-a` 改：如果你的基线里已经有 “Merge ui-fix-a” 的提交，就在那里接上二维码；没有就只交函数和一行接入说明，由主会话接。
5. **AI 接入**：`transfer.start/stop/status` 照旧（人和 AI 共用），状态里加连接方式（Wi-Fi 或热点）、网址、已收到的文件；授权码和热点密码不给 AI、不进快照。

## 范围和预算

- 只改：新增 `ports/esp32s31/vendor/ap_transfer/`、`ports/esp32s31/hal_transfer_s31.c`（新增，替掉桩）、`ports/esp32s31/hal_stubs.c` 里传书那几行、`ports/esp32s31/hal_wifi_s31.c` 里开关热点需要的最少几处、`platform/include/ink/hal_transfer.h`、`ports/sim/hal/hal_transfer_sim.c`、`system/services/file/transfer.c`、`ports/esp32s31/main/CMakeLists.txt` 和 S31 测试 CMake 里传书自己的几行、传书测试、生成的 `docs/system/capabilities.md`、`docs/tasks/transfer-s31-1/`。
- 不碰别的任务在改的文件（阅读器、键盘、输入框、短信电话、锁屏、设置页、4G、电源、蓝牙、相机）。
- 胶水新增不超过 500 行（拷来的原文不算）。

## 验收

1. 宿主机测试：授权码对/错/错三次/过期、成员切换后旧授权失效、上传落在成员空间且经过存储准入、空间不够时拒绝、不能用 `../` 逃出根目录、删除走现有策略；S31 宿主机测试里用假 `httpd` 测原厂路由经胶水后的同样规则；反向对照。
2. 主工程普通全量 ctest 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件增大多少。
3. 报告写“上机核对清单”（主会话在真机开传书：连 Wi-Fi 时用电脑浏览器传一本书、没有 Wi-Fi 时连热点传一本书、扫码打开、传完书架里能看到）。
