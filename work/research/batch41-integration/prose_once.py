import csv
from pathlib import Path

R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
def put(p,s):p.write_text(s.rstrip()+'\n')
def append(p,s):put(p,p.read_text().rstrip()+'\n\n'+s)
assert not (R/'docs/briefs/041-news-reading-and-retention.md').exists()
p=L/'cases/M-044-inoreader.md';s=p.read_text()
s=s.replace('**16条任务均D2文档重建，无登录、规则执行、AI生成或App操作。**实际查看两张官方编辑历史示意截图，未操作图内控件，不算连续流程。','**17条任务均D2文档重建，无登录、规则执行、AI生成或App操作。**此前两张编辑历史图，加本批12张官方静态图共14张；没有操作图内控件，不算连续流程。本批另读原目录38篇帮助及4个博客/功能来源；短FAQ不逐篇另算新任务。')
s=s.replace('邮件/网页监控/密码源全过程、OPML冲突、删源恢复及当前移动菜单','邮件源创建/取消后的实际结果、网页监控/密码源全过程、OPML冲突及当前移动菜单')
s=s.replace('被过滤条目回看及删除传播','移除记录的实际内容/恢复及删除传播')
s=s.replace('手势/布局、精确阅读位置、全文失败、历史保留及批注修改删除','布局的实际切换、精确阅读位置、全文失败、历史保留及批注修改/删除结果')
s=s.replace('文档称web/app/API均不显示过滤条目；回溯恢复、误去重与停用后的旧数据仍未知。','功能页称过滤内容不出现在常规阅读范围；2024博客另给Removed today数量→移除记录入口。记录回看不等于可恢复；当前回溯范围、误去重与停用后旧数据未知。历史图有冲突和过去30天提示，与当前新文章说明分别保留。')
s=s.replace('已保存/加标签的条目取消订阅后仍在个人账户；普通源历史继续存在有“该源仍有读者”的条件。','普通源文档说已保存/加标签条目取消订阅后仍在个人账户，普通源历史继续存在有“该源仍有读者”的条件；不把这条规则直接套到邮件源F17。')
s=s.replace('修改/删批注、断网同步及取消连接后副本去留未知。','本批静态图已看到Edit/Delete入口；修改/删除实际结果、撤销、断网同步及取消连接后副本去留仍未知。')
patches={
 '01':('；2024记录入口及静态编辑器见下文','E-INRS-39'),
 '03':('；百分比、预计时长和0%多选归档不能当理解/完成证据','E-INRS-05～13/17/40'),
 '04':('；RSS原本可能只有标题/摘要，先区分发布者输入和提取失败','E-INRS-04'),
 '05':('；帮助分别给iOS右滑区域/Android长按的离线入口，要求联网下载后核单篇离线标识','E-INRS-17/34～38'),
 '06':('；静态播放器有速度、进度、混合队列和Clear all，未听音或执行','E-INRS-24～27/39'),
 '09':('；布局可按源/文件夹/标签分别设置，按源/日期分组；历史X可标已读并隐藏，撤销未知','E-INRS-01～03/39'),
 '10':('；帮助选高亮后Add note与旧Edit入口分别保留，已有笔记不证明来自当前选区','E-INRS-28～33/40'),
 '12':('；静态33%为设置清单，不能借作学习掌握率','E-INRS-39'),
 '13':('；本批列表图含颜色、名称、创建日期和状态开关，未操作','E-INRS-40'),
 '16':('；归档图中4篇多选与单行/Archive all分开，未见执行结果','E-INRS-40'),
}
out=[]
for line in s.splitlines():
    matched=False
    for n,(extra,e) in patches.items():
        if line.startswith('| F-M044-'+n+' '):
            cells=line.split('|');cells[-3]=cells[-3].rstrip()+extra+'。 ';cells[-2]=cells[-2].rstrip()+'；'+e+' ';line='|'.join(cells);matched=True;break
    out.append(line)
