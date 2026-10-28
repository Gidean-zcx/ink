# M012 核心 App 帮助深化：冻结交接

2026-10-27，工作目录 `/home/workspace/reader`。仅Mudita Kompakt；继承已取得K1.6.1和Center4.0.0发布基线，没有重判最新版本。本轮18篇帮助没有OS/桌面构建号。正式交付已冻结，不再扩源。

## 交付与状态

- 18新E：E-MACD-01—18，supersedes:null、hands_on:false、visual_observed:false。
- 6新F：F-M012-11—16，均局部D2、not_observed。分别为联系人增删、联系人导入、Notes增删、Recorder保存/回听/删除、定位范围、指纹增删。
- 6旧F、18字段建议；2旧E本地原件复读追加；无旧E字段替换。
- 完整保留111项目录，18项从index_only改body_read；现30 text_read、34 body_read、1 prior_body_read、46 index_only。未把控制项或标题数算作完成任务数。
- 没有新增图像观察。HTML解析发现的是网站图片，附件图片计数为0；未下载/查看这些装饰图。原有两张静态图本轮也未复看。
- 状态只改变所属案例、新批/摘记和本work；没有写主表、旧批、全局文档，没有登录、发送、安装、执行设备/账户操作，未下载固件/APK/音视频。

正式文件共5个：

1. `docs/research/interaction-library/cases/M-012-mudita.md`
2. `docs/research/interaction-library/batches/mudita-app-controls-deepening-evidence.jsonl`
3. `docs/research/interaction-library/batches/mudita-app-controls-deepening-coverage.csv`
4. `docs/research/interaction-library/batches/mudita-app-controls-deepening-inventory.csv`
5. `docs/research/interaction-library/artifacts/mudita-app-controls-deepening/source-notes.md`

正式路径、字节和SHA256逐项在 `formal-manifest.json`；原始18HTML、18正文提取、18图片属性列表以及4个复读旧原件/文本，共58项在 `raw-manifest.json`。图片属性JSON不是图像观察。

## 合并前只读检查

```sh
python3 work/mudita-app-controls-deepening/check.py
```

已PASS，exit0；结果保存 `check-results.json`。验证新E/F编号、URL全库复用、旧before精确匹配、字段格式、真实LF、111项标题与URL保留、18项阅读状态、正式/原件/摘记散列、相对链接、hands_on/深度、案例反向散列依赖。检查器不写文件、不联网。合并后新ID/before断言会失效，不要再运行此合并前检查。

```sh
git diff --check -- docs/research/interaction-library/cases/M-012-mudita.md docs/research/interaction-library/batches/mudita-app-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/mudita-app-controls-deepening-coverage.csv docs/research/interaction-library/batches/mudita-app-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/mudita-app-controls-deepening
```

exit0。研究库目标可能仍为未跟踪文件，该命令不能替代自定义检查。不要重跑 `assemble.py`，首次生成后已定点收紧来源范围和修正旧段措辞；从未运行上批Mudita或其他旧build/merge/check。

## 旧记录建议格式

`old-coverage-updates.json`：每对象有 `flow_id`、完整 `before`、完整 `after`、`changed_fields:{field:{before,after}}`。对应F01/02/06/07/08/09，各只改evidence_ids、known_branches、gaps，共18字段；task/depth/visual_status/hands_on/checked_on均不动。F03/04/05/10不作无意义更新。

- F01：隐藏/非联系人号码阻止、手动名单及解封，保持电话与消息范围；不作删除。
- F02：四拆页与旧汇总重合；精化新号码MMS收件确认，保留Pencil/Plus差异。
- F06/07：音乐筛选、全选和文件类别文案；不能保证解析/入库或单项恢复。
- F08：仅补定位全局/逐App入口，其他权限和具体等级仍未知。
- F09：忘PIN/图案FAQ也要求Recovery清空；不写无损找回。

