import csv,hashlib,json,re,shutil,difflib
from pathlib import Path
R=Path('/home/workspace/reader');W=R/'work/khan-library-controls-deepening';L=R/'docs/research/interaction-library';B=L/'batches';A=L/'artifacts/khan-library-controls-deepening';A.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,d):p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n')
def writecsv(p,rows,fields):
 with p.open('w',newline='') as f:
  out=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');out.writeheader();out.writerows(rows)
source=save
sm=json.loads((W/'source-map.json').read_text()); arts={x['id']:x for x in sm if x['kind']=='articles'}; olde=json.loads((W/'evidence-before.json').read_text());oldf=json.loads((W/'coverage-before.json').read_text()); em={x['evidence_id']:x for x in olde};fm={x['flow_id']:x for x in oldf}
for name in ['mute.png','look-at-my-map.png']:shutil.copyfile(W/name,A/name)
notes='''# Khan Academy Kids：帮助目录与操作补强

查阅日 2026-10-27。公开帮助正文由 web 工具正常读取，原返回 JSON 保存在 `work/khan-library-controls-deepening/`，不是直接取得的 HTML。旧 HTTP 403 文章路线未重试；未使用账户接口、登录或安装。2026 年帮助更新时间不等于 App 构建。已读 6 个栏目、8 个分页索引；目录不是全产品功能完成清单。

## 来源定位与短摘意

各原件路径、URL、SHA 和更新日期见 [本批 E](../../batches/khan-library-controls-deepening-evidence.jsonl)。`text/articles-ID.txt` 还保留 web 行号，定位不依赖 HTML 行号。

- E-KLCD-01：六栏目及家长/教师第二页，只有索引。家长、教师、IT/学区、多语言、公告、夏令营分开；无正文的标题不作为功能实现证明。原六栏目 JSON 为前轮成功公开取得的原件，本轮复读。
- E-KLCD-02，Learning Path，L23–54：绿色播放进入；按科目轮换，初始年级与后续表现影响难度；需要支持时给更基础活动。没有几次错误触发的阈值或退出状态。
- E-KLCD-03，背景音乐，L23：主页右下音符变成划线状态表示音乐静音；不证明朗读/提示音也关闭或设置跨端保存。
- E-KLCD-04/05，名字/头像，L25–31：成人区 My Account 选择档案编辑；没有保存按钮、冲突或撤销说明。
- E-KLCD-06，删档案，L25–30：成人区启用 Enable Delete 后点头像 X；明确清除该档案的进度和 Learning Path 历史。作品、撤销及远端删除时点未知。
- E-KLCD-07，中文家长指南，L23/27/39：文档中文版不是中文 App；称邮件账号用于跨设备/重装保留进度，但没有同步回执。点 Kodi 被译为重复活动说明，与旧英文帮助的下一句话表述不同；未据此断言上一句回放。
- E-KLCD-08，无声排错，L24–44：区分设备静音、音量和外部音频路由；旧侧键、旧 iOS 菜单和重置建议不能当所有现行机型的操作。未做声音测试或重置。
- E-KLCD-09，重装，L22–34：iOS 给旧式图标删除/商店重下，Android 只到卸载；不是完整账号恢复或课程重新下载说明。
- E-KLCD-10，教师报告，L37–74：Students→Class Reports 切两个报告；展开多次尝试；邮件导出 ZIP 内 HTML，须设备配置邮件。未发送，不证明可回导或所有家庭账号可用。
- E-KLCD-11，阅读流利度，L35–38：Reading 年级菜单找 Sight Words；Books 的 Early Readers 先听后自行读是成人安排的练习建议，未给即时切换或自动评分规则。
- 旧 E-KHK-01 新采集，Books 正文 L30–43/81：同书 Books 两种读法，Reading 加提问/点图；较高年级可含选择题。字母书可移动部分图形；错误反馈、暂停/续读未知。图书门类和教学建议不是全部题型规则。

## 本轮实际查看的两张静态图

仅用 view_image 查看下载的官方 PNG，没有操作 App 或播放声音。没有以静态图升级 D3。

1. `mute.png`，来自背景音乐帮助的附件 4420861003803：主页、大播放钮、Library、角色与右下角带斜线音符可见，官方蓝箭头指向该处。没有前后状态对照。
2. `look-at-my-map.png`，来自 Books 帮助附件 4409150302747：单页书图上可见 Home 图标、左右箭头及底部三角按钮。未标阅读模式，三角按钮作用、跳页/返回后状态不能从图确定。保留必要单页供控件核对，不复制整书。

改名帮助的附件 360007362331 在 web 工具打开超时；停止且未观察。旧 GIF 不重看、不下载；本批两张图片与前轮抽样帧独立。完整 URL、原件/图片散列及失败记录另见 work 中 `source-map.json`、`web-images-response.json` 和图片 capture.json。
'''
(A/'source-notes.md').write_text(notes); notehash=sha(A/'source-notes.md')
# New source IDs: one directory record and ten newly read help bodies.
config=[('02','360048828572','Learning Path入口与分级','2026-02-27','L23–54','F-M030-02'),('03','360006764152','背景音乐静音','2026-02-26','L23–25','F-M030-12'),('04','360006761932','编辑儿童名字','2026-02-27','L25–31','F-M030-09'),('05','360007053871','编辑儿童头像','2026-02-27','L25–31','F-M030-09'),('06','360006763332','删除儿童档案与进度','2026-02-27','L25–30','F-M030-09'),('07','4457349308059','中文家长指南的语言与提示范围','2026-02-23','L23/27/39/62–81','F-M030-02;F-M030-04;F-M030-10'),('08','360007053151','无声排错与系统条件','2026-02-27','L24–44','F-M030-12'),('09','360006761532','按平台重装的文档边界','2026-02-27','L22–34','F-M030-10'),('10','4403614100109','教师进度报告与邮件导出','2026-02-23','L37–74','F-M030-13'),('11','4403706778637','Sight Words与先听后读练习入口','2026-02-26','L35–38','F-M030-01')]
records=[]; maps={}
def common(eid,title,url,raw,loc,date=None):
 return {'evidence_id':eid,'product_id':'M-030','title':title,'url':url,'checked_on':'2026-10-27','source_type':'official_help','access_mode':'web_tool_public_page_text','version_scope':'Khan Academy Kids mobile help; iOS/Android build and region unspecified; family/teacher scope separated','app_build':None,'firmware_version':None,'source_updated_on':date,'locator':loc,'note':'仅文档重建；来源摘要与范围见独立摘记，未登录或运行App。','hands_on':False,'visual_observed':False,'archived_artifact':'artifacts/khan-library-controls-deepening/source-notes.md','sha256':notehash,'supersedes':None,'capture_state':'durable_short_notes_with_temporary_web_tool_return','raw_work_path':raw,'raw_sha256':sha(R/raw)}
