# backup-pages 执行报告

基线：`558c7ba8604fc321073b943716f49512a7dc3690`。仅本地分支开发，不推送、不碰设备。

本轮主会话复查的修复和最终验证见文末“复查后的修复”；此前的验收数据保留为首次开发记录。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】178 项检查，触摸备份、预览、取消确认、AI 发起恢复、人确认、原文件内容恢复 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^backup-pages$'` |
| 2 | 通过 | 【实测】缺卡错误保留、损坏备份失败、断网可返回、联网后服务继续；失败重试能到保存完成 | 同上 |
| 3 | 通过 | 【实测】六页真实快照、上下文可解析、动作已注册；触摸和 AI 均调用 `backup.now`、`restore.preview`、`restore.apply`；AI 不能批准覆盖 | 同上 |
| 4 | 通过 | 【实测】最终普通和 ASan/UBSan 各 233/233，无跳过；【实现】C 产品代码 208 行，含构建接线 214 行 | 下列两条全量命令；`git diff --numstat 558c7ba..HEAD -- system/boot/main.c system/ui/shell/backup_pages.c CMakeLists.txt` |
| G4 | 六页已对照，均超目标 | 【实测】分数及差异见下表；未改设计稿和阈值 | `INK_DESIGN="$INK_DESIGN" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --reuse --out build/backup-compare --scenes Backup BackupFail Restore RestoreRun RestoreDone RestoreFail` |

