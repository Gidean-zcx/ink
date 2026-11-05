# 界面壳第二轮集成修复报告

2026-11-06。此次起点 `a380121a`，先合入 `FETCH_HEAD`（`103382b1`），合并提交 `9ef1958b`；没有文本冲突。上轮报告保留在 `a380121a:docs/tasks/ui-shell/REPORT.md`，原任务复查见 [REVIEW.md](REVIEW.md)。本报告只验收 gate-2 的两项测试阻塞及本轮必要改动。

**【实测】普通和 ASan/UBSan 全量各 151/151，无跳过；两项修复的反向对照在两套构建中均检出原故障。未放宽验收断言、超时或资源预算。**

## 验收表

| 编号 | 结论 | 证据 | 命令或位置 |
| --- | --- | --- | --- |
| G2-1 合并 | 已完成 | 【实测】先合入最新集成提交；未产生文本冲突 | `git merge --no-edit FETCH_HEAD`，退出 0 |
| G2-2 能力文档 | 已修，两套通过 | 【实测】补齐 4 个 AI 面板能力及受限的 `confirm.page`；99 个 AI 调用、7 个限制调用、37 个状态、10 个设置、20 个事件均无漏项，未改检查器和能力权限 | `services-mid-capability-doc` |
| G2-3 键盘快照 | 已修，两套通过 | 【实测】原 9 项失败已消失；增强的键盘专项两套各 55 项检查通过；真实 AI 请求入口保留目标与版本，凭据仍掩码 | `services-mid-regressions`；两套 `Testing/Temporary/LastTest.log` |
| G2-4 普通全量 | 通过 | 【实测】严格构建退出 0；151/151，52.29 秒，无跳过 | `build-final.log`、`ctest.log` |
| G2-5 内存检查 | 通过 | 【实测】相同依赖、WERROR，加 ASan/UBSan；构建及测试退出 0；151/151，107.29 秒，无跳过；核对编译与链接标志，完整测试日志无 sanitizer 错误诊断 | `build-asan.log`、`ctest-asan.log` |
| G2-6 反向对照 | 两套通过 | 【实测】旧文档检出原 5 个漏项；旧 UI 源码正常完成专项，29 项功能断言失败（含原 9 项），没有把崩溃或编译失败当反例；两套脚本均退出 0 | `tests/ui_shell_gate2_reverse.py`；`reverse.log`、`reverse-asan.log` |
| G2-7 结束检查 | 通过 | 【实测】本轮仅 6 个相关文件；差异空白、隐私、文件大小、报告链接及 I/O 核查通过，只本地提交 | 下方结束检查 |

构建最多 `-j4`、ctest 最多 `-j3`，构建与全量测试串行。临时日志保存在忽略目录 `work/ui-shell-gate-2/`。

复跑命令（沿用用户提供的环境变量和上一轮已准备的 `hub/.venv`，两套串行）：

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
  UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/python" -B \
    tests/ui_shell_gate2_reverse.py --build "$task_build"
done
```

## 没做到的

- 本轮仅 macOS/Apple clang 实测，未运行 Linux gcc 或远端 CI；未接设备或启动带窗口的模拟器。GUI smoke 使用 SDL dummy。
- 本轮没有重做原 U1～U13 的完整视觉和交互验收；上轮报告及原复查中的欠项仍保留，不能把全量测试通过等同于原界面任务全部验收完成。

## 和设计不一致的

本轮恢复系统服务设计中“页面与输入面板组合快照”的既有合同；能力总表只补登记，不修改交互或权限约定。没有新增设计偏差；原欠项见上轮报告和复查。

## 改过的已有测试

- `keyboard_snapshot.c`：保留全部原断言，追加 AI 请求入口、AI 面板下的上下文、阻断浮层、敏感输入、缓冲区不足及结束输入的检查；新增上下文缓冲按真实 AI 请求的 3072 字节检查，原快照缓冲仍为 4096 字节。
- `whole_product.h`：场景记录缓冲从 256 改为真实 AI 请求使用的 3072 字节；原来的短键盘文本换成完整组合上下文后，首轮全量有五个 Ime 场景因记录缓冲不足失败（同场景的 4096 字节快照检查通过）。仅对齐记录器容量，保留所有原断言、预算和超时；新专项仍要求 8 字节缓冲不足时返回失败并清空输出。
- 文档覆盖检查器未改；新增独立的反向对照脚本。没有删除或跳过测试。

## 要用户定的

无。没有 push、PR、发布、硬件或付费操作。

## 顺手发现

- 【实现】两条快照入口共用同一组装逻辑，保留页面、受掩码保护的输入会话，以及可见候选和动作；阻断浮层仍独占快照，AI 面板只在构造请求上下文时略过。沿用已有两块临时 PSRAM 缓冲，没有新增常驻缓冲、设备任务或服务接口。按钮和 AI 的原子能力路径未变（R-061）。
- 【实测】两套 AI Plus 内部最低余量均为 35,544 字节，PQBN 均为 32,880 字节，与上轮结果一致；原门槛仍为 32,768 字节。这里只是模拟器结果。
- 【实测】本轮相对 `9ef1958b` 的差异空白检查退出 0；相对集成分支的完整差异仍含原有 Outfit/NanoSVG 许可证与栅格器空白告警，本轮未改这些第三方文件。
- 【实测】相对合并基线和集成分支的隐私扫描均为 0 命中；相对集成分支改动的 50 个 C/H 文件去掉注释和字符串后，`write/read/fgets` 实际调用为 0。这不能代替 Linux CI。报告链接、文件大小和新增 Python 脚本语法检查通过；日志、反向副本和构建产物未提交。
