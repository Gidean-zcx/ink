from pathlib import Path
from hashlib import sha256
from html.parser import HTMLParser
import csv
import json
import shutil
from collections import Counter

ROOT = Path('/home/workspace/reader')
WORK = ROOT / 'work/moaan-controls-deepening'
LIB = ROOT / 'docs/research/interaction-library'
ART = LIB / 'artifacts/moaan-controls-deepening'
CASE = LIB / 'cases/M-054-moaan.md'
ART.mkdir(exist_ok=True)
BASE = json.loads((WORK / 'baseline.json').read_text())
assert sha256(CASE.read_bytes()).hexdigest() == BASE['case_sha256'], 'Do not rerun after case edits/merge'

def digest(path): return sha256(path.read_bytes()).hexdigest()
def dump(path, value): path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
def relative(path): return str(path.relative_to(ROOT))
old_e = {x['evidence_id']: x for x in json.loads((WORK/'evidence-before.json').read_text())}
old_f = {x['flow_id']: x for x in json.loads((WORK/'coverage-before.json').read_text())}
caps = {x['key']: x for x in map(json.loads, (WORK/'captures.jsonl').read_text().splitlines())}
images = json.loads((WORK/'image-captures.json').read_text())
for r in images:
    shutil.copyfile(ROOT/r['raw_work_path'], ART/(r['key']+'.jpg'))

# Reuse the old captures; no repeat network requests or alteration of the old artifacts.
existing = {
 'E-MOD-01':'moaan-guide-index.html', 'E-MOD-03':'moaan-transfer-2020.html',
 'E-MOD-04':'root-moaan-87.html', 'E-MOD-10':'root-moaan-82.html',
 'E-MOD-11':'inkpalm-plus-b.png', 'E-MOD-12':'inkpalm-plus-c.png', 'E-MOD-13':'inkpalm-plus-a.png'
}
local_records = {}
for eid, name in existing.items():
    src=ROOT/'work/chinese-reader-deepening'/name
    dst=WORK/'raw'/name
    shutil.copyfile(src,dst)
    local_records[eid]={'raw_work_path':relative(dst),'raw_sha256':digest(dst),'original_work_path':relative(src)}

notes='''# M054 墨案：来源复读与静态观察

查阅/观察2026-10-27。全部hands_on=false；无连续观察。此摘记记录来源身份和视觉边界，操作与数据分支见M054案例，逐项条目见同名inventory.csv；不重复全文。

## 同一Plus短说明书：E-MOD-11/12/13

本地复读社区固定提交保存件，没有重新下载。a为封面及07，b为01/02/08—10，c为03—06及保修卡。三张图均实际查看；章节号不是页码，也没有PDF页序。封面/参数明确inkPalm Plus、Android11，固件未知；厂商样式与生产者标识不等于官网托管，扫描出处仍未知。目录核到全部01—10及所见控件，但它不是完整软件帮助。

这次新增核点为c的04指示灯条目；05处理失败后仍要求电量/售后分流，不保证恢复及数据保留。保修卡表格里的通用身份栏不能证明本机有蜂窝通信功能。正式图片仍复用artifacts/chinese-reader-deepening/inkpalm-plus-{a,b,c}.png。

## 官方历史网页：E-MOD-01、03—10

01/03/04/10复读旧HTTP原件；05—09正常HTTP GET取得200。八篇都标2020-06-08，正文未固定型号。仅提取正文，网页内GIF未打开/下载；目录中的AI录音笔超出本案例范围，未跟进。

- 03保留USB的books与FTP的book；文件名建议不当普遍技术解释。
- 04明确未加入书架则多看不显示；“页面和状态栏的返回”原句不够清晰，触发顺序未由图确认。
- 05写“目标文件夹外面”，不能把目标位置自行改成目录内。
- 06还列浏览器按软件名搜索APK的第三途径，仅登记历史目录，不执行或推荐其安装渠道。
- 07只到PDF空白处唤出底栏；08首菜单带日期；09只描述网格预览；10列列表或缩略图两种入口。退出、覆盖、恢复均未补造。

## E-MOCD-01：2022年Plus作者记录与两张照片

来源为搜狐号“竖锯评测”，文章标原创、2022-05-30；作者自报使用，不是官方验证。正文及静态照片固定Plus，固件/阅读App版本未知；未接受文章的性能、护眼、续航或任意App兼容概括。

实际查看plus-quick-panel.jpg：可见八个快捷项、四条调节条、两个自动调节框、更多音量设置和无通知提示。低处开关的文字不清，不命名其功能。plus-refresh-panel.jpg：刷新弹窗有两选项和取消，普通项在照片中选中。两照片时间不同，不能组成点击轨迹或耗时测量；未观察切换、取消、自动调节或关闭面板的结果。

作者另自报接Type-C耳机听书；没有播放器、耳机型号或睡眠恢复链。这个记录不能补成离线/后台播放、接U盘自动入库或拔插恢复保证。正式观察仅两张照片；没有看其他文章图片或视频。
'''
(ART/'source-notes.md').write_text(notes)
note_rel='artifacts/moaan-controls-deepening/source-notes.md'
note_sha=digest(ART/'source-notes.md')

