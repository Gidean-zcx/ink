# hub-link-1 执行报告

基线：`6171c2bf8d59238c95f2d548663f8b2ab3c6f43a`。日期：2026-11-12。
状态：本机实现和验证完成，已本地提交；未接设备、未请求真实模型、未推送。完整生产配对尚有下述范围外缺口。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】宿主机通过 | 真实 HAL + 替身 TLS/transport：连接、错误 pin 在升级前拒绝、CA 默认校验、断线后新建连接、70 KB 二进制/文字、分片及 ping、flush/free。撤去比较后断言失败，exit -6，普通和 ASan 均复现 | 下方 S31 CTest；单项 `ctest --test-dir build-s31-tests -R unit-s31-ws --output-on-failure` |
| 2 | 【实测】本机假服务器通过 | 两种接法各完成对话、流式文字和设备工具调用；无密钥回退，错误路径脱敏。专项 9 项、独立完整 pytest 166 项通过 | `PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests/contract/test_models.py` |
| 3 | 【实测】本机全部通过 | CTest：100% tests passed out of 423，847.65 秒；pytest：166 passed in 59.70s；S31 普通和 ASan/UBSan 各 77 项；固件编过、无新告警 | 见下方命令和固件对照 |
| 4 | 【实现】步骤已写，未上机 | Mac 启动、地址/SPKI、日志判据和现有配对阻断已列出 | 见“真机联调步骤” |

## 没做到的

- 真机联调按任务要求留给主会话。
- 手机首次设置未补：原厂 v2.0.77 已删除网页配网；现有传书热点不提供配网路由。【估算】完整补齐超过 150 行，按任务书只报告缺口，见下文出处。
- 当前中枢只支持 loopback HTTP 和开发协议 1；设备配对使用协议 2。完整配对联调仍有跨范围缺口。
- Linux CI 未运行；未上机测 TLS 握手和内部堆最低值。宿主机替身测试不等于实机验证。

## 和设计不一致的

- 【实现】模型接法提供 SSE 增量接口，现有网关仍通过 `answer()` 汇总后发送设备增量；本任务未改网关。
- 【实现】网关只传能力名，模型拿到通用对象参数描述；完整 Schema 仍由原网关校验。

## 改过的已有测试

- `ports/esp32s31/tests/stubs.c`：原来两条 WebSocket 必须返回空的断言改为仍未实现的 provisioning 必须失败；WebSocket 的拒绝连接行为改由新增真实 HAL 宿主机测试覆盖，没有删除测试。

## 要用户定的

- 真实模型厂商、模型名和付费请求仍待用户选择。

## 顺手发现

- 中枢的 loopback/协议 1 与设备生产配对不匹配，证据和影响见联调步骤；按任务范围未改网关。

## 新增的机制

- 每个连接一个 `ws` 任务：6144 字节 PSRAM 栈，HAL 原有异步网络回调语义及任务书第 1 项要求；关闭后退出，不做后台重连。
- `INK_HUB_MODEL_PROVIDER`：选择模型接法，任务书第 3 项要求。
- `INK_HUB_OPENAI_API_KEY`：仅环境读取的凭据，任务书第 3 项要求。
- `INK_HUB_OPENAI_MODEL`：可配置模型名，任务书第 3 项要求。
- `INK_HUB_OPENAI_BASE_URL`：可配置兼容接口前缀，任务书第 3 项要求。
- `INK_HUB_ANTHROPIC_API_KEY`：仅环境读取的凭据，任务书第 3 项要求。
- `INK_HUB_ANTHROPIC_MODEL`：可配置模型名，任务书第 3 项要求。
- `INK_HUB_ANTHROPIC_BASE_URL`：可配置 Messages 前缀，任务书第 3 项要求。
- SDK 既有 `MBEDTLS_EXTERNAL_MEM_ALLOC`：任务书第 1 项要求 TLS 缓冲尽量放 PSRAM。
- SDK 既有 `MBEDTLS_CERTIFICATE_BUNDLE`：HAL 要求无 pin 时正常验证证书。
- SDK 既有 `MBEDTLS_SSL_KEEP_PEER_CERTIFICATE`：任务书第 1 项要求读取握手后的公钥哈希。

## 要改的文档

- 合并主会话更新系统端口完成状态和中枢实现状态：设备已有 TLS/WebSocket 适配；模型可选真实接口；首次配网、局域网 WSS 入口及生产配对仍未闭环。本任务没有改共享文档、能力表或需求。

## 首次设置热点缺口

