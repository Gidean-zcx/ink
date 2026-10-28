import csv,hashlib,json,re,shutil
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch51-integration'
assert not (W/'APPLIED').exists()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def rows(p):return list(csv.DictReader(p.open()))
def csvout(p,rs):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rs[0]));w.writeheader();w.writerows(rs)
def add(p,t):p.write_text(p.read_text().rstrip()+'\n\n'+t.strip()+'\n')
def join(a,b):return ';'.join(dict.fromkeys((a+';'+b).strip(';').split(';')))
date='2026-10-27';qdir=L/'artifacts/quaderno-file-lifecycle';adir=L/'artifacts/ak-openapp-states'
xs=json.loads((W/'extraction-audit.json').read_text())
for x in xs:
    assert sha(R/x['pdf'])==x['pdf_sha256'] and sha(R/x['text'])==x['text_sha256']
    x['reading_status']='root_read_full_extracted_page_text_no_image_or_ui_operation'
dump(W/'extraction-audit.json',xs)
g2=sorted({n for x in xs if x['key'].startswith('gen2') for n in x['pages_extracted']});g3=sorted({n for x in xs if x['key'].startswith('gen3c') for n in x['pages_extracted']})
assert len(g2)==17 and len(g3)==18
prior=json.loads((L/'artifacts/quaderno-gen2-ink/source-audit.json').read_text())
dump(qdir/'source-audit.json',dict(checked_on=date,evidence_ids=['E-QGNT-01','E-QUA-01'],captures=xs,gen2_pages_this_batch=g2,gen3c_pages_this_batch=g3,gen2_explicit_full_body_pages_in_batches50_51=sorted(set(prior['read_pages'])|set(g2)),gen2_identity_only_pages=[120],historical_scope='Gen.3C prior76-page body_read_for_this_batch is not a global union; old p68/69 visual evidence remains. No worldwide unread-page count inferred.',hands_on=False,visual_observed=False,source_identity='Gen.2 B6FK-5131-08 Z0/Z0-02 and Gen.3C B5FL-2101-02 Z0/Z0-02; issued2025-04; installed builds unknown',network_access='none for PDFs; existing files reused'))
parts=(W/'ak-web-capture.txt').read_text().split('--------------------------------------------------------------------------------');assert len(parts)==2
ac=[]
for n,(eid,faq) in enumerate([('E-AK-06','974'),('E-AK-07','1014')]):
    assert 'faq_'+faq+'.php' in parts[n]
    p=W/f'ak-faq{faq}.txt';p.write_text(parts[n].strip()+'\n')
    ac.append(dict(evidence_id=eid,url=f'https://www.iriver.jp/support/faq_{faq}.php',raw_work_path=str(p.relative_to(R)),raw_sha256=sha(p),access_mode='web_tool_complete_article_text_reread',http_status='not_exposed_by_web_tool',body_read=True))
imgs=json.loads((W/'ak-images/fetch-audit.json').read_text())
for x in imgs:
    assert x['status']==200 and sha(R/x['path'])==x['sha256']
    p=adir/Path(x['path']).name;shutil.copyfile(R/x['path'],p)
    x['artifact_path']=str(p.relative_to(L));x['viewed_by_root']=True
dump(adir/'source-audit.json',dict(checked_on=date,text_captures=ac,images=imgs,visual_scope='Four disjoint official SP1000M tutorial photos; no sequence observation or button execution; Spotify8.4.67.886 visible in package list',not_read='small blurred permission text not transcribed',web_image_error='AK1006980 web click timed out; bounded direct public GET then HTTP200; no authorization denial',hands_on=False,visual_observed=True,compatibility_table_as_of='2025-05-01'))
caps={}
for eid,key in [('E-QGNT-01','gen2'),('E-QUA-01','gen3c')]:
    caps[eid]=dict(checked_on=date,access_mode='existing_official_pdf_selected_page_text_reread',hands_on=False,visual_observed=False,archived_artifact=str((qdir/'source-notes.md').relative_to(L)),sha256=sha(qdir/'source-notes.md'),source_files=[x for x in xs if x['key'].startswith(key)],pages=g2 if key=='gen2' else g3,artifact_hashes={str((qdir/'source-audit.json').relative_to(L)):sha(qdir/'source-audit.json')},note='本次明确页表，不把下载、全文提取或单批清单补集当全局阅读状态')
