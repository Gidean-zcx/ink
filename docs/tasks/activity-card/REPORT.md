# activity-card 执行报告

状态：实现、反向对照、两套全量验证与收尾检查结束；未通过项列在下方。基线 `50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`。本次实测均在 macOS 模拟器中进行。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过（media 会话） | 同一真实音频标题、暂停／接着放、停止消失 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^activity-card$' --output-on-failure -j2` |
| 2 | 【实测】通过 | 模拟调制解调器实际接通；触摸和 AI 返回 PhoneCall；挂断消失 | 同上 |
| 3 | 【实测】通过 | 声音继续；触摸和 AI 不能在锁屏暂停；电源键唤醒后可暂停 | 同上 |
| 4 | 【实测】通过 | 真实 job.list、阶段更新、job.cancel 回调、快照与家人隔离 | 同上 |
| 5 | 【实测】部分通过 | 常规、ASan/UBSan 均 241/242 通过，唯一失败均为共享能力文档未生成；未报告内存错误。产品代码 308 行（含增删和 CMake），五屏截图已留存 | 全量命令与证据见下文 |

## 没做到的

- 两套全量唯一未通过项均为 `capability-table-doc`：总规则第 7 节禁止改共享生成文档，留给合并时统一生成。
- 没有运行 Linux/gcc 或实机验证。
- 五屏设计差分超过目标；ActivityCard 组件在两个宿主页截图中检查，未单独测量组件画板的差分。
- 非 media 会话的播报、提示音没有对应的暂停／续播能力，未新增这类控制逻辑；`audio.now_playing` 用于真实听音的锁屏标记。

验证命令（按总规则配置两个构建目录，均开启 Werror；内存目录另开 `INK_SANITIZE`；另加 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 复用已有依赖）：

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub/src" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

常规证据：`build/activity-card-build.log`、`build/activity-card-ctest.log`（333.92 秒）。内存检查证据：`build-asan/activity-card-build.log`、`build-asan/activity-card-ctest.log`（699.67 秒）。两次 ctest 均退出 8，均无跳过；唯一失败为上述文档校验。两套活动验收均为 149 条断言、0 失败，内部 RAM 最低余量均为 44,040 字节，未放宽 32,768 字节预算。备份相关回归也通过（`build/activity-card-targeted.log`）。

## 和设计不一致的

【实测】对照命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/activity-card-compare --reuse --scenes MainPlaying ControlPlaying MainInCall LockPlaying Tasks`。

| 画板 | 1/4 灰度差（目标 ≤12/255） | 主要差异 |
| --- | ---: | --- |
| MainPlaying | 51.734 | 现行 1.2 倍尺寸、较深的静态灰；音频真实文件名和状态，无章节数据 |
| ControlPlaying | 50.494 | 同一字号和灰度差；真实家人、音量、网络和通知；底部增设任务页入口 |
| MainInCall | 72.390 | 保留状态栏，黑色通话条在内容顶部；号码代替示例联系人，无分钟时长 |
| LockPlaying | 51.062 | 保留的前台是主屏，设计图的底图是阅读页；小标记为正在听 |
| Tasks | 32.283 | F4 固定行高与分页，结束项为独立描边行；真实数量、可取消条件和阶段；无未知百分比进度条 |

截图和拼图：`build/activity-card-compare/`；原始帧：`build/test-out/activity-card/snap-*.png`。遵守现行尺寸、灰度规范；服务没有的章节、联系人名、进度未编造。

## 改过的已有测试

`tests/integration/ai_surface.c` 的 AI 可调用清单新增 `phone.return`、`shell.tasks`、`tasks.prev`、`tasks.next` 四项（均无参数），使既有逐项覆盖断言检查新增能力；已有断言和原期望未删改。

## 要用户定的

无。

## 顺手发现

【实现／实测】全局能力门禁按家人空间是否打开判断；锁屏短暂保留期内，部分 AI 调用仍可执行，既有 `integration-member-wake-stream` 就以 AI 的 Wi-Fi 扫描覆盖这段窗口。这与规范 §3.5 的统一拒绝描述有差异，留主会话核对；本任务在媒体控制入口限制锁屏暂停／续播／停止。

## 新增的机制

F6 要求的共用活动呈现和 Tasks 页面；复用媒体、电话、作业和列表组件，无新线程、定时器、文件格式、配置项或后台重试。

增量接口：`job.revision` 通知真实阶段变化；`phone.return` 返回现有通话页；`shell.tasks`、`tasks.prev`、`tasks.next` 开放页面和翻页。锁屏 AI 暂停／续播限制在媒体服务既有 `need_session` 入口集中处理；只在活动内容改变时重画主屏和控制中心。

【实测】六个独立撤回：隐藏主屏卡片、断开通话返回、移除锁屏媒体限制、替换正在听标记、取消按钮不调 job.cancel、撤回阶段通知，均构建退出 0、测试退出 1，并命中对应断言；恢复后通过。复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/activity_card_reverse.py --build build`。证据：`build/activity-card-reverse/summary.json`。

产品代码超过 100 行的原因：共用卡片、job.list 解析及分组分页、三屏接线和能力需要一起提供。更小方案是各页直接拼字符串、静态作业或不做分页，分别违反共用数据、真实作业和无滚动要求，未采用。

## 要改的文档

合并时更新 `docs/system/ui-dependencies.md` 的 F6 和 Tasks 状态，并重新生成 `docs/system/capabilities.md`（新增能力和 job.revision）；按总规则第 7 节未修改共享文档。生成预览在 `build/activity-card-capabilities.md`。

## 收尾检查

【实测】隐私扫描相对基线无命中（grep 退出 1）；`git diff --check` 通过。改动文件均属于本任务，未修改共享文档和历史调研。未触碰设备、推送或创建 PR。
