# hub-link-1：设备连上家庭中枢（S31 加密 WebSocket），中枢能接真模型

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7 条；第 8 条不碰设备）。

现状：模拟器里设备和中枢的整条链路是通的（`hub-simulator-e2e`）；真机上 `ports/esp32s31/hal_stubs.c` 73–78 行的 `hal_ws_*` 全是空函数，所以设备上的 AI、听写、天气、远程网页、备份都不通。中枢 `hub/ink_hub/models/__init__.py` 只有假模型。用户 2026-11-12 同意“中枢有必要启动配合开发的也可以搞”。

## 要做的

1. **S31 的 `hal_ws_*`**：用 ESP-IDF 已有的组件（`esp_websocket_client` 或 `esp_transport` 的 ws/ssl，TLS 用 mbedTLS/esp-tls），按 `system/include` 里 `hal_ws_*` 的头文件语义实现：`hal_ws_open_pinned` 校验服务器公钥的 SHA-256（和模拟器一致，见 `ports/sim` 里的实现），`hal_ws_peer_key` 给出对方公钥哈希，收发、刷新、释放、断线回调。新代码放 `ports/esp32s31/hal_ws_s31.c`，`hal_stubs.c` 里删掉对应的空函数。内部内存吃紧（实测空闲约 114 KB、最低 78 KB）：TLS 缓冲尽量放 PSRAM，报告写连接时内部内存最低值的估算和依据。
2. **配网热点**：首次设置里“用手机设置”要开热点，查真机这一步现在缺什么（`hal_transfer_hotspot` 等）。缺的部分小（不超过 150 行）就照原厂 v2.0.77 的热点做法补上；大就只写清缺口。
3. **中枢接真模型**：在 `hub/ink_hub/models/` 加两种模型接法，一种是 OpenAI 兼容的对话接口（DeepSeek、豆包/火山方舟、通义、Kimi 等都用它，base URL 和模型名可配），一种是 Anthropic Messages 接口；都支持流式输出和工具调用（工具就是设备的能力表，现有假模型怎么接的就怎么接）。密钥只从环境变量读，不进配置文件、日志和测试；不配密钥时照旧用假模型。用本地假服务器测，不发真实请求（花钱的事等用户定）。在 `hub/README.md` 写一段“怎么换成真模型”（放哪个环境变量、怎么验证）。
4. 语音（听写和朗读）现在是什么状态，`hub/ink_hub/speech/` 能不能用同样的方式接真服务：只写报告，不实现。

## 范围和预算

- 只改：`ports/esp32s31/hal_ws_s31.c`（新）、`ports/esp32s31/hal_stubs.c` 里对应几行、`ports/esp32s31/main/CMakeLists.txt` 和 `sdkconfig.defaults` 里需要的几行、热点相关的少量文件、`hub/ink_hub/models/`、`hub/tests/`、`hub/README.md`、S31 宿主机测试、`docs/tasks/hub-link-1/`。不碰别的任务在改的文件（`fixes-1112`、`graft-mogo-1`、`fw-size-2` 同时在跑）。
- 设备侧新代码不超过 500 行；中枢侧不超过 600 行。

## 验收

1. S31 宿主机测试里给 `hal_ws_s31.c` 的连接、公钥校验失败、断线重连、收发大消息写测试（可以替身网络层）；公钥不对时必须拒绝连接（反向对照：去掉校验测试要失败）。
2. 中枢测试：两种模型接法对本地假服务器各跑通一轮对话加一次工具调用；没配密钥时自动用假模型；日志里查不到密钥。
3. 主工程普通全量 ctest 过；`hub` 的 pytest 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小变化。
4. 报告写真机联调步骤（主会话来做）：中枢在这台 Mac 上怎么起、设备怎么配地址和公钥、日志里看哪几行算连上。
