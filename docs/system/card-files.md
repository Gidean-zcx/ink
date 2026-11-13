# 卡上必需文件（D-138）

应用要的东西如果放在存储卡上（现在是阅读（应用 id `reader`）的字体），由系统自动放进去，用户不用进 U 盘模式、不用拷文件。固件带一份清单，开机核对；插着装机电脑时经 USB 串口自动补齐；缺的时候应用页面说清楚原因，不白屏。任务书：auto-provision-1。

## 1. 清单

- 唯一的手写来源 `tools/card_files.json`：每项 `path`（卡上物理路径）、`app`（要它的应用 id）、`make`（怎么做出来）。现在两项：`/content/fonts/InkSC.ttf`（`mogo_font`，由 `tools/mogo_font.py` 的 `convert()` 从 `INK_CJK_FONT` 转出）和旁边的 `/content/fonts/InkSC.OFL.txt`（`mogo_font_ofl`，字体许可）。
- 构建时生成，不提交哈希（不同机器的 fontTools 可能做出不同字节）：
  - `tools/card_files.py bundle` 做出文件，写 `build/card-files/files/<序号>` 和 `build/card-files/manifest.json`：`{"version":1,"digest":"<16位十六进制>","entries":[{"path","size","sha256","app","file"}]}`。
  - `tools/card_files.py header` 写 `build/generated/ink/card_files_manifest.h`（C 表 `INK_CARD_FILES[]`、`INK_CARD_FILES_COUNT`、`INK_CARD_FILES_DIGEST`）；内容没变就不改文件。
  - `tools/card_files.py verify` 核对二者一致。S31 构建从主机构建的 `build/card-files/manifest.json` 生成同一个头文件，`ports/esp32s31/build.sh` 把整个装机包拷到 `build-s31/card-files/` 并 verify，保证固件里的清单和旁边的装机包是同一份。
- 做字体要 fontTools。CMake 的 `INK_FONT_PYTHON` 没给时，依次试 `Python3_EXECUTABLE`、PATH 里的 `python3`，取第一个能 `import fontTools` 的（`card_files.py python`），都不行就停下并写明装法。
- **digest**：按清单顺序把每项写成 `"<path> <size> <sha256十六进制> <app>\n"`，整段取 sha256，取前 16 位十六进制。固件直接用编进去的字符串，不在设备上算。
- 构建时就拒绝（`bundle`、`header` 都查）：不在 `/content/fonts/` 或 `/content/.system/` 下；含 `..`、`//`、反斜杠、控制字符；以 `.part` 或 `/` 结尾；路径重复；单个文件超过 5 MiB；超过 8 项（`card_files.c` 里有同样的静态检查）。固件启动时再查一遍前缀和这些字符，不合格的条目当作不存在并记日志。

## 2. 设备上

`system/services/card_files/card_files.c`，公开接口 `ink/sys/card_files.h`。

- **什么时候核对**：开机（`ink_card_files_init()` 在界面启动前调用，最后把第一次核对投到后台执行器）；插卡（`storage.card` 变真）；U 盘模式结束（`storage.mode` 回到 `normal`）；连上电脑（`usb.connected` 变真）；能力 `card.files_check`。正在核对或传输时又来一次，只记一个“再来”，结束后补做一次。
- **怎么核对**：在后台执行器上分步做，每步最多读 256 KiB 算 sha256，然后重新投递自己。每一步都先等屏幕没在刷新、0.5 秒内没有按键和触摸（和存储服务的后台活一样），不在开机画首页、开应用或有人在用的时候占着后台执行器和卡；装好的卡每次开机都要整份读一遍字体（约 3.9 MB），这样才不挡别的事。核对在等安静时连上的电脑，就由这次核对开的传输接待，它晚到的“已连接”通知不再算新情况。大小不对算“缺”；大小对、哈希不对算“不对”。`<path>.part` 大小和清单相同、整份哈希也对时，在本机直接改名，不需要电脑（防止删旧文件和改名之间断电）。卡借给电脑时（`INK_FS_ERR_USB`）状态不变，等弹出后再核对。
- **状态**：`card.files`（`ready`、`checking`、`missing`、`copying`、`failed`、`no_card`）和 `card.files_progress`（0–100，最多每 2 秒通知一次）。日志一行 `card files: need=… ok=… failed=… check_ms=…`；传输结束一行 `card files: session usb ok=… failed=…`。探针：`card_files.check`、`.session`、`.chunk`、`.chunk_retry`、`.rejected`、`.written`、`.bad_hash`、`.restart`。
- **传输**：有缺且连着电脑时，开一个临时任务 `cardfiles`（低优先级，6 KiB 栈放 PSRAM，做完就退出）。它只碰存储卡、状态表、日志、探针和执行器投递，不碰内部闪存。从发第一行 `HELLO` 起不让设备睡，到 `BYE` 或放弃为止。
  - 只在写文件时占着卡；卡要借给电脑时（`ink_fs_on_release`），读串口按 200 ms 一段，看到就关掉 `.part`、放手，`.part` 留着下次续传。
  - 写之前查剩余空间，不够就这一项判 `io`（日志 `no_space`）。
  - 写到 `<path>.part`：已有且不比清单大，从“大小向下取整到 64 KiB”处接着写（不截断）；比清单大就删掉从 0 开始。每 64 KiB 同步一次。写完整份重读算 sha256，和清单相等才改名；不等就删 `.part`，如果这次是续传的，就在同一次传输里从 0 再来一次（可能是旧清单留下的 `.part`），从 0 开始的还不对就判失败（`bad_hash`）。
  - **换文件**：正式路径上没有文件时直接改名。已有文件（内容不对）时，先在界面任务上确认要它的应用放开了它：应用是 `STOPPED`，或者它的页面正显示缺文件说明（第 5 节：显示说明时应用不开着它的任何文件）；再在后台执行器上删旧文件、改名。应用开着它（比如正在用旧版本字体读书）就这次不换，日志一行 `not replaced yet`，`.part` 留着，下次核对时在本机换上。改名后调 `ink_fs_changed_outside()`，文件索引和按版本的条件操作知道卡变了。
  - **结果照实说**：一项只有真的改名到正式路径才回 `DONE <idx> ok`、算进 `BYE` 的 ok。整份对了但这次没换上（应用开着旧文件、改名失败）回 `DONE <idx> io`、算 failed，接着做下一项；卡被借走就结束这次传输。
