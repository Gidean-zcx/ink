# polish-1 执行报告

基线：`2587faa2fca29425bf913127c3e6914daa576e8e`。仅本地提交，不推送、不操作设备。
表内命令为简写：`python`、`ctest` 使用 `$INK_DEPS/build-deps/pyenv/bin/` 下的工具，macOS 命令统一加 `taskpolicy -c utility`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 密码键盘高度与恢复、页面排版 | 部分完成【实测】 | 高度、候选隐藏和普通键盘恢复通过；Wi-Fi 当前焦点常规／120% 不遮挡；密码转普通短输入有遮挡：字段底 771，键盘顶 627，基线该布局断言不失败 | `ctest --test-dir build -R '^ime-keyboard1$'`；`python docs/tasks/polish-1/validate.py --build build --layout-probe`；加 `--baseline` 对照 |
| 1b 短信回车发送 | 通过【实测】 | 新建和回复均显示“发送”，按真实键盘控件后只发一次，号码／正文／回执一致；AI 的 `keyboard.key`、`text.submit` 均需确认，拒绝后不发送 | `ctest --test-dir build -R '^sms$'` |
| 1c 日期标头与五分钟合并 | 通过，保留原页首时间【实测／实现】 | 今天、昨天、同年更早、跨年；跨方向的连续 4 分钟／299 秒合并，300／301 秒分组，快照与实际文字一致 | 同上 |
| 1d 反向对照 | 通过【实测】 | 基线键盘退出 1、5 条失败；基线短信退出 1、29 条失败；当前常规驱动分别 2513／104 条通过、0 失败（独立布局复现见 1a）；来源改回触摸另有 4 条预期失败 | `python docs/tasks/polish-1/validate.py --build build --baseline`；去掉 `--baseline` 验证修改；`--source-control` 验证来源回归 |
| 2 普通全量测试 | 未全过【实测】 | 413 项：408 通过、2 项旧等高断言失败、3 项缺中枢依赖跳过；未改范围外的旧断言 | `ctest --test-dir build --output-on-failure -j2` |
| 3 常规与 120% 前后截图 | 通过【实测】 | [8 张截图](SCREENSHOTS.md)，单张最大 47,142 字节 | 同上 |
| 内存检查（总规则） | 已跑完，未全过【实测】 | ASan+UBSan 全量：408 通过、同样 2 项旧断言失败、3 项跳过；全量日志无 sanitizer 错误 | `ctest --test-dir build-asan --output-on-failure -j2` |

## 没做到的

- 密码转普通短输入的遮挡未修复；普通和 ASan+UBSan 全量各有 2 项旧断言失败。原因和需扩大的范围见“要用户定的”。
- 【实现／实测】短输入框在建立新会话前按上一个输入类型测量键盘，恢复的普通键盘会遮住字段。`--layout-probe` 目标退出 1、1 条布局失败；基线该断言不失败（基线另有 5 条新高度验收失败）。复现驱动退出 0 表示成功命中已知问题，不表示排版验收通过。
- 未编 S31 固件，未跑 Linux 全量或操作实机；主工程已编译并运行 S31 宿主机用例。GCC 汇编检查不等于 Linux／S31 固件运行验证。

## 和设计不一致的

- 【实现】密码键盘高度依任务书变小，候选／编辑／听写保持原高度；旧“所有状态同高”断言须随需求更新，未修改设计合同。
- 【实现】文字完成回调没有来源参数。本次仅在键盘回车的同步调用期间提供来源，短信发送继续走 `message.send`；来源未知的 `text.submit` 按 AI 处理，保留确认。共享输入接口与确认策略没有修改。
- 【实现】分页沿用既有的每页首条显示时间规则；五分钟分组按相邻消息计算，续页保留页首时间上下文。

## 改过的已有测试

- `ime_keyboard1.c/.py`：增加密码高度、符号页与普通输入恢复检查，以及两种字号的真实 Wi-Fi 页面；截图数量 18 改为 20，因为增加了这两张图。
- `sms/sms.c`：原 `type_short` 用回车结束草稿，现改为失焦，避免提前发送；新增回车、日期、相邻间隔、AI 回车确认检查；原标头计数从识别固定日期字符串改为核对实际分组的预期文字。
- `comm_ux2.c`：仅短信正文草稿的测试助手用失焦结束输入，保留后续明确发送和选择联系人前保留草稿的原有断言。
- 未删除、放宽既有断言或增加等待时长。

## 要用户定的

- 【实现／实测】`tests/ui_shell_regressions.c:563` 仍要求密码、候选和编辑状态的键盘区域完全相同，与本任务“密码键盘矮一截”直接冲突。该文件不在允许修改范围内，本任务保留断言；需主会话允许更新为“候选／编辑同高，密码变矮”，不能用恢复空白来迁就旧断言。
- 【实现】需主会话允许修改 `system/ui/textfield.c` 的 `focus`（约 200、211 行）：按新输入会话的键盘类型测量并定位，统一解决跨类型切换；[任务范围](../polish-1.md)只允许两个产品 C 文件，本任务没有逐页补位置或修改范围外组件。

## 顺手发现

- 【实测】120% Wi-Fi 密码页仍露出原网络设置的少量背景文字／边线，前后截图均可见；本任务不修改 `navigation.c`。
- 【实测】初轮普通全量的 `unit-s31-uhci-negative` 在自身基准的 `cell.c:16`（AT/ACK）中止；最终普通／ASan 全量本项均通过（27.94／29.88 秒），独立 `unit-s31-cell-negative` 也通过。该目标不链接本次修改的系统／应用代码，未改端口或用例。

## 新增的机制

- 无新增任务、线程、状态机、文件格式、配置、缓存或重试。现有回车同步调用传递来源，用于满足“发送规则、确认不变”；没有改变能力注册或发送流程。

## 要改的文档

- 主会话更新任务台账，记录密码键盘缩短和短信回车／标头规则；本任务不改共享文档。

## 规模与检查

- 产品代码（含英文表）：新增 49／删除 21，合计 70 行差异，低于 150。
- 【实测】`INK_WERROR=ON` 普通构建成功；最终针对性 4 项原话：`100% tests passed out of 4`（15.71 秒）。命令均加 `taskpolicy -c utility`，构建及 ctest 均为 `-j2`。
- 【实测】`gcccheck: 5 compiled, 0 with findings`，覆盖两个产品 C 文件及三个修改的测试 C 文件。
- 【实测】最终普通全量退出 8，原话：`99% tests passed, 2 tests failed out of 413`；`Total Test time (real) = 853.43 sec`。失败仅 `ui-snapshot-regressions-metalio-plus`、`ui-snapshot-regressions-desktop-lcd`；跳过为 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`。
- 【实测】ASan+UBSan 构建成功（`INK_SANITIZE=ON`、`INK_WERROR=ON`），全量退出 8，原话：`99% tests passed, 2 tests failed out of 413`；`Total Test time (real) = 1536.83 sec`。失败／跳过项同普通全量；使用 `UBSAN_OPTIONS=halt_on_error=1`，无 sanitizer 错误报告。
- 【实测】独立布局复现在普通／ASan 构建均为 1 条坐标断言失败（771 对 627），无 sanitizer 错误；基线同一布局断言不失败。命令为 `validate.py --build build --layout-probe`，换 `build-asan` 复测，加 `--baseline` 对照。
- 【实测】基线至最终差异隐私扫描 0 匹配；文件范围检查 0 越界；`git diff --check`、Python 语法和本地链接检查通过。8 张图均小于 300 KB，手动对照临时运行目录已清理；未推送、未开 PR、未启动带窗口 GUI 或操作设备。
