# factory-pages 执行报告

基线：`2228df7de99de9fea15fddc22868a0de1caae770`。仅模拟器；下文先保留首轮执行记录，最新修复与验收结果见末节“复查后的修复”。以下构建、测试均以 `taskpolicy -c utility` 运行，并行最多 `-j2`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 两组隔离 Flash/卡；逐个核对书、音频及其他内容的 SHA-256；个人照片、录音和备忘已清；远端备份逐字节保留 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build -R "^factory-pages$" --output-on-failure -j2` |
| 2 | 【实测】通过 | 错误密码不写清空标记、文件仍在；AI 不可代确认；孩子不能开页、选择清卡或调用后端 | 同上 |
| 3 | 【实测】通过 | 按两次“先备份”实际提交到模拟服务器，时间递增；显示的分钟随第二次回执更新，无改动时显示已备好；改备忘和新建文件后如实显示未备份 | 同上 |
| 4 | 【实测】流程通过；对照分数超标 | 真实重启后 FactoryDone → 知道了 → FirstBoot；两页快照和截图在 `build/test-out/factory-pages-sim-factory-pages/`；对照 51.608 / 27.647 | 同上；`python tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/factory-compare --scenes FactoryReset FactoryDone --reuse` |
| 5 | 【实测】全量未通过；预算通过 | 普通与 ASan 均为 253 通过、2 失败、3 跳过；无 ASan/UBSan 报错；产品新增 200 行、删除 17 行 | `$INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2`；目录换为 `build-asan` 再跑 |

## 没做到的

验收 5 未通过：生成能力表按规则未更新；备份恢复断电用例第 45 次写入后报告冲突。两套全量日志分别在 `build/factory-full.log`（318.08 秒）和 `build-asan/factory-full.log`（436.42 秒）。

中枢 Python 依赖缺失，原有 CTest 规则自动跳过 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`；未改跳过规则。没有运行 Linux／CI，也没有实机验证。

没有扩展验收外的故障恢复。页面取当前家人的备份状态，未汇总其他家人。`unbacked` 核对当前家人的集合版本、个人文件及清单内已有裸状态文件；清单外新建的裸状态文件尚未逐项核对。

## 和设计不一致的

FactoryReset 对照 51.608/255，FactoryDone 27.647/255，超过 12：使用现行规范的 1.2 倍尺寸、#888888 灰、纯黑文字；原画板是旧尺寸与浅灰。状态栏、备份时间用真实值；完成页不能假称服务器离线或已移除设备，改为请在手机上核对。未为分数改主题。

## 改过的已有测试

`tests/integration/ai_surface.c`：`backup.status`、`system.factory_reset` 从“无参数”改为“参数全可选”（新增 `changes`／`clear_media`）；新增三个 factory 能力及 `factory.media` 必需的 `clear_media`。完整性断言保留，专项复跑通过。

## 要用户定的

暂无。

## 顺手发现

【实测】`backup-restore-recovery` 在第 45 次 Flash 写入掉电后的 `memo.notes` 校验处报“预览后资料有改动”。原基线整套通过；把基线的五个改动编译单元和启动入口重新编译，再使用本次清空生成的数据，仍失败；本次代码使用基线数据则通过。说明新布局触发了原恢复路径的问题，未靠改变掉电位置或断言规避。复现命令：`INK_KEEP_EVIDENCE=1 python tests/factory_pages_baseline.py --build build --current-data build/backup-restore-5wnoc8no`，脚本退出 0：基线整套通过；基线代码＋新数据按预期失败；本次代码＋基线数据通过。证据在 `build/factory-baseline/` 和 `build/factory-baseline-final.log`。新数据目录可由 `backup-restore-recovery` 的失败日志重新取得。未定位更深层原因，也未越过 200 行预算继续改恢复服务。

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

【实测】`INK_KEEP_EVIDENCE=1 python tests/factory_pages_reverse.py --build build`：依次撤掉清卡选择、关于页入口、完成页分流、未备份提示、按实际备份版本核对和家长密码要求，六组都按预期失败（各退出 1，脚本退出 0），产品源码哈希不变。命令与摘要在 `build/factory-pages-reverse/`。


## 验证与收尾

- 【实测】普通与 ASan 的 Factory Pages 均完成 193 条检查；备份时间显示分钟确实更新，未改动时为已备好，改备忘／新增文件后为未备份。两种清空模式分别跨真实模拟进程重启验证。
- 【实测】内部 RAM 最低余量：清空前 45,176 字节，完成页 47,224 字节，均高于 32,768；预算未改。ASan 配置仅在总规则命令基础上改构建目录并加 `-DINK_SANITIZE=ON`。
- 【实测】最终画板对照为 51.608／27.647；PNG、快照 JSON 和指标在 `build/test-out/factory-pages-sim-factory-pages/clear/`，对照图与分数在 `build/factory-compare/`。设计稿和构建产物未提交。
- 【实测】`git diff --check`、新增文件及报告引用路径核对通过。对基线到 HEAD 的差异按总规则隐私表达式扫描：0 匹配（grep 退出 1）。`git diff --stat` 仅任务相关的 16 个文件，共享文档未改。

## 复查后的修复

复查起点：`0a79b043`；主会话复查仅有 FP-H1（高）。以下为本轮记录，前文保留上一轮的实测结果。

