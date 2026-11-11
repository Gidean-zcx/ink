# sms-thread-2 执行报告

基线：`83ec2e4e4483d530866603c2e93474ed30ce96a3`。状态：交付完成；普通及内存全量均有 1 项基线可复现失败，未全绿。以下【实测】均为本机模拟器。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 1／5／20 条、常规／120% 字号；混合方向、失败、未知及长正文；12 张 PNG 均小于 300 KB | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" docs/tasks/sms-thread-2/check.py --build build --screenshots` |
| 2 | 【实现＋实测】通过 | 多气泡、底部最新、顶部翻页、早页往返、AI 重发、逐气泡语义和坐标、五分钟分组、长正文逐页拼回全文；单条分页反向退出 1，63 通过／2 失败／正常收尾；整页重画反向退出 1，34 通过／2 失败／正常收尾 | 上述脚本；追加 `--reverse`、`--reverse-refresh`；最终 `sms`、`comm-ux2`、`sms-multipart`、`integration-ai-surface`、`integration-races` 五项全过 |
| 3 | 【实测】未全绿，失败可在基线复现 | 原话：`99% tests passed, 1 tests failed out of 393`，946.78 秒，退出 8；389 通过、1 失败、3 跳过。最终五项定向回归全过（25.74 秒） | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 补充 | 【实测】未见内存诊断；静态检查通过 | ASan＋UBSan 全量同一基线失败；最终内存定向五项全过；GCC 3 文件 0 发现、UI 尺寸 0 违规、隐私 0 命中；产品净增 104 行 | 见下文 |

| 会话 | 常规字号 | 120% 字号 |
| --- | --- | --- |
| 1 条 | [最新页](thread-1.png) | [最新页](thread-1-large.png) |
| 5 条 | [最新页](thread-5.png)、[较早页](thread-5-older.png) | [最新页](thread-5-large.png)、[较早页](thread-5-large-older.png) |
| 20 条 | [最新页](thread-20.png)、[较早页](thread-20-older.png)、[长正文页](thread-20-long.png) | [最新页](thread-20-large.png)、[较早页](thread-20-large-older.png)、[长正文页](thread-20-large-long.png) |

【实现】按正文、时间和状态的实际高度从最新片段向前装页，打开会话定位最后一页。沿用原列表的翻页与按键；一页容器内放多条气泡，快照逐条给出方向、时间、状态、可见正文和仅对明确失败项启用的重发动作。短信集合、成员边界及全部公开能力名称不变，按钮和 AI 仍走同一能力。没有引入第三方资源。

【实测】发送时页面对象不变；以下为 LVGL `LV_EVENT_FLUSH_START` 向显示服务输出的刷新包围框，逻辑画布 684×1216，横坐标均为 0～683。没有用截图像素差冒充刷新证据，也未测试真机波形驱动区。

| 发送前会话 | 常规字号：纵坐标／输出条带数 | 120%：纵坐标／输出条带数 |
| --- | --- | --- |
| 1 条 | 682～1177／7 | 633～1177／7 |
| 5 条（先重发） | 461～1177／8 | 394～1177／9 |
| 20 条 | 216～1177／10，页数改变，包含顶部页码 | 394～1177／9 |

页数不变时只重画旧、新气泡实际占据区域的并集及输入区；组装控件期间完成布局后再恢复失效通知，避免将控件的临时原点也刷出。普通发送未重建页面。页数变化时顶部页码也必须更新，见“和设计不一致的”。

【实测】内存构建用同一套配置，另加 `-DINK_SANITIZE=ON`，保留 `-DINK_WERROR=ON`；构建和 ctest 均以 `taskpolicy -c utility`、`-j2` 执行。`ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1` 下全量原话同样是 `99% tests passed, 1 tests failed out of 393`，1465.97 秒、退出 8；389 通过、1 个相同通知断言失败、3 个相同 Hub 跳过，未见 AddressSanitizer 或 UBSan 诊断。

末次仅将一次同结构体字段间的字符串复制改为等长 `memcpy`；之后重编两种构建，普通与内存五项定向复核分别为 `100% tests passed out of 5`（25.74／42.16 秒），六组会话场景也在两种构建中各全部通过。内存复核命令：`ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan -R '^(sms|comm-ux2|sms-multipart|integration-ai-surface|integration-races)$' --output-on-failure -j2`；会话场景用上述脚本追加 `--build build-asan`。

