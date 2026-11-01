# M004 第28批：阅读与笔记交接

2026-10-27。范围收束为10篇既有官方正文和4张新静态图。对象仍是Note Air5 C / Android 15 / V4.1.2功能基线；实际构建、该机该版本专用手册未知。没有将2026年编辑的通用帮助全部当作V4.1.2已验证功能。正式文件已冻结；不得重跑本批生成器或任何旧批脚本。

## 交付与状态

- 13新E：E-BXRD-01～09为9篇此前目录级正文的派生记录，来源仍是旧分类API快照E-BXCD-01；E-BXRD-10～13为4张实际查看的官方静态图。
- 5新D2 F-M004-15～19：书库视图与最近文件、阅读版式与局部控制、阅读统计、标签定位、编辑已有Text Notes及导出。全为hands_on=false，没有D3/D4或测时。
- 2旧F / 8字段建议：F09的页面/图层/对象与撤销边界，F10的识别/录音/Infinite Notes边界。
- 2旧E追加建议：E-BOOX-16本地完整正文复读及两张相关静态图；E-BXCD-01既有59篇目录与10篇选读范围复用。没有重新获取正文，原采集日期仍2026-10-26。
- 目录127行＝59个原文章节点＋68个正文标题/控件组；14节不变。父子包含关系明确，不是127个功能或完成任务。只完整阅读选定10篇，未扩展到全部59篇。
- 状态仅改变M004案例与本批研究文件，没有改master、旧batch、README/需求/ideas等全局文档，没有设备、账号、安装、同步、外部消息或产品代码动作。

## 确切文件

正式10文件（逐文件SHA见formal-manifest.json）：

1. `docs/research/interaction-library/cases/M-004-boox-neoreader.md`
2. `docs/research/interaction-library/batches/boox-reading-controls-deepening-evidence.jsonl`
3. `docs/research/interaction-library/batches/boox-reading-controls-deepening-coverage.csv`
4. `docs/research/interaction-library/batches/boox-reading-controls-deepening-inventory.csv`
5. `docs/research/interaction-library/artifacts/boox-reading-controls-deepening/source-notes.md`
6. 同目录`observed-images.json`
7. 同目录`library-scan.jpg`
8. 同目录`notes-page-manager.png`
9. 同目录`notes-layers.png`
10. 同目录`epub-more-settings.png`

本work新增：case-before.md、baseline.json、38旧E/14旧F快照、原59项目录副本与解析JSON、article-index.json、raw内10篇原文章JSON/抽取TXT和4图片、image-fetch.json、summary.json、3份旧行建议、formal/raw manifest、precheck.py、3个单次提取/获取/生成脚本与本HANDOFF。

## 原件与版本边界

正文原件：`work/boox-controls-deepening/epaper-pc-articles.json`，59篇，next_page=null。SHA256：`02e30182beee444963ce691bbec9523380dcf91000c32685e27bea33f08a2cd8`。

历史获取URL：<https://booxsupport.zendesk.com/api/v2/help_center/en-us/categories/38115205868180/articles.json?per_page=100>。此前HTTP200，本批只本地读取，不报告为本批HTTP200。每篇原JSON派生到本work/raw/{article_id}.json，完整抽取文字为同名.txt，原件与派生共25文件见raw-manifest.json。没有PDF，因此页码不适用；使用文章ID、原文标题、图片序号定位。

| 文章ID | 本轮E | 标题 | 正文范围 |
| --- | --- | --- | --- |
| 10701497927316 | E-BXRD-01 | Choose a Book Display Mode | Library Modes / Layout Options / Recent Files |
| 10701489683092 | E-BXRD-02 | Customize Status Bar | 设置入口与项目顺序 |
| 10701488996500 | E-BXRD-03 | Set Up Page-Turn Area | Settings/Touch Settings |
| 10701486858388 | E-BXRD-04 | Adjust PDF and Similar | Theme / Zoom / Layout / OCR |
| 25400769451540 | E-BXRD-05 | Adjust EPUB and Similar | Style基础与Advanced各项 |
| 10701491126548 | E-BXRD-06 | Floating Toolbar | General及Handwriting设置 |
| 10701454503572 | E-BXRD-07 | Reading Dashboard | 查看/开关、仅本机不同步 |
| 15196391289108 | E-BXRD-08 | Text Notes | 现有文本笔记的工具/保存/输出 |
| 10701570125588 | E-BXRD-09 | Tag System | Add / Search / Management |
| 10701578837268 | 沿用E-BOOX-16 | Handwritten Notes | 全文28标题、具体工具和版本限定 |

