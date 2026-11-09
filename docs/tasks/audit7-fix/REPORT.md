# audit7-fix 执行报告

日期：2026-11-10。基线：`10e14629b4464e8d6a4c06319ae415d3e0506238`。在独立任务分支执行，只修 B7-01～B7-04。依赖只读使用 `$INK_DEPS`；没有设备操作或远端写入。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B7-01 | 专项通过 | 【实现】拨出、来电均产生代次并保存对方号码；入确认前重建内部绑定，摘要包含绑定，说明显示号码；结束时复用过期流程移除待确认卡；执行再核对代次和号码。【实测】原 token 失效、结果为 expired、新通话没有旧按键；基线对照失败 | `audit7-b7-01`；正反例命令见下 |
| B7-02 | 专项通过 | 【实现】序列留在通信服务供本机显示，事件不带按键值，快照仅有秘密字段的有无与位数；沿用不记参数的活动日志。【实测】实际点按合成序列后，模组和本机显示保留原值，两个快照、六个事件、实际活动记录及运行日志不泄露序列；AI 自发按键仍需确认；基线对照失败 | `audit7-b7-02`、`phone-calls` |
| B7-03 | 专项通过 | 【实现】借卡前同步写入现有 dirty 标记，失败则不借出；归还及重启走已有补建和清标记流程。【实测】电脑新增 5 B 后归还即断电，重启新文件有编号，占用从旧的 3 B 更新为 8 B；基线漏编号、统计仍 3 B | `audit7-b7-03`、`audit5-b5-02` |
| B7-04 | 专项通过 | 【实现】删去路径编辑内的两次逐文件元数据查询，保留原大小、修改时间。【实测】同配置、300 个 1 B 文件：最大持锁 14,828,930 → 289,027 μs，操作 14,828,959 → 289,056 μs，大小查询 304 → 4；300 个编号、路径、大小及总量保持正确。此为模拟计时 | `audit7-b7-04`、`index-durable` |
| 全量、内存检查、S31 构建 | 通过 | 【实测】最终普通 290/290（280.76 s），ASan/UBSan 290/290（384.75 s），CTest 均无跳过、退出 0；两套均保留 -Werror，内存检查完整日志无 sanitizer 错误；S31 完整编译脚本退出 0 | 文末构建命令；普通、内存检查及 S31 日志见下 |
| 产品代码净增 ≤150 行、隐私检查 | 通过 | 【实测】产品文件含头文件、注释净增 57 行；指定基线到最终差异的隐私正则零命中，diff 检查通过；仅 12 个相关文件 | `git diff 10e14629..HEAD --numstat -- system platform apps drivers boards ports`；`build/audit7-fix/privacy.log` |

## 没做到的

B7-01～B7-04 的指定验收无未完成项。未做实机测试、真实电话或 Ubuntu 原生 gcc 验证；S31 的 GNU 交叉编译不能替代 Ubuntu 的宿主测试。

## 和设计不一致的

本次未发现新增不一致。保留键盘本机明文显示和原子能力路径；AI 导出按任务要求收紧。既有画板分差及实机通话音频接线限制沿用原报告，不在本任务修改。

## 改过的已有测试

`tests/phone_calls/phone_calls.c`：把“完整已发送序列必须出现在快照中”的单条断言，改为“原值不出现，字段为 secret 且只显示有值”。这是 B7-02 明确要求；仍检查本机显示、模组按键、确认、成员隔离和挂断清空。其余旧期望及预算未改。新增两种专项场景和可替换源文件的手动基线入口。

## 要用户定的

暂无。

## 顺手发现

宿主链接器有重复链接 `libink_port_sim.a` 的警告，初次基线构建就已出现；两套构建均退出 0，未在本任务调整无关的链接结构。

## 新增的机制

通话代次与号码绑定：B7-01 明确要求；复用现有确认摘要、过期和结果存储。B7-02 把已有本机显示序列移至通信服务，用既有短锁复制给界面，避免按键经事件旁路导出。B7-03 复用现有卡上标记。没有新增任务、状态机、持久格式、配置项、缓存或重试流程。

## 要改的文档

由主会话更新 backlog 的 B7-01～B7-04 状态、phone-2 的快照隐私说明、index-durable 的 USB 归还与 300 文件目录计时证据；本分支不改这些共享记录或历史报告。

## 复现命令与证据

先按总规则配置并完整构建（专项可执行文件本身不生成全部字体/图标资源）。均使用 `taskpolicy -c utility`、并行度最多 2；`INK_KEEP_EVIDENCE=1` 保留证据。

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^audit7-|^phone-calls$|^index-durable$|^audit5-b5-02$'
# 手动反向对照：四项均预期退出 1。
for N in b7-01 b7-02 b7-03 b7-04; do
  INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audit7_fix/run.py --build build --case "$N" --baseline 10e14629b4464e8d6a4c06319ae415d3e0506238
  test "$?" -eq 1 || exit 1
done
```

【实测】四种最终基线对照均退出 1，模拟器进程本身正常退出 0，失败来自断言：B7-01 为 6 条（包含第二通实际收到旧按键），B7-02 为 7 条（六次事件及快照），B7-03 为借出阶段 2 条、重启阶段 2 条，B7-04 为持锁和查询次数 2 条。修复后分别为 33、58、22、611 条通过；相关七项 CTest 全部通过。反向对照只在显式 `--baseline` 下读取历史并临时编译原始产品源码；注册的四项 CTest 不依赖 Git 历史。两侧使用相同测试和依赖。

首次只构建专项可执行文件时缺字体/图标，完整构建后重测；首次过期结果查询早于落盘，按已有查询契约等待该结果集合落盘后重测。均未放宽断言或预算。复核又把确认说明的绑定参数仅用于 DTMF，其他能力保留原来的显示顺序；最终版本已再跑普通全量。

证据：`build/test-out/audit7-b7-0{1,2,3,4}{,-baseline}/summary.json`、各场景日志与 `metrics.json`；专项汇总为 `build/audit7-fix/targeted.log`。所有产物均在忽略的构建目录。

## 全量构建与检查

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
# 内存检查：同配置，目录改为 build-asan，仅增加 -DINK_SANITIZE=ON。
taskpolicy -c utility bash ports/esp32s31/build.sh
```

普通日志：`build/audit7-fix/{configure,build,ctest}.log`；内存检查对应 `build-asan/audit7-fix/`，另检查了 `build-asan/Testing/Temporary/LastTest.log`，未命中 ASan/UBSan 错误。中枢端到端及无窗口 GUI smoke 都实际执行。构建和全量测试串行进行，并行度均不超过 2。

S31 日志：`build-s31/audit7-fix/build.log`，使用现有 SDK、GNU 15.2.0，编译告警和错误扫描零命中；静态 DIRAM 83,042 B、外部 RAM 499,864 B，见 `build-s31/size.txt`。`build-s31/stack.txt` 也已生成；其中已知直接调用链不是完整栈上界，不作为实机验收。

范围核对：`git diff --stat` 只含六个产品文件、CMake 测试注册、四个测试文件及本报告。按总规则第 6 节原正则扫描基线到最终差异，隐私命中 0；未修改禁止的共享文档、历史审计资料、设备或依赖。所有修改均本地提交，未推送。
