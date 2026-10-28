from pathlib import Path
import hashlib, json, re

r=Path('docs/research/interaction-library')
files=['M-024-casio.md','M-025-youdao-pen.md','M-026-eudic.md']
before={n:(r/'cases'/n).read_text() for n in files}
f_before={n:sorted(set(re.findall(r'F-M\d+-\d+',s))) for n,s in before.items()}
Path('work/dictionary-research/inventory-baseline.json').write_text(json.dumps({'flow_ids':f_before,'sha256':{n:hashlib.sha256(s.encode()).hexdigest() for n,s in before.items()}},ensure_ascii=False,indent=2)+'\n')

def replace_section(name,start,end,body):
    p=r/'cases'/name;s=p.read_text()
    assert s.count(start)==1 and s.count(end)==1
    a=s.index(start);z=s.index(end,a)
    p.write_text(s[:a]+body.strip()+'\n\n'+s[z:])

replace_section('M-024-casio.md','## 功能目录与实体输入','实体键在',r'''
## 功能目录与覆盖

2026-10-26对照MA2307-A手册目录（4—8页）及对应章节重新盘点。下表的“可继续”指已经定位公开材料，不等于已完成流程分析；按本型号手册列功能，不以现有9条F充当全功能清单。收录内容须再按178—182页区分E-W100/E-W220，不能认为两机每本词典相同。E-CASIO-14。

| 功能域 | 官方章节定位（PDF/印刷页） | 已关联F / E | 尚缺入口或分支、可继续程度 |
| --- | --- | --- | --- |
| 初次准备、电源与恢复 | 32—39页；231页复位 | F-M024-09；E-CASIO-01、14 | 合盖续用已查；初次语言/时钟、复位调整与数据保留未成链。**有正文可继续**，不能用合盖恢复替代复位核查 |
| 实体、手写和屏上输入 | 40—43、77—89页 | F-M024-01、02仅键盘部分；E-CASIO-02、12、13、14 | 手写识别候选、改字、无法识别与软键盘切换尚未分析；**有正文可继续** |
| 基础与高级检索 | 49—76、91—107页 | F-M024-01～03；E-CASIO-03、04、08、14 | 基础词条已有；例句/惯用语组合筛选、并排查询、快查窗返回链未覆盖全；**有正文可继续** |
| 解释显示与导航 | 60—71页 | F-M024-01、02局部；E-CASIO-04、14 | 字号、画面格式、汉字放大及显示语言没有独立分析；重设/退出后的保持范围未核；**有正文可继续** |
| 语音、录音对照与演讲 | 108—123、220—222页 | F-M024-06只涉及特定课本发音；E-CASIO-07、14 | 通用语音和演讲并非同一任务；演讲保存/回放/删除、语音中断和麦克风设置未形成链；**有正文可继续** |
| 学习簿：生词本和背诵卡 | 140—152、164—173页 | F-M024-04、05；E-CASIO-05、06、09、14 | 已覆盖核心制作/练习；满额、删后恢复、断电与数据流出尚缺；手册可继续查已写明的容量/删除边界，外部同步无充足资料 |
| 学习簿：随记与便笺 | 142—144、153—163页 | **无对应F**；E-CASIO-09、14 | 空白随记、词典画面批注和词条便笺的保存/找回/删除未分析；不能由背诵卡推导；**有正文可继续** |
| 内置内容：查词和课标训练 | 175—193页 | F-M024-06、07；E-CASIO-07、14 | 已核课标/高考词及同步练；仍需按词典指南核可用功能，不能泛化题型和评分 |
| 内置阅读与其他语言训练 | 194—216页；型号清单178—182页 | **无对应F**；E-CASIO-14 | 名著/对照阅读及书签、Oxford iSpeaker、其他发音/口语、SAT阅读和新东方训练尚未分析；**有内容专章可继续**；不是任意外部电子书兼容证据 |
| 追加资源、存储卡与教学连接 | 132—139页 | F-M024-08；E-CASIO-01、10、11、14 | 无线下载已有；卡的识别/替换与白板连接未形成链。卡有正文；白板仅给机构专用软件与咨询入口，**缺公开安装/操作链**；还受已记录的停服时间约束 |
| 首页与系统个性设置 | 217—230页 | **无对应F**；E-CASIO-14 | 常用内容排序/移除、一般设置、用户名改删未分析；**有正文可继续**；用户名不等于云账号 |
| 工具和异常提示 | 124—131、234—244页 | **无对应F**；E-CASIO-14 | 计算/日历工具及按提示诊断未系统拆解；查不到词、音声/电源和存储提示有官方对照可继续；实际提示时序与恢复效果仍未观察 |

优先补齐“查词之外仍有的任务”是随记/便笺、内置阅读和通用语音；优先补齐“现有任务的失败分支”是输入无命中、复位及存储提示。AI接口、账号云同步和任意PDF/ePub导入不因目录未列而判为不支持，也不从其他EX-word系列迁入。此盘点未增加F、未提高深度。
''')

