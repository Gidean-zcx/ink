# sound-clock 执行报告

基线：`62b0e83da1d34f50629b5e576807db1381b26078`。分支：`claude/codex-sound-clock`。日期：2026-11-10。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 设置、声音、振动 | 【实测】最终专项通过 | 人和 AI 更改四项设置；两个实体键、两次触摸各恰好两声；高优先级通知、来电、闹钟各一震；来电选柔音、闹钟选双音；进程重启后四项保留 | C1 |
| 2 倒计时 | 【实测】最终专项通过 | 触摸及 AI 开始、暂停、继续、取消、到点响铃和关闭；成人、孩子分别计时；分钟更新，2 秒内没有额外刷新；重启明确取消 | C1 |
| 3 声音页对照、计时截图 | 【实测】46.001/255，超过 12；基线也是 46.001 | 原有字号、灰度、分页造成首屏与画板不同；新开关和铃声在第二页。计时截图见下，**没有画板，待用户看** | C2 |
| 4 全量及内存检查 | 【实测】两套各 269 项通过、0 跳过，均仅 1 项复查豁免的文档同步失败；无 ASan/UBSan 错误 | 原短按及录音场景通过；能力表在临时目录生成、核对通过，详见复查节 | C3、C4、C7、C8 |

复现命令（依赖位置使用任务指定的环境变量，先按总规则配置并构建）：

```bash
# C1：真实页面、音频、振动、成员隔离、进程重启
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(sound-clock|clock-alarm|integration-races|integration-ai-surface)$'
# C2：与外部画板比较；外部画板未入库
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out work/ui-compare --scenes SetSound
# C3 / C4：完整检查，保留实际失败，不跳过断言；此前三个依赖跳过，本轮已补齐
# 加载现成锁定依赖和本仓库服务器包，以补齐这三个测试
export PYTHONDONTWRITEBYTECODE=1
export PYTHONPATH="$PWD/hub:$INK_DEPS/hub/.venv/lib/python3.11/site-packages"
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# C5：撤掉关键实现，要求测试确实失败
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/sound_clock_negative.py --build build
# C6：仅将 clock.c 换为基线对象，重链现有测试，核对 AI 页面版本回归
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/sound_clock_negative.py --build build --baseline-clock
# C7：在临时目录生成并核对能力表，遵守共享文档不改的边界
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/capabilities.md
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/capabilities.md --check
```

【实测】原七个反向对照均失败且跑到 `test.finished=1`：按键开关、振动开关、铃声选择、设置保存值读取、暂停保留时长、到点响铃、成员隔离。复查新增的反向对照见文末。只编译临时替换对象，不修改检出源码、不改断言。

【实现】设置沿用 `settings.set` 及原有保存文件；短音和试听沿用音频任务，铃声在同一任务生成。触摸在 LVGL 的输入设备确认点击处统一接入；实体键在输入服务统一接入。振动沿用 `hal_haptic_pulse`，未加 HAL、模型或线程。板级依据为 `boards/metalio_plus/metalio_board.h` 的 P03、50 ms，以及 `docs/research/simulator/hardware-parity.md` 第 9 项（查阅于 2026-11-10）。通知继续遵守原有优先级和勿扰规则，普通静默通知不强行提示。

【实现】倒计时使用单调时间、既有共享服务定时器和打断卡；运行期间持有已有的保持唤醒引用。各成员分别持有倒计时，按钮和 AI 使用同一组 `clock.timer*` 能力。页面用语义快照开放当前状态和动作。重启选择“明确取消”：省去新持久格式及墙钟调整语义，并在计时页明确写明。

【实现】声音在 `system/services/audio/audio_service.c` 程序生成，无下载素材、无第三方音源；音色基频为 880、660、330 Hz，按键为 1100 Hz，许可见 [SOUNDS-LICENSE.txt](SOUNDS-LICENSE.txt)。