【实测】`taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build system/apps/sms/sms.c tests/sms/sms.c tests/comm_ux2/comm_ux2.c`：`gcccheck: 3 compiled, 0 with findings`；`python3 tools/ui_scale_check.py system/apps/sms/sms.c`：1 文件、0 违规。按任务书隐私正则检查基线到最终提交的差异，0 命中（grep 退出 1）；文件、图片大小和报告链接检查通过。上述 GCC 检查不等同于运行 Linux CI。

## 没做到的

- 没有碰设备、运行真实运营商业务、推送或开 PR；没有 Linux CI 或 S31 固件编译证据。主工程中的 S31 宿主机检查按普通全量执行。
- 短输入框直接粘贴超过参数表容量的正文、且没有可复用的长文草稿时，仍走原来的创建草稿输入会话；该路径未提供局部刷新实测。普通短回复直接传正文，既有长文草稿按原引用发送。
- 普通全量“全部通过”的验收条件未满足；通知竞态测试在基线也会失败，按任务范围未修改其源码或断言。内存全量同样受此既有断言影响。

## 和设计不一致的

- 发送导致总页数改变时，除会话和输入区，还会更新顶部页码；不刷新联系人标题和状态栏。为保证页面与快照页数一致保留这项更新，没有将页码留旧。

## 改过的已有测试

- `tests/sms/sms.c`：长正文原先从第一片开始检查，现在先断言默认落在最新片，再用 `sms.prev` 回到第一片；随后逐片拼回完整原文的断言不变。可见片段从第 0 个气泡缓冲读取，替代旧的“页号对 4 取模”。长短信断言增加可复现的页号、可见数信息。
- 既有 `SmsThread` 阶段额外执行 20 条会话验收；新增独立的 1／5／20 条、120% 字号和刷新测量阶段。等待发送后的可见记录数增加，避免旧回执提前满足等待条件。补上底部锚定和失败提示同能力断言。未删测试、抬预算或放宽既有断言，未靠 sleep 掩盖时序。
- `tests/comm_ux2/` 无改动，既有触摸重发继续回归。

## 要用户定的

页数变化时，建议允许同步更新顶部页码（本次做法），保留准确页码；若仍要求顶部完全不动，需另定页码呈现方式。本次未改现有分页控件的设计。

## 顺手发现

- 普通全量的 `integration-races` 在 `tests/integration/races.c:121` 失败：`there were moments to try (2)`，要求至少 3 个。仅把短信应用替换为基线源码、其余链接对象不动，也退出 1，106 通过／1 失败，同一时刻和断言失败；随后最终五项定向回归中的原测试通过。未改通知服务或竞态测试。复现命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" docs/tasks/sms-thread-2/races-baseline.py --build build`。本次退出 1，产物在 `build/test-out/races-baseline/`；最终再跑该命令得到 132 通过／0 失败，记录为间歇失败，尚未定位根因；不是注册进 ctest 的历史依赖。首次失败的原始摘要保留在 `build/races-baseline.log`。
- 三项跳过为 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，沿用现有 Hub 依赖检测，未改跳过条件。
- 120% 字号截图中，底部既有短输入框的“内容”提示偏低；本任务未改受保护的 `textfield.c`，留给 `textfield-1`。

## 新增的机制

- 可变高度气泡分页：任务“要做”第 1、2 项要求，放在现有短信页面内，复用原列表翻页和能力总线。
- 新任务／线程、状态机、持久文件格式、后台重试、缓存、队列、配置项、抽象层和通用框架：均无。

产品代码新增 147 行、删除 43 行、净增 104 行，低于净增 150 行预算。超过 100 行的原因：任务同时需要按高度装页、分组时间、独立状态、逐条语义和局部刷新。看过的更小方案：只增列表容量仍按固定行高排，无法尽量填满；继续整页重建无法满足少刷新。未改通信服务、输入框或键盘。

## 要改的文档

主会话可在 `docs/tasks/backlog.md` 记录通知竞态覆盖数问题及输入框提示偏低；在 `docs/system/ui-dependencies.md` 将短信会话状态更新为多气泡、顶部翻页、逐条快照；能力名称不变，无需重生成能力表。本任务未修改共享文档。
