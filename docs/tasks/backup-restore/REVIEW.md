# backup-restore 复查

复查日期：2026-11-06。任务基线 `a51a804c`，受检提交 `df3d46d6`。本轮复查完成：**严重 0、高 1，暂不建议验收通过**。上轮并发覆盖已修；网络等待已修，但同类 Flash 等待仍会阻塞后台。清空后恢复的产品重绑定链路仍未实现，见 B3 的证据边界。

按任务开头 G1～G5、backup-restore 一节、`codex-rules.md` 和 `review-prompt.md` 独立检查。只修改本文件，不提交；构建和探针产物留在忽略的构建目录。资源限制为构建最多 `-j4`、CTest 最多 `-j3`，同一时间仅一项构建或全量测试，不启动窗口 GUI。

## 优先复核

- 上轮 BR-R4（高）：不同大小、同大小、目标不存在三组并发写回归通过；撤掉稳定采样后，真实恢复交错检出正文/结果错误；只撤掉非阻塞安装或 USB 归还换代也各被检出。
- 上轮 BR-R3（高）：慢链路回归通过；改回同步网络等待时，快链路通过、慢链路失败 3 条并出现 1 次能力超时。网络部分已修，但同步 Flash 安装仍阻塞后台，见下文。
- 早期 BR-R1（高，递归栈）、BR-R2（严重，成员 ID 复用泄漏）：原专项通过；恢复递归遍历会触发原栈警告，撤掉身份不可复用/持久化/回执检查均被检出。

## 本轮证据

已读取上一轮提交中的复查报告（本次开工时工作区报告为空）、开发报告、任务约束、设备端设计说明及专项测试入口。证据边界为 macOS/clang、合成身份、模拟器和本机模拟中枢；无 Linux/gcc 和实机结果。

- `cmake --build build -j4` 退出 0；普通全量 `ctest --test-dir build --output-on-failure -j3`：105/105，50.14 秒，退出 0。
- `cmake --build build-asan -j4` 退出 0；ASan/UBSan 全量同参数：105/105，92.16 秒，退出 0。
- 两套备份专项各 63 次启动通过；证据 `build/backup-restore-5yqn2p8o/`、`build-asan/backup-restore-oneue7xx/`。
- `review_reverse.py --build build --evidence build/backup-restore-5yqn2p8o` 退出 0，证据 `build/backup-review-reverse-gbsy5ips/`。源码和正式归档保持不变。
- `reverse.py --build build` 退出 0，证据 `build/backup-reverse-x5e01dl7/`；集合版本、日志代、Flash 条件写、卡文件条件写、目录成员绑定各自被反向用例检出，空闲启动变体跌破原 32,768 B 内部 RAM 预算。
- 两套专项均零断言失败、零警告、零能力超时/迟到；普通专项内部 RAM 最低余量 41,144 B，后台栈最高 4,976 B，存储栈最高 3,472 B。ASan 插桩栈不作为设备栈预算证据。
- 在原 `flash-cut`/`flash-verify` 场景补跑第 140、170 次写入的真实撕裂，两次重启均恢复完整资料，证据 `build/review-later-cuts-18592abi/`；同批第 210～700 次的六组未触发撕裂，只作恢复完成后的重启验证，不计断电覆盖。
- `git diff --check a51a804c..HEAD` 通过；任务指定隐私模式扫描为 0 匹配。

## 逐条验收

| 验收项 | 本轮结论 | 独立证据 |
| --- | --- | --- |
| B1 备份清单、上传回执 | 现有成员范围通过 | `backup-restore-recovery` 的 seed、receipt-reboot、wrong-receipt、legacy-journal、slow-status；received 与 durable 分开，重启后回执可查，错误身份不能授予回执 |
| B2 恢复预览、覆盖确认、没恢复的项目 | 通过现有专项 | pin、stale、member-confirm、corrupt、malicious；AI 不能接受覆盖，错误 PIN/成员/版本不写入，未知集合和未恢复范围有结果说明 |
| B3 备份→清空→恢复 | **部分：只证明独立重绑定夹具之后的成员资料恢复** | seed→wipe→rejoin→restore-cut→restore-resume→verify-reboot；两位成员的正文、空文件、记录编号/版本/删除标记和阅读器第 4 页核对通过。`rejoin` 直接导入单独保存的合成成员表，属于测试前置条件，不是可用的产品重绑定能力 |
| B4 中途断电、断网继续 | 已覆盖场景通过；响应性仍不通过 | collect/upload/restore 的切断与接续、原六组 Flash 撕裂和新增两个较晚撕裂点通过；较大 Flash 数据安装时存在 BR-R3 |
| B5 按成员隔离 | 通过已覆盖场景 | 两组 reused-id→identity-reboot、member-confirm、member-resume、slow-preview-switch、slow-switch、slow-pause；旧成人身份、清单、回执、正文不被同名新孩子继承 |
| G1 全量、内存检查和原预算 | 提交内测试通过，补充探针检出缺陷 | 两套各 105/105；原预算未变。新增独立负载探针失败，不能仅凭全量绿灯认定本任务全部通过 |
| G2 能力、AI、分层 | 通过现有范围核查 | 五个调用和两个事件统一注册；覆盖确认走现有人工/PIN 策略；capability-table、integration-ai-surface、sdk-boundary、确定性测试通过。无新增页面/常驻任务；网络回执回调只保存接续状态，文件条件安装归文件服务、集合安装归数据服务 |
| G3 依赖表状态 | 通过 | `docs/system/ui-dependencies.md:87` 只将状态改为“部分”，仍列明页面、设备级资料、增量对账未接，未改判断栏 |
| G4 页面规则和对照图 | 不适用 | 本任务为服务层，未新增页面；未启动带窗口的 GUI。全量自带 gui-smoke 使用 SDL dummy 驱动，不属于窗口验图 |
| G5 范围与总规则 | 未发现新增严重/高违规 | 基线差异为备份服务、相关存储/文件/成员/传输接口、启动注册、测试和文档；未改需求/决策/历史调研，未放宽预算、删原断言、跳过测试或加 sleep 掩盖结果。新增 `fgets` 返回值有检查；Linux/gcc 未实际运行 |

