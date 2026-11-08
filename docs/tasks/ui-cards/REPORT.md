# ui-cards 执行报告

任务基线：`1bbf1a49fb8d79d513d6260820fb164a89dedef1`。日期：2026-11-09。最新结论见“复查后的修复”；其后各节保留首轮执行时的结果，便于追溯。

## 复查后的修复

修复起点：`b0b22da6`。REVIEW 只有两个中级问题，均已修复；严重、高、低均无待修条目。

- 【实现】R1：`app.cards_rev` 只调用已有 `ink_ui_kick()`，由 UI 任务现有 settle 检查可见性和一秒节流，不再调用重建状态栏的 `refresh()`。
- 【实现】R2：仅在 `cards_reply()` 中切分生产端的扁平数组元素，逐卡复用 JSON 解析和五个卡位的原选择逻辑；切分跳过字符串内的引号、反斜杠和分隔符。全局仍为 128 tokens，回执仍为 2 KiB。读取失败清掉半成品、禁用动作、显示“卡片没能读取”，快照明确失败；下次普通变更可恢复。
- 【实测】`tests/ui_cards.c` 共 149 项检查通过，`test.finished=1`，无失败、无警告。Main 隐藏和控制中心覆盖时实际渲染／显示提交均为 0；十次可见变更合并为一次读取、一次额外渲染和提交（另有无变化的翻页对照各 1 次）。七卡、八卡加真实闹钟均保持首卡、打开能力、Today 标签和 AI 页面快照同源；八卡确实超过原整数组 token 上限。超长来源失败及恢复也通过。已查看 `build/test-out/ui-cards/clean-today-eight-cards.png`，听书和闹钟正常显示。

### 验收表（本轮）

| 编号 | 结论 | 证据／命令 |
| --- | --- | --- |
| 1 同源卡片 | 【实测】通过 | `ui-cards` 包含原单卡及新增七／八卡、闹钟、转义字符场景 |
| 2 成员过滤 | 【实测】通过 | 保留成人→孩子→成人及应用可见性全部原断言，149 项中通过 |
| 3 可见更新与节流 | 【实测】通过 | 新增 `LV_EVENT_REFR_READY` 与 `ink_display_commits()` 断言；`test.cards_burst_updates/renders/commits=1`，`test.cards_covered_commits=0` |
| 4 同一打开能力 | 【实测】通过 | 原真实触摸及 AI 打开同一页面断言保留，多卡仍导出同一能力 |
| 5 全量、内存、预算 | 【实测】通过 | 普通、ASan/UBSan 各 208/208，均 0 失败、0 跳过，无内存诊断；累计产品代码 186/250 行 |

### 反向对照

【实测】只在忽略目录生成独立 `navigation.c` 对象，依次链接同一份新测试；未原地撤改工作区，也未修改断言或预算。三例编译、链接退出均为 0，模拟器均退出 1 且运行到 `test.finished=1`：

| 撤掉的修复 | 失败检查数 | 决定性证据 |
| --- | ---: | --- |
| R1：恢复状态栏通用刷新 | 4 | 十次变化产生 11 次额外渲染／提交；覆盖态四次变更多提交 4 次 |
| R2：恢复整数组一次解析 | 3 | 八卡时页面快照失败、首卡及闹钟丢失 |
| R2：恢复空态文案 | 1 | 来源读取失败被显示成“还没有在听的内容” |

命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B work/ui-cards-fix-negative.py`。结果及每例完整编译／链接／运行命令：`work/ui-cards-fix-negative/results.json`、同目录日志。生成能力表前的 `tools/capabilities.py --check` 退出 1，补齐生成表后退出 0，是文档失败的反向对照。

### 原有失败与跳过的处理

- 【实测】`capability-table-doc` 只是生成文档过期：本轮按用户“原来就失败的测试查清并处理、全量全过”的明确要求，更新此前留给合并会话的共享生成表，仅补 `app.cards_rev` 一行。运行原生成器，不改检查程序，250 项定义校验通过。
- 【实测】此前构建所选的通用 Python 缺 `pytest`、`pytest_asyncio`、`aiohttp`、`yaml` 和中枢包，导致 `hub-contract`、依赖其消息的 `hub-json-crosscheck`、`hub-simulator-e2e` 跳过。改用已存在的只读 `$INK_DEPS/hub/.venv/bin/python`：Python 3.11.15，pytest 8.4.2、pytest-asyncio 1.2.0、aiohttp 3.12.15、PyYAML 6.0.3，全部 18 个适用于 macOS 的包与 `hub/uv.lock` 一致。无需安装或升级依赖。设置 `PYTHONPATH="$PWD/hub"` 并核对模块来自本任务检出，避免误测依赖目录代码；禁写 bytecode。环境核对见 `work/ui-cards-fix-hub-env.log`。

### 全量与内存检查（本轮）

配置沿用总规则的字体、Python、LVGL 与 `INK_WERROR=ON`，两目录均选上述现有中枢 Python；只有 `build-asan` 加 `INK_SANITIZE=ON`。已核查编译参数包含 `-fsanitize=address,undefined`。构建最多三路、CTest 最多两路，两轮顺序运行：

```sh
export PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub"
# 总规则配置参数另加：-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】普通全量 208/208 通过，122.71 秒；ASan/UBSan 全量 208/208 通过，223.59 秒。两轮均 0 失败、0 跳过，配置、构建、CTest 全部退出 0；原四项失败／跳过测试均实际通过。两份完整 `Testing/Temporary/LastTest.log` 无 ASan、LeakSanitizer 错误或 UBSan runtime error。两轮定向场景均为 149 项检查通过，内部内存最低余量仍为 47,840 字节，内外部内存分配失败均为 0。逐阶段退出码和耗时见 `work/ui-cards-fix-validation.json`；完整命令及输出见 `work/ui-cards-fix-build-{configure,build,ctest}.log` 和 `work/ui-cards-fix-build-asan-{configure,build,ctest}.log`。