- 【实测】修前重跑 `backup-restore-recovery`，稳定在 flash-45 后的 `flash-verify` 失败；证据：`build/factory-review-baseline.log`。
- 【实测】临时编译诊断副本，不改产品源码和断点表：第 45 次 Flash 操作是 `backup.journal.tmp` → `backup.journal` 重命名中的 384 字节 program，40% 时断电；LittleFS 保留上一份 `PH_CHECK/next=5` 日志。重启在 `next=10` 校验 `memo.notes` 时，集合尚未注册，`current_hash` 返回全零，与预览时空集合的有效哈希不同，于是报“预览后资料有改动”。证据：`build/factory-review-trace/flash-cut.log`、`flash-verify.log`。
- 【实现】根因是原有启动顺序：`ink_backup_start()` 立即派发断电恢复，而 `ink_memo_init()` 等集合注册在其后。清空标记多出一字节会改变 LittleFS 布局及后续物理操作分布；断点计数在预览后重新设为 45，并非把前面 wipe 的写入累加进去。
- 【实现】只在 `system/boot/main.c` 把既有 `ink_backup_start()` 移到所有应用初始化之后；不放松成员、预览哈希、版本或确认校验。删掉本任务页面中无用的 `draw` 前置声明，给这一行移动腾出预算；本轮没有新增机制。任务累计产品代码新增 200 行、删除 18 行。
- 【实测】普通构建的恢复全部既有断点（1、8、20、45、75、120）通过；页面测试及三个原来跳过的中枢测试通过，共 6/6（含能力导出夹具）。命令：`INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(backup-restore-recovery|factory-pages|hub-contract|hub-json-crosscheck|hub-simulator-e2e)$' --output-on-failure -j2`；日志：`build/factory-review-targeted.log`。
- 【实测】新增反向验证脚本只在编译副本里撤回启动顺序：撤回本次移动后，原断点表在 flash-45 如期失败；用旧代码制造的同一份断电镜像交给修复后的代码，恢复完成，原有逐项校验全部通过。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/factory_pages_recovery_reverse.py --build build`；脚本退出 0，内部失败用例退出 1；证据：`build/factory-review-reverse.log`、`build/factory-review-reverse/`，含断电镜像 SHA-256。产品源码哈希不变，已有测试、断点表及断言均未改。
- 【实测】原来三个跳过项缺的是所选 Python 环境中的 `aiohttp`、`PyYAML`、`pytest`、`pytest-asyncio`；依赖目录已有完整环境，未安装或升级依赖。两套 CMake 均增设 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，测试设 `PYTHONPATH="$PWD/hub"`，确保导入本检出的中枢代码。已核对 Python 3.11.15、aiohttp 3.12.15、PyYAML 6.0.3、pytest 8.4.2、pytest-asyncio 1.2.0，与锁定版本一致。
- 【实现】补充更正首轮基线证据：旧 `tests/factory_pages_baseline.py` 只替换 `device.c` 中的直接 include，而启动入口实际上经 `product.h` 间接包含，因此“启动入口也已换回基线”的说法不成立。本轮新脚本显式展开该头文件再替换入口；根因依据启动代码核查、修前失败、反向对照及同镜像恢复，不依赖旧脚本。该脚本问题记为顺手发现，未扩展本轮修复范围。

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| FP-H1 | 【实测】已修复，普通和 ASan 的全部既有断点通过 | 1、8、20、45、75、120 各自在断电后恢复完成；完整逐断点输出在两套 `factory-review-tests.log` | 以下全量命令均包含 `backup-restore-recovery` |
| 验收 1～4 | 【实测】回归通过 | 两套页面测试各 193 条检查；保留／清卡、密码与孩子拒绝、备份时间与未备份提示、完成页到开机流程均通过；截图和快照路径沿用首轮 | 以下全量命令均包含 `factory-pages` |
| 验收 5：普通全量 | 【实测】按本轮豁免通过 | 258 项中 257 通过、1 项能力表文档同步失败、0 跳过；222.11 秒；`build/factory-review-full.log` | 下方命令，目录 `build` |
| 验收 5：内存检查 | 【实测】按本轮豁免通过 | ASan/UBSan 同为 257 通过、1 项能力表文档同步失败、0 跳过；540.67 秒；`build-asan/factory-review-full.log`；完整日志无 sanitizer 错误诊断 | 下方命令，目录换为 `build-asan` |

```sh
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

- 【实测】两套 CTest 均退出 8，唯一失败为 `capability-table-doc`（能力表未重新生成）。这是用户本轮明确豁免的文档同步检查，仍实际执行；没有改生成文档、跳过规则、已有测试期望或预算。原有恢复失败和三个跳过项均已处理，无剩余缺失依赖；两套 `hub-contract` 内部各 135 项 Python 测试也全通过，无跳过。
- 【实测】两套完整测试日志另存为 `build/factory-review-tests.log`、`build-asan/factory-review-tests.log`；本轮恢复数据在 `build/backup-restore-6kb1q2ks/`、`build-asan/backup-restore-cvblb7hg/`。页面内部 RAM 最低余量均为清空前 45,176 字节、完成页 47,224 字节，预算仍为 32,768 字节。
- 【实现】架构审视：在启动入口满足集合注册这一既有依赖，恢复仍由备份服务执行；能力、成员策略、页面快照及 AI 调用路径未增加分支。本轮新增机制：无。没有改存储格式或断点表，没有用写入次数调整避开故障。
- 【实测】收尾检查：本轮仅 4 个相关文件，任务累计产品新增 200 行、删除 18 行；`git diff --check` 通过，基线到最终差异的隐私扫描为 0 匹配（rg 退出 1）。未提交构建产物、设计稿或主会话提供的未跟踪 `REVIEW.md`；未推送、未运行真机或 Linux/CI。
