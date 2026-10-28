"""One-time navigation and summaries update after the reviewed batch merge."""
from pathlib import Path
R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
def replace(path, old, new):
    p = path if isinstance(path, Path) else R / path
    text = p.read_text()
    assert text.count(old) == 1, (p, old[:70], text.count(old))
    p.write_text(text.replace(old, new))

replace('README.md', '- [第十三期：家庭内容、练习录音与音频角色]', '- [第十四期：新闻控制、学习记录与离线故事](/home/workspace/reader/docs/briefs/014-news-learning-offline-evidence.md)：最新简报；新闻选择与评级、学习完成与打卡、离线故事与家庭绑定，附资料深度和复用方法。\n- [第十三期：家庭内容、练习录音与音频角色]')
replace('README.md', '：最新简报；家庭进度、临时/保存录音', '：家庭进度、临时/保存录音')
replace(L/'README.md', '本批续补[reMarkable', '此前续补[reMarkable')
replace(L/'README.md', 'Yoto 及部分 Kobo/Khan Kids 条目采用官方搜索索引正文，逐条记录 access_mode。', '早期Yoto及部分Kobo/Khan Kids条目采用官方搜索索引正文；Yoto本轮已取得部分公开HTML内嵌正文，作为additional_captures追加，保留最初取得方式。')
replace(L/'README.md', '本轮 Yoto 的 HTTP 页面只得到壳结构，不能把 HTTP 200 或文件哈希当作正文已归档。事实采用官方索引文本并标明此限制。', '早期Yoto请求只得到页面壳，最初依据为索引文本；2026-10-27已从公开分类HTML取得选定文章的内嵌正文，并登记原件、文章定位及追加采集。不能仅凭HTTP 200或文件哈希判断读到正文。')
replace(L/'README.md', '本批入口：[第十三期简报](../../briefs/013-household-recording-and-audio-roles.md)。Tolino、CASIO和FiiO均保留型号/版本、目录、旧任务修订与数据边界；只有公开选馆器新增有限D4。', '最新入口：[第十四期简报](../../briefs/014-news-learning-offline-evidence.md)。Ground、Yoto和扇贝补目录、操作条件及恢复边界；同一任务补回原编号，本轮没有新增D4。此前[第十三期](../../briefs/013-household-recording-and-audio-roles.md)记录Tolino、CASIO与FiiO及有限选馆器操作。')

rows = '''| [M046-Ground帮助目录](../batches/ground-controls-deepening-inventory.csv) | 11分类及65篇唯一文章，共76行 | 目录发现、当前正文审读和既有来源分开；不是完整App菜单，匿名搜索403后停止 |
| [M037-Yoto帮助与App设置](../batches/yoto-controls-deepening-inventory.csv) | 7一级分类、87文章及29设置/页签，共123行 | 未展开订单与第四代类目；只选定文章读正文，第三代/Mini2024不继承第四代能力 |
| [M019-扇贝帮助及扩展目录](../batches/shanbay-controls-deepening-2-inventory.csv) | 12栏目、34旧文章、5外部条目、14扩展小节及3声明域，共68行 | 只取得8个旧栏目，另4个未取；历史四App、Web和现代扩展分开，声明不当交互 |
'''
replace(L/'inventories/README.md', '| [M058-T302', rows + '| [M058-T302')

replace(L/'comparison.md', '| 继续安排学习是否等于恢复上一题？ | [扇贝 F-M019-06/08](cases/M-019-shanbay.md) 历史计划保留未完成文章，但未说明精确句子或未提交答案；新阅读日志又增加自动打卡。 | 学习计划、题目状态、完成记录和播放位置分别查询；不要把系统自动记录当用户明确确认或能力测量。 | G-001/002、R-019/021 |', '| 继续安排学习、学完与打卡是否同一状态？ | [扇贝 F-M019-06/08](cases/M-019-shanbay.md) 旧听力至少三遍后揭晓、未懂句再安排；旧帮助另有完成并上传后的七日补卡例外。 | 计划、句子位置、掌握、已上传和打卡分别核查；午夜规则与例外保留年代/条件，现版仍缺。 | G-001/002、R-019/021 |')
replace(L/'comparison.md', '| 离线准备什么时候算完成？ | [Yoto F-M037-01/03](/home/workspace/reader/docs/research/interaction-library/cases/M-037-yoto.md) 区分书库与下载；[Reader F-M015-02](/home/workspace/reader/docs/research/interaction-library/cases/M-015-readwise.md) 正文可读不证明 TTS 可离线。 | AI 应依据哪些完成状态回答“已经准备好了”；文字、图片、音频和题库附件是否分别检查。 | R-016～018 |', '| 离线准备什么时候算完成？ | [Yoto F-M037-01/03](cases/M-037-yoto.md) 按设备核Audio Download Status；书库与缓存分开，两篇淘汰规则有差异。[Reader F-M015-02](cases/M-015-readwise.md) 正文可读不证明TTS可离线。 | AI回答“准备好了”需核哪台设备、内容和附件；缓存删除不等于书库或个人断点删除，实际恢复待测。 | R-016～018 |')
replace(L/'comparison.md', '## 新闻与内容选择', '| 闹钟设置值是否等于实际输出？ | [Yoto F-M037-16](cases/M-037-yoto.md) 保存/取消、启停和删除分开，音量受日夜上限约束且须开机。 | 读取配置后还要核执行条件和实际结果；文档未保证离线/关机唤醒。 | G-002、H-003 |\n\n## 新闻与内容选择')
replace(L/'comparison.md', '## AI上下文与执行结果', '| 评级和推荐是否会随个人操作改变？ | [Ground F-M046-03/17/18/22](cases/M-046-ground-news.md) For You含使用行为；可改个人政治倾向评级，颜色另设且独立Edition；事实性来自媒体评级。 | AI需分清公共评级、个人覆盖、颜色和推荐依据；中立不等于准确，机构数量文案冲突保留。 | R-020、G-001/002 |\n\n## AI上下文与执行结果')
replace(L/'comparison.md', '## 导入笔记与内容关联', '| 恢复出厂、退出家庭与内容所有权是否联动？ | [Yoto F-M037-18/19](cases/M-037-yoto.md) 重置删下载但不解绑；共享MYO仍由原作者拥有，更新传播至关联卡。 | 设备副本、家庭访问、原作者控制和个人进度分别界定；离线更新/撤回实际结果未知。 | H-004/005、G-002 |\n\n## 导入笔记与内容关联')

