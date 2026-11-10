# open-app-latency 任务报告

基线：`5cc0a6ebefceb8f28d7b0e49b7d99e63cef014d3`。改动提交：`942f3266`。状态：测量、优化与验证完成；两轮全量唯一未过项可在原基线复现。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 五个入口 | 【实测，模拟器】通过 | 前后各 100 次有效点按；短信从两次刷新减为一次，五页的最终帧缓冲哈希均一致 | A、B |
| 1b 确定性 | 【实测】通过 | 原有 `determinism`、`determinism-reader` 在普通版和 ASan/UBSan 版都通过 | C、D |
| 2a 普通全量 | 【实测】未全绿，基线同样失败 | 362/363 通过；`integration-dictation` 的 7 条断言在基线和优化版完全相同 | C、基线复核 |
| 2b 内存检查 | 【实测】无 sanitizer 错误；用例未全绿 | ASan/UBSan 全量 362/363，仅同一听写用例失败；测量驱动额外跑完五个入口 | D |
| 2c S31 | 【实测】通过 | 完整编译、链接、资源打包和尺寸／栈报告退出 0 | E |
| 2d gcccheck | 【实测】通过 | 2 个产品 C 文件和 1 个测量驱动 C 文件，0 findings，无跳过 | E |

## 前后对比

【实测，模拟器】每个入口每版 20 次。单位 ms，每格为 p50 / p95；从电容屏事件的按下或释放，到首个含有效内容的帧对应的波形完成。

| 入口 | 释放到可读：前 | 释放到可读：后 | 按下到可读：前 | 按下到可读：后 |
| --- | ---: | ---: | ---: | ---: |
| 设置 | 955.77 / 993.65 | 945.38 / 983.19 | 1065.78 / 1103.66 | 1055.38 / 1093.20 |
| 文件 | 839.27 / 850.30 | 826.71 / 831.56 | 949.28 / 960.31 | 936.71 / 941.55 |
| 短信 | 1590.74 / 1600.74 | 809.47 / 813.54 | 1700.75 / 1710.75 | 919.48 / 923.55 |
| 相册 | 1445.31 / 1454.59 | 1430.17 / 1438.45 | 1555.32 / 1564.60 | 1540.17 / 1548.46 |
| AI | 800.26 / 805.59 | 783.34 / 790.09 | 910.26 / 915.60 | 893.35 / 900.10 |

【实测，模拟器】以下只拆释放之后的时间，同样为 ms、p50 / p95。分位数分别取各列第 10 和 19 个值，不能把一行的分位数直接相加；[逐样本数据](samples.ndjson)中七段之和等于该次总时间（舍入误差除外）。

| 入口 | 输入 | 能力调用 | 页面创建 | 应用数据读取 | 布局 | 送屏 | 刷新 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 设置前 | 12.65 / 13.16 | 8.02 / 9.01 | 44.46 / 59.41 | 0.00 / 0.00 | 7.43 / 8.58 | 267.17 / 287.69 | 617.48 / 617.49 |
| 设置后 | 12.65 / 13.16 | 7.94 / 9.41 | 43.83 / 53.06 | 0.00 / 0.00 | 7.42 / 8.35 | 257.31 / 281.74 | 617.48 / 617.49 |
| 文件前 | 12.65 / 13.09 | 1.52 / 1.74 | 16.29 / 17.96 | 0.00 / 0.00 | 1.81 / 2.03 | 189.82 / 197.72 | 617.48 / 617.49 |
| 文件后 | 12.65 / 12.68 | 1.55 / 2.06 | 16.32 / 17.54 | 0.00 / 0.00 | 1.82 / 1.99 | 176.73 / 180.72 | 617.48 / 617.49 |
| 短信前 | 11.01 / 11.03 | 1.61 / 1.95 | 11.60 / 12.11 | 6.23 / 6.84 | 2.55 / 2.86 | 322.72 / 331.86 | 1234.96 / 1234.97 |
| 短信后 | 11.02 / 11.06 | 1.68 / 2.03 | 11.03 / 11.98 | 5.27 / 6.14 | 1.73 / 1.91 | 161.14 / 164.74 | 617.48 / 617.49 |
| 相册前 | 11.01 / 11.04 | 0.81 / 1.21 | 7.12 / 10.61 | 40.79 / 44.68 | 2.39 / 2.81 | 309.11 / 317.21 | 1072.24 / 1072.25 |
| 相册后 | 11.01 / 11.05 | 0.87 / 0.97 | 7.47 / 9.67 | 39.80 / 45.42 | 2.39 / 2.70 | 295.13 / 302.75 | 1072.24 / 1072.25 |
| AI 前 | 12.68 / 12.74 | 2.50 / 3.15 | 6.53 / 7.60 | 0.00 / 0.00 | 1.67 / 2.01 | 159.71 / 164.22 | 617.48 / 617.48 |
| AI 后 | 11.04 / 11.10 | 2.34 / 2.96 | 6.37 / 7.85 | 0.00 / 0.00 | 1.82 / 2.07 | 143.43 / 149.94 | 617.48 / 617.49 |

