# auto-provision-1：卡上必需文件自动到位——固件带清单，开机核对，插着装机电脑时经串口自动补齐，缺的时候应用说清楚

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `AGENTS.md`。**不碰设备**：不打开 `/dev/cu.*`，不跑 esptool；上机由主会话做（见文末“上机先核对清单”）。

## 为什么

试用版 21 装上后，新阅读打开是整页空白：它要的 TrueType 字体 `InkSC.ttf` 不在存储卡上（`docs/hardware/2026-11-10-trial.md:176`、`docs/tasks/backlog.md:301`），而 `ink_mogo_setup()` 打不开字体就直接返回失败，页面什么都不画（`apps/mogo/port/ink_page_exports.h:19`、`apps/mogo/mogo.c:24-38`）。

用户 2026-11-13 定了 D-138：“我觉得你下次最好把自动把需要的东西帮我拷贝到卡上，系统或者应用升级都应该遵循自动原则”。意思是：装机或升级后，应用需要的东西由系统自动放到卡上，不要求用户进 U 盘模式、拷文件或做任何设置；缺什么时界面要说清楚原因，不能白屏。D-138 已经写明做法：固件带一份“卡上必需文件”清单，开机核对；插着电脑装机时由装机工具经串口把缺的文件送进去（就是本任务），以后联网时从中枢取（本任务只设计接口，不做）。

## 来源（动手前先读）

- 决定：`docs/decisions/README.md` D-138、D-139（新阅读成为唯一的“阅读”；D-139 现在只有决定记录，可能有任务同时改 `apps/mogo/`，见“范围”）。
- 字体从哪来：`tools/mogo_font.py` 的 `convert(source, output, face=2)`（把 Noto Sans CJK SC 转成 Mogo 能读的 TrueType 子集，同时在旁边写 `InkSC.OFL.txt`，原文取自 `tools/appfw-spike/licenses/NotoSansCJK-OFL.txt`）；证据 `docs/tasks/graft-mogo-2/font-evidence.json`（3,897,328 B，fontTools 4.60.2 下 sha256 `15ffa9ea…`，主会话复跑过，本机约 11 秒、结果一致）。今天只有测试在生成它（`tests/graft_mogo_page/run.py:46-51`，那里会找一个带 fontTools 的 Python：本机构建配置的 `build-deps/pyenv/bin/python` **没有** fontTools，系统的 `python3` 有）。
- 阅读怎么找字体：`apps/mogo/port/runtime.c:52-60`（家人自己的 `/me/fonts/InkSC.ttf` 优先，否则全家一份 `/family/fonts/InkSC.ttf`）；`/family/...` 在卡上就是 `/content/...`（`system/services/file/file_service.c:232-234`，大人可以写）；嫁接接口只放行读 `/family/fonts/`（`system/graft/graft.c:152`）。
- 系统往卡上写文件的现成做法：`file_service.c:647-670` 的 `fs_write_atomic_unlocked`（临时文件、`hal_fs_rename`）、复制作业的 `.part`（`:795`、`:817`）；文件服务列目录时不显示 `.` 开头和 `.part`/`.tmp` 结尾的名字（`file_service.c:173-177`）。读卡加 sha256 的写法可参考 `system/services/update/update_service.c:958-1000`（注意它是写进内部闪存，不是写卡）。卡被借给电脑前会通知持有者，最多等 3 秒（`ink_fs_on_release`，`system/include/ink/sys/file.h:143-153`；`file_service.c:1177-1196`）。卡变了时文件服务做的三件事见 `on_card()`（`file_service.c:705-709`）。
- 装机工具：`tools/device/trial_run.py`（`--install` 写 ota_1、字库、otadata，校验后读控制台到 `starting system services` 为止，`install_and_run()`、`main()` 的 install 分支）；串口读写参考 `tools/hw-selftest/run.py:218` 的 `capture()`（端口掉了会重开）和 `tools/bringup/serial_log.py`（`open_port()` 不复位，`reset()` 是 RTS 脉冲；它在模块顶层 import pyserial）。
- 构建：`ports/esp32s31/build.sh:43`（S31 用主机构建的 `$ROOT/build/assets` 打包）；S31 生成头文件的写法 `ports/esp32s31/components/ink_system/CMakeLists.txt:21-27`（`execute_process` 写进 `${CMAKE_BINARY_DIR}/generated/ink/`）；主机的 `CMakeLists.txt:512-523`（i18n、licenses 头文件）和 `:554-558`（`ink_assets`）。CI：`sim.yml` 三个任务都装了 fontTools；**`s31-port.yml` 的 S31 任务没装 fontTools，主机构建只编 `ink_fonts ink_ui_fonts ink_icons`（:63-66）**，本任务必须改它。
- 控制台：S31 的控制台是 USB Serial/JTAG（`ports/esp32s31/sdkconfig.defaults:42`）；现在没有任何代码读控制台输入。原厂代码也没有（`ink-hw/upstream/Metalio-E-INK4-Plus-874538e/main/boards/common/power_policy/power_hw.cc:143-156` 和 `storage/usb_virtual_disk.cc:235-258` 只切 PHY 和焊盘），所以按 IDF 标准驱动做：`usb_serial_jtag_driver_install()`、`usb_serial_jtag_read_bytes()`、`usb_serial_jtag_vfs_use_driver()`。IDF 装了驱动以后，写最多阻塞 50 ms 就丢弃（`~/esp/esp-idf/components/esp_driver_usb_serial_jtag/src/usb_serial_jtag_vfs.c:741-756`）；**接收方向没有流控**：中断把 FIFO 推进环形缓冲，满了就丢（`usb_serial_jtag.c:143-144`），所以接收缓冲必须装得下一整行。“连着电脑”现在的判断在 `ports/esp32s31/hal_usb_s31.c:70-73`（每秒随卡的定时器刷新一次）；浅睡唤醒后重开焊盘在 `hal_power_s31.c:107-112`。
- 中枢以后怎么接：块传输 `blob/fetch`（`docs/hub/device-protocol.md` §3.8），设备端已有调用（`system/services/backup/backup_service.c:810`），模拟中枢有实现（`simulator/environment/sim_backup.c:163`）。
- 模拟器：脚本动词在 `simulator/runtime/script.c`（`usb plug|unplug|eject|copy-in`，`:31-32,158-162`；`sim_usb_plug` 只在脚本上下文里调），USB 模型 `ports/sim/hal/hal_usb_sim.c`；多次开机的测试写法见 `tests/boot_flow_reboot.py`；反向对照写法见 `tests/control_center_reverse.py`。
- 框架边界：`tests/sdk_boundary.cmake:3-4`（应用只能 include 允许列表里的 `ink/sys/*.h`）；能力登记：`tests/integration/ai_surface.c`（每个能力必须在两张表之一，`:276-285`）、`tools/capabilities.py --check`（ctest，`CMakeLists.txt:1013`）。

