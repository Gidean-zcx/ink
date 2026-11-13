# auto-provision-1 报告

（子代理不能写报告文件，由主会话照它交回的原文存进分支。）

TASK_COMPLETE

# auto-provision-1 报告

现在缺字体的情况：新阅读打不开字体时，页面会写一段说明，不再是白屏。插上装机电脑后，设备自己发 HELLO，向装机工具要缺的文件，写进 `.part`，整份哈希核对无误才改成正式文件名；改完说明自动消失，回到阅读。不碰设备，上机由主会话做。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 工具和清单 | 通过【实测】 | 这些都会被拒：坏路径（`/user/sys/x`、`/content/../x`、`/content/.m/owner/x`、`.part` 结尾）、路径重复、超过 5 MiB、超过 8 项；`header` 两次生成逐字节相同；`verify` 能发现 digest 不一致，也能发现条目不一致；`INK_FONT_PYTHON` 第一个解释器没有 fontTools 时选第二个，一个都没有就失败。真实构建 verify 退出 0：InkSC.ttf 3,897,328 B，sha256 前 16 位 15ffa9ea43e5d6ae；InkSC.OFL.txt 4,301 B，前 16 位 6a73f9541c2de741；digest 39628215e347e53d；fontTools 4.60.2（本机选中的 INK_FONT_PYTHON 是 /usr/bin/python3）；生成约 10.2 秒（user 9.85 s） | ctest card-files-tool；`python3 tools/card_files.py verify --manifest build/card-files/manifest.json --header build/generated/ink/card_files_manifest.h` |
| 2 设备端协议单元测试 | 通过【实测】 | 测试清单是手写的 `tests/card_files/manifest/`（生成器会拒绝里面的坏路径，固件也必须拒绝）。覆盖：坏路径在启动时被当作不存在；HELLO 和 GET 的格式与第 3 节逐字一致；HELLO 每 2 秒一次、30 秒放弃；SERVE 带别的 digest、NOBUNDLE、need=0 收到任何回答，三种情况都不开传输；DATA 的 CRC、序号、偏移、长度、base64 字符、base64 长度任一项错都丢弃并重要（rejected=2）；超过 3000 字节的行整行丢弃；`ink_crc32` 和 zlib.crc32 用 3 组固定向量对过 | ctest card-files-unit |
| 3 模拟器多次开机 | 通过【实测】 | A：状态 missing；快照和屏幕上都有“字体还没放到存储卡上”；没有 `.part`；written=0；默认字号和最大一档各截一张。B：两个文件和装机包逐字节相同；chunk_retry=2；状态 ready；打开书后 `面板=正文`。C：只发了 `HELLO <digest> 0`，GET 为 0 次，文件大小和修改时间都没变，written=0。D：拔线后留下 InkSC.ttf.part。E：第一个 GET 的偏移大于 0 且是 64 KiB 的整数倍，最后文件正确。E2：restart=1，最后文件正确。E3：written=0，本机改名后状态 ready。F：bad_hash=1，状态 failed，正式路径上没有字体，`.part` 已删，说明写“没放成”。G1（digest 不同）和 G2（回 HELLO 时直接发 DATA）：什么都没写，GET 为 0 次。H：说明写“存储卡正给电脑用”，没有写“没有存储卡”；弹出后 card_files.check 加一，阅读自己回来了（停在书架，原因见“和设计不一致的”第 7 条）。整个测试 3～4 秒（ASan 下 11 秒）。截图 4 张放在 docs/tasks/auto-provision-1/，每张 15～20 KB | ctest card-files-sim（`tests/card_files/run.py --build build`） |
| 4 装机工具对假串口 | 通过【实测】 | SERVE、NOBUNDLE、ACK（need=0，立即结束为 all-present）都对；GET 的四种不合格情况（序号不存在、哈希不对、越界、len>2048）都回 NO；从中间偏移续传的 DATA 和 CRC 正确；DONE、BYE 回 ACK；日志行照原样记录、不回复；重发的 HELLO 每次都回；端口中途掉了再回来能接着服务；设备不说话时按 wait_s 结束为 no-hello；装机包里哈希不符的文件回 `NO <idx> sha`。trial_run.py：`--install --dry-run` 缺装机包时在写任何东西之前退出 2；`--card-files-only` 不要 `--backup`、不调 esptool，结果为 all-present 时退出 0，有失败时退出 5。用 build-deps/pyenv 的 Python 跑（没有 fontTools 和 pyserial） | ctest card-files-serve |
| 5 S31 | 通过【实测】 | unit-s31-usb 通过（新用例：驱动只装一次，RX ≥ 3001，TX 1024，内部 RAM 不够时拒绝且什么都不装，打开时清掉残留，超时向上取整到 tick（1 ms 换成 1 tick，200 ms 换成 20 tick），关闭时先切回非阻塞再卸驱动，U 盘模式下打不开）；unit-s31-usb-reverse 照旧通过。S31 构建、ELF 里的符号、大小变化和内部 RAM 见 s31 字段 | `ctest --test-dir build-s31-host -R unit-s31-usb`；`ports/esp32s31/build.sh` |
| 6 反向对照 | 通过【实测】，7 条都按要求失败 | 去掉改名前的 sha256 比较，F 失败：`wrong bytes are not installed: ready`。去掉每块的 CRC 检查，B 失败：`fetched and installed: failed`。续传总从 0 开始，E 失败：`AssertionError: [... 'GET', '0', '15ffa9ea…', '0', '2048']`。去掉“续传不对就从 0 重传”，E2 失败：`fetched and installed: failed`。任何回答都当作 SERVE，G 失败：`another bundle counts as no tool: ready`。去掉阅读的说明，A 失败：`snapshot names it: `（快照是空的）。装机工具不查 GET 的哈希，serve_test 失败：`FAIL: another file's hash` | `python3 tests/card_files/reverse.py build` |
| 7 界面规矩 | 部分通过 | ui_scale_check 通过（65 个文件，0 处违规，包括 card_files_notice.c）；说明页只用 `ink_ui_empty` 和主题令牌，不画框、不画圈、没有按钮；sdk-boundary 通过。“进度变化只刷那一行”【实现】：状态变化时只对那一个标签调 `lv_label_set_text`，进度按 10% 一档，文字不变就不调用。测试里**没有数刷新区域**，也没有用现有计数核对 | `python3 tools/ui_scale_check.py`；ctest sdk-boundary |
| 8 全量 | 通过，只有 1 项失败，是基线原有的 | 三段的汇总原话、ASan 结果见 full_ctest 字段。gcccheck：12 个主机文件编译后有 1 个报问题（unit.c 里 `%x` 配 uint32_t、截断告警），改掉后 0 处；ports/esp32s31/tests/usb.c 0 处。hal_usb_s31.c、hal_stubs.c 不在主机构建里，由 S31 构建检查（0 条 warning） | 见 full_ctest；`python3 $INK_DEPS/../ink-codex/tools/gcccheck.py build <files>` |
| 9 隐私和夹带 | 通过 | 隐私 grep 0 行。diff 里比任务书清单多了两个文件，都有原因：system/apps/gallery/gallery.c（挡路的修复）、tests/index_durable/run.py（基线适配）；另外 docs/system/capabilities.md 是任务书要求重新生成后提交的。装机包和生成的字体都没提交 | `git diff c4ad7dea..HEAD \| grep -nE '/Users/\|/private/tmp\|…'`；`git diff --stat` |