【实现】每轮用同一份已配置的合成 Flash 镜像新开模拟器进程，SD 目录各自初始化。设置、相册点主屏图标，文件、短信、AI 对话记录点“全部”里的实际图标；进入启动页的准备操作不计时。文件测分类首页，短信、相册、AI 测空列表；没有在线请求。复用触摸驱动、LVGL 与显示 trace 和平台 probe。测试确认目标页已有实际内容（相册须退出读取状态），按送屏编号找对应波形，排除只出了页头的帧。

【实现】输入止于 `shell.launch` 调用；能力调用扣除其中的页面创建、应用读数据和布局。布局来自 LVGL 的实际调用；应用数据包含状态恢复、短信集合和相册索引读取，设置、文件、AI 的内存取值归在页面创建。送屏包括等待绘制、像素绘制和旋转、队列、上电及差分计算；先刷了加载页时，两次波形都计入刷新。并行区间只计一次，优先级为刷新、输入、布局、数据、创建、能力、送屏。

【实测，模拟器】使用同机 `RelWithDebInfo`、`metalio-plus.json` 和 `--cpu measured`；档案的宿主 CPU 缩放 40.0 未动。点按按住约 110 ms。前后分别有 308／311 条模拟内核的长 CPU 段告警，照原样保留在本地日志；两轮各 600 项驱动检查通过，逐样本未见显示欠载恢复。设置的总时延收益约 10 ms，宿主波动比其它单次刷新页面明显。

## 最大开销和改动

- **刷新。**【实测，模拟器】一次全区 GC16 约 617 ms，占三个单刷入口基线 p50 的约 65%～77%。冷开短信读数据只占 6.23 ms，但数据未就绪时先发了一张只有页头的帧，第二次再全区刷新。现在只有冷开的短信列表在发出原异步读取后重置现有 LVGL 刷新计时器，给回执一个现有的 30 ms 刷新周期。读得慢仍可显示原来的初始页；读取、成员校验和刷新意图不变。20/20 次从两次 GC16（96 相）降为一次（48 相），p50 少 781 ms，约 49%。
- **送屏。**【实测，模拟器】整区差分一次约 77.5 ms，需重新上电时约 35 ms。改动只针对其中的软件旋转：90° 按 32 行分段，沿原生缓冲区连续写；每 32 行的让出点保留。设置／文件／AI 的旋转 p50 分别为 124.36→111.97、74.70→61.50、40.26→23.94 ms；五页最终原生帧缓冲哈希均与基线相同。其余角度沿用原路径，没有改档案中的旋转成本。
- **相册和数据。**【实测，模拟器】相册的第二次刷新仍存在（48 相 GC16 + 37 相 GL16）。基线应用读取 p50 约 41 ms，其中 SD 模型读 13.90 ms、写 10.06 ms。全局重置一周期试验（每入口 5 次）没有减少相册刷新，还使文件和 AI 的 p50 各增加约 35 ms；已撤。仅在相册重置一周期并跳过不存在的公共照片目录（8 次）只有 4 次合并成功，p95 1479.35 ms，仍高于基线 1454.59 ms；两处都撤。没有加等待、缓存或调整文件服务。

