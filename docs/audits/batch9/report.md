# batch9 独立质量复查

日期：2026-11-10。范围：`a6f4140d..af8ab90afe47551181d6172ca8cfcd07ec129406`。
本轮按 `TASK.md` 只读产品代码与既有测试，不修改产品、不提交；实验和构建产物只放忽略的构建目录。环境沿用用户提供的 `INK_DEPS`、`INK_DESIGN`。开始时 HEAD 为 `af8ab90a`，本目录仅有未跟踪的任务书。

结论：发现 3 个有复现证据的中等问题，本轮未新增严重或高等级发现。普通与 ASan/UBSan 全量均为 288/288 通过，但未覆盖下述 UI 边界，不能据此判定整批无问题。

证据标记：【实测】本轮执行结果；【实现】代码核查；【估算】说明假设的推算；未证实的问题不计入等级。

## 已证实的问题

### B9-01【中】AI 重新打开蓝牙后，页面持续显示“蓝牙已关闭”

- 位置：`system/ui/shell/bluetooth_page.c:132–139`、`:141–143`。`changed()` 只在关闭时更新 `B.status`，开启时仅重画；AI 调用不经过页面的触摸回执 `reply(kind=2)`，因此旧提示不会被替换。
- 【实测】独立驱动通过真实能力依次打开页面、`settings.set(bt.enabled=0)`、`settings.set(bt.enabled=1)`，每步等待后台与屏幕完成。开启后快照同时出现 `value:"开"` 和 `status:"蓝牙已关闭"`；随后 AI 配对并成功连接，仍同时出现“已连接”和“蓝牙已关闭”。没有暂停线程或修改产品实现。
- 复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit9/compile_driver.py build/audit9/bt_boundary.c`；再运行 `taskpolicy -c utility build/audit9/bt_boundary --profile simulator/profiles/metalio-plus.json --script tests/bluetooth.script --assets build/assets --content build/test-content --out build/audit9/bt-boundary --end 60s --quiet --no-random-outages --budget tests/ui_scene.budget.json`。退出 1，失败断言为 `enabled page must not still say Bluetooth is off`；证据 `build/audit9/bt-boundary.log`、`build/audit9/bt-boundary/snap-on.png`、`snap-ai-connected.png`。
- 影响：合法 AI 操作已完成，用户和 AI 读到的页内状态却仍称蓝牙关闭；只测按钮路径不能检出。
- 最小修法：在现有设置变化回调中同时处理开、关，开启时清除或替换旧关闭提示，不依赖触摸调用的专属回执；补 AI 开关后的快照断言。

### B9-02【中】候选按钮取消伸缩后，长候选将“更多”挤出屏幕，快照仍称其可见

- 位置：本批改动 `system/ui/shell/keyboard.c:429–431`；受影响的快照 `:132–140` 只检查 `HIDDEN` 标志，不检查实际裁切。
- 【实测】用正式词库，经公共输入服务输入 `zhrmghg`（“中华人民共和国”的简拼）。屏幕宽 684 px；“更多”按钮位于 x=724～824，标签 x=743～804，`lv_obj_is_visible=0`；第六候选“只好”也被右边界裁切。AI 上下文仍列出全部六个候选及 `更多候选=1`。本轮实际截图确认屏幕上没有“更多”。
- 【实测，归因对照】仅在构建目录编译恢复原伸缩属性的对象，保持正式词库与其余产品相同，`zhrmghg` 的“更多”回到 x=431～531，完全可见；对照未改仓库源文件。当前实现使用支持的大字号 `display.font_size=3` 时，连 `nh` 的“更多”也部分出界（x=628～728）。证据 `build/audit9/ime-layout-before.log`、`ime-layout-big.log`；对照构建命令为 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit9/keyboard_baseline.py`，运行参数同下，二进制改为 `ime_layout_before`。
- 复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit9/compile_driver.py build/audit9/ime_layout.c`；`taskpolicy -c utility build/audit9/ime_layout --profile simulator/profiles/metalio-plus.json --assets build/assets --content build/test-content --out build/audit9/ime-layout --end 60s --quiet --no-random-outages`。观察实验退出 0，证据为 `build/audit9/ime-layout.log` 的 `CASE zhrmghg` 和 `build/audit9/ime-layout/snap-zhrmghg.png`；不是内存错误或模拟器崩溃。
- 影响：触摸候选入口超出当前屏幕，页面快照与实际可见内容不符（G-002）；既有短词画板及直接调用 `ink_text_pick` 的质量测试不能证明候选栏完整可操作。未据此声称全部拼音输入不可用，也未把可能的横向滚动当作已验证的补救。
- 最小修法：在现有候选栏为“更多”和话筒保留固定可见位置，约束折叠候选的总宽度；超出项通过已有展开入口访问，快照按实际展示范围输出。补长词/简拼及大字号检查；单纯恢复旧均分样式会重新裁切长候选，不是完整修复。

### B9-03【中】正式词库未与实际字体取交集，常用拼音候选中包含无法显示的字

- 位置：`tools/imedict/build_base.py:49–70`、`:88`；相关字体契约 `CMakeLists.txt:279–286`，UI 字体选择 `system/ui/theme.c:145–172`。
- 【实现】新生成器接受整个 U+4E00～U+9FFF 范围并加入读音，但设备常规字库只含 `ascii,punct,pinyin,gb2312`；UI 的备用字库仍是同一 `sans-44.inkf`，不能补上缺字。构建没有检查正式候选的字形覆盖。
- 【实测】直接读取本轮生成的 IKD1 词条及 INKF 字形索引，24,138 个不同文字项里有 **508 项**包含缺字。`ren` 前六为“人、任、紝、认、忍、岃”，其中“紝、岃”缺字；`cuo` 第二候选“剉”、第五候选“虘”缺字。这不只是低频候选翻到最后才碰到的情况。
- 【实测，显示链路】B9-02 的同一驱动输入 `ren`，实际截图显示“人、任、□、认、忍、□”；`ink_font_has` 对“紝、岃、剉”均为 false，`ink_font_glyph` 均返回替代字符 U+25A1。正常字“人、错”仍返回原码点，与 `system/text/ink_font.c:87–92` 的缺字回退一致。
- 复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit9/font_coverage.py`，退出 0（数据检查），证据 `build/audit9/font-coverage.json`；页面复现命令同 B9-02，证据 `build/audit9/ime-layout.log` 和 `build/audit9/ime-layout/snap-ren.png`，已目视核查。
- 影响：候选数据存在且可被选择，屏幕却无法让人识别这些候选；当前 90 条测试仅核对候选字符串，没有证明同一资源包能将它们显示出来。
- 最小修法：生成时复用现有字体字符集校验，先过滤不可显示的词条并重新裁剪、计量；若要保留这些字，应同步补字形并复核字体资源预算。增加候选与随包字体覆盖的一致性检查，无需修改拼音引擎。

