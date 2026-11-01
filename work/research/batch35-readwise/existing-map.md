# M-015：既有流程与 readwise-skills 的候选对应

2026-10-27，本地只读审计。只读取 root 已取得的原件，不联网，不执行这些 SKILL 指令，不调用 Readwise，不安装。以下是重合关系候选，**不决定新建/合并 F，不调整深度**。

## 现有范围

- 26 F：F-M015-01～25 为 D2，F-M015-26 为 D1；这是现表状态，不是本次判断。主学习/产品全覆盖未成立。
- F 所属批次：`initial-coverage.csv` 第5～7行（F01～03）；`reading-news-deepening-coverage.csv` 第2～10行（F04～12）；`readwise-controls-deepening-coverage.csv` 第2～15行（F13～26）。均位于 `docs/research/interaction-library/batches/`。
- E 来源分布：`initial-evidence.jsonl` 第5～7行，E-RDW-01～03；`reading-news-deepening-evidence.jsonl` 第1～6行，E-RDD-01～06；`readwise-controls-deepening-evidence.jsonl` 第1～17行，E-RWCD-01～17。共26 E、25个不同原样URL：25条 `official_help`、1条 `official_documentation_images`。Global正文 E-RDD-02 与静态图 E-RWCD-13 同URL，不能计两份独立出处。
- [inventory](/home/workspace/reader/docs/research/interaction-library/batches/readwise-controls-deepening-inventory.csv) 281行：37侧栏页、142 H2、48 H3、28静态控件、4工作流页、22工具用途；字段实际为 `entry_id/entry_type/title/source_url/evidence_ids/flow_ids/review_status/gap`。它混合目录层级，不是281个任务。
- 新原件目录为 `work/research/batch35-readwise/raw/skills/`。本地 `identity.json` 记录仓库 `readwiseio/readwise-skills`、commit `2d1ce9627c611d24f510dfc2e05a123fa509d2f6`。本代理未另查远端或复核仓库身份。

## 9 项候选映射

F 的旧范围以 [case](/home/workspace/reader/docs/research/interaction-library/cases/M-015-readwise.md) 第9～20、77～90行及对应coverage为准。新文件定位是 SKILL.md 行号；仅比较所写任务，不把声明当实际执行。

