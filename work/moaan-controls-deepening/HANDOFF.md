# M054 墨案深化交接（冻结）

2026-10-27。1新E（E-MOCD-01）、0新F；82目录行；8旧F共27字段建议；12旧E追加采集；另2旧E共3字段措辞修正。所有hands_on=false，无D3/D4、设备或账户操作。主表、旧批和全局文档未由本任务修改。正式案例没有待合并等临时文字。

## 正式文件与基线

仅修改 `docs/research/interaction-library/cases/M-054-moaan.md`。新增：

- `docs/research/interaction-library/batches/moaan-controls-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/moaan-controls-deepening-coverage.csv`（只有表头，0新F）
- `docs/research/interaction-library/batches/moaan-controls-deepening-inventory.csv`
- `docs/research/interaction-library/artifacts/moaan-controls-deepening/source-notes.md`
- 同artifact目录 `plus-quick-panel.jpg`、`plus-refresh-panel.jpg`

全部7个正式文件的SHA256在 `work/moaan-controls-deepening/formal-manifest.json`；manifest自身SHA256：`9f4aece3982738a6ecd94aa3d6c49d527a4a784e7e531cfb6588510a775f168a`。

案例基线保存为 `case-before.md`，SHA256 `9884a58f8076492a57c39031cab91121072c34de7bf3eda5d8c2d1637fe80567`；最终案例SHA256 `f9992c1a2cb850e290c4257415e78b974abfa5f9f662cbce5a9f1d9f5a518b26`。`baseline.json`记录154个JSON/JSONL资料的反向依赖扫描，未发现既有E以本case路径/哈希作审计引用；无需迁移旧E的case散列。

目录构成：Plus三图短说明书27行（封面、10章、6部件、04节7条、05节2条、保修卡）；官网历史目录/分支17行；2022作者文字/所见控件26行；其他型号边界6行；未闭合任务6行。父子项均有parent_id；本目录不是82个任务或完整功能完成率。

## 合并建议格式

- `old-coverage-updates.json`：每行含完整before/after及逐字段changed_fields。F01改known_branches/gaps/checked_on（3）；F02另改evidence_ids（4）；F03另改evidence_ids/visual_status（5）；F04—08各3字段。合计8行27字段，depth和hands_on均不变。
- `old-evidence-additional-captures.json`：每项含evidence_id、expected_url、before_additional_captures和append_additional_captures。12个旧ID：E-MOD-01、03—13。其中01/03/04/10/11/12/13复用本地原件；05—09同HTTP URL再取。保留旧采集日期，追加2026-10-27的读取方式/定位/散列。
- `old-evidence-field-updates.json`：E-MOD-05改locator及note（保留“外面”歧义）；E-MOD-06改locator（原文只写Apps，不擅称分类，补第三取得入口）。2行3字段，均含完整before/after和changed_fields。

建议三组before在写入任何修改前先统一断言。字段更新和additional_captures分别应用，不用完整after覆盖刚追加的采集。旧F建议SHA256 `ec9b7e9a5af66853cf6bcc24b8298640a343f0deda13313edcd0f77a939aa274`；旧E字段建议 `05cc7530294540aba0b2227a23e48610ad6ce8ca0ea95bc10fc9c21a7796e3e5`；追加采集建议 `11edf66a37ab48e0f95eceee43fdafeef7ce92179e9e3bd5af4cb0d889667e67`。

## 关键原件、定位与实看范围

