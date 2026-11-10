# small-gaps-1 执行报告

基线：`4c64ae16d383ca833084d8f3683478becb9754d8`。功能提交：`0fd7658c`、`0fff5ac9`。状态：四项功能验收通过，语音完整验收未通过，按三次规则收尾。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 语音插话开关 | 【实现】已接入；【实测】未通过 | 关闭检查通过，但完整语音用例仍失败；按三次规则停止该项，保留断言 | `python tests/small_gaps/run.py --build build --case voice` |
| 1b 设备详情与快照隐私 | 【实测】通过 | HAL 型号、版本、8 GB 总量、4 GB 剩余、73% 电量；合成识别号不进入两种快照 | `python tests/small_gaps/run.py --build build --case pages` |
| 1c 开源许可列表与全文 | 【实测】通过 | 15 项；触摸打开；实体键与 AI 翻页；逐字重建全文，最大字号逐项检查 | 同上 |
| 1d 通话三分钟与局部刷新 | 【实测】通过 | 模拟累计 180 秒，注入分钟通知，0→1→2→3；同分钟重复通知不改内容版本、页面和控件保留 | `python tests/small_gaps/run.py --build build --case phone` |
| 1e 电量页切换家人 | 【实测】通过 | 低电量页保持打开，大人→孩子→大人，省电控件及快照对应身份 | pages 用例 |
| 2a 原有功能测试、全量、内存 | 【实测】普通与 ASan/UBSan 均 323/325，未全部通过 | 两轮均无跳过；仅语音完整验收与共享能力目录失败，原有语音、设置、电话、电量功能测试通过；无内存诊断 | 见验证命令 |
| 2b S31 编译 | 【实测】通过 | `build-s31/size.txt`、`size-components.txt`、`stack.txt`；仅编译和链接 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2c gcccheck | 【实测】通过 | `gcccheck: 9 compiled, 0 with findings` | `taskpolicy -c utility python3 ../../tools/gcccheck.py build`，后接本任务改动的 9 个 C 文件 |
| 3 中英文截图 | 【实测】已提供 | 10 张截图，10 个本地链接均有效；`INK_KEEP_EVIDENCE=1` 可重建并保留 | `python tests/small_gaps/run.py --build build` |

## 没做到的

语音完整验收未通过。测试三次调整后仍失败，已遵照总规则 §7 停止调整；没有删除、跳过或放宽断言。最后一轮的等待要求回答播放期间 `ink_storage_pending()==0`，但音频会阻止 Flash 写入（D-36），该条件不适合证明插话可用；因此不能把失败全部归因于产品，也不能报告语音验收通过。失败快照、指标和警告保留在构建目录。

普通及内存全量均已执行，未达到全部通过。S31 已编过，未接触设备。七项反向对照完成；语音的反向失败不替代其尚未通过的正向验收。

## 和设计不一致的

【实现】HAL 设备信息没有 MAC／序列号字段，本页不增设识别号读取路径。设备详情显示存储卡总量与剩余，复用 `file.free` 和 `storage.free`。

## 改过的已有测试

`tests/integration/ai_surface.c` 只在能力契约表新增三项：`shell.device`、`shell.licenses` 无参数，`shell.license` 必须提供 `index`。理由：本任务新增三个可由 AI 调用的页面能力；原有断言、预算及条目保留。

## 要用户定的

无。

## 顺手发现

【实现】现有 `time.minute` 按墙钟分钟边界发布，通话用单调时间计算已满分钟；中途接通时，显示更新可能等到下一个墙钟分钟。测试明确注入分钟通知，核对累计 180 秒和局部刷新；未为此新增定时器。

## 新增的机制

- 设置 `voice.interrupt`，任务第 1 项明确要求；默认开，使用现有设置保存和能力。
- 构建期许可清单及只读头文件，任务第 3 项要求列表与原文；使用现有分页控件，无运行期持久格式。
- 无新增任务、线程、状态机、重试、队列或缓存。

## 要改的文档

合并后更新共享能力清单的 `shell.device`、`shell.licenses`、`shell.license`、`voice.interrupt`；目录统计由 413 能力/事件、28 设置变为 416、29。`capability-table-doc` 因此检查失败，遵照总规则 §7 不改生成的共享文档。界面依赖中四个已验收缺口可更新，语音仍须标记未验收。

## 过程记录