replace_section('M-025-youdao-pen.md','## 功能目录','## 操作链与状态',r'''
## 功能目录与覆盖

2026-10-26重新对照同一普通2.0历史产品页及直链帮助。**已取得的操作帮助只覆盖登录和手机实时显示；没有完整硬件手册目录**。下表因此区分“有操作正文可继续”“只有声明/模板”和“未取得来源”，不把专业/加强版材料补进来。

| 功能域 | 来源章节/资源 | 已关联F / E | 尚缺入口或分支、可继续程度 |
| --- | --- | --- | --- |
| 印刷中英词查询及词典选择 | 产品页扫描、专业词典模块 | F-M025-01（D1）；E-YDP-01 | 扫描姿态、选错词/识别改字、词典切换及无命中未分析；**仅功能声明，缺硬件操作帮助** |
| 多行文本翻译 | 产品页多行扫描翻译模块 | F-M025-01只列范围；E-YDP-01 | 多行如何追加、确认/清空、重扫、译文回原文未知；**不能由单词扫描流程外推** |
| 发音与蓝牙耳机 | 产品页真人英美音、蓝牙耳机模块 | F-M025-01仅发音声明；E-YDP-01 | 切音/重播、配对入口、断联回退、离线音库未知；**声明可定位，操作资料仍缺**；手机实时显示蓝牙帮助不等于耳机帮助 |
| 语音助手 | 产品页语音查词/古诗/天气模块 | **无对应F**；E-YDP-01 | 唤醒、录音结束、取消、识别纠错和网络失败均未分析；**仅声明，无足够操作正文** |
| 硬件输入和系统维护 | 产品页触屏、在线更新模块 | **无对应F**；E-YDP-01 | 实体键、启动/休眠/复位、联网更新入口与回滚无型号明确帮助；产品更新承诺不证明历史服务现仍可用 |
| 笔端登录与设备关联 | 帮助function_explain“如何为词典笔登录账号” | F-M025-02；E-YDP-02、05 | 正常扫码成功反馈已查；码过期、相机权限、账号冲突未说明；**现有帮助已覆盖其给出的正常链，需另补异常资料** |
| 手机实时释义和重连 | 帮助function_explain“如何使用查词实时显示” | F-M025-03；E-YDP-02 | 蓝牙失败与离页断开有正文；手机退后台/多笔切换/未同步缓存未知；不能称全部数据同步 |
| 学习数据、周词单 | 主面板与学习详情模板chunk-3734dbd6 | F-M025-04（D1）；E-YDP-03 | 可定位统计及取数据失败文案；扫描词→入词本→词单浏览/练习的保存动作未分析；**模板可继续核标签，不能补出成功流程** |
| 提醒和解绑 | 设置模板chunk-f5a293f4 | F-M025-04（D1）；E-YDP-04 | 有提醒开关和解绑结果文案；通知权限、失败重试、解绑前后数据范围未说明；**缺操作与服务现状证据** |
| 收藏练习、导出与恢复 | 当前取得的普通2.0材料无完整专章 | 无完整F；E-YDP-01～05为本轮范围索引 | 是否携带原句、练习题型、重复收藏、批量导出、删后恢复和多端冲突均待取证；未知不是不存在 |

本型号最关键的资料缺口仍是“扫描结果校订→保存到词本→取回学习”。小程序的计数和提醒不能补齐这条链。下一步须先取得同型号硬件帮助，现有模板再拆也不能替代；此盘点不增加F或提升D1。
''')

