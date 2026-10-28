from pathlib import Path
import csv,json
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';work=root/'work/office-aipaper-deepening'
x=lib/'cases/M-009-iflytek-office.md';old=x.read_text();old_evidence=old.split('## 证据\n',1)[1]
oldrows=[l for l in old.splitlines() if l.startswith('| F-M009-')]
replace={
1:'| F-M009-01 | 电脑书籍/文档入库 | USB或同LAN；本地文件 | 历史手册将书放入讯飞阅读、Office放office文档；WiFi路径为电脑导入→网址拖文件。较新未标版帮助概括内部共享存储。 | 原手册另有手机读写客上传→设备显式下载；TF卡路径见印刷49。Office帮助不保留文件夹展示。 | E-IFLYO-02；E-IXCD-01 | 版本间目录差异、同名覆盖、索引刷新和断线；未从共用路径推导自动扫描全部目录。 |',
2:'| F-M009-02 | 云盘/邮件资料送入阅读 | 已登录相关账号 | 百度网盘/OneDrive下载后按格式进书架或文档；邮件附件可保存邮件附件目录并预览。 | 下载到设备与编辑云原文件分开；邮件附件目录与书架条目关系未完全核定。 | E-IFLYO-02；E-IXCD-01 | 双向回传、授权失效、删本地与云文件、重名/中断。 |',
4:'| F-M009-04 | 把笔记连同需要的文本带到PC | 同账号；可联网 | 历史手册记录→上传→读写客PC生成Word；后续未标版本帮助要求转写先加入笔记，再同步/导出。 | 保留两个时期的差别；写作插入音频需PC，不能据历史概述省略后续帮助的步骤。 | E-IFLYO-01；E-IXCD-01 | 加入笔记具体按钮、重复同步、当前PC/手机构建及可编辑性。 |',
6:'| F-M009-06 | 分享/导出失败后定位 | 笔记已存在 | 帮助：邮件文本可取、音频仅收听；排查标题特殊字符；所称离线转写内容可先导至书架再USB取出。 | 播放链接不等于音频文件；本条出口没有证明识别是在设备本地完成。 | E-IFLYO-01；E-IXCD-01 | 链接期限/权限、取消/保真；历史手册的断网补转另见F12。 |',
7:'| F-M009-07 | 控制自动上传 | 历史X2账号设置；现行通用政策菜单另列 | 历史笔记自动上传开关在编辑退出时触发；不开可手动上传。当前政策名为自动上传读写笔记/笔记自动同步。 | 开关、手动上传与加密后的云副本删除分别处理；不能据退出编辑就宣称服务器完成。 | E-IFLYO-04；E-IXCD-01；E-IXCD-07 | 当前构建菜单、上传失败/排队、关闭前旧云副本与同步冲突。 |'}
oldrows=[replace.get(int(l.split('|')[1].strip().split('-')[-1]),l) for l in oldrows]
prefix='''# M-009 讯飞智能办公本 X2：记录、回听与数据去向

## 范围与版本

查阅2026-10-26。固定中国市场 **智能办公本X2**，不含X2-LAMY、学习机X2、X3/X5、Air2、国际版AINOTE或听见独立服务。实际设备及读写客构建未知。

本次取得官方域名的54文件页原手册：封面明确X2，目录i–iii、正文1–50。PDF元数据写2020-11-05，示意图也有2020日期，但没有固件号，**元数据不当官方发布日期或当前版本**。2023 AI升级公告、未标版本的X2帮助、2026通用政策分别保留，不能拼成已实测的一版系统。[E-IXCD-01；E-IFLYO-01～04]

D1/D2文档研究。全文提取阅读，并查看封面及部分操作摘页；本轮归档6张关键页。它们是静态示意，未连续观察操作，没有实机、登录、录音、配对或导入。两篇旧X2帮助仍只有官方搜索索引全文，直链404；服务中心X2使用指南链接也未成功，原PDF通过搜索取得后HTTP200。

## 功能目录与覆盖

[历史手册目录台账](../inventories/M009-X2-manual-inventory.csv)登记64条章/节，含父子重叠，**不是64项独立功能或当前完成率**。全文读到不等于每项已建立完整任务；空F映射继续留作缺口。

| 域/印刷页 | 已补范围 | 尚未闭环 |
| --- | --- | --- |
| 入门1–4 | 电源/指纹、桌面、AIUI语音找笔记；普通长按与强制关机分开 | 首次激活、全局手势、语音搜索结果及异常；AIUI不等同2023大模型纪要 |
| 写作5–13 | 新建/保存、子标题、输入与笔势纠错、录音条、页管理、手写转文字和搜索 | 输入误判撤销、图片流转、分页移动后的锚点、原笔迹与转文字保留关系 |
| 记录13–19 | 即时录音转写、标记/专注、多人限制、文字/笔迹回听、纠错、断网补转 | 锁屏/强退、修正后的时间对齐、上传失败与重复更新 |
| 笔记保护19–21 | 独立密码、短信恢复、加密后云副本规则 | 当前服务实效、加密前已分享链接、离线传播与取消加密后重传 |
| 邮件21–31 | 历史账号授权、草稿/废弃、附件、删除目录 | 当前提供商授权、发送失败、附件与远端邮件关系；通讯录/星标仍待任务化 |
| 阅读31–42 | 入库、导航、书签/摘录、听书、PDF模式、Office/云盘、新闻网文 | 听读断点、跳转回退、书城交易、Office原件批注保真、当前新闻来源/编辑 |
| 日历/应用42–43 | 日期找笔记、手写日程；商店能力存在 | 结构化提醒、下载/卸载完整流程及数据保留 |
| 设置/同屏44–50 | 账号切换、自动上传、存储格式、更新及重置边界、两条投屏路径 | 数据合并冲突、整机恢复、当前固件与云服务；无线/指纹完整错误链未完成 |

## 任务流程

操作和反馈依文档重建，静态图只支持所示布局；表中的“返回”“删除”等未经本项目实际执行。F01–08保留原编号并补证，F09起补此前未覆盖任务。

| F 编号 | 任务 | 前置状态 | 操作与文档反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
'''
suffix='''
## 分支、矛盾与修订

| 问题 | 依据及修订 | 不作的推定 |
| --- | --- | --- |
| “离线录音转写”是否离线识别？ | 历史手册印刷18明确断网停止即时转写但继续录音，之后WiFi上传再更新文本。旧例“离线识别与云端条款冲突”改为按时期/模式核对。[E-IXCD-04] | 这段文字不证明本地ASR；后续固件是否增加另一离线模式仍未知。 |
| 写作录音与会议记录 | 写作内有独立录音块；会议记录有原声、转写、手写时间关联。[E-IXCD-01/03] | 不把两者删除/导出/续录规则互套，也不保证AI纪要已继承关联。 |
| “注销”后笔记不见 | 手册上下文指登出，重登原账号可见；与恢复出厂清除数据不同。[E-IXCD-07] | 不将该菜单名直接解释为删除云账户。 |
| 加密是否只是加锁？ | 历史手册规定加密停止上传并删除已同步云笔记。[E-IXCD-05] | 未确定删除时点、云备份/分享链接和现版执行，不承诺可逆或安全效果。 |
| TTS与批注冲突 | 印刷36要求先暂停听书才可批注；印刷38省电模式也不支持批注。[E-IXCD-01] | 不推断缺按钮即不支持该功能；先查当前模式。 |
| 翻页方向 | 印刷33将左侧点击/左滑写成上一页，右侧点击/右滑写成下一页。[E-IXCD-01] | 保留原表述疑点，不按常见手机习惯擅自改正，也未建议照搬。 |
| 回听、搜索与纠错 | 文字/笔迹可定位原声，转写有编辑/替换入口。[E-IXCD-03/04] | 修正文字后时间码是否重排、搜索用旧稿或新稿都未知。 |
| 2023大模型升级 | 公告确认X2会议纪要/语篇规整，原始手册年代早于此。[E-IFLYO-03] | 本次传统录音链补至D2，不把AI纪要F08一并升级。 |
| 账户权益和永久保存 | 手册有历史VIP/存储及网文长期保存说法。[E-IXCD-01] | 不能作为2026套餐或永久可用承诺。 |

## 对本项目的分析

以下是助手分析，尚未成为设计决定。G-002需要能区分“正在录音、转写暂停、等待上传、已有新稿但尚未更新页面”等状态；只读当前截图无法准确判断任务完成。原音、转写、手写锚点、手动加入笔记的文字与AI纪要须分别研究，尤其要知道修改影响哪些对象。

对成人备忘/知识整理，点笔迹回原声、关键字回溯和显式纠错值得比较。对5–6岁读听，暂停、返回原文、定时和继续故事不是一个动作；X2只补了历史入口，仍不能证明R-018的完整续播。Plus上的音频链、显示与触控成本须单独验证，X2的硬件和Android应用能力不自动迁移。

下一项最小核查：固定X2实际版本，短录音中断网再恢复，编辑一个转写词后点击回听，观察重复补转是否覆盖修订；随后验证听书暂停/批注/返回及账号切换。加密和重置的本地、云端对象应使用专门测试资料核对。本轮没有执行这些测试。

## 证据
'''
x.write_text(prefix+'\n'.join(oldrows)+'\n'+(work/'x2-new-rows.md').read_text()+suffix+old_evidence)
# Append new sources as a second table for a clear acquisition boundary.
evidence=[json.loads(l) for l in (lib/'batches/office-aipaper-deepening-evidence.jsonl').read_text().splitlines()]
def sources(pid):
 text='\n### 本次原手册与静态页\n\n所有来源查阅2026-10-26；同一PDF的不同页不是独立交叉验证。\n\n| E 编号 | 来源与定位 | 限制/保存 |\n| --- | --- | --- |\n'
 for e in evidence:
  if e['product_id']!=pid:continue
  artifact='；[摘页](../'+e['archived_artifact']+')' if e['archived_artifact'] else ''
  text+=f'| {e["evidence_id"]} | [{e["title"]}]({e["url"]})；{e["locator"]} | {e["note"]}{artifact} |\n'
 return text