for x in ac:
    caps[x['evidence_id']]=dict(checked_on=date,access_mode=x['access_mode'],hands_on=False,visual_observed=False,archived_artifact=str((adir/'source-notes.md').relative_to(L)),sha256=sha(adir/'source-notes.md'),raw_work_path=x['raw_work_path'],raw_sha256=x['raw_sha256'],locator='full article; old SP1000M tutorial or dated compatibility table; separate E-AKOP-01 owns image observation',artifact_hashes={str((adir/'source-audit.json').relative_to(L)):sha(adir/'source-audit.json')})
ec=[]
for p in sorted((L/'batches').glob('*-evidence.jsonl')):
    es=[json.loads(t) for t in p.read_text().splitlines()];changed=False
    for e in es:
        if e['evidence_id'] in caps:
            before=json.loads(json.dumps(e));e.setdefault('additional_captures',[]).append(caps[e['evidence_id']]);changed=True
            ec.append(dict(owner=str(p.relative_to(R)),before=before,after=e))
    if changed:p.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in es))
assert len(ec)==4;dump(W/'old-evidence-updates.json',ec)
ah={str((adir/'source-audit.json').relative_to(L)):sha(adir/'source-audit.json')};ah.update({x['artifact_path']:x['sha256'] for x in imgs})
new_e=dict(evidence_id='E-AKOP-01',product_id='M-065',title='Open APP旧SP1000M教程四幅状态照片',url='https://www.iriver.jp/support/faq_974.php',checked_on=date,source_type='official_help_static_photos',access_mode='public_image_GET_and_root_view_image',version_scope='SP1000M shared regional tutorial; Spotify8.4.67.886 package list; no SR35 UI proof; actual build/date of photos unknown',app_build=None,firmware_version=None,source_updated_on=None,locator='AK1006978/6979/6980/6981.jpg: menu, package row, installer, login entry',note='静态原图；确认按钮存在不是执行成功，登录页不是登录成功；小字未读。',hands_on=False,visual_observed=True,visual_kind='official_help_static_photos',archived_artifact=str((adir/'source-notes.md').relative_to(L)),sha256=sha(adir/'source-notes.md'),artifact_hashes=ah,capture_state='original_photos_and_short_notes_archived',supersedes=None,derived_from='E-AK-06')
(L/'batches/quaderno-files-ak-states-evidence.jsonl').write_text(json.dumps(new_e,ensure_ascii=False)+'\n')
updates={
'F-M060-04':('E-QGNT-01','；Gen.2 p35–39核三种删除入口、回收站原目录恢复/同名改名；自建同名目录不是系统回收站，PC不能管理回收站','；两代按原页区分；设备删除尚占空间、PC及指定同步删除绕过回收站，不保证全部删除可回退'),
'F-M060-06':('E-QGNT-01','；Gen.2 p87–88/113核USB、完成提示、日期ZIP、双盘空间及备份目录不得同步；备份文件可选后删除','；备份删除后的确认/撤销与备份包含设置/关系的完整清单未知'),
'F-M060-07':('E-QGNT-01','；Gen.2 p87独立核确认/完成、删除较新数据、重写创建/修改时间和重置浏览时间，Samples排除','；不是合并导入；没有断点/所有元数据保留保证'),
'F-M060-08':('E-QGNT-01','；Gen.2 p89–90/92核App连接启动自动同步、连接期间默认60分钟、空目录不传及删除/双改规则','；自动触发、最新同步时间和文件完整性不能互代；重绑首次合并未知'),
'F-M060-11':('','；Gen.2 p91另核电脑搜索范围、结果按文档去重、双击在PC阅读器打开与关闭结果','；搜索结果打开不等于设备跳页或持久传出'),
'F-M060-12':('','；Gen.2 p110核休眠页面、保护/空间不足只读、PC操作占用及外部笔迹限制开关','；等待、解锁、释放空间、编辑权限是不同故障路径，未测实际恢复'),
'F-M060-16':('E-QGNT-01','；Gen.2 p111也独立核同一特定旧固件、一或两份修复副本及同名/空间失败','；仍不泛化到全部损坏或其他固件'),
'F-M065-06':('E-AKOP-01','；旧SP1000M图补Service列表中的包/版本、CANCEL/INSTALL确认及应用登录页；包存在、安装和服务授权分开','；仅静态旧机照片，取消结果与当前SR35安装/登录仍未知'),
'F-M065-07':('','；2026-10-27复读表内日期仍为2025-05-01，未获得当前测试记录','；维持D1，不因重新取得网页而提升兼容性结论')}
fc=[]
for p in sorted((L/'batches').glob('*-coverage.csv')):
    rs=rows(p);changed=False
    for r in rs:
        if r['flow_id'] in updates:
            old=r.copy();eid,known,gap=updates[r['flow_id']]
            if eid:r['evidence_ids']=join(r['evidence_ids'],eid)
            r['known_branches']+=known;r['gaps']+=gap;r['checked_on']=date
            if r['flow_id']=='F-M065-06':r['visual_status']='static_reference_only'
            fc.append(dict(owner=str(p.relative_to(R)),before=old,after=r.copy()));changed=True
    if changed:csvout(p,rs)
