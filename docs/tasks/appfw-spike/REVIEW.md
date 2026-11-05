# 应用框架实测复查

复查日期：2026-11-05～06（Asia/Singapore）。基线 `39a349d`，被审提交 `6f7bd3baab2dd33cdf410d2a8533ed2a31d54475`。依据原始 brief、`docs/tasks/review-prompt.md`、`docs/tasks/codex-rules.md` 和设计文档独立核对。

**结论：B1～B4 在本次单页测量原型范围内通过；未发现严重或高问题。** 另记一项已披露的低级别外观差异。S31 Flash／静态 RAM、真机性能、完整脚本沙箱和 Linux／gcc 未获本轮实测结论，不能据此批准产品运行时或最终界面。

开工时本任务的 `REVIEW.md` 为空，无上一轮问题可逐项销号。开发报告里的编辑完成、计时结算和开关外观三项自查修复，已另行复核。本轮只修改本报告，没有修改仓库源码、测试和其他受版本管理的文件，没有提交。构建和全量测试顺序执行，分别至多 `-j4`、`-j3`；没有启动窗口或接触设备。

## 逐条验收

| 编号 | 复查结论 | 独立证据与命令 |
| --- | --- | --- |
| B1 | 通过，限固定单页 | `INK_SPIKE_OUT=work/appfw-spike/review-evidence bash tools/appfw-spike/run.sh all` 退出 0。C、JSON、Lua 各完成两轮确定性回放和五轮计时，每轮 20 条主检查通过。独立打开三张新截图，均有页头、八个开关、备注框和完成按钮；与提交的三张 PNG 逐像素一致。`harness.c:46` 检查真实触摸和 AI 进入同一能力、拒绝旧版本及非法参数；`:61` 检查文字输入；`:67` 检查按钮和释放；`bridge.c:144` 提供同源快照。 |
| B2 | 通过，允许的未测项已明列 | 普通及 ASan 新汇总逐项比较提交样本：确定性内存、输入行数／字符数／哈希、快照、截图哈希、源码清单和第三方行数全部一致。时间按原方法重跑，波动见下表。`export.sh` 独立检查退出 1，确实缺少 ESP-IDF v6.1 所需 Python 环境；没有目标 ELF，不把宿主数字写成 S31 数字。设计第 3.4、6.5 节已回填证据，未改原有准入条件。 |
| B3 | 通过，本机已复跑 | 普通全量 91/91，ASan／UBSan 全量 91/91；两轮都没有失败或跳过。原型 ASan 模式另完成同样的 21 轮验证。`git diff --exit-code 39a349d..HEAD -- system apps tests CMakeLists.txt hub platform simulator ports drivers boards` 退出 0；默认构建未注入实验目标，原型三个目标均为 `EXCLUDE_FROM_ALL`。 |
| B4 | 通过，可核实部分符合 | 三个提交从 22:52:51 到 23:52:26（+08:00），跨度 59 分 35 秒，支持报告“约一小时”的估算；这不是对开发全过程的独立计时。真机独有问题列于 `docs/research/app-framework-spike.md:35`，静态大小可仅编译获得也已分清。本次只构建、运行合成场景和只读环境检查。 |

【实测】本次普通模式的五次计时中位数，单位微秒；不是 S31 速度：

| 入口 | 初始化 | 建页 | 开关回调 |
| --- | ---: | ---: | ---: |
| C | 1 | 1048 | 160 |
| JSON | 1 | 1091 | 159 |
| Lua | 27 | 1174 | 161 |

原报告分别为 `1/1075/159`、`1/1053/160`、`20/1062/161`。宿主线程 CPU 测量不要求每次微秒数相同；方法、样本数及范围可复现。确定性轮的整轮堆峰值仍为 C `110416/2855456`、JSON `110416/2855464`、Lua `110416/2867120` 字节（内部／PSRAM），与提交记录完全相同。已核查 `vtk_sched_point` 的 CPU 结算及 `--cpu measured:1` 停用周期标注的实现，文档没有把宿主倍率 1 伪装成目标校准。

## 自查修复和反向对照

未直接运行会改仓库源码的 `negative.py`。复用原构建的编译参数和静态库，只在系统临时目录生成变体源文件、对象和可执行文件；每次只编译、运行一个变体，调用未修改的 `measure.check`。原源码前后 SHA-256 相同，正向运行仍通过。

