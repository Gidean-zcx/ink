# voice-name-assist 执行报告

基线：`a108101cbb81bd4a0b4785d66f0ab3a4fcb32003`。仅本地编译、宿主测试；不操作设备、不调用付费接口、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 改叫法 | 【实测】通过定向测试 | 支持三个名字，选择后旧名不触发、新名触发；无前端档案显示不支持；儿童不能改 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/voice_name_assist.py --build build` |
| 1 打字接话 | 【实测】通过定向测试 | 开关、800 ms 停顿、点按插入、作废、密码/隐藏框、离线、成员、儿童禁用、500 字边界、重启保存；日志 `work/voice-name-assist/focused.log` | 同上 |
| 2 验证 | 待运行 | 待补 | 待补 |
| 3 设计与截图 | 待补 | 待补 | 待补 |

## 没做到的

真实中枢没有接续模型，本次按任务要求只返回不可用；模拟中枢的三条接续是合成数据。没有上机。

【实现】S31 无前端的依据是 `ports/esp32s31/hal_stubs.c:29`；本次没有改硬件机制、参数或驱动，不作实机唤醒可用的结论。

## 和设计不一致的

设置页沿用 `SetVoice` 画板的行和开关；点名字轮换 HAL 支持项，不新增输入页。`ImeAi` 画板的长建议改用现有候选条中的短接续，带 AI 图标；三个候选可展开。停顿为 800 ms，超时为 3 s，未做实机体验校准。`system-services-design.md` §9、§15 中“AI 联想未实现”的旧描述待主会话更新。

## 改过的已有测试

无。

## 要用户定的

无。

## 顺手发现

无。

## 新增的机制

唤醒词设置 `voice.name`：任务第 1 项要求；只认 HAL 支持项。

成员设置 `text.assist` 和设置定义的 `per_member` 标志：任务第 2 项要求；沿用原设置 JSON 标量文件，以带成员后缀的键保存，没有新文件格式。

停顿定时器：任务第 2 项要求；回调只投递现有 UI 执行器，结果按输入会话、版本、成员/连接代际核对。不新增线程、队列、状态机、缓存或重试。

架构审视：前端走 HAL，选择由音频任务应用；按钮与 AI 共用 `settings.set` 和 `keyboard.pick`，候选进入现有输入快照；请求和回复沿用中枢成员边界。

## 要改的文档

按总规则第 7 节不提交共享文档。合并时重新生成 `docs/system/capabilities.md`，登记 `voice.name`、`text.assist`；`docs/system/ui-dependencies.md`、服务设计和 backlog 更新 G3a/G3b 状态。协议已更新 `docs/hub/device-protocol.md`。

## 为什么不能更小

新增两项设置、两端 HAL、输入停顿请求/结果失效、候选展示，以及中枢不可用桩，跨越任务要求的完整路径。只改设置页会留下假开关；另做接话面板或接入模型更大。本次复用设置文件、执行器、候选条和 `keyboard.pick`，没有新增服务层。

## 里程碑

- 实现及宿主定向测试已完成；13 个源码副本反向对照均使原断言失败，原文件不变。日志 `work/voice-name-assist/reverse.log`。
- 中枢新契约测试 1/1 通过；把不可用桩替换为假成功后，原契约测试失败，日志 `work/voice-name-assist/hub-{focused,reverse}.log`。
- 尚待全量、ASan 和 S31 编译，最终数字会在收尾时补齐。
- 首轮全量 362/365 通过，日志 `work/voice-name-assist/ctest-first.log`：默认关闭也创建定时器，导致两项听写场景内部 RAM 余量为 32672 B，低于原预算 32768 B；改为首次真正需要接话时创建。另将过长的英文不支持提示缩短，避免覆盖下一行。原测试和预算不改。
- 【实测】上述三项原测试与 `voice-name-assist` 复测 4/4 通过，日志 `work/voice-name-assist/fix-check.log`。两项听写场景内部 RAM 余量恢复为 32768 B；首轮失败即保留修复前的反向证据。

截图在 `build/voice-name-assist-evidence/fixed/`：`metalio-plus-handsfree/clean-voice-name.png`、`clean-assist-candidates.png`、`clean-assist-expanded.png`，以及 `metalio-plus/clean-voice-unsupported.png`。均为宿主合成场景，不提交生成图片。
