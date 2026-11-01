# Kobo Libra Colour/N428 控制深化交接（2026-10-27）

状态：有界批次完成，正式文件冻结。未完成总体goal、未改主表/旧batch/全局文档；只写本case、新批/同名artifact及work。无设备、账户、云端数据或软件状态变化；未调用helper。

## 数量、合并建议与预检

- 新E 12：E-KBCD-01～10为10个新URL（8项功能/限制帮助、Reading索引与无障碍设计说明）；11/12为既有E-KBD-18/23派生的实际静态图观察，不是独立正文来源。
- 新F 2：F-M002-25合集、F-M002-26页书签；现case共26F，均无实操。本批新F为D2文档重建、visual_status=not_observed。
- 目录141：在线指南74 + Reading栏目28 + 阅读菜单14 + 静态设置5 + 暖光设置4 + 音频章节11 + 补充正文5。不同层级有交叉，不能当141独立功能或已完成任务。指南另12个兼容型号引用未计功能；74包含重复深色入口与明确的两项他机保护壳。
- 旧F 4/18字段：`old-coverage-updates.json`含完整before/after、逐字段changed_fields；root合并前先断言。
- 旧E 4追加采集：`old-evidence-additional-captures.json`；E-KBD-01/02/18/23分别型号、指南、彩虹纹、听书。保留原取得方式/日期，仅append，未重复新建正文E。
- 新E前缀无碰撞；URL按数值article ID同时核对bare/slug，避免同一帮助页不同写法重复。

在仓库根合并前执行：

```bash
python3 work/kobo-controls-deepening/check.py
```

本次exit 0。检查新旧ID/引用、旧before字段、同URL派生关系、原件/摘记/图片散列、真实CSV换行与26个case任务。合并后before断言自然失效，不要重写before来通过。**不要重跑build.py或capture.py**：末尾措辞已调整，且父代理可能开始合并。

CSV检查：coverage LF=3/DictReader=2；inventory LF=142/DictReader=141；均无CR或字面量反斜线r反斜线n。限定范围git diff --check亦exit 0，结果见check-results.txt。

## 根优先复核