replace(L/'backlog.md', '| 扇贝四个应用 | 分别核商店条目与版本；历史词书/听力计划，阅读日志和雅思智能口语局部路径 | 当前误评撤销、自动打卡阈值、录音提交/重录、退出恢复；旧帮助的计划保留不能代替精确断点 |', '| 扇贝四个应用及浏览器扩展 | 68目录；旧提示门槛、词量入口、午夜与补卡条件；现代扩展加词/来源与账号边界 | 当前误评撤销、提示后判分、录音提交/重录、精确恢复与跨端冲突；4个旧栏目未取，历史规则不套现版 |')
replace(L/'backlog.md', '| Yoto 第三代/Mini 2024 | 实体键、家庭、MYO、配对、书库归属 | 换卡/手机/两台设备的实际断点；下载调度冲突；删除列表时正播结果；闹钟与计时打断关系 |', '| Yoto 第三代/Mini 2024 | 123目录；正文补下载/计时、家庭、共享MYO、闹钟、夜灯和重置 | 实际断点、两种淘汰表述、Mini容量逐型号核定、离线/关机响铃、共享更新/撤回后离线副本；未读目录继续补 |')
replace(L/'backlog.md', '| Ground News | 地区/检索/专题/来源过滤、外链、通知、权益恢复 | 真实统计计数与下钻；已读撤销、全局与事件过滤优先级、外站返回位置、筛选零结果与内容更正 |', '| Ground News | 76目录；评级三图、个人评级/配色、登录绑定、删数据与故障分支 | 评级机构数/专题套餐冲突、个人覆盖影响统计/推荐、已读撤销、零结果/优先级、外站返回；匿名搜索403，未重试或新增实操 |')
replace(L/'backlog.md', '除公开样刊、驾考宝典及TIME for Kids的有限桌面操作外', '除公开样刊、驾考宝典、TIME for Kids、Duolingo及Tolino的有限桌面操作外')

replace(L/'cases/M-037-yoto.md', '追加采集建议交根代理合并。', '追加采集已合回原E，原取得方式保留。')
replace(L/'cases/M-019-shanbay.md', '在work提供additional_captures建议；', '在原E追加additional_captures，合并审计留work；')
replace(L/'cases/M-019-shanbay.md', '原F逐字段和旧E附加记录仅交根代理建议，未改旧批/主台账。', '原F的26项字段修订及7条旧E附加记录已由根代理核对后合回旧批和主台账。')

with (L/'analysis-status.md').open('a') as f:
    f.write('| 新闻控制、学习记录与离线故事深化（10月27日） | Ground17E/6F/76目录和三图；Yoto14E/4F/123目录；扇贝7E/0F/68目录；21旧F的80字段补回，20旧E追加原件 | 匿名Ground搜索403停止，无新增D4；保留评级、套餐、缓存、午夜规则及型号容量差异，全部仍部分覆盖 |\n')
with (R/'docs/ideas.md').open('a') as f:
    f.write('\n2026-10-27继续I-021/I-022：Ground补个人评级/配色、推荐依据及账号恢复，Yoto补缓存/闹钟/共享与重置，扇贝补旧提示/午夜规则及现代扩展来源语境。21个旧任务继续沿用编号，目录发现与已还原任务分开；不同文档的冲突和实际操作缺口保留，见[第十四期简报](/home/workspace/reader/docs/briefs/014-news-learning-offline-evidence.md)。这些是后续设计的可追溯依据，尚未完成全部产品功能分析，没有新增已确认功能或开始全系统设计。\n')
print('Updated navigation, comparison, gaps, cases and research status.')
