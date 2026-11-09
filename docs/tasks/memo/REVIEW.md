# memo 第 1 轮复查

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。受查提交：`c326fd63`。日期：2026-11-09。

**结论：本轮不通过。发现 1 项高、3 项中，无严重项。** 主要阻断是保存失败及重试成功后的后续输入无法正常恢复。普通和 ASan／UBSan 全量均为 224/225，唯一失败是已披露的共享能力文档未同步。

按任务书、总规则第 7 节和复查提示执行。只新增本报告，不修改产品、已有测试或共享文档，不提交；临时驱动、构建和截图证据位于忽略目录。依赖及设计稿使用用户指定的 `INK_DEPS`、`INK_DESIGN`。

## 验收逐项核对

| 编号 | 复查结论 | 独立证据 |
| --- | --- | --- |
| 1 新建、编辑、删除撤销、清单、重启 | 基础场景通过，编辑组合场景有缺陷 | 原测试写入 142、重启 22 项断言通过；1,800 字节正文、撤销后的原编号和孩子数据重启保留。补测清单删行后勾选错位（R2），同批编辑与完成时备忘正文仍为旧值（R4）。 |
| 2 保存失败提示且内容不丢 | **不通过** | 原失败场景 31 项通过，但只覆盖原地保留和即时重试；补测两种离开后恢复均失败（R1）。 |
| 3 成员隔离及反例 | 通过 | 成人／孩子数据独立；重新运行 `--mutant member`，跨成员读取及删除两项断言失败，模拟器退出 1，`test.finished=1`。 |
| 4 AI 快照、按钮和 AI 同能力 | 能力路径通过，内容完整性有缺口 | 六页快照及 3,072 字节生产上下文通过；额外四行中文清单快照通过。按钮调用 `memo.*`，全量 `integration-ai-surface` 通过；长清单项尾部既不显示也不进入快照（R3）。 |
| 5 能力表、全量、内存、350 行 | 规模和内存通过，全量尚未全绿 | 产品保守计数 288 行；运行时 223 项能力、16 项设置与生成结果一致。两套全量均只失败 `capability-table-doc`；无 ASan／UBSan 诊断，零跳过。共享文档按第 7 节留合并主会话更新，不认定为虚报。 |

## 问题

### R1（高）：失败页另建草稿，继续输入后退出会恢复旧内容

- **文件和行号**：`system/apps/memo/memo.c:97`、`:122`、`:189`、`:226`；原覆盖范围见 `tests/memo/memo.c:149`。
- **复现命令**：下方补充驱动命令，场景选 `draft_loss retry_edit`。
- **操作与结果【实测】**：
  1. 建立 `ORIGINAL`，编辑时沿用原测试的写入拒绝注入，输入 `UNSAVED`，进入失败页，再输入 `_CONTINUED`。等 `ink_text_draft_saved()==1`，恢复写入，回主页再编辑原备忘；实际只剩 `UNSAVED`。
  2. 同样进入失败页，输入为 `RETRY`；恢复写入、触摸“重试”并成功，再输入 `_AFTER_SUCCESS`，等草稿落盘后退出重开；实际只剩 `RETRY`。
  两场景各 1 项恢复断言失败，模拟器均退出 1、`test.finished=1`；换新镜像重跑结果一致。没有卡住线程，也没有靠短等待制造窗口。证据：[结果](../../../build/memo-review-probes/results.log)、[重复核验](../../../build/memo-review-probes/repeat.log)。
- **原因与影响【实现】**：失败跳转销毁原编辑器并清零草稿引用，新建 `MemoSaveFail` 草稿；普通编辑只恢复同名 `MemoEdit` 草稿。成功重试后仍处于 FAIL，第 122 行继续禁止自动保存。较新的字虽存在孤立草稿中，普通备忘入口无法恢复，用户会失去刚才的输入。原测试未离开失败页，测不到这个恢复断点。
- **最小修法**：让失败提示复用当前长文会话和草稿，去掉失败时另建草稿的流程；重试成功后恢复自动保存。不要增加第二套草稿恢复、缓存或后台重试机制。

### R2（中）：删除清单前面的行，会把完成状态挪到另一项

- **文件和行号**：`system/apps/memo/memo.c:67`、`:76`、`:157`。
- **复现命令**：补充驱动场景 `check_edit`。
- **操作与结果【实测】**：创建三行 `alpha / beta / gamma`，勾选 beta，通过当前唯一的清单编辑入口删除 alpha，保存并打开清单。正文正确变成 `beta / gamma`，但 `checked=2`，gamma 变成已完成，beta 变回未完成。模拟器退出 1，1 项断言失败，`test.finished=1`。
- **原因【实现】**：正文整段替换，按行号保存的勾选位完全不变；原测试分别验证正文编辑和勾选，未组合验证。
- **最小修法**：在现有 `memo.save` 一处随行的插入或删除调整勾选位，保留原条目的完成状态，新条目未完成；不要新增项级存储框架。

