# index-compact 执行报告

日期：2026-11-10。基线：`5e1e3964b5aafae3587b805c2edcf13922cccd44`。工作分支 `claude/codex-index-compact`，开工工作区干净。验收 1、2 通过；验收 3 未通过，普通及 ASan/UBSan 全量均有 4 项回归，S31 编译通过。全部只本地提交，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过【实测，模拟器】 | 下表及 [before.csv](before.csv)、[after.csv](after.csv)；重新从基线源码编译的汇总、逐次 CSV 与开工测量完全一致 | 下文计量命令 |
| 2 | 通过【实测】，普通全量已复核 | schema 1/2 的 6 个种子、升级、重启阶段共 50 个检查通过，编号均为 `[2, 1, 3]`；旧记录迁移前即可查询；重启目录读取、索引写盘均为 0；155 B 中文路径持久化回读；换回基线后 6 阶段均预期失败 | `ctest --test-dir build -R '^(index-compact|index-durable|file-index-3)$' --output-on-failure -j2`；下文反向命令 |
| 3 | 未通过【实测】：普通和 ASan/UBSan 全量均 285/289；S31 编译通过 | 两种全量均 4 项失败、0 跳过，分别 334.45 / 444.78 s，均退出 8；57 份内存版日志 sanitizer 扫描 0 匹配；S31 完整构建退出 0 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存版换 `build-asan` 并设 `UBSAN_OPTIONS=halt_on_error=1`；`taskpolicy -c utility bash ports/esp32s31/build.sh` |

## 计量与实现

同一工具、`metalio-plus`、种子 1、`tail` Flash 时序，关闭随机断电。每组为 1000 个合成文件首次建索引；耗时均为虚拟时间，单位 ms。`prepare` 测首次追加和压缩；`probe` 在压缩已被存储任务取走后提交另一集合的同步写盘；`control` 是空卡同内容写盘。没有改存储服务、数据帧、队列、调度或测量断言。

| 测量项【实测，模拟器】 | 基线 | schema 3 |
| --- | ---: | ---: |
| 首次追加字节 | 252000 | 92000 |
| 首次追加耗时 | 4614.425 | 1602.159 |
| 压缩字节（含快照头） | 252032 | 92032 |
| 压缩耗时 | 4426.503 | 1659.688 |
| 竞争探针的压缩耗时 | 4428.007 | 1660.259 |
| 等待任务排队与索引写盘重叠 | 4427.878 | 1660.130 |
| 等待任务全部排队时间 | 4428.110 | 1660.362 |
| 同步等待总耗时 | 4442.595 | 1683.199 |
| 空卡对照与索引重叠 | 0 | 0 |

字节量减少约 63.5%；竞争时索引直接造成的排队减少约 62.5%，仍约 1.66 秒，不能宣称停顿消失或等于真机结果。

【实现】产品代码只改 `file_service.c`，55 行增加、19 行删除，共 74 行；另有 4 行 CMake 注册测试。磁盘 payload 为卡身份 16 B、大小 8 B、修改时间 8 B、UTF-8 路径及结尾零。读入口统一解码，写入口统一编码，共用路径长度/终止符/内容区前缀校验，另检查 schema 与精确长度。元数据通过 `memcpy` 读写，避免未对齐访问。schema 1/2 均读取固定 216 B；原编号交给既有扫描更新，schema 2 的改名匹配继续使用元数据。完成记录的固定 payload 和原有落盘流程保留，版本随索引升为 3，使旧完成标记触发既有后台补建。

155 B 是前次报告给出的映射后路径上界；本次用 155 B UTF-8 路径直接验证同一编解码与真实持久化，payload 恰为 188 B，进程重启后经扫描和单条读取均能取回，不把它称为长度上限为 128 B 的公开虚拟路径。

计量和反向复现（均先按总规则 §5 配置、构建；依赖只读，最多并行 2）：

```bash
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_cost/run.py --build build --baseline 5e1e3964b5aafae3587b805c2edcf13922cccd44 --cases prepare probe control
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_cost/run.py --build build --cases prepare probe control
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/run.py --build build
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/run.py --build build --baseline 5e1e3964b5aafae3587b805c2edcf13922cccd44
```

【实测】定向测试 3/3，退出 0。反向对照只在构建目录替换文件服务，基线 6 阶段失败数为 `1, 2, 2, 1, 2, 2`，仅变长布局及升级后 schema/长度断言失败；工具确认这些预期失败后退出 0。ctest 新测试直接构造旧 payload，不依赖 Git 历史。原始日志在 `work/index-compact/`，保留的重启镜像和指标在 `build/test-out/index-compact/`。

