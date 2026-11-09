# ui-list-paging 执行报告

日期：2026-11-09。基线：`fa7c18803f8f0eefc44db39acc2da053c7aa9386`；开工时工作区干净。实现提交：`a059ee20`；描边与既有测试修订：`918b5a7c`。状态：五项本机验收完成；视觉分数未达目标，差异见下文。仅本地提交，未推送。

代码：[接口](../../../system/include/ink/sys/ui_list.h)、[实现](../../../system/ui/ui_list.c)、[通知示范页](../../../system/ui/shell/navigation.c)、[验收测试](../../../tests/ui_list_paging.c)、[快照校验](../../../tests/ui_list_snapshots.py)。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 1000 项：3 个项根对象＋3 个文字对象；含容器、空态和分页条的子树共 14 个对象；翻页复用项对象 | `ctest --test-dir build -R '^ui-list-(paging|snapshots)$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 7 份完整 JSON 快照；末页 1 项，空页 0 项，单页 2 项 | 同上 |
| 3 | 【实测】通过 | 真实文件服务生成编号，进入后改名，返回第 2 页且仍选中同一编号 | 同上 |
| 4 | 【实测】通过 | AI／触摸分页条／实体翻页键均调用同一能力；快照有页码、总页数及截断说明 | 同上 |
| 5 | 【实测】通过 | 普通、ASan／UBSan 全量各 217/217；产品保守计数 288 行 | 见“完整验证命令” |

## 没做到的

- Linux／gcc 和真机未运行；本报告的运行证据仅是 macOS／clang 模拟器。没有碰设备、推送或开 PR。
- 只迁移通知列表；全部应用网格、设置分组、附近网络、选人等其它分页保留现状。
- 未扩展到未知总数、数据重排／删除后的重新定位、跨重启位置恢复；这些不在本任务验收内。

## 和设计不一致的

- 【实测】通知画板对比为 **24.323/255，未达到 12/255**；对照图和分数在忽略目录 `work/ui-list-compare/`。现有场景只发一条“今日报纸”合成通知，画板有三条；产品仍缺画板里的时间、回复／回拨、“读未读通知／全部清除”，底部保留已有“全部已读”。分页条按容量固定位置。字号和灰色使用现行规范，原图尚是旧尺寸。这些差异如实保留，没有为分数调整规格或补做其它应用。
- 【实现】空页定义为第 1/1 页、总项数 0、无项动作，显示空态；不生成 0 页或虚构记录。单页同样显示 1/1，前后动作不可用。

## 改过的已有测试

- `tests/sdk_boundary.cmake`：公开 SDK 允许表只追加 `ui_list`，使应用能包含分页接口。
- `tests/ui_shell_test.c`：原“翻回后快照逐字节相同”改为仅排除 `page_version` 数字、其余内容仍逐字节相同；新增断言要求恢复页的版本更新。F1 要求每次翻页使旧请求失效，原始全量因此有 1 项失败；未改变记录、动作、分页及资源预算的期望。

## 要用户定的

- 无；按任务书完成可逆实现，没有提出新产品选择。

## 顺手发现

- 【实测】测试夹具误用了 Plus 没有的返回键，已改走现有返回按钮能力；重复运行遗留的合成改名目标已由夹具定点清理。
- 【实测】单独构建测试目标时，首次缺图标资产；完成规则要求的全量构建后测试正常。没有修改依赖或测试预算。

## 新增的机制

- F4 要求的可见项复用、列表／网格排列、分页与位置记录；建立在 F1 声明／快照／能力校验上，所有视图内存使用 PSRAM。
- 没有新增产品任务、线程、持久格式、后台重试、缓存或配置项。
- F4 验收程序使用一个测试驱动任务，并经 `vtk_host_call` 输出 NDJSON 快照供 Python 校验；仅存在于测试可执行文件和被忽略的构建目录。

## 要改的文档

- 由合并主会话更新 `docs/tasks/backlog.md`、`docs/system/ui-dependencies.md` 的 F4 实际状态，并关联本报告；这里按总规则第 7 节不改共享文档。
- 后续 F13 的应用开发说明应收录 `ui_list.h` 用法：提供当前页绑定、已有翻页能力和按成员保存的位置。生成的能力总表无需变更，本任务没有新增产品能力编号。

## 阶段记录

- 【实测】首次完整普通构建退出 0。定向 ASan／UBSan 两项通过，1441 项断言；内部 RAM 最低余量 47,472 字节，原阈值 32,768；内外部内存分配失败均为 0。
- 【实测】尺寸检查 27 个文件、零违规；按当前新增／删除均计的保守产品代码计数为 288 行（含测试 CMake 接线），未超过 300 行。
- 【实现】只替换通知列表；通知数据与动作来自现有服务，没有改服务内部状态，也没有增加新能力编号。
- 【实测】普通首轮全量 216/217，唯一失败为上述旧快照比较。更新测试后定向通过；再临时撤掉 `ink_ui_content_changed()`，新版本断言确实失败，恢复后继续全量。

## 规模与架构审视

- 【实测】相对基线，产品保守计数 **230 行新增＋58 行删除＝288 行**，连 10 行 CMake 测试接线也计入；测试和文档不计。没有改服务、策略或模拟器实现。
- 为什么不能更小：需要公开的 F1 绑定接口、列表／网格可见项池、位置记录、完整分页快照和一个真实页面接线。仅包装现有页码条不能复用项对象、恢复选择或生成同源语义；只存页码也不能保留对象身份。这两种更小方案均不满足 F4。
- 【实现】界面组件只处理可见范围和导航位置；服务保有数据和业务动作。通知翻页继续使用 `shell.notify_next/prev`，读通知继续使用 `notify.read`；触摸、按键、AI 均进入既有能力总线及成员／版本校验。F1 从同一组项声明导出标签、对象和动作；测试还实际读取 `file.index/lookup` 验证文件编号改名后可用。
- 【实现】每页最多 16 项，单列是列表，多列是网格；绑定回调只更新当前页，调用方负责预先备好数据。组件及 LVGL 对象使用外部内存，不增加内部大缓冲、常驻任务或线程。

## 反向对照

【实测】每次仅临时更改 `system/ui/ui_list.c` 的一处行为，测试与预算不动；顺序构建目标并运行相应 CTest，然后恢复。6 次构建均退出 0，测试均退出 8，且 `test.finished=1`；没有以崩溃冒充反例。日志及摘要在忽略目录 `work/ui-list-negative/`。

| 撤掉的行为 | 复现改法 | 失败断言数 |
| --- | --- | ---: |
| 项对象复用 | 每次绑定前删除旧槽对象再创建 | 1 |
| 选中项编号记录 | 记录对象编号处改为清空 `selected` | 4 |
| 末页边界 | 移除 `turn()` 的正向末页判断 | 1 |
| 快照截断标记 | `truncated` 固定为 `false` | 2 |
| 按键能力路径 | 按键直接调用 `turn()`，绕过 `invoke()` | 4 |
| 翻页版本更新 | 移除 `ink_ui_content_changed()`，运行已有壳测试 | 1 |

前五项使用 `cmake --build build --target sim-ui-list-paging -j3` 和验收表中的 CTest；第六项改用 `sim-ui-shell-test` 和 `ctest --test-dir build -R '^ui-shell-priorities$' --output-on-failure -j2`。命令均加 `taskpolicy -c utility`。原有壳测试新加的版本断言确实能抓住第六项；恢复源码后定向通过。

## 完整验证命令

配置使用[总规则](../codex-rules.md)第 5 节的全部依赖参数，两个目录都额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；`build-asan` 只再加 `-DINK_SANITIZE=ON`。复用已有锁定依赖，未安装或升级包。全部构建最多 `-j3`，CTest 最多 `-j2`，逐轮串行；没有启动窗口。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out work/ui-list-compare --scenes Notifications
```