e=common('E-KLCD-01','Khan Kids公开帮助六栏目及分页目录','https://khankids.zendesk.com/api/v2/help_center/en-us/sections.json?per_page=100','work/khan-controls-deepening/12-sections.json','6 sections; parent/teacher 2 pages each; others 1 page')
e.update(source_type='official_help_directory',access_mode='existing_public_directory_json_and_web_tool_section_text',note='112条栏目文章入口含重复列示；标题不是已读正文或完成任务。目录范围以本次8页可见列表为限。',additional_captures=[{'capture_url':x['url'],'checked_on':'2026-10-27','raw_work_path':x['source_response'],'raw_sha256':sha(R/x['source_response']),'locator':'visible article links; pagination read','hands_on':False,'visual_observed':False} for x in sm if x['kind']=='sections'])
records.append(e)
for num,aid,title,date,loc,flows in config:
 a=arts[aid];e=common('E-KLCD-'+num,title,a['url'],a['source_response'],loc,date);maps[aid]=(e['evidence_id'],flows)
 scopes={'03':'Khan Academy Kids Home screen; platform/build unspecified; background music only','04':'Khan Academy Kids Parent Section/My Account; platform/build unspecified','05':'Khan Academy Kids Parent Section/My Account; platform/build unspecified','06':'Khan Academy Kids Parent Section; deletion of a child profile, not entire parent account','07':'Chinese translation of Khan Academy Kids parent/home guide; app language claim is dated help, not UI observation','08':'Khan Academy Kids sound help; device-level checks and legacy iOS/side-switch examples, no verified current OS build','09':'Khan Academy Kids reinstall help: legacy iOS steps; Android text stops at uninstall','10':'Khan Academy Kids Class Account teacher view; reports/email; not family progress screen','11':'Khan Academy Kids Library; Reading grade 1/2 sight words and Books Early Readers; caregiver practice guidance'}
 if num in scopes:e['version_scope']=scopes[num]
 if num=='03':
  e.update(visual_observed=True,visual_kind='static_official_help_image',visual_audit_path='artifacts/khan-library-controls-deepening/source-notes.md',visual_audit_sha256=notehash,observed_on='2026-10-27',artifact_hashes={'artifacts/khan-library-controls-deepening/mute.png':sha(A/'mute.png')},visual_scope='One static screenshot; crossed-out music icon and official arrow; no action/audio observation')
 records.append(e)