assert len(fc)==9;dump(W/'old-flow-updates.json',fc)
nf=[]
def flow(n,task,es,known,gap):nf.append(dict(flow_id=f'F-M060-{n:02d}',product_id='M-060',task=task,depth='D2',evidence_ids=es,known_branches=known,gaps=gap,visual_status='not_observed',hands_on='false',case_path='cases/M-060-quaderno.md',checked_on=date))
flow(18,'使用PC App传入、传出并区分临时预览','E-QGNT-01;E-QUA-01','Gen.2文件夹视图拖入/取出，打印先在电脑转PDF到Received；传出中断再传加(1)；Gen.3C电脑双击只读临时副本在App退出删除','两代分别查页；同名传入、取消残留、完整性、接收端持久保存未实测')
flow(19,'连接已配对设备并核对或切换目标','E-QUA-01;E-QGNT-01','Gen.3C已配对后USB/同Wi-Fi/受支持BT连接与PC状态核对；按ID切换/加配/解除；Gen.2故障核目标ID/时钟等','首次配对后半只写按提示；支持平台/BLE条件保留；解除授权的数据作用和中断恢复未核')
flow(20,'按入口整理文件及目录','E-QUA-01;E-QGNT-01','设备阅读/总列表复制在同目录，文件夹入口可选目的地；Gen.3C文件夹新建与移动；Gen.2PC内复制不跨电脑剪贴板','同名、取消、批量部分失败以及文件改名后side note/同步关系需分别验证')
flow(21,'在PC App添加、改名或删除笔记模板','E-QUA-01','Gen.3C设置→模板→添加→选PDF/命名→OK；删除确认，改名OK；多页仅第一页，加密PDF不可','同名冲突、删除对既有笔记影响、导出/恢复未核')
flow(22,'从书库和阅读页快速列表切换资料','E-QUA-01','Gen.3C总列表按类型/排序找文档，文件夹按路径打开；阅读页快速列表切同目录/最近/收藏','空结果、排序保存、返回原进度、异常/重启及统计语义未核')
csvout(L/'batches/quaderno-files-coverage.csv',nf)

# Only the explicitly read start pages change inventory status; not whole chapters.
inventory_changes=[]
for name,valid,gen in [('quaderno-gen2-ink-inventory.csv',g2,2),('quaderno-controls-deepening-inventory.csv',g3,3)]:
    p=L/'batches'/name;rs=rows(p)
    for r in rs:
        m=re.search(r'\bp(\d+)',r['locator'])
        if not m or int(m[1]) not in valid:continue
        pg=int(m[1]);old=r.copy()
        if gen==2:
            fmap={35:'04;20',36:'04;20',37:'04;20',38:'04',39:'04',85:'18',86:'18',87:'06;07',88:'06',89:'08',90:'08;04',91:'11',92:'20;08;04',110:'12',111:'16',112:'19',113:'06;18;19'};eid='E-QGNT-01'
        else:
            fmap={38:'22',39:'22',40:'22',41:'20',42:'20;04',43:'20;04',44:'20;04',45:'20;04',46:'04',89:'19;18',90:'19;20',91:'19',92:'19',93:'19',94:'19',103:'21',104:'18',105:'19'};eid='E-QUA-01'
        refs=';'.join('F-M060-'+x for x in fmap[pg].split(';'))
        r['flow_ids']=join(r['flow_ids'],refs);r['evidence_ids']=join(r['evidence_ids'],eid)
        r['coverage_status']='selected_body_pages_read_task_partial';r['gap']='本次核该起页正文，完整页表见quaderno-file-lifecycle/source-audit.json；不将父级整章判为已读；未操作与异常/恢复缺口保留'
        inventory_changes.append(dict(file=name,before=old,after=r.copy()))
    csvout(p,rs)
