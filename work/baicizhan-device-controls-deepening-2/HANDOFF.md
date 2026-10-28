# M022 Mini / Max：公开帮助型号边界交接

2026-10-27。本批正式资料和建议文件已冻结。旧`baicizhan-device-controls-deepening`目录/批次存在，故使用`-2`，未覆盖或运行旧脚本。

结果：**1新E、0新F**；3条原F仍全部D1。新E-BDCQ-01只用于型号适用范围核查，不能据其升级Mini/Max流程。目录45行，其中17条既有目标/历史来源条目，另5父项+23问是超级单词机范围排除目录；不是45个已分析完成的功能。

## 具体文件和合并入口

修改唯一既有案例：`docs/research/interaction-library/cases/M-022-baicizhan-device.md`。

新增正式文件：

- `docs/research/interaction-library/batches/baicizhan-device-controls-deepening-2-evidence.jsonl`：E-BDCQ-01，`supersedes:null`，独立短摘记及raw hash。
- `docs/research/interaction-library/batches/baicizhan-device-controls-deepening-2-coverage.csv`：仅正常LF标准表头，0新F。
- `docs/research/interaction-library/batches/baicizhan-device-controls-deepening-2-inventory.csv`：既有图/宣传/作者正文、历史证据记录和新帮助正文状态分开。
- `docs/research/interaction-library/artifacts/baicizhan-device-controls-deepening-2/source-notes.md`：正式短摘记；无新图片。

本work目录交接建议：

- `existing-coverage-updates.json`：3条旧F共8字段。F01为evidence_ids/gaps/checked_on，F02为evidence_ids/gaps，F03为evidence_ids/gaps/checked_on。新增关联E仅解释为什么不能套其他设备资料；known_branches/depth/visual_status均不变。F02保留此前长短按、复习同步影响、固件/App构建未知的全部范围。
- `existing-evidence-additional-captures.json`：E-BCZH-01、E-BDCD-01各追加1次旧原件本地复读，准确before/after和原batch路径。未把旧HTML写成新网络采集。
- `manifest.json`：29项文件的路径、bytes和SHA；含正式文件、原件、基线、建议和脚本，排除manifest自身、检查输出、冻结后HANDOFF。
- `case-before.md`、`case-after.md`、`coverage-before.json`、`evidence-before.json`、`baseline.json`：精确基线。旧视觉记录无case哈希依赖，旧3图散列未变；没有新增或改写视觉事实。

## 最重要的可复核依据

公开GET：`https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=dcj2`，HTTP200，11055字节。原件`work/baicizhan-device-controls-deepening-2/qa-dcj2.json`，SHA256 **774b63ea53111bdf9366af03e54563612949813c8266dd7d11da6f1a510c9bd9**。另存pretty JSON和按问号展开的qa-dcj2-text.txt。

来源路由不是猜测：本地复读先前取得的官方`/qa`HTML所引用`https://7n.bczcdn.com/baicizhan/h5/fe_qa/assets/index-CvNBM2bC.js`，字符偏移226396起`sT()`把`deviceVersion=2.0`映射`dcj2`，紧邻`ES.fetchQAList`GET上述内容地址。脚本原件`work/baicizhan-controls-deepening/qa-index.js`未修改，SHA **faa2b162d1675eb819006ec6c69ba4336ef82bed48a7c0fbcdb0cfe942ee31f0**。没有执行JS、调用统计/反馈/账号/学习接口；没有猜测其他platform，也未取sxj。

JSON定位：

| 内容 | JSON位置/稳定ID | 证据意义 |
| --- | --- | --- |
| 主机、手柄 | `data.question_categories[4].questions[0]`，Q10146 | 正文称超级单词机，涉及手柄；精确SKU未知 |
| 扫描笔按钮、灯色 | 同类别`questions[2]`，Q10148 | 不是Mini图圆键3秒解锁的补证 |
| 离线打卡回传 | `[0].questions[1]`，Q10153 | 不证明Mini/Max回传对象和时点 |
| 收藏范围/句子同步 | `[1].questions[0/3]`，Q10163/10166 | 不套Mini/Max收藏和同步规则 |
| 解绑/离线范围 | `[2].questions[0/1]`，Q10144/10145 | 不套Mini/Max退出登录、权益或图背限制 |
| 投屏 | `[3].questions[0..4]`，Q10158—10162 | 进一步说明资料所述设备场景；非Mini/Max能力结论 |

