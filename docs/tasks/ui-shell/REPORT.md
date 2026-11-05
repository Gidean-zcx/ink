# 界面壳集成修复报告

2026-11-06。此次任务起点 `4b996dc0`，先合入 `FETCH_HEAD`（`2c844243`），合并提交 `ec74e15e`。`gate-1` 只有合并失败日志，七处冲突均已解决。本报告验收此次集成修复；原界面任务报告保留在 `4b996dc0:docs/tasks/ui-shell/REPORT.md`，原复查见 [REVIEW.md](REVIEW.md)。

**【实测】普通及 ASan/UBSan 全量各 148/148，无跳过；七组撤回修复的反向对照在两套构建中均检出预期故障；成员隔离相关专项各 46 次启动、600 项检查通过。** 原有 146 项全部保留，新增两项；未降低原断言、超时、故障注入或资源预算。

## 验收表

B/T/S/N/H 是下方实际命令；日志和中间产物在忽略目录 `work/ui-shell-integration/`，不提交。

| 编号 | 结论 | 证据 | 命令或位置 |
| --- | --- | --- | --- |
| M1 合并 | 完成 | 【实现】七处冲突：构建、总规则、能力、显示、UI 任务、AI 能力测试、确认测试；同时保留 UI 与中枢测试、显示故障计数和两段刷新，采用集成分支的 D-105 规则 | `ec74e15e`；`git merge-base --is-ancestor FETCH_HEAD HEAD` 退出 0 |
| M2 启动 | 已修 | 【实测】订阅合计超过旧 24 项，电源菜单注册失败；PSRAM 中的订阅表改为 32 项。撤回容量修复，真实启动退出 2，明确报 UI 初始化失败 | T、N/watch |
| M3 刷新 | 原预算通过 | 【实测】同帧反馈与内容合并为一次 DU 预览，再 GL16 补灰；由 HAL 使用已有屏幕历史保留旧灰像素，删除重复整帧缓冲。FAST 统计实际预览完成，TEXT 仍统计最终灰阶完成。旧显示服务撤回后仍超过原 1,200 ms 上限 | T、N/display；资源表 |
| M4 预览边界 | 通过 | 【实测】新增像素测试：旧灰像素保持、黑白区域更新、GL16 补齐、错误波形拒绝且画面不变。撤回 HAL 实现，测试完成但像素断言及预算失败 | T/selftest-binary-preview；N/preview |
| M5 通知与成员 | 已修 | 【实测】提示音排队后才向列表发布；读取时校验发布代次。新增仅引用已加载成员集合的接口，核对成员生命周期且不触发冷加载；后台通知在收件人已回到设备时补发原成员事件。旧代码分别复现声音断言和原成员交错断言失败 | T/integration-notify、integration-races；S；N/notify、members |
| M6 工具清单 | 已修 | 【实测】工具清单排除不可调用事件，完整调用能力和 schema 保留；事件仍注册并经原总线转发。生产导出由 33,147 降至 28,716 字节；原确认测试 32,768 字节缓冲通过，未增大缓冲。撤回后复现原工具清单断言失败 | T/integration-confirm、integration-ai-surface、hub-contract；N/manifest |
| M7 PMIC | 已修 | 【实测】寄存器读写仅对总线超时重试，最多三次；探测仍交给原整套上电重试，NACK/其他错误立即返回。8% 故障注入保持，上电失败为 0，真实整套重试 4 次；新单测检查重试耗尽、非超时失败及 VCOM 多次写入。撤回读写修复，单测退出 3 | T/scenario-reader_basic-panel-power-faults、unit-pmic-retry；N/pmic |
| M8 中枢场景 | 通过 | 【实测】首个点击改到现有 Dock 的书架位置；原流程和全部端到端断言保留。旧坐标的定向复现为 `tool_calls=0`，新坐标两套全量及连续五次通过，连续检查 87.62 秒、退出 0 | T、H |
| M9 普通全量 | 通过 | 【实测】严格构建退出 0；最终 148/148，47.37 秒，退出 0，无跳过 | B/T；`ctest-final.log` |
| M10 内存检查 | 通过 | 【实测】相同依赖、WERROR，加 ASan/UBSan；148/148，112.93 秒，退出 0。核对编译和链接均启用 sanitizer，全量日志及 LastTest 无 sanitizer 错误诊断 | B/T；`ctest-asan.log` |
| M11 专项与反向 | 通过 | 【实测】S 两套各 46 次启动、600 项检查；N 两套各七种撤回均产生指定失败，脚本退出 0；编译失败或崩溃不冒充功能反例，启动容量反例除外（其故障本来就是启动 panic） | S/N；`services*.log`、`reverse*.log` |
| M12 结束检查 | 本轮通过 | 【实测】本轮差异空白检查、隐私、文件大小、报告链接及 I/O 返回值核查完成；只本地提交。完整 UI 分支的第三方原文空白告警保留，见顺手发现 | 下方结束检查 |

复跑命令（使用用户提供的 `INK_DEPS`、`INK_DESIGN`；依赖与设计稿只读）：

