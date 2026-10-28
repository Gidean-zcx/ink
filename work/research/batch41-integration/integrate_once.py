import csv
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path('/home/workspace/reader')
LIB = ROOT / 'docs/research/interaction-library'
B = LIB / 'batches'
WORK = ROOT / 'work/research/batch41-integration'
IWORK = ROOT / 'work/research/batch41-inoreader-visuals'
GWORK = ROOT / 'work/research/batch41-ground'
IA = LIB / 'artifacts/inoreader-reading-state'
GA = LIB / 'artifacts/ground-source-scope'
DATE = '2026-10-27'
assert not (B / 'inoreader-reading-state-evidence.jsonl').exists(), 'one-time integration'
assert (WORK / 'before-manifest.json').exists()
assert hashlib.sha256((ROOT/'docs/requirements.md').read_bytes()).hexdigest() == 'e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def put(p, text):
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text.rstrip()+'\n')
def jp(p, x): put(p, json.dumps(x, ensure_ascii=False, indent=2))
def rows(p): return list(csv.DictReader(p.open()))
def cw(p, x):
    with p.open('w', newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(x[0]));w.writeheader();w.writerows(x)
def eid(n): return f'E-INRS-{n:02d}'
def fid(n): return f'F-M044-{n:02d}'

# Short researcher paraphrases; exact raw captures stay in work, not the permanent reading layer.
help_notes = [
 '列举五种文章布局，未给切换/保存步骤。',
 '可按源或日期分组，未示分组控件及生效范围。',
 '布局可分别作用于源、文件夹、标签；默认继承和重置未知。',
 'RSS由发布者提供全文、摘要或标题；缺正文不必然是下载失败。',
 'Saved聚合稍后读、归档、批注、网页和标签；阅读意图、保留和分类是不同对象。',
 '浏览或阅读时点Save保存；没有实际写入或重复保存结果。',
 'Read later位于Saved，承接留待阅读的文章。',
 'Continue reading声明记录阅读进度，未给精确位置和跨端恢复步骤。',
 'Preferences中可选邮件提醒；频率、静默和发送结果未说明。',
 '归档保留已读后仍想留存的文章，可移回稍后读；不证明系统强制读完。',
 '归档可恢复到稍后读或永久移除；未给确认、撤销和保留期限。',
 '外部网页可用Add web page或浏览器扩展保存。',
 '手机用分享菜单；目标缺席时帮助要求报告机型、系统与源App，未给自动修复。',
 '可设主题、字号和可选按钮；未给设置保存或默认值。',
 'Tab bar提供Account和Preferences入口；不是完整账户恢复流程。',
 '声明有账号和订阅导出选项，未界定完整备份范围或恢复兼容性。',
 '声明源、阅读进度和偏好自动跨端同步；冲突、离线顺序和粒度未知。',
 '声明Web/iOS/Android可用；应用商店链接未访问，构建未固定。',
 'Newsletter feed是接收邮件到账号的专用源。',
 'Add feed→Newsletter feed→命名和选地址；到发布者订阅或转发已有邮件。',
 '邮件源可用规则和过滤；没有实际自动处理或外发。',
 '取消关注后地址及历史邮件删除、停止接收且失去访问；与功能页90天保留口径未统一。',
 '未到信先核准确地址、仍在关注及发布者确认订阅；没有重投/恢复保证。',
 'TTS将文章转成音频；未生成或听音。',
 '打开文章→Listen with text-to-speech；没有加载、失败和完成实测。',
 '文章与播客可混合入同一队列；未测顺序和断点。',
 'Web/iOS/Android后台播放为厂商声明；不证明锁屏、强退或来电后恢复。',
 '批注包括高亮和笔记；没有真实数据写入。',
 '选择文本→Highlight text；未知选区跨段、取消和保存。',
 '选择高亮片段→Add note；与旧功能页高亮下Edit的入口措辞分别保留。',
 '文章底部Add note写整篇笔记；与段落笔记对象不同。',
 'Saved→Annotations回访；未测源文消失和离线恢复。',
 '帮助称笔记各套餐可用、高亮属Pro；没有账号核验。',
 '移动离线模式访问选定内容；不推及Web或所有音频。',
 '离线对象列源、文件夹、带标签文章、批注、已保存文章。',
 '区域属性启用离线；iOS右滑区域找Offline settings，Android长按；联网下载并查离线标识。',
 '联网后同步离线变更是文档声明；未给冲突合并规则。',
 '不能离线读时，分别核区域已启用与单篇已下载；联网完成下载再试。',
]
help_flows = [[9],[9],[9],[4],[3],[3],[3],[3],[3],[3],[3],[3],[3],[],[],[],[3,5],[],[17],[17],[17],[17],[17],[6],[6],[6],[6],[10],[10],[10],[10],[10],[10],[5],[5],[5],[5],[5]]
assert len(help_notes)==len(help_flows)==38
captured=json.loads((ROOT/'work/research/batch41-inoreader-help/root-results.json').read_text())
assert len(captured)==38
manifest=[]
for x,note,fs in zip(captured,help_notes,help_flows):
    assert sha(ROOT/x['raw_path'])==x['raw_sha256']
    assert sha(ROOT/x['body_path'])==x['body_sha256']
    y={k:v for k,v in x.items() if k not in ['links','images']}
    y.update(evidence_id=eid(x['order']),body_status='root_read_article_body',researcher_note=note,related_flow_ids=[fid(f) for f in fs],hands_on=False)
    manifest.append(y)