x.write_text(x.read_text()+sources('M-009'))
v=lib/'cases/M-007-viwoods.md';s=v.read_text()
s=s.replace('D1+D2资料研究；没有登录、安装、亲自操作、连续视频或单帧视觉观察。官网手册链接可发现，但PDF未由web工具打开；另读到厂商手册的70页镜像提取文字，部分乱码，未把乱码猜成按钮。','D1+D2资料研究；没有登录、安装、亲自操作或连续视频。本次沿官网AiPaper英文链接取得70页原PDF并查看部分静态页，归档7页；web因文件过大未解析，普通HTTP200取得。PDF元数据2025-04-30不当版本号；SHA256与原镜像URL标识相同，因此补的是原件取得与图像证据，不是新手册。编码异常处凭图核读，不猜按钮。[E-VICD-01～08] ')
s=s.replace('只表示已定位目录，不表示所有菜单均完成重建。[E-VIW-12]','本次按实际查看的目录建立[17章台账](../inventories/M007-historical-manual-inventory.csv)；只表示历史章节范围，不是当前全部菜单或独立功能总数。[E-VIW-12；E-VICD-02]')
s=s.replace('表中“反馈”均是文档描述，不是本轮观测。','表中“反馈”均依文档；静态图已查看的条目标E-VICD编号，但没有执行其动作。')
s=s.replace('\n## 版本差异、错误与数据边界','\n'+(work/'viwoods-new-rows.md').read_text()+'\n## 版本差异、错误与数据边界')
s=s.replace('| 修改、取消和删除 | 文档可改名/锁定/删除；当前大纲/标记已有移除路径。 | 整本/单页/录音/摘要/云副本的删除范围与回收期未统一核实。 |','| 修改、取消和删除 | 历史Daily事件可编辑/删除；解除外部日历绑定仅保留本地创建事件。[E-VICD-04/05] | 事件、手写任务、日历笔记与云笔记是不同对象；不把解绑规则套到全部内容，回收期未统一核实。 |')
s=s.replace('本轮只读文本，部分网页通过requests取得HTTP200正文；没有测延迟、残影、60分钟录音或AI准确率。','本轮补原PDF静态观察，图片中的按钮未实际触发；没有测延迟、残影、60分钟录音或AI准确率。历史手册不含V3.15原生云说明，F11仍D1；Daily图可见任务框，但不能证明手写待办创建/完成闭环，F12也保持D1。')
s=s.replace('全部查于2026-10-26。版本文章来源日期未明者不推算；12为历史厂商手册镜像，部分文字乱码、无视觉验证。','全部查于2026-10-26。版本文章来源日期未明者不推算；旧12为文字镜像证据，新VICD为原件与实际看过的静态页。')
s=s.replace('AiPaper英文/中文PDF链接可发现，未成功web读取原PDF。','旧记录为仅发现链接；本次由英文链接普通HTTP取得原件，见E-VICD-01。')
s += sources('M-007')
v.write_text(s)
# Update existing rows only where materially strengthened.
p=lib/'batches/ai-notebooks-coverage.csv';rows=list(csv.DictReader(p.open()));fields=list(rows[0])
changes={
'F-M009-01':dict(evidence_ids='E-IFLYO-02;E-IXCD-01',known_branches='历史手册区分讯飞阅读/office文档及TF路径；后续未标版本帮助概括内部共享存储；手机上传后须设备下载。',gaps='版本目录差异、重名覆盖、索引刷新、断线；Office帮助不展示文件夹。'),
'F-M009-02':dict(evidence_ids='E-IFLYO-02;E-IXCD-01',known_branches='云盘按格式下载归类；邮件附件另有本地目录，不能当云原件编辑。'),
'F-M009-04':dict(evidence_ids='E-IFLYO-01;E-IXCD-01',known_branches='历史上传读写客与后续帮助先加入笔记的前提分别保留；写作录音PC出口不同。'),
'F-M009-06':dict(evidence_ids='E-IFLYO-01;E-IXCD-01',known_branches='邮件播放链接不等于音频文件；导出说明不证明识别在设备本地完成。'),
'F-M009-07':dict(task='控制自动上传',evidence_ids='E-IFLYO-04;E-IXCD-01;E-IXCD-07',known_branches='历史开关在编辑退出时触发；手动上传、加密后云删除与自动上传分开；上传触发不等于服务器完成。',gaps='现版菜单、上传失败/排队、关闭前云副本、同步冲突。',visual_status='static_manual_pages_only')}
updates=[]
for row in rows:
 if row['flow_id'] in changes:
  before=row.copy();row.update(changes[row['flow_id']]);updates.append(dict(flow_id=row['flow_id'],before=before,after=row.copy()))
with p.open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(rows)
(work/'existing-coverage-updates-applied.json').write_text(json.dumps(updates,ensure_ascii=False,indent=2)+'\n')
print('integrated X2/Viwoods cases; 5 existing coverage rows refined')