- 【实测】普通配置、构建与最终 CTest 均退出 0，**217/217，零失败、零跳过**，148.38 秒；包括确定性、SDK／分层、能力总表和服务器契约检查。日志：`build/ui-list-configure.log`、`build/ui-list-build.log`、`build/ui-list-ctest.log`。
- 【实测】ASan／UBSan 配置、构建和最终 CTest 均退出 0，**217/217，零失败、零跳过**，309.94 秒；日志为 `build-asan/ui-list-configure.log`、`build-asan/ui-list-build.log`、`build-asan/ui-list-ctest.log`。编译参数核实含 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`，运行设置 `UBSAN_OPTIONS=halt_on_error=1`。
- 【实测】两轮 F4 均 1441 项断言通过、零失败、`finished=1`；项对象 3 个、项子树对象 6 个、整个分页容器子树 14 个；内部 RAM 最低余量 47,472 字节，原阈值 32,768；内外部内存分配失败均为 0。全部原有资源预算保持不变。
- 【实测】最终改动只含组件、通知接线、CMake、对应测试和本报告，共 10 个相关文件。`git diff --check` 退出 0，本报告本地链接均存在；按总规则第 6 节扫描基线至最终提交的差异，隐私项零匹配（grep 退出 1）。没有修改共享文档、调研、简报、既有审计资料或设计稿。
