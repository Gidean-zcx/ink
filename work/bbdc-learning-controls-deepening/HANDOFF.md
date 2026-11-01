# M018 本批冻结交接

2026-10-27。已收束：检查6候选，采用3新E（E-BBLC-01～03），0新F；M018仍5F/3D1/2D2。2旧F共8字段提案，0旧E追加采集。9行增量目录是历史入口/规则与缺口的映射，不是现行帮助菜单或9条任务；与旧34项只建立注明性质的关联，不能累加宣称完整产品。

## 合并输入与状态

- 修改：docs/research/interaction-library/cases/M-018-bbdc.md。旧官方商店/政策事实保留，并将此前无图观察表述限定到此前复核，避免与本次历史静态图矛盾。
- 新批：docs/research/interaction-library/batches/bbdc-learning-controls-deepening-evidence.jsonl（3行）、同前缀coverage.csv（标准表头/0行）、inventory.csv（9行）。
- 正式归档：docs/research/interaction-library/artifacts/bbdc-learning-controls-deepening/source-notes.md、static-observation-log.md，以及c04-version-table.jpg/c04-flow.jpg/c04-study-ui.jpg/c05-first-word.jpg/c05-spelling.jpg。
- 旧行建议：work/bbdc-learning-controls-deepening/existing-coverage-updates.json，数组每项含flow_id/source_batch/changes[field].before/after/replacement_row。目标旧批vocab-coverage.csv；F-M018-01和02分别修改evidence_ids、known_branches、gaps、visual_status；两条depth仍D1，visual_status由not_observed改static_reference_only。没有执行合并。
- 旧E：existing-evidence-additional-captures.json为空数组；本轮没有重抓旧URL。9个旧E均没有指向case的视觉审计散列依赖，新E使用独立审计文件。
- 基线：case-before.md与baseline.json。主表、旧批、全局文档未编辑；外部账号/产品状态未改变。state_changed=true仅指所授权的研究文档和work文件。

## 根代理优先抽核

1. E-BBLC-02 https://agirls.aotter.net/post/51654 ，亭伊，2017-07-23，标题Android/iOS但图未标构建。raw/c05-agirls2017.txt第101–104行及两张相邻图：Learn→不认识→释义/看例句；第二拼图明确辨识完成后放弃/开始拼写。键盘和灯泡属于两个不同词样例，不能当同词提示前后；没有听音、拼错/重试/终结或放弃后画面。第102行称五次，只标作者历史说法。
2. E-BBLC-01 https://www.woshipm.com/evaluating/4425622.html ，FuzzFace，2021-03-26，§3.1自报体验2021-03-19/iOS13.6.1（raw txt99–101），表列V4.3。§3.7.3流程图含选错→答案→继续，但为作者绘图；实际四App学习页拼图只用不背右侧crayon区域。后文扇贝改版建议没有迁入本产品。
3. E-BBLC-03 https://sspai.com/post/76700 ，胡鹤仙，页面2023-10-15；txt40/76分别记2022-11-06经历和将到2022-11-18活动，不能把页面日期当体验日。txt50为四次/重学说法；71–72区分首组学习完成与拼写完成。首张图片403后整域停止，所有配图未看，不承认作者所称官方小窍门已核原厂正文。

本批实际看5张正式图（3张2021材料+2张2017拼图），另看论文第4页/印页2487图3但仅用于排除“UI截图”，未归档为产品视觉证据。无连续操作、音视频、安装或登录。原图URL与HTTP/散列在fetch-selected-results.json和fetch-detail-images-results.json，独立静态记录列精确分工。

## 排除与未决

- C01 https://www.woshipm.com/evaluating/3548342.html （2020）正文可读，但关于结算分享等被同页评论质疑；不选择评论为真，也不用作者未经固定构建的规则补现版。
- C02 https://www.sohu.com/a/579227156_819205 ：web.open内部400 Timeout fetching，无正文，不换路重试。
- C03 https://pdf.hanspub.org/ae20220700000_69509379.pdf （2022-07-25/王雅萱）：6页论文，第4页图3是作者算法图，非App UI，无构建或可复核实际序列。四次答对/清零不升为产品规则或效果验证。
- C06图片域首请求403后停止，错误响应虽扩展名jpg但不是图像。旧官方脚本/知乎403、微博robots与自制词书登录路线均没有重试。
- 仍缺当前同一词的错答→提示→重试→通过、误评返回是否更改记录、错词专项空集合/结束、放弃拼写后状态、离线/跨日/重装恢复。2017五次与较晚四次没有足够模式/构建可对齐；完成/奖励不等于掌握。历史材料不足以升级三条D1。

下一有效材料是可匿名访问并固定构建的官方逐题帮助/短教程，需同词重试和结算/恢复；仅新商店功能概述或无构建流程图不满足。本轮不再扩源。

## 检查与执行

完整命令/退出记录见commands.md；候选与HTTP结果见candidate-register.json及fetch-*.json。

合并前只读入口：`python3 work/bbdc-learning-controls-deepening/precheck.py`，已exit0/PASS（3次）；只核本批与旧行基线，不建大型检查体系。合并后旧before变化，不重跑它或write-batch.py。git diff --check exit0但仓库文件为untracked，已另外读diff和校验散列。diff -u exit1表示预期文档差异；一次rg因不存在scripts目录exit2，未改变状态。普通GET脚本均exit0；HTTP403按规则停止。

frozen-manifest.sha256覆盖case、新批/正式artifacts及本work原件/脚本/交接（不含manifest自身）；可用`sha256sum --check work/bbdc-learning-controls-deepening/frozen-manifest.sha256`只读验证。冻结后不再编辑上述内容。

## 核心SHA256

- `docs/research/interaction-library/cases/M-018-bbdc.md`：`c9674767b37c1e041cffb980a927a64adad5fafccddcc82951aedc1414798e29`
- `docs/research/interaction-library/artifacts/bbdc-learning-controls-deepening/source-notes.md`：`6b0bbf416de2be32640a715bca36aebda0d82f0243674bd266d10c5f86df39b5`
- `docs/research/interaction-library/artifacts/bbdc-learning-controls-deepening/static-observation-log.md`：`1fd81bdc952e6ba810e06b3052e7d52d774e0b7795fb78d9b8b57089a5b2c64e`
- `work/bbdc-learning-controls-deepening/existing-coverage-updates.json`：`3cecb815edbbcbd2321c2b30784cf1ae685a613526818b40e310ea3534437856`
- `work/bbdc-learning-controls-deepening/case-before.md`：`5ff40f5ab1b1141b6ae561cfd9ee11d0a929da6ef02af5b40f1b30da777867c2`
- `work/bbdc-learning-controls-deepening/raw/c04-review2021.html`：`f421f35afd9d1c6aa0be73be8840341a587d15737e0b8c4cb42bdbb6c444d1d2`
- `work/bbdc-learning-controls-deepening/raw/c05-agirls2017.html`：`17771d6a7abc1df1c28265859affa7edcd8ad31e767bd958c1eede37123e6444`
- `work/bbdc-learning-controls-deepening/raw/c06-sspai2023.html`：`d34cd3191ef0e96f5e54cf79381d8a256e92ee7b32b10bd8dc78a8be74136f1f`
- `work/bbdc-learning-controls-deepening/raw/c03-paper2022.pdf`：`de8f31eac97e927fa4b5a8943d85609757fe0456b41e33617d48f2d503519d0a`