`old-evidence-additional-captures.json`：每对象有 `evidence_id`、`expected_url`、`before_additional_captures`、`append_additional_captures`。仅E-MDCD-04和06，明确 `existing_local_source_text_reread_no_network`；不是重新下载、不是新修订。E-MDCD-04复读发布附件p2 Contacts/p4 Notes/p5 SMS和Voice Recorder；E-MDCD-06复读原SMS/MMS汇总与删除。`old-evidence-field-updates.json`为[]。

case-before.md、baseline.json保存起始案例及SHA；旧E/F全量快照分别为evidence-before.json（36条）和coverage-before.json（10条）。扫描旧JSONL未发现将本案例路径或起始SHA作为归档的反向依赖。

## 优先复核的来源与原文

1. **录音停止与保存分两步**：
   https://support.mudita.com/en/support/solutions/articles/77000596747-how-to-record-a-voice-recording-
   E-MACD-07，`raw/I-M012-044.html`及同名txt；步骤5 Done停止、步骤6 New recording命名、步骤7 Done保存。原件SHA256 `4dba9c8b691b23fade20dfe39387525ee5048a6781aa8e428a1cb0b020dd3004`。Pause后恢复按钮未说明。

2. **回放Done只说明关画面**：
   https://support.mudita.com/en/support/solutions/articles/77000596769-how-to-listen-to-a-voice-recording
   E-MACD-08，`raw/I-M012-045.html`及txt；末步文字为“Tap Done to close the recording screen”。原件SHA256 `dbfa0cff728233ae20628867e5df85f316ab9bb85858def318f93ab7886d16ef`。未据它判定后台继续或停止。

补充定位范围：
https://support.mudita.com/en/support/solutions/articles/77000579756-how-to-manage-location-sharing-on-mudita-kompakt
E-MACD-15，`raw/I-M012-058.html`，SHA256 `e92045fcbb578d47fe324b5b69b5521fb7f9f9bf4c5280cf501180e6aefa2955`。只有全局切换、选App、选共享等级的步骤，未列等级名称/优先级/保存反馈；不套通用Android权限菜单。

其他18篇原件的准确URL、最终URL、HTTP状态、字节、SHA、正文定位均在 `captures.json` 和新E。新URL先对主E核重，无相同URL；SMS/MMS四篇拆页与旧汇总内容重复、Center删除拆页与综合页重复已明确标注，不称独立交叉验证，也不重计F。

## 关键未知与文案差异

- 帮助原件均显示2026-10-03，但没有固件号；web缓存曾把Notes/Contacts显示2025-08-08，记录实际HTML日期，不据此认定所有菜单属于K1.6.1。
- Notes标题54字符、Done和两次Delete有文档依据；退出草稿、自动保存、超长标题反馈、回收恢复未核。Task list仅旧附件增量声明。
- 联系人删除、阻止、导入、同步分开。机内解封段中间步骤仍写want to block，末键为Unblock contact，保留文字疑点。导入禁止断线的提示不构成失败后自动续传保证。
- 指纹管理先输入passcode；忘PIN/图案FAQ的Recovery会清数据。不能据指纹功能推断可无损绕过凭据。
- Recorder退出未保存、暂停续录、锁屏/来电/Offline+、音频导出及回收仍未知。附件p5只提save and leave弹窗排版修复，不造按钮和保存规则。
- 阅读位置、音乐队列/断点、其他权限、恢复备份的覆盖/合并及失败原子性仍未闭合；未用Pure补资料。
- 111项是上批目录的阅读状态更新，没有重抓整个站点断言最新全量；46个index_only依旧是待读。联系人6个拆页未逐页取得，即使综合正文涵盖同题也不改为已读。

## 散列

