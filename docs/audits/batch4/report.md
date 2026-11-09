# batch4 独立质量复查

日期：2026-11-09。范围：`9eac583f5f2d7c6c89092b2e10ddebc00dcb6085..533b07298c12614186a2efafdd714ea2be36d2a2`，检出版本为范围终点。

本轮只读产品代码和既有测试；仅在构建目录做验证，更新本报告。不改产品代码，不提交、不推送、不碰设备。时间上限一小时；【实测】指本次命令结果，【实现】指代码路径证据，【估算】明确说明假设。

结论：确认 **2 条高、4 条中**等级问题，未发现有证据的严重问题。普通与 ASan／UBSan 全量各 **257/257 通过**，但补充实验暴露了现有测试未覆盖的到期控制、索引生命周期、调用来源和断线编辑问题，不能据全绿认定整批验收成立。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 正确性与持久化 | 不通过 | B4-02／03 分类不更新，B4-04 卡身份遭覆盖，B4-06 未提交文字丢失；既有成员切换、重启、数据断电及传输拔卡场景两版均过 | V1、V2；各问题的单场景命令 |
| 2 确认、儿童权限与秘密 | 不通过 | B4-01 已开始的媒体越过用时限额；原有家长 PIN、清理确认、儿童网页／传输拒绝、密码与一次性数字快照遮蔽专项均过，未发现本批新增的秘密快照泄漏 | V1、V2；`probe.py --case media-limit` |
| 3 能力一致性与快照 | 部分成立 | 页面动作基本复用能力；B4-05 经通知的 AI 动作漏记。现有快照、能力暴露及目录同步测试均过；人工抽看三页截图 | V1、V2；`extra_probe.py --case notify-source` |
| 4 资源与固定上限 | 未撑满；栈余量待核实 | 内部 RAM 最低 43,520 B，固定表仍有余量；模拟器估计 esp_timer 栈只余 32 B，不能据此保证实机栈安全 | V3 与下方资源表 |
| 5 合并后配合 | 不通过 | B4-01 是用时策略与播放的交叉缺口；B4-02 依赖测试私自扫全成员；B4-03、B4-05、B4-06 是生命周期／上下文接线缺口。六项专项及 audit3-fix 回归均执行 | V1、V2；定点实验 |
| 6 Linux/GCC 可移植性 | 验证不足 | Apple clang 两版 Werror 构建通过；未找到可用 GNU GCC，未运行 Ubuntu CI。增量代码的 POSIX 返回值、单行控制语句及格式化写入做了静态核查，未证实新的 GCC 专有阻塞 | V1、V2；V4 |

## 已证实的问题

### B4-01（高）：孩子用时到期后音频继续播放，连人的暂停操作也被拒绝