【实现】产品代码新增 29 行、删除 9 行，合计 38 行（预算 150）。架构审视：两处分别归在短信页和 UI 送屏代码；触摸、AI 继续调用原有能力，成员数据和页面快照走原接口。

## 验证与复现

在仓库根目录设置 `INK_DEPS`。构建配置按总规则第 5 节，使用 CMake 默认的 Unix Makefiles，另外只读复用 SDL2、ImGui 源码与已锁定的中枢虚拟环境；构建和全量测试不重叠，均为 utility 优先级、并行数 2。采样单进程串行，无本会话的其它构建或测试并行；沙箱不允许列出整机进程。

```sh
py="$INK_DEPS/build-deps/pyenv/bin/python"
export PYTHONDONTWRITEBYTECODE=1
export PYTHONPATH="$PWD/hub:$INK_DEPS/hub/.venv/lib/python3.11/site-packages"
# C：普通构建和全量测试；D：将 build 换为 build-asan，配置时加 -DINK_SANITIZE=ON，
# 运行 CTest 时设置 ASAN_OPTIONS=halt_on_error=1 和 UBSAN_OPTIONS=halt_on_error=1。
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" -DPython3_EXECUTABLE="$py" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DFETCHCONTENT_SOURCE_DIR_SDL2="$INK_DEPS/build/_deps/sdl2-src" \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI="$INK_DEPS/build/_deps/imgui-src" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2

# A：默认各 20 轮；使用新的输出目录。基线和反向对照需要本地 Git 历史。
bench=tests/open_app_latency/run.py
o=work/open-app-latency/reproduce
taskpolicy -c utility "$py" "$bench" --build build --reverse all --out "$o/before"
seed="$o/before/seed/user.img"
base="$o/before/samples.json"
taskpolicy -c utility "$py" "$bench" --build build --seed "$seed" --compare "$base" --out "$o/after"
# B：只换回一处源文件，两个命令均预期退出 1。
taskpolicy -c utility "$py" "$bench" --build build --seed "$seed" --compare "$base" --reverse rotate --out "$o/rotate"
taskpolicy -c utility "$py" "$bench" --build build --seed "$seed" --compare "$base" --reverse sms --out "$o/sms"

# E：S31 只编译；gcccheck 使用实际编译参数，手工驱动的编译表由基准脚本写出。
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh
gcccheck="$INK_DEPS/../ink-codex/tools/gcccheck.py"
taskpolicy -c utility "$py" "$gcccheck" build system/ui/ui_task.c system/apps/sms/sms.c
taskpolicy -c utility "$py" "$gcccheck" "$o/after" tests/open_app_latency/device.c
```

【实测】B 两个反向对照均按预期退出 1，驱动检查和最终帧缓冲哈希均通过。仅恢复原旋转：文件、AI 的旋转 p50 为 74.05、38.48 ms，均未达到基线下降 10% 的断言；仅恢复原短信代码：20/20 次恢复到两次刷新（96 相）。正向比较退出 0。日志及 trace 位于 `work/open-app-latency/`，不提交。

【实测】C 构建退出 0，CTest 退出 8，原话 `99% tests passed, 1 tests failed out of 363`，333.16 s，无跳过。基线复核：用两个原始 C 文件按同一编译表重链接 `sim-dictation`，执行原 `integration-dictation` 命令；当前版也原样单跑。两版均退出 1、220 条通过、7 条失败，`metrics` 逐项相同；第一条失败为点“说完了”后状态未回到空闲（`over (2)`）。在基线检出按 C 配置可用 `ctest --test-dir build -R '^integration-dictation$' --output-on-failure` 单跑；本次的重链接命令和原始日志在 `work/open-app-latency/dictation-control*`。按任务范围未改听写代码或断言。