PDF/EPUB帮助明确把相应Style/Layout界面称为V4.1；Text Notes文称V3.3.2引入。Handwritten Notes旧API的edited_at为2026-04-28，updated_at为2026-09-29，两者都不是固件发行日期；新套索、模板中心、Page+等没有逐项证明本机V4.1.2支持。旧E-BXCD-13是同Handwritten Notes URL的历史记录，本批不新增另一整页来源，也不改其旧引用。

## 实际查看的四张图

四次匿名普通GET均HTTP200，未重试。接着对以下绝对路径调用view_image，均成功；工具不是shell，不适用exit code。正式副本与raw字节/散列一致，observed-images.json记录实际查看；image-fetch.json中actual_view=false是下载当时尚未查看的获取状态，以后续独立观察审计为准。

| E | 原文章/图序 | work实际查看文件（均位于/home/workspace/reader/） | SHA256 |
| --- | --- | --- | --- |
| E-BXRD-10 | 10701497927316 / image2 | work/boox-reading-controls-deepening/raw/library-scan.jpg | ca5a1051603cde55ef83b70150598f8a55e0240f3853142df6674a562c329ae3 |
| E-BXRD-11 | 10701578837268 / image10 | work/boox-reading-controls-deepening/raw/notes-page-manager.png | 089285229033c6d565e943bd546493cc136fe02ee23b471e93d5c23c26114d09 |
| E-BXRD-12 | 10701578837268 / image13 | work/boox-reading-controls-deepening/raw/notes-layers.png | 964f0841729cd1dd9de8c729de6c981b795ff42a9ee3dec92ef8aacdc8c84e9a |
| E-BXRD-13 | 25400769451540 / image2 | work/boox-reading-controls-deepening/raw/epub-more-settings.png | bd070475b2dce000ea5c15535470b922513698b50afbed7391132098444a919c |

真实附件URL按上表顺序：

- https://booxsupport.zendesk.com/hc/article_attachments/25400385712276
- https://booxsupport.zendesk.com/hc/article_attachments/40687025430292
- https://booxsupport.zendesk.com/hc/article_attachments/40687065448468
- https://booxsupport.zendesk.com/hc/article_attachments/40686663742996

只看到打开菜单/大纲/图层/Copy to弹窗；没有点击后的状态。图示机型与固件未识别，不称Note Air5 C实机图。图层锁图标效果未明；Copy to示例Bold选中不能当默认，Cancel/OK只有入口。未看其他正文插图。

## 旧行建议与合并前断言

`old-coverage-updates.json`为数组，各项包含flow_id、完整before/after及changed_fields（字段→before/after）：

- F-M004-09：evidence_ids、known_branches、gaps、visual_status、checked_on，共5字段。新增静态E11/12，视觉从not_observed改static_reference_only；D2与hands_on不变。
- F-M004-10：known_branches、gaps、checked_on，共3字段。证据仍原E-BOOX-16/02；没有额外静态或新F计数。

`old-evidence-additional-captures.json`为数组，2项均含evidence_id、expected_url、before_additional_captures、append_additional_captures；每项追加1个本地复读范围，保留旧原件路径/散列及本轮独立摘记hash。`old-evidence-field-updates.json`为空，无其他旧E字段修改。

M004案例基线SHA：`8f3b034ee2f93ffce631bf2b836db3a16edd619610a61e41df547af0fef8907a`。初始扫描全库JSONL未发现旧E以本案例路径/散列作为归档反向依赖（baseline.reverse_refs=[]）。原目录SHA：`4dc0ae66854d030dce8111696a0b2b053f65c56f03241857375610802e10abd3`，真实60行LF、0个字面量\\r\\n，DictReader59行；没有改旧目录。

## 根代理优先复核与未知