- 位置：`system/policy/members.c:862`、`:875`；`system/ui/shell/kid_pages.c:58`；交叉路径 `system/services/audio/media_service.c:231`。到期仅更新 `member.time_up`、显示遮罩并拦截后续能力，没有暂停已运行的媒体；允许清单也没有 `media.pause` / `media.stop`。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch4/probe.py --case media-limit`。先在成人空间录制 165 秒合成音频，给孩子设每天 1 分钟，孩子开始播放后推进 75 秒虚拟时间。`KidTimeUp` 已出现，用时为 80 秒，媒体仍为 `INK_MEDIA_PLAYING`，播放位置已到 74,700 ms；随后以 `INK_SRC_TOUCH` 调 `media.pause` 得到 `rejected`。场景最终扬声器累计播放 165,060 ms，说明音频持续播到结束。24 条检查通过、2 条失败、场景完整结束。证据：`build/audit-batch4/probe-media.log`、`build/audit-batch4/media-limit/metrics.json`。一次性实验未修改产品代码。
- 影响：验收要求的“时间到以后只能用电话和短信”不成立；既有播放可以持续越过限额，用户也不能直接暂停。这是 `kid-2` 与媒体／正在进行卡片合并后的缺口。
- 建议的最小修法：在现有到期状态处理处暂停当前孩子的非通信媒体，保留停止／暂停这种收尾操作的权限；电话、短信仍按原规则放行。补查播放中到期、到期后暂停及加时后的恢复。

### B4-02（中）：存储页一直“正在整理”，完成全机索引的流程只有测试在调用

- 位置：`system/services/file/file_service.c:1421`、`:1523`、`:1632`；`system/ui/shell/settings_pages.c:379`；测试对照 `tests/storage_usage/storage_usage.c:55`、`:225`。
- 原因【实现】：分类就绪要求 `/family` 和每一个成员根目录都扫描过，标志仅在 `index_scan` 设置；存储页只调用 `file.usage` 和清理预览。启动流程没有负责完成这些扫描的调用，文件页／相册／搜索只按使用的目录索引。测试自己定义 `index_device()` 扫全成员，替产品补上了缺失步骤。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch4/probe.py --case storage-cold`。启动后创建 8 字节文件，打开真实存储页并推进 30 秒，仍返回 `{"status":"organizing"}`；显式扫描家庭和本人目录后仍未就绪；再扫描尚未使用的孩子目录，立即能读到正确分类。17 条通过、2 条失败、正常退出 0。证据：`build/audit-batch4/probe-storage-cold.log`、`build/audit-batch4/storage-cold/metrics.json`。
- 影响：普通使用路径无法完成任务承诺的分类显示，重启后内存就绪标志也会重新丢失；空的其他成员目录同样会阻塞本人的统计。
- 建议的最小修法：给现有索引流程接上明确的全机完成入口，在现有后台执行器上按需触发并发送完成事件；个人与全机就绪状态应分别如实呈现。不让测试私下预热成为功能成立的前提。

### B4-03（中）：索引就绪后新增文件或同卡重插，分类仍返回过期数值并标为 ready

- 位置：`system/services/file/file_service.c:1421`、`:1441`、`:1633`、`:1639`、`:1652`。就绪标志不随文件修改失效；统计只遍历已经记录的索引条目。换回同一张卡时 UUID 相同，缓存也不失效；拔卡失败路径只更新 `file.index_state`，不清统计缓存。
- 复现 A【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch4/probe.py --case storage-new`。全根索引后本人书籍为 8 字节，用正式 `ink_fs_write_atomic` 新增 17 字节文本，`file.usage` 仍为 ready／8；显式再扫 `/me` 后才变为 25。15 条通过、1 条失败。
- 复现 B【实测】：同命令换 `--case storage-card`。缓存 8 字节统计后真实模拟拔卡，确认查询失败；在宿主卡目录把同一文件改成 32 字节，再插回原卡，查询仍为 ready／8。显式再扫 `/me` 后变为 32。15 条通过、1 条失败。
- 证据：`build/audit-batch4/probe-storage-{new,card}.log` 及对应 `storage-{new,card}/metrics.json`；两场景均正常退出 0，应检查 `test.fail`，不能只看退出码。
- 影响：传入新文件、其他应用保存文件、卡交给电脑修改后，存储页与 AI 可得到已过期的占用值；这不是短暂“正在整理”，而是无限期宣称就绪。原开发报告已披露未验证同卡离线修改，本轮补查证实失败。
- 建议的最小修法：在现有文件变更／卡重新可用的集中路径失效相关索引完成标记和统计缓存，再由 B4-02 的现有后台流程刷新；新条目尚未纳入时返回整理中，不能把旧总量标为 ready。

### B4-04（高）：卡身份文件读打开失败，会覆盖原身份并使已有文件 ID 失效

- 位置：`system/services/file/file_service.c:1429`–`:1440`。本批把先读大小改为直接打开，却把所有 `hal_fs_open(..., HAL_O_READ)` 失败都解释成“身份文件不存在”；后续写临时文件并重命名覆盖原文件。
- 复现【实测，单次故障注入】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch4/extra_probe.py --case marker-open`。在构建目录编译文件服务包装，仅让**已存在**的 `/content/.file-index-card` 的一次只读打开返回 NULL，其余 HAL 调用原样执行。此前创建并索引合成文件，保存文件 ID 和 16 字节卡身份；随后调用只读 `file.usage`。结果原身份被替换，`file.lookup` 原 ID 返回 failed。9 条通过、2 条失败，场景结束且进程退出 0；证据为 `build/audit-batch4/probe-marker-open.log`、`marker-open/metrics.json`。
- 影响：【实现＋实测】瞬时打开错误会变成持久身份变化，原索引记录因卡身份不符失效；这会破坏引用旧文件 ID 的功能。实验没有删除文件内容，也没有声称实机出现了该故障。相对基线，身份文件大小读取成功后再打开失败原本会直接返回错误。
- 建议的最小修法：打开失败时区分“确认不存在”和“暂时不可读”；仅前者创建新身份，其余返回 IO 并保留原文件。失败支路再做存在性核验即可，不必给正常路径恢复额外遍历或新增格式。