反向对照【实测】：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/backup_pages_reverse.py --build build` 通过；替换对象分别撤去 summary 参数、错误保留判断、服务阶段决定页面的判断，三次运行均以 1 退出并命中对应失败断言（分别 1、2、2 个失败）。产品源文件哈希未变；命令和结果留在 `build/backup-pages-reverse/{commands,summary}.json`。

实现与测试源码提交：`e707ee25`。配置沿用[总规则第 5 节](../codex-rules.md#5-构建和测试)全部参数，两套均额外选择 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；内存检查只换为 `build-asan` 并加 `-DINK_SANITIZE=ON`，仍保留 `-DINK_WERROR=ON`。构建与测试均用 `taskpolicy -c utility`，并发均为 2，未启动窗口程序。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测，模拟器】两套新页面流程均通过 178 项检查，`mem.internal_total.min_free_bytes` 均为 45,312 字节（门槛 32,768），内部与外部内存分配失败均为 0。普通全量耗时 214.05 秒，ASan/UBSan 全量 689.02 秒；日志分别为 `build/ctest-release.log`、`build-asan/ctest.log`，页面指标和截图留在两套目录的 `backup-compare/`。内存检查的完整测试日志未发现 ASan/UBSan 错误。

差异和隐私复核【实测】：基线差异仅 9 个相关文件，无构建产物、设计稿或共享设计文档；`git diff --check` 通过。按总规则第 6 节扫描基线至 HEAD，零匹配（grep 退出 1）。只提交本地分支，未推送。

## 没做到的

- 【实现】服务只提供当前家人的最新备份，无历史备份选择、全家汇总和自动备份设置接口；页面只提供最新备份预览，并显示限制。
- 【实现】完成页没有逐项失败清单和各人的阅读位置；当前仅展示服务明确的排除类目，不编造数量。
- 未实测 Linux/gcc、真机或真实家庭服务器。

## 和设计不一致的

- 【实现】只展示当前家人和项目总数，不编造全家人数、分类数量或阅读位置。
- 【实现】失败不承诺整台设备从未改变；恢复服务逐项替换，页面如实说明可能已恢复部分内容。
- 【实现】服务要求恢复后重启，完成页显示提示；未新增重启确认流程。
- 【实现】为避开同时开发的 settings-b 文件，入口复用“全部／工具”的应用注册，ID 为 `backup`。存储页现有行的后续接线列在“要改的文档”。

六页共同差异：设计稿为旧字号、旧浅灰和示例状态栏；实现使用现行主题（灰色为 #888888），没有变更规格迁就分数。状态页复用居中区域，Backup 的图标和标题位置也与画板不同。

| 页面 | 1/4 灰度平均差 | 其它可见差异 |
| --- | ---: | --- |
| Backup | 30.124/255 | 显示服务器已收到的实际时间；自动备份无接口，改为说明；增加恢复入口 |
| BackupFail | 25.495/255 | 截图为真实缺卡失败，不虚构未备份条数 |
| Restore | 37.020/255 | 当前家人一行、真实日期和 36 项；无全家分类数量和设备来源 |
| RestoreRun | 22.200/255 | 进度按实际 checking/restoring，无步骤竖线，末项提示重启；未承诺恢复失败后全部原样 |
| RestoreDone | 38.558/255 | 单人摘要卡无头像；排除类目改用说明文字，无阅读位置或虚构遗漏数量 |
| RestoreFail | 25.358/255 | 多返回顶栏；显示服务校验失败原因，保留取消和重试 |

## 改过的已有测试

- `tests/integration/apps.c` 应用数量断言由 11 精确改为 12，描述加入 backup：新增可见页面入口必然增加一项，未放宽为区间或删除断言。

## 要用户定的

暂无。

## 顺手发现

- 【实测】首轮全量的应用总数旧断言失败，已精确更新；默认 Python 缺中枢依赖导致 3 项显式跳过。改用只读依赖目录现成的 `hub/.venv/bin/python` 配置既有 `INK_HUB_PYTHON` 后，233 项全部通过，没有安装或升级依赖。
- 【实测】初次缺卡失败会被下一轮无变化的状态查询覆盖；现保持错误直至服务阶段变化，反向对照验证有效。
- 【实测】AI 在恢复预览时调用备份曾留下恢复模式；现统一按 collecting/uploading/saved 决定备份页，测试与反向对照均验证。

## 新增的机制

- 页面可见期间的 LVGL 定时器：查询现有 `backup.status`，关闭页面即删除；任务要求显示 AI 和人发起的恢复进度，服务开始恢复时不发布事件，因此不能只靠事件。没有后台重试逻辑或常驻任务。
- 无新能力、新服务状态机、文件格式或配置项。

超过 100 行的原因【实现】：六页共用布局、F1 快照、按钮能力声明、回执处理和页面生命周期。更小的纯文字页会丢画板结构；只监听服务事件会漏掉经确认后开始的恢复；直接调用服务内部函数会绕过确认和成员隔离。复用现有总线与应用注册，服务代码未改。

架构核对【实现】：页面只做展示、快照和能力路由；上传、校验、覆盖、成员归属与确认仍在现有服务和策略层。所有操作均使用既有原子能力，未新增服务内部状态访问。

## 要改的文档

按总规则第 7 节，不直接修改共享文档；页面完成后需主会话更新 `docs/system/ui-dependencies.md` 对应状态。

- 合并时将 `SetStorage` 的备份行从直接 `backup.now` 改接页面入口；该文件由 settings-b 同时开发，本任务未改。

## 复查后的修复

复查基线：`47c4e3f8`。按主会话 `REVIEW.md` 的 BP-L1（中）修复，本轮没有其它未修复条目。修复与测试提交：`d841ec88`。

- 【实现】取消独立备份应用注册；新增任务明确要求的导航能力 `backup.open`，由现有 UI 执行器打开 `Backup`。`SetStorage` 最上面的备份行和 AI 使用同一能力，进入页面只查询状态，立刻备份仍由页内按钮调用 `backup.now`。
- 【实现】导航复用现有页面栈和成员策略；重复打开后仍能直接返回存储页。未改服务、确认流程或文件格式，未新增任务、状态机、重试、缓存或配置项。
- 【实现】测试同步：应用总数精确由 12 改回 11，增加目录不含备份的断言；AI 能力清单列入 `backup.open`。页面流程由存储行进入，新增触摸／AI 导航不触发备份、重复打开后的返回、儿童触摸与 AI 均被拒绝的断言；原备份恢复与失败分支保留。
- 改过的已有测试补充：`settings-a` 原先把点击存储行当成立刻备份；现断言点击后仅打开页面、服务仍为 `none`，返回存储页后由 AI 发起 `backup.now`，保留原有“服务已接受任务、事件刷新页面与 AI 快照”的全部检查。
- 【实测】普通构建通过；`backup-pages`（196 项检查）、`settings-a`、`integration-apps`、`integration-ai-surface` 共 4/4 通过。反向对照 5/5 有效：重新注册独立应用、把存储行改回 `backup.now` 均退出 1 并命中新增断言，原有摘要、错误保留、AI 阶段路由的对照也均退出 1；源文件哈希不变。
- 【实测】最终普通全量和 ASan/UBSan 全量均为 232/233 通过、0 跳过，耗时分别为 268.80、313.51 秒；两种设备档案的 `ui-snapshot-regressions-*` 均通过。唯一失败均为用户明确允许忽略的 `capability-table-doc`（生成文档尚未列入新导航能力），未删除或跳过此测试。首轮额外失败的 `settings-a` 已按上述新行为调整并通过。
- 【实测】两套构建的备份流程均通过 196 项检查，内部总可用内存最低 45,792 字节（门槛 32,768），内部与外部内存分配失败均为 0；内存检查完整测试日志及保留的测试输出没有 ASan、UBSan 或 LeakSanitizer 错误。
- 【实测】历史跳过原因已核查：字体工具 Python 缺 `pytest`、`pytest-asyncio`、`aiohttp`、`PyYAML`；两套构建的 `INK_HUB_PYTHON` 沿用 `$INK_DEPS/hub/.venv/bin/python`，其版本分别为 8.4.2、1.2.0、3.12.15、6.0.3，与锁定依赖一致。中枢测试本轮实际运行通过，无安装或升级依赖，无未满足依赖。

【实现】本轮产品代码差异为 10 行新增、4 行删除；相对任务原始基线新增 221 行、删除 1 行，按增删合计 222 行，低于 300 行预算。只在现有导航注册和存储行接线处修改。

复现命令（沿用原配置和依赖，均以 utility 优先级运行）：

```sh
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/backup_pages_reverse.py --build build
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

证据：`build/recheck-focused.log`、两套构建的 `recheck-build.log` 和 `recheck-ctest.log`、`build/backup-pages-reverse/{commands,summary}.json`，以及两套 `test-out/backup-pages-sim-backup-pages/device/` 的指标与截图。首轮失败日志保留在 `build/recheck-ctest-initial.log`。两次全量 CTest 退出 8，均只因上述文档同步项；构建、定向测试和反向对照命令均退出 0。

差异与隐私复核【实测】：本轮仅 8 个相关文件，`git diff --check` 通过；按总规则第 6 节扫描任务原始基线至提交，无匹配（grep 退出 1）。没有构建产物、设计稿或共享文档夹带。主会话原有未跟踪的 `REVIEW.md` 未改动、未加入提交。本轮没有新增未完成项、设计差异或需用户决定的事项。

要改的文档：主会话合并时重新生成 `docs/system/capabilities.md` 并同步 `docs/system/ui-dependencies.md`；上节关于存储行尚待接线的记录已由本轮修复完成。不直接修改共享文档。