【实测】产品代码增删合计 236 行（含 6 行构建接线），未超过 300 行预算；复查前为 196 行。为什么不能更小：现有时间服务是持久的墙钟闹钟，不能直接表达可暂停的单调倒计时；已检查复用其记录的方案，会混入闹钟列表并改变重启语义。采用应用内倒计时数据及现有定时节点，四个设置、两处输入接线、振动和选音均复用现有服务。

## 截图

声音首屏及实际可操作的第二页：

![声音首屏](SetSound.png)

![声音选项](SetSound-options.png)

计时页：**没有画板，待用户看**。

![设定时长](timer-idle.png)

![正在计时](timer-running.png)

![到点打断卡](timer-ring.png)

![重新启动后](timer-restart.png)

## 没做到的

- `capability-table-doc` 仍失败：新增六项能力、四项设置尚未汇入共享生成文档。`REVIEW.md` 明确豁免、留给主会话合并时生成；总规则第 7 节也禁止改该文件。C7 的临时生成及核对通过（428 项定义），没有跳过此测试或伪称共享文档已同步。
- 未接触设备，未做真机听感、振动或续航测试；未在 Linux 上实际运行。模型的更慢扫描边界见“顺手发现”。

## 和设计不一致的

- 声音页分数超标。原有 D-107 的 1.2 倍字号及分页、已选的深灰底、图标与行结构和旧画板不同；本次改动在第二页，标准工具截的首屏与基线分数完全相同。
- 计时只按分钟向上取整显示，最后一分钟不跳秒；人可选四档常用时长，AI 能通过 `seconds` 设置 1～86400 秒。
- 共用闹钟打断卡原来固定显示“按电源键再睡 10 分钟”；计时的电源键实际关闭这次计时。受“时钟只改应用及其测试”边界限制，未改共用卡片，该文案需合并主会话处理。

## 改过的已有测试

- `tests/integration/ai_surface.c`：给严格的 AI 能力清单新增六项计时能力，`timer_set` 必需参数为 `seconds`，`timer_dismiss` 为 `id`；保留全部原断言。未修改其余已有测试期望。
- 复查在 `tests/sound_clock.c` 增加两条断言：关闭提示音的当次触摸不响，开启的当次触摸响一声；保留原有次数、试听、成员隔离和重启断言。未修改任何已有测试期望、场景时长或预算。

## 要用户定的

- 查看无画板的计时页，确认常用时长及布局；本次保守采用“重启明确取消”。

## 顺手发现

- 原列表组件只接受 2～3 个固定动作。保持原有三动作，在时钟快照另列 `clock.timer`，避免扩展公共列表框架。
- 复查的额外种子检查中，种子 10 仍可丢失 80 ms 盖板键：模型慢扫描额外 58.131422 ms，加上 20 ms 检出和约 1.8 ms I2C 传输，本身已接近或超过按键时长。这不属于原验收场景；本次消除就绪后的调度饥饿，未修改模型的扫描估算或增加锁存。主会话仍需核实真实控制器的扫描和帧保持行为，不能据原场景通过承诺所有 80 ms 按键都不丢。

## 新增的机制

- `audio.key_tone`：按键提示音开关，任务书第 1 项要求；复用既有设置文件。
- `audio.vibration`：振动开关，任务书第 1 项要求；复用既有设置文件。
- `audio.call_ring`：来电铃声选项，任务书第 2 项要求；复用既有设置文件。
- `audio.alarm_ring`：闹钟铃声选项，任务书第 2 项要求；复用既有设置文件。
- 倒计时运行、暂停和取消状态及每成员定时节点：任务书第 3 项要求；复用现有共享定时器、UI 执行器、唤醒引用与打断卡。
- 无新增常驻任务、线程、文件格式、后台重试、缓存、队列或通用抽象层。

## 要改的文档

- 合并主会话生成 `docs/system/capabilities.md`，纳入六项 `clock.timer*` 能力及四项 `audio.*` 设置。
- `docs/system/ui-dependencies.md` 更新声音设置与计时的实现状态。
- 未改共享需求、想法、决策、任务总表，也未改已有调研、简报、审计资料。

## 最终检查