未实现的真实中枢重绑定、跨设备身份恢复、设备设置/成员凭据备份、增量对账和 dataset/restore_token 换代，开发报告已明确列出；本轮不把模拟夹具或自定义协调消息当作这些能力的完成证据。

## 已确认问题

### BR-R3（高，部分未修）：Flash 日志及恢复安装仍同步占用共享后台执行器，录音时连启动备份都会超时

**位置**：`system/services/backup/backup_service.c:172`、`:173`、`:803`、`:807`、`:809`、`:1111`；`system/services/data/datastore.c:1113`、`:1114`。录音持有 Flash 写入暂停的生产路径为 `system/services/audio/audio_service.c:592`～`:600`。

**复现命令**：

```sh
"$INK_DEPS/build-deps/pyenv/bin/python" build/review_flash_wait.py --kind collection --bytes 262144
"$INK_DEPS/build-deps/pyenv/bin/python" build/review_flash_wait.py --kind state --bytes 524288
"$INK_DEPS/build-deps/pyenv/bin/python" build/review_held_start.py --audio
```

**方法与结果【实测】**：只在忽略的构建目录编译 `tests/backup_restore/device.c` 的隔离测试副本，链接未修改的正式产品归档，使用原 `metalio-plus.json`，不改变能力期限和原响应性断言。第一组增加 32 条各 8 KiB 的合成集合记录（正好是默认 256 KiB 集合内容限额）；第二组增加 512 KiB 成员 Flash 状态（小于报告声明的 1 MiB 上限）。先备份 A，再把本地内容改为 B，预览并人工确认恢复 A；沿用 `large-restore` 的进度查询和解锁探针。三条命令均独立重现，并由探针检查断言计数后退出 1；模拟器本身正常退出，未崩溃。

- 集合组：备份 138 条检查通过；恢复 `test.fail=2`、`cap.timeouts=1`、`backup.restored=1`、`test.finished=1`，进度查询超时，整份集合正文验证通过。证据 `build/review-flash-collection-g3jqklfc/`。
- Flash 状态组：备份 136 条检查通过；恢复 `test.fail=3`、`cap.timeouts=1`、`backup.restored=1`、`test.finished=1`，进度查询超时，PIN 后台回调超过原 2 秒等待，完整正文验证通过。证据 `build/review-flash-state-2tvdi11f/`。
- 录音组：保持原 98,304 B 卡文件和小集合/状态，先持久化成员身份，再经 `ink_audio_capture_begin_local` 启动实际模拟录音服务，等到录音帧后调用 `backup.now`。请求超时，结束录音后备份才继续；`test.fail=2`、`cap.timeouts=1`、`cap.late=1`、`backup.saved=1`、`test.finished=1`，其余 91 条检查通过。录音采集约 3.1 秒，未丢音频帧。证据 `build/review-held-backup-u4ce6d9q/`。直接使用同一 `ink_storage_hold_writes` 接口也复现，证据 `build/review-held-backup-9yxclu56/`；未使用真实麦克风或设备。
- 对照：64 KiB 状态、128 KiB 集合均通过，零超时；不是破坏网络、放大卡文件或缩短期限造成的失败。探针同时核对正式源码和 `libink_system.a` 未变。

**原因及影响【实现】**：网络现在会归还 `exec.bg`，但 `save_journal`、`apply_item` 和 `ink_db_import` 仍在后台执行器内轮询等待 Flash。`osal_task_delay_us` 只让出 CPU，没有让后面的后台能力获得执行机会。录音本来就会暂停 Flash 写入，因此普通备份请求也会超时并迟到执行；恢复较大集合/状态时则拖住进度、解锁等后台工作。现有大文件测试放大的是卡文件，集合和 Flash 状态只有很小的夹具；暂停场景只暂停后台作业，未覆盖实际录音的 Flash 写入暂停。

**修法**：把日志持久化、Flash 条件安装和集合导入也拆成提交、落盘回调、完成续接；等待期间保留原成员代次、预览版本、缓冲及安装状态，但归还共享执行器。状态查询也应避免等待正在执行长写入的存储队列。保留先持久化意图再改数据、条件写和断电原子性；增加录音期间启动备份、上述实际恢复场景及反向对照，不提高期限或缩小支持范围迁就测试。

SEVERE=0 HIGH=1