add=[]
for eid,r in local_records.items():
    image=eid in ('E-MOD-11','E-MOD-12','E-MOD-13')
    cap={'checked_on':'2026-10-27','access_mode':'existing_local_static_image_reobserved' if image else 'existing_local_official_HTTP_capture_reread',
         **r,'source_captured_on':'2026-10-26','locator':old_e[eid]['locator'],
         'hands_on':False,'visual_observed':image,'note':'本地复读，未重新联网；不更新原采集日期。',
         'archived_artifact':old_e[eid]['archived_artifact'] if image else note_rel,
         'sha256':old_e[eid]['sha256'] if image else note_sha,
         'audit_artifact':note_rel,'audit_sha256':note_sha}
    if image: cap['observed_on']='2026-10-27'
    add.append({'evidence_id':eid,'expected_url':old_e[eid]['url'],'before_additional_captures':old_e[eid].get('additional_captures',[]),'append_additional_captures':[cap]})
for eid,key in [('E-MOD-05','moaan-88'),('E-MOD-06','moaan-89'),('E-MOD-07','moaan-85'),('E-MOD-08','moaan-84'),('E-MOD-09','moaan-83')]:
    r=caps[key]
    assert r['status']==200 and r['url']==old_e[eid]['url']
    cap={k:v for k,v in r.items() if k not in ('key','state_change')}
    cap.update(locator=old_e[eid]['locator'],hands_on=False,visual_observed=False,
               archived_artifact=note_rel,sha256=note_sha,note='同URL补原件与散列；2020历史指南型号未知，GIF未看。')
    add.append({'evidence_id':eid,'expected_url':old_e[eid]['url'],'before_additional_captures':old_e[eid].get('additional_captures',[]),'append_additional_captures':[cap]})
dump(WORK/'old-evidence-additional-captures.json',add)

