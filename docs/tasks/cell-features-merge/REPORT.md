# cell-features-merge 执行报告

基线：`ac18303cf0dcf0459a5a8f7c7d57f6b17ddc193a`。本地分支 `claude/codex-cell-features-merge`；仅编译、宿主机测试和本地提交，不操作设备、不推送。来源为 `origin/archive/mobile-network-2`（`713eb6b1`）、`origin/archive/emergency-call-2`（`98ac6b08`）；带入的原报告保留为历史，本轮结果以本报告为准。工作证据统一放在 `work/cell-features-merge/`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 功能合并 | 【实测】专项通过 | 两个归档已合入；移动网络、紧急呼叫、AI 契约 3/3 通过 | `ctest --test-dir build -R '^(mobile-network|emergency-call|integration-ai-surface)$' --output-on-failure -j2` |
| 1 开机延迟 | 【实测】原回归消失，反向命中 | 初始化约 2107 ms 不变；仅移动网络时成员就绪 5056.334 ms，完整合并 2417.671 ms；只撤销显示互斥恢复到 5056.327 ms | `tests/cell_features/boot.py --build build`，追加 `--reverse-display` 做反向 |
| 1 普通全量与原分支测试 | 【实测】通过 | 原话 `100% tests passed out of 347`，318.33 秒、0 跳过；移动网络四阶段 204/42/24/49，紧急七场景 826 条检查 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 1 ASan/UBSan 全量 | 【实测】通过 | 原话 `100% tests passed out of 347`，516.02 秒、0 跳过；日志扫描无消毒器诊断 | 普通全量命令换 `build-asan`，环境 `ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` |
| 1 原分支反向测试 | 【实测】通过 | 移动网络 7 组、紧急呼叫 6 组均命中预期失败；没有放宽原断言 | 下文命令；`mobile-reverse.log`、`emergency-negative-*.log` |
| 2 S31 | 【实测】通过 | 完整编译、链接、镜像、尺寸和栈报告退出 0；编译警告 0，SDK 配置警告 5；蜂窝桩普通／ASan 宿主测试各 1/1 | `taskpolicy -c utility bash ports/esp32s31/build.sh`；`s31.log`、`build-s31-host*-tests.log` |
| 2 GCC | 【实测】通过 | 主构建 `gcccheck: 20 compiled, 0 with findings`；蜂窝桩另 `2 compiled, 0 with findings`，0 跳过 | 下文命令；`gcccheck.log`、`gcccheck-stubs.log` |
| 2 隐私与范围 | 【实测】通过 | 总规则扫描无命中，grep 退出 1；diff 检查通过，45 个相关文件，最大 128948 字节 | 总规则 §6 原扫描、`git diff --check`、`git diff --stat` |
| 3 能力清单与差异 | 【实现／实测】通过 | 530 项，重生成和 `--check` 均通过；紧急入口按标记、五项能力归系统；主线电源状态读取顺序和短信草稿契约均保留 | `python3 -B tools/capabilities.py`、追加 `--check`；差异见下文 |

开机延迟的原因与证据：

- 【实测】基线、仅移动网络、完整合并的系统装配分别为 2107.476、2107.466、2107.480 ms，成员确认分别为 2416.153、5054.898、2416.171 ms。新增的约 2.64 秒在确认后的夹具建成员阶段，不在系统装配阶段。
- 【实现】`member.add` 在 `system/policy/members.c:676/713` 首次打开 `member.retired`；`datastore.c:594/633` 读取快照及两代日志。每个文件先经 `storage_service.c:699/740` 查大小；存储任务在 `:354` 申请显示互斥，非异步屏要等在途波形完成（`display_service.c:198/293`）。
- 【实测】临时 trace 使用虚拟内核时间，不增加 `osal_now_us()` 时间收费；三个不存在文件的大小查询为 2366.368→3267.243、3267.243→4159.847、4159.847→4923.222 ms，中间分别夹着整屏刷新。存储任务此时服务同步读取，并非持续轮询流量写入的空闲条件。`storage.wait_for_idle_ms` 在循环开始记时、循环中服务读取、循环结束才计值（`:497/501/524`），所以会把这些等待算进去。
- 【实现／实测】仅合移动网络时，存储与显示的调度交错发生变化，原夹具又在装配结束立即建成员；固定在 3 秒点按因此撞上首次设置页。完整迁入紧急分支的 `hal_display_sim.c:542/550` 上电互斥后，本基线下三秒前已就绪。只撤这两行即可复现相同 5056 ms、`tool_calls=0`，其余业务全部保留。该互斥本来用于显示与睡眠准备串行；其对本回归的效果包含调度时序变化，不声称它为所有首次读盘提供时延上界。
- 【实测】进一步只恢复旧 LTE 判断、撤销注册事件查询、撤销漫游状态发布，三组结果仍为 5056.327 ms。没有按这些假设改产品。保留原中枢脚本、3 秒断言、超时和预算，不额外添加产品补丁。
- 证据：`work/cell-features-merge/{baseline,mobile,combined}-e2e.log`、`storage-trace/out/trace.json`、`size-requests.txt`、`isolate/summary.json`、`boot-{positive,negative}.log`。前述路径均相对同一工作证据目录；每次对照使用独立新输出，未把旧日志环混作本次证据。