## 没做到的

- 验收 7 的“测试里数刷新区域”没有做，只读代码确认了只改一个标签（见上表）。
- 启动、文件服务和头文件这一类 30 行，超了预算 25 行：card_files.h 16 行，hal_usb.h 的 4 个接口带注释共 9 行。看过的更小做法：把 hal_usb.h 的注释并到行尾，能省约 3 行，但 HAL 契约会变得难读，所以没这么做。
- 吞吐、开机核对 3.9 MB 用的时间、内部 RAM 的最低剩余量都没实测，要上机。

## 和设计不一致的

1. `ink_card_source_t` 多了一个成员 `done(idx, how)`，用来发串口的 `DONE` 行。中枢实现可以让它什么都不做。
2. 换文件时，只有正式路径上**已经有文件**才去界面任务确认应用是 STOPPED；正式路径上没有文件时直接改名。原因：没有文件就不可能有人打开着它；如果照字面一律等应用 STOPPED，阅读在显示“缺字体”说明时就是 FOREGROUND，字体永远装不上。
3. 传输任务等改名完成用的是每 10 ms 看一次标记，没有用信号量。原因：模拟器里一个信号量要占约 104 B 内部堆，ui-scene-ImeVoice 的内部 RAM 最低剩余量正好压在预算线上（32664 < 32768），去掉信号量后回到 32768。
4. 状态结构 CF 和说明页的结构 N 放在 PSRAM（INK_PSRAM_BSS）。S31 的内部 .bss 只多了 48 B。
5. 多加了一个探针 `card_files.check`（每次核对加一）。H 要证明“弹出后核对了一次”，靠的就是它。
6. INK_FONT_PYTHON 的选择写在 `card_files.py python <候选…>` 里，CMake 调这个子命令。这样规则能单独测，规则本身和任务书一样。
7. 状态变成 ready 后，说明页先调一次 `ink_ui_redraw()`，等后台执行器做完一轮，再调一次。原因：嫁接的阅读关闭时在后台清理，清理没完之前 attach 会失败，所以第一次重画会是空页面。另外重画不带书的参数，阅读回来停在书架，不是原来那本书（H 的截图和快照是 `面板=书架`）。
8. card_files.h 没写 `extern "C"`，为了省行数，C++ 代码不 include 它。
9. build-asan 配置时多加了 `-DINK_GUI=OFF`，少编一个窗口程序，和内存检查无关。
10. 用 `--card-files-only` 时没有镜像，推不出装机包的默认位置，必须写 `--card-files DIR`。trial-guide 里写明了。
11. 应用名用的是现在的“新阅读”，应用 id 是 `mogo`（D-139 的改名还没合入）。说明快照是“新阅读：新阅读要用的字体还没放到存储卡上…（card.files=missing）”，应用名出现了两次。