(B/'khan-library-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in records))
fields=list(oldf[0]);newf=[dict(zip(fields,['F-M030-12','M-030','背景音乐静音与无声排错','D2','E-KLCD-03;E-KLCD-08','主页右下音符划线表示背景音乐静音；系统静音/音量/蓝牙路由另查','恢复音乐动作、持久化、朗读/提示音作用与现行OS适配未知','static_reference_only','false','cases/M-030-khan-kids.md','2026-10-27'])),dict(zip(fields,['F-M030-13','M-030','查看课堂多次尝试并导出进度报告','D2','E-KLCD-10','教师Students到Class Reports切报告；勾号/百分比展开尝试；Email导出ZIP内HTML需设备邮件配置','取消/发送失败、附件权限、格式字段、同步冲突和回导未知；不可套家庭账户','not_observed','false','cases/M-030-khan-kids.md','2026-10-27']))]
writecsv(B/'khan-library-controls-deepening-coverage.csv',newf,fields)
# Exact old-row proposals only.
changes={
'F-M030-01':{'evidence_ids':fm['F-M030-01']['evidence_ids']+';E-KLCD-11','known_branches':'Books两种读法；Reading含提问/点图及高年级选择题；Early Readers先听后读是成人练习建议','gaps':'中途切换、逐字点读、答案判定与提示、退出续接未知；静态三角按钮作用未核','visual_status':'static_reference_only'},
'F-M030-02':{'task':'路线级别与家长进度','evidence_ids':fm['F-M030-02']['evidence_ids']+';E-KLCD-02;E-KLCD-07','known_branches':'年级决定起点；表现影响主题推进及基础支持；Library勾号按档案/级别查看','gaps':'调级阈值、提示计分、失败重试与课内保存未知；中断另见F06；教师报告另见F13'},
'F-M030-04':{'evidence_ids':fm['F-M030-04']['evidence_ids']+';E-KLCD-07','known_branches':'Kodi求助与分步示范；中文指南称重复活动说明，旧英文称下一句','gaps':'两种帮助措辞未解；上一句回放、触发次数、示范后操作及结束页未知'},
'F-M030-09':{'task':'家长账户下新增、编辑及删除儿童档案','evidence_ids':fm['F-M030-09']['evidence_ids']+';E-KLCD-04;E-KLCD-05;E-KLCD-06','known_branches':'新增同旧；成人区My Account编辑名字/头像；Enable Delete后头像X删除，清进度及路线历史','gaps':'编辑保存回执、误删撤销、作品去留、账号整体删除及跨端冲突未知'},
'F-M030-10':{'evidence_ids':fm['F-M030-10']['evidence_ids']+';E-KLCD-07;E-KLCD-09','known_branches':'家庭/课堂/Clever退出不同；单设备重置后逐台登录；邮件账号保留进度是官方说明非恢复实测','gaps':'未同步数据、过期链接和作品恢复未知；重装页iOS旧菜单/Android仅卸载，不证明完整恢复'}
}
updates=[]
for fid,vals in changes.items():
 for field,after in vals.items():updates.append({'flow_id':fid,'source_batch':'docs/research/interaction-library/batches/'+('khan-controls-deepening-coverage.csv' if int(fid[-2:])>=7 else ('children-control-deepening-coverage.csv' if False else '')),'field':field,'before':fm[fid][field],'after':after,'reason':'新正文补原任务分支，保持D2；家长/教师/语言/构建范围分开'})
# Locate actual owner batches, never guess or modify them.
for u in updates:
 hits=[]
 for p in B.glob('*-coverage.csv'):
  if p.name=='khan-library-controls-deepening-coverage.csv':continue
  if any(r.get('flow_id')==u['flow_id'] for r in csv.DictReader(p.open())):hits.append(str(p.relative_to(R)))
 assert len(hits)==1,(u,hits);u['source_batch']=hits[0]
save(W/'existing-coverage-updates.json',updates)
a=arts['4409036780955'];old=em['E-KHK-01'];cap={'capture_url':old['url'],'checked_on':'2026-10-27','access_mode':'web_tool_public_page_text_and_static_image','source_updated_on':'2026-02-26','raw_work_path':a['source_response'],'raw_sha256':sha(R/a['source_response']),'locator':'Books article L30–43/81; one look-at-my-map image','archived_artifact':'artifacts/khan-library-controls-deepening/source-notes.md','sha256':notehash,'hands_on':False,'visual_observed':True,'visual_scope':'Single page image: Home, arrows, triangular control; reading mode and effects unknown','artifact_hashes':{'artifacts/khan-library-controls-deepening/look-at-my-map.png':sha(A/'look-at-my-map.png')}}
owner=[]
for p in B.glob('*-evidence.jsonl'):
 if p.name=='khan-library-controls-deepening-evidence.jsonl':continue
 if any(json.loads(s).get('evidence_id')=='E-KHK-01' for s in p.read_text().splitlines() if s.strip()):owner.append(str(p.relative_to(R)))
assert len(owner)==1
save(W/'existing-evidence-additional-captures.json',[{'evidence_id':'E-KHK-01','source_batch':owner[0],'field':'additional_captures','before':old.get('additional_captures',[]),'after':old.get('additional_captures',[])+[cap],'reason':'同URL取得完整正文并实际看单张静态图，保留原ID'}])
echanges={'checked_on':'2026-10-27','source_updated_on':'2026-02-26','visual_observed':True,'visual_kind':'static_official_help_image','visual_audit_path':'artifacts/khan-library-controls-deepening/source-notes.md','visual_audit_sha256':notehash,'observed_on':'2026-10-27'}
save(W/'existing-evidence-field-updates.json',[{'evidence_id':'E-KHK-01','source_batch':owner[0],'field':k,'before':old.get(k),'after':v,'reason':'实际查看一张官方书页，新增审计独立于可变案例；仍非实操'} for k,v in echanges.items() if old.get(k)!=v])
# Full visible section list: parent/teacher pagination is included; no unread title becomes a feature claim.
labels={
'360001481891':'无障碍声明|联系支持|产品欢迎|家庭开始指南|建账号|验证邮箱|家长资源表|可打印活动|每周计划|级别与进度|阅读学习路径|读前准备|自然拼读|阅读理解|流利阅读|家长控制|密码管理|网络错误|家庭阅读习惯|设备兼容|新增档案|西语打印活动|档案改名|西语日程|西语家庭学习建议|更换头像|删除档案|转换课堂账号|离线内容|背景音乐静音|卸载与重装|入学准备|重启设备|无声音|三年级以上',
'360008453772':'无障碍声明|开学五种用法|教师快速开始|网页教师面板|学生区域控制|课堂账号设置|学习主题|班级码|每日计划|分配课程|查看学生进度|创作工具|新学年账号准备|会议背景|Clever班级码|班级码登录|退出登录|关闭视频涂色|增加班级|学生学习级别|增删学生|按标准分配|网络连接问题|教师邮件系列|家庭合作识字|主站与Kids区别|学习路线|书籍说明|常见词视频|Clever教师|进度报告|教师工具|儿童目标设置|二年级书籍活动|课堂挑战',
'39182012411547':'无障碍声明|Clever学区指南|非Clever技术指南|学校产品|Clever接入|Chromebook兼容|管理员面板',
'46978742033051':'乌克兰语家长指南|西班牙语家长指南|中文家长指南|波兰语家长指南|葡语家长指南|西班牙语书籍|韩语家长指南|法语家长指南|亚美尼亚语家长指南|保加利亚语家长指南|土耳其语家长指南|捷克语家长指南|瑞典语家长指南|匈牙利语家长指南|阿塞拜疆语家长指南',
'46978591932315':'冬季学习|女性主题新书|新角色朋友|Library目录|作品导出|乐器与昆虫名称|播客推荐|角色介绍|识字研究新闻|一年级课程|歌曲活动',
'47204226038171':'2025夏令营报名|2025作品展|2025材料表|2025翅膀周|2025世界旅行周|2025友谊周|2025手工周|2024作品展|2023作品展'}
def norm(t):return re.sub(r'[^a-z0-9]+','',t.lower())
bodymap={norm(a['title'].replace(' – Khan Academy','')):(maps[aid][0],maps[aid][1],a['url'],'body_read_this_batch') for aid,a in arts.items() if aid in maps}
bodymap[norm(arts['4409036780955']['title'].replace(' – Khan Academy',''))]=('E-KHK-01','F-M030-01',arts['4409036780955']['url'],'body_read_this_batch')
oldtitle={'How does the learning level adjust and how do I view my child\'s progress?':('E-KHK-02','F-M030-02'),'How do I access parental controls?':('E-KHCD-04','F-M030-11'),'How do I set up, reset, or manage a password?':('E-KHCD-09','F-M030-10'),'How do I add a new user?':('E-KHCD-07','F-M030-09'),'Learn on the go with offline content in Khan Academy Kids':('E-KHK-04','F-M030-03'),'Parent and Home Account Users: Getting Started and creating an account with Khan Academy Kids':('E-KHD-03','F-M030-04;F-M030-05'),'✨ NEW: Manage student access to areas of the app':('E-KHCD-06','F-M030-11'),'Learning Topics: Using Khan Academy Kids in Educational Settings':('E-KHD-02','F-M030-06'),'How to use creative tools inside the Khan Kids app':('E-KHCD-01','F-M030-07'),'FAQ: How do I sign out?':('E-KHCD-08','F-M030-10'),'FAQ: How do I turn off videos and coloring ?':('E-KHCD-05','F-M030-11'),'Find books and lessons in the Khan Kids Library':('E-KHCD-03','F-M030-01;F-M030-02'),'How do I save artwork from the Gallery?':('E-KHCD-02','F-M030-08')}
for title,(eid,flows) in oldtitle.items():bodymap[norm(title)]=(eid,flows,em[eid]['url'],'previous_body_record_only')
inv=[];counts={k:0 for k in labels};fieldsinv=['inventory_id','product_id','node_type','function_domain','source_url','source_locator','evidence_ids','flow_ids','scope','body_status','visual_status','remaining_gap','checked_on']
for sec in [x for x in sm if x['kind']=='sections']:
 text=(R/sec['path']).read_text();entries=re.findall(r'^L(\d+):\s+\* 【(\d+)†([^】]+)】',text,re.M)
 for line,link,title in entries:
  if title in ['Next ›','Last »','« First','‹ Previous']:continue
  idx=counts[sec['id']];lbl=labels[sec['id']].split('|')[idx];counts[sec['id']]+=1
  eid,flows,url,status=bodymap.get(norm(title),('','','','index_only'))
  if title=="Sandy's Instruments and Reya's Bugs":status='body_read_names_only_no_operation_claim'
  inv.append(dict(zip(fieldsinv,[f'KLC-I{len(inv)+1:03d}','M-030','help_article_index',lbl,sec['url'],f'web L{line}, link {link}'+('; '+url if url else ''),'E-KLCD-01'+(';'+eid if eid else ''),flows,sec['title'].replace(' – Khan Academy',''),status,'not_observed','标题不是操作完成；未读项入口/分支待核' if not eid else '见对应F及来源，未做App操作','2026-10-27'])))
for k,n in counts.items():assert n==len(labels[k].split('|')),(k,n)
# Additional content/control nodes, not extra tasks.
for domain,eids,flows,scope,loc,gap,visual in [
('Learning Path起点与动态支持','E-KLCD-02','F-M030-02','家庭/课堂移动帮助','L23–54','错误次数与独立答对计分未知','not_observed'),
('Books朗读/自主读','E-KHK-01','F-M030-01','移动Books','L41','音频暂停/同页切换未知','static_reference_only'),
('Reading提问/点图/高年级选择题','E-KHK-01','F-M030-01','移动Reading','L42–43','答案判定与提示重试未知','not_observed'),
('字母书可移动图片','E-KHK-01','F-M030-01','移动Books字母书','L81','只描述部分图片，非通用可拖拽规则','not_observed'),
('Sight Words年级筛选','E-KLCD-11','F-M030-01','Reading一年级/二年级','L35','非本家庭英语水平判定','not_observed'),
('Early Readers先听后读','E-KLCD-11','F-M030-01','Books','L37–38','成人安排，不证明自动测评/即时切换','not_observed'),
('字母及CVC教学目录','E-KLCD-07','F-M030-02','Letters','L62','仅课程分类无具体判错链','not_observed'),
('计数比较分组与运算目录','E-KLCD-07','F-M030-02','Math','L66','仅分类无具体题型控件','not_observed'),
('阅读语言与词法目录','E-KLCD-07','F-M030-02','Reading','L69','英文课程，不等于中文自主阅读','not_observed'),
('记忆分类与社交情感目录','E-KLCD-07','F-M030-02','Logic+','L73','难度/错误提示/结算未知','not_observed'),
('Kodi活动说明','E-KLCD-07','F-M030-04','中文家长帮助；构建未知','L39','与英文下一句措辞不同，上一句回放未核','not_observed'),
('名字和头像编辑','E-KLCD-04;E-KLCD-05','F-M030-09','家长My Account','L25–31','保存/取消反馈未知','not_observed'),
('Enable Delete及头像X','E-KLCD-06','F-M030-09','家长My Account','L25–30','误删恢复、作品与跨端时序未知','not_observed'),
('音乐划线图标','E-KLCD-03','F-M030-12','Home','L23及单张PNG','其他音轨与设置持久化未知','static_reference_only'),
('系统无声排错','E-KLCD-08','F-M030-12','设备/部分旧iOS说明','L24–44','现行机型适用性与修复结果未测','not_observed'),
('重装与进度保留声明','E-KLCD-07;E-KLCD-09','F-M030-10','iOS/Android不同说明','指南L27；重装L22–34','同步回执和未上传数据恢复未知','not_observed'),
('Assignments与All Progress','E-KLCD-10','F-M030-13','教师账户','L37–65','不是家长勾号同一界面','not_observed'),
('报告邮件ZIP/HTML','E-KLCD-10','F-M030-13','教师账户及设备邮件配置','L67–74','取消失败权限与回导未知','not_observed')]:
 inv.append(dict(zip(fieldsinv,[f'KLC-C{len(inv)-112+1:02d}','M-030','documented_function_or_control',domain,'',loc,eids,flows,scope,'body_read_this_batch',visual,gap,'2026-10-27'])))
writecsv(B/'khan-library-controls-deepening-inventory.csv',inv,fieldsinv)
save(W/'directory-counts.json',{'section_pages':8,'section_count':6,'article_entries':sum(counts.values()),'section_entry_counts':counts,'control_rows':18,'total_rows':len(inv),'note':'article entries include repeated listing, not distinct product feature count'})
print(json.dumps({'new_E':len(records),'new_F':len(newf),'old_F_fields':len(updates),'old_E_capture_fields':1,'old_E_metadata_fields':len(echanges),'inventory_rows':len(inv),'note_sha256':notehash},indent=2))
