import csv
from pathlib import Path

ROOT = Path('/home/workspace/reader')
LIB = ROOT / 'docs/research/interaction-library'
DATE = '2026-10-27'
BRIEF = ROOT / 'docs/briefs/040-learning-input-and-sync.md'
assert not BRIEF.exists(), 'Prose integration already applied'

def put(p, text):
    p.write_text(text.rstrip() + '\n')

def append(p, text):
    put(p, p.read_text().rstrip() + '\n\n' + text.strip())

def replace(p, old, new):
    text = p.read_text()
    assert old in text, (p, old)
    put(p, text.replace(old, new, 1))

p = LIB / 'cases/M-017-maimemo.md'
replace(p, '9 项任务为 D2 文档重建，其中 F07 只重建静态接口契约，未调用接口、未取得任何真实用户结果。本轮无安装、登录、音频试听或图片观察。',
    '现有12项任务为D2文档重建，其中F07只重建静态接口契约及厂商使用条件，未调用接口、未取得真实用户结果。第40批补8篇正文与9幅鸿蒙官方静态图；无安装、登录、音频试听或产品操作。')
replace(p, '| 好友、补签、签到失败、学习提醒 | F06；E-MMO-08、E-MMCD-03/04 | 补签条件有正文；奖励发放与正常签到结算未完整分析。提醒帮助仅登记；断签专页读到的正文只有客服入口 |',
    '| 好友、补签、签到失败、学习提醒 | F06/12；E-MMO-08、E-MMCD-03/04、E-MMSC-03 | 提前学完且签到会停当日提醒；通知两处开关已有正文，到点/拒绝/跨日未知。奖励发放与签到结算仍缺；断签专页仅客服入口 |')
replace(p, '| 发音、随身听、外设、小组件 | E-MMCD-11/12、E-MMO-10/11 | iOS 排障有正文；随身听队列/离线、中断续播与外设完整操作未深入 |',
    '| 发音、随身听、外设、小组件 | F11；E-MMCD-11/12、E-MMO-10/11、E-MMDVC-03、E-MMSC-01 | 外设连接/作用范围/排障已补，默认按键表和实操仍缺；随身听仅历史功能声明，队列/离线/续播未知 |')
replace(p, '| 鸿蒙特色、网页版、Android/Web 日志、好评指南 | E-MMO-10 目录 | 本轮未读正文；不能把 iOS 或 HarmonyOS 条目套到这些端 |',
    '| 鸿蒙特色、网页版、Android/Web日志、好评指南 | F01/02/04/10；E-MMSC-02/04/05/06、E-MMDVC-03 | 第40批已读正文及9图；平台与构建分别记录，识词提交、误评恢复和真实接续仍未知；好评指南属于商店支持流程 |')
replace(p, '| 开放平台与公开 API；开发流程、memo 方法及 schemas | F07；E-MMCD-09/10 | 已查合同对象/条件；未调用，无真实成功/失败或同步结果 |',
    '| 开放平台与公开API；开发流程、方法、schemas和服务协议 | F07；E-MMCD-09/10、E-MMSC-07 | 已查技术契约与§3/4/7内容使用条件，未调用或取得授权；不能据接口推断可永久复制内容到AI语料库 |')
replace(p, '答案显隐、拼写判分/重试、误评回滚、完成页及跨日恢复未知。返回上词的日志不是撤销依据。E-MMO-01～03、E-MMCD-01/02/12',
    '鸿蒙答案前后手势规则及手表静态显隐已补，连续拼写判分/重试、误评回滚、完成页及跨日恢复未知。返回上词不证明撤销。E-MMO-01～03、E-MMCD-01/02/12、E-MMSC-02/06')
replace(p, '冲突合并、恢复点列表、失败后回滚未知；不能称无损双向合并。E-MMO-05',
    'Web须App切后台触发并等待同步；鸿蒙接续另需相同华为/墨墨账号及源端复习页。冲突合并、答案阶段恢复、恢复点列表与回滚未知。E-MMO-05、E-MMSC-02/05')