## 改过的已有测试

- tests/sdk_boundary.cmake：`ALLOWED` 加了 `card_files`。框架多了一个头文件，规则本身没放宽，别的头文件照样不许用。
- tests/integration/ai_surface.c：在末尾追加了 `card.files_check`（给 AI 的那张表）和 `card.files`（STATES）。仍然是每个能力必须在且只在一张表里。
- tests/index_durable/run.py：这个测试编译旧基线的 file_service.c 来比较性能，旧文件里没有新函数，链接失败。现在旧源码里缺 `ink_fs_changed_outside` 时补上一行 `on_card(NULL)`，和它已有的 `fs_write_atomic_for` 适配是同一种做法。比较的内容和阈值都没变；在 CI 里它本来就因为没有历史而跳过。
- ports/esp32s31/tests/usb.c 和 usb_stub/：照任务书补了控制台四个函数的用例（第 5 条），原有断言都没改。usb_reverse.py 照旧通过。

## 顺手发现（含一处挡路的修复）

1. **修了（挡路）**：gallery.c 的 `photo_question_ready` 碰到 `ink_fs_revision()==0`（后台索引 `index_background` 正占着 FS.edits）时，会把用户的照片提问悄悄丢掉。读照片的路径（read_photos）本来就会重试，提问这条路没有。卡上文件的开机核对让索引的运行时刻挪了一点，ai-history-photo 因此失败：`simulated hub validates received gray attachment`。用调试输出确认过：6.738 s 时锁的持有者是 exec.bg 上的 index_background。修法：同一时间只让一个提问等待，等 100 ms 再取一次缩略图，最多 50 次，共 15 行。反向对照：不带这个修复时 ai-history-photo 失败（上面那句原话）；该测试自带的 19 个反向变体（含 photo-member、photo-changed、photo-id、photo-display、photo-switch）仍然全部按要求失败。
2. autotest-cap（camera 脚本回不到主页）在 c4ad7dea 上就失败，和本任务无关。
3. 说明页在最大一档字号下，第二行开头是一个逗号，是 LVGL 的断行规则造成的，只影响外观。
4. 新阅读的 `mogo.open` 在应用没打开时，回执是 QUEUED，不带“font not on the card”；要等页面打开后再问一次，回执才说原因。
5. 每次连上电脑都会把 3.9 MB 字体整份重算一遍哈希，并发 30 秒 HELLO；这 30 秒里设备不睡。开机核对要多久要上机看 check_ms。

