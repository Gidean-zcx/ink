# M049 本轮文档目录草案说明

2026-10-27；只读五个已有文本，本次未联网、看图、登录或执行互动。`document-inventory.csv`只供root合并，不新增F/E，也不代表任务完成。旧源沿用E-BRIT-02/03/04；两checklist只用source_key，URL/E留空待root核配。

草案146行：来源分组{'school_guide': 53, 'early_guide': 39, 'onboarding': 18, 'early_checklist': 16, 'school_checklist': 20}；类型{'chapter': 9, 'feature': 43, 'control_or_subfeature': 34, 'faq_question': 18, 'checklist_action': 36, 'research_gap': 6}。数量包括父章节、子控件、FAQ和重复来源的自查动作，不能相加当已完成任务。Early原11条（7章+4模式）不再复制；细项的parent_entry指向I-M049-E01–E11。主指南章节父子及清单重复由parent_entry/overlap_note标明。F01–06仅主题对应，context_only不是完整任务映射；partial_scope_existing_flow仅关联旧任务原有学段，不扩为整行所列全部学段；空flow_ids加unmapped明确未映射。

关键边界：

- 主指南9章按p2目录及正文标题定位；p7抽取只有标题，本次没有看图，不补各级按钮矩阵。p8–9所有Library措辞/条件保留，Lexile、ImageQuest、Advanced Search不普遍套给School。
- Early视频下载/字幕/描述音轨及画板擦除/重置/打印只是文字功能。下载不保证全产品离线；打印不证明云保存或可重新编辑；Fundamentals需订阅启用。
- FAQ共18题，行号按本轮onboarding.txt精确定位。分享权限、个人账号/教师角色、个人阅读级别、Middle/High原始资料与电子书库、可能性故障解释均保留。
- 两张checklist全部动作已归档；复合找寻项保留集合，未为控件数量拆细。教师分发scavenger hunts、Read/Play/Connect、分层讲义、Exit Tickets不当软件评分/自动课程。F06只作教师组织背景关联，没有从这些材料补写回收主题具体规则。
- 明确6条研究缺口，与正文/目录不同；本草案不含root正在进行的公开互动实际观察。

输入快照（只计算已有文本hash，不重新抓取）：

| source_key | 文本 | SHA-256 |
| --- | --- | --- |
| school_guide | `work/news-interactions/brit-school.txt` | `093b1fa2288a8888d2721521ab370b8f08cb4c942a90c898b2cfad014a22d365` |
| early_guide | `work/news-interactions/brit-early.txt` | `ae378a1c75c1131a1bf13ea71a4cf08918194900aceaaf3096e39efaa65a4890` |
| onboarding | `work/britannica-controls-deepening/raw/onboarding.txt` | `a3cacaeb7b31fc6b2f58cc7984e80da436d759789f28b16e4e54abeba0910d01` |
| early_checklist | `work/britannica-controls-deepening/raw/early-checklist.txt` | `14a64740645d2303cb632108418b39ee7286d8e3194f3e67365ff4436cf76546` |
| school_checklist | `work/britannica-controls-deepening/raw/school-checklist.txt` | `a1f051ae8177bdc9738bad44b318b63d7f83b3d66fa94ad5f9e966c64f511ba2` |

输出CSV SHA-256：`6b392477b9966cc6f9fd6a3fb685362daa00349fc5c3de35424f4d7b2dbc295c`。

实际读取命令与状态：`cat work/news-interactions/brit-school.txt`（exit0）；`cat work/news-interactions/brit-early.txt && cat work/britannica-controls-deepening/raw/onboarding.txt work/britannica-controls-deepening/raw/early-checklist.txt work/britannica-controls-deepening/raw/school-checklist.txt`（exit0；合并工具输出截断，随后补读）；`sed -n '/PDF PAGE 2/,$p' work/news-interactions/brit-early.txt`（exit0）；`nl -ba work/britannica-controls-deepening/raw/onboarding.txt`（exit0）；`sed -n '200,262p' work/britannica-controls-deepening/raw/onboarding.txt`（exit0）。另只读旧coverage/evidence中M049六F与E-BRIT-02/03/04、case范围及`inventories/M049-early-guide-inventory.csv`用于映射，不作为新增来源。

内联Python生成与检查仅写本目录`document-inventory.csv`和本说明，exit0。没有修改case、旧批、master、全局文档、root原件/浏览器审计，账号/设备状态未变。