s='\n'.join(out)+'\n'
s=s.replace('Team、社会源、网页变化跟踪、通讯简报、全部菜单/接口及账号恢复还留有目录缺口','Team、社会源、网页变化跟踪、邮件源的真实结果/恢复、全部菜单/接口及账号恢复还留有目录缺口')
put(p,s)
append(p,'''## 新增：邮件源创建、收信与取消（F-M044-17）

前提为相应Newsletter权限。Add feed→Newsletter feed→名称与专用地址→Create email address；再到发布者使用该地址订阅，或从既有邮箱转发。功能页描述确认邮件进入源后再确认订阅，之后读取来信。静态图有名称至少5字符提示、@ino.to地址字段和管理入口，但没有校验、创建成功或确认邮件结果。未到信时排查地址、仍关注该源及发布者确认，不能承诺重投。（E-INRS-19～23/41/42）

取消关注的官方口径未统一：帮助说地址及历史邮件删除、失去访问；功能页说停止收信，保留90天且期内重新关注可恢复。可能涉及访问/后台保留的区别或版本变化，未操作前不能统一成确定的删除或恢复规则。多个发行者用同一地址时，停用地址影响多个来源；原私人邮箱的另一份订阅不自动停止。

**设计复查入口：**[本批逐项摘记与12幅画面](../artifacts/inoreader-reading-state/source-notes.md)、[42来源的范围/日期/散列](../artifacts/inoreader-reading-state/source-manifest.json)。归档图与继续阅读入口、预览/保存、过滤记录/恢复、区域启用/单篇下载分别记录。邮件源只增加这一个任务；其余细节补回原F。本批没有给公开API的页面覆盖、黑白屏体验、学习收益或实机恢复补证。
''')

p=L/'cases/M-046-ground-news.md'
append(p,'''## 目录正文补读：联网、语言与版本范围（2026-10-27）

原目录四篇补正文，保持21个任务，不把简短功能说明另算交互。原址、日期和静态套餐图见[来源范围核查](../artifacts/ground-source-scope/source-notes.md)。

| 补回任务 | 已有依据与限制 | 来源 |
| --- | --- | --- |
| F-M046-02、F-M046-16 | Web/App需网络，无离线浏览；相同凭据可访问权益，不证明全文缓存、阅读位置或全数据同步 | E-GNRS-01 |
| F-M046-16 | 套餐页及一张概览图列权益，仍残留Vantage移动版即将推出的措辞；不能把更新时间当所有描述已核准，也不作当前套餐保证 | E-GNRS-02 |
| F-M046-01、F-M046-22 | 多源聚合/Blindspot为机制声明，三评级机构措辞未解决已有口径差异；不等于单篇事实核验 | E-GNRS-03 |
| F-M046-06、F-M046-11 | 非英语来源按原语言展示；与移动翻译能力分开，不能由此推断中文UI、自动翻译或全部中文媒体覆盖 | E-GNRS-04 |

没有登录、离线尝试、订阅更改或实际界面操作。四篇各一次GET均200；全局安全脚本/CSP曾触发抓取器误判，离线核查保留正常文章正文及纠正记录，没有把误判报告当产品行为。
''')

# Reusable lookup points reference existing evidence, without multiplying tasks.
rp=list(csv.DictReader((L/'reference-points.csv').open()));assert len(rp)==26
base='artifacts/inoreader-reading-state/'
def add(n,tags,fs,es,locator,artifact,pre,action,visible,basis,persist,unknown,version,product='M-044'):
    rp.append(dict(zip(rp[0].keys(),[f'RP-M{product[-3:]}-{n:02d}',product,tags,fs,es,locator,artifact,pre,action,visible,basis,persist,unknown,version,'2026-10-27'])))