replace_section('M-026-eudic.md','## 功能目录','## 操作流程',r'''
## 功能目录与覆盖

2026-10-26按官方帮助目录E-EUD-13逐项对照，并补读此前未分析的六篇帮助。下表的“有正文”只标明可研究材料；未新增F或把新功能目录自动算成D2。平台依章节区分，帮助未固定App构建时仍保留未知。

| 功能域 | 官方帮助章节 | 已关联F / E | 尚缺入口或分支、可继续程度 |
| --- | --- | --- | --- |
| 单词输入、纠错和查询历史 | 单词查询；发音和跟读 | F-M026-01、08；E-EUD-01、11 | 正常查词/纠错已查；相机、语音输入的校订/失败、历史撤销不足。正文返回新补依据见E-EUD-16；不等于文件阅读返回 |
| 跨软件取词 | 跨软件取词：iOS、Android两个页签 | F-M026-01只含URL回调，**未覆盖该功能域**；E-EUD-14 | 有翻译窗口、系统默认翻译App、共享/iPad分屏、Android浮窗与复制查词正文；权限拒绝、后台结束和回来源仍未成链。iOS默认翻译要求18.4+，Android复制取词的系统限制须分开处理 |
| 全文检索与索引 | 全文搜索 | **无对应F**；E-EUD-15 | 已定位词库启用索引、完成提示和查询入口；尚未分析索引中断、更新/删除词库后的重建及空结果。**有正文可继续**，不能用普通词头查询代替 |
| 解释页结构与返回 | 解释页面设置1—6 | F-M026-01、03仅局部；E-EUD-16 | 词典折叠/排序/隐藏、释义内搜索、主题未展开；已确认上一词与返回搜索是两种返回目标；配置保持/误隐藏恢复待核。**有正文可继续** |
| 原句资料与笔记 | 笔记和标注；例句卡片；文件阅读 | F-M026-02、03；E-EUD-02～04 | 词条、词典例句和外部原文是不同来源；自动携带阅读原句、误划线撤销和重排后锚点未闭合 |
| 生词组织和同步 | 我的生词本；单词学习常见问题；VIP功能介绍 | F-M026-04、07；E-EUD-05、08、19 | 分组和手动同步已有；冲突、重复词、账号切换仍缺。VIP页另列图片/声音笔记及账号权益，**仅声明，不承诺备份永不丢失** |
| 正式记忆、自由补练与恢复 | 单词学习；算法及设置；回顾练习；常见问题 | F-M026-05～07；E-EUD-06～09、12 | 已覆盖旧帮助主要训练机制；FSRS与撤回只见新版日志，仍需对应构建操作帮助。不能套旧调度规则 |
| 词库、文档与离线资源 | 添加扩充词库；文件阅读 | F-M026-02、09；E-EUD-02、10 | 文件/资源导入已有；损坏、重名替换、资源缺配及学习数据导出全链未覆盖；离线不包括全部AI/语音服务 |
| 外接键盘 | 外接键盘快捷键：按当前页面分组 | **无对应F**；E-EUD-17 | 有苹果系统键位及笔记保存/不保存两种退出说明；快捷键加词与触屏按钮不是同一入口。焦点、冲突、外设断联未分析；**有正文可继续** |
| 外部调用与带上下文查询 | API & URL Scheme | F-M026-01仅回调局部；E-EUD-18 | 当前页给带context的查询及复习/相机/同传入口；API详情另有链接，本轮未读完API正文。**传入上下文不等于保存原句或读写全部数据**；失败返回/鉴权/幂等仍未知 |
| 新版AI、翻译与扩展学习 | 官方更新日志；API & URL Scheme中的平台限定入口 | F-M026-10（D1）；E-EUD-12、18 | AI问答、翻译引擎、视频学词、家长控制及同传未形成任务链；停止录音、数据保存、额度和离线范围不能据功能名补全 |
| 会员权益和跨端 | VIP功能介绍；客户端下载目录项 | **无独立F**；E-EUD-13、19 | 可继续找具体端/账户资料；当前文档不能给出现行价目、免费/会员全部差异或各端功能等价结论 |

新增材料使“跨软件取词、全文索引、显示组织、外接键盘、外部接口”成为明确待深化的主功能域；它们此前未被10条F完整覆盖。API入口是重要线索，但本轮没有调用接口、读取账户或安装扩展。
''')