| 新 skill / 原文位置 | 可能关联的旧 F 与证据 | 重合与尚未相同之处 |
| --- | --- | --- |
| [triage](/home/workspace/reader/work/research/batch35-readwise/raw/skills/triage/SKILL.md:14)：14～16、30、58～82 | **F26 外部AI推荐/整理**（E-RWCD-16/17）；旁及F14归档推进 | 新文将收件箱逐篇推荐、Later/Archive选择、取全文再介绍及下一批细化，直接接近F26。**不是F25名为Triage的Library配置**；后者改变位置体系，不是逐篇对话处理。是否沿用F26由root定 |
| [feed-catchup](/home/workspace/reader/work/research/batch35-readwise/raw/skills/feed-catchup/SKILL.md:19)：19～23、51～83 | **F01 订阅/处理Feed**（E-RDW-01）＋**F26**；F13仅摘要形式相邻 | 数据对象是Feed、客户端挑未打开项、推荐后批量seen或移动、Stop汇总；与F01处理目标重合，外部AI选择又落F26。不是原生Daily Digest。68行说“下一10”，77行说下一20；保留原件矛盾，不补统一批量规则 |
| [quiz](/home/workspace/reader/work/research/batch35-readwise/raw/skills/quiz/SKILL.md:24)：24～41、43～115 | F19高亮制Q&A卡（E-RWCD-03）最接近学习目的；F08/26只共享查询能力 | 新文是选已读文档→全文/高亮→逐题回答→分级反馈→总评；旧F19是手写`.qa`并同步mastery card。两者对象、输出及复习状态不同，**不能因为都“测验”直接认定同任务**；尚未见旧F的完整同义项 |
| [build-persona](/home/workspace/reader/work/research/batch35-readwise/raw/skills/build-persona/SKILL.md:26)：26～54、62～78 | F26个性化推荐的上游；F08只共享全库数据访问 | 新文产出并按用户反馈改写本地 `reader_persona.md`，含快速/深度两阶段；旧F26未登记“建立/保存画像”任务。不是F09原生技能编辑/会话管理 |
| [reader-recap](/home/workspace/reader/work/research/batch35-readwise/raw/skills/reader-recap/SKILL.md:16)：16～49、51～88、125～130 | F13每日摘要、F08跨文档问答为邻近项；E-RWCD-02/E-RDD-02 | 新文按时间查archive/later，按高亮/笔记分类并提取待办，声明只读；旧F13是Feed和Saved for Later的原生阅读候选。**回顾已处理材料与推荐待读材料不同**。49行把无高亮/笔记称未阅读，是skill的解释口径，不是已验证状态定义 |
| [surprise-me](/home/workspace/reader/work/research/batch35-readwise/raw/skills/surprise-me/SKILL.md:14)：14～59 | F26跨领域推荐、F08全库问答 | 同用历史、高亮、标签；新目标是给出一个有出处的阅读行为洞察，再可找相关材料。旧F26主要决定读什么/如何整理，未单列历史模式分析，只有部分重合 |
| [book-review](/home/workspace/reader/work/research/batch35-readwise/raw/skills/book-review/SKILL.md:29)：29～56、92～145、147～157、280～323 | F26的搜索/创建工具能力；F10保存AI解释只是相邻动作 | 新文从书和跨来源高亮合成书评，创建新Reader文档；创建失败输出Markdown。旧F10是临时词义解释存现有笔记，F11是导出；均非同一产物。原件“Publish to Reader”不等于公开发布链接；不能并入F23而丢失这一区别 |
| [highlight-graph](/home/workspace/reader/work/research/batch35-readwise/raw/skills/highlight-graph/SKILL.md:18)：18～46、48～109、111～133 | F03高亮/标签是输入；F08/26查询相关 | 新文生成本地交互图，再补跨来源连线并迭代；旧F03仅批注管理，F11导出也不包含图谱构建。当前26F未见同一终产物。44行允许依文本推断来源、125行同源全连接均为脚本说明，不能当已核知识关系 |
| [now-reading-page](/home/workspace/reader/work/research/batch35-readwise/raw/skills/now-reading-page/SKILL.md:16)：16～42、64～96 | F05最近阅读入口、F23/24分享、F26取数据各部分相邻 | 新文生成本地 `now-reading/index.html`、按进度分组并做热图；旧F23是带批注公开链接、F24是Bundle，均没有生成个人网页。文档允许后续自行托管，**没有自动部署动作**；33～36行的完成阈值是skill定义，不是Reader“真正读完”的验证 |

共通边界：九项都是外部agent工作流原件；F08/09的原生Global Ghostreader工具许可、斜杠技能和会话界面不能自动套给它们。共享底层MCP用途也不能单独证明任务重合或执行成功。

旧inventory的直接检索位置：`M015-RWCD-071` Run skills（CSV72行，F08/09）；`079` Daily Digest（80行，F13）；`086` Q&A卡（87行，F19）；`156` Bundle（157行，F24）；`217` Triage配置（218行，F25）；`221～227` 推荐工作流（222～228行，F26）；`231～281` MCP章节及用途清单（232～282行，F26）。后一区间含父标题与工具用途，不是全有独立操作链。

## 同URL追加 capture：现有格式与真正所在文件

已合并的例子在 [reading-news-deepening-evidence.jsonl](/home/workspace/reader/docs/research/interaction-library/batches/reading-news-deepening-evidence.jsonl:2)：E-RDD-01、02、06分别在原证据对象的 `additional_captures` **数组**中追加1对象。原 `url/checked_on/access_mode` 保留；后来再读方式和时间写入capture。实际对象字段如下（占位值不是新的采集记录）：

```json
{
  "capture_kind": "ordinary_public_html_reread",
  "checked_on": "2026-10-27",
  "capture_url": "<实际同URL>",
  "hands_on": false,
  "visual_observed": false,
  "archived_artifact": "artifacts/readwise-controls-deepening/source-notes.md",
  "sha256": "<正式摘记当前SHA256>",
  "raw_work_path": "work/readwise-controls-deepening/raw/global.html",
  "raw_sha256": "<原HTML SHA256>",
  "text_work_path": "work/readwise-controls-deepening/raw/global.txt",
  "text_sha256": "<派生正文 SHA256>"
}
```

