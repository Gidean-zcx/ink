# activity-card 执行报告

状态：两项复查意见已修复；反向对照、两套全量验证与收尾检查结束，仅能力表文档同步检查按用户指示豁免。任务基线 `50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`，复查修复基线 `c48270a3`。本次实测均在 macOS 模拟器中进行。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过（media 会话） | 同一真实音频标题、暂停／接着放、停止消失 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^activity-card$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 模拟调制解调器实际接通；触摸和 AI 返回 PhoneCall；挂断消失 | 同上 |
| 3 | 【实测】通过 | 锁屏声音继续、触摸不能暂停；电源键唤醒后可暂停；远程暂停／续播照常生效 | 同上 |
| 4 | 【实测】通过 | 真实 job.list、阶段更新、job.cancel 回调、快照与家人隔离 | 同上 |
| 5 | 【实测】通过（1 项按指示豁免） | 常规、ASan/UBSan 均 241/242 通过，唯一失败均为共享能力文档未同步；未报告内存错误。产品代码 305/350 行（含增删和 CMake），五屏截图已留存 | 全量命令与证据见下文 |

## 没做到的

- 两套全量唯一未通过项均为 `capability-table-doc`：能力表未同步新增的四个能力和 `job.revision` 等内容；用户本轮明确豁免，留给主会话合并时统一生成。
- 没有运行 Linux/gcc 或实机验证。
- 五屏设计差分超过目标；ActivityCard 组件在两个宿主页截图中检查，未单独测量组件画板的差分。
- 非 media 会话的播报、提示音没有对应的暂停／续播能力，未新增这类控制逻辑；`audio.now_playing` 用于真实听音的锁屏标记。

验证命令（按总规则配置两个构建目录，均开启 Werror；内存目录另开 `INK_SANITIZE`；另加 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 复用已有依赖）：

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub/src" INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub/src" INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

本轮常规证据：`build/activity-card-review-configure.log`、`build/activity-card-review-build.log`、`build/activity-card-review-ctest.log`（212.33 秒）。内存检查证据：`build-asan/activity-card-review-configure.log`、`build-asan/activity-card-review-build.log`、`build-asan/activity-card-review-ctest.log`（615.06 秒）。两套配置、构建均退出 0；两次 ctest 均退出 8，均无跳过，唯一失败为上述文档校验。两套活动验收均为 152 条断言、0 失败、0 警告；内部 RAM 最低余量均为 44,040 字节，未放宽 32,768 字节预算。

## 和设计不一致的

【实测】对照命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/activity-card-compare --reuse --scenes MainPlaying ControlPlaying MainInCall LockPlaying Tasks`。

| 画板 | 1/4 灰度差（目标 ≤12/255） | 主要差异 |
| --- | ---: | --- |
| MainPlaying | 51.465 | 现行 1.2 倍尺寸、较深的静态灰；音频真实文件名去扩展名和状态，无章节数据 |
| ControlPlaying | 50.215 | 同一字号和灰度差；真实家人、音量、网络和通知；底部增设任务页入口 |
| MainInCall | 72.390 | 保留状态栏，黑色通话条在内容顶部；号码代替示例联系人，无分钟时长 |
| LockPlaying | 50.837 | 保留的前台是主屏，设计图的底图是阅读页；小标记为正在听 |
| Tasks | 32.283 | F4 固定行高与分页，结束项为独立描边行；真实数量、可取消条件和阶段；无未知百分比进度条 |

截图和拼图：`build/activity-card-compare/`；原始帧：`build/test-out/activity-card/snap-*.png`。遵守现行尺寸、灰度规范；服务没有的章节、联系人名、进度未编造。

## 改过的已有测试

`tests/integration/ai_surface.c` 的 AI 可调用清单新增 `phone.return`、`shell.tasks`、`tasks.prev`、`tasks.next` 四项（均无参数），使既有逐项覆盖断言检查新增能力；已有断言和原期望未删改。

本轮按复查改 `tests/activity_card.c`：主屏／控制中心的标题期望改为去掉 `.opus`；停止后检查去扩展名的标题也消失；删除“锁屏 AI 暂停必须被拒绝”的错误期望，改验锁屏快照没有暂停动作、真实触摸无效、远程暂停／续播成功且仍锁定、唤醒后按钮暂停成功。没有删测试或放宽预算。

## 要用户定的

无。

## 顺手发现

【实现／实测】初轮发现全局能力门禁按家人空间是否打开判断，锁屏短暂保留期内部分 AI 调用仍可执行，既有 `integration-member-wake-stream` 就以 AI 的 Wi-Fi 扫描覆盖这段窗口。主会话复查 AC-M1 明确“暂停先唤醒”约束的是人的锁屏触摸，远程媒体控制应照常工作；本轮撤回媒体服务额外限制，全局成员门禁未改。

## 新增的机制

F6 要求的共用活动呈现和 Tasks 页面；复用媒体、电话、作业和列表组件，无新线程、定时器、文件格式、配置项或后台重试。

增量接口：`job.revision` 通知真实阶段变化；`phone.return` 返回现有通话页；`shell.tasks`、`tasks.prev`、`tasks.next` 开放页面和翻页；只在活动内容改变时重画主屏和控制中心。初轮加的锁屏媒体来源限制已按 AC-M1 删除。

【实测】本轮七个独立反向对照：隐藏主屏卡片、断开通话返回、加回旧锁屏限制、撤回去扩展名、替换正在听标记、取消按钮不调 job.cancel、撤回阶段通知，均构建退出 0、测试退出 1，并命中对应断言；恢复后 152 条断言通过。复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/activity_card_reverse.py --build build`。证据：`build/activity-card-reverse/summary.json`、`build/activity-card-review-reverse.log`。