【实测】普通及内存版的 6 个新回归阶段均通过，各 50 个检查；既有 `index-durable`、`file-index-3` 在两轮全量中也均通过。配置沿用总规则 §5 字体、Python、LVGL 参数，增加 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 复用中枢锁定依赖，内存版只换目录、加 `-DINK_SANITIZE=ON`，两版都开 `INK_WERROR`。构建命令是 `taskpolicy -c utility cmake --build build -j2`（内存版换目录），均退出 0；没有安装或升级依赖。

【实测】S31 使用既有 ESP-IDF 和 GCC 15.2.0，完整编译、链接并生成尺寸及栈报告，退出 0；Flash 静态占用 1179504 B、内部 DIRAM 83042 B、外部静态 BSS 507648 B，非实机内存测量。证据：`work/index-compact/s31-build.log`、`build-s31/size.txt`、`build-s31/stack.txt`。仅构建，没有连接设备。结束时清理未引用的生成图片、镜像和二进制，保留日志、JSON、索引及失败场景证据。

【实测】结束检查：基线至 HEAD 共 12 个相关文件，产品与 CMake 增删合计 78 行，在 180 行预算内；差异格式、报告链接、单文件大小与范围检查通过。总规则 §6 的基线至 HEAD 隐私扫描 0 匹配（grep 退出 1）；没有构建产物、设计稿或受保护的共享文档入库。本地提交后工作区干净，未推送。

## 全量未通过项

【实测】同一构建依赖、其他产品源码均不变，只将文件服务替换为任务基线并重新链接，以下 4 项在普通及内存构建中均通过。因此不能把它们归为基线已有失败。未删除断言、改预算、增加等待或改刷新算法来掩盖结果。

| 测试 | 当前失败证据 | 基线文件服务对照 |
| --- | --- | --- |
| `scenario-reader_basic-metalio-plus-pqbn` | 首次可见反馈 p95 460.733 ms，预算 460 ms | 447.611 ms，退出 0 |
| `sms` | `tests/sms/sms.c:49`，`capture is settled` | 15 阶段均通过，退出 0 |
| `settings-b` | `tests/settings_b.c:226`，`image style has no labels over the photo` | write/restart 均通过，退出 0 |
| `confirm-pages` | `tests/confirm_pages/pages.c:67`，ConfirmWeb 的 `screenshot is fully presented`；其余 5 场景过 | ConfirmWeb 通过，退出 0 |

