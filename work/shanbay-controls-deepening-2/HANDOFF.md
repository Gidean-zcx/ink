# M019 扇贝第二次控制补证交接

查阅2026-10-27；已收束，无待执行网络或设备动作。原指定shanbay-controls-deepening已是已合并批；已告知root并采用独立后缀 **shanbay-controls-deepening-2**，未覆盖旧批。

## 数量与合并建议

- 新E **7**：E-SBC2-01目录；02旧听力揭晓门槛；03午夜/记录；04单词量按钮；05扩展帮助；06扩展数据政策；07公司D1介绍。全库JSONL检查没有新ID或相同URL冲突。
- 新F **0**；coverage文件仅标准表头。原11F不变（10 D2、1 D1），不因补分支增加重复任务。
- 旧F01/03/04/05/06/08/10共**26逐字段建议**：`work/shanbay-controls-deepening-2/existing-coverage-updates.json`；同值UTF-8/LF CSV。
- 旧E追加 **7条 additional_captures 建议**：`work/shanbay-controls-deepening-2/additional-captures-updates.json`，每条有before/after。涉及E-SBY-04/05/06/10/11/12及E-SBCD-02。这些是2026-10-26旧响应在2026-10-27本地复核，不是新网络抓取或新E。
- 目录 **68行**：12父栏目+34文章+4独立商店条目/1雅思指南+14扩展帮助小节+3产品声明域。只有8栏目文章已取，另外4栏目未取；父子项与其他平台不算完成任务数。

## 正式改动（5文件）

1. `docs/research/interaction-library/cases/M-019-shanbay.md`
2. `docs/research/interaction-library/batches/shanbay-controls-deepening-2-evidence.jsonl`
3. `docs/research/interaction-library/batches/shanbay-controls-deepening-2-coverage.csv`
4. `docs/research/interaction-library/batches/shanbay-controls-deepening-2-inventory.csv`
5. `docs/research/interaction-library/artifacts/shanbay-controls-deepening-2/source-notes.md`

主台账、旧批、根文档未改；没有媒体或截图artifact。完整原件/文件散列在 `hash-manifest.json`，检查结果 `validation.json`。

## 根可抽核的正文

