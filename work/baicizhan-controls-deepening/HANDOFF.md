# M016 百词斩 App 本轮交接（已冻结）

2026-10-27。只做公开文档研究；正式资料、建议JSON与manifest已冻结，不再修改。新增 **2 E、2 F**；补 **4条旧F的17个字段**，建议 **6条旧E各追加1次采集**。主任务F-M016-01保持D1；不声称完整首学/复习闭环或实操。父目录5项+唯一问题50项=55目录行；热门8项重复不另计；爱阅读11问不当主App背词规则。

## 交付文件

唯一修改旧正式文档：`docs/research/interaction-library/cases/M-016-baicizhan.md`。

新增正式文件：

- `docs/research/interaction-library/batches/baicizhan-controls-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/baicizhan-controls-deepening-coverage.csv`
- `docs/research/interaction-library/batches/baicizhan-controls-deepening-inventory.csv`
- `docs/research/interaction-library/artifacts/baicizhan-controls-deepening/source-notes.md`
- `docs/research/interaction-library/artifacts/baicizhan-controls-deepening/visual-audit.md`
- `docs/research/interaction-library/artifacts/baicizhan-controls-deepening/qa-rhyme-mode.jpg`

基线、原件、7份HTTP采集元数据、派生文本、脚本、检查结果与本交接均在`work/baicizhan-controls-deepening/`。`manifest.json`含46个文件：正式资料、建议、原件/派生文件、基线与脚本；不含manifest自身、检查输出和冻结后交接。没有改master、旧batch、README、requirements、ideas或全局索引。

## 根代理合并入口

- `existing-coverage-updates.json`：逐字段before/after，含原batch路径。F01 4字段，F04 4字段，F05 5字段（含任务名），F06 4字段，共17；没有深度变更。
- `existing-evidence-additional-captures.json`：E-BCZ-01/02/03/04/06、E-BCD-07追加采集，before均为原数组；不改原证据ID/URL或旧顶部日期。01/03/06为新GET；02/04/BCD07为旧原件本地复读，明确原采集2026-10-26。
- 新F-M016-07：Q10用铜板补学习日历，D2文档局部；不补小班/斩家活动；确认、费用、失败和撤回未知。
- 新F-M016-08：Q22收藏词导出PDF，D2文档局部；不扩成任意整本词表导出、打印成功或可导回。

两项新任务目标不同于旧学习/收藏流程；没有为相同恢复/同步步骤另增F。2个新E均`supersedes:null`；视觉枚举沿用`not_observed`/`static_reference_only`。

## 最重要的原件定位

1. 官方入口`https://www.baicizhan.com/qa`引用JS `https://7n.bczcdn.com/baicizhan/h5/fe_qa/assets/index-CvNBM2bC.js`；静态代码`ES.fetchQAList`调用公开内容GET，`sT()`非设备分支为bczApp。未执行JS、未调用其统计POST/用户反馈/账户路径。
2. 公开帮助正文`https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=bczApp`；新E-BCCD-01。原件`qa-bczApp.json`；Q11=`data.question_categories[3].questions[5]`，Q20=`[0].questions[5]`，Q51=`[3].questions[13]`，Q10=`[3].questions[4]`，Q22=`[0].questions[7]`。每个对象有稳定id，派生qa-text.txt可直接定位标题。Q11是已斩对象恢复；Q20警示离线未同步前清缓存会丢记录；Q51要求完成并上传，停留10秒只是建议。
3. 政策`https://passport.baicizhan.com/privacy/privacy.html`，复用E-BCZ-06，本次原件日期2026-09-04；`privacy.txt`322—354行，独立数据请求与删除、保留例外。Q20205旧措辞把个人信息删除引到注销，不能替代新政策。
4. Q18称暂不支持词表导入；`introductions.html`却声明自定义上传。3.8.3离线范围与未标日期FAQ不同；2023深度模式公告与官网也不同。未解冲突均并列，不替现版裁定支持/不支持。

App Store同URL新HTML显示7.10.25/2026-10-19、最低iOS/iPadOS13.0，但帮助JSON没有日期/修订号/build/OS，不能把50问整体标成该构建。旧官网页脚7.8.10也不是本次安装版本。新F的构建均未知。

## 静态观察与视觉审计依赖

本轮仅实际查看Q49一张原图，E-BCCD-02；`view_image`原1125×3346、展示689×2048整图。观察到主页修改、计划页押韵/图片模式选择和4本词书范围；图中2023-06-22是示例计划日期。“切换成功”为图文说明，不是本轮执行反馈。没有查看QA其他附图、播放任何音频/视频或操作App。