## 要用户定

- `/content/fonts/InkSC.ttf` 和 `InkSC.OFL.txt` 定为系统保留的文件名，内容不对会被换掉；家人自己的字体放 `/me/fonts/`，它优先。这是照任务书做的，已写进 docs/system/card-files.md，需要用户知道。
- 文件明明在卡上却打不开时，说明写“现在打不开，正在重新检查”，同时触发一次核对。核对完仍打不开时，文字不再变化。这样措辞是否可以，由用户定。

## 新增的机制（都是任务书要求的）

- 清单格式和生成步骤（tools/card_files.json → bundle → manifest.json → 头文件；verify）：任务书第 1 节。
- 串口行协议第 1 版（INKCF1）：第 3 节。
- 临时任务 `cardfiles`（低优先级，6 KiB 栈在 PSRAM，做完就退出）：第 2 节。
- 状态 `card.files` 和 `card.files_progress`：第 2 节。
- 能力 `card.files_check`：第 6 节。
- `.part` 续传（沿用已有约定）：第 2 节。
- trial_run.py 的退出码 5 和 `--card-files-only`（另有 `--card-files`、`--no-card-files`、`--reset`）：第 5 节。
- CMake 缓存变量 `INK_FONT_PYTHON`：第 1 节。
- 模拟器参数 `--installer`：第 4 节。
- 探针 `card_files.check`：见“和设计不一致的”第 5 条。

## 要改的文档（由主会话改）

- docs/tasks/backlog.md:301“下次接设备时用 U 盘模式拷进去”作废：现在装机时自动放到卡上。
- D-138 的实施状态：已实现，等上机核对。
- docs/hardware 的试用记录：下次交付要带上 `card-files/` 目录（trial-guide 已改）。

## 上机先核对清单（主会话做）

1. 装了 USB Serial/JTAG 驱动后，日志照常输出；电脑不读时设备不卡（IDF 说写最多阻塞 50 ms 就丢）；卸驱动、切回后日志照常，没有崩溃。
2. 接收缓冲 4096 字节够不够：记下每块 CRC 重发的次数（card_files.chunk_retry、card_files.rejected）。重发多就加大缓冲或把块改小。
3. `trial_run.py --install` 在看到系统启动后重开串口，能收到重发的 HELLO。need>0 和 need=0 两种情况都要试。
4. 整份字体传完用多久，阅读能不能打开；再开机时是否只发 `HELLO … 0`。
5. 日志里的 `check_ms`（开机核对 3.9 MB 用的时间）。超过 3 秒就回来商量要不要加“已核对”记录。
6. 传输期间设备不睡；U 盘模式和传输互相让开：借卡时传输 1 秒内放手，`.part` 留下。
7. 打开串口期间内部 RAM 的最低剩余量。以下是【估算】、上机要核对：驱动约 5.3 KB；打开前要求内部 RAM 剩余 ≥ 收发缓冲 + 24 KiB，这个余量是我定的；`flockfile` 在设备上的实际效果，以及在 USB 断开、设备复位后 `usb_serial_jtag_is_connected()` 的表现。

## 文件

- 新文件：
  - system/services/card_files/card_files.c
  - system/include/ink/sys/card_files.h
  - system/ui/card_files_notice.c
  - simulator/environment/sim_installer.c
  - tools/card_files.json、tools/card_files.py
  - tools/device/card_files_serve.py
  - docs/system/card-files.md
  - docs/tasks/auto-provision-1/notice-{default,largest,failed,lent}.png
  - tests/card_files/：tool_test.py、unit.c、manifest/、serve_test.py、run.py、device.c、fake_host.py、scenario.script、reverse.py
- 结束时删掉了 build-asan、build-s31-own、build-s31-host、build-s31-host-asan，只留 build。

## 复核修复（两路审查后的修复轮，修复代理交回的原文）

## auto-provision-1 复核修复结果

