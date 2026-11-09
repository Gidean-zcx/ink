# batch3 独立质量复查

日期：2026-11-09。范围：`9b3d2da675199713e19af7a297a18458bae26426..de4de5229a9335caffeea82b581608c7d5cffc44`；工作区 HEAD 与终点一致。

本轮只读产品代码和现有测试；仅在构建目录进行编译、测试和一次性实验，不修产品代码、不提交。上一段被中断，本次按用户给出的最多 35 分钟接续，证据随检查追加。B3-01 沿用主会话补记，没有重查或重跑其内存申请失败实验。【实测】指命令结果；【实现】指代码路径核查；未复现的猜测标为“待证实”。

结论：现有普通与 ASan/UBSan 全量均为 **242/242 通过**，但补查路径复现了 B3-02～B3-09，不能据现有测试全绿判定整批验收通过。本报告共保留 9 条有证据的问题，其中 B3-01 为接续前已知问题。

## 验收表

| 编号／项目 | 结论 | 证据与命令 |
| --- | --- | --- |
| A1 正确性与数据保留 | 未通过补查；已有持久化、恢复测试通过 | B3-02 拔卡、B3-05 拍照语义、B3-08 名称损坏；C1/C2 包含文件和照片重启、备份恢复、数据断电测试 |
| A2 确认规则、儿童限制、秘密字段 | 本轮未发现新的跨成员泄露、儿童越权或确认绕过；来源记录有缺陷 | 【实现】删除文件/成员、恢复覆盖仍要求确认，PIN 仍仅人可提交；C1/C2 中 `kid-1`、`files-app`、`camera-gallery`、`search`、`backup-pages` 的隔离/秘密字段断言通过；B3-09 丢失 AI 来源 |
| A3 能力一致性与页面快照 | 未通过补查 | B3-03～B3-07、B3-09；一次性脚本和截图见各条；已实看备份失败前后和图片打开后的屏幕 |
| A4 资源与固定上限 | 正常路径未见新增超预算；保留 B3-01 | C3 实测容量见下文，补查没有申请失败或预算违规；未重跑 B3-01，也未穷举所有峰值组合 |
| A5 合并后的相互影响 | 有未统一入口与重复实现 | B3-05、B3-06；另有 Records/AiLog 双份呈现，见架构审视；共用启动、导航和注册表已核查 |
| A6 Linux/gcc 可移植性 | 有限验证，不能认定 Linux CI 通过 | C4 用独立 GNU 14.3 交叉编译 20 个改动中的 C 文件，17 个通过、3 个因目标类型相关格式警告失败；本机 Clang 普通/ASan 构建通过，未运行 Ubuntu/gcc，详见“顺手发现” |

## 有证据的发现

### B3-01（高）：相机 PNG 编码在内存扩容失败时写坏内存（主会话据本轮实验补记）

- 位置：system/apps/camera/png.c 用 stb_image_write 编码；其中 `stbiw__sbgrowf` 扩容失败只做 `STBIW_ASSERT(p)`（发布构建里不起作用），随后照旧往原来的小缓冲区写。
- 复现【实测】：`build/audit-batch3/png_oom.c` 用会失败的 `osal_malloc/osal_realloc` 替身调用真实的 `ink_camera_png`。第 1 次申请失败时正常返回 NULL；第 4 次（`realloc` 10 字节）失败时进程段错误退出（-11），日志 `build/audit-batch3/png-oom-4.log`。
- 影响：真机上 PSRAM 紧张时拍照，可能写坏别的内存，而不只是拍照失败。
- 建议的最小修法：只改 png.c。编码期间记下每块申请的内存；扩容失败时（例如把 `STBIW_ASSERT` 定义成失败就 `longjmp` 回 `ink_camera_png`）释放这些内存并返回 NULL，相机页显示“没能完成”。配测试：替身在每一次申请上失败，都返回 NULL、不崩、不漏。

### B3-02（中）：拔掉存储卡后，内置应用和设置也无法搜索

