# clock-alarm 任务报告

日期：2026-11-09。基线：`d0cacd1d4b348a8e885904026f3796a10996c646`。工作分支：`claude/codex-clock-alarm`。

## 复查后的修复

- 本轮按用户补充要求补齐编辑、开关，处理 CA-R1-01～04。沿用原闹钟 ID 和 schema 1，新增的小服务入口同时供页面和 AI 调用；不增加线程、持久格式或后台机制。
- 【实现】CA-R1-01：增加 `alarm.update` 和内部 `ink_time_alarm_update_for` 小接口；修改、启停均按原 ID 写原记录，更新既有调度及 `alarm.revision`。详情复用新建表单，列表开关直接走同一个原子能力；成员切换后不能修改别人的闹钟。
- 【实现】CA-R1-02：名称输入字节上限为记录容量减结尾，复用输入服务按完整 UTF-8 字符截取；保存沿用能力的 15 字符验证，失败在原表单显示中文提示，AI 快照包含同一提示。CA-R1-03：两个减号改用已有字库的 ASCII `-`，不改字体管线。
- 【实测】CA-R1-04：保留快照、持久记录断言，新增直接检查可见 LVGL 时间／名称／重复标签、真实触摸行进入详情。最终普通场景创建 379、重启 31 项断言全过；覆盖页面新建、编辑、删除、列表及详情启停、三种重复规则、16 ASCII 拒绝并可见提示、15 汉字保存、16 汉字不切半、响铃操作及成员隔离。最终截图位于 `build/test-out/clock-alarm-69k5r1u2/create/` 的 `snap-edit.png`、`snap-name-error.png`、`snap-scene.png`；已核对新增表单及提示，无重叠或减号方框。
- 【实测】最终反向对照命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/clock_alarm_negative.py --build build`，退出 0。分别撤掉输入快照、编辑、列表开关、错误提示、字节上限、减号修复、列表标签，变体均编译成功并以非零退出，分别捕获 12／15／5／2／1／5／74 项失败；只撤掉更新写盘时，创建轮 379 项通过、重启轮 6 项失败。各轮均完成，未改正常源码、可执行文件或断言。详情见 `build/clock-r2-negative-final.log` 和 `build/clock-negative/summary.json`。
- 【实现】列表开关的点击区域采用完整的 125×101 控件，不只绑定 125×67 的可见轨道，新增断言检查不小于 82×82。开关与行详情分别响应各自区域，页面位置和字形不变。
- 【实测】首轮全量发现 `integration-confirm` 整份清单超过测试的固定 32,768 字节；本次启动导出清单压紧后为 33,058 字节。该测试改走现有分页接口，逐页检查确认／拒绝不可见，同时保留查询／撤回必须可见的断言，没有扩大缓冲或资源预算。`services-mid-capability-doc` 缺新增的 `alarm.update`，已在服务设计能力表补一处声明，不改生成的共享总表。
- 【实测】产品代码保守合计 279 行：页面 230、boot 3、CMake 5、时间服务 21、能力 19、内部声明 1；未超过 300。编辑和启停原来没有服务入口，需补服务及能力接线；只恢复表单按钮不能保存，删除重建会改变 ID，因此没有采用。没有新线程、文件格式、重试、缓存、队列或配置项。普通／Sanitizer 全量检查进行中；以下阶段证据保留首轮背景，最终结果以本节为准。
- 【实现／实测】架构审视：页面只调用能力，不写服务内部数据；更新属于时间服务的原子操作，按钮和 AI 都走 `alarm.update`，持久记录及调度仍归时间服务。详情草稿不会被周期性的 `alarm.revision` 刷新覆盖；F1／F2／F4、3072 字节 AI 快照和成员隔离均由场景检查。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 页面触摸新建、编辑、删除、列表及详情启停；原 ID 不变，重启读取修改后的时间、名称和关闭状态 | 定向命令 |
| 2 | 【实测】通过 | 每天 127、工作日 62、自选周日／周六 65；显示、原始记录、重启读取均断言 | 定向命令 |
| 3 | 【实测】通过 | 实际响铃，触摸稍后响保存十分钟，AI 调原能力关掉；页面与 AI 的 3072 字节快照一致 | 定向命令 |
| 4 | 【实测】普通完成，Sanitizer 待完成 | 普通全量 217/218，唯一失败为用户允许忽略的生成能力表同步；产品 279 行 | 完整验证命令 |

## 没做到的

- 共享 `docs/system/capabilities.md` 按用户补充指示留给合并主会话生成；本轮完整运行了对应检查，未跳过或放宽。编辑、开关缺口已补齐，见[时间能力表](../../../system/services/time/time_caps.c)及[内部接口](../../../system/services/time/time_internal.h)。
- 尚未运行 Linux／gcc 和实机；不操作真机，不推送或开 PR。

## 和设计不一致的

- 本轮 Clock 对照 43.577/255，ClockRing 为 35.467/255，均超过 12。Clock 未放任务排除的“计时”入口；下一个闹钟只显示真实时间；增加 F4 页码条。详情现已复用表单支持编辑和开关；字号、比例、灰色均取当前主题，不为分数改规格。ClockRing presenter 本任务未改。
- 【实测】逐图核查：Clock 三张卡片随当前主题放大，上方没有画板中的“明早／还有多久”，修改后时间为 8:31，周末闹钟已关闭；ClockRing 原 presenter 没有画状态栏，图标更大、时间字号较小；它的旧场景实际为 22:47、11 月 4 日，画板是 7:30、11 月 5 日。未改真实时间或主题来压低分数。对照图在 `build/clock-r2-compare/`。

## 改过的已有测试

- `tests/integration/apps.c`：精确应用数量 6 → 7，新增时钟应用；并追加其初始状态应为 stopped 的断言，原有状态检查保留。
- `tests/integration/ai_surface.c`：在 AI 能力白名单末尾追加七个时钟能力；open 必须有 id，draft 参数可选，其余无需参数。没有删除断言、放宽资源预算或跳过测试。
- 本轮追加 `alarm.update`，要求 `id`；`tests/clock_alarm.c` 原“详情没有保存能力”断言改为必须同时有保存、开关及删除，因为补齐了编辑。持久断言改为核对修改后的时间、名称和关闭状态，原三种重复规则检查保留；其余为增加可见文字、触摸和输入边界断言。`clock_alarm.py` 仅更新通过说明。
- `clock_alarm_negative.py` 扩展现有独立对象变体；原固定“四次失败”改为逐变体要求编译成功、测试完成且非零退出／断言失败，持久化变体还要求创建轮通过、重启轮失败。场景增加了输入次数，不再是原来的四次；正常场景仍严格要求零失败，没有放宽预算或断言。
- `tests/integration/confirm.c`：整份清单检查改为逐页检查，每页仍禁止 `confirm.accept`、`confirm.reject`；跨页仍须找到 `confirm.list`、`confirm.withdraw`，缓冲保持 32,768 字节。原因及溢出证据见本轮修复节。

## 要用户定的

- 暂无；任务内可逆选择按最保守方案处理。

## 顺手发现

- 新增能力后，生成的能力目录需要主会话统一更新；依任务总规则第 7 节，本任务不修改该共享文件。

## 新增的机制

- Clock 列表、新建表单及详情页：任务书要求；复用 F4、F2、F1 和时间服务，新增七个页面导航／草稿／保存能力，底层写入仍走 `alarm.set`／`alarm.cancel`。
- 本轮补充用户明确要求的编辑和开关：小接口 `alarm.update` 更新同一记录，沿用原数据表、成员检查、调度和 UI 执行器；这是任务要求的原子操作，没有新增持久机制。
- 无新产品任务、线程、状态机、持久格式、配置项、缓存或队列；只保留三条可见记录和当前表单，位于 PSRAM。测试驱动任务仅在测试程序内。

## 要改的文档

- 完成后由主会话更新 backlog、ui-dependencies 和生成的能力目录；本任务不改共享文档。

## 首轮阶段证据（历史）

- 【实测】`taskpolicy -c utility cmake --build build -j2` 退出 0；开启 `INK_WERROR=ON`，没有放宽任何预算。定向测试退出 0，包含两个独立进程的保存／重启读取。
- 【实现】页面不写服务内部状态；触摸和 AI 都经注册能力，响铃继续使用既有系统打断卡；`alarm.revision` 通过既有 UI 执行器刷新页面，LVGL 只在 UI 任务调用。
- 【实测】首轮定向检查发现键盘打开时快照超出 AI 的 3072 字节缓冲；输入期间的操作归键盘，表单快照在这时不重复列出不可执行的页面按钮，完整字段及真实输入会话仍可读。十分钟起点按实际点击前后虚拟时间界定，未新增 sleep。

定向命令（`INK_DEPS` 指用户给定的依赖检出）：

```bash
taskpolicy -c utility cmake --build build --target sim-clock-alarm -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^clock-alarm$' --output-on-failure -j2
```

日志：`build/clock-build.log`、`build/clock-targeted.log`；截图、模拟闪存及两轮结果在忽略目录 `build/test-out/clock-alarm-*/`。

## 首轮反向对照（历史，当前脚本已扩展）

- 【实测】仅在独立编译对象中撤掉“输入期间不重复列出页面动作”的修复；编译、链接均为 0，原测试程序退出 1，162 项通过、4 项失败、finished=1。四次名称输入均触发 AI 快照缓冲断言，未改原源码、断言或正常可执行文件；每轮使用独立输出目录，避免读取旧指标。最终复跑同样成立，日志及指标在 `build/clock-negative/`，命令输出见 `build/clock-negative-final.log`。
- 可复现命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/clock_alarm_negative.py --build build`。