p=L/'batches/ak-directory-deepening-inventory.csv';rs=rows(p)
for r in rs:
    if r['inventory_id'] not in ['I-AKDD-170','I-AKDD-171']:continue
    old=r.copy();r['coverage_status']='body_reread_old_model_static' if r['inventory_id'].endswith('170') else 'dated_compatibility_body_reread_not_current_test'
    r['locator']='完整官方帮助正文；2026-10-27复读'
    r['gap']='旧SP1000M静态图不等于SR35当前安装链；取消/失败/重装数据未知' if r['inventory_id'].endswith('170') else '表内截至2025-05-01；当前登录/授权/兼容仍未验证，F07保持D1'
    if r['inventory_id'].endswith('170'):r['evidence_ids']=join(r['evidence_ids'],'E-AKOP-01')
    inventory_changes.append(dict(file=p.name,before=old,after=r.copy()))
csvout(p,rs);dump(W/'inventory-updates.json',inventory_changes)

rp=rows(L/'reference-points.csv');basis='official_manual_text_reconstruction';q='artifacts/quaderno-file-lifecycle/source-notes.md'
def ref(rid,pid,tags,fs,es,loc,pre,act,res,persist,gap,version,artifact=q,kind=basis):
    r={k:'' for k in rp[0]};r.update(reference_id=rid,product_id=pid,question_tags=tags,flow_ids=fs,evidence_ids=es,source_locator=loc,artifact_path=artifact,precondition=pre,action=act,visible_result=res,evidence_basis=kind,persistence_result=persist,unknown_branches=gap,version_scope=version,checked_on=date);rp.append(r)
