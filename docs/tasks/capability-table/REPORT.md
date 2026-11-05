# capability-table 执行报告

基线：`1c43c28a`。范围：仅 `backlog.md` 的 `capability-table`。状态：本节交付完成，遗留操作缺口按任务要求列出。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| C1 代码生成能力总表 | 已实现【实现】 | 115 个调用、27 个事件、14 个设置、42 个状态；关联面有源码链接 | `python3 tools/capabilities.py` |
| C2 表与代码一致，每个能力有描述和风险 | 通过【实测】 | 缺描述/风险、重复 id、未知权限、过期文档均拒绝；完整运行时目录和 3976 种实际 C 确认策略组合一致 | V1；`python3 tests/capability_table/test_catalogue.py`（10 项） |
| C3 每个设置项都有能力 | 通过【实测／实现】 | 14 项均映射 settings.get/set，运行时设置目录一致；按设置项检查儿童与确认；删掉任一通用能力的负对照失败 | 同上；V1 |
| C4 用户操作缺口及小修 | 核查完成【实现】 | 无能力和已有能力但绕过总线两类缺口见下；涉及界面、输入仲裁或身份边界，未以简单包装冒充共享入口 | 源码逐项核查 |
| G1 全量测试及内存检查 | 通过【实测】 | 最终普通 100/100（40.77 秒）、ASan/UBSan 100/100（103.14 秒），均无跳过；预算未改 | V2 |
| G2 原子能力与 AI 共用入口 | 目录与覆盖守卫已完成【实现】 | 没有新增用户操作/页面；不宣称既有按钮路径全部共用能力 | C4 缺口表；`integration-ai-surface` |
| G3 依赖状态 | 已更新【实现】 | 只改“页面对 AI 可见”的状态单元格，仍保留“部分”及原说明 | 文件差异 |
| G4 页面验收 | 不适用【实现】 | 没有新增或修改页面，不涉及设计稿或视觉对照 | 文件差异 |
| G5 范围 | 符合【实现】 | 只改生成工具、测试、构建登记、总表、状态及报告 | 文件差异 |
| P1 差异与隐私检查 | 通过【实测】 | 8 个相关文件；208 个本地链接无缺失，均小于 5 MB；空白检查通过；提交差异隐私正则 0 匹配 | V3 |

V1：`python3 tools/capabilities.py --check --runtime build/test-out/capability-table/capability-inventory.json`。142 项（含事件、仅人工和非当前页能力）及 14 个设置元数据一致；每项能力核对 7 种来源 × 有无成人 PIN × 有无 asked，来自编译的原 `confirm_for`，PIN 有无仅用测试替身，没有设真实密码。`python3 tests/capability_table/check_runtime.py build/test-out/capability-table/capability-inventory.json` 的 9 个反向对照（删能力/设置、改描述/风险/暴露/儿童权限/确认策略/设置确认/设置类型）均被拒绝。源目录检查另有 13 个负输入覆盖遗漏、未知值和过期文档。反向只改内存副本和临时输出，不撤改工作区代码。

V2：两套均按总规则完整参数配置，另用相同的 `-DINK_HUB_PYTHON=$PWD/work/hub-venv/bin/python`，保留 `INK_WERROR=ON`，内存套只另加 `INK_SANITIZE=ON`。中枢测试环境在忽略的 `work/hub-venv`，Python 3.11.15、uv 0.9.5 按 `hub/uv.lock` 冻结安装。构建和全量测试逐轮串行，构建 `-j4`，CTest `-j3`；`gui-smoke` 使用原 SDL dummy，无窗口。

以下四条命令均退出 0；最终全量日志为 `work/capability-ctest-final.log`、`work/capability-ctest-asan.log`，两套各 100/100、无跳过。运行时导出在各自 `build*/test-out/capability-table/capability-inventory.json`，无 ASan/UBSan 错误。本次只增加测试工具，固件资源预算和产品源码不变。

```sh
cmake --build build -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
```

V3：`git diff --stat 1c43c28a..HEAD`、`git diff --check 1c43c28a..HEAD`，及总规则 §6 的原隐私正则：grep 退出 1、无输出，即 0 匹配。受保护的需求、想法、决策、研究、简报和审计原文均未改；没有提交依赖、构建产物、真实身份或凭据。没有 push、PR、设备操作或付费接口调用，中枢只用本机合成身份。

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

未修改任何已有断言、期望、预算、时限或跳过规则。新增四项 CTest：生成一致性、解析/缺失回归、完整运行时导出、运行时与策略核对（含反向对照）；产品服务、策略、界面和应用源码均未改。

## 要用户定的

无。

## 顺手发现

配置的中/粗字体变量仍未被原 CMake 使用，链接器仍提示重复库；与本节无关，未修。新增 C 导出全部文件操作都在 `vtk_host_call` 内执行，检查输出/关闭返回值；没有新增 `write/read/fgets` 调用。没有新增设备常驻任务或改资源预算。