- **安全边界**（同时成立）：
  1. 写的路径只来自固件里的清单（启动时再查一遍）；电脑那边根本没有办法说出一个路径。写到卡上的只有清单路径和它旁边的 `<path>.part`。
  2. 改名到正式路径前，整份 sha256 必须和清单相等。
  3. 只有设备自己发出 `HELLO`、并在等待窗口里收到 digest 相同的 `SERVE`，才算“装机工具连着”，才会创建 `.part`；之外收到的任何行一概丢弃。打开串口时先清掉接收缓冲里的残留。
  4. 一次只有一个未完成的 `GET`；回来的 `DATA` 序号、偏移、长度、CRC 有一样不对就丢弃。
- “装机工具”只是一次握手，不是身份认证：电脑上任何程序都能回 `SERVE`，但写进卡的只能是和清单哈希一致的字节（第 2 条）。
- **保留的文件名**：`/content/fonts/` 大人可以经 `/family/fonts` 写，U 盘模式下电脑也能改，不是系统独占的地方。`InkSC.ttf`、`InkSC.OFL.txt` 是系统保留的文件名，内容不对会被换掉。家人要用自己的字体，放 `/me/fonts/InkSC.ttf`，它优先，系统不动它。

## 3. 串口行协议（第 1 版）

ASCII 文本行，以 `INKCF1 ` 开头，字段之间一个空格，十六进制小写，一行不超过 3000 字节（2048 字节转 base64 是 2732 字节）。设备发的行可能带 `\r\n`。不以 `INKCF1 ` 开头的行是普通日志，两边都只记录、不处理。

设备 → 电脑：

| 行 | 什么时候 |
| --- | --- |
| `INKCF1 HELLO <digest> <need>` | 核对完，`need` = 要取的条目数。不论 need 是几，每 2 秒重发一次，收到回答或过 30 秒为止。`need = 0` 时收到任何回答都结束 |
| `INKCF1 GET <idx> <sha256> <offset> <len>` | 第 `idx` 项从 `offset` 起的 `len` 字节，`len` ≤ 2048 |
| `INKCF1 DONE <idx> ok\|bad\|io\|no` | 一项结束：成功 / 整份哈希不对 / 卡或传输出错 / 电脑说没有 |
| `INKCF1 BYE <ok> <failed>` | 这次传输结束 |

电脑 → 设备（设备每发一条协议行，电脑恰好回一行）：

| 收到 | 回 |
| --- | --- |
| `HELLO`，need = 0 | `INKCF1 ACK` |
| `HELLO`，need > 0 | digest 和装机包相同：`INKCF1 SERVE <digest>`；不同：`INKCF1 NOBUNDLE <装机包的 digest>`（设备当作没有装机工具） |
| `GET` | `INKCF1 DATA <idx> <offset> <len> <crc32> <base64>`，或 `INKCF1 NO <idx> unknown\|sha\|range\|len\|read` |
| `DONE`、`BYE` | `INKCF1 ACK` |

