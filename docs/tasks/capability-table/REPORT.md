# capability-table 执行报告

原开发基线：`1c43c28a`；本轮修复基线：`7c393cde`（2026-11-06）。范围：`capability-table` 及独立复查 R1。复查列出严重 0、高 1，没有中级条目。本轮 R1 定向验证已通过，完整普通与内存检查待执行；原 C2、C3 的策略语义结论已按反例修正。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1 成员范围、撤销确认、设置儿童权限 | 已修，定向通过【实测／实现】 | 修正所有同类成员文字和设置撤销说明；14 项设置均导出、严格比较三个权限布尔字段；真实调用及撤回修复的对照通过 | V1、V2 |
| C1 代码生成能力总表 | 已实现【实现】 | 115 个调用、27 个事件、14 个设置、42 个状态；关联面有源码链接 | `python3 tools/capabilities.py --check` |
| C2 表与代码一致，每个能力有描述和风险 | 定向通过【实测】 | 13 项 Python 回归；142 项运行时元数据、3976 个外层确认组合；另核对实际调用及完整撤销路径，不再把外层闸门当成整个调用 | V1 |
| C3 每个设置项都有能力 | 定向通过【实测／实现】 | 14 项均映射 settings.get/set；逐项核对 child_may_change、system、ask_by_hand；儿童可写、不可写及权限撤销结果符合总表 | V1 |
| C4 用户操作缺口及小修 | 核查完成【实现】 | 原缺口表保留；本轮未新增产品入口 | 下文缺口表 |
| G1 全量测试及内存检查 | 待本轮执行 | 不引用上轮的 100/100 作为本轮证据；预算、断言、时限未放宽 | V3 |
| G2 原子能力与 AI 共用入口 | 本轮无新增产品操作或页面【实现】 | 生成器、运行时元数据和真实总线行为分别核对；已有入口缺口仍保留 | C4；V1 |
| G3 依赖状态 | 保留“部分”并更新校验证据【实现】 | 只改对应状态单元格，保留原判断 | 文件差异 |
| G4 页面验收 | 不适用【实现】 | 未改页面，不涉及设计稿或视觉对照 | 文件差异 |
| G5 范围 | 符合【实现】 | 只改生成工具、测试、CTest 登记、总表、状态和报告；产品源码、设计合同未改 | 文件差异 |
| P1 差异与隐私检查 | 待最终提交核对 | 本轮定向空白检查通过 | V4 |

V1（定向实测，退出 0）：

```sh
python3 tests/capability_table/test_catalogue.py
python3 tools/capabilities.py --check --runtime build/test-out/capability-table/capability-inventory.json
python3 tests/capability_table/check_runtime.py build/test-out/capability-table/capability-inventory.json
python3 tests/capability_table/check_policy.py build/test-out/capability-policy/capability-policy.json
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3 -R '^capability-table-'
```

6/6 CTest，1.10 秒，无跳过；日志 `work/capability-r1-targeted.log`。真实总线验证：无匹配白名单的儿童改音量到 35、读设置、列通知成功；改孩子音量上限被拒绝且值不变；child_ok=false 的合成能力可由白名单额外放行，无白名单则拒绝；普通设置及闹钟撤销成功；AI 撤销已由人确认关闭的剪贴板权限时等待确认，权限仍关闭。8 个系统闸门验证人工儿童关机/重启放行、AI 拒绝，以及白名单不能放行 child_ok=false 的系统操作；没有执行关机/重启动作。

同类排查覆盖全部 115 个调用和 14 个设置：102 个非 system 且 child_ok=true 直接允许儿童，3 个非 system 且 child_ok=false 允许白名单额外放行，2 个 system 且 child_ok=true 仅人工儿童可调，其余 8 个 system 仅成人。成员列说明能力闸门，设置写入/撤销另注明设置项检查。运行时导出在测试程序中直接读注册定义，不修改产品权限或公开 JSON 接口。82 个负对照包含全部设置的三种权限标记翻转、漏掉儿童权限、源码儿童权限翻转，以及非布尔值；另有 4 个错误说明文字被真实行为对照拒绝。

V2（撤回修复的实测）：新增 3 项回归在修复前运行 `python3 tests/capability_table/test_catalogue.py`，13 项中出现 112 个失败子项，退出 1；修复后 13/13 通过。另将 `git show 7c393cde:tools/capabilities.py` 写到忽略的 `work/capabilities-before-r1.py`，用 importlib 临时作为 capabilities 加载，再用 runpy 执行上述两个 check 脚本：运行时检查因接受错误 child_may_change 失败，实际行为检查因 settings.get 的错误白名单文字失败。恢复当前模块后两个脚本均通过；未撤改工作区产品代码。日志 `work/capability-r1-before.log`、`work/capability-r1-reverse.log`。

