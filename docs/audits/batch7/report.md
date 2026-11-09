# batch7 独立复查报告

日期：2026-11-10。范围：`8d2a1d9a..d392a00a49dbdbd3e06bdd6b746932c27dae9438`；工作树 HEAD 与范围终点一致。

复查结论：证实 **2 项高、2 项中**。现有普通及 ASan/UBSan 全量均为 286/286 通过，专项实验补出了确认对象、凭据导出、USB 索引恢复和目录改名延迟的覆盖缺口。

本轮只读产品代码和已有测试，仅在构建目录做临时实验；不修改产品代码、不提交。依赖使用 `$INK_DEPS`，设计参考使用 `$INK_DESIGN`。结论按【实测】【实现】【估算】区分；没有证实的问题不计入等级。复查限时一小时。

## 验收表

| 编号 | 复查项 | 结论 | 证据、命令 |
| --- | --- | --- | --- |
| 1 | 持久化、断电、重启、成员隔离、拔卡 | 部分不通过：USB 改卡断电漏索引，见 B7-03 | 普通全量中的 `audit5-b5-02`、`index-durable`、`file-index-3`、`app-framework-gaps`、`phone-calls`、`sound-clock` 通过；专项 `experiments.py usb_durable` 失败。已有换卡、拔卡、换成员与重启测试未发现新失败 |
| 2 | D-105、孩子限制、秘密字段 | 不通过：确认对象和通话凭据边界失效，见 B7-01、B7-02 | 两项电话实验均检出失败；现有确认策略、AI 不能自批、孩子不能控制成人通话、孩子音量上限回归通过 |
| 3 | 按钮与 AI 能力一致、页面快照 | 原子能力接线成立，安全语义与视觉验收有缺口 | `capability-table-*`、`integration-ai-surface`、电话及计时专项通过；DTMF 快照暴露凭据见 B7-02；画板对照见下文 |
| 4 | RAM、栈、任务与固定上限 | 未观察到耗尽，但栈和内部 RAM 余量紧 | 普通全量预算通过；能力 365/512，设置 21/48；宿主栈估算与其他固定表见“资源核查” |
| 5 | 七项任务合并后的相互影响 | 部分不通过：B7-03、B7-04 为索引与存储配合遗漏及性能回归 | 普通全量 286/286；存储同步批次等待 20 ms、10 次短按和 10 次触摸均送达、丢麦克风样本 0 ms；目录改名前后对照见 B7-04 |
| 6 | Linux/gcc 可移植性 | 本机及 S31 交叉编译通过；Linux 原生环境未验证 | Apple clang 两套 `-Werror` 构建成功；`taskpolicy -c utility bash ports/esp32s31/build.sh` 使用 GNU 15.2.0，退出 0；未发现新的确定性 gcc 编译问题，但不能代替 Ubuntu/gcc，见“没做到的” |

## 有证据的问题

### B7-01【高】旧通话的 DTMF 确认可在下一通电话执行