### R3（中）：清单条目超过 24 字后静默截断，分页和快照仍声称完整

- **文件和行号**：`system/apps/memo/memo.c:144`、`:146`、`:148`、`:161`、`:210`。
- **复现命令**：补充驱动场景 `long_line`。
- **操作与结果【实测】**：创建 `123456789012345678901234TAIL_MUST_READ` 并转清单。界面标签与快照只有前 24 个字符；快照却是 `pages=1,total=1,truncated=false`，也没有下一页动作。尾部读取断言失败，模拟器退出 1、`test.finished=1`。证据：[实际快照](../../../build/memo-review-probes/long_line/out/snapshots.ndjson)。
- **影响**：购物规格、地址或事项尾部可能看不到；数据库正文仍完整，进入编辑器可读取，故不按数据丢失定级。
- **最小修法**：在现有条目呈现处明确省略，保留已有“编辑备忘”入口读取完整正文，快照同步说明截断及该读取入口；不要另造分页或阅读框架。

### R4（中）：同批输入后立即完成，最后一段没有更新到备忘记录

- **文件和行号**：`system/apps/memo/memo.c:118`、`:124`、`:225`。相关既有行为：`system/ui/textfield.c:172`、`system/ui/ui_task.c:799`。
- **复现命令**：补充驱动场景 `burst`。
- **操作与结果【实测】**：已有 `ORIGINAL`，打开编辑并全选，在一个 UI 调用内依次执行 `ink_text_commit_utf8("BURST_FINAL")`、`ink_text_enter()`，等页面和数据库空闲，再打开详情。备忘记录及详情仍是 `ORIGINAL`，正文更新断言失败，模拟器退出 1、`test.finished=1`。
- **原因与边界【实现】**：自动写入仅在每批消息结束的 `settle` 中发起；“完成”先退页并清除 `M.field`，旧页的最后一次保存被略过。该复现用公开输入接口模拟同批处理，尚无独立物理触摸时序证据；长文草稿仍保留新字，不据此声称草稿断电丢失，按中级记录。
- **最小修法**：在编辑页退出的统一处理处，使用该页已保留的草稿引用补交最后一次 `memo.save`，不要依赖失焦后已归零的 `ink_text_draft_id()`；不要给每个返回或导航调用方打补丁。

## 十屏视觉核对

【实测】重新生成并逐张打开了 `out/ui-compare/` 中的下列十张对照图。结束时发现 `out/` 未被 Git 忽略，已将本次生成的对照证据移入 `build/memo-review-ui-compare/`，保持仓库只新增本报告。目标为 12/255，全部超出。六个备忘页的分数与原报告一致；当前主题已按规范放大并加深灰色，不能为了旧稿分数缩小字号或调浅颜色。

| 页面 | 差值 /255 | 肉眼观察 |
| --- | ---: | --- |
| Memo | 46.208 | 没有搜索和分类分段；清单变成普通文本摘要；统一描边卡片，记录时间和类型层次消失；“写一条”取代原稿录音主入口。录音不属于本任务。 |
| MemoDetail | 28.821 | 统一标题“备忘”，正文放在大描边卡片中，底部三个胶囊紧贴无间隔。录音播放和 AI 提取事项未做，原报告已披露。 |
| MemoEdit | 32.268 | 原稿为正文和底部动作，实际为长文输入框、固定键盘及页码；使用长文编辑器符合任务要求，不能仅因键盘占屏判为本任务缺陷。 |
| MemoChecklist | 25.131 | 原稿是连续分组、勾选后划线；实际每项单独大卡片，没有划线、逐项删除或“加一项”，统一进编辑页修改；长项缺字另列 R3。 |
| MemoUndo | 55.471 | 黑底撤销条变成白底整条胶囊，撤销和列表使用相同描边层次；与原稿主操作的层级差异明显。 |
| MemoSaveFail | 35.612 | 失败卡和“腾出空间”入口被两行文字替代，提示末字“试”换到下一行；提示确实在编辑器和键盘上方，未遮挡正文；恢复缺陷另列 R1。 |
| Main | 54.194 | 时钟和图标更大、灰底更深；阅读提示为真实空状态；Dock 中备忘入口存在。属于共享主题和夹具差异。 |
| Apps | 42.961 | 应用分两页，灰底及组间距离增大；备忘只在“记和联系”出现，没有重复工具项。 |
| ImePinyin | 39.031 | 编辑区更矮，候选数、首选候选黑底、麦克风及底行布局不同；沿用已有输入法。 |
| ImeEdit | 36.919 | 选区用方括号呈现，原稿用黑底；工具行是圆形按钮，包含“剪切”，与原稿“全选”位置不同；沿用已有输入法。 |

