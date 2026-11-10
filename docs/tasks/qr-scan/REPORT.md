# qr-scan 执行报告

基线 `eec5d569be028b539e8778690c239a6947767864`；2026-11-10。仅本地提交，未推送，未接设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过：网址/文字复制、电话确认后拨号、MECARD/vCard 确认保存、Wi-Fi 确认联网、失败提示、AI 调用、儿童扫码与成员隔离 | `qr-scan exit 0 assertions 162 failures 0`；`build/qr-scan-7pkutksj/out/metrics.json`；密码不出现在快照、回执及文本日志 | C2；反向对照待补 |
| 2 | 【实测】部分：普通全量仅共享生成文档检查失败；原相机/相册、联系人、电话及 AI 能力目录通过；S31 编译通过 | 全量原话 `99% tests passed, 1 tests failed out of 321`；317 通过、3 跳过、1 失败；`work/ctest.log`。ASan 全量、最终 gcccheck 待补 | C1～C5 |
| 3 | 【实测/实现/估算】选 quirc；两库同图实测及 S31 探针链接完成，中英截图齐全；旧 esp-code-scanner 无可运行目标 | 下表、来源、截图；未用 N/A 冒充测量值 | C8；复现方法见 `tests/qr_scan/README.md` |

## 选库与硬件依据

| 库 | 许可、版本 | 同图识别 | 内存，VGA | S31 Flash 装载增量 | 识别耗时 |
| --- | --- | --- | --- | --- | --- |
| **quirc，选用** | ISC；自报 1.0，固定 `f2e4271`，完整哈希与许可见 `third_party/quirc/` | 【实测】有效图 10/10、空白 0 次误识别 | 【估算/实现】识别工作区约 342 KiB，含既有 RGB565 与临时 RGB888 的流程峰值约 1.8 MiB，均在 PSRAM；没有新任务栈 | 【实测】20,906 字节，约 20.4 KiB（识别探针，含所引入的数学函数） | 【实测】模拟器内识别函数 14 帧共 17,851 µs，平均 1.28 ms；选库时独立主机基准 1.361～3.570 ms |
| zxing-cpp | Apache-2.0；2.3.0；源包 SHA-256 `64e4139103fdbc57752698ee15b5f0b0f7af9a0331ecbdc492047e0772c417ba` | 【实测】同样 10/10、空白无误识别 | 【估算】约 0.6～1.1 MiB 工作区：灰度 307200、字节二值矩阵 307200、8×8 阈值块 4800 字节，再给临时容器预留 512 KiB；预定 PSRAM，未做分配适配，非实测上界 | 【实测】590,386 字节，约 576.5 KiB；保留 C++ 异常/RTTI，TLS 为 local-exec | 【实测】独立主机基准 0.560～1.829 ms；未集成模拟器 |
| esp-code-scanner（ESP-WHO 旧版） | ESP-WHO `idfv4.4` / `db6874aa`，根许可 ESPRESSIF MIT、限乐鑫产品；组件无独立完整许可清单 | N/A：仅 Xtensa 二进制，无本机目标 | N/A：无源码和匹配目标，无法给可信工作区估算 | N/A：只有 ESP32/S2/S3 归档，没有 S31/RISC-V | N/A |

【实现】两种可比较的库在这组图上识别率相同；quirc 的探针增量约为 ZXing 的 1/28，C 分配可直接接 OSAL，所以选 quirc。上表探针采用同一个 S31 工程替换初始化入口再链接，计算代码、只读数据与初始化 RAM 的装载字节；没有将归档或 ELF 文件大小冒充 Flash 增量。它不包含完整扫码页面的增量，也不是烧录镜像的填充大小。

【实测】图片是同一组 640×480 的小 PNG，含网址、电话、MECARD、vCard、Wi-Fi、中文/英文文字、转义分隔符、23° 旋转、模糊半径 1.2、小码 3 像素/模块和空白。生成器、CC0 声明及固定载荷见 `tests/qr_scan/README.md`、`generate.py`、`images/payloads.json`。测试运行无需生成器或网络。

【估算】S31 两库暂按每像素 100～500 周期、透视/ECC 另计 20～100 ms、320 MHz 估为每帧约 0.12～0.58 s；这只是规划假设，不是实测上限，也不据此比较快慢。若工作在 160 MHz，CPU 部分约翻倍。模拟器墙钟计时只记录识别函数，不含取帧、灰度转换、UI，也未换算为 S31 指令周期。

原厂依据（只读，查阅 2026-11-10）：CloudZao/Metalio-E-INK4-Plus `690a023`，`main/boards/common/camera/dvp_camera.cc:24–28` 定义 GC2145 的 640×480、大端 RGB565、614400 字节；`:144–145` 将帧分配到 PSRAM/DMA。`main/boards/common/power_policy/README.md:200–205` 列出 160/240/320 MHz、默认 320 MHz；本项目 S31 配置也选择 320 MHz。没有修改传感器参数。

