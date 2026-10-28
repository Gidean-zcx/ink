# M055 Hisense controls deepening：冻结交接

2026-10-27。仅文档研究；本批正式文件已冻结。未改主 evidence/coverage、旧批、README、需求或全局索引；未登录、安装、下载固件/App/音视频、操作设备或账户。新增本地研究文件和修改所属案例是本轮的状态变化。没有实机测试，也没有连续视觉观察。

## 交付与合并入口

- 修改 `docs/research/interaction-library/cases/M-055-hisense.md`。
- 新建 `docs/research/interaction-library/batches/hisense-controls-deepening-evidence.jsonl`：6 E，E-HSCD-01—06；前5条为旧文章衍生静态照片，06为新作者文字来源。均 `supersedes:null`、`hands_on:false`。
- 新建 `docs/research/interaction-library/batches/hisense-controls-deepening-coverage.csv`：2 F，F-M055-09/10，均D2、`static_reference_only`、`hands_on=false`。
- 新建 `docs/research/interaction-library/batches/hisense-controls-deepening-inventory.csv`：114行，含父子项、旧线索和缺口，不等于114个完成任务。
- 新建 `docs/research/interaction-library/artifacts/hisense-controls-deepening/source-notes.md` 及同目录5张实际查看的图片，下表列名。
- `work/hisense-controls-deepening/formal-manifest.json` 列10个正式文件的冻结SHA256。原件、工作脚本和增量建议只在本work目录。

目录构成：A9旧范围/2022评测12；TOUCH原版/联名款作者目录18；Hi Reader开箱18节目录18；墨智键引导10；水墨屏控制引导21；设置顶层16；2022速记/选词13；型号化帮助缺口6。保留不同证据深度，文章比较段标出界，不把未看图片章节当视觉观察。

合并前命令（工作目录 `/home/workspace/reader`）：

```sh
python3 work/hisense-controls-deepening/check.py
```

最后结果 `PASS`，exit 0；这是只读、离线、批次预检。它检查本批ID/URL未入主表、5条旧F before与主表一致、4条旧E原URL/原captures一致、路径/散列/引用、两CSV真实LF与行数、hands_on/视觉边界和10个正式文件。合并后因“新ID未入主表”及before断言应失败，不要当作回归检查重复跑。不要运行本轮 `build.py` 或其他旧批构建器。

`check-results.json` 保存结果。第一次预检exit 1是检查器误将新批LF约定套到既有主CSV的正常CRLF；只修正检查器读取规则，未修改主CSV或研究数据。修正后exit 0。

定向格式检查已执行，exit 0：

```sh
git diff --check -- docs/research/interaction-library/cases/M-055-hisense.md docs/research/interaction-library/batches/hisense-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/hisense-controls-deepening-coverage.csv docs/research/interaction-library/batches/hisense-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/hisense-controls-deepening
```

## 旧字段建议格式

`old-coverage-updates.json` 为5条对象，逐条含 `flow_id`、完整 `before` / `after` 行、`changed_fields:{字段:{before,after}}`。应先断言before再合并；只替换列出的字段，保持旧任务编号/等级。共22字段：

| 旧F | 字段 |
| --- | --- |
| F-M055-01 | known_branches、gaps、checked_on（3） |
| F-M055-05 | evidence_ids、known_branches、gaps、checked_on（4） |
| F-M055-06 | evidence_ids、known_branches、gaps、visual_status、checked_on（5） |
| F-M055-07 | evidence_ids、known_branches、gaps、visual_status、checked_on（5） |
| F-M055-08 | evidence_ids、known_branches、gaps、visual_status、checked_on（5） |

F02/03/04沿用历史材料，未重访，不更新查阅日。F08看到选词照片仍为D1，未把菜单与词典例子当完整批注保存流程。

`old-evidence-additional-captures.json` 为E-HSD-05/08/09/03共4条；每条含 `evidence_id`、`expected_url`、`before_additional_captures`、`append_additional_captures`。是追加建议，不替换最初采集。E-HSD-03只成功读web工具正文，独立GET超时，未保存完整web原件，故raw路径/散列为null；此限制明确写在capture和摘记。`old-evidence-field-updates.json` 为 `[]`，本批没有其他旧E字段修正。

baseline.json记录起始案例散列和反向依赖扫描：当时扫描157个JSON/JSONL，无旧E将本案例文件/基线散列作为artifact依赖。`case-before.md` 保留旧案例；没有静默修改旧E。

## 核心事实与建议根复核的材料

