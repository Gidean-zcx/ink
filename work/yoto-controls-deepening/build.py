# FINALIZED: do not rerun after root merge; it would overwrite reviewed case/batch.
from pathlib import Path
import json,csv,hashlib,re,sys
from urllib.parse import unquote,urlsplit,urlunsplit
sys.path.insert(0,'work/yoto-controls-deepening');from acquire import Text
W=Path('work/yoto-controls-deepening');L=Path('docs/research/interaction-library');B=L/'batches';A=L/'artifacts/yoto-controls-deepening';A.mkdir(parents=True,exist_ok=True);D='2026-10-27'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def norm(u):
 s=urlsplit(u);return urlunsplit((s.scheme,s.netloc.lower(),unquote(s.path).rstrip('/'),'',''))
oldE=json.loads((W/'old-evidence-before.json').read_text());oldF=json.loads((W/'old-coverage-before.json').read_text());allE=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()]
masterurls={norm(x['url']):x['evidence_id'] for x in allE};assert not any(x['evidence_id'].startswith('E-YTCD-') for x in allE)
manifest={x['name']:x for x in json.loads((W/'source-manifest.json').read_text())};cats={p.stem:json.loads(p.read_text()) for p in (W/'extracted').glob('category-*.json')}
articles={}
for k,v in cats.items():
 for a in v['articles']:articles[a['hash']]=(a,k)
cat_order=['cards-content-make-your-own','using-your-yoto','new-to-yoto','fixing-a-problem-safety','yoto-club']
new_defs=[('06','BJIxYtimMl','闹钟：条件、保存/取消、启停与删除','步骤1–18；App>=2.46、OS>=2.10.7；未确认实际构建'),('07','r1BYtjXfg','Player夜灯和Mini日夜模式边界','通用翻面/亮度/颜色；3rd Gen传感器；排除4th Gen段'),('08','HJq3OKomzx','家庭成员可控制全部登记设备','Family Account lets anyone who is a member'),('09','H162uFjmfl','离开家庭或移除其他成员','每位成员及Settings用户三点菜单'),('10','S1vytKsmMg','接收共享MYO及原作者所有权','Receiving / Giving or receiving / Reactivating；未核发起分享菜单'),('11','ByE1FKsXzl','恢复出厂与家庭解绑分开','Player / Mini重置步骤及Remove device / Leave family'),('12','SJNFKiQGe','本机容量、缓存淘汰与恢复下载','型号表；Mini序列规则；When storage is full；不采用小时估算作保证'),('13','r1ssdFimfl','自建MYO列表的容量限制','100 tracks、单条/整卡限制；不套用已购卡关联'),('14','rJ_A_Ki7Mx','网络状态图标与下载前提','Online/offline；green/white Yoto Face；图仅读alt/正文，未看图')]
new=[]
for i,k in enumerate(cat_order,1):
 name='category-'+k;r=manifest[name];assert norm(r['url']) not in masterurls
 new.append(dict(evidence_id=f'E-YTCD-{i:02}',product_id='M-037',title=cats[name]['category']['title']+'：公开帮助目录',url=r['url'],checked_on=D,source_type='official_help_index',access_mode='anonymous_public_category_html_embedded_data',version_scope='英文GB帮助分类；含历史/第四代条目；目录逐项限定范围，不表示全部适用Player3/Mini2024',app_build=None,firmware_version=None,source_updated_on=None,locator=f"initial-data.data.articles；{len(cats[name]['articles'])}项；仅按标题清点，不声称所有正文已分析",note='公开网页内嵌数据，无API请求；分类body虽随HTML取得，仅选定文章实际读正文。',hands_on=False,visual_observed=False,raw_work_path=r['raw_work_path'],raw_sha256=r['raw_sha256'],extracted_work_path=r['extracted_work_path'],extracted_sha256=r['extracted_sha256']))
def article_capture(h):
 a,k=articles[h];r=manifest[k];p=W/'extracted'/('article-'+h+'.json');p.write_text(json.dumps(a,ensure_ascii=False,indent=2)+'\n');t=Text();t.feed(a['body']);q=W/'extracted'/('article-'+h+'.txt');q.write_text(''.join(t.parts)+'\n')
 return a,dict(checked_on=D,access_mode='anonymous_public_category_html_embedded_article',capture_url=r['url'],canonical_article_url='https://support.yotoplay.com'+a['href'],raw_work_path=r['raw_work_path'],raw_sha256=r['raw_sha256'],extracted_work_path=str(p),extracted_sha256=sha(p),extracted_text_work_path=str(q),extracted_text_sha256=sha(q),locator=f"initial-data.data.articles[hash={h}].body",source_updated_on=a['updatedAt'],hands_on=False,visual_observed=False)
