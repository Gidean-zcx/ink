# kid-2 执行报告

基线：`50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`。状态：任务范围内实现和验证已完成；未达项如下，没有把文档失败算作全量通过。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 两分钟到时；锁屏 90 秒不计时；自动跨本地零点清零；第四页阅读位置保留 | `python tests/kid_2.py --build build` |
| 2 | 【实测】通过 | 错密码不改、不加；正确密码加 15 分钟；孩子未获验证不执行；去掉家长密码标记时测试失败 | 同上；`python tests/kid_2_negative.py --build build --case parent` |
| 3 | 【实测】通过 | AI、通知进入受限应用均出现 KidLimit；正确密码只打开一次，下一次仍拦；目录隐藏受限应用 | 同上；反向对照 `--case entry` |
| 4 | 【实测】范围内通过 | KidControl 四项及音量上限；本任务三页有真实快照，按钮复用能力。SetKid 归 settings-b | 同上；反向对照 `--case volume`；`python tools/ui_scale_check.py` |
| 5 | 【实测】重启、内存、预算通过；全量留一项文档失败 | 独立进程重启保留用时和加时；普通及 ASAN 专项各 306 个断言通过；产品新增 305 行。ASAN 全量 238 通过、3 既有跳过、1 文档失败 | 同上；全量命令见结束检查 |

## 没做到的

- 普通和 ASAN 全量仍有能力总表过期一项待主会话统一生成。没有达到“全量零失败”；未在 Linux 或实机上运行。
- 检查点每分钟落盘，锁屏、换人及策略调用也保存。没有测试检查点之间突然断电，最多一分钟的未落盘用时不保证恢复。
- SetKid 按任务边界由 settings-b 接线；没有声称其第四页快照在本任务完成。

## 和设计不一致的

已查看四张画板。对照分数：KidTimeUp **24.608**、KidLimit **23.687**、KidControl **43.693** /255，均超过目标 12。

按更新的规格使用 1.2 倍尺寸、孩子 110% 字号、#888888 底色和纯黑次要字；原稿仍是较小字号和浅灰。KidControl 按文字规范增加暖光，使用真实的空通知状态，原稿有两条示例通知。TimeUp 增加短信入口以落实 D-25，原稿只有电话。没有为分数改规格。

## 改过的已有测试

- `tests/ui_shell_scenes.c`：孩子的 Control 场景期望名称改为 KidControl，成人期望保留。
- `tests/ui_shell_regressions.c`：孩子打开设置被拒后，必须仍在 KidControl（原期望为 Control）。
- `tests/integration/ai_surface.c`：末尾追加 10 个新能力及必填参数，保留“每个能力恰好进一张表”的严格校验。

没有删除测试、放宽预算、跳过测试或放宽原有断言。

## 要用户定的

“爸爸妈妈加时间”暂按每次 15 分钟，密码页明确写出加时量；能力允许指定 0～1440 分钟。任务书未规定界面默认加多少，没有新增设置项。

## 顺手发现

- `reader.open_book` 直接压页面，导航栈满（6 层）时可能返回完成却不打开。本任务不改阅读器；阅读位置测试先回主页再打开已保存的书。
- 文件应用当前标成 child_ok，与“孩子主页不显示文件”的文字规范不一致；没有改并行的文件应用。受限跳转测试使用明确的合成家长应用。

## 新增的机制

- 现有 member.table 记录尾部追加用时字段：任务书明确要求和成员数据一起保存；保留旧长度读取。
- 复用现有服务定时器和后台执行器做分钟检查点、到时与跨日通知：任务书要求计时和重启保留。
- 无新任务、线程、队列、通用框架或配置项。

为什么不能更小：后端策略和落盘、统一权限关口、两个阻挡页、控制中心及本地密码复用需要一起交付。已采用的更小方案是复用成员记录、原确认流程、原密码页和原控制中心，没有为每个孩子新增设置，也没有逐应用加计时器。产品新增 305 行，低于 350 行预算。

## 要改的文档

- SetKid 的“每天能用多久”接 `member.time_get`、`member.time_set`；`id` 是孩子编号，读自己可省略，`minutes` 为 0～1440，0 表示不限；当天加时用 `member.time_add`。修改与加时进入现有家长密码确认。
- 主会话统一更新 `docs/system/ui-dependencies.md` 对应状态，并重新生成 `docs/system/capabilities.md`（新增 10 个能力与 member.time_up 状态）。遵守总规则第 7 节，没有改这两份共享文档，也没有改 settings_family.c。
- 服务设计 §1.3 仅追加任务书要求的三项用时能力和状态登记，没有改设计规则；相应文档检查已复测通过。

## 结束检查

【实测】普通全量首次 5 项失败：两处旧名称断言（共三项测试）、新增能力清单未登记、生成能力总表过期。更新名称和追加清单后，最终全量 237 项通过、3 项既有跳过、2 项文档检查失败（242 项，445.53 秒）。补充服务设计接口登记后，`services-mid-capability-doc` 单独复测通过。剩余失败是按任务要求未改的生成能力总表。

【实测】四项反向对照均命中指定断言：家长密码标记、锁屏暂停、音量钳位、受限应用准入；不是编译失败或缺资源导致的失败。专项与三个页面的最低内部 RAM 余量均为 47,736 字节，分配失败为 0。

【实测】ASAN／UBSAN 全量：238 通过、3 项既有跳过、1 项文档失败，242 项共 321.91 秒。失败仅为生成能力总表过期；服务设计文档检查已通过。随后用 `UBSAN_OPTIONS=halt_on_error=1`、`INK_KEEP_EVIDENCE=1` 运行 `tests/kid_2.py --build build-asan`，四阶段共 306 个断言通过，四份日志没有 AddressSanitizer 或 undefined behavior 报错。

构建配置使用总规则 §5 的全部依赖参数，保留 `INK_WERROR=ON`；内存构建另加 `INK_SANITIZE=ON`。所有构建 `taskpolicy -c utility cmake --build <build 或 build-asan> -j2`；全量 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir <目录> --output-on-failure -j2`。命令输出保留在各构建目录的 kid-2 日志中。

`tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/kid-2-compare --reuse --scenes KidTimeUp KidLimit KidControl` 已运行。能力总表在 `build/kid-2-capabilities.md` 临时生成并用 `--check` 校验通过，没有写共享文件。

普通专项证据在 `build/kid-2-23qp37j0`；严格内存专项证据在 `build-asan/kid-2-9gi6rpfs`。最终四项反向对照的命令为 `tests/kid_2_negative.py --build build --case parent`、`--case locked`、`--case volume`、`--case entry`；各有 `build/kid-2-negative-*-final.log`，对应目标断言分别失败 1、3、2、26 次。

以上 Python 命令均使用 `$INK_DEPS/build-deps/pyenv/bin/python`。普通与内存构建退出码为 0；全量 ctest 的退出码为 8（文档失败），服务文档单项复测和全部专项验证退出码为 0。

架构审视：【实现】成员模块负责策略与落盘，能力入口统一准入，界面只读状态并调用能力；电话／短信所需输入与三种通讯集合有集中例外，其他数据读写仍拒绝。密码输入保持设备专用，AI 无权填写或提交。

【实测】结束检查：相对基线产品代码新增 305 行、删除 19 行（合计 324 行也未超 350）；`git diff --check` 通过；改动文件均小于 5 MiB。按总规则的隐私表达式扫描提交差异，匹配 0 行。差异中没有设计稿、构建产物或其他任务文件。
