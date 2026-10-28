from pathlib import Path
import json,csv,hashlib,shutil
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';work=root/'work/huawei-controls-deepening';batch=lib/'batches';art=lib/'artifacts/huawei-controls-deepening';art.mkdir(exist_ok=True)
sh=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
logs=sum([json.loads((work/n).read_text()) for n in ['fetch-log.json','fetch-extra-log.json','fetch-regional-log.json']],[]);cap={x['file']:x for x in logs}
imgs=json.loads((work/'image-fetch-log.json').read_text())
for im in imgs:shutil.copyfile(root/im['raw_work_path'],art/im['file'])
notes='''# M-061 MatePad Paper 控制、笔记与恢复补证

查阅2026-10-27；公开匿名HTML正文及5幅实际查看的静态图。无设备/账户操作，无音视频、App或固件下载。中文系统专属帮助以HarmonyOS2.1为基线；加拿大笔记页同为2.1，其他三份区域帮助只列Wi-Fi4GB+64GB，未显示系统版本，不能当当前大陆构建验证。共用同步/贴纸FAQ中仅采用明确点名本机的条件。

## 来源目录边界

官方[中国支持页](https://consumer.huawei.com/cn/support/tablets/matepad-paper/)与[全球支持页](https://consumer.huawei.com/en/support/tablets/matepad-paper/)返回HTTP200，但下载/问题栏为空壳，没有取得完整手册或全部帮助目录。只读了公开页面所加载的一份JS定位文档列表线索，未调用其服务接口。第三方镜像出现HMW-W09/HarmonyOS2_01、17章用户指南索引，但没有下载或用其正文填事实。正式inventory因此是**已发现官方来源/功能域与未决项清单**，不是完整功能清单。

## 新证据定位与短摘记

| E | 来源 | 定位、范围与使用边界 |
| --- | --- | --- |
| E-HWCD-01 | [官方支持入口](https://consumer.huawei.com/cn/support/tablets/matepad-paper/) | 产品名称、文档下载与FAQ栏；只D1目录入口。全球页产品选择项明确HMW-W09、Wi-Fi4+64；价格/顶部其他设备不用于事实。 |
| E-HWCD-02 | [笔记创建/分享](https://consumer.huawei.com/cn/support/content/zh-cn15879070/)；[加拿大同ID](https://consumer.huawei.com/ca/support/content/en-us15879070/) | 中文HarmonyOS2.1：新建、模板、套索、手写转文本、单篇图/PDF分享、多篇勾选后按提示分享。加拿大4+64/2.1才明确保存后Move分类；中文页未列此步，不能混作同版界面。 |
| E-HWCD-03 | [新西兰录音标记](https://consumer.huawei.com/nz/support/content/en-gb15879066/) | Wi-Fi4+64、系统未知；笔记页开始/打标/停止；标记跳时间点，长按拖动/删除。未说明音频跟随笔迹或转写句子；不是录音机App。 |
| E-HWCD-04 | [笔记同步失败](https://consumer.huawei.com/cn/support/content/zh-cn15931676/) | 1–6及仍无法解决；共用FAQ明确MatePad Paper强停路径。笔记12.3.1.460+、云空间12.2.0.301+为文档门槛，不证明本机已安装；前台/同账号/网络/容量/电量有条件。修改一笔重试、云端延迟和复制副本均非恢复保证。 |
| E-HWCD-05 | [华为分享](https://consumer.huawei.com/cn/support/content/zh-cn15879068/) | 方法1–3；本机Wi-Fi及LTE列入，2.1基线；双端开分享→选择文件/接收端→接收。未证明导入书架、断线续传或重名处理；应用单向分享仅目录边界，本轮未安装。 |
| E-HWCD-06 | [分布式文件管理](https://consumer.huawei.com/cn/support/content/zh-cn15879275/) | HarmonyOS2.1；以多屏协同已连接为前提，从电脑此电脑打开本机盘符、调用或保存文件。不是首次配对教程，也不是云同步/备份。 |
| E-HWCD-07 | [PDF裁边/对比度/水印](https://consumer.huawei.com/cn/support/content/zh-cn15898848/) | 本机华为阅读，页面标不涉及系统版本；裁边保存作用各页、可恢复原裁边；上下滑动两页取占比大页，等分取第一页；水印开关可反向恢复。未证明修改源PDF或导出处理后的文件。 |
| E-HWCD-08 | [斯里兰卡悬浮导航](https://consumer.huawei.com/lk/support/content/en-us15892864/) | Wi-Fi4+64，版本未列；控制中心启用、单击展开/双击刷新、长按移位；闲置超过5秒贴边隐藏。未证明跨App位置持久或性能。 |
| E-HWCD-09 | [孟加拉手势导航](https://consumer.huawei.com/bd/support/content/en-gb15879072/) | Wi-Fi4+64，版本未列；设置System & updates/System navigation/Gestures；边缘返回、上滑主页、上滑停顿最近任务。网页标题提hand不是隔空手势证据。 |
| E-HWCD-10 | [分屏笔记](https://consumer.huawei.com/cn/support/content/zh-cn15879069/) | HarmonyOS2.1；笔从右上内划→分屏笔记；横屏手指从左右边缘内划或笔菜单退出。与旧掌阅焦点/禁止手写特例分别记录。 |
| E-HWCD-11 | [同步后贴纸不可见](https://consumer.huawei.com/cn/support/content/zh-cn16009712/) | 页面适用系统HarmonyOS4.2属跨端背景；正文笔记12.4.2.265+、彩色源端柔光纸和可改色贴纸→Paper灰度相近。改纸/贴纸颜色后再同步；不能据此称Paper运行4.2，也不能把所有不可见都判为灰度问题。 |
| E-HWCD-12 | [专属桌面](https://consumer.huawei.com/cn/support/content/zh-cn15879074/) | 2.1；左导航、首页服务卡片、头像设置入口。仅功能目录，不展开登录/所有卡片编辑任务。 |
| E-HWCD-13 | [产品重要信息](https://consumer.huawei.com/cn/support/content/zh-cn16086936/) | 2.1、Wi-Fi4+64/6+128；型号HMW-W09。型号/适用范围材料，不用于交互深度。 |

## 实际查看的静态图

仅E-HWCD-03和07的visual_observed=true。2026-10-27通过view_image逐一查看，没有连续前后状态、点击、录音或PDF操作。

- `record-nz-image-1.png`：原HTML tkb-content内第2个img（0基索引1），麦克风图标；第3个img `record-nz-image-2.png`为圆形录音按钮；第4个img `record-nz-image-3.png`为标记图标；第5个img `record-nz-image-4.png`为方形停止按钮。只用于识别正文被Image占位的控件，不证明在设备上的布局或反馈。
- `help-15898848-image-1.png`：原HTML第2个img，阅读页上方剪刀按钮及下方工具浮层；尚未进入裁边框，不能把此图说成已观察拖动/保存。裁边/还原效果来自文字。

5图已原样存入同目录，URL和SHA在证据artifact_hashes及本批work/image-fetch-log.json。没有图像加工。

## 旧材料复读及失败

E-HWP-01～06、08的2026-10-26HTML原件在本轮本地复读，建议追加采集元数据而不重复编号；没有再次请求这些URL。E-HWP-07原本地件是搜索壳，仍不作为正文原件，现有web正文依据保留。

中文15879066/15892864/15879072均正常请求后跳转搜索页，记录失败取得正文；不伪称中国帮助。区域来源是搜索实际找到的官方链接。全部HTTP请求未遇401/403/429；不访问账户或提交表单。首次JS临时文件被文本提取覆盖，明确降为派生件，随后仅复采该公开JS到独立original文件留真实哈希；不把派生件登记为原件。

## 尚未解决

完整官方手册目录、笔记删除/回收/重装、录音与笔迹/文本双向关联、转写断网/额度失败、笔记冲突保留规则、导出文件是否含可编辑笔迹/录音、华为阅读进度与笔迹全量恢复、全局手势当前构建表现。同步不是备份，已有副本也不保证所有对象可恢复。
'''
(art/'source-notes.md').write_text(notes)
# New E rows: one logical article; regional same-ID capture is attached, not a second source ID.
spec=[
('01','MatePad Paper官方支持入口与型号选择','support-cn.html','official_support_index','文档下载、FAQ；全球页型号选择','CN support landing; no manual body obtained'),
('02','新建、整理及分享笔记','help-15879070.html','official_help','新建/套索/转文本；单个与多个分享；CA Note/Move','CN HarmonyOS2.1 listed Wi-Fi/LTE; CA Wi-Fi4+64 HarmonyOS2.1 separately'),
('03','笔记录音开始、标记与停止','record-nz.html','official_help','Try it out1–2；Note；正文4控件图','NZ MatePad Paper Wi-Fi4GB+64GB; system/build unstated'),
('04','笔记云同步失败排查','help-15931676.html','official_help','1–6；仍无法解决；Paper强行停止专段','Shared FAQ explicitly names Paper; Notes12.3.1.460+/Cloud12.2.0.301+; installed build unknown'),
('05','华为分享跨设备文件接收','help-15879068.html','official_help','方法1–3；提示','CN Paper Wi-Fi/LTE HarmonyOS2.1; phone mixed system scope kept separate'),
('06','协同后从电脑使用本机文件','help-15879275.html','official_help','方法1–2','CN Paper HarmonyOS2.1; existing Multi-Screen Collaboration connection prerequisite'),
('07','PDF裁边、还原、对比度和水印开关','help-15898848.html','official_help','PDF裁边；对比度设置；PDF去水印；裁边入口图','CN Paper Huawei Reading; system and app build unstated'),
('08','悬浮导航展开、移位与刷新','dock-lk.html','official_help','Try it out；Note','LK official regional help; Paper Wi-Fi4GB+64GB; version unstated'),
('09','系统手势和最近任务入口','gestures-bd.html','official_help','Try it out；Gestures','BD official regional help; Paper Wi-Fi4GB+64GB; version unstated'),
('10','分屏笔记正常入口与退出','help-15879069.html','official_help','方法：开启/退出；提示','CN Paper Wi-Fi/LTE HarmonyOS2.1; landscape finger-exit condition'),
('11','灰度相近导致同步贴纸不可见','help-16009712.html','official_help','问题现象/原因/解决方案','Cross-device Notes12.4.2.265+; page lists HarmonyOS4.2 for source context, not proof Paper runs4.2'),
('12','专属桌面与服务卡片目录','help-15879074.html','official_help','我的帐号；首页；提示','CN Paper Wi-Fi/LTE HarmonyOS2.1; no precise build'),
('13','HMW-W09型号信息','help-16086936.html','official_product_information','型号表与适用产品/版本','CN Paper Wi-Fi4+64/6+128; HarmonyOS2.1; hardware identity only')]
new=[]
for num,title,name,typ,loc,scope in spec:
 c=cap[name];r=dict(evidence_id='E-HWCD-'+num,product_id='M-061',title=title,url=c['url'],checked_on='2026-10-27',source_type=typ,access_mode='public_http_html_body_read' if c['article_body'] else 'public_http_landing_read',version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note='适用范围、来源差异、步骤与缺口见正式短摘记；非实机验证。',hands_on=False,visual_observed=num in ['03','07'],archived_artifact=str((art/'source-notes.md').relative_to(lib)),sha256=sh(art/'source-notes.md'),capture_state='short_note_and_raw_capture',raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],supersedes=None)
 if num in ['01','02']:
  a=cap['support-en.html' if num=='01' else 'notes-ca.html'];r['additional_captures']=[dict(capture_url=a['url'],checked_on='2026-10-27',raw_work_path=a['raw_work_path'],raw_sha256=a['raw_sha256'],locator='产品选择HMW-W09' if num=='01' else '保存后Move分类；CA 2.1/Wi-Fi4+64',network_refetched=True)]
 if num in ['03','07']:
  ii=[x for x in imgs if x['source_html']==('record-nz.html' if num=='03' else 'help-15898848.html')];r.update(visual_kind='official_help_static_icons' if num=='03' else 'official_help_static_screenshot',visual_audit_path=str((art/'source-notes.md').relative_to(lib)),rendered_artifacts=[str((art/x['file']).relative_to(lib)) for x in ii],artifact_hashes={str((art/x['file']).relative_to(lib)):sh(art/x['file']) for x in ii})
 new.append(r)