【实测】最终普通及 ASan/UBSan 构建均退出 0。两套全量各 269 项通过、0 跳过，唯一失败均为复查豁免的 `capability-table-doc`，原始全量命令仍退出 8。最终结果见 [validation.json](validation.json)，复查前后计量及十二项反向对照见 [review-evidence.json](review-evidence.json)；原功能计量见 [evidence.json](evidence.json)。此前三个依赖跳过项均在两轮全量中实际执行通过，未安装或升级软件。

【实测】报告内链接及截图文件存在，单文件均小于 5 MB；`git diff --check` 通过。产品增删合计 236 行；端口仅改现有任务的优先级，无波形算法、模型检出参数、存储服务或受保护共享文档的改动。按总规则扫描基线至提交及本轮待提交改动，隐私模式零命中。

## 复查后的修复

- 复查仅有 SC-H1（高），无严重、中、低项。
- 【实现＋实测】原场景没有进入时钟页、也没有运行倒计时；`timer_tick` 仅由开始计时及已运行的分钟截止点调度，空闲时没有新增周期唤醒。只撤掉六项计时能力注册或恢复基线 `clock.c`，原场景均恢复 8 次按键送达；单独撤掉输入提示音仍为 6 次。新增能力注册会改变场景时序，不能把它解释成空闲倒计时刷新，也不能把提示音认作这次漏读的直接原因。
- 【实现＋实测】决定性的延迟发生在触摸芯片就绪后：输入任务优先级原为 12，两条显示送行任务为 24，可占住双核接近 10 ms；本次失效的监测扫描额外耗时为 52.518442 ms，连同 20 ms 检出，再等 9.204092 ms 才读芯片，已超过 80 ms 按键。仅修改两个现有任务的优先级：输入为 24，送行为 23。修改集中在模拟器端口的任务配置；未改 D-110 的波形、扫描耗时、预填、队列或模型检出参数，未加线程。只优化时钟页无法修复未进入该页时的输入饥饿，因此扩到这两处配置。
- 【实现＋实测】实体键先过滤、投递事件，再请求短音；触摸确认回调仅作记号，在本条 UI 消息处理后请求短音。音频 API 仍只更新既有待播请求并唤醒音频任务，调用方不做 I2C、输出初始化或等待播放。短音不得覆盖已经请求的铃声试听；沿用同一个待播槽，没有新增音频队列。
- 【实现＋实测】首轮普通全量暴露 `scenario-ai_tools-metalio-plus` 的 90 ms 麦克风丢失：录音开始后，禁止 Flash 写入的既有引用直到音频任务下一轮才取得。把取得、失败回退及音频任务后续释放集中到同一个持锁函数，开始硬件采集前先取得引用。原场景及媒体、竞争测试通过；只撤掉采集开始处的取得动作，原场景再次因麦克风丢失失败。未改存储服务或增加避让时长。
- 【实测】新增 `sound-clock-input` 原样运行既有脚本、模拟器档案、中枢脚本和预算，另查 8 次按键送达、松键前读到芯片，以及本例芯片就绪至首次读取不超过 1 ms。它能检出“仅抬高输入优先级、仍和送行同级”的不完整修复。恢复输入优先级后送达退回 6 次；恢复送行优先级虽有 8 次，但就绪后仍等 5.681239 ms，新增断言失败。恢复旧触摸提示音顺序、去掉试听保护，也分别触发次数断言和原有试听断言失败，均跑到 `test.finished=1`。
- 【实测】原来跳过的原因已查明：构建用 Python 缺少 `pytest`、`pytest_asyncio`、`aiohttp`、`yaml`。指定依赖目录已有锁定版本 `pytest 8.4.2`、`pytest-asyncio 1.2.0`、`aiohttp 3.12.15`、`PyYAML 6.0.3`；通过上方 C3/C4 的 `PYTHONPATH` 加载，不安装、不升级依赖。
- 【实测】最终十二项反向对照全部检出（原七项，加触摸反馈顺序、试听保留、输入优先级、送行优先级、录音写盘引用五项），反向命令整体退出 0。
- `capability-table-doc` 按复查明确要求不改共享生成文档，在临时目录生成并核对成功（428 项定义）；保留全量原始结果，单独列明这一例外。复查未新增任务、状态机、持久格式、配置项或抽象层。

