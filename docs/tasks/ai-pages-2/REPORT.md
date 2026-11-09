# ai-pages-2 执行报告

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。分支：`claude/codex-ai-pages-2`。

首轮执行结果保留如下；本次修复与最新验证见文末“复查后的修复”。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 真实 AI 请求的断网、45 秒超时、明确额度错误；触摸、主页键、AI 关闭后正文和位置不变 | `ctest --test-dir build --output-on-failure -j2 -R '^ai-pages$'` |
| 2 | 【实测】通过 | 能力总线实际写入十条活动；最新在前，实体键和 AI 分页；孩子读取大人记录编号仍为空，屏幕也无大人记录 | 同上，`records` 场景 |
| 3 | 【实测】通过 | 点击记录打开对应回执；十种已有回执状态逐页可读 | 同上，`records` 场景 |
| 4 | 【实测】通过 | 五页结构化快照，使用生产 AI 请求的 3072 字节上限；孩子额度文案与错误页清理 | 同上 |
| 5 | 【实测】全量门槛未通过；内存检查未见运行期错误；行数通过 | 普通、ASan/UBSan 全量各 228 通过、2 文档检查失败、3 环境跳过；含构建接线新增 185 行产品代码、删除 1 行 | `ctest --test-dir build --output-on-failure -j2`；同命令换 `build-asan`；`git diff 3fce2bee..HEAD --numstat -- system CMakeLists.txt` |
| G4 | 【实测】超过目标，回执图无法直接评分 | 四页分数见下表；回执原图尺寸不同，工具退出 1 | `python tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/ai-pages-compare --scenes AiOffline AiTimeout AiBudget AiLog --reuse` |

## 没做到的

- 普通、ASan 全量均有两项失败：`capability-table-doc` 的生成能力文档过期；`services-mid-capability-doc` 的服务设计 §1.3 缺四个新界面能力。按任务约束未改文档，也未改检查器、跳过或放宽断言。
- 当前中枢没有真实的每日计费状态；设备识别明确的 `error.data.code="budget"`（兼容 `error.message="budget"`），测试使用合成中枢响应，未验证真实厂商额度。
- ACTIVITY 没有保存完整回执原文和细分失败原因，本次显示现有十种标准状态、操作、时间和对象；未接画板里的撤销/改回控件。
- 必要接线未严格满足“共用文件只在文件末尾追加”：AI 状态展示函数末尾加两行，服务错误分支加一行、回调替换一行并加两行。没有改其他并行任务的页面文件。
- 没有在 Linux 或真机上验证。

## 和设计不一致的

超时不写“什么都没有改”，因为已有服务不能证明超时前没有执行操作；改为“没有收到结果，不会自动重试”。离线的“再试一次”和超时的“再说一次”都开启一次新的话筒提问，不重放已执行过的工具调用。

| 画板 | 1/4 灰度平均差 | 差异 |
| --- | ---: | --- |
| AiOffline | 46.679/255 | 实际主题为放大后的字号与尺寸；真实阅读内容、位置、图标与旧画板不同 |
| AiTimeout | 44.719/255 | 同上；超时说明按实际证据改写 |
| AiBudget | 46.222/255 | 同上；说明随实际家人身份变化 |
| AiLog | 34.283/255 | F4 白色分页行和真实合成夹具；画板为灰色连组、示例内容和不同动作 |
| AiReceipts | 无直接分数 | 原图是 1448×1360 的状态组件总览；目标画面 684×1216，工具报告尺寸不匹配，未裁切或缩放设计图凑分数 |

## 改过的已有测试

- `tests/integration/ai_surface.c`：给严格的 AI 能力白名单增加六项新导航；回执的 `id` 是可选参数，其余无参数。
- `tests/integration/apps.c`：精确数量从 11 改为 12，新增一项可从应用列表打开的“AI 做过的事”。

## 要用户定的

无。

## 顺手发现

锁定的中枢 Python 依赖环境缺失，既有 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 自动返回跳过；未把它们计为通过，也未安装依赖。

## 新增的机制

无新任务、状态机、文件格式、配置项、缓存或重试。五页使用既有活动、浮层、应用入口和 F4 列表；用户主动点重试时开启新的话筒提问。

## 要改的文档

合并时更新 `docs/system/ui-dependencies.md` 的五页状态、生成 `docs/system/capabilities.md`（新增六项），并在 `docs/system/system-services-design.md` §1.3 登记 `ai.pages.close/retry/next/prev`。本分支未改这些文档。

## 里程碑