(batch/'huawei-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in new))
flows=[
('11','新建、移动与分享笔记','E-HWCD-02','CN笔记右上新建→选模板/手写；套索移动、转文本插入；单篇图/PDF分享；多篇勾选后按提示分享；CA同ID另有保存后Move分类','保存按钮/取消、分类创建与误移撤销、重名/失败分享、各格式可编辑性/录音是否包含未知','not_observed'),
('12','录音中标记并回听定位','E-HWCD-03;E-HWP-06;E-HWP-07','NZ Wi-Fi4+64笔记页麦克风→圆形录音→打标→方形停止；点标记跳音频时间，长按移动/删除','系统构建/国内对应、离开与中断保存、删除标记撤销、音频跟随笔迹/转写句子及跨设备恢复未知','static_reference_only'),
('13','排查笔记同步与看似丢失','E-HWCD-04;E-HWCD-11','同账号/前台联网、版本/容量/电量；Paper在应用管理强停后重开；单篇改一笔返回重试；颜色灰度相近可致贴纸不可见','排查不保证成功；冲突覆盖、删除回收、录音/标签备份、等待时长；4.2背景不可套Paper系统','not_observed'),
('14','通过华为分享发送文件','E-HWCD-05','两端开启华为分享→发送端长按文件分享/电脑拖到目标→接收端接收；应用分享另有限制','完成回执/目标目录、拒绝/取消、断线续传、同名覆盖、接收后进书架未知','not_observed'),
('15','已协同时在电脑读写本机文件','E-HWCD-06','以多屏协同已连接为前置；电脑此电脑→本机盘符→分类文件夹；可作为附件或将编辑文件存回','首次连接/电脑版本、读写权限、断连保存、缓存副本和编辑冲突未知；不当备份','not_observed'),
('16','调整PDF显示并还原裁边','E-HWCD-07','中央调工具→剪刀进入裁边/拖框/保存且影响各页；两页取大页/等分取第一页；再次裁边可还原；对比度与去水印开关','保存/还原精确按钮、源PDF是否改写、导出效果、笔迹位置、关闭书籍后持久性未知','static_reference_only'),
('17','使用系统手势与悬浮导航','E-HWCD-08;E-HWCD-09','区域Wi-Fi4+64：设置手势→边缘返回/底部上滑主页/停顿最近任务；悬浮球单击展开、长按移动，闲置超5秒贴边','区域帮助系统未标；最近任务关闭/锁定、误触、跨App位置与重启保存未知；非隔空操作','not_observed')]
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on'];cov=[]
for n,task,es,known,gaps,vis in flows:cov.append(dict(flow_id='F-M061-'+n,product_id='M-061',task=task,depth='D2',evidence_ids=es,known_branches=known,gaps=gaps,visual_status=vis,hands_on='false',case_path='cases/M-061-huawei-paper.md',checked_on='2026-10-27'))
with (batch/'huawei-controls-deepening-coverage.csv').open('w',newline='') as f:w=csv.DictWriter(f,fields,lineterminator='\n');w.writeheader();w.writerows(cov)
old={}
for l in (batch/'extension-manuals-evidence.jsonl').read_text().splitlines():
 r=json.loads(l)
 if r.get('product_id')=='M-061':old[r['evidence_id']]=r
adds=[]
for eid,r in old.items():
 if not r.get('raw_work_path'):continue
 a=dict(capture_kind='local_reread_of_prior_official_html',original_acquisition_date='2026-10-26',checked_on='2026-10-27',network_refetched=False,source_url=r['url'],raw_work_path=r['raw_work_path'],raw_sha256=sh(root/r['raw_work_path']),archived_artifact=str((art/'source-notes.md').relative_to(lib)),sha256=sh(art/'source-notes.md'),hands_on=False,visual_observed=False)
 before=r.get('additional_captures');adds.append(dict(evidence_id=eid,field='additional_captures',before=before,after=(before or [])+[a],reason='复读旧HTML，不重复新建E；原始取得日期仍2026-10-26'))
(work/'additional-captures-updates.json').write_text(json.dumps(adds,ensure_ascii=False,indent=2)+'\n')
oldrows={r['flow_id']:r for r in csv.DictReader((batch/'extension-manuals-coverage.csv').open(newline='')) if r['product_id']=='M-061'}
changes={
'01':dict(evidence_ids='E-HWP-01;E-HWCD-08',known_branches='智能/普通刷新与一键刷新分开；灯光一键熄灯/白天/夜间/自定义；区域4+64悬浮球可双击刷新',gaps='逐app持久性、灰度/实际速度功耗；区域悬浮球当前大陆构建未知'),
'02':dict(evidence_ids='E-HWP-02;E-HWP-03',known_branches='华为阅读导入同账号对端本地书；对端限HarmonyOS2/3所支持机型；建议5GHz Wi-Fi，2.4GHz可能不稳；看不到书时重开两端Wi-Fi/蓝牙',gaps='重连失败、重复/DRM导入、文件完成回执和进度/笔迹同步；Huawei Share文件接收另见F14'),
'06':dict(evidence_ids='E-HWP-07;E-HWCD-02',known_branches='HarmonyOS2.1笔记App不支持原录音导出；转写文字可随图/PDF分享，原音频另用录音机；笔记单篇/多篇分享不证明包含原音频',gaps='录音机实际导出步骤、笔记录音备份/恢复及后续版本变化；CN/CA笔记整理差异'),
'08':dict(evidence_ids='E-HWP-08;E-HWCD-10',known_branches='正常笔右上内划选分屏；中文2.1横屏可手指左右边缘退出或笔菜单退出；掌阅禁止手写会转截图，退出需先聚焦笔记',gaps='竖屏手指退出、当前构建焦点/误触、分屏笔记保存与异常恢复；不能据区域简写取消中文横屏条件')}
suggest=[]
for n,d in changes.items():
 fid='F-M061-'+n;d['checked_on']='2026-10-27'
 for field,after in d.items():
  before=oldrows[fid][field]
  if after!=before:suggest.append(dict(flow_id=fid,source_batch='docs/research/interaction-library/batches/extension-manuals-coverage.csv',field=field,before=before,after=after,reason='同任务补机制/条件，编号和D2不变；由root合并'))
(work/'existing-coverage-updates.json').write_text(json.dumps(suggest,ensure_ascii=False,indent=2)+'\n')
with (work/'existing-coverage-updates.csv').open('w',newline='') as f:w=csv.DictWriter(f,list(suggest[0]),lineterminator='\n');w.writeheader();w.writerows(suggest)
# Source-linked functional inventory, explicitly incomplete: not a fabricated manual TOC.
inv=[]
def item(title,es,fs,status,gaps,urls=''):
 inv.append(dict(item_id=f'M061-I{len(inv)+1:02}',kind='functional_domain',title=title,evidence_ids=es,flow_ids=fs,status=status,gaps=gaps,source_urls=urls,checked_on='2026-10-27'))
item('官方完整手册/全部帮助目录','E-HWCD-01','','landing_only','支持页无下载正文/完整目录；第三方17章镜像不当官方完整目录')
item('型号/系统与区域条件','E-HWCD-13;E-HWCD-01','','identity_only','HMW-W09仅所列Wi-Fi；LTE硬件型号未核；当前安装构建未知')
for title,es,fs,gaps in [
('刷新与灯光','E-HWP-01;E-HWCD-08','01','实际速度/功耗、跨应用持久状态'),('书籍格式/分布式导入','E-HWP-02;E-HWP-03','02','DRM/重复/失败；未证明学习进度同步'),('华为阅读批注/撤销','E-HWP-04','03','重启/跨端保存及撤销边界'),('PDF重排','E-HWP-05','04','位置持久、原文件导出'),('语音转写','E-HWP-06','05','额度/断网失败与恢复'),('笔记录音出口限制','E-HWP-07;E-HWCD-02','06','原音频导出及恢复未闭合'),('掌阅全屏系统栏','E-HWP-08','07','触发区域/实测'),('分屏笔记及焦点','E-HWCD-10;E-HWP-08','08','保存粒度、竖屏退出'),('掌阅旋转定位','E-HWP-08','09','跨排版稳定锚点'),('掌阅隐藏笔迹','E-HWP-08','10','区别华为阅读；跨设备恢复')]:item(title,es,'F-M061-'+fs,'body_read_partial',gaps)
for r in cov:item(r['task'],r['evidence_ids'],r['flow_id'],'body_read_partial',r['gaps'])
item('桌面服务卡片/帐号入口','E-HWCD-12','','body_read_directory','未展开全部卡片编辑/帐号切换/退出流程')
item('笔记删除/回收/重装恢复','','','not_located','没有对应型号完整帮助，不据同步推断恢复')
item('录音与笔迹/转写文本关联','E-HWCD-03;E-HWP-06','','unresolved_relation','只确认时间标记与转写插入，未确认双向关联')
item('听书/锁屏/下载与DRM','','','not_located','未取得型号化完整操作链')
item('系统备份/重置/升级回滚','','','not_located','不套普通MatePad帮助；未执行任何系统操作')
item('中文同ID录音/导航文章失效','','','redirect_only','15879066/15892864/15879072跳搜索；区域实际URL独立保留',';'.join(cap['help-'+n+'.html']['url'] for n in ['15879066','15892864','15879072']))
for r in inv:
 if not r['source_urls']:r['source_urls']=';'.join((old.get(e) or next((x for x in new if x['evidence_id']==e),{})).get('url','') for e in r['evidence_ids'].split(';') if e)
with (batch/'huawei-controls-deepening-inventory.csv').open('w',newline='') as f:w=csv.DictWriter(f,list(inv[0]),lineterminator='\n');w.writeheader();w.writerows(inv)
print('newE',len(new),'newF',len(cov),'inventory',len(inv),'oldFields',len(suggest),'oldCaptures',len(adds),'actual_static_images',len(imgs))
