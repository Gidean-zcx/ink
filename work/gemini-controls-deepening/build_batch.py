from pathlib import Path
import csv,json,copy,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/gemini-controls-deepening';B=L/'batches'
assert not (W/'build-result.json').exists(),'one-time builder'
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
caps={x['name']:x for x in json.loads((W/'captures.json').read_text())};assert all(x['status']==200 for x in caps.values())
A=L/'artifacts/gemini-controls-deepening/source-notes.md'
specs=[('availability','Connected Apps availability table','Table: 85 categorized app entries; search row excluded','Mixed app/platform/account/age/location/language restrictions; directory snapshot only'),('custom','Custom app connection and removal','Connect / Disconnect / Unlink / Remove / write confirmation','Web setup; mobile/web use; US,18+,English,personal account,Keep Activity on'),('phone','Phone actions and routing','Connect / Make a phone call / permissions / unsupported','Android with phone plan and Google Phone permission; no Live; build unknown'),('messages','Message composition, routing and scope','Connect / Send / Add image / permissions / unsupported','Android SMS permission and plan; image additional default-app/RCS conditions; no Live'),('live','Live conversation and action lifecycle','Manage / camera-screen / Connected Apps Undo / background / settings','Android; age and account restrictions; gradual rollout; no fixed build'),('locked','Lock screen permission branches','General setting / Allow calls and text messages','Android mobile, Pixel Tablet exception; age and supervised-account limits')]
es=[];name_e={}
for n,(name,title,loc,scope) in enumerate(specs,1):
 c=caps[name];eid=f'E-GECD-{n:02}';name_e[name]=eid
 e=dict(evidence_id=eid,product_id='M-042',title=title,url=c['url'],checked_on='2026-10-27',source_type='official_help',access_mode='public_html_get_and_text_read',version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note='正文已核；目录条目仅提取，不代表子帮助或全部动作已分析。范围与缺口见独立来源摘记。',hands_on=False,visual_observed=False,archived_artifact=str(A.relative_to(L)),sha256=sha(A),capture_state='raw_html_text_and_short_note_archived',supersedes=None)
 for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']:e[k]=c[k]
 es.append(e)