## 设计

### 1. 清单：编进固件，构建时生成

- 唯一的手写来源 `tools/card_files.json`（提交）：每项 `path`（卡上物理路径）、`app`（需要它的应用 id）、`make`（怎么生成）。首批两项：
  - `/content/fonts/InkSC.ttf`，`app` = 新阅读的应用 id（现在是 `mogo`；D-139 的改名先合入的话用新的），`make` = `mogo_font`（调用 `tools/mogo_font.py` 的 `convert()`，源是 `INK_CJK_FONT`、`INK_CJK_FACE`）；
  - `/content/fonts/InkSC.OFL.txt`，同一个 `app`，`make` = 上一项的附带文件。
- 新工具 `tools/card_files.py`，三个子命令：
  - `bundle --spec tools/card_files.json --noto <ttc> --face N --out <dir>`：生成的文件放进 `<dir>/files/<序号>`，写 `<dir>/manifest.json`：`{"version":1,"digest":"<16位十六进制>","entries":[{"path","size","sha256","app","file"}]}`。
  - `header --manifest <dir>/manifest.json --out <...>/ink/card_files_manifest.h`：写 C 表 `INK_CARD_FILES[]`（`path`、`size`、`sha256[32]`、`app`）、`INK_CARD_FILES_COUNT`、`INK_CARD_FILES_DIGEST`。上限 `INK_CARD_FILES_MAX` = 8 在 `ink/sys/card_files.h` 里命名；`_Static_assert(INK_CARD_FILES_COUNT <= INK_CARD_FILES_MAX)` 写在 `card_files.c` 里 include 生成头文件之后（codex-rules §8）。
  - `verify --manifest <json> --header <h>`：两者的 digest 和条目一致，否则退出非 0。
- `digest` 的算法（两边一致，写进 `docs/system/card-files.md`）：按清单顺序把每项写成 `"<path> <size> <sha256hex> <app>\n"`，整段取 sha256，取前 16 位十六进制。固件直接用编进去的字符串，不在设备上算。
- 构建时就拒绝的（`bundle` 和 `header` 都查）：路径不以 `/content/fonts/` 或 `/content/.system/` 开头；含 `..`、`//`、反斜杠、控制字符；以 `.part` 结尾；路径重复；单个文件超过 5 MiB（codex-rules §4）；条目超过 8 个。
- 主机 `CMakeLists.txt`：
  - 新缓存变量 `INK_FONT_PYTHON`：没给就依次试 `${Python3_EXECUTABLE}`、PATH 里的 `python3`，选第一个能 `import fontTools` 的（和 `tests/graft_mogo_page/run.py:49-50` 一样的规则）；都不行就 `FATAL_ERROR`，写清装法（和缺 CJK 字体时一样）。**不要**只查 `${Python3_EXECUTABLE}`：本机的 `build-deps/pyenv` 没有 fontTools，codex-rules §5 的构建命令会因此失败。
  - 新目标 `ink_card_files`（`add_custom_command` 生成 `${CMAKE_BINARY_DIR}/card-files/` 和 `${CMAKE_BINARY_DIR}/generated/ink/card_files_manifest.h`，依赖 `tools/card_files.json`、`tools/card_files.py`、`tools/mogo_font.py`、`INK_CJK_FONT`、OFL 原文件），`add_dependencies(ink_system ink_card_files)`。
  - 不同机器的 fontTools 版本不同，字体哈希可能不同，所以清单**只在构建时生成，不提交哈希**。
- S31：`ports/esp32s31/components/ink_system/CMakeLists.txt` 照 i18n 头文件那样 `execute_process(... tools/card_files.py header --manifest ${root}/build/card-files/manifest.json --out ${CMAKE_BINARY_DIR}/generated/ink/card_files_manifest.h COMMAND_ERROR_IS_FATAL ANY)`；没有 manifest 就报“先做主机构建（ink_card_files 目标）”。`ports/esp32s31/build.sh` 在打包那一行之后把 `$ROOT/build/card-files` 拷成 `$BUILD/card-files`，再跑 `card_files.py verify`，保证 `ink-s31.bin` 旁边的装机包和固件里的清单是同一份。
- CI：`.github/workflows/s31-port.yml` 的“Generate the same resources as the simulator”一步：apt 加 `python3-fonttools`，`--target` 加 `ink_card_files`（≤ 2 行改动）。`sim.yml` 不用改（三个任务都已有 fontTools）。

### 2. 设备端：卡上文件服务 `system/services/card_files/card_files.c`

公开头文件 `system/include/ink/sys/card_files.h`（属于应用框架：在 `tests/sdk_boundary.cmake` 的 `ALLOWED` 里加 `card_files`，列进“改过的已有测试”，理由是框架多了一个头文件，规则本身没放宽）：

