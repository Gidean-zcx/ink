# ui-file-picker 执行报告

日期：2026-11-09。基线：`c6dede1945807b2cd688e6967c7299154813827e`。状态：第 1 轮复查修复进行中；以下原验收结论保留为历史记录，以文末本轮证据为准。仅本地提交，未推送。

代码：[接口](../../../system/include/ink/sys/file_picker.h)、[选择器](../../../system/ui/file_picker.c)、[测试调用方](../../../tests/ui_file_picker.c)、[取消后画面检查](../../../tests/ui_file_picker_frames.py)。实现提交：`02326868`；夹具修订：`7c6f4691`；换人收尾：`3ab4830d`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 7 张图片按 4/3 分页；混入音频和文字不出现；触摸、AI 均返回同一稳定编号，调用方 lookup 成功 | T1 |
| 2 | 【实测】通过 | 原活动不重建，第十页第二项的位置与快照不变；取消前后每个像素相同 | T1 |
| 3 | 【实测】通过 | 孩子仅见家庭及自己的图片；私有编号伪造失败；换人关闭旧浮层；撤掉文件服务的一处权限检查，2 条断言失败，恢复后通过 | T1、N1、N2 |
| 4 | 【实测】通过 | 普通、ASan/UBSan 全量各 219/219，零跳过；产品代码新增及删除合计 201 行 | T2、T3 |

## 没做到的

- 没有运行 Linux/gcc 或真机；证据限 macOS/clang 模拟器。没有碰设备、运行可见窗口或对外发布。
- 按任务范围只做系统选择器和测试调用方，未实现文件应用、缩略图、解码或转换流程。图片资格沿用文件服务的扩展名分类及访问权限；不声称文件内容可解码。
- 生成能力表按规则留给合并者统一提交；干净检出需先运行下文的生成命令再跑全量。

## 和设计不一致的

【实测】`FilePicker` 画板对照 **28.414/255**，超过 12/255。画板是四本书及准备状态，本次验收图是实际图片文件和“可以选择”；F4 现有组件是独立描边行，页码条跟在可见项池后面，画板是灰底分组、页码在底部。规范的字号已统一放大 1.2 倍、静态灰已改深，画板尚未同步。没有为分数改规格或伪造书籍准备状态。对照图：`work/ui-file-picker/compare/FilePicker.png`。

【实现】为使 F1/F4 在系统浮层上导出快照，`ui_semantics.c` 仅将页面身份匹配集中改用已有 `ink_ui_action_page()`；浮层与活动沿用同一能力及成员检查。未改服务接口和设计合同。

## 改过的已有测试

`tests/integration/ai_surface.c` 仅追加四项新能力及 `select` 的必填 `id`；`tests/sdk_boundary.cmake` 仅追加公开选择器头文件。已有断言与预算未放宽。

## 要用户定的

无；没有新增产品选择。

## 顺手发现

【实测】首轮普通全量 218/219，唯一失败是新测试重复运行时上次留下的孩子图片进入大人总数。已在新测试开头定点清理该合成夹具，未改产品代码或放宽断言。最终复核时又用新增断言复现换人后旧浮层残留，已在选择器这一处修复，见 N2。

## 新增的机制

F9 要求的临时文件选择模型、结果回调和四项能力；数据放 PSRAM，读取复用现有 BG/UI 执行器，换人取消复用已有成员状态订阅。不新增常驻任务、线程、文件格式或配置项。

## 要改的文档

- 合并者更新 `docs/tasks/backlog.md` 和 `docs/system/ui-dependencies.md`：F9 已有真实索引、按类型分页、编号返回、取消恢复和 AI 接线，视觉差异见本报告。
- 合并者运行 `tools/capabilities.py`，统一提交 `file_picker.prev/next/cancel/select` 的能力总表；本任务未提交共享文档修改。
- 后续 F13 应用开发说明可收录 `file_picker.h`：UI 上打开、用回调取编号、BG 上 lookup；成员或认证变化丢弃旧会话回调。

## 命令与证据

- T1：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ui-file-picker' --output-on-failure -j2`。2/2 通过，91 条断言、零告警；内部 RAM 最低余量 47,472 字节，原阈值 32,768；内部和外部分配失败均为 0。日志：`work/ui-file-picker/restored-test.log`。
- N1：临时仅撤掉 `file_service.c` 的 `resolve_for` 中 `/members/` 分支的 `if (!c->adult) return INK_FS_ERR_DENIED;`，构建 `sim-ui-file-picker -j3`，运行 T1 中的 `^ui-file-picker$`。构建退出 0，测试退出 8，`test.finished=1`、`test.fail=2`；源码逐字节恢复，再构建及 T1 均退出 0。脚本：`work/ui-file-picker/reverse.py`；日志：同目录 `negative-build.log`、`negative-test.log`、`restored-build.log`、`restored-test.log`。首次夹具未清理的反向结果为 4 条失败，已由这次可重复结果替代。
- N2：新增测试先在大人的选择器仍打开时切到孩子。修复前“旧浮层移除”断言确实失败，`test.finished=1`、`test.fail=1`、CTest 退出 8；接入已有 `member.*` UI 状态通知后 T1 通过。换人或认证变化会关闭旧浮层，并丢弃旧调用方回调，避免对已经销毁的调用方回调。日志：`work/ui-file-picker/handoff-before-test.log`、`handoff-after-test.log`。最终版本再次执行 N1，仍准确失败 2 条、恢复后通过。
- 视觉：将 T1 产生的 `snap-scene.png` 和 `metrics.json` 复制到 `work/ui-file-picker/compare/FilePicker/`，执行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out work/ui-file-picker/compare --scenes FilePicker --reuse`。使用真实、已静止且断言通过的截图，未改图。

T2/T3 配置沿用[总规则](../codex-rules.md)第 5 节的全部参数，两个目录都额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；T3 仅改目录 `build-asan`，再加 `-DINK_SANITIZE=ON`。两者均保留 `INK_WERROR=ON`。实际编译参数已核对含 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`。

