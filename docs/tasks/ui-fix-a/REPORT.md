# ui-fix-a 执行报告

基线：`da3296b75fda740f680dc5639d835d4e06d9a33b`。本地分支交付，不推送。**原工作区的全量验收未满足**：新文案需要追加英语词条，但词表不在任务允许范围内，原文件未改。只附加这 40 条候选译文的隔离副本已通过普通、ASan 全量；四个产品文件及所有变更的构建/测试代码与原工作区逐字核对一致。

[前后截图索引](SCREENSHOTS.md) · [40 条候选译文](en-additions.tsv)。全部画面使用合成数据，非真机证据。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| UX04：U03～U06 | 通过页面验证 | 【实测】两个字号各 30 检查；预览列人、日期、数量、清单内替换范围；四个阶段对应服务字段；完成按钮调 `system.reboot`；部分失败说明不确定范围，再次操作先预览。基线各失败 9 条 | 下述四组测试与反向对照，`--case backup` |
| UX05：X01～X09 | 通过页面验证；缺字段限制见下 | 【实测】两个字号各 70 检查；独立显示身份、来源、真实有效期和完整标题；正文逐字分页，120% 到末页才能确认；掩码、本人密码、过期和 AI 禁止代确认均保留。基线各失败 7 条 | 同上，`--case confirm` |
| UX11：U07～U11 | 通过页面验证 | 【实测】两个字号各 27 检查；等待、未保存的接收中、收到后可去文件打开分开；退出底栏固定；USB 不承诺自动停应用；一次性码不进 AI。基线各失败 8 条；既有 HTTP/USB 专用测试通过 | 同上，`--case transfer` |
| UX23：X10/X11 | 通过页面验证；整卡保留限制见下 | 【实测】两个字号各 25 检查；所有家人清空后果、备份状态、退出和本人验证可辨；默认不额外删共享书/音频，重开复位危险选项；完成入口为“重新设置设备”。基线各失败 4 条 | 同上，`--case factory` |
| 验收 1：前后图和反向对照 | 通过 | 【实测】四组 100% / 120% 前后图齐全，恢复完成、部分失败分别附图；撤掉整页修复后四组均失败 | `reverse.py`，见下 |
| 验收 2：原工作区普通全量 | **未通过，受范围限制** | 【实测】普通与 ASan 原目录构建均被英语词条检查拒绝；未绕过检查，也未擅改词表 | 总规则原配置及 `cmake --build build -j2` |
| 含候选词条的普通全量 | 通过 | 【实测】原话：`100% tests passed out of 395`；`Total Test time (real) = 927.18 sec`。392 项执行通过、3 项跳过；`ui-scale: 52 files, 0 violations` | 候选目录普通构建、ctest，见下 |
| 含候选词条的内存检查 | 通过 | 【实测】`INK_SANITIZE=ON` 的 ASan + UBSan 完整构建与全量，原话：`100% tests passed out of 395`；`Total Test time (real) = 1337.89 sec`；同 3 项跳过 | 候选目录 ASan 构建、ctest |
| GCC 检查 | 通过 | 【实测】最终 `gcccheck: 12 compiled, 0 with findings`；最后仅修测试中的 3 处格式化类型转换，普通和 ASan 的 `confirm-pages` 随后均复跑通过 | 下述 GCC 命令 |
| S31 | 未编译固件 | 【实测】主工程中的 S31 宿主单元、负向检查通过；没有运行固件构建、烧录或设备操作 | 主工程全量 |
| 隐私、改动范围和预算 | 收尾核对 | 四个产品文件 +87/-62，共 149 行变动：UX04 34、UX05 53、UX11 41、UX23 21；每项少于 60，合计少于 250 | 按总规则第 6 节扫描提交差异，最终结果见下 |

## 没做到的

- 原工作区没有达到“普通全量 ctest 过”的验收；只有附加必要译文的候选副本达到了。40 个键、格式占位符已逐一核对，不替换原有键。
- 3 个既有环境跳过项：`hub-contract` 缺锁定的 hub Python 环境，`hub-json-crosscheck` 缺由它生成的消息，`hub-simulator-e2e` 同样缺锁定环境。未安装或升级依赖来改变任务环境。
- Linux CI、S31 固件和真机未运行；没有真实支付、通信或设备清空。新预览测试截获重启/清空请求；既有重置测试仅处理隔离的合成闪存和卡数据。
- U01/U02 的备份入口布局、自动备份轮换及失败另页未重做；未认定它们不需要，留给对应界面整改。
- U08 服务不提供文件名和格式可用性，未编造文件名或保证任意格式都能阅读；只说明已收文件可到文件中打开。
- X04/X06 缺结构化商户、金额、币种或旧值时仍显示服务原文，不补猜值；无法确定类型的请求保留“继续操作”，未改执行策略。