visuals=[]
observations={
 'redesign-image-08.png':('侧栏Feeds选中、文件夹未读数、文章列表逐行X。','没有实际点击；X的已读加隐藏语义来自正文，不从外形猜测。'),
 'redesign-image-09.png':('看板含文章卡片、33%设置清单、阅读统计及widget控件。','33%是账号/订阅设置进度，不是学习掌握；刷新/折叠结果未观察。'),
 'redesign-image-11.png':('过滤编辑器有Keep/Remove、条件组合、Whole words、Save/Cancel；提示与其他过滤器可能冲突及过去30天范围。','官方合成示意；2024历史页面与当前新文章说明不能直接合并；没有日志结果、取消或恢复操作。'),
 'redesign-image-13.png':('播客播放器显示19:06、剩余时间、1x、暂停及10字样圆箭头、混合队列和Clear all。','没有听音、拖动或清队列；图标的实际跳转与进度保存未知。'),
 'active-reading-image-08.png':('列表和正文两个保存书签状态：空心/黄色实心。','官方拼图，不是本轮点击前后；不证明同步和持久保存。'),
 'active-reading-image-09.png':('Read later有All/Continue reading、来源、预计长度、百分比进度、保存日期及Archive all。','All当前选中；未见Continue reading筛选结果。预计时长不是实际学习时间，进度不是理解程度。'),
 'active-reading-image-10.png':('4篇多选及批量工具栏；选择含0%进度；另有单行归档和Archive all。','选中批次与全部范围不同；没有归档后列表或撤销结果，不能断言强制读完才能归档。'),
 'active-reading-image-11.png':('Add web page弹窗含URL、Preview、缩略图/标题/描述和Save web page。','输入栏左端截断不是已证无效URL；背景旧列表不当本次保存成功；解析/重复/保存失败未观察。'),
 'active-reading-image-12.png':('文章顶部已有笔记带Edit/Delete；选中文字弹层有Highlight text/Add note。','已有笔记未证由当前选区生成；删除确认、撤销、同步及保存结果未知。'),
 'active-reading-image-13.png':('Spotlights列表有Color/Name/Created/Status、开关和菜单、Create入口；样例日期2025-06。','未见全部21项；未操作开关/编辑；彩色机制不能直接迁移黑白屏。'),
 'newsletter-blog-01.png':('Add feed的Newsletter feed选中；Name提示至少5字符，Email address带@ino.to；Create email address及Preferences > Content管理入口。','静态占位提示不等于实际校验；未见创建成功、重名或地址冲突、确认邮件。'),
 'newsletter-blog-02.png':('Newsletters文件夹下分源、未读数，列表与所选邮件正文并列；内容样例含促销购物。','不是上一图新建同源的成功结果；专用来源不自动保证高价值或免商业内容。'),
}
imeta=json.loads((IWORK/'image-downloads.json').read_text())
for name in ['newsletter-blog-01','newsletter-blog-02']:
    imeta.append(json.loads((IWORK/(name+'.meta.json')).read_text())|{'source_name':'newsletter-blog'})