(B/'gemini-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
old=json.loads((W/'coverage-before.json').read_text());changes={
 'F-M042-01':dict(known_branches='Ask about与其他建议的自动提交不同；当前屏幕/文件上传；文本、截图开关分开，智能屏幕推断限英语',gaps='屏幕外/历史/完整库覆盖、上传取消及上下文更新；动态共享另见Live任务'),
 'F-M042-02':dict(evidence_ids='E-GEM-02;E-GECD-01;E-GECD-02',known_branches='按账号/平台过滤目录；详情支持/不支持动作；85目录项仅发现；普通连接器Add/Disconnect与自定义应用生命周期分开',gaps='各连接器完整动作、部分失败、返回真实结果；跨App依赖及重试'),
 'F-M042-04':dict(evidence_ids='E-GEM-03;E-GECD-03;E-GECD-04;E-GECD-06',known_branches='Google电话/SMS/通讯录/通知权限、连接器开关、锁屏一般回答和免解锁通信分别控制；断开Messages不保证其他读取停止',gaps='撤权对正在执行任务与既有数据的影响、确认回执；Allow/Make按钮文案版本差异')}
props=[]
for x in old:
 if x['flow_id'] in changes:
  y=copy.deepcopy(x);y.update(changes[x['flow_id']],checked_on='2026-10-27');props.append(dict(flow_id=x['flow_id'],before=x,after=y))
dump(W/'existing-coverage-updates.json',props)
fields=list(old[0]);new=[]
for n,task,eids,branches,gaps in [
 (7,'依据上下文选择对象并拨号','E-GECD-03;E-GECD-06','请求或@Phone→连接提示→默认通信路由；可用前文对象；不支持Live或接听/拒接','拨出确认、号码歧义、双SIM、失败/挂断/重试与返回位置'),
 (8,'起草修改并发送消息','E-GECD-04;E-GECD-06','起草→修改→发送请求；同联系人上次应用/系统默认SMS；图片与通知回复条件分开','实际发送回执、确认取消、重复发送/离线重试、送达与撤回'),
 (9,'语音会话暂停、共享与接续','E-GECD-05;E-GECD-06','Live/左滑进入；Hold/Mute/End分开；后台通知返回；锁屏与共享停止、恢复各有条件','断网/进程终止/重启续接、音频冲突；原生控件与实际状态未观察'),
 (10,'Live中执行并撤销应用动作','E-GECD-05;E-GEM-02','未连接先到设置连接，再回Live重复请求；前台可Undo，后台与三星应用例外需目标App编辑','撤销时限、复合任务部分成功、重复请求幂等与目标对象回执'),
 (11,'添加和撤销自定义应用接入','E-GECD-02','Web添加MCP链接，@调用；停用/解除授权/移除分开；解除仍留目录，移除同时解除；写入人工确认声明','授权失败/凭据失效、确认卡与取消、部分写入、撤权后服务端副本和重连状态')]:
  new.append(dict(flow_id=f'F-M042-{n:02}',product_id='M-042',task=task,depth='D2',evidence_ids=eids,known_branches=branches,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-042-gemini.md',checked_on='2026-10-27'))
with (B/'gemini-controls-deepening-coverage.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(new)
ee={x['evidence_id']:x for x in json.loads((W/'evidence-before.json').read_text())};updates=[]
for name,eid in [('screen','E-GEM-01'),('connectors','E-GEM-02')]:
 c=caps[name];cap={k:c[k] for k in ['checked_on','url','raw_work_path','raw_sha256','text_work_path','text_sha256']};cap.update(capture_method='public_html_get_and_text_read',archived_artifact=str(A.relative_to(L)),sha256=sha(A),note='2026-10-27追加原件；未操作App或取得新视觉记录')
 updates.append(dict(evidence_id=eid,before=ee[eid].get('additional_captures'),after=(ee[eid].get('additional_captures') or [])+[cap]))
dump(W/'additional-captures-updates.json',updates)
rows=[]
def row(title,typ,scope,loc,url,eid,flows,status,gap):
 rows.append(dict(entry_id=f'I-GECD-{len(rows)+1:03}',scope=scope,entry_type=typ,title=title,locator=loc,url=url,evidence_id=eid,related_flow_ids=flows,review_status=status,gap=gap,checked_on='2026-10-27'))
table=json.loads((W/'connected-app-table.json').read_text())
for n,r in enumerate(table,1):
 row(r['cells'][0].split(':')[0],'connector_directory_entry',r['category'],f'HTML categorized row {n}; detailed conditions in captured table',caps['availability']['url'],'E-GECD-01','F-M042-02','directory_extracted_not_child_help_read','仅目录声明；单项详情/动作/错误/回执未逐项核查')
groups={
 'screen':('E-GEM-01','F-M042-01',['建议与上传对象','Ask about自动提交','文本/截图开关','英语智能上下文']),
 'connectors':('E-GEM-02','F-M042-02',['目录按身份/平台过滤','支持与不支持动作详情','@指定应用','Add与Disconnect','Keep Activity跨平台差异']),
 'custom':('E-GECD-02','F-M042-11',['Web添加MCP链接','无动态注册时凭据分支','停用开关','解除账号授权保留入口','移除入口同时解除','写入人工确认声明']),
 'phone':('E-GECD-03','F-M042-07',['@Phone连接提示','前文对象和联系人选择','同联系人上次通信应用','默认拨号应用','电话/联系人/日志权限','不支持接听拒接或Live']),
 'messages':('E-GECD-04','F-M042-08',['起草修改发送','默认消息路由','图片默认App/RCS条件','通知回复另属设备协助','不读消息库和群聊','断开连接仍可能读取其他对象']),
 'live':('E-GECD-05','F-M042-09;F-M042-10',['入口与Hold恢复','Mute与共享继续','End和文字记录','摄像头自动停止/恢复','屏幕共享停止/重开','先连接再重复请求','前台Undo','后台/三星编辑替代','后台通知返回','锁屏接续限制','声音选择与Confirm','语音打断与触摸打断','字幕持久开关/样式']),
 'locked':('E-GECD-06','F-M042-04',['免解锁一般回答','免解锁通信开关','个人内容仍需解锁','平板/年龄/监管账号限制'])}
for name,(eid,flows,titles) in groups.items():
 for title in titles:row(title,'documented_control_or_branch','official help; version unknown',name+' body headings',caps[name]['url'],eid,flows,'body_read_no_operation','完整运行结果、异常和持久状态未验证')
with (B/'gemini-controls-deepening-inventory.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
case=(W/'case-before.md').read_text().replace('查阅：2026-10-26。','初查2026-10-26，续查2026-10-27。',1)
case=case.replace('| Live、电话等其他入口 | 官方目录可发现 | 尚未做完整任务分析，不将它们算成已覆盖 |','| Live、电话、消息及自定义连接 | 续查补文档任务与状态分支 | 未操作；各连接器全动作、实际回执与恢复仍缺 |')
case+='''

## 2026-10-27：通信、语音接续与应用接入生命周期

新增[逐项目录](../batches/gemini-controls-deepening-inventory.csv)：官方表85个连接器入口与选定帮助中的控制分支分开；不作为85个完整任务或新增市场产品。完整表仍按账号、年龄、地区、语言、客户端和Chats/Tasks/Live区分。独立[取证说明](../artifacts/gemini-controls-deepening/source-notes.md)保存原件范围；没有视觉/实机记录。

| F编号 | 入口、操作与结果边界 | 返回、取消和恢复 | 证据 |
| --- | --- | --- | --- |
| F-M042-07 拨号 | Android套餐/电话权限等满足后提请求，必要时@Phone连接；可引用前文联系人。先按同联系人上次通信应用选择，Phone再使用系统默认拨号App。 | 未看到拨出确认、接通、挂断及重试；Phone连接器不接听/拒接、不可在Live用。 | E-GECD-03、E-GECD-06；D2文档 |
| F-M042-08 发消息 | 提出草稿→继续修改→要求发给联系人；@Messages可指定。静态图发送另受默认Google Messages及RCS条件限制。 | 不能据此读取完整消息库；通知读写属于Device assistance。送达、撤回、离线和重复发送未知。 | E-GECD-04、E-GECD-06；D2文档 |
| F-M042-09 语音接续 | Live按钮/左滑/指定唤醒进入；Hold暂停后点Live恢复，Mute保留会话及可能的共享，End回文本记录，旧会话可再进Live。 | 后台靠通知返回；锁屏设置决定继续或暂停。屏幕共享在暂停/锁屏后需重新开启；摄像头暂停后恢复与离开App后恢复规则不同。强退、断网、重启未知。 | E-GECD-05、E-GECD-06；D2文档 |
| F-M042-10 Live应用动作 | 未连接先到设置连接，回Live重复请求；支持能力依账户和灰度。 | 前台可点Undo或口头请求；后台、三星应用不支持同一路径，文档建议回目标App编辑；撤销时限和部分执行未知。 | E-GECD-05、E-GEM-02；D2文档 |
| F-M042-11 自定义应用 | 美国18+个人账号、英语及活动开启；Web设置添加MCP地址，必要时填凭据，再按提示连接。手机可使用已接入服务，@指定。 | 开关停用、More details解除关联、移除入口分开；解除授权仍留目录，移除同时解除。文档称写操作需人工确认，具体取消/失败/重连数据未知。 | E-GECD-02；D2文档 |

F-M042-01补Ask about与其他建议的提交区别；F-M042-02补目录与自定义应用边界；F-M042-04补锁屏与操作系统权限。关闭Messages连接并不意味着Google的通讯录权限、另一个连接器的通知读取也关闭。普通连接页、Live设置页、自定义连接页给出的菜单文案不同，按来源保留，不拼成实测一致界面。

对G-002的启发（尚非设计）：页面上下文、完整数据集合、可调用动作和动作实际结果应分别查证；“继续”也必须识别会话、麦克风、屏幕共享和目标App数据。资料中的年龄/账号条件使这些流程不能直接当作5–6岁儿童可用方案；Plus不是Android，连接协议也不证明可安装这些App。原屏幕自动操作的型号/地区冲突与全部未验证分支继续保留。
'''
(L/'cases/M-042-gemini.md').write_text(case)
dump(W/'build-result.json',dict(newE=len(es),newF=len(new),oldF=len(props),old_fields=sum(sum(x['before'][k]!=x['after'][k] for k in x['before']) for x in props),oldE_captures=len(updates),inventory_rows=len(rows),directory_rows=len(table),hands_on=False))
print((W/'build-result.json').read_text())
