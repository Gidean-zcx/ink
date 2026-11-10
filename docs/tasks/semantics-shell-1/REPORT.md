# semantics-shell-1 执行报告

基线：`4c64ae16d383ca833084d8f3683478becb9754d8`。状态：普通版全量通过，内存检查及 S31 编译进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| A01-1 确认卡 | 通过【实测／实现】 | F1 导出；PIN 只有有值／无值，1 位与 4 位快照一致；过期目标和旧点按不取消事务 | `ctest --test-dir build -R '^semantics-shell-confirm$' --output-on-failure` |
| A01-2 打断卡 | 通过【实测／实现】 | F1 导出；旧目标／点按拒绝；底页更新后新点按仍执行；只改铃声不重画 | 同上，测试名换 `semantics-shell-interrupt` |
| A01-3 电源菜单 | 通过【实测／实现】 | F1 导出；能力名称照旧；旧目标／取消点按拒绝；重开后对象版本不同 | 同上，测试名换 `semantics-shell-power` |
| A01-4 家人门禁 | 通过【实测／实现】，本地选人边界见下文 | F1 导出可见家人和密码有无；旧目标／头像点按拒绝；无变化不重画 | 同上，测试名换 `semantics-shell-member` |
| A01-5 锁屏标记 | 通过【实测／实现】 | F1 导出实际文字、时钟及相机入口；旧目标／相机点按拒绝；无变化的音频事件不刷新 | 同上，测试名换 `semantics-shell-lock` |
| A01-6 关机画面 | 通过【实测／实现】 | F1 导出实际说明；没有虚构可操作按钮；旧目标被语义桥拒绝 | 同上，测试名换 `semantics-shell-off` |
| A02-1 原测试和契约 | 通过【实测】 | 原测试、hub 契约及新增测试均在普通版全量通过；已有测试未修改 | `ctest-final.log`，命令见下文 |
| A02-2 全量／内存 | 普通版通过【实测】，内存版待记录 | 普通版 330／330，无跳过，376.15 秒，退出 0 | 见下文 |
| A02-3 S31 | 待验证 | 只编译，不连接设备 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| A02-4 gcccheck | 通过【实测】 | `gcccheck: 15 compiled, 0 with findings`，退出 0，无跳过 | `taskpolicy -c utility python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build <8 个产品 C 文件和 7 个新增测试 C 文件>` |
| A03 行数及预算 | 通过【实测】 | 下表；产品 C／头文件净增 194 行，连同 CMake 测试注册保守计 203 行，低于 250 行 | `wc -l <六个文件>`；`git diff --numstat <基线> -- system CMakeLists.txt` |

`<基线>` 指本报告首行提交。所有主机构建／测试均用 `taskpolicy -c utility`，构建和 ctest 均最多 `-j2`，同一时刻仅一轮。

| 文件 | 迁移前 | 迁移后 | 净增 |
| --- | ---: | ---: | ---: |
| `system/ui/shell/confirm_card.c` | 365 | 415 | 50 |
| `system/ui/shell/interrupt_card.c` | 129 | 165 | 36 |
| `system/ui/shell/power_menu.c` | 102 | 125 | 23 |
| `system/ui/shell/member_gate.c` | 171 | 205 | 34 |
| `system/ui/lock_mark.c` | 115 | 156 | 41 |
| `system/ui/shell/off_picture.c` | 81 | 83 | 2 |

【实现】六个文件的七个 `.snapshot =` 注册（确认卡含过期态）现在只接 F1 适配函数，没有手写快照拼接。`snprintf` 仅保留原契约的字段值，例如“页面=”“家人=”“确认编号=”“阅读=”“排队=”“可确认=”和闹钟的“时间=”“日期=”。能力名和已有参数契约不变；本人确认、PIN 和本地身份选择未开放给 AI。

超过 100 行的原因：六处导出、可见按钮字段、动作版本及原控件绑定发现接口都须一起迁移。只替换快照文本而保留旧按钮路径不能拒绝旧点按；统一递增底页版本会破坏原确认事务的页绑定。因此复用 F1 的对象 `revision` 与现有浮层代数，没有增加通用框架。

## 没做到的