- 已读任务书与总规则，确认基线、独立分支及干净工作区。
- 五页及六个导航能力完成；新页面在 `system/ui/shell/ai_pages.c`。AI 状态展示函数末尾加桥接，启动函数末尾注册；服务错误回调增量识别明确额度标记。界面只读已缓存的当前成员活动集合，不做存储 I/O。
- 【实测】Werror 全量构建成功；四个定向场景通过；`tools/ui_scale_check.py` 检查 34 个文件，0 违规。
- 规模超过默认 100 行的原因：三个错误视图、两个记录视图、快照及能力接线共 180 行 C 代码，另有 5 行构建接线。只替换旧状态文字虽更小，却没有独立页面布局、动作和快照；本次复用原 AI 面板生命周期、现有活动集合及 F4，不再建服务、记录格式或异步加载器。
- 【实测】普通全量和修正清单后的全量都已运行；界面、预算、确定性、成员隔离测试通过。临时生成的能力文档与 Plus 运行时核对：268 项能力、16 项设置全部匹配。
- 【实测】普通、ASan/UBSan 的正向四场景均为 21 / 20 / 30 / 85 个检查，无失败；最终证据 `build/ai-pages-jxn56dib/`、`build-asan/ai-pages-c9kkzepc/`。命令 `python tests/ai_pages.py --build build`，以及换成 `build-asan`。
- 【实测】反向对照都编译成功、运行退出 1：`--mutant errors` 检出 6 处；`member` 检出 6 处；`receipts` 检出 10 处；`snapshot` 检出 3 处。命令均为 `python tests/ai_pages.py --build build --mutant <名称>`；只编译临时源副本，不改工作区产品源码。
- 【实测】构建、测试均使用 `taskpolicy -c utility`、最多 `-j2`，证据用 `INK_KEEP_EVIDENCE=1` 保留；ASan 配置仅在总规则参数上增加 `-DINK_SANITIZE=ON`，保留 Werror。

| 场景 | 普通 / ASan 内部 RAM 最低余量（字节） | 原下限 |
| --- | ---: | ---: |
| 离线 | 47832 / 47832 | 32768 |
| 超时 | 42200 / 42200 | 32768 |
| 额度 | 41728 / 41728 | 32768 |
| 记录 | 47840 / 47840 | 32768 |

最终差异仅含五页、必要接线、测试和本报告。`git diff 3fce2bee..HEAD --check` 通过；按总规则第 6 节对该差异做隐私扫描，0 命中（grep 退出 1）。没有提交构建产物、设计图或依赖副本。

## 复查后的修复

### 主会话补充 AP-L1、AP-L2

本次从 `50ccea4b` 接续；主会话追加的复查记录原样保留，不计入本次提交。

| 问题 | 修复 | 验证 |
| --- | --- | --- |
| AP-L1 中 | 【实现】AI 服务传明确的离线、断线、超时、额度类型，页面不再按提示文案判断错误 | 【实测】`python tests/ai_pages.py --build build --case typed`，与 AP-L2 合计 62 项检查通过；相同改写文案对应四种错误，只有旧错误文案而没有错误类型时不建错误页 |
| AP-L2 中 | 【实现】普通面板与三种错误页各用常量浮层描述，始终只登记一个 AI 浮层；面板集中移除／登记当前描述，收起、展开、关闭沿用同一生命周期；声明移入头文件 | 【实测】同一场景验证同层浮层登记排序、正常与错误互换、隐藏期间换错误、展开及关闭，62 项检查通过；证据 `build/ai-pages-3dv42u4g/typed/` |

新增机制：无。没有新线程、任务、状态机、缓存、配置或重试。已有测试只适配带错误类型的函数签名，未改原断言与预算。

【实测】普通 Werror 构建成功，日志 `build/ap-l-configure.log`、`build/ap-l-build.log`。相对任务基线产品新增 264、删除 9 行，合计 273／300 行（含新头文件，不含测试和文档）。

【实测】普通全量 233 项：231 通过、0 跳过，仅 `capability-table-doc`、`services-mid-capability-doc` 两项获准忽略的既有文档同步失败，CTest 退出 8，303.07 秒。AI 十场景全部通过，检查数为 21／20／30／117／32／22／38／38／60／62；证据 `build/ai-pages-4365yz7w/`，日志 `build/ap-l-ctest.log`、`build/Testing/Temporary/LastTest.log`。

【实测】ASan／UBSan 全量也为 231 通过、0 跳过、上述两项文档失败，CTest 退出 8，284.55 秒。构建保留 Werror，实际编译参数含 `-fsanitize=address,undefined -fno-omit-frame-pointer`；完整 `LastTest.log` 未见 sanitizer 或运行期未定义行为诊断。十场景检查数与普通构建相同，证据 `build-asan/ai-pages-819i07tx/`；日志 `build-asan/ap-l-configure.log`、`build-asan/ap-l-build.log`、`build-asan/ap-l-ctest.log`、`build-asan/Testing/Temporary/LastTest.log`。两套十场景均完成、零失败、零预算违规；内 RAM 最低余量均为 41728 字节，原下限 32768；内部、外部 failed_allocs 均为 0。