| 项目 | 独立结果 | 结论 |
| --- | --- | --- |
| Lua 配额，`candidate_lua.c:18` | 只移除配额判断，编译／链接／程序退出均为 0，但原验证器以 `quota_rejected` 拒绝；该探针变为 0。原代码通过。 | 配额反例实际能检出缺失的守卫，并非固定成功值。 |
| CPU 结算，`bridge.c:19` | 只移除 `vtk_sched_point()`，初始化为 0 微秒，原验证器以 `unsettled_cpu_clock` 拒绝。原代码正向对照为 34 微秒，通过。 | 计时修复有效；34 是对照轮读数，不混入正式五次样本。 |
| 编辑完成回调，`bridge.c:81` | 临时移除 `ink_text_blur(0)`，程序产生 3 条失败，原验证器拒绝；原代码 20 条主检查通过。 | 与开发报告描述吻合，当前回调修复到位。 |
| 开关边框，`bridge.c:102` | 独立打开重生成的三张图并比较提交图；黑白状态可辨，未再见报告所说的边框缺口。 | 当前截图修复可见；外观尚未达到产品规范，见 V1。 |

反向对照的编译参数取自 `build-spike/CMakeFiles/appfw-lua.dir/flags.make`，链接参数取自同目录 `link.txt`；只替换对应的临时对象和输出二进制路径。运行参数与 `measure.py:44` 相同，配额／编辑变体用 `--cpu annotated`，计时变体用 `--cpu measured:1`。结果在系统临时目录 `appfw-review-controls-*/summary.json` 和各变体的 `run/` 中。复现时按上表逐项替换临时副本，不要直接运行修改仓库源码的脚本。

## 严重和高问题检查

没有形成严重或高级别的新问题，重点核对如下：

- **测量是否冒充产品完成**：`REPORT.md:37`、`:43`～`:45` 明列 S31、MicroQuickJS、A2UI、完整沙箱、长列表／长文、GC 和输入避让缺口，没有宣称 SP-08 已通过。字符数是源码载荷与紧凑快照，不含提示词、manifest 或 token；共同桥／测试代码和第三方源码另列，口径可重算。
- **软总线与 AI 原生**：`bridge.c:47` 登记 `spike.set/submit`；C／JSON 走 `ink_spike_dispatch`，Lua 的受限 `set` 也走它，触摸与 AI 最终到同一个 handler。快照读取同一行模型和文本框当前值；LVGL 创建、回调、AI 检查和销毁均在 UI 执行上下文。测试驱动与测量桥留在独立工具，没有把模拟器头文件带进产品层。此次不认定已经补齐字段定址、安装包权限或通用应用生命周期。
- **隔离与确认**：示例只有普通合成备注及本地开关，不含密码、身份变更或外发行为，没有增加代替本人确认的接口。沿用产品能力表的页面／成员策略，旧页面版本反例本轮通过。此单页没有覆盖换人竞态，不能用它替代完整成员隔离验收。
- **资源与失控**：模型、源码暂存和 Lua 分配均走 OSAL 外部内存，Lua 保留 128 KiB 配额、20000 指令 hook，未打开标准库；循环中断和模块隐藏探针通过。关闭时模型与 VM 标签无存活分配。整轮峰值包括原系统、输入服务和测试任务，各标签峰值不相加冒充同时峰值；没有提高已有预算。
- **测试可信性**：普通、内存版共 42 轮原型验证全部实际执行；三项临时反向对照都被原验证器检出。没有修改既有断言、跳过测试或新增 sleep。原型沿用的 `WAIT_FOR` 是有条件、有期限的虚拟时间等待。既有 `gui-smoke` 有一次重试机制，本轮两次全量运行均首次通过。
- **总规则第 1、4 节**：差异只含任务原型、证据、新调研和设计第 3／6 节；旧研究资料、需求、决定、依赖版本和产品代码未改。Lua 归档与解压源码校验通过，LVGL 提交和许可文件与记录一致，字体哈希一致；没有提交第三方运行时源码。最大新增文件 74212 字节，低于 5 MB。隐私正则扫描零命中。新增 C 没有 `write/read/fgets`；`harness.c:30` 的 `fread` 检查上限和 `ferror`，并检查关闭结果，输出也检查 `fprintf/fclose`。

## 图像核对与低级别观察