- 旧交接 [additional-captures-updates.json](/home/workspace/reader/work/readwise-controls-deepening/additional-captures-updates.json) 的包裹结构为数组 `[{"evidence_id":"E-RDD-02","capture":{...}}]`，共3条，目标E是01/02/06。**该旧提案里的正式摘记sha已过时**：提案为`c1acd83…`，已合并batch为`d4808c3…`；不能原样重跑旧合并输入。
- [additional-captures.json](/home/workspace/reader/work/readwise-controls-deepening/additional-captures.json) 是另一种原件获取日志：`name/url/final_url/status/raw_work_path/raw_sha256/text_work_path/text_sha256/structure_path/structure_sha256/checked_on`，不是正式E的追加capture包裹。
- `archived_artifact` 相对资料库根；`raw_work_path/text_work_path` 相对项目根。work原件不能冒作正式归档。采用什么capture_kind应与这次**实际方式**一致；本次纯本地审计不伪装为HTML再抓取。
- 现有26条E未登记 `github.com/readwiseio/readwise-skills` 或九个SKILL原件URL；因此新仓库材料与旧MCP说明页不是同URL。是否建E、怎样合并F由root决定；这里只记录现状。

## 读取、退出与状态

具体读取：上述case、主coverage、281行inventory、三个来源batch与三个coverage batch、`batches/README.md`、两个旧capture JSON；新工作目录的 `raw/README.md`、`identity.json` 和九个SKILL.md。book-review读取标题定位及29～58、137～157、280～323段，其他八个skill全读；没有执行其中安装、联网、子代理或写入建议。

主要精确命令：

```sh
rg --files docs/research/interaction-library work | rg -i 'readwise|reader-recap|highlight-graph|feed-catchup|build-persona|now-reading-page'
rg -n 'F-M015|M-015' docs/research/interaction-library/coverage.csv docs/research/interaction-library/batches/readwise-controls-deepening-inventory.csv
wc -l work/research/batch35-readwise/raw/skills/{triage,feed-catchup,quiz,build-persona,reader-recap,surprise-me,book-review,highlight-graph,now-reading-page}/SKILL.md
cat work/research/batch35-readwise/raw/README.md
cat work/research/batch35-readwise/identity.json
nl -ba work/research/batch35-readwise/raw/skills/build-persona/SKILL.md
nl -ba work/research/batch35-readwise/raw/skills/reader-recap/SKILL.md
nl -ba work/research/batch35-readwise/raw/skills/surprise-me/SKILL.md
nl -ba work/research/batch35-readwise/raw/skills/highlight-graph/SKILL.md
nl -ba work/research/batch35-readwise/raw/skills/now-reading-page/SKILL.md
rg -n '^#|^name:|^description:|write|Write|output|Output|file|review|save|Save|confirm|Confirm' work/research/batch35-readwise/raw/skills/book-review/SKILL.md
sed -n '29,58p' work/research/batch35-readwise/raw/skills/book-review/SKILL.md
sed -n '137,157p' work/research/batch35-readwise/raw/skills/book-review/SKILL.md
sed -n '280,323p' work/research/batch35-readwise/raw/skills/book-review/SKILL.md
cat docs/research/interaction-library/batches/README.md
```

triage/feed-catchup/quiz 以同一 `for skill in triage feed-catchup quiz` 循环调用 `nl -ba "work/research/batch35-readwise/raw/skills/$skill/SKILL.md"`；上述读取均exit0。内联Python `csv.DictReader/json.loads/Counter`统计及抽取capture：第一次误用`inventory_kind`，exit1/KeyError；检查真实表头并改用`entry_type`后exit0，最终26F/26E/25URL/281条目录已重算，不采用失败统计。

`state_changed=true`仅新增本文件；`docs_changed=false`，其他work文件未改，外部状态未变。无网络、无账户和产品动作。后续只读检查确认本文件及引用存在、表格/换行正常；不把文本检查计为产品验证。