四条都查过了，都是真问题，都已修好，一条也没驳回。修复在一个提交里：d9825cb6，Card files review fixes: DONE ok only when installed, a notice frees its app's files, the reader reopens the same book on the same screen, header lines within budget（在 e49ef401 之上）。工作目录 .claude/worktrees/wf_7b1bb08e-589-33，没有推送，没有碰设备。

全量 ctest 汇总行原话（先清掉 build/test-out，重新 configure 和构建，`ctest -j2` 分三段跑）：
- -I 1,200：`100% tests passed out of 200`
- -I 201,330：`100% tests passed out of 131`
- -I 331,460：`99% tests passed, 1 tests failed out of 124`（memo-save-busy 本来就是 Disabled）

唯一失败的是 autotest-cap。它在基线 c4ad7dea 和 e49ef401 上就失败。这个测试按时间点点屏幕，结果随运行时的先后顺序变：这次全量里挂在 memo 一段（`backspace erases the new draft text: h`）；之后用同样的代码直接跑两次，又挂在 camera 一段，和基线一样。把 e49ef401 的 6 个源文件换回来再跑，camera 一段照样失败。和本次修复无关，没改这个测试。

---

# auto-provision-1 报告（含复核修复）

现在缺字体的情况：新阅读打不开字体时，页面会写一段说明，不再是白屏。插上装机电脑后，设备自己发 HELLO，向装机工具要缺的文件，写进 `.part`，整份哈希核对无误才改成正式文件名。改完以后，阅读在同一个屏幕上重新打开原来那本书，然后拿掉说明。中间不重建页面，不出现空白页，也不整屏闪刷。不碰设备，上机由主会话做。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 工具和清单 | 通过【实测】 | 这些都会被拒：坏路径、路径重复、超过 5 MiB、超过 8 项；`header` 两次生成逐字节相同；`verify` 能发现 digest 不一致和条目不一致；INK_FONT_PYTHON 会挑一个装了 fontTools 的解释器。真实构建 verify 退出 0：digest 39628215e347e53d，共 2 项 | ctest card-files-tool |
| 2 设备端协议单元测试 | 通过【实测】 | 和原报告相同。坏路径当作不存在；HELLO、GET 的格式逐字核对；DATA 的各项检查；超长的行整行丢掉；CRC 和 zlib 的结果一致 | ctest card-files-unit |
| 3 模拟器多次开机 | 通过【实测】 | A～G 和原来一样。H 改严了：弹出后阅读自己回到**原来那本书**（`面板=正文`），而且还是同一个屏幕对象（`the same screen: no rebuilt page, no blank one in between`）。新增 I：卡上的字体是半截（只有前 64 KiB），阅读正显示“还没放到存储卡上”；这时插上电脑，旧文件在说明还显示着的时候被换掉，工具日志里有 `DONE 0 ok`、`BYE 1 0`，阅读在同一个屏幕上打开书。新增 J：卡上是另一个版本的字体（改了一个字节，能正常打开），阅读正开着它读书；插上电脑后传完但不换，工具日志里是 `DONE 0 io`、`BYE 0 1`，状态 failed，书照常显示；关掉阅读再核对一次，`.part` 在本机换上，状态 ready，文件和装机包逐字节相同，written=1。整个测试 4 秒（ASan 下 8.4 秒） | ctest card-files-sim |
| 4 装机工具对假串口 | 通过【实测】 | 和原报告相同 | ctest card-files-serve |
| 5 S31 | 通过【实测】 | 构建退出 0。"factory screen output layer @690a023" 出现 2 次，"没有屏幕输出层" 0 次，"warning:" 0 次；最后一步输出 `card files: header and manifest agree, digest 39628215e347e53d, 2 entries`。ELF 里有 ink_card_files_init、ink_card_files_notice、ink_card_files_notice_snapshot，没有 ink_card_files_start 了。镜像大小见“S31”一节 | `ports/esp32s31/build.sh` |
| 6 反向对照 | 通过【实测】，10 条都按要求失败 | 原有 7 条照旧失败（其中“去掉阅读的说明”一条因为代码改了，换成了 `else if (0 && …)` 的写法）。新加 3 条：fetch 不管有没有换上都回 ok，J 失败（工具日志里出现 ok）；说明显示时不算应用已经放开文件，I 失败（`replaced under the notice: failed`）；重开改回整页重画，H 失败（快照是空的，也就是空白页） | `python3 tests/card_files/reverse.py build` |
| 7 界面规矩 | 部分通过 | ui_scale_check：65 个文件，0 处违规。说明页仍然只用 `ink_ui_empty` 和主题令牌，不画框、不画圈。状态变化时只改那一个标签；就绪后不再整页重画（H、I 用“屏幕对象没变”来证明）。测试里没有数刷新区域 | `python3 tools/ui_scale_check.py`；ctest sdk-boundary |
| 8 全量 | 通过，只有 autotest-cap 失败，基线上就失败 | 汇总行原话见上。gcccheck：card_files.c、card_files_notice.c、mogo.c、main.c、tests/card_files/device.c、unit.c、third_party/mogo/main/apps/ink_book_page.c（它 include 了 ink_page_exports.h），7 个文件 0 处问题 | 见上 |
| 9 隐私和夹带 | 通过 | 修复提交的 diff 隐私 grep 0 行。改动的文件都在卡上文件这块：card_files、说明页、阅读接线、启动、测试、文档 | `git diff e49ef401..HEAD` |