- 起始case：`f0a6ade9987cedc2b3fd61594ca2a33ab2f98126c703aa88c1e743a26edea8f5`
- 冻结case：`98bebb84e9573421200e9eae0b4120c4705a25994f30d21b1e20309a2b8e4218`
- 独立source-notes：`50b3015be7094b8d6931c95d659e8e90a7a1092257f8e09c786753bbce301727`
- formal-manifest：`303caa8812ef68f262355c161a33dfc837214aac3b137d81cfb2c5a2b6965ac8`
- raw-manifest：`4aa89f52afdee5d06e453f5bd92cfef2781b161b94c4a8254bde5fd13e8c7d8c`
- 旧F建议：`d9bdb9845f73ba53813f86bad205651f2cfb1dd197ee0400f0e9b01bb8d0ea96`
- 旧E追加：`e91b55c62a43373ce487fb8745238db22d67a65f774c7aa1f0e70a7fbd57156c`
- check.py：`db6428146927a7dcacf268eb0d3d4362f660378802bc58df8459dc18e4d7440f`

## 实际命令、工具和退出状态

本轮所有shell调用均exit0。前缀不存在通过Python断言，不把rg无匹配当失败。没有HTTP失败、401/403/429或重试；上批fetch-log非200列表为空，未重试其他历史失败路线。

1. `cat AGENTS.md && sed -n '1,95p' README.md && sed -n '1,130p' docs/requirements.md && rg -n 'Mudita|Kompakt|M012|M-012|个人AI|联系人|权限' docs/ideas.md && rg --files docs/research/interaction-library work | rg '(mudita|M-012)'`：exit0。
2. `cat docs/research/interaction-library/cases/M-012-mudita.md && cat work/mudita-controls-deepening/HANDOFF.md && cat docs/research/interaction-library/artifacts/mudita-controls-deepening/function-inventory.csv && cat work/mudita-controls-deepening/fetch-log.jsonl`：exit0，组合输出截断；后续用定向Python完整读取36E/10F、逐项打印111目录与相关fetch状态，不依赖截断输出定结论。
3. inline Python新建本work（先assert不存在）、case基线、旧E/F、前缀碰撞和反向依赖扫描：exit0；完整结构结果在baseline.json等。读取旧 `parse_html.py` 仅借鉴stdlib文本提取，没有运行旧脚本。
4. 官方web open共8页：Notes、Contacts、Center联系人、定位、录制、回放、删除录音、阻止号码。工具成功、无shell退出码；后续全部正文用普通HTTP存原件。没有打开登录/反馈提交或产品操作接口。
5. `python3 work/mudita-app-controls-deepening/capture.py`：exit0；18个GET全200，三个并行读取，精确URL由脚本SELECT对应旧目录行，全部保存captures.json。每个URL只采集一次原件，脚本对已有文件拒绝复抓。没有媒体请求。
6. inline Python打印18个txt完整正文、检查HTML图片属性、读取旧release-1.6.pdf.txt的Contacts/Notes/SMS/Voice Recorder和完整111目录：exit0。阅读旧messaging.txt核四拆页重复；没有执行设备。
7. `sed -n '1,170p' docs/research/interaction-library/template.md && sed -n '1,125p' docs/research/interaction-library/README.md && sed -n '/Modified on:/,/Sorry!/p' work/mudita-controls-deepening/messaging.txt && rg -n 'D2|supersedes|visual_status|archived_artifact' docs/research/interaction-library/README.md`：exit0，组合输出部分截断，D2和源件约定已见；不用于额外产品事实。
8. `python3 work/mudita-app-controls-deepening/assemble.py`：首次生成exit0。随后apply_patch修改所属case及摘记，inline Python追加18来源链接并刷新摘记引用SHA，均成功；这些收尾未写旧批。禁止重跑生成器覆盖收尾。
9. `sed -n '1,165p' docs/research/interaction-library/cases/M-012-mudita.md && cat docs/research/interaction-library/artifacts/mudita-app-controls-deepening/source-notes.md`：exit0，自审后修正旧“音频只取得两段”措辞和收件人限定。
10. inline Python写formal/raw manifest后运行上列check.py：PASS/exit0；另行运行精确git diff --check：exit0。读SHA的inline Python exit0。check-results由明确文本写入work，检查器本身无写入。

合并后只读审校应使用冻结清单/正文；旧before检查只供首次应用增量之前运行。