```c
#define INK_CARD_FILES_MAX 8
typedef enum { INK_CARD_READY, INK_CARD_CHECKING, INK_CARD_MISSING, INK_CARD_COPYING, INK_CARD_FAILED, INK_CARD_NO_CARD } ink_card_state_t;
int ink_card_files_init(void);                 /* states, capability, watches; before ink_ui_start */
void ink_card_files_start(void);               /* boot task, after ink_fs_refresh_state(): first check */
/* What the app needs: READY when every manifest entry of this app is in place; percent while copying. Any task. */
ink_card_state_t ink_card_files_state(const char *app, int *percent);
/* UI task: the explanation on an app's page while a file it needs is not usable (section 6). */
lv_obj_t *ink_card_files_notice(lv_obj_t *screen, const char *app, const char *what);
int ink_card_files_notice_snapshot(char *out, size_t cap);
```

**核对**（后台执行器，分步）：
- 什么时候核对：开机（`system/boot/main.c` 在 `ink_fs_refresh_state()` 之后调 `ink_card_files_start()`）；`storage.card` 变成真（插卡）；`storage.mode` 回到 `normal`（U 盘模式结束，人可能在电脑上删了文件）；`usb.connected` 变成真；能力 `card.files_check`。正在核对或传输时又来一次，只记一个“再来”标记，结束后补做一次。
- 怎么核对：`ink_fs_card_hold()` 期间对每项 `hal_fs_size()`，大小不对记“缺”；大小对的读出来算 sha256，每次投递最多读 256 KiB，然后重新投递自己（不要一口气占住后台执行器几秒），缓冲 16 KiB 放 PSRAM。另外，`<path>.part` 的大小和清单相同、整份 sha256 也对上时，**直接在本机完成改名**（见“换文件”），不需要电脑（防止在删掉旧文件和改名之间断电后，要等下次接电脑才补回来）。结果：每项“在 / 缺 / 不对”。
- `ink_fs_card_hold()` 的返回值分开处理：`INK_FS_ERR_NO_CARD` → `no_card`；`INK_FS_ERR_USB`（卡借给电脑了）→ 状态不变，等 `storage.mode` 回到 `normal` 再核对，**不**报成没卡。
- 结论写进状态：`card.files`（字符串：`ready`、`checking`、`missing`、`copying`、`failed`、`no_card`）和 `card.files_progress`（0–100，`min_interval_ms` = 2000）。先写好内部结论，再改状态（codex-rules §8 “先收尾、再改对外能看到的状态”）。日志一行：`card files: need=N ok=… failed=… check_ms=…`（不写别的）。
- 有缺的、并且 `hal_usb_host_connected()` 为真时，开一次传输（见下）。缺但没连电脑：状态 `missing`，什么都不写。
- **换文件**（正式路径上已经有文件，但内容不对；或 `.part` 已完整，等改名）：先在界面任务上确认 `ink_app_state_of(app) == INK_APP_STOPPED`（`system/include/ink/sys/appreg.h:76`），再把“删旧文件、`hal_fs_rename`”作为一个作业投到 `INK_EXEC_BG`。Mogo 开关字体都在后台执行器的作业里（`mogo.c:40-53`、`:73-80`），而且是排在它的 closed/cleanup 之后的，所以换文件时字体一定已经关了。应用在运行就这次跳过，等下次。原因：卡上文件系统删除正打开的文件不安全。

**传输**（只在需要时建的临时任务，做完就退出）：
- `osal_task_create`：名字 `cardfiles`，`OSAL_PRIO_LOW`，栈 6 KiB，`OSAL_TASK_STACK_EXTERNAL`（照 `system/autotest/device.c:148-151`）。这个任务**不许碰内部闪存**：不调 `ink_db_*`、设置、`/user`；只用 `hal_fs_*`（卡）、状态表、`ink_log`、`ink_probe_count`、`ink_exec_post`。同一时间只有一个。
- 从发第一行 `HELLO` 起就 `ink_power_hold(INK_NEED_AWAKE)`，到 `BYE` 或放弃时释放。`ink_fs_on_release` 注册一个回调：卡要借给电脑时置“停”标记。任务读串口时**按 ≤ 200 ms 一段读**，每段之间看这个标记；看到就关掉 `.part`、`ink_fs_card_release()`，留着 `.part` 下次续传。要求 1 秒内放手（`ink_usb_disk_start` 最多等 3 秒）。
- 只在写文件时持有 `ink_fs_card_hold()`（文件关了就放），符合 `file.h:150-153` 的要求。
- 写之前查剩余空间（`hal_fs_volume_info`）≥ 这一项还差的字节，不够就把这一项判 `DONE <idx> io`，日志写 `no_space`。
- 写法：目标 `<path>.part`。已有 `.part` 且大小 ≤ 清单大小：从“大小向下取整到 64 KiB”处接着写（不截断：`HAL_O_WRITE` 不带 `HAL_O_TRUNC`，用 `hal_fs_seek`）；比清单大就删掉，从 0 开始。父目录不在就逐级建。每 64 KiB `hal_fs_sync` 一次。写完关文件，**整份重新读一遍算 sha256**，和清单的哈希、大小都相等才去“换文件”（上面的流程）；不相等就删 `.part`。如果这次是从中间续传的，就在同一次传输里从 0 重传一次（可能是旧清单留下的 `.part`）；从 0 开始的还不对，这一项算失败（`bad_hash`）。
- 改名成功后调文件服务的新函数 `ink_fs_changed_outside()`（`file_service.c` 加 ≤ 3 行，函数体就是 `on_card(NULL)`：`revision` 加一、`index_invalidate()`、刷新剩余空间；声明在 `file.h`），让索引和按版本的条件操作知道卡变了。
- 结束：状态 `ready` 或 `failed`（任一项失败）；`ink_log` 一行汇总；探针 `card_files.session`、`card_files.chunk`、`card_files.chunk_retry`、`card_files.rejected`、`card_files.written`、`card_files.bad_hash`、`card_files.restart`（测试看这些）。

