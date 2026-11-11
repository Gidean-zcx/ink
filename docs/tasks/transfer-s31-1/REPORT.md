# transfer-s31-1 执行报告

基线 `83ec2e4e4483d530866603c2e93474ed30ce96a3`；产品代码提交 `80a74535`。2026-11-11 开始，2026-11-12 收尾。仅本地提交，未操作设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 源码 | 通过【实测】 | 原厂 874538e 整文件复制并适配；31 份入库文件哈希已核对，原始克隆 HEAD 仍为 690a023；[来源、许可证、逐段修改](../../../ports/esp32s31/vendor/ap_transfer/SOURCE.md) | SHA-256 对照 SOURCE.md |
| 1a | 通过【实测】 | 真实 TCP：正确/错误/三次错误/过期、成员切换、成员收件箱、满盘、路径逃逸；原文件服务集成测试覆盖删除 | A；`ctest --test-dir build -R 'integration-(wireless-transfer|files)$'` |
| 1b | 通过【实测】 | 实际原厂路由和实际 transfer.c 策略，假 HTTPD/文件服务边界；identity、zlib、三次错误关闭、飞行模式、删除回调、存储准入均检查 | B；`build-s31-host/test-s31-transfer` |
| 1c | 通过【实测】 | 原 STA 和原发现定时器恢复；关闭恢复选项不重启射频；启动失败不留锁、不带出前一成员文件名 | B；`build-s31-host/test-s31-transfer-wifi` |
| 1d | 通过【实测】 | 11 个策略/原厂路由反向对照；7 个真实 TCP 反向对照；Wi-Fi 恢复撤销补丁时失败 | B；`python tests/wireless_transfer/reverse.py --build build` |
| 2a | 不通过【实测】 | 389 通过、3 自动跳过、1 失败：integration-races；回退本任务主机端改动仍在相同断言失败 | A；[手动对照](compare_races.py) |
| 2b | 通过【实测】 | S31 宿主机：`100% tests passed out of 67` | B |
| 2c | 通过【实测】 | S31 固件编译、打包、栈报告完成；编译器 warning/error 均为 0；CMake 警告位置无新增 | C |
| 内存 | 通过【实测】 | S31 ASan/UBSan 全 67 项通过；最后修改后相关 3 项再测通过；模拟器真实 TCP 和页面两项通过 | D |
| 二维码 | 接口完成【实现＋编译】 | 原文 qrcodegen 生成网址加授权码，包含四模块静区；基线没有 Merge ui-fix-a，按任务要求只交函数 | C；`hal_transfer_qr` |
| AI | 已接入【实测】 | start/stop/status 共用现有能力；状态增加 connection/url/files/last_file；AI 的 code 固定为 000000；快照保留 secret 字段 | A、B、D |
| 3 | 清单完成；实机未测 | 文末列出 Wi-Fi、热点、扫码和书架核对；自动入架依赖尚缺 | 不操作设备 |

A：按任务规则的 CMake 参数配置，`taskpolicy -c utility cmake --build build -j2`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。最终全量前已清空本工作区生成的 `build/test-out`。

B：`taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host`；同样以 `-j2` 构建和运行 ctest。`unit-s31-transfer-reverse` 不依赖 Git 历史，适用于 CI 浅检出。

C：`INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh`。基线在独立的 `work/baseline` 检出，用 `INK_S31_BUILD` 指向本工作区的 `build-s31-base`，编译同一提交以比较。

D：S31 宿主机同 B，换 `build-s31-host-asan` 并加 `-DINK_SANITIZE=ON`。主工程同任务参数配置 `build-asan`，加 `-DINK_SANITIZE=ON`，构建 `ink_fonts ink_ui_fonts ink_icons ink_test_content sim-wireless-transfer sim-transfer-pages`，再以 `-j2` 运行 `integration-wireless-transfer`、`transfer-pages`。

## 没做到的