【实现】检索所有产品调用，索引记录的直接读取都已接入统一解码，完成记录单独保留定长格式；未发现其他产品文件绕过文件服务读原始索引。记录长度改变伴随仿真时序变化是回归原因的假设，尚未定位和修复这四项的具体根因。基线复核日志保留在 `work/index-compact/baseline-check.log`、`baseline-check-asan.log` 和两个 `baseline-check-build*` 目录；脚本使用 CMake 编译/链接参数及 `git show` 的原文件，不改工作区源码。复核命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/check_baseline.py --build build`；内存版换 `build-asan`。两版均退出 0。

## 没做到的

- 尚未使全量测试通过：上表 4 项回归未修，普通及内存构建都复现；因此不能报告全部验收通过。未连接设备或运行 Linux/远端 CI。

## 和设计不一致的

- 无本次新增差异。【实现：架构审视】这是文件服务内部记录编码，用户和 AI 仍走原有文件能力及成员鉴权；没有页面、能力、软总线边界变化。

## 改过的已有测试

- `tests/index_durable/device.c`：v1 升级后的期望由 schema 2、216 B 改为 schema 3、`32 + 路径字节数 + 1`，契合本任务要求；没有放宽断言或预算。
- `tests/file_index_3/boundary.c`：路径观测助手兼容 schema 3 的路径偏移；保留对 schema 1/2 基线的读取，不改已有测试期望。
- `tests/index_cost/run.py`：仅增加手动 `--baseline` 对照入口，原计量及断言不变。

## 要用户定的

- 是否另开明确范围修复上表 UI 回归。本任务保留索引实现及失败证据，未扩展到刷新、截图同步或锁屏实现；验收 3 当前未达到。

## 顺手发现

- 暂无。

## 新增的机制

- schema 3 变长 payload：任务书第 1 项明确要求。沿用既有任务、完成记录与后台补建；产品无新增任务、状态机、配置项或迁移流程。
- `compact-test` 测试任务与 `SYNTH.compact` 合成集合：验收 2，检查旧格式和长路径；只进入模拟器测试，不进入固件。

## 要改的文档

- 主会话可在 `docs/system/system-services-design.md` 同步 schema 3 布局、schema 1/2 随后台补建升级及本次模拟器计量；共享文档与历史调研均未改。

## 复查后的修复

- 复查基线：`6d47a1f6`。`REVIEW.md` 仅有 `IC-H1`（高），没有其他未修的严重、高、中、低项。以下为本次接续记录；上文保留初次实现的测量和失败历史，最终验收结果待两套全量完成后更新。

| 修复 | 根因与改动【实现】 | 测试和反向对照【实测】 |
| --- | --- | --- |
| 完成标记与记录格式解耦 | `file.index_ready` 保持既有版本 2，接受版本 1/2 的有效完成标记；不会仅因 payload 升级而扫描整卡。旧记录继续读取，单条重写或后续正常扫描时写成 schema 3 | `index-compact` 扩为 12 阶段、134 个检查，覆盖旧完成标记、混合格式、逐条升级、完整升级、重启零扫描/零写盘/零通知，编号始终为 `[2, 1, 3]`。只换回复查基线文件服务，新增断言出现 24 次预期失败 |
| 短信及时投递重画 | `message.received` 的回调只负责投递 UI 工作，改为直接执行，去掉先在 BG 排队的一跳 | 原 `sms` 的 15 阶段通过；仅撤回这一处，原 `capture is settled` 断言再次失败 |
| 锁屏请求纳入界面忙碌 | 既有锁屏请求、状态和通知进度尚未交付时不能报告界面空闲；通知结束后才更新既有 `notified`。两项既有标志改为原子访问，查询不取锁 | 原 `settings-b` write/restart 通过；仅撤回忙碌判断中的电源查询，原第 226 行断言再次失败 |
| 去掉无效状态栏重画 | 电源状态变化不改变任何状态栏字段，导航回调明确忽略 `power.state`，保留时间、网络、通知等真正可见状态的刷新 | 原 `confirm-pages` 六场景通过；仅撤回这一处，ConfirmWeb 的原截图断言再次失败 |
| 降低阅读场景绘制开销 | 既有 LVGL 条带由 96 行改为 128 行，减少大字跨条带重复绘制；该屏幕多用 PSRAM `684 × 32 = 21888 B`，没有增加内部 RAM 或缓存 | 原 reader_basic 预算通过，反馈 p95 `456.628 ms ≤ 460 ms`；五张 `clean-*.png` 与修复前逐像素相同，UI 总 CPU 从 `2655.724` 降到 `2635.774 ms`。仅恢复 96 行，原预算再次失败（`460.756 ms`） |

【实测，原因核查】初次四项失败均从新镜像启动，没有旧完成记录。reader、sms、ConfirmWeb 各扫描 3 个根目录、发出 2 次 `indexed`；settings-b 全过程 7 次扫描、7 次通知，4 次 `ink_fs_lookup` 全部成功。因此旧完成标记和壁纸编号丢失均不是这四项失败的原因，不修改本来可以读取旧编号的查询路径。reader 的通知在约 `1.387 s`，sms/ConfirmWeb 在约 `2.757 s`，settings-b 最后一次约 `71.046 s`，均早于对应失败时刻（`44.872 / 18.265 / 9.311 / 83.880 s`）。完整计数和时间在 `work/index-compact/observe-before/summary.json`。

【实测，阅读时延】基线和紧凑记录的 UI 总 CPU、缓存命中/缺失、最终画面一致；最后一次 HOME 的慢扫描额外延迟分别为 `37.139525 / 50.262229 ms`，差值 `13.122704 ms`，与反馈时延差值一致。【实现】Flash 时序和触控慢扫描共用模拟器随机序列，写盘量改变会影响后续样本。没有改随机数、种子、硬件模型或预算；通过减少实际绘制工作留下余量。

【实测，验证进度】原四项定向测试 4/4；第一轮普通全量 288/289，唯一新增失败是能力目录的源码提取结果变化，已用明确忽略状态的等价分支修复，不改共享生成文档。第二轮普通全量进行中；ASan/UBSan、S31 和最终检查待完成。

反向命令只在忽略目录生成替换对象，不修改工作区产品源码，也不注册为依赖历史的 ctest：

```bash
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/run.py --build build --baseline 6d47a1f6
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/check_baseline.py --build build --revert strip
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/check_baseline.py --build build --revert sms
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/check_baseline.py --build build --revert lock
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/index_compact/check_baseline.py --build build --revert navigation
```

【实现，范围】相对任务基线，产品代码累计 105 行增删，其中文件服务 83 行，UI/电源/短信 22 行；加上原来的 4 行 CMake 为 109 行，低于 180 行预算。只改完成版本已实测不能修复原四项回归，因此补上对应的小修；没有改存储服务、帧格式、队列、写盘顺序或 D-110。没有新增任务、状态机、持久格式、缓存、配置项或抽象层；电源查询只读取已有状态。

【改过的已有测试】`index-durable` 的旧记录升级断言前增加一次普通索引查询：已完成旧卡现在按复查要求延迟升级，不能再断言开机强制迁移。仍要求 schema 3、精确长度和编号不变；没有改原四项回归的测试、等待或预算。新兼容测试增加断言，原手动对照工具增加逐项撤回入口。

【架构审视】文件记录仍是文件服务的内部编码；界面通过既有电源服务读取待交付状态，短信经既有 UI 执行器更新。用户与 AI 的原子能力、成员鉴权、软总线事件和能力开放范围均不变。