## 复核修复

1. **只有真的换上了，才回 DONE ok（真问题，已修）**
   - 原因：原来 swap_bg 跳过（应用没放开、借卡失败、改名失败）后，fetch_file 照样返回 D_OK。复现：反向条目 done-ok-without-install 就是原来的写法，J 下工具日志里是 `DONE 0 ok` 和 `BYE 1 0`，测试失败。
   - 修法：改名真成功了才回 ok。卡被借走回 io，这次传输结束。整份已经对了、只是这次没换上，回 io、算 failed，接着做下一项。这第三种在协议里也写 `io`，协议第 1 版不变。没换上时 `.part` 留着，日志写一行 `card files: <path> not replaced yet, its .part stays`。

2. **说明还显示着时，内容不对的旧文件换不掉（真问题，已修）**
   - 原因：原规则只看应用是不是 STOPPED，而阅读显示说明时是 FOREGROUND。复现：反向条目 notice-holds-the-file 就是原规则，I 下状态停在 failed。
   - 修法：先在界面任务上判断应用放开了没有。STOPPED，或者它的页面正显示说明，都算放开。说明页的规矩写进了头文件：显示说明期间，应用不开着它要的任何文件。新阅读这边，打不开字体时调 `ttf_font_unload()`。原因是 mogo 的 ttf_font_open 失败时会退回上一份字体文件，可能正好是卡上那份，卸掉以后就一份也不开着了。
   - 上机注意【推断】：sdkconfig.defaults 没设 CONFIG_FATFS_FS_LOCK，按 IDF 默认值，删掉一个开着的文件不会被文件系统拒绝。所以“应用放开了才删”这条规矩在设备上是必须的。
   - 剩下一个很窄的窗口：说明显示期间，如果 AI 调 mogo.open 的后台任务排在换文件之前，而且这次正好把内容不对、但能用的旧字体打开了，就会删到开着的文件。这要求同一份文件刚刚打不开、马上又能打开，在测试里做不出来。

3. **就绪后重画两次，第一次是空白页（真问题，已修）**
   - 原因：反向条目 reopen-by-redraw（把重开换回 `ink_ui_redraw()`）复现了：H 的快照是空的，也就是空白页。根子在 attach 时先关掉旧会话，mogo 的清理在后台进行，`ink_mogo_opened` 碰到 busy 就拒绝。
   - 修法：`ink_card_files_notice(screen, app, what, reopen)` 多了一个参数。就绪时说明调一次 reopen()；只有 reopen 为 NULL 的页面才重画一次。新阅读的 reopen 就是在现有会话上再调一次 `ink_mogo_start`，所以回到原来那本书（M.path 还在），不建新屏幕，原来的“和设计不一致”第 7 条随之消失。页面画好以后，阅读自己删掉说明（`M.notice`，删掉时有回调把指针清空）。说明文字一直留到页面画好，没有白的一刻。
   - 防循环：调过 reopen 后把 from_ready 置真；重开仍然失败时，阅读重新放一张说明，按原来的规矩只再核对一次。原来的 barrier/again 两段删了。

