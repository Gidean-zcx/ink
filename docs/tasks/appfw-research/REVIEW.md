# 应用框架调研和设计复查

复查日期：2026-11-05。基线 `2163603`，被审提交 `1083f5d837eeaf94e90b8a8b736ea93d3cff8b31`。依据原始 brief、`docs/tasks/review-prompt.md` 和 `docs/tasks/codex-rules.md` 独立验收。

**结论：A1～A5 在本次纯设计交付范围内通过；未发现严重或高问题。** 这不代表应用框架、脚本沙箱或 S31 性能已经实现或验证。开工时本任务的 `REVIEW.md` 为空，未找到上一轮问题清单，因此没有可逐项销号的旧问题。

## 逐条验收

| 编号 | 复查结论 | 独立证据与核对方法 |
| --- | --- | --- |
| A1 | 通过 | 完整阅读设计第 0～9 节。一页结论回答控件是否统一、LVGL 复用边界、系统／生成／第三方应用的写法、输入与 AI；五项候选列出选择、代价和可逆性。见 `docs/system/app-framework-design.md:7`、`:403`；Python 核对章节序号、候选数量及 JSON 示例。 |
| A2 | 通过 | 逐项检查 24 组来源的链接、日期和版本／分支快照声明；关键数值、许可与平台边界回查官方材料。比较表没有把桌面或 ARM 示例冒充 S31 数据；同配置缺失明确写“没查到”。资源预估写明假设，25 KiB 页面模型和 406.125 KiB 全屏图的算式重算一致。见设计 `:124`、`:140`、`:388`，调研 `docs/research/app-framework-survey.md:13`、`:40`。 |
| A3 | 通过 | 对照规范、控件表和全部 177 张原图的九张缩略总览，另放大查看下列 12 张原图；未发现影响验收的控件遗漏。逐项表包含档位器、保存回执、分步状态、二维码、录音、选区与候选区。独立重算文件名集合和内容摘要，与调研一致。见设计 `:45`、`:63`、`:112`，调研 `:57`。 |
| A4 | 通过 | 对照规范 §4、§7、§8、§10、§12 与设计 `:179`～`:268`，核对字段规则、输入流程及待补稿入口。覆盖网络／PIN、搜索／问句、备忘／短信、电话／DTMF、日期时间、文件夹名、远程字段；显示码与输入码分开。键盘四种避让、长表单、下一项／完成／发送、返回、实体键、来电、草稿恢复、换人、密码和 AI 代填都有合同。脚本的 46 个重点页面检查仅作辅助，没有拿名称命中代替人工核对。 |
| A5 | 通过 | `git diff --name-status 2163603..HEAD` 仅新增 TASK、设计、调研和 REPORT 四份授权文档；原始 brief 与 TASK 逐字相同。产品代码、测试、依赖、需求、想法、决定和 backlog 均未改。提交时间从 21:17 至 22:09，跨约 52 分钟，与报告约 55 分钟相符；不是对开发全过程的独立计时。两轮构建及全量测试由复查者重新执行，结果见下。 |

图像核对放大查看：`Components`、`Foundation`、`ImePinyin`、`ImeEdit`、`ImeLong`、`ImeVoice`、`SetNetwork`、`RemoteType`、`BootWifiPass`、`SmsCompose`、`Search`、`FileMove`。九张总览覆盖其余原图，未把“文件名在清单里”当成已经看图。本任务没有新增页面实现，`out/ui-compare/` 不存在；上述结论是设计覆盖核对，不是实现与设计稿的像素验收。

【实测】177 个 PNG 的清单摘要为 `9cc976825afb7c5f09f57ef433af509ea21604827c1c899cee9e884f6c80ecf1`。算法：按文件名排序，每行拼接 `文件名 + 空格 + 文件内容 SHA-256 + 换行`，再对整份清单求 SHA-256；文件名集合、数量与摘要均一致。规范写 176、实际为 177 的差异已在原报告披露，没有删图或修改规范掩盖。

## 严重和高问题深挖

未发现达到严重或高级别的问题，问题清单为空。重点核查如下：