replace(p, '失败响应、幂等/撤销、权限清单和硬件下发未知。具体限制见下。E-MMO-09、E-MMCD-09/10',
    '失败响应、幂等/撤销、权限清单和硬件下发未知。厂商协议对版权内容另有展示/缓存限制，见下。E-MMO-09、E-MMCD-09/10、E-MMSC-07')
replace(p, '## 分支与证据冲突', '''## 第40批：新增任务及已有任务的补证

| F编号 | 前置、入口与动作 | 有依据的结果/状态 | 未闭合分支 |
| --- | --- | --- | --- |
| F-M017-10 从图片识词并检查候选 | 鸿蒙“选词”搜索栏右侧A→拍照或选图 | 官方静态图显示识别列表、未选/已选/全部分类与双栏详情；顶部40已选和底部当前已选0不是同一计数范围 | 未见授权、识别错误校正、实际勾选/提交成功、额度和保存；不把灰色按钮当已实测禁用。E-MMSC-02 |
| F-M017-11 连接外设并执行复习快捷动作 | Home+B长按5秒黄灯快闪→系统蓝牙连接→App手柄设置核状态 | 文档限定复习页，列自评/音频/答案/上词/确认取消；系统连接与App识别分开，映射修改与iOS改名另有排障 | 默认按键表未取得、实际连接/回连/休眠首键/连发/误评恢复未测；正文iOS版本要求冲突。E-MMSC-01 |
| F-M017-12 核对学习提醒条件与开关 | 提醒前已学完且签到则当天不提醒；否则检查系统通知及App学习提醒/系统提醒 | 文档规定了合理不通知的条件与两处开关 | 时间设置器、权限拒绝、实际通知、跨日及补发未知。E-MMSC-03 |

已有F01补鸿蒙手势：答案出现前下滑显示答案、上滑返回；答案后下滑认识、上滑忘记。同样动作依赖学习阶段，不推断返回就是撤销。图示双栏左词inheritance、右词the，不能拼成同词详情的连续过程；手表同词的隐藏/显示/例句图也不是实际点击链。设置搜索例句图把答案前提示和答案后自动发音分开。见[9幅图与逐图观察](../artifacts/maimemo-input-and-sync/source-notes.md)。

已有F04补Web公测：要求iOS5.5.27/Android5.5.25/HarmonyOS5.5.03或更高，自动同步先完成一次；学完切App到后台触发，再等待同步，最后Web刷新。开关已开启不是最新进度已到达。鸿蒙接续还需相同华为账号与墨墨账号、源设备复习页；目标dock图标仅为入口，答案阶段、待提交反馈和两端并行的结果未知。

已有F02补Android历史固定学习量/顺序声明；日志与FAQ的建议口径分开。F07补服务协议：§7版权内容的在线展示、短期缓存及撤回条件不能替代实际授权；不泛化为个人学习记录都不能备份。Web日志列出的功能比公测指南的核心复习更广，未据此声称全端一致。

## 分支与证据冲突''')
replace(p, '## 数据与学习激励（分析及推断）', '''第40批补查E-MMSC-07（2026-06-26版）§3/4/7：释义、助记、例句等版权内容限审核后的在线展示及必要缓存（最长168小时），另有署名、撤回和删除要求。技术接口存在不等于允许永久离线语料库、再分发或改写；此处仅说明厂商公布的接入条件，未做完整法律审查，也未决定接入该服务。详见[条款定位与范围](../artifacts/maimemo-input-and-sync/source-notes.md)。

## 数据与学习激励（分析及推断）''')
append(p, '''第40批E-MMSC-01～07及复用E-MMDVC-03见[证据批](../batches/maimemo-input-and-sync-evidence.jsonl)和[短摘/画面/原址](../artifacts/maimemo-input-and-sync/source-notes.md)。原61项目录中的8项追加正文取得范围；它们不是8个新任务。新增F10～12、补旧F01/02/04/07，仍无产品实操。飞书所链手柄PDF仅取得查看器外壳，页内按键表保持未读。''')