4. **启动、文件服务和头文件超了 25 行预算（真问题，已修）**
   - 现在按新增的非空行算正好 25 行：card_files.h 12、hal_usb.h 9、main.c 2、file.h 1、file_service.c 1。hal_usb.h 的非空行是 9，复核者数的 10 多算了一个空行。
   - 怎么省下来的：
     - `ink_card_files_start` 并进了 `ink_card_files_init`：init 最后把第一次核对投到后台执行器。
     - `INK_CARD_FILES_MAX` 从头文件拿掉：数组改按行数本身 `INK_CARD_FILES_COUNT` 开，`_Static_assert(INK_CARD_FILES_COUNT <= 8, …)` 留在 card_files.c。数组大小就是行数，不会越界；这个检查同时保护 32 位的掩码和工具里的上限。
     - 去掉 `<stddef.h>`（lvgl.h 已经带了 size_t）。
     - 说“已显示说明”不另加接口：`ink_card_files_notice_snapshot` 多了 app 参数，用 `(app, NULL, 0)` 就能问那个应用的页面有没有显示说明（返回 0 表示没有）。
   - 曾试过把 init 挪到 `ink_fs_refresh_state()` 之后：ui-scene-ImeVoice 的内部 RAM 最低剩余量变成 32760，低于 32768 的预算，所以 init 留在界面启动前原来的位置。

**这次改过的测试**（都是有意改变行为的地方，没有放宽）：
- tests/card_files/device.c：H 原来只要求 `面板=`（停在书架也算），现在要求 `面板=正文`，而且是同一个屏幕对象；新增 I、J 两段。
- tests/card_files/run.py：加上 I、J 两段，并核对卡上文件、`.part`、工具日志里的 DONE/BYE 和 written 计数。
- tests/card_files/reverse.py：“去掉阅读的说明”一条换成适合新代码的写法；新加 3 条反向对照。
- tests/card_files/unit.c：加了 `ink_card_files_notice_snapshot` 的空实现，链接要用，断言没改。

**跑过的检查**：
- 受影响的测试：card-files-unit/tool/serve/sim、mogo、graft、sdk-boundary、integration-ai-surface、ai-history-photo、index-durable、ui-scene-ImeVoice、camera-gallery，共 26 个，`100% tests passed out of 26`。
- ASan（build-asan）：card-files-unit 和 card-files-sim `100% tests passed out of 2`。
- `tools/capabilities.py --check` 通过：`PASS: 653 definitions`。
- 结束时删掉了 build-asan 和 build-s31-own，留着 build。

## 产品代码行数

按新增的非空行算，测试、文档、生成文件都不算：
- 固件合计 649 行（预算 ≤680）：card_files.c 440（≤440）、card_files_notice.c 83（≤90）、S31 控制台 48（≤60）、模拟器 HAL 35（≤40）、阅读接线 18（mogo.c 12 + ink_page_exports.h 5 + runtime.h 1，≤25）、启动/文件服务/头文件 25（≤25）。
- 算上预算外、挡路的 gallery.c 15 行，共 664 行，仍然 ≤680。
- 模拟器环境 88 行（≤150），工具约 385 行（≤480），都没变。

## S31

ink-s31.bin 是 4,499,136 B：比修复前（4,498,912）大 224 B，比基线 c4ad7dea（4,484,704）大 14,432 B。Flash 段 4,369,794（修复前 4,369,582）；DIRAM .text 94,616、.bss 48,640，都和修复前一样。打开串口时多占的内部 RAM 约 5.3 KB【估算】，没变，要上机核对。

## 没做到的

- 验收 7 要求的“测试里数刷新区域”没有做。现在用“屏幕对象没变”证明没有重建页面，只改了一个标签是读代码确认的。
- 吞吐、check_ms、打开串口时内部 RAM 的最低剩余量都没实测，要上机。
- autotest-cap 在基线上就失败，没修。

## 和设计不一致的

