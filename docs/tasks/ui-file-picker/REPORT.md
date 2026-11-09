# ui-file-picker 执行报告

基线：`c6dede1945807b2cd688e6967c7299154813827e`。状态：实施中；仅本地提交，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 7 张图片按 4/3 分页；混入音频和文字不出现；触摸、AI 均返回同一稳定编号，调用方 lookup 成功 | T1 |
| 2 | 【实测】通过 | 原活动不重建，第十页第二项的位置与快照不变；取消前后每个像素相同 | T1 |
| 3 | 【实测】通过 | 孩子仅见家庭及自己的图片；私有编号伪造失败；换人关闭旧浮层；撤掉文件服务的一处权限检查，2 条断言失败，恢复后通过 | T1、N1、N2 |
| 4 | 待验证 | 全量、ASan/UBSan、250 行产品代码预算 | 待补 |

## 没做到的

- 尚待最终普通与内存检查全量结果；没有运行 Linux/gcc 或真机。
- 按任务范围只做系统选择器和测试调用方，未实现文件应用、缩略图、解码或转换流程。图片资格沿用文件服务的扩展名分类及访问权限；不声称文件内容可解码。

## 和设计不一致的

【实测】`FilePicker` 画板对照 **28.414/255**，超过 12/255。画板是四本书及准备状态，本次验收图是实际图片文件和“可以选择”；F4 现有组件是独立描边行，页码条跟在可见项池后面，画板是灰底分组、页码在底部。规范的字号已统一放大 1.2 倍、静态灰已改深，画板尚未同步。没有为分数改规格或伪造书籍准备状态。对照图：`work/ui-file-picker/compare/FilePicker.png`。

【实现】为使 F1/F4 在系统浮层上导出快照，`ui_semantics.c` 仅将页面身份匹配集中改用已有 `ink_ui_action_page()`；浮层与活动沿用同一能力及成员检查。未改服务接口和设计合同。

## 改过的已有测试

`tests/integration/ai_surface.c` 仅追加四项新能力及 `select` 的必填 `id`；`tests/sdk_boundary.cmake` 仅追加公开选择器头文件。已有断言与预算未放宽。

## 要用户定的

无；没有新增产品选择。

## 顺手发现

【实测】首轮普通全量 218/219，唯一失败是新测试重复运行时上次留下的孩子图片进入大人总数。已在新测试开头定点清理该合成夹具，未改产品代码或放宽断言；其后普通全量 219/219，零跳过，122.24 秒，退出 0。最终复核补上换人时关闭旧浮层，正在重跑两套全量。

## 新增的机制

F9 要求的临时文件选择模型、结果回调和四项能力；数据放 PSRAM，读取复用现有 BG/UI 执行器，换人取消复用已有成员状态订阅。不新增常驻任务、线程、文件格式或配置项。

## 要改的文档

待合并时统一更新共享能力表及界面依赖记录，本任务不提交共享文档修改。

## 命令与证据

- T1：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ui-file-picker' --output-on-failure -j2`。2/2 通过，91 条断言、零告警；内部 RAM 最低余量 47,472 字节，原阈值 32,768；内部和外部分配失败均为 0。日志：`work/ui-file-picker/restored-test.log`。
- N1：临时仅撤掉 `file_service.c` 的 `resolve_for` 中 `/members/` 分支的 `if (!c->adult) return INK_FS_ERR_DENIED;`，构建 `sim-ui-file-picker -j3`，运行 T1 中的 `^ui-file-picker$`。构建退出 0，测试退出 8，`test.finished=1`、`test.fail=2`；源码逐字节恢复，再构建及 T1 均退出 0。脚本：`work/ui-file-picker/reverse.py`；日志：同目录 `negative-build.log`、`negative-test.log`、`restored-build.log`、`restored-test.log`。首次夹具未清理的反向结果为 4 条失败，已由这次可重复结果替代。
- N2：新增测试先在大人的选择器仍打开时切到孩子。修复前“旧浮层移除”断言确实失败，`test.finished=1`、`test.fail=1`、CTest 退出 8；接入已有 `member.*` UI 状态通知后 T1 通过。换人或认证变化会关闭旧浮层，并丢弃旧调用方回调，避免对已经销毁的调用方回调。日志：`work/ui-file-picker/handoff-before-test.log`、`handoff-after-test.log`。最终版本再次执行 N1，仍准确失败 2 条、恢复后通过。
- 视觉：将 T1 产生的 `snap-scene.png` 和 `metrics.json` 复制到 `work/ui-file-picker/compare/FilePicker/`，执行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out work/ui-file-picker/compare --scenes FilePicker --reuse`。使用真实、已静止且断言通过的截图，未改图。

## 规模与架构审视

【实现】产品保守计数 201 行（增加 200、删除 1；包括 9 行 CMake 接线），低于 250 行。为什么不能更小：须同时提供选择会话、真实服务的后台读取、F4 绑定、系统浮层、异步编号回传、成员归属、四个共用能力和 AI 快照。只用 `file.list` 会裸传路径；在 UI 直接调用索引会阻塞，两种更小方案均不满足任务要求。

【实现】文件身份与权限归文件服务；选择器只保存临时编号和名称，全部放 PSRAM。选择前再次 lookup，回调和 AI 最终回执只返回编号。输入经能力总线，快照来自同一组 F1/F4 声明；没有直接修改文件服务状态。浮层保留原活动对象，取消只移除浮层。能力注册及 CMake 均追加，未重排共用清单。