for n,h,title,loc in new_defs:
 a,c=article_capture(h);u='https://support.yotoplay.com'+a['href'];assert norm(u) not in masterurls,(u,masterurls.get(norm(u)))
 new.append(dict(evidence_id='E-YTCD-'+n,product_id='M-037',title=title,url=u,source_type='official_help',version_scope='Player第3代/Mini2024研究范围；英文GB共用帮助，无实际App/OS构建；不同代际段落不互套',app_build=None,firmware_version=None,note=loc,**c))
recap=[]
for eid in ['E-YOD-01','E-YOD-02','E-YOD-19','E-YOD-20','E-YOT-01','E-YOT-02','E-YOT-03']:
 before=next(x for x in oldE if x['evidence_id']==eid);h=unquote(before['url']).rsplit('-',1)[-1];a,c=article_capture(h);after=json.loads(json.dumps(before));after.setdefault('additional_captures',[]).append(c);recap.append({'evidence_id':eid,'before':before,'after':after,'changed_fields':{'additional_captures':{'before':before.get('additional_captures'),'after':after['additional_captures']}}})
notes='''# Yoto 本轮来源与边界

查阅2026-10-27。Player第三代与Mini2024分别研究；无实际OS/App构建。6个普通网页GET均HTTP200：App总览和5个分类；网页initial-data内含文章正文，不调用API。旧7E新增采集建议在work，正式批仅新增不同URL的14E。分类索引不是全部任务完成。

| E | 短摘记/定位 |
| --- | --- |
| E-YTCD-01–05 | 五分类清点87条文章；另记录7个一级分类。未抓订单/第四代分类，不把该站宣称为完整固定版本说明书。 |
| E-YTCD-06 | 闹钟有明确版本下限、保存/取消、单项启停/删除；设置需开机联网，响铃需开机，正文未保证离线/关机唤醒。 |
| E-YTCD-07 | 翻面与旋钮调夜灯；第三代温度传感器暂停；Mini无夜灯。第四代自动夜灯亮度不套第三代。 |
| E-YTCD-08/09 | 家庭成员可控所有登记设备；每位成员可退出或移除其他用户。没有只读亲属/儿童角色依据。 |
| E-YTCD-10 | 收到链接先预听再接受到库、可关联卡；原作者仍拥有列表，更新会反映到关联卡。邮件地址随分享可见；链接可转发。撤回/离线延迟未知。 |
| E-YTCD-11 | 恢复出厂删本机下载、需重新设置，却不解除家庭绑定；设备解绑与成员退出各有独立入口。没有执行。 |
| E-YTCD-12 | Player3为32GB；当前文档将Mini2024与2021–23合列16/32GB并给序列规则，未单独确认2024每种装配。容量页按最长未播放淘汰，旧离线页按least-played，算法冲突保留。 |
| E-YTCD-13 | 自建MYO限100轨；单轨100MB/60分钟、总500MB/5小时；原文并列的字节/时长上限均保留。已购卡关联不套此限。 |
| E-YTCD-14 | 红时钟在线、白时钟离线、绿脸仅连WiFi无互联网；是正文描述，未观察屏幕。 |

本轮旧E复读：E-YOT-01/02/03下载、计时与状态；实体控制明确长按加速而非精确定位；睡眠计时可回同图标设off。OS/App日志仍只是公布列项，不证明实际安装版本。关于页仍有Account/Menu旧命名。

两张闹钟附图正常匿名GET返回403，停止该路径；未保存图像。随后一次view_image因无本地文件失败，不构成观察。全部visual_observed=false、hands_on=false；无连续记录、登录、购买、配对、发送、音频或固件下载。

原件、文章hash定位、日期与SHA256见新证据/旧E additional_captures建议及work/source-manifest.json。正式档案仅此短摘记；原始HTML保存在work。
'''
(A/'source-notes.md').write_text(notes)
for e in new:e.update(archived_artifact='artifacts/yoto-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),capture_state='raw_html_in_work_and_short_note_archived',supersedes=None)
for x in recap:
 c=x['after']['additional_captures'][-1];c.update(archived_artifact='artifacts/yoto-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'))
