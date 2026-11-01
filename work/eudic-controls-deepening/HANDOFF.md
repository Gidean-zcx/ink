# M026 欧路词典有限补证交接

2026-10-27 已收束；没有网络或编辑动作待完成。正式变化仅 M026 case、新同名批次、source-notes。主表、旧批、根文档未改。

## 数量与旧行建议

- 新 E **3**：E-EUCD-01 官网产品/新日志入口，E-EUCD-02 分端日志公开摘要（非正文），E-EUCD-04 厂商 AI 数据声明。草稿 E-EUCD-03（受限 AI 稿）撤出，不复用，不进入合并。
- 新 F **3**：F-M026-11 跨软件取词、12 全文索引查询、13 解释页组织；均 D2，复用已核官方 E-EUD-14/15/16。
- 案例现 13 F，其中12D2、1D1；总 E22。没有实操或图像观察。
- 帮助目录22页＋12个已展开章节，共34行；章节属于父页，不是34个完成功能。
- `work/eudic-controls-deepening/existing-coverage-updates.json` 是 root 所需逐字段 before/after，另有完全等值的 UTF-8/LF CSV。共22字段：F01/03/04/05/10各4字段，F07两字段；所有旧depth保持不变。
- F01补先前已在case的两种返回与context边界；F03/04补旧帮助键盘分支；F05补旧帮助新词配额/追加条件；F07明确备份恢复未知；F10补新日志摘要取得边界。

## 正式文件

1. `docs/research/interaction-library/cases/M-026-eudic.md`
2. `docs/research/interaction-library/batches/eudic-controls-deepening-evidence.jsonl`
3. `docs/research/interaction-library/batches/eudic-controls-deepening-coverage.csv`
4. `docs/research/interaction-library/batches/eudic-controls-deepening-inventory.csv`
5. `docs/research/interaction-library/artifacts/eudic-controls-deepening/source-notes.md`

原件与全部散列在 `work/eudic-controls-deepening/hash-manifest.json`；正式摘记只保存定位/URL/哈希，不复制正文。旧E重新取得Markdown的原件也在摘记逐条登记，没有为了获取格式改变重复增E。

## root 抽核入口与关键定位

