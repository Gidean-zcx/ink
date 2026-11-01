# Batch43 新闻邻近对象本地来源盘点

日期：2026-10-27。仅本地只读核对 M-043、M-045、M-047 的 catalog、F/E、inventory 和正式来源记录；未联网，未修改 `docs/`，未重复分析 M-045 案例正文。

## 对象确认

- M-043：**iOS / iPadOS 快捷指令**，系统动作与AI研学邻近对象。
- M-045：**The Economist Espresso**，成人新闻简报。
- M-047：**TIME for Kids**，儿童新闻。

当前正式任务：M-043 有12F（均D2），M-045有5F（4D2/1D1），M-047有5F（3D4/2D2）。

## TIME旧目录中尚未实际进入的官方 URL（12个）

根代理正在另行调查2026-05-08 K–1公开样刊及一篇新G3文章，本表不重复这些路线。以下12项均来自正式 `time-vocabulary-deepening-inventory.csv`：已有菜单链接/标签证据，但除K–1 Science外没有实际进入目标页，也没有读取目标页正文。

| # | URL | 原来源编号 / 目录行 | 已有状态 | 下一步可核范围 |
| --- | --- | --- | --- | --- |
| 1 | https://www.timeforkids.com/g2/ | E-TKVD-02 / F-M047-01 / M047-TKVD-003 | 链接与标签已核；未进入 | Grade 2目录结构、阅读/音频/西语标识是否与K–1一致 |
| 2 | https://www.timeforkids.com/g56/ | E-TKVD-02 / F-M047-01 / M047-TKVD-005 | 链接与标签已核；未进入 | G5–6目录、阅读层级与成人/儿童边界 |
| 3 | https://www.timeforkids.com/k1/topics/animals/ | E-TKVD-02 / F-M047-01 / M047-TKVD-007 | 只发现目录链接 | 儿童常见主题的卡片、分页与内容工具 |
| 4 | https://www.timeforkids.com/k1/topics/books/ | E-TKVD-02 / F-M047-01 / M047-TKVD-010 | 只发现目录链接 | 书籍类内容与普通新闻文章的入口/读听差异 |
| 5 | https://www.timeforkids.com/k1/topics/debate/ | E-TKVD-02 / F-M047-01 / M047-TKVD-015 | 只发现目录链接 | K–1是否实际有观点对照内容、空目录或年龄门槛 |
| 6 | https://www.timeforkids.com/k1/topics/environment/ | E-TKVD-02 / F-M047-01 / M047-TKVD-020 | 只发现目录链接 | 环境主题的分页、音频/西语标识与文章新旧范围 |
| 7 | https://www.timeforkids.com/k1/topics/news/ | E-TKVD-02 / F-M047-01 / M047-TKVD-030 | 只发现目录链接 | “News”专题与K–1总目录的差异、编辑筛选边界 |
| 8 | https://www.timeforkids.com/k1/topics/podcasts/ | E-TKVD-02 / F-M047-01 / M047-TKVD-033 | 只发现目录链接 | Podcast是否公开、播放器对象、退出/恢复及是否登录限制 |
| 9 | https://www.timeforkids.com/k1/topics/world/ | E-TKVD-02 / F-M047-01 / M047-TKVD-042 | 只发现目录链接 | 世界新闻覆盖、来源背景与分页范围 |
| 10 | https://www.timeforkids.com/partner/pwc/g4/ | E-TKVD-02 / F-M047-01 / M047-TKVD-045 | 菜单“Your $ Financial Literacy Content”下仅核链接 | 合作专题是否公开、年级和品牌边界、是否属于主新闻体验 |
| 11 | https://www.timeforkids.com/partner/pwc/g56/ | E-TKVD-02 / F-M047-01 / M047-TKVD-046 | 同上，仅核链接 | G5–6金融素养内容与主目录差异；不能自动归入K–1 |
| 12 | https://www.timeforkids.com/?age-gate=1 | E-TKVD-02 / F-M047-01 / M047-TKVD-048 | 已从菜单进入成人门并看到出生年月表单，但未提交 | 成人资源门的取消/返回、资格与隐私边界；未经授权不提交表单 |

其他K–1主题入口同样处于“目录发现、未实际进入”，正式台账编号 M047-TKVD-008～043。这里选择能覆盖年级差异、主题编辑、音频、观点/世界新闻、合作内容和成人门的12个代表项，不把35个近义目录全列为优先来源。

### 其他两对象的范围事实

