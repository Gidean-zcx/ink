from pathlib import Path
import json,csv,hashlib

W=Path('work/meebook-controls-deepening');L=Path('docs/research/interaction-library');A=L/'artifacts/meebook-controls-deepening';A.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
dump=lambda p,o:p.write_text(json.dumps(o,ensure_ascii=False,indent=2)+'\n')
old=json.loads((W/'coverage-before.json').read_text());es=json.loads((W/'evidence-before.json').read_text());captures=[json.loads(x) for x in (W/'captures.jsonl').read_text().splitlines()];cap={x['key']:x for x in captures}

notes='''# Meebook M7：本轮取证与边界

查阅 2026-10-27；所有来源只读，hands_on=false。本轮没有看新的图片或视频，也没有取得原 PDF。六项任务沿用原编号，新增 F/E 均为零；同 URL 的采集交根代理追加旧 E。

## E-MEE-01 官网

[厂商首页](https://www.haoqingtech.com/)正常 GET 200。HTML 仅提供语言切换、zh.png/en1.png及隐私链接，未见 M7 帮助或固件栏目链接；这只能说明本页未提供，不能说明厂商没有其他说明书。未重新查看宣传长图；原观察属于上一批。

## E-MEE-02 监管记录镜像

[提交记录](https://fccid.io/2A53EMEEBOOKM7/User-Manual/User-Manual-6662213)由 web 工具先读到元数据：MEEBOOKM7，12页，提交日2023-07-13、文档修改日2023-07-10，标题仍为P6说明书。页面报告 SHA-256 为7461db01fbc9f46a6745de12f99c81e69b0833b6001713344af6a21121509984；这是镜像报告的值，**没有本地PDF可比对**。后续匿名GET403即停止，不把拒绝响应当手册。

## E-MEE-03 快速指南转录

[ManualsLib](https://www.manualslib.com/guide/4006924/meebook-m7-manual.html)本轮 web 工具先读到4个正文栏目及7项基本操作，之后本地GET403并停止。栏目台账完整限定于该转录，不是完整系统手册。顶部任务管理与应用页Snow图标是两条管理入口；清任务和后台启动限制不合并为同一数据效果。字体作用于本地书，词典资源由用户另备。A2段尾的笔支持条件与下一项换笔尖相邻，缺原版布局，仍不确定条件归属；未认定M7支持手写。维修前备份是要求，不是备份方法。

## E-MEE-04 销售商历史日志

[eReader.co.th](https://ereader.co.th/firmware/)GET200，完整HTML与文本保存在work。仅提取M7标题下两版共22条；V1.5.2有两条配置入口，V1.5.3-2023092114有自动旋转、蓝牙自动关闭等声明。不同版本长按键声明的组合、优先级未知。京东读书记录同步不能扩展到原生书、笔记或全部App。其余修复/优化条目用于待测问题索引，不是已验证结果，也不证明当前仍有故障；其他型号的云盘、笔记导出、MDX、Regal不移植到M7。

## 失败和后续材料

旧批只记录PDF/图未取回，没有对应HTTP码，本轮未补写历史码。此次本地ManualsLib、fccid.io、device.report、fcc.report和manuals.plus均403，Good e-Reader web读取403；各路径收到拒绝后停止。搜索发现的其他型号手册、销售商规格和用户帖不作为M7操作依据。

下一证据应为可正常取得且明确M7/固件版本的整册说明或设备内帮助：打开书→批注/笔记→保存/导出，显示菜单及A2条件，文件删除/恢复、更新失败/恢复出厂。没有这些资料不等于功能不存在。目录中的研究缺口不是官方菜单项。
'''
(A/'source-notes.md').write_text(notes)

