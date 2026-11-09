# factory-pages 执行报告

基线：`2228df7de99de9fea15fddc22868a0de1caae770`。仅模拟器；页面、后端增量与专项验收已完成，反向对照已完成；全量复核与内存检查进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 两组隔离 Flash/卡；逐个核对书、音频及其他内容的 SHA-256；个人照片、录音和备忘已清；远端备份逐字节保留 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build -R "^factory-pages$" --output-on-failure -j2` |
| 2 | 【实测】通过 | 错误密码不写清空标记、文件仍在；AI 不可代确认；孩子不能开页、选择清卡或调用后端 | 同上 |
| 3 | 【实测】通过 | 按两次“先备份”实际提交到模拟服务器，时间递增；显示的分钟随第二次回执更新，无改动时显示已备好；改备忘和新建文件后如实显示未备份 | 同上 |
| 4 | 【实测】流程通过；对照分数超标 | 真实重启后 FactoryDone → 知道了 → FirstBoot；两页快照和截图在 `build/test-out/factory-pages-sim-factory-pages/`；对照 51.629 / 27.647 | 同上；`python tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/factory-compare --scenes FactoryReset FactoryDone --reuse` |
| 5 | 待验证 | 全量、ASan、200 行产品代码预算 | 待补 |

## 没做到的

全量尚未全过：生成的能力总表按规则未更新；备份恢复断电用例第 45 次写入后报告冲突。ASan 和最终全量复核进行中。

未扩展验收外的故障恢复。`unbacked` 核对当前家人的集合版本、个人文件及清单内已有裸状态文件；清单外新建的裸状态文件尚未逐项核对。

## 和设计不一致的

FactoryReset 对照 51.629/255，FactoryDone 27.647/255，超过 12：使用现行规范的 1.2 倍尺寸、#888888 灰、纯黑文字；原画板是旧尺寸与浅灰。状态栏、备份时间用真实值；完成页不能假称服务器离线或已移除设备，改为请在手机上核对。未为分数改主题。

## 改过的已有测试

`tests/integration/ai_surface.c`：`backup.status`、`system.factory_reset` 从“无参数”改为“参数全可选”（新增 `changes`／`clear_media`）；新增三个 factory 能力及 `factory.media` 必需的 `clear_media`。完整性断言保留，专项复跑通过。

## 要用户定的

暂无。

## 顺手发现

【实测】`backup-restore-recovery` 在第 45 次 Flash 写入掉电后的 `memo.notes` 校验处报“预览后资料有改动”。原基线整套通过；把基线的五个改动编译单元和启动入口重新编译，再使用本次清空生成的数据，仍失败；本次代码使用基线数据则通过。说明新布局触发了原恢复路径的问题，未靠改变掉电位置或断言规避。证据：`build/factory-baseline/backup-restore.log`、`build/factory-restore-probes/baseline-on-new-flash/`、`build/factory-restore-probes/current-on-baseline-flash/`。可复现脚本为 `tests/factory_pages_baseline.py`；最终命令和执行结果待补。

## 架构审视

【实现】界面只调用能力；清空留在系统策略和文件服务，备份核对留在备份服务。新操作均在能力表中，按钮与 AI 共用；页面快照含家人、清除范围、开关、备份状态和动作。密码与确认继续由现有策略把关，快照不含密码及文件正文。

## 新增的机制

- 既有清空标记加 1 字节，携带可选 `clear_media`；任务书要求选中才清书和音频，旧空标记按不选处理。
- `backup.status` 增加可选 `changes`，返回 `unbacked`（无法核实时为 null）；任务书要求真实备份状态。复用已有清单、集合版本、文件索引与哈希。集合按实际备份清单版本比较：活动记录虽被备份但不更新同步确认版本，不能用同步版本代替备份版本。该误报已由新增的无改动断言捕获并修正。
- 沿用 UI 定时器和能力表；没有新任务、线程、状态机、缓存或队列。
- 【实现】新增产品代码 200 行（含 CMake 接线、注释和空行），删除 17 行。超过 100 的原因：两张页面加清卡参数与备份状态缺口；看过直接复用备份页／在关于页直接调用后端，都无法满足两张指定画板和清空范围审阅。

## 要改的文档

按总规则第 7 节不改共享文档：

- 合并时更新 `docs/system/ui-dependencies.md` 中 FactoryReset／FactoryDone 的实际状态。
- 运行 `python tools/capabilities.py` 统一生成 `docs/system/capabilities.md`：能力从 348 项变为 351 项，增加 `factory.open`、`factory.media`、`factory.finish`，两个已有能力增加可选参数。该规则导致本分支 `capability-table-doc` 仍失败，未跳过或放宽检查。

## 反向对照

【实测】`INK_KEEP_EVIDENCE=1 python tests/factory_pages_reverse.py --build build`：依次撤掉清卡选择、关于页入口、完成页分流、未备份提示和家长密码要求，五组都按预期失败（退出 1），产品源码哈希不变。命令与摘要在 `build/factory-pages-reverse/`。