- 未上机，未在 Linux 实跑。原厂请求工作任务在宿主机由同步假调度替代，未据此宣称并发网络或真机吞吐已经验证。
- 未自动入架：基线阅读器只扫描 `/content/books`（`apps/reader/reader.c:129`），没有从成员收件箱导入的可调用规则；阅读器不在允许修改范围。本任务写成员收件箱，网页自动分类控件已隐藏。
- 未实现流式覆盖：文件服务没有流式原子替换接口，目标已存在则拒绝；没有移动原文件去做跨步骤替换。
- 未恢复断电遗留 `.part`：文件服务目录接口隐藏未完成文件，原厂恢复扫描看不到；正常断开、取消和到期会清理已知临时路径。
- 状态保留本次接收计数、字节数和最后文件名，没有另建完整会话清单；更早文件通过既有文件能力查询收件箱。
- 英文 gzip 已嵌入，网页目前固定中文，语言选择尚未接入。

## 和设计不一致的

文件服务不允许点开头的临时名，原厂 `.cz-名称.part` 映射成已有 `名称.part`，备份后缀映射成 `.tmp`。现有删除接口是永久删除，没有回收站接口；网页调用该接口，没有另设删除策略。

设备页面仍写“只能放到传进来的”，S31 网页实际可管理当前授权成员空间；该文案按任务边界留给主会话。二维码页面接入也按没有 ui-fix-a 的分支条件留待合并。

## 改过的已有测试

- `tests/transfer_pages/device.c` 两处取授权码改为人工来源；新增 AI 结果必须为 000000 的断言；地址、二维码位图、快照和原有 UI 断言保留。
- `tests/wireless_transfer/device.c` 新增收到的文件名、计数和 AI 脱敏断言；没有删除或放宽既有断言。
- Apple SDK 对原文 cJSON 的六处 `sprintf` 报弃用提示，仅该第三方文件保留 `-Wno-error=deprecated-declarations`，不修改库原文，不关闭 sanitizer。S31 固件没有这类警告。

## 要用户定的

多文件网页会话暂沿用两分钟，到期停止，包括正在上传的文件；未添加续期。模拟器保留既有单文件开始后可超出两分钟的行为。是否延长多文件会话，以及补齐流式原子覆盖、入架接口，由主会话结合相关模块确定。

## 顺手发现

首轮主工程为 `99% tests passed, 5 tests failed out of 393`：启动时多分配一把 96 字节锁，使两个语音输入场景越过原预算；改为开启时创建、退出时回收。能力总表重生成、页面测试取码来源修正后，四项传书相关失败修复。旧目录的六项复测曾全部通过，其中竞态复测的通过由残留状态造成，不能作为修复证据；最终全量保留该失败。预算和竞态断言未改。

最终差异检查的失败注入复现了“启动失败留下 1 把锁、成员变化后带出上次文件名”；在统一启动入口清空新授权结果，并回收未开启授权的锁。修改前失败，修改后锁计数为 0、files 为 0、last_file 为空。

主工程 ASan 首次只构建两个程序，因资源未生成而退出；补齐原有资源目标后两项通过。反向对照曾发现仅查找 error 字段会误判满盘结果，已收紧为精确匹配 error 事件。

## 新增的机制

- 原厂 HTTPD 任务：16 KiB PSRAM 栈，随传书启停；任务第 1、3 条要求复用。
- 原厂 ap_upload：16 KiB PSRAM 栈、8 项请求队列；任务第 1 条要求复用。
- 原厂 ap_files：16 KiB PSRAM 栈、8 项请求队列；同上。
- 原厂 UploadPhase 上传状态机：整段保留，任务第 1 条要求复用。
- 128 bit 内存会话 cookie：首次数字授权后逐请求校验，任务第 2 条要求。
- 一把临时授权互斥锁：保护原厂回调访问统一服务状态，开启时分配，正常退出和失败时回收。
- 构建启用 `CONFIG_HTTPD_WS_SUPPORT`：原厂 WebSocket 协议需要。
- 构建启用 `CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT`：在握手前校验授权并保存连接凭证需要。

没有新增持久格式、产品配置项、后台重试或缓存。复用原有 Wi-Fi 发现定时器。HAL 用 native start/stop 加授权、文件两类回调；S31 走原厂服务器，模拟器保留字节流。授权、成员 epoch 和文件回执由同一个服务持有；底层不直接读写成员卡目录。人和 AI 继续使用既有能力，文件变更沿文件服务的成员事件发布。