【实测】十二组反向对照均编译成功、`test.finished=1`，运行退出 1。本次新增两组如下；前十组失败数与上轮一致。只编译构建目录内的临时副本，没有撤改工作区产品文件。命令：`python tests/ai_pages.py --build build --mutant <名称>`；汇总 `build/ap-l-mutants.json`，逐组日志 `build/ap-l-mutant-<名称>.log`。

| 变体 | 撤掉的修复 | 通过／失败 | 证据 |
| --- | --- | --- | --- |
| typed | AP-L1：恢复按中文提示比对错误种类 | 29／33 | `build/ai-pages-d6tupr70/typed/` |
| overlay | AP-L2：恢复运行时修改已登记的浮层描述 | 45／17 | `build/ai-pages-o1y8id_5/typed/` |

【实现＋实测】原三项环境跳过均恢复通过：`hub-contract` 的 125 个 pytest 用例通过，`hub-json-crosscheck`、`hub-simulator-e2e` 通过。所需依赖已齐，无需安装：`$INK_DEPS/hub/.venv/bin/python` 为 Python 3.11.15，其中 aiohttp 3.12.15、PyYAML 6.0.3、pytest 8.4.2、pytest-asyncio 1.2.0 均可导入。继续指定 `INK_HUB_PYTHON`，运行加 `PYTHONPATH="$PWD/hub"`，根因和设置方法同下方前一轮记录。

【实测】`tools/ui_scale_check.py`：35 文件、0 违规。按原命令复算四页对照仍为 46.807／44.719／46.222／34.283，未改变布局；已查看断线与额度页截图。超过 12/255 的差异原因同首轮。

本次改过的已有测试：`tests/ui_shell_regressions.c` 仅补 `INK_AI_ERROR_NONE` 实参；`tests/ai_pages.c` 增加 typed 场景，`tests/ai_pages.py` 适配原反向变体并增加 typed／overlay。没有放宽断言、删除测试、增加跳过或修改资源预算。没做到的／和设计不一致：沿用上轮已披露项，R1-06 仍超过一两行，按本次范围保留。要用户定的：无；要改的共享文档：仍由主会话合并时处理。

【实测】两套构建、全量及反向命令均使用 `taskpolicy -c utility`；构建与 CTest 均最多 `-j2`，串行进行。全量、配置和定向命令沿用下方前缀与参数。本次产品修复新增 50、删除 21 行；提交范围为六个产品文件、三份测试和本报告，没有共享文档、依赖或构建产物。任务基线至暂存版本的 `git diff --cached 3fce2bee --check` 通过；按总规则的隐私表达式扫描该差异，0 匹配（grep 退出 1）。

### 前一轮修复记录

复查起点：`d0d793e9`。下面是本次修复的结论；上文保留首轮执行时的结果。

| 问题 | 修复与定向证据 | 命令（前缀见下文） |
| --- | --- | --- |
| R1-01 中 | 【实测】复用 AI 面板的同一个浮层；收起后 AI 看到阅读页、下一页键可用，重新展开恢复错误页；32 项检查通过 | `python tests/ai_pages.py --build build --case hide` |
| R1-02 中 | 【实测】真实请求第 10 秒断网进入 AiOffline；保留“回答没收全，不会自动重试”，关闭后原阅读位置不变；22 项检查通过 | 同上，`--case lost` |
| R1-03 中 | 【实测】冷集合调用已有 `activity.list` 在 BG 加载；加载和失败均进入屏幕与快照，回执校验页面代次与成员归属；存储恢复后原页显示首条活动，无需重开。新成员、集合槽耗尽后恢复各 38 项检查通过 | 同上，`--case newmember`、`--case loadfail` |
| R1-04 中 | 【实测】回执状态及对象完整换行，按主题行高减少 F4 容量；断言实际标签行高、行边界、无省略与无重叠，检查真实亮度对象及最大字号长状态；已查看两张截图 | 同上，`--case real`，与 R1-05 合计 60 项检查 |
| R1-05 中 | 【实测】读取已有 `object/revision/undo_ms/when`；窗口内触摸按钮和 AI 均调 `system.undo`，亮度恢复，重复撤销被拒绝，过期回执不提供撤销；不修改记录格式 | 同上，`--case real` |
| R1-06 低 | 未改：需分别保存两个视图的位置并在成员代次变化时清空，超过一两行；按本次低级修复范围保留 | 复查记录 R1-06 |