1. 原版Hi Reader的2023开箱机内引导，非官方托管完整手册。作者[DealiAxy原文](https://sblt.deali.cn/blog/eink-hisense-hi-reader)，旧E-HSD-09。只有文章前段原版Hi Reader三图归属被使用，后段TOUCH Lite/A9对比不套用。
   - E-HSCD-01 `hireader-key-guide.webp`：“非锁屏”短按/双击启动所设App，设置→墨智键→短按/双击入口；绑定保存反馈和启动结果未观察。对应新F09。
   - E-HSCD-02 `hireader-screen-guide.webp`：引导写“当前应用显示模式”；深色“流畅”是示例。下拉的截屏便笺说明为截取当前界面并出现在便笺，对应新F10。不是收藏文章/速记的统一恢复入口。
   - E-HSCD-03 `hireader-settings.webp`：12个设置入口与底部标签；全局收藏管理/截屏便笺管理可见，未点入。
2. [IT之家2022评测](https://www.ithome.com/0/611/514.htm)，旧E-HSD-05。第三部分速记与选词照片E-HSCD-04/05：速记00:01是图中状态；图标不证明删除/导出结果；词典照片仅一个汉语大词典结果。与2023照片的构建均未知，不能拼成连续流程。

以上五图由本代理实际 `view_image` 查看成功；无PDF或印刷页码，只有文件/文章段落定位。根尚未在交接时报告实际看图，不宣称根已观察。

新E-HSCD-06为[马马马不前2021-10-01 TOUCH文章](https://www.bilibili.com/read/cv13411020/)，明确西安博物院联名款、作者自报Android11、固件未知。与旧官方黛青SKU分开。只补四标签/三键入口等D1线索，删除阅读标签不等于卸载或删数据。未看文章图片/视频。普通HTML GET200但仅JS壳；web工具匿名正文成功，工具自动最终URL带 `?opus_fallback=1`，没有自行构造接口/执行JS/API/登录。原文保存为web工具精确返回文本的JSON序列化，不能当原HTML。

## 原件与散列

正式摘记 SHA256：`b7775a8330b022ad88e1e720f904054c02fcf0f082430e849c6d856ff0792eaf`。

案例基线：`ab4d771db83f77ce24916c341a1891341d2916734b85ad196a05113eac19d625`。

案例冻结：`71b13b0560a2b58fca080e6096fbf63129b6c6b2a1ff3097f316d3b5803ab897`。

formal-manifest.json：`bf26e0e0d6b356c39c18a342ab0644faedbfd56e790c8fc511b1f555a4242cfe`。

下列原件均在 `work/hisense-controls-deepening/raw/`。五图另以相同字节归档到正式artifact目录；URL与实际获取日2026-10-27见新E和image-captures.json。

| 文件 | SHA256 |
| --- | --- |
| hireader-deali.html | 19198e3328ab67026e1829b2ca37e0e4366ef31e8dcef09434cf88ad5ab343cb |
| hireader-ithome.html | d03b139d5167157a61cba1153d93c17f40a175f2f0807854cf8ba70a91ec5ede |
| a9-yesky.html | 6aebcdd35bbf6ea17fbe9aa2ada580ed6a3e58afa17c8f94a9ab9c04ec84a400 |
| a9-yesky.gb18030.txt | 061a6f89d6d97bbdebe11d000fd8e17afe12d91ecd9f7f91ade9672b7afc962c |
| touch-bilibili.html（JS壳） | 36aff595bb593c81711f4fcb9e332815c2abab07ab0547d761957f8a66c35ff5 |
| touch-web-result.json（完整web工具文字输出） | 2f7301c95fd7d50417e223625f0d1933c26535c329895d50339aaa6fb170e014 |
| hireader-key-guide.webp | b041e7c415aa62fa63e3ebce04337188420b950b7aecba3b2ac1549d761483ae |
| hireader-screen-guide.webp | b0835be77481a58479ee1d408d1d09cb34891ee39ec72b4f26a39058386ca67c |
| hireader-settings.webp | 915066bca2f51a57b0bbfb8a60d8e62afed6f9bfbe59d4ab402d8daf7e7acc8d |
| hireader-note.jpeg | b30d3493e9a77e3ba3dae3d0bd0a3c9506ae09d75b58c55ee1308279fa50166a |
| hireader-selection.jpeg | 6c0fb281a9435f6dbd34c677eb88019f314ac535e7cb8a1ac67c31476654d328 |

文本提取散列在captures-corrected.json及旧E追加建议中。A9原HTTP头为GB2312，初次脚本UTF-8解码有乱码，后按GB18030提取正确正文；初次 `a9-yesky.txt` 留作中间件，不用于结论或正式capture。

## 实际命令、工具与退出状态

所有shell命令工作目录为 `/home/workspace/reader`。读取AGENTS、README、requirements和相关ideas、当前case、主E/F/旧work；没有运行旧批脚本。

- `cat AGENTS.md && sed -n '1,65p' README.md && sed -n '1,105p' docs/requirements.md && rg -n '海信|Hisense|M-055|恢复|后台' docs/ideas.md | head -n 35`：exit 0。
- `rg --files docs/research/interaction-library/cases work | rg -i '(M-055|hisense|eink-phone)' && rg -n 'E-HSCD|hisense-controls-deepening' docs/research/interaction-library --glob '*.jsonl' --glob '*.csv' --glob '*.md'`：exit 1，仅末段前缀无命中，属于预期查重；本轮前缀未占用。
- `rg --files work | rg -i '(hisense|hisd|touch|reader.*manual)' && sed -n '1,120p' work/eink-phone-deepening/build.py && head -c 400 work/extended-readers/www.hisense.com_.html`：exit 0。旧build仅阅读，旧HTML为日本通用主页，未用于产品结论。
- 当前case全文、来源笔记、CSV逐行和JSON建议读取，以及Python基线/反向依赖扫描：均exit 0；基线扫描结果保存baseline.json。
- 下列第一次capture命令exit 0，但首个网络操作超时，脚本立即停止；后面的参数没有被执行，不把exit 0当HTTP成功：

```sh
python3 work/hisense-controls-deepening/capture.py download-index https://mobile.hisense.com/yidongziliao/ faq-index https://mobile.hisense.com/qiyezhanshi/ hireader-deali https://sblt.deali.cn/blog/eink-hisense-hi-reader hireader-ithome https://www.ithome.com/0/611/514.htm a9-yesky https://mobile.yesky.com/283/725998783.shtml touch-bilibili https://www.bilibili.com/read/cv13411020/
```

- 以下不同来源正常采集，exit 0，4次HTTP200。TOUCH原HTML是JS壳；不当已取得HTML正文。

```sh
python3 work/hisense-controls-deepening/capture.py hireader-deali https://sblt.deali.cn/blog/eink-hisense-hi-reader hireader-ithome https://www.ithome.com/0/611/514.htm a9-yesky https://mobile.yesky.com/283/725998783.shtml touch-bilibili https://www.bilibili.com/read/cv13411020/
```

- 五图片通过一次inline Python `urllib.request.urlopen(Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=30)`循环取得，exit 0，5次HTTP200；精确URL/final_url/content_type/raw路径/散列在image-captures.json。抓取前对全库E的URL逐一查重，无旧图像E。随后5次view_image均成功；不是自动将GET记为观察。
- 匿名web搜索/打开和官方FAQ正常导航：工具成功，无shell退出码。搜索三组固定型号/说明书/后台/墨智键查询，未找到官方完整手册；其他型号/生成式回答排除。下载页web文字可读，但无对应旧三款手册链接；随后独立GET超时即停。FAQ七问是记录仪等设备，未作为阅读器功能证据。
- B站web工具打开正常返回正文（无shell退出码）；精确结果见touch-web-result.json。没有因JS壳调用账户/API或执行JS。旧论坛受限路线、商城旧403图、TOUCH旧文与A9转载旧超时路线均未重试。
- `python3 work/hisense-controls-deepening/build.py`：本轮首次生成exit 0，之后只进行措辞收紧与manifest刷新；现已冻结，不可重跑。
- `python3 work/hisense-controls-deepening/check.py`：首次exit 1（检查器换行规则过严），修正后exit 0；定向git diff --check exit 0。没有跑全库构建/测试，也没有调用外部helper。

## 未决与资料边界

三款完整官方设置/机内帮助目录仍未取得，114行是实际来源的分层台账。A9历史用户构建原写“NK OS”，未改成推测的版本名；TOUCH原版/联名款/TOUCH Lite分别处理；Hi Reader不套Pro/2026。没有电话/短信打断、后台保活、音频锁屏恢复、卸载数据规则、备份/更新失败/出厂恢复的型号化证据。

新D2来自机内引导照片上的文字，不代表本项目按过按钮、看过成功结果或确认了当前固件。短按启动的绑定保存、返回原书和失败分支仍未知；截屏便笺的格式、目录、OCR、重开/导出/删除、重启保持未知。收藏可离线阅读的旧商业文意不扩大为断网首次抓取。只做短摘记与菜单名，未转载文学正文或长篇宣传段。
