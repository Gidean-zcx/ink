import csv,json,hashlib,re
from pathlib import Path
root=Path('/home/workspace/reader'); lib=root/'docs/research/interaction-library'; work=root/'work/shortcuts-controls-deepening'
fetch={x['key']:x for x in json.loads((work/'fetch-results.json').read_text())}
# E ID, raw key, title, locator, Chinese short note, related F
specs=[
(1,'06-guide27','27指南目录','Table of Contents','19分组与95主题；有父子和非功能条目，不能当独立功能数量。',''),
(2,'07-create27','自然语言与手动创建','Create using Apple Intelligence / manually; Run / Stop','生成和命名后可描述修改或直接编辑；编辑撤销不证明执行副作用可撤销。','01'),
(3,'08-actions27','动作发现与输入输出','Search Actions; Apps; Show Info; Pin','可按应用筛选并查看动作输入输出；第三方仅暴露其提供的动作。','01'),
(4,'03-model27','模型动作与追问','Use Model; Follow Up; Output; Broad World Knowledge','模型输出可传后续动作；Follow Up允许先修订响应。设备、区域、网络、额度有条件。','07'),
(5,'09-graph27','内容图与对象转换','Content Graph; View Content Graph','按类型提取或转换对象供后续动作使用；不是任意数据自动可读。','08'),
(6,'10-variable27','变量类型、属性与来源','Type; Rename; Reveal Action; Go Back','改类型和属性改变传递值；回溯来源动作与重命名引用各有入口。','08'),
(7,'11-filter27','查询、过滤与数量限制','Add Filter; All/Any; Sort; Limit; Nothing','连续Find可能筛前一输出；Nothing可使后一步重新获取；数量和执行环境有条件。','09'),
(8,'12-auto-intro27','自动化概念与设备状态','Automation; iCloud backup / syncing','按条件触发；同步到另一设备默认停用，不等于执行会话迁移。','10;11'),
(9,'13-auto-add27','添加与移除自动化','Edit; Automation; Privacy; Allow Running When Locked','锁屏运行许可与各动作自动运行条件须分开，未证明绕过所有交互。','10'),
(10,'14-auto-events27','事件触发条件','Time; Alarm; Sleep; Keyboard; Screenshot; Notification','闹钟延后/停止及多种新事件分别设置；未验证离线错过、时区或去重。','10'),
(11,'15-auto-travel27','地点与车载触发','Arrive / Leave / CarPlay','地点边界和时间段可设置；外设连接与断开不同，实际延迟未知。','10'),
(12,'16-auto-comms27','通信触发筛选','Email; Message','同一触发器多条件须全部满足；未证明所有App消息可读。','10'),
(13,'17-auto-settings27','系统与外设触发','Battery; App; NFC; Wi-Fi; Bluetooth','App切走可算关闭但不等于进程终止；NFC按唯一ID而非标签全部内容。','10'),
(14,'18-auto-transaction27','钱包触发条件','Transaction trigger','轻触选定卡片时触发，不能据此断言支付已经结算成功。','10'),
(15,'04-sync26','26指令同步边界','iCloud Sync; personal automations','26指南明确个人自动化不跨设备同步；保留历史规则而非套用27。','11'),
(16,'05-sync27','27指令同步与自动化','iCloud Sync; other devices','新增、修改、删除和排序可同步；自动化在其他设备默认停用。','11'),
(17,'19-share27','分享文件、链接及停止分享','iCloud link; File; Anyone; People Who Know Me; Stop Sharing','链接和文件分支不同；停分享需再次确认，已导入副本去留未知。','12'),
(18,'20-import27','导入问题与试答','Setup; Add New Question; Customize Shortcut','共享时清空相应参数，接收者作答填入；试答会覆盖原配置。','12'),
(19,'21-delete27','删除指令','Edit; Delete; Delete Shortcut','有删除确认并可跨同账户同步；恢复与冲突处理未核。','11'),
(20,'22-alert27','暂停与取消对话框','Show Alert; Done; Cancel','Done继续，启用Cancel可停止流程；不是对先前副作用的回滚保证。','06'),
(21,'23-notification27','通知后继续执行','Show Notification','通知立即发出，后续动作继续；通知不能当用户已确认。','06'),
(22,'24-receive27','接收当前屏幕项目','Receive What’s On Screen','受支持App提供当前项目给第一步；不是任意页面全部数据。','02'),
(23,'25-app-shortcuts27','运行应用提供的指令','App Shortcuts; Use in New Shortcut','提供的动作可独立执行或嵌入新指令；不等于应用全能力已开放。','01'),
(24,'26-release27','27版本更新声明','Shortcuts updates; Published Date September 29 2026','声明更结构化屏幕输入和跨运行存储；具体存储键、覆盖、删除恢复流程仍缺。','02;07'),
(25,'27-release26','26版本更新声明','Shortcuts updates; Published Date September 28 2026','确认26有Use model；网页发布日期不是功能首次上线时间。','07')]
existing={json.loads(x)['evidence_id'] for p in (lib/'batches').glob('*-evidence.jsonl') for x in p.read_text().splitlines()}
assert not any(f'E-SCCD-{n:02}' in existing for n,*_ in specs)
notes=['# Shortcuts目录与动作证据摘记','','查阅：2026-10-27。全部为公开官方文字，不含产品操作或图像观察。27份HTTP请求均200；其中旧模型路径重定向到欢迎页，未据此引用模型流程。原HTML留临时work，以下短摘记与任务索引正式保存。','','新流程适用Shortcuts 10.0 / iOS 27指南；旧9.0 / iOS 26单列。未安装或验证设备构建、账号或地区。','','| E | 来源与定位 | 本次支持及边界 |','| --- | --- | --- |']
evidence=[]
for n,key,title,loc,note,flow in specs:
 f=fetch[key]; eid=f'E-SCCD-{n:02}'; raw=root/f['raw_work_path']; assert hashlib.sha256(raw.read_bytes()).hexdigest()==f['raw_sha256']
 old=n in (15,25)
 evidence.append(dict(evidence_id=eid,product_id='M-043',title=title,url=f['final_url'],checked_on='2026-10-27',source_type='official_release_notes' if n>=24 else 'official_help',access_mode='public_http_html_text',version_scope=('Shortcuts 9.0 / iOS 26' if old else 'Shortcuts 10.0 / iOS 27')+' guide/release documentation; installed device build untested',app_build=None,firmware_version=None,source_updated_on=('2026-09-29' if n==24 else '2026-09-28' if n==25 else None),locator=loc,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='durable_notes_temporary_source_html',source_notes_path='artifacts/shortcuts-controls-deepening/source-notes.md',raw_work_path=f['raw_work_path'],raw_sha256=f['raw_sha256'],supersedes=None))
 notes.append(f'| {eid} | [{title}]({f["final_url"]})；{loc} | {note} |')