append(LIB / 'cases/M-030-khan-kids.md', '''第40批尝试补10个账户/网络/课堂目录条目：家长和教师两栏目各一次普通GET均403验证页后停止，未解析文章URL，未请求目标文章。原目录成功快照保留，10项仍是未读正文；没有新增F/E或提升深度。见[访问范围](../artifacts/khan-remaining-source-attempt/source-notes.md)。''')

# Reference points are a retrieval layer, not a second feature counter.
rp_path = LIB / 'reference-points.csv'
with rp_path.open(newline='') as f:
    refs = list(csv.DictReader(f))
assert len(refs) == 16 and not any(x['product_id'] == 'M-017' for x in refs)
def rp(n, tags, fids, eid, locator, asset, before, action, result, basis, unknown):
    return dict(reference_id=f'RP-M017-{n:02}', product_id='M-017', question_tags=tags, flow_ids=fids,
        evidence_ids=eid, source_locator=locator, artifact_path='artifacts/maimemo-input-and-sync/' + asset,
        precondition=before, action=action, visible_result=result, evidence_basis=basis,
        persistence_result='未实际操作；保存与恢复未验证', unknown_branches=unknown,
        version_scope='官方软件帮助；平台依本行；实际构建未固定，非Plus实测', checked_on=DATE)