### B4-05（中）：AI 打开通知所触发的修改丢失来源，动作记录为空

- 位置：`system/ui/shell/navigation.c:1900`–`:1908`；交叉路径 `system/policy/cap.c:271`–`:273`。新增 `shell.notify_open` 执行通知动作时将来源固定成 `INK_SRC_APP`，动作日志仅记录 AI、REMOTE 和 CONFIRMED 的写操作。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch4/extra_probe.py --case notify-source`。发布一条带 `settings.set` 的合成本地通知，AI 经 `shell.notify_open` 打开；亮度确实变成 70，但随后 `activity.list` 是 `{"items":[]}`。对照：AI 直接调用同一 `settings.set`，动作正常入账。10 条通过、1 条失败；进程退出 0。证据：`build/audit-batch4/probe-notify-source.log`、`notify-source/metrics.json`。
- 影响：同一原子能力经通知入口执行时不再可追溯，违反 AI 动作记录与统一调用来源的约定。本轮没有据此推断已绕过高风险确认。
- 建议的最小修法：在通知动作这一集中转发点保留原调用来源，并核对确认链所需上下文；加一个 AI 经通知触发修改的记录对照。

### B4-06（中）：网页输入尚未提交时短暂断线，重连会丢掉本地文字

- 位置：`system/apps/remote/remote.c:428`–`:432`、`:704`–`:708`、`:731`–`:735`，恢复入口 `:205`。离线页直接删除含输入框的 chrome；恢复后转到 Remote，再次进入输入页从空字符串创建输入框，没有保留同一会话未提交的文字。
- 复现【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch4/extra_probe.py --case remote-draft`。沿用现有网页专项的真实系统键盘和合成中枢，输入“合成文字”但不提交；断线 6 秒，连接在五分钟内恢复。中枢提交计数仍为 0，但页面已是 Remote、输入框不存在；再次触发同一输入控件，文字长度为 0。49 条通过、2 条失败，场景完成、进程退出 0。证据：`build/audit-batch4/probe-remote-draft.log`、`remote-draft/metrics.json`。
- 影响：“打完才发”的本地编辑与自动重连接起来后会静默丢失用户输入；正常断线即可触发，不需要断电或人为卡住线程。
- 建议的最小修法：在现有会话生命周期内保留正在编辑的输入框／普通文本草稿，短暂离线后恢复到原编辑状态；会话真正结束时清理，秘密字段继续遵循原有隐藏和清理规则。无需引入持久草稿格式。

## 待证实

【估算，待证实】模拟器栈估计已接近上限：`esp_timer` 为 3,552/3,584 B，`main` 为 3,952/4,096 B，`exec.media` 为 7,552/8,192 B。证据分别为本轮 `build/kid-2-kngij07l/no-pin/metrics.json`、`build/kid-2-compare/SetKid/metrics.json`、`build/storage-compare/SetStorage/metrics.json`，汇总见 `build/audit-batch4/resource-final.json`。这些是宿主编译与模拟器换算的估计，未出现栈越界实证，不计入缺陷等级；需目标编译器调用栈／实机高水位确认余量，不应直接调高预算来宣称安全。