### 改动边界

【实现】本轮产品只改 `navigation.c`，新增 26、删除 13 行；任务基线起累计新增 161、删除 25，共 186 行（含 CMake），未超 250 行。不新增任务、线程、定时器、状态机、格式、缓存、重试、队列、配置项或抽象层。原 `app.cards` 能力、UI 执行器和 F1 快照／动作路径继续共用，成员过滤不变。

改过的已有测试仅 `tests/ui_cards.c`：保留全部原断言；扩展合成提供者的卡数及超限模式，新增实际渲染／提交与多卡断言。新测试用同一翻页的无变化调用作对照，并在尾随更新后经原 UI 作业入口等待本轮绘制完成；没有加 sleep 掩盖问题，没有放宽任何期望或预算。没有其它验收内未完成项；未测 Linux/gcc、实机或交互窗口。其它页面视觉差异及阅读离页卡片缺口维持首轮报告的归属，不扩展修复。

【实测】`git diff --check` 通过，`git diff --stat` 只含本轮两处修复、配套测试、此报告和一行生成文档。按总规则对任务基线至提交差异做本机路径／MAC 扫描，grep 退出 1、无匹配。仅本地提交，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 同源卡片 | 【实测】通过 | 实际 Today 标签、F1 快照、AI 上下文与 `app.cards` 的标题、文本、版本、能力相同；既有闹钟增删也同步 | 下述定向命令 |
| 2 成员过滤及反例 | 【实测】通过 | 成人→孩子→成人，数据与应用可见性隔离；撤掉成员过滤失败 7 项，撤掉应用过滤失败 1 项 | 下述定向命令与反向对照 |
| 3 可见更新及节流 | 【实测】通过 | Main、控制中心覆盖、另一活动下各 0 次；10 次连续变更合并为 1 次；解除覆盖补 1 次；无变更无心跳 | 下述定向命令 |
| 4 同一打开能力 | 【实测】通过 | 真实触摸与 AI 调用快照里的同一能力，各打开同一个实际页面一次 | 下述定向命令 |
| 5 全量、内存及代码预算 | 【实测】部分通过，全量未全绿 | 普通与 ASan/UBSan 各 204 通过、1 文档失败、3 缺依赖跳过；无内存诊断；产品改动含 CMake 共 173 行，未超 250 | 下述全量命令、`git diff --numstat` |

## 没做到的

第 5 项要求的全量全绿未达到。能力总表按[总规则第 7 节](../codex-rules.md)留给合并会话统一生成，因此 `capability-table-doc` 仍失败；没有规避或修改这项检查。三项中枢测试因现有 Python 环境缺少锁定依赖而跳过，未安装或升级依赖。未运行 Linux／gcc 和实机。

## 和设计不一致的

本轮未发现 F5 范围内的设计差异。保留原五个卡位和样式，卡位取对应应用的第一张卡；其它卡仍可经 `app.cards` 读取，选择和排序留给既定 F8。

## 改过的已有测试

- `tests/ui_shell_scenes.c` 的 Today 身份断言：从查找旧文本 `Today` 改为核对 JSON 的 `title=今日`，原页面／画板身份检查保留。
- 同文件 Today 语义断言：从三个 `标签=` 子串改为五个固定卡位的 id、角色、标题、空态值，以及无虚构动作的 `actions=[]`；适配 F1 格式，覆盖比原来更完整。普通、大字号、儿童场景共用。
- 新建 `tests/ui_cards.c` 和场景；未删用例、放宽断言或修改资源门槛。

## 要用户定的

无。本轮按任务边界保留五个卡位，不增加主页编辑或业务应用。

## 顺手发现

【实现】既有阅读卡提供者 `apps/reader/reader.c` 仍依赖 `RD.text`，离开阅读页后没有这张卡。F5 如实显示空态，未扩展到阅读进度持久化或开发其它业务应用。

## 新增的机制

- 页面模型：任务 F5 要求 Today、AI 同源，复用 F1 字段与动作；存放在外部内存。
- 一次性 LVGL 定时器：任务 F5 要求节流，变更在 1 秒内合并，退出今日页时销毁；无独立心跳、常驻任务、线程、状态机、持久格式或配置项。
- 卡片可选 `member` 和公开 `rev`：任务 F5 要求成员隔离及同源数据；空成员沿用既有提供者按当前成员取数据的合同，过滤集中在 `ink_app_card_visible()`。