- **M-043快捷指令不是新闻产品。** 其iOS27目录仍有93个`directory_only`节点，可用于以后研究新闻抓取/筛选/通知的系统动作，但本轮不与TIME内容入口混列。已读的官方正文没有产品实操；真实运行需要Apple设备、系统版本、权限与App动作。
- **M-045 Espresso没有发现新的独立公开App操作入口。** HKU指南、2017作者记录、App Store版本窗口和Pro API边界已读；I-ESCD-056～061是缺口行而非新URL。Economist robots限制及旧ACG失败路线不重试。

## 已有可操作的公开网页入口

| 产品 | 入口 | 已有操作范围 |
| --- | --- | --- |
| M-047 | https://www.timeforkids.com/k1/spotlight-on-plants-k1/ | E-TKCD-01：匿名公开文章，展开/暂停Read Aloud、语速、进度、关闭重开、刷新对照、搜索入口；未实际听音 |
| M-047 | https://www.timeforkids.com/k1/spotlight-on-plants-k1/?lang=es | E-TKCD-02：英语/西语页面切换及西语朗读控件；未核声音 |
| M-047 | https://www.timeforkids.com/k1/?text=plants | E-TKCD-03：匿名搜索层、输入/清空及返回文章；无意义词仍返回普通文章，不能称零结果正确 |
| M-047 | https://www.timeforkids.com/k1/topics/science/ | E-TKVD-02：年级→K–1→Science→分页→浏览器返回及成人资源门；不能泛化全部主题 |
| M-047 | https://www.timeforkids.com/g34/food-for-thought/?rl=en-760 | E-TKVD-01：公开G3–4词汇卡的打开、关闭、推进、暂停和刷新；未实际听音 |

M-043当前只有公开帮助正文，没有可在本环境运行的Shortcuts Web产品；真实动作需要对应Apple设备、系统版本、权限和可用App动作。M-045当前只有公开网页/商店/机构指南，没有已登记的匿名Espresso Web阅读器操作入口。

## 既有失败、登录和资格限制

- **M-043**：27份既有官方帮助HTTP请求均200；旧模型路径曾重定向欢迎页，已排除为模型流程证据。没有安装iOS/iPadOS 27、登录Apple账号或操作Shortcuts；Apple Intelligence另受设备、地区、网络和额度条件限制。
- **M-045**：Economist主站存在robots限制，既有研究未绕过；旧ACG机构指南直开失败路线未重试。HKU指南要求学生自行建账号且该路径不选SSO；本项目没有注册、登录、订阅或运行App。相邻Economist Pro API需要合同授权，不是Espresso个人控制接口，未申请Key或认证。
- **M-047**：公开文章可匿名操作；教师/成人资源门曾显示出生年月表单，本次既有操作未提交。没有登录教师账号、订阅、打印或提交课堂作业。Read Aloud操作中有一次音频请求`ERR_ABORTED`及若干页面Load error，原因未定位；不能复用为确定故障路线。成人订阅、答案指南和精确跨端恢复仍受权限/资料缺口限制。

## 执行记录

1. Python读取 `catalog.csv` 中 M-043/M-045/M-047：exit 0，确认对象名称、类别、案例和seed URL。
2. `rg --files`定位三对象的case、coverage、evidence、inventory、source-notes和操作审计：exit 0；清单保存为 `file-list.txt`。
3. Python读取两个现有inventory并统计 `review_status`：exit 0。Shortcuts为93条`directory_only`、21条正文已读；Espresso为25版本记录、16静态项、6历史图/报告、4正文、4排除边界、6 unresolved。
4. Python读取master `coverage.csv`/`evidence.jsonl`，列三对象F/E、URL、capture状态、hands_on与视觉状态：exit 0。
5. `cat`/`sed`读取Shortcuts、Espresso正式source-notes与TIME浏览器操作审计：exit 0；未重读M045案例正文。
6. Python定点筛Shortcuts `directory_only`主题的Siri、其他App、隐私、流程、完成、顺序、测试和Web API URL：exit 0。
7. 写入本报告并计算SHA-256：exit 0。

## 不确定性与状态变化

- 本轮未联网，无法确认12个候选此刻是否仍返回相同正文、是否新增登录墙或已改版。
- “值得补的范围”只依据目录标题、既有相邻证据和当前缺口，不表示正文必然包含完整步骤。
- M047没有独立功能inventory；其“原目录行”只能以稳定E/F和正式审计定位，不能补造目录ID。
- 状态变化仅新增 `work/research/batch43-news-inventory/file-list.txt` 与本报告；未修改 `docs/`，未复用或重试任何失败路线。
