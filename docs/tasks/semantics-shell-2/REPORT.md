# semantics-shell-2 执行报告

基线：`e65bfc261b68e9fb063fc60da507122119ae4fc6`。状态：实现完成，最终验证中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| A1-1 键盘 | 专项通过【实测／实现】 | 同一 F1 声明导出候选、拼音、模式；密码与隐藏通话输入只有有无；旧点按、旧 AI 页版本拒绝；实际 3072 字节组合快照核查 | `ctest --test-dir build -R '^semantics-shell2-keyboard$' --output-on-failure` |
| A1-2 AI 面板 | 专项通过【实测／实现】 | 保留原 F1 字段／动作，更新对象版本；回答变化后旧收起点按拒绝 | 同上，测试名换 `semantics-shell2-ai` |
| A1-3 家人表单 | 专项通过【实测／实现】 | MemberSwitch、member_gate、PinSet、PinForgot（含家长输入）、PinWait；PIN 一位／三位快照相同，提交没有开放给 AI，旧表单按钮拒绝 | 同上，测试名换 `semantics-shell2-member` |
| A1-4 孩子页 | 专项通过【实测／实现】 | KidLimit／KidTimeUp；家长密码继续走本人表单；旧取消点按拒绝 | 同上，测试名换 `semantics-shell2-kid` |
| A1-5 电量页 | 专项通过【实测／实现】 | BatteryLow／BatterySaving／Charging／off；只导出真实动作，旧目标拒绝，无关成员事件不重画 | 同上，测试名换 `semantics-shell2-battery` |
| A1-6 主页 | 专项通过【实测／实现】 | Main／Apps／Today 共用现有声明与语义导出；旧主页点按和应用图标拒绝 | 同上，测试名换 `semantics-shell2-home` |
| A2-1 原测试、hub、普通版全量 | 通过【实测】 | `100% tests passed out of 353`；无跳过，400.93 秒，退出 0；已有测试和期望未改 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| A2-2 ASan／UBSan 全量 | 通过【实测】 | `100% tests passed out of 353`；无跳过，758.70 秒，退出 0 | `INK_KEEP_EVIDENCE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2` |
| A2-4 S31 | 待最终验证 | 正在执行 | 完整结果待补 |
| A2-3 gcccheck | 通过【实测】 | `gcccheck: 15 compiled, 0 with findings`，无跳过，退出 0 | `taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build <8 个产品 C 文件和 7 个新增测试 C 文件>` |
| A3 行数与预算 | 通过【实现】 | 产品代码净增 156 行；连 CMake 注册保守计 165 行，低于 250 行 | `git diff --numstat <基线> -- system apps` |

主机构建／测试均使用 `taskpolicy -c utility`，并行最多 `-j2`。日志保存在忽略目录 `work/semantics-shell-2/`，临时证据保存在 `build/test-out/semantics-shell2-*`。

| 文件 | 迁移前 | 迁移后 | 净增 |
| --- | ---: | ---: | ---: |
| `system/ui/shell/keyboard.c` | 478 | 528 | 50 |
| `system/ui/shell/ai_panel.c` | 241 | 260 | 19 |
| `system/ui/shell/member_pages.c` | 258 | 277 | 19 |
| `system/ui/shell/kid_pages.c` | 109 | 121 | 12 |
| `system/ui/shell/power_pages.c` | 174 | 182 | 8 |
| `apps/home/home.c` | 27 | 27 | 0 |
| `system/ui/shell/navigation.c`（实际主页） | 2233 | 2254 | 21 |
| `system/ui/shell/ai_chat.c` | 121 | 121 | 0 |
| `system/ui/shell/ai_chat.h` | 10 | 10 | 0 |
| `system/ui/ui_semantics.c` | 87 | 110 | 23 |
| `system/ui/ink/ui_shell.h` | 65 | 68 | 3 |
| `system/include/ink/sys/textinput.h` | 140 | 141 | 1 |

## 没做到的

- S31 编译尚在执行。
- 没有连接设备，也没有运行 Linux CI；不把本机检查写成实机或 Linux 全量结果。

## 和设计不一致的

- 【实现／实测】主页实际实现位于 `system/ui/shell/navigation.c`，`apps/home/home.c` 只是注册入口；任务书的 `system/apps/home` 不存在。修改实际导出处和它使用的既有绑定器，注册入口不需要改动。
- 【实现／实测】键盘和 Main／Apps 保留文本格式，由私有 `ink_ui_semantics_summary()` 从同一 F1 声明生成。直接嵌入完整 F1 JSON 会超过现有组合快照缓冲，且破坏原测试要求的 `Apps；` 前缀。保留 `page`／`keyboard`／`keyboard_ui` 的类型和结构，补充版本、对象元数据；不扩大 AI 缓冲。Today（含 KidToday 的兼容外壳）继续使用原 F1 JSON。其余迁移页面使用标准 F1 JSON。