p=r/'cases/M-026-eudic.md';s=p.read_text()
s=s.replace('历史可侧栏清空或单词左划删除；iOS6.3.0+ URL回调可把返回设到来源App | E-EUD-01。普通查询返回、误识别校订和清历史撤销未核','历史可侧栏清空或单词左划删除；解释页右划/左上箭头回上一词，左下返回回搜索；iOS6.3.0+ URL回调可设来源App | E-EUD-01、16。误识别校订和清历史撤销未核；两种返回不套文件阅读')
s=s.replace('查词当刻加词按钮、冲突和完成提示未明确','查词当刻触屏加词按钮、冲突和完成提示未明确')
s=s.replace('确切加词按钮与回到原书位置未由本轮帮助闭合','确切触屏加词按钮与回到原书位置未由本轮帮助闭合')
p.write_text(s)

new=[]
def E(id,p,title,url,locator,note,scope,typ='official_help',mode='opened_text'):
    new.append(dict(evidence_id=id,product_id=p,title=title,url=url,checked_on='2026-10-26',source_type=typ,access_mode=mode,version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None))
E('E-CASIO-14','M-024','E-W100/E-W220 手册功能目录及未覆盖章节','https://www.casio.com/content/dam/casio/global/support/manuals/exword/pdf/003-zh-cn/e/E-W_series_full_CN.pdf','PDF/印刷4—8页目录；77—89、108—144、153—163、175—182、194—244页章节定位','复用已下载官方PDF，用PyMuPDF核对目录和章节；只新增覆盖盘点，不声称已完整研究每章。','E-W100/E-W220 China; MA2307-A July 2023; exact firmware unknown','official_manual','existing_downloaded_pdf_text')
base='https://docs.eudic.net/1/'
for n,title,path,loc,note,scope in [
(14,'跨软件取词','shi-yong-zhi-yin/dan-ci-cha-xun/kua-ruan-jian-qu-ci','iOS/Android页签各分支','正文盘点；iOS默认翻译和Android复制查词限制分别记录；未看GIF/截图，未运行。','Eudic mobile; iOS/iPadOS default translation requires 18.4+; Android branches; exact app build unknown'),
(15,'全文搜索','shi-yong-zhi-yin/dan-ci-cha-xun/quan-wen-sou-suo-1','词库详情索引开关；完成状态；搜索及解释页展示','正文盘点；与词头搜索分开，未测索引耗时。','Eudic mobile help; exact app build/platform limits unknown'),
(16,'解释页面设置','shi-yong-zhi-yin/jie-shi-ye-mian-she-zhi','1—6：折叠、排序、笔记、搜索、主题、返回','正文；补充上一词和搜索页两种返回目标，不能套文件阅读器。','Eudic explanation page; exact app build unknown'),
(17,'外接键盘快捷键','jin-jie-gong-neng/wai-jie-jian-pan-kuai-jie-jian','首页/解释/翻译/笔记界面分组；iOS条件','正文盘点；保存和不保存退出分开；未连接外设。','Eudic Apple mobile help; page says iOS9 or above; translation shortcuts iPhone only; build unknown'),
(18,'API & URL Scheme','jin-jie-gong-neng/url-scheme','仅欧路词典表；API详情链接','正文盘点；context参数与入口不证明原句持久保存；没有调用URL或API。','Eudic app; platform qualifiers per entry; not other language assistant apps'),
(19,'VIP功能介绍','hui-yuan-jie-shao/vip-gong-neng-jie-shao','学习数据同步；多媒体笔记；账号/跨设备；阅读批注','正文权益声明；无构建和现行价目，不将营销保证当数据不丢失验证。','Eudic VIP public help; exact date/build/entitlement changes unknown')]:
    E(f'E-EUD-{n:02}','M-026',title,base+path,loc,note,scope)

for name in files:
    p=r/'cases'/name;s=p.read_text()
    product={'M-024-casio.md':'M-024','M-025-youdao-pen.md':'M-025','M-026-eudic.md':'M-026'}[name]
    extras=[x for x in new if x['product_id']==product]
    if extras:
        s+='\n'+'\n'.join(f"- {x['evidence_id']}：[{x['title']}]({x['url']})；{x['locator']}。{x['note']}" for x in extras)+'\n'
        p.write_text(s)
    assert sorted(set(re.findall(r'F-M\d+-\d+',s)))==f_before[name],name

(r/'batches/dictionary-inventory-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in new))
print('UPDATED 3 cases; ADDED',len(new),'evidence; flow ID sets unchanged')