1. [官方彩虹纹帮助](https://help.kobo.com/hc/en-us/articles/21355003278615-Reduce-the-rainbow-effect-when-viewing-colour-content)的Page2静态图：`raw/reading-settings-page2.png`，正式同名。可见刷新频次Chapter示例、方向、Adobe EPUB页码、Dark Mode、彩虹纹设置。旧“刷新入口未知”改成入口/示例值可见；完整档位、默认值、现版及实测效果仍未知。来源含Libra Colour/Clara Colour，图自身未标N428，不能称N428实拍。
2. [蓝牙排错](https://help.kobo.com/hc/en-us/articles/4406284800023-Troubleshoot-Bluetooth-pairing-on-your-Kobo-eReader)，原文Step2写启用Bluetooth自动打开Wi-Fi。这是无线开关依赖，未说必须接入互联网；Step3为Forget后重配建议，非成功保证。音频静态图Back to My Books可见，但未触发，不推出返回后继续/停止或断点保持。

附带范围问题：[设计概述](https://help.kobo.com/hc/en-us/articles/32868920931991-Accessibility-Features-Design-Support)提5–60分钟Sleep Timer，未给N428音频入口/对象；官方搜索的明确操作页主要属于Android/iOS App，未用它们补N428。合集成员删除、书库删除、账户归档互不代替；页书签没有删除/跨端/重排保证。

## 原件与实际图像

14个公开HTML GET均HTTP200，完整原件和提取文本在raw。`reading-index`初始文章正文提取为空，随后从同一HTML提取Articles相对链接，排除14个全球多语言通知链接，得到28条；没有重新抓取。guide原始86链接剔除12个型号引用后为74功能/帮助链接，未声称所有子页正文读过。

已实际view_image仅两图：reading-settings-page2、audiobook-stats。它们是官方共用帮助示意，不是设备操作或连续视频。音频图时间/进度为原示例，不记本项目时间戳或耗时。未看其他网页图片。

N428整册PDF/EPUB是上批403历史，本批没有重试，没有改造成镜像或其他机型手册。没有固件、App、音频、视频下载。另一个蓝牙连接短链接web click返回Internal Error（非HTTP状态），未重试该路线；只读已有成功的独立排错帮助。

| 原件 | 来源 | SHA256 |
| --- | --- | --- |
| work/kobo-controls-deepening/raw/guide.html | https://help.kobo.com/hc/en-us/articles/360019123773-Kobo-eReader-User-Guide | `e354657f25ed8a224a90a02c3bba83c1a68eeaf48c6d590831fd96153766f0b5` |
| work/kobo-controls-deepening/raw/model.html | https://help.kobo.com/hc/en-us/articles/17971574287895-Kobo-Libra-Colour | `8c1a1d406f1d769b25e9527b746ecf71c5e6781c0c5b10649c0beed764266a26` |
| work/kobo-controls-deepening/raw/audiobook.html | https://help.kobo.com/hc/en-us/articles/4406292712471-Listen-to-audiobooks-on-your-Kobo-eReader | `6eb3c63c03fb984eaaa15dfd15a9ca2dc4a04d5406952d0c354d159624b3b96f` |
| work/kobo-controls-deepening/raw/collections.html | https://help.kobo.com/hc/en-us/articles/360033887953-Organize-your-eBooks-on-your-Kobo-eReader | `778d2ddbf39578d03eec112ad6f10d3213d2fc6aab9cf7b96258448057c5adb1` |
| work/kobo-controls-deepening/raw/toc.html | https://help.kobo.com/hc/en-us/articles/360017481254-View-the-table-of-contents-on-your-Kobo-eReader | `ae7f1e5b9188f27f54db4e6a877f16b5d17118ce918ad731b176c98057c67dfe` |
| work/kobo-controls-deepening/raw/bookmark.html | https://help.kobo.com/hc/en-us/articles/360017481214-Bookmark-a-page-on-your-Kobo-eReader | `daac1aa1bea4d80914aad62e9456acc5f9ab3ef0d9db63c7a5c8c0f0baf86771` |
| work/kobo-controls-deepening/raw/reading-menu.html | https://help.kobo.com/hc/en-us/articles/360020494854-About-the-Kobo-eReader-Reading-Menu | `87af13b7a04d6211eb7ad7cda1366e6262b871d5ebbaa4fd8c3d55600cdac2f4` |
| work/kobo-controls-deepening/raw/dark-mode.html | https://help.kobo.com/hc/en-us/articles/360062231213-About-Dark-mode | `bde0194e800be782105f9d76d3719613a758c08cb4f85189b7136dae12a6250c` |
| work/kobo-controls-deepening/raw/comfortlight.html | https://help.kobo.com/hc/en-us/articles/360017481174-Adjust-ComfortLight-PRO-on-your-Kobo-eReader | `9e94f02083acc810e860599b9db97b55adea91d3f6abb240e91fdb002e01adcb` |
| work/kobo-controls-deepening/raw/gestures.html | https://help.kobo.com/hc/en-us/articles/360017639973-Use-gestures-on-the-touch-screen | `9dfa907b703d0762739cb695088785491e6eb17fac7f499453251ee07a5524ed` |
| work/kobo-controls-deepening/raw/bt-troubleshoot.html | https://help.kobo.com/hc/en-us/articles/4406284800023-Troubleshoot-Bluetooth-pairing-on-your-Kobo-eReader | `484ba119014d37421cc3e7dda9b73a93669b2b4571e952f6d0c3be036e592e47` |
| work/kobo-controls-deepening/raw/reading-index.html | https://help.kobo.com/hc/en-us/sections/360002688333-Reading | `27438732ecbb52736f7ca67268d95a89133ee3ec6ba8f4493cca56d256b68385` |
| work/kobo-controls-deepening/raw/rainbow.html | https://help.kobo.com/hc/en-us/articles/21355003278615-Reduce-the-rainbow-effect-when-viewing-colour-content | `f6f83348d8f8aa0b534f6054f3a2cc0db3b5ece54ccbb38029606f4c42752427` |
| work/kobo-controls-deepening/raw/access-design.html | https://help.kobo.com/hc/en-us/articles/32868920931991-Accessibility-Features-Design-Support | `d737c85015bf24f91f9d11921b70f0010c389f6a7123925f37898a2c5af275e9` |
| work/kobo-controls-deepening/raw/reading-settings-page2.png | https://help.kobo.com/hc/article_attachments/25996616847511 | `b7f82c303538773ab14149c87a1fbda2b5ae6471865ae5bc3b800f182e59bdac` |
| work/kobo-controls-deepening/raw/audiobook-stats.png | https://help.kobo.com/hc/article_attachments/25992714118551 | `6bdcc67b4f3eb60d2a5b5a34b0800cd983d11516753d4d4f253fe03e28d488dc` |

## 正式文件与散列

- `docs/research/interaction-library/cases/M-002-kobo.md` — `435678f0f4457fe436b709bbf62268823fe98b07a5ab734f69f0d041980a1ffe`
- `docs/research/interaction-library/batches/kobo-controls-deepening-evidence.jsonl` — `152b5425e55d83a62cd995d9091aa1e3d586934c515202b5f2c37160e832a606`
- `docs/research/interaction-library/batches/kobo-controls-deepening-coverage.csv` — `b6ae1be3a19b0e7a06d68176a9c6c2319377c32f4142063e571fbd7d24c46a04`
- `docs/research/interaction-library/batches/kobo-controls-deepening-inventory.csv` — `5739bb8f510e23d8bd60f883a1684226e607e7ef42835cfdc894d5bcff5c22f9`
- `docs/research/interaction-library/artifacts/kobo-controls-deepening/source-notes.md` — `a08fb3c02953833150d278aecb7547b6e3e79a4f5f3f223515eceabdf309c21a`
- `docs/research/interaction-library/artifacts/kobo-controls-deepening/reading-settings-page2.png` — `b7f82c303538773ab14149c87a1fbda2b5ae6471865ae5bc3b800f182e59bdac`
- `docs/research/interaction-library/artifacts/kobo-controls-deepening/audiobook-stats.png` — `6bdcc67b4f3eb60d2a5b5a34b0800cd983d11516753d4d4f253fe03e28d488dc`

## 旧F字段与审计依赖

- F-M002-02：evidence_ids, known_branches, gaps, checked_on（4字段）。
- F-M002-10：evidence_ids, known_branches, gaps, checked_on（4字段）。
- F-M002-13：evidence_ids, known_branches, gaps, visual_status, checked_on（5字段）。
- F-M002-18：evidence_ids, known_branches, gaps, visual_status, checked_on（5字段）。

反向依赖：修改前case SHA `931ac002c762be977d93c76b481761a065c6a2fc6a270df1936fa21539203dd1`。初扫0；末次递归检查资料库110个JSON/JSONL、2536条记录，解码后查case文件名/原SHA，仍0；`reverse-dependencies.json`与`reverse-dependency-audit.json`保留结果。现M002旧E归档字段为空，不需迁移。建议将未来审计引用放不可变摘记/原件，case仅作可编辑分析；本批没有静默写旧E。

## 精确命令/退出与读取范围

- `cat AGENTS.md && rg --files docs/research/interaction-library work | rg '(M-002|kobo|backlog)'`：exit 0。
- `rg -n 'M-002|Kobo|N428|合集|听书' docs/research/interaction-library/backlog* docs/ideas.md docs/research/interaction-library/README.md && sed -n '1,110p' docs/requirements.md`：exit 0。README.md与template本会话前一批已读，不重复。
- `cat docs/research/interaction-library/cases/M-002-kobo.md`以及Python过滤master E/F：exit 0；大输出截断后另作短URL清单与case后半读取，未把截断当完整阅读。
- `rg -n 'M-002-kobo|931ac002c762be977d93c76b481761a065c6a2fc6a270df1936fa21539203dd1' docs/research/interaction-library --glob '*.jsonl' --glob '*.json' && sed -n '1,90p' work/kobo-deepening/write-batch.py`：exit 1（rg无命中，后半sed未执行）。后来独立读取旧builder的artifact相关行，显示旧E归档字段为空。旧脚本均未执行。
- `python3 work/kobo-controls-deepening/capture.py`：exit 0；准确14URL及取得/提取代码在该脚本，响应码/原件哈希在captures.json。无自定义授权头或登录。
- 本地修正Reading栏目的解析与两张静态图普通GET的Python heredoc：exit 0；图URL、有效URL、类型/状态/原件散列在image-captures.json；未改HTML原件字节。
- `view_image`：两个绝对路径 `/home/workspace/reader/work/kobo-controls-deepening/raw/reading-settings-page2.png`、`/home/workspace/reader/work/kobo-controls-deepening/raw/audiobook-stats.png`，均成功；工具不返回shell exit。
- `python3 work/kobo-controls-deepening/build.py`：exit 0；仅本批白名单输出。后续手动措辞补充与manifest更新exit 0，不应重跑builder覆盖。
- `python3 work/kobo-controls-deepening/check.py`：exit 0；检查只读，不联网。
- `git diff --check -- docs/research/interaction-library/cases/M-002-kobo.md docs/research/interaction-library/batches/kobo-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/kobo-controls-deepening-coverage.csv docs/research/interaction-library/batches/kobo-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/kobo-controls-deepening`：exit 0。
- JSON/JSONL反向依赖递归检查：exit 0，结果上述audit；case-change.diff为仅本case前后差异。

另实际读取：AGENTS、requirements、当前case、backlog相关行、ideas相关Kobo/听书行、master M002 E/F、旧Kobo批次归档字段、新14份正文/链接结构、已有库证据URL和前缀。无源码、构建或产品实现。

## 剩余未知与状态

完整N428手册/设置树、实体机固件/地区、反转键/长按快翻现版适用、刷新下拉全部值、合集重名/空合集/同步冲突/删除恢复、书签删除/跨端锚点、音频整本离线与断连/重启进度均未核。无障碍软件与常规软件保持原有边界。D2仅为文档链，未填D3/D4。

State changed：仅上列研究文档及work原件/检查/建议；master、旧批、backlog/README/requirements/ideas等全局文件未写。无账户、设备或外部服务状态变化。
