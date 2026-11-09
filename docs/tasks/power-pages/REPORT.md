# power-pages 执行报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。分支：`claude/codex-power-pages`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 15% 通知、5% 打断、触摸/AI 开关不改用户设置、实际 60/120 秒锁屏、充电锁屏/唤醒/拔线、2% 保存及关机回执；五场景 108 条断言 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build --output-on-failure -R '^power-pages$' -j2` |
| 2 | 【实测】通过 | 四页都有快照；低电量列真实能力，充电显示电量和唤醒方式；关机区分保存成功与超时 | 同上；`python tests/power_pages_negative.py --build build` |
| 3 | 【实测】除用户豁免的生成文档同步检查外通过；【实现】代码在预算内 | 常规及 ASan/UBSan 均 224/225，无跳过；唯一失败为 `capability-table-doc`。相对任务基线产品新增 206、删除 3，保守按增删合计 209/250 行 | `ctest --test-dir build --output-on-failure -j2`；`ctest --test-dir build-asan --output-on-failure -j2`；`git diff <基线> --numstat` |
| G1 | 【实测】除用户豁免项外通过 | 常规及 ASan/UBSan 均 224/225，未改预算或跳过测试，内存诊断 0 条 | 见复现命令 |
| G2 | 【实测】通过 | 按钮和 AI 共用 `power.set_saving`、`interrupt.ack`；四页快照经过实际模拟器读取，新能力及状态进入 AI 运行期覆盖测试 | `power-pages`；`integration-ai-surface` |
| G3 | 按总规则第 7 节记入“要改的文档” | 共享文档留主会话统一改 | 见下文 |
| G4 | 【实测】功能及规格检查通过；视觉目标未达到 | 四张画板均先看；无动画；使用真实状态/书签；对照分数见下文 | `python tools/ui_scale_check.py system/ui/shell/power_pages.c`；`python tools/ui_compare.py --build build --out build/ui-compare-power-r1 --scenes BatteryLow BatterySaving Charging PowerOffEmpty --reuse` |
| G5 | 【实现】通过 | 原公共界面文件仍仅作末尾接线；复查为保留原设置，在现有电源服务小增量加入运行期开关并登记接口；没有修改并行页面文件 | `git diff --stat`；见“复查后的修复” |

## 没做到的

- 本次在 macOS/clang 实测；未运行 Linux/gcc 或实机。
- 四页视觉分数未达到 12/255，差异如下；未改规格追分。
- 未覆盖提醒显示期间切换家人的交互；页面没有订阅家人变化，开关显示可能保留创建时的状态，能力调用仍检查当前家人权限，AI 快照仍受现有成员代次保护。按任务范围未增加监听。

## 和设计不一致的

| 页面 | 1/4 灰度平均差 | 主要差异 |
| --- | ---: | --- |
| BatteryLow | 26.484/255 | 按规范用 192 状态底圆、#888888 灰、较大字号/按钮；增加明确写出作用的省电开关和原时长说明。画板底圆更小且更浅。 |
| BatterySaving | 12.413/255 | 按规范用较大的字号与纯黑次要文字；三点是静态图形。 |
| Charging | 18.897/255 | 用现有主题构造更大的圆环和数字；公共状态栏没有充电闪电/锁图标；保留真实电量。 |
| PowerOffEmpty | 18.235/255 | 按复查意见直接把公开书签正文中的真实书名/页码作为主标题，换行后说明跟在下方；没有解析或写死画板第 148 页，画面明确写已关机。 |

四页统一使用现有纸白色及主题，与未同步规范的画板背景、字体和灰度有差异。`Charging` 按画板在锁屏显示；使用中接电不抢走前台页面。`BatterySaving` 按画板表示“2% 时保存后关机”，不是省电开关页。

## 改过的已有测试

- `tests/power_pages.c`：把“开关写入 60/600 秒”的旧期望改为“开关不改用户设置、实际按 60/120 秒锁屏”；用户补充明确要求这种行为。保留阈值、充电、保存结果与真实关机检查，另加书签标题、换行不重叠和无书签回退。
- `tests/power_pages.py` 增加无书签场景；脚本终点由 180 秒改为 300 秒，容纳真实的 60 秒省电及 120 秒原时长检查，没有加等待掩盖时序。`tests/integration/ai_surface.c` 明确列入新能力及必填布尔参数、只读状态，原检查和预算不变。
- `tests/power_pages_negative.py` 增加复查基线重链接选项；保留原来撤掉整组页面接线的反向测试。

## 要用户定的

无新增待定项。用户已明确省电开关不改原锁屏设置：本轮只在现有锁屏计时器上施加运行期 60 秒上限，关闭后按用户当前设置执行。没有扩展亮度、网络等省电策略；创建页面时仍只对已验证的大人显示开关，能力调用继续检查当前家人权限。

## 顺手发现

暂无。

## 新增的机制