- 位置：`system/ui/search.c:74`、`:91`、`:97`、`:106`。应用和设置先匹配成功，随后联系人/备忘库或文件索引任一读取失败，就把整次搜索改成 `failed`，并丢弃已匹配的结果。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch3/run_probe.py nocard`。有卡时搜索“电话”成功；通过模拟器输入拔卡后，“电话”和“音量”都得到 `status=failed`、无结果；重新插卡后相同查询成功。14 条断言中 12 通过、2 失败，测试完整结束，进程退出 0（此实验应看 `test.fail`，不能只看退出码）。证据：`build/audit-batch3/nocard.log`、`build/audit-batch3/nocard/search-no-card.json`。
- 影响：没有存储卡时，搜索入口连无需卡的电话应用和本地设置也用不了；页面只显示“暂时搜不了”，没有保留可用分类。
- 建议的最小修法：各来源读取失败时保留其他来源的结果，标明哪些分类暂不可用；拔卡和重新插卡各查一次内置应用、设置，断言仍返回匹配结果。

### B3-03（低）：从回执返回 AI 记录时丢失原分页位置

- 位置：`system/ui/shell/ai_pages.c:145`～`:147`、`:202`～`:204`。`AiLog` 与 `AiReceipts` 共用一份 `L.position`，每次重新创建页面都清零；返回原列表也重新创建，因而回到第一页。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch3/run_probe.py records-back`。用真实 `settings.set` 产生 18 条合成活动，打开 `AiLog` 并翻到第 2 页，打开回执，再以 `shell.back` 返回：`log-before.json` 是 `"page":2`，`log-after.json` 是 `"page":1`。35 条断言通过、保留第 2 页的断言失败，测试完整结束，进程退出 0。证据在 `build/audit-batch3/records-back/` 和同名 `.log`。
- 影响：逐条查看较早操作的结果时，每返回一次都要重新翻页。
- 建议的最小修法：为记录列表和回执分别保留现有 F4 位置，换成员或主动重新打开列表时再重置；无需新增存储格式或后台任务。

### B3-04（中）：同样的备份失败，AI 调用不会更新备份页面

- 位置：`system/ui/shell/backup_pages.c:54`～`:63`、`:141`～`:148`、`:50`～`:52`；底层立即失败分支在 `system/services/backup/backup_service.c:1471`。页面的失败处理只接在按钮自己的 `done=reply` 上；AI 直接调用 `backup.now` 没有这条回调，而轮询的 `backup.status` 仍是旧状态。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch3/run_probe.py backup-no-card`。打开 Backup 后拔卡，AI 调用 `backup.now` 得到 `failed`，等过两个轮询周期，页面仍为 Backup、显示旧备份状态；再实际点“现在备份一次”，才进入 BackupFail 并显示“请插入存储卡后备份”。16 条断言通过、AI 页面同步断言失败，测试完整结束。证据：`build/audit-batch3/backup-no-card/{before,ai-failed,touch-failed}.json` 及同名 `.log`；已查看同目录 `snap-ai-failed.png`、`snap-touch-failed.png`，屏幕确实分别显示“还没有备份”和“还没备份好／请插入存储卡后备份”。
- 影响：按钮和 AI 虽调用同一个服务能力，用户看到的结果不同，失败回执没有成为共享的页面状态，违背本任务的能力与页面一致性验收。
- 建议的最小修法：把操作结果接到现有的统一回执/备份状态通知入口，再由页面统一读取；覆盖即时拒绝/失败，不仅覆盖服务已经开始运行的状态转换。

### B3-05（中）：合并后有两条“拍照”能力，旧的 AI 入口仍报告成功却不保存

- 位置：`system/services/camera/camera_service.c:50`～`:75` 与 `system/apps/camera/camera.c:44`～`:68`、`:100`～`:102`。全局 `camera.capture` 的标题仍是“拍一张照片”，成功说“拍好了”，只返回临时 `photo:N`；新页快门用 `camera.photo` 才落卡并返回稳定 `file:N`，且限于 Camera 页面。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch3/run_probe.py camera-capabilities`。在主页由 AI 调用 `camera.capture`，得到成功及“拍好了”，媒体索引前后均为 0；再打开 Camera 调用 `camera.photo`，索引增加为 1。证据 `build/audit-batch3/camera-capabilities/legacy-result.json` 和同名 `.log`。该实验同时检查下一条问题，共 21 条断言通过、2 条失败，正常结束；没有注入申请失败。
- 影响：AI 的现有通用拍照入口和新的快门语义分叉；走旧入口的照片不在相册，重启或帧被覆盖后无法找回。开发报告曾把是否合并留给主会话，本次合并终点仍未统一。
- 建议的最小修法：让对用户开放的拍照能力统一复用落盘流程；内存取帧作为明确命名的内部/预览接口保留，避免两种保存语义都以“拍好了”结束。

