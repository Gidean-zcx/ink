import csv,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'work/notebooklm-controls-deepening'
LIB=ROOT/'docs/research/interaction-library'
case=(WORK/'M-041-notebooklm.md.before').read_text()
case=case.replace('# M-041 NotebookLM：资料、练习、证据与音频的不同生命周期','# M-041 NotebookLM / Gemini Notebook：资料与 Studio 产物的生命周期')
case=case.replace('另用 2025-11-06、2026-03-20 的发布记录限定历史增量。未登录、生成或操作产品，无连续视觉观察。','另用 2025-11-06、2025-12-23、2026-03-20 的发布记录限定历史增量。未登录、生成、安装或操作产品，未观察界面图片或视频，无连续视觉观察。本轮 Android 分支以明确选择 Android 的英语帮助为准（文档要求 Android 10+），应用构建未知；网页与 App、年龄与套餐条件分别记录。文档路由变化不证明每个地区/账户已部署所有新功能。')
start=case.index('| 功能域 |')
end=case.index('\n## 任务流程',start)
case=case[:start]+'''按官方帮助目录及 Create a notebook 中的 Studio 清单逐项对照；数据表在后者出现，不能因主目录无独立文章而遗漏。[逐项目录与缺口](../batches/notebooklm-controls-deepening-inventory.csv)有 19 行，包含尚未展开的功能。索引标题不单独证明操作流程。

| 功能域 | 来源章节、F/E 对应 | 尚缺入口或分支 |
| --- | --- | --- |
| 来源收集、导入、搜索 | Sources: Add / Research；F01 | 重复去重、失败页与部分保留 |
| 来源同步、失效、移除 | Sources: Drive / YouTube；F02 | 人工删除确认、恢复、旧产物刷新 |
| 来源标签 | Sources: Label & Categorize；E-NBCD-02，D1 | 只有操作类别，未拆具体菜单 |
| 问答、引用、笔记 | Chat / Notes；F03、F07 | 矛盾来源核对、去引用后的溯源 |
| 卡片、测验、进度 | Cards: Study / Controls；F04、F05 | 人工改题、撤销自评、复习调度 |
| 音频、互动、离线 | Audio 旧帮助、Android Audio；F06 | 精确断点、缓存删除与跨端冲突 |
| 思维导图 | Mind Maps: Generate / Interact；F08 | 节点编辑、重生成时旧图状态 |
| 交互报告、文档报告 | Reports: Generate / Customize；F09 | 文本校订、嵌入内容导出、取消/恢复 |
| 数据表 | Create: Data Tables；F10 | 单元格校订、重复行处理、源变更传播 |
| 幻灯片、信息图、视频 | 各产物帮助；F11–F13 | 生成失败/取消、误删恢复、内容准确性 |
| 通知、导出与分享 | Create、各产物 Share；附属分支 | 通知拒绝、组织策略、下载副本回收 |
| 笔记本复制 | Create 与 FAQ；E-NBCD-04、E-NBCD-03 | 正文互相矛盾，不新增“已解决”的 F |
| 连接应用、Gemini / AI Mode、公共笔记本、套餐/设置 | E-NBCD-01 目录 | 未逐页展开；外层产品动作不能套本界面 |

本案例现有 13F，均为 **D2 文档流程**，不代表每个任务的全部分支完整；7 个旧 F 保留，新增 6 个独立产物任务。原件与 SHA-256 记录在新证据的 `raw_work_path/raw_sha256`；没有将 work 原件填入正式 `archived_artifact`，没有把 HTML 抓取当视觉观察。
''' +case[end:]
rows={
'01':'| F-M041-01 找到并选入学习资料 | 自有笔记本 → Add sources 导入本地资料；或搜索 Web/Drive → View 核对原文 → 选取导入。 | Deep Research 后台执行后选报告/来源；退出丢弃未导入结果，超限可能部分导入，另限 18+。超过 500000 词、上传超过 200MB 或受复制保护 PDF 可失败；重复去重及实际错误页未知。 | E-NBL-01、E-NBCD-02、E-NBCD-03 |',
'02':'| F-M041-02 更新来源并识别失效 | 打开 Drive 来源 → Click to sync with Google Drive；帮助亦描述自动更新。失权后可经提供的链接请求访问或从列表移除。 | 失权/原文件被删时不可用、仍占额度，不参与新问答和新产物；不改删 Drive 原文件。网页取文字、音频取转写、YouTube 取字幕；视频私密/删除后对应来源按文档在 30 天内自动删除。旧产物更新及人工删除确认/恢复未知。 | E-NBL-01、E-NBCD-02、E-NBCD-04 |',
'04':'| F-M041-04 生成并完成一次卡片/测验 | 有编辑权限和来源 → Studio 卡片/测验或铅笔配置 → 后台生成 → 作答；Hint 辅助测验，Explain 解释，前后箭头导航。 | 卡片自评 Got it / Missed it；结果可重练全部或错项，卡片可打乱、重启、删除及 CSV 下载，书源可能限制下载。配置不等于逐题校订；未见题干/答案文本编辑或自评撤销步骤，自评不证明掌握。 | E-NBL-02、E-NBCD-05 |',
'05':'| F-M041-05 中断后继续学习 | 部分学习后离开，再打开同一产物。 | 当前帮助也述保存进度；2026-03-20 旧发布证据补充单卡/单题删除、结果重练。本轮该发布页 HTTP 429，已停止复取。Android 同步延迟建议退出重开；精确断点、多端冲突和删题恢复未知。 | E-NBL-04、E-NBL-07、E-NBCD-05、E-NBCD-12 |',
'06':'| F-M041-06 听摘要并插入问题 | 原网页任务：Studio 生成 Audio Overview → 播放 → 符合条件的新音频 Interactive → Join → 等邀请后提问。Android 附属路径：Download 或 Studio 左滑下载 → 首页 Downloaded 找回。 | 原帮助称回答后回原音频，互动限英语。Android 可后台听和离线缓存，该缓存不能当独立文件下载。逐字朗读、离线互动、精确续播和缓存删除恢复未证实。 | E-NBL-05、E-NBCD-12 |',
'07':'| F-M041-07 保存自己的知识笔记 | Sources → Create note 或 Save to note；回答笔记移除引用后可编辑。旧 Studio 笔记打开 Convert 或 More 批量转换。来源笔记悬停 More → Export to Docs。 | 勾选笔记才参与问答/生成；Docs 在新标签页成为副本，编辑不回同步、权限不继承。移动端可看/选/问答，不能编辑。FAQ 明确已删除笔记不能恢复；删除入口/确认仍缺证。 | E-NBL-06、E-NBCD-06、E-NBCD-03 |',
}
lines=case.splitlines()
for i,line in enumerate(lines):
 for n,new in rows.items():
  if line.startswith('| F-M041-'+n+' '):lines[i]=new