**安全边界（必须同时成立，测试逐条覆盖）**：
1. 写的路径只来自固件里的清单，启动时再查一遍前缀（`/content/fonts/`、`/content/.system/`），不合格的条目当作不存在并记日志；电脑那边**根本没有办法说出一个路径**。写到卡上的只有清单路径和它旁边的 `<path>.part`。
2. 改名到正式路径前，整份 sha256 必须和清单相等。
3. 只有设备自己发出 `HELLO`、并在等待窗口里收到 digest 相同的 `SERVE`，才算“装机工具连着”，才会创建 `.part`；之外收到的任何行一概丢弃。打开串口时先清掉接收缓冲里的残留。
4. 设备一次只有一个未完成的 `GET`；回来的 `DATA` 序号、偏移、长度、CRC 有一样不对就丢弃。

说明：“装机工具”只是一次握手，不是身份认证。电脑上任何程序都能回 `SERVE`，但写进卡的内容只能是和清单哈希一致的那些字节，这就是第 2 条存在的原因。`/content/fonts/` 大人可以经 `/family/fonts` 写，U 盘模式下电脑也能改，不是系统独占的地方。需求指定了这个路径，所以保留，在 `docs/system/card-files.md` 写明：`InkSC.ttf`、`InkSC.OFL.txt` 是系统保留的文件名，内容不对会被换掉；家人要用自己的字体，放 `/me/fonts/`（它优先）。

### 3. 串口行协议（第 1 版）

全部是 ASCII 文本行，以 `INKCF1 ` 开头，字段之间一个空格，十六进制用小写，一行不超过 `INK_CARD_LINE_MAX` = 3000 字节（2048 字节转 base64 是 2732 字节，加上前面的字段）。设备发的行可能带 `\r\n`，电脑那边去掉 `\r`；不以 `INKCF1 ` 开头的行是普通日志，两边都只记录、不处理。

设备 → 电脑：

| 行 | 什么时候 |
| --- | --- |
| `INKCF1 HELLO <digest> <need>` | 核对完。`need` = 要取的条目数。**不论 need 是几**，都每 2 秒重发一次，收到回答或过 30 秒为止（电脑可能晚一点才开始读，IDF 在没人读时会丢输出）。`need = 0` 时收到任何回答都结束，不传输 |
| `INKCF1 GET <idx> <sha256> <offset> <len>` | 要第 `idx` 项从 `offset` 起的 `len` 字节；`len` ≤ 2048（`INK_CARD_CHUNK`） |
| `INKCF1 DONE <idx> ok\|bad\|io\|no` | 一项结束：成功 / 整份哈希不对 / 卡或传输出错 / 电脑说没有 |
| `INKCF1 BYE <ok> <failed>` | 这次传输结束 |

电脑 → 设备（**设备每发一条协议行，电脑恰好回一行**，这样模拟器能同步转接，真串口也一样）：

| 收到 | 回 |
| --- | --- |
| `HELLO`，need = 0 | `INKCF1 ACK` |
| `HELLO`，need > 0 | digest 和装机包相同：`INKCF1 SERVE <digest>`；不同：`INKCF1 NOBUNDLE <装机包的 digest>`（设备当作没有装机工具，什么都不写） |
| `GET` | `INKCF1 DATA <idx> <offset> <len> <crc32> <base64>`，或 `INKCF1 NO <idx> unknown\|sha\|range\|len\|read`（`len` > 2048 回 `len`） |
| `DONE`、`BYE` | `INKCF1 ACK` |

CRC 用 `ink_crc32`（`system/include/ink/sys/crc.h:12`，初值 0），电脑那边用 `zlib.crc32`，单元测试要证明两者一致。base64 解码器在 `card_files.c` 里自己写，≤ 25 行（`update_service.c` 里那个是 static 的，不动它）。

超时和重试（集中成命名常量，写在 `card_files.c` 顶上一处）：`GET` 等回答 2 秒（按 ≤ 200 ms 一段读）；同一块连续失败（超时、CRC 不对、长度不对、对不上号）就重发，第 4 次还不行就 `DONE <idx> io`，结束这次传输（留着 `.part`）；收到 `NO` 这一项直接 `DONE <idx> no`，做下一项。电脑那边：等 `HELLO` 默认 90 秒，传输中 30 秒没有新的协议行就结束。

吞吐【估算】：2 KiB 一块，约 1900 个来回，USB 全速下每块几毫秒，整份字体 20～40 秒。实测由主会话上机做。

### 4. 平台层：控制台的“行通道”

不新开 HAL 头文件，加到 `platform/include/ink/hal_usb.h`（它本来就是“USB 那头的电脑”）：

```c
/** The USB serial console as a line link to an install tool (card files, D-138). 0, or -1 (no computer, USB disk mode, no memory). Task context, one user. */
int hal_usb_console_open(void);
/** Bytes that arrived, waiting up to timeout_ms: n > 0, 0 when none, -1 when closed or the cable went. */
int hal_usb_console_read(void *buf, size_t cap, uint32_t timeout_ms);
/** One whole line, the caller ends it with '\n'. Works without open. Never split by log output. */
int hal_usb_console_write(const char *line, size_t n);
void hal_usb_console_close(void);
```

