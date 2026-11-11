# small-gaps-3 执行报告

日期：2026-11-11。基线：`a108101cbb81bd4a0b4785d66f0ab3a4fcb32003`。仅宿主机测试、模拟数据和 S31 离线编译；不碰设备，不对外发布。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| G5 | 正向与反向通过 | 【实测】头像走 `shell.member_select`；AI 拒绝、过期点按拒绝 | `ctest --test-dir build -R '^semantics-shell-member$' --output-on-failure -j2` |
| G9 备份 | 正向与反向通过 | 【实测】每天／每周到期；未到期、未充电、LTE、活跃时均不跑；成功结果显示 | `ctest --test-dir build -R '^small-gaps-3-backup$' --output-on-failure -j2` |
| G9 更新说明 | 正向与反向通过 | 【实测】44 段合成说明分 12 页，拼回后和原文逐字相同；无说明不显示 | `ctest --test-dir build -R '^small-gaps-3-(notes|empty-notes)$' --output-on-failure -j2` |
| G10 | 正向与反向通过 | 【实测】离线点开原创 CC0 示例、跳过、真实模拟 AI 请求带这一页正文 | `ctest --test-dir build -R '^small-gaps-3-sample$' --output-on-failure -j2` |
| V1 普通全量 | 367/368，唯一失败为共享表 | 【实测】`99% tests passed, 1 tests failed out of 368`，652.35 秒，0 跳过；日志 `work/small-gaps-3/ctest-final.log` | 见下方复现命令 |
| V2 内存全量 | 367/368，唯一失败为共享表 | 【实测】`99% tests passed, 1 tests failed out of 368`，1487.04 秒，0 跳过；ASan/UBSan 全量及最终日志无诊断 | 同上，目录换成 `build-asan` |
| V3 能力表 | 副本通过；共享表待合并 | 【实测】534 definitions；运行时导出、策略与源码一致 | `python tools/capabilities.py --output build/small-gaps-3-capabilities.md`，再加 `--check` |
| V4 S31 | 通过 | 【实测】脚本退出 0；固件 3277120 字节；资源包、尺寸及静态栈报告生成 | `INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| V5 gcccheck | 通过 | 【实测】`gcccheck: 13 compiled, 0 with findings`；日志 `work/small-gaps-3/gcccheck-final.log` | 见下方复现命令 |
| V6 ui-scale | 通过 | 【实测】`ui-scale: 51 files, 0 violations` | `python tools/ui_scale_check.py` |
| V7 隐私 | 通过 | 【实测】相对指定基线扫描，0 匹配；构建与设计产物均未提交 | 见下方复现命令 |

## 没做到的

功能验收无未完成项。共享 `docs/system/capabilities.md` 按任务规则未改，因此默认 `capability-table-doc` 未通过；不能称全量全绿。本机没有原生 Linux 运行证据，使用宿主 clang、gcccheck 和 S31 GCC 核查。

## 和设计不一致的

【实现】按任务书在 BootHello 保留引导页，用“看看示例”打开已有分页阅读器；没有直接把引导页替换成正文。未修改设计合同。

## 改过的已有测试

`tests/semantics_shell/member.c` 加头像能力与 AI 权限断言；`tests/integration/ai_surface.c` 为新增能力登记开放／禁止原因；`ports/esp32s31/tests/asset_pack_test.py` 加示例打包断言。没有放宽或删除已有断言。

`tests/i18n_lang.c:185`：英文首启页的按钮期望由 `Bookshelf` 改为 `Read a sample`，因为任务书 G10 明确将该入口改成“看看示例”；仍同时检查英文标题和跳过按钮。

## 要用户定的

暂无。按任务书选择最小、可逆实现。

## 顺手发现

【实测】初版自动调度错误地要求中枢已经在线；现有中枢空闲会断开，导致到点也不启动。已移除此先决条件，由既有备份作业按需连接。新测试的长说明解析器容量不足曾越界，已在宿主调用中用完整 JSON 解析器读取；不是产品崩溃。

## 新增的机制

配置项 `backup.auto`（关／每天／每周，默认关）：任务书 G9 明确要求。复用现有设置持久化、`B.retry` 定时节点、`B.job` 和 `backup.now`；无运行期新任务、线程、状态机、文件格式或框架。既有应用框架增加 `ink_app_asset_read`，供 G10 读取资源包并保持 HAL 边界。

测试驱动任务 `small-gaps-test` 只编进测试程序，用于验收，不进入产品。测试夹具用 `INK_GAPS_CASE` 选验收场景，模拟中枢用 `INK_SIM_UPDATE_NOTES` 注入说明（任务书要求模拟数据验证）；均不进入设备设置。

## 要改的文档

共享能力表由合并会话同步：新增 `shell.member_select`（仅人）、`reader.open_sample`、`shell.update_notes_page` 和 `backup.auto`。本任务在 `build/small-gaps-3-capabilities.md` 生成副本并 `--check`，【实测】534 definitions。共享界面依赖的 G5／G9／G10 状态，以及中枢 `update/check` 的可选 `notes` 字段也需同步；本任务不改共享文档。

## 实现边界与截图

【实现】自动备份按设备设置周期，只处理当前仍可访问的成员空间；关闭或过期的成人空间不绕过验证。有历史备份时沿用上次成功时间；无历史备份时从本次首次检查计时（此起点不另写持久文件，重启会重算）。定时节点空闲时每分钟检查，条件不满足先等待。

【实现】更新说明读取模拟协议的可选 `notes` 字段，全长保留在 PSRAM，页面按实际文字高度和 UTF-8 边界分页；缺字段没有说明区。没有新增跨重启说明存储。原创短文 `assets/sample/welcome.txt` 随既有资源包离线提供，不导入成员文件；复用阅读器的正文快照和阅读位置。

【实测】截图已查看：`build/test-out/small-gaps-3-notes/device/snap-notes-first.png`、`snap-notes-last.png`；`build/test-out/small-gaps-3-sample/device/snap-boot-hello.png`、`snap-sample-first.png`；备份截图 `build/test-out/small-gaps-3-backup/device/snap-automatic-backup.png`；无说明 `build/test-out/small-gaps-3-empty-notes/device/snap-no-notes.png`。

## 里程碑一：功能和反向对照

【实测】`tests/small_gaps_3/reverse.py --build build --baseline a108101cbb81bd4a0b4785d66f0ab3a4fcb32003` 五组旧源码均编译和链接成功，测试均退出 1：成员 2 条、备份 18 条、长更新说明 4 条、无说明 1 条、示例 5 条失败断言。仅替换对应产品编译单元，不改工作区，也不让注册进 CTest 的用例依赖历史。证据 `build/small-gaps-3-reverse/summary.json`、`work/small-gaps-3/reverse-final.log`。无说明用实际可见控件断言，基线旧占位文字使测试失败。

【实测】定向正向用例分别通过；`ui-scale: 51 files, 0 violations`。最终 gcccheck 为 `13 compiled, 0 with findings`。

## 规模与架构审视

【实现】产品源码（`apps/`、`system/`、`simulator/`、`tools/`）新增 211 行、删除 38 行，净增 173 行；另有 CMake 12 行、英文词条 8 行、原创资源 10 行，合计仍低于 400 行预算。三项缺口合计超过 100 行的原因是调度条件、UTF-8 分页和资源加载都要落到真实链路。更小方案已看过：头像继续直接调服务不满足 G5；定时另起线程违反任务书；说明截到小缓冲不满足“完整”；把测试书当产品示例不满足资源许可与打包。最终复用既有机制。

【实现】成员服务仍拥有验证和身份，成员门只通过人用能力发起选择；设置服务拥有周期，中枢和备份服务拥有业务执行；更新服务保留说明，界面负责分页；资源包和阅读器提供示例。按钮、AI 共用设置、说明翻页、读示例能力，唯一新增人用能力不向 AI 暴露；PIN 和本人确认规则不变。页面快照包含当前可见说明或示例正文，未开放私有成员空间。通过现有应用框架增加一个只读资源读取接口，阅读器不直接引用 HAL；没有引入模拟器头文件到产品层。

原创示例为 1080 字节，SHA-256 `c076341606cce24765c7a3fc946776c75147a5207bb1ef2f196c4a6c57f92e90`，正文内明确 CC0 1.0。原资源预算未放宽，打包为 font_data 4718252 字节、embedded 576672 字节，样本在包内。

## 里程碑二：全量发现的接入问题

【实测】首轮普通全量原话 `99% tests passed, 4 tests failed out of 368`，843.41 秒。除禁止改写的共享能力表外，发现英文旧按钮期望、阅读器直连 HAL、备份首页快照超过既有小解析器容量三项。已更新明确变更的英文期望、通过既有应用框架读取资源、去掉备份首页未显示的明细字段（保留成员、状态、日期和周期）；没有删测试或放宽阈值。第二轮三项及其余功能测试均通过，唯一失败为共享能力表。

【实测】S31 脚本退出 0，日志 `work/small-gaps-3/s31.log`，尺寸和栈报告为 `build-s31/{size,size-components,stack}.txt`。实际 `ink-embedded.bin` 里的示例为 1080 字节，逐字节与源文件一致。静态栈报告不含间接回调等，不能当成完整运行期栈上界；没有设备运行证据。本轮没有硬件参数或驱动改动。

## 验证命令

在仓库根目录使用用户给定的 `INK_DEPS`。两套目录均沿用任务规则的字体、Python 和 LVGL 参数，另用 `INK_HUB_PYTHON` 指向依赖目录已有的 `hub/.venv/bin/python`，SDL2／ImGui 源码也只读复用依赖目录。ASan 目录只额外开启 `INK_SANITIZE=ON`；两边保留 Werror。命令均用 utility 优先级，构建与全量测试串行，最多 `-j2`。

```bash
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/small_gaps_3/reverse.py --build build --baseline a108101cbb81bd4a0b4785d66f0ab3a4fcb32003
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" ../../tools/gcccheck.py build \
  apps/reader/reader.c system/ui/app.c system/services/backup/backup_service.c \
  system/services/update/update_service.c system/ui/shell/backup_pages.c \
  system/ui/shell/boot_flow.c system/ui/shell/member_gate.c system/ui/shell/update_pages.c \
  simulator/environment/sim_hub.c tests/small_gaps_3/main.c tests/semantics_shell/member.c \
  tests/integration/ai_surface.c tests/i18n_lang.c
git diff a108101cbb81bd4a0b4785d66f0ab3a4fcb32003..HEAD | rg -n '/(Users/|private/tmp)|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}'
```

无窗口运行；`gui-smoke` 使用已有 dummy 驱动。构建产物、截图和日志均不提交。

【实测】最终内存全量的 AddressSanitizer/UBSan 参数已在编译数据库核对；扫描 `work/small-gaps-3/ctest-asan.log` 和保留的 `work/small-gaps-3/asan-full-LastTest.log`，0 条 sanitizer 错误。日志 `work/small-gaps-3/sanitizer-final.log` 为空。最后一行将自动设置事件的重画范围限制在备份首页；两版定向复验 `backup-pages`、`small-gaps-3-backup` 均为 `100% tests passed out of 2`，普通版 25.53 秒、内存版 41.71 秒。日志 `work/small-gaps-3/{last-normal,last-asan}.log`，定向内存日志也无诊断。