上游：[quirc 固定源码](https://github.com/dlbeer/quirc/tree/f2e4271ae43a15d9263704e07bc86cba5697d74d)、[ZXing 2.3.0](https://github.com/zxing-cpp/zxing-cpp/tree/v2.3.0)、[旧 esp-code-scanner 目标](https://github.com/espressif/esp-who/blob/db6874aad97ee05e59e13475d1b4ac48611aa33d/components/esp-code-scanner/CMakeLists.txt)。另核查 ESP-WHO 当前 `b39e2fe5`，其 [who_qrcode](https://github.com/espressif/esp-who/blob/b39e2fe55d035a9ba1332ec62c3bcff6f96a794e/components/who_qrcode/idf_component.yml) 已依赖 `espressif/quirc`；未把这个新包装与旧二进制库混为一项。

## 截图

均来自真实模拟器渲染，未启动窗口，生成物未提交：

- 中文：`build/qr-scan-screens/snap-url-zh.png`、`snap-wifi-zh.png`、`snap-tel-zh.png`、`snap-blank-zh.png`。
- 英文：`build/qr-scan-screens/snap-url-en.png`、`snap-wifi-en.png`；ASan 同名截图在 `build-asan/qr-scan-screens/`。

## 没做到的

- 旧 esp-code-scanner 的同图识别、RAM、S31 体积及耗时无法测量；ZXing 只做主机与交叉链接基准，没有做完整模拟器适配。
- 全量未全绿：唯一失败为 `capability-table-doc`，因为本任务明令不改共享生成文档。3 个跳过项为 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，不是通过。
- 未上机；既有 S31 相机 HAL 仍为桩，扫码能力跟随 `camera.capture` 的可用性注册。真实采集、识别耗时、PSRAM 争用须接好原有端口后核对。
- 文本支持 UTF-8、最多 300 字节（与既有剪贴板入口对齐），超限明确失败；联系人支持姓名与一个裸 `TEL:` 号码。未验证带属性/折行/多号码的 vCard、企业 Wi-Fi、隐藏网络及二进制载荷。

## 和设计不一致的

任务书未给文本长度上限；此次有上述 300 字节限制，未将它升级为需求。扫码按一次一帧、结果页提供重试；没有新增自动重扫线程。新增结果页及快照中英齐全，复用的系统确认卡沿用既有语言支持。

## 改过的已有测试

`tests/integration/ai_surface.c` 只在完整能力清单增加 `camera.scan`、`camera.scan_apply` 两行登记（同一源码行）；没有删除或改动原断言及既有参数期望。原相机/相册、通讯录、电话测试原样运行。

## 要用户定的

没有需要暂停本任务的选择。超过 300 字节及更完整名片格式的处理方式留待主会话确定，未擅自扩大剪贴板协议。

## 顺手发现

未修改范围外代码。新能力会触发共享生成能力表的同步检查，处理方式见下一节。

## 新增的机制

- 任务书第 2、3 项要求：扫码页、`camera.scan`、需人确认的 `camera.scan_apply`；每次一帧，结果仅在当前成员、当前页面内保留。
- 任务书资源要求：quirc 堆区接 OSAL PSRAM；将其解码临时区从栈移到堆。S31 栈帧从 9280 字节降到 96 字节，保持原 MEDIA 栈 8192 字节。
- 没有新增常驻任务、状态机、持久格式、产品配置、后台重试、队列或通用框架。按钮与 AI 走同一能力；复制、拨号、存联系人、联网复用现有能力。凭据不进入扫码结果/快照，AI 无法代替人确认。

## 要改的文档

合并时由主会话运行 `python tools/capabilities.py` 更新禁止本任务修改的 `docs/system/capabilities.md`；新增两条能力及 Camera 订阅段差异见全量日志。还需同步 `docs/system/ui-dependencies.md` 的扫码完成状态。其他共享需求、决策、审计、调研资料均未改。

## 验证、规模与命令

【实测】保守计新增产品行数 **244 行**：包含 C/H、构建接线、翻译表以及第三方本地补丁 21 行；不含第三方原代码、测试和文档，低于 300 行。为什么不能更小：跨执行器采集、类型解析、凭据隔离、结果页、确认及软总线接线构成验收所需的最小链路。考察过直接回传原文（会暴露密码）和仅返回识别文字（缺少后续操作），均未采用。

- C0：按总规则第 5 节配置 `build` / `build-asan`，两者均 `INK_WERROR=ON`，后者另加 `INK_SANITIZE=ON`；生成编译数据库。
- C1：`taskpolicy -c utility cmake --build build -j2`；退出 0。
- C2：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；退出 8，679.39 秒，原话见验收表。
- C3：同 C1/C2 换 `build-asan`；最终结果待补。早期扫码专项 ASan/UBSan 已有 144 条断言通过，最终版本按全量重新验证。
- C4：`INK_DEPS=... taskpolicy -c utility bash ports/esp32s31/build.sh`；编译退出 0，`build-s31/size.txt`、`stack.txt`。最新小幅 UI 修改后的增量编译待补。
- C5：`taskpolicy -c utility python ../../tools/gcccheck.py build system/apps/camera/scan.c system/apps/camera/camera.c tests/qr_scan/device.c tests/integration/ai_surface.c`；最终结果待补。
- C6：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility python tests/qr_scan/run.py --build build --negative`；待补。
- C7：`taskpolicy -c utility python tests/qr_scan/stack.py --build build-s31`（再加 `--negative`）；待补。
- C8：选库及 S31 三次链接的复现命令在 `tests/qr_scan/README.md`，产物 `build-s31/qr-size/`；最终口径重算待补。
- 隐私及文件检查：按总规则第 6 节检查提交差异，已提交部分命中 0；最终提交后复核待补。`git diff --check` 已通过。
