# ui-input-host 任务报告

日期：2026-11-09。基线：`3e503e74daa9c8003bcd788073fe00e74c0def04`。
范围：应用框架 F2，产品代码预算 250 行；本轮接续 `8e960a88`，修复[独立复查](REVIEW.md)的 R1（高）、R2（中）。初始工作区干净，复查为严重 0、高 1、中 1；保留复查原文。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1 | 【实测】已修复 | 现有符号行补 `=`、`_`、`%`、`#`；实际查找可见键、切换模式并触摸，得到 `example.com/a_b?x=1%20#c`。单独撤掉修复后 5 项断言失败 | 定向命令；反向对照 |
| R2 | 【实测】已修复 | `compose_line()` 为已绑定的短宿主按标签字体、字距和像素宽度调整窗口起点，为光标留宽；40 字符网址末尾 `?x=1` 和完整光标均在可见区。单独撤掉修复后 1 项断言失败 | 同上 |
| 1 | 【实测】通过 | 搜索／网址的快照矩形与实际 LVGL 一致，字段不被遮挡，字母／符号切换保持键盘矩形；实际按键均在固定区域内 | 定向命令 |
| 2 | 【实测】通过 | 焦点轮廓、光标可见；完成／HOME 后字段和相邻标题恢复原坐标；新增普通长度网址的光标可见性断言 | 同上 |
| 3 | 【实测】通过 | 人工粘贴与 AI `text.insert` 的字段值、change／submit 事件相同；网址由真实触摸输入，不再直接注入键码 | 同上 |
| 4 | 【实测】通过 | 换人后旧 insert／submit 拒绝，字段与事件无副作用；仅撤成员检查时 2 项断言失败 | 同上；反向对照 |
| 5 | 【实测】本机通过 | 普通及 ASan／UBSan 全量各 207/207、零跳过；两套 F2 均 251 项断言通过；累计产品改动保守计数 166 行，不超过 250 | 完整验证命令 |

定向命令：

```bash
taskpolicy -c utility cmake --build build --target sim-ui-input-host -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ui-input-host$' --output-on-failure -j2
```

## 没做到的

- 本轮 R1、R2 均已修复，无未处理的严重／高问题；五项本机验收均完成。
- Linux／gcc、实机未运行；只接入任务要求的测试页面，未迁移既有业务页，未做长文／表单编辑页。

## 和设计不一致的

- 本轮未发现 F2 范围内新增差异。短宿主仍要求固定位置的父容器；复查列出的既有输入法外观差异未在本轮改动，不能将本报告当作完整业务页还原验收。

## 改过的已有测试

- `tests/ui_input_host.c`：网址样例从 `example.com/path?x=1` 改成包含全部四个缺失符号的 `example.com/a_b?x=1%20#c`；逐字注入 `keyboard.key` 改为查找可见标签、读取按键矩形并触摸其中心。保留完整值、无拼音组合的断言，新增按键存在与区域断言。
- 同一测试的符号切换改走真实触摸；新增 40 字符网址的完整模型值、末尾内容和整个光标位于字段内的断言及两张截图。没有删用例、放宽已有断言或修改资源门槛。

## 要用户定的

- 无。

## 顺手发现

- 无新增范围外修复；未改 F1、策略、文件／传输服务。没有推送、开 PR、访问设备或调用外部服务。

## 新增的机制

- 本轮无新增任务、线程、状态机、持久格式、配置项、缓存、队列或抽象层。
- 原任务的短字段绑定、固定键盘区域、位置恢复和 URL 类型均来自 F2；本轮只补现有符号行和既有字段裁窗。

## 要改的文档

- 主会话可在共享 backlog、UI 依赖表关联 F2 及本轮 R1／R2 的实测结果；本任务不改共享文档或生成的能力表。

## 阶段证据与规模

