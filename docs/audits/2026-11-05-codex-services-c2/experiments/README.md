# C2 路的既有测试与核查证据

本目录没有新增测试/实验程序。只保存既有构建、既有测试的日志，以及从源码/既有指标文件提取的摘要。源码未修改，未做反向变异。

## 文件

| 文件 | 内容 |
| --- | --- |
| [configure.log](configure.log)、[build.log](build.log)、[ctest.log](ctest.log) | 普通 Werror 构建与 85 项既有测试，三个命令退出码均 0 |
| [configure-asan.log](configure-asan.log)、[build-asan.log](build-asan.log)、[ctest-asan.log](ctest-asan.log) | ASan + UBSan 构建与同样 85 项测试，三个命令退出码均 0 |
| [validation-summary.json](validation-summary.json) | 提交、命令状态、85 个测试的结果/时间和选取的既有 metrics 摘要 |
| [surface-comparison.json](surface-comparison.json) | 从原 ai_surface 源码表提取的计数、名称差异和 JSON 字节数；不是新抓包或运行时全量目录转储 |

## 执行命令

工作目录为本次审计检出根；`D` 是用户指定的已有依赖目录。未下载依赖。

```sh
D=<主检出目录>
cmake -S . -B build-audit -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT="$D/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$D/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$D/build/_deps/lvgl-src"
cmake --build build-audit -j 4
"$D/build-deps/pyenv/bin/ctest" --test-dir build-audit -j 4 --timeout 400 --output-on-failure

cmake -S . -B build-audit-asan -DINK_WERROR=ON -DINK_GUI=OFF -DINK_SANITIZE=ON \
  -DINK_CJK_FONT="$D/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$D/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$D/build/_deps/lvgl-src"
cmake --build build-audit-asan -j 4
"$D/build-deps/pyenv/bin/ctest" --test-dir build-audit-asan -j 4 --timeout 400 --output-on-failure
```

标准输出/错误输出保留在对应日志。普通 CTest：85/85，30.10 s；内存检查 CTest：85/85，81.36 s。`INK_SANITIZE` 在 `CMakeLists.txt:28–30` 同时打开 AddressSanitizer 和 UndefinedBehaviorSanitizer。

`integration-ai-surface` 原测试从模拟中枢分页拉清单，检查全局调用的名称、标题和必需参数；4 个页面专属调用仅检查本地定义。本次没有改它来捕获目录，也没有新增运行时探针。`surface-comparison.json` 只是从 `tests/integration/ai_surface.c` 的期望表剔除 4 个 `book` 专属调用后计算紧凑 JSON 长度；其中 actions 容量不足是已知 HUB2-006，不是本轮新问题。与 §1.3 的名称差异包含旧阅读/通信等域，报告仅把应属于系统服务表的 8 个 call 及任务状态/事件列为 AUD18-404。

生成的模拟器文件系统、指标、音频和截图仍在 `build-audit/test-out/` 与 `build-audit-asan/test-out/`，没有复制为大附件。内存数是模拟器记账，不能作为硬件资源实测。没有运行历史审计实验、没有重做历史反向对照。
