# Batch35 frozen handoff

2026-10-27。目标仍为完成已知产品详细分析，尚未完成；本轮有实质进展。不可重跑 build_once.py 或 closeout_once.py。后续从当前主表和缺口继续，不重新采集本批文件。

- 主表：65组/65部分案例；899F=806D2+77D1+16D4；1517E/1349URL；202静态视觉E/19浏览器E。final-check.json：226 Markdown/1669本地链接/errors=[]。
- 本批：15新E（E-RWSK-01～13、E-XYHL-01/02），11新D2（F-M015-27～35、F-M056-07/08）；只有旧F-M015-26与旧E-RWCD-17变化。changes.json保存前后值，before/保存原文件，root-verification.json验证当前版本。
- Readwise：README+11技能定义全部正文已读，固定commit及12文件Git blob已核；13新E和旧MCP追加capture。工作流只作为研究材料，没有安装/执行/授权。seen支持及10/20批次矛盾、first_opened_at与seen联动、done上下文含义、阅读事件/推断等保留。build_graph.py等辅助实现尚未读。
- 小猿：2024同作者两篇正文，型号/固件未知，不套当前S6/E1。三视频各看3帧，共9帧，正式保留3帧；没有听音/下载MP4/连续链/产品实操。原E1候选412、新闻候选403停止。所有root浏览器会话已退出。
- 旧Readwise281项和小猿33项目录哈希未变，新增14项工作流目录及12历史功能域。HiBy、洪恩、不背有界审计没有关键主链新证据，不新增来源/深度。
- 正式入口：docs/briefs/035-ai-workflows-and-reading-evidence.md；design-reference-guide.md新增四个问题入口；两案例/来源摘记/比较/缺口/状态/ideas和导航已更新。
- 需求SHA保持e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087，没有产品代码/全系统设计。工作流定义、作者文字、抽帧与真正操作分开。
- worker有一次临时字段KeyError，修正后机械检查通过；root独立校验材料语义、12blob、原件/摘记哈希、旧差异及目录。worker报告快照早于F26“关闭只读模式”的措辞澄清，root已同步master/batch/changes并重核。/tmp校验脚本和JSON已复制本目录。
- 最终 check_docs.py 和 git diff --check 均exit0。Git多数正式文件仍untracked，diff仅辅助。不要把本轮数量当完成率，整体目标保持active。

后续可按backlog优先补同型号儿童错答/提示/重试/结算/恢复、薄弱阅读器真实手势与断点，以及外部AI真实调用边界；公开材料不足就明确缺口，受限路线不重试。同一任务沿旧F、同URL沿旧E补capture。