1. **Plus短说明书复用旧E-MOD-11/12/13，不新增另一份手册E。** 原URL保持GitHub固定提交 `ca9246421618d51dd1fe9ab9883e7c564a3dd471` 下的manual/cn/a_cn.png、b_cn.png、c_cn.png。现工作副本是raw/inkpalm-plus-{a,b,c}.png；正式图仍复用artifacts/chinese-reader-deepening同名图。a SHA256 `00ae755dbad032a799e3821872e9e8d7827a6109f378c33ed34c0a1aba1a674e`，b `091a998e689dda0622ea93c3fcf31abf8812baae4ae0ee062c97144ae3005064`，c `d5fd8a0f63d847d345abac9e0995a83218cb9247759923134499f1ef4a5dd852`。实际复看全部三图；a=封面/07，b=01/02/08—10，c=03—06/保修卡。**不是PDF，没有印刷页码映射；01—10为章节号。** 新增事实只包括04灯态等旧任务分支。社区保存、原始扫描出处/固件未知。

2. **旧官方HTTP路径。** `http://www.moaansmart.com/?show=87`，raw/root-moaan-87.html，SHA256 `6e5fa6a9d46f8f59ccecc66072053ec18c84552b3e52c65325b235d7a96d543f`，正文“不加入书架多看书架则不会显示”；下拉打开与两处返回的连写句不补成确定顺序。`http://www.moaansmart.com/?show=88`，raw/moaan-88.html，SHA256 `6fd73bf9358aa1bb614cb86b39a80af6eac5755b361725cc722532ccd881857f`，正文“目标文件夹外面”措辞保留；网页GIF未看。root已复核这两句。

3. `http://www.moaansmart.com/?show=86`，raw/moaan-transfer-2020.html，SHA256 `acf75b537ce92a2165fe2c07d9358be02809678e8fa43cbd45426e48b4f0435d`，USB books/FTP book冲突保留。USB文字只到进入书架，不能当自动扫描发现反馈。`http://www.moaansmart.com/?show=82`，raw/root-moaan-82.html，SHA256 `7894f00e6cbbbd9a43f7b856c18951cd43f4dc42eeb2396bbcfc3135b5b518f8`，列表/缩略图均有导出入口，导出至Note不说明原笔迹关系。2020全部指南机型/固件仍未标，不升级为Plus/W7/W8的D2。

4. **新E-MOCD-01**，`https://www.sohu.com/a/552509726_254916`，2022-05-30署名竖锯评测，标原创并自报使用；非官方、固件未知。raw/plus-report.html SHA256 `8b7525cd1be160d245f83827e9199fe9f1fb2d2925fecc6fc2dd82b4642eadf5`。实际查看两张2160×1500作者照片，工具显示缩至1889×1312；raw与正式副本相同：
   - plus-quick-panel.jpg：`https://p7.itc.cn/q_70/images01/20220530/1e57b2c830e44ebc9bd0de045b3571cf.jpeg`，SHA256 `8bf0f72f6a219b4b469500019ea5b703d39cf4a4262241da711481a3560e4fd0`。
   - plus-refresh-panel.jpg：`https://p2.itc.cn/q_70/images01/20220530/5f4187359b5b4feb9463dd75956a06dc.jpeg`，SHA256 `e7b7e620c0e20761c99b16ba8228bcdfb19fa104916179d40074b76732029481`。
   root也已实际查看两图。普通项选中只属该静态状态，不是出厂默认、切换生效或取消结果。低处开关文字不清，未命名。未看该文章其他图片；不采其续航、护眼或任意App兼容结论。作者Type-C耳机报告没有播放器、睡眠或断连恢复链。

正式archived_artifact全部相对interaction-library且实际存在；原件仅用raw_work_path/raw_sha256，无work路径伪作正式归档。

## 命令与退出状态

cwd均为 `/home/workspace/reader`。已读AGENTS.md、README.md前50行、requirements.md前80行及ideas/requirements相关命中；当前case全文、8F/19E、旧HTTP原件和三图。未运行其他批次的构建器/检查器。

主要精确命令：

