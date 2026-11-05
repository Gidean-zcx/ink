# 应用框架实测报告

基线 `39a349d`，分支 `claude/codex-appfw-spike`。开工约 14:50 UTC；2026-11-05 15:51 UTC 整理终检，【估算】约一小时。只写独立原型、任务证据、新实测记录和设计第 3／6 节；没有产品源码、既有测试或默认构建改动。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B1 | 通过 | 【实测H】C／受限 JSON／Lua 均跑出同页；[C](screenshots/c.png)／[JSON](screenshots/json.png)／[Lua](screenshots/lua.png) 截图逐像素相同；真实触摸和 AI JSON 进入能力表，文本框与按钮可用，快照含成员／版本 | `bash tools/appfw-spike/run.sh all` |
| B2 | 通过，目标缺口明列 | 【实测H】[对比表](../../research/app-framework-spike.md)、[普通样本](measurements.json)、[ASan 样本](measurements-asan.json)；每项有口径和环境。S31 Flash／静态 RAM 没测，不以宿主值替代 | 同上；IDF 检查见“没做到的” |
| B3 | 通过 | 【实测】默认普通 91/91（55.60 秒）、ASan／UBSan 91/91（100.73 秒），没有失败或跳过；独立原型两模式各验证三个候选，每候选两轮确定性＋五轮计时，每轮 20 条主检查 | 下列命令；产品差异检查与文件范围检查 |
| B4 | 通过 | 【估算】约一小时；只编译／运行电脑程序，无设备操作、付费调用、推送或 PR；只有真机能回答的问题单列 | 本报告时间、命令和差异记录 |

H：macOS 15.7.9 / arm64、clang 17、`-O2`、`INK_WERROR=ON`，同一 Plus 档案和字库；CPU 型号读取受沙箱限制。池内对象仍为宿主 64 位。时间是模拟器宿主线程 CPU ×1 加调度／固定时间成本，未经 S31 标定；不包含光学时延。字符数是本轮页面载荷，没有调用真实 AI。[工具说明](../../../tools/appfw-spike/README.md)列明全部口径。

【实测】执行和复现：

```bash
bash tools/appfw-spike/run.sh all
INK_SPIKE_BUILD=build-spike-asan INK_SPIKE_SANITIZE=ON \
  INK_SPIKE_OUT=work/appfw-spike/evidence-asan bash tools/appfw-spike/run.sh all
"$INK_DEPS/build-deps/pyenv/bin/python" tools/appfw-spike/negative.py
cmake --build build -j4
PYTHONPATH="$PWD/hub" "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
PYTHONPATH="$PWD/hub" "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
```

默认两目录按[总规则 §5](../codex-rules.md#5-构建和测试)配置，内存版只加 `-DINK_SANITIZE=ON`。两边另相同设置 `INK_HUB_PYTHON` 为已有的 Python 3.11.15 锁定依赖环境（依赖见 `hub/uv.lock`，可按 `hub/README.md` 重建），并复用 `$INK_DEPS/build/_deps/{lvgl,imgui,sdl2}-src`。这些路径只通过 CMake 参数传入，没有改锁文件或环境。全量测试的 `gui-smoke` 两轮均首次通过，用既有 `SDL_VIDEODRIVER=dummy`，没有窗口；构建和全量测试依次执行。

【实测】Lua 另有模块隐藏、循环中断、配额、非法行四项检查，JSON 有非法描述检查。配额与计时的[反向对照](negative-control.json)分别移除一行检查／CPU 结算，原验证器均失败；逐字恢复源文件、重建后通过。移除计时结算时初始化读成 0 微秒，恢复后该对照轮为 120 微秒；这不是性能预算或正式五次样本。

原始运行和构建日志在忽略的 `work/appfw-spike/`；全量日志在两构建目录的 `Testing/Temporary/LastTest.log`。没有提交 ELF、库、字体、运行时源码、设计稿或其他构建产物。初始任务提交 `fdc9892`，原型里程碑 `db70007`；本次收尾再提交。

## 没做到的

- S31 Flash／静态 RAM 没测。【实测环境检查】ESP-IDF 为 v6.1，`export.sh` 提示缺少其 Python 虚拟环境，随后找不到 `idf.py`；没有安装全局工具，也没有生成目标 ELF。复现：`bash -c 'source "$HOME/esp/esp-idf/export.sh" && command -v idf.py && idf.py --version'`。
- Linux／gcc 没在本机运行。新增自写 C 的文件读取使用 `fread` 并检查返回长度、`ferror` 和关闭结果；没有新增 `write/read/fgets` 调用。
- 只有真机能回答：系统与驱动并发时真实内部 RAM 余量、缓存关闭／PSRAM 停顿、GC 对音频和显示的干扰，以及触控、残影、稳定可读时间与耗电。静态大小可在补齐编译环境后测，不需要设备。

## 和设计不一致的

- 按任务书的三候选上限选 C 基线＋受限 JSON＋Lua，没有量设计 §8 的 MicroQuickJS；没有取消后续候选或选择产品运行时。
- 这是固定单页原型。JSON 没有实现 A2UI 兼容，Lua 没有达到完整沙箱／安装包准入；长文、长列表、最坏 GC 停顿和系统输入避让未实现。不能据此验收 SP-08。
- 示例用基线 32px 字库和 64px 高开关，未实现规范的最终标题字号／84px 控件等外观。没有修改界面规范；设计只回填本轮证据。

## 改过的已有测试

无。没有删除／放宽断言、提高预算、加等待掩盖问题或新增跳过。

## 要用户定的

无。本轮不作产品语言／运行时选择，不改变需求、决定或发布范围。

## 顺手发现

- 首轮原型没配编辑完成回调，3 条主检查失败；补齐回调后全部通过。初次开关边框有缺口，截图核查后修正；两次旧结果均未当作通过证据。
- 直接读虚拟时钟会漏掉未结算的宿主 CPU，已修正并加入可检出的反向对照；旧计时弃用。CPU 型号读取受沙箱限制，工具已记录缺口并保留架构信息。
- 默认 CMake 没使用 medium／bold 字体参数，沿基线未修；根构建要求和预算没有变化。
- 【实测】终检核对任务书逐字一致、文件／链接／样本／截图、允许范围和文件大小；`git diff --check` 通过。`git diff --stat` 仅本任务文件；对 `git diff 39a349d..HEAD` 按总规则隐私表达式扫描零命中（未匹配退出 1），新增待提交文件另查同样零命中。提交后复核工作树与同一隐私扫描；完整记录保留在本地忽略目录。
