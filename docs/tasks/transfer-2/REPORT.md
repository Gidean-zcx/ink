# transfer-2 执行报告

基线 `2747eea2e12bdc551fdba82f5a18b7c27e83cf77`；分支 `claude/codex-transfer-2`；产品代码提交 `8393445d`；日期 2026-11-12。仅本地提交，未操作设备或对外发布。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过【实测】 | 实际新服务器路由 + 实际授权策略；三次错误关闭、正确授权、家人目录、GBK 原书与浏览器目录、取消无残片、成功重传、改名、复制、批量删除、各路由到期拒绝、路径/满盘/非法名、设置 404；10 项策略反向对照 | A |
| 2 | 通过【实测】 | 入库网页目录算法 7 种编码 7/7，章节字节偏移正确；目录上传失败不会误报已保存，撤掉响应检查时断言失败 | B |
| 3 | 原策略场景通过【实测】 | S31 全量 `100% tests passed out of 78`，57.61 秒；原模拟器无线传输和页面断言保留。S31 的旧 WebSocket/zlib 私有协议夹具改由新 HTTP 协议夹具执行等价策略检查，未声称旧协议仍可用 | A、C |
| 4 | 部分完成【实测】 | 默认、最大文字的等待/已收到及实际两行文件列表截图已取得；增量行反向对照失败符合预期。网页截图被浏览器权限审查拒绝，未取得 | D；[截图目录](screenshots/) |
| 5 | 部分通过【实测】；普通全量有 1 项失败 | 普通全量原话 `99% tests passed, 1 tests failed out of 438`，唯一失败 `unit-s31-cell-started`；传书相关通过。S31 ASan/UBSan 78/78、主工程相关内存检查 3/3。S31 最后复编通过，编译器 warning/error 均为 0；固件满足体积预算。固件减少 97,376 字节，产品行数 494，gcccheck 已检查四份 C，0 findings | C、E、F、G；[资源数据](resources.json)、[行数](line-count.json) |

命令均从仓库根目录运行，已设置任务给定的 `INK_DEPS`。下列脚本在 macOS 自动加 `taskpolicy -c utility`，构建和 ctest 均最多 `-j2`，不启动 GUI：