## 没做到的

- 未运行 Linux/GCC 或设备，未验证实机断电、真实卡热插拔、物理 USB、真实短信／电话、真实 Chromium 网页。中枢测试使用本检出的代码及既有合成替身。
- 儿童计时的正常重启保留由全量中的既有专项覆盖；没有新增“检查点之间突然断电”的实验。原 `kid-2/REPORT.md` 已披露每分钟检查点及最多一分钟未落盘用时的限制，仍保留此边界。
- 本轮优先审计指定增量及高风险组合，不是整仓穷举。已人工抽查 KidTimeUp、SetStorage、RemoteType 与画板，未重算所有页面的 G4 像素分数。
- 所有一次性实验及日志仅留在被忽略的构建目录，未加入正式测试、未修复上述问题。

## 和设计不一致的

- 用时到期只保留通信的行为不成立（B4-01）；分类值能完成并及时反映真实占用不成立（B4-02／03）；AI 行为完整记录在通知转发处缺失（B4-05）。
- 【本轮截图抽查】SetStorage 缺画板的总容量和分段占用条；RemoteType 复用通用输入框，没有画板的网页表单标题；KidTimeUp 增加短信入口、字号和灰度不同，短信入口符合本次任务要求。截图在 `build/{kid-2,storage,remote}-compare/<页面>/snap-scene.png`，参考为 `$INK_DESIGN/png/<页面>.png`。数值、合成内容与时钟差异不作为视觉缺陷。
- 【实现，原报告已披露】`system/ui/shell/transfer_pages.c:120` 仍显示“占用的应用暂不能查看”，没有实现任务要求的 USB Busy 具体占用者；真实浏览器节点也未完成。本轮将这些已知未完成项保留，不重复计入新缺陷。

## 改过的已有测试

无。本轮不修改产品源码和已有测试。

【增量核查】原任务对既有断言的调整有对应行为依据：应用数 15→16（新增 remote）、Control→KidControl、备份失败重开保留 BackupFail、儿童无线传入从成功改为拒绝、AI 能力清单补新项；未看到资源预算或超时被放宽。`storage-usage` 由自动索引改为测试先索引，虽保留了分类断言，却遗漏了产品谁负责启动索引的问题，已列 B4-02。截图超出设备 128 token 的首页改由宿主 cJSON 校验这一变化已在原报告披露，不把它当作设备解析器扩大。

## 要用户定的

无新增产品决定。

## 顺手发现

范围内还包含 `audit3-fix` 的合并改动；纳入交叉核查，避免把已修问题重复报告。

【架构审视】ActivityCard 复用媒体、电话和 job 状态；今日屏配置放在按成员集合；网页输入走实时会话；存储统计和清理归文件服务；传输页面调用现有 transfer／USB 能力。这些划分总体符合“软总线＋原子能力”。本轮缺口集中在已有能力之间的状态传播和调用上下文，优先在集中入口修复，无需另起服务。能力目录、AI 暴露测试已在合并终点通过，原任务报告中的共享目录失败不再成立。

## 新增的机制

无。本轮仅审计。

## 要改的文档

除本报告外未修改文档。后续修复应更新相关任务报告中“通过”的范围，尤其是存储索引流程和儿童时间到期后的既有媒体；不要修改需求来迁就当前行为。

## 验证记录

