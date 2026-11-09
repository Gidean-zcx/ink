# phone-2 执行报告

基线：`62b0e83da1d34f50629b5e576807db1381b26078`。日期：2026-11-10。执行结束，验收结论见下表。仅在自己的本地分支提交，不推送。【实测】产品代码增删合计 **213 行**（预算 300）。

以下命令先按任务环境设置 `INK_DEPS`、`INK_DESIGN`。构建最多 `-j2`，测试最多 `-j2`，均使用 `taskpolicy -c utility`。

最终全量测试另设 `PYTHONDONTWRITEBYTECODE=1`、`PYTHONPATH="$PWD/hub:$INK_DEPS/hub/.venv/lib/python3.11/site-packages"`，只读复用已有中枢依赖，避免三个中枢测试因缺依赖跳过。普通及内存构建参数均照任务总规则，后者只更换目录并加 `-DINK_SANITIZE=ON`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 静音、拨号音、路由、录音边界 | 模拟器通过；实机与 NT26 专用指令未核实 | 【实测】controls 阶段 98 条断言；上行 640、静音丢弃 160、喇叭 640、耳机 160 个注入样本；DTMF 为 `10*#2`；挂断清空，跨成员操作被拒；录音按钮及 AI 均返回接线未核实 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^phone-calls$'` |
| 2 AI 快照、画板对照 | 快照通过；画板分数超标 | 【实测】PhoneCall、PhoneKeypad 均进入真实 AI 上下文；PhoneCall 为 **20.972/255**，目标 ≤12；证据在 `build/test-out/phone-calls/` | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/test-out/phone-calls --scenes PhoneCall --reuse` |
| 3 全量与内存检查 | **未全通过** | 【实测】普通和 ASan/UBSan 构建均成功；两轮均为 **266/268 通过、2 失败、0 跳过**，退出 8。两轮电话专项各 98 条断言通过；内存日志无 Sanitizer 错误。失败均为 `capability-table-doc`（共享文档按规则待生成）和 `scenario-ai_tools-metalio-plus`（基线也丢样 90 ms） | `taskpolicy -c utility cmake --build build -j2`；`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存检查把目录换成 `build-asan`（加上述环境） |

【实测】最终日志：`work/test-final.log`（404.42 秒）、`work/test-asan.log`（647.74 秒）；Sanitizer 全部输出另见 `build-asan/Testing/Temporary/LastTest.log`。`git diff --check` 及报告本地链接检查通过。按任务隐私正则检查基线差异，命中 4 处，均为规则指定的合成耳机地址；本机路径、真实标识命中 0 处。变更范围为 17 个任务相关文件，无共享文档、设计稿或构建产物夹带。

