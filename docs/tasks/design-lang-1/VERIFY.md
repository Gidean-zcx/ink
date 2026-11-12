# 验证与复现

命令中的 `INK_DEPS`、`INK_DESIGN` 使用任务提供的只读目录。所有构建、截图原图、控件树、日志与反向变体留在忽略的构建目录。以下均不启动窗口、不连接设备、不调用真实远端服务。

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT=$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT=$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf \
  -DINK_CJK_BOLD_FONT=$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf \
  -DPython3_EXECUTABLE=$INK_DEPS/build-deps/pyenv/bin/python \
  -DFETCHCONTENT_SOURCE_DIR_LVGL=$INK_DEPS/build/_deps/lvgl-src \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2
```

内存检查：以上命令的构建目录全部换成 `build-asan`，配置时另加 `-DINK_SANITIZE=ON`。不调整资源预算或其他功能开关。普通全量的原始输出在报告中按原话列出；内存构建出现的布局失败与 sanitizer 报错分开统计。

最后两轮定向复核分别为：

```sh
INK_KEEP_EVIDENCE=1 taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2 -R '^ui-|^settings-a$'
INK_KEEP_EVIDENCE=1 taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2 -R '^ui-(theme|slider|context)-regressions-|^ui-input-host$|^ui-scale$|^ui-list-'
```

内存版换为 `--test-dir build-asan`，其余不变。前一轮 118 项是在负坐标与热区修正后；后一轮 10 项是在最终排版和字重修正后，不能合并为最终全量通过。

```sh
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host -DPython3_EXECUTABLE=$INK_DEPS/build-deps/pyenv/bin/python
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-s31-host --output-on-failure -j2
taskpolicy -c utility env INK_DEPS=$INK_DEPS INK_UPSTREAM=$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus bash ports/esp32s31/build.sh
```

仓库中该脚本的文件模式是 100644，直接执行会在启动前退出 126；本次使用 `bash` 运行同一脚本，未改变文件模式。

截图采集的先后顺序不可颠倒：本次 `before` 是在产品四文件仍为基线、基线构建完成时采集，之后才改产品并构建 `after`。不要在修改后代码上重跑 `--label before` 冒充基线。驱动拒绝覆盖已有证据目录；重新采集时应换一个构建目录。

从干净检出复现基线时，手动建立临时 worktree，再把两个采集文件复制进去。基线已有场景入口只加测试复用的条件编译保护，产品源码保持基线。此步骤需要本地具有基线提交，不属于 ctest：

```sh
git worktree add --detach work/design-lang-before bf95f981b9594b563d23d43fe7667513d8cca49d
cp tests/design_lang_1.c tests/design_lang_1.py work/design-lang-before/tests/
$INK_DEPS/build-deps/pyenv/bin/python - <<'PY'
from pathlib import Path
p = Path('work/design-lang-before/tests/ui_shell_scenes.c')
s = p.read_text()
assert s.count('void ink_main(void) {') == 1
p.write_text(s.replace('void ink_main(void) {', '#ifndef INK_DESIGN_LANG\nvoid ink_main(void) {') + '\n#endif\n')
PY
```

随后复用上方全部配置参数，把 `-S . -B build` 换成 `-S work/design-lang-before -B build-before`，构建 `build-before`，运行 `work/design-lang-before/tests/design_lang_1.py --build build-before --label before --capture`。修改后版本在另一新构建目录采集 `after`，再把两个采集目录放在同一个 `design-lang-1/` 下调用 `--compare`。这样不需要回退当前分支，也不会更改基线的产品文件。

```sh
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/design_lang_1.py --build build --label before --capture
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/design_lang_1.py --build build --label after --capture
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/design_lang_1.py --build build --label after --check-targets
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/design_lang_1.py --build build --compare docs/tasks/design-lang-1
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/design_lang_1_reverse.py --build build
```

控件树检查包括有事件或能力绑定的可点对象，以及原生矩阵中的独立按键。按画布及祖先容器裁切后的实际区域检查宽、高；不以整个键盘的大小代替每个键。小于 108 的项会使检查命令返回 1，完整列表在 `build/design-lang-1/after/targets.json`。它是几何检查，不代表已经验证所有控件重叠时的实际命中顺序。

截图取 `clean-scene.png`（无模拟残影），不画调试框、不裁掉页面。每个对比图都有完整左右画布；SHA-256 在 `screenshots.json`。模拟显示模型的 `snap-scene.png` 也保留在构建目录，可另行检查残影。

反向对照临时编译撤去数字字重、两行上下排、黑色占位文字、字体合并、自动分组去框、清除按钮透明底、输入焦点边框、负坐标舍入、图标热区的版本，运行同一组共用控件断言，要求明确失败且测试执行到结束。脚本不修改产品文件，不依赖 Git 历史，未注册进 ctest。

Linux 警告核对使用任务指定的 `ink-codex/tools/gcccheck.py`，读取 `build/compile_commands.json`：

```sh
taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python $INK_DEPS/../ink-codex/tools/gcccheck.py build \
  system/ui/theme.c system/ui/textfield.c system/ui/ui_list.c \
  tests/settings_a.c tests/ui_input_host.c tests/ui_list_paging.c \
  tests/ui_shell_regressions.c tests/ui_shell_scenes.c tests/design_lang_1.c
```

结果及跳过项在报告中单列。源码范围、链接、图片大小及隐私检查同样在报告中列出。
