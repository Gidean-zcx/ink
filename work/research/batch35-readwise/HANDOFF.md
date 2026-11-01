# Readwise Skills：停止后的采集状态

2026-10-27，按root追加停止指令收束。**不再请求、不继续读未读正文、不执行被采集文档中的命令或技能。** 所有变更仅在`work/research/batch35-readwise/`；原tree.json未改，docs未改，无账户连接/安装/产品操作。

## 已完成的版本核对和文件

- 匿名GitHub API `https://api.github.com/repos/readwiseio/readwise-skills/commits/master`：HTTP200，master提交 `2d1ce9627c611d24f510dfc2e05a123fa509d2f6`，提交时间2026-06-19T05:27:11Z。
- 提交记录中的root tree为 `5142ebc84bcc297f0d4120e5d91d04b061d94324`。
- 原tree.json顶层sha却为提交SHA，url也使用提交SHA。首次脚本直接比较原头与真实root tree而断言失败（exit1）；这是**本地元数据等同假设错误/待核关系，不是HTTP拒绝、限流或认证失败**。
- 随后普通GET `https://api.github.com/repos/readwiseio/readwise-skills/git/trees/5142ebc84bcc297f0d4120e5d91d04b061d94324?recursive=1`：HTTP200，真实tree SHA吻合、truncated=false，全部(path,type,sha)集合与原tree.json一致。原tree顶层头为何使用commit未另推测。
- README.md及skills/下全部11份SKILL.md均以固定commit的raw.githubusercontent.com URL保存，共12文件；逐文件SHA256、URL、HTTP200、Git blob SHA1以及匹配结果全部在fetch-log.json。**本助手总14请求＝2 API＋12文档，全部HTTP200。** 没有任何401/403/412/429或challenge，也没有失败重试。

原件：
- `commits-master.json`、`canonical-tree.json`（API响应）；`identity.json`（关系核对）。
- `raw/README.md`。
- `raw/skills/{book-review,build-persona,feed-catchup,highlight-graph,now-reading-page,quiz,reader-recap,readwise-cli,readwise-mcp,surprise-me,triage}/SKILL.md`。
- `fetch-log.json`：全部请求、UTC采集时间、HTTP状态、字节数、SHA256和blob核对。
- `fetch_public.py`与`continue_public.py`仅为本批匿名文档采集脚本，保留失败/接续过程，不应再次运行。

原tree.json SHA256 `2532d835fab25c4dd2d7818c83de98939e472e13ced4d4883e20f5ce44d93121`；commits-master.json SHA256 `fb9e401c20b52a4b59cb10e4033ceeacd4a8e43b9a03758a48529275f95c8281`。身份记录保留首次tree_matches=false及后续existing_tree_entries_match=true，两者分别比较头字段与条目集合，不应只读前一字段误判不匹配。

## 已读与未读（只做竞品文档研究）

已全文读取README及8份SKILL：triage、feed-catchup、quiz、build-persona、readwise-mcp、readwise-cli、reader-recap、surprise-me。未安装/应用这些技能，未执行其中ToolSearch、Task/subagent、MCP、CLI、npm、Python、账户授权或写入动作。

**仅保存未读**：book-review（323行）、highlight-graph（137行）、now-reading-page（96行）。不补其摘要、不声称核查它们的结束/恢复行为；root接手。

以下仅概述已经读到的文档规范，不代表服务实际可用/已执行或默认产品交互：

