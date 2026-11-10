# small-gaps-2 执行报告

基线：`e65bfc261b68e9fb063fc60da507122119ae4fc6`。仅在任务分支本地提交，不推送；未接触设备。执行中，下面的待验证不表示完成。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 继续读 | 【实测】定向测试通过 | 标题、按正文当前页起点字节比例显示百分比；停在今日屏插拔卡自动刷新；跨重启、成员隔离保留。`work/small-gaps-2/reader-after.log` | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/reader_continue.py --build build` |
| 1.2 全部页图标 | 【实测】通过 | 给三份注册声明补 `.icon`；复用现有 SVG、许可与构建检查。改后 19 条检查通过，撤回三处声明后 3 条失败 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/small_gaps_2/run.py --build build --case icons` |
| 1.3 拨号页 | 【实测】通过 | 提示移到拨号按钮左侧，100%/120% 下均与 12 个原尺寸数字键不重叠；改前失败 6 条，改后 94 条通过 | 同上，`--case phone`；`work/small-gaps-2/phone-after.log` |
| 1.4 飞行模式 | 【实测】通过 | 控制面板关闭飞行模式后收起；AI 的“关闭飞行模式”错误面板也收起；仅操作成功时关闭，失败走原拒绝提示。原测试与新增测试 2/2 通过 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(airplane-mode|small-gaps-2-airplane)$' --output-on-failure -j2` |
| 1.5 日期时间选择 | 【实测】通过 | 两页复用 `ink_ui_datetime_picker`；5 行文字和 10 个按钮逐一比坐标，并检查加减均使用实际打包图标；原 `time-settings`、`reminder-edit` 未改，3/3 通过，见 `work/small-gaps-2/picker-icons-test.log` | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(small-gaps-2-picker|time-settings|reminder-edit)$' --output-on-failure -j2` |
| 1.6 计算器 120% | 【实测】无需改产品代码 | 63 字符算式完整分两行，文字高度不超容器，标题、算式、结果和按键不重叠；已查看 `work/small-gaps-2/before/calculator/snap-calculator-120.png`，补 100%/120% 回归截图 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/small_gaps_2/run.py --build build --case calculator` |
| 2.1 普通全量 | 最终图标修正后待复测 | 上轮 `99% tests passed, 1 tests failed out of 352`，467.67 秒、退出 8；仅能力清单失败 | 见下方命令；`work/small-gaps-2/ctest-final.log` |
| 2.2 内存检查 | 最终图标修正后待复测 | 上轮完整 ASan/UBSan 原话：`99% tests passed, 1 tests failed out of 352`，654.38 秒；仅能力清单失败，无 sanitizer 错误 | 见下方命令；`work/small-gaps-2/ctest-asan.log` |
| 2.3 S31 | 【实测】通过 | 编译、链接、资源包、尺寸和栈报告退出 0；镜像 3,210,144 字节。无 C/C++ 编译告警；5 条 SDK 私有 include 配置警告，未改 SDK、未屏蔽 | 见下方命令；`work/small-gaps-2/s31.log`、`build-s31/size.txt`、`build-s31/stack.txt` |
| 2.4 GCC | 【实测】通过 | 原话 `gcccheck: 13 compiled, 0 with findings`，无跳过 | 见下方命令；`work/small-gaps-2/gcccheck.log` |
| 3 改前改后截图 | 待生成 | — | — |

## 截图对照

改前用基线或单独撤掉对应修复生成，改后由最终定向复测保留。均是模拟器实际输出的 PNG，不编辑截图，不提交生成文件；改后文件待本轮全量结束后统一留存。

| 内容 | 改前 | 改后 |
| --- | --- | --- |
| 继续读标题和百分比 | `work/small-gaps-2/reverse/reader-out/read/snap-continue-progress.png` | `work/small-gaps-2/after/reader/read/snap-continue-progress.png` |
| 停在今日屏拔卡 | `work/small-gaps-2/reverse/reader-card-out/restart/snap-continue-removed.png` | `work/small-gaps-2/after/reader/restart/snap-continue-removed.png` |
| 停在今日屏插卡 | `work/small-gaps-2/reverse/reader-card-out/restart/snap-continue-inserted.png` | `work/small-gaps-2/after/reader/restart/snap-continue-inserted.png` |
| 全部页：时钟 | `work/small-gaps-2/reverse/icons-out/snap-apps-1.png` | `work/small-gaps-2/after/icons/snap-apps-1.png` |
| 全部页：相机、相册 | `work/small-gaps-2/reverse/icons-out/snap-apps-2.png` | `work/small-gaps-2/after/icons/snap-apps-2.png` |
| 拨号提示，120% | `work/small-gaps-2/reverse/phone-out/snap-phone-120.png` | `work/small-gaps-2/after/phone/snap-phone-120.png` |
| 关闭飞行模式后 | `work/small-gaps-2/reverse/airplane-out/snap-airplane-control-off.png` | `work/small-gaps-2/after/airplane/snap-airplane-control-off.png` |
| 提醒日期时间选择器 | `work/small-gaps-2/reverse/picker-out/snap-reminder-picker.png` | `work/small-gaps-2/after/picker/snap-reminder-picker.png` |
| 时间设置选择器 | `work/small-gaps-2/reverse/picker-out/snap-time-picker.png` | `work/small-gaps-2/after/picker/snap-time-picker.png` |
| 计算器，120%（产品代码未改） | `work/small-gaps-2/before/calculator/snap-calculator-120.png` | `work/small-gaps-2/after/calculator/snap-calculator-120.png` |

## 没做到的

- 共享能力清单未更新，故普通和 ASan/UBSan 全量均为 351/352，完整验收未全绿，见“要改的文档”；未屏蔽、跳过或放宽此检查。
- 没有运行原生 Linux CI 或连接设备；S31 是完整编译与链接，内存结果限宿主模拟器。
- 未迁移缺少 `offset` 的旧版 `reader.last`：旧记录能继续打开原页，卡片百分比初始按 0% 显示，翻页保存后补全。本次新保存记录已验证重启后保留准确比例。

## 和设计不一致的

- 暂无。保留 `SetDateTime` 的五行加减选择样式：它直接表达增减，原有时间设置的字段、按钮顺序和能力合同可保留；提醒页改用同一组件。截图发现原文字减号缺字，故共用组件改用已有 `plus`／`minus` 图标；两页保留原能力的参数适配及日期校验，未改能力名、权限、保存动作或提醒年份范围。

## 改过的已有测试

- `tests/reader_continue.c`：增加百分比、标题、插拔卡不离开今日屏的断言与截图；原页数、重启和成员隔离期望保留。`read_to` 改为格式化页码，增加第 20 页的非零百分比用例。
- `tests/airplane_mode.c`：增加成功关闭后不再停留 `AiOffline` 的断言，未修改原断言。
- `tests/power_pages.c`：关机书签标题的精确期望由 `《星星邮差》第 1 页` 改为 `《星星邮差》第 1 页 · 0%`。它复用阅读卡片的正文，任务 1 新增百分比后这处可见文字随之变化；保留精确匹配、标题与保存结果不重叠的断言。

## 要用户定的

- 暂无。

## 顺手发现

- 图标的 SVG 和 manifest 在基线已经完整；根因是三份应用注册漏填 `.icon`，因此不新增重复资源。

## 新增的机制

- 任务 1：复用原 `reader.last`，增加可选 `offset` 字段以保留百分比；旧记录仍能读取。没有新文件、新定时器或任务。
- 任务 1：阅读卡片提供者订阅现有 `storage.card` 状态，复用原卡片版本事件和刷新节流。
- 任务 5 明确要求统一选择器：从原 `SetDateTime` 提取一个无状态 UI 组件；没有新增任务、线程、队列、缓存、配置项或状态机。

## 要改的文档

- `docs/system/capabilities.md`：合并时重新生成；阅读模块关联状态由“—”改成 `storage.card`，生成差异仅这一行。按总规则第 7 节未修改共享文件；本地生成及检查在 `work/small-gaps-2/capabilities.md`，检查通过。

## 复现命令与范围

配置使用总规则第 5 节的全部字体、Python 和 LVGL 参数；另指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`、SDL2/ImGui 的只读依赖覆盖、`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`。ASan 只换 `build-asan` 目录并加 `-DINK_SANITIZE=ON`，保留 WERROR。构建和完整测试逐轮串行，均为 utility 优先级、并行上限 2。

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(small-gaps-2-.*|reader-continue|time-settings|reminder-edit|airplane-mode|power-pages)$' --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/small_gaps_2/reverse.py --build build
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build \
  apps/reader/reader.c system/apps/calendar/calendar.c system/apps/camera/camera.c \
  system/apps/clock/clock.c system/apps/gallery/gallery.c system/apps/phone/phone.c \
  system/ui/shell/navigation.c system/ui/shell/time_settings.c system/ui/theme.c \
  tests/reader_continue.c tests/airplane_mode.c tests/power_pages.c tests/small_gaps_2/ui.c