v2='Gen.2 FMVDP41/FMVDP51; B6FK-5131-08 Z0-02; 2025-04; build unknown';v3='Gen.3C FMVDP43CA4/FMVDP53CA5; B5FL-2101-02 Z0-02; 2025-04; build unknown'
ref('RP-M060-05','M-060','导出;临时文件;预览','F-M060-18','E-QGNT-01;E-QUA-01','Gen.2 p86; Gen.3C p104','设备已连接PC App','取出到目录，或双击预览','取出与只读临时显示是不同入口','Gen.3C文档称退出App删除临时副本','异常退出;另存;文件完整性',v2+' / '+v3)
ref('RP-M060-06','M-060','导入;格式转换;资料准备','F-M060-18','E-QGNT-01','Gen.2 p85','原应用能打印且设备已连接','打印到QUADERNO','电脑转换PDF，传完设备打开，落Received','文档说明保存位置，未实际回读','转换失败;格式损失;重复内容',v2)
ref('RP-M060-07','M-060','备份;同步;删除','F-M060-06;F-M060-08','E-QGNT-01','Gen.2 p87/89','选择备份/同步目录','将两个用途分开配置','手册警告同步可能删除备份目录文件','同步不提供历史副本保留保证','误绑后恢复;并行;失败残留',v2)
ref('RP-M060-08','M-060','恢复;时间;进度;确认','F-M060-07','E-QGNT-01','Gen.2 p87','存在旧备份且USB连接','选备份→导入→确认→完成','文档称较新数据被删除，时间改写、浏览时间重置','不等于合并恢复；Samples不导入','中断回滚;对象完整清单;进度',v2)
ref('RP-M060-09','M-060','删除;回收站;作用范围','F-M060-04','E-QGNT-01;E-QUA-01','Gen.2 p35–39/90/92/113; Gen.3C p42–46','设备或PC/同步入口','删除文件，尝试从回收站恢复','设备普通删与PC/指定同步删路径不同','系统回收站有恢复条件，自建同名目录无该角色','永久删除;阈值边界;取消后对象',v2+' / '+v3)
ref('RP-M060-10','M-060','复制;目录;剪贴板;AI','F-M060-20','E-QGNT-01;E-QUA-01','Gen.2 p35–37/92; Gen.3C p43–45','阅读页、总列表或文件夹','复制/移动并指定目标','同名按钮依入口改变默认目的地与跨电脑可用性','未观察数据落盘或关系迁移','同名覆盖;取消;关联笔记',v2+' / '+v3)
ref('RP-M060-11','M-060','多设备;目标;连接;AI','F-M060-19','E-QUA-01','Gen.3C p105','已配对多机且不在同步中','工具→切换→确认→选ID→切换','目标由设备ID选定；添加与解除配对另有动作','没有家庭账号或跨机合并证据','误选;解除后文件/授权;重连',v3)
ref('RP-M060-12','M-060','连接;休眠;反馈;时钟','F-M060-19','E-QUA-01;E-QGNT-01','Gen.3C p92–94; Gen.2 p113','设备Wi-Fi/PC服务与目标已检查','核SSID和PC连接状态；处理指定时钟错误','服务连接与联网不同；Gen.2连接可校正设备时间','休眠Wi-Fi保持另有条件；未实测','休眠断连;时区;时钟修正的后果',v2+' / '+v3)
ref('RP-M060-13','M-060','模板;学习资料;导入;删除','F-M060-21','E-QUA-01','Gen.3C p103','PC App已连接','设置→模板添加/命名→OK，或删除确认','多页PDF仅第一页；加密PDF排除','模板与已有笔记的关系未核','同名;取消;删除对现有笔记影响',v3)
ref('RP-M060-14','M-060','同步;进度;触发;取消','F-M060-08','E-QGNT-01','Gen.2 p89–90/92','已绑定有效目录并连接PC App','启动/定时/手动同步，或解除绑定','文档有图标/最新时间；解除不删两端文件','最新时间不能证明每项完整；双改保留两版本','中断;部分完成;重绑首轮',v2)
a='artifacts/ak-openapp-states/source-notes.md';av='Official regional SP1000M tutorial photographs; Spotify8.4.67.886 visible; no SR35 operation/build verification'
ref('RP-M065-01','M-065','应用安装;包;状态','F-M065-06','E-AKOP-01','AK1006978/6979.jpg','旧教程Service菜单','查看APK条目和版本','列表中有包与安装图标，不等于装好','未观察任何安装结果','重复包;安装失败;发现延迟',av,a,'official_help_static_photos')
ref('RP-M065-02','M-065','确认;取消;应用安装','F-M065-06','E-AKOP-01','AK1006980.jpg','旧教程安装页','CANCEL或INSTALL入口','静态按钮可见，小字权限未辨认','按钮存在不证明取消无写入或可回滚','点击反馈;部分安装;存储不足',av,a,'official_help_static_photos')
ref('RP-M065-03','M-065','登录;服务授权;离线;兼容','F-M065-06;F-M065-07','E-AKOP-01;E-AK-07','AK1006981.jpg; table date2025-05-01','旧应用入口照片及历史兼容表','选择注册/登录入口','登录页可见，不证明账号或内容可用','未观察认证、订阅或离线数据','当前版本;旧缓存授权;重装恢复',av,a,'official_help_static_photos_and_dated_table')
assert len(rp)==135;csvout(L/'reference-points.csv',rp)

