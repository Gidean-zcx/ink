"""One-time scoped document assembly; not a product implementation."""
import csv
import hashlib
import json
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'work/boox-reading-controls-deepening'
LIB=ROOT/'docs/research/interaction-library'
ART=LIB/'artifacts/boox-reading-controls-deepening'
PREFIX=LIB/'batches/boox-reading-controls-deepening'
assert not ART.exists(), 'Do not rerun the assembler'
ART.mkdir()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,obj):p.write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n')
def csvwrite(p,rows,fields):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(rows)

index=json.loads((WORK/'article-index.json').read_text())
articles={x['article_id']:x for x in index}
ids=[10701497927316,10701489683092,10701488996500,10701486858388,25400769451540,10701491126548,10701454503572,15196391289108,10701570125588]
eid={a:f'E-BXRD-{i:02}' for i,a in enumerate(ids,1)}
eid[10701578837268]='E-BOOX-16'
oldE=json.loads((WORK/'evidence-before.json').read_text());oldF=json.loads((WORK/'coverage-before.json').read_text())
byE={x['evidence_id']:x for x in oldE};byF={x['flow_id']:x for x in oldF}
original=ROOT/'work/boox-controls-deepening/epaper-pc-articles.json'

notes='''# BOOX阅读与笔记：本批取证范围

2026-10-27。对象仍为Note Air5 C / Android 15 / V4.1.2功能基线，实际构建及专用手册未得。本轮读10篇既有官方ePaper Tablet PCs帮助正文，不刷新网站正文：原件为2026-10-26取得的work/boox-controls-deepening/epaper-pc-articles.json，SHA256 02e30182beee444963ce691bbec9523380dcf91000c32685e27bea33f08a2cd8。文章updated_at与edited_at是帮助元数据，不是设备固件发布日期。

9篇此前仅目录级的正文单独建E，均derived_from旧分类快照E-BXCD-01；Handwritten Notes沿用E-BOOX-16追加本地复读，旧E-BXCD-13保持历史引用。4张新附件普通匿名GET均200并实际view_image；图像机型/构建没有核明，不能认作Note Air5 C V4.1.2画面。没有实机、连续操作、点击结果、测时、账户、安装或同步动作。

## 各正文的有效范围

| 文章ID / 定位 | 本轮支持的任务或控件 | 不能据此推出 |
| --- | --- | --- |
| 10701497927316 / Library Modes、Layout Options、Recent Files | 扫描/目录两模式及图标；扫描模式菜单有书架/筛选排序；两模式切封面/列表入口不同；最近最多12文件。 | 切模式移动文件、自动扫描所有路径、书架与磁盘目录同一对象；Manage Library图示不能补成移动/删除/回收流程。 |
| 10701489683092 / 操作步骤 | NeoReader中央菜单→More→Settings/Global Settings；状态栏开关、项目选择与拖动排序。 | 预计剩余时间的计算、读完判定、模式退出或配置持久化。 |
| 10701488996500 / 操作步骤 | NeoReader中央菜单→菜单→Settings/Touch Settings，选择预设或自定义翻页区。 | 当前默认九宫格、冲突优先级或误触恢复。 |
| 10701486858388 / Theme、Zoom、Layout、OCR | 文中明确V4.1：PDF主题、固定版式专用Layout；双指缩放须启用，框选缩放是另入口。OCR列在线中英每日300页与离线30+语言无页限。 | 不把OCR宣传的无需等待/更高准确率当测量；离线包/额度耗尽/识别失败与回写未核，未建独立完整OCR任务。 |
| 25400769451540 / Basic、Advanced及子项 | 文中明确V4.1 Style；More Settings或再点主题细调，可复制设定到其他主题。静态图见Copy to、For this book only、Cancel/OK。 | 示例Bold选中不是默认；没有按OK或Cancel，不证明撤销或全库批量即时生效。 |
| 10701491126548 / Standard、Handwriting | 浮动栏General与Handwriting设置分开，可增删按钮/看说明；手写栏在手写状态出现。 | 与系统导航球混同、当前预设按钮全部适用本机、关闭后笔迹自动保存。 |
| 10701454503572 / 两组步骤 | Library右上统计入口；Settings/Library settings开关。正文明确统计仅此设备，不能跨设备同步。 | 阅读时间等于理解/完成；停用会清历史；阅读进度同步自动包含统计。 |
| 10701578837268 / Page Manager、Layers、Lasso、More等 | 页浏览与全屏页管理不同；图层正文称最多新增5层。Erase All Layers保留模板层；Page-明确不可撤销。当前套索有跨层Select All/Move to Layer；Notes链接可指向页并查历史。 | 当前新增套索/模板/页导入等全部已在V4.1.2；图层锁图标效果；把Page-不可撤销扩大到所有删除；套索跨层选择包含隐藏/锁定层与否。 |
| 15196391289108 / Text Notes工具 | 文中称V3.3.2引入。已有文本笔记可改样式/列表、查找替换、插链接/图/附件；More/Save；导出TXT/.note。录音固定在画布，可回放/删/导出分享。 | 新建首屏、导出目标/覆盖/取消、.note回导或录音与文字同步定位；其格式不同于手写PNG/PDF/.note。 |
| 10701570125588 / Add、Search、Management | 套索选笔迹→Tag→识别建议名/手输/最近标签→OK；Sidebar/Search查看本笔记标签并跳转；Notes列表Options/Tag Management管设备标签。 | 删标签同时删笔迹、跨本/云同步、识别错误回滚或无限标签数。 |

## 静态观察边界

- library-scan.jpg：菜单实际可读New Bookshelf、Manage Library、Filter and Sort、List Mode、Scan Book Covers、Library Settings、Sync Settings；这只是打开菜单的一帧。
- notes-page-manager.png：实际是Outline视图，12条示例条目、页号、三点菜单；未显示执行Copy/Move/Delete的结果。
- notes-layers.png：新增、眼睛、锁、垃圾桶和普通层/模板层可见；正文未说明锁行为，眼睛/垃圾桶也没有操作。
- epub-more-settings.png：Copy to对话框及上述选项；当前选择和数字值仅示例。

四图均为独立静态帮助图片，不组装成连续流程。完整来源、URL、定位、散列和实际查看记录在本批E及observed-images.json；功能台账保留旧59篇文章，并另列本轮正文标题/控件，父子项目不是新任务数量。
'''
(ART/'source-notes.md').write_text(notes)
notes_sha=sha(ART/'source-notes.md')
common={'product_id':'M-004','checked_on':'2026-10-27','app_build':None,'firmware_version':'V4.1.2 case baseline; actual installed build unknown','version_scope':'Note Air5 C case; generic ePaper Tablet PCs help is not model/build validation','hands_on':False,'supersedes':None}
evidence=[]
for article_id in ids:
    a=articles[article_id];raw=WORK/'raw'/f'{article_id}.json'
    evidence.append(common|{'evidence_id':eid[article_id],'title':a['title'],'url':a['url'],
        'source_type':'official_help','access_mode':'existing_local_public_api_article_body_read',
        'source_updated_on':a['source_updated_on'],'edited_at':a['edited_at'],
        'locator':f'Article {article_id}; full body text read; controls separately indexed',
        'note':'2026-10-26公开原件于2026-10-27本地阅读；无正文重新请求，不是该固件逐项验证。',
        'visual_observed':False,'archived_artifact':'artifacts/boox-reading-controls-deepening/source-notes.md','sha256':notes_sha,
        'raw_work_path':str(raw.relative_to(ROOT)),'raw_sha256':sha(raw),
        'raw_parent_path':str(original.relative_to(ROOT)),'raw_parent_sha256':sha(original),
        'derived_from':'E-BXCD-01','capture_state':'local_article_body_read_short_notes_archived'})