case='''# M-057 Meebook：M7 资源导入、按键配置与资料边界

查阅2026-10-26；2026-10-27补目录与来源归档。固定 **M7**，快速指南未标系统构建；销售商日志独立限定 **V1.5.2、V1.5.3-2023092114**，不称当前固件。没有实机、安装、账号操作、连续视频；本轮没有新增静态图观察。P6/M6/P78/P10 Pro/M8等型号不能补入M7范围。

官网首页未取得M7帮助目录。监管记录镜像将12页指南关联MEEBOOKM7，但PDF元数据标题仍是P6说明书；本轮仍未取得原PDF。因此继续以明确标成M7的快速指南转录为操作依据，并保留共享模板及排版风险。型号、固件和来源性质不可互相替代。详见[本轮取证摘记](../artifacts/meebook-controls-deepening/source-notes.md)。

## 功能目录与覆盖

逐项登记见[本轮台账](../batches/meebook-controls-deepening-inventory.csv)：快速指南4个正文栏目及其子项、销售商M7两版日志22条，另列研究缺口。父项/子项和更新声明不计独立完成任务；转录站的下载/语言导航不算设备功能。**尚未核定M7完整系统、阅读器或设置菜单**。

| 功能域 | 已读资料/关联任务 | 未闭合范围 |
| --- | --- | --- |
| 外观与维护 | 示意图、注意事项、保修栏目；E-MEE-03 | 本轮未看原图；按键短按/休眠/强制重启时序未核 |
| 文件与资源 | 基本操作1–3；F01–03 | 传输完成不等于书架索引或打开成功；重名/删除/撤销未知 |
| 阅读与笔记 | 字体应用；指定App同步声明；F03/05 | 章节、书签、选词、批注、笔记导出和阅读断点缺入口/结果 |
| 显示与按键 | A2说明；M7历史键位/旋转日志；F04 | A2条件归属、菜单档位、键优先级及当前构建未知 |
| 应用与后台 | 安装声明、任务管理/后台管理；F05/06 | 安装/卸载取消、冻结后重开和未保存内容未知 |
| 恢复与备份 | 维修前备份要求；F06 | 没有统一备份、更新回滚、重置/误删恢复操作 |

## 稳定任务与分支

以下仍是文档重建或声明，不是执行记录。本轮没有新增F。

| F 编号 | 前置/入口与操作 | 文档反馈、数据边界及结束 | 等级/证据/缺口 |
| --- | --- | --- | --- |
| F-M057-01 USB传书 | 电脑USB复制到根目录Books，再进文件管理 | 指南称可找到文件并分类；没有给出打开书的后续步骤 | D2；E-MEE-03。支持格式、索引、重名、中断及拔线时机未知 |
| F-M057-02 WiFi传书 | 电脑与设备在同一路由器；应用页WiFi传书，按地址或二维码指引 | 只确定局域网入口，不等于有跨网/云端传书 | D2；E-MEE-03。认证、完成/失败提示、取消、半文件及重复导入未知 |
| F-M057-03 词典与字体 | 自备词典进Dict并在词典App查看；字体进fonts，本地书Aa选择并确认 | 两类资源及本地阅读字体分别处理 | D2；E-MEE-03。词典格式/离线查词/原句回跳与字体失败回退未知 |
| F-M057-04 翻页键适配 | V1.5.2在用户设置→设备设置改长按；第三方App图标长按进入优化滚动设置 | V1.5.3另声明长按翻页键竖屏180°旋转；自动旋转是另一条声明 | D2仅配置入口；E-MEE-04。两版动作如何组合、默认值、焦点/按键冲突及实际App反馈未知 |
| F-M057-05 阅读记录与扩展 | 快速指南列应用商店/Google Play；V1.5.3列京东读书记录同步 | 安装提示流程、同步触发/完成未给；只归指定App | D1；E-MEE-03/04。原生进度、笔记导出、账户/网络条件及统一同步未知 |
| F-M057-06 后台管理与恢复 | 顶栏任务管理清理任务；应用模块左侧Snow进入后台管理、减少不必要的后台启动 | 两条入口不证明同一效果或编辑保留。送修前要求另做备份，未给工具 | D1；E-MEE-03/04。单项控制、确认/撤销、冻结后重开、恢复出厂和数据恢复未核 |

## 版本、显示与异常

M7日志两版共22条已逐条登记，都是历史声明。自动旋转、蓝牙自动关闭、文件夹名和灯光/唤醒等条目没有完整入口与反馈；“优化/修复”不作当前缺陷或修复有效的证明。尤其不能把同页其他型号的笔记/云盘/字典格式/刷新模式套给M7。

转录中A2与“支持笔版本”条件相邻，原版布局未核，本轮继续保留疑点，不断言M7有笔或确定的A2配置菜单。词典/字体资源导入也不能直接证明查词保存、复习或批注导出已经闭合。

USB/WiFi重复导入、取消、字典格式错误、字体失败、误删、应用更新后键位、后台音频/重启断点和同步冲突都仍待核。下一步优先取得明确M7构建的机内帮助或完整指南，再选同一本短书观察导入、书架索引、打开、批注保存/导出与退出恢复；本轮未执行这些动作。

## 对项目的启发（推断）

对G-001/G-002：系统键映射、App实际接收事件，以及文件传入/被索引/能打开是不同状态。后续设计可分别提供可诊断结果，儿童读听场景减少手工目录步骤；这仍是建议，不是Plus能力或本项目已定方案。没有从安装商店或维修备份措辞推定儿童可独立使用、全App统一同步或恢复保证。

## 证据与获取限制

- E-MEE-01：[深圳皓晴官网](https://www.haoqingtech.com/)。本轮HTML GET200；未重新看旧宣传图。首页不提供所需目录不等于没有其他帮助。
- E-MEE-02：[M7监管提交记录镜像](https://fccid.io/2A53EMEEBOOKM7/User-Manual/User-Manual-6662213)。web工具读到型号/12页/标题元数据，本地GET403停止；PDF原件未取回。提交日2023-07-13不等于系统版本。
- E-MEE-03：[M7快速指南转录](https://www.manualslib.com/guide/4006924/meebook-m7-manual.html)。web工具读取4栏目/7项操作，本地归档GET403停止；不是厂商现行在线帮助。目录覆盖不等于完整功能覆盖。
- E-MEE-04：[销售商固件日志](https://ereader.co.th/firmware/)。本轮GET200，M7两版原文已保存；未取得OEM原始发布页，也未升级设备。新型号及其他机型段不作为M7依据。

完整采集/拒绝响应和哈希留在work，正式摘记只保留必要定位；没有把403页面当正文。现有原件取得失败由本轮记录补充，未擅填旧批HTTP状态。D1/D2维持，D3/D4均未增加。
'''
(L/'cases/M-057-meebook.md').write_text(case)

