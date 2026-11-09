# power-pages 执行报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。分支：`claude/codex-power-pages`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过（省电范围见下文） | 15% 通知、5% 打断、触摸/AI 开关、实际 60 秒锁屏、充电锁屏/唤醒/拔线、2% 保存及关机回执；72 条断言 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build --output-on-failure -R '^power-pages$' -j2` |
| 2 | 【实测】通过 | 四页都有快照；低电量列真实能力，充电显示电量和唤醒方式；关机区分保存成功与超时 | 同上；`python tests/power_pages_negative.py --build build` |
| 3 | 【实测】通过；【实现】代码在预算内 | 常规及 ASan/UBSan 均 225/225，无跳过；170 行 C 产品代码（含 CMake 接线 176 行） | `ctest --test-dir build --output-on-failure -j2`；`ctest --test-dir build-asan --output-on-failure -j2`；`git diff <基线> --numstat` |
| G1 | 【实测】通过 | 常规及 ASan/UBSan 均 225/225；不放宽预算 | 见复现命令 |
| G2 | 【实测】通过 | 按钮和 AI 共用 `settings.set`、`interrupt.ack`；四页快照经过实际模拟器读取 | `power-pages` |
| G3 | 按总规则第 7 节记入“要改的文档” | 共享文档留主会话统一改 | 见下文 |
| G4 | 【实测】功能及规格检查通过；视觉目标未达到 | 四张画板均先看；无动画；使用真实状态/书签；对照分数见下文 | `python tools/ui_scale_check.py system/ui/shell/power_pages.c`；`python tools/ui_compare.py --build build --out build/ui-compare-power --scenes BatteryLow BatterySaving Charging PowerOffEmpty --reuse` |
| G5 | 【实现】通过 | 只新增界面和测试；公共文件仅在末尾增加接线；没有修改服务和并行页面文件 | `git diff --stat` |

## 没做到的

- 本次在 macOS/clang 实测；未运行 Linux/gcc 或实机。
- 四页视觉分数未达到 12/255，差异如下；未改规格追分。
- 未覆盖提醒显示期间切换家人的交互；页面没有订阅家人变化，开关显示可能保留创建时的状态，能力调用仍检查当前家人权限，AI 快照仍受现有成员代次保护。按任务范围未增加监听。

## 和设计不一致的

| 页面 | 1/4 灰度平均差 | 主要差异 |
| --- | ---: | --- |
| BatteryLow | 25.431/255 | 按规范用 192 状态底圆、#888888 灰、较大字号/按钮；为验收增加了明确写出作用的省电开关。画板底圆更小且更浅。 |
| BatterySaving | 12.413/255 | 按规范用较大的字号与纯黑次要文字；三点是静态图形。 |
| Charging | 18.897/255 | 用现有主题构造更大的圆环和数字；公共状态栏没有充电闪电/锁图标；保留真实电量。 |
| PowerOffEmpty | 17.284/255 | 书签使用公开服务卡片的“在读”及真实书名/页码，没有将画板的第 148 页写死；画面明确写已关机。 |

四页统一使用现有纸白色及主题，与未同步规范的画板背景、字体和灰度有差异。`Charging` 按画板在锁屏显示；使用中接电不抢走前台页面。`BatterySaving` 按画板表示“2% 时保存后关机”，不是省电开关页。

## 改过的已有测试

无。

## 要用户定的

暂定的省电范围是复用 `power.lock_after_s`：打开为 60 秒、关闭为默认 600 秒，界面直接写“省电：1 分钟锁屏”。没有新增设置或业务策略。若要恢复用户先前自定的锁屏时长、调亮度/网络等专用省电策略，需要确定独立开关的语义；当前关掉开关会把自定时长改回 600 秒。创建页面时只对已验证的大人显示开关，能力调用继续检查当前家人权限。

## 顺手发现

暂无。

## 新增的机制

无新任务、业务状态机、文件格式、配置项、队列或后台重试。任务要求的四种界面复用现有 UI 调度、电源采样、打断和关机流程。

## 要改的文档

- 主会话更新 `docs/system/ui-dependencies.md` 第 4.2 节电源行及第 7 节关机图行：补四页实际实现、模拟器验证与视觉差异，去掉对应的“占位外观”。
- 能力表没有新增能力/设置，不需要补条目；两项操作复用已有能力。任务清单由主会话统一更新。

## 过程与验证记录

- 【实测】开工工作区干净，HEAD 等于指定基线；没有上次遗留报告或任务提交。
- 【实测】常规 `INK_WERROR=ON` 构建成功；尺寸检查 0 违规。测试没有改动任何已有期望。
- 【实测】正向四场景断言分别为 25、17、15、15；内部 RAM 最低余量均为 47,840 字节，原预算 32,768 未改。
- 【实测】反向对照用基线 `off_picture.c` 的原初始化接线重新链接同一测试，撤掉本任务整组界面；四场景退出码均为 1，分别有 4、3、4、1 条断言失败。脚本保留在 `tests/power_pages_negative.py`，不修改工作区产品代码。
- 【实现】为何超过 100 行：四页绘制、状态订阅/打断转接、AI 快照和等关机画面确实提交，合计仍在 250 行内。看过直接复用原通用打断卡/关机卡的方案：它们没有充电圆环、保存中状态及定制关机图；改公共绘制函数还会进入并行任务区域。本次只增加末尾转接，普通关机、来电和闹钟继续走原呈现函数。
- 【实测】验收曾发现同一电量因 idle→active 被重复绘制，丢掉首次触摸；集中跳过未变化的页面后通过。充电页浮层放在锁屏标记之上，重新锁屏通过；没有加等待掩盖。
- 【实测】第一次全量测试有 1 项失败：关闭空间后的关机快照漏掉“没电了”；已保留隐私过滤并补齐原因，原 `integration-power-menu` 和 `power-pages` 均通过（退出码 0），没有修改旧断言。自定义动作的低电量卡片仍由通用呈现函数处理。
- 【实测】默认字体工具 Python 缺中枢测试依赖，第一次全量有 3 项自动跳过；改用依赖目录已有的 `hub/.venv/bin/python` 后，常规全量 225/225 通过（退出码 0，215.91 秒），没有安装或升级依赖。测试时设 `PYTHONPATH="$PWD/hub/src" PYTHONDONTWRITEBYTECODE=1`，确保读本检出的中枢源码且不写只读依赖目录。
- 【实测】ASan/UBSan 全量 225/225 通过（退出码 0，378.07 秒），四页断言仍为 25、17、15、15，内部 RAM 最低余量仍为 47,840 字节。两个构建都开 `INK_WERROR=ON`，仅内存检查构建加 `INK_SANITIZE=ON`。
- 【实测】最终差异仅 9 个任务相关文件；空白检查通过，按总规则第 6 节检查绝对路径及 MAC 字样，0 命中。未提交设计稿、构建产物或证据目录。

## 复现命令

首次配置用总规则第 5 节的字体/解释器/LVGL 参数，并增加 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；`build-asan` 同样配置再加 `-DINK_SANITIZE=ON`。构建与测试均用 utility 优先级：

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -R '^power-pages$' -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/power_pages_negative.py --build build
```

已保留的对照证据在忽略目录 `build/ui-compare-power/`，含四张实际屏幕、拼接图、`metrics.json` 和分数；上表的 `ui_compare.py --reuse` 可直接重算。
