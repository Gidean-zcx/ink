# 复核命令

在仓库根目录运行，`INK_DEPS` 采用任务提供的只读依赖目录。以下是本机 macOS 命令；不启动窗口，不连接设备。构建与全量测试串行，均最多两路。

```bash
PY="$INK_DEPS/build-deps/pyenv/bin/python"
CTEST="$INK_DEPS/build-deps/pyenv/bin/ctest"
TASK_BASE=cfd1a3dd5550fda64d40c23e80a7d60a0a8ee7c9

taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" \
  -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$PY" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$CTEST" --test-dir build --output-on-failure -j2

taskpolicy -c utility "$PY" tests/ui_fix_d/run.py --build build
taskpolicy -c utility "$PY" tests/ui_fix_d/run.py --build build --phase before --baseline "$TASK_BASE"
taskpolicy -c utility "$PY" tests/ui_fix_d/run.py --build build --phase reverse --baseline "$TASK_BASE"
# Restore current-source standalone compile commands before GCC checking.
taskpolicy -c utility "$PY" tests/ui_fix_d/run.py --build build
taskpolicy -c utility "$PY" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build \
  system/ui/shell/navigation.c system/ui/shell/boot_flow.c system/apps/memo/memo.c \
  tests/memo/memo.c tests/ui_shell_regressions.c tests/settings_small.c \
  tests/ui_fix_d/network.c tests/ui_fix_d/memo.c tests/ui_fix_d/boot.c

taskpolicy -c utility cmake -S . -B build-asan -DINK_WERROR=ON -DINK_SANITIZE=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" \
  -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$PY" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
taskpolicy -c utility cmake --build build-asan -j2
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 INK_KEEP_EVIDENCE=1 \
  taskpolicy -c utility "$CTEST" --test-dir build-asan --output-on-failure -j2 \
  -R '^(boot-flow|boot-flow-reboot|memo|i18n-memo|i18n-lang|settings-small|ui-scene-(big-)?SetNetwork|ui-context-regressions-metalio-plus|ui-privacy-regressions-metalio-plus)$'
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  taskpolicy -c utility "$PY" tests/ui_fix_d/run.py --build build-asan --no-copy
```

`before`／`reverse` 是人工模式，明确要求基线历史；没有注册进 ctest，不影响浅克隆 CI。`after` 不读 Git 历史。三个模式都保留同样的断言；`before` 只要求场景完成以采集旧页面，失败断言仍如实记入结果，不称基线功能通过。反向模式必须完成场景并出现失败断言才成功，不能把崩溃或未完成当作有效对照。每次结果在对应构建目录的 `ui-fix-d/<模式>/results.json`；截图按模式复制到本任务目录。

ASan／UBSan 覆盖内存访问和未定义行为；本机关闭不支持的 LeakSanitizer，没有把它说成泄漏检查通过。S31 是否实际编译、全量结果及跳过项以报告为准。