for x in imeta:
    src=ROOT/x['path']; assert sha(src)==x['sha256']
    IA.mkdir(parents=True,exist_ok=True);shutil.copy2(src,IA/src.name)
    e={'redesign':39,'active-reading':40,'newsletter-blog':42}[x['source_name']]
    visible,unknown=observations[src.name]
    visuals.append(x|{'evidence_id':eid(e),'archived_artifact':str((IA/src.name).relative_to(LIB)), 'visual_observed':True,'observation':visible,'limits':unknown,'hands_on':False,'operation_method':'static_official_image_view_only'})
jp(IA/'visual-observation.json',{'checked_on':DATE,'images':visuals,'continuous_sequence':False,'product_operations':0})

bloginfo=[
 (39,'redesign','The new Inoreader experience is here!','2024-10-15','历史改版：X已读并隐藏、过滤日志路径、冲突/过去30天提示、播放器与看板。'),
 (40,'active-reading','Build active reading habits','2024-11-28','保存/阅读进度/归档范围、网页预览和批注控件；部分图片路径及画面为2025-06，确切修订未知。'),
 (41,'newsletters','Newsletter feeds',None,'专用地址、发布者确认及取消关注90天保留口径；与帮助删除说明未统一。'),
 (42,'newsletter-blog','A better way to keep up with newsletters','2026-08-14','建立邮件源的字段与阅读布局；没有取消关注保留语义。'),
]
for n,name,title,date,note in bloginfo:
    x=json.loads((IWORK/(name+'.meta.json')).read_text()); assert sha(ROOT/x['raw_path'])==x['sha256']
    manifest.append({'evidence_id':eid(n),'title':title,'url':x['url'],'http_status':x['status'],'checked_on':DATE,'source_published_on':date,'raw_path':x['raw_path'],'raw_sha256':x['sha256'],'body_path':str((IWORK/(name+'.txt')).relative_to(ROOT)),'body_status':'root_read_main_article','researcher_note':note,'hands_on':False,'canonical_url':x['url'].replace('us.inoreader.com/kr/','www.inoreader.com/').replace('us.inoreader.com/','www.inoreader.com/'),'app_build':None})
jp(IA/'source-manifest.json',manifest)
jp(IA/'access-review.json',{'checked_on':DATE,'help_requests':38,'help_http_200':38,'first_two_original_classification':'worker false-positive challenge/parser failure','correction':'Root inspected actual h1 and public_help_center_article_body in existing HTML. Global reCAPTCHA script is not a challenge. Reused first two captures without repeat GET; fetched remaining 36 once each.','original_report_work_path':'work/research/batch41-inoreader-help/worker-results-original.json','final_work_path':'work/research/batch41-inoreader-help/root-results.json','account_operations':0})