- **E-SBC2-02/F06**：[旧听力文章](https://web.shanbay.com/web/help/bbumgi/bchgxx)，原件 `sby-col-bbumgi.json`中objects[id=bchgxx].content_html，提取副本article-bchgxx.txt。产品自述段给至少三遍后揭晓、刮开、未懂句再安排、之后关键词测试/对照回听。包含宣传，但没有将效果承诺当结论；现版阈值未知。原JSON SHA `cd01612c5c90b2c105b2cd250c80d825c53cc014b4da319978ebc7dd7f319a99`。
- **E-SBC2-03/F08**：[为什么打卡](https://web.shanbay.com/web/help/svgln/zguwz)，原件sby-col-svgln.json，objects[id=zguwz]的“怎样打卡”；SHA `7676d689d2c0715c193a1cc6509c5fa177fb381281fc351e1f2db645e49941d8`。笼统午夜不能补与E-SBY-12“已完成上传，仅遗漏打卡”七日期限分范围保留；不宣布当前统一日界线。
- **E-SBC2-04/F03**：[单词量按钮](https://web.shanbay.com/web/help/blmqnc/orqdq)，sby-col-blmqnc.json对象orqdq；SHA `bab9e0f69216899b04f212c48c933d0da5a1d708efcbe66f10bf021cb49d6448`。历史App我的→单词书→每日新学量右侧按钮；未给保存反馈。
- **E-SBC2-05/F10**：[扩展帮助](https://web.shanbay.com/bay-translate/support)，2026-09-14，1.1/1.3/2.1/4.1/4.4；原HTML translate-support.html SHA `1373f8578330528174bffa5f98790c7f40a600089a81fa5d93879721f4a0bc4c`。Safari站点权限、词卡与相同账号要求，只属于浏览器扩展；恢复购买不算恢复学习数据。
- **E-SBC2-06/F10数据边界**：[扩展政策](https://web.shanbay.com/bay-translate/privacy)，更新/生效2026-09-24，1.2.2；translate-privacy.html SHA `22264a5da1439c6a10fc80bae1acba454fd9e5bce4b4aa1017279e27d0ff84c5`。主动加词关联词条ID/来源URL，标题是部分场景；政策声明不能证明手机原句UI、入库成功或同步冲突。
- **E-SBC2-07仅D1**：[公司单词介绍](https://web.shanbay.com/company/jieshao)，未给日期/版本。company.html SHA `8748b51e5df1b1d6549497be2f12d8df33e08074854e5108d846f432f655194c`。AI助记“超过3次”是举例，不当实际稳定阈值；上传词表/实时草稿/导出没有完整控制链。

前三篇复用2026-10-26保存的公开CMS栏目JSON，未再次请求。前端公开路由为 `https://apiv3.shanbay.com/cms/v2/columns/{id}/articles?page=1&ipp=50`，正文在对象content_html；assign_date为编排字段，不是更新日。原件现复制到本批work，原旧work未动。

## 实际命令、工具与退出状态

工作目录 `/home/workspace/reader`。

- `rg --files docs/research/interaction-library work | rg 'shanbay|M-019|SBCD'`、读取当前case/旧E/F/旧脚本与CMS：exit0。初始检查新前缀E-SBC2/E-SBCT和新公开URL无命中；最终check再次全库核重。
- `web__run`两轮检索“撤销/导入/录音重来/同步离线”等及一次打开扩展帮助/公司页/wordsweb。首页JS壳不作正文；第三方教程、逆向账户工具未用。首轮一条查询误写site.shanbay.com，后续已改site:shanbay.com；未因此采用非官方结果。
- `python3 work/shanbay-controls-deepening-2/fetch_public.py`初次exit1，原因bs4未安装，**未发请求**。改用Python标准库HTMLParser，未安装包；第二次exit0，3正文HTTP200、扩展首页重定向后HTTP404且不重试，详见http-results.json。最大3并行，timeout=(5,18)。
- `python3 work/shanbay-controls-deepening-2/prepare_local.py`：exit0；只复制已有八栏目/JS/HTML并抽目录及指定正文；没有新的CMS请求。
- `python3 work/shanbay-controls-deepening-2/write_batch.py`：exit0，7E/0F/68目录/26旧F字段/7旧E附加记录。
- `python3 work/shanbay-controls-deepening-2/update_case.py`：exit0；写当前case。
- `python3 work/shanbay-controls-deepening-2/check_batch.py`：exit0/PASS；检查ID/URL全库碰撞、E/F引用、原件与正式短摘记SHA、本地链接、表格连续性、CSV LF与JSON等值、4旧批SHA未变。
- `git diff --check -- docs/research/interaction-library/cases/M-019-shanbay.md docs/research/interaction-library/batches/shanbay-controls-deepening-2-evidence.jsonl docs/research/interaction-library/batches/shanbay-controls-deepening-2-coverage.csv docs/research/interaction-library/batches/shanbay-controls-deepening-2-inventory.csv docs/research/interaction-library/artifacts/shanbay-controls-deepening-2/source-notes.md`：exit0。存在未跟踪研究文件，不能以空git diff代替Python检查。

不要在根合并或修订后重跑写入脚本。仅文档检查，没有产品测试、helper调用、安装/登录、提交、媒体/APK下载。

## 未决与状态

仍缺现版手机误评撤销、简单词恢复、口语录音起止/提交/重录、离线队列、跨端冲突和账户学习数据恢复。旧听力星级的30/60边界含混，未推为精确算法。四个App Store版本沿用2026-10-26快照，不称2026-10-27最新版本。

旧指南图片之前返回浏览器校验内容，本轮不重试。没有视觉观察、连续体验或实机验证。只有授权资料文件变动，产品、账号、学习数据与额度均未改变。