- 位置：`system/services/comm/comm_service.c:313`、`:317`、`:341`；确认参数绑定在 `system/policy/confirm.c:108`、`:325`。
- 【实现】`call.dtmf` 只绑定 `key` 和成员，没有绑定通话代次或对方号码。执行时仅检查当前通话属于同一成员；挂断也没有使该通话的待确认请求失效。它没有 `activity` 约束，通用页面代次检查不能补上通话身份。
- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch7/experiments.py phone_rebind` 退出 1：25 条检查通过，1 条失败。拨打合成号码 `00000000001`，AI 请求发送 `7` 并留下真实待确认卡；第一通远端挂断后接听 `00000000002`，人确认原 token，`ink_confirm_decide` 返回 0，第二通模组实际收到了 `7`。未暂停线程，也未修改产品代码。证据：`build/audit-batch7/phone_rebind/{out-summary.json,out.log}`。
- 影响：人的旧确认被用于另一个对外对象，可能触发新通话的菜单、授权或付费选项；违反 D-105 的确认对象绑定。
- 最小修法：在通信服务统一产生通话代次，并在请求进入确认前绑定该代次及对方；执行控制动作时校验，结束通话即令旧请求失效。确认说明显示对象，补上“待确认期间挂断并接通另一通”的回归。

### B7-02【高】通话键盘输入的凭据没有秘密字段边界，完整序列进入 AI 上下文

- 位置：`system/apps/phone/phone.c:234` 的 `P.fields[4]`（`tones`）；`system/services/comm/comm_service.c:332–344`；`system/policy/cap.c:934–951`、`:981–991`。
- 【实现】已发送按键以普通字段导出，`secret=false`；每个获模组确认的按键还通过默认向 AI 暴露的 `call.controls` 事件进入中枢发送队列。挂断清空页面数组不会撤回已导出的数据。任务明确包括银行/客服按键，现有路径没有密码、验证码输入模式。
- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch7/experiments.py phone_privacy` 退出 1：38 条检查通过，1 条失败。在合成通话键盘实际点按 `246802`（假 PIN），`ink_ui_context_snapshot` 返回 `{"id":"tones",...,"value":"246802","secret":false}`。证据：`build/audit-batch7/phone_privacy/ai-context.txt`、`out-summary.json`。这证明真实 AI 上下文会暴露该输入；事件发往中枢的部分为代码证据，没有连接真实服务。
- 影响：用此键盘输入 IVR 密码/验证码时，D-33、D-111 和任务“秘密字段不进快照/日志”的约束失效。已有 `phone-calls` 反而断言完整按键应出现在快照中，未覆盖秘密输入。
- 最小修法：在同一通话输入边界提供安全的凭据输入语义；凭据只由本人输入，快照掩码，发往中枢的事件不携带按键值。未区分普通菜单与凭据前，至少保守掩码整段已输入序列并阻止原值经事件旁路导出；只加快照 `secret` 标记不够。

### B7-03【中】电脑归还存储卡后立即断电，索引仍复用旧的“建好”记录

- 位置：`system/services/file/file_service.c:1209–1227`、`:1464–1480`、`:1733–1736`。
- 【实现】新持久标记只接到了设备内的新建、改名、复制、恢复入口；USB 归还仍只经 `changed(...,"rescan")` 在 RAM 中失效。借卡前、归还成功前都不建立 `.file-index-dirty`。这是 B5-02 的残留入口，`audit5-fix` 第 2 项明确列出了“电脑归还后的 rescan”，并非另一项功能需求。
- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch7/experiments.py usb_durable` 退出 1。先让仅有 3 B 合成文件的卡完成并落盘索引，通过真实 USB 借出接口交给模拟电脑，电脑新增 5 B 文件，`ink_usb_disk_stop()` 成功后、后台补建前断电（读取一次标记状态后 1 μs 触发）。断电前 11 条检查全部通过、卡标记不存在；同一 Flash/Card 新进程重启后，新文件确实为 5 B，但索引报告 ready、新文件编号为 0、书籍统计仍为 3 B（应为 8 B），10 条通过、1 条失败。证据：`build/audit-batch7/usb_durable/{usb-cut-summary.json,usb-reboot-summary.json}`。
- 影响：文件字节未丢，但索引完成状态和统计不实，依赖编号的功能漏掉文件；现有 `audit5-b5-02` 仅覆盖设备内新建/改名，未检出此入口。
- 最小修法：复用本批卡上失效标记，在把卡交给电脑之前持久写入，确保借出期间断电也能恢复；归还后沿用现有补建及标记清除流程。补 USB 改卡后立即断电的回归。

### B7-04【中】目录改名新增逐文件同步查询，300 个文件时持锁从 189 ms 增至 8.78 s

- 位置：`system/services/file/file_service.c:1524–1538`，尤其 `:1535`；外层 `ink_fs_move` 持有 `FS.edits`。
- 【实现】改目录路径时，新增对每条后代索引记录同步调用 `hal_fs_size` 和 `hal_fs_mtime`；原来只改索引中的路径。目录中的文件数量越多，持锁时间越长，其他需要该编辑锁的文件操作只能等待。
- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch7/index_directory.py` 退出 0。相同模拟器配置、300 个 1 B 合成文件、索引已完成且 UI 已呈现，调用真实 `ink_fs_move` 改目录名。仅替换原样的 `file_service.c` 版本作对照：`1705e7ca`（index-durable 前）操作 188,976 μs、最大持锁 188,947 μs；本批终点操作 8,784,847 μs、最大持锁 8,784,766 μs。两边功能检查均 10/10、无警告；实验没有暂停线程，也没有修改产品源码。证据：`build/audit-batch7/index-directory/summary.json` 和两侧 `run.log`、`out/metrics.json`。
- 影响：在 1024 条索引上限之内，普通目录改名就产生多秒同步阻塞；现有 `index-durable` 的 5 次改名延迟测试只覆盖单个文件，未覆盖目录后代的放大效应。本次测得的是模拟时延，不是实机耗时。
- 最小修法：单纯路径移动复用已有文件大小和修改时间；确实需要更新的元数据交给现有后台索引流程，避免在编辑锁内逐条读卡。补一个中等大小目录的前后延迟对照，保持已有资源预算。

