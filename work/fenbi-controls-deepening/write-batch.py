from pathlib import Path
import json,csv,hashlib,shutil
root=Path('docs/research/interaction-library');w=Path('work/fenbi-controls-deepening');case=root/'cases/M-032-fenbi.md'
baseline=json.loads((w/'baseline.json').read_text())
assert hashlib.sha256(case.read_bytes()).hexdigest()==baseline[str(case)],'case changed concurrently'
art=root/'artifacts/fenbi-controls-deepening';art.mkdir(parents=True,exist_ok=True)
for name in ['historical-practice-screens.jpg','historical-mock-screens.png']:shutil.copyfile(w/name,art/name)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
audit='''# M-032 历史静态界面核查

查阅与观察：2026-10-26。E-FBCD-07；来源为“五彩奥特曼”2019-06-24 在人人都是产品经理发表的一手分析，文章自报 iPhone 6s、iOS 12.1.4、粉笔公考 6.8.8、体验时间 2019 年 6 月。地区仅截图可见“公务员行测-国考”，不套北京或当前客户端。取得方式：公开文章链接的两张拼接截图 HTTP GET→本地 view_image；无登录、视频、连续操作或实机测试。

| 文件 | 原文定位与原始 URL | 已观察的局部 | 限制 | SHA-256 |
| --- | --- | --- | --- | --- |
| historical-practice-screens.jpg | 四、2.2题目练习，第二图；https://image.woshipm.com/wp-files/2019/06/SpJTKydk5g80ceQLfY2o.jpg | 上排：快速练习、题目选项、答题卡、交卷入口；中排：报告、题目解析；下排：考点/来源/笔记区 | 作者标箭头的拼图；未显示点击瞬间、交卷确认或保存；原文试题只作界面背景，不转录题目 | {PRACTICE_SHA} |
| historical-mock-screens.png | 四、2.3模拟考试，第二图；https://image.woshipm.com/wp-files/2019/06/j2CA2WiioOVT3MPNyGaC.png | 左：模考入口；中：已报名和报考职位；右：职位选择及完成报名；中部说明有无法进入考场/答题时联系客服的文字 | 2019-06-23赛事页面，不是当前日历；没有答题过程；客服入口不证明可恢复考试 | {MOCK_SHA} |

两张图均为静态原图保留，不裁剪、不改绘。另在 work/ 看过同节作者流程图，但不正式归档，也不增加独立证据或覆盖数量。全部 hands_on=false；visual_status=static_reference_only；最高 D2。
'''.replace('{PRACTICE_SHA}',sha(art/'historical-practice-screens.jpg')).replace('{MOCK_SHA}',sha(art/'historical-mock-screens.png'))
(art/'static-observation-log.md').write_text(audit)
case_text='''# M-032 粉笔：考试类别、模考赛事与平时练习分开

查阅 2026-10-26。本案例主范围是大陆粉笔 App 的公务员行测；北京历年真题（F03）、国考历史快速练习（F09）、通用行测模考分别标注。事业单位公基（F07）仅保留原有独立类别，不能拿其成绩/解析课规则补行测。没有登录、购买、参与考试、操作账号或实机测试。

## 范围、版本与证据强度

- **查阅日渠道快照**：中国 App Store 开发者为北京粉笔蓝天科技有限公司，iPhone/iPad 当前正文列 6.19.9，HTML 日期字段为 2026-10-20；6.19.8 为 10-15，日志均仅称细节优化，没有交卷/断网规则。官网移动下载页仍列 Android 6.19.6、iOS 6.19.7，页示 2026-09-29；App Store 的 6.19.7 日期为 09-30。渠道日期/版本分别记录，不推断两个构建完全相同。PC 5.1.114/2026-10-21 沿用旧证据。E-FBCD-01、02；E-FENB-02。
- **FAQ/帮助不是当前构建说明**：行测模考 FAQ 公开 HTML 内 9 条内容记录的 `updatedTime` 为 2024-09-10，正文含 2025 考季例子；这是内容记录时间，不是 App 版本或当前赛事安排。E-FBCD-03。
- **历史一手图**：作者“五彩奥特曼”，发布 2019-06-24，自报 iPhone 6s/iOS 12.1.4/粉笔 6.8.8、体验月 2019-06。只观察静态拼图，采用其“公务员行测-国考”分支；不声称亲自作答或连续观察。E-FBCD-07。
- **托管帮助边界**：2020 年环信 tenant8342 知识库正文署名“粉笔”（导出页同租户但未显示署名），构建、系统及地区未注明。环信自身 2021 年客户案例确认粉笔使用其客服服务；本轮未找到粉笔官网指向该具体租户的反向链接。因此记为“署名粉笔的历史托管帮助”，不写成已核定现行官方手册。E-FBCD-08—12。

当前完整真题/计时考试仍为 D1 缺口；D2 只描述材料能重建的局部任务。**F04、F05 由原 D2 收紧为 D1**：原备考文章确有入口及功能声明，但不能确定实际控件、执行后状态、清理规则；保留事实声明和历史差异，未认定功能不存在。

## 功能目录与覆盖

下表按已取得来源建立目录，不声称穷尽全 App；没有足够反馈/状态的功能不单独增加 F。

| 功能域 | 来源章节与适用范围 | 已关联 F/E | 尚缺入口、分支或下一材料 |
| --- | --- | --- | --- |
| 考试类别、地区与试卷 | 开发者描述“考试类型/全国真题”；北京备考文章；历史国考练习图 | F03、F09；E-FENB-07、E-FBCD-01、07 | 当前地区切换、空题库、换类别后未交卷保留；需同构建界面 |
| 专项与智能出题 | App Store 功能特点2—4，声明考点练习、个性出题、预测分 | E-FBCD-01；历史快速练习F09 | 当前题量/难度设置、推荐依据与重新组卷；预测不等于能力测评 |
| 真题/套卷作答和计时 | 北京路径、计时备考文章；历史图只展示快速练习 | F03、F06；E-FENB-07、08、E-FBCD-06 | 跳题、改选、答题卡状态图例、确认、漏题、超时；现行主链未闭合 |
| 模考报名、岗位、错过后补练 | 行测FAQ“参加/岗位/往季/错过”；2019报名图 | F01、F02；E-FBCD-03、07、09 | 当前考场错误提示、中断/补交；旧时间不作日历 |
| 报告、解析与笔记 | 2020“行测模考报告查看方法”；2019快速练习结果图 | F02、F09；E-FBCD-07、09 | 练习历史与模考报告是不同入口；笔记编辑/保存、解析纠错、旧题修订未查清 |
| 错题重做、错因与清理 | 官网备考大全10063/11493；2020四方块目录 | F04；E-FBCD-04、05、08 | 重做何时回收、清理按钮/确认/撤销、按题删除与整库重置、统计是否重算 |
| 收藏与回看 | 11493入口声明；历史“我的收藏”目录 | F05；E-FBCD-05、08 | 星标改变与保存结果、重复收藏、撤销和跨端冲突；未据截图补成功状态 |
| 错题导出及纸面回录 | 2020“错题导出方法”，明确包括公务员行测 | F10；E-FBCD-10 | 当前套餐限额、文件格式、导出失败、扫码回录计分及重复提交 |
| 关键字/图片搜题 | 2020“行测搜题方法” | E-FBCD-11；不新增F | 有入口/发起动作，但结果列表、无匹配、图片识别失败和返回原练习未闭合 |
| 练习历史、用户报告、资料缓存 | 2020“四个小方块”目录 | E-FBCD-08；F10只覆盖导出后缓存入口 | 能发现入口不代表已证恢复草稿；删除历史、缓存清理、账号切换/重装未知 |
| 课程、离线与PC机考 | 开发者描述“直播/回放/离线下载”；官网PC目录 | F08；E-FBCD-01、02、E-FENB-02 | 离线承诺对象是课程；不能推断题库、未交卷会话或解析全离线 |
| 公基解析课 | 事业单位FAQ，独立科目 | F07；E-FENB-04 | 不补行测；课程退款、断流续播不在本轮主范围 |

## 流程

| F 编号 | 入口/前置 | 操作与反馈 | 数据/结束与返回 | 证据与缺口 |
| --- | --- | --- | --- | --- |
| F-M032-01 行测模考报名 | 行测题库→模考大赛 | 选类别/岗位报名；FAQ称未选岗位用通用卷，进入考场时段后锁岗位。历史图另见报名结果与职位编辑页 | 职位表待公告发布后导入；2019无法进入考场/答题的说明只引导联系客服，未给恢复结果 | D2；E-FENB-03、E-FBCD-03、07。FAQ无构建；图为2019国考，不能套当前时间/入口 |
| F-M032-02 错过赛事与旧报告 | 错过考试或回看考季 | FAQ：赛后历年试卷→全部试卷→国考/省考模拟题。2020帮助细化报告：模考大赛→右上模考报告→年份/考季→折线图场次→查看报告 | 补练有分数无职位排名；报告查询不恢复参赛资格 | D2；E-FENB-03、E-FBCD-03、09。无报告、切季加载失败、考中恢复仍未知 |
| F-M032-03 真题选择→解析 | 北京公务员行测题库 | 备考文章描述地区北京→历年真题→年份→练习→解析 | 宏观路径保留，未证选项反馈/交卷状态 | D1；E-FENB-07。F09国考快速练习不能补成北京真题主链 |
| F-M032-04 错题重做与清理 | 2026备考文章称我的→错题；2020帮助称练习→选考试→右上四方块→我的错题 | 文章声明自动汇集、筛选、重做/错因/移除；仅保留声明，不重构按钮状态 | 入口存在历史差异；“重做对后移除”不等于自动删除，更不等于已掌握 | D1（本轮收紧）；E-FENB-05、06、E-FBCD-04、05、08。清理确认、误删恢复和首次/历次答案口径未知 |
| F-M032-05 收藏与回看 | 2026文章称练习页或我的内收藏；2020帮助列我的收藏 | 保存/回看是文章的使用建议与功能声明；历史解析页有星形图标，但未见状态切换 | 收藏和错题集合分开；不推断取消收藏是否影响错题 | D1（本轮收紧）；E-FENB-06、E-FBCD-05、08。没有已证的保存反馈、撤销、去重/同步规则 |
| F-M032-06 计时套卷 | 备考文章称套卷页计时模式 | 声称交卷后看模块耗时/超时题；“按真实考试不可暂停”是含训练要求的表述 | 未核定计时使用本地还是服务器时钟；历史快速练习图的计时显示不能代替套卷规则 | D1；E-FENB-08、E-FBCD-06。确认、漏题、到零、后台/断网、重复交卷均未知 |
| F-M032-07 公基结果和解析课 | 事业单位公基，已交卷 | 本人成绩先查，最高/平均等赛事结束；模考处另购解析课，购后我→我的课程 | 分数、排名与课程权益分别记录 | D2；E-FENB-04。仅公基，不套行测或等同免费题目解析 |
| F-M032-08 离线与桌面 | 官方客户端目录 | 课程离线下载；PC声明模拟机考 | 两端版本及离线对象分开 | D1；E-FENB-01、02、E-FBCD-01、02。题库离线和进度同步未知 |
| F-M032-09 历史国考快速练习→结果→解析 | 2019 iOS6.8.8；练习页“公务员行测-国考”→快速练习 | 作者箭头图将题目页、答题卡“交卷并查看结果”、练习报告相连；报告提供全部/错题解析，展示题目结果颜色、用时与下方解析 | 图中报告为15题；解析下方有考点、来源和笔记入口。可见返回箭头，不补目标页或自动保存 | D2，仅历史静态重建；E-FBCD-07。不是连续操作；未见选项改动、跳题、确认/漏题/超时、退出恢复；不把15写成所有练习固定题量 |
| F-M032-10 历史错题导出→纸面作答→回录 | 2020托管帮助；公务员行测在适用目录，已有错题 | 练习右上四方块→我的错题→导出，按考点多选或近一周选择；导出物从资料缓存→题目下载查看/共享/打印；纸面作答后用扫一扫扫末页码，录入答案看解析 | 历史限额：对应类别会员每次最多300题，非会员5题；导出与回录是不同数据阶段 | D2，仅该历史帮助、租户归属仍有缺口；E-FBCD-10、12。未核文件格式、权限到期/空数据、失败反馈、重复回录与当前价格/限额 |

## 分支、计时和恢复

| 问题 | 本轮可确认范围 | 仍未知 |
| --- | --- | --- |
| 选项、改选、跳题与答题卡 | 2019快速练习图有选项和编号答题卡、交卷入口；只能确认展示 | 点击是否自动换题、改选覆盖哪次记录、未答/标记题色彩含义、回跳动作 |
| 交卷与解析 | 历史图连至练习报告和解析；2020导出帮助说回录后可看解析 | 是否再确认、漏题阻断/提醒、提交中状态、取消、超时自动交卷、提交失败重试 |
| 暂停、后台、断网 | 备考文章要求仿真计时；无具体异常操作文档 | 是否继续计时、保存时点、本地/云优先、重启续答和多设备冲突；不把课程离线扩成离线作答 |
| 报告与历史 | 官方FAQ可区分赛事资格/补练；托管目录列练习历史 | 历史是否含未完成、删除是否影响统计、重复练习覆盖/追加、错题清空与恢复 |
| 题库更新 | FAQ只说明招考岗位公告后导入职位资料 | 题ID更换、撤题/答案纠正如何影响旧答案、错题、笔记及分数 |

**审源修正**：10063 是官方域名备考文章，自动归集、重做等可作 D1 声明；其中“重做时不能看答案”处于复习建议上下文，不能解释成技术禁止。11493 本轮最后一次直接读取已返回正文（此前为加载壳），确有入口描述，但“移出只收藏的心态/每日清单”不能当按钮名。页首与页尾分别出现 2026-09-24/09-18；10063 为08-30/08-28。两页均要求以当期 App 为准，不是绑定构建的验收材料。E-FBCD-04、05。

## 数据、动力与本项目关系

应分开记录考试类别/地区、岗位报名、赛事资格、一次练习、答案草稿与最终提交、题目版本、结果报告、错题集合、收藏、笔记、导出文件和回录尝试。报告可见或错题被移除都不能证明中间答案已同步，也不证明知识掌握。定时赛事/排名提供时间约束与比较反馈；本轮未验证提分效果，预测分仍是开发者声明。

**本项目推断**：G-001 可借鉴“作答后回看错误及原因”，但不以收藏量、移出量或预测分替代学习结果。G-002/R-007/R-021 需要 AI 分清未交卷、提交中、已计分、补练不排名、导出后尚未回录等状态，并在清理错题前知道影响范围。Plus 的选项焦点、长题导航与草稿保存需要原型验证；未决定实现相同题库/课程服务。

## 已查范围与下一取证

本轮核对官方移动下载页、中国 App Store 描述/版本记录、行测模考 FAQ 公开HTML及内嵌内容时间、10063/11493/11264备考文章；检索粉笔官网与 tenant8342 的交卷、暂停、断网、练习历史和错题清理；读取4篇相关托管帮助及环信客户案例。2019作者文章仅保留两张相关界面拼图；同页市场分析、用户画像不作为产品事实。申论智能批改/纸笔录入帮助明确排除，不能填行测漏洞；下载站转载教程、真实公务员考试的纸质答题卡规则均未采用。

取得限制：App Store web打开失败后公开HTTP取得200；10063直接页仍是加载壳、索引有正文；11493先壳后有正文；导出帮助 web正文可读，但保存临时HTML的一次请求超时，未重试。两张历史图的web点击曾超时，公开HTTP取得200并实际 view_image。没有403绕过、账户/学习接口调用或视频下载。官网公开入口JS未见租户关联，未递归拉取全部脚本。检索无结果不能证明功能不支持。

下一最小取证：取得粉笔现行行测操作手册或由获授权者在固定 iOS/Android 构建完成一份短卷，依次展示改选、跳题、答题卡、漏题交卷/取消、超时与一次后台/断网恢复；再展示历史和错题清理确认及恢复。需同时记录考试类别、地区、题ID/版本、网络和计时状态。另核粉笔官方入口到 tenant8342 的来源链。上述均未执行。

## 证据

旧证据保留编号，旧批次不改：

- E-FENB-01：[移动端下载](https://www.fenbi.com/page/download/ios)，版本与课程离线；本轮复核见 E-FBCD-02。
- E-FENB-02：[PC端下载](https://www.fenbi.com/page/download/desktop)，5.1.114/2026-10-21与模拟机考；不套移动界面。
- E-FENB-03：[行测模考FAQ](https://www.fenbi.com/page/mokaodasai-qa/gwy/xingce)，参加、岗位、报告、补练；时间补证 E-FBCD-03。
- E-FENB-04：[事业单位FAQ](https://www.fenbi.com/page/mokaodasai-qa/sydw)，仅公基成绩与解析课。
- E-FENB-05：[错题备考文章10063](https://www.fenbi.com/page/information-hub-detail/10063)，本轮按声明和建议使用，见 E-FBCD-04。
- E-FENB-06：[收藏/错题文章11493](https://www.fenbi.com/page/information-hub-detail/11493)，本轮可读正文及日期差异，见 E-FBCD-05。
- E-FENB-07：[北京真题文章10140](https://www.fenbi.com/page/information-hub-detail/10140)，旧官方索引宏观路径，仍D1。
- E-FENB-08：[计时备考文章11264](https://www.fenbi.com/page/information-hub-detail/11264)，仍D1，见 E-FBCD-06。

本轮新批均查阅 2026-10-26：

- E-FBCD-01：[中国App Store](https://apps.apple.com/cn/app/id584660358)，开发者描述/版本历史；公开HTML正文，6.19.9/2026-10-20；未看商店截图。
- E-FBCD-02：[官方下载页复核](https://www.fenbi.com/page/download/ios)，正文；Android6.19.6/iOS6.19.7/页示09-29；不覆盖商店日期差异。
- E-FBCD-03：[行测FAQ记录时间](https://www.fenbi.com/page/mokaodasai-qa/gwy/xingce)，正文及同页application/json的9条updatedTime=2024-09-10；不是版本号。
- E-FBCD-04：[10063来源审校](https://www.fenbi.com/page/information-hub-detail/10063)，官方索引正文与直接加载壳；08-30/08-28冲突；只证文章声明。
- E-FBCD-05：[11493来源审校](https://www.fenbi.com/page/information-hub-detail/11493)，本轮直接正文；09-24/09-18冲突；入口、建议与控件分开。
- E-FBCD-06：[11264来源审校](https://www.fenbi.com/page/information-hub-detail/11264)，官方索引正文，页示2026-09-21；“不可暂停”不证明实现规则。
- E-FBCD-07：[五彩奥特曼2019一手分析](https://www.woshipm.com/evaluating/2492895.html)，一、1体验环境；四、2.2/2.3第二图；静态观察见[定位与SHA](../artifacts/fenbi-controls-deepening/static-observation-log.md)。仅历史iOS6.8.8。
- E-FBCD-08：[行测题库四方块](https://kefu.easemob.com/v1/tenants/8342/knowledge/entries/html/50542)，2020-07-03署名粉笔，正文；只目录，无历史删除/恢复细节。
- E-FBCD-09：[行测模考报告](https://kefu.easemob.com/v1/tenants/8342/knowledge/entries/html/50643)，2020-07-05署名粉笔，正文；考季/场次/查看报告。
- E-FBCD-10：[错题导出方法](https://kefu.easemob.com/v1/tenants/8342/knowledge/entries/html/50784)，2020-07-06，同租户正文；导出/缓存/打印/扫码回录与旧限额；当前适用未证。
- E-FBCD-11：[行测搜题](https://kefu.easemob.com/v1/tenants/8342/knowledge/entries/html/50797)，2020-07-06署名粉笔，正文；关键字/图片入口，结果与异常缺失。
- E-FBCD-12：[环信客户案例](https://www.easemob.com/press/6746)，2021-06-18，环信厂商正文；只证与粉笔的客服合作，不足以核具体tenant或App按钮。
'''
case.write_text(case_text)
def e(n,title,url,stype,access,scope,locator,note,date=None,build=None):
 return dict(evidence_id=f'E-FBCD-{n:02d}',product_id='M-032',title=title,url=url,checked_on='2026-10-26',source_type=stype,access_mode=access,version_scope=scope,app_build=build,firmware_version=None,source_updated_on=date,locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
B='https://www.fenbi.com/page/';K='https://kefu.easemob.com/v1/tenants/8342/knowledge/entries/html/'
ev=[
e(1,'中国App Store当前描述与版本历史','https://apps.apple.com/cn/app/id584660358','official_app_store','opened_public_html','China iPhone/iPad snapshot 2026-10-26','开发者描述；版本历史6.19.9/6.19.8/6.19.7；time datetime','正文；6.19.9日期2026-10-20，6.19.8为10-15，6.19.7为09-30；日志仅细节优化，不证作答控制。web入口失败后HTTP200读取；未看图片。','2026-10-20','6.19.9'),
e(2,'移动下载页版本复核',B+'download/ios','official_download','opened_public_html','China mobile download page snapshot 2026-10-26','移动端下载版本与更新日期','正文仍列Android6.19.6/iOS6.19.7、2026-09-29；与商店分别记录，旧版本非现行按钮保证。','2026-09-29','Android6.19.6 / iOS6.19.7'),
e(3,'行测模考FAQ及内容记录时间',B+'mokaodasai-qa/gwy/xingce','official_help','opened_text_and_embedded_public_json','Civil-service aptitude mock FAQ; no app build','9条问答；同页application/json中updatedTime','正文与公开内嵌内容记录；9条updatedTime均2024-09-10，不是App版本或当期赛事日历；无需调用账户或学习接口。','2024-09-10'),
e(4,'错题文章10063来源审校',B+'information-hub-detail/10063','official_editorial_advice','official_index_text_and_shell_recheck','Unversioned preparation advice; current UI unverified','一/二功能声明；七重做看答案；末尾信息更新','官方索引正文；直接页加载壳。页首08-30与页尾08-28不同；自动归集/重做/移除仅声明，频率与不能看答案是学习建议；旧F04收紧D1。'),
e(5,'收藏与错题文章11493来源审校',B+'information-hub-detail/11493','official_editorial_advice','opened_text','Unversioned preparation advice; current UI unverified','一入口；三移出措辞；四导出；页首/页尾日期','本轮最后一次直接打开有正文；我的→错题与收藏入口是文章声明；移出心态/每日清单非已证按钮。09-24/09-18冲突；旧F04/F05收紧D1。'),
e(6,'计时备考文章11264来源审校',B+'information-hub-detail/11264','official_editorial_advice','official_index_text','Civil-service aptitude timing advice; no app build','二粉笔怎么用；三操作步骤；四问答','官方索引正文；2026-09-21。计时/报告声明保留；不可暂停夹杂真实考试训练要求，无计时到零/后台/断网实现证据。','2026-09-21'),
e(7,'2019 iOS6.8.8一手作者静态练习与模考图','https://www.woshipm.com/evaluating/2492895.html','third_party_firsthand_review','opened_text_and_static_images','Author 五彩奥特曼; iPhone6s iOS12.1.4; June2019; civil-service aptitude national exam','一1体验环境；四2.2第二图；四2.3第二图','正文自报版本；已查看作者箭头拼图：快速练习/答题卡/报告/解析及报名职位页。不是连续操作或当前构建；无交卷确认与中断恢复。','2019-06-24','6.8.8'),
e(8,'历史行测题库四方块目录',K+'50542','vendor_hosted_help_attributed','opened_text','2020 help attributed to 粉笔; OS/build/region unknown','标题；2020-07-03署名；目录正文','正文；练习→考试题库→四方块列练习历史、我的错题/收藏、资料缓存等；入口存在不证明删除或恢复。具体租户官方反链未核；图片未看。','2020-07-03'),
e(9,'历史行测模考报告查看方法',K+'50643','vendor_hosted_help_attributed','opened_text','2020 help attributed to 粉笔; OS/build/region unknown','2020-07-05；查看成绩步骤','正文；年份/国省考考季、折线场次、查看报告；具体租户归属反链未核；无报告/失败分支缺失，图片未看。','2020-07-05'),
e(10,'历史错题导出方法',K+'50784','vendor_hosted_help_attributed','opened_text','2020 tenant8342 help; includes civil-service aptitude; build unknown','正文导出、资料缓存、扫一扫；温馨提示1—2','web正文可读；临时HTML请求一次超时未重试。旧限额会员对应科目300/非会员5；包含导出后扫码回录；同租户未显示署名，官方反链未核，不宣称当前有效。','2020-07-06'),
e(11,'历史行测搜题方法',K+'50797','vendor_hosted_help_attributed','opened_text','2020 help attributed to 粉笔; OS/build/region unknown','2020-07-06；关键词与图片搜题正文','正文；放大镜后关键字回车/搜索，或图片搜题上传；没给结果/无匹配处理，不增完整F；图片未看。','2020-07-06'),
e(12,'环信关于粉笔客服合作的厂商案例','https://www.easemob.com/press/6746','official_vendor_case_study','opened_text','Easemob customer case 2021; not Fenbi operation manual','一环信全渠道客服；合作说明','正文确认粉笔使用环信客服；仅作托管来源的外围归属线索，不足以证明tenant8342唯一归属或现行操作。','2021-06-18')]
ev[6].update(visual_observed=True,archived_artifact='artifacts/fenbi-controls-deepening/historical-practice-screens.jpg',sha256=sha(art/'historical-practice-screens.jpg'),capture_state='archived_static_images_with_observation_log',visual_audit_path='artifacts/fenbi-controls-deepening/static-observation-log.md',visual_audit_sha256=sha(art/'static-observation-log.md'),additional_artifacts=[{'path':'artifacts/fenbi-controls-deepening/historical-mock-screens.png','sha256':sha(art/'historical-mock-screens.png')}])
(root/'batches/fenbi-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in ev))
with (root/'batches/chinese-apps-coverage.csv').open() as f:
 reader=csv.DictReader(f);fields=reader.fieldnames;old={r['flow_id']:r for r in reader if r['product_id']=='M-032'}
new=[]
for num,task,depth,es,branches,gaps,vis in [
(9,'历史iOS6.8.8国考快速练习到报告解析','D2','E-FBCD-07','静态拼图：答题卡交卷入口、报告及全部/错题解析','当前版适用；改选/跳题/确认/漏题/超时/恢复；返回目标','static_reference_only'),
(10,'历史行测错题导出与纸面作答回录','D2','E-FBCD-10;E-FBCD-12','2020托管帮助；按考点或近一周；旧会员300题/非会员5题；缓存查看与扫码回录','具体租户官方反链；当前限额；格式/失败/空数据/重复回录计分','not_observed')]:
 new.append(dict(zip(fields,[f'F-M032-{num:02d}','M-032',task,depth,es,branches,gaps,vis,'false','cases/M-032-fenbi.md','2026-10-26'])))
with (root/'batches/fenbi-controls-deepening-coverage.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(new)
changes={
'F-M032-01':dict(evidence_ids='E-FENB-03;E-FBCD-03;E-FBCD-07',known_branches='FAQ通用卷、岗位导入与锁定；2019图有职位编辑/报名结果；无法入场或答题仅引导客服',gaps='现行构建入口、报名失败、答题与交卷恢复；FAQ内容时间非版本',visual_status='static_reference_only'),
'F-M032-02':dict(evidence_ids='E-FENB-03;E-FBCD-03;E-FBCD-09',known_branches='补练有分无职位排名；2020托管帮助：年份/国省考季→折线场次→查看报告',gaps='具体托管租户官方反链；当前入口、空报告/失败、考中恢复与补交'),
'F-M032-04':dict(depth='D1',evidence_ids='E-FENB-05;E-FENB-06;E-FBCD-04;E-FBCD-05;E-FBCD-08',known_branches='备考文章声明自动汇集、筛选/重做/归因/移除；2020四方块目录与2026我的入口并列',gaps='控件与执行结果未核；清理确认/误删恢复/统计重算、首次与历次答案口径、同步'),
'F-M032-05':dict(depth='D1',evidence_ids='E-FENB-06;E-FBCD-05;E-FBCD-08',known_branches='文章声明练习页或我的收藏；2020托管目录按考试类别列我的收藏',gaps='收藏保存反馈、取消与撤销、重复与同步；当前构建与具体租户官方反链'),
'F-M032-06':dict(evidence_ids='E-FENB-08;E-FBCD-06',known_branches='计时模式/模块耗时是备考文章声明；不可暂停未证为技术规则',gaps='现行套卷答题卡、改选/跳题、确认/漏题/超时、后台/断网与重复提交'),
'F-M032-08':dict(evidence_ids='E-FENB-01;E-FENB-02;E-FBCD-01;E-FBCD-02',known_branches='课程离线；商店iOS6.19.9与下载页6.19.7并存；PC版本独立',gaps='题库/草稿离线、缓存清理及端间进度同步')}
proposed=[];deltas=[]
for fid,delta in changes.items():
 r=old[fid].copy();r.update(delta);proposed.append(r)
 deltas.append({'flow_id':fid,'field_updates':{k:{'before':old[fid][k],'after':v} for k,v in delta.items()},'status':'proposal_only_master_and_old_batch_unchanged'})
with (w/'existing-coverage-updates.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(proposed)
(w/'existing-coverage-field-changes.json').write_text(json.dumps(deltas,ensure_ascii=False,indent=2)+'\n')
print('wrote case,12 new E,2 new F,6 old-row proposals,2 static images+audit; no old batch/master writes')