全部5类23问文字已读；没有Mini、Max或BCZ-Ace对应说明，没有发布日期/修订号/固件。`deviceVersion=2.0`也不能当已核硬件代次或固件2.0。

旧官网和拆解只有本地复读：官网22个href仍没有手册入口；作者Wen5/2025-05-23的BCZ-Ace-Max开箱正文给键名、接口和复位孔，但这些不证明复位动作或数据恢复。之前的京东/抖音失败和52audio图片403均未重试。两份旧HTML本次重新HTMLParser提取为`01-official-home-reread.txt`（66段）、`02-52audio-ace-max-reread.txt`（197段），只用于正文定位，不声称观察照片。

## 命令与退出状态

完整读取/搜索范围在`research-log.md`。实际工作cwd为`/home/workspace/reader`，核心命令：

```bash
python3 work/baicizhan-device-controls-deepening-2/capture.py qa-dcj2.json 'https://assistant.baicizhan.com/feedback_api/get_qa_info?platform=dcj2'
python3 work/baicizhan-device-controls-deepening-2/build.py
python3 work/baicizhan-device-controls-deepening-2/check.py > work/baicizhan-device-controls-deepening-2/check-result.json
cat work/baicizhan-device-controls-deepening-2/check-result.json
```

以上均exit0；capture为唯一新网络GET，HTTP200。合并前检查 **2924 PASS / 29项manifest**：旧before、4份相关旧batch hash、新ID/URL无重复、0新F、目录计数、排除项无目标F映射、3F保持D1、新E supersedes/相对归档/hash、旧3图不变、LF与本地链接均通过。正式文件未写work归档路径，raw_work_path单独记录原件。

本次未处理成成功的情况：最初错误case文件名cat exit1；旧work `rg --glob '*.txt'`无匹配exit1；一次大JSON读取输出截断，随后精确重读links数组；web.open `/qa?deviceVersion=2.0`工具内部不可访问，没有HTTP码，不算正文。4条有限官方域搜索为空。没有出现或绕过本轮HTTP401/403/429。

`sed -n '61,67p' work/baicizhan-device-controls-deepening/02-52audio-ace-max.txt`及HTMLParser复读、hash打印均exit0。没有运行旧M016或其他产品的build/check。**本check仅供合并前使用**；根合并旧字段后勿再重跑。

## 冻结散列

| 文件 | SHA256 |
| --- | --- |
| M022 case | `356c92cceb52756913036dfbaa0f29ad7ace25762f6689cc9d91da84d0f3c60e` |
| 正式source-notes | `101dae305557f5f02ddcb23f17edf95d2d80cc3856c790f4600f1d5a9d49996a` |
| 新evidence | `43192cca42b9ea5ce67f9c9cb8fa4aab54761e1c8ac738d064dc0de4f211edac` |
| 空coverage表头 | `aa0cf671b21fa0390e9b0cfb58c6cecf3a0d7df66eaad894707f17ea224f6025` |
| inventory | `cd9269f1cfcab53f1c3c60ab5f7d1d55bc331c4ea0577235348fed64b4d83803` |
| 旧F建议 | `f0e69332e3294ba7a453b244872f83fceb5e84098a6f5bf80a0d80344943f389` |
| 旧E追加建议 | `4888b3e7c02a8ecf7b513beabada8cdb480e117d6207bab5c4ac3c8de0cb91e0` |
| manifest | `1c4caf1cd74e319797c7ba3779cf151bf95cc2a52accbb1c4f7150d23843d7e0` |

## 限制与状态变化

没有补齐Mini/Max首学错误反馈、提示重试、按键状态规则、听词续播、资源/记录同步、误斩或宠物恢复。仍需匹配型号/修订页的原厂说明书或完整连续演示。没有把未知写为不支持，也不因多读23问而增加目标任务数。

状态改变仅为本案例、本批文档和work资料；旧批/master/全局/其他案例未改。无账号、购买、安装、固件、音视频下载、外发消息或设备操作。所有正式与建议文件已冻结。
