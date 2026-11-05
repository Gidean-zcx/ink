# 界面壳第三轮集成修复报告

2026-11-06。此次起点 `c536f0dc`；先执行 `git merge --no-edit FETCH_HEAD`，目标 `103382b1` 已在当前分支历史中，退出 0，没有冲突。第二轮报告保留在 `c536f0dc:docs/tasks/ui-shell/REPORT.md`，原任务复查见 [REVIEW.md](REVIEW.md)。本轮只处理 gate-3 的 CI 阻塞，不重新验收原任务的视觉欠项。

**【实测】普通与 ASan/UBSan 全量各 151/151，零跳过；GCC 对两处修复的反向对照通过。修复提交 `93b61974`，只在本地提交。没有放宽断言、超时、预算或编译警告。**

## 验收表

| 编号 | 结论 | 证据 | 命令或位置 |
| --- | --- | --- | --- |
| G3-1 合并 | 已完成 | 【实测】FETCH_HEAD 已合入，无冲突 | `git merge --no-edit FETCH_HEAD`，退出 0 |
| G3-2 CI 编译 | 已修，本地严格检查通过 | 【实测】两处相邻 `if` 分行；去掉空白后源码与基线逐字一致。普通 WERROR 构建退出 0，GCC 14.3.1 对两个真实编译单元的严格语法检查均退出 0 | `build.log`；`reverse.log` |
| G3-3 反向对照 | 通过 | 【实测】独立恢复两个基线源码；各自退出 1，且仅有原 `-Werror=misleading-indentation` 错误；修复源码均退出 0 | `tests/ui_shell_gate3_reverse.py`；`reverse.log` |
| G3-4 普通全量 | 通过 | 【实测】严格构建退出 0；151/151，64.16 秒；JUnit 确认零失败、零禁用、零跳过 | `build.log`、`ctest.log`；`build/work/ui-shell-gate-3/ctest.xml` |
| G3-5 内存检查 | 通过 | 【实测】严格构建退出 0；151/151，101.93 秒；JUnit 确认零失败、零禁用、零跳过。编译和链接均带 ASan/UBSan，两套完整测试日志无 sanitizer 错误诊断；两套字体、LVGL、Python、中枢依赖一致 | `build-asan.log`、`ctest-asan.log`、`ctest-asan.xml`；两套 `Testing/Temporary/LastTest.log` |
| G3-6 结束检查 | 通过 | 【实测】仅 4 个相关文件；差异空白、隐私、文件大小、报告链接和 Python 语法检查通过；没有提交日志、反向副本、编译器或构建产物 | 下方结束检查；`acceptance.json` |

资源限制：构建最多 `-j4`，ctest 最多 `-j3`，构建与全量测试串行；不用窗口模拟器。临时日志放在忽略目录 `work/ui-shell-gate-3/`，反向源码副本和完整编译命令放在脚本输出的 `build/gate3-reverse-*` 目录。

复跑命令（沿用用户给定环境变量与现有 `hub/.venv`；`TASK_GCC` 指向 GNU C 编译器，两套串行）：

```sh
set -e
for task_build in build build-asan; do
  task_sanitize=OFF
  if [ "$task_build" = build-asan ]; then task_sanitize=ON; fi
  cmake -S . -B "$task_build" -DINK_WERROR=ON -DINK_SANITIZE="$task_sanitize" \
    -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
    -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
    -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
    -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
    -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
    -DINK_HUB_PYTHON="$PWD/hub/.venv/bin/python"
  cmake --build "$task_build" -j4
  SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 \
    "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir "$task_build" --output-on-failure -j3
done
"$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ui_shell_gate3_reverse.py --cc "$TASK_GCC" --build build
```

## 没做到的

- GCC 检查使用 macOS 上的 Arm 交叉编译器，仅检查真实编译单元；本轮没有运行 Linux 链接、测试或远端 CI。全量测试与 ASan/UBSan 均在 macOS 上完成。
- 原任务 U1～U13 的视觉和交互欠项见第二轮报告与复查，本轮不扩大范围。

## 和设计不一致的

本轮仅修编译诊断，不改设计、设置范围或能力路径。

## 改过的已有测试

- `tests/ui_shell_regressions.c` 只把同一行两个 `if` 分开；断言、档位、输入序列、超时和预算均未变。
- 新增 GCC 反向脚本，读取实际 CMake 定义、头路径和严格警告参数；交叉检查只去掉 Darwin SDK 参数，不削弱警告。源码副本及完整命令写入忽略的构建目录，不改工作树源码。

## 要用户定的

无。只在本地工作，不 push、PR、发布、接设备或调用付费服务。

## 顺手发现

- 【实测】GCC 还检出了滑杆测试中相同的潜在 CI 编译阻塞，已随同修复。
- 【实测】临时 GCC 来自 [Arm 官方归档](https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.3.rel1/binrel/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi.tar.xz)，归档 SHA-256 与官方校验文件一致：`30f4d08b219190a37cded6aa796f4549504902c53cfc3c7e044a8490b6eba1f7`。仅作为本轮诊断工具，不提交编译器或改项目依赖。
- 【实测】相对集成分支改动的 50 个 C/H 文件去掉注释和字符串后，`write/read/fgets` 实际调用为 0。
- 【实测】两套 AI Plus 内部最低余量均为 35,544 字节，PQBN 均为 32,880 字节，与上一轮相同，原门槛 32,768 字节未变。两套滑杆专项均通过，保留亮度、暖光、音量的 11 档点按与拖动检查；这里只是模拟器结果。
- 【实测】相对本轮基线 `c536f0dc` 和集成分支 `103382b1` 的隐私扫描均为 0 命中；`git diff --check` 退出 0，4 个改动文件均小于 5 MB，报告链接及新增 Python 语法检查通过。验收摘要在 `work/ui-shell-gate-3/acceptance.json`。
- 【实现】按钮经 `settings.set` 原子能力修改设置；本轮不改变模块边界、AI 接口或软总线路径（R-061）。