(lib/'artifacts/shortcuts-controls-deepening/source-notes.md').write_text('\n'.join(notes)+'\n')
(lib/'batches/shortcuts-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in evidence))
fields=list(next(csv.DictReader((lib/'coverage.csv').open())))
def writecsv(p,rows,fields):
 with p.open('w',newline='') as out:
  w=csv.DictWriter(out,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(rows)
new=[
(7,'将模型作为动作链的一步','04;24;25','选择模型、提示和输入；Follow Up允许修订后再把响应传下去；生成与写入成功分别判断。','追问取消、输出类型、错误重试、额度与离线反馈'),
(8,'查看转换对象并回溯来源','05;06','变量Type和属性改变传递对象；Reveal Action高亮来源，Go Back返回；URL转Article可能获取外部内容。','转换失败、字段缺失、来源被删与更新'),
(9,'筛选并限制后续动作的内容','07','All/Any、排序、数量与变量；连续Find可能筛上一输出，Nothing可重取；大数据执行环境有限制。','空结果、取消、不匹配、实际性能'),
(10,'添加移除和查看自动化','08;09;10;11;12;13;14','触发条件、锁屏许可与各动作交互要求分别处理；App关闭包含切走，卡片触发不证明支付结算。','漏触发、去重、循环、并发、锁屏实际反馈及新触发无询问覆盖'),
(11,'同步与删除指令配置','08;15;16;19','26自动化不跨设备同步；27同步到其他设备默认停用；删除跨同账户设备传播。','恢复、并发冲突、迁移执行会话与业务数据'),
(12,'分享可配置指令并停止分享','17;18','导入问题清空相应共享参数；试答覆盖原配置；文件与链接及两类文件接收条件不同。','已经导入副本能否撤销、传播延迟、误覆盖恢复')]
flows=[]
for n,task,es,known,gaps in new:
 flows.append(dict(flow_id=f'F-M043-{n:02}',product_id='M-043',task=task,depth='D2',evidence_ids=';'.join('E-SCCD-'+e for e in es.split(';')),known_branches=known,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-043-shortcuts.md',checked_on='2026-10-27'))
writecsv(lib/'batches/shortcuts-controls-deepening-coverage.csv',flows,fields)
# Preserve actual source TOC URLs, including unversioned welcome.
toc=json.loads((work/'toc27-fixed.json').read_text()); assert len(toc)==114
items=[]
for t in toc:
 matched=[s for s in specs if s[0] not in (1,15,24,25) and t['id'].lower() in fetch[s[1]]['final_url'].lower()]
 fs=sorted({f'F-M043-{int(x):02}' for s in matched for x in s[5].split(';') if x})
 items.append(dict(entry_id='SCT-'+t['id'],parent_id='SCT-'+t['parent'] if t['parent'] else '',entry_type=t['kind'],title=t['title'],upstream_id=t['id'],url=t['url'],scope='Shortcuts 10.0 / iOS 27 TOC',link_scope='unversioned welcome as supplied' if t['id']=='APDA2B83D0E0' else 'versioned topic' if t['url'] else 'group without URL',evidence_id=';'.join(['E-SCCD-01']+[f'E-SCCD-{s[0]:02}' for s in matched]),related_flow_ids=';'.join(fs),review_status='body_read_documented_not_operated' if matched else 'directory_only',gaps='未执行，任务具体分支见案例' if matched else '本轮仅目录；未核本版本正文完整流程',checked_on='2026-10-27'))
writecsv(lib/'batches/shortcuts-controls-deepening-inventory.csv',items,list(items[0]))
# Proposals for established tasks; root will apply with all before checks once.
old=list(csv.DictReader((lib/'batches/ai-coverage.csv').open())); updates=[]
changes={1:('E-SCCD-02;E-SCCD-03;E-SCCD-23','27可自然语言生成后修改/运行；动作Info可看输入输出，App Shortcuts可嵌入；编辑撤销不代表执行副作用回滚。','生成错误、缺动作、额度耗尽、运行副作用与撤回'),2:('E-SCCD-22;E-SCCD-24','受支持App当前项目作为第一步输入；27更新声明更结构化，仍非任意页面全部数据。','各App实际字段、空页面、屏幕外数据与读取失败'),6:('E-SCCD-20;E-SCCD-21','原输出/返回语义保留；Show Alert的Done继续、Cancel可停止；Show Notification发出即继续，不能当已确认。','进程终止、停止后的对象去留、事务回滚、恢复入口')}
for n,(es,known,gaps) in changes.items():
 row=next(x for x in old if x['flow_id']==f'F-M043-{n:02}')
 for field,after in dict(evidence_ids=row['evidence_ids']+';'+es,known_branches=known,gaps=gaps,checked_on='2026-10-27').items():
  updates.append(dict(flow_id=row['flow_id'],field=field,before=row[field],after=after,reason='根代理已核27指南正文；保留旧26证据及D2'))
writecsv(work/'existing-coverage-updates.csv',updates,list(updates[0]))
print(json.dumps(dict(new_evidence=len(evidence),new_flows=len(flows),inventory=len(items),groups=sum(t['kind']=='group' for t in toc),old_flow_proposals=3),ensure_ascii=False))