put(IA/'source-notes.md', '''# Inoreader：阅读状态、过滤与邮件源的生命周期

查阅2026-10-27。38篇帮助正文、3篇官方博客及1个功能页；来源、短摘记、原件散列见[source-manifest.json](source-manifest.json)。12张官方静态图逐幅查看，见[画面审计](visual-observation.json)。没有注册、订阅、保存文章、播放音频或执行过滤；D2文档重建，不能视作连续操作或当前构建实测。

## 阅读进度、归档与批量范围

E-INRS-05～13/17/40。Saved是容器，Read later表达稍后阅读意图，Archive表达移出待读列表并保留。帮助说归档可移回稍后读或永久移除，未说明确认/撤销。不能把永久移除与归档混为一谈。

[阅读列表](active-reading-image-09.png)同时显示预计长度、进度百分比和保存日期；这三个量不是实际花费时间、理解程度或掌握程度。All页签处于选中状态，图上有Continue reading入口，但没有点开后的条目、精确阅读锚点或跨端恢复结果。[归档图](active-reading-image-10.png)有4篇被选中，其中包含0%条目；批量工具栏、单行归档和Archive all为不同范围。只能确认画面里的操作入口，不能宣称已归档、强制读完或可以撤销。

[保存书签](active-reading-image-08.png)是官方拼图，不是本项目的操作前后。邮件提醒只明确可在Preferences选择，频率与停止条件没有充分说明。自动同步声明不等于刷机后全量恢复证据。

## 网页进入个人库的几个阶段

E-INRS-12/13/40。[添加网页](active-reading-image-11.png)呈现URL输入、预览内容及保存按钮。取得标题/缩略图并不表示完整正文已解析，背景已有列表也不证明本次保存成功。URL输入栏左端显示不完整，不能据此判为非法输入。网页付费墙、解析失败、重复导入和取消后数据去留未知。

手机使用源App分享菜单；缺少Inoreader目标时，帮助只要求提供设备、系统和源App信息给支持人员，不代表本项目已试过分享或修复。E-INRS-16的泛化导出说明不能证明正文、批注、进度、设置或账号可一起恢复。

## 过滤、隐藏和历史处理

E-INRS-39与已有E-INCD-04分别保留。2024改版正文给出Automate→Filters→点击Removed today数量查看移除记录的路径，但本轮未取得记录列表或恢复结果。不能继续写成“完全没有回看入口”，也不能升级为可撤销过滤。

[过滤编辑图](redesign-image-11.png)有Keep/Remove、条件组合、Whole words、Save/Cancel，以及规则冲突警告和过去30天处理提示。现有功能页强调新文章处理；两者可能分别涉及创建/编辑时回溯及后续新内容，也可能有版本变化，现有证据不能统一。不要承诺当前规则创建后必定重扫30天或从不影响历史。取消按钮存在不证明修改已回滚。

[文章列表](redesign-image-08.png)的X依据正文在Preferences→Settings→Article listing启用Mark as read and hide后使用。该动作把标已读与隐藏组合；不表示删除文章、停止来源或学会内容。误触撤销、当前移动版及跨端传播仍缺。

## 批注、看板与声音的可见边界

E-INRS-28～33/39/40。[批注图](active-reading-image-12.png)已有笔记带Edit/Delete，选区弹层另有Highlight text/Add note；不能把已有笔记拼成当前选区提交的结果。旧E-INCD-12写高亮下Edit，帮助E-INRS-30写选高亮后Add note，入口随来源分别保留。编辑、删除、确认、撤销和离线合并仍未操作。

[看板](redesign-image-09.png)的33%是开户、订阅、保存等设置清单，不能当学习成就或掌握进度。[Spotlights](active-reading-image-13.png)的颜色、名称、创建日期和开关是关键词突出管理，不证明排序或过滤；这些颜色在黑白屏的替代编码尚未研究。

[播放器](redesign-image-13.png)可见进度、剩余时间、速度、暂停、10字样圆箭头、队列及Clear all；正文说可混合文章和播客。没有听音或按键结果，清队列是否停止当前播放、跨端秒点与离线音频都未知。背景播放声明不能替代强退/重启/来电恢复。

## 邮件源：创建地址、确认订阅、收信和停止是不同阶段

E-INRS-19～23/41/42。路径为Add feed→Newsletter feed→填写名称、选择专用地址→到发布者使用该地址订阅，或转发已有邮件。功能页另说发布者确认邮件会作为源文章进入，确认后接收后续内容。[创建图](newsletter-blog-01.png)有名称至少5字符提示、地址的@ino.to后缀和Create email address，管理入口指向Preferences > Content。占位提示不是实际校验，尚未观察地址冲突、创建成功和确认邮件。

[收信布局](newsletter-blog-02.png)展示文件夹、分源、列表和选中邮件正文，不能接在创建图后当同一源的成功流程。样例含促销邮件，说明订阅工具本身并不保证本项目要求的高价值内容。多个发行者可用同地址，但停用该地址会影响其所有来信；个人原邮箱的另一份订阅也不会因此自动停止。

**保留语义尚未一致：**[功能页](https://www.inoreader.com/features/newsletters)称取消关注后停止收信，历史邮件保留90天，期间重新关注可恢复；[帮助](https://www.inoreader.com/help/pro/newsletter-feeds/what-happens-if-i-unfollow-a-newsletter-feed)称地址及邮件被删除、失去历史访问。可能涉及可见性与后台保留的区别，或文档版本不同；没有实际取消关注，不能断言立即物理删除，也不能向用户保证90天恢复。2026-08博客未解释此点。

未到信时文档分别排查准确地址、仍关注该源及发布者确认；没有丢信重投保证。Pro/20源等限制是查阅时厂商声明，不是账号实测或本项目套餐决定。

## 版本与复用

改版博客标2024-10-15；阅读习惯博客标2024-11-28，但部分图URL和画面日期为2025-06，不把所有图归为2024构建。邮件博客标2026-08-14，帮助多无更新日。两个镜像博客与规范地址视作同来源，不重复计算独立验证。

对G-001的研究建议是分开评价挑选内容、阅读投入、理解、保留与复习，不能拿已读或归档证明收益。对G-002的建议是让AI识别具体选区、操作对象、当前阶段及可证实结果；这些为后续原型问题，尚未决定系统数据结构或交互。
''')

