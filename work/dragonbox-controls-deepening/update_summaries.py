from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
def rep(p,old,new):
 p=p if isinstance(p,Path) else R/p;s=p.read_text();assert s.count(old)==1,(p,old[:55],s.count(old));p.write_text(s.replace(old,new))
rep('README.md','- [第十四期：新闻控制、学习记录与离线故事]','- [第十五期：自由探索、撤销与自动停止](/home/workspace/reader/docs/briefs/015-exploration-undo-and-stopping.md)：最新简报；儿童构数与教材提问、误评撤销与媒体恢复、自动推进及计时条件。\n- [第十四期：新闻控制、学习记录与离线故事]')
rep('README.md','：最新简报；新闻选择与评级','：新闻选择与评级')
rep(L/'README.md','最新入口：[第十四期简报](../../briefs/014-news-learning-offline-evidence.md)。Ground、Yoto和扇贝补目录、操作条件及恢复边界；同一任务补回原编号，本轮没有新增D4。此前[第十三期](../../briefs/013-household-recording-and-audio-roles.md)记录Tolino、CASIO与FiiO及有限选馆器操作。','最新入口：[第十五期简报](../../briefs/015-exploration-undo-and-stopping.md)。DragonBox、Anki和HiBy补任务结构、撤销/恢复和自动停止条件；同一任务补回原编号，本轮没有新增D4。[第十四期](../../briefs/014-news-learning-offline-evidence.md)记录新闻选择、家庭绑定与学习打卡的状态差异。')
rows='''| [M050-Numbers教学页与相关家庭帮助](../batches/dragonbox-controls-deepening-inventory.csv) | 9教学页、4模式、5Kids标题、6报告章节及3档案章节，共27行 | 教材问题不当游戏自动出题；Kids外部帮助不是Numbers菜单；Run仍D1 |
| [M020-Anki桌面手册与补充来源](../batches/anki-controls-deepening-inventory.csv) | 55手册页面加5补充来源，共60行 | 25页部分正文已读、30页仅目录；不含AnkiMobile全目录，不把文档当软件操作 |
| [M035-R3II2025指南及平台FAQ](../batches/hiby-controls-deepening-inventory.csv) | 35标题、71一级设置、6播放菜单名及33FAQ标题，共145行 | 型号中英文、规格、插图和FAQ有冲突；标题/设置重叠不是完整任务数量 |
'''
rep(L/'inventories/README.md','| [M046-Ground帮助目录]',rows+'| [M046-Ground帮助目录]')
rep(L/'comparison.md','## 读听与中断恢复','| 数学探索一定要有一张判分试卷吗？ | [DragonBox F-M050-07/08](cases/M-050-dragonbox.md) Sandbox指南支持自由构数/比较，教材Quiz气泡不证明自动出题；Run只得模式声明。 | 自由表达、目标练习、提示与能力测量分开；游戏目标和教材给成人的提问分开。 | G-001、R-014/019 |\n| “撤回刚才”能任意选择历史动作吗？ | [Anki F-M020-01/02/10/21](cases/M-020-anki.md) v3可按顺序撤回答题与暂埋；2.1.45列多步Redo和清空历史的条件。 | 保存、顺序撤销、定点改历史及备份回退有不同范围；跨重启栈仍未知。 | G-002、R-007/015 |\n| 连续学习何时停止自动动作？ | [Anki F-M020-15](cases/M-020-anki.md) 2025维护者议题解释当时失焦停止，当前构建未核。 | 自动翻题、评分、音频和焦点分别记录，明确启动/停止/恢复；不从历史意图推定现版行为。 | G-001/002、R-019/021 |\n\n## 读听与中断恢复')
rep(L/'comparison.md','| 换内容后计时属于谁？ | [Yoto F-M037-02](/home/workspace/reader/docs/research/interaction-library/cases/M-037-yoto.md) 换卡不重置睡眠计时；[HiBy F-M035-02](/home/workspace/reader/docs/research/interaction-library/cases/M-035-hiby.md) 提供播放循环与系统关机倒计时，切曲行为仍未知。 | 播放会话、内容片段、设备与家庭成员的状态是否混淆。 | R-018、G-002 |','| 换内容后计时属于谁？ | [Yoto F-M037-02](cases/M-037-yoto.md) 换卡不重置；[HiBy F-M035-02/06](cases/M-035-hiby.md) 平台FAQ称普通切曲不重计，但重扫规则有冲突，电脑USB/充电画面有例外。 | 播放会话、内容、设备与计时器状态分开；睡眠/省电重启后开关不同，实际固件仍待核。 | R-018、G-002 |')
rep(L/'comparison.md','| 同步能否代替备份？ | [Anki F-M020-03](/home/workspace/reader/docs/research/interaction-library/cases/M-020-anki.md) 自动备份不含媒体，首次同步还涉及方向。 | 误删、刷机、重装后的恢复目标分别是什么；需要哪一份文本、媒体及版本。 | H-004/005 |','| 同步能否代替备份？ | [Anki F-M020-03/18](cases/M-020-anki.md) 自动/即时备份不含媒体；集合单向替换与媒体合并分开，附件删除可能传播。 | 分别核文本、学习记录和附件恢复；deleted.txt格式取决于档案建立版本，不能只看安装版本。 | H-004/005、G-002 |')
rep(L/'comparison.md','- [HiBy](cases/M-035-hiby.md)：型号手册与平台 FAQ 的音效范围、旋钮文案有疑点；睡眠 FAQ 对重扫是否重计时也有矛盾。保留问题，不挑其中一句承诺表现。','- [HiBy](cases/M-035-hiby.md)：型号中英文指南、规格和两张插图对按键/旋钮、DAC及蓝牙有差异；图本身不能确认2025装配。平台FAQ对重扫是否重计时仍矛盾，同源多语言不能当独立验证。')
rep(L/'backlog.md','| Anki Desktop | 从资料到卡片、练习、调度、历史、导出和档案 | 固定构建的误评撤销、自动推进退出、模板/媒体清理恢复；移动端差异；统计与插件剩余功能域 |','| Anki Desktop | 55手册页+5补充源；v3顺序撤销/历史Redo、模板错误、媒体与日志恢复条件 | 当前学习页Undo/Redo题面、跨重启栈、自动推进停止/重进/音频；媒体回收位置/期限、档案删除/降级；30页仅目录 |')
rep(L/'backlog.md','| HiBy R3II 2025 | 型号手册全部一级目录及部分任务 | 队列/删除具体对象、歌单入口和恢复、精确断点；睡眠重计时的文档矛盾；无线传歌成功/失败链 |','| HiBy R3II 2025 | 145目录/菜单/FAQ项；播放范围、跨目录和专辑；双语/规格/两图交叉审校 | 型号部件与音效/计时冲突；队列/删除对象、歌单与精确断点；无线传歌成功/失败链仍未核 |')
rep(L/'backlog.md','| Anki Desktop |','| DragonBox Numbers | 27教学页/模式/相关家庭帮助条目；Sandbox静态构数与外部档案；Run只得声明 | 当前完整菜单、撤销/清空、拼图错放到结算、Ladder碰撞、Run输入/错误、离线授权和局内断点 |\n| Anki Desktop |')
rep(L/'cases/M-035-hiby.md','旧E-HBD-01/02/03/04仅提additional_captures合并建议；','旧E-HBD-01/02/03/04的additional_captures已核对并合回旧批；')
rep(L/'cases/M-020-anki.md','旧F01/02/03/07/10/15/18/20/21的字段修订建议及旧E追加采集建议在本批work交接文件，由root合并；本代理未改主表或旧批。','旧F01/02/03/07/10/15/18/20/21的36字段及6旧E追加采集已由root核对后合回旧批和主台账，审计留work。')
row='| 儿童探索、撤销与自动停止深化（10月27日） | DragonBox5E/2F/27目录及两摘页；Anki7E/0F/60目录；HiBy4E/1F/145目录及两插图；19旧F的75字段补回，11旧E追加采集 | 新任务2D2/1D1；无新增实操；保留教材/游戏区别、撤销与媒体边界、历史焦点行为及型号/计时冲突，全部部分覆盖 |'
rep(L/'analysis-status.md','\n本轮之前的工作属于','\n'+row+'\n\n本轮之前的工作属于')
with (R/'docs/ideas.md').open('a') as f:f.write('\n2026-10-27继续I-021/I-022：DragonBox区分自由构数、教材提问和目标任务，Anki补顺序撤销/保存/媒体恢复，HiBy补播放范围与计时条件并保留同型号资料冲突。目录发现、实际看图与文档重建分别登记，19旧任务沿用编号；见[第十五期简报](/home/workspace/reader/docs/briefs/015-exploration-undo-and-stopping.md)。学习和AI执行的启发保持建议，未将未知操作补成事实，未开始全系统设计。\n')
print('Updated brief navigation, inventories, comparisons, gaps, cases and status.')