report=caps['plus-report']
new_e={
 'evidence_id':'E-MOCD-01','product_id':'M-054','title':'inkPalm Plus 2022原作者报告及快捷/刷新两张静态照片',
 'url':report['url'],'checked_on':'2026-10-27','source_type':'firsthand_user_report',
 'access_mode':'anonymous_public_HTML_GET_and_two_selected_static_photos_observed',
 'version_scope':'inkPalm Plus / Android11; 2022-05-30 author report; firmware and reading-app builds unknown',
 'app_build':None,'firmware_version':None,'source_updated_on':'2022-05-30',
 'locator':'冷暖双阅读灯/安卓系统段；顶部快捷栏与刷新模式两张照片；Type-C耳机段',
 'note':'作者自报使用，非官方/本项目实测；只看两张照片，不组成连续操作。手势生效、睡眠音频、文件入库和恢复未知。',
 'hands_on':False,'visual_observed':True,'supersedes':None,
 'raw_work_path':report['raw_work_path'],'raw_sha256':report['raw_sha256'],
 'extracted_work_path':report['extracted_work_path'],'extracted_sha256':report['extracted_sha256'],
 'archived_artifact':note_rel,'sha256':note_sha,'observed_on':'2026-10-27',
 'observed_artifacts':[{'path':'artifacts/moaan-controls-deepening/'+r['key']+'.jpg','sha256':r['raw_sha256'],'source_url':r['url'],'raw_work_path':r['raw_work_path']} for r in images],
 'capture_state':'firsthand_report_text_and_selected_static_photos_archived'
}
(LIB/'batches/moaan-controls-deepening-evidence.jsonl').write_text(json.dumps(new_e,ensure_ascii=False)+'\n')
with (LIB/'batches/moaan-controls-deepening-coverage.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(next(iter(old_f.values()))),lineterminator='\n');writer.writeheader()

changes={
 'F-M054-01':{
  'known_branches':'inkPalm5开机5秒/菜单2秒；Plus开机或关机/重启菜单3秒，短按待机/唤醒；Plus图示说明充电白灯闪烁、充满常亮。',
  'gaps':'实际固件、首启、自动休眠、唤醒阅读位置、未保存内容及音频恢复；充电灯不证明系统响应或数据已保存。'},
 'F-M054-02':{
  'evidence_ids':'E-MOA-01;E-MOD-12;E-MOD-14;E-MOCD-01',
  'known_branches':'Plus手册默认阅读页侧键翻页/非阅读页音量，Logo点按返回/长按全刷；2022作者照片仅补快捷栏、刷新两选项和取消的可见状态，不闭合切换。',
  'gaps':'改键保存/冲突/恢复默认、所有第三方阅读App映射、Logo返回后的阅读位置；旧inkPalm5英文译义仍不能用Plus倒推；照片固件未知。'},
 'F-M054-03':{
  'evidence_ids':'E-MOA-01;E-MOCD-01',
  'known_branches':'inkPalm5手册给设备设置中的网络设置及WiFi/蓝牙分支；Plus 2022照片仅可见WiFi/蓝牙快捷项，不补成Plus配网或配对步骤。',
  'gaps':'各机型连接成功提示、错误密码、断开重连；Plus配对入口、音频路由和睡眠连接未核。',
  'visual_status':'static_reference_only'},
 'F-M054-04':{
  'known_branches':'均按电源12秒后释放；inkPalm5再按5秒/Plus3秒；仍无解再检查电量/联系售后，属于手册异常处理建议。',
  'gaps':'无恢复出厂或数据删除步骤；重启是否成功、未保存笔记/文件保留、备份还原及更新失败恢复均未知。'},
 'F-M054-05':{
  'known_branches':'inkPalm5旧手册/用户记录与Plus短说明书分开；2020未标机型PDF正文仅给点空白区唤出底栏，再找对应图标。',
  'gaps':'固定机型完整导入开书查词返回续读；PDF批注/裁边/重排后保存、切回原页和返回栈未核，历史指南不能归属Plus/W7/W8。'},
 'F-M054-06':{
  'known_branches':'历史USB/FTP/多看WLAN分支及蓝牙内部存储books；蓝牙正文明确不加入书架则多看不显示，文件接收与书架条目分开。',
  'gaps':'机型未知；USB books/FTP book冲突；蓝牙返回原句顺序未看图确认；拒绝加入后重加、断传/重名/取消/删除恢复未说明。'},
 'F-M054-07':{
  'known_branches':'历史长按复制/剪切正文写到目标文件夹外面粘贴，位置歧义保留；APK三种取得入口与Apps/download路径，不代表安装完成或当前兼容。',
  'gaps':'文件移动目标位置需对照未看的GIF；机型、覆盖/剪切中断、未知来源权限、安装失败及卸载数据保留未知；不以第三方作者泛称兼容作保证。'},
 'F-M054-08':{
  'known_branches':'历史新建保存命名或首项日期菜单改名；页码切4/9格；已保存笔记在列表或缩略图模式经省略号导出PDF至Note。W7/W8仍各自仅旧作者线索。',
  'gaps':'历史指南机型未归属；返回原笔迹、可编辑原稿/导出副本关系、同名覆盖、导出失败和删除/同步恢复未核。'}
}
updates=[]
for fid,patch in changes.items():
    before=old_f[fid];after={**before,**patch,'checked_on':'2026-10-27'}
    updates.append({'flow_id':fid,'before':before,'after':after,
                    'changed_fields':{k:{'before':before[k],'after':v} for k,v in after.items() if before[k]!=v}})
dump(WORK/'old-coverage-updates.json',updates)

rows=[]
def inv(family,level,item,locator,eid,flows='',status='source_body_read',gap='',parent='',scope=''):
    evidence=new_e if eid=='E-MOCD-01' else old_e[eid]
    row={'inventory_id':f'I-MOCD-{len(rows)+1:03d}','source_family':family,'parent_id':parent,'level':level,
         'item':item,'locator':locator,'source_url':evidence['url'],'evidence_ids':eid,'flow_ids':flows,
         'coverage_status':status,'model_version_scope':scope or evidence['version_scope'],'gap':gap}
    rows.append(row);return row['inventory_id']

family='Plus三图短说明书'
inv(family,0,'封面','a_cn封面','E-MOD-13',status='static_identity_observed',gap='同一手册；不是页码或版本页')
chapter_specs=[('01 产品介绍','b_cn','E-MOD-11'),('02 基本参数','b_cn','E-MOD-11'),('03 配件清单','c_cn','E-MOD-12'),('04 功能介绍','c_cn','E-MOD-12'),('05 异常处理','c_cn','E-MOD-12'),('06 注意事项','c_cn','E-MOD-12'),('07 产品中有害物质的名称及含量','a_cn','E-MOD-13'),('08 保修说明','b_cn','E-MOD-11'),('09 非保修条例','b_cn','E-MOD-11'),('10 联系我们','b_cn','E-MOD-11')]
chapter_ids={}
for title,loc,eid in chapter_specs:
    cid=title[:2];flows={'04':'F-M054-01;F-M054-02','05':'F-M054-04'}.get(cid,'')
    chapter_ids[cid]=inv(family,1,title,loc+'章节号'+cid,eid,flows,'static_chapter_observed','父级目录；子项不作独立任务；无固件构建')
for item,flow in [('电源键','F-M054-01'),('功能键','F-M054-02'),('Logo触控','F-M054-02'),('麦克风',''),('Type-C口',''),('指示灯','F-M054-01')]:
    inv(family,2,item,'b_cn 01部件图','E-MOD-11',flow,'static_control_observed','只有部件标注；接口用途不能由外形补全',chapter_ids['01'])
for item,flow,gap in [('开机','F-M054-01','首启及恢复内容未知'),('关机','F-M054-01','未保存内容未知'),('重启','F-M054-01','不代表恢复出厂'),('待机/唤醒','F-M054-01','睡眠音频/返回位置未说明'),('充电指示灯','F-M054-01','状态不等于数据保存'),('阅读/非阅读页面功能键','F-M054-02','第三方App映射未逐一核'),('Logo单点/长按及设备设置更改或关闭','F-M054-02','只有默认动作与可配置声明；保存反馈未知')]:
    inv(family,2,item,'c_cn 04对应条目','E-MOD-12',flow,'static_document_branch_read',gap,chapter_ids['04'])
for item in ['死机后长按释放再开机','仍无解时检查电量/联系售后']:
    inv(family,2,item,'c_cn 05条目','E-MOD-12','F-M054-04','static_document_branch_read','条件性处理建议；成功和数据保留未验证',chapter_ids['05'])
inv(family,1,'产品保修卡','c_cn下半部未编号表格','E-MOD-12',status='static_noninteraction_form_observed',gap='不以通用表格身份字段推导蜂窝能力')

family='官网历史支持目录'
support=inv(family,0,'使用指南','本地2026-10-26 HTTP索引','E-MOD-01',status='existing_official_index_reread',gap='本轮未重新抓索引；不是固定型号手册')
inv(family,1,'墨案AI录音笔——使用说明','索引首条','E-MOD-01',status='index_only_out_of_scope',gap='录音笔不同产品，未打开',parent=support)
official=[('如何下载第三方应用','E-MOD-06','F-M054-07'),('如何移动文件的位置','E-MOD-05','F-M054-07'),('蓝牙传输的文件在哪里查看','E-MOD-04','F-M054-06'),('三种常见的传输方式','E-MOD-03','F-M054-06'),('PDF如何批注、裁边、重排','E-MOD-07','F-M054-05'),('如何修改笔记名称','E-MOD-08','F-M054-08'),('多页笔记如何快速查看','E-MOD-09','F-M054-08'),('如何导出PDF笔记','E-MOD-10','F-M054-08')]
legacy_ids={}
for item,eid,flow in official:
    legacy_ids[eid]=inv(family,1,item,'2020-06-08同名正文',eid,flow,'official_body_read_model_unassigned','步骤不能归属Plus/W7/W8；页面内GIF未看',support)
for item in ['USB传输（非Mac）','系统网络传输（FTP）','多看书架菜单WLAN传输']:
    inv(family,2,item,'show86 方法一/二/三','E-MOD-03','F-M054-06','official_body_read_model_unassigned','父文分支；books/book矛盾，不改译统一',legacy_ids['E-MOD-03'])
for item in ['电脑取得APK后USB传入Apps','浏览器取得APK后download','浏览器按软件名搜索APK']:
    inv(family,2,item,'show89 方法一/二/三','E-MOD-06','F-M054-07','official_body_read_model_unassigned','仅历史取得路径；未执行、未推荐当前渠道',legacy_ids['E-MOD-06'])
inv(family,1,'常见问题：电子说明书链接','旧E-MOD-02采集','E-MOD-02',status='previous_capture_only_blocked_target',gap='Maitube目标旧403，本轮未重试；正文缺失')

family='2022 Plus原作者静态/文字'
rep=inv(family,0,'Plus上手文章所选系统段','2022-05-30 冷暖双阅读灯/安卓系统段','E-MOCD-01',status='firsthand_report_text_read',gap='固件未知；不采营销性能/护眼/续航及普遍兼容结论')
panel=inv(family,1,'快捷面板照片','plus-quick-panel.jpg','E-MOCD-01','F-M054-02','static_control_inventory_only','只确认可见控件，不等于动作执行；正文给亮屏顶部下拉',rep)
for item,flow in [('Wi-Fi','F-M054-03'),('蓝牙','F-M054-03'),('刷新','F-M054-02'),('加速',''),('截屏',''),('省电模式',''),('普通刷新','F-M054-02'),('设置','F-M054-02'),('亮度调节条',''),('色温调节条',''),('对比度图标调节条',''),('音量图标调节条',''),('自动亮度调节',''),('自动色温调节',''),('更多音量设置',''),('低处文字不清的开关',''),('没有通知状态','')]:
    inv(family,2,item,'plus-quick-panel.jpg 可见项','E-MOCD-01',flow,'static_control_inventory_only','单帧值非默认；不补动作、算法、睡眠策略或效果',panel)
refresh=inv(family,1,'刷新模式弹窗照片','plus-refresh-panel.jpg','E-MOCD-01','F-M054-02','static_control_inventory_only','照片中普通项选中；打开/关闭和切换未观察',rep)
for item in ['普通刷新','快速刷新','取消']:
    inv(family,2,item,'plus-refresh-panel.jpg 弹窗','E-MOCD-01','F-M054-02','static_control_inventory_only','父弹窗子项；不计独立流程或生效验证',refresh)
for item,gap in [('虚拟按键/手势','只有作者描述，无完整设置和手势方向'),('Type-C耳机听书','作者自报使用，应用/耳机/锁屏和后台恢复未给'),('锁屏壁纸','作者描述不说明CPU、联网、音频或持久进度状态')]:
    inv(family,1,item,'文章对应段落','E-MOCD-01',status='firsthand_report_claim_only',gap=gap,parent=rep)

family='其他型号保留边界'
for item,eid,flow,gap in [
 ('inkPalm5原单页英文手册','E-MOA-01','F-M054-01;F-M054-02;F-M054-03;F-M054-04','依旧采集文本；旧本地下载403，未重试；不拿Plus消除译义'),
 ('inkPalm5 2022用户系统段','E-MOD-14','F-M054-05;F-M054-07','本轮未重新读取正文；作者版本不代表当前'),
 ('W7借机报告','E-MOD-15','F-M054-08','本轮仅复核旧记录；未获得型号手册及菜单链'),
 ('W8用户报告','E-MOD-16','F-M054-08','旧直接打开超时停止；本轮不重试、不以索引当实测'),
 ('迷你阅超核版导航项','E-MOD-01','','旧官网导航只标产品名，不与Plus同一型号'),
 ('墨案助手服务协议','E-MOD-17','','旧采集；服务范围不证明任何型号数据已备份或可恢复')]:
    inv(family,1,item,'旧证据定位（本轮未重取）',eid,flow,'previous_capture_scope_retained',gap)
family='未闭合任务'
for item,eid,flow,gap in [
 ('Plus本机阅读返回/续读','E-MOD-12','F-M054-05','需型号/构建与完整阅读帮助；Logo返回不等于恢复进度'),
 ('Plus文件进入/自动扫描/删书恢复','E-MOD-03','F-M054-06','2020指南未标型号；需Plus书库索引规则'),
 ('Plus睡眠听书/断连/定时','E-MOCD-01','','只有接口使用报告；需应用和系统保活、锁屏恢复依据'),
 ('重启/更新后的用户数据','E-MOD-12','F-M054-04','重新开机不是备份恢复；需保存对象及失败路径'),
 ('W7型号化阅读/手写/导出','E-MOD-15','F-M054-08','需独立手册/固件/操作正文'),
 ('W8型号化录音/笔迹/恢复','E-MOD-16','F-M054-08','需独立手册/固件/操作正文')]:
    inv(family,1,item,'证据缺口（非源文标题）',eid,flow,'gap_not_supported_or_disproved',gap)
inv_path=LIB/'batches/moaan-controls-deepening-inventory.csv'
with inv_path.open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
counts=dict(Counter(r['source_family'] for r in rows))
dump(WORK/'inventory-summary.json',{'total':len(rows),'families':counts,'new_evidence':1,'new_flows':0,'old_flows':len(updates),'old_fields':sum(len(u['changed_fields']) for u in updates),'old_evidence_captures':len(add)})

case=(WORK/'case-before.md').read_text()
case=case.replace('查阅及新增静态图观察2026-10-26。','初查2026-10-26；2026-10-27复读Plus三图短说明书、官网历史正文，并查看2022作者照片中的两个系统界面。')
case=case.replace('构建未知，不将仓库Root/OTA过程当原厂教程。与inkPalm5按键时长不同。E-MOD-11—13','构建未知，不将仓库Root/OTA过程当原厂教程。与inkPalm5按键时长不同；2022作者系统照片仅限当时界面。E-MOD-11—13、E-MOCD-01')
case=case.replace('以及第04/05节。','以及全部01—10章节标题、04/05条目与硬件标注。')
needle='两份简要说明书还列配件、注意事项、有害物质/保修等非交互章节。Plus图页编号01—10并非十个软件功能；未因取得短说明书而声称已核全系统清单。大屏页/商品图只用于确认产品线，不替代步骤。'
replacement=f'''逐项[来源/控件目录](/home/workspace/reader/docs/research/interaction-library/batches/moaan-controls-deepening-inventory.csv)共{len(rows)}条：Plus短说明书{counts['Plus三图短说明书']}条、官网历史目录/分支{counts['官网历史支持目录']}条、2022 Plus作者文字/静态控件{counts['2022 Plus原作者静态/文字']}条、其他型号边界{counts['其他型号保留边界']}条及未闭合任务{counts['未闭合任务']}条。父章节和子控件明确关联，不按行数计算任务或完整率。旧原件复读、正文、图片、未重取的旧记录和缺口分别标注。

Plus三图的印刷章节逐项为：01产品介绍、02基本参数、03配件清单、04功能介绍、05异常处理、06注意事项、07产品中有害物质的名称及含量、08保修说明、09非保修条例、10联系我们；另有封面和未编号保修卡。a承载封面/07，b承载01/02/08—10，c承载03—06/保修卡，**章节号不是页码，没有PDF页序**。这只是短说明书，不是全系统菜单；保修与合规章节只作目录，不作当前政策结论。

2022年“竖锯评测”自报使用Plus，给亮屏时顶部下拉的说明。实际查看两张文章照片：快捷栏有网络/刷新等控件，刷新弹窗可见普通、快速和取消。照片中的普通项选中不证明默认值；未看到切换、取消或返回的连续过程，不据两张不同时刻的照片计时。自动调节框的存在不说明传感器或算法；“加速”“省电模式”未取得下级帮助。E-MOCD-01。照片及[观察审计](/home/workspace/reader/docs/research/interaction-library/artifacts/moaan-controls-deepening/source-notes.md)仅补D1界面线索。'''
assert needle in case;case=case.replace(needle,replacement)
case=case.replace('两型号文档D2；E-MOA-01、E-MOD-12。阅读位置、未保存编辑、自动锁屏与唤醒耗时未知','两型号文档D2；E-MOA-01、E-MOD-12。Plus另标充电白灯闪烁、充满常亮；灯态不证明系统响应或数据保存。阅读位置、未保存编辑、自动锁屏、睡眠音频与唤醒耗时未知')
case=case.replace('用户报告的多看冲突不当原厂已确认缺陷','用户报告的多看冲突不当原厂已确认缺陷。手册“阅读页”不保证所有第三方App采用同一映射；返回动作未说明保留选区或阅读位置。2022快捷栏/刷新照片只补可见控件，E-MOCD-01')
case=case.replace('D2仅inkPalm5，E-MOA-01。不并入Plus；密码错误、取消、自动重连未知','D2仅inkPalm5，E-MOA-01。E-MOCD-01的Plus照片只有Wi-Fi/蓝牙快捷项，未给配对步骤；密码错误、取消、自动重连及音频路由未知')
case=case.replace('目标是再次启动，不是清除用户数据','手册提供再次启动的处理建议；没有给恢复出厂或清除数据的动作')
case=case.replace('文档D2；E-MOA-01、E-MOD-12。未承诺文件/笔记不丢失，无恢复出厂动作','文档D2；E-MOA-01、E-MOD-12。处理成功、未保存笔记/文件保留及更新失败恢复均未保证')
case=case.replace('## 新增历史指南任务：步骤可读，型号适用仍D1','## 历史指南任务：步骤可读，型号适用仍D1')
case=case.replace('蓝牙接收文件在内部存储`books`，通知栏打开后退出时按提示加入书架','蓝牙接收文件在内部存储`books`；正文将下拉状态栏打开、页面/状态栏返回和加入书架提示连写，未用图确认具体返回顺序')
case=case.replace('局域网分支依赖电脑/设备同网，不是云同步','正文明确不加入书架则多看不显示。局域网分支依赖电脑/设备同网，不是云同步')
case=case.replace('E-MOD-05：文件管理长按文件→复制/剪切→目标目录→粘贴。','E-MOD-05：文件管理长按文件→复制/剪切；随后原文写“目标文件夹外面粘贴”，位置有歧义，未观看其GIF，不自行改成目录内粘贴。')
case=case.replace('设备浏览器下载则到`download`找APK。','设备浏览器下载则到`download`找APK；原文另列按软件名搜索APK的第三途径，仅作为历史入口目录。')
case=case.replace('或笔记左上菜单首项改名','或笔记左上菜单首项（日期）改名')
case=case.replace('已保存笔记缩略图的“…”→导出PDF','已保存笔记在列表或缩略图模式点“…”→导出PDF')
case=case.replace('原稿是否随导出更改、是否包含可编辑笔迹层，文档没有说明','原稿是否随导出更改、是否包含可编辑笔迹层、导出后回到哪一页，文档没有说明')
case=case.replace('已知恢复分支只有两份手册的卡死再启动，以及历史传书的路径/文件名注意。','已知恢复建议仍只有两份手册的卡死再启动，以及历史传书的路径/文件名注意。Plus作者自报Type-C耳机听书（E-MOCD-01），没有给播放器、耳机型号、锁屏/后台或拔插恢复，不能补成睡眠续听。文中U盘用途也没有到书架自动索引的完整链。')
case=case.replace('F-M054-05/08；E-MOD-07/14','F-M054-05；E-MOD-07/14')
case=case.replace('没有型号化官方模式完整列表；不据体验文承诺清残影/刷新速度','无当前官方模式全集；2022 Plus照片另见E-MOCD-01，仅限当时普通/快速两项，不承诺效果')
case=case.replace('E-MOD-14/16；未新增F','E-MOD-14/16、E-MOCD-01；未闭合任务')
case += '\n- E-MOCD-01：[Plus原作者2022上手文章](https://www.sohu.com/a/552509726_254916)，署名竖锯评测，2022-05-30；正文HTTP200，固件未知。实际只看[快捷栏照片](/home/workspace/reader/docs/research/interaction-library/artifacts/moaan-controls-deepening/plus-quick-panel.jpg)与[刷新弹窗照片](/home/workspace/reader/docs/research/interaction-library/artifacts/moaan-controls-deepening/plus-refresh-panel.jpg)，不计连续操作。\n'
CASE.write_text(case)

formal=[CASE,LIB/'batches/moaan-controls-deepening-evidence.jsonl',LIB/'batches/moaan-controls-deepening-coverage.csv',inv_path,*sorted(ART.iterdir())]
dump(WORK/'formal-manifest.json',[{'path':relative(p),'sha256':digest(p)} for p in formal])
print(json.dumps(json.loads((WORK/'inventory-summary.json').read_text()),ensure_ascii=False))