## 没做到的

- 没有运行 Linux 原生 CI、连接设备或进行真实运营商验证；S31 蜂窝接口仍为桩（`ports/esp32s31/hal_stubs.c:51/58`），不能把模型通过写成真机可用。
- 移动分支具体哪一条初始化使调度相位变化，未缩到单行；三项局部对照均未消除原回归，按总规则停止该旁路。已定位实际阻塞路径，且完整合并与撤销两行的正反结果可复现。

## 和设计不一致的

- 继承两个归档的边界：接入点的 CID/IP/PAP、无 SIM 紧急拨号、临时射频及真实接口流量尚未上机核实；分钟采样决定月界字节归属，每日检查点后当天未保存的流量掉电可能丢失；清零确认卡仍为中文。没有修改设计合同迁就实现。

## 改过的已有测试

- `ai_surface.c` 迁入移动网络项，`sms.send` 必填参数保留主线的 `to`，未迁入归档的 `to,text` 变更。
- `systest.c`：设置整表的测试缓冲 2048→4096；能力清单改为逐页检查原来的四类内部名称禁入断言，原 8192 字节缓冲不变。
- `export_caps.c`：用已有分页接口导出能力，原大缓冲与 768 字节溢出对照保留。
- `whole_product.h`：返回成功的那次观测，取消退出循环后的二次求值；超时与 20 ms 间隔不变，原紧急测试含反向回归。
- `fw_gaps/app.c`：两条 DIRECT 注册检查移到既有 UI 屏障后；原 100 次页面循环及全部断言保留。
- `kid_apps.c`：明确列出五个系统紧急能力，验证不归电话应用且孩子可用；其他 phone/call 归属检查保留。`ports/esp32s31/tests/stubs.c` 增加真实桩不可用断言，普通和 ASan 版均通过。
- 新增手动反向脚本 `tests/cell_features/boot.py`：临时编译副本只撤销显示上电互斥，运行未修改的真实中枢端到端脚本；不依赖 Git 历史、不写产品源文件、不注册重复的 CTest。

## 要用户定的

- 无新增产品决定。真实模组设置、拨号和上机验证继续等明确授权；本任务不开展。

## 顺手发现

- 两个原分支均曾把 `sms.send` 必填预期改成 `to,text`；本任务按当前主线合同保留 `to`。
- 【实测】普通全量的中枢测试确实运行，没有因缺 Python 包而跳过；`hub-contract` 内部执行锁定环境的完整 pytest。没有使用真实厂商或凭据。
- 【实测】扫描 ASan 全量、LastTest、场景和紧急呼叫共 279 份日志，无 ASan/UBSan 诊断；额外蜂窝桩内存测试也无诊断。
- S31 的 5 条配置警告均为 SDK 内 `esp_wifi`／`wpa_supplicant` 私有 include 依赖，原分支报告已记载；没有改 SDK、屏蔽警告或升级依赖。临时编排脚本因 macOS Bash 空数组停过一次，修正后只补跑未执行的宿主与 GCC 检查，未重跑已有 S31 编译。
- 已目视检查合并后的中文移动网络、英文无模组和中文紧急通话截图：`build/test-out/mobile-network/main/snap-mobile-zh.png`、`build/test-out/mobile-network/no-modem/snap-mobile-no-modem-en.png`、`build/emergency-call-dbppf0e7/lock/snap-active.png`。

## 新增的机制