复查最终计量（普通和 ASan/UBSan 一致，时间为模拟时间）：

| 检查 | 修复前 | 修复后 |
| --- | --- | --- |
| 原样 80 ms 短按场景 | 送达 6 次，done=5、stale=0 | 送达 8 次，done=4、stale=1 |
| 首次触摸读取 / 松键 | 19.581722534 s / 19.580000000 s | 19.564570975 s 开始，19.566407975 s 读完；松键时刻不变 |
| 芯片就绪后至首次读取 | 9.204092 ms（慢扫描额外 52.518442 ms） | 0.081239 ms（慢扫描额外 44.489736 ms）；新增回归断言不超过 1 ms |
| 原录音场景 | 丢 90 ms，6 次 DMA 覆盖 | 丢 0 ms，0 次覆盖 |
| 普通全量 | 初轮有短按、共享文档失败及三项依赖跳过；复查首轮另暴露录音失败 | 269 过、1 项文档豁免失败、0 跳过，268.89 s，退出 8 |
| ASan/UBSan 全量 | 初轮短按、共享文档失败 | 269 过、1 项文档豁免失败、0 跳过，458.21 s，退出 8；无内存或未定义行为报错 |

```bash
# C8：新增输入回归及原样短按、录音场景；两套全量 C3/C4 均已包含
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(sound-clock|sound-clock-input|scenario-ai_page_version-metalio-plus|scenario-ai_tools-metalio-plus)$'
# C9：本轮五项修复的反向对照；C5 则含原七项，共十二项
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/sound_clock_negative.py --build build --review-only
# 内存全量使用相同依赖环境，并遇到未定义行为立即失败
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

## 第二轮复查后的修复

- 本轮仅处理 SC-H2，保留 SC-H1 修复。已执行 `git fetch origin`，合入 `origin/claude/system-layer`（`168eba7f`），自动合并无冲突。
- 【实测】原样复现 `hub-simulator-e2e` 的 `tool_calls=0`。不是输入任务不停扫描、误报活动或反复唤屏：第 1～2 秒间输入任务只用 0.309 ms CPU，共享 I2C 的扩展器等待最大 0.0583 ms。主要延迟在确认写盘及前后的调度，成员切换只用 3.387 ms。
- 【实现＋实测】模拟器启动任务固定在核心 0；原来可跨核心的高优先级输入改变了交接顺序，确认回执唤醒启动任务时，UI 已占住核心 0 渲染确认卡。回执从产品初始化结束到被夹具收到，多等了 192.473 ms；确认写入因此排到刷屏后，又等 2605.147 ms。3 秒第一次点按落在引导页，继续产生刷新，最后 6028.077 ms 才切好成员。原优先级 12/24 的临时对象对照为 2399.504 ms；仅升输入到 24、送行仍为 24 则为 6048.077 ms，和复查一致。
- 【实现＋实测】只将现有输入任务固定在核心 0，使输入与启动控制的交接留在同一核心，另一核心可并行渲染 UI。输入优先级仍为 24，送行仍为 23。确认回执等待降到 0.746 ms，确认落盘等待降到 0.133 ms，原样端到端通过。没有保留试过但不足以修复的存储调度修改：前台写入提前申请闪存保护仍要 5679.251 ms，只提高存储任务优先级仍要 6027.950 ms。

| SC-H2 检查 | 修复前 | 修复后 | 命令 |
| --- | --- | --- | --- |
| 产品初始化完成 | 2090.021 ms | 2090.285 ms | C10 |
| 收到待确认回执（此时不代表已落盘） | 2282.494 ms | 2091.031 ms | C10 |
| 确认完成 | 6024.690 ms | 2398.129 ms | C10 |
| 成员切换完成 | 6028.077 ms | 2399.530 ms，早于原定 3 秒点按 | C10 |
| 原样短按及新增输入回归 | 两项已过 | 两项仍过；送达 8 次，19.564564 s 开始读取，松键仍为 19.580000 s；芯片就绪后等待 0.0741 ms | C10 |
| 声音计时、原录音场景、存储排序 | 已有测试 | 三项通过，未改脚本、时间、预算或已有断言 | C10 |
| 撤掉本轮修复的反向对照 | 恢复跨核心输入 | 原样端到端退出 1，确认退回 6028.077 ms；对照脚本退出 0，检出成功 | C11 |
| 普通全量 | 曾有端到端失败 | 283 过、0 跳过；仅 1 项能力表文档豁免失败，205.46 s，原始退出 8 | C12 |
| ASan/UBSan 全量 | 需合入主线后复验 | 283 过、0 跳过；同一项文档豁免失败，366.68 s，原始退出 8；无内存或未定义行为报错 | C13 |

- 改过的已有测试：`tests/hub/e2e_device.c` 仅分段计量原有操作，超时和条件保持；`tests/hub/e2e.py` 新增成员就绪必须早于 3000 ms 的断言。`tests/sound_clock_negative.py` 增加只撤掉本轮亲和性的对照入口。未重做 SC-H1，未修改声音页、计时器、音频或存储服务。
- 新增的机制：无。只改一个现有任务的核心配置；不新增任务、状态、持久格式、队列、配置项或抽象层。该配置属于平台内部调度，不新增用户能力或改变按钮和 AI 的调用路径（R-061）。
- 【实测】普通和内存构建均启用 `INK_WERROR=ON`、退出 0；内存版启用 `-fsanitize=address,undefined`，未定义行为遇错即停。两套全量和反向对照已完成，原始全量命令均因豁免文档项退出 8，没有将其写成退出 0。计量汇总见 [second-review-evidence.json](second-review-evidence.json)。
- 【实测】本轮产品代码增删 3 行；相对合入主线 `168eba7f`，本任务累计 239 行（含 6 行 CMake 接线），低于 300 行。没有新增没做到的功能、设计差异或待用户定事项；本轮仅在 macOS 模拟器验证，未做 Linux 或实机测试。
- 原失败及跳过项：唯一遗留失败是主会话明确豁免的 `capability-table-doc`，本轮也未修改共享生成文档；C14 的临时生成及核对为 434 项定义，均退出 0。原来缺的四项 Python 依赖，本轮仍从指定只读依赖目录加载：`pytest 8.4.2`、`pytest-asyncio 1.2.0`、`aiohttp 3.12.15`、`PyYAML 6.0.3`，两套全量三项原跳过测试均已实际执行通过。未安装、升级依赖；没有新增缺失依赖。
- 最终检查：【实测】`git diff --check`、报告链接与文件尺寸检查通过。本轮相对合并提交的隐私扫描零命中；相对原任务基线共命中 4 行主线合入的合成耳机 MAC，均为规则允许的固定假数据，无本机路径及真实标识。本轮只提交修复、测试、报告和计量；用户提供的未跟踪 `REVIEW.md` 保留原样，未纳入提交。共享文档仍由主会话统一生成。

```bash
# C10：原样场景及相关回归；依赖环境沿用 C3/C4
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(hub-simulator-e2e|scenario-ai_page_version-metalio-plus|sound-clock-input|storage-order|sound-clock|scenario-ai_tools-metalio-plus)$'
# C11：只撤掉本轮核心配置，运行原样端到端测试
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/sound_clock_negative.py --build build --second-review-only
# C12 / C13：完整检查；普通及内存构建均沿用总规则的字体和 LVGL 参数
export PYTHONDONTWRITEBYTECODE=1
export PYTHONPATH="$PWD/hub:$INK_DEPS/hub/.venv/lib/python3.11/site-packages"
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# C14：在工作目录生成并核对，不写共享文档
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/sc-h2-capabilities.md
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/sc-h2-capabilities.md --check
```
