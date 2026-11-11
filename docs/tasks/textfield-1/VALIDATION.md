# 复核命令

所有命令从仓库根目录执行；先设置任务提供的 `INK_DEPS` 和 `INK_DESIGN`。后者只读，本任务没有复制设计稿。构建和测试串行进行，每次最多两个并发。

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

内存检查：配置命令把 `build` 换成 `build-asan`，追加 `-DINK_SANITIZE=ON`，再运行 `taskpolicy -c utility cmake --build build-asan -j2`。本次全量命令为：

```sh
taskpolicy -c utility env UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/textfield_forms.py --build build --label before --baseline da3296b75fda740f680dc5639d835d4e06d9a33b
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/textfield_forms.py --build build --label after
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/textfield_reverse.py --build build
```

前后图驱动分别编译基线／当前真实 presenter 与组件，使用模拟 Wi-Fi、假网址、假文件和示例应用。同一场景运行 100% 和 120% 两档；`clean-*.png` 是模拟器导出的理想帧，供排版对比，`snap-*.png` 保留屏幕仿真效果。截图索引采用理想帧；不代表真机显示效果或触控测试。

`--baseline` 只用于手动复核；注册到 ctest 的检查不读 Git 历史。反向对照只编译临时替换对象，不撤销工作区源码：分别移除旧尺寸覆盖拦截、移除随字号增高，要求真实 `ui-input-host` 断言失败。

GCC 检查使用任务提供的 `ink-codex/tools/gcccheck.py`，对本任务修改的全部 C 文件执行；新增截图驱动也单独提供同一构建的编译参数。工具用 GCC 编译到汇编，不连接、不接设备，不能代替 S31 固件编译。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
import json, os, subprocess, sys
from pathlib import Path
root = Path.cwd()
area = root / 'work/gcccheck'
area.mkdir(parents=True, exist_ok=True)
db = json.loads((root / 'build/compile_commands.json').read_text())
for target, template in [('tests/textfield_forms.c', '/tests/ui_input_host.c'),
                         ('tests/services_finish/text_window.c', '/tests/integration/media.c')]:
    entry = next(e for e in db if e['file'].endswith(template)).copy()
    old = entry['file']
    entry['file'] = str(root / target)
    entry['command'] = entry['command'].replace(old, entry['file'])
    db.append(entry)
(area / 'compile_commands.json').write_text(json.dumps(db))
files = subprocess.check_output(['git', 'diff', '--name-only',
    'da3296b75fda740f680dc5639d835d4e06d9a33b..HEAD', '--', '*.c'], text=True).splitlines()
checker = Path(os.environ['INK_DEPS']).parent / 'ink-codex/tools/gcccheck.py'
sys.exit(subprocess.run([sys.executable, str(checker), str(area), *files]).returncode)
PY
```