urls={x['evidence_id']:x['url'] for x in es};inv=[]
def item(domain,name,e,loc,status='body_read',f='',gap='入口/反馈/数据结果需按任务行判断；目录不计完成任务'):
 inv.append(dict(inventory_id=f'I-MECD-{len(inv)+1:03}',domain=domain,item=name,source_url=urls.get(e,''),locator=loc,coverage_status=status,evidence_ids=e,flow_ids=f,gap=gap))
for name in ['Schematic Diagram','Warm Tips','Basic operating guidance','About maintenance guarantee']:
 item('快速指南正文栏目',name,'E-MEE-03',name,'heading_read_body_as_noted',gap='图像未取得；文字已读。栏目不等于完整系统菜单')
quick=[('Loading Books','1','F-M057-01;F-M057-02'),('Loading Dictionary','2','F-M057-03'),('Change the fonts','3','F-M057-03'),('Download and install apps','4','F-M057-05'),('A2 Refresh','5',''),('Renewal nib','6',''),('Power save technique','7','F-M057-06')]
for name,n,f in quick:item('快速指南基本操作',name,'E-MEE-03','Basic operating guidance '+n,'body_read',f,'A2/换笔尖条件归属仍待原版；安装与后台仅入口；无实操')
for name,loc,f in [('USB','1 Method 1','F-M057-01'),('WiFi','1 Method 2','F-M057-02'),('Task manage','7 Technique 1','F-M057-06'),('Application/Snow','7 Technique 2','F-M057-06')]:item('快速指南操作子项',name,'E-MEE-03',loc,'body_read',f)
for name in ['maintenance guarantee scope','maintenance guarantee clauses','Execute maintenance guarantee','Data backups']:item('快速指南维护子项',name,'E-MEE-03',name,'body_read','F-M057-06' if name=='Data backups' else '', '保修条款只登记，不作当前地区法律/服务承诺；备份无操作法')
groups=[('V1.5.3-2023092114','New functions',['Automatic rotation','Bluetooth auto off','JD reading records sync','Long press / portrait 180°']),('V1.5.3-2023092114','Optimizations',['Wallpaper','Bluetooth transfer','UI translations','Third-party image brightness','Folder name display']),('V1.5.2','New Functions',['Long-press mapping','Third-party key scrolling']),('V1.5.2','Optimizations',['WeChat image display','Automatic power off','KOReader key response','Punctuation','Book opening crash','microSD Books listing','Copied/preinstalled book opening','Clock wallpaper / frontlight','Warm/cold light after wake','Heat / power use','WiFi connection'])]
for version,group,names in groups:
 for n,name in enumerate(names,1):
  f='F-M057-04' if name in ['Long press / portrait 180°','Long-press mapping','Third-party key scrolling','KOReader key response','Automatic rotation'] else ('F-M057-05' if name=='JD reading records sync' else 'F-M057-06' if name in ['Automatic power off','Heat / power use','WiFi connection'] else '')
  item('M7 '+version,name,'E-MEE-04',f'Meebook M7 / {version} / {group} {n}','release_claim_only',f,'销售商历史声明；未核OEM发布/现行构建/完整操作/修复效果')
