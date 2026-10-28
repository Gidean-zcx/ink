from pathlib import Path
from hashlib import sha256
from collections import Counter
import json,csv,io,re,shutil

ROOT=Path('/home/workspace/reader');W=ROOT/'work/hisense-controls-deepening';L=ROOT/'docs/research/interaction-library';A=L/'artifacts/hisense-controls-deepening';A.mkdir(exist_ok=True)
C=L/'cases/M-055-hisense.md'
def h(p):return sha256(p.read_bytes()).hexdigest()
def dump(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def rel(p):return str(p.relative_to(ROOT))
assert h(C)==json.loads((W/'baseline.json').read_text())['case_sha256'],'One-time script; do not rerun'
OE={e['evidence_id']:e for e in json.loads((W/'evidence-before.json').read_text())};OF={f['flow_id']:f for f in json.loads((W/'coverage-before.json').read_text())}
caps={x['key']:x for x in json.loads((W/'captures-corrected.json').read_text())};imgs=json.loads((W/'image-captures.json').read_text())
assert len(imgs)==5 and all(x['status']==200 for x in imgs)
for x in imgs:shutil.copyfile(ROOT/x['raw_work_path'],A/Path(x['raw_work_path']).name)

notes='''# M055 海信：来源与五张静态照片观察

2026-10-27；hands_on=false，无连续观察。旧型号手册仍未取得。下列机内文字来自作者设备照片，属于厂商界面保存件，但不是官方托管手册、固定固件证明或本项目操作。照片没有页码；日期/时钟不当作操作时间或耗时。

## E-HSCD-01—03：Hi Reader 2023开箱中的三张图

来自E-HSD-09原文“进入系统/设置界面”部分；HTML中的图片地址正常GET取得。文章后段TOUCH Lite/A9对比排除。实际查看：hireader-key-guide.webp、hireader-screen-guide.webp、hireader-settings.webp，均600×800。文件名日期不当固件版本。

01的墨智键说明限制应用快捷启动为非锁屏，并给设置入口；速记可按已配置触发方式启动，不将所有按法都当默认。02的面板是引导页中的示意，模式作用文字为当前应用；流畅深色只是示例，未观察切换。该图把截屏便笺与收藏文章并列。03只有12个设置入口与底部标签，未点进任何子页；全局收藏管理入口不证明旧收藏内容已恢复。

## E-HSCD-04/05：Hi Reader 2022编辑部照片

来自E-HSD-05的速记和选词段。实际查看hireader-note.jpeg（1182×792）与hireader-selection.jpeg（1187×792）。04有语音输入提示、00:01、方形停止符号及垃圾桶/分享/软盘形图标；不能据图标证明删除、导出或保存结果，00:01不是本项目计时。05可见选区、六个菜单和一个词典结果，来源标汉语大词典；不证明离线或所有文本都可查询。未转录照片中的文学正文。

2022评测与2023开箱的构建均未知；不能把二者拼成同一连续操作。原文章阅读/收藏/速记文字仍沿用E-HSD-05，未另立重复整篇来源。

## E-HSCD-06：TOUCH联名款2021作者文字

作者“马马马不前”自述购买西安博物院联名款；与旧官方黛青SKU分别保留，非TOUCH Lite。只读七节目录和系统段：作者删过阅读标签、称应用/设置不可删；音乐标签到播放器，三键导航在应用内出现。没有后台终止或数据删除证据。普通GET仅得到空JS壳；web工具公开正文可读并自动带opus_fallback参数，未自行构造接口、执行JS、登录或观看图片/视频。不采性能、健康或任意App兼容概括。

## 旧证据复读与失败范围

E-HSD-05/08/09正常GET正文200并保存原件；08按响应的GB2312用GB18030正确提取，初次UTF-8乱码文件留作中间件，不作依据。E-HSD-03的web文字仍未列旧三款下载，独立原址GET超时后停止。官网FAQ所列七问属于记录仪，本轮未把它当阅读器帮助；仅访问范围记录，无新增产品流程E。

未重试旧论坛访问限制、商城403详情图、TOUCH旧文章和A9转载的超时路线。没有取得电话/短信打断、后台保活、锁屏音乐恢复、系统备份或恢复出厂的固定型号帮助。
'''
(A/'source-notes.md').write_text(notes);nr='artifacts/hisense-controls-deepening/source-notes.md';nh=h(A/'source-notes.md')
E=[]
labels=['墨智键机内引导','水墨屏控制机内引导','设置入口照片','灵感速记静态界面','选词与词典结果静态界面']
for n,(x,title) in enumerate(zip(imgs,labels),1):
    name=Path(x['raw_work_path']).name
    E.append({'evidence_id':f'E-HSCD-{n:02d}','product_id':'M-055','title':'Hi Reader原版：'+title,
      'url':x['url'],'checked_on':'2026-10-27','source_type':'firsthand_photo_of_device_guide' if n<=2 else 'firsthand_device_photo',
      'access_mode':'anonymous_image_GET_and_static_image_viewed','version_scope':'Hi Reader original; '+('2023 DealiAxy unboxing section' if n<=3 else '2022-04-06 IT Home review')+'; firmware/build unknown; not TOUCH Lite/Pro/2026',
      'app_build':None,'firmware_version':None,'source_updated_on':None,'locator':name+'; no printed/PDF page number',
      'note':'只观察这一张静态照片，非操作序列；机型归属来自父文章对应段。固件及默认值未证。',
      'hands_on':False,'visual_observed':True,'supersedes':None,'derived_from':x['derived_from'],
      'raw_work_path':x['raw_work_path'],'raw_sha256':x['raw_sha256'],'archived_artifact':'artifacts/hisense-controls-deepening/'+name,'sha256':x['raw_sha256'],
      'visual_audit_path':nr,'visual_audit_sha256':nh,'observed_on':'2026-10-27','capture_state':'selected_static_photo_and_independent_audit_archived'})
webraw=W/'raw/touch-web-result.json'
E.append({'evidence_id':'E-HSCD-06','product_id':'M-055','title':'TOUCH西安博物院联名款：2021作者系统导航记录',
 'url':'https://www.bilibili.com/read/cv13411020/','checked_on':'2026-10-27','source_type':'first_person_user_report',
 'access_mode':'public_web_tool_text_read; anonymous_HTML_GET_returned_JS_shell',
 'version_scope':'Hisense TOUCH original, Xi’an Museum edition, 2021 author unit; Android11 self-report; firmware unknown; not TOUCH Lite or exact official dark-green SKU',
 'app_build':None,'firmware_version':None,'source_updated_on':'2021-10-01','locator':'七节文章目录；第四节配置及系统体验；web正文L133–162',
 'note':'作者自述购买使用；只取导航/标签及可见入口描述，D1。未观察文章图片/视频，不采用性能和兼容性概括。',
 'hands_on':False,'visual_observed':False,'supersedes':None,'raw_work_path':rel(webraw),'raw_sha256':h(webraw),
 'raw_capture_kind':'JSON serialization of exact web-tool text result, not original HTML',
 'html_shell_work_path':caps['touch-bilibili']['raw_work_path'],'html_shell_sha256':caps['touch-bilibili']['raw_sha256'],
 'tool_final_url':'https://www.bilibili.com/read/cv13411020/?opus_fallback=1',
 'archived_artifact':nr,'sha256':nh,'capture_state':'public_tool_text_and_short_note_archived'})
(L/'batches/hisense-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))

# Ordinary same-URL captures stay attached to old evidence.
adds=[]
for eid,key in [('E-HSD-05','hireader-ithome'),('E-HSD-08','a9-yesky'),('E-HSD-09','hireader-deali')]:
    x=caps[key];assert x['url']==OE[eid]['url'] and x['status']==200
    cp={k:v for k,v in x.items() if k!='key'};cp.update(hands_on=False,visual_observed=False,archived_artifact=nr,sha256=nh,note='同文章复读；选定照片另以衍生E记录，不重复整篇来源。')
    adds.append({'evidence_id':eid,'expected_url':OE[eid]['url'],'before_additional_captures':OE[eid].get('additional_captures',[]),'append_additional_captures':[cp]})
support={'checked_on':'2026-10-27','url':OE['E-HSD-03']['url'],'access_mode':'public_web_tool_text_read_then_separate_urllib_timeout',
 'locator':'资料下载标题下至页脚；web返回正文未列旧三款下载','hands_on':False,'visual_observed':False,
 'raw_work_path':None,'raw_sha256':None,'archived_artifact':nr,'sha256':nh,
 'note':'未保存完整web原文；独立GET超时，不冒充HTML原件。旧型号手册仍未取得；未重试。'}
adds.append({'evidence_id':'E-HSD-03','expected_url':OE['E-HSD-03']['url'],'before_additional_captures':OE['E-HSD-03'].get('additional_captures',[]),'append_additional_captures':[support]})
dump(W/'old-evidence-additional-captures.json',adds)
dump(W/'old-evidence-field-updates.json',[])

patches={
 'F-M055-01':{'known_branches':'A9原版官方容量/阅读手机定位沿用旧采集；2022评测仅列四刷新模式、安装App与多种锁屏主题，未给主链操作。','gaps':'导入开书、查词划线、音频/通话打断、锁屏恢复、后台与同步恢复仍缺固定构建证据；不借Hi Reader机内引导补A9。'},
 'F-M055-05':{'evidence_ids':'E-HSD-01;E-HSD-06;E-HSCD-06','known_branches':'普通TOUCH与2021西安博物院联名款分开：新作者称删阅读标签，应用/设置标签保留，音乐标签进入播放器；三键导航在具体App内出现。只是入口与局部观察。','gaps':'联名款与黛青SKU构建关系、编辑保存/重加标签、后台管理结果、USB入库/队列/断点、输出切换与DAC退出未知；删标签不证明卸载或数据删除。'},
 'F-M055-06':{'evidence_ids':'E-HSD-05;E-HSCD-01;E-HSCD-04','known_branches':'2022评测给长按录入、松开结束、短按收起保存；2023机内引导另说明按已配置按法启动后可新建/查看/删除。速记照片有录入状态和操作图标，未触发。','gaps':'两年构建不拼同一操作链；转写网络/账号/失败、原音与文字、查看/删除具体步骤及恢复、分享/软盘图标效果未核。','visual_status':'static_reference_only'},
 'F-M055-07':{'evidence_ids':'E-HSD-05;E-HSCD-02;E-HSCD-03','known_branches':'2022评测为支持的App双指长按识屏后收藏、声称存本地；2023设置照片可见全局收藏管理，控制引导另列收藏文章。截图便笺是另一入口。','gaps':'管理入口未点击，未观察收藏项重开/删除/恢复；App资格、离线首次抓取、图片完整性、重复及同步仍未知；不拼两构建。','visual_status':'static_reference_only'},
 'F-M055-08':{'evidence_ids':'E-HSD-05;E-HSCD-05','known_branches':'2022作者长按文字调工具栏；照片可见选区、划线/批注/复制/搜索/词典/百科，所示词典结果注明汉语大词典。','gaps':'单帧不闭合选词→保存→重开；词典切换/离线、查询失败、标注取消/导出/删除及返回位置未知。','visual_status':'static_reference_only'}
}
updates=[]
for fid,p in patches.items():
    b=OF[fid];a={**b,**p,'checked_on':'2026-10-27'}
    updates.append({'flow_id':fid,'before':b,'after':a,'changed_fields':{k:{'before':b[k],'after':v} for k,v in a.items() if v!=b[k]}})
dump(W/'old-coverage-updates.json',updates)

new_f=[
 {'flow_id':'F-M055-09','product_id':'M-055','task':'Hi Reader非锁屏时按键启动指定应用','depth':'D2','evidence_ids':'E-HSCD-01;E-HSCD-03',
  'known_branches':'2023作者拍到机内指南：设置→墨智键→短按/双击墨智键可重设启动应用；非锁屏时用对应按法启动所设App。引导页另有查看全部功能入口。',
  'gaps':'候选App列表、选择/保存确认、默认映射、启动失败、切回原书、锁屏是否有其他动作均未核；未实际按键。','visual_status':'static_reference_only','hands_on':'false','case_path':'cases/M-055-hisense.md','checked_on':'2026-10-27'},
 {'flow_id':'F-M055-10','product_id':'M-055','task':'Hi Reader从当前界面生成截屏便笺','depth':'D2','evidence_ids':'E-HSCD-02;E-HSCD-03',
  'known_branches':'机内指南将截屏便笺放在下拉快捷开关：点它截取当前界面，并在水墨屏截屏便笺中出现。另一照片有截屏便笺管理入口，未点击。',
  'gaps':'格式、文件位置、识字/编辑、管理项重开/导出/删除、隐私内容限制、失败/重复/重启保持未知；不能等同文章收藏或语音速记。','visual_status':'static_reference_only','hands_on':'false','case_path':'cases/M-055-hisense.md','checked_on':'2026-10-27'}
]
with (L/'batches/hisense-controls-deepening-coverage.csv').open('w',newline='') as f:
    wr=csv.DictWriter(f,fieldnames=list(next(iter(OF.values()))),lineterminator='\n');wr.writeheader();wr.writerows(new_f)

ALL={**OE,**{e['evidence_id']:e for e in E}};rows=[]
def inv(family,level,item,eid,loc,flows='',status='source_body_read',gap='',parent='',scope=None):
    e=ALL[eid];r={'inventory_id':f'I-HSCD-{len(rows)+1:03d}','source_family':family,'parent_id':parent,'level':level,'item':item,'locator':loc,'source_url':e['url'],'evidence_ids':eid,'flow_ids':flows,'coverage_status':status,'model_version_scope':scope or e['version_scope'],'gap':gap};rows.append(r);return r['inventory_id']

fam='A9旧范围与2022评测目录'
for item,eid,flow,status in [('官方SKU/阅读手机定位','E-HIS-01','F-M055-01','previous_capture_only'),('2023导航反馈','E-HIS-02','F-M055-02','previous_capture_only_access_restricted'),('漂白搜索入口','E-HSD-07','F-M055-03','previous_capture_only_timeout'),('条件式锁屏阅读','E-HSD-07','F-M055-04','previous_capture_only_timeout')]:
    inv(fam,1,item,eid,'旧E定位；本轮未重访',flow,status,'保持型号/作者构建；不当现版普遍结果')
for item in ['外观深度融入东方文化，让阅读更具书香气和氛围感','经典特性搭配全新技术，健康护眼阅读体验持续升级','软硬件性能皆出众，阅读、游戏、音乐皆能“玩”得开心']:
    inv(fam,1,item,'E-HSD-08','2022-07-01原文小标题','F-M055-01','article_heading_read','原文标题索引；健康/性能措辞不采为结论')
for item in ['电源/音量/墨智键','清晰/均衡/流畅/极速','安装App','壁纸/艺术封面/闹钟锁屏','舒尔特方格/番茄时钟']:
    inv(fam,2,item,'E-HSD-08','正文对应段','F-M055-01','feature_claim_only','没有设置保存/退出/恢复步骤')

fam='TOUCH原版与联名款作者目录'
for item in ['一、开箱','二、简单使用','三、音质','四、配置及系统体验','五、显示效果','六、综合感受','七、主观总结']:
    inv(fam,1,item,'E-HSCD-06','2021文章同名节','F-M055-05','article_heading_read','联名款独立作者样本；不采健康/性能/普遍兼容结论')
for item in ['阅读标签','音乐标签','应用标签','设置标签','App内三键导航','滑动手势方式','Touch管家','下拉四显示模式','墨智键设置','海信账号入口']:
    inv(fam,2,item,'E-HSCD-06','第四节文字/图片说明','F-M055-05','text_entry_only_images_unseen','仅入口/作者描述；图片未观察，设置与数据变化未闭合')
inv(fam,1,'官方音频接口与USB DAC定位','E-HSD-01','旧官方参数','F-M055-05','previous_capture_only','与联名款版本不拼；未得连接/退出及恢复链')

fam='Hi Reader开箱文章18节目录'
headings=[(1,'前言'),(1,'包装盒'),(1,'外观'),(1,'进入系统'),(1,'进入桌面'),(2,'书架标签页'),(2,'书城标签页'),(2,'应用标签页'),(2,'应用文件夹'),(2,'设置界面'),(1,'特色：屏保功能'),(1,'对比其他海信墨水屏'),(2,'海信A9'),(2,'海信A9 和 Touch Lite'),(2,'touch lite的设置界面'),(2,'对比 iQOO 手机'),(2,'与墨案mix7对比'),(1,'小结')]
for i,(level,item) in enumerate(headings):
    inv(fam,level,item,'E-HSD-09','2023文章目录及同名标题',status='article_body_read_selected_images' if i<11 else 'out_of_scope_comparison_not_used',gap='目录不等于操作；后段其他机型/传闻不采；设置图片限定原版Hi Reader')

fam='Hi Reader墨智键引导所见项'
key=inv(fam,1,'墨智键','E-HSCD-01','照片标题','F-M055-06;F-M055-09','static_guide_text_read','配置状态与固件未知')
for item,flow in [('设置→墨智键','F-M055-09'),('短按/双击/长按的速记入口','F-M055-06'),('速记新建/删除/查看','F-M055-06'),('长按持续录入语音','F-M055-06'),('非锁屏短按/双击启动所设App','F-M055-09'),('短按/双击重新设定应用','F-M055-09'),('查看墨智键全部功能','F-M055-09'),('返回',''),('下一步','')]:
    inv(fam,2,item,'E-HSCD-01','对应机内文字/按钮',flow,'static_guide_text_read','父引导子项；无实际触发/保存/返回结果',key)

fam='Hi Reader水墨屏控制引导所见项'
panel=inv(fam,1,'下拉快捷开关/水墨屏控制','E-HSCD-02','照片标题及首段','F-M055-10','static_guide_text_read','指南中的示例图，不是实操后状态')
for item,flow in [('Wi-Fi',''),('蓝牙',''),('静音',''),('收藏文章','F-M055-07'),('自动旋转',''),('动态刷新',''),('悬浮球',''),('海信互传',''),('截屏便笺','F-M055-10'),('截屏',''),('亮度条和加减',''),('黑夜/手动/自动/关闭',''),('当前应用显示模式',''),('清晰',''),('均衡',''),('流畅',''),('极速',''),('截屏便笺生成说明','F-M055-10'),('返回',''),('下一步','')]:
    inv(fam,2,item,'E-HSCD-02','引导示例面板/下方文字',flow,'static_control_or_guide_text_read','仅可见项；当前应用范围不是全局；流畅深色为示例；未核保存/退出',panel)

fam='Hi Reader设置页所见入口'
settings=inv(fam,1,'设置标签','E-HSCD-03','照片底部选中标签',status='static_control_inventory_only',gap='只有顶层界面，非设置全集')
for item,flow in [('WLAN',''),('蓝牙',''),('声音',''),('显示和亮度',''),('锁屏显示',''),('墨智键','F-M055-09'),('水墨屏设置',''),('截屏便笺管理','F-M055-10'),('全局收藏管理','F-M055-07'),('使用指南',''),('系统更新',''),('查看所有设置',''),('书架标签',''),('书城标签',''),('应用标签','')]:
    inv(fam,2,item,'E-HSCD-03','设置磁贴/底部标签',flow,'static_control_inventory_only','未点击，不能假定下级菜单/内容恢复',settings)

fam='Hi Reader2022速记与选词照片'
for item in ['语音录入提示','00:01与方形停止符号','垃圾桶形图标','分享形图标','软盘形图标']:
    inv(fam,1,item,'E-HSCD-04','速记静态照片','F-M055-06','static_control_inventory_only','图标不是动作成功，未测计时/删除/分享/保存')
for item in ['选区与拖拽柄','划线','批注','复制','搜索','词典','百科','汉语大词典来源标注']:
    inv(fam,1,item,'E-HSCD-05','选词静态照片','F-M055-08','static_control_inventory_only','只看一个词条结果；离线/保存/重开未知')

fam='仍未取得的型号化帮助'
for item,eid,flow in [('三型号完整官方手册','E-HSD-03',''),('A9电话/短信/音频打断','E-HIS-01','F-M055-01'),('TOUCH音频入库/队列/锁屏/DAC退出','E-HSD-01','F-M055-05'),('Hi Reader后台音频和转写失败','E-HSD-05','F-M055-06'),('三型号备份/删除/更新恢复','E-HSD-03',''),('Hi Reader文章收藏重开与冲突','E-HSD-05','F-M055-07')]:
    inv(fam,1,item,eid,'研究缺口，非原文标题',flow,'gap_not_support_denial','未取到不等于不支持；各型号/构建分开')
ip=L/'batches/hisense-controls-deepening-inventory.csv'
with ip.open('w',newline='') as f:
    wr=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');wr.writeheader();wr.writerows(rows)
summary={'new_e':len(E),'new_f':len(new_f),'inventory_rows':len(rows),'families':dict(Counter(x['source_family'] for x in rows)),'old_f':len(updates),'old_f_fields':sum(len(u['changed_fields']) for u in updates),'old_e_captures':len(adds)};dump(W/'inventory-summary.json',summary)

s=(W/'case-before.md').read_text()
s=s.replace('查阅2026-10-26。','初查2026-10-26；2026-10-27复读部分原文并观察五张Hi Reader照片。',1)
s=s.replace('官方参数Android11、TOUCH UI；2021-05-31首发体验，具体固件未知。','官方参数Android11、TOUCH UI；2021-05-31首发体验。另列2021西安博物院联名款作者样本，固件均未知，不能当同一构建。')
s=s.replace('E-HSD-01/06。不是TOUCH Lite，也不从Lite教程补USB传输步骤。','E-HSD-01/06、E-HSCD-06。不是TOUCH Lite，也不从Lite教程补USB传输步骤。')
s=s.replace('官方商城仍能返回这三款历史商品的文字','2026-10-26旧采集中官方商城能返回这三款历史商品的文字')
s=s.replace('现行官网墨水屏栏目列A10与Hi Reader 2026；资料下载页本次返回正文没有旧三款下载条目','旧采集的官网墨水屏栏目列A10与Hi Reader 2026；资料下载页2026-10-27的web正文仍未列旧三款下载条目，独立GET超时后停止')
s=s.replace('**D1+D2；无实机、无连续视觉记录、无本轮图像观察。**','**D1+D2；无实机、无连续视觉记录。2026-10-27只看五张静态照片，其中三张含机内引导/设置。**')
s=s.replace('TOUCH评测直接打开超时但搜索索引提供正文。','旧TOUCH评测直接打开超时但搜索索引提供正文；新联名款作者正文经公开web工具取得，未看其图片/视频。')
s=s.replace('不存在一本已取得的共同“海信墨水屏手册”，不能伪造统一章节号。','不存在一本已取得的共同“海信墨水屏手册”，不能伪造统一章节号。新[逐项来源/控件目录](/home/workspace/reader/docs/research/interaction-library/batches/hisense-controls-deepening-inventory.csv)分别登记文章目录、实看控件、旧线索和缺口；父子项不作任务数。')
s=s.replace('开箱“进入系统/桌面”仅文字线索，E-HSD-09','开箱三张引导/设置已观察，E-HSCD-01—03')
s=s.replace('初次账号/密码、返回/通知面板、取消与重启后状态；开箱引导图未观察。','初次账号/密码、实际返回/取消、重启状态仍缺；照片中的返回/下一步未触发。')
s=s.replace('F-M055-06速记；E-HSD-05','F-M055-06速记、F-M055-09非锁屏启动应用')
s=s.replace('配置信息','配置信息')
s=s.replace('配置保存范围、恢复默认、逐app规则、刷新与音频是否联动；无速度或残影测量。','Hi Reader引导写当前应用显示模式；保存/恢复默认及跨App规则仍未核，不能改成全局。无速度或残影测量。')
s=s.replace('F-M055-06、07','F-M055-06、07、10')
s=s.replace('未取得同版本完整帮助 | 未取得 | 未取得','未取得同版本完整帮助 | 联名款作者只述三键在App内出现 | 设置/控制照片没有后台保活步骤')
anchor='## 已有与新增任务'
intro='''## Hi Reader静态机内指南的具体范围

2023开箱文章前半段属于原版Hi Reader；后半段TOUCH Lite/A9对比不用于这些图的型号归属。三张照片能读到“设置→墨智键”、非锁屏启动限制、当前应用显示模式和截屏便笺说明；设置页另有使用指南、全局收藏管理、截屏便笺管理等入口。只看到顶层，不把“查看所有设置”当已读完整菜单。E-HSCD-01—03。

控制引导内的四模式用于**当前应用**，深色“流畅”只是示例值；没有观察选中、持久保存或返回阅读后的效果。刷新、前光和后台播放的关系仍未知。截图生成的便笺、识屏收藏的文章、速记录入的语音/文字是三个分别取证的对象，不能把某个管理入口当其他对象的备份恢复入口。

'''
s=s.replace(anchor,intro+anchor)
s=s.replace('旧F-M055-01/02保留；新增从03开始。','F-M055-01—08保留；新增09/10来自照片中的机内说明。')
s=s.replace('| F-M055-05 | TOUCH阅读/音乐导航与侧键 D1','| F-M055-05 | TOUCH阅读/音乐导航与侧键 D1')
s=s.replace('只到入口与局部反馈，未取得传输模式、扫描、队列和退出完整链。官方USB DAC为功能声明。','另有2021联名款作者删阅读标签、应用/设置标签不可删的描述；音乐标签到播放器，三键导航在具体App内出现。删标签不等于卸载或删除阅读数据。仍未闭合传输/扫描/队列链。')
s=s.replace('| E-HSD-01；E-HSD-06 |','| E-HSD-01；E-HSD-06；E-HSCD-06 |')
s=s.replace('文中称语音可转文字，终点是便签收起/保存；没有原音频去向说明。','2022文章称收起/保存；2023机内引导另列已配置按法启动及新建/查看/删除。速记照片有录入状态和图标，未触发，不把两构建拼成连续流程。')
s=s.replace('| E-HSD-05 | 联网/账号/费用','| E-HSD-05；E-HSCD-01/04 | 联网/账号/费用')
s=s.replace('文中称页面存到本地、可离线阅读；未给重新打开的入口。','2022文称存到本地、可离线阅读；2023设置图能见全局收藏管理，未点入或打开任何收藏项。')
s=s.replace('| E-HSD-05 | 不是所有app保证','| E-HSD-05；E-HSCD-02/03 | 不是所有app保证')
s=s.replace('仅确定菜单；选择→写注释→保存→再次定位→返回的链仍缺。','照片可见选区、六菜单和词典结果，来源标汉语大词典；保存标注/重开/返回链仍缺。')
s=s.replace('| E-HSD-05 | 词典来源/离线','| E-HSD-05；E-HSCD-05 | 词典切换/离线')
needle='## 异常和恢复的证据边界'
newrows='''## 新文档任务：机内引导照片，不是操作记录

| F编号 | 前置与操作 | 文档给出的结束/数据状态 | 限制与证据 |
| --- | --- | --- | --- |
| F-M055-09 Hi Reader非锁屏启动指定应用，D2 | 已配置墨智键动作；指南给设置→墨智键→短按/双击墨智键的重设入口；在非锁屏状态用对应按法 | 指南称启动所设App；改绑定的保存反馈与启动后画面未观察 | E-HSCD-01/03；默认绑定、候选列表、锁屏其他动作、启动失败/返回原书未知 |
| F-M055-10 Hi Reader截图到便笺，D2 | 下拉快捷开关中点击截屏便笺 | 指南称截取当前界面并出现在水墨屏截屏便笺；设置图另有管理入口，未点入 | E-HSCD-02/03；图像格式、文件位置、识字编辑、导出删除、失败/重复/重启保持未知；不等同文章收藏或语音速记 |

'''
s=s.replace(needle,newrows+needle)
s=s.replace('只有对应型号材料才能关闭相应缺口。','只有对应型号材料才能关闭相应缺口；静态设置入口不算已经完成再打开或恢复。')
s=s.replace('全部查阅2026-10-26。','旧来源初查2026-10-26；E-HSD-05/08/09于2026-10-27同URL复读，E-HSD-03复核公开正文并记录独立GET超时。')
s=s.replace('图片为未加载占位，不能转述机内引导图；后段TOUCH Lite/A9及未经证实传闻不采。','原轮图片未加载；本轮从HTML公开地址取得并实看三张，衍生记录见E-HSCD-01—03。后段TOUCH Lite/A9和传闻仍不采。')
s += '''

本批图像和来源身份审计见[独立摘记](/home/workspace/reader/docs/research/interaction-library/artifacts/hisense-controls-deepening/source-notes.md)。五张图片无PDF页号、未组成连续操作，机内文字所在照片也不等于取得完整手册：

| E编号 | 图像/新来源 | 定位 |
| --- | --- | --- |
| E-HSCD-01 | [墨智键引导照片](/home/workspace/reader/docs/research/interaction-library/artifacts/hisense-controls-deepening/hireader-key-guide.webp) | E-HSD-09“进入系统”；非锁屏启动与配置入口 |
| E-HSCD-02 | [水墨屏控制引导照片](/home/workspace/reader/docs/research/interaction-library/artifacts/hisense-controls-deepening/hireader-screen-guide.webp) | E-HSD-09“进入系统”；当前应用/截屏便笺 |
| E-HSCD-03 | [设置入口照片](/home/workspace/reader/docs/research/interaction-library/artifacts/hisense-controls-deepening/hireader-settings.webp) | E-HSD-09“设置界面”，原版Hi Reader段 |
| E-HSCD-04 | [速记照片](/home/workspace/reader/docs/research/interaction-library/artifacts/hisense-controls-deepening/hireader-note.jpeg) | E-HSD-05第三部分；00:01为图中状态，不是本项目计时 |
| E-HSCD-05 | [选词照片](/home/workspace/reader/docs/research/interaction-library/artifacts/hisense-controls-deepening/hireader-selection.jpeg) | E-HSD-05第三部分；未转录文学正文 |
| E-HSCD-06 | [马马马不前：TOUCH主观使用体验](https://www.bilibili.com/read/cv13411020/) | 2021-10-01，联名款，第四节；只读文字，未看图片/视频 |
'''
C.write_text(s)
formal=[C,L/'batches/hisense-controls-deepening-evidence.jsonl',L/'batches/hisense-controls-deepening-coverage.csv',ip,*sorted(A.iterdir())]
dump(W/'formal-manifest.json',[{'path':rel(p),'sha256':h(p)} for p in formal]);print(json.dumps(summary,ensure_ascii=False))
