"""One-time research record assembly. Never build or run AntennaPod."""
import csv
import hashlib
import json
import re
from html.parser import HTMLParser
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
W=Path(__file__).resolve().parent
LIB=ROOT/'docs/research/interaction-library'
ART=LIB/'artifacts/antennapod-queue-controls-deepening'
P=LIB/'batches/antennapod-queue-controls-deepening'
C='175e5ef8fe3e125a5014d70ef0c57cf85661c71b'
assert not ART.exists(),'Do not rerun the builder'
ART.mkdir()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,o):p.write_text(json.dumps(o,ensure_ascii=False,indent=2)+'\n')
def writecsv(p,rows,fields):
    with p.open('w',newline='') as f:
        out=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');out.writeheader();out.writerows(rows)
class Article(HTMLParser):
    def __init__(self):super().__init__();self.active=False;self.parts=[]
    def handle_starttag(self,t,a):
        if t=='article':self.active=True
        if self.active and t in ['p','li','h1','h2','br']:self.parts.append('\n')
    def handle_endtag(self,t):
        if t=='article':self.active=False
        if self.active and t in ['p','li','h1','h2']:self.parts.append('\n')
    def handle_data(self,d):
        if self.active:self.parts.append(d)

fetch=json.loads((W/'fetch-log.json').read_text());assert len(fetch)==9 and all(x['status']==200 for x in fetch)
for r in fetch[:3]:
    a=Article();a.feed((ROOT/r['raw_work_path']).read_text())
    (W/'raw'/r['name'].replace('.html','.txt')).write_text(re.sub(r'\n\s*\n+','\n',''.join(a.parts)).strip()+'\n')
local=json.loads((W/'local-source.json').read_text())
notes='''# AntennaPod：队列、过滤与下载请求的证据范围

2026-10-27。本批严格静态研究3.12.3 / commit 175e5ef8fe3e125a5014d70ef0c57cf85661c71b；未安装或运行。3篇官方帮助不指定app构建；6个新源码原件及复用的SubscriptionFragment均按旧官方tree的Git blob哈希核对。只读这10个来源，未扩展到下游下载服务、数据库写入、过滤器调用方或新的备份/计时源码。

## 帮助与版本

| 来源 | 本轮支持 | 限定 |
| --- | --- | --- |
| E-APQD-01 Multiple queues | 当前帮助明确不支持多队列。 | 不据文章标题虚造建队列流程；未核未来版本或未合并issue。 |
| E-APQD-02 Subscription tags | 单播客齿轮→Tags是赋标签入口；标签不能生成独立队列。 | 未取得命名/保存/删除确认全链；本批新F仅研究已有标签浏览。 |
| E-APQD-03 Refreshing podcasts | 全局刷新周期/关闭，单播客Keep Updated；关闭后不参与自动及Queue/Episodes发起的批量刷新，仍可单独刷新。 | 页面称默认12小时，未确认3.12.3真实初始化值；刷新目录不等于媒体下载。 |

## 源码定点结论

| 证据/文件 | 已读入口与变化 | 明确缺口 |
| --- | --- | --- |
| E-APQD-04 QueueFragment | 锁定警告有取消及不再提示；Keep Sorted开启时隐藏锁菜单。排序选择回调即存偏好/请求重排，Random关闭Keep Sorted，换回非随机可在当前对话框恢复。拖动先改内存，结束时才请求DB移动。清队列先确认。 | 不证明排序/清空可撤回；未读DBWriter本批相关方法，锁定不是防删除保证；当前播放项固定规则未知。 |
| E-APQD-05 EpisodeMultiSelectActionHandler | 入队只选有媒体且未标队列的项；下载按传入列表顺序请求且排除已下载/本地源。删除会清已下载/本地媒体，下载中未完成项则取消；移出Inbox把New改Unplayed。 | 调用方的数量提示由分支计数产生，不等待逐项成功；单项提示被此helper略过。当地文件警告只见调用，文案/确认未读。并发/失败回滚未知。 |
| E-APQD-06 ItemFilterDialog | 选项变化立即调用onFilterChanged；确认仅dismiss；Reset先传空集再清选中。 | 这是抽象控件；未读当前屏幕调用方，不能保证每处入口、保存范围或关闭即回滚。本批不据此新增完整过滤任务。 |
| E-APQD-07 FeedItemFilterQuery | 各维度AND；Unplayed包含New；Paused按position>0；Downloaded看下载时间字段；所有订阅状态都未选时回落Subscribed。 | SQL状态不检查物理文件存在；字段不是实时播放状态，不证明当前UI已选了这些条件。 |
| E-APQD-08 DownloadLogAdapter | 成功项不显示重试；失败项之前的列表记录若有同类型/ID成功也隐藏。点重试先隐藏按钮，再取源/媒体；有对象才请求刷新或下载，找不到只写日志。 | 未读日志列表的排序提供方，严格时间先后未核；按钮消失/下载中提示不等于成功。没有核自动退避和服务内网络/权限条件。 |
| E-APQD-09 DownloadLogDetailsDialog | OK退出，复制全部详情或点击URL/技术原因复制；源存在且允许才显示前往播客，已订阅开本机页，其他状态开在线页。 | 旧帮助的长按URL在浏览器打开与此点击复制不同；长按行为本批未核。原对象缺失可保留日志标题，URL仍unknown；加载异常只日志。 |
| E-APCD-13追加：SubscriptionFragment | 选已有标签重载订阅；普通订阅页onPause存last_tag，归档从root开始。标签不在已过滤结果时退回root；结果数量下降时结束多选。只有存在实际自定义标签才显示标签条。 | root仍是当前过滤条件下集合，不保证展示全库；赋标签及长按管理下游未读。onPause保存不是异常终止保证；空库与加载失败区别未实测。 |

多选入口的长按/点击细节属于未读adapter，不能补造。Queue中的多选动作发出后结束选择模式；订阅页委托批量handler，选择≤1才结束，具体批量实现未读。归档页隐藏部分菜单属于静态条件，不保证当前所有Android布局。

无图像观察、无连续流程、hands_on=false、visual_observed=false。官方帮助正文经web读取并普通HTTP归档，6个源码GET均200；本地复用没有重新请求。没有设备/账户动作、剪贴板复制、媒体下载、外部发送或代码执行。网页帮助并非源码版本互证；完整定位和散列见本批E、read-ranges.json及manifest。
'''
(ART/'source-notes.md').write_text(notes)
common={'product_id':'M-040','checked_on':'2026-10-27','firmware_version':None,'hands_on':False,'visual_observed':False,'supersedes':None,
        'archived_artifact':'artifacts/antennapod-queue-controls-deepening/source-notes.md','sha256':sha(ART/'source-notes.md'),
        'capture_state':'text_and_static_code_notes_archived_no_product_visual'}