- 【实现】首次设置调用 `pairing.hotspot_start` → `hal_provision_start`；后者仍返回 -1。`hal_transfer_hotspot()` 只是传书服务当前模式的查询，不负责启动首次设置。
- 【实现】原厂 `874538e` 的 `main/boards/common/board/wifi_board.cc:100–105` 明确移除 SoftAP 网页配网；`main/display/screen/cloud_screen/ap/ap_transfer.cc:1109` 起是文件路由，`:1169` 起是传书热点，本项目已接入该传书实现。
- 缺少：使用设置密码的热点启动/恢复、设置网页、配置 JSON 的接收和有界异步回复、与传书占用射频/HTTP 端口的互斥。原厂现有传书页面不提供这些。【估算】完整接线超过任务给的 150 行，不另写一套底层流程。

## 语音现状（只核查，未实现）

- 【实现】`hub/ink_hub/speech/fake.py:13` 的 ASR 对任何非空帧返回固定文字；`:21` 的 TTS 返回 0xA5 开头的合成包，注释明确不是 Opus。因此现在不能把中枢语音称为真服务。
- 【实现】可沿用“环境变量选择 + aiohttp + 本机假服务器”接厂商，但接口不是简单换模型 URL：`Recognizer.transcribe(frames)`、`Speaker.synthesize(text)` 要处理协议约定的 16 kHz 单声道、60 ms Opus。厂商若收 PCM 或返回 WAV/PCM，须在中枢解码、重采样、重新编码 Opus；没有相应厂商适配和编解码接线。
- 【实现】现有语音流按整轮识别、整句合成后返回列表，设备侧听写再按 480 字节切段。接真服务还需核对取消、成员代次、持续识别和首帧预算。未调用语音服务、未改 `speech/`。

## 真机联调步骤（交主会话）

1. 先启动本次可用的本机中枢：`PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m ink_hub serve --data work/hub-data --dev-allow-proto1`。终端应出现“家庭服务器已就绪”。真实模型按 `hub/README.md` 配环境变量；未获付费授权时保持默认假模型。
2. **现有阻断必须先补齐**：CLI 仅允许 loopback HTTP（`hub/ink_hub/__main__.py:22`），网关拒绝远端 IP（`gateway/server.py:461`）；`hello` 只接受开发协议 1（`:360`），设备 `pairing.c:266` 使用协议 2。需要后续获准修改网关的任务提供局域网 TLS 入口及 `pair/*`、协议 2，不能仅填 Mac 地址就认为能配对。本任务未绕过这些校验。
3. 有可用 WSS 入口后，主会话核对服务器证书公钥：`openssl x509 -in work/hub-device.crt -pubkey -noout | openssl pkey -pubin -outform DER | openssl dgst -sha256`。设备要填的是这 64 个十六进制字符的 SPKI SHA-256，不是证书文件的哈希；公钥必须通过可信的带外途径给设备，不能首次连接自学。
4. 用已有 `pairing.configure` 能力设置 `url=wss://<Mac局域网地址>:<TLS端口>/v1/device`、`console=https://<设置入口>/` 和 `pin`。也可由主会话制作 TF 根目录的 `ink-pair.json`（设备映射为 `/content/ink-pair.json`），走现有 `pairing.import`；按后续网关提供的配对格式填写，禁止把真实凭据写进仓库。手机设置分支尚不可用。
5. 日志 `ink_ws: TLS and WebSocket connected` 只证明公钥及升级握手通过；必须再确认 `hub.online=true`、完成一轮设备工具调用才算业务连上。`ink_ws: peer key rejected` 后应只有 `connection closed`，绝不出现 connected；断网后应关闭，系统已有退避负责新建连接。不要把 TLS connected 当成完成配对。
6. 上机核对内部堆的连接前、握手中最低值、连接后值及任务栈高水位；并核对 Wi-Fi/4G 切换后能恢复。估算不能代替这些真机数据。本任务未烧录、未接串口、未读任何设备或硬件备份。

## 为什么不能更小

- S31 直接用 ESP-IDF 的 TLS 和 WebSocket 协议实现，不重写加密或帧编码。已查 `esp_transport_ssl.h`，它没有公开取出 TLS 上下文供 SPKI 校验的接口；因此用 `esp_tls` 做父传输，校验后交给既有 WS transport。一个连接任务实现 HAL 的异步回调，重连仍归原有中枢服务。
- 两种模型共用一个 SSE 读取器和原有 `ModelResult`，未引入厂商 SDK。通过现有默认入口选择环境变量，避免修改任务范围外的网关和业务流程。超过 100 行是因为同时覆盖两套流事件和分片工具参数。

## 固件和内存