## 要改的文档

- 合并会话更新 `docs/tasks/backlog.md`、`docs/system/ui-dependencies.md` 的 F5 状态。
- 系统服务设计补卡片可选成员字段和 `app.cards` 的版本字段；共享文档与能力生成文档未改。
- `docs/system/capabilities.md` 的 `system/ui/shell/navigation` 状态引用列表须补 `app.cards_rev`。生成器验证 250 项定义通过，差异只有这一行；建议稿和差异分别在 `work/ui-cards-capabilities.md`、`work/ui-cards-capabilities.patch`，未修改共享原件。

## 定向验证与规模

```sh
taskpolicy -c utility cmake --build build --target sim-ui-cards -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ui-cards$' --output-on-failure -j2
```

【实测】104 个检查通过，`test.finished=1`，无失败、无警告，十次变更对应 `test.cards_burst_updates=1`。证据：`build/test-out/ui-cards/metrics.json` 和同目录成人、孩子的无窗口截图。

【实现】含 CMake，产品代码新增 148、删除 25，共 173 行。超过 100 行的原因是需要把既有占位卡位接入真实能力回执、F1 模型、成员过滤和可见时节流。看过更小的直接枚举应用回调方案，但那样 Today 不实际读取 `app.cards`；仅换文案也不能满足快照和同一打开路径。既有闹钟信息接入 home 的卡片提供者，没有重新注册应用。关机书签仅复用同一个成员过滤，避免新增成员字段被另一个消费者忽略。

## 反向对照

【实测】每次只改下表一处产品逻辑，运行上述定向构建与 CTest；六次均构建退出 0、CTest 退出 8，检查实际失败，随后逐字恢复源码。没有删断言或放宽预算；恢复后定向检查再次通过。日志在 `work/ui-cards-negative-*.log`，结构化汇总在 `work/ui-cards-counterexamples.json`。

| 临时撤掉的逻辑 | 失败检查数 | 首个决定性失败 |
| --- | --- | --- |
| `ink_app_card_visible()` 的成员相等条件改为 true | 7 | 两条路径出现另一成员的卡片 |
| `cap_cards()` 的 `ink_app_visible(d)` 条件 | 1 | 孩子的卡片出现成人报纸 |
| settle 的 `ink_ui_top_name()==home` 条件 | 1 | 控制中心覆盖时更新 |
| settle 的 1 秒间隔改为 `now < N.cards_at` | 2 | 十次变更未按间隔合并 |
| 小卡标签改用固定错误文案 | 12 | 真实标签与 `app.cards` 不同 |
| 触摸绑定的动作 id 改为 reader | 3 | 真实点击未走声明能力打开页面 |

## 架构审视

【实现】数据提供者经原应用目录暴露 `app.cards`；成员过滤集中一处；今日页只读能力回执并呈现 F1 模型。触摸与 AI 都经能力总线，F1/F2 实现未改。UI 定时器和模型只在 UI 任务使用，没有服务访问 LVGL，也未新增原生能力、应用注册机制、常驻任务或持久化。

## 全量与内存检查

配置使用总规则第 5 节的字体、Python 和 LVGL 参数，`INK_WERROR=ON`；内存检查只换成 `build-asan` 并加 `INK_SANITIZE=ON`。构建最多三路、CTest 最多两路，逐轮串行。

```sh
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】普通全量：208 项中 204 通过、1 失败、3 跳过，CTest 退出 8，耗时 87.47 秒。唯一失败为 `capability-table-doc`；跳过为 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，原因是缺少锁定中枢依赖。Today 普通／大字号／儿童场景、F1、F2 与阅读确定性检查全部通过。日志：`build/ui-cards-ctest.log`、`build/Testing/Temporary/LastTest.log`。

【实测】ASan/UBSan 全量：同样 204 通过、1 文档失败、3 缺依赖跳过，CTest 退出 8，耗时 197.46 秒。确认 `INK_SANITIZE=ON`、`INK_WERROR=ON`，系统实际编译参数含地址与未定义行为检查；完整 `LastTest.log` 中无 ASan、LeakSanitizer 错误或 UBSan runtime error。定向场景仍为 104 项检查通过、十次变更合并为一次。日志：`build-asan/ui-cards-ctest.log`、`build-asan/Testing/Temporary/LastTest.log`。

【实测】定向场景内部内存最低余量 47,840 字节，内外部内存分配失败均为 0；原资源门槛未变。全量的 `gui-smoke` 使用既有 SDL dummy 路径，无交互窗口。

## 提交与隐私检查

【实测】按总规则第 6 节对基线至 HEAD 的差异执行原隐私检查命令：grep 退出 1，无匹配。`git diff --check` 通过，`git diff --stat` 与产品行数已核对，仅包含任务相关源码、测试、报告及基线后已有的任务书；未提交设计稿、构建产物或本机路径。全部改动已在本地分支分里程碑提交，未推送。
