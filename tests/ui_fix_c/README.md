# ui-fix-c 专用验证

使用主工程已编译的 `sim-ai-pages` 链接选项，只替换测试驱动及本项页面；不改 CMake、共享测试或产品工作树。中文、100% / 120% 两档，使用合成图片、家人与服务器回执。

```sh
PY="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility "$PY" -B tests/ui_fix_c/run.py --build build --screens
taskpolicy -c utility "$PY" -B tests/ui_fix_c/run.py --build build --before 69cc9868a5e6b95c8376293b385308431fef92a0 --screens
taskpolicy -c utility "$PY" -B tests/ui_fix_c/run.py --build build --reverse 69cc9868a5e6b95c8376293b385308431fef92a0
taskpolicy -c utility "$PY" -B tests/ui_fix_c/existing.py --build build
```

正向覆盖 8 组：AI 面板、结果记录、相册、空相册各两档字号。`--case panel|records|gallery` 可独立运行。前图模式只跳过新增行为的断言，原有路径和截图流程不变；反向模式把同一组新断言应用于基线，必须收到断言失败且驱动完整结束。

`--before` 和 `--reverse` 是依赖历史的手动模式，不注册进 CTest。日常正向模式不读取 Git 历史。详细日志、截图、语义快照、摘要分别在 `build/ui-fix-c/{before,after,reverse}/`；提交的 PNG 是同一帧的 `clean-*` 灰阶图，用于布局比较，墨水屏残影模型的 `snap-*` 原图仍保留在构建证据目录。没有启动窗口模拟器，也不代表真机触摸或刷新验证。

内存检查换用 `--build build-asan`，编译和链接沿用该构建的 ASan/UBSan 选项。专用驱动不放宽 `tests/ui_scene.budget.json` 的现有断言预算。

`existing.py` 只把当前 AI 面板按真实选项重编译、链接进五项未改的现有回归路径（触摸停止及其快照、两个设备档案的布局、离线重播说明）；既不改原测试，也不替换主构建的执行文件。它用于检查全量排查中发现的问题，不能代替普通全量构建或验收。