证据：[分数](../../../build/memo-review-ui-compare/scores.json)、[备忘列表](../../../build/memo-review-ui-compare/Memo.png)、[清单](../../../build/memo-review-ui-compare/MemoChecklist.png)、[失败页](../../../build/memo-review-ui-compare/MemoSaveFail.png)。这些已披露的视觉差异不冒充视觉通过，也不把任务明确排除的录音功能计成漏做。

## 品味和规模、架构审视

- 【实测】产品新增 288 行：应用 267，共享启动／文本接口与实现／列表去重 17，CMake 4。未超过 350 行；没有放宽预算、删测试或用 sleep 改造产品时序。已有应用数断言从 7 到 8 与注册新应用一致，AI 白名单仅追加。
- 【实现】成员数据落在 `ink_db`，写操作经后台能力，LVGL 位于 UI 路径；没有新增常驻任务、线程、队列、后台重试、配置层或独立持久文件格式。新增的 `ink_text_draft_read` 是复用草稿所需的小接口，未发现需要拆出新子系统的依据。
- **建议删的重复流程**：R1 中失败页销毁再建编辑会话、丢弃原草稿引用的流程。失败提示是任务要求，第二份编辑草稿不是要求；复用会话同时减少状态分支和恢复错误。该项已计入 R1，不重复计数。
- 【实现／实测】新用户操作注册十项 `memo.*`，触摸和 AI 进入同一能力；成员集合开启 AI 读取，未发现绕过确认或跨成员泄露。快照的内容边界仍需按 R3 修正。
- 【实现】共用文件除 `navigation.c:411` 的一行备忘去重外均为末尾接线。该行与已有电话排除相邻、范围小、原报告已披露，且相关全量检查通过，不建议为“只在末尾加行”另造过滤机制。
- 【实测】基线差异没有受保护资料、其它并行任务文件、本机绝对路径或 MAC 模式；`git diff --check` 通过。此次复查未碰设备、真实通信、付费接口或远端写入。

## 可复现命令和证据

以下命令从仓库根目录执行，环境变量使用用户给定值。两个构建目录已核对 `INK_WERROR=ON`、字体、Python、LVGL／SDL2／ImGui 路径一致；ASan 目录额外启用 `INK_SANITIZE=ON`，实际编译参数含 `-fsanitize=address,undefined`。构建及全量串行，均不超过 `-j2`，未启动 GUI。

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/memo/run.py --build build --visual
```

结果：普通构建 0、全量退出 8（139.92 秒）；内存检查构建 0、全量退出 8（252.18 秒）；定向加六页截图退出 0。日志：[普通全量](../../../build/memo-review-ctest.log)、[内存检查全量](../../../build-asan/memo-review-ctest.log)、[定向](../../../build/memo-review-visual.log)。定向证据在 `build/memo-vap_s4c1/`，零告警、零预算违规，内部 RAM 总余量最低 47,840 字节，原门槛 32,768 未改。

反向对照逐个运行：

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/memo/run.py --build build --mutant member
# 分别将 member 换为 write、failure、undo
```

四个命令均编译、链接成功后退出 1，分别有 2、25、6、2 项断言失败，均 `test.finished=1`。日志为 `build/memo-review-mutant-<名称>.log`；没有用编译失败冒充有效反例。

补充复现使用 [临时驱动](../../../build/memo-review-probes/driver.c) 和 [运行器](../../../build/memo-review-probes/run.py)。驱动沿用原验收夹具与写入拒绝边界，仅追加场景；直接包含当前产品源码，未修改产品实现。运行器沿用目标的编译和链接参数，逐个输出模拟器退出码和断言数；汇总脚本本身正常退出，问题场景的**模拟器子进程**退出 1。

```sh
INK_MEMO_REVIEW_OUT="$(mktemp -d build/memo-review-rerun-XXXXXX)" \
  taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" \
  build/memo-review-probes/run.py draft_loss retry_edit check_edit long_line burst full_snapshot
```

前五个场景各失败 1 项，`full_snapshot` 的 21 项全部通过；全部运行到结束标记。证据保留在 `build/memo-review-probes/`。这些复查生成的文件不进入源码改动。

能力表实查：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output build/memo-review-capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
```

退出 0：223 项能力、16 项设置一致，共 284 项定义。正式文档由合并主会话生成后再通过 `capability-table-doc`。

视觉复现：先运行上述 `--visual`，将 `build/memo-compare/<六个备忘页>/` 的 `snap-scene.png` 与 `metrics.json` 放入同名 `out/ui-compare/` 子目录，再运行：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out out/ui-compare --scenes Main Apps ImePinyin ImeEdit
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out out/ui-compare --scenes Memo MemoDetail MemoEdit MemoChecklist MemoUndo MemoSaveFail Main Apps ImePinyin ImeEdit --reuse
```

两次均退出 0；`ui_compare` 的退出 0 表示产出了有效对照，分数仍全部超过目标。没有 Linux／gcc 或实机验证结论。

SEVERE=0 HIGH=1