旧E的5张图使用独立`artifacts/chinese-vocab-deepening/static-observation-log.md`，SHA仍为`764b4a90084a97e7a9798db01139511544029883c936ba9d3daa1e3e3a085204`；无视觉审计依赖M016案例，因此不需迁移。未重看或改写旧图的视觉审计。

## 检查与实际命令

执行目录`/home/workspace/reader`。7次网络capture命令完整列在`research-log.md`，均HTTP200、命令exit0；未遇本轮401/403/429，也未重试旧失败路径。只读正文与静态脚本、JSON解析命令exit0。

```bash
python3 work/baicizhan-controls-deepening/build.py
python3 work/baicizhan-controls-deepening/check.py > work/baicizhan-controls-deepening/check-result.json
cat work/baicizhan-controls-deepening/check-result.json
sha256sum docs/research/interaction-library/cases/M-016-baicizhan.md docs/research/interaction-library/artifacts/baicizhan-controls-deepening/source-notes.md docs/research/interaction-library/artifacts/baicizhan-controls-deepening/visual-audit.md work/baicizhan-controls-deepening/manifest.json work/baicizhan-controls-deepening/existing-coverage-updates.json work/baicizhan-controls-deepening/existing-evidence-additional-captures.json
```

最终build exit0；check exit0、**1698 PASS / 46项manifest**，核旧before、旧4批完整hash未变、新E/F冲突、引用、相对归档路径、JSON/CSV、LF、所有原件/正式hash及旧视觉审计。check只读；输出重定向只写本work检查结果。**仅适用于合并前**；根合并旧字段后不要再运行build/check。无需运行其他产品的旧脚本。

已记录的非零/异常：首轮build因源answer含空数组exit1，过滤空块后再次exit0；第一次失败时case尚未写。diff exit1是预期文档差异。前缀rg无命中exit1；误查不存在`scripts/check_docs.py`的rg exit2（管道exit0），未执行全库脚本。web.open短App Store URL发生工具内部错误、无HTTP状态；随后用旧E完整URL普通GET200，未绕过拒绝。没有隐瞒这些失败或将其算成功取证。

## 关键SHA256

| 文件 | SHA256 |
| --- | --- |
| M016 case | `4d910c9531045cd6fbade4924375c2a0fe46f97461e99b8f976878f9eb38c936` |
| source-notes.md | `a8e593cc3fe506258a61b75de641fcaf7911262ce05759497af43624db551e16` |
| visual-audit.md | `de7fe0ee554ddc1ec14fbb55e4d548230334e1211a8587cae6f59b1d7e09983e` |
| qa-bczApp.json | `2fae2b92327222c9641122ae218671bb3fa5efb52c1f3646bad06f7cd2a3d7ab` |
| qa-index.js | `faa2b162d1675eb819006ec6c69ba4336ef82bed48a7c0fbcdb0cfe942ee31f0` |
| qa-rhyme-mode.jpg | `5791d1a6c0a0160d43dcd1a5826a28351356fa3c190cf18b5387287d62fc2fc2` |
| privacy.html | `826be3ac31442ba974f792ddc5c61c28b43790c1935eb2dae5ce49e7a9f7fcad` |
| existing-coverage-updates.json | `138f59780516cee554155c0df7362196b1b62e54d137c318e9d29891ac3dea81` |
| existing-evidence-additional-captures.json | `66bcb0941e7e9f2f71fd504c0162a4f8522f2b648bc9dbe5cbdb2647ca3ac9c1` |
| manifest.json | `aae41394469d05ffc2e3b3ecdcc614bcc448e3e78d6d3c421c8209fd50e64aae` |

其余原件和正式文件SHA完整在manifest，归档字段只用library相对路径。

## 冻结后简报只读审校

实际命令`sed -n '1,240p' docs/briefs/023-learning-completion-and-records.md`，exit0；对照本轮已读QA和privacy原件，没有联网或改文件。发现一处口径宜收窄：首行“误把词标为已掌握”应为“误斩单词/误把词标为已斩”，因为Q11不证明已斩等同掌握；已发根代理。其余百词斩段与来源边界一致，包括不能把停留10秒当上传回执。未审校其他产品段。

## 未知与状态变化

仍未知：当前首学/复习错答—提示—重试—结算全链、即时撤销、恢复后调度/计数、退出题号、同步回执/冲突、下载完成标记及离线题型；导出格式字段和导回；帮助资料确切年代与构建。没有把已斩、已学、完成或日历记录当长期掌握。

状态变化仅为上述文档/原件/图和检查文件；没有外部账户、学习记录、购买、发送、安装或设备状态变化。没有改用户需求或产品代码。资料不足已写明确下一类证据：同一OS/build连续首学/复习与中断恢复说明。
