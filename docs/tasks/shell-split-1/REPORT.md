# shell-split-1 执行报告

基线：`bf95f981b9594b563d23d43fe7667513d8cca49d`。任务仅拆分外壳文件，保持行为与对外接口。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 未全通过 | 【实测】普通与 ASan 构建成功；S31 宿主机 77/77、固件编译通过，无新增警告，固件增加 752 字节。普通仍有 4 项检查、ASan 有 1 项旧插桩未通过；原始统计见过程记录。GCC 为 `6 compiled, 0 with findings` | 见下文复现命令 |
| 2 | 通过 | 【实测】普通、ASan 各 81 个场景、243 张 PNG，全部 RGBA 像素与基线相同，各变化像素 0；正常及最大字号、儿童场景全部覆盖 | `python docs/tasks/shell-split-1/verify.py --images-before work/shell-split-1/before --images-after build/test-out`；将后者换为 `build-asan/test-out` 同样通过 |
| 3 | 未通过；能力元数据不变 | 【实测】109 个函数体逐字相同，33 项定义不变；运行时 513 个能力、43 项设置与基线完全一致。标准文档检查报过期，见“没做到的” | `python docs/tasks/shell-split-1/verify.py --baseline bf95f981b9594b563d23d43fe7667513d8cca49d`；`python tools/capabilities.py --check` |
| 4 | 通过 | 【实测】文件分工如下；胶水代码 80/80 行，包含尺寸说明注释和 CMake 的 5 行增量 | `wc -l system/ui/shell/{navigation,status_bar,shell_home,control_center,settings_body,today_edit,shell_internal}.*` |

### 文件分工

| 文件 | 行数 | 内容 | 后续修改边界 |
| --- | ---: | --- | --- |
| `status_bar.c` | 128 | 时间、日期、状态栏与顶栏；电量和信号绘制 | 状态栏任务 |
| `shell_home.c` | 541 | 主屏、今日卡片、全部应用、Dock、儿童用时与应用筛选 | 主屏任务；`graft-mogo-1` 的桌面入口 |
| `control_center.c` | 196 | 成人与儿童控制中心、滑杆、开关、通知摘要 | 控制中心任务 |
| `settings_body.c` | 752 | 设置页定义、正文、分页、网络回执与设置项定义 | 设置任务 |
| `today_edit.c` | 278 | 今日编辑、图标菜单、原有成员集合与私有状态 | 今日编辑任务 |
| `shell_internal.h` | 119 | 原共享状态布局、跨文件声明、原有小型共用函数 | 多任务共用；改声明需协调 |
| `navigation.c` | 407 | 导航能力、绑定、返回和手势、共用刷新与初始化 | 导航公共部分 |

后续任务除 `graft-mogo-1` 外尚未在本基线分配正式编号。应用注册表与对外 `ink/ui_shell.h` 均未修改。

## 没做到的

- 标准 `tools/capabilities.py --check` 在基线通过，拆分后报 `capability catalogue is stale`。生成器把定义文件及按文件划分的关联面写进文档；搬移定义必然改变这些内容。总规则禁止修改共享生成文档，因此保留原文件，未伪造通过。
- 普通全量仍有 4 项需合并时迁移：
  - `ui-scale-regressions`：`tests/ui_scale_check_test.py:151` 固定要求 `navigation.c` 内出现文本框样式调用；调用现已分别搬至主屏和设置。
  - `i18n-literals`：`tests/i18n_literals.py:27` 的 AI 快照白名单固定在旧文件名；45 个报告项都是原快照内容。手动诊断用相同源码、旧文件名扫描得到 0 项，证实是路径耦合。
  - `semantics-shell2-home`：`tests/semantics_shell2/home.c:5` 的宏只包住 `navigation.c`，未覆盖另行编译的 `shell_home.c:540`；原快照语义转换未被测试拦截，2 个断言失败。
  - `capability-table-doc`：同标准能力文档检查，定义位置与关联面改变。
- 第一轮 `ui-scale` 的 8 项是尺寸说明注释漏分配，已把原有说明分配到对应文件；复查原话 `100% tests passed out of 1`，函数体及 80 行预算不变。未修改检查器。
- 本机为 macOS，未执行 Linux CI；GCC 检查是使用现有 RISC-V GCC 按宿主编译参数检查这 6 个 C 文件，不替代 Linux 全量运行。按任务规定未推送或启动远端 CI。

## 和设计不一致的

- 【实现】能力名称、描述、页面名、快照字段和函数体保持原样；仅文件位置、内部链接可见性及共用声明变化。没有产品设计变更。

## 改过的已有测试

- 无；新增手动核对脚本，不注册依赖 Git 历史的 ctest，不放宽任何断言或预算。

## 要用户定的

- 无产品取舍；上列 3 个旧测试文件及生成能力文档需由合并会话统一迁移。本任务未越界修改。

## 顺手发现

- 若继续运行手动反向脚本，`tests/fw_f8/reverse.py`、`tests/notify_manage_reverse.py` 等仍以旧单文件为目标，也需按部件迁移；未在本任务修改或重跑这些历史对照。
- `build.sh` 在基线没有执行位，直接运行被拒；实际通过 `taskpolicy -c utility bash ports/esp32s31/build.sh` 执行原脚本，未改文件模式。

## 新增的机制

- 无；只增加任务书要求的文件拆分及内部声明。

## 要改的文档

- `docs/system/capabilities.md`：由合并会话重新生成定义链接与关联面；能力与设置元数据不变。本任务只把生成预览放在忽略目录 `work/shell-split-1/capabilities-generated.md`。

## 过程记录

