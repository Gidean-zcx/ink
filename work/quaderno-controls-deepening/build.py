"""Only for this bounded batch; do not rerun after root merges/edits."""
from pathlib import Path
import json,csv,hashlib,collections,difflib
w=Path('work/quaderno-controls-deepening');lib=Path('docs/research/interaction-library');batch=lib/'batches';art=lib/'artifacts/quaderno-controls-deepening';date='2026-10-27';base='https://www.fmworld.net/digital-paper/';pdf=base+'support/download/pdf/B5FL2101.pdf'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,x):Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvwrite(p,rows,fields):
 with Path(p).open('w',newline='') as f:
  wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(rows)
read_pages=[1,2,3,4,5,6,7,8,18,20,21,22,23,25,26,31,34,35,36,37,46,47,48,51,52,55,56,57,58,59,60,61,62,63,64,65,66,67,70,71,72,73,74,78,79,80,81,82,83,84,85,86,87,95,96,97,98,99,100,101,102,106,107,110,111,113,114,115,118,119,120,121,123,124,133,134]
dump(w/'read-pages.json',{'extracted_pages':list(range(1,135)),'body_read_for_this_batch':read_pages,'visual_pages':[57,66,86],'note':'文本提取134页不等于逐页阅读；目录212项均核读。旧68/69观察保留原E-QUA-04，不算本次新看图。'})
notes='''# QUADERNO Gen.3C 本批来源与观察摘记

查阅/观察：2026-10-27。匿名官方文本及本地PDF静态读取；hands_on=false，无连续演示。

- E-QUA-01原件：B5FL2101.pdf，134页；封面B5FL-2101-02 Z0；末页134印B5FL-2101-02 Z0-02、发行2025-04。E-QUA-02官网目录的2026-04-01是公开日，两者不合并成同一个“出版日”。本次没有重新下载PDF。
- E-QUA-03重读：Gen.3C两型号要求Mobile App1.0.6+；仅日本区发布。当前下载页列Android14–16、iOS16/17/18/26且排除平板/iPad；不是本批安装版本。PC App启动须联网，Windows ARM不支持，macOS不支持蓝牙。旧Digital Paper PC App机型段不套入Gen.3C。
- E-QCD-01～08：官网Mobile帮助索引及7页。页面仍提Digital Paper PC App，未标构建/更新日；因此只作官方共用帮助的条件性流程。目录3类7篇；未观察其手机截图。NFC只帮助建立Wi-Fi链路，不是Bluetooth传输。
- QHG00001有一处把新直连建立与设备/手机Wi-Fi断开并写，语义冲突；不能据此解释手机蜂窝或互联网回退。下一句明确切换到其他App或停止设备热点会断开直连并恢复设备与接入点连接。恢复耗时、失败重试、当前OS后台行为未知。
- QHG00006中的iOS单次一文件是该未标版本帮助限制，不推广为当前iOS系统普遍限制；排队仅作传输预约，执行/落盘前不是设备持有文件。
- 手册p35：PDF内已有书签作为目录显示，无书签时入口隐藏；本批没有取得新建PDF书签步骤。p57/59检索是文字、特定图形、颜色/笔型，不能称任意手写OCR。
- p71：关联笔记替换会把旧关联笔记变独立笔记，不说删除原稿。p72删页不可撤回，与p46回收站文件恢复不同。
- p121修复仅针对曾升级2.1.00.26250FP的文档问题，产生recovery副本并保留原件；不是一般备份还原。

## 实际查看的3张静态页（E-QCD-09）

| 页码（PDF=印刷） | 可直接观察 | 限制 |
| --- | --- | --- |
| 57 | 文本中的五角星及六辐星形标记；搜索结果按页合并、退出说明 | 是手册规则，不是识别成功率或实际搜索界面录像 |
| 66 | 选择/粘贴步骤及单次粘贴、重启清空；批注列表定位说明 | 未触发操作，未测等待时间 |
| 86 | 扫描完成静态对话框有返回扫描/文档列表两个按钮，注明Received；下方设备切换列表 | 仅官方示意，未连接扫描仪 |

正式图为gen3c-p57.png、gen3c-p66.png、gen3c-p86.png；原PDF和原始HTML在work，哈希见批次E和work交接。完整原文只留work；本摘记是边界摘要，不是全文翻译。
'''
(art/'source-notes.md').write_text(notes)
caps=json.loads((w/'captures.json').read_text()); capmap={x['name']:x for x in caps}
for x in caps:
 p=w/'extracted'/f"{x['name']}.txt"
 if p.exists():x.update(extracted_work_path=str(p),extracted_sha256=sha(p))