【实测】反向对照：临时撤掉模型的 CMUT/VTS 支持、恢复通话中延后路由，并取消按钮回执回调；同一 controls 阶段退出 1，86 条通过、12 条失败，四类回归均被检出。恢复源码并重建后退出 0，98 条通过、0 失败。复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/phone_calls/reverse_controls.py`（会临时改三个产品文件并在 finally 恢复；勿与构建、测试并发运行）。产物在 `build/test-out/phone-negative/{negative,restored}/metrics.json`，构建日志在忽略目录 `work/`。

## 没做到的

- 未操作设备、未拨打真实电话。【实现】录音没有实现为文件：现有[音频接线结论](../../research/eink4-plus-cellular.md)及[模拟器硬件对照](../../research/simulator/hardware-parity.md)均未证明 NT26 双向通话音频经过 BH1098G/MCU。需要目标板原理图的连线、目标固件音频接口说明，以及能证明双向 PCM 可采集的实机证据；不能用本机麦克风录音冒充通话录音。
- 总规则第 7 节禁止读取 `ink-hw`，因此未读取该目录的 NT26 手册。新增命令暂按既有驱动所用的 TS 27.007 形式：`AT+CMUT=0/1`、`AT+CMUT?`、`AT+VTS="<键>"`。**尚未满足“用目标 NT26 手册核对”的要求**。公开[利尔达产品资料](https://opendocs.lierda.com/docs/CAT.1_Doc_Protal/zh_CN/index.html)也不能证明目标固件支持这几个命令（查阅：2026-11-10）。模型接受命令不等于硬件支持。
- 模拟通话样本注入假设声音经 BH1098G 模式 1/2 出去；计量仅证明控制逻辑，不证明真实板卡音频接线、声学质量或录音能力。
- 本次实测环境为 macOS/Apple clang；未执行 Linux/gcc 构建。

## 和设计不一致的

- [服务设计 §10.4](../../system/system-services-design.md)原定通话中延后切路由；任务明确要求通话中切换，本次在音频任务唯一的路由应用点取消这一限制，未动其他焦点规则。
- PhoneCall 与画板相差 20.972/255：真实测试显示合成号码和通用联系人图标，画板为“妈妈/妈”和“通话中 · 3 分钟”；头像位置、尺寸、静态底色、字号、挂断圆大小、状态栏图标及纸色不同。未用虚构姓名、时长或路由掩盖差异。键盘没有单独画板；截图已检查，无控件重叠。

## 改过的已有测试

- `tests/phone_calls/`：保留原有拨打、接听、历史及重启隔离的全部断言和期望，新增 controls、PhoneKeypad 两个阶段与音频计量断言。
- `tests/integration/ai_surface.c`：AI 能力清单增加五项新能力及各自必填参数，仍逐项核对真实工具接口，没有跳过或放宽断言。
- `tests/integration/media.c`：唯一改变的状态期望为“通话中连接耳机后仍是 speaker”→“通话中已是 bt_headset”，按本任务要求改变；结束通话后的期望仍是 bt_headset，只改说明文字。全部资源预算不变。

## 要用户定的

- 主会话需解决“禁止读取 ink-hw”与“按该目录 NT26 手册实现”的冲突，并补目标板音频证据。本次按保守、可逆方案继续：录音始终不打开麦克风、不生成文件。
- D-105：【实现】静音、路由、键盘导航无高风险副作用，不弹确认；普通 DTMF 按键走 `INK_FX_EXTERNAL`，只有可信中枢信封里的 `_meta.asked:true` 才可直接发送，否则确认，AI 不能自批。【实测】未授权 AI 按键等待确认且不发音；人确认后才发。本次未识别 IVR 的密码、付费用途，不能把单键测试视为这类高风险流程已获验收。`call.record` 当前只读不可用原因，无录音副作用，也无确认卡；真正支持录音后须重新按 D-105 审视开启录音的动作。

## 顺手发现

- 总规则第 7 节禁止读取 `ink-hw`，任务第 4、5 条要求查其中的资料；遵循第 7 节，先查仓库现有硬件资料，缺证据如实记录。
- 系统现有蓝牙设置行尚无连接页面；耳机未连时，本次按钮保持未连接状态，点击说明需先连接耳机，不虚构一个可用的蓝牙设置页。
- 【实测】全量首轮 268 项有 4 失败、3 跳过。`scenario-ai_tools-metalio-plus` 的麦克风丢样 90 ms 已做基线对照：临时将全部改动的产品文件换成指定基线，重建 `sim-configured` 后同一场景仍失败 90 ms；恢复后仍为 90 ms。未修改该场景、预算或相关 AI/音频采集逻辑。对照日志为 `work/test-ai-{baseline,restored}.log`，度量为 `work/ai-tools-{baseline,restored}-metrics.json`。

## 新增的机制

- 新任务、线程、状态机、持久格式、后台重试、缓存、队列、配置项：均无。
- 新增 `call.mute`、`call.dtmf`、`call.route`、`call.record`、`phone.keypad` 及确认结果事件 `call.controls`，由任务第 5 条要求；使用现有能力总线、后台执行器和界面状态订阅。
- 人工反向脚本 `tests/phone_calls/reverse_controls.py` 对应总规则第 3 条，不加入并发 CTest；原有历史隔离反向脚本保持原样。通话临时按键序列由任务第 2 条要求，挂断清空，不落盘。
- 超过 100 行的原因：四个动作跨应用、通信服务、HAL、NT26 驱动及硬件模型，另有键盘与两页 AI 快照。更小的直接从应用发 AT 命令或仅改按钮状态均违反分层/回执要求。HAL 与 S31 空端口只补必要声明和明确失败的占位，保持各构建目标可链接。

## 要改的文档

- 主会话统一重新生成 `docs/system/capabilities.md`，补上述能力及事件；本分支遵守第 7 节，不修改生成的共享文档。
- `docs/system/ui-dependencies.md`：通话页四按钮的现状、录音接线证据缺口和蓝牙设置入口缺口。
- `docs/system/system-services-design.md`：§10.4 通话中允许切换的变化及硬件证据边界；本分支不改合同迁就实现。