## 改过的已有测试

无。只新增专项程序和六项 ctest 注册。专项中对紧凑导出器做观察钩子：核查生产文本，再用同一模型导出标准 F1 JSON，以检查字段、秘密和对象版本；另用原生产输出核查 3072 字节组合快照。原回归仍读取真实生产格式。

## 要用户定的

暂无；没有新增产品能力或改变本人确认边界。

## 顺手发现

- 【实现】电量页原先对所有 `member.` 事件重画，包括与电量控件无关的时间变化；本次仅保留影响成员身份和授权的变化。
- 【实现】家人页只核对成员／锁定状态，孩子页只在限制状态切换时重画；键盘只在键盘类型变化时重建，AI 音频订阅更新局部控件；主页已有事件过滤。专项检查了无关事件不重画。

## 新增的机制

- 私有 F1 文本导出函数：任务第 3、5 项要求保持旧文本和键盘组合契约，因此在已有语义导出器旁增加一处兼容格式化；没有新线程、状态机、持久格式、缓存、后台重试或配置项。
- 既有控件绑定器记录按下时的版本与浮层代数，AI 面板记录回答对象版本，候选展开使用独立布局对象版本；任务第 2 项要求拒绝过期点按。复用原输入服务的修订号，只补充公开声明，没有增加缓存。
- 新测试驱动任务只存在于测试程序，用于验收第 1 项。

## 要改的文档

合并时在 `docs/system/app-framework.md` 补充系统壳保留文本契约时的语义导出说明。共享文档和受保护资料未修改。

## 过程记录

- 开工：工作区干净，位于指定基线的独立任务分支；前置 semantics-shell-1 已合入。
- 第一轮相关回归：`focused-5.log`，原文 `100% tests passed out of 13`，耗时 15.96 秒，退出 0。
- 超过 100 行的原因：同时覆盖六组页面、秘密字段、原有动作及真实按下事件，且必须兼容原快照格式。更小的只替换文本方案不会拒绝旧点按；全部改成嵌套 F1 JSON 的方案已由旧回归证实会破坏组合快照和 Apps 前缀。
- 首轮全量：`ctest-full.log`，353 项中 9 项失败，483.19 秒，退出 8；依次定位旧文案、活动卡分隔符、共享绑定器误扩展到相机页、键盘借用浮层版本、数字输入误用底页版本，以及候选展开被无关入口变化打断。均已修正，未修改原测试。
- 两项遗留失败的对照：`existing-baseline.log`，同一构建中换回基线键盘、家人表单，原大字号候选与完整家人流程均退出 0。临时探针 `control-trace.log` 分别测到一个过期候选对象／页版本和一个过期 PIN 数字事件；探针只存在于忽略目录。
- 修复后专项：`focused-input.log`，新增六项及大字号候选、完整家人流程共 8／8，6.05 秒，退出 0。`focused-compat.log` 中原 hub 契约、组合快照、活动卡、扫码也已通过。
- 数字键保留原可信本地输入路径（成员 epoch、忙碌、等待保护和运行时触摸代数）；表单动作核对按下版本。将底页版本套到每个数字会吞掉真实输入，数字不是对 AI 开放的语义动作。候选展开使用独立布局对象版本，复用既有 F1 的动作对象引用。
- 六组反向对照：`reverse-final.log`。键盘、家人、孩子、电量、主页替换为指定基线源文件，仅在构建目录编译和链接；各测试程序退出 1，分别 13、10、3、7、8 个断言失败。AI 面板基线已使用 F1，因此该组只撤掉新增的过期点按保护，退出 1，2 个断言失败。六个验证脚本均退出 0；ctest 默认模式不依赖 Git 历史。
- 最终普通版：`ctest-final.log`，全部 353 项通过，400.93 秒，退出 0，没有跳过项；包括原 hub 契约、原控件／场景、确定性与资源预算检查、新增六项。最终 GCC 复核见 `gcccheck-final.log`。
- 内存版：与普通版相同配置，仅换目录 `build-asan`、增加 `-DINK_SANITIZE=ON`；保留 `-DINK_WERROR=ON`、字体、依赖和 hub Python。配置、完整构建均退出 0，实际编译参数含 `-fsanitize=address,undefined`。`ctest-asan.log` 原文 `100% tests passed out of 353`、`Total Test time (real) = 758.70 sec`，退出 0，无跳过；执行所有原测试和六项新增回归。
