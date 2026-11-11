# ui-fix-a 执行报告

基线：`da3296b75fda740f680dc5639d835d4e06d9a33b`。状态：执行中；只在本地分支提交，不推送。

里程碑一：【实测】已建立四组专用模拟器测试，100% / 120% 基线反向对照均出现预期失败；前图在 [before/](before/)。重启、清空和 USB 的页面请求由测试截获，不执行实际操作；秘密掩码额外经过真实确认服务。

里程碑二：【实测】四页修改的专用测试在两个字号均通过；见[前后截图索引](SCREENSHOTS.md)。原目录的普通和 ASan 构建均被新增文案缺少英语词条阻止，正在隔离副本验证[40 条词表候选](en-additions.tsv)；候选尚未写入原词表，不能把候选验证当作原目录全量通过。

里程碑三：【实测】首轮隔离全量暴露了确认卡回归：附加信息混入正文改变了原有分页，来电后重算秒数改变了画面。已改成独立信息区，保持交易正文逐字分页；有效期明确从首次显示起算，同一请求重现时保持该文案。17 项针对性复查全部通过，原有无缺字、画面一致、来源限制、FIFO 与孩子权限断言未改。第二轮完整验证进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| UX04（U03～U06；U01/U02 未改项见下） | 页面测试通过 | 【实测】100% / 120% 各 30 检查；清单范围、四个实际服务阶段、重启调用、部分失败说明；基线各失败 9 条 | `reverse.py --build build --current --case backup`；反向去掉 `--current` |
| UX05（X01～X09） | 页面测试通过；服务缺字段限制见下 | 【实测】两个字号各 70 检查；实际有效期、完整对象、具体动词、掩码、末页阅读限制、本人密码和过期；17 项跨页复查全部通过；基线待最终补录 | 同上，`--case confirm` |
| UX11（U07～U11） | 页面测试通过 | 【实测】两个字号各 26 检查；接收中不冒充成功、成功指明打开位置、稳定退出位置、USB 忙不承诺代停、一次性码不进 AI；基线各失败 7 条 | 同上，`--case transfer` |
| UX23（X10/X11） | 页面测试通过；整卡保留限制见下 | 【实测】两个字号各 25 检查；默认保留共享书/音频、清空所有家人的后果、备份、退出、卡选项重置、下一步；基线各失败 4 条 | 同上，`--case factory` |
| 全量测试 | 原目录构建受阻，未通过最终验收 | 【实测】英语词条校验拒绝新增文案；隔离候选验证中 | `taskpolicy -c utility cmake --build build -j2` |
| ui-scale | 通过 | 【实测】`ui-scale: 52 files, 0 violations` | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_scale_check.py` |
| 内存检查 | 原目录构建受阻，未运行 ASan 测试 | 【实测】`build-asan` 已配置，构建同样被词表校验阻止 | `taskpolicy -c utility cmake --build build-asan -j2` |
| S31 | 未编译 | 界面任务无设备操作；候选验证后再记实际执行范围 | 无 |
| gcccheck | 通过，最终修改后待复核 | 【实测】`gcccheck: 11 compiled, 0 with findings`，包括四页、四个新驱动和三个既有专用驱动 | `gcccheck.py build <这 11 个 C 文件>` |
| 隐私检查、行数预算 | 收尾时核对 | 四个产品文件合计 +86/-62，共 148 行变动，未超 250 行；UX04 34、UX05 53、UX11 40、UX23 21 | `git diff --numstat` |

## 没做到的

- 没有真机操作、真实清空或真实支付。全量 ctest、ASan 最终验收尚未完成，不能写“工程通过”。
- U01/U02 的正常备份布局、自动备份轮换、失败另页没有重做；它们不属于本任务的恢复预览、阶段和结果修正。
- U08 服务不返回文件名、格式可用性，页面只说明收完后可到文件中打开，未虚构文件名或保证阅读器支持任意格式。
- X04/X06 服务未给结构化商户、金额、币种或旧值时仍呈现服务原文，不编造缺少的字段；未改服务的执行策略。无法确定动作类型的请求保留“继续操作”。

## 和设计不一致的

- 【实现】“默认不动 SD 卡”不能解释成整张卡不写不删：`system/services/file/file_service.c:632` 的既有重置固定清掉成员目录、更新目录及缓存，`clear_media=false` 只避免额外删除共享 books/audio。页面明确这项后果，未越界改重置逻辑。
- 【实现】保留“恢复需本人确认”“重置需本人密码”的已有策略；不新加确认、不改变 D-105 的分类。前后截图展示的是服务实际可提供的信息。

## 改过的已有测试

- `tests/backup_pages.c`：原“开始用→主页”期望改为完成页含 `system.reboot`；后续场景主动调用主页能力继续测试，避免真的执行模拟重启。恢复按钮短为“恢复备份”，120% 不再省略。
- `tests/factory_pages.c`：两处“继续”改为“验证并清空”，完成动作“知道了”改为“重新设置设备”；密码、备份、清空和字节对照断言均保留。
- `tests/transfer_pages/device.c`：三处“停下，开始”改为“开始连接”，“结束”改为“结束传输”；实际 HTTP 字节和 USB 占用检查均保留。
- `tests/confirm_pages/pages.c`：按钮期望“确认修改/确认支付/确认”改为“修改设置/支付/继续操作”，队列第二项使用相同新文案；`run.py` 的负向变异匹配相应更新。没有放宽执行次数、队列目标或来源限制断言。
- 公共测试文件未改。新增测试的 AI 拒绝断言接受既有网关的 `REJECTED` 或隐藏能力返回的 `INVALID`；均不接受执行成功。

## 要用户定的

- 新文案需要向 `assets/i18n/en.tsv` 追加 [40 条必要词条](en-additions.tsv)，不替换已有键，所有格式占位符已核对。任务书“只改四个页面、专用测试和任务目录”没有包含词表；已请求该单一范围扩展，未获答复前不改原词表。

## 顺手发现

- 【实测】在线决策板无法读取；本任务按给定审计与任务书执行，不修改共享决策记录。

## 可复现命令

- 基线反向对照（手动，不注册进 ctest，不依赖 CI 的 Git 历史）：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fix_a/reverse.py --build build --evidence docs/tasks/ui-fix-a/before`。
- 四组当前页面的定点复编与运行：同上追加 `--current --evidence docs/tasks/ui-fix-a/after`。这是页面定点验证；原目录整工程仍受词表校验阻止。
- 常规注册的四组测试：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^ui-fix-a-'`。它们不依赖 Git 历史。
- 原词表未授权时，运行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fix_a/candidate.py` 建立真实源码副本。随后用总规则的同一配置参数，把 `-S` 换为 `work/ui-fix-a-validation/source`、`-B` 换为 `work/ui-fix-a-validation/build`；普通构建、ctest 命令同样换该构建目录。ASan 使用 `work/ui-fix-a-validation/build-asan` 并加 `-DINK_SANITIZE=ON`。只有该副本附加任务目录中的词条。
- 所有测试都使用合成数据；截图生成时转换为无损灰度 PNG，单张不超过 300 KB。

## 新增的机制

- 无；不新增产品任务、状态机、文件格式或配置项。
- 测试驱动复用已有整机测试框架，各启动一个仅模拟器使用的驱动任务；可选 `INK_UI_FIX_EVIDENCE` 只决定测试截图输出目录。均为任务书要求的测试和截图，不编入设备固件。

## 要改的文档

- 主会话合并后将 U03～U11、X01～X11 的实际完成范围与上述服务信息缺口同步到审计追踪；共享需求、决策、能力表和依赖文档未改。