- 【实测】普通定向九场景分别 21／20／30／117／32／22／38／38／60 项检查通过，全部 `test.finished=1`、失败和预算违规为 0。证据：`build/ai-pages-w3hzfwag/`。内 RAM 最低余量 41728 字节，原下限 32768；内部、外部 failed_allocs 均为 0。
- 【实现＋实测】原三项跳过的根因是 `INK_HUB_PYTHON` 选用了只有构建工具的 Python，而非锁定中枢环境。指定依赖目录已有 Python 3.11.15、aiohttp 3.12.15、PyYAML 6.0.3、pytest 8.4.2、pytest-asyncio 1.2.0，无需安装或升级。构建改选 `$INK_DEPS/hub/.venv/bin/python`；运行设置 `PYTHONPATH="$PWD/hub"`，确保导入本检出的中枢源码。
- 【实测】`ui_scale_check.py`：34 文件、0 违规。四页对照为 46.807（途中断网）／44.719／46.222／34.283，仍超 12/255；原因同首轮。回执使用原组件总览核对完整文案与撤销入口，不缩放原图凑分数。
- 【实现】新增机制：无。删除错误页重复的浮层所有权；首次读取只调用一次已有 BG 能力，失败后在既有 settle 中查询已加载集合；没有加载线程、后台重试、缓存或新导航机制。撤销是否仍有效由已有能力最终校验。
- 改过的已有测试：仅扩展 `tests/ai_pages.c`、`tests/ai_pages.py` 的场景和反向变体，未改原期望、删断言、放宽预算或跳过测试。
- 没做到的／和设计不一致：R1-06、真实供应方额度、原始回执未保存的细节、Linux 和真机验证仍保留；本次不扩展范围。共享文件接线仍在现有函数内，原因同首轮。要用户定的：无。要改的共享文档：同上，按用户明确指示忽略两项能力文档同步失败。

【实测】普通全量 233 项：231 通过、0 跳过，仅 `capability-table-doc`、`services-mid-capability-doc` 两项既有文档同步失败（用户明确允许忽略），CTest 退出 8，433.94 秒。原三个跳过全部恢复为通过；其中中枢契约 125 个 pytest 用例全部通过。日志：`build/recheck-build.log`、`build/recheck-ctest.log`。

【实测】十组反向对照均编译成功、场景到达 `test.finished=1`，以断言失败退出 1；只编译临时产品副本，不改工作区源码。命令：`python tests/ai_pages.py --build build --mutant <名称>`；日志 `build/recheck-mutant-<名称>.log`，其中记录证据目录。

| 变体 | 对应问题 | 通过／失败 |
| --- | --- | --- |
| hide | R1-01：恢复独立错误浮层 | 28／4 |
| lost | R1-02：撤掉途中断网映射 | 15／7 |
| load | R1-03：不发起 BG 加载 | 635／3 |
| recover | R1-03：撤掉缓存恢复 | 635／2 |
| wrap | R1-04：恢复单行省略 | 46／4 |
| undo | R1-05：撤掉撤销动作 | 41／10 |
| errors／member／receipts／snapshot | 保留首轮四组反例 | 15／6、111／6、107／10、18／3 |

【实测】ASan／UBSan 全量同为 231 通过、0 跳过、两项获准忽略的文档失败，CTest 退出 8，367.79 秒；构建成功。完整 `LastTest.log` 中没有 sanitizer 或运行期未定义行为诊断。全量中的九场景检查数与上表相同；普通证据 `build/ai-pages-icubdrza/`，内存检查证据 `build-asan/ai-pages-38gux690/`，均完成且零失败、零预算违规。两套九场景的内 RAM 最低余量均为 41728 字节，内部、外部 failed_allocs 均为 0。日志：`build-asan/recheck-build.log`、`build-asan/recheck-ctest.log`、两套 `Testing/Temporary/LastTest.log`。

复现前缀与命令（沿用总规则全部配置参数，另设 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；ASan 只再加 `-DINK_SANITIZE=ON`，两套保留 Werror）：

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# 上述定向／反向命令中的 python 同样使用：
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ai_pages.py --build build --case real
```

【实测】修复提交 `fc03f1a1`。相对复查起点只改两个产品文件、两份测试及本报告；产品修复新增 65、删除 23 行。相对任务基线，产品新增 228、删除 2 行，按相加计 230／300 行。没有夹带共享文档、依赖、设计稿或构建产物。`git diff 3fce2bee..HEAD --check` 通过；按总规则对任务基线至 HEAD 差异做隐私扫描，0 命中（grep 退出 1）。