for name,f in [('阅读导航/章节/书签','F-M057-05'),('选词/批注/笔记导出','F-M057-05'),('显示刷新菜单与手势','F-M057-04'),('本地文件删除/恢复','F-M057-01'),('应用卸载与数据保留','F-M057-05'),('音频/后台/重启断点','F-M057-06'),('更新失败/回滚/恢复出厂','F-M057-06'),('备份范围/导入恢复','F-M057-06')]:item('研究缺口（非官方目录）',name,'','未取得M7对应帮助','not_found_in_available_sources',f,'需要明确M7及构建的原厂说明/机内帮助；未知不等于不支持')
with (L/'batches/meebook-controls-deepening-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');w.writeheader();w.writerows(inv)
(L/'batches/meebook-controls-deepening-evidence.jsonl').write_text('')
with (L/'batches/meebook-controls-deepening-coverage.csv').open('w',newline='') as f:
 csv.DictWriter(f,fieldnames=list(old[0]),lineterminator='\n').writeheader()

branches=[
 'Books根目录→文件管理；指南只说明可找回/分类，未覆盖书架索引和打开书的后续步骤',
 '同一路由器；应用页WiFi传书→按地址/二维码指引；未证明跨网传输',
 'Dict资源与fonts字体分开；词典App查看资源，本地书Aa选择字体确认',
 'V1.5.2用户设置/设备设置长按键配置及App图标长按优化滚动；V1.5.3另声明自动旋转和长按竖屏180°',
 '商店/Google Play仅安装声明；V1.5.3京东读书记录同步仅归该App，不扩展到其他App/原生书',
 '顶栏任务管理清任务；应用页Snow进入后台启动管理，两者效果分开；送修前备份要求没有备份工具/步骤'
]
gaps=[
 '支持格式、书架索引、同名处理、中断/半文件、拔线时机及打开结果；原PDF未取得',
 '身份验证、成功/失败反馈、取消、半文件、重试去重及内容打开；原PDF未取得',
 '词典格式/离线查词/原句回跳/生词复习；字体失败回退；未核当前构建',
 '两版长按动作组合/优先级、默认键值、实际App接收、冲突恢复；A2条件归属和菜单未知',
 '原生进度/章节/书签/批注导出、安装卸载数据、账号网络条件与跨App统一同步',
 '单项后台控制、确认/撤销、冻结重开与未保存内容、音频/重启断点、更新回滚/重置恢复；历史修复未复现'
]
updates=[]
for row,known,gap in zip(old,branches,gaps):
 after={**row,'known_branches':known,'gaps':gap,'checked_on':'2026-10-27'}
 updates.append({'flow_id':row['flow_id'],'before':row,'after':after,'changed_fields':{k:{'before':row[k],'after':v} for k,v in after.items() if row[k]!=v}})
dump(W/'old-coverage-updates.json',updates)

add=[]
for e,key in [('E-MEE-01','official'),('E-MEE-02','metadata'),('E-MEE-03','quick'),('E-MEE-04','firmware')]:
 c=cap[key];ok=c['http_status']==200
 capture={'checked_on':'2026-10-27','source_url':urls[e],'access_mode':'anonymous_public_html_GET' if ok else 'web_text_read_then_local_GET_403_stopped','http_status':c['http_status'],'hands_on':False,'visual_observed':False,'archived_artifact':'artifacts/meebook-controls-deepening/source-notes.md','sha256':sha(A/'source-notes.md'),'locator':next(x['locator'] for x in es if x['evidence_id']==e),'capture_state':'raw_source_in_work_and_short_note_archived' if ok else 'web_text_read_but_raw_capture_denied_no_retry'}
 if ok:
  capture.update({k:c[k] for k in ['raw_work_path','raw_sha256','extracted_work_path','extracted_sha256']})
 else:
  capture.update(failed_response_work_path=c['raw_work_path'],failed_response_sha256=c['raw_sha256'],raw_work_path=None,raw_sha256=None,note='此前web工具已读；本地文件只是403响应，不是正文。未重试拒绝路线，无PDF/新图。')
 add.append({'evidence_id':e,'before_additional_captures':next(x for x in es if x['evidence_id']==e).get('additional_captures',[]),'append_additional_captures':[capture]})
dump(W/'old-evidence-additional-captures.json',add)
dump(W/'inventory-summary.json',{'total':len(inv),'quick_guide_rows':19,'retailer_M7_release_rows':22,'research_gaps':8,'new_evidence':0,'new_flows':0,'old_flow_updates':len(updates),'old_flow_changed_fields':sum(len(x['changed_fields']) for x in updates),'old_evidence_captures':len(add),'new_visuals':0})
print((W/'inventory-summary.json').read_text())