1. `ink_card_source_t` 多了一个成员 `done(idx, how)`，用来发 DONE 行。
2. 换文件：正式路径上没有文件就直接改名；已有文件时，要它的应用是 STOPPED，**或者它的页面正显示说明**（复核修复 2），才删掉旧的再改名。整份对了但没换上时回 `DONE io`、算 failed，接着做下一项（复核修复 1）。
3. 传输任务等改名完成，用的是每 10 ms 看一次标记，没用信号量（省内部 RAM）。
4. CF 和 N 两个状态结构放在 PSRAM。
5. 多加了一个探针 `card_files.check`。
6. INK_FONT_PYTHON 由 `card_files.py python` 子命令来选。
7. 说明页的接口多了 reopen 参数，快照接口多了 app 参数（复核修复 3、4）。原来的“重画两次、回到书架”已经去掉，现在回到原来那本书。
8. 没有单独的 `ink_card_files_start` 了：init 在界面启动前调用，最后把第一次核对投到后台执行器，所以开机核对可能在 `ink_fs_refresh_state()` 之前开始。卡的状态在文件服务初始化时就已经设好，`ink_fs_card_hold` 能用。
9. card_files.h 没写 `extern "C"`。
10. build-asan 配置时多加了 `-DINK_GUI=OFF`。
11. `--card-files-only` 时必须写 `--card-files DIR`。
12. 应用名用的是“新阅读”，快照里应用名会出现两次。

## 改过的已有测试（整个任务）

- tests/sdk_boundary.cmake：允许列表加了 `card_files`。
- tests/integration/ai_surface.c：加了 `card.files_check` 和 `card.files`。
- tests/index_durable/run.py：给旧基线源码补上 `ink_fs_changed_outside`。
- ports/esp32s31/tests/usb.c 和 usb_stub/：补了串口控制台四个函数的用例，原有断言没改。
- 本轮新改的 4 个测试文件见“复核修复”一节。

## 顺手发现

1. gallery.c：照片提问碰到 revision 0 会被丢掉，已修（15 行，挡路，见原报告）。
2. autotest-cap 按时间点点屏幕，结果随运行时的先后顺序变：基线挂在 camera，本次全量挂在 memo，再跑又挂在 camera。建议另开任务把它改成等条件满足再往下走。
3. 说明页在最大字号下，第二行开头是一个逗号。
4. `mogo.open` 在应用没打开时，回执是 QUEUED，不说原因。
5. 每次连电脑都要把 3.9 MB 字体整份重算一遍哈希，并发 30 秒 HELLO。

## 要用户定

- `/content/fonts/InkSC.ttf` 和 `InkSC.OFL.txt` 是系统保留的文件名，内容不对会被换掉；家人自己的字体放 `/me/fonts/`。
- 文件在卡上却打不开时，说明写“现在打不开，正在重新检查”。这个措辞行不行，请用户定。

## 要改的文档（由主会话改）

- docs/tasks/backlog.md:301 那条“用 U 盘模式拷进去”作废。
- D-138 的状态改成：已实现，等上机核对。
- 试用交付要带上 `card-files/` 目录。

## 上机先核对清单

1. 装了驱动后日志照常输出，卸掉后也照常。
2. 接收缓冲 4096 够不够：看 chunk_retry 和 rejected 的次数。
3. `--install` 能收到重发的 HELLO，need>0 和 need=0 都要试。
4. 整份字体传完要多久；再开机时是否只发 `HELLO … 0`。
5. 看 check_ms。另外开机核对现在在界面启动前就投到后台，要看它会不会拖慢第一屏。
6. 传输期间设备不睡；借卡时 1 秒内放手。
7. 打开串口期间内部 RAM 的最低剩余量。
8. 新加：阅读显示“缺字体”时插电脑，确认字体被换上、回到原来那本书、整屏没有闪；阅读正开着旧版本字体时插电脑，确认这次不换（工具报 failed），关掉阅读后再核对能换上。

## 文件

- 本轮改动：
  - system/services/card_files/card_files.c
  - system/include/ink/sys/card_files.h
  - system/ui/card_files_notice.c
  - system/boot/main.c
  - apps/mogo/mogo.c
  - apps/mogo/port/ink_page_exports.h
  - tests/card_files/device.c、run.py、reverse.py、unit.c
  - tools/card_files.py（只改了注释）
  - docs/system/card-files.md（第 1、2、5 节）
- 整个任务新增的文件和原报告相同。
- 结束时删掉了 build-asan、build-s31-own，只留 build。