- **是否把未实现写成完成**：设计 `:23`～`:39` 区分基线代码、只读 UI 副本与缺项；接口草图 `:334` 明示不是可发送的 A2UI 报文或已实现 API；`:413`～`:423` 将运行时、失控反例和真机测量留给后续。抽查 `textfield.c`、`text_caps.c`、`textinput.h`、`cap.c` 及 UI 副本主题组件，现状与这些声明相符。没有因现有 91 项测试通过就宣称新框架通过。
- **丢字、换人和迟到回调**：设计 `:251`～`:258`、`:288`、`:358`～`:375` 明确普通草稿按旧成员保存、秘密清空、撤销旧会话、页面／对象／会话版本校验以及保存失败的真实状态。下一项与发送分开，远程输入不以坐标作为提交目标；`:419` 将换人旧回调、断网、来电和保存失败列为必须验证的反例。未发现设计允许旧成员内容进入新成员会话的路径。
- **AI 越权或代替本人确认**：设计 `:274`～`:290` 要求同源语义模型和能力路径；身份与动作来源由可信会话注入；秘密不导出值、真实长度或可回读引用；生成页不能伪造系统确认或扩大权限。最终确认只收本人设备输入。`ConfirmSecret` 与既有规范的矛盾已在 `:264`、`:408` 单列，未擅自把建议改成用户决定。
- **脚本失控、模块和资源边界**：设计 `:145`～`:156`、`:284`～`:290`、`:330`、`:395`～`:423` 明确限制宿主接口、执行中断、内存配额、UI 任务访问与失败清理；不开放第三方原生模块，不增加每应用常驻任务，不提高现有预算。R-061 的能力、状态、设置、事件、内部机制及 AI 范围在 `:296`～`:304` 分开列明。这里只接受设计与实测计划，未认定任何 VM 已形成可靠沙箱。
- **来源、许可和无依据数字**：复核 [LVGL 9.5 XML 调整公告](https://lvgl.io/blog/announcement-lv_xml-removal-from-v9-5)、[Moddable 许可说明](https://github.com/Moddable-OpenSource/moddable/blob/public/licenses/readme.md)、[QuickJS 手册](https://bellard.org/quickjs/quickjs.html)、[Berry README](https://github.com/berry-lang/berry/blob/master/README.md)、[MicroQuickJS README](https://github.com/bellard/mquickjs/blob/main/README.md)，相关许可和上游示例数字与调研一致（本轮查阅 2026-11-05）。没有引入 GPL 代码、SDK 或新运行时；可变分支明确不是已锁定的依赖。Android、A2UI、WAMR 和小设备参考也未被写成本机移植成果。
- **是否靠改测试过关／范围越界**：基线以来没有产品代码或测试差异，没有本任务新增的 sleep、跳过、预算放宽或替换断言。没有修复代码，故不要求伪造“撤掉修复”的反向实验；后续原型的反向对照已列在设计 `:421`。

## 独立执行记录

下列构建和全量测试顺序执行，没有并发两轮。复用已配置目录，检查了 `INK_WERROR=ON`、字体／LVGL／Python 路径和内存版的实际编译、链接参数；内存版启用 `-fsanitize=address,undefined`。测试使用原报告的独立 Python 环境，并设置 `PYTHONPATH`。

| 命令／检查 | 结果 |
| --- | --- |
| `cmake --build build -j4` | 退出 0 |
| `PYTHONPATH="$PWD/hub" "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3` | 退出 0；91/91，无失败或跳过；38.74 秒 |
| `cmake --build build-asan -j4` | 退出 0 |
| `PYTHONPATH="$PWD/hub" "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3` | 退出 0；91/91，无失败或跳过；66.39 秒 |
| `git diff --exit-code 2163603..HEAD -- system apps tests CMakeLists.txt hub platform simulator ports drivers boards` | 退出 0，无差异 |
| `git diff --check 2163603..HEAD` | 退出 0 |
| 对 `git diff 2163603..HEAD` 按总规则的本机路径和 MAC 正则扫描 | 无命中；退出 1 表示未匹配 |
| `python3 work/appfw-research/check_docs.py` | 退出 1：前面的内容检查全部通过；最后范围断言因本次复查的 `REVIEW.md` 失败，说明见下 |
| 另用 Python 独立核对 TASK 原文、授权文件集合、章节／候选、JSON、177 图集合／摘要及估算算式 | 全部通过，退出 0 |

复现说明：

1. `check_docs.py:49` 把所有未跟踪文件也要求严格等于开发的四文件集合，因此新增本复查报告后必然失败。未修改脚本、删除或隐藏复查文件；独立确认开发差异仍是四个授权文件，额外未跟踪项只有本报告。这是复查环境与脚本范围断言的冲突，不记为开发验收造假。
2. 普通轮的 `gui-smoke` 首次未观测到翻页，既有脚本自动第二次通过；内存轮首次通过。证据在 `build/Testing/Temporary/LastTest.log`，机制在 `tests/simulator/gui_smoke.cmake:12`。该脚本与基线相同，本任务未加重试；不能把普通轮描述成所有子尝试一次成功。两轮都使用脚本第 10 行的 `SDL_VIDEODRIVER=dummy`，未启动带窗口的模拟器。
3. 两轮测试日志位于各构建目录的 `Testing/Temporary/LastTest.log`；构建和测试更新了已有忽略目录内的产物，没有把它们加入版本控制。

## 验证边界

- 本机实测是 macOS／clang，未运行 Linux／gcc；本任务没有新增 `write`、`read`、`fgets` 或其他 C 调用，不能将本机通过写成 Linux 已测。
- 未测 S31 运行时、实际可用内存、光学刷新、触控和功耗；这些不足已明确列为后续准入验证，不影响纯调研设计交付，但不能据此批准运行时集成或设备发布。
- Apple 部分资料只能核到官方检索摘要；决策板集合无法读取。原文披露了这两项边界，本轮也未把建议当成新增用户选择。
- 本轮只写本地 `docs/tasks/appfw-research/REVIEW.md`，未修改其他受版本管理的文件，未提交、推送、碰设备或调用付费接口。

SEVERE=0 HIGH=0