dump(w/'captures.json',caps)
capmap={x['name']:x for x in caps}
titles=['Mobile帮助完整目录（3类7篇）','手机连接方式与切后台断开条件','NFC辅助直连与首次认证','手动Wi-Fi直连与首次认证','同一接入点连接与首次认证','手机单文件传入','暂存预约与批量传入','设备文档传出手机']
names=['mobile-index']+[f'QHG{i:05d}' for i in range(1,8)]
E=[]
for i,(name,title) in enumerate(zip(names,titles),1):
 c=capmap[name];E.append(dict(evidence_id=f'E-QCD-{i:02d}',product_id='M-060',title=title,url=c['url'],checked_on=date,source_type='official_help',access_mode='anonymous_public_html_GET',version_scope='QUADERNO Mobile App共用日文帮助，构建/更新日未标；Gen.3C FMVDP43CA4/FMVDP53CA5须App1.0.6+（E-QUA-03）；不证明当前OS实机表现',app_build=None,firmware_version=None,source_updated_on=None,locator='全文正文；索引之外的7篇均已读',note='设备型号适用条件与共用帮助分开；页面图片未查看。',hands_on=False,visual_observed=False,raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],extracted_work_path=c['extracted_work_path'],extracted_sha256=c['extracted_sha256'],archived_artifact='artifacts/quaderno-controls-deepening/source-notes.md',sha256=sha(art/'source-notes.md'),capture_state='raw_html_in_work_and_short_note_archived',supersedes=None))