- 【实测】同一检出、同一依赖和构建目录，先临时还原三个接线文件到指定基线编译，随后恢复任务实现重编；镜像 `4,831,328 → 4,998,928` 字节，增加 `167,600`；5 MiB OTA 分区剩余 `243,952` 字节。内部 DIRAM 静态占用 `165,262 → 165,350` 字节，增加 `88`；PSRAM 静态区仍 `746,504` 字节。基线与最终编译器告警均为 0，6 条 SDK CMake 组件依赖提示的正文完全一致。
- 【实现】TLS 用 SDK 的 PSA SHA-256（首次构建发现公开 `mbedtls/sha256.h` 已移除，已修正）；没有打开全局跳过证书验证。仅带 pin 的连接允许 CA/时钟检查不通过，随后必须在 HTTP Upgrade 之前通过 SPKI 比较；无 pin 连接使用 SDK 根证书包。
- 【实现】`sdkconfig.defaults:11` 强制 mbedTLS 动态分配到 PSRAM；SDK `components/mbedtls/port/esp_mem.c:18–19` 明确使用 `MALLOC_CAP_SPIRAM`。连接对象、6 KiB 栈、1 KiB 接收片段、大消息收发副本都放 PSRAM。TLS 输入/输出缓冲分别保留 SDK 默认 16 KiB/4 KiB，没有缩小 TLS 接收记录上限。
- 【估算】以任务书内部空闲约 114 KiB、历史最低约 78 KiB 为起点，固定增量 88 字节；lwIP 收发窗口各 5760 字节（本次生成的 sdkconfig），再留 RTOS/信号量、TCP PCB、pbuf 及报文突发余量，新增内部堆按 16–24 KiB 估计。则连接时空闲约 90–98 KiB，最低约 **54–62 KiB**。假设同时只开一个中枢连接、没有其他新增负载；这不是实机最低值。TLS 证书和握手临时分配归 PSRAM，不能把模拟器原先的 6 KiB TLS 内部估计当作本次实测。
- 【实现】本次连接栈为 6 KiB，模拟器档案仍是未标定的 4 KiB 估计；没有修改共享资源预算。主会话需上机测高水位后同步档案。
- 【实测】`ink-codex/tools/gcccheck.py build-s31` 检查 `hal_ws_s31.c` 和 `hal_stubs.c`：`gcccheck: 2 compiled, 0 with findings`。
- 【实测】S31 宿主机普通测试：`100% tests passed out of 77`，106.08 秒；同一测试集 ASan/UBSan：`100% tests passed out of 77`，121.03 秒，未报告 sanitizer 错误。内存检查包含新 WS 的连接、收发、释放和反向对照。主工程按 `vendor-common.md` 第 7 条跑普通全量，未另外跑主工程 ASan。
- 产品新增行数按 diff 的新增行计（含注释和空行，保守统计，不含测试/文档）：设备 **287/500**，中枢 **153/600**。

## 验证命令

构建依赖使用任务书给的 `INK_DEPS`，主工程配置按 `codex-rules.md` 第 5 节，并指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`。实际并发均为 2，Mac 命令带 `taskpolicy -c utility`。

```sh
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility env INK_KEEP_EVIDENCE=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility env INK_HUB_MODEL_PROVIDER=fake INK_HUB_CAPS="$PWD/build/test-out/hub/caps.json" PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-tests -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility cmake --build build-s31-tests -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-tests --output-on-failure -j2
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-tests-asan -DINK_SANITIZE=ON -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility cmake --build build-s31-tests-asan -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-tests-asan --output-on-failure -j2
taskpolicy -c utility env INK_DEPS="$INK_DEPS" bash ports/esp32s31/build.sh
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31 ports/esp32s31/hal_ws_s31.c ports/esp32s31/hal_stubs.c
```

本机原始日志在忽略目录 `work/hub-link-1/`，未把构建产物或本机路径提交。

## 最终差异和隐私检查

- 【实测】按总规则第 6 节扫描 `git diff 6171c2bf8d59238c95f2d548663f8b2ab3c6f43a..HEAD`：无命中，grep 退出 1（表示没有匹配）。未提交密钥、MAC 样式值、本机绝对路径或构建产物。
- 【实测】`git diff --check` 通过；差异仅包含任务允许的端口接线/新 WS、S31 宿主机测试、模型/测试/说明和本报告，没有共享文档或其他任务的文件。

## 里程碑

- 开工时工作区干净，HEAD 与指定基线一致，使用既有任务分支；先提交验收报告骨架。
- 模型实现和本机专项测试已提交。
- S31 实现、宿主机测试、反向对照和固件对照已提交。
- 普通全量、独立 pytest、S31 普通/内存检查及 GCC 检查全部通过，报告随收尾提交。