- S31（写在 `ports/esp32s31/hal_usb_s31.c`）：`open` 时装驱动，**RX 环形缓冲 ≥ 4096 字节（装得下一整行 DATA；驱动满了会直接丢，没有流控）**，TX 1024，装之前查内部 RAM 剩余，不够就返回 -1；然后 `usb_serial_jtag_vfs_use_driver()`，并清掉接收缓冲。`read` 用 `usb_serial_jtag_read_bytes()`。`write` 在 `flockfile(stdout)` 下一次 `fwrite` 加 `fflush`，保证一行不被别的日志插断。`close` 在 `flockfile(stdout)` 下先 `usb_serial_jtag_vfs_use_nonblocking()` 再卸驱动（不让 printf 撞上卸驱动）。U 盘模式（TinyUSB）期间 `open` 返回 -1。`ports/esp32s31/main/CMakeLists.txt:90` 的桩替换表加上这四个符号；`hal_stubs.c` 给出返回 -1 的桩。S31 宿主机测试 `ports/esp32s31/tests/usb.c` 和 `usb_stub/` 补这四个函数的用例（驱动只装一次、RX 大小 ≥ 一行、关闭时先切回再卸、超时换算、U 盘模式下拒绝）。
- 模拟器（`ports/sim/hal/hal_usb_sim.c`）：设备写的协议行交给 `simulator/environment/sim_installer.c`。它在 `vtk_host_call` 里把这一行写给装机进程的标准输入（`posix_spawn` 加两根管道），同步读回一行，再按虚拟时间（每行 2 ms，加上按 1 MB/s 算的传输时间）放进设备的接收队列。回来的行以 `#SILENT` 开头就不送（模拟超时）；以 `#UNPLUG` 开头就**经脚本动作的路径**投递一次 `usb unplug`（`sim_usb_plug` 只能在脚本上下文里调）；装机进程退出或读到 EOF，当作以后都不回答。装机进程由新的命令行参数 `--installer "<命令>"` 给出（`sim_main.c`），只在 `usb plug` 状态下转接；没给这个参数就是“没有装机工具”。这样虚拟时间不受主机快慢影响，确定性测试照旧。

### 5. 装机工具：`tools/device/card_files_serve.py`，由 `trial_run.py` 调用

- `card_files_serve.py`（**模块顶层不 import pyserial、esptool**：CI 没装这两个）：
  - `Server(bundle_dir)` 载入 `manifest.json`，逐个文件算 sha256，和清单不符的文件**不服务**（回 `NO <idx> sha`）。
  - `handle(line)`：返回回复行，非协议行返回 None，规则按第 3 节的表。
  - `serve(open_port, log, wait_s, idle_s)`：`open_port` 是调用方给的“打开串口”函数，返回一个有 `read`/`write` 的对象。读串口，把全部原始行记进 `<out>/card_files.txt`，按表回复。端口掉了（复位、重新枚举）就照 `capture()` 的办法重开，接着服务。返回结果：`all-present` / `done ok=… failed=…` / `no-hello` / `nobundle` / `idle`。
  - 另有 `--stdio --bundle DIR` 模式：每读一行标准输入就写一行标准输出（非协议行写 `#NONE`），给模拟器测试用。
- `trial_run.py`：
  - 新参数 `--card-files DIR`，默认 `<镜像所在目录>/card-files`。`--install` 时第 1 步就检查它存在、`manifest.json` 能读，**不存在就在写任何东西之前停下（退出 2）**，除非明确给 `--no-card-files`。这样不会因为忘了带包，自动原则就悄悄失效。
  - `--install`：看到 `starting system services` 之后，用 `serial_log.open_port()`（不复位）重新打开串口，调 `serve()`，结果写进 `run_log.txt`。新退出码 **5**：装好了、在运行，但卡上文件没有全部到位（看 `card_files.txt`）。0 的含义变成“装好了，卡上文件也齐了”；1～4 不变。
  - 新模式 `--card-files-only`：不需要 `--backup`、`--image`，不调 esptool，不写闪存，只服务；可加 `--reset`（RTS 复位，让设备重新开机核对）。给“装好以后字体被人删了”这类情况用。
  - 文件头的说明、用法和退出码说明一起改。
- 普通的 `trial_run`（不带 `--install`，跑完就恢复出厂）不服务。

### 6. 应用缺文件时说清楚（D-138“不能白屏”）

- 新的共用小件 `system/ui/card_files_notice.c`（声明在 `ink/sys/card_files.h`，见第 2 节）：
  - `ink_card_files_notice(screen, app, what)`：用 `ink_ui_empty(screen, 文字, NULL, NULL, NULL)` 画一段说明，放在嫁接页的图像上面（**不加按钮**：下一步是“连上电脑”，系统会自己做，规范第 0 节第 1 条）。只用主题令牌和现有控件，不画框、不画圈。
  - `ink_card_files_notice_snapshot(out, cap)`：给 AI 的快照，写出应用名、缺什么、当前状态词，例如“阅读：字体还没放到存储卡上，连上电脑后会自动放进去（card.files=missing）”。
  - 监听 `card.files`、`card.files_progress`、`storage.mode`：只改那一个文字对象（D-127 只刷变了的地方，进度按 10% 一档改）。变成 `ready` 且这页还在最上面时调 `ink_ui_redraw()`，页面自己换成阅读。**监听在说明对象删除时（`LV_EVENT_DELETE`）注销**，回调里先确认对象还在（监听表只有 96 格，a80a28b0）。
  - 文字（照规范第 0 节的词表：用“存储卡”“电脑”，不写“装机工具”“同步”“TF 卡”），`%s` 是应用名和 `what`：
    - 缺，没连电脑：“%s要用的%s还没放到存储卡上，连上电脑后会自动放进去”；
    - 正在放：“正在从电脑放%s，%d%%”；
    - 没放成：“%s没放成，下次连上电脑会再放”；
    - 没有卡：“没有存储卡。%s要用的%s放在卡上，插上卡后连电脑会自动放进去”；
    - 卡借给电脑了（`storage.mode=usb`）：“存储卡正给电脑用，在电脑上弹出后%s就能打开”；
    - 状态是 `ready` 但应用还是打不开（例如正在换文件，或家人自己的字体坏了）：“%s要用的%s现在打不开，正在重新检查”，同时调一次 `card.files_check`。
  - 新字符串的英文加进 `assets/i18n/en.tsv`（只在末尾追加）；新文件加进主机的 `INK_I18N_SOURCES`（`CMakeLists.txt:463`）和 S31 的 i18n 源文件表（`ports/esp32s31/components/ink_system/CMakeLists.txt:25`）。`card_files.c` 在 `system/services/` 下，两边都已按目录收进来了，不用加。
