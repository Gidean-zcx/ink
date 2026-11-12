# 构建、离线检查与证据边界

日期：2026-11-12。macOS 15.7.9，Apple clang；只在 `work/replan-reader/` 操作第三方工作副本。没有执行任何 ESP32 固件，没有运行 `ink-sim-gui`，没有打开串口。构建串行、最多 `-j2`，编译和测试均使用 `taskpolicy -c utility`。

## 命令与实际结果

| 编号 | 实际命令或操作 | 结果原话／退出状态 | 解释 |
| --- | --- | --- | --- |
| C1 | `git status --short`、`git rev-parse HEAD` | 开工无差异；基线 `d09b79195e2dc57908e33219216048e529e082c6` | 已在独立任务分支 |
| C2 | `git -C "$INK_DEPS/../ink-hw/upstream/oss/<候选>" rev-parse HEAD`，逐文件 `rg -n`、读取源码、SHA-256 | [55 个固定文件、56 处证据](source-evidence.json) | 源码证据，不是运行结果 |
| C3 | `PICO_SIM_FIRMWARE="$PWD/work/replan-reader/kiikoread-xiugai" taskpolicy -c utility python3 work/replan-reader/kiikoread-xiugai/tools/simulator/build.py` | **退出 0，但 `Compiled 48 failed 6`** | 没有生成 `pico-engine`；不能写构建成功 |
| C4 | `taskpolicy -c utility python3 work/replan-reader/kiikoread-xiugai/tools/simulator/web/build.py` | 未执行编译；`emcc` 不存在 | 所需 Emscripten 6.0.13 不在环境中；网页构建也复用同一不匹配的源码清单 |
| C5 | `PLATFORMIO_CORE_DIR="$PWD/work/replan-reader/pio-core" PATH="$PWD/work/replan-reader/host-bin:$PATH" taskpolicy -c utility work/replan-reader/host-env/bin/pio run -d work/replan-reader/crossmux -e simulator_readpico -j2` | 最终退出 1；`[FAILED] Took 201.35 seconds` | `lib/Epub/Epub/css/CssParser.cpp:244:23: error: 'from_chars' is unavailable: introduced in macOS 26.0` |
| C6 | 同 C5，将目录换成 `CrossInk`、环境换成 `simulator` | 退出 1；`[FAILED] Took 163.75 seconds` | 同一错误，`CssParser.cpp:137:23`；没有生成宿主程序 |
| C7 | 在 `work/replan-reader/dpreader/` 执行 `taskpolicy -c utility python3 tools/book_sync_position_host_test.py` | 退出 1，子程序 `SIGABRT` | `Assertion failed: (!"sync conversion must never close the live reader's fonts"), function ttf_font_close_embedded, file book_sync_position_host_test.c, line 9.` |
| C8 | 同目录 `taskpolicy -c utility python3 tools/txt_utf16_host_test.py` | 退出 1，子程序 `SIGABRT` | `Assertion failed: (used+len<=n&&!memcmp(out,want+used,len)), function verify, file test.c, line 57.` |
| C9 | `taskpolicy -c utility node docs/tasks/replan-reader/check-transfer.cjs`（已设置 `INK_DEPS`） | **退出 0，7/7 通过**；[原始结构化输出](transfer-checks.json) | 原样提取改版网页目录函数，合成 8 章，核编码和原文字节偏移；不测 HTTP/设备写卡 |
| C10 | Playwright 临时配置启动 headless Chrome；随后 Computer Use 创建本地预览标签页 | 前者 `SIGABRT`；后者审批拒绝访问本地预览地址 | 没有运行截图。后者拒绝后未换浏览器、CDP 或其它方式绕过 |
| C11 | 最终范围、链接、哈希、图片大小与隐私检查 | 见 [verification.txt](verification.txt) | 只交本任务目录 |

C7 的现有脚本给 C 代码启用了 `-fsanitize=undefined`；它在位置映射断言处中止，后续 XPointer 子测试没运行。C8 在内容比对断言处中止。两项的根因均未定位，不能据此断言所有实机文件都会失败；也不能写成通过。本任务不修改第三方测试或产品源码。

## 构建准备与中止位置

**操作偏差**：dpReader 新克隆带回 82 个 `releases/*.bin`，听趣带回 1 个触控固件文件，违反不下载固件的限制；均未运行或提交。已清理这些文件及两个任务新克隆的 Git 缓存，保留源码快照。后续取源码应先读取树清单，只取文本／所需资源；现有上游与主工程 Git 历史未改。数量和字节数见 [operation-deviation.json](operation-deviation.json)。

1. 从给定只读副本 `git clone --no-hardlinks` 到 `work/`；dpReader、听趣与 ThingsOS 从公开仓库另取。没有修改已有上游副本。
2. 在 `work/replan-reader/host-env/` 新建 Python 环境，安装 PlatformIO；缓存全部指向 `work/`。没有升级主工程依赖。
3. CrossMux 第一次停在未初始化的 `freeink-sdk`，初始化任务副本的固定子模块后，第二次停在依赖目录的 `sdl2-config` 不可执行。
4. 在 `work/replan-reader/host-bin/` 放一个宿主 `sdl2-config` 适配脚本，指向 `$INK_DEPS/build/_deps/sdl2-src/include` 与 `sdl2-build/libSDL2.a`，提供已有静态库需要的 macOS framework 参数。没有改只读 SDL 文件、第三方阅读器源码或依赖版本。之后到达 C5/C6 的真实 C++ 编译错误；没有升级系统、关闭检查或改写 CSS 解析器。
5. kiikoread 改版的模拟器声明 rc97，任务副本是 rc107 派生。失败包括 `music_player.h`、`cJSON.h` 找不到，以及 `epd_set_inverted`、`epd_get_inverted`、`font_diag` 未声明。尝试取得 manifest 指定提交时，部分克隆报告缺对象，GitHub 树接口随后返回限流 403；到此停止，没有用另一版界面冒充改版。
6. CrossPoint、旧 crosslink 在核查提交的 `platformio.ini` 中没有 `simulator` 构建入口；不拿其它分支的程序贴这个标签。dpReader 有宿主单元测试，未找到可直接启动的完整界面模拟器；原厂、听趣无本次可用的完整宿主 UI 构建入口。没有启动 IDF 整机固件构建。

工作目录的日志为 `kiiko-build.log`、`crossmux-build.log`、`crossmux-build2.log`、`crossmux-build3.log`、`crossink-build.log`、`dpreader-sync-test.log`、`dpreader-txt-test.log`。日志含环境路径，未提交原文；上表保留真实失败语句和退出状态。

## 主工程检查

主工程全量测试原话：**未运行（本任务书明确豁免）**。主工程 ASan：未运行；S31：未编译；`gcccheck.py`：未运行。产品代码改动 **0 行**，已有测试改动 **0 行**，不存在“撤掉修复”的反向对照。研究检查不冒充主工程验收。

本地前端检查服务在浏览器拒绝后已停止（KeyboardInterrupt，退出 130）。`serve-transfer.py` 仅保留为可复核的合成 GET 预览工具：只绑定回环地址，用 CSP 限制页面资源，未实现上传、删除或其它写入。没有运行设备上传测试。