### B3-06（中）：文件应用的图片“打开”没有接上同批合入的看图能力

- 位置：`system/apps/files/files.c:187`～`:192`、`:45`～`:47`；`system/apps/gallery/gallery.c:170`～`:183`。文件页的“打开”仍调用 `file.thumb`，回调只改提示；全尺寸 Photo 只能从 Gallery 当前列表进入。
- 复现【实测】：上述 `camera-capabilities` 实验用真实拍照保存 PNG，再经 `app.open(files)`、`files.select(id)` 选中它，实际触摸“打开”。前后都停在 FileActions，动作仍是 `file.thumb`，没有进入 Photo；“进入完整看图页”的断言失败。证据：`build/audit-batch3/camera-capabilities/file-image-{before,opened}.json`；已查看同目录 `snap-file-image-opened.png`，只显示小缩略图和“已完成”。
- 影响：用户从文件页打开已经可由相册查看的同一图片，只得到原有小缩略图；同批模块未接通。搜索文件结果也仍走独立 `SearchFile` 信息页（`system/ui/search.c:241`），是原分支“尚无文件打开能力”假设残留，未为这一旁证另计问题。
- 建议的最小修法：为已有 Photo 入口复用稳定文件编号和文件服务授权检查，供文件页调用；保留儿童隔离，不能仅移除 Gallery 的归属检查。

### B3-07（中）：AI 改成员称呼后，成员列表和快照仍显示旧称呼

- 位置：`system/ui/shell/settings_family.c:110`～`:116`、`:203`～`:216`；`system/policy/members.c:808`。列表只在创建时读成员，初始化只订阅 settings 变化，没有订阅改名/增删成员已经发布的 `member.changed`。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch3/run_probe.py family-live`。保持 SetMembers 打开，用 `member.rename` 将 kid 改成“合成新称呼”；`member.list` 已返回新名，等 1.5 秒后屏幕模型和快照仍是旧名。13 条断言通过、页面刷新断言失败，正常结束。证据 `build/audit-batch3/family-live/{before,after}.json` 和同名 `.log`。
- 影响：人和 AI 看到的成员身份标签与后端不一致，需离开页面再进入才更新；现有测试在操作后主动重开页面，掩盖了这个缺口。
- 建议的最小修法：在现有 UI 执行器上订阅 `member.changed`，更新当前成员列表；正在编辑的表单保留输入，目标已删除时使操作失效。

### B3-08（中）：改名接受合法长度的中文，却截出非法 UTF-8 并写入成员数据

- 位置：`system/policy/members.c:802`～`:812`；底层 `system/util/ink_args.c:104`～`:110`、`:180`。能力声明 `maxLength:31` 按 Unicode 字符计数，但接收数组 `name[32]` 只有 31 字节；忽略 getter 返回的原长度后直接保存，截断可能落在汉字中间。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch3/run_probe.py member-utf8`。把 kid 改为 11 字的“一二三四五六七八九十甲”，能力返回 done，但名字变成前十字加一个孤立字节 `e7`。随后 `member.list` 仍返回 done；Python 对 `build/audit-batch3/member-utf8/member-list.json` 执行 `read_bytes().decode('utf-8')` 在第 110 字节报 `UnicodeDecodeError`。8 条断言通过、完整保存名称的断言失败，正常结束。
- 影响：正常中文输入可以产生坏的成员名和无法按 UTF-8 解码的接口结果；数据写入已走正常成员持久化路径，本次未单独重启验证该损坏。
- 建议的最小修法：在改名入口核对实际字节长度，超限就明确拒绝，不静默截断；输入框与能力声明使用一致的容量约束，合法接受的名字必须完整保留。