locators=[
 'Article body: current multiple-queue limitation',
 'Article body: tag entry and no tag-based queues',
 'Article body: global interval and per-feed Keep Updated',
 '88–137;230–365;419–490;527–728: queue menus, multi-select, ordering, drag write request',
 '24–251: selected-item action dispatch and count feedback',
 '1–122: option callbacks, reset and dismiss',
 '1–103: SQL filter predicate generation',
 '28–153: success/error row, retry visibility and dispatch',
 '35–168: detail fields, copying and conditional feed navigation'
]
E=[]
for i,(r,locator) in enumerate(zip(fetch,locators),1):
    code=r['kind']=='pinned_source'
    row=common|{'evidence_id':f'E-APQD-{i:02}','title':r['name'],
        'url':r.get('source_url',r['url']),'source_type':'official_source_code' if code else 'official_help',
        'access_mode':'ordinary_HTTP_fixed_commit_then_targeted_static_read' if code else 'official_web_body_read_and_HTTP_archive',
        'source_updated_on':None,'app_build':'3.12.3 (versionCode 3120395); source only' if code else None,
        'version_scope':f'Android 3.12.3 source at {C}; not installed APK' if code else 'Official help read 2026-10-27; app build unspecified',
        'locator':locator,'raw_work_path':r['raw_work_path'],'raw_sha256':r['raw_sha256'],
        'note':'源码回调/文档描述，不是执行结果；下游未读处见独立摘记。'}
    if code:row.update(commit=C,verified_git_blob=r['verified_git_blob'])
    E.append(row)