grows=json.loads((GWORK/'results.json').read_text())['results']
gids=['335105','648513','485057','593857']
ginv={r['entry_id'].split('-A')[-1]:r for r in rows(B/'ground-controls-deepening-inventory.csv')}
gnotes=[
 'Web和移动浏览需联网；同一凭据可访问账户权益，不证明收藏全文缓存或阅读位置同步。',
 '套餐功能概述及旧比较图；仍写Vantage移动版即将推出，不能直接作为当前跨端权限保证。',
 '同事件对照、Blindspot与来源评级的机制概述；三机构措辞未解决旧方法论数量差异，无新增操作链。',
 '按来源原语言显示并可搜索非英语媒体；不证明中文界面、自动翻译或中文覆盖完整。',
]
gm=[]
for n,gid in enumerate(gids,1):
    x=next(x for x in grows if x['id']==gid)
    raw=GWORK/(gid+'.html');assert sha(raw)==x['sha256']
    gm.append(x|{'evidence_id':f'E-GNRS-{n:02d}','title':ginv[gid]['title'],'raw_path':str(raw.relative_to(ROOT)),'body_path':str((GWORK/(gid+'-body.txt')).relative_to(ROOT)),'body_sha256':sha(GWORK/(gid+'-body.txt')),'body_status':'root_read_article_main_text_excluding_common_support_footer','researcher_note':gnotes[n-1],'hands_on':False,'checked_on':DATE})