CRC 是 `ink_crc32`（初值 0），和 `zlib.crc32` 相同（单元测试用固定向量证明）。超时和重试：`GET`、`DONE`、`BYE` 等回答 2 秒；同一块连续失败（超时、CRC、长度、对不上号）就重发，第 4 次还不行就 `DONE <idx> io` 并结束这次传输（`.part` 留着）；收到 `NO` 这一项 `DONE <idx> no`，做下一项。电脑那边：等 `HELLO` 默认 90 秒，传输中 30 秒没有新的协议行就结束。

吞吐【估算】：2 KiB 一块，整份字体约 1900 个来回，USB 全速下 20～40 秒，实测待上机。

## 4. 平台层和装机工具

- `ink/hal_usb.h` 的 `hal_usb_console_open/read/write/close`：S31 装 IDF 的 USB Serial/JTAG 驱动（接收环形缓冲 4096 字节，装得下一整行 `DATA`；驱动没有流控，满了就丢），写在 stdout 锁下一次写完；关时先切回非阻塞再卸驱动；U 盘模式下打不开。模拟器把设备写的行交给 `--installer` 给的装机进程，按虚拟时间送回回答（`simulator/environment/sim_installer.c`）。
- `tools/device/card_files_serve.py`：`Server`（只服务和清单一致的文件）、`serve()`（端口掉了会重开）、`--stdio`（给模拟器）。模块顶层不 import pyserial、esptool。
- `tools/device/trial_run.py`：`--install` 装好、看到系统启动后服务装机包（默认是镜像旁边的 `card-files/`），记在 `card_files.txt`；没有装机包就在写任何东西之前停下（退出 2），除非 `--no-card-files`。退出码 5：装好了、在运行，但卡上文件没有全部到位。`--card-files-only [--reset]`：不要备份和镜像，不调 esptool，只服务。

## 5. 应用缺文件时

`ink_card_files_notice(screen, app, what, reopen)` 在应用页上画一段说明（`ink_ui_empty`，不加按钮），只随状态改那一段文字（进度按 10% 一档）。约定：显示说明期间应用不开着它要的任何文件（阅读打不开字体时会把字体卸掉），所以系统可以在这时换掉内容不对的文件。状态变成 `ready` 且这页还在最上面时，说明调一次 `reopen()`：应用在同一个屏幕上重新打开原来的内容（阅读回到原来那本书或书架），画好后自己把说明拿掉；说明的文字一直留到页面画好，不重建页面、中间没有空白页、不整屏闪刷。`reopen` 为 NULL 的页面改为重画一次。`ink_card_files_notice_snapshot(app, out, cap)` 给 AI 的快照写出应用名、说明和状态词；这个应用没有显示说明时返回 0。阅读的判断依据是“它真要用的字体打不开”（家人自己的 `/me/fonts/InkSC.ttf` 照旧优先），不是“清单说缺”。 阅读自己的说明页只管书本身的问题（书打不开、书架空、文件损坏或格式不支持）；字体和存储卡的问题只走这段说明，同一个原因不会有两种页面。

## 6. 以后从中枢取（只设计，没有代码）

- `card_files.c` 里“从哪取一块”是一个结构 `ink_card_source_t`（`begin`、`fetch`、`done`、`end`），现在只有串口一个实现（`done` 是为串口的 `DONE` 行加的，中枢实现可以什么都不做）。
- 中枢实现：`begin` = 中枢在线且已加入家庭；`fetch` = `blob/fetch {"id":"sha256:<清单里的哈希>","offset","len":65536}`（[设备协议](../hub/device-protocol.md) §3.8），复用备份服务现有的取块代码（`system/services/backup/backup_service.c`；现在那里带 `member` 参数，系统文件不按家人取）；写文件、核对、改名完全走同一条路。触发：中枢上线时状态是 `missing` 或 `failed`；后台优先级，实时会话期间不取下一段（`ink_jobs_hold`）。
- 中枢要加的规则（以后改设备协议 §3.8）：系统文件不按家人授权，按“这个哈希在中枢收到的某个固件版本的装机包清单里”授权；装机包随固件一起上传到中枢。
- 系统升级：新固件的清单变了，开机核对就会发现，连上电脑或（以后）连上中枢时自动补，这就是 D-138 的“升级也遵循自动原则”。

## 7. 迁移

1. 已经装了试用版 21 的设备：用新的 `trial_run.py --install` 装新版（交付目录连 `card-files/` 一起拷），设备开机核对、自动要字体，用户什么都不用做。
2. 家人自己的 `/me/fonts/InkSC.ttf` 照旧优先，不动、不删。
3. 以后字体换了版本（哈希变了）：卡上旧文件算“不对”，下次连电脑、阅读没在运行时自动换掉；旧的 `.part` 续传对不上时自动从 0 重传。