- A：`bash docs/tasks/transfer-2/commands.sh host`；完整浏览器目录载荷另跑 `node docs/tasks/transfer-2/check-transfer.cjs build-s31-host/transfer2-fixtures`、`build-s31-host/test-s31-transfer build-s31-host/transfer2-fixtures`。
- B：`node docs/tasks/transfer-2/check-transfer.cjs`。脚本读取本次入库网页，不依赖外部副本；本环境有 node，未跳过浏览器函数检查。
- C：`bash docs/tasks/transfer-2/commands.sh main`。
- D：`bash docs/tasks/transfer-2/commands.sh visual`。其中 `reverse-ui.py` 在临时副本撤掉增量更新，要求对应断言失败，不改源文件。
- E：`bash docs/tasks/transfer-2/commands.sh s31`。另以 `check-radio.py build-s31-host` 检查原厂热点接线及反向对照。基线从指定提交归档到 `work/transfer-2/baseline`，同脚本构建；`resources.py build-s31-base build-s31` 比较结果。
- F：`bash docs/tasks/transfer-2/commands.sh asan`。S31 宿主机全量 ASan/UBSan；主工程 ASan/UBSan 的无线传输、页面、增量列表三项。
- G：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/services/file/transfer.c system/ui/shell/transfer_pages.c`；再以 `build-s31-host` 检查 `ports/esp32s31/vendor/read_pico_transfer/read_pico_transfer.c`、`ports/esp32s31/tests/transfer2_policy.c`。

## 没做到的

- 网页浏览器截图：浏览器权限审查拒绝打开本机预览地址，原因是未授予访问授权；没有尝试其他浏览器、CDP 或命令行绕过。离线预览脚本为 `preview.py`，只绑定回环地址、只返回合成数据，已停止本次预览服务。
- FTP 按任务允许的保守方案关闭；`/wifi`、`/clock`、`/signature` 不注册。媒体接口只保存普通文件，不安装字体或设置壁纸。
- 文件服务没有流式原子覆盖接口，同名上传拒绝并提示改名；不移动旧文件拼凑替换。未恢复断电遗留片段，正常取消/断开/过期清理已知 `.part`。
- `/titles` 依赖上游自己的阅读器显示名数据库，本次不接；文件实际改名通过 `/files` 完成。没有修改阅读器，未声称它会读取新目录文件或自动入架。
- 未实机验证并发网络、吞吐、热点手机兼容性、运行内存峰值；未在 Linux 实跑。
- 上游快捷目录上传仍以 `books` 生成侧文件路径；本次只验收默认目录，未覆盖把 `books` 根目录改名后再快捷上传的组合。书改名/复制后的目录文件关联也未接阅读器元数据。

## 和设计不一致的

- 采用上游通用同步 HTTP 分支，屏蔽其网络、NVS、时钟、电源与阅读器元数据依赖；路径、接收、清理、分页、文件操作、复制、下载函数仍取自整文件。无线底层保留基线已集成的原厂 `StartRadio` 整函数及原暂停/恢复钩子，没有自行重写时序。
- `hotspot-1`、`usb-disk-1` 不在本分支补实现。热点沿用当前行为；“插电脑当 U 盘”进入现有 `transfer.usb` 页，再调用现有 `usb.mount`，沿用其占用/确认策略。
- 新内容按钮用共用令牌，125 px；文件行没有圆形图标，没收到时没有文件行。基线共用顶栏的返回键仍为 101 px 且带圆圈，严格的“所有可点控件 ≥108 px”尚有这一处共用控件缺口，未改任务范围外的顶栏。
- 两分钟授权沿用现状，包含上传过程，不新增续期；接收状态保留总数和最近两个文件名，更多文件由既有文件能力查询。网页可管理当前成员 `/me`，书籍快捷上传在 `/me/books`。

## 改过的已有测试

- `tests/transfer_pages/device.c` 只增加最大文字、完成态截图和选择截图的入口；原地址、二维码位图、授权码保密、USB 占用、坏卡和孩子断言保留。
- `tests/ui_fix_a/transfer.c` 只追加原生多文件回执场景：未收到不画行；第一行到达保留页面；第二行到达保留第一行；AI 取得两个文件名。原断言未改。
- S31 CMake 的传书目标换接 `transfer2.cxx`、`transfer2_policy.c` 和本次上游 C 文件。旧 `transfer.cxx`、`transfer_policy.c` 保留原文，旧私有协议不再注册进当前目标。新夹具以合成内存文件逐字节验证内容，不把文件写入假装成成功。
- `transfer_reverse.py` 的旧 HTTP/WebSocket 注入点换成新服务器请求鉴权与过期残片清理；保留三错、成员、到期、AI 脱敏、删除策略和启动失败检查。容量断言增加“满盘不尝试打开写入”，不是放宽为只看错误状态。
- 编译只对整份保留的上游文件抑制未调用静态函数诊断（其 NVS/联网辅助函数不在本项目运行）；其余告警、原预算、断言和 sanitizer 未放宽。CI 注册测试不依赖 Git 历史。

## 要用户定的

由主会话决定是否接网页设置能力、FTP 授权、流式覆盖、阅读器显示名数据库和中英文网页切换；本次全部采用任务指定或保守可逆方案，不等待选择，不把它们记成用户新需求。

## 顺手发现

- 合并阅读器翻译表在 CMake 配置时生成，追加英文后需重新配置才能更新；已用重新配置解决，没有改任务外的构建机制。
- 平台资产 HAL 也使用共享 miniz，删除旧传书接线时不能移除这项链接。首次 S31 编译的缺头错误已在原 CMake 链接行修复，资产 HAL 未改。
- 页面快照原上限是 128 token。新增两个重复展示字段和重复返回动作会超过上限，已在页面本身合并重复信息，字段总数保持六项；解析上限不变。
- 截图核查发现首次回执后旧说明残留在列表底部，已在增量更新时隐藏该说明并保持已有行；最终截图已刷新并视觉核对，说明无残留、文件行无外框。

## 新增的机制

- kiikoread 原有 HTTPD 单任务取代旧 HTTPD + 两个工作任务；任务第 1、2 条要求替换服务。内部栈 10 KiB，接收缓冲 16 KiB 在 PSRAM，不新增上传线程或队列。
- 本次接收的两项有界文件名列表：任务第 5、6 条要求设备页和 AI 状态可见；随新授权清空计数，成员切换不带出历史名字。
- 浏览器定时校验授权并禁用失效页面：任务第 2 条要求授权结束网页失效；设备端仍逐请求和逐块做实际校验。
- `.toc.json` 是上游原格式，任务明确要求保留；gzip 是构建嵌入产物，不是新的持久用户数据格式。

没有新增设备配置项、后台重试、缓存、常驻任务或状态机；授权 cookie、授权互斥锁与不睡眠需求沿用已有机制。

## 要改的文档

`python3 tools/capabilities.py` 已运行：611 定义，生成结果与基线一致，无需提交空差异。没有新增能力，`tests/integration/ai_surface.c` 两张表无需追加。英文仅在 `assets/i18n/en.tsv` 末尾追加。主会话后续同步传书协议、默认书籍目录、目录文件格式、最近两项回执与上述缺口；未改任务禁止的共享需求、决策、研究或审计文档。

## 资源与来源证据

【实测】固件 `.bin`：4,270,528 → 4,173,152 字节，净减 97,376 字节，满足净增 ≤40 KiB。新网页 35,040 → gzip 12,479 字节；原两语言压缩网页合计 36,550 字节。静态 DIRAM 165,358 → 164,486（−872）；静态 PSRAM 746,744 → 747,576（+832）。完整值见 [resources.json](resources.json)。

【实现】新 HTTP 栈 10 KiB 为内部 RAM；16 KiB 接收缓冲、分页条目、目录包装、复制树路径缓冲在 PSRAM；下载的 1 KiB 小缓冲在 HTTP 栈，JSON 序列化仍用已有 cJSON 分配方式。旧 HTTPD 和两个工作任务各 16 KiB PSRAM 栈被移除。仅比较这些明确项，新栈增加 10 KiB 内部需求、移除 48 KiB PSRAM 栈并新增 16 KiB PSRAM 接收缓冲；这不是运行峰值实测。

产品代码保守计 **494 行**：翻译、注释、空行均计入，只扣除上游整文件中的相同行和原文 `FillSsid` 六行，其余无线接线文件整份计入，测试和文档不计，见 [line-count.json](line-count.json)。超过 100 行因为服务器需要完整接入已有文件/目录回调和授权，另有页面回执、构建和网页安全接线；更小的直接写卡或复制授权策略方案均会违反成员隔离/平台接口要求。

[来源与逐段修改](../../../ports/esp32s31/vendor/read_pico_transfer/SOURCE.md)记录提交、原文/入库 SHA-256、许可范围和每一处原文修改行号；24 个入库文件哈希已核对。原厂无线参数出处为 `main/display/screen/cloud_screen/ap/ap_transfer.cc` 的 `StartRadio`（`874538e:1169`，SSID 生成在 187 行，信道/连接数/认证在 1212–1214 行），Plus 发射功率来自该版本 `components/esp-wifi-connect/include/wifi_station.h:16`；沿用基线实现，无新增硬件参数。

目录文件与书同名换后缀，例如 `synthetic.txt` 对应 `synthetic.toc.json`，UTF-8 JSON：`{"encoding":"gbk","entries":[{"t":"第一章","o":0}]}`。`o` 是原始书文件的字节偏移，`t` 是上游最多 40 UTF-8 字节的章节标题；编码值来自原目录算法。书的字节不转码，目录在手机/电脑浏览器计算，设备只保存。

## 上机核对清单

1. 手机在已有 Wi-Fi 打开设备网址、输授权码（或扫码后手输），上传 GBK TXT；在当前成员 `books` 同时核对原书与 `.toc.json`。本次阅读器不读它。
2. 上传中取消一次，确认没有原书或 `.part`；随后成功重传并核对接收行只增一行。试改名、复制、批量下载和删除，确认家人空间外路径无法访问。
3. 三次错码关闭；重新授权后切家人、开飞行模式或到期，旧网页请求被拒，授权码/密码不出现在 AI 快照或日志。
4. 没有 Wi-Fi 时确认现有热点入口可用；关闭后恢复原 Wi-Fi 状态和自动发现；传输期间不睡。插电脑走现有 USB 页面并确认占用提示。
5. 记录开启、上传、取消、退出时内部 RAM/PSRAM 余量和 HTTP 栈高水位，确认关闭后任务/缓冲释放；实测手机吞吐和 10 KiB 内部栈余量。

## 最终验证记录

- 【实测】S31 ASan/UBSan：`100% tests passed out of 78`，82.10 秒。主工程相关 ASan/UBSan：`100% tests passed out of 3`，31.55 秒。
- 【实测】S31 最后一轮编译 warning/error 均为 0，基线同为 0；`build.sh` 的构建、打包、size、size-components、栈报告均完成。

- 【实测】热点名称接线已恢复原厂 `FillSsid`；独立替身驱动检查实际 SSID、信道、连接数、开放认证及暂停/恢复，通过。改回固定名称后断言失败（exit −6）。该只涉及设备的变更已完成最后复编。

- 普通全量中 `unit-s31-cell-started` 在 `cell.c:16` 的 `at_ok` 断言中止；同测试已在独立普通/ASan S31 全量通过。传书任务没有改动蜂窝源文件，保留失败，不越界修蜂窝驱动；全量结束后的单独复测已通过，仍保留全量失败。

- 【实测】普通全量结束：`99% tests passed, 1 tests failed out of 438`，1620.78 秒。共 439 条目，hub 三项按既有配置自动跳过，`memo-save-busy` 按基线禁用；本任务未新增跳过或禁用。唯一失败仍为 `unit-s31-cell-started`。

- 【实测】蜂窝失败项单独复测：`100% tests passed out of 1`，11.88 秒。未改源码；保留普通全量的失败，不用单次复测替换全量记录，失败原因未定位。
- 【实测】最终默认/最大文字截图各 33 条检查通过，增量行撤销更新后 exit 1；原厂热点接线的普通与 ASan/UBSan 替身测试通过，固定名称回退对照 exit −6。

- 【实测】最后 S31 固件：4,173,152 字节，相对基线净减 97,376 字节；warning/error 各 0，栈报告、打包和资源统计已完成。
- 【实测】最终 `gcccheck.py`：主工程 2 份 C 为 0 findings，S31 上游适配和策略夹具 2 份 C 为 0 findings；实际 GBK + 浏览器目录载荷重新逐字节检查通过。

- 【实测】提交后按任务指定的隐私模式扫描基线到 HEAD 的完整差异：0 匹配（grep exit 1）；范围和 diff stat 已核对，无禁止文件或构建产物。来源哈希 24/24、报告本地链接 6/6、17 张截图均小于 300 KB；英文基线前缀不变，仅末尾追加。