```sh
# 锁定的中枢环境：uv 0.9.5，Python 3.11.15；uv lock --check 已退出 0
UV_CACHE_DIR="$PWD/work/uv-cache" build/hub-venv/bin/uv sync --project hub --frozen \
  --python "$INK_DEPS/build-deps/pyenv/bin/python"

# B：普通配置；内存配置同参，只换目录并加 -DINK_SANITIZE=ON
cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DINK_HUB_PYTHON="$PWD/hub/.venv/bin/python"
cmake --build build -j4
# T：两套串行执行，下列 build 再换成 build-asan
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
# S/N：普通一遍，内存一遍（加 --build build-asan）；各组编译、运行均串行
UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/python" -B \
  tests/services_finish/run.py member_caps late_writers removal_alarm offpanel
UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/python" -B \
  tests/ui_shell_integration_reverse.py
# H：与 CI 的连续检查一致
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^hub-simulator-e2e$' \
  --repeat until-fail:5 --output-on-failure -j1
```

【实测】全过程构建最多 `-j4`，ctest 最多 `-j3`；同一时间只有一轮构建或测试。GUI smoke 使用 SDL dummy，无带窗口程序。安装、构建日志和反向副本均在忽略目录。

资源与延迟（毫秒、字节；模拟器模型，非实机）：

| 项目 | 修前 | 最终普通构建 | 原门槛 |
| --- | ---: | ---: | ---: |
| Plus 主页首变化 p95 | 431.915 | 295.645 | ≤400 ms |
| Plus 主页 FAST p95 | 686.628 | 535.105 | ≤600 ms |
| Plus 主页 TEXT p95 | 1,928.255 | 1,145.164 | ≤1,200 ms |
| PSRAM16 主页 TEXT p95 | 1,707.376 | 1,073.471 | ≤1,200 ms |
| AI Plus 内部最低余量 | 35,856 | 35,544 | ≥32,768 字节 |
| AI PQBN 内部最低余量 | 33,192 | 32,880 | ≥32,768 字节 |

延迟修前值来自本轮合并后、仅恢复启动的 `targeted-watch.log`；RAM 修前值来自前轮报告，未重建前轮源码。PQBN 最低内部余量仅高 112 字节，原预算未变。删除 1216×684 的预览缓冲，少占约 831,744 字节 PSRAM；字体与图标预算未调整。

## 没做到的

- 仅 macOS/Apple clang 实测，未运行 Linux gcc 或远端 CI；未接设备、真实厂商、真实家庭数据或窗口 GUI。
- 本轮只修集成和全量阻塞，未重做 30 屏视觉对照，也未处理原复查 UI-R4-001/002 的尺寸/图标检查器缺口、字体 4 MiB 目标及剩余页面流程。不把全量测试通过等同于原 U1～U13 整体验收通过。

## 和设计不一致的

- 预览只把原黑白像素更新为二值目标，原灰阶像素留给后续 GL16；不声称预览整帧已经纯黑白。这样避免用 DU 驱动灰阶转换，最终 TEXT 延迟包含完整补灰。新增 HAL 标志只在模拟器端口验证，未来真机端口仍须实现和校准。
- 未改设计文档迁就代码；原完整外观与流程欠项仍见前轮报告和复查。

## 改过的已有测试

- 合并 `confirm.c` 时保留集成分支 D-105 的直接执行、无确认卡和撤销断言，替换过时的普通系统设置等待确认期望；高风险确认、只能由人接受等断言保留。
- `ai_surface.c` 合并两边能力清单、集成分支的可选参数 schema；修复时只追加事件不得成为可调用工具的断言。
- `races.c` 原交错、成员隔离、等待阈值不变，只追加缓存命中、错误生命周期拒绝、冷集合不加载三条断言。
- `hub/e2e.script` 只更新首个书架点击坐标；原后续脚本及 Python 断言不变。
- 新增 `selftest-binary-preview`、`unit-pmic-retry` 和独立反向脚本。没有删除、跳过已有测试或修改已有预算；新增预算只约束新增像素测试。

## 要用户定的

无。按现有授权完成可逆修复；无 push、PR、发布、硬件或付费操作。

## 顺手发现

- 【实现】服务/策略增量逐项为：PSRAM 订阅容量、工具清单的事件过滤、显示调度与 HAL 预览标志、通知发布代次与原成员事件、仅引用已加载成员数据的接口。PMIC 读写重试是故障注入阻塞的最小驱动修复。没有新增常驻任务；按钮和 AI 的业务入口仍为原子能力，缓存和预览属于内部机制（R-061）。
- 【实测】本轮修复相对 `ec74e15e` 的空白检查退出 0；完整分支相对集成分支仍有 Outfit-OFL、NanoSVG 许可证与原栅格器缩进告警，未批量改第三方原文或历史审计资料。
- 【实测】相对合并基线和集成分支的隐私正则均 0 命中；48 个改动 C/H 去掉注释和字符串后，`write/read/fgets` 实际调用为 0，没有新增忽略返回值的写法；这不能替代 Linux CI。报告相对链接和本轮文件大小检查通过，生成物不提交。