case=L/'cases/M-060-quaderno.md';t=case.read_text();old='Gen.2传出/同步/备份、删除及故障章节尚未核读；目前不能保证笔迹在传出再导入后仍可编辑。';assert old in t;t=t.replace(old,'上轮尚未核读的Gen.2传出/同步/备份、删除及故障章节，本轮已补所列页段；笔迹传出再导入后的可编辑性仍未知。');case.write_text(t)
add(case,'''## 2026-10-27续查：连接、文件入口与恢复后果

[本轮原页摘记](../artifacts/quaderno-file-lifecycle/source-notes.md)及[页表审计](../artifacts/quaderno-file-lifecycle/source-audit.json)分别列Gen.2的17页和Gen.3C的18页，含复读；没有看图或实际操作。两条原E追加采集，未新增独立来源。

| 原任务 | 本次补到的分支 | 仍需核对 |
| --- | --- | --- |
| F04、F06、F07、F08 | Gen.2分别核删除入口/系统回收站、USB备份完成、旧备份导入和自动/手动同步；PC删除与指定同步删除绕过设备回收站。 | ZIP与数据完整性、恢复中断、重绑与部分失败；回收站不等于所有删除可回退。 |
| F11、F12、F16 | Gen.2电脑搜索范围与结果打开；不能写的多种原因；特定旧固件修复副本。 | 搜索打开不是持久传出；等待、空间、保护、设备来源限制分开，修复不能泛化。 |

| F编号 | 入口、动作与文档反馈 | 版本与缺口 |
| --- | --- | --- |
| F-M060-18 PC传输与临时预览 | Gen.2进入目标文件夹拖入PDF或取出至电脑；打印在电脑侧转PDF并存Received；中断后同目标重传加(1)。Gen.3C双击在电脑只读显示，临时副本退出PC App删除。 | E-QGNT-01 p85–86、E-QUA-01 p104。预览、持久文件和续传不同；取消残留/完整性未知。 |
| F-M060-19 已配对连接与切换 | Gen.3C按USB/同Wi-Fi/受支持BT连接，核PC左上状态；多机工具页按ID切换，另可添加或解除。Gen.2故障页补目标ID和特定时钟错误。 | E-QUA-01 p89–94/105、E-QGNT-01 p112–113。初配后半仅按提示；BLE/macOS/休眠条件保留，解除后的数据与其他授权未知。 |
| F-M060-20 文件和目录整理 | 设备阅读页/总列表复制到同目录；文件夹入口可选目的地并移动。Gen.3C可新建目录；Gen.2 PC App内剪贴不跨电脑。 | E-QUA-01 p41–45、E-QGNT-01 p35–37/92。同名、取消、side note和同步关系变化未核。 |
| F-M060-21 管理PDF笔记模板 | Gen.3C PC设置→模板→添加→选PDF/命名→OK；删除确认、改名OK。 | E-QUA-01 p103。只用多页PDF第一页，加密PDF不可；现有笔记影响、冲突与恢复未知。 |
| F-M060-22 选择与切换阅读资料 | Gen.3C总列表按类型/排序找内容，文件夹按路径选；阅读页快速列表可切同目录/最近打开/收藏。 | E-QUA-01 p38–40。未读标记不证明掌握，原进度/排序持久化和空结果未核。 |

备份与同步目录不能混用：Gen.2手册直接警告同步可能删除备份文件。旧备份导入会删除较新数据、改文件时间并重置浏览时间；不是合并恢复。传输结束提示、同步时间、服务连接状态与真正持久保存分别登记。原275行Gen.3C目录及197行Gen.2目录只就地更新本次页段，不把父级整章算已读。
''')
add(L/'cases/M-065-astell-kern.md','''## 2026-10-27续查：旧安装照片与版本表

复读E-AK-06/07正文，并实际查看四幅官方旧SP1000M照片，另记E-AKOP-01；[原图、逐图摘记及审计](../artifacts/ak-openapp-states/source-notes.md)可复查。F06补Service菜单、包条目/版本、CANCEL与INSTALL页和应用注册/登录页；小字权限模糊未读，图片之间不是连续操作。包被发现、安装确认、应用启动与账号授权不能合为“已可用”。

本次兼容表仍标2025-05-01，F07继续D1。旧照片的Spotify版本与今天可下载/登录版本无等同关系；不把SP1000M的按钮当SR35当前安装界面。F06保持D2、仅增加静态参考，取消、失败、卸载、更新/重装及离线内容恢复仍未知。原202项台账只更新170/171两项；M3U流程缺口没有因本次复读解决。
''')
(W/'APPLIED').write_text('Applied once; do not rerun.\n')
print(json.dumps(dict(new_flows=5,old_flows_updated=9,new_evidence=1,old_evidence_captures=4,new_reference_points=13,inventory_updates=len(inventory_changes),inventory_by_file={k:sum(x['file']==k for x in inventory_changes) for k in sorted({x['file'] for x in inventory_changes})},requirements_sha256=sha(R/'docs/requirements.md')),ensure_ascii=False))