refs.extend([
    rp(1, 'AI;手势;答案;阶段', 'F-M017-01', 'E-MMSC-02', '鸿蒙特色正文：隔空手势', 'source-notes.md', '鸿蒙系统隔空滑动已开；复习页答案未出现', '下滑 / 上滑', '文档分别规定显示答案 / 返回', 'official_help_documented', '返回范围;误触;连发;实际结果'),
    rp(2, 'AI;手势;自评;阶段', 'F-M017-01', 'E-MMSC-02', '鸿蒙特色正文：隔空手势', 'source-notes.md', '鸿蒙复习页答案已出现', '下滑 / 上滑', '文档分别规定认识 / 忘记', 'official_help_documented', '模糊对应方式;误评撤销;排期结果;保存'),
    rp(3, 'AI;OCR;选词;计数;导入', 'F-M017-10', 'E-MMSC-02', '鸿蒙特色第4幅图', 'harmony-04.jpg', '图示识别结果宽屏双栏', '查看分类和当前勾选计数', '顶部40已选、底部已选0/建议选0及灰色加入记忆；没有本次40词新增证据', 'official_static_images', '按钮是否禁用;实际勾选;识别校正;提交结果'),
    rp(4, 'AI;双栏;对象;学习', 'F-M017-01', 'E-MMSC-02', '鸿蒙特色第5幅图', 'harmony-05.jpg', '官方宽屏复习示意', '查看两栏当前内容', '左inheritance复习，右the释义选择；对象不同', 'official_static_images', '两栏切换来源;焦点;作用范围;持久选择'),
    rp(5, '学习;提示;答案;音频;设置', 'F-M017-01', 'E-MMSC-02', '鸿蒙特色第9幅图及设置搜索正文', 'harmony-09.jpg', '我的→更多设置→搜索', '搜索例句', '列出答案前中英文提示与答案后自动播放第一条例句', 'official_static_images', '默认值;设置保存;跨端;订阅范围;实际声音'),
    rp(6, 'AI;同步;后台;恢复', 'F-M017-04', 'E-MMSC-05', 'Web指南：使用前准备与同步注意事项', 'source-notes.md', '相应最低App版本、Web公测资格、已启用自动同步', 'App学完→切后台→等待同步→Web刷新', '文档规定完成同步后Web才取得最新进度；不是实际完成记录', 'official_help_documented', '等待状态UI;失败;双端冲突;强制结束进程'),
    rp(7, 'AI;接续;同步;账号;学习', 'F-M017-04', 'E-MMSC-02', '鸿蒙特色应用接续正文及第1图', 'source-notes.md', '相同华为与墨墨账号；自动同步；源端复习页', '目标设备点击dock接续图标', '文档规定同步进度后学习；静态图仅显示图标', 'official_help_documented', '答案显隐;待提交反馈;离线;并行学习;页面位置'),
    rp(8, 'AI;提醒;学习;完成;通知', 'F-M017-12', 'E-MMSC-03', '学习提醒正文', 'source-notes.md', '已设学习提醒', '核对当日学习/签到及系统和App开关', '文档说明提醒时间前学完且签到则当天不再提醒', 'official_help_documented', '时间选择;拒绝权限;跨日;到点;补发'),
    rp(9, 'AI;外设;连接;输入', 'F-M017-11', 'E-MMSC-01', '外设指南：快速入门与连接问题', 'source-notes.md', '墨墨口袋手柄；目标App复习页', 'Home+B黄灯配对→系统蓝牙连接→App核状态', '文档要求App显示手柄已连接；蓝牙连接不等于App识别', 'official_help_documented', '默认映射;版本冲突;实际回连;睡眠首键;误评恢复'),
    rp(10, '学习;答案;手表;自评', 'F-M017-01', 'E-MMSC-02', '鸿蒙特色第6/7/8图，附件直达第7图', 'harmony-07.png', '官方手表示意，tone同词', '对照隐藏/答案/例句静态画面', '答案图有认识及预计间隔；没有连续点击或实际排期记录', 'official_static_images', '全组自评;切页手势;声音;结算;同步'),
])
with rp_path.open('w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(refs[0]))
    w.writeheader()
    w.writerows(refs)

guide = LIB / 'design-reference-guide.md'
replace(guide, '当前16条仅覆盖AiPaper历史任务和The Week Junior已有操作样例，其余任务仍查下表/案例，不能把未收录当作未支持或已完成。',
    '当前26条覆盖AiPaper历史任务、The Week Junior已有操作样例及墨墨输入/同步细节，其余任务仍查下表/案例；这是增量检索入口，不能把未收录当作未支持或已完成。')
replace(guide, '## 从设计问题进入', '''## 怎样判断资料已经足够深入

先列固定型号/版本的功能与帮助目录，再按实际用户任务补入口、前置条件、逐步动作、界面反馈、数据影响、结束与恢复。一个按钮的存在只能证明入口；成功主流程有依据后，还需单列错误、取消、空内容、权限、离线、中断与重装等适用分支。无资料的分支标未知，不根据常见设计补写。

后续正式设计引用时，需要能回答“哪个版本、哪个条件、动作影响什么、结果凭什么确认、失败能否恢复”，并从任务直达来源页/时间点/操作审计。宣传、帮助步骤、静态图和真实操作分别标记；奖励机制描述不证明学习有效，用户测评不替代本家庭儿童观察。每产品都继续保留覆盖缺口，直到明确范围有依据闭合，才能讨论该范围分析完成。

## 从设计问题进入''')
needle = '| --- | --- | --- | --- |\n'
replace(guide, needle, needle + '''| AI发出同一个手势前，要先读取什么状态 | [墨墨F01](cases/M-017-maimemo.md) | [鸿蒙手势规则](artifacts/maimemo-input-and-sync/source-notes.md)；RP-M017-01/02 | 答案前后同一下滑分别是揭晓/认识；文档规则，不是实操；误评撤销仍未知 |
| AI识别图片后，怎样判断真正加入了多少词 | [墨墨F10](cases/M-017-maimemo.md) | [识词双栏与两种计数](artifacts/maimemo-input-and-sync/harmony-04.jpg)；RP-M017-03 | 分类里的已选和本次勾选不同；不能把图中40当本次成功导入量，提交仍未观察 |
| 已打开同步，为什么另一端还没更新 | [墨墨F04](cases/M-017-maimemo.md) | [后台触发、等待及接续条件](artifacts/maimemo-input-and-sync/source-notes.md)；RP-M017-06/07 | 开关、触发、上传完成和目标接收分开；答案阶段/并行冲突未知，不当整机备份 |
''')
dd = LIB / 'data-dictionary.md'
replace(dd, '当前16条示例', '当前26条增量示例')
replace(dd, '`evidence_basis`当前为`manual_diagram_static`或`automated_public_browser`。前者的结果只是手册所画，不能解读成亲自成功执行。',
    '`evidence_basis`当前为`manual_diagram_static`（手册示意）、`official_help_documented`（官方正文规定）、`official_static_images`（官方静态图）或`automated_public_browser`（审计过的网页操作）。前三者不能解读成亲自成功执行；文字预期结果与静态画面中的结果均需保留各自边界。')

put(BRIEF, '''# 第四十期：同一个动作，为什么需要不同的上下文

2026-10-27。目前仍未深入完成所有产品的所有功能。已有研究并非只有宣传页，但原生App/设备实操、错误与恢复仍明显不足；本次补墨墨帮助目录中的正文和9幅官方静态图，不把它们当实际使用记录。

| 设计问题 | 本次取得的细节 | 还缺什么 |
| --- | --- | --- |
| AI执行手势前要知道什么 | 鸿蒙复习页在答案前，下滑显示答案；答案后，下滑表示认识 | 误触、防连发、模糊自评、撤销和实际排期 |
| 识别到的单词是否已经导入 | 同屏顶部40已选，底部当前已选0；加入记忆另有入口 | 实际选中、识别校正、提交数量和保存 |
| 多栏页面的内容属于谁 | 官方图左边复习inheritance，右边选择the的释义 | 两栏来源、焦点和动作影响范围；不能拼成同词连续操作 |
| 开启同步是否代表另一端已有最新数据 | Web指南要求App学完切后台触发、等待完成，再由Web取得进度 | 失败、双端并行、离线冲突及答案阶段能否恢复 |
| 没收到提醒是否代表故障 | 在设定时间前完成学习并签到，当天不再提醒；系统/App各有开关 | 到点、权限拒绝、跨日和补发 |
| 蓝牙显示连接后能否操作App | 外设指南还要求核对App连接状态及手柄模式 | 默认映射、实际回连、休眠首键和误评恢复 |

逐项原址、画面和范围已放入[墨墨来源记录](../research/interaction-library/artifacts/maimemo-input-and-sync/source-notes.md)，沿原任务补证，新增识词、外设和提醒3条任务。Android历史日志复用已有来源编号；商店好评指南没有被误归为学习反馈。开放平台条款另表明，技术上能取内容不等于允许永久纳入自有离线语料；本项目未决定采用该服务。

Khan Kids的两个帮助栏目本次返回403验证页，已停止这条来源路线。待查10篇文章的直链未取得，也没有请求文章正文；它们继续标未读，不能据标题补写流程。[访问记录](../research/interaction-library/artifacts/khan-remaining-source-attempt/source-notes.md)保留这一限制。

为方便后续设计复用，[交互依据表](../research/interaction-library/reference-points.csv)本次增加10条定位，按前置、动作、结果、保存证据、未知分支和来源组织；它只是全库的增量入口。[按问题查依据](../research/interaction-library/design-reference-guide.md)同时明确深度标准：固定版本，补主流程，再核适用的错误、取消和恢复；没有证据就保留未知。简报用于阅读结论，任务/证据库用于追溯，不需要按期翻完所有简报。

这些差别服务G-001的有效学习及G-002的可核对AI操作，仍属于研究发现与候选取舍，没有新增未经确认的产品需求，也未开始完整系统设计。
''')

replace(ROOT / 'README.md', '- [第三十九期：', '- [第四十期：学习输入与同步](/home/workspace/reader/docs/briefs/040-learning-input-and-sync.md)：手势的学习阶段、识词计数、后台同步触发与提醒条件；补可检索的原文/画面和未验证分支。\n- [第三十九期：')
replace(LIB / 'README.md', '- [第三十九期：', '- [第四十期：学习输入与同步](/home/workspace/reader/docs/briefs/040-learning-input-and-sync.md)：墨墨手势状态、识词对象、Web同步和提醒；交互依据表现含26条增量示例，全部产品仍部分覆盖。\n- [第三十九期：')
replace(LIB / 'README.md', '[交互依据表](reference-points.csv)新增按条件/结果检索的16条示例', '[交互依据表](reference-points.csv)首批新增按条件/结果检索的16条示例')
append(ROOT / 'docs/ideas.md', '''2026-10-27继续I-021/I-022：墨墨输入、OCR、跨端同步和提醒补8篇正文与9幅静态图，揭示同手势依赖答案阶段、同“已选”文案的不同计数范围及后台同步触发。原文与画面补回原F/E并增加10条检索依据；Khan两栏目403后停止，10篇目标文章仍未读。见[第四十期简报](/home/workspace/reader/docs/briefs/040-learning-input-and-sync.md)。未将文档或静态图宣称为App实操，全部产品仍部分覆盖；没有新增产品需求或进入完整设计。''')
append(LIB / 'analysis-status.md', '''第40批补墨墨目录8项正文范围（协议重点§3/4/7）及9幅官方静态图：新增识词/外设/提醒3个D2，补旧F01/02/04/07；7个新E，Android日志复用E-MMDVC-03。交互依据表增10条，共26条增量入口。Khan两栏目403后停止，未请求目标10篇文章，不增加任务/深度。当前数量仍只取生成索引；65组全部部分覆盖，无本批产品实操。见[简报40](../../briefs/040-learning-input-and-sync.md)。''')
append(LIB / 'backlog.md', '''## 第40批：输入、同步及未读栏目

- 墨墨：原目录8项已取得正文范围，勿再笼统标为未读。鸿蒙9图只有静态示意，仍缺同构建识词校正→选中→提交保存、手势误评撤销、手表完整结算、后台同步失败/并行冲突和提醒实际结果；历史随身听日志不能补队列与断点恢复。
- 墨墨外设：默认按键表在App，所链飞书PDF只取得查看器外壳，页内容未读。iOS最低版本表述冲突、HarmonyOS Next适配声明不能自行统一；不把蓝牙连接当App可执行。
- Khan Kids：KLC-I005/006/018/028/034/041/046/049/050/051仍是目录标题。两个栏目本次普通GET403后停止，未解析文章URL、未请求文章；不能写成10篇正文均403，也不绕过验证继续请求。
''')
append(LIB / 'comparison.md', '''## 学习输入与同步条件（第40批）

| 问题 | 有来源的区别 | 本项目候选取用，非既定需求 |
| --- | --- | --- |
| 同一输入是否总是同一行为 | [墨墨F01](cases/M-017-maimemo.md)鸿蒙答案前后下滑分别是显示答案/认识；文档规则未实操 | AI动作需要学习阶段和当前对象，手势名称本身不足 |
| 内容列表计数是否等于本次导入结果 | [墨墨F10](cases/M-017-maimemo.md)静态图顶部40已选、底部当前0；加入记忆另有入口 | 分开候选、既有状态、本次选择与执行成功数量 |
| 同步开关或连接图标是否足以确认可用 | [墨墨F04/11](cases/M-017-maimemo.md)分别需切后台并等待同步，以及App识别外设 | 配置、触发、连接层、目标接收及结果都需要可查；整机备份另讨论 |
| 通知未出现是否都是失败 | [墨墨F12](cases/M-017-maimemo.md)提前学完且签到则停止当日提醒 | 先核任务是否已完成及提醒条件，再判断故障；未选定通知策略 |
''')
print('Updated cases, 10 reference points, guide/dictionary, brief40 and navigation. Requirements untouched.')