GA.mkdir(parents=True,exist_ok=True)
gx=json.loads((GWORK/'image-download.json').read_text());assert sha(ROOT/gx['path'])==gx['sha256']
shutil.copy2(ROOT/gx['path'],GA/'tiers-reference.png')
jp(GA/'visual-observation.json',{'checked_on':DATE,'evidence_id':'E-GNRS-02','image':gx|{'visual_observed':True,'archived_artifact':'artifacts/ground-source-scope/tiers-reference.png'},'observation':'Vantage/Premium对照表，所有权项与My News Bias范围不同；是套餐概览图，没有可操作控件或状态变化。','limits':'图片版本未知；正文仍含移动版即将推出的旧措辞，不能当当前权益实测；来源数量/无限额度是厂商宣称。','hands_on':False})
jp(GA/'source-manifest.json',gm)
put(GA/'source-notes.md','''# Ground News：联网、原语言和套餐文案的范围

2026-10-27读4篇官方帮助正文，各一次普通匿名GET均200；没有操作账号、收藏、订阅或产品新闻页。原址、可见更新日、散列与短摘见[source-manifest.json](source-manifest.json)。最初两篇被工具因全局CSP/reCAPTCHA字符串误标挑战，离线检查证实有正常文章正文，不是一次失败后绕过访问限制。

- **E-GNRS-01 / Where Can I Read**：正文2026-03-16更新，明确Web及移动浏览需网络，不支持离线浏览；与已有收藏需网络证据一致。账号权益可用同一凭据访问两端，不证明已保存全文、精确阅读位置或所有状态同步。不能把收藏等同下载。
- **E-GNRS-02 / Subscription Features**：正文2026-03-09更新，列Premium/Vantage权益，但仍说Vantage移动版即将推出，和已有移动使用帮助不能无条件拼接。[表图](tiers-reference.png)展示所有权、My News Bias等差别，见[观察记录](visual-observation.json)；图没有版本或操作结果。本文不作为当前套餐购买建议、实际额度或免费升级保证。
- **E-GNRS-03 / See News Differently**：正文2026-03-16更新，说明同事件多源对照与Blindspot机制，并称数据来自三家评级机构。此概述不消除此前案例中教程/方法论两种数量口径；不证明每条评级使用同样三家，更不能把来源评级当单篇事实核验。没有新按钮流程。
- **E-GNRS-04 / Non-English Articles**：正文2026-03-09更新，说明按原来源语言展示，并可查找非英语媒体。它不证明中文UI、自动全文翻译或中国新闻覆盖完整；应与已有移动翻译任务分开引用。

以上4篇是已知目录正文补读，未新增独立任务。帮助正文之后的通用支持/反馈说明不当成产品内报错；没有发送邮件或反馈。继续保留未读目录、账号操作、外链返回位置、实际取消/恢复和当前版本权限缺口。
''')

def evidence(e,product,title,url,raw,rawhash,art,note,date=None,visual=False,body=None):
    return {'evidence_id':e,'product_id':product,'title':title,'url':url,'checked_on':DATE,'source_type':'official_help_article' if e.startswith('E-GNRS') or int(e[-2:])<=38 else ('official_feature_page' if e.endswith('41') else 'official_blog_with_static_ui'),'access_mode':'anonymous_http_get_main_text_and_static_images' if visual else 'anonymous_http_get_main_text','version_scope':'平台及历史文档范围见摘记；未固定实际App构建，不是E-Ink4 Plus实测','app_build':None,'source_updated_on':date,'locator':note,'note':note+'；没有产品实际操作。','hands_on':False,'visual_observed':visual,'archived_artifact':str((art/'source-notes.md').relative_to(LIB)),'sha256':sha(art/'source-notes.md'),'supersedes':None,'raw_work_path':raw,'raw_sha256':rawhash,'parsed_text_path':body,'http_status':200}

ies=[]
for x in manifest:
    visual=int(x['evidence_id'][-2:]) in [39,40,42]
    e=evidence(x['evidence_id'],'M-044',x['title'],x['url'],x['raw_path'],x['raw_sha256'],IA,x['researcher_note'],x.get('source_published_on'),visual,x['body_path'])
    if 'canonical_url' in x:e['canonical_url']=x['canonical_url']
    es=[v for v in visuals if v['evidence_id']==e['evidence_id']]
    artifacts=[IA/'source-notes.md',IA/'source-manifest.json',IA/'access-review.json']
    if visual:
        e['visual_kind']='official_static_help_images';e['visual_audit_path']=str((IA/'visual-observation.json').relative_to(LIB));e['visual_audit_sha256']=sha(IA/'visual-observation.json')
        artifacts += [IA/'visual-observation.json',*[LIB/v['archived_artifact'] for v in es]]
    e['artifact_hashes']={str(p.relative_to(LIB)):sha(p) for p in artifacts};ies.append(e)