- 阅读（`apps/mogo/`）只做最小接线，**判断依据是“它真要用的字体打不开”**，不是“清单说缺”（家人自己的 `/me/fonts/InkSC.ttf` 照旧优先；`tests/graft_mogo_page` 和 `graft_mogo_flow` 就是这样放字体的，不能因此弹出说明）：
  - `ink_page_exports.h:19` 字体打不开时置一个“缺字体”标记（经 `port/runtime.h` 给 `mogo.c` 读）；
  - `ink_mogo_create` 把 `screen` 存进 `M`；`ink_mogo_finished()` 看到这个标记，就在存下的 screen 上画 `ink_card_files_notice(screen, "mogo", TR("字体"))`，能力调用的结果写 `detail`“font not on the card”；
  - 客户端快照 `ink_mogo_snapshot`（`mogo.c:82-85`，`ink_graft_snapshot` 只是转过去）在说明显示时返回 `ink_card_files_notice_snapshot()`；
  - `mogo.c` 加起来不超过 25 行，只动 `create`、`finished`、`ink_mogo_snapshot` 这三处和 `M` 的一个字段。
- 能力 `card.files_check`（“再核对一次存储卡上系统要用的文件”，`INK_FX_LOCAL_WRITE`，`child_ok = true`，只会写清单里核对过的文件）：AI 和以后的中枢用，和“连上电脑自动核对”走同一个函数。按惯例：`python3 tools/capabilities.py` 重新生成 `docs/system/capabilities.md` 后提交；能力进 `tests/integration/ai_surface.c` 给 AI 的那张表，状态 `card.files` 进 `STATES`；这两个文件都只在末尾追加，合并冲突由主会话处理。

### 7. 中枢以后怎么接（只设计，写进新文档 `docs/system/card-files.md`，不写代码）

- `card_files.c` 里把“从哪取一块”做成一个结构，现在只有串口这一个实现：

```c
typedef struct {
    const char *name;                                   /* "usb" now; "hub" later */
    int (*begin)(void);                                 /* 0: someone answers (installer handshake / hub online and bound) */
    int (*fetch)(int idx, uint32_t offset, uint8_t *buf, uint32_t len); /* one checked chunk: 0, or why not */
    void (*end)(int ok, int failed);
} ink_card_source_t;
```

- 中枢实现（以后）：`begin` = 中枢在线且已加入家庭；`fetch` = `blob/fetch {"id":"sha256:<清单里的哈希>","offset","len":65536}`，复用备份服务现有的取块代码（`backup_service.c:810`；现在那里带 `member` 参数，系统文件不按家人取）；写文件、核对、改名完全走同一条路。触发：中枢上线时状态是 `missing` 或 `failed`；后台优先级，实时会话期间不取下一段（`ink_jobs_hold`，设备协议 §3.8）。
- 中枢要加的规则（以后改 `docs/hub/device-protocol.md` §3.8）：系统文件不按家人授权，按“这个哈希在中枢收到的某个固件版本的装机包清单里”授权；装机包随固件一起上传到中枢（概要设计里固件那一行）。
- 系统升级：新固件的清单变了，开机核对就会发现，连上电脑或（以后）连上中枢时自动补，这就是 D-138 的“升级也遵循自动原则”。

### 8. 迁移

1. 已经装了试用版 21 的设备：主会话用新的 `trial_run.py --install` 装新版，装机包在 `build-s31/card-files/`（交付目录要连它一起拷），设备开机核对、自动要字体，用户什么都不用做。
2. 家人自己的 `/me/fonts/InkSC.ttf` 照旧优先，不动、不删。
3. 以后字体换了版本（哈希变了）：卡上旧文件算“不对”，下次连电脑、阅读没在运行时自动换掉；旧的 `.part` 续传对不上时自动从 0 重传。
4. `docs/handoff/trial-guide.md` 的装机命令改成带装机包的写法，写上退出码 5 和 `--card-files-only`。`backlog.md:301`“下次接设备时用 U 盘模式拷进去”作废，写进报告的“要改的文档”，由主会话改。

## 范围和预算