## 首轮规模与验证（历史）

- 【实测】产品源文件 220 行，boot 注册 3 行，CMake 共 5 行（含测试接线也计入），保守合计 228 行，不超过 300。超过 100 行是因为包含列表、表单、详情、F1／F2／F4 接线和七个能力声明；看过只接 `alarm.set` 的更小方案，但它不提供新建表单、分页和同源快照。未增加时间服务实现。
- 【实测】普通全量首轮 215/218；更新上面两份固定清单后，最终重跑为 217/218，CTest 退出 8，409.81 秒，无跳过。仅 `capability-table-doc` 因共享目录过期失败；单独生成到 `build/clock-capabilities.md` 并 `--check` 通过，265 条定义。最终配置／构建参数不变，日志为 `build/clock-build-final.log`、`build/clock-ctest-final.log`。
- 【实测】ASan／UBSan 的配置和完整构建退出 0；已核对产品及场景测试编译参数含 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`，全量启用 `UBSAN_OPTIONS=halt_on_error=1`。217/218，退出 8，368.31 秒，无跳过；唯一失败同为 `capability-table-doc`，没有 Sanitizer 错误。日志：`build-asan/clock-build.log`、`build-asan/clock-ctest.log`。
- 【实测】新场景创建轮 166 项断言通过，重启轮 24 项通过；内部 RAM 最低余量分别为 48,192 和 47,840 字节，原门槛 32,768；内外部内存分配失败均为 0。`ui_scale_check.py system/apps/clock/clock.c` 通过。
- 【实测】最终 ASan／UBSan 定向复跑退出 0，创建／重启仍全部通过；日志为 `build-asan/clock-targeted-final.log`。
- 【实测】对照图及分数在 `build/clock-compare/`。命令为 `tools/ui_compare.py --build build --out build/clock-compare --reuse --scenes Clock ClockRing`；Clock 截图和指标直接复制自成功验收场景，ClockRing 由该工具的正常场景入口生成。

完整验证命令：配置使用总规则第 5 节全部依赖参数；额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 使用已有、只读的服务器测试依赖。ASan 目录只另启用 `-DINK_SANITIZE=ON`，未更改其他设置；未安装系统或 Python 包，CMake 仅在忽略的构建目录按仓库固定版本准备 SDL2／ImGui，未升级依赖。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
export UBSAN_OPTIONS=halt_on_error=1
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

## 首轮最终检查（历史）

- 【实测】基线至最终提交共九个任务相关文件；产品代码保守计数仍为 228 行，未夹带共享文档、设计稿或构建产物；`git diff --check` 通过，报告的两个本地链接均存在。
- 【实测】按总规则第 6 节原样扫描基线至 HEAD 的提交差异：零匹配，grep 退出 1；没有本机绝对路径、MAC 或其他实际身份数据。
- 本地提交，未推送、未开 PR、未碰设备。验收 1 部分完成、验收 4 未全通过，不能把本报告当作全部功能验收通过。