case='\n'.join(lines)+'\n'
new='''
## 新任务：生成、审阅、导出与返回

| F 编号 | 前置、入口和动作 | 反馈、结束和分支 | 尚缺什么 / 证据 |
| --- | --- | --- | --- |
| F-M041-08 从思维导图分支追问并保存图 | 网页笔记本有来源 → 聊天区 Mind Map chip → Studio 打开生成的图 → 展开/折叠分支，选节点进入该主题问答。 | 可缩放、下载，右上退出；以后从 Studio 重开。帮助把 More → Delete note 写为重生成动作，但未解释中间状态。 | 下载格式、节点手改、误删恢复未知；App 帮助称不支持此功能。E-NBCD-07 |
| F-M041-09 生成报告并嵌入学习产物 | 有编辑权限 → Studio Reports → Interactive → Learning Overview；或 Document → 模板。模板旁 Edit 修改生成设置，再选 Generate now/later。 | 交互报告对建议产物点 Add 才嵌入，亦另列 Studio。打开 Share 可复制链接；报告 More 导出 Docs，报告内表格可导出各自 Sheets 页。 | 部分交互内容限 18+，生成/定制仅网页；嵌入互动导出形态、排程取消、删除恢复未知。E-NBCD-08、E-NBCD-04 |
| F-M041-10 将来源整理成带引用的数据表 | Studio Data Tables → 直接生成，或铅笔设语言、用提示描述行列 → 生成表 More → Export to Sheets。 | 新 Sheets 第一页为数据、第二页为引用；导出修改不回同步，权限不继承。2025-12-23 公告限定历史发布。 | 应用内改单元格、合并重复行、源变更传播、误删恢复未知；Android 尚不支持生成。E-NBCD-04、E-NBCD-13、E-NBCD-12 |
| F-M041-11 修订幻灯片并导出演示文件 | 18+、有编辑权限 → Studio Slide Deck → 设置 → Generate now/later。已有产物 Revise → 逐页填写修改，也可拖缩略图重排。 | 待改项集中显示；More → Delete slide 后页变灰，可 Restore slide；Generate revised deck 另生成新版且修订不参考来源。More 导出 PDF/PPTX；幻灯播放用箭头翻页、Esc 退出。 | Ultra 可先审大纲，Proceed 后不能再改该大纲；稍后生成跳过大纲。Android 文档仍称修订不能增删页。新版回滚、失败恢复未知。E-NBCD-10、E-NBCD-12 |
| F-M041-12 生成信息图并下载或删除产物 | 18+、有编辑权限 → Studio Infographic / 铅笔 → 设语言、详略、方向和提示 → 后台生成 → 按标题打开、缩放。 | More 可改名、下载 PNG、分享或删除；缩小/退出回笔记本。分享需相应笔记本权限，删除使旧链接失效。书源可能限制下载。 | 图内逐项修改、误删恢复、失败重试未知；未证明下载副本能回收。E-NBCD-09 |
| F-M041-13 审阅视频制作说明并播放产物 | Studio Video Overview → 配置 → Generate now/later；生成后按标题 Play、调速度/进度。Ultra 可先 Review producer notes → 就绪后 Review → 集中反馈 → Proceed。 | Proceed 后不能再改制作说明；稍后生成跳过审阅。可返回后打开、下载/分享；删除令旧链接失效，Android 另有 Studio 左滑删除入口。 | Cinematic 限 18+ 且英语。取消/失败、排程取消、下载格式、断网续播、误删恢复未知；未播放/下载视频。E-NBCD-11、E-NBCD-12 |

这些入口和反馈来自官方操作帮助，支持限定的 D2；“生成完成”不等于内容正确或已核对，不能把待审阶段、生成版本、打开状态及学习成绩合成同一个完成标记。

## 跨任务状态与未解冲突

| 对象 | 已有依据 | 不能据此推出 |
| --- | --- | --- |
| 取消勾选与删除来源 | 创建帮助以 unselect 描述 remove；来源/笔记专页把勾选用于上下文选择。E-NBCD-02、E-NBCD-04、E-NBCD-06 | 取消勾选即物理删除；人工删除菜单、确认、撤销仍未知 |
| 完成通知和未读 | Settings → Notifications 开启并获浏览器允许后通知；Studio 蓝点在打开前跨网页会话保留。E-NBCD-04 | 打开一次或通知送达即核对了内容；拒绝权限/通知丢失分支未知 |
| 笔记本复制 | Create 写 Copy → Create，复制来源与 Studio 内容，不复制聊天/笔记；可设置 Allow copies。FAQ 仍称不支持复制。E-NBCD-04、E-NBCD-03 | 不判定哪项部署有效；此冲突也不能回答重复来源去重。04 原始 HTML 的 `nb_copy` 章节有该正文 |
| 幻灯片局部恢复 | 桌面有待删除页 Restore，提交后生成新版。E-NBCD-10 | 不等于删除整套产物可恢复，也不等于 Android 同时具备该控制 |
| 删除产物/笔记/整本 | FAQ 称笔记不能恢复；信息图/幻灯片/视频帮助称删除令链接失效。E-NBCD-03、E-NBCD-09、E-NBCD-10、E-NBCD-11 | 未找到产物回收站/回滚承诺；整本删除连带范围及恢复缺本界面操作链 |
| 出口与权限 | 各类文件出口见 F04、F07–F13；部分书源有限制，Docs/Sheets 副本不回同步或继承权限 | 未说明的格式不猜；导出副本不自动受原链接撤销影响，离线音频不等于文件出口 |
| 移动端范围 | Android 帮助未提供笔记/导图/报告/表格生成，已述学习/音频/图/幻灯片分支。E-NBCD-12 | 桌面菜单不能直接套 App；无构建号，也不能排除帮助更新不同步 |

本轮已读官方目录、来源/FAQ、笔记、卡片、本轮新增 Studio 产物帮助（音频沿用旧证据）、Android 差异与数据表公告；另检索官方域的 delete notebook / duplicate source / remove source。检索出现 Gemini Apps、AI Mode 外层入口时未借用为本界面动作，也未用社区回复补事实。旧 2026-03-20 公告本轮返回 429 后停止；web 工具部分正文超时则只以成功取得的公开 HTML 核文。取得记录保存在 `work/notebooklm-controls-deepening/fetch-results.json`。

'''
case=case.replace('## 关键判断与空白',new+'## 关键判断与空白')
case=case.replace('来源变更不会自动证明旧练习或旧音频已重生成。','本轮补充的设计线索（推断）：幻灯片修订不参考来源，说明“改成功”与“仍有依据”可能分离；待审大纲、最终版本、导出副本必须区分。对 R-016 / G-002，建议把来源选中范围、生成配置和各阶段状态纳入 AI 可读可控对象。这不表示竞品已提供可供本项目使用的全量 API。\n\n来源变更不会自动证明旧练习或旧音频已重生成。')
case=case.replace('后续观察：','后续观察（本轮未执行，目录登记不等于全功能完成）：')
case=case.replace('另验证听音频插问后的真实恢复点。当前没有执行。','另验证听音频插问后的真实恢复点、重复来源、删除确认/恢复，以及网页与 Android 同一笔记本的修订差异。需要明确构建与账户范围的官方录屏或获授权实际观察。当前没有执行。')
case+='\n### 本轮新增来源\n\n查阅日均为 2026-10-26；除数据表公告外无可靠更新日期。均读取官方文字，不代表已操作；E-NBCD-01 是索引，其余是正文。E-NBL-04 本轮因 HTTP 429 未复取，沿用旧证据。\n\n'
for line in (LIB/'batches/notebooklm-controls-deepening-evidence.jsonl').read_text().splitlines():
 e=json.loads(line)
 case+=f"- {e['evidence_id']}：[{e['title']}]({e['url']})，{e['locator']}"+('；公告日期 2025-12-23。' if e['source_updated_on'] else '；正文版本/更新时间未标。')+'\n'
(LIB/'cases/M-041-notebooklm.md').write_text(case)
print('WROTE case',len(case),'characters; 13 stable/new task rows')