- **只改**：
  - 新文件：`system/services/card_files/card_files.c`、`system/include/ink/sys/card_files.h`、`system/ui/card_files_notice.c`、`tools/card_files.json`、`tools/card_files.py`、`tools/device/card_files_serve.py`、`simulator/environment/sim_installer.c`、`docs/system/card-files.md`、`tests/card_files/`（验收里的测试）、`docs/tasks/auto-provision-1/`。
  - 改动：`platform/include/ink/hal_usb.h`、`ports/esp32s31/hal_usb_s31.c`、`ports/esp32s31/hal_stubs.c`、`ports/esp32s31/main/CMakeLists.txt`（桩替换表）、`ports/esp32s31/components/ink_system/CMakeLists.txt`（生成头文件、i18n 源）、`ports/esp32s31/build.sh`（拷装机包并 verify，≤ 8 行）、`ports/esp32s31/tests/usb.c` 和 `usb_stub/`、`ports/sim/hal/hal_usb_sim.c`、`simulator/runtime/sim_main.c`（`--installer`）、`simulator/sim.h`、`CMakeLists.txt`（源文件表、`ink_card_files` 目标、`INK_FONT_PYTHON`、新测试）、`.github/workflows/s31-port.yml`（≤ 2 行）、`system/boot/main.c`（`init` 和 `start` 各一行）、`system/services/file/file_service.c` 和 `system/include/ink/sys/file.h`（`ink_fs_changed_outside()`，≤ 5 行）、`apps/mogo/mogo.c`、`apps/mogo/port/ink_page_exports.h`、`apps/mogo/port/runtime.h`（合计 ≤ 25 行）、`assets/i18n/en.tsv`（末尾追加）、`tests/sdk_boundary.cmake`（`ALLOWED` 加一个词）、`tests/integration/ai_surface.c`（末尾追加）、`tools/device/trial_run.py`、`docs/handoff/trial-guide.md`、重新生成的 `docs/system/capabilities.md`。
  - 下面这些文件别的任务（设置、控制中心、相机）也可能在改：`CMakeLists.txt`、S31 `ink_system` 的 i18n 源表、`main.c`、`en.tsv`、`ai_surface.c`、`hal_stubs.c`、S31 `main/CMakeLists.txt` 的桩替换表、`sim_main.c`、`sim.h`、`s31-port.yml`。在这些文件里只追加或只加最少几行，不调整别人的行，冲突由主会话处理。
- **不碰**：`tools/mogo_font.py`（只调用它的 `convert()`）、`third_party/mogo/`、`apps/reader/`、`system/graft/`、`system/services/update/update_service.c`、外壳和主屏、锁屏、控制中心、设置的文件、`tools/hw-selftest/run.py`（只调用它的 `serial_helpers`）、`tools/bringup/`、`tools/device/flash_plan.py`、`hub/`、codex-rules §7 列的其它共用文档（`backlog.md`、`ui-dependencies.md`、`requirements.md`、`ideas.md`、`decisions/`）。
- D-139 的改名任务如果同时改 `apps/mogo/`：本任务在 `mogo.c` 只动上面三处；合并时冲突，以对方的改名和能力为准，把这三处接到新名字上，清单里的 `app` 一起改，写进报告。
- **产品代码行数预算**（不算测试、文档、生成的头文件）：
  - 固件（`system/`、`apps/`、`platform/`、`ports/`）合计 ≤ **680 行**：`card_files.c` ≤ 440，`card_files_notice.c` ≤ 90，S31 控制台 ≤ 60，模拟器 HAL ≤ 40，阅读接线 ≤ 25，启动、文件服务、头文件 ≤ 25。
  - 模拟器环境（`sim_installer.c`、`--installer`）≤ **150 行**。
  - 工具（`tools/card_files.py` ≤ 170，`card_files_serve.py` ≤ 220，`trial_run.py` 增改 ≤ 60，`build.sh` ≤ 8，CMake 和 workflow ≤ 50）合计 ≤ **480 行**。
  - 快超预算时停下，写清楚原因和看过的更小做法。
- **新增的机制**（任务书明确要求的，报告里逐行列出）：清单格式和生成步骤、串口行协议第 1 版、临时任务 `cardfiles`、状态 `card.files` / `card.files_progress`、能力 `card.files_check`、`.part` 续传（沿用已有约定）、`trial_run.py` 退出码 5 和 `--card-files-only`、`INK_FONT_PYTHON`。此外不加别的机制（不加“已核对”的缓存记录，不加后台定时重试）。

## 验收

每条在报告的验收表里占一行：编号、结论、证据、命令。

1. **工具和清单**：`tests/card_files/tool_test.py`（ctest `card-files-tool`）：坏路径（`/user/sys/x`、`/content/../x`、`/content/.m/owner/x`、`.part` 结尾）、重复、超过 5 MiB、超过 8 项都被拒；同一份 manifest 两次生成的头文件逐字节相同；`verify` 能发现 digest 或条目不一致；`INK_FONT_PYTHON` 的选择规则（第一个解释器没有 fontTools 时选第二个）。真实构建后：`python3 tools/card_files.py verify --manifest build/card-files/manifest.json --header build/generated/ink/card_files_manifest.h` 退出 0；报告写两项的大小和哈希前 16 位、用的 fontTools 版本、生成字体用了多少秒。
2. **设备端协议单元测试**：`tests/card_files/unit.c`（ctest `card-files-unit`，用一份测试清单编译 `card_files.c` 的解析和校验部分）：`DATA` 的 base64 坏、长度不对、CRC 不对、序号或偏移对不上都丢弃；`SERVE` 的 digest 不同不开传输；不合格的清单路径在启动时被当作不存在；`HELLO`、`GET` 的格式和第 3 节逐字一致；一行超过 `INK_CARD_LINE_MAX` 整行丢弃；`ink_crc32` 和 `zlib.crc32` 对同一份数据的结果相同（用固定向量）。
3. **模拟器多次开机**（ctest `card-files-sim`，`tests/card_files/run.py`；用同一个 `--sd-dir` 和 `--flash-image` 连续开机。装机进程是 `tests/card_files/fake_host.py --bundle build/card-files [故障选项]`，它包着 `card_files_serve.Server` 注入故障；**注入“内容错”时绕过 Server 自己的哈希检查**）：
   - A 没有装机工具（插着线、没给 `--installer`）：`card.files=missing`；打开阅读，快照里有“字体还没放到存储卡上”，屏幕上有这段字（不是空白），截图；卡上没有 `.part`，探针 `card_files.written=0`。
   - B 有装机工具，第 5 块数据错一个字节（CRC 仍按正确内容算）一次、第 9 块不回答一次：两个文件都写到 `/content/fonts/`，和 `build/card-files` 逐字节相同；`card_files.chunk_retry=2`；状态 `ready`；`mogo.open` 打开一本书，快照有正文（`面板=正文`）。
   - C 再开机，装机工具还在：只有 `HELLO … 0`（收到 `ACK` 就停），装机进程收到的 `GET` 为 0，文件修改时间和大小不变，`card_files.written=0`。
   - D 新卡，装机工具传到第 300 块时拔线（`#UNPLUG`）：留下 `.part`。E 再开机：第一个 `GET` 的偏移 > 0 且是 64 KiB 的整数倍，最后文件正确。
   - E2 在 D 留下的 `.part` 上把前 64 KiB 里的一个字节改掉（模拟旧清单留下的 `.part`），再开机：续传后哈希不对，自动从 0 重传一次，最后文件正确，`card_files.restart=1`。
   - E3 卡上放一份完整、正确的 `<path>.part`、正式路径上没有文件，不给装机工具：开机后在本机改名完成，状态 `ready`，`card_files.written=0`。
   - F 新卡，fake_host 送出的字体中间改了一个字节（每块 CRC 都按改过的内容算，绕过 Server 的检查）：正式路径上没有文件、`.part` 已删、`card_files.bad_hash=1`、状态 `failed`，阅读的说明写“没放成”。
   - G 新卡，装机包的 digest 不同：设备回到“没有装机工具”，什么都没写。另一轮：fake_host 回答 `HELLO` 时直接发一行 `DATA`，之后对 `GET` 正常回答：同样什么都没写。
   - H 卡借给电脑（`storage.mode=usb`）时打开阅读：说明写“存储卡正给电脑用”，不写“没有存储卡”；弹出后核对一次。
   - 整个测试 ≤ 300 秒；默认字号和最大一档各截一张说明页，放进 `docs/tasks/auto-provision-1/`（单张 ≤ 300 KB）。