- 尚无实机验证；本任务不连接或操作设备。
- 门禁的头像选人没有现成的“仅本人选择某位家人”能力。保留原本地成员服务入口并加按下／点击版本校验，头像以语义字段导出；没有伪造能力或让 AI 换人。是否补独立原子能力见“要用户定的”。PIN 数字输入同样保留本地可信路径。
- 关机画面本来没有可点控件，测试核对空动作表和过期目标拒绝，没有制造“关闭关机画面”能力。

## 和设计不一致的

- 【实现】F1 原内部判断只允许已验证的底页，锁屏／关闭空间的系统浮层会导出失败。按任务第 5 项的最小绕过，增加私有 `ink_ui_action_current()`：底页仍走原验证，浮层核对其所属家人；`ui_semantics.h` 公共接口没有变化。涉及范围外的私有头、运行时和导出器共 8 行净增。
- 【实测／实现】框架文档对普通页面要求调用 `ink_ui_content_changed()`。浮层不能照搬，否则原 `integration-confirm` 的“确认卡不改变底页版本”契约失败；本次沿用 F1 已有的对象 `revision` 表示浮层内容变化，底页 `page_version` 保留。新按下捕获当时版本，已按下后版本变化的点击被拒绝。

## 改过的已有测试

无；原测试源文件、期望、断言和预算均未修改。新增六项测试及可选手动基线对照，不在 ctest 中依赖 Git 历史。

## 要用户定的

是否另开任务补“仅本人选择某位家人”原子能力，并同步能力目录及完整能力契约表。推荐补；本次按“能力契约不变”和共享文档冻结要求保留原本地入口，避免越界扩展。

## 顺手发现

- 【实现】打断卡原先判断“无需重画”只比较标题、正文、按钮数量，会漏掉按钮标签、能力和参数变化；本次一并比较这三项，只有铃声变化仍不重画。
- 【实现】确认卡的成员／电源／打断订阅仅负责关闭失效说明页，没有无关事件整页重画；新增排队数量订阅只推进对象版本。门禁只在需要打开／关闭时改页面；锁屏分钟和音频事件只更新实际变化的文字。电源菜单仅在锁定时关闭；关机画面没有状态事件订阅。

## 新增的机制

没有新任务、线程、状态机、持久格式、缓存或配置项。页面模型、对象版本和按下时的动作版本记录均用于任务第 1、2 项，复用既有 F1／LVGL 路径。

## 要改的文档

- 合并时在 `docs/system/app-framework.md` 补充：系统浮层的家人归属判断，以及底页 `page_version` 与浮层对象 `revision` 的区别。
- 能力目录保持原样，不需要生成 `docs/system/capabilities.md`；本任务没有保留新增能力。

## 过程记录

- 开工检查：工作区干净，当前分支基于指定提交，未接触设备或外部服务。
- 专项：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R 'semantics-shell-|ui-scene(-big)?-Lock|capability-table-doc|integration-ai-surface|integration-notify$|sound-clock$|phone-calls$|activity-card$|lock-camera$|integration-confirm$|confirm-pages$|ui-semantics$'`，`100% tests passed out of 20`，16.71 秒，退出 0。
- hub：使用依赖检出的 `hub/.venv/bin/python` 作为 `INK_HUB_PYTHON`；默认字体 Python 缺少 hub 测试依赖时会明确跳过，已纠正为真实执行。`focused-2.log` 的 13 项含 hub 契约全部通过，退出 0。
- 反向对照：依次运行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/semantics_shell/run.py --build build --scene <confirm|interrupt|power|member|lock|off> --baseline <基线>`；仅在构建目录中编译对应基线文件替换对象，不修改产品源码。六个基线运行均退出 1，分别 12、10、6、4、7、3 个断言失败，全部命中旧快照不符合 F1 的断言；对照脚本均退出 0。
- 普通版最终全量：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；原文 `100% tests passed out of 330`、`Total Test time (real) = 376.15 sec`；退出 0，无跳过。
- 日志均在忽略目录 `work/semantics-shell-1/`；专项和反向证据在 `build/test-out/semantics-shell-*`。首轮全量为 319／330，通过回归查明并修复兼容问题；最终普通版为 330／330。