v='官方网页帮助/静态图；查阅2026-10-27，实际App构建未固定'
add(1,'阅读;进度;收益;继续','F-M044-03','E-INRS-08;E-INRS-40','阅读习惯博客列表图',base+'active-reading-image-09.png','Saved→Read later，All页签','查看Continue reading入口及列表','预计长度/百分比/保存日期分列','official_static_images','只见状态展示，未观察恢复锚点','继续页结果;理解程度;重启;同步','博客标2024-11，图片为2025-06；'+v)
add(2,'归档;批量;范围;完成','F-M044-03;F-M044-16','E-INRS-40','归档多选图',base+'active-reading-image-10.png','列表4项选择，含0%进度','批量归档、单行归档与Archive all不同入口','工具栏与范围可见，未见归档结果','official_static_images','未观察写入/撤销','批量完成;误触;撤销;永久删除','官方静态组合示意；'+v)
add(3,'导入;网页;预览;保存','F-M044-03','E-INRS-12;E-INRS-40','Add web page图',base+'active-reading-image-11.png','网页保存弹窗','URL→Preview→Save web page（图示控件）','标题/缩略图/描述预览；背景已有列表不是新结果','official_static_images','保存结果未观察','解析失败;重复;付费墙;取消','博客旧日期，图片2025-06；'+v)
add(4,'过滤;回看;恢复','F-M044-01','E-INRS-39','2024改版正文：Filters',base+'source-notes.md','过滤dashboard有Removed today数量','点数量查看移除记录（文档路径）','声明可回看记录，未见列表结果','official_help_documented','不证明可恢复被过滤内容','恢复按钮;历史范围;当前版本;误过滤','2024-10改版说明；'+v)
add(5,'过滤;冲突;历史;取消','F-M044-01','E-INRS-39;E-INCD-04','过滤编辑器静态图',base+'redesign-image-11.png','编辑器及条件列表','Keep/Remove与组合条件、Save/Cancel入口','冲突警告及过去30天提示','official_static_images','取消/保存和历史重算未执行','当前创建/编辑差异;回溯范围;撤销','2024历史图与当前新文章处理说明并存；'+v)
add(6,'隐藏;已读;误触','F-M044-09','E-INRS-39','2024改版Article listing段',base+'source-notes.md','设置启用Mark as read and hide','点行X（文档规则）','组合标已读与隐藏，不表示删除或掌握','official_help_documented','传播/撤销未验证','误触;同步;当前移动版','2024-10说明；'+v)
add(7,'批注;编辑;删除;保存','F-M044-10','E-INRS-30;E-INRS-40','批注图',base+'active-reading-image-12.png','顶部已有笔记、正文选区','Edit/Delete与Highlight text/Add note分别可见','入口存在，已有笔记未证来自本选区','official_static_images','未见提交/删除/撤销结果','取消;确认;断网;原文更新','图片2025-06；旧Edit/新Add note措辞分别保留；'+v)
add(8,'离线;下载;可用;同步','F-M044-05','E-INRS-36;E-INRS-38','移动离线启用/排障帮助',base+'source-notes.md','区域支持离线，当前在线','iOS右滑区域/Android长按→启用→下载→核单篇标识','文档区分启用区域与单篇下载完成','official_help_documented','重连同步为声明，无冲突结果','下载中断;容量;失效;音频缓存',v)
add(9,'邮件源;创建;校验;确认','F-M044-17','E-INRS-20;E-INRS-42','创建邮件源图',base+'newsletter-blog-01.png','Add feed→Newsletter feed','名称与专用地址→Create email address','名称最少5字符提示、@ino.to、管理入口','official_static_images','没有创建成功或确认邮件结果','重复地址;名称校验;确认失败;重投','2026-08博客；'+v)
add(10,'取消订阅;删除;恢复;冲突','F-M044-17','E-INRS-22;E-INRS-41','帮助与功能页保留口径',base+'source-notes.md','已有邮件源及历史邮件','Unfollow（本轮未执行）','帮助说删除失访问；功能页说90天可恢复','official_help_documented','物理删除/可见性/后台保留未统一','恢复期限;重复关注;多发行者;真实删除',v)
add(11,'音频;队列;清除;续播','F-M044-06','E-INRS-26;E-INRS-27;E-INRS-39','改版播放器图',base+'redesign-image-13.png','混合文章/播客队列画面','播放控件、速度、Clear all入口','显示当前进度与队列，未听音/点击','official_static_images','重启和跨端秒点未知','清队列是否停播;寻位;来电;离线','2024-10改版图；'+v)
add(1,'收藏;离线;账户;同步','F-M046-02;F-M046-16','E-GNRS-01','Where Can I Read联网注释','artifacts/ground-source-scope/source-notes.md','Web或移动阅读、已有账号','收藏/同凭据访问权益（文档范围）','明确浏览需网络；不把收藏当全文下载','official_help_documented','无离线浏览支持；位置/备份未证','原文消失;网络失败;跨端位置','帮助正文2026-03-16；未固定App构建','M-046')
assert len(rp)==38
with (L/'reference-points.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rp[0]));w.writeheader();w.writerows(rp)
p=L/'design-reference-guide.md';s=p.read_text().replace('当前26条覆盖AiPaper历史任务、The Week Junior已有操作样例及墨墨输入/同步细节','当前38条覆盖AiPaper历史任务、The Week Junior已有操作样例、墨墨输入/同步及Inoreader/Ground新闻状态细节')
marker='| AI发出同一个手势前，要先读取什么状态 |'
idx=s.index(marker)
s=s[:idx]+'''| 读了多少、是否读懂与归档，怎样分别判断 | [Inoreader F03/16](cases/M-044-inoreader.md) | [阅读进度图](artifacts/inoreader-reading-state/active-reading-image-09.png)、[批量归档图](artifacts/inoreader-reading-state/active-reading-image-10.png)；RP-M044-01/02 | 预计时长不是实际投入，百分比不是掌握；选中含0%条目不证明归档结果 |
| AI保存网页后，怎样核对真正完成 | [Inoreader F03](cases/M-044-inoreader.md) | [预览/保存分离](artifacts/inoreader-reading-state/active-reading-image-11.png)；RP-M044-03 | 缩略图与标题不是完整正文或已入库；失败、重复与撤销未知 |
| 新闻被过滤或退订后，能否找回 | [Inoreader F01/17](cases/M-044-inoreader.md) | [日志路径、历史范围和退订冲突](artifacts/inoreader-reading-state/source-notes.md)；RP-M044-04/05/10 | 回看不等于恢复；取消邮件源的删除/90天保留口径未统一，不能承诺 |
| 收藏和开启离线，是否已经能在断网时阅读 | [Inoreader F05](cases/M-044-inoreader.md)、[Ground F02](cases/M-046-ground-news.md) | [单篇离线标识](artifacts/inoreader-reading-state/source-notes.md)、[联网限制](artifacts/ground-source-scope/source-notes.md)；RP-M044-08、RP-M046-01 | 分开意图、下载完成和账户权益；没有实际断网、冲突或重装验证 |
'''+s[idx:]
put(p,s)
p=L/'data-dictionary.md';put(p,p.read_text().replace('当前26条增量示例','当前38条增量示例'))

put(R/'docs/briefs/041-news-reading-and-retention.md','''# 第四十一期：新闻读到哪里，处理后还剩下什么

2026-10-27。你的质疑成立：早期研究偏功能盘点；后续已加入操作帮助、手册、界面图和少量公开网页实际操作，但**仍未完成每个产品的每个功能交互**。65组全部是部分覆盖。记录数不能充当深度或完成率；目前最明显的不足仍是同一版本的连续操作、异常反馈和持久恢复。

本轮深入个人新闻的阅读、过滤、保存和停止订阅：补读Inoreader原目录38篇帮助，另核3篇博客、1个功能页与12张图；Ground补4篇正文及1张套餐图。沿用16个旧任务，新增一个邮件源生命周期任务。这些都是文档和静态画面，没有新增实际产品操作。

| 设计时要回答的问题 | 这次具体查到了什么 | 尚不能据此认定什么 |
| --- | --- | --- |
| 读完、理解、归档是不是同一件事 | 列表同时有预计时长、进度、保存日期；多选归档图里有0%条目，另有Archive all | 不能用百分比或归档次数证明理解/掌握；尚未执行归档和撤销 |
| AI找到网页后算不算已导入 | URL预览展示标题、缩略图和描述，保存另有按钮 | 预览不是完整正文或已入库，背景旧列表不是本次成功结果 |
| 被过滤的新闻能否回看 | 2024官方说明给出Removed today数量进入移除记录的路径 | 日志入口不是恢复能力；历史图的30天处理范围不能直接套当前版本 |
| 批注能否修改、删除 | 静态图明确有Edit/Delete，选区另有高亮/加笔记入口 | 入口存在不证明保存、删除确认、撤销与跨端结果 |
| 开启离线是否等于准备好了 | 帮助分别要求启用区域、联网下载、检查单篇离线标识；Ground明确浏览需网络 | 收藏、开关、下载完成、全文可读不同；不能承诺音频也缓存 |
| 取消邮件源会不会丢历史 | 帮助说删除且失去访问，功能页说保留90天且可重新关注恢复 | 两种口径可能涉及可见性/后台保留或版本差异，尚不能承诺可恢复 |

详情可以从[Inoreader原始依据与图](../research/interaction-library/artifacts/inoreader-reading-state/source-notes.md)及[Ground范围核查](../research/interaction-library/artifacts/ground-source-scope/source-notes.md)复查。官网图片的发布日期、图片更新日期和实际构建分别记录；看到“更新于2026”也不能忽略正文残留的旧版限制。

后续正式设计按“**设计问题→任务→条件与动作→有依据的结果→来源→未知分支**”检索。[交互依据表](../research/interaction-library/reference-points.csv)本轮增加12条，共38条直接定位示例；其余任务仍经案例与F/E表查询，不声称所有控件都已经结构化。[设计参考入口](../research/interaction-library/design-reference-guide.md)可直接查“退订后能否恢复”“AI导入何时算完成”等问题，无需顺序翻简报。

对本项目的启发是：G-001要分别衡量内容选择、阅读投入和知识收益；G-002要区分动作请求、可见反馈与实际保存。来源订阅本身也不保证高价值，官方邮件样例就包含促销内容。这些均为待讨论的设计依据，没有新增已确认功能，也未进入全系统设计。
''')

p=R/'README.md';s=p.read_text();marker='- [第四十期：';i=s.index(marker);s=s[:i]+'- [第四十一期：新闻阅读与保留](/home/workspace/reader/docs/briefs/041-news-reading-and-retention.md)：阅读进度、归档范围、网页预览/保存、过滤回看、离线可用及邮件源取消的文档冲突；补直接检索依据。\n'+s[i:];put(p,s)
p=L/'README.md';s=p.read_text().replace('交互依据表现含26条增量示例','该批交互依据表累计26条增量示例');i=s.index('- [第四十期：');s=s[:i]+'- [第四十一期：新闻阅读与保留](/home/workspace/reader/docs/briefs/041-news-reading-and-retention.md)：区分读到/归档/保存/离线等状态，保留邮件源取消的口径冲突；交互依据表现有38条直接检索示例，仍非全功能覆盖。\n'+s[i:];put(p,s)
append(R/'docs/ideas.md','2026-10-27继续I-021/I-022：承认尚未逐产品逐功能完成，补Inoreader阅读/归档/过滤/网页/离线和邮件源生命周期，Ground补联网、原语言及旧套餐措辞。沿16旧任务补证，仅新增邮件源1任务；静态图不称实际操作。按设计问题增12条检索依据，见[第四十一期简报](/home/workspace/reader/docs/briefs/041-news-reading-and-retention.md)。将读到、理解、归档和真实保存分开仍为研究建议，未升级需求或进入系统设计。')
append(L/'analysis-status.md','第41批补Inoreader原目录38篇帮助、3博客/1功能页，Ground原目录4篇正文；46新E、1新D2邮件源任务、16旧任务补证。13张官方静态图属于4个来源E；没有新增产品实操，全部65组仍部分覆盖。RP新增12条，共38条增量检索。当前统计以生成索引为准；取消邮件源保留和历史过滤范围尚未统一，见[简报41](../../briefs/041-news-reading-and-retention.md)。')
append(L/'comparison.md','''## 补充：新闻状态和实际结果（2026-10-27）

| 问题 | 依据 | 比较时的边界 |
| --- | --- | --- |
| 阅读投入、完成和保留 | [Inoreader F03/16及画面](artifacts/inoreader-reading-state/source-notes.md) | 预计时长/百分比/归档是不同量；0%被选中不证明完成操作或理解 |
| 过滤与停止接收 | [Inoreader F01/17](cases/M-044-inoreader.md) | 回看日志不等于恢复；旧30天范围与当前规则、删除与后台保留分别留冲突 |
| 收藏和离线可用 | [Inoreader F05](cases/M-044-inoreader.md)、[Ground F02](cases/M-046-ground-news.md) | 区域启用、单篇已下载、联网限制不同；不能仅凭两者都有收藏就等同恢复能力 |

这些支持后续原型提问，未证明减少消遣时间、扩大认知或适合5–6岁儿童，未作为系统架构结论。
''')
append(L/'backlog.md','''## 第41批留下的明确缺口

- Inoreader：同一当前构建核选中/全部归档、真实保存和撤销；网页预览失败/重复/取消；批注编辑/删除结果。历史图与帮助不代替实际操作。
- 过滤：已取得移除记录入口，下一步是记录内容与恢复能力；创建/编辑是否回溯30天未知，不能反复只补“支持过滤”。
- 邮件源：名称/地址冲突、订阅确认、丢信和停止收信；需澄清不可见、后台保留与实际删除的差别，未核前不承诺90天可恢复。
- 离线/音频：区域开关、单篇下载、真实断网阅读、部分失败、混合队列Clear all和来电/重启/跨端断点；本轮无账号或听音结果。
- Ground：本轮4篇原目录正文已读，套餐概览仍有旧移动版措辞；不要把它当现行权限矩阵。原产品域搜索403路线不重试；未读条目及实际操作继续保留。
- 检索层：RP当前38条仅增量样例，全库尚无统一逐控件/逐页面状态索引。继续随有证据的任务补条件、结果、持久化和未知分支，不能批量猜填制造完整性。
''')
print('Updated cases, 12 reference points, brief 41, and research navigation; requirements untouched.')