renders=json.loads((w/'rendered.json').read_text());paths=[str(Path(x['path']).relative_to(lib)) for x in renders]
E.append(dict(evidence_id='E-QCD-09',product_id='M-060',title='Gen.3C标记/剪贴/扫描完成三页静态观察',url=pdf,checked_on=date,source_type='official_manual',access_mode='local_pdf_page_render_viewed',version_scope='FMVDP43CA4/FMVDP53CA5；B5FL-2101-02 Z0，末页Z0-02发行2025-04，官网公开日2026-04-01；安装固件未知',app_build=None,firmware_version=None,source_updated_on=None,locator='PDF与印刷页57、66、86；各页单帧已view_image',note='派生观察，非新增独立来源、非连续操作；三页限定映射F11/F12/F14。',hands_on=False,visual_observed=True,raw_work_path='work/research/extension-manuals/quaderno-gen3c.pdf',raw_sha256=sha('work/research/extension-manuals/quaderno-gen3c.pdf'),archived_artifact=paths[0],sha256=renders[0]['sha256'],rendered_artifacts=paths,artifact_hashes={p:r['sha256'] for p,r in zip(paths,renders)},capture_state='selected_static_page_excerpt',derived_from='E-QUA-01',supersedes=None))
(batch/'quaderno-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
old=json.loads((w/'coverage-before.json').read_text());fields=list(old[0]);changes={
4:('设备删除后回收站还原；1GB以上阈值提供直接永久删除或取消。回收站内不能打开文件/查看子文件夹，需先还原；PC App删除及特定同步删除不入回收站。原目录重建/同名改名沿用。','未测阈值/同名恢复；永久删除、笔记删页和特定旧固件修复不受此链保证。'),
5:('Windows MTP的Import暂存由设备目标目录导入后消失；不得删除或重命名Import/Export/Template。macOS走PC App，不能移植MTP步骤；PC App再传回电脑的重名编号规则不套到本入口。','本入口同名碰撞、复制未完就导入、拔线损坏及重试仍未说明；不借PC App规则填补。'),
6:('USB备份ZIP、同日编号、排除回收站；备份与同步目录分开。手册要求保存盘及App安装盘各有至少相当于数据总量的空间（目安）；中途拔线会失败。备份改名后不在列表显示（手册口径）。','空间阈值/失败回执未实测；不保证失败残留ZIP可用。'),
7:('导入备份覆盖备份后新增数据，创建/修改时间改为导入时间、浏览时间重置；Samples及其中内容不导入。确认后才执行；中途拔线会失败，官方建议先导出新增资料。','中断后的设备部分状态、自动回滚/续传、改名备份如何重新索引均未知。'),
8:('双方改同PDF时设备版保留原名、电脑版另名；一边删另一边改则保留修改版。只同步目录下PDF和含PDF目录；解除同步不删文件。特定PC/同步删除绕过回收站；同步中不能用笔。','冲突命名细节/回执/重复重试未知；电脑同步目录改名或移动会解除绑定，重设前不能视作继续同步。')}
updates=[]
for n,(known,gaps) in changes.items():
 b=old[n-1];a=dict(b,known_branches=known,gaps=gaps,checked_on=date)
 updates.append({'flow_id':b['flow_id'],'before':b,'after':a,'changed_fields':{k:{'before':b[k],'after':a[k]} for k in b if b[k]!=a[k]}})
dump(w/'old-coverage-updates.json',updates)
newdata=[
(9,'手机配对后传入PDF','E-QUA-01;E-QUA-03;E-QCD-02;E-QCD-03;E-QCD-04;E-QCD-05;E-QCD-06;E-QCD-07','NFC辅助/手动热点/同AP三路，首次输入设备认证码；即时传入与稍后预约分开。热点窗口占用设备、切其他App会断直连；预约列表可改目录/移除条目。','共用帮助未标版本；错误认证/权限拒绝/同名/空间满/传中切后台的结果未知；不推断完成回执或当前iOS系统限制。','not_observed'),
(10,'传出手机并检查批注可见性','E-QUA-03;E-QCD-08','App取出入口→连接→同文件夹选文档→取出；Android打开/分享，iOS接收App或存文件。接收器可能不显示批注；官方建议换支持批注的阅读器。','接收App保存确认、取消/失败、是否保持可编辑元数据未核；不能把打开临时副本算持久备份。','not_observed'),
(11,'按标记和颜色定位原文','E-QUA-01;E-QCD-09','文内正文/图形/颜色笔型搜索按页跳转，外点关闭；全库图形搜索按文档去重且排除回收站。已有PDF书签为目录、无书签隐藏；历史跳转不记普通划页。','任意手写OCR不在本链；图形识别可失败，跨文档色搜索/新建书签未取得步骤；未测搜索速度。','static_manual_pages_only'),
(12,'移动笔迹并在当前页撤回','E-QUA-01;E-QCD-09','书写即保存；范围选择圈选复制/剪切，目标页粘贴调位确认；选择/粘贴模式不能翻页。一次粘贴后入口隐藏，重启/再复制清空旧剪贴；换页清空撤销/重做。','切取后崩溃、剪贴跨页期间撤销关系未测；文本高亮不适用于图片/手写；受保护或空间不足可能只读。','static_manual_pages_only'),
(13,'关联资料笔记并维护页面','E-QUA-01','文档选项新建side note→模板→左右并列，同目录加_Note；每文档一个，替换把旧稿变独立笔记。可单独显示；笔记可插页/末页划动续页，删页不能还原。','重名、文件改名移动后关联、导出再传回是否保留关系未知；笔记不能再建side note，见开模式限制创建/增删页。','not_observed'),
(14,'扫描多页后核对文件落点','E-QUA-01;E-QCD-09','同AP或连接扫描仪热点→设备ScanSnap入口选机→设备扫描→续扫/完成→文档列表，Received保存。iX100只有单面且无继续按钮；空闲断开需重连，列表最多10机。','兼容扫描仪范围须另核；卡纸/取消/传输中断未说明，不保证有部分文件；未进行扫描。','static_manual_pages_only'),
(15,'升级后处理数据转换只读','E-QUA-01','升级后转换不足空间时只读，按消息所列空间清理回收站或先导出资料；收到开关机提示才重启重试。等待转换时不操作设备/App/MTP。','升级本身后续仅写按屏幕提示，回滚/断电恢复未知；不能保证整理空间即完成或保证无数据损失。','not_observed'),
(16,'特定旧固件文档问题生成修复副本','E-QUA-01','仅曾升2.1.00.26250FP且出现恢复入口的文档；文件夹菜单恢复→检测确认OK→报告文件名。可产一/两份recovery副本，原件保留；同名或空间不足会失败。','不是通用修复；未实测内容完整度或两副本哪份正确；进行中不得休眠/关机，异常中断行为未知。','not_observed')]
F=[]
for n,title,es,known,gap,visual in newdata:F.append(dict(zip(fields,[f'F-M060-{n:02d}','M-060',title,'D2',es,known,gap,visual,'false','cases/M-060-quaderno.md',date])))
csvwrite(batch/'quaderno-controls-deepening-coverage.csv',F,fields)
# Catalogue: exact embedded outline, separately inventoried menu headings and mobile catalogue.
outline=json.loads((w/'outline.json').read_text());inv=[]
mapids={24:12,33:5,34:9,35:9,36:9,37:6,49:11,54:1,55:1,67:4,70:2,79:11,80:11,81:11,84:12,85:12,86:12,87:12,88:11,89:12,90:3,91:3,92:3,93:13,94:13,95:13,96:13,97:13,98:13,99:13,100:5,101:5,110:14,111:14,112:14,128:6,129:6,130:7,132:8,133:8,134:8,135:8,136:8,169:15,181:12,182:12,183:12,184:16,188:4,190:6,191:7,192:14,193:14,194:14,195:14,196:14}
for i,(level,title,page) in enumerate(outline,1):
 f=mapids.get(i);status='task_partially_analysed' if f else ('body_read_not_closed' if page in read_pages and level>1 else 'catalogued_only')
 inv.append(dict(inventory_id=f'I-QCD-{len(inv)+1:03d}',domain=f'PDF目录L{level}',item=title,source_url=pdf,locator=f'PDF/印刷p{page}; outline#{i}',coverage_status=status,evidence_ids='E-QUA-01',flow_ids=f'F-M060-{f:02d}' if f else '',gap='仅关联对应分支，不表示整节闭合或实操' if f else '目录已核；未形成完整任务链，正文读取范围见read-pages.json'))
settings={78:['Wi-Fi','Wi-Fi設定','Wi-Fi設定/スリープ中にWi-Fi接続を維持','Wi-Fi設定/手動でネットワークを追加','Wi-Fi設定/ネットワーク一覧を更新','Wi-Fi設定/ネットワーク一覧','Bluetooth','Bluetooth設定','Bluetooth設定/本機にBluetooth機器を追加','Bluetooth設定/Bluetoothペアリングを待ち受け','Bluetooth設定/Bluetooth Low Energy(BLE)設定','Bluetooth設定/スリープ中にBluetooth接続を維持','Bluetooth設定/すべてのBluetooth機器とのペアリング情報を削除','NFC','Wi-Fiアクセスポイントを起動'],79:['Wi-Fiアクセスポイント情報','本体設定','本体設定/ペン先の位置調整','本体設定/ペンの機能設定','本体設定/書き込みの編集制限','本体設定/ノートのテンプレート','本体設定/画面ロック','画面ロック/画面ロック','画面ロック/ロックパスワードの変更','画面ロック/ロック解除に使用するNFCカードの設定'],80:['オートスリープ','オートスリープ/有効・無効','オートスリープ/オートスリープまでの時間','スリープ画面設定','スリープ画面設定/標準','スリープ画面設定/選択したページ','スリープ画面設定/スリープ前の画面','日付と時刻','日付と時刻/日付と時刻を設定','日付と時刻/日付表示形式','日付と時刻/時刻表示形式','日付と時刻/タイムゾーン','連携サーバー設定','すべてのドキュメントを削除','本機を初期化'],81:['この機器について','この機器について/機器情報','この機器について/使用許諾契約書','認証情報']}
for page,items in settings.items():
 for title in items:
  f=9 if 'アクセスポイント' in title else (12 if title.endswith('ペンの機能設定') or '編集制限' in title else (13 if 'テンプレート' in title else None))
  inv.append(dict(inventory_id=f'I-QCD-{len(inv)+1:03d}',domain='本体设置条目',item=title,source_url=pdf,locator=f'PDF/印刷p{page}',coverage_status='task_partially_analysed' if f else 'body_read_not_closed',evidence_ids='E-QUA-01',flow_ids=f'F-M060-{f:02d}' if f else '',gap='设置说明已读；未观察实际菜单/确认/错误'))
for title in ['ボールペン(color1)','ボールペン(color2)','ボールペン(color3)','消しゴム','マーカー','ハイライト','範囲選択','拡大','無効']:
 inv.append(dict(inventory_id=f'I-QCD-{len(inv)+1:03d}',domain='笔侧键/尾键可分配选项',item=title,source_url=pdf,locator='PDF/印刷p79',coverage_status='task_partially_analysed',evidence_ids='E-QUA-01',flow_ids='F-M060-12',gap='列表与默认值有文档；逐个键映射未实操'))
for title in ['クアデルノへファイルを入れる','クアデルノからファイルを取り出す','クアデルノとスマートフォンの接続方法を確認する']:
 inv.append(dict(inventory_id=f'I-QCD-{len(inv)+1:03d}',domain='Mobile目录分类',item=title,source_url=base+'mobileapp/helpguide/',locator='3个目录类',coverage_status='catalogued_only',evidence_ids='E-QCD-01',flow_ids='',gap='分类不算任务'))
for i in range(1,8):
 inv.append(dict(inventory_id=f'I-QCD-{len(inv)+1:03d}',domain='Mobile帮助文章',item=titles[i],source_url=capmap[f'QHG{i:05d}']['url'],locator=f'QHG{i:05d}',coverage_status='task_partially_analysed',evidence_ids=f'E-QCD-{i+1:02d}',flow_ids='F-M060-10' if i==7 else 'F-M060-09',gap='官方共用帮助无构建/日期；版本与真实设备表现未核'))
csvwrite(batch/'quaderno-controls-deepening-inventory.csv',inv,list(inv[0]))
# Existing evidence: append captures only; separate top-level date correction proposal.
olds=json.loads((w/'evidence-before.json').read_text());oe={x['evidence_id']:x for x in olds};ec=[]
for eid,name in [('E-QUA-01',None),('E-QUA-02','support'),('E-QUA-03','download')]:
 c=dict(checked_on=date,access_mode='existing_pdf_reread_PyMuPDF' if name is None else 'anonymous_public_html_GET',hands_on=False,visual_observed=False,archived_artifact='artifacts/quaderno-controls-deepening/source-notes.md',sha256=sha(art/'source-notes.md'))
 if name is None:c.update(raw_work_path='work/research/extension-manuals/quaderno-gen3c.pdf',raw_sha256=sha('work/research/extension-manuals/quaderno-gen3c.pdf'),locator='全212项outline；正文页列表work/quaderno-controls-deepening/read-pages.json；p134发行2025-04与官网公开日不同')
 else:c.update({k:v for k,v in capmap[name].items() if k in ['url','status','raw_work_path','raw_sha256','extracted_work_path','extracted_sha256']})
 before=oe[eid].get('additional_captures',[]);ec.append({'evidence_id':eid,'field':'additional_captures','before':before,'after':before+[c],'append':c})
dump(w/'old-evidence-additional-captures.json',ec)
cor=[]
for eid in ['E-QUA-01','E-QUA-02','E-QUA-03','E-QUA-04','E-QUA-05']:
 b=oe[eid]['version_scope'];a='QUADERNO Gen.3C FMVDP43CA4/FMVDP53CA5; B5FL-2101-02 Z0 cover, Z0-02 colophon issued 2025-04; support index public date 2026-04-01; installed firmware unknown'
 c={'version_scope':{'before':b,'after':a}}
 if eid in ['E-QUA-01','E-QUA-04']:c['source_updated_on']={'before':oe[eid]['source_updated_on'],'after':None}
 cor.append({'evidence_id':eid,'changed_fields':c,'reason':'旧published 2026-04-01混淆官网公开日与PDF内发行月；精确日期分别保留capture定位，不猜PDF更新日。'})
dump(w/'old-evidence-date-corrections.json',cor)
dump(w/'counts.json',{'new_evidence':len(E),'new_flows':len(F),'old_flow_updates':len(updates),'old_flow_changed_fields':sum(len(x['changed_fields']) for x in updates),'old_evidence_captures':len(ec),'inventory_rows':len(inv),'inventory_domains':dict(collections.Counter(x['domain'] for x in inv)),'read_body_pages':len(read_pages)})
print((w/'counts.json').read_text())
