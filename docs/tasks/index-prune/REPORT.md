# index-prune 执行报告

日期：2026-11-10。基线：`32cff5a9ee114315fd7cbd13a1453c7a4cbd8ff8`。三项验收全部通过；普通及 ASan/UBSan 全量各 299/299，S31 编译通过。仅本地提交，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过【实测，模拟器】 | 60 条删除 20 个、跨根目录改名 5 个后为 40 条；改名保留编号，删除编号从数据库和 lookup 消失，缓存失效。公共区扫完、成员区没扫完时拔卡，原 60 条和缓存全部保留；空替换卡不清理原卡记录。真实进程重启后结果不变 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_prune/run.py --build build` |
| 2 | 通过【实测，模拟器】 | 1024 条满索引，电脑删除 20 个并加入 12 个后，自动完成并得到 1016 条；新编号 1025～1036，重启保持。8 阶段共 411 个断言通过 | 同上 |
| 3 | 通过【实测】 | 普通及 ASan/UBSan 全量各 299/299，均退出 0、0 跳过，耗时 220.26 / 385.34 s；完整输出无 sanitizer 错误。S31 的 GNU 15.2.0 编译、链接、尺寸及栈报告退出 0 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存版换 `build-asan` 并设 `UBSAN_OPTIONS=halt_on_error=1`；`taskpolicy -c utility bash ports/esp32s31/build.sh` |

## 没做到的

- 未运行 Linux/远端 CI；按任务约束未连接设备。本次验收项无遗留。

## 和设计不一致的

- 无本次新增差异。【实现，架构审视】清理是文件服务内部索引维护；用户和 AI 继续共用既有文件能力、状态、事件和成员鉴权，无需新增用户操作或能力接口。
- 基线已有一处数值差异：`docs/system/system-services-design.md` 后台索引表写每批 32，基线 `INDEX_BATCH` 为 16。本次沿用既有 16，不改规范或扩大批次，交主会话核对。

## 改过的已有测试

- 无。

## 要用户定的

- 无。

## 顺手发现

- 无。

## 新增的机制

- 产品无新增任务、状态机、持久格式、配置项、缓存或队列。任务书要求的清理复用原扫描执行器、16 条批次、数据库删除、卡上脏标记和落盘完成标记；只为既有扫描增加临时游标与满索引/已清理标志。
- `prune-test` 为验收用模拟任务，不进入固件。

## 要改的文档

- 主会话可在 `docs/system/system-services-design.md` 的后台建索引说明补充：所有根目录完成后分批清理同卡缺失路径；满索引时先完整遍历和清理，再补扫被延后的新路径；编号不复用。另核对既有 32/16 的批次数值差异；本次未修改共享文档。

## 过程与检查

- 【实测】开工时工作区干净，HEAD 与指定基线相同，分支为 `claude/codex-index-prune`。
- 产品代码预算：增删合计不超过 100 行，仅修改 `system/services/file/file_service.c`。
- 构建与测试使用 utility 优先级，并行度最多 2；没有同时运行多轮全量或多个构建。
- 配置采用[总规则 §5](../codex-rules.md#5-构建和测试) 的字体、Python、LVGL 参数，两套都开 `INK_WERROR`，内存版加 `INK_SANITIZE=ON`；另用 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 复用锁定依赖，未安装或升级依赖。已核对实际编译参数，内存版确含 `-fsanitize=address,undefined`。构建命令：`taskpolicy -c utility cmake --build build -j2`，内存版换 `build-asan`。
- 初始合成文件的大小、时间戳各不相同，5 次跨根目录改名可由既有元数据规则唯一识别；没有改动改名算法。
- 【实现】产品代码仅改文件服务，39 行增加、3 行删除，共 42 行，低于 100 行预算；不改上限、记录格式和存储服务。
- 【实测】最终批次最大编辑锁持有：正常清理 38.474 ms、拔卡恢复 40.863 ms、换卡恢复 40.834 ms、满索引恢复 48.782 ms，均低于沿用的 250 ms 检查线。
- 【实测】初版功能断言通过，但交错查路径与提交删除导致多次 Flash 写入，锁内累计 314.644 ms。Trace 核实后改成先查一批路径，再集中提交这一批删除；没有改预算、断言、存储接口或增加 sleep。测试预置缩略图的解码在锁外完成，耗时统计从初始建索引结束后开始，仅统计本轮重扫及验证。
- 【实测】反向对照只在忽略的构建目录取基线文件服务并重新编译链接，其余测试及产品文件相同。四种场景分别出现 22、22、21、35 个预期失败，失败限于缺失记录、缩略图和满索引恢复；拔卡不删除、换卡隔离及改名保号仍通过。命令：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_prune/run.py --build build --baseline 32cff5a9ee114315fd7cbd13a1453c7a4cbd8ff8`。ctest 的正常入口不依赖 Git 历史。
- 最终定向证据：`work/index-prune/targeted-final.log`、`negative-final.log`；结构化结果与重启镜像在 `build/test-out/index-prune/`、`index-prune-baseline/`，通过 `INK_KEEP_EVIDENCE=1` 保留。
- 【实测】未修版本 `fea31201` 的首轮普通及 ASan/UBSan 全量均 298/299，退出 8，耗时 261.93 / 402.45 s。唯一失败：`file-index-3` 的 busy 场景，第 95～96 行要求 5 秒内提交写盘并让其他 BG 工作执行。【实现】清理完成前一并推迟了原有异步提交；现将“可提交写盘”和“清理已完成”分开判断，所有根目录扫完即照常提交，ready 仍等清理及落盘完成。不改已有测试、等待或断言。
- 【实测】修正后 `file-index-3`、`index-prune`、`index-durable`、`index-compact` 共 4/4 通过，退出 0，11.82 s。该时序修正的反向证据就是前述两套未修版本的原断言失败；只增加一个局部判断后原测试通过。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(file-index-3|index-prune|index-durable|index-compact)$' --output-on-failure -j2`；日志 `work/index-prune/focused-final.log`。
- 【实测】S31 编译与链接退出 0，GNU 15.2.0；生成尺寸及栈报告。Flash 静态占用 1182584 B，内部 DIRAM 83042 B，外部静态 BSS 507808 B；这是编译结果，不是实机内存或栈高水位。证据：`work/index-prune/s31-build.log`、`build-s31/size.txt`、`build-s31/stack.txt`。未连接设备。
- 【实测】最终普通及内存全量日志：`work/index-prune/ctest-final.log`、`ctest-asan-final.log`；均 299/299、0 跳过。另检查内存版完整 `LastTest.log`，没有 AddressSanitizer/UBSan 错误报告。实现提交为 `fea31201`、`d1b54fa5`。
- 【实测】结束检查：基线以来仅 6 个相关文件；范围、差异格式、报告链接、单文件大小检查通过。隐私扫描无匹配，grep 退出 1；没有绝对本机路径、MAC、构建产物、设计稿或受保护文档入库。仅本地提交，未推送。