1. [Reading Dashboard](https://booxsupport.zendesk.com/hc/en-us/articles/10701454503572-Reading-Dashboard)：本work/raw/10701454503572.txt第7行明确统计只本机、不同步；1–3和9–12行为两个入口。不要把F11云阅读状态自动扩展为统计同步；停用是否清历史未知。
2. [Handwritten Notes](https://booxsupport.zendesk.com/hc/en-us/articles/10701578837268-Handwritten-Notes)：本work/raw/10701578837268.txt的Eraser（37–38）为清空内容而非删除图层，保留模板层；Layers（60–62）是最多“新增5层”；Page Manager（46–50）和More/Page-分别陈述，只有后者明说不可撤销。本机新控件与其他删除撤销规则仍未知。

其他保留：书库Manage Library只见入口，未补书籍移动/删除/恢复链；阅读版式改变不等于改写源书；OCR配额/语言数是帮助条件而非实测，未闭合离线包/额度耗尽/失败；Text Notes输出TXT/.note与手写PNG/PDF/.note分开，其附件不能套手写20MB；标签定位不证明删除标签会删笔迹；图层隐藏、锁定、跨层选择范围需进一步证据。没有借这些任务宣称G002已验证或增加明确产品需求。

## 命令、退出状态与冻结检查

从`/home/workspace/reader`运行下面**只读**命令即可复现合并前检查；检查不联网、不读设备、不写文件，合并后before值变化，不再运行此precheck：

```sh
python3 work/boox-reading-controls-deepening/precheck.py
```

已实跑exit **0 / PASS**。检查38旧E/14旧F基线、2旧F8字段、2追加E的before、13E/5F引用与URL无重复、59原文章标题/ID/URL/元数据、68子项父引用、CSV真实换行、4图原/归档hash及观察标记、本地链接、10正式/25原件manifest，hands_on=false。

```sh
git diff --check -- docs/research/interaction-library/cases/M-004-boox-neoreader.md docs/research/interaction-library/batches/boox-reading-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/boox-reading-controls-deepening-coverage.csv docs/research/interaction-library/batches/boox-reading-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/boox-reading-controls-deepening
git status --short -- docs/research/interaction-library/cases/M-004-boox-neoreader.md docs/research/interaction-library/batches/boox-reading-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/boox-reading-controls-deepening-coverage.csv docs/research/interaction-library/batches/boox-reading-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/boox-reading-controls-deepening work/boox-reading-controls-deepening
```

均exit0；正式文件当前git为untracked、work被忽略。因此空diff结果不是全量内容检查，必须结合baseline/manifest/precheck。

本批实际运行的单次操作命令：

- `python3 work/boox-reading-controls-deepening/extract_selected.py`：exit0，从本地API拆出10篇正文，network_requests=0。
- `python3 work/boox-reading-controls-deepening/fetch_images.py`：exit0，4个图片GET均200；脚本有防重跑断言，不重复请求。
- `python3 work/boox-reading-controls-deepening/build_batch.py`：exit0，13E/5F/2旧F8字段/2旧E/127目录；只运行一次。案例补充与措辞修正随后定点完成，不能重跑生成器覆盖冻结文件。
- `cat`读取AGENTS、当前案例、旧HANDOFF、旧目录；`sed`读取README、requirements；`rg -n 'BOOX|关联笔记|阅读统计|图层' docs/ideas.md`；上述读取均exit0。长合并输出曾截断，目标案例重点、源正文随后按选定文件完整读取。
- `cat work/boox-reading-controls-deepening/raw/10701497927316.txt work/boox-reading-controls-deepening/raw/10701486858388.txt work/boox-reading-controls-deepening/raw/25400769451540.txt work/boox-reading-controls-deepening/raw/10701454503572.txt`：exit0。
- `cat work/boox-reading-controls-deepening/raw/10701489683092.txt work/boox-reading-controls-deepening/raw/10701488996500.txt work/boox-reading-controls-deepening/raw/10701491126548.txt work/boox-reading-controls-deepening/raw/10701570125588.txt work/boox-reading-controls-deepening/raw/15196391289108.txt`：exit0。
- `sed -n '1,180p' work/boox-reading-controls-deepening/raw/10701578837268.txt`及`sed -n '181,230p' work/boox-reading-controls-deepening/raw/10701578837268.txt`：均exit0，共189物理行全文。
- 一次对旧压缩JSON搜索附件ID的rg输出过大而截断，命令exit0；随后改为JSON解析只输出匹配ID/下载记录，确认没有相同附件旧E或旧下载。没有将截断结果当新增正文阅读或证据。

本批没有非零shell退出、HTTP拒绝或新失败路线；旧System Split Screen读取失败及其他历史访问限制未重试。不使用其他助手，不运行旧脚本/构建器。

## 冻结散列

- 最终案例：`11a8421b29f2e4a9b33a28ab8ec04f441661ab563ea1ef86f15f00fb644ef22d`
- 独立source-notes：`49d6ffbbb7edc9937b3d66b5941d2a95eb47fb72d514071fe3ded09a13bf2dbe`
- formal-manifest.json：`cbba4986c9ce4a8974e87697202f834568a69ebec5090b558c332e8f1bace7a0`
- raw-manifest.json：`282dfd58ee908243cf1c70e43379277eea41dd0348523d8fda05704a0c8a4a75`
- precheck.py：`09298960c1abf38756dec06547310ef16de2408dd10711c256512374df91b399`

没有完成总体goal的声明；根代理独立复核、决定合并。冻结后不再编辑正式文件。