- `git rev-parse HEAD`：为指定终点；起始 `git status --short` 仅有未跟踪的 `docs/audits/batch4/`。
- **V1【实测】** 普通版 Werror 配置、构建退出 0；全量 **257/257 通过**，零跳过，退出 0，用时 232.45 秒。日志：`build/audit-batch4/{configure,build,ctest}.log`。
- **V2【实测】** ASan／UBSan 版 Werror 配置、构建退出 0；全量 **257/257 通过**，零跳过，退出 0，用时 558.20 秒。日志：`build-asan/audit-batch4/{configure,build,ctest}.log`；核对编译参数包含 `-fsanitize=address,undefined`，完整日志未见 sanitizer 错误。
- 两版构建及全量串行执行，各自最多 `-j2`，使用 `taskpolicy -c utility`；复用给定 `$INK_DEPS` 字体、Python、LVGL、SDL2 和 ImGui，未升级依赖。配置保留 `INK_WERROR=ON`，内存版另设 `INK_SANITIZE=ON`；中枢解释器为 `$INK_DEPS/hub/.venv/bin/python`。

```sh
# 按 codex-rules.md 配置 build / build-asan 后；以下两轮依次执行。
export PYTHONPATH="$PWD/hub"
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

- **V3【实测】** `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch4/extra_probe.py --case resources`：仅在构建目录直接包含原策略源码，加只读计数函数；产品正常启动后计数，4 条检查通过、0 失败、退出 0。未扩大任何被测表，结果见 `probe-resources.log`。

| 固定资源 | 本轮启动计数 / 上限 | 余量 |
| --- | ---: | ---: |
| 能力 | 348 / 512 | 164 |
| 事件订阅 | 31 / 64 | 33 |
| 状态 | 48 / 64 | 16 |
| 状态订阅 | 51 / 64 | 13 |
| 设置 | 17 / 48 | 31 |
| 设置订阅 | 6 / 64 | 58 |
| 应用 | 12 / 32 | 20 |

上限依据：`system/policy/cap.c:26`、`:859`，`system/policy/watch.h:16`，`system/include/ink/sys/{state,settings}.h`，`system/ui/app_registry.c:27`。状态订阅占 79.7%，是当前较紧的表；计数是正常启动时占用，不宣称运行期全场景峰值。批内把事件表 32→64，其他上述上限未在本批扩大；没有新增产品常驻线程，主要复用现有执行器、服务调度和 UI 定时器。

【实测＋估算边界】汇总本轮相关专项与补查的 33 份指标，内部 RAM 总池最低 **43,520 B**（既有门槛 32,768 B），外部 RAM 最低 **11,536,392 B**，内部／外部申请失败均 **0**。BG、MEDIA 栈配置各 8,192 B、位于外部 RAM；估计接近上限的线程已单列“待证实”。JSON token 上限仍为 **128**（`system/include/ink/sys/json.h:17`）；本轮未扩大解析器，相关新页快照解析／缓冲专项通过，不代表任意输入长度均可处理。

- **V4【实现＋环境核查】** 检查 `.github/workflows/sim.yml` 的 Ubuntu Werror 配置，以及本批新增 C 行的 `read`／`write`／`fgets` 返回值、控制语句和 `snprintf` 调用。新增产品行未出现上述 POSIX 调用；格式警告仍须真实 GCC 复验。当前 `gcc` 为 Apple clang，给定依赖目录未找到 GNU GCC；不把 clang 结果或 batch3 的旧 GNU 交叉结果当作本次 Linux 通过证据。
- 定点实验均正常结束，但模拟器退出码不代表断言通过；以上缺陷以 `test.fail`、失败文本和控制组为据。B4-06 的一次性实验首次因删掉后半测试后遗留未用变量而未编译，修正的仅是构建目录实验，随后成功复现；正式源文件和测试均未改。
- **V5【实测，收尾】** `git diff --stat` 为空，跟踪文件无改动；工作区仅保留开工时已有的未跟踪 `TASK.md` 和本轮新增的 `report.md`。`git diff --check` 无问题，报告另做空白与引用文件／行号检查通过。按总规则扫描指定基线至 HEAD 的全部差异与本报告，隐私表达式均匹配 **0**；统计行与六条问题等级一致。证据：`build/audit-batch4/final-checks.json`。本轮在一小时时限内结束，未提交、未推送。

SEVERE=0 HIGH=2 MEDIUM=4 LOW=0
