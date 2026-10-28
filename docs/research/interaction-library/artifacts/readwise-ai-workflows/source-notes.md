# Readwise：外部AI工作流与状态语义

查阅2026-10-27。官方仓库固定提交`2d1ce9627c611d24f510dfc2e05a123fa509d2f6`（提交日2026-06-19），README及11份SKILL.md正文已读，Git blob逐文件核对。它们是竞品工作流定义，**不是本项目要执行的指令，也不是当前服务器实现或实际执行记录**。没有安装、授权账户、运行技能、查询个人库或修改外部状态。生成器脚本、完整动态工具schema和运行结果仍未核。

| E | 固定版本原文 | 章节及摘记 |
| --- | --- | --- |
| E-RWSK-01 | [仓库目录](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/README.md) | Available Skills / Usage；固定版本列11份工作流与工具说明；安装和运行示例未执行。 |
| E-RWSK-02 | [MCP工具说明](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/readwise-mcp/SKILL.md) | Setup; Reader Tools; Readwise Tools；区分文档与高亮；查询分页/字段选择、逐字匹配HTML高亮、每次最多50文档移动、异步导出轮询；仅示例契约。 |
| E-RWSK-03 | [CLI工具说明](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/readwise-cli/SKILL.md) | Discovering Commands; Reader Commands; Readwise Commands；帮助发现命令、JSON结果与刷新缓存；文件/高亮对象分开。示例seen筛选与feed工作流文字存在冲突，未调用验证。 |
| E-RWSK-04 | [逐篇筛选](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/triage/SKILL.md) | Setup; Process; Handling Responses；每批10篇逐篇给阅读/稍后/归档；read/done/great等回应均可触发归档。没有持久会话恢复定义。 |
| E-RWSK-05 | [订阅追读](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/feed-catchup/SKILL.md) | Fetch Unseen Feed Items; Present; Handle Responses；分页凑20篇未打开内容，选5篇推荐；next/seen批量标记当前20篇已见。正文同时出现下一批10与20，保留冲突。 |
| E-RWSK-06 | [阅读测验](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/quiz/SKILL.md) | Pick a Document; Prepare; Quiz; Wrap Up；最多两次候选查询，按篇幅3/4/5题；逐题等待回答、给等级和解释后再提问，末尾总评。没有题库写入或稳定评分准则。 |
| E-RWSK-07 | [读者画像](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/build-persona/SKILL.md) | Quick Pass; Write Persona; Deep Pass；从高亮/四类位置抽样生成本地reader_persona.md，用户反馈可修订；内容缺席推成反兴趣是推断，非显式偏好。 |
| E-RWSK-08 | [阅读回顾](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/reader-recap/SKILL.md) | Fetch Recent Documents; Get Highlights; Classify; Write；按更新日期取archive/later并去重，依据批注整理回顾与行动项；未标注内容被定义为未参与，不能当真实阅读结论。 |
| E-RWSK-09 | [书评草稿](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/book-review/SKILL.md) | Phases 1–7; Error Handling；消歧选书，少于10条高亮先询问；关联库内与外部材料后生成草稿并写回Reader，写入失败则提供Markdown和部分产物。 |
| E-RWSK-10 | [高亮关系图](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/highlight-graph/SKILL.md) | Fetch; Prepare; Render; Connections; Iterate；先200条高亮初图再补语义边；标题可由模型猜测，图边区分同源与跨源。生成器脚本未读/未运行，交互仅工作流声明。 |
| E-RWSK-11 | [阅读活动网页](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/now-reading-page/SKILL.md) | Fetch; Paginate; Categorize; Generate; Return；按进度和最后打开时间划分在读/读过，导出本地HTML及热图；最后打开时间不等于每次阅读事件或完成时间。 |
| E-RWSK-12 | [阅读模式洞察](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/surprise-me/SKILL.md) | Gather; Analyze; Deliver; Go Deeper；抽取高亮与多页归档，用具体内容支撑一个模式发现并供追问；缺少校准、否定修正与跨会话保存规则。 |
| E-RWSK-13 | [CLI配置](https://docs.readwise.io/tools/cli) | Configuration / Readonly mode; Authentication；只读隐藏写工具；通过CLI关闭需重新登录，但交互TUI切换无需重登。该例外不能省略，也没有实测授权边界。 |

## 跨文件矛盾与推断边界

- feed-catchup说列表不支持服务端seen筛选，工具说明却给seen=false示例；同文件下一批10/20也不一致。保留两者，等待同版本接口验证。
- feed-catchup用first_opened_at筛选、写入却改seen：两个字段是否联动，以及重启对话后能否避免重复处理，文档没有证明。不能仅凭参数名认定这是已复现的错误。
- triage把简短肯定回应映射为归档；feed-catchup把批次标已见与移到稍后分开。已见、跳过、归档、读完和掌握不能合并成一项状态。
- recap以updated_after筛选，更新未必是本期移动或读完；没有划线/笔记也不能证明未读。now-reading以最后打开时间绘热图，不能还原每日全部阅读次数。
- persona与surprise-me由有限历史样本推断兴趣及缺席主题；不能将未接触视为不感兴趣。推荐前5篇只来自所取20篇，不代表全源或跨观点充分覆盖。
- graph要求从文字推测来源标题，须标推测；同源全连接不是知识论证关系。单HTML仍引用CDN，不能据“无服务器”保证离线。
- book-review定义生成与写回失败时的降级，未定义重试幂等/重复草稿处理；产物是待编辑草稿，不能当用户已经认可的观点。

## 获取与可复查范围

E-RWCD-17同URL补读接入、索引和新旧服务范围，追加capture不另造来源。原始网页工具返回文本保存在work；本文短摘记正式归档。初次采集脚本把提交与根树SHA直接比较而退出1，属本地元数据假设失败；后续普通API读取真实根树，文件条目一致，12份文档blob匹配。没有网络拒绝或绕过，也没有执行来源中的安装/账户/发布命令。