1. `rg --files docs/research/interaction-library/cases work | rg -i '(M-054|moaan|inkpalm|chinese-reader-deepening)' && cat docs/research/interaction-library/cases/M-054-moaan.md`：exit0。
2. `rg -n 'M-054|E-MOD|E-MOA|inkPalm' docs/research/interaction-library/coverage.csv docs/research/interaction-library/evidence.jsonl && cat AGENTS.md && sed -n '1,50p' README.md && sed -n '1,80p' docs/requirements.md`：exit0；长输出后另用Python完整筛出8F/19E保存基线。
3. `rg -n 'E-MOCD|moaan-controls-deepening' docs/research/interaction-library --glob '*.md' --glob '*.jsonl' --glob '*.csv'`：初查无匹配exit1，表示前缀未占用；后面的cat成功，复合工具命令最终exit0。新搜狐URL也先全库JSONL检查，无旧E。
4. `python3 work/moaan-controls-deepening/capture.py plus-report https://www.sohu.com/a/552509726_254916 moaan-88 'http://www.moaansmart.com/?show=88' moaan-89 'http://www.moaansmart.com/?show=89' moaan-85 'http://www.moaansmart.com/?show=85' moaan-84 'http://www.moaansmart.com/?show=84' moaan-83 'http://www.moaansmart.com/?show=83'`：exit0，6次HTTP200。完整元数据与原件/提取散列在captures.jsonl。
5. 两图用一次内联Python `urllib.request.urlopen(Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=30)` 顺序GET，URL见上文；exit0，两次HTTP200。image-captures.json记录原始URL、类型、散列。随后view_image实际查看两图，均成功；三张旧手册图也用view_image复看成功，无联网。
6. `python3 work/moaan-controls-deepening/build.py`：exit0。随后独立本地Python修正案例开头/网络行、USB入库表述、Apps措辞，生成两条旧E字段建议，并更新manifest；exit0。**不要重跑build.py**，它有案例基线断言且后续收紧内容不在旧生成结果内。
7. `python3 work/moaan-controls-deepening/check.py`：exit0、PASS。检查before、全部正式/原件散列、引用/路径、1E/0F/82目录、8F27字段、2E3字段及12追加采集。合并后before自然不适用。
8. `git diff --check -- docs/research/interaction-library/cases/M-054-moaan.md docs/research/interaction-library/batches/moaan-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/moaan-controls-deepening-coverage.csv docs/research/interaction-library/batches/moaan-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/moaan-controls-deepening`：exit0。部分资料未跟踪，空diff不能代替内容/散列检查。

检查CSV真实换行：新coverage 1个换行/0数据行，inventory 83个换行/82行，均没有字面量反斜线r/n。检查结果保存check-results.json。

本轮两项本地读取错误均已处理且没有安装工具：一次假定旧artifacts/chinese-reader-deepening/source-notes.md存在，sed报不存在导致该命令exit2；一次试用bs4报ModuleNotFoundError，exit1，随后用标准库HTMLParser正常提取，exit0。并非网站拒绝或证据取得成功。未重试历史Maitube/ManualsPlus 403、旧HTTPS失败或W8超时路线；本轮没有新增访问拒绝。

web工具只用于限定发现和公开正文读取，查询为“墨案 inkPalm Plus 使用说明书 睡眠 音乐”、“site.moaansmart.com 墨案 说明书 迷你阅 Plus 休眠”、“site.moaansmart.com W7 W8 用户指南 备份 恢复”，随后打开上述搜狐原文。工具成功，无shell退出码；无效跨品牌结果未作证据，未知型号21页预览/商城泛述未拿来补流程。

## 限制与状态

没有取得Plus完整机内帮助、固定固件、各App实体键映射、Logo返回后的页/选区、文件扫描和删书恢复、睡眠听书及断点、备份还原。W7/W8仅保留原作者历史线索；本轮未取得两款独立手册，不以2020指南补型号归属。书架未显示不等于文件已删除，重新开机不等于数据恢复。

只改变授权案例、本批文档/静态照片及work研究文件；未登录、发消息、操作设备/账户、安装、下载固件/APK/音视频或执行产品API。没有改需求、开始实现或改变总体goal状态。正式文件已冻结，root可按上述check合并前核验。