产品代码超过 100 行的原因：共用卡片、job.list 解析及分组分页、三屏接线和能力需要一起提供。更小方案是各页直接拼字符串、静态作业或不做分页，分别违反共用数据、真实作业和无滚动要求，未采用。

## 要改的文档

合并时更新 `docs/system/ui-dependencies.md` 的 F6 和 Tasks 状态，并重新生成 `docs/system/capabilities.md`（新增能力和 job.revision）；按总规则第 7 节未修改共享文档。生成预览在 `build/activity-card-capabilities.md`。

## 复查后的修复

本轮按主会话的 `REVIEW.md` 修复 AC-M1 和 AC-L1，基线 `c48270a3`。删除媒体服务新增的锁屏来源限制；卡片取标题后用两行去掉最后的扩展名。未新增机制。

| 复查项 | 结论与证据 | 命令 |
| --- | --- | --- |
| AC-M1（中） | 【实测】通过。锁屏原暂停位置触摸无效；远程暂停／续播改变真实媒体状态且不唤醒；电源键唤醒后原卡片可暂停。加回旧限制，反向对照失败 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^activity-card$' --output-on-failure -j2`；反向命令见上文 |
| AC-L1（低） | 【实测】通过。主屏、控制中心的可见标题和 AI 快照共用“星星邮差”，不带 `.opus`；撤回两行去扩展名，反向对照失败 | 同上 |

【实测】先只补测试：旧实现构建退出 0，ctest 退出 8，命中 6 条失败断言（标题 3 条、锁屏远程操作 3 条）；修复后同一 ctest 退出 0。证据：`build/activity-card-review-red-build.log`、`build/activity-card-review-red-test.log`、`build/activity-card-review-green-test.log`。两项独立反向对照及原有五项均通过。

【实现】R-061：标题处理留在共用卡片；锁屏触摸仍由原输入服务拦截；按钮、AI、远程继续共用媒体能力，成员与会话检查保留，没有新增能力或机制。本轮产品代码共改 7 行（增 3、删 4），相对任务原始基线总计 305/350 行（含增删和 CMake）。

【实测】本轮常规和 ASan/UBSan 全量均已完成，结果及日志见上文；没有豁免项以外的失败，也没有跳过。开工时两目录记录的旧失败同样只有能力表同步检查。中枢的契约、JSON 交叉校验和端到端测试全部通过；复用 `$INK_DEPS/hub/.venv` 的 `pytest`、`pytest_asyncio`、`aiohttp`、`PyYAML`，用 `PYTHONPATH="$PWD/hub/src"` 指向本检出源码。缺依赖：无。

## 收尾检查

【实测】隐私扫描相对任务基线和复查基线均无命中（grep 退出 1）；`git diff --check` 通过。复查增量为 5 个文件：两处产品文件、活动测试、反向对照和本报告。开工时已有且未跟踪的 `REVIEW.md` 保留原样，不纳入提交。未修改共享文档和历史调研，未触碰设备、推送或创建 PR。