V3：两套沿用总规则的完整配置参数和锁定中枢环境 `work/hub-venv`，保留 INK_WERROR=ON；内存套只另加 INK_SANITIZE=ON。构建最多 -j4，CTest 最多 -j3，构建与测试逐轮串行；不启动带窗口的模拟器。全量结果待填。

V4：最终差异、隐私、文件大小和本地链接检查待填。

## 没做到的

本节按任务允许列出以下遗留操作，不代表这些功能不需要或首版不做。未新增产品能力：给下列操作加名字不足以完成共用入口；必须连同按钮、页面/会话版本、儿童白名单、锁定和敏感输入一起改验。按 G5，本节保持服务/策略/测试范围，没有改界面或具体应用。

| 没有能力的已实现操作 | 【实现】证据 | 不能作为本节小改的原因 |
| --- | --- | --- |
| 通用返回上一页 | `system/ui/ui_task.c:644`，直接 `ink_ui_pop`；`system/apps/remote/remote.c:563` 同类返回 | 需统一应用、AI 面板及浮层对返回的消费顺序 |
| 电源键锁屏、唤醒 | `system/services/input/input_service.c:186`，直接 `ink_power_unlock/lock` | 物理唤醒、吞键及成员授权时序相关，不能把 AI 解锁等同人工唤醒 |
| AI 按住说、松开/再按结束、关闭面板、以文字提问 | `system/ui/ui_task.c:628,642`；`system/services/ai/ai_service.c:200,617,720` | 会话、麦克风和输入状态机与界面连在一起；需要保护原成员与轮次，不能嵌套开启 AI |
| 键盘逐键组合、退格、选候选、候选翻页、符号/大小写切换 | `system/ui/shell/keyboard.c:39,59,67` | `text.insert` 不等于拼音组合；须保留会话/版本约束及密码的人工输入边界 |
| 文本框复制、粘贴、按字符移动/扩展选区 | `system/ui/textfield.c:152,164,180` | 已有 text/clipboard 的组合，但没有相同操作的一次性原子入口；焦点、成员、敏感内容应一起绑定 |
| 电源菜单展开和取消 | `system/ui/shell/power_menu.c:26,50,60` | 属于浮层及按键仲裁；关机/重启按钮本身已走能力，不应只添加一个无法共用的接口 |
| 远程网页点击坐标 | `system/apps/remote/remote.c:424,446`，直接发 `INK_RT_INPUT` | 需绑定会话和画面版本，当前只有 remote.open；完整远程交互属于 backlog 的 remote-app |
| 当前占位主页的计数按钮及示例三页切换 | `apps/home/home.c:41,53,61,69` | 状态只在应用内部，需配套应用界面的操作与快照，未擅改具体应用 |

只在本机 macOS clang、模拟器及合成中枢验证；未执行 Linux gcc 或实机验证。生成器只解析当前显式初始化和单行初始化宏，无法解析时失败；运行时目录检查用于防止静态提取遗漏。关联面按提供方目录和源码引用汇集，未声称是逐操作的数据流合同。

## 和设计不一致的

没有改设计合同迁就代码。R-061 的全入口共享在基线尚不成立：已有 `app.open`、`reader.open_book`、`system.home`，主页/书架点击和 HOME 仍直接调 UI/应用函数（`apps/home/home.c:48`、`apps/reader/reader.c:108`、`system/ui/ui_task.c:646`）；文本框撤销、键盘听写按钮、确认卡和成员门也有直接服务入口（`system/ui/textfield.c:174`、`system/ui/shell/keyboard.c:87`、`confirm_card.c:44,55`、`member_gate.c:55,66`）。这些与“没有能力”分开记录；仅 PIN/人工确认不能向 AI 开放的限制仍须保留。现有音量键、阅读翻页、电源菜单执行按钮和打断卡的动作转发已有能力路径。

## 改过的已有测试

未修改任何原有断言、期望、预算、时限或跳过规则。本轮只新增 3 项 Python 回归、扩展运行时负对照，并增加两项 CTest（真实策略执行导出及说明文字对照）；连同原四项共六项。新增策略测试有独立的完成计数检查，未完成不能复用旧导出冒充通过。产品服务、策略、界面和应用源码均未改。

## 要用户定的

无。

## 顺手发现

配置的中/粗字体变量仍未被原 CMake 使用，链接器仍提示重复库；与本节无关，未修。新增 C 导出全部文件操作都在 `vtk_host_call` 内执行，检查输出/关闭返回值；没有新增 `write/read/fgets` 调用。没有新增设备常驻任务或改资源预算。
