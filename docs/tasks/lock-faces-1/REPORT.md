# lock-faces-1 执行报告

基线：`69cc9868a5e6b95c8376293b385308431fef92a0`。2026-11-11～12；检查与本地提交已完成。**完整验收未通过，新表盘还不能从真实设置入口使用。**

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 部分完成【实测】 | [40 张截图](SCREENSHOTS.md)，均小于 300 KB；新枚举为绘制替身，真实设置入口未接通 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_faces.py --build build` |
| 2 | 部分完成【实测】 | 最终普通／ASan 各 343 条正向断言过；分钟不变、成员隔离、继续读的反向断言均触发；跨日反向未到预期断言；节气和锁屏自动调度未做 | 同上；反向命令见下文 |
| 3 | 未通过【实测】 | 普通全量 5 失败／3 跳过，ASan 全量 4 失败／3 跳过；最终锁屏相关回归均过；gcccheck 过；S31 固件按任务书留给合并主会话 | 见“检查结果” |
| 4 | 已完成文档【实现】 | 布局出处、宜忌缺口和上机核对清单见下文 | `SOURCE.md`、本报告 |

## 没做到的

- 宜忌按任务书不显示；CrossMux 按日支循环选词不能作为准确黄历。
- 新表盘真实设置枚举、设置页入口和锁屏自动分钟／跨日调度：受文件范围限制，未接通，不能当作可交付的完整锁屏功能。
- 节气：本地历法没有接口；没有把 CrossMux 近似公式包装成与 `lunar.c` 一致的结果。
- 锁屏句子：已选功能中有这一项，但现有阅读器和公开接口未提供当前成员明确选句的来源；本任务未自建摘录格式或存储。
- S31 固件编译和真机刷新、残影：按任务书由合并主会话跑，本任务没有碰设备。
- 新文案的英文翻译与 TR 标记未完成：现有翻译表不在允许修改范围，国际化检查未过；没有绕过检查或另建翻译机制。
- 跨日反向：三次尝试后仍停在等待新日期，900 秒模拟上限结束时 `test.finished` 未记录、未触发预期跨日断言；按规则停下，不把超时当成断言型对照通过。

## 和设计不一致的

【实现】有三个范围外接线缺口，不能称为完整可用功能：

- `navigation.c:1995–2002` 将 `display.lock_style` 固定为 0～2，设置服务拒绝重复定义。新样式 3～5 不能由实际设置能力选中。
- 实际 Wallpaper 页在 `settings_family.c:254–260`，不在 `wallpaper.c`。后者已提供 `ink_lock_face_settings()` 绘制入口，但原页面没有调用它。
- `time_service.c:197–209` 锁屏时取消分钟调度。本任务消费已有 `time.minute`，没有新增定时循环或改电源、睡眠代码；不能承诺锁屏自动走时。

上述三个文件不在允许修改范围；没有通过改写只读元数据或偷偷加循环绕过限制。

新增页面还未符合既有英文接入合同；国际化失败是实际缺口，不能只当文档检查问题。

## 改过的已有测试

未改已有测试期望；新增测试明确使用表盘枚举替身验证绘制，并断言真实 `settings.set(display.lock_style=3)` 仍被拒绝。

## 要用户定的

- 宜忌算法及可信来源：可选传统建除十二神规则（需核定流派、时区和边界），或有明确许可的历书数据；未确认前不显示。
- 壁纸图库内容未定，不内置第三方图片。
- 主会话需安排上面三个范围外接线点，才能完成真实设置页选择和锁屏自动刷新。

## 顺手发现

- 【实现】`lunar.c` 和 `ink/sys/time.h` 没有节气接口。CrossMux 节气是世纪常数近似公式，源码自己注明有一天误差；未混入第二套未经核定的历法。
- 【实测】普通全量 `unit-s31-cell` 报 ACK 超时并在 `cell.c:16` 断言 `r==HAL_AT_OK` 失败；单独复跑 `100% tests passed out of 1`。该目标未链接本次 UI 文件，未改模组代码；尚未证明整轮为什么失败。
- 【实现】公用 `WAIT_FOR` 只累计轮次内的 20ms 延迟，未计条件中 `on_ui()` 的等待时间；条件耗时会拉长实际等待。【实测】跨日反向未在模拟上限内到达断言，未进一步定位；本任务未改公用测试辅助。

## 新增的机制

- 产品常驻任务、定时循环、文件格式：均无新增。
- 配置项：`display.lock_todo`、`display.lock_reading`、`display.lock_background`，均为任务书要求的现有设置服务布尔项，按成员保存，默认关。
- 继续阅读只保存一次待执行意图及所属成员，完成验证后调原有 `reader.continue`，换成员清除；不存 PIN、不加能力或后台重试。
- 仅测试的 `lock-faces-test` 驱动与用例选择参数：任务书要求验证／反向对照，复用既有测试辅助，不进入产品固件。

## 要改的文档

共享文档由主会话合并时更新；本任务不改：界面规范的表盘枚举和开关、能力文档的设置项说明、锁屏分钟／跨日调度契约。

还需主会话更新 `assets/i18n/en.tsv` 并完成新文案的 TR 接入；`docs/system/capabilities.md` 要按 38 个设置项重新生成（原来 35）。

## 检查结果

- 【实测】普通构建成功；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` 原话：`99% tests passed, 5 tests failed out of 402`；`Total Test time (real) = 1298.31 sec`。失败：`unit-s31-cell`、`i18n-shell-remaining`、`i18n-extras`、`i18n-literals`、`capability-table-doc`。
- 【实测】3 个跳过：`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`。缺本检出的锁定中枢 Python 环境及其生成消息；没有安装／升级依赖。
- 【实测】国际化原文：`i18n-literals: 31 unmarked Chinese literals`；另两项分别报剩余壳文案缺 6 次翻译、日历／文件页的缺翻译计数非 0。能力文档检查报 `capability catalogue is stale`。
- 【实测】`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_faces_reverse.py --build build --case minute`：撤掉分钟相等保护后触发 `same minute does not refresh`；`--case member`：撤掉成员变化时重建锁屏后触发 `child pixels contain no adult memo label`；`--case reading`：撤掉 `reader.continue` 调用后触发 `unlock resumes the reader`。三项均在构建目录编译替身，不改产品文件、不依赖 Git 历史。
- 【实测】恢复实现后的最终正向，普通／ASan 均为 `lock-faces: exit=0, pass=343, fail=0, finished=1`。跨日反向的未完成情况见上文。
- 【实测】ASan/UBSan 按总规则同一配置加 `-DINK_SANITIZE=ON` 构建整个主工程，运行 `UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2`：`99% tests passed, 4 tests failed out of 402`；`Total Test time (real) = 1299.51 sec`。失败为上述 4 项国际化／文档检查，跳过同 3 项中枢测试；完整测试日志未检出 ASan/UBSan 诊断。
- 【实测】全量检查后补齐旧分钟时钟的壁纸叠加和文字底色。普通／ASan 的 7 项锁屏相关回归各为 `100% tests passed out of 7`（25.58／44.55 秒）；最终壁纸对比度与日期行整理后，`lock-faces`、`ui-scale` 两项在普通／ASan 下又分别通过。没有把这些回归写成全量全过。
- 【实测】最终 GCC 检查：`gcccheck: 4 compiled, 0 with findings`。指定隐私正则扫描基线至最终提交：0 命中，grep 返回 1；`git diff --check` 无输出。只提交任务允许的源码、测试、截图和本任务文档。