observed=[]
image_notes=[
    '扫描模式菜单可见New Bookshelf/Manage Library/Filter and Sort/List Mode等；未触发操作',
    'Outline示例列表与页码、三点菜单可见；没有复制移动删除结果',
    '普通层/模板层及新增、眼睛、锁、垃圾桶可见；锁行为正文未说明',
    'Copy to对话框有For this book only及Cancel/OK；示例Bold选中非默认，未确认或取消'
]
for i,(r,note) in enumerate(zip(json.loads((WORK/'image-fetch.json').read_text()),image_notes),10):
    assert r['status']==200
    raw=ROOT/r['raw_work_path'];out=ART/raw.name;shutil.copyfile(raw,out)
    row=common|{'evidence_id':f'E-BXRD-{i:02}','title':raw.name+' 官方帮助静态图','url':r['url'],
        'article_url':articles[r['article_id']]['url'],'source_type':'official_help_image',
        'access_mode':'anonymous_GET_then_view_image','source_updated_on':None,
        'locator':f'Article {r["article_id"]} image {r["image_number"]}', 'note':note,
        'visual_observed':True,'derived_from':eid[r['article_id']],
        'archived_artifact':str(out.relative_to(LIB)),'sha256':sha(out),
        'raw_work_path':r['raw_work_path'],'raw_sha256':r['raw_sha256'],
        'capture_state':'static_image_archived_actually_viewed','visual_audit_path':'artifacts/boox-reading-controls-deepening/observed-images.json'}
    evidence.append(row)
    observed.append({'evidence_id':row['evidence_id'],'article_id':r['article_id'],'image_number':r['image_number'],
        'observed_on':'2026-10-27','actual_view':'view_image','raw_work_path':r['raw_work_path'],
        'raw_sha256':r['raw_sha256'],'archived_artifact':row['archived_artifact'],'sha256':row['sha256'],
        'hands_on':False,'continuous_sequence':False,'model_build_unconfirmed':True,'note':note})