## 验收表

| 编号／项目 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 正确性、持久化与成员隔离 | 既有回归通过；新增边界有 B9-01～03 | `bluetooth-settings` 含关闭状态重启；`integration-text-*`、成员、拔卡及存储回归均在全量中通过。正式词库的质量和加载数据复现一致；未把小词库故障测试说成正式词库的全部故障验证 | 下文两套全量命令及 B9-01～03 复现命令 |
| 2 D-105、孩子权限与秘密字段 | 本批未见规则放松；旧 batch7 问题仍需另行修复 | 【实现】蓝牙沿用能力门禁、设备级配对集合及孩子可用规则；输入法未改密码输入、学习边界。电话只新增未连耳机时的导航；不代表旧 DTMF 风险已修复 | `git diff a6f4140d af8ab90a -- system`；两套全量的确认、孩子、成员及密码场景 |
| 3 R-061、G-002、模块边界 | 能力复用成立；UI 同步和可见范围有缺口 | 【实现＋实测】页面调用既有 `settings.*`、`bt.*`，只新增导航能力；生成词库在电脑端，运行时仍用原引擎，索引计量不进入产品。但存在 B9-01、B9-02，B9-03 也使字符串快照不能代表可读字形 | B9-01～03；全量 `integration-ai-surface` 与能力文档检查 |
| 4 RAM、栈、任务、固定上限 | 未抬高预算、未新增产品常驻线程；部分余量紧 | 【实测】状态监听 54/64，最低内部可用总量 33,200 B；详见资源节。JSON 蓝牙响应的已知最大 token 数未到 128 | 全量资源汇总；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit9/capacity.py` 后运行其驱动 |
| 5 合并协同与回归 | 两套全量通过；正式词库与 UI、字体协同仍有缺陷 | 普通、ASan/UBSan 均 288/288，退出 0、无跳过；合并后的生成能力文档检查通过。索引成本两套各 7 场景复现；B9-02、03 是质量字符串测试遗漏的资源配合问题 | 两套全量；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_cost/run.py --build build`，内存版改为 `--build build-asan` |
| 6 Linux GCC 可移植性 | 已有 GCC 编译证据，未运行原生 Linux CI | 【实测】可用的 RISC-V GCC 15.2 对本批 CMake 中的 9 个变动 C 文件按原警告选项（含 `-Werror`）编译到汇编，均退出 0、无诊断。它使用不同 libc，不等于 Ubuntu GCC 全构建 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit9/gcc_check.py`；逐条命令见 `build/audit9/gcc/results.json` |

## 没做到的

- 未操作设备，未运行原生 Ubuntu GCC／远端 CI；GCC 交叉检查不覆盖 Linux libc 的全部诊断或链接。
- 未穷举正式词库加载中所有断电、拔卡时点和所有页面组合的资源峰值；复用既有故障回归，报告中的固定表占用是启动并进入首页后的单次采样。
- 本轮只报告，没有实施修复；构建目录的一次性探针及截图留供本机复核，未加入产品或既有测试。

## 和设计不一致的

- B9-01～03 分别影响状态反馈、G-002 可见内容一致性及正式输入法可用性，应按已有合同修复。
- 【实测】`tests/bluetooth_compare.py --build build --design "$INK_DESIGN"` 重跑得到 **31.816/255**，超过工具 12/255 参考线；命令前使用 `taskpolicy -c utility` 和依赖目录的 Python，退出 0。对照图 `build/bluetooth-compare/SetBluetooth.png`、分数 `scores.json`。与原报告相同，工具退出 0 不代表分数达标；统一 1.2 倍主题差异按任务书豁免，纸色、灰度、状态栏与提示差异保留记录，不重复列新缺陷。

## 改过的已有测试

本轮未改既有测试、断言或预算。被审计差异中的调整逐项核对：

- `tests/integration/ai_surface.c` 新增 3 个蓝牙导航能力期望；`tests/phone_calls/phone_calls.c` 新增进入蓝牙、返回通话且通话仍在的断言，没有删旧断言。
- `CMakeLists.txt` 将 `integration-text-*` 继续绑定独立 `mini-content` 以保留原排序契约，并另加正式词库质量测试；因此两类通过的证据不能互相替代。
- `tests/index_durable/run.py` 在基线提交不可用时明确跳过历史对照和 v1 迁移，符合总规则的浅克隆要求；当前仓库有历史，本轮普通和内存全量日志均无 `SKIP`，相关对照实际运行。浅克隆 CI 仍不会证明迁移链路。

## 要用户定的

本轮三个缺陷均可按现有要求修复，无需新增产品决定。索引方案 B 已在 `docs/tasks/index-cost/REVIEW.md` 被主会话采纳，本轮不改该决定，也不将其描述为已实现。

## 顺手发现

- 【已知，不重复计数】`docs/tasks/audit7-fix.md` 已安排 B7-01～B7-04；指定终点尚未包含相应产品修复。旧通话确认对象绑定、通话按键秘密字段、USB 归还断电后的索引状态及目录改名持锁问题仍以 batch7 报告为准，本轮没有声称它们已消失。
- 【已知，不重复计数】索引小时脚本再次得到物理文件 1000、数据库记录 1010；原 `index-cost/REPORT.md` 已披露电脑删除、改名后残留记录会消耗 1024 条上限，本轮未将其算成新发现。

## 资源与词库复核

- 【实测】正式词库 1,048,560 B，重复生成字节一致，SHA-256 为 `163bab7cad0b13816627252b14494bf78f2b671d56fccdd6b3620ded9df70bb4`；本轮 90 条首选 75/90、前五 83/90，兜底均为 16/90。加载 879.95 ms、词库 PSRAM 1,048,560 B，均为模拟器结果；缺文件反向对照退出 1、三项失败。证据 `build/ime-lexicon-0_eo8la2/summary.json`。未扩大原 1 MiB 上限，引擎未改，四份许可证随产物生成。
- 【实测】普通全量成功且无告警的场景中，内部 RAM 可用总量指标 `mem.internal_total.min_free_bytes`（含 DMA 池）最低为 `member-wake-stream` 的 **33,200 B**，比 32 KiB 参考预算仅多 **432 B**；`media` 为 33,424 B。这里只比较预算线，不将其称为本轮新增泄漏或硬件实测。
- 【估算：本轮模拟器栈统计】`esp_timer` 3552/3584 B、`main` 4000/4096 B、`ui` 9664/10240 B、`storage` 3504/4096 B；前两者仅余 32 B、96 B。未见越界告警，但不能据此说资源宽裕。完整路径在 `build/audit9/resources.json`。
- 【实测】启动并进入首页后的表占用：能力 **368/512**、事件订阅 **33/64**、状态项 **48/64**、状态监听 **54/64**、设置项 **22/48**、设置监听 **7/64**、数据库定义 **20/32**。均未满，状态监听仅余 10 项，不能按此单点采样宣称所有页面峰值安全。证据 `build/audit9/capacity-out/metrics.json`；驱动命令为 `taskpolicy -c utility build/audit9/capacity --profile simulator/profiles/metalio-plus.json --assets build/assets --content build/test-content --out build/audit9/capacity-out --end 15s --quiet --no-random-outages`，退出 0、无告警。
- 【实现】本批未新增产品常驻线程，蓝牙页面大数组为 `INK_PSRAM_BSS`，JSON 解析对象和扫描缓冲使用外部内存。页面最多存 16 个已配对＋8 个扫描项；两次响应分开解析，按现有每项 `addr/name` 两个字段计算，分别最多 81／41 token，低于解析器的 128 上限。

## 索引成本复核

- 【实测，模拟器】普通与 ASan/UBSan 构建分别手动运行全部 7 场景，均退出 0、各场景完成且无失败／告警，两个 `summary.json` 完全一致。证据 `build/audit9/index-cost.log`、`build/test-out/index-cost/summary.json`、`measurements.csv`，内存版对应 `build-asan` 路径。该工具没有注册 CTest，不能混入 288 项计数。
- 冷启动小时脚本：追加 26 次、压缩 1 次，索引写入 522,608 B；初次整批追加 4614.425 ms、压缩 4426.503 ms。已有索引的热启动小时脚本：追加 25 次、压缩 0 次、18,576 B。受控争用与空索引对照继续支持约 4.4 秒排队归因；不是每次日常写盘都多等 4.4 秒。
- `typ` 下追加／压缩分别 3826.615／3832.016 ms，`max` 下 17483.927／17490.249 ms，均与原报告一致。这是对 Flash 模型参数的敏感性实验，不是本轮真机测量。
- 【实现】原报告给出 4 方案、文件与改动量、验证顺序，并限定变长记录方案的收益是字节估算；没有把方案 B 说成已消除存储阻塞。本范围的索引任务仅有测量工具与报告，未覆盖或修改其他任务的产品实现。

## 新增的机制

本轮审计没有新增产品机制。被审计批次的新增项：

- `bt.enabled` 持久设置：由 `set-bluetooth.md` 明确要求；复用既有设置存储和蓝牙执行器。
- `bt.pair` 的可选 `connect` 参数、3 个页面导航能力：服务于同任务的配对后连接与 AI 页面操作；没有新增产品线程或文件格式。
- 正式词库宿主生成流程：由 `ime-lexicon.md` 要求，继续输出既有 IKD1，不增加设备端生成任务。
- 索引工作负载、受控争用探针及本轮边界驱动：只在测试／忽略的构建目录，分别由索引计量任务与本轮复查授权，不进入产品常驻任务。

## 要改的文档

- 后续修复应在两份功能任务的验收记录中补上 B9-01～03 的反向对照，不能只沿用短词质量分数或触摸蓝牙截图判定完成。
- `docs/system/system-services-design.md` 的蓝牙节仍需由主会话同步 `bt.enabled`、可选连接参数和关闭时的扫描边界（原蓝牙报告已列）；生成能力文档在当前合并终点已通过检查，不重复要求生成。
- 本轮按限制只写本报告，不改共享需求、决策及设计文档。

## 执行记录

- 已读取本轮任务书、总规则及三个任务的说明、报告、复核记录，并核对实际合并差异。
- 本轮任务书明确“不提交”，优先于通用开发规则的提交要求。
- 【实测】普通 `INK_WERROR=ON` 构建退出 0；全量 CTest 288/288 通过，276.94 秒，无跳过，日志 `build/audit9/{configure,build,ctest}.log`。此结果不替代新增边界实验。
- 【实测】ASan/UBSan 配置、构建及全量测试均退出 0，仍启用 `INK_WERROR=ON`；288/288，通过，616.01 秒，无跳过。日志 `build-asan/audit9/{configure,build,ctest}.log`。
- 【实测】两套 `LastTest.log` 未见 AddressSanitizer、UndefinedBehaviorSanitizer、LeakSanitizer 或 `runtime error:` 诊断。B9-01 的故意失败探针单列，未混入全量通过结论。
- 【实测】`git diff --check a6f4140d af8ab90a` 与工作区 `git diff --check` 均退出 0。指定范围隐私正则命中 2 处，均为规则允许的假耳机地址，位于 `tests/bluetooth.c`、`tests/bluetooth.script`；本机路径匹配 0、本报告匹配 0，证据 `build/audit9/privacy.json`。
- 【实测】结束时 HEAD 仍为 `af8ab90a`，已跟踪文件无修改；未跟踪文件只有用户提供的 `TASK.md` 和本轮 `report.md`。本轮无提交、推送或产品修复。

本轮完整构建／全量复现配置如下；`build-asan` 同配置另加 `-DINK_SANITIZE=ON`，两套先后运行，均限制 `-j2`。原始词库下载缓存从只读依赖目录复制到本仓库忽略的 `build-deps/imedict`，依赖目录未写入。

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src"
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" \
  taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

SEVERE=0 HIGH=0 MEDIUM=3 LOW=0