```

反向对照只编译忽略目录内的临时对象并重新链接，不改工作区产品文件；这是手动模式，依赖指定基线提交，没有注册进 CTest。原时间设置、提醒编辑测试保持原样。

【实测】手动反向对照命令退出 0，七组均检出原问题，且驱动均执行结束：基线阅读器 7 条失败；仅撤掉存储卡订阅 1 条；图标 3 条；拨号页 6 条；控制面板 1 条；AI 飞行模式面板 1 条；旧提醒选择器 17 条。证据 `work/small-gaps-2/reverse/summary.json`、`work/small-gaps-2/reverse.log`；基线程序本身的非零退出是预期结果。

【实现】架构审视：共用选择器只负责画控件，日期草稿与校验仍归各页面；阅读进度按原成员目录保存，卡片通过原版本事件刷新。按钮和 AI 继续调用同一批原子能力，卡片百分比和两处选择结果仍在既有 AI 快照中；没有新服务或模拟器头文件依赖。

【实现】产品代码新增／替换 80 行、删除 35 行，增删合计 115 行，均低于 250 行预算。若按增删相加超过 100 行计：主要开销是共同绘制、公开声明和两侧能力参数适配。看过的更小方案是只对齐两页尺寸（仍保留重复选择器），或让提醒直接用时间设置页（会改变设备时钟及原能力合同），均不能满足任务 5，故保留无状态组件和原能力适配。
