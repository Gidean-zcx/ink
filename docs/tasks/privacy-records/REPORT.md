# privacy-records 执行报告

基线：`2228df7de99de9fea15fddc22868a0de1caae770`。分支：`claude/codex-privacy-records`。状态：实现及定向验证完成，首轮全量已跑；补齐接口契约清单后重跑全量，内存检查待运行。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 部分完成 | 【实测】四行导航及活动件数；【实现】对话没有历史读取后端，显示“暂时读不到次数”，没有伪造 0 次 | `ctest --test-dir build -R '^privacy-records$' --output-on-failure` |
| 2 | 通过 | 【实测】大人查看孩子，孩子读自己；指定大人被拒；孩子的 181 天旧记录不进入件数或 AiLog | 同上；`python tests/privacy_records/reverse.py build` |
| 3 | 通过（现有数据范围） | 【实测】删除前确认、AI 不能代确认、取消保留、本人及孩子活动记录清空并归零、互不影响；页面说明其余记录暂不能删除 | 同上 |
| 4 | 通过 | 【实测】隐私页两入口分别打开本人、孩子段；页面文字与返回的 365/180 天一致 | 同上 |
| 5 | 待全量结果 | 【实测】严格构建、相关已有测试、截图、反向对照已跑；全量和内存检查进行中 | 见下文 |

定向检查：`ui-scale`、`settings-a`、`settings-b`、`ai-pages` 均通过；新测试 151 项断言通过，内部 RAM 最低余量 47,224 字节，高于 32,768 字节门槛。证据在 `build/test-out/privacy-records-sim-privacy-records/`。

反向对照：只编译替换对象，不修改产品源文件；放开成员权限、取消实际删除、接错活动入口、取消孩子入口参数，分别产生 1、4、3、1 项失败，退出码均为 1。命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/privacy_records/reverse.py build`；摘要：`build/privacy-records-reverse/summary.json`。

产品改动目前增加 195 行、删除 64 行；含构建接线增加 200 行，增删合计 264 行，均低于 300 行预算。超过 100 行的原因：四行汇总、分段、说明、删除入口和快照需要同屏布局，活动接口及 AiLog 还需同时支持孩子权限和真实期限。更小方案：保留原 Records 分页列表不符合汇总页合同；直接用数据库原始件数会把过期记录计入；两者均未采用。没有新增后端、常驻任务或通用组件。

## 没做到的

- 对话次数：`ai_chat.c` 的 AiHome 只有空状态，AI 服务不提供可读取的历史集合或统计；没有新建持久格式来凑次数。
- AI 记忆、运行记录的读取与删除没有后端，按任务边界显示不可用。运行记录入口打开说明页。
- 尚未在 Linux/gcc 或实机运行；本次不触碰设备。

## 和设计不一致的

- `Records` 用“我／孩子”而非画板的示例姓名；件数为真实合成夹具，其他类显示实际缺口。保留期限仅承诺活动记录，大人 1 年、孩子 180 天，不照抄画板里尚无实现的期限或自动物理删除。
- 【实测】`tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/privacy-records-compare --scenes Records SetPrivacy AiLog AiMemory --reuse`：Records 57.446、SetPrivacy 36.133、AiLog 28.666、AiMemory 28.311，单位均为 /255，四屏均未达到 12。字号按规范放大，灰色按现有主题，未为分数改变规格；SetPrivacy 沿用框架分页，AiLog 用真实夹具内容且沿用既有列表，AiMemory 沿用任务指定的不可用状态。截图和三列对照在 `build/privacy-records-compare/`。

## 改过的已有测试

- `tests/settings_b.c` 的 25 条活动遍历：从 Records 点进 AiLog，再用 `ai.pages.next` 遍历；原来在 Records 使用 `family.next` 和“更早记录”的预期改为新导航合同。保留全部 25 条可达断言、JSON 可读断言及持久化检查；截图名改为 AiLog25。

- `tests/integration/ai_surface.c` 的能力契约清单：登记 `activity.clear`（必填 `member`）和 `shell.runtime_records`（无参数）；`shell.records`、`shell.ai_log` 从无参数改为可选参数。对应产品接口已改变，保留原有“能力必须入表”和 schema 必填项检查。首轮全量据此检出 5 项旧契约预期，已按实际接口修正。

## 要用户定的

- 多个孩子时，分段沿用现有 SetKid 选择第一位孩子的方式；`shell.records`、`shell.ai_log` 和 `activity.list` 可通过 `member` 指定其他孩子。是否要增加选择孩子的界面，留给主会话决定。

## 顺手发现

- 能力总表会因新增能力过期。按总规则第 7 节不改共享文档，不放宽或跳过对应测试；最终全量结果单列。
- 活动保留期限原实现是读取时过滤，没有按日期物理清除；本次 AiLog 同样过滤，全部删除会实际删除包括过期记录在内的活动记录。

## 新增的机制

- `activity.clear`：任务验收 3 要求的原子能力，复用 `ink_db_del`、现有后台执行器及确认卡；不记录“清空记录”本身，避免清空后立刻多出一条。
- `activity.list` 增加可选 `member` 及返回 `total`、`keep_days`；`shell.records` 和 `shell.ai_log` 增加目标参数：任务验收 1、2、4 要求。
- `shell.runtime_records`：任务第一项要求的运行记录入口，仅显示缺少后端的事实及 AI 快照。
- 新任务／线程、状态机、持久文件格式、配置项：无。

能力与界面边界：【实现】按钮和 AI 共用能力；读取和删除由策略层鉴权；UI 仅持有页内数据；新说明页和汇总页都有快照，确认只允许人作出。

## 要改的文档

- 主会话生成 `docs/system/capabilities.md`；候选已生成到 `build/privacy-capabilities.md`，415 项定义，单独 `--check` 通过。
- 主会话更新 `docs/system/ui-dependencies.md` 的 Records、SetPrivacy、AiLog 状态，保留对话、记忆、运行记录后端缺口；按第 7 节本分支不改这些共享文件。