- 【实测】本轮定向构建与测试均退出 0，`test.pass=251`、无失败、`test.finished=1`、无警告，内部 RAM 总池最低余量 47,840 字节，原门槛 32,768 未改；人工查看 `clean-url-symbols.png`、`clean-url-long-cursor.png`，四个符号、网址末尾和光标均可见。截图在 `build/test-out/ui-input-host/`，不提交。
- 【实现】本轮产品新增 12、删除 1，共 13 行：补键 2 行、裁窗 11 行。相对指定基线，产品新增 149、删除 11，连 CMake 6 行计入共 166，低于 250。累计超过 100 行来自原 F2 的字段身份、同源快照、固定键盘与位置恢复；仅包装 `ink_text_focus` 缺少这些验收项，不能替代。两项复查修复均未超过单问题默认 30 行。
- 【实现】R-061：按键继续调用能力总线，文本编辑和成员校验复用文本服务；像素裁窗和字符位置映射集中在 UI 字段，不为页面或 AI 另建编辑入口。现有秘密字段规则与 AI 快照边界未变。
- 【实测】首轮普通全量 206/207，`integration-text-main` 的两项旧断言失败：它要求非短宿主的长字段固定保留前 30 字符，且点击首字对应索引 12。将像素裁窗限定为 F2 已绑定的短宿主后，相关两项测试通过、普通全量重跑 207/207；没有修改这两项断言。旧长字段的像素裁窗仍未处理，属长文任务范围。

## 反向对照

【实测】保留本轮测试，逐项临时撤掉产品修复。R1／R2 分别用 `git show 8e960a88:<对应文件>` 的内容；成员反例只删除 `text_caps.c` 的 `ink_text_session_current()` 拒绝分支。每次运行上述定向构建、CTest，随后恢复原字节；实验程序用 `finally` 保证恢复，最终全量会重新构建恢复后的源码。

| 仅撤掉什么 | 编译／CTest 退出码 | 实际失败 |
| --- | --- | --- |
| R1：`system/ui/shell/keyboard.c` 的补键 | 0／8 | `_`、`=`、`%`、`#` 四个可见按键缺失，且完整网址值不符，5 项失败；`test.finished=1` |
| R2：`system/ui/textfield.c` 的像素裁窗 | 0／8 | 光标 x=685，字段内容范围 79..604，完整光标越界，1 项失败；`test.finished=1` |
| 成员检查：`system/services/text/text_caps.c` | 0／8 | 旧 insert 被接受、字段／事件被改，2 项失败；`test.finished=1`。submit 此时仍被 revision 拒绝，不声称单独验证了 submit 反例 |

本地证据：`work/ui-input-host-fix/{r1,r2,member}-{build,test}.log`、对应 `*-metrics.json` 及 R1／R2 的反例 PNG，不提交。恢复后 `git diff --exit-code -- system/services/text/text_caps.c` 退出 0。

## 完整验证命令

复用已有两套配置，均为 `INK_WERROR=ON`；`build-asan` 另有 `INK_SANITIZE=ON`。字体、Python、LVGL、SDL2、ImGui 和中枢 Python 使用原报告已配置的 `$INK_DEPS` 只读依赖，未更改版本或配置。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

- 【实测】两套构建均退出 0；普通全量退出 0，207/207、零失败、零跳过，110.01 秒；ASan／UBSan 全量退出 0，207/207、零失败、零跳过，192.96 秒。日志分别为 `build/ui-input-host-fix-{build,ctest}.log` 和 `build-asan/ui-input-host-fix-{build,ctest}.log`。包含确定性、分层、SDK 边界、F1 与旧文本集成测试；GUI 冒烟使用既有 SDL dummy，没有启动窗口版。
- 【实测】两套 F2 均 `test.pass=251`、无失败、`test.finished=1`、无警告、无预算违规，内部 RAM 总池最低余量均为 47,840 字节。没有更改测试预算。
- 【实测】`git diff --stat` 核对无夹带，本轮只改两个产品文件、F2 测试和本报告；F1、旧文本测试及资源门槛文件相对复查提交无差异。`git diff --check` 退出 0；报告链接有效。按总规则第 6 节对基线至 HEAD 的隐私扫描零匹配（grep 退出 1），包含工作区的同规则扫描也零匹配。