put(B/'inoreader-reading-state-evidence.jsonl','\n'.join(json.dumps(e,ensure_ascii=False) for e in ies))
ges=[]
for x in gm:
    visual=x['id']=='648513'
    e=evidence(x['evidence_id'],'M-046',x['title'],x['requested_url'],x['raw_path'],x['sha256'],GA,x['researcher_note'],x['source_updated_display'],visual,x['body_path'])
    artifacts=[GA/'source-notes.md',GA/'source-manifest.json']
    if visual:
        e['visual_kind']='official_static_help_images';e['visual_audit_path']='artifacts/ground-source-scope/visual-observation.json';e['visual_audit_sha256']=sha(GA/'visual-observation.json');artifacts += [GA/'visual-observation.json',GA/'tiers-reference.png']
    e['artifact_hashes']={str(p.relative_to(LIB)):sha(p) for p in artifacts};ges.append(e)
put(B/'ground-source-scope-evidence.jsonl','\n'.join(json.dumps(e,ensure_ascii=False) for e in ges))

# Update the same discovered directory entries, never count each short FAQ as a new task.
ip=B/'inoreader-controls-deepening-inventory.csv';ir=rows(ip);byid={r['entry_id']:r for r in ir}
for x,note,fs in zip(captured,help_notes,help_flows):
    r=byid[x['entry_id']];r['evidence_id']=';'.join(dict.fromkeys(r['evidence_id'].split(';')+[eid(x['order'])]));r['related_flow_ids']=';'.join(fid(f) for f in fs);r['review_status']='body_read_not_operated';r['gaps']=note+' 未实操，正文覆盖不等于完整交互。';r['checked_on']=DATE
cw(ip,ir)
gp=B/'ground-controls-deepening-inventory.csv';gr=rows(gp)
gflows=[[2,16],[16],[1,22],[6,11]]
for n,gid in enumerate(gids):
    r=next(r for r in gr if r['entry_id']=='M046-A'+gid)
    r['evidence_ids']+=';'+f'E-GNRS-{n+1:02d}';r['flow_ids']=';'.join(f'F-M046-{f:02d}' for f in gflows[n]);r['body_review_status']='已读正文；未操作产品';r['gap']=gnotes[n]
cw(gp,gr)