## 和设计不一致的

- 【实现】“默认不动 SD 卡”不能解释为整卡零删除/零写入：`system/services/file/file_service.c:632` 的既有重置固定清掉成员目录、更新目录和缓存；`clear_media=false` 只避免额外删共享 books/audio。页面已说明卡上个人资料会删，没有越界改服务。备份状态来自当前家人，不是全家备份汇总。
- 【实现】按钮与 AI 仍走原子能力；状态来自服务回执，信息进入页面快照；协议、重置逻辑及确认分类均未改。确认和密码仍只由本人操作，不增加确认次数。

## 改过的已有测试

- `tests/backup_pages.c`：原“开始用→主页”改为断言完成页提供 `system.reboot`；后续场景主动调主页继续，避免执行重启。恢复按钮短为“恢复备份”，120% 不再省略。
- `tests/factory_pages.c`：两处“继续”改为“验证并清空”，“知道了”改为“重新设置设备”；密码、备份、清空和字节对照断言保留。
- `tests/transfer_pages/device.c`：三处“停下，开始”改为“开始连接”，“结束”改为“结束传输”；实际 HTTP 字节与 USB 占用检查保留。
- `tests/confirm_pages/pages.c`：“确认修改/确认支付/确认”改为“修改设置/支付/继续操作”，队列第二项同改；`run.py` 的负向变异匹配同步。另给 3 处 `%u` 参数加 `(unsigned)`，消除 RISC-V 的 `uint32_t` 类型告警，不改断言。
- 没改公共测试、预算或上限。首次全量发现的确认回归已集中修在确认页：身份信息独立于正文，同一请求沿用首次显示的有效期文案；原有无缺字、来电前后画面一致、FIFO、来源和孩子权限测试通过。

## 要用户定的

- 推荐主会话把 [40 条译文](en-additions.tsv) 追加到 `assets/i18n/en.tsv`，再在原目录构建验证。[任务书](../ui-fix-a.md)限定的可改文件不含词表；已请求该单一范围扩展，未获答复，故保留原词表。另一方案是继续保持当前范围，并接受原目录构建受阻。
- 若要求整张 SD 卡零删除/零写入，需要另行调整重置服务；本任务只把现有后果说明白。

## 顺手发现

- 【实测】在线决策板不可读取，未改共享决策记录。
- 【实测】隔离验证需要真实源码副本，且构建目录位于该副本内；否则既有负向测试及证据路径断言不成立。已提供专用准备脚本，原始日志留在 `work/ui-fix-a-validation/logs/`。

## 新增的机制

- 产品任务、线程、状态机、持久文件格式、后台重试、配置项：无。确认页只增加当前请求的显示缓冲和标签。
- 测试驱动复用既有整机框架，各开一个仅模拟器使用的驱动任务；可选 `INK_UI_FIX_EVIDENCE` 只决定截图输出目录。均为任务书要求的验证，不编入固件。

## 要改的文档

- 主会话合并后将上述审计编号、实际完成范围和服务信息缺口同步到审计追踪；共享需求、想法、决策、能力表及依赖文档均未改。

## 复现命令

先设置任务给定的 `INK_DEPS`。运行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fix_a/candidate.py`，然后使用总规则的完整 CMake 配置参数，将源码和构建目录分别换为 `work/ui-fix-a-validation/source`、`work/ui-fix-a-validation/source/build`。ASan 构建目录为 `work/ui-fix-a-validation/source/build-asan`，仅另加 `-DINK_SANITIZE=ON`。两者均 `taskpolicy -c utility cmake --build <构建目录> -j2`。

- 普通全量：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir work/ui-fix-a-validation/source/build --output-on-failure -j2`；ASan 将目录换成 `build-asan`。
- 四组专用测试：同一 ctest 命令追加 `-R '^ui-fix-a-'`，不依赖 Git 历史。
- 手动基线反向对照：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" work/ui-fix-a-validation/source/tests/ui_fix_a/reverse.py --build work/ui-fix-a-validation/source/build --evidence "$PWD/docs/tasks/ui-fix-a/before"`。基线历史仅用于手动模式，不注册进 ctest。
- 同一反向脚本加 `--current`，证据目录换为 `after`，可复编当前四页并生成后图。所有 PNG 均无损灰度、684×1216，单张不超过 300 KB。
- GCC：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/ui/shell/{backup_pages,confirm_card,transfer_pages,factory_pages}.c tests/ui_fix_a/{backup,confirm,transfer,factory}.c tests/{backup_pages,factory_pages}.c tests/transfer_pages/device.c tests/confirm_pages/pages.c`。

## 收尾检查

- 隐私扫描、文件链接、截图大小和最终提交范围：待最终提交后补录。