按第 7 节不提交生成能力表：测试前备份其原内容，运行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py` 临时生成，两套全量结束后已逐字节恢复，并核对与 HEAD 原内容相同。合并者仍须统一生成能力表；干净检出的全量测试先执行该生成命令。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】T2、T3 配置、完整构建、最终 CTest 均退出 0；没有安装或升级依赖。全部命令遵守构建 `-j3`、测试 `-j2`，两套串行；GUI 冒烟沿用已有 SDL dummy 模式，没有可见窗口。

| 检查 | 最终结果 | 耗时 | 日志 |
| --- | --- | --- | --- |
| T2 普通全量 | 219/219，零失败、零跳过 | 151.46 秒 | `build/ui-file-picker-final-ctest.log` |
| T3 ASan/UBSan 全量 | 219/219，零失败、零跳过，未见 sanitizer 报错 | 285.63 秒 | `build-asan/ui-file-picker-ctest.log` |

【实测】最终两套选择器测试均 91 条断言通过、零失败、`finished=1`；取消后的画面对照均通过。普通与内存检查版的内部 RAM 最低余量同为 47,472 字节，内部及外部分配失败均为 0。确定性、分层、SDK、能力清单和中枢契约检查均保留并通过。

## 规模与架构审视

【实现】产品保守计数 201 行（增加 200、删除 1；包括 9 行 CMake 接线），低于 250 行。为什么不能更小：须同时提供选择会话、真实服务的后台读取、F4 绑定、系统浮层、异步编号回传、成员归属、四个共用能力和 AI 快照。只用 `file.list` 会裸传路径；在 UI 直接调用索引会阻塞，两种更小方案均不满足任务要求。

【实现】文件身份与权限归文件服务；选择器只保存临时编号和名称，全部放 PSRAM。选择前再次 lookup，回调和 AI 最终回执只返回编号。输入经能力总线，快照来自同一组 F1/F4 声明；没有直接修改文件服务状态。浮层保留原活动对象，取消只移除浮层。能力注册及 CMake 均追加，未重排共用清单。

【实测】最终差异只含选择器、UI 接线、测试和本报告；未修改文件服务、共享文档或已有调研、简报、审计资料。`git diff --check` 通过，本报告链接均存在；按总规则第 6 节指定正则扫描基线至最终提交的补丁，隐私项零命中（grep 退出 1）。构建产物、设计稿、对照图及反向脚本均未提交。

## 第 1 轮复查修复（功能完成，完整验证进行中）

【实现】复查列出严重 0、高 1、低 1，无中级。两项均已修复；选择器集中绑定调用页现有的删除事件，页面销毁时关闭浮层、丢弃回调，正常结束时解除绑定；第二行位置跟随已有动态行高。没有修改导航或文件服务。产品累计 208 行，低于 250 行；本轮产品增删合计 15 行。

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1-01 高 | 【实测】已修复 | 真实 AI `system.home` 销毁调用页，浮层随之消失，后续取消不再回调；单独撤回生命周期修复后 6 条断言失败，包含销毁后回调及其后续计数失败 | R1-T、R1-N |
| R1-02 低 | 【实测】已修复 | 真实最大字号和孩子字号各检查 4 行文字框，均不重叠；单独撤回行高修复后仅这 8 条断言失败；已打开两张截图核对 | R1-T、R1-N |

- R1-T：`taskpolicy -c utility cmake --build build --target sim-ui-file-picker -j3` 后执行 T1。构建、测试均退出 0，2/2、121 条断言、零失败和告警；内部 RAM 最低余量仍为 47,472 字节，内外部分配失败均为 0。日志 `work/ui-file-picker/r1-after-build.log`、`r1-after-test.log`，指标 `r1-after-metrics.json`。
- R1-N：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" work/ui-file-picker/r1-negative.py`。只在编译器标准输入中分别撤回完整生命周期修复、行高修复，链接本轮原测试，不改源文件或原库。两次编译、链接退出 0；运行均退出 1、`finished=1`，分别 6、8 条失败。日志及指标在 `work/ui-file-picker/r1-negative-{home,text}*`。初次只撤掉事件绑定，仍保留解绑代码，夹具访问了已销毁的页面而退出信号 11；未将其作为反向证据，改为撤掉完整生命周期修复后得到上述结果。
- 原始修复前回归也已运行：`r1-before-test.log`，退出 8、`finished=1`、14 条失败；修复后同一测试通过。新增测试保留全部既有断言和预算，没有改已有期望、跳过或增加掩盖错误的等待。
- 当前默认字号画板差值仍为 28.414/255，原因同“和设计不一致的”；本轮对照在 `work/ui-file-picker/r1-compare/FilePicker.png`。
- 普通及 ASan/UBSan 全量验证尚在进行；本轮没有新增任务、状态机、文件格式、配置项或其他机制。共享文档按第 7 节保留原样。
