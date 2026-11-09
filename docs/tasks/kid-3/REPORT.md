# kid-3 执行报告

基线：`2228df7de99de9fea15fddc22868a0de1caae770`。状态：实现、定向回归完成；全量及内存检查进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | `kid-3` 检查 KidHome、KidToday、KidApps；大人 Main、Today、Apps 的名称和入口保留 | 下文定向命令 |
| 2 | 【实测】通过 | 两页应用均无禁止入口；白名单能力可用；用时耗尽后目录恰好两项且电话、短信能打开；说明完整显示 | 下文定向命令 |
| 3 | 【实测】通过 | 大人两次改变 TodayEdit，孩子故事、报纸不受影响；`fw-f8` 继续验证孩子独立选择的持久化 | 下文定向命令 |
| 4 | 【实测】截图、同能力通过；画板差值超标 | 三页稳态截图；主页和全部触摸、AI 共用 `shell.launch`，今日卡片共用提供方能力 | 下文定向、画板命令 |
| 5 | 待验证 | 全量、内存检查、产品代码预算 | 待补 |

## 没做到的

- 最终全量测试、内存检查待补。
- 未改阅读器已有缺口：【实现】`apps/reader/reader.c:495` 返回主页时释放阅读状态，`:529` 只在正文仍打开时提供阅读卡片。本任务的成员阅读卡片隔离用合成提供方验证，不能据此宣称真实阅读器回主页后的卡片已经可用；真实服务没数据时显示空状态。

## 和设计不一致的

- 使用规范第 1 节现行字号、孩子 110% 和灰度；导出的画板仍是旧尺寸和浅灰。三屏都没有达到 12/255，未缩小规范尺寸或改灰度来压分：KidHome 51.766，KidToday 47.332，KidApps 39.221。
- KidHome：现行大字和深灰图标、Dock，真实日期/用时与示例不同；为避免用时与图标重叠，孩子的时钟区域上移。KidToday：现行大字、深灰 Dock、真实卡片和空状态，卡片内部没有服务未提供的章节、百分比和播放章数。KidApps：现行尺寸下分两页，工具在第二页；画板的一屏十二个入口未照搬为小图标。
- 真实阅读器未提供卡片时，今日首卡为空状态，未照画板填入示例书名和进度。

## 改过的已有测试

- `tests/ui_cards.c` 一处页面名断言：孩子由 `Today` 改为任务要求的 `KidToday`，大人仍断言 `Today`；数据、版本、内存和动作断言不变。
- `tests/ui_shell_scenes.c`：孩子三页的画板名改为 KidHome、KidToday、KidApps；孩子今日第五格按画板改查真实用时文本，其他四格及空动作断言保留，大人的五格不变。
- `tests/ui_shell_regressions.c`：成员切换后的孩子主页由 Main 改查 KidHome；成人查询、延迟回执、输入内容的隔离断言不变，仍在两个设备档案运行。

## 要用户定的

- 暂无。

## 顺手发现

- 阅读器服务卡片的生命周期缺口见“没做到的”；未扩大本任务修改范围。

## 新增的机制

- 无新增任务、线程、定时器、状态机、文件格式、配置项或服务接口。剩余用时复用 `member.time_get`、现有 UI 回调和时间刷新；页面操作沿用已有能力。

## 验证记录

- 【实测】`taskpolicy -c utility cmake --build build -j2` 通过，`INK_WERROR=ON`。
- 【实测】定向命令：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(kid-3|kid-2|fw-f8|ui-cards|ui-scale)$'`，5/5 通过。
- 首次全量 253/258 无失败预算的项目完成（含 3 项原有跳过）；五项失败均是上述旧页面名或孩子第五格期望。更新后第二轮 255 通过、3 项中枢测试因 Python 依赖缺失跳过、0 失败。已找到只读依赖目录中的锁定中枢环境，以现有 `INK_HUB_PYTHON` 指定，最终结果待补；没有安装或升级依赖。
- 【实测】补查孩子主页正在进行的事：通过真实任务服务建立孩子作业，活动行保留，用时行让位，日期不进入状态栏；只有空闲时才上移孩子主页。最新 `tests/kid_3.py` 三轮各 188 条检查通过，内部 RAM 最低余量均为 47,224 字节（原下限 32,768）。
- 截图和快照：`build/kid-3-evidence/{KidHome,KidToday,KidApps}/`；拼接对照图和 `scores.json` 在其父目录。已目检并修复孩子主页用时与图标重叠，新增断言防回归。构建目录不提交。
- 【实测】反向命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/kid_3_reverse.py --build build`。基线导航替换对象后 22 条断言失败；将今日配置错误地统一读大人后 7 条失败；撤掉空闲布局判断后 2 条失败。三轮都运行到 `test.finished`；未修改工作源码，摘要在 `build/kid-3-reverse/summary.json`。
- 【实测】画板命令：先用 `INK_KEEP_EVIDENCE=1` 运行 `tests/kid_3.py --build build`，再运行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/test-out/kid-3-sim-kid-3 --scenes KidHome KidToday KidApps --reuse`。每屏有一份已核实的稳态帧，分数如上；已另存证据供本次复核。
- 【实测】产品源码差异 91 行新增、8 行删除，合计 99 行；另有 CMake 测试登记 4 行，即保守计算 103 行，低于 250 行预算。不能更小：只改页名、删除三个占位入口的方案无法覆盖注册应用、能力白名单、用时耗尽、卡片说明及剩余用时；过滤集中在一个孩子入口判断中，没有逐应用补丁。

## 要改的文档

- 合并时更新 `docs/system/ui-dependencies.md` 孩子三页状态，保留真实阅读卡片缺口；按总规则第 7 节，本分支不改共享文档。