4. **装机工具对假串口**（ctest `card-files-serve`，`tests/card_files/serve_test.py`，用临时生成的两个小文件做装机包，不需要 fontTools、pyserial、esptool）：`HELLO` need>0 时 digest 相同回 `SERVE`、不同回 `NOBUNDLE`；need=0 回 `ACK`，立即结束为 `all-present`；`GET` 的序号不存在、哈希不对、越界、`len > 2048` 都回 `NO`；从中间偏移续传的 `DATA` 正确；`DONE`、`BYE` 回 `ACK`；日志行原样记进 `card_files.txt`，不回复；重发的 `HELLO` 每次都回；端口中途掉了再回来能接着服务；设备一直不说话，按 `wait_s` 结束为 `no-hello`；装机包里哈希不符的文件拒绝服务。`trial_run.py`：`--install` 缺装机包时在写任何东西之前退出 2（用 `--dry-run` 和临时生成的假备份测）；`--card-files-only` 不要求 `--backup`、不调 esptool。
5. **S31**：`ctest -R unit-s31-usb` 通过（含新用例）；`ports/esp32s31/build.sh` 编过、没有新警告；`build-s31/card-files/manifest.json` 存在，`card_files.py verify` 对 `build-s31/generated/ink/card_files_manifest.h` 退出 0；报告写出 ELF 里有 `ink_card_files_start` 的证据、固件增大多少字节，以及打开期间额外占多少内部 RAM。
6. **反向对照**（手动，`tests/card_files/reverse.py`，照 `tests/control_center_reverse.py`：改源码、重编、跑对应场景、改回）：去掉改名前的 sha256 比较 → F 失败；去掉每块的 CRC 检查 → B 失败；续传总从 0 开始 → E 失败；去掉“续传不对就从 0 重传” → E2 失败；`HELLO` 的任何回答都当作 `SERVE` → G 失败；去掉阅读的说明 → A 失败；装机工具不查 `GET` 的哈希 → `serve_test` 失败。每条记下失败的原话。
7. **界面规矩**：`python3 tools/ui_scale_check.py` 通过（含新文件）；说明页只用 `ink_ui_*` 和主题令牌；进度变化只刷那一行（测试里数刷新区域，或在报告写清用的是哪个现有计数）；`sdk-boundary` 通过。
8. **全量**：codex-rules §5 的普通全量 ctest（含 `capabilities` 检查、`ai_surface`）、`build-asan` 的内存检查（至少 `card-files-*`、`mogo-*`、`unit-s31-usb`）都跑，报告写结果原话；`ink-codex/tools/gcccheck.py` 的结果；产品代码行数（按上面三类分别写）。已有测试只改 `tests/sdk_boundary.cmake`（加一个允许的头文件）和 `ai_surface.c`（末尾追加），逐条列进“改过的已有测试”，说明为什么仍然一样严；别的都不改。
9. **隐私和夹带**：`git diff --stat` 只有上面列的文件；`git diff <基线>..HEAD | grep -nE '/Users/|/private/tmp|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}'` 为空；装机包和生成的字体不提交。

## 上机先核对清单（主会话做，执行者只写进报告）

1. 装了 USB Serial/JTAG 驱动后日志照常输出；电脑不读时设备不卡（IDF 说写最多等 50 ms 就丢）；卸驱动、切回之后日志照常，没有崩溃。
2. 接收缓冲按一整行 DATA 配的大小够用：驱动在缓冲满时**会丢**数据（没有流控），记下每块 CRC 重发的次数；重发多就把缓冲再加大或把块改小。
3. `trial_run.py --install` 在看到系统启动后重开串口，能收到重发的 `HELLO`（need>0 和 need=0 两种都试）。
4. 整份字体传完的时间、阅读能打开；再开机只有 `HELLO … 0`。
5. 开机核对 3.9 MB 字体花多少毫秒（日志里的 `check_ms`），超过 3 秒就回来商量要不要加“已核对”记录（本任务不做）。
6. 传输期间设备不睡；U 盘模式和传输互相让开（借卡时传输 1 秒内放手，留下 `.part`）。
7. 打开期间内部 RAM 的最低剩余量（驱动的收发缓冲）。