dump(ART/'observed-images.json',observed)
Path(str(PREFIX)+'-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in evidence))

fields=list(oldF[0])
flows=[]
def newflow(n,task,ev,known,gaps,visual='not_observed'):
    flows.append({'flow_id':f'F-M004-{n:02}','product_id':'M-004','task':task,'depth':'D2','evidence_ids':ev,
        'known_branches':known,'gaps':gaps,'visual_status':visual,'hands_on':'false','case_path':'cases/M-004-boox-neoreader.md','checked_on':'2026-10-27'})
newflow(15,'切换书库视图并找最近文件','E-BXRD-01;E-BXRD-10',
    'Library书本图标切Scan/Directory，分别按书架或文件路径查看；封面/列表入口因模式而异；时钟入口列最近最多12文件。',
    '当前通用帮助；新增书架名称/移动删除确认、扫描范围、重复书与回收规则未说明。菜单图不代表操作结果。','static_reference_only')
newflow(16,'按文档格式调整阅读版式与局部控制','E-BXRD-02;E-BXRD-03;E-BXRD-04;E-BXRD-05;E-BXRD-06;E-BXRD-13',
    'V4.1帮助区分固定版式Style/Layout与重排Style；主题再次点击或More Settings细调，重排可复制到其他主题；另有状态栏、翻页区和浮动栏设置。',
    '本机逐项布局/保存与恢复未知；Copy to图的选择非默认，未确认/取消；排版不证明修改源书，OCR额度/失败/回写未闭环。','static_reference_only')
newflow(17,'查看或停用本机阅读统计','E-BXRD-07',
    'Library右上Reading Statistics查看周阅读时间/书量；菜单Settings/Library settings开关Enable reading statistics；正文明确仅本设备且不同步。',
    '统计计算口径、后台/待机计时、停用是否清历史、恢复出厂后保留及导出未知；时间不等于理解或完成。')
newflow(18,'给笔迹加标签并回到标记内容','E-BXRD-09;E-BOOX-16',
    'Lasso选区→Tag→识别建议名/手输/最近标签→OK；Notes侧栏Search显示本笔记标签并跳转；列表Options/Tag Management增删设备标签。',
    '识别失败、改标签/删标签是否影响原笔迹、同名合并、跨本与云同步规则未核；通用帮助未证本机每控件。')
newflow(19,'编辑已有Text Notes并导出','E-BXRD-08',
    '已有文本笔记内Undo/Redo、样式/列表、查找替换和插入；More/Save手动保存，Export输出TXT/.note；录音固定画布并可回放/删/导出分享。',
    '新建入口、Exit保存时刻、导出路径/覆盖/取消、附件限制、.note回导和录音对齐未说明；不套手写PNG/PDF输出。')
csvwrite(Path(str(PREFIX)+'-coverage.csv'),flows,fields)

updates=[]
def update(fid,changes):
    before=byF[fid];after=before|changes
    updates.append({'flow_id':fid,'before':before,'after':after,'changed_fields':{k:{'before':before[k],'after':after[k]} for k in changes if before[k]!=after[k]}})
update('F-M004-09',{
    'evidence_ids':byF['F-M004-09']['evidence_ids']+';E-BXRD-11;E-BXRD-12',
    'known_branches':'Exit保存回列表；More/Save手动保存，Page-不可撤销。当前帮助补页管理侧栏/全屏区别、最多新增5图层、全层擦除保留模板、套索跨层选择及移层；不据此倒填全部4.1.2控件。',
    'gaps':'Home/切App/断电保存、.note回导、页/层/对象其他删除的确认与撤销未知；跨层选择是否含隐藏/锁定层、图层锁效果及新工具本机适用未核。',
    'visual_status':'static_reference_only','checked_on':'2026-10-27'})
update('F-M004-10',{
    'known_branches':'识别文本、墨迹与录音分开；More/Recorder标记与管理不等于笔画时间对齐。V4.1 Infinite Notes可双指拖画布、空白长按Pin并从三点改名/删；Smart Scribe帮助限部分机型。',
    'gaps':'Smart Scribe/新套索识别的本机适用、原位替换/撤回、录音笔画对齐、锁屏/来电中断、语言包离线条件仍未知。',
    'checked_on':'2026-10-27'})
dump(WORK/'old-coverage-updates.json',updates)
captures=[]
for old_id,locator,visual in [
    ('E-BOOX-16','Article 10701578837268 full body, especially Page Manager/Layers/Lasso/Insert/More; actual images 10 and 13 only',True),
    ('E-BXCD-01','Existing 59-article inventory reused; selected ten article bodies read locally; not recaptured category',False)]:
    cap={'checked_on':'2026-10-27','access_mode':'existing_local_public_help_capture_reread_no_body_network_request',
        'raw_work_path':str(original.relative_to(ROOT)),'raw_sha256':sha(original),'locator':locator,
        'archived_artifact':'artifacts/boox-reading-controls-deepening/source-notes.md','sha256':notes_sha,
        'hands_on':False,'visual_observed':visual,'continuous_sequence':False,
        'note':'原件采集2026-10-26；本地复读不是新版本或独立互证，help元数据不是固件发行日期。'}
    if visual:cap['visual_audit_path']='artifacts/boox-reading-controls-deepening/observed-images.json'
    captures.append({'evidence_id':old_id,'expected_url':byE[old_id]['url'],
        'before_additional_captures':byE[old_id].get('additional_captures',[]),'append_additional_captures':[cap]})
dump(WORK/'old-evidence-additional-captures.json',captures)
dump(WORK/'old-evidence-field-updates.json',[])

# Each subentry is a named source heading or explicit named control group.
# Parent rows preserve all 59 article IDs; they are not additional user tasks.
controls={
10701497927316:[
 ('Library Modes','书本图标切Scan/Directory；图标随模式变','切换不等于移动源文件'),
 ('1. Scan Mode','菜单新增书架、筛选/排序','命名/移动/删除全链未给'),
 ('2. Directory Mode','按文件目录路径显示','更名/移动后状态关联未知'),
 ('Layout Options','Scan菜单；Directory第二图标切Cover/List','默认/重启持久化未知'),
 ('Recent Files','Library时钟图标，最多12','清理历史/已删文件/过期条目未知')],
10701489683092:[('Enable / customize Status Bar','中央菜单/More/Settings/Global Settings；开关、拖顺序、选项目','预测时间算法与重启保留未知')],
10701488996500:[('Set up page-turn area','中央菜单/菜单/Settings/Touch Settings；预设/自定义','默认与冲突优先级未知')],
10701486858388:[
 ('PDF Reading Theme','V4.1 Original/Clear/Scrolling/Dual-Page/Customize','主题是阅读视图；非源文档重写证明'),
 ('Enhance Text/Image','滑条/输入值增粗文字或增强暗色','精度/残影未测'),
 ('Zoom','选模式或手动调整','恢复原倍率与锚点未知'),
 ('1. Zoom Mode','Style/选主题/Zoom Mode：Screen/Width/Original','Fit文案crop不推定裁掉原PDF'),
 ('2. Manual Zoom','浮动栏Gestures启用双指；另有Marquee框选','手势冲突/退出选区未知'),
 ('Layout','V4.1仅固定版式；Original/Extract/Comic/Article/Crop；再点细调','具体文档需试调；不能保证顺序正确'),
 ('PDF OCR','V4.1 Crop/Page；在线中英每日300页，离线30+语无页限；方向/页段','包/额度耗尽、取消、失败、结果写回未完整；不是新完成任务')],
25400769451540:[
 ('Basic Settings','V4.1 Style：主题/字号/字体/背景','不是本机实测'),
 ('Advanced Settings','More Settings或再点主题；复制设定到其他主题','Copy to静态Cancel/OK未触发'),
 ('Font','标题/正文/辅助字体；增粗/色增强/编码','字库缺失/内容乱码恢复未知'),
 ('Spacing','点图标或拖滑条调间距','参数范围/取消未知'),
 ('Margin','滑条或输入值','范围/溢出未知'),
 ('Contrast','图像锐化/水印淡化/暗色增强','效果与性能未测'),
 ('Others','汉字/缩进/对齐/方向/连字符/页显示','每项当前值及格式例外未核')],
10701491126548:[
 ('Standard Floating Toolbar','Settings/Floating Toolbar Settings/General；增删按钮和说明','关闭/复原不保证内容保存'),
 ('Handwriting Toolbar','手写状态栏；独立Handwriting Toolbar Settings','不把本栏当系统导航球')],
10701454503572:[
 ('Check Reading Statistics','Library右上统计；周时间/书量','仅本机不同步；计算口径未述'),
 ('Enable / disable Reading Statistics','菜单Settings/Library settings开关','关停不推定清空或保留历史')],
10701578837268:[
 ('Template Hub','联网/Notes模板图标；预览下载；Downloaded页','当前新内容；本机4.1.2和登录细节未核'),
 ('Top Toolbar','顶部工具容器','父标题不作独立完成任务'),
 ('Full Screen','全屏Note Toolbox；More/Customize Toolbar','文称V4.1增强；当前每按钮不倒填'),
 ('Exit','保存并回Notepad View','不覆盖Home/切App/断电'),
 ('Rename / Open','改笔记名或开另一本','重名/未保存确认未知'),
 ('Brushes','7种笔；再点当前笔设置/删；Pen压力选项','当前新增压力项不能当4.1.2已证'),
 ('Eraser','Pixel/Stroke/Lasso；自动回笔；当前层/全层擦除','全层不含模板；隐藏/锁定层范围未知'),
 ('Undo & Redo','后退/前进一步','历史长度/跨关本保留未知；不覆盖Page-'),
 ('Page Selector','切页，Next Page可快速加页','边界/新页模板规则未知'),
 ('Page Manager','侧栏浏览；全屏Copy/Move/Delete；Thumbnails/Outline/Tag','图是Outline示例；完整确认/撤销缺'),
 ('Sidebar','按住拖图标生成可贴边浮动栏','父容器，不等于所有工具已验证'),
 ('Templates','背景模板/设置','覆盖墨迹与自定义文件条件未知'),
 ('Layers','正文最多新增5层；显隐/删除/排序','图见锁但效果未说明；层删除恢复未知'),
 ('Margins','倍宽/倍高/四倍扩展或手调','与Infinite无限画布不同'),
 ('Shapes','颜色/线宽；长按选后调整','新形状和笔迹是否同对象未知'),
 ('Lasso','Smart/Freehand/Rectangle/All；自动回笔；剪贴/色/链/标签/翻转/移层/删/日程/转文','Select All称跨所有层；隐藏/锁定层、日程联动本机版本未核'),
 ('Page Layout','单页或Scroll向下扩展','当前帮助未给4.1.2条件'),
 ('Fill','绘形填色','封闭区域失败/撤销范围未知'),
 ('Gestures','手掌/手指防误触开关','手势冲突未测'),
 ('Sync','登录ONYX后同步','现有F13；本轮不重述授权流程'),
 ('Zoom','预设/框选/fit各项；Canvas=100%；滑条/右上窗平移','放大不改文件像素的效果未测'),
 ('Text Box','创建文本框输入/调整样式','对象退出/覆盖未知'),
 ('Insert','图裁剪/识别；附件≤20MB；文件/页/网站/笔记链接与时间戳','链接移文件后失效/附件复制关系未知'),
 ('AI Smart Scribe','形状一笔停1秒；波浪擦/圈选；Marker擦除例外3.5.1；语言包','部分型号；当前本机支持清单未给，不当实测时间'),
 ('Page+','新增位置/模板/来源；其他笔记/图/文件；PDF链接可点击','当前新入口版本、复制/关联关系未知'),
 ('More','Recorder/Search/Refresh/Page-/Save/Export/Recordings/Toolbar','Page-不可撤销只限该入口；PNG/PDF/.note回导未知'),
 ('FreeMark','控制中心/命名路径OK/标记/保存至Gallery','在NeoReader及Notes禁用；不纳本批原生编辑任务'),
 ('Infinite Notes','V4.1新建选项；双指拖动；空白长按Pin及三点改删','Pin不等于页锚，跨端/导出关系未知')],
15196391289108:[
 ('Undo & Redo','前/后一步','跨关本和动作覆盖未知'),
 ('Style','字体/字号/色/粗斜下划线','字体缺失处理未知'),
 ('Format','正文及多级标题','折叠/大纲跳转未明'),
 ('Spacing','行/字距/页边距','值域/溢出未知'),
 ('Align','左中右/缩进','混合选区结果未明'),
 ('List','待办/项目/编号样式','打勾不是学习完成验证'),
 ('Insert','网站链接/图片/附件','附件上限未在本篇说明'),
 ('Record','录音固定画布；回放/删/导出/分享','录音中断与文字时序未知'),
 ('Export','TXT/.note','路径/覆盖/回导未知；不套手写格式'),
 ('Search','关键词搜索或替换','全局范围/替换取消未知'),
 ('Sync','ONYX登录后同步','冲突/本地副本未知'),
 ('More','Save/Control/Clear Format/Customize Toolbar','Clear Format不是删除文字；保存结束回执未知')],
10701570125588:[
 ('Add Tags to Your Content','Lasso/Tag/识别名或输入/最近标签/OK','识别失败与重名合并未知'),
 ('Search for Tagged Content','Notes侧栏Search列本笔记标签；点标签定位','跨本搜索和位置变动未知'),
 ('Tag Management','Notes列表菜单Options/Tag Management；新增/删除','删除标签不证明删除原笔迹')]
}
article_flow={10701497927316:'F-M004-15',10701489683092:'F-M004-16',10701488996500:'F-M004-16',10701486858388:'F-M004-16',25400769451540:'F-M004-16',10701491126548:'F-M004-16',10701454503572:'F-M004-17',10701578837268:'F-M004-09;F-M004-10',15196391289108:'F-M004-19',10701570125588:'F-M004-18'}
old_inventory=json.loads((WORK/'inventory-before-parsed.json').read_text())
rows=[]
for old in old_inventory:
    a_id=int(old['article_id']);selected=a_id in controls
    row=old|{'item_id':'A-'+old['article_id'],'parent_item_id':'','entry_kind':'article',
        'locator':'Article '+old['article_id'],'body_read_on':'2026-10-27' if selected else '',
        'read_basis':'existing_local_article_body' if selected else 'prior_inventory_inherited',
        'control_scope':'完整文字已读；子控件逐行登记' if selected else '',
        'source_capture_on':'2026-10-26'}
    if selected:
        row.update(coverage='body_read_partial_task_or_control_scope',flow_ids=article_flow[a_id],evidence_ids=eid[a_id],
                   gap='通用帮助正文；没有本机V4.1.2逐项验证、连续操作或完整错误/恢复链。')
    rows.append(row)
    for j,(title,detail,gap) in enumerate(controls.get(a_id,[]),1):
        child=row|{'order':old['order']+'.'+str(j),'item_id':f'C-{a_id}-{j:02}','parent_item_id':'A-'+old['article_id'],
            'entry_kind':'body_heading_or_control_group','title':title,'locator':f'Article {a_id} / {title}',
            'control_scope':detail,'gap':gap,'coverage':'body_control_read_not_whole_task_complete'}
        if title in ('PDF OCR','Template Hub','Top Toolbar','Sidebar','FreeMark','Sync','Page Layout','Fill'):
            child['flow_ids']=''
        rows.append(child)
inventory_fields=list(old_inventory[0])+['item_id','parent_item_id','entry_kind','locator','body_read_on','read_basis','control_scope','source_capture_on']
csvwrite(Path(str(PREFIX)+'-inventory.csv'),rows,inventory_fields)
summary={'new_e':len(evidence),'new_f':len(flows),'old_f':len(updates),'old_f_fields':sum(len(x['changed_fields']) for x in updates),
    'old_e_captures':len(captures),'articles_read':len(index),'original_article_rows':59,'new_control_rows':len(rows)-59,
    'inventory_rows':len(rows),'actual_images':len(observed),'body_network_requests':0,'image_network_requests':4,'hands_on':False}
dump(WORK/'summary.json',summary)
print(json.dumps(summary,ensure_ascii=False))