【实测】D 构建退出 0，ASan/UBSan 全量 CTest 退出 8，原话 `99% tests passed, 1 tests failed out of 363`，546.12 s，无跳过。仍是同一听写用例的 7 条断言，完整 CTest 日志未发现 sanitizer 错误。另用 A 的优化版命令换 `--build build-asan --runs 1` 并去掉 `--compare`，五个入口共 30 条驱动检查全部通过，退出 0。

【实测】E 的 GCC 原话：`gcccheck: 2 compiled, 0 with findings` 和 `gcccheck: 1 compiled, 0 with findings`；两次退出 0，无跳过。测试驱动实际使用 B 的 `reverse-rotate` 输出目录里的编译表，编译的是当前驱动源码。

【实测】E 的 S31 使用本地 ESP-IDF v6.1（`fff9895c`）和 GCC 15.2.0；两个产品 C 文件都重新编译，`Total image size: 3267634 bytes (.bin may be padded larger)`。`build-s31/` 生成镜像、尺寸和静态栈报告；有 5 条 SDK 私有头目录配置警告、1 条 Bash 补全警告，无产品 C 编译告警。没有连接设备。

【实测】收尾：200 条样本、90 个分位数单元格重算一致，每次七段与总时长相差不超过 0.002 ms。报告链接、Python 语法、`git diff --check` 均通过。对照基线只有报告、样本、两个测量文件和两个产品文件；按总规则扫描提交差异，绝对路径及 MAC 模式均 0 命中。

## 没做到的

- 相册还会先显示加载状态再更新。现有一个刷新周期无法稳定涵盖 SD 读取；保留慢盘的加载提示。
- 未运行真机或原生 Linux。真机先核对冷／热闪存和 SD 读速、首个可读波形、残影和相位、上电与送屏拷贝时间；本报告不能用模拟器的 617 ms 推导真屏时间。

## 和设计不一致的

【实现】[界面规范 §5](../../product/system-ui-spec.md#5-给实现的提醒)规定打开页面清刷内容区，状态栏和 Dock 有变化才刷，GC16 为 36 相。基线及改后在所测档案里，整区 GC16 都触发了模拟器的 48 相分支；仍会重画整个新页面。原厂只读副本 CloudZao/Metalio-E-INK4-Plus `690a023`（2026-11-11 核查）中，`main/boards/metalio-e-ink4-plus/display/highlevel.c:205–243` 先按区域比较前后帧，`display/waveforms/my_waveform.h:10` 的均衡档为 GC16／GL16 36／37 相。仓库的 S31 端口则走 [PQ-BN 驱动路径](../../../ports/esp32s31/hal_display_s31.c#L207)。本任务未改刷新区域、档案波形或 D-110 算法。

## 改过的已有测试

无。新脚本手工运行，使用构建的 `sim-configured` 重新链接带探针的临时对象；探针不进产品，也不改变 CTest 的注册。

## 要用户定的

相册是否值得再做首帧合并，建议拿到真实 SD 和面板的时序后决定；保留当前加载提示。

## 顺手发现

设置打开时可同时见到后台 SD 活动，目标页本身无外部数据读取。短信本场景的 Flash 读模型时间仅 1.79 ms，不能据此判断实机是否也会在一周期内准备好。

## 新增的机制

- 产品任务、定时器、状态机、文件格式、配置项：均无。短信复用现有 LVGL 刷新计时器。
- 模拟测试任务 `test.open-app`：任务书要求从触摸到可读做前后量化；只在手工基准程序中启用。

## 要改的文档

主会话合并时可据本报告更新 `docs/tasks/backlog.md`；共享文档未改。