Path(str(P)+'-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in E))
dump(W/'read-ranges.json',[{'evidence_id':r['evidence_id'],'raw_work_path':r['raw_work_path'],'locator':r['locator'],'whole_product_read':False} for r in E]+
     [{'evidence_id':'E-APCD-13','raw_work_path':local['raw_work_path'],'locator':'SubscriptionFragment 128–230;246–300;335–510','whole_product_read':False}])
oldF=json.loads((W/'coverage-before.json').read_text());byF={r['flow_id']:r for r in oldF}
oldE=json.loads((W/'evidence-before.json').read_text());byE={r['evidence_id']:r for r in oldE}
newF=[{'flow_id':'F-M040-13','product_id':'M-040','task':'按已有标签浏览订阅并恢复选择','depth':'D2',
       'evidence_ids':'E-APQD-01;E-APQD-02;E-APCD-13',
       'known_branches':'订阅页点标签重载；普通订阅onPause保存last_tag，归档从root开始。标签不在过滤结果则退root；结果数量下降结束多选；标签不能生成独立队列。',
       'gaps':'赋标签的命名/确认、长按管理、标签同步/删除恢复和异常终止保存未核；root仍受现有过滤限制，未实操。',
       'visual_status':'not_observed','hands_on':'false','case_path':'cases/M-040-antennapod.md','checked_on':'2026-10-27'}]
writecsv(Path(str(P)+'-coverage.csv'),newF,list(oldF[0]))
changes=[]
def upd(num,known,gaps,extra,task=None):
    key=f'F-M040-{num:02}';before=byF[key];after=before|{'known_branches':known,'gaps':gaps,'evidence_ids':before['evidence_ids']+';'+extra,'checked_on':'2026-10-27'}
    if task:after['task']=task
    changes.append({'flow_id':key,'before':before,'after':after,'changed_fields':{k:{'before':before[k],'after':after[k]} for k in before if before[k]!=after[k]}})
upd(2,'排队/下载/已播分开；下载入队可配置。3.12.3批量入队过滤无媒体和已标队列项；Queue多选无项提示，发动作后结束选择；数量提示不是DB成功回执。',
    '多选启动手势、并发重复入队、DB写入失败/回滚、实际安装默认未核；空队列导向Inbox只是有New记录时的界面分支。','E-APQD-04;E-APQD-05')
upd(3,'旧帮助Smart shuffle为尽量分散；3.12.3 Keep Sorted隐藏锁菜单，Random关闭Keep Sorted；选排序即存偏好并请求重排。锁定/Keep Sorted/全选等禁移动；拖动结束才请求写库；清队列先确认。',
    '当前播放项固定、排序/清空撤回、拖动取消和DB失败回滚未知；未把锁队列当防删除。当前帮助明确多队列不支持，不按标签造队列。','E-APQD-01;E-APQD-04;E-APQD-05')
upd(4,'自动下载数量仍有帮助50/XML25/Java回退20边界。Keep Updated关闭排除自动及Queue/Episodes批量刷新但可单独刷新；手动批量下载按传入顺序请求，跳过无媒体/已下载/本地源。',
    '帮助12小时刷新默认未核本版初始化；批量数量是请求资格计数，非下载完成。半文件、自动退避、实际传输并发/次序、断网恢复及初始默认仍未知。','E-APQD-03;E-APQD-05',task='准备离线节目：自动资格与手动批量')
upd(5,'保留既有文件失败仍可能清状态边界。批量删除对已下载/本地媒体调用删除，下载中未完成项调用取消；仅见本地文件警告helper入口。统计数在请求时累加，不能当实际删净。',
    '警告文案/确认/取消、单项撤销、存储失败可见提示、批量中途回滚仍缺；本地原文件无重下载保证，取消下载后残留处理未读。','E-APQD-05')
upd(10,'3.12.3失败项可请求重试；按钮先隐藏，找不到源/媒体只日志。此前列表同类型/ID有成功则隐藏重试。详情可复制URL/技术原因或整段；对象存在才可转播客。',
    '日志列表严格时间顺序、自动退避、服务内部许可/取消/重试、失败阻塞未知；提示或按钮消失不是成功。旧帮助长按URL与本版点击复制分开，未人为制造错误。','E-APQD-08;E-APQD-09')
dump(W/'old-coverage-updates.json',changes)
capture={'evidence_id':'E-APCD-13','expected_url':byE['E-APCD-13']['url'],
         'before_additional_captures':byE['E-APCD-13'].get('additional_captures',[]),
         'append_additional_captures':[{'checked_on':'2026-10-27','access_mode':'existing_related_source_local_reread',
             'related_source_url':local['url'],'raw_work_path':local['raw_work_path'],'raw_sha256':local['raw_sha256'],
             'verified_git_blob':local['verified_git_blob'],'locator':'SubscriptionFragment 128–230;246–300;335–510',
             'note':'原E已将SubscriptionFragment列为关联源码；本轮只复读标签/列表分支，未重新GET。',
             'hands_on':False,'visual_observed':False,'archived_artifact':common['archived_artifact'],'sha256':common['sha256']} ]}
dump(W/'old-evidence-additional-captures.json',[capture]);dump(W/'old-evidence-field-updates.json',[])

oldI=list(csv.DictReader((W/'inventory-before.csv').open(newline='')))
rows=[]
for r in oldI:
    x=r|{'read_basis':'prior_inventory_inherited','checked_on':'2026-10-27','current_detail':''}
    if r['inventory_id'] in ['H-04','H-44','H-47']:
        eid,flow,detail={'H-04':('E-APQD-03','F-M040-04','刷新周期与Keep Updated的排除范围；页面默认不当安装默认'),
                         'H-44':('E-APQD-01','F-M040-03','帮助正文明确不支持多队列；未据标题新造流程'),
                         'H-47':('E-APQD-02','F-M040-13','赋标签仅入口；不能创建标签队列，已有标签浏览另由源码支撑')}[r['inventory_id']]
        x.update(read_basis='official_help_body_read',coverage='body_read_with_version_boundary',evidence_ids=r['evidence_ids']+';'+eid,flow_ids=flow,current_detail=detail,gap='帮助未绑定3.12.3；边界详见本批摘记。')
    rows.append(x)
controls=[
 ('Queue','菜单锁定/取消警告','E-APQD-04','F-M040-03','290–360','Keep Sorted时锁菜单隐藏；警告可取消，保存不再显示偏好','锁不是删除保护；默认与实机未知'),
 ('Queue','清空队列确认','E-APQD-04','F-M040-03','299–312','确认后关闭表单并请求DB清队列','撤回/错误和媒体状态下游未读'),
 ('Queue','排序与随机例外','E-APQD-04','F-M040-03','579–625','选择回调存偏好/请求排序；Random禁Keep Sorted，当前弹窗换回可恢复','没有承诺退出可撤销'),
 ('Queue','移动资格与拖动写入','E-APQD-04','F-M040-03','636–728','锁/Keep Sorted/全选或端部连续项限制移动；拖动结束请求DB更新','过程取消/失败回滚未知'),
 ('Queue','限定队列搜索','E-APQD-04','F-M040-02','314–318','Search传QUEUED及全部订阅状态条件','搜索页面正文与结果点击未读'),
 ('Queue','空队列与Inbox入口','E-APQD-04','F-M040-02','527–552','New记录存在时空页给Inbox按钮；保存/恢复滚动位置另有条件','加载异常仅日志；不保证强退恢复'),
 ('Queue','多选动作结束','E-APQD-04','F-M040-02;F-M040-05','438–487;559–577','选择期间脱离滑动；无项提示；动作分派后退出多选','进入多选手势及adapter未读'),
 ('Batch','入队/出队','E-APQD-05','F-M040-02','76–93','入队筛有媒体且未标队列；出队按选择ID请求','并发去重和逐项成功未核'),
 ('Batch','下载资格与请求顺序','E-APQD-05','F-M040-04','140–150','有媒体、未下载、非本地；按传入列表逐项请求','不是网络传输实际完成顺序'),
 ('Batch','删除媒体或取消下载','E-APQD-05','F-M040-05','55–56;152–169','先委托本地警告；已下载/本地删，下载中的未完成项取消','本地警告文案/残留文件未读'),
 ('Batch','移出Inbox','E-APQD-05','','95–105','仅New项标Unplayed','不是删除文件；没有单独新任务'),
 ('Batch','标已播/未播','E-APQD-05','','107–137','改状态并条件性排同步动作，PLAY用全长位置','请求同步不等于远端已更新'),
 ('Batch','收藏状态','E-APQD-05','','171–194','增删收藏，数量按标签前态计','未核单项失败'),
 ('Batch','重置位置','E-APQD-05','','196–217','位置非零才处理；当前媒体清播放标记并请求Media3 stop/clear','停止实际反馈/撤销未运行'),
 ('Batch','分享与移至两端','E-APQD-05','','219–238','分享只取传入首项；移动委托DB请求','未触发外部分享；同向多项顺序下游未读'),
 ('Batch','数量提示','E-APQD-05','F-M040-02;F-M040-04;F-M040-05','240–249','此helper在numItems=1时不发汇总；其余按分支计数事件','不是逐项成功或原子事务回执'),
 ('Filter','选项/确认/重置','E-APQD-06','','36–77;101–122','选项变化callback；确认只dismiss；Reset空集并清按钮','抽象控件；实际入口/调用方保存未读'),
 ('Filter','查询条件组合','E-APQD-07','','23–102','维度AND；未播含New、Paused看position、Downloaded看日期字段','不是实时播放状态或物理存在性证明'),
 ('Filter','订阅状态回退','E-APQD-07','','72–91','三状态全部未选则Subscribed；全部选不加限制','不代表用户界面可同时选全部条件'),
 ('Subscriptions','选择现有标签','E-APCD-13','F-M040-13','SubscriptionFragment 207–229','点击标签重载；普通订阅读last_tag，归档从root开始','标签创建/编辑handler未读'),
 ('Subscriptions','保存选择和结果回退','E-APCD-13','F-M040-13','SubscriptionFragment 344–409','onPause存；过滤结果没当前标签则回root，数量下降结束多选','root仍是已过滤集合；强退/异常保存未知'),
 ('Subscriptions','标签条与筛选提示','E-APCD-13','F-M040-13','SubscriptionFragment 413–456;491–510','有自定义标签才显示；多选期间隐藏；筛选启用时有提示入口','SubscriptionsFilterDialog不是ItemFilterDialog；不混其回调'),
 ('Subscriptions','归档/批量菜单限制','E-APCD-13','F-M040-01','SubscriptionFragment 184–205','归档隐藏若干菜单；批量委托handler，选≤1才退出多选','handler没读，未补批量数据更新保证'),
 ('Download Log','成功与重试显隐','E-APQD-08','F-M040-10','70–133','成功隐藏；列表前项同类型/ID成功也隐藏失败项重试','未核列表按时间排序，旧失败不等于尚未恢复'),
 ('Download Log','分别请求刷新/下载','E-APQD-08','F-M040-10','98–119','先隐藏按钮，再取对象；为空仅日志，有值才分派','没有自动退避/许可/完成证明'),
 ('Download Log','错误详情复制及退出','E-APQD-09','F-M040-10','61–90;131–154','OK退出；整段或URL/技术原因点击复制','未执行剪贴板；旧帮助长按浏览器未核'),
 ('Download Log','原对象消失与跳转','E-APQD-09','F-M040-10','100–130;156–168','缺对象用日志标题，URL可unknown；有源才跳本机或在线播客页','加载错误只日志；不保证找回原媒体')
]
byid={r['evidence_id']:r for r in E}
for n,(domain,item,eid,flow,locator,detail,gap) in enumerate(controls,1):
    url=byid[eid]['url'] if eid in byid else local['url']
    rows.append({'inventory_id':f'C-{n:02}','kind':'targeted_source_branch','domain':domain,'item':item,
        'source_locator':locator,'source_url':url,'version_scope':f'3.12.3 source {C}; not runtime observation',
        'coverage':'static_branch_read_partial_not_whole_task','flow_ids':flow,'evidence_ids':eid,
        'gap':gap,'read_basis':'targeted_pinned_source_read','checked_on':'2026-10-27','current_detail':detail})
writecsv(Path(str(P)+'-inventory.csv'),rows,list(oldI[0])+['read_basis','checked_on','current_detail'])
summary={'new_e':9,'new_f':1,'old_f_updated':len(changes),'old_f_fields':sum(len(x['changed_fields']) for x in changes),
         'old_e_additional_captures':1,'sources_read':10,'help_articles_read':3,'source_files_read':7,
         'original_inventory':131,'help_index_entries':47,'setting_nodes':84,'new_branch_rows':len(controls),'inventory_rows':len(rows),
         'ordinary_http_success':9,'static_images_viewed':0,'hands_on':False}
dump(W/'summary.json',summary);print(json.dumps(summary,ensure_ascii=False))
