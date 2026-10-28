from pathlib import Path
import json,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch35-integration'
assert (W/'applied.json').exists() and not (W/'navigation-applied.json').exists()
before=json.loads((W/'before-manifest.json').read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p):
    if str(p) not in before:
        q=W/'before'/p.relative_to(R);q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(p.read_bytes());before[str(p)]=sha(p)
brief='[第三十五期：AI工作流与阅读证据](/home/workspace/reader/docs/briefs/035-ai-workflows-and-reading-evidence.md)'
for p in [R/'README.md',L/'README.md']:
    save(p);t=p.read_text();marker='- [第三十四期：'
    i=t.index(marker);t=t[:i]+f'- {brief}：外部AI逐篇筛选、订阅批次、测验与画像；小猿历史订正/听书链，保留版本和运行证据缺口。\n'+t[i:];p.write_text(t)
p=L/'design-reference-guide.md';save(p);t=p.read_text();mark='| AI批量修改学习资料'
i=t.index(mark);extra='''| AI怎样给我一批新闻，并允许结束 | [Reader F27/28](cases/M-015-readwise.md) | [E-RWSK-04/05固定工作流与冲突](artifacts/readwise-ai-workflows/source-notes.md)；[官方订阅工作流](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/feed-catchup/SKILL.md) | 取样范围、推荐理由、当前批次与写入对象；next不能默认等于全读完，未运行 |
| AI能否从阅读出题，再形成可保存的学习成果 | [Reader F29/31/32](cases/M-015-readwise.md) | [E-RWSK-06/08/09](artifacts/readwise-ai-workflows/source-notes.md)；[官方测验工作流](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/quiz/SKILL.md) | 逐题等待/评分与真实掌握分开，生成/写回/导入题库分别验证；没有登录执行 |
| 个性化会不会把我限制在旧兴趣中 | [Reader F30/34/35](cases/M-015-readwise.md) | [E-RWSK-07/11/12](artifacts/readwise-ai-workflows/source-notes.md)；[画像原文](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/build-persona/SKILL.md) | 历史缺席不是排斥，最后打开不是完整阅读事件；推断、用户纠正和持久保存分别核对 |
| 儿童订正通过和首次独立答对怎样区别 | [小猿历史F07](cases/M-056-xiaoyuan-hardware.md) | [E-XYHL-01作者文字与抽帧](artifacts/xiaoyuan-historical-learning/source-notes.md)；[原报告](https://post.smzdm.com/p/a3x3nk5d/) | 2024未知型号不套S6/E1，等级阈值/误判修正/奖励到账与同题完整操作仍缺 |
''';t=t[:i]+extra+t[i:];p.write_text(t)
p=L/'comparison.md';save(p);p.write_text(p.read_text()+'''\n## 第35批补充：AI工作流中的状态与推断\n\n| 比较问题 | 新依据 | 可参考的差别与边界 |\n| --- | --- | --- |\n| 新闻的有限批次和完成 | [Reader F27/28](cases/M-015-readwise.md)，[固定定义](artifacts/readwise-ai-workflows/source-notes.md) | 逐篇选择、整批标已见、停止汇总是不同动作；推荐5篇来自20篇样本，不保证全源多样性。仅工作流定义 |\n| 阅读之后如何练习和输出 | [Reader F29/31/32](cases/M-015-readwise.md) | 每题等待与分级反馈、从批注回顾、书评生成后写回；评分无稳定量规，历史位置/更新不证明掌握，失败产物出口不证明写入已测 |\n| 学习记录的意义 | [Reader F30/34/35](cases/M-015-readwise.md)、[小猿历史F07/08](cases/M-056-xiaoyuan-hardware.md) | 兴趣推断、最后打开、首次正确与订正、音频位置各自限定；历史机型未知，不能为当前设备补证 |\n\n以上支持设计问题和验证脚本，未决定本项目采用这些阈值、工作流或运行时。\n''')
p=L/'inventories/README.md';save(p);p.write_text(p.read_text()+'''\n第35批新增两个有明确范围的目录：[Readwise 14项](../batches/readwise-ai-workflows-inventory.csv)含固定仓库README、11份定义和两篇工具帮助，全部只读未运行；[小猿历史12域](../batches/xiaoyuan-historical-learning-inventory.csv)按2024作者文章列功能名，仅学习/听书补局部文字任务。它们分别补既有帮助目录与历史样机范围，不能与现款菜单合并计算完成率。\n''')
p=L/'backlog.md';save(p);p.write_text(p.read_text()+'''\n## 第35批后的具体证据缺口\n\n- Readwise：固定客户端/服务版本核seen过滤与下一批10/20冲突；逐篇歧义回应、20项批量写入部分成功、撤销/恢复、查询分页尽头；测验评分/退出续答/结果写入。工具参数例子不是动态schema或运行回执。生成图脚本尚未检查；画像对错误推断的用户修正和各家庭成员分离仍未知。见[固定来源](artifacts/readwise-ai-workflows/source-notes.md)。\n- 小猿：2024同作者样机现补文字链，不能抬高当前S6/E1。三视频九帧只有题面/诗文等，仍需同型号同题错误→提示→重试→结算及退出恢复。E1视频正常访问412、新闻来源403后停止，没有重试。\n- 洪恩：官网视频此前已抽样，不再登记为未打开候选。本轮官方教程检索未获得新直链；重点仍是同字错选/错笔/录音失败、提示后重试及保存。\n- 不背：新发现十周年BV12H4y1n7rh仅搜索索引、账号官方性未独立核，未观看；不优先于教学材料。“不背学堂/快速上手”只有转载线索，没有原厂视频直链。当前同词拼错→提示→重拼、组结束及04:00跨日恢复仍缺。\n- HiBy：复读中英型号指南/X1600 FAQ，没有新增歌单编辑或M3U导入按钮链；旧F04/F05保持D1。UTF-8 BOM是推荐，playlist_data约定与路径解析位置分开；已有记录无需重复新增F/E。\n\n有界排查详见work/research/batch35-child-learning/source-audit.md、batch35-hiby/report.md。它们是检索/复读日志，不能当作新增产品行为证据。没有取得公开材料不表示产品没有该功能。\n''')
p=L/'analysis-status.md';save(p);t=p.read_text();marker='本轮之前的工作属于';i=t.index(marker);t=t[:i]+'''第35批继续补证：Readwise固定11份工作流/工具定义、原MCP页补读及CLI只读边界；小猿2024作者样机两任务另列，当前S6/E1不升级。新增15E/11D2，旧外部AI任务由用途声明补到文档链，原E追加1次采集；九视频摘帧正式保留三帧，没有新产品实操。洪恩/不背/HiBy的限定排查未补关键链，具体缺口进backlog。所有65组仍部分覆盖。\n\n'''+t[i:];p.write_text(t)
p=R/'docs/ideas.md';save(p);p.write_text(p.read_text()+f'\n2026-10-27继续I-021/I-022：Readwise固定工作流深入逐篇/分批筛选、逐题回答、画像推断和书评写回失败；小猿2024历史样机补提交订正与听书，不套当前型号。见{brief}。区分已见、归档、读完、独立掌握及真实/推断状态仍是研究建议；未新增已确认需求或进入全系统设计，65组仍部分覆盖。\n')
(W/'before-manifest.json').write_text(json.dumps(before,ensure_ascii=False,indent=2)+'\n')
(W/'navigation-applied.json').write_text(json.dumps(dict(files=len(before),requirements_sha256=sha(R/'docs/requirements.md')),ensure_ascii=False,indent=2)+'\n')
print('navigation updated')