- `net.mobile` 私有集合、`net.roaming` 设置、两个移动网络状态：原 mobile-network 任务要求，复用既有数据、设置、分钟事件和后台执行器。
- `ink_cap_t.locked_entry`、紧急浮层、临时射频标记及独立紧急挂断能力：原 emergency-call-2 任务要求，复用既有策略和通信任务。
- 本次合并没有再加任务、线程、队列、重试、缓存、文件格式、配置项或新的产品机制；显示互斥沿用原有租约。

## 要改的文档

- 已按任务授权重新生成 `docs/system/capabilities.md`，迁入服务能力表的一行和框架源码锚点校正；正文合同不变。需求、想法、决策、backlog、ui-dependencies 以及已有 research/briefs/audits 均未改。
- 后续主会话可同步 ui-dependencies 的移动网络入口；本任务按共享文档禁改要求只记录。

## 与两个归档实现的差别

- 移动网络保留主线的 80 个订阅槽，未降回归档的 68；能力、页面、私有凭据、漫游、流量计数、持久化与无模组语义照归档保留。原移动网络测试文件与归档逐字相同。
- 紧急能力保留标记闸门、系统归属、独立挂断、双语、公开快照和两个锁定浮层的语义动作；`ink_power_state()` 仍是锁屏表达式的第一个判断。原紧急测试文件与归档逐字相同。
- 两边词表、页面、能力表及通信服务按意图合并；短信保留当前主线的 `maxLength:765`、仅 `to` 必填和草稿路径。新增能力清单为 530 项。
- 架构审视（R-061）：页面经能力总线调用；凭据仍限本人，AI 只读是否有密码；紧急拨号／按键／关闭／挂断限本人，AI 只开公开页；紧急能力不挂电话应用，禁用电话的儿童夹具验证普通呼叫受限而紧急操作可用。

## 原厂依据与规模

- 2026-11-11 只读查阅原厂 `690a023`：`main/boards/common/board/uart_eth_modem.cc:1474/1505` 解析 CEREG 1/5，`:1596` 先启射频、无 SIM 保留 AT；`main/boards/common/power_policy/README.md:69` 要求冻结 UI、等待物理刷新再睡眠。未复制原厂源码或修改硬件参数。
- 【估算】NT26 接入点认证、紧急呼叫命令、无 SIM／受限网络及临时射频仍需上机核对；上机只读查询 APN、CEREG 和接口字节来源，不拨真实紧急号码。
- 相对基线，产品 C/H 新增 475、删除 53 行（增删合计 528）；另有词表 26 行、产品构建 2 行、生成器新增 4／删除 2 行。超过 100 行的原因是完整恢复两个已验收功能；本次没有在归档之外再追加产品代码。只合页面会丢失模组模型、凭据隔离和策略，直接用普通电话页会暴露家庭资料，均未采用。
- 【实测】S31 镜像 3209520 字节，静态 DIRAM 145746、外部 RAM 659304 字节；`build-s31/size.txt`、`size-components.txt`、`stack.txt` 已生成。这些不代表实机堆或栈水位。

## 复现命令

普通配置按总规则，额外使用只读 SDL2、ImGui 和锁定中枢环境；ASan 只换目录并加 `-DINK_SANITIZE=ON`，保留 WERROR：

```sh
export PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" INK_KEEP_EVIDENCE=1
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -c utility cmake --build build -j2
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# build-asan 按相同配置构建；再运行全量。
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/cell_features/boot.py --build build --reverse-display
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/mobile_network/reverse.py --build build
for mode in radio page ownership entry prefix wait; do
  taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/emergency_call/run.py --build build --negative "$mode"
done
export INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"
taskpolicy -c utility bash ports/esp32s31/build.sh
```

- GCC：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B "$INK_DEPS/../ink-codex/tools/gcccheck.py" build`，后接 `gcccheck-files.txt` 中 20 个文件；另将构建目录换 `build-s31-host` 检查 `ports/esp32s31/hal_stubs.c` 和 `ports/esp32s31/tests/stubs.c`。该宿主构建按端口 README 配置、只构建 `test-s31-stubs` 并运行 `unit-s31-stubs`；内存目录换 `build-s31-host-asan`、加 `-DINK_SANITIZE=ON`。
- 日志：普通全量 `full-first.log`，内存全量 `asan-tests.log`，扫描 `sanitizer-scan.log`，最终隐私 `privacy-final.log`；辅助执行脚本在工作证据目录。构建和全量测试串行分轮，最多 `-j2`，全部使用 utility 优先级；未启动带窗口的模拟器。