`out/ui-compare/` 不存在，本任务只交付三种入口的同一张实验页，不能声称完成了十屏产品实现对照。本轮实际打开三张新生成页面，并查看 `$INK_DESIGN/png/` 下十张完整界面：`SetNetwork`、`SetVoice`、`TodayEdit`、`ImePinyin`、`ImeEdit`、`RemoteType`、`SetDisplay`、`RemoteAddress`、`SmsCompose`、`BootWifiPass`；另看 `Foundation`、`Components`、`TopBar`、`Toggle` 四张组件／规范图。没有把参考图当成本任务实现的截图。

**V1｜低｜示例页仍使用占位外观，与最终规范存在明显差异（已在原报告披露）。**

- 文件和行号：`tools/appfw-spike/bridge.c:94`、`:99`、`:104`、`:123`、`:129`；规范 `docs/product/system-ui-spec.md:58` 起，原披露 `REPORT.md:45`。
- 复现命令：执行上面的 B1 命令，打开 `work/appfw-spike/review-evidence/{c,json,lua}-annotated-1/snap-page.png`，对照上述设计图。
- 结果：标题和列表同为 32px，缺少设计稿中 44px 标题及返回区形成的层级；输入文字贴近框左上角，留白不足；底部主按钮为小圆角描边矩形，没有设计稿的胶囊形和主要动作强调。页边距 32px、底部按钮 104px 高符合对应规格，八行文字清楚，无错字或截断。开关实际可见图形与热区的最终规格尚未验收，不能仅靠三个入口像素相同就声称符合设计。
- 修法：进入正式 UI 接入任务时复用已定主题、标题栏、文本框和按钮组件，再生成实现／设计对照图；本次不修改基线 UI，也不把这项已披露限制升为 B1 的测量原型阻断。

## 独立执行记录

以下命令均在用户提供的 `INK_DEPS`、`INK_DESIGN` 环境下运行。普通与内存构建复用已有配置；已检查 `INK_WERROR=ON`、字体、依赖路径，以及 ASan 版（包括 Lua）的实际 `-fsanitize=address,undefined` 编译／链接参数。两轮全量使用相同的已有 Python 3.11 依赖环境，并设置 `PYTHONPATH`。

| 命令 | 结果 |
| --- | --- |
| `INK_SPIKE_OUT=work/appfw-spike/review-evidence bash tools/appfw-spike/run.sh all` | 退出 0；3 候选 × 7 轮，全部通过 |
| `cmake --build build -j4` | 退出 0 |
| `PYTHONPATH="$PWD/hub" "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3` | 退出 0；91/91，32.59 秒 |
| `cmake --build build-asan -j4` | 退出 0 |
| `PYTHONPATH="$PWD/hub" "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3` | 退出 0；91/91，65.29 秒 |
| `INK_SPIKE_BUILD=build-spike-asan INK_SPIKE_SANITIZE=ON INK_SPIKE_OUT=work/appfw-spike/review-evidence-asan bash tools/appfw-spike/run.sh all` | 退出 0；3 候选 × 7 轮，全部通过 |
| `bash -c 'source "$HOME/esp/esp-idf/export.sh" && command -v idf.py && idf.py --version'` | 退出 1；缺少所需 Python 虚拟环境；没有安装工具或烧录 |
| `git diff --check 39a349d..HEAD` | 退出 0 |
| 对 `git diff 39a349d..HEAD` 执行总规则中的隐私正则扫描 | 无命中；搜索退出 1 表示未匹配 |
| Python 核对 TASK 与原始 brief、全部新增 Markdown 本地链接、样本静态字段、截图像素和文件大小 | 全部通过，退出 0 |

新原型证据在忽略目录 `work/appfw-spike/review-evidence/`、`review-evidence-asan/`；全量日志在各构建目录的 `Testing/Temporary/LastTest.log`。反向对照证据在系统临时目录 `appfw-review-controls-*` 下。构建和测试只更新忽略目录或临时产物；工作树只多出本 `REVIEW.md`，没有提交、推送或外部写入。

## 验证边界

- 本机为 macOS／clang，未执行 Linux／gcc 构建；检查返回值和本机 `-Werror` 通过不能替代 Linux 实跑。
- S31 Flash／静态 RAM 因环境不完整而未测，未安装全局工具补环境；任务书允许据实记录未测原因。
- 本轮的有限反例不等于 Lua 完整沙箱安全验收，也不覆盖成员切换、重入、最坏 GC、长文／长列表、输入避让或实机光学和功耗。
- 已记录的外观差异只影响最终界面验收，本次三种入口使用同一负载的资源比较仍成立。

SEVERE=0 HIGH=0