### B3-09（中）：AI 经选择器设置壁纸时被记成触摸操作，AI 活动记录缺失

- 位置：`system/ui/shell/wallpaper.c:52`～`:56`。文件选择器完成回调不区分来源，固定用 `INK_SRC_TOUCH` 调用 `settings.set`；`system/policy/cap.c:264` 的活动记录只接受 AI 来源的实际写操作。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch3/run_probe.py wall-source`。写入合成 PNG，AI 依次调用 `shell.wallpaper`、`wallpaper.choose`、`file_picker.select`，确认 `display.wallpaper_file` 已变且图片加载成功；等两秒读 `activity.list` 仍是 `{"items":[]}`。再直接由 AI 调用 `settings.set` 移除壁纸，同样等待后立即读到 `settings.set` 的 done 记录，排除活动记录服务未启动的解释。17 条断言通过、选择器路径记录断言失败，正常结束。证据 `build/audit-batch3/wall-source/{activity,direct-activity}.json` 和同名 `.log`。
- 影响：同一个壁纸设置，AI 直接调 settings.set 会记录，经页面选择器却不记录；记录/回执页无法交代 AI 实际做过的修改，违背 R-061 的来源可追溯要求。
- 建议的最小修法：让最终选择提交保留真实调用来源，通过同一个设置能力完成写入；不要在可被 AI 调用的完成回调里固定伪装成人的触摸。

## 没做到的

- 未在真机、Linux/gcc 上运行；没有验证硬件断电、真实存储卡热拔插和实际相机。模拟器全量中的断电/重启测试通过，不代替真机结论。
- 时间范围内没有穷举全部成员切换时序、全部数据规模和所有峰值资源组合；不把“未复现新越权”写成安全性证明。
- `SetKid` 的应用/时段编辑、`AiMemory` 的实际记忆后端仍缺失，已在原开发报告披露，本次不重复计为新发现。

## 和设计不一致的

R-061/G-002 的同一能力、来源记录和可观察结果仍有 B3-04～B3-09 所述缺口；B3-03 丢失列表返回位置。详细证据和最小修法已逐条列出，未修改设计文档迁就实现。

架构审视【实现】：文件修改、确认、成员策略和备份流程仍归既有服务/软总线，未发现页面另起持久化服务。相机保存放在应用层，旧服务拍照能力继续对 AI 开放，造成 B3-05；文件/搜索/相册的打开入口未收敛，造成 B3-06。`settings_family.c:54` 的 Records 经 `activity.list` 读取，`ai_pages.c:87` 的 AiLog 则直接读 `activity.log`，有两份分页和状态文案转换；这是合并维护风险，本次没有仅凭重复代码另行评级。凭据入口复用 PinSet，不要求向 AI 开放秘密字段。

## 改过的已有测试

无。

## 新增的机制

无。

## 要用户定的

无。

## 要改的文档

本轮只写本报告。修复采纳后由主会话同步相应能力语义、任务验收状态与共享能力表，本轮未改共享文档。

## 顺手发现

- 【GNU 交叉实测；Linux 待证实】`camera.c:67`、`gallery.c:75`～`:76`、`ai_pages.c:104`～`:105` 把 `uint32_t` 直接交给 `%u`。Arm GNU 14.3 的该目标将其定义为 `unsigned long`，所以 `-Werror=format` 拒绝编译，日志在 `build/audit-batch3/system_*.gcc.log`。可用 `PRIu32` 或显式匹配的转换修正。但这依赖目标 ABI，不能把这些诊断当作 Ubuntu/gcc 已失败的证据，不纳入等级计数；本轮没有证实新的 Linux 专有构建阻塞。

## 验证命令与资源证据

- C1【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`，退出 0，**242/242、0 跳过，425.03 秒**；日志 `build/audit-batch3/ctest-full.log`。
- C2【实测】：按 `docs/tasks/codex-rules.md` 的 CMake 参数配置 `build-asan`，启用 `INK_SANITIZE=ON`、`INK_WERROR=ON`，额外指定 `INK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`；`taskpolicy -c utility cmake --build build-asan -j2` 后，运行与 C1 相同的 CTest 命令、目录换为 `build-asan`。构建和测试均退出 0，**242/242、0 跳过，517.11 秒**；日志 `build/audit-batch3/asan-build.log`、`asan-ctest.log`。未放宽预算、未排除失败测试。
- C3【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch3/resources.py`。一次性翻译单元直接包含原产品文件，只增加只读计数函数；6 条检查通过、0 失败，进程退出 0。`resources-out/counts.json`：能力 **315/512**，事件订阅 **24/32**，状态 **46/64**，状态订阅 **43/64**，设置 **17/48**，设置订阅 **6/64**，应用 **11/32**。事件订阅是其中余量最小的表，还余 8 项；本轮未撑满。
- 七种补查场景的 `metrics.json`：内部 RAM 总池最低余量均 **47,728 字节**，PSRAM 最低 **9,790,840 字节**（含截图留证的正常拍照场景），内部/外部申请失败计数均 0；没有预算违规。栈数字来自模拟器的估计，不当作硬件高水位。JSON token 上限仍为 **128**，备份页继续使用 summary、Records 每批 6 项，没有通过扩大解析器规避问题。
- C4【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch3/gcc_check.py`。沿用对应 CMake 目标的警告开关和包含路径，只移除 macOS SDK/部署参数；使用 GNU 14.3.1 的 arm-none-eabi 编译器，只生成审计目录中的对象、不链接运行。逐文件退出码在 `gcc-summary.json`，完整命令在 `gcc-commands.json`。PNG 第三方编译单元遵守现有 CMake 的 `-w`，没有把错误的目标选项当作产品问题。
- 编译器仅下载、解包到构建目录，未安装或修改系统工具链。来源为 [Arm 官方工具链](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)的 14.3.rel1 macOS arm64 分发包，查阅日期 2026-11-09；本次下载文件 SHA-256：`30f4d08b219190a37cded6aa796f4549504902c53cfc3c7e044a8490b6eba1f7`（本地计算，未核对独立发布签名）。

## 执行记录

- 开始：UTC 11:31（本地 19:31）。工作区初始仅有未跟踪的 `docs/audits/batch3/TASK.md`。
- 已读取任务书、执行规则、仓库协作约定和相关产品背景；本轮以 TASK.md 的只读、不提交要求为准。
- 接续开始：UTC 11:47；按用户的新限制，最多再用 35 分钟。B3-01 保留主会话补记，不重查、不再做内存申请失败实验。
- 接续修正了上一段一次性 `probe.c` 缺少 `ink/ui_shell.h` 的编译问题；只改构建目录实验文件，没有修改产品或已有测试。
- 结束核对：`git diff --check`、`git diff --stat` 均无产品/测试改动；`git status --short` 仍只有未跟踪的审计目录。对指定基线至 HEAD 的产品、测试、CMake 和 CI 差异扫描绝对路径及 MAC 模式，0 命中（`build/audit-batch3/privacy-scan.log`）。报告无本机绝对路径或真实凭据；未提交、未推送。
- 接续完成：UTC 12:18（本地 20:18），约 31 分钟，未超过本次 35 分钟上限。

SEVERE=0 HIGH=1 MEDIUM=7 LOW=1