- 主构建成功；`settings-a`、`settings-b`、`integration-listening`、`ai-speech-long`、`phone-calls` 五项原有测试全部通过，原话：`100% tests passed out of 5`。
- 最终 pages 用例中英文分别 382、383 条断言通过；phone 分别 38、39 条通过。语音为 19 条通过、6 条失败。新增用例内部 RAM 最小空闲：pages/phone 50304 字节，voice 33240 字节；预算未调整。
- 首次许可翻页发现只读行没有 action id，已按现有 F4 合同补齐；随后 pages 全部通过。
- 语音测试首轮未观察到完整播放；后改用实体按键并等实际请求、输入驱动和前一帧，完整用例仍失败，按三次规则停止。
- 首次全量原话：`99% tests passed, 4 tests failed out of 325`，另有三个 hub 用例因默认 Python 缺依赖自动跳过。已改用只读依赖目录中现有的 hub Python，不安装依赖；复跑 hub 三项及布局、AI 契约（含自动附带的 caps fixture），原话为 `100% tests passed out of 6`。
- 截图检查发现英文许可标题换行后被内容覆盖；已缩为 `Licenses`。全文页统一使用短标题，正文首屏保留组件名称、版本、许可和来源。
- 最终普通全量原话：`99% tests passed, 2 tests failed out of 325`，耗时 1009.53 秒，无跳过；失败为 `small-gaps-1`、`capability-table-doc`。日志：`work/ctest-final.log`。
- ASan/UBSan 全量原话：`99% tests passed, 2 tests failed out of 325`，耗时 868.59 秒，无跳过；同样两项失败。覆盖全部 325 项，没有 `AddressSanitizer`、`UndefinedBehaviorSanitizer` 或 `runtime error:` 诊断。日志：`work/ctest-asan.log`。
- 反向对照 7/7 触发指定失败：AI 键开关、语音检测开关、设置开关绑定、型号来源、许可翻页、分钟刷新、换家人订阅。每项编译/链接退出 0、用例退出 1，源文件哈希前后不变。证据：`build/small-gaps-reverse/summary.json`、`work/reverse.log`。语音正向仍失败，不能据此认定语音完成。

## 规模与架构

产品代码 **204 行**：C 增删 171 行，许可生成脚本 25 行，构建接线 8 行；按增删合计，低于 250。测试、文档、许可原文、翻译和清单数据另计。没有提高资源预算。

超过 100 行的原因是设备详情、许可列表、完整原文分页及三项页面能力要一起接入。评估过仅列名称与许可摘要，但不满足全文验收；最终复用现有设置行、F1 快照和 F4 分页。页面通过 `settings.set`、`file.free` 和软总线能力操作，未引入新服务层。设备状态来自 HAL/服务，按钮和 AI 使用相同导航能力。

## 截图

最新全量运行生成在 `build/test-out/small-gaps-product/`，下表为相对此报告的路径。未提交构建产物。

| 页面 | 中文 | 英文 |
| --- | --- | --- |
| 设备详情 | [device](../../../build/test-out/small-gaps-product/pages-zh/snap-device.png) | [device](../../../build/test-out/small-gaps-product/pages-en/snap-device.png) |
| 许可列表 | [licenses](../../../build/test-out/small-gaps-product/pages-zh/snap-licenses.png) | [licenses](../../../build/test-out/small-gaps-product/pages-en/snap-licenses.png) |
| 许可全文 | [text](../../../build/test-out/small-gaps-product/pages-zh/snap-license-text.png) | [text](../../../build/test-out/small-gaps-product/pages-en/snap-license-text.png) |
| 语音开关 | [voice](../../../build/test-out/small-gaps-product/pages-zh/snap-voice.png) | [voice](../../../build/test-out/small-gaps-product/pages-en/snap-voice.png) |
| 通话分钟 | [phone](../../../build/test-out/small-gaps-product/phone-zh/snap-call-3-minutes.png) | [phone](../../../build/test-out/small-gaps-product/phone-en/snap-call-3-minutes.png) |

英文模式中新增的开关、详情、许可与时长文案已翻译；既有电话控件、电量和语音说明文案沿用基线，未宣称全系统国际化完成。

## 验证命令

沿用任务给定的 `INK_DEPS`，主机配置按总规则 §5；LVGL、SDL2、ImGui 源码均指向其 `build/_deps/`，`INK_HUB_PYTHON` 指向其 `hub/.venv/bin/python`。不安装依赖。普通及 ASan 两个目录配置一致，后者增加 `INK_SANITIZE=ON`。所有构建 `taskpolicy -c utility`、`-j2`；所有 ctest `-j2`，一次仅跑一个构建或一轮全量。

```sh
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility python3 tests/small_gaps/reverse.py --build build
taskpolicy -c utility python3 ../../tools/gcccheck.py build system/apps/phone/phone.c system/services/ai/ai_service.c system/services/audio/audio_service.c system/ui/shell/power_pages.c system/ui/shell/settings_pages.c tests/small_gaps/{pages,phone,voice}.c tests/integration/ai_surface.c
```

## 提交检查

`git diff --stat` 仅含本任务代码、测试、许可资料和本报告；共享需求、决策、能力文档及历史资料未改。隐私扫描仅两处命中，均为任务指定的合成 MAC 测试数据；无本机绝对路径或真实识别号。报告的本地链接 10/10 有效，`git diff --check` 无问题。构建产物、日志、截图、设计稿均未提交，未推送。
