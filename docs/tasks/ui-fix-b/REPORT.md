# ui-fix-b 执行报告

基线：`7bb9386a0330eb6be71301a06579e7f26aad73a2`。本报告仅记录本任务的本地修改和证据。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| UX08（T16～T25） | 中文专项通过 | 【实测】每档 35 项；不支持叫名字时无可点档位并显示原因；备份行含自动备份当前值；读数失败可重读；放大后清空动作和后果可见；翻页恢复可用状态 | C1 |
| UX10（T26～T29） | 中文专项通过 | 【实测】每档 63 项；禁用原因、短标题、单选勾；年、月、日、时、分均直接输入，保留加减和最后保存 | C1 |
| UX16（S19～S26） | 中文专项通过 | 【实测】每档 36 项；验证对象、4～8 位规则、3×4 键盘右下确认；真正三次错误触发既有冷却并禁用数字及确认，秘密仍掩码 | C1 |
| UX24（T14～T15） | 中文专项通过 | 【实测】每档 20 项呈现检查；发现、已配对、连接中分组；原有蓝牙测试新增实际点按“忘记”并核对删除配对记录，原长按检查保留 | C1、C3 |
| 普通与 120% 前后截图、反向对照 | 通过 | 【实测】[74 张图](SCREENSHOTS.md)，最大 35,880 B；基线四组分别触发 5、4、6、4 项失败，两档字号均如此；[结构化结果](EVIDENCE.json) | C1 |
| 普通全量 ctest（含 ui-scale） | 未全过；修复复测剩 2 项共享收尾 | 【实测】首轮原话 `98% tests passed, 8 tests failed out of 401`，1140.88 秒，3 项中枢跳过；随后 14 项复测 12 过、仅 `i18n-literals` 和 `capability-table-doc` 未过；`ui-scale` 已过 | C2、C3 |
| 内存检查、S31、gcccheck | 待执行 | 待补 | 待补 |
| 范围、行数与隐私检查 | 待执行 | 产品代码预算 280 行 | 待补 |

## 没做到的

- 新文案的共享英文词条、生成的能力文档未合并，导致两项检查仍失败；补丁已在隔离副本通过词条和能力文档检查，正式工作区未应用。
- 本批未处理：T17 候选名选择、T20 语言选择页、T21 危险组、T22～T25 信息和隐私页整理；S19 家人管理入口、S20 恢复目的地、S24 再次核对新密码、S26 真实任务摘要；T28 地区分组和 UTC 偏移。
- 未实现密码找回；未触碰驱动、设备或原厂代码；不把模拟器结果当真机验收。

## 和设计不一致的

【实现】为给固定数字键盘和错误提示留出空间，密码页去掉装饰头像，用家人姓名和阶段文字标明对象。键位统一为删除／0／确定，取消或忘记密码在数字区上方。

新增中文文案尚未接入共享翻译表，英文界面会出现这些中文；任务范围不允许写该共享文件，已给主会话提供具体收尾补丁。

## 改过的已有测试

`tests/settings_a.py` 改用已有的支持叫名字的档案，保留原有点击档位和 AI 调用断言；默认不支持档案由本任务专项测试覆盖。`tests/settings_a.c` 的“正在备份”可见值增加“自动备份：当前值”，精确核对新的完整文案；未删或放宽断言。

`tests/bluetooth.c` 增加一次重新配对后点按可见“忘记”的路径，核对配对记录消失；原有长按和失败断言均保留。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

无。

扩展已有草稿能力 `shell.time_step`：`delta=0` 打开数字输入，可选整数 `value` 更新草稿，原必填 `field,delta` 保留；是 UX10 要求的直接输入入口，仍走现有文本服务和能力总线。蓝牙仅增加显示用的连接中标记。未新增能力、任务、线程、状态机、持久格式、队列或配置项。

## 要改的文档

合并时由主会话同步生成的能力表（仅新增 `backup.auto` 依赖），并给新中文提示补统一英文词条和 `TR` 标记。具体内容见 [共享收尾](SHARED-FOLLOWUP.md)及未应用的 [补丁草案](SHARED-FOLLOWUP.patch)。本任务未修改这两处共享文件，相关检查尚不能通过。

## 复现命令与证据边界

- C1：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fix_b/run.py --build build --reverse --screenshots docs/tasks/ui-fix-b/screenshots`。
- C2：按任务书配置与 `cmake --build build -j2` 后，`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。
- C3：C2 加 `-R '^(ui-scale|i18n-shell|i18n-literals|capability-table-doc|integration-ai-surface|settings-b|storage-usage|file-index-3|time-settings|bluetooth-settings|settings-a|ui-scene-Pin|ui-scene-big-Pin|semantics-shell2-member)$'`。
- 专项测试直接编译相应呈现文件，基线对照用临时对象替换，不改工作区、不切历史提交；仅 `--reverse` 需要 Git 历史。没有改共享 CMake 或公共测试。
- 截图和错误状态用合成输入；蓝牙分组／连接中是呈现夹具，密码等待由三次真实错误密码触发。完整业务由原有时间、蓝牙、成员测试补充。
- 完整命令输出留在忽略的 `work/ui-fix-b/` 和 `build/ui-fix-b/`，不提交构建产物。