无新任务、业务状态机、文件格式、持久配置项、队列或后台重试。复查修复增加用户要求的运行期开关，注册 `power.set_saving` 能力和 `power.saving` 状态，继续使用原自动锁屏计时器及其唤醒信号，没有独立省电服务或策略框架。四种界面仍复用现有 UI 调度、电源采样、打断和关机流程。

## 要改的文档

- 主会话更新 `docs/system/ui-dependencies.md` 第 4.2 节电源行及第 7 节关机图行：补四页实际实现、模拟器验证与视觉差异，去掉对应的“占位外观”。
- 主会话重新生成 `docs/system/capabilities.md`，补 `power.set_saving {enabled: boolean}` 和只读状态 `power.saving`；没有新增设置。能力表文档同步检查按用户要求留给合并主会话，本分支不改生成文档。任务清单由主会话统一更新。
- `docs/system/system-services-design.md` §1.3 手工维护的接口表已补这一能力和状态，仅登记用户本轮已明确要求的行为，没有修改设计约束或确认策略。

## 过程与验证记录（首次交付）

- 【实测】开工工作区干净，HEAD 等于指定基线；没有上次遗留报告或任务提交。
- 【实测】常规 `INK_WERROR=ON` 构建成功；尺寸检查 0 违规。测试没有改动任何已有期望。
- 【实测】正向四场景断言分别为 25、17、15、15；内部 RAM 最低余量均为 47,840 字节，原预算 32,768 未改。
- 【实测】反向对照用基线 `off_picture.c` 的原初始化接线重新链接同一测试，撤掉本任务整组界面；四场景退出码均为 1，分别有 4、3、4、1 条断言失败。脚本保留在 `tests/power_pages_negative.py`，不修改工作区产品代码。
- 【实现】为何超过 100 行：四页绘制、状态订阅/打断转接、AI 快照和等关机画面确实提交，合计仍在 250 行内。看过直接复用原通用打断卡/关机卡的方案：它们没有充电圆环、保存中状态及定制关机图；改公共绘制函数还会进入并行任务区域。本次只增加末尾转接，普通关机、来电和闹钟继续走原呈现函数。
- 【实测】验收曾发现同一电量因 idle→active 被重复绘制，丢掉首次触摸；集中跳过未变化的页面后通过。充电页浮层放在锁屏标记之上，重新锁屏通过；没有加等待掩盖。
- 【实测】第一次全量测试有 1 项失败：关闭空间后的关机快照漏掉“没电了”；已保留隐私过滤并补齐原因，原 `integration-power-menu` 和 `power-pages` 均通过（退出码 0），没有修改旧断言。自定义动作的低电量卡片仍由通用呈现函数处理。
- 【实测】默认字体工具 Python 缺中枢测试依赖，第一次全量有 3 项自动跳过；改用依赖目录已有的 `hub/.venv/bin/python` 后，常规全量 225/225 通过（退出码 0，215.91 秒），没有安装或升级依赖。测试时设 `PYTHONPATH="$PWD/hub/src" PYTHONDONTWRITEBYTECODE=1`，确保读本检出的中枢源码且不写只读依赖目录。
- 【实测】ASan/UBSan 全量 225/225 通过（退出码 0，378.07 秒），四页断言仍为 25、17、15、15，内部 RAM 最低余量仍为 47,840 字节。两个构建都开 `INK_WERROR=ON`，仅内存检查构建加 `INK_SANITIZE=ON`。
- 【实测】最终差异仅 9 个任务相关文件；空白检查通过，按总规则第 6 节检查绝对路径及 MAC 字样，0 命中。未提交设计稿、构建产物或证据目录。

## 复现命令