产品 C/C++ 新增和替换 460 行，加 CMake 26 行、网页 7 行，共 493 行（含注释和空行，原文复制、测试、文档除外）。超过 100 行的原因是必须替换原厂文件、目录、容量依赖，接授权、二维码和构建。考察过直接保留原厂 POSIX 写卡和在端口复制授权两种更小做法，均不能满足成员隔离和统一策略，所以只在边界写胶水。

## 要改的文档

`docs/system/capabilities.md` 在本任务书明确允许范围内，由原生成器更新飞行模式依赖。其余共享文档未改。主会话需记录上面的入架、覆盖、会话清单和语言缺口，并修改设备页说明。

二维码接入一行：UI 任务中调用 `hal_transfer_qr(P.address, P.code, pixel, ctx)`；pixel 收模块坐标与颜色，返回边长包含四模块静区。仅人工页面持有 code，二维码内容和图像均不得进入 AI 快照；模拟器保留原 LVGL 二维码。

## 构建和检查证据

【实测】基线 `.bin` 4,383,952 字节；最终 `.bin` 4,599,872 字节，增加 215,920 字节（约 210.9 KiB、4.93%），固件版本含 `80a74535`。编译器 warning/error 均为 0；SDK 原有 CMake 警告位置没有新增。栈报告已生成，但间接调用与第三方帧不构成完整上界，内部 RAM 峰值待上机。

【实测】`ink-codex/tools/gcccheck.py` 检查主工程 3 个 C 文件及 S31 策略夹具 1 个 C 文件，最终 0 findings；模拟器 socket HAL 因 newlib 缺 `arpa/inet.h` 跳过。S31 实际固件用 GCC 编译通过。另用 `gccscan.py` 检查胶水和新测试，未发现对应模式。

最终全量原话：`99% tests passed, 1 tests failed out of 393`，耗时 788.17 秒（产品代码 c6db133e）；随后仅将同结构体内的格式化拷贝改成局部缓冲拷贝，最终普通传书两项、内存两项、S31 相关三项及 GCC 检查复测通过。三项自动跳过为 hub-contract、hub-json-crosscheck、hub-simulator-e2e，未主动添加跳过。

唯一失败是 `tests/integration/races.c:121`，覆盖到 2 次交错，断言要求至少 3 次。手动把本任务的两个主机实现及 HAL 头文件换回基线，两者全新输出目录均在 7.480 秒以同断言失败、exit=1；保留输出再跑均 exit=0。这是针对本任务改动的回退对照，不是整仓基线全量。早先旧目录复测通过不能作为该问题已修复的证据。竞态断言和通知代码未改。

复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" docs/tasks/transfer-s31-1/compare_races.py build`；手动模式才读取历史，未注册进 ctest。完整记录保留本次全量失败，未用旧目录制造通过。

差异范围和文档链接检查通过；复制文件及复用依赖的 35 个哈希通过。【实测】按任务给定模式扫描最终差异，0 匹配（grep 退出 1 表示无匹配）。未提交本机绝对路径、真实设备标识、凭证或构建产物；差异共 62 个允许范围内的文件，最大文件小于 5 MiB。

## 上机核对清单

1. 已连 Wi-Fi：电脑用设备网址输入授权码，上传一本合成测试书；在“文件→传进来的”核对内容，验证列表、下载、新建、改名、移动、删除都限于授权成员。
2. 没有 Wi-Fi：确认原厂热点就绪、地址为 `192.168.4.1`，连热点上传一本书；关闭或到期后热点消失，原 Wi-Fi 状态和自动发现恢复；飞行模式不开热点。
3. 主会话接入二维码后，扫码携带授权码打开；错误三次、成员切换、到期后旧网页与 WebSocket 不能继续访问。日志和 AI 快照没有授权码、热点名称或密码。
4. 阅读导入接口接上后，再验证传完书架可见；当前基线不能把这一项写成已通过。全过程检查原厂三个工作任务在退出后结束、电源持有释放，并观察实际空闲内部 RAM。