- [跨软件取词](https://docs.eudic.net/1/shi-yong-zhi-yin/dan-ci-cha-xun/kua-ruan-jian-qu-ci.md)：iOS页签的默认翻译18.4+，Android浮动图标取词1–4及LightPeek限制；原件 `07-cross.txt`，SHA-256 `96eef09643f9d77f2bdcc9bdb698084fae2788ca62f9620c3c715668055a5226`。
- [全文搜索](https://docs.eudic.net/1/shi-yong-zhi-yin/dan-ci-cha-xun/quan-wen-sou-suo-1.md)：启用索引、完成状态、后台、首页与解释页两入口；原件 `08-fulltext.txt`，SHA-256 `0cf89c90345c5b504ae73b709d498e602e466d4ac72313fb83785ced1a4c40e8`。没有以帮助耗时估计冒充测试。
- [解释页面设置](https://docs.eudic.net/1/shi-yong-zhi-yin/jie-shi-ye-mian-she-zhi.md)：第1/2/4/5/6节；原件 `09-display.txt`，SHA-256 `cfa95c1c1e9d4f0c4cd008f9055458cb5adc501407693d01042f341c1a2e8701`。隐藏与删除分开；误隐藏恢复未推断。
- [外接键盘](https://docs.eudic.net/1/jin-jie-gong-neng/wai-jie-jian-pan-kuai-jie-jian.md)：iOS条件，解释页Command+A/N及笔记页S/W；原件 `10-keyboard.txt`，SHA-256 `1f85fab0bf6c31c24fe5c712b5402c41a59c2c493b0f618779872684f05ba209`。不外推触屏入口。
- [旧学习FAQ](https://docs.eudic.net/1/shi-yong-zhi-yin/untitled-3/untitled-2.md)：1(3–5)及7，原件 `11-learnfaq.txt`，SHA-256 `ccce5fd6759acac39f4b41dc4a3e29cad9b6361e99e96b96847c47538493430b`。周期备份不是同步，没给恢复按钮。
- [AI数据政策](https://www.eudic.net/v4/home/privacy)：第五节与正文最近更新2025-09-20，原件 `18-privacy.html`，SHA-256 `3c5479db3846b3d53dded78af9cda8d2f959e4de9f9a4ed0dd1e8e0a685cfb8d`。仅厂商声明，未推断词典UI/本地模型行为。

## 取得方式和失败边界

官网产品页链接 [新日志目录](https://eudic.yuque.com/org-wiki-eudic-fxu2ea/mfxd3t/fuke8887s5mulqyf?singleDoc)。普通HTTP得到HTML；解析已返回 `window.appData = JSON.parse(decodeURIComponent(...))` 里的公开元数据，未运行网页JS或调用其内容API。子页为目录明确给出的iOS、Android、桌面slug。三个doc.body_asl为空；description有摘要、content_updated_at只是文档字段，不能据此认为已取得完整日志或功能发布日期。各原HTML散列在E-EUCD-02.additional_raw_sources。

API入口 `http://my.eudic.net/OpenAPI/Doc_Index` 返回HTTP200登录壳，未登录、未找token、未调用接口。AI短文稿先由搜索返回正文，随后直接HTTP429即停止，不换域/改参重取；根据root收束指示，只记限制，不入E/F。搜索显示文本的人工留存在work，不是原HTML、未观察图片。网页搜索中错误appkey=eusoft_ti、未核身份镜像、第三方安装目录均未用作事实。

## 实际主要命令与退出码

工作目录均 `/home/workspace/reader`：

- `python3 work/eudic-controls-deepening/fetch.py initial`：exit0；6请求200，其中API是空内容登录壳。
- `python3 work/eudic-controls-deepening/fetch.py help`：exit0；7请求200、AI稿1请求429后停止。每请求一次、timeout=(5,18)、最多3并行。
- `python3 work/eudic-controls-deepening/fetch.py limits`：exit0；4请求200。
- `python3 work/eudic-controls-deepening/parse_yuque.py 13-log` 和 `python3 work/eudic-controls-deepening/parse_yuque.py 15-ios-log 16-android-log 17-desktop-log`：exit0；只读本地原HTML元数据。
- `python3 work/eudic-controls-deepening/write_batch.py`：exit0；初稿后按root指示撤出受限AI来源，再运行一次，最终3E/3F/34目录/22旧字段。
- `python3 work/eudic-controls-deepening/update_case.py`：exit0；之后局部修正Markdown表连续性、词条排序措辞与资源配套条件。请勿合并后重跑写入脚本。
- `python3 work/eudic-controls-deepening/check_batch.py`：exit0/PASS。检查编号唯一、E/F引用、原件/正式摘记SHA、Markdown本地链接、CSV LF、JSON/CSV等值及3个旧批SHA未变。
- `git diff --check -- docs/research/interaction-library/cases/M-026-eudic.md docs/research/interaction-library/batches/eudic-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/eudic-controls-deepening-coverage.csv docs/research/interaction-library/batches/eudic-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/eudic-controls-deepening/source-notes.md`：exit0。含未跟踪文件，实际内容校验以check_batch为准。
- 起始 `rg -n 'E-EUCD' docs/research/interaction-library --glob '*.jsonl' --glob '*.csv'`：exit1表示无碰撞；一次批量cat误含未取得的 `14-aiarticle-parsed.txt`，exit1，明确没有该原件。其余读取与解析命令exit0。

## 未决与状态

仍缺现版触屏查词→原句保存→练习闭环、词库损坏/替换、索引中断恢复、备份恢复/冲突、AI翻译校订/额度/导出/删除完整任务。不能以旧Help、摘要或政策凑这些流程。无账户、学习数据、文件服务、设备或额度状态变化；只写授权研究文件。