(B/'yoto-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in new))
(W/'old-evidence-additional-captures.json').write_text(json.dumps(recap,ensure_ascii=False,indent=2)+'\n')
newflows=[
('16','设置、取消及停响闹钟','App至少2.46、OS至少2.10.7；设置时目标设备开机联网，非已验证构建','Settings → 目标设备 → Alarms → Set/Add an alarm；选时间、重复日、声音和音量 → Save alarm，Cancel不保存。默认全周，去掉不用的日；手机/设备预听是不同目标。','保存后列在该机闹钟列表，每机最多10项；点已有项编辑后保存或取消，右侧开关启停，详情底部Delete删除。到时需开机；任意设备键或app停止响铃。音量受日夜上限约束。','E-YTCD-06','离线响铃、自动关机后唤醒、正在播故事时打断/恢复、时区变更和多机同步未说明；预听耗时未实测。'),
('17','设日夜作息与第三代夜灯提示','Player3或Mini2024；家庭设备已登记；夜灯仅Player3','Settings → 目标设备，分别设Morning/Bedtime；钟面日/月标记按时段变化，可分设限音/亮度/右键快捷。Player3另设两段夜灯颜色；翻面开、竖起关，伏面转右旋钮调亮至0。','日夜作息、夜灯、F02停播和F16闹钟是不同状态；Mini2024仅用日夜设置，没有夜灯。Player3夜灯模式暂不读室温；不套第四代自动夜灯亮度。','E-YOD-02;E-YTCD-07','更改作息时正播内容、时区/离线转换、设置下发失败及跨设备冲突未知；第三代屏幕自动亮度不等于夜灯自动亮度。'),
('18','接受亲友MYO列表并保留作者关系','收到共享MYO链接；使用app；已有可用MYO；家庭加入不是此页接收步骤','从收到的链接在app预听，检查创作者身份；接受后加入自己的Library，再按F07关联实体卡。正文未给发起分享的当前完整按钮路径，此行从收件开始。','收件人接受的是共享列表入口，所有权仍属原作者；后续作者更新会反映在关联卡。作者邮件地址可见，持链接者可继续转发。内容被暂停后，官方仅受理创作者申请复核。','E-YTCD-10;E-YOD-06','未接受/拒绝的具体按钮、复制成独立副本、撤回链接、删除成员对共享列表的影响、离线缓存何时失效和正在播放时更新未知。'),
('19','清空本机后重新设置并区分家庭解绑','固定Player3/Mini2024；官方Player/Mini三键步骤，无实际固件；删除性动作仅文档研究','同时按住三键：Player夜灯红脉动，Mini屏显红点；在3秒内松开并按一次右橙键。Player短橙闪/Mini橙点后关机；然后重走F04设置。','下载内容清除，但家庭绑定保留。赠送前另在Settings → 设备删Player Name，底部Remove from family account；用户退出则Settings → 本用户三点 → Leave this family account。','E-YTCD-11','三键等待时长、错过3秒窗口、清除失败、缓存/进度/设置完整恢复、解绑传播与他人书库归属未核；不把F05硬重启当出厂重置。')]
fields=list(oldF[0]);cov=[]
for n,task,pre,op,state,eids,gap in newflows:
 cov.append(dict(flow_id='F-M037-'+n,product_id='M-037',task=task,depth='D2',evidence_ids=eids,known_branches=state,gaps=gap,visual_status='not_observed',hands_on='false',case_path='cases/M-037-yoto.md',checked_on=D))
with (B/'yoto-controls-deepening-coverage.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(cov)
updates={
'01':dict(evidence_ids='E-YOT-01;E-YTCD-12',known_branches='逐机在Settings→设备→Audio Download Status核下载；空闲接电联网准备只是建议，不保证隔夜完成。Player3为32GB；Mini2024与2021–23被当前容量页合列16/32GB并附序列规则，不能据此确认2024存在两种装配；缓存移除不删App库，联网播放可触发重下。',gaps='没有实机完成/失败反馈；离线页least-played与容量页longest time未播放的淘汰算法冲突；手机离线缓存、授权复查及两机断点未核。'),
'02':dict(known_branches='换卡或换内容不重置；月亮星星入口再选off可提前取消；到时停播而非已证明关机。',gaps='重启/断电/跨设备、手机后台、闹钟与睡眠计时重叠状态未说明。'),
'03':dict(evidence_ids='E-YOT-01;E-YOT-03;E-YTCD-14',known_branches='网络/下载分流：正文红时钟在线、白时钟离线、绿脸为连WiFi但无互联网。离线帮助称首播边流边下，故障帮助仍称无卡空闲才下，未据新正文消除冲突。',gaps='冲突所涉型号/版本未知；状态图标未实际查看；重试与等待不保证恢复，首播或隔夜不等于已下载完成。'),
'05':dict(evidence_ids='E-YOD-01;E-YOD-20',known_branches='旧代旋钮长按超过1秒快进/退，文档随持续按压加速；短按电源可暂停App发起的音频。10秒硬重启文档称保留WiFi和音频，区别出厂重置；不采第四代绿键/电源语义。',gaps='长按停止与约3秒关机分界仍不明；取卡/换卡/重启与跨机秒点保存均未闭合；公布加速比未实测，不等于精确寻位。'),
'07':dict(evidence_ids='E-YOD-06;E-YTCD-13',known_branches='卡关联云端列表与本机下载分开；自建MYO最多100轨，单轨100MB/60分钟、整卡500MB/5小时。该限制不套直接关联已购Yoto卡。',gaps='字节/时长超限反馈、上传取消/断网、重复命名、卡旧关联保留时点、误覆写撤销未知。'),
'11':dict(evidence_ids='E-YOD-11;E-YOD-12;E-YOD-13;E-YOD-19;E-YTCD-08;E-YTCD-09',known_branches='家庭成员可控全部登记设备和设置；每个成员可在Settings→用户三点移除他人或离开家庭，不能推定仅管理员可执行。一个用户加入新家庭会离开旧家庭。',gaps='邀请码与设备现场六码保留不同路径；未证实儿童/只读亲属分权、移除后的缓存/购买/录音归属或撤权延迟，也未证实跨机秒点。')}
up=[]
for n,changes in updates.items():
 before=next(r for r in oldF if r['flow_id']=='F-M037-'+n);after={**before,**changes,'checked_on':D};up.append({'flow_id':before['flow_id'],'before':before,'after':after,'changed_fields':{k:{'before':before[k],'after':after[k]} for k in before if before[k]!=after[k]}})
(W/'old-coverage-updates.json').write_text(json.dumps(up,ensure_ascii=False,indent=2)+'\n')
# Directory: title-level article inventory, every retrieved category entry; not all articles are read.
byhash={unquote(x['url']).rsplit('-',1)[-1]:x['evidence_id'] for x in oldE};byhash.update({h:'E-YTCD-'+n for n,h,_,_ in new_defs});flow_by_e={}
for r in oldF+cov:
 for e in r['evidence_ids'].split(';'):flow_by_e.setdefault(e,set()).add(r['flow_id'])
for x in up:
 for e in x['after']['evidence_ids'].split(';'):flow_by_e.setdefault(e,set()).add(x['flow_id'])
read_new={h for n,h,t,l in new_defs};read_old={unquote(x['before']['url']).rsplit('-',1)[-1] for x in recap};inv=[]
def add(domain,item,url,loc,status,eids='',flows='',gap=''):
 inv.append(dict(inventory_id=f'I-YTCD-{len(inv)+1:03}',domain=domain,item=item,source_url=url,locator=loc,coverage_status=status,evidence_ids=eids,flow_ids=flows,gap=gap))
rootcats=json.loads((W/'extracted/app-overview-embedded.json').read_text())['data']['categories']
for c in rootcats:
 k=c['slug'];i=cat_order.index(k)+1 if k in cat_order else None
 add('帮助一级分类',c['title'],'https://support.yotoplay.com'+c['href'],'公开App页initial-data.data.categories','category_titles_observed_text' if i else 'outside_this_batch','E-YOD-02'+(f';E-YTCD-{i:02}' if i else ''),'','订单/第四代分类本轮不取正文；不是整站完整性保证' if not i else '分类下文章另逐项登记')
for i,k in enumerate(cat_order,1):
 for a in cats['category-'+k]['articles']:
  e=byhash.get(a['hash']);scope='旧代/其他专属条目不套本轮硬件' if any(t in a['title'] for t in ['1st Generation','2nd Gen','2nd Generation','SD card formatting']) else '标题不证明支持；未查项目需该型号正文/实际版本'
  status='body_read_this_batch' if a['hash'] in read_new|read_old else 'prior_case_evidence_only' if e else 'index_only'
  add(cats['category-'+k]['category']['title'],a['title'],'https://support.yotoplay.com'+a['href'],'category.articles hash='+a['hash'],status,f'E-YTCD-{i:02}'+(';'+e if e else ''),';'.join(sorted(flow_by_e.get(e,[]))),scope)
settings=[('页签','News'),('页签','Create'),('页签','Library / Device Picker / Now Playing / Maxi Player'),('页签','Discover'),('页签','Settings / Family users and players'),('STATUS','Player name'),('STATUS','Battery level'),('STATUS','Manage Wi-Fi'),('STATUS','Audio Download Status'),('ALARMS','Alarms'),('DAY AND NIGHT','Morning'),('DAY AND NIGHT','Bedtime'),('DAY AND NIGHT','Morning Light / Nightlight Color (V2/V3)'),('DAY AND NIGHT','Auto Display Brightness (V3)'),('DAY AND NIGHT','Display Brightness'),('DAY AND NIGHT','Volume Limit'),('DAY AND NIGHT','Right Button Shortcuts'),('ADVANCED','Battery saver'),('ADVANCED','Auto Power-Off'),('ADVANCED','Bluetooth pairing'),('ADVANCED','Headphone volume limit'),('ADVANCED','24-hour clock'),('ADVANCED','Automatic time zone'),('ADVANCED','Yoto OS version'),('ADVANCED','Yoto App version'),('ADVANCED','Registration code'),('ADVANCED','Player MAC Address'),('ADVANCED','Yoto Beta labs'),('ADVANCED','Unpair this player from Family Account')]
for d,item in settings:add('App '+d,item,next(x['url'] for x in oldE if x['evidence_id']=='E-YOD-02'),'App总览：'+d,'menu_inventory_only','E-YOD-02','','入口目录不算任务闭合；夜灯仅Player3，V3显示自动亮度不套Mini或夜灯')
with (B/'yoto-controls-deepening-inventory.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');wr.writeheader();wr.writerows(inv)
# Modify only assigned case. Preserve historical table, label explicitly; new facts and old branches below.
s=(W/'case-before.md').read_text();s=s.replace('查阅：2026-10-26。','查阅：2026-10-26；本轮补证2026-10-27。',1)
s=s.replace('**D2 文档重建；无设备操作、登录、购买、连续视觉观察或单帧图像观察。** 主要取得官方搜索索引正文；直接打开多为动态壳，索引不是已归档网页截图。','**D2 文档重建；无设备操作、登录、购买、连续视觉观察或单帧图像观察。** 2026-10-26主要为官方搜索索引；本轮6个正常公开HTML已归档，分类页内嵌正文可直接解析，未调用API。两个闹钟配图返回403后停止，均未观察。',1)
s=s.replace('本轮未取得固定版本完整单册。','下表保留2026-10-26基线，旧“未查”状态以本轮更新及新台账为准。仍未取得固定版本完整单册。',1)
s=s.replace('新增 E-YOD 为本轮定位。','E-YOD 为2026-10-26新增定位。',1)
s=s.replace('保留原有 F-M037-01–03；本轮从 04 追加。','历史追加04–15保留；2026-10-27仅新增16–19，相同任务补原号。',1)
s=s.replace('| E-YOT-01 |\n| F-M037-02', '| E-YOT-01；E-YTCD-12 |\n| F-M037-02',1)
s=s.replace('换卡/内容不重置；重启、断电、跨设备未知。','换卡/内容不重置；返回月亮星星图标选off可提前取消。重启、断电、跨设备未知。',1)
s=s.replace('每台独立按容量/使用缓存；淘汰不移除 app 书库，在线重播可重新下载。','每台独立缓存；淘汰不移除app库，在线重播可触发重下。Player3为32GB；当前帮助把Mini2024与2021–23合列16/32GB并给机底序列规则；这不证明2024本身有两种装配，实际容量需核本机/型号专属资料。准备路径不保证隔夜完成。',1)
s=s.replace('| E-YOT-01、E-YOT-03 |','| E-YOT-01、E-YOT-03、E-YTCD-14 |',1)
s=s.replace('书库可见与本机下载完成不同；后台下载描述有冲突，见下。','书库可见与本机完成不同；正文称红时钟在线、白时钟离线、绿脸为连Wi-Fi而无互联网。图标未实际观察；下载调度冲突见下。',1)
s=s.replace('屏显箭头表示加速。','屏显箭头表示加速：文档写超过1秒、4秒、7秒后依次20/50/120音频秒每秒，不当实测或精确寻位。',1)
s=s.replace('| E-YOD-01 | 普通','| E-YOD-01、E-YOD-20 | 普通',1)
s=s.replace('离线仍查 F-M037-01。 | E-YOD-06 |','离线仍查F01。自建MYO限100轨，单轨100MB/60分钟、整卡500MB/5小时；直接关联已购卡不套此限。 | E-YOD-06、E-YTCD-13 |',1)
s=s.replace('可访问家庭书库、管理设备；一个用户加入新家庭会离开原家庭。','成员可控全部家庭设备/设置；每位成员在Settings → 用户三点可移除他人或离开家庭，不推定管理员独占权限。加入新家庭会离开原家庭。',1)
s=s.replace('| E-YOD-11、E-YOD-12、E-YOD-13、E-YOD-19 |','| E-YOD-11、E-YOD-12、E-YOD-13、E-YOD-19、E-YTCD-08、E-YTCD-09 |',1)
rows='\n'.join('| F-M037-'+n+' | '+task+' D2 | '+pre+' | '+op+' | '+state+' | '+eids.replace(';','、')+' | '+gap+' |' for n,task,pre,op,state,eids,gap in newflows)
s=s.replace('\n## 分支、数据状态与证据冲突','\n'+rows+'\n\n## 分支、数据状态与证据冲突',1)
s=s.replace('- **下载调度冲突保留。**','- **下载调度冲突保留（2026-10-27正文仍如此）。**',1)
s=s.replace('缓存按设备算，另一台已完成不证明这一台可离线。','缓存按设备算，另一台已完成不证明这一台可离线。新容量页称删除“最长时间未播放”的音频，离线页则写least-played；频次与最近使用不是同一指标，当前算法仍未知（E-YTCD-12）。小时容量是语音样本估算，不作为保证。',1)
s=s.replace('其打断/续播优先级未核定。','F16已补闹钟编辑/取消/停止，F17补日夜与夜灯；其打断/续播优先级仍未核定。',1)
append='''
## 2026-10-27目录深化与原件

[逐项目录](../batches/yoto-controls-deepening-inventory.csv)记录7个一级分类、5个本轮相关分类的87条文章（23内容/MYO、30使用、7新用户、19排错、8Club），以及29项App页签/设置，共123条入口。文章标题清点与正文分析分别标记；订单和第四代分类不在本轮展开，不能称整站全部正文已分析。第三代/Mini2024的完整固定版本手册仍未取得。

本轮补6条旧任务（01/02/03/05/07/11）和4个新目标（16–19），现19F。目录中的Groups、交互卡、RSS、Beta、时区及第三方开发者授权等没有闭合任务。右键旧代快捷不扩展为第四代可离线浏览20标题的绿键列表。

当前容量帮助将Mini2024与2021–23合并一行，未给2024单独分行；不能据此断定2024本身存在两种装配。其早期Mini序列规则：序列以5或更小数字开头且不以-H/-H7/-U结尾时列16GB；其他情形列32GB。没有实机序列，故不锁定容量。硬重启、清本机下载、家庭解绑、退出家庭、删除账户是不同层；本轮仅补前四项，不据菜单名推导账户数据恢复。

共享MYO的拥有者与家庭成员身份分开。移除成员不等于已证明收回所有共享链接、清掉离线内容；内容原作者后续编辑会传播到关联卡，具体离线同步/正在播放时切换未说明。官方暂停分享后的复核也只是申请路径，不保证恢复。

来源短摘记见[source-notes](../artifacts/yoto-controls-deepening/source-notes.md)。原件在work/yoto-controls-deepening，逐个URL/采集路径/正文hash定位/SHA记于新证据及旧E追加采集建议；不是页面截图或实操。

| 新E | 官方来源 | 定位 |
| --- | --- | --- |
'''
for e in new:append+=f"| {e['evidence_id']} | [{e['title']}]({e['url']}) | {e.get('note','')}；原件定位见evidence |\n"
append+='\nE-YOD-01/02/19/20、E-YOT-01/02/03沿用编号；本轮直接读到公开HTML中的正文，追加采集建议交根代理合并。附图403、无图像观察；全部hands_on=false。只改研究记录，无设备、账号、播放、家庭或订阅状态变化。\n'
s+=append;s=s.replace('\n\n| F-M037-16','\n| F-M037-16');(L/'cases/M-037-yoto.md').write_text(s)
(W/'build-counts.json').write_text(json.dumps({'new_E':len(new),'new_F':len(cov),'old_F':len(up),'old_E_additional_captures':len(recap),'inventory':len(inv),'root_categories':7,'article_entries':sum(len(cats['category-'+k]['articles']) for k in cat_order),'settings_entries':len(settings)},indent=2)+'\n')
print((W/'build-counts.json').read_text())