## 没做到的

- 未进行实机验证，未接触设备、未拨打真实电话。NT26 命令支持、通话音频接线和最高优先级输入任务对真实刷新/Wi-Fi 的影响仍需设备证据；既有电话报告已披露这些边界。
- 未运行 Ubuntu 原生 gcc 测试；本机没有该运行环境。clang 通过及目标芯片交叉编译均不能替代 Linux CI。
- 本轮只做指定范围的一次复查，没有修改四项问题，也没有将全部潜在时序组合逐一穷举。一次计时取消实验被遮罩权限门拒绝，未满足“取消成功”的前提，不计为产品问题。
- 仅配对现有可验证截图，154 张标准尺寸画板中 26 张没有本轮可用截图；名单在 `build/audit-batch7/ui-current/unmatched.json`。

## 和设计不一致的

- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fidelity.py --build build --design "$INK_DESIGN" --out build/audit-batch7/ui-current` 退出 0；复用本轮全量产生的已呈现帧，128/154 张配对，128 张全部超过 12/255。PhoneCall 20.972、SetSound 44.811、Clock 44.863、FactoryReset 51.346。退出 0 只代表成功计分，不代表达标。分数、源帧哈希和拼图均在该目录。
- 【实现＋截图检查】旧画板与现行 1.2 倍规格、纸色和真实测试内容不同；SetSound 的新开关/铃声在第二页，旧画板在同一屏。该问题在 `ui-fidelity/REVIEW.md` 已明确，未作为新缺陷重复计数，也未降低阈值。计时页没有独立画板，仍待用户查看。
- D-105 的确认对象绑定、D-33 的凭据边界、索引延迟验收分别见 B7-01、B7-02、B7-04。

## 改过的已有测试

本轮无；临时驱动全部位于忽略的构建目录。对被审计范围中旧测试期望变化的核查如下：

- `integration/media.c` 将通话中连接耳机的期望改为立即走耳机，符合 phone-2；`camera_gallery` 和 `media` 删掉按短键前的刷盘等待，恢复直接输入，未放宽预算。
- `hub/e2e.script` 从 7 s 恢复到 3 s；新增成员就绪早于 3 s 的检查。本轮实测为 2399.530 ms。
- `integration/dictation.c` 从完全等于已确认句子改成允许紧随其后的合法部分转写。核对 `system/services/text/voice_input.c:251` 的既有 `fail_with` 会保存 interim，该变化有实现依据；不代表允许任意额外文本。
- `esp32s31/osal_ticks.c` 按任务规定的相对时长向上取整再加 1 改期望并增加起点差用例；存储故障桩、索引计数桩仅适配新接口，容量断言保留。未发现预算文件或已有测试超时上限被提高。

## 新增的机制

本轮没有新增产品机制。被审计范围新增的后台开集合上下文、可取消订阅句柄、存储等待优先提示、四个声音设置、按成员倒计时、卡上 dirty 标记及索引 v2 均能对应原任务要求；复用既有执行器/服务定时器，未见新增产品常驻线程。目录操作的同步查询代价见 B7-04。

## 要用户定的

- 本轮四项问题不依赖新的产品选择，建议修复后用所列场景复测。
- 延续既有 UI 复核的待选项：按现行规格重出画板，或改用结构/位置等对照方法；本轮不代替用户选、不改验收阈值。计时页布局仍待用户查看。

## 要改的文档

- 主会话应在任务状态中保留 B7-01～B7-04 及 UI 未达标、NT26/实机待验证的边界；“全量测试通过”不能替代这些验收。
- `index-durable` 的性能结论需增加目录样本，B5-02 的完成范围需明确包含 USB 归还。本轮不修改共享文档或既有报告。

## 顺手发现

没有另列范围外缺陷；资源紧张情况见下文，不把宿主估算当作实机故障。

## 资源核查

- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" build/audit-batch7/capacity.py` 退出 0、无警告。原样产品启动后的占用：能力 365/512、事件订阅 32/64、状态 48/64、状态订阅 53/64、设置 21/48、设置订阅 6/64、集合定义 20/32。状态订阅只剩 11 槽；这是开机样本，不是所有页面组合的峰值。证据：`build/audit-batch7/capacity/summary.json`；原 `app-framework-gaps` 100 次开关页面的三种退订检查在两套全量中均通过。
- 【实测】普通全量无失败/警告的成功场景中，`mem.internal_total.min_free_bytes` 最低为 `media` 的 33,424 B，距 `tests/ui_scene.budget.json` 的 32 KiB 参考线仅 656 B；`dictation` 为 34,984 B。数据提取在 `build/audit-batch7/resources.json`，不是硬件实测，也不是声称 media 自身设有同一预算断言。
- 【估算（本轮模拟器输出）】`esp_timer` 栈 3552/3584 B（余 32 B）、`main` 4000/4096 B（余 96 B）、`ui` 9664/10240 B、`exec.media` 7552/8192 B。对应证据路径在上述 JSON；未出现栈越界告警，但余量不足以支持“资源宽裕”的结论。
- 【实现】索引仍上限 1024 条，新增 `missing[1024]` 为约 4 KiB PSRAM；集合待写仍 256 KiB。`storage-order` 的 1041 条压力数据全部落盘、1024 条整卡索引完成。JSON 解析器仍为 128 token，本轮未做全部能力参数的极限 token 压测。
- 【交叉编译】S31 链接占用 DIRAM 83,042 B、外部 RAM 静态区 499,728 B，镜像分区剩余 19%。这是静态链接量，不含全部运行时分配；`stack.txt` 的已知直接调用链也不包括间接回调、库内帧等，不能当作栈用量上界。证据：`build-s31/size.txt`、`build-s31/stack.txt`。