首次配置用总规则第 5 节的字体/解释器/LVGL 参数，并增加 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；`build-asan` 同样配置再加 `-DINK_SANITIZE=ON`。构建与测试均用 utility 优先级：

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -R '^power-pages$' -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/power_pages_negative.py --build build
```

已保留的对照证据在忽略目录 `build/ui-compare-power/`，含四张实际屏幕、拼接图、`metrics.json` 和分数；上表的 `ui_compare.py --reuse` 可直接重算。

## 复查后的修复

本轮从 `7ad41562` 接续；PP-R1-01 按用户补充改为运行期缩短现有锁屏计时，保留用户设置；PP-R1-02 直接把已有书签正文放到主标题。下面补记本轮验证，历史记录中的 60/600 秒切换已被本轮替代。

| 编号 | 结论 | 证据与命令 |
| --- | --- | --- |
| PP-R1-01（中） | 【实测】已修 | 120 秒开关前、中、后保持不变；省电中修改为 180 秒后仍保留新值；原设 60 秒不被延长；实际省电 60 秒、关闭后 120 秒锁屏。`power-pages`、`integration-ai-surface` 及下方复查反向对照 |
| PP-R1-02（低） | 【实测】已修 | 主标题直接匹配真实书签正文及标题字体，保存成功/超时均检查不重叠，无书签显示“已关机”。`power-pages` 及下方复查反向对照 |

- 【实测】先补测试、再修产品。修复前：`BatteryLow` 退出 1、14 条断言失败，包含开启时覆盖 120 秒及关闭后丢失 120 秒；`PowerOffEmpty`、`BatterySaving` 各退出 1，唯一失败为主标题不是书名和页码；无书签回退原本通过。证据在忽略目录 `build/test-out/power-pages-r1-before/`。
- 【实现】电源服务只增加用户要求的运行期开关、一个能力 `power.set_saving` 和只读状态 `power.saving`；开启时给现有自动锁屏时长加 60 秒上限，关闭后使用 `power.lock_after_s`，不写该设置、不加持久配置或独立策略。按钮和 AI 共用能力；原权限关口不变。页面显示原设置，AI 快照含开关和原时长。服务小接口是这两项表注册，没有新增 C API。
- 【实现】更小方案的取舍：只缓存原值并继续调用 `settings.set`，开启期间仍会覆盖用户设置，也可能盖掉期间的新设置；改通用设置框架会扩大范围。本轮在现有电源服务的计时取值处统一限时，界面只改能力绑定、快照和说明，避免新增策略框架。开关为运行期状态，不新增持久字段。
- 【实现】PP-R1-02 的数据修复只替换两行，直接用 `card.text` 做主标题，不解析书名、不改阅读服务，保存结果仍作说明。截图核查发现正文换成大标题后会换行，另用现有 LVGL 对齐把说明放在标题底下；新增已保存、保存超时、无书签及不重叠检查，没有新增排版机制。
- 【实测】撤回复查修复的界面：`python tests/power_pages_negative.py --build build --review-baseline 7ad41562` 退出 0，子场景 `BatteryLow`、`BatterySaving`、`PowerOffEmpty` 均如期退出 1，分别失败 5/2/2 条断言；脚本核对必须命中覆盖用户设置、主标题错误的断言。保留的整组接线反向对照也退出 0，四个子场景如期退出 1（6/5/4/3 条失败）。
- 【实测】正向五场景为 42/19/15/17/15 条断言，均无失败，内部 RAM 最低余量仍为 47,840 字节；尺寸检查 0 违规。四页新对照分数分别为 26.484、12.413、18.897、18.235/255，仍未达 12/255。低电量页多了原时长说明，关机页主标题改为真实书签并让说明跟随换行，其余差异同前表。已人工检查低电量及关机截图；证据在 `build/ui-compare-power-r1/`。
- 【实测】首次全量 223/225 通过，无跳过；失败是允许留给主会话的能力表文档同步，以及新能力尚未写入 `integration-ai-surface` 的显式覆盖表。已补该测试的能力、参数和状态断言，定向两项复测通过。
- 【实测】补入运行期 AI 覆盖表后，`services-mid-capability-doc` 检出服务设计 §1.3 漏记同一能力和状态；仅补一行接口清单，定向检查通过。生成能力总表仍未改，`capability-table-doc` 按用户要求留给主会话。
- 【实测】最大字号探针沿用本轮测试，仅在启动后通过 `settings.set` 设 `display.font_size=3` 再重链接，不改工作区；低电量、正常保存关机、保存超时及无书签四场景均退出 0。人工检查最大字号的低电量和书签关机截图，文字没有重叠；探针源码及帧在忽略目录 `build/test-out/power-pages-r1-max-font/`。
- 【实测】常规最终全量 224/225 通过、无跳过（128.92 秒，CTest 退出 8）；唯一失败为用户明确豁免的 `capability-table-doc`，不是运行期或权限测试。完整日志在 `build/power-pages-r1-normal-ctest.log`。
- 【实测】历史三项中枢测试跳过的原因已核实：字体工具 Python 缺 `pytest`、`pytest-asyncio`、`aiohttp`、`PyYAML`，进而也没有供交叉检查使用的消息夹具。本轮 `INK_HUB_PYTHON` 使用依赖检出的 `hub/.venv/bin/python`，四项依赖齐全，`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 均实际运行通过，无需安装或升级依赖。
- 【实测】ASan/UBSan 最终全量 224/225 通过、无跳过（255.55 秒，CTest 退出 8）；唯一失败同为豁免的 `capability-table-doc`。实际编译含 `-fsanitize=address,undefined`，两套构建均开 `INK_WERROR=ON`；完整测试日志中内存/未定义行为诊断 0 条，五场景断言数和 RAM 余量与常规一致。日志在 `build-asan/power-pages-r1-{build,ctest}.log`。
- 【实测】最终只修改 9 个本轮相关文件；相对任务基线产品新增 206 行、删除 3 行，按增删合计 209 行，小于 250 行预算。空白检查通过；最终差异按总规则检查本机绝对路径和 MAC 字样，0 命中，未提交构建产物或设计稿。

本轮复查反向命令（其子进程应失败，脚本本身应退出 0）：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/power_pages_negative.py --build build --review-baseline 7ad41562
```