- 开工：工作区干净，当前提交与指定基线一致；已读任务书与总规则。后续只在本分支提交，不推送。
- 基线：普通构建成功；81 个场景及能力导出共 `100% tests passed out of 82`。保存 243 张 PNG、完整能力导出与 S31 大小报告到忽略目录 `work/shell-split-1/before/`。基线 S31 编译成功，`ink-s31.bin` 为 4,998,976 字节。
- 拆分：109 个函数体（含放入内部头的 6 个小型共用函数）与原文逐字比较；33 项定义与原共享结构字段原文不变。23 个函数改为内部跨文件可见；`N` 仍只有一份定义并保留 `INK_PSRAM_BSS`，`T` 仍为私有 PSRAM 状态。尺寸检查注释随代码分配，保留原来的尺寸来源说明。
- R-061 审视：【实现】这里只拆展示部件；原能力注册、按钮与 AI 调用路径、成员检查和快照保留，不新增业务入口或状态机制。
- S31：【实测】原 `build.sh` 改前、改后均退出 0；固件 4,998,976 → 4,999,728 字节，增加 752 字节（0.015043%）。编译器警告 0 → 0；ESP-IDF 另有 6 条依赖目录配置警告，逐块比对与基线相同，新增警告 0。未连接或操作设备。
- 普通全量原话：`99% tests passed, 5 tests failed out of 423`，耗时 974.96 秒；其中 3 项中枢测试跳过。修正尺寸注释后 `ui-scale` 单项通过；随后只读使用 `$INK_DEPS/hub/.venv` 补跑中枢 3 项、依赖夹具及 `ui-scale`，原话 `100% tests passed out of 5`（60.12 秒），原跳过项均通过。剩余 4 个失败没有修改或跳过。
- 内存检查：`INK_SANITIZE=ON` 同时启用 AddressSanitizer 与 UBSan；以 `UBSAN_OPTIONS=halt_on_error=1` 跑全部 81 个场景和外壳相关测试，共 112 项。原话 `99% tests passed, 1 tests failed out of 112`（193.65 秒），111 项通过，唯一失败是上述 `semantics-shell2-home` 的同一组断言；无 ASan/UBSan 错误。
- S31 宿主机：独立配置、构建 `ports/esp32s31/tests`，原话 `100% tests passed out of 77`（120.68 秒）。
- 最终普通构建补跑全部界面场景，原话 `100% tests passed out of 81`（25.99 秒）；再次比对 243 张截图，全部像素与基线一致。
- 收尾：【实测】代码里程碑提交 `f4e83e44`。`git diff --stat <基线>..HEAD` 核对共 11 个范围内文件；`git diff --check` 退出 0。按总规则第 6 节的原隐私正则扫描暂存区及基线到 HEAD 的差异，均为 0 命中（grep 退出 1 表示无匹配）。未夹带构建产物、设计稿或共享文档改动；只作本地提交，未推送。

### 复现命令与本地证据

命令均在仓库根目录执行。使用任务给定的 `$INK_DEPS`；所有构建和测试均限制为 2 并发并使用 utility 优先级。

```bash
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

内存构建用同样配置，把目录换成 `build-asan` 并加 `-DINK_SANITIZE=ON`，随后执行：

```bash
taskpolicy -c utility cmake --build build-asan -j2
PYTHONDONTWRITEBYTECODE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2 \
  -R '^ui-scene-|^ui-shell-|^semantics-shell|^i18n-shell|^settings-|^notify-manage$|^fw-f8$|^activity-card$|^airplane-mode$|^kid-|^lock-faces$|^bluetooth-settings$|^time-settings$|^ui-scale$'
```

构建与原始测试输出在忽略目录 `work/shell-split-1/`：`build-after.log`、`build-final.log`、`ctest-full.log`、`ctest-ui-scale-after.log`、`ctest-hub-completion.log`、`build-asan.log`、`ctest-asan-shell.log`、`scenes-final.log`。图像比对在首次提交前完成，两边的嵌入版本号均取指定基线；复现时同样在基线检出中应用未提交的任务补丁，以固定版本元数据。基线图片在 `before/ui-scene-*/`；比对输出为 `pixels-after.log`、`pixels-asan.log`、`pixels-final.log`。

`verify.py --baseline <基线>` 为手动检查，依赖本地基线对象；不加入 ctest。它同时检查函数体、定义、共享布局及胶水行数；反向对照能拒绝函数体变化、缺少定义和图片摘要变化。`source-check.log`、`capabilities-before.log`、`capabilities-after.log`、`capabilities-runtime.log` 保留对应输出。仅对忽略目录里的生成预览执行 `--check --output ... --runtime ...` 通过，不把它冒充标准文档检查通过。

```bash
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" \
  "$INK_DEPS/../ink-codex/tools/gcccheck.py" build \
  system/ui/shell/{navigation,status_bar,shell_home,control_center,settings_body,today_edit}.c
INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh
```

S31 基线与候选日志分别为 `s31-before.log`、`s31-after.log`；基线大小报告在 `before/size.txt`，候选报告在 `build-s31/size.txt`。以上产物与图片均不入库。

```bash
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-host --output-on-failure -j2
PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility cmake -S . -B build \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
taskpolicy -c utility cmake --build build -j2
PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" \
  --test-dir build --output-on-failure -j2 -R '^(hub-contract|hub-json-crosscheck|hub-simulator-e2e|ui-scale)$'
```

宿主机输出为 `configure-s31-host.log`、`build-s31-host.log`、`ctest-s31-host.log`。最终范围、空白和隐私检查在提交前后核对，不提交任何构建产物或设计稿。