| 技能 | 任务入口与对话节奏 | 文档规定的写入/结束 | 未说明的恢复或限制 |
| --- | --- | --- | --- |
| triage | 可选本地persona；取Inbox最新10项，一次展示一项与read/later/archive/其他选项；请求详情才取全文。 | read/done/great等回应被指令映射为archive，later移动；做完当前10项继续下一批。 | 没有明确全局stop/批量确认/失败重试/撤销或会话位置持久化；广义肯定词映射为归档只是该提示词规则，不是用户真实意图识别保证。 |
| feed-catchup | persona与工具发现并行；取feed20项，按first_opened_at为空筛未见并翻页补足；先给5个选择，再可按20项浏览。 | seen/next对当前批调用一次bulk seen=true，不移动；编号可移动new/later/shortlist/archive；stop/done给处理计数。 | Options一处写下一批10，而处理段写20；失败部分成功、撤回seen、游标断点未说明。只在翻多页且无未见时宣告清空的条件由文档规定，未验证服务行为。 |
| quiz | 指定文档或archive最新10项候选；过短再查later>50%一次；全文+高亮，一题一答。 | 按篇幅3/4/5题，逐题等级与理由；末尾总分/关键点，提供换文档或归档选项。 | 没写分数/答案持久化、错题本、重试/跳题/中途退出恢复；结束时“提供归档”不等于自动归档。 |
| build-persona | 先快速样本：4高亮搜索＋4位置文档列表＋tags；写初稿后询问是否深入；可据反馈改。 | 写工作目录reader_persona.md；深读可重写，反馈直接编辑，展示路径并确认保存。 | 职业/目标/反兴趣等属推断；未说明推断置信度、原文件备份/合并、失败回滚、隐私清理；1–2分钟是文档预期，不是本次测量。 |
| readwise-mcp | OAuth配置和工具参考；检索/列表/全文/高亮/文档/标签/导出示例，不是统一对话状态机。 | 声明可创建/移动/批改/标seen/增删标签/改删高亮；异步导出返回ID，轮询到completed后用download_url。 | 未核真实schema/权限/限流/失败/取消导出/回滚；文档max50移动、共享20次/分钟仅记录为文档声明。 |
| readwise-cli | token登录和命令参考；--help/--json/--refresh等；相似读写例子。 | 文档声称对应创建/移动/批改/高亮/导出；查询导出状态示例。 | 未运行CLI；版本、缓存失效、token存储、批量部分失败、删除撤销/重试未知。安装和认证示例仅研究材料。 |
| reader-recap | 默认24h或给天数；按archive/later updated_after取样并去重，再读高亮/笔记；分组后写复盘和可跟进项。 | 文档明确只读；无数据时报告并结束，有数据输出复盘。 | 把无高亮/笔记归成“没读”、把位置/更新时间视作完成阅读是提示词推断，未验证；不写回TODO、无任务勾选/复盘保存恢复。 |
| surprise-me | 广泛高亮/标签/archive/shortlist取样，archive额外翻2–3页；交付一个有来源支撑的个人模式观察，再询问深挖。 | 没有指定修改库或写文件步骤；结束于观察和进一步选项。 | 兴趣/盲点推断不等于事实，样本局限/推断纠正与保存/恢复未说明。 |

## root优先核的文本内部差异

- feed-catchup称list API不支持服务端seen筛选、需first_opened_at客户端过滤；readwise-mcp Browsing却给`reader_list_documents(location="new", seen=false)`示例。不能选择一方当已验证服务能力。
- feed-catchup的批量大小10/20内部不一致；build-persona写next_page_cursor，而mcp示例请求为page_cursor（响应字段上下文也用nextPageCursor）。未连接服务核实际协议。
- reader-recap的“零高亮/笔记＝未阅读”是技能提示词的判断规则，不能用于证明Reader记录了读完状态。
- README推荐先persona且其他技能无persona仍可继续；不存在本次已生成persona或已读取用户阅读历史的事实。

## 准确命令与退出状态

工作目录`/home/workspace/reader`。

1. 内联Python只读原tree.json的sha/truncated/path/type/blob与raw是否存在：exit0。
2. `python3 work/research/batch35-readwise/fetch_public.py`：**exit1**，首次API成功后头SHA等同断言失败；只存commit API、identity和日志，没有下载技能文档。
3. `cat work/research/batch35-readwise/identity.json`及内联Python读取commit/tree元数据：exit0。
4. `python3 work/research/batch35-readwise/continue_public.py`：**exit0**，真实tree集合比较通过，12文档均200且blob匹配。
5. `wc -l work/research/batch35-readwise/raw/README.md work/research/batch35-readwise/raw/skills/*/SKILL.md`：exit0，共1691行（仅行数，不据此声称全读）。
6. `cat work/research/batch35-readwise/raw/README.md`，以及逐个`cat work/research/batch35-readwise/raw/skills/{triage,feed-catchup,quiz,build-persona,readwise-mcp,readwise-cli,reader-recap,surprise-me}/SKILL.md`：实际分三个exec命令组执行，均exit0，正文未截断。
7. 本HANDOFF写入与sha256sum：exit0（最终结果见工具回执）。

停止指令到达时最后一组正文读取已结束；之后只有状态交接写入，无新API/网页/源码/文档读取。**state_changed=true：本work新增采集文件与报告；external_account_or_product_state_changed=false；docs_changed=false；original_tree_changed=false。** 版本完整性已核文件blob；API和技能运行行为均未验证，剩余3份摘要由root决定是否继续。