## 布局、边界与阶段证据

- 简约：取 CrossMux `SloppyClockFace.cpp:58–67` 的上下两行时分；加公历、星期、农历和原有电量栏。待机：时间放上方，下面两行给待办和继续读。
- 月历：取 `ChineseCalendarFace.cpp:177–244` 的公历、农历、干支、生肖层次和按日更新条件。上游实际为单日黄历；月格是任务书要求的扩展，每格公历＋农历，初一显示月名，今天黑底。
- 来源、固定提交、SHA-256 与 MIT 许可见 `system/ui/lock_faces/SOURCE.md` 和 `LICENSE`；没有拷待机循环、随机字形引擎或历法表。
- 【实测】最终针对性场景：`test.pass=343, test.fail=0, test.finished=1`，内部 RAM 最小空闲 50,088 字节，外部 RAM 分配失败 0；120% 月历组件已压为单行，壁纸上的文字使用纸白底色，复测通过。
- 【实测】`ui-scale: 54 files, 0 violations`；`gcccheck: 4 compiled, 0 with findings`。
- 【实现】壁纸透明度处理沿用 `system/services/media/image.c:39–45`，透明像素合成到纸白后抖动，不新增解码器。测试只造合成 PNG。
- 【实现】设置预览和开关均绑定原有 `settings.set`；快照复用统一语义模型；备忘只读当前成员集合，阅读复用原有卡片与能力；密码仍由现有成员门验证，AI 不能代输。
- 产品代码：新增 262 行、删除 8 行、净增 254 行（含头文件，排除测试、文档、CMake），小于 450 行预算。为什么不能更小：三个表盘、月格、两个异步／按成员组件、快照和设置预览是独立验收项；只扩旧分钟时钟或搬上游循环均不满足合同。复用了农历、备忘数据集合、阅读卡片、异步打开集合、图片服务和设置能力，未加通用框架。

## 上机核对清单

- 各表盘分钟局部更新、跨日月历更新的实际刷新区域和残影。
- 透明 PNG 与前景文字的对比度、120% 字号和触摸目标。
- 相机、紧急呼叫和继续阅读的锁屏点按及验证流程。