# Add evidence and precise branches to existing tasks; one new newsletter lifecycle task.
updates={
 'F-M044-01':([39],'2024文档给Removed today数量→移除记录入口；图含Keep/Remove、条件冲突及过去30天提示。','恢复操作、当前创建/编辑回溯范围、误去重和额度冲突未测；不能以旧图统一当前新文章说明。',True),
 'F-M044-03':([5,6,7,8,9,10,11,12,13,17,40],'Saved/Read later/Archive不同；图有预计时长、百分比、日期；0%条目也在多选归档图中；外部URL预览与Save分开。','精确阅读锚点、保存/归档完成、取消/永久移除/撤销、跨端冲突、网页解析失败及整库恢复未知。',True),
 'F-M044-04':([4],'RSS只含标题/摘要可能由发布者决定；不是每次缺正文都为下载故障。','真实全文解析失败、付费墙及重试结果未知。',False),
 'F-M044-05':([17,34,35,36,37,38],'iOS右滑区域→Offline settings，Android长按；启用区域后联网下载并核单篇离线标识；恢复网络后同步为声明。','离线开关不等于下载完成；中断/容量不足/失效/冲突、音频缓存和跨端精确恢复未知。',False),
 'F-M044-06':([24,25,26,27,39],'文章Listen入口；静态播放器有进度/倍速/暂停、10字样圆箭头与Clear all、混合队列；后台播放为文档声明。','没有实际听音、寻位、清队列、强退/来电/跨端断点及离线音频结果。',True),
 'F-M044-09':([1,2,3,39],'五布局、按源/日期分组、源/文件夹/标签分别设布局；历史可选X执行标已读并隐藏。','默认继承/重置、当前移动菜单、X撤销与跨端传播、未读过期对象及Magic sorting权重仍未知。',True),
 'F-M044-10':([28,29,30,31,32,33,40],'静态笔记有Edit/Delete，选区有Highlight text/Add note；整篇和段落笔记入口分开，旧Edit入口措辞保留。','已有笔记不证明来自当前选区；修改/删除结果、确认/撤销、离线合并和副本去留未观察。',True),
 'F-M044-12':([39],'静态看板含文章、设置清单33%和阅读统计；设置完成度不是学习掌握。','移动布局、空栏目、widget操作结果、保存和跨端冲突未知。',True),
 'F-M044-13':([40],'静态Spotlights列表有颜色、名称、创建日期、状态开关和菜单；不等同筛选/排序。','当前开启/编辑/保存、颜色重叠、黑白屏与读听表现未知。',True),
 'F-M044-16':([40],'官方归档图显示4项选择含0%条目，批量工具栏、单行归档、Archive all范围不同。','真实执行/撤销和永久删除、批量标已读恢复的明确控件仍未验证。',True),
}
changed=[]
for p in B.glob('*-coverage.csv'):
    rs=rows(p);dirty=False
    for r in rs:
        if r['flow_id'] in updates:
            ns,known,gaps,vis=updates[r['flow_id']]
            r['evidence_ids']=';'.join(dict.fromkeys(r['evidence_ids'].split(';')+[eid(n) for n in ns]));r['known_branches']+=' '+known;r['gaps']=gaps
            if vis:r['visual_status']='static_official_images_only'
            r['checked_on']=DATE;dirty=True;changed.append(r['flow_id'])
        if r['product_id']=='M-046':
            nums=[n+1 for n,fs in enumerate(gflows) if int(r['flow_id'][-2:]) in fs]
            if nums:
                r['evidence_ids']=';'.join(dict.fromkeys(r['evidence_ids'].split(';')+[f'E-GNRS-{n:02d}' for n in nums]))
                r['known_branches']+=' '+' '.join(gnotes[n-1] for n in nums)
                r['checked_on']=DATE;dirty=True;changed.append(r['flow_id'])
    if dirty:cw(p,rs)
assert len([f for f in changed if f.startswith('F-M044')])==10
cw(B/'inoreader-reading-state-coverage.csv',[{'flow_id':fid(17),'product_id':'M-044','task':'创建邮件订阅源、核对收信并停止接收','depth':'D2','evidence_ids':';'.join(eid(n) for n in [19,20,21,22,23,41,42]),'known_branches':'Add feed→Newsletter feed→名称/专用地址→到发布者订阅或转发→确认邮件→来信；静态字段和列表已看；缺信核地址/关注/确认。','gaps':'名称校验/地址冲突/创建成功/确认/重投未操作；取消后删除与90天保留两种口径未统一，不能承诺恢复。','visual_status':'static_official_images_only','hands_on':'false','case_path':'cases/M-044-inoreader.md','checked_on':DATE}])
jp(WORK/'changed-task-ids.json',{'updated':sorted(set(changed)),'added':[fid(17)],'new_evidence':[e['evidence_id'] for e in ies+ges],'static_images':len(visuals)+1})
print(json.dumps({'new_sources':len(ies)+len(ges),'new_tasks':1,'updated_tasks':len(set(changed)),'images':len(visuals)+1},ensure_ascii=False))