## 构建、测试与范围检查

- 【实测】普通配置/构建成功，启用 `INK_WERROR=ON`；全量 **286/286，通过，0 跳过，220.55 s**。ASan/UBSan 同样保留 `-Werror`，构建成功，全量 **286/286，通过，0 跳过，614.03 s**。两次 CTest 均退出 0；内存检查日志没有 AddressSanitizer、LeakSanitizer 或 UBSan `runtime error` 命中。
- 日志：`build/audit-batch7/{configure,build,ctest}.log`、`build-asan/audit-batch7/{configure,build,ctest}.log`。`INK_HUB_PYTHON` 指向现有中枢虚拟环境，端到端测试实际执行，没有因缺依赖跳过；GUI smoke 使用无窗口后端。
- 【实测】`taskpolicy -c utility bash ports/esp32s31/build.sh` 完整完成，退出 0，使用现有本地 SDK 和 GNU 15.2.0；日志 `build-s31/audit-batch7/build.log` 未见编译 warning/error。SDK 自带若干 `-Wno-error` 例外，且没有编译宿主机测试源码，因此不将此结果写成 Ubuntu/gcc 严格全量通过。
- 配置使用任务规定的字体、Python、LVGL，并补足 Medium/Bold 字体、SDL2/ImGui 的现有本地依赖；ASan 配置仅更换目录并增加 `-DINK_SANITIZE=ON`。构建与测试均通过 `taskpolicy -c utility`，并行度最多 2，同一时间只跑一轮完整构建或全量测试。

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# 内存检查同上，目录换成 build-asan，配置加 -DINK_SANITIZE=ON。
```

- 【实测】扫描指定基线到 HEAD 的 diff，隐私正则命中 4 行，均为任务允许的合成耳机地址；本机绝对路径零命中。结果为 `build/audit-batch7/privacy-scan.json`。`git diff --check` 及基线 diff 检查通过；报告源码行号均存在，报告无本机绝对路径。最终跟踪文件无改动，审计目录只含原 TASK.md 与本报告；临时脚本及证据均位于忽略的构建目录。未读取 `ink-hw`，未提交、推送或改动产品代码。

SEVERE=0 HIGH=2 MEDIUM=2 LOW=0
