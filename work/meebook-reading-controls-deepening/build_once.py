from pathlib import Path
import json,csv,copy,hashlib,shutil
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches';W=R/'work/meebook-reading-controls-deepening';A=L/'artifacts/meebook-reading-controls-deepening'
assert not A.exists(),'one-time build';A.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
notes='''# M7：设置、实际模式与应用按键

查阅2026-10-27。五个公开来源，不是五次设备验证。没有操作M7、安装KOReader或修改按键文件；没有新图像观察。旧指南/销售商日志与本批历史作者体验、2026问题报告分别保存。

## E-MRCD-01：Helpix，2024-09-07，Alex

[原文](https://www.helpix.ru/opinion/202409/60517-meebook_m7.html)，定位Использование/Софт，暂存文本713–1045行。本次HTTP200；此前web再次定位超时，普通原址取回成功。作者接触过不同M7，未给各自系统构建。

作者描述下拉面板选Regal/Normal/Fast/A2，以顶栏模式标识查看状态；应用图标长按可优化，但他报告选定模式与进入App后的模式不一致，原因未定。User Settings的Hide Store和首页样式、Read返回书库有文字路径。原生Notes与第三方读书记录不同；文中的集中摘录是朋友制作的工具，不是原生导出。Wi-Fi传书的私网地址仅是作者实例；Factory reset只给入口，未给确认和恢复链。

原页选定七张相关图，第一张所用cdn1.picturebb.com发生DNS解析失败，随即停下，其余未请求，故没有截图支持。图像失败与型号版本差异见[采集记录](acquisition.json)、[图像记录](images-acquisition.json)。未采用作者的续航归因、厂商关系猜测、性能或灰阶效果作为技术结论。

## E-MRCD-02：KOReader问题16100，2026-09-22

[原始报告与回应](https://github.com/koreader/koreader/issues/16100)。作者自报M7/Android11、KOReader2026.07.1：开EPUB后按键不翻页，其他程序能识别92/93。成员给出291/292到RPgBack/RPgFwd的映射，说明自己无M7；报告者随后反馈有帮助并关闭问题。没有日志和完整配置前后录像。两组数字不可直接等同，不能据关闭状态断言所有M7已兼容，亦不证明上游代码已修复。

## E-MRCD-03/04：项目Wiki的应用内映射方法

[Keymapping](https://github.com/koreader/koreader/wiki/Keymapping)显示编辑日2026-10-13：在koreader/settings/event_map.lua为整数事件关联动作，按设备查实际键。只读定义与方法段，不套Kobo/PocketBook示例。

[Android tips and tricks](https://github.com/koreader/koreader/wiki/Android-tips-and-tricks)显示编辑日2026-03-17：不同设备可能使用不同键码，非launcher应用不能覆盖Home行为。这是项目文档范围；未核M7系统长按、Logo键角色或每个App。没有执行配置或验证重启加载/冲突/回退。

## E-MRCD-05：作者自配方案，不是内置产品能力

[Bluemoondragon07的M7体验及首条补充](https://www.reddit.com/r/eink/comments/1825l9d/meebook_m7_review_my_optimizations/)，历史帖、精确日期/固件未核。先由web工具读取正文与评论；之后原址普通GET403，停止，不尝试API/其他域名或登录。拒绝HTML不是正文原件。

作者另用书库管理器把书交给KOReader；并描述用外部按键映射器记录触发键、指定滑动动作、限制前台App、保存，再复制反向动作。软件名称/版本及权限初始化未核，坐标属于其设备配置，不作为通用方案；未证明原生书库共享进度。只保留文本记录，不声称看过帖内图片或亲自配置。

## 取用结论

F04补同型号问题与应用侧方法，仍D2、非实测；F02/05/06增加范围限定，不升级D1。新增F07仅为历史文字重建的刷新模式任务。首页、原生笔记导出、重置确认/恢复等只进聚焦目录，仍缺完整结果。系统配置、App实际状态和动作结果值得分别观察，这是给Plus原型的建议，不承诺相同兼容性或刷新效果。
'''
N=A/'source-notes.md';N.write_text(notes)
for name in ['acquisition.json','images-acquisition.json']:shutil.copyfile(W/name,A/name)
acq=json.loads((W/'acquisition.json').read_text());specs=[('helpix','2024-09-07','历史M7操作和应用模式不一致报告','historical_firsthand_device_review','Использование/Софт; root read text713–1045; no images'),('issue16100','2026-09-22','M7实体键问题及映射反馈','upstream_issue_firsthand_report','description and all three comments; user-reported KOReader2026.07.1'),('keymapping','2026-10-13','KOReader应用内按键映射文档','project_wiki','What is key mapping / How does key mapping work / generic key discovery'),('androidkeys','2026-03-17','KOReader Android按键范围与Home限制','project_wiki','Customize keys before device-specific examples'),('reddit',None,'M7作者外部书库与按键映射方案','historical_firsthand_user_post','main post Reading and first author comment; web extraction only')]
es=[];existing=[json.loads(l) for p in B.glob('*-evidence.jsonl') for l in p.read_text().splitlines() if l.strip()]
for i,(key,date,title,stype,loc) in enumerate(specs,1):
 r=next(x for x in acq if x['key']==key);e=dict(evidence_id=f'E-MRCD-0{i}',product_id='M-057',title=title,url=r['url'],checked_on='2026-10-27',source_type=stype,access_mode='public_web_text_then_HTTP403_stopped' if key=='reddit' else 'public_text_and_ordinary_GET',version_scope='M7 historical/individual report; device firmware unknown' if key in ['helpix','reddit'] else 'KOReader reported2026.07.1' if key=='issue16100' else 'mutable project wiki as inspected; not device-tested',app_build='2026.07.1 (reporter stated)' if key=='issue16100' else None,source_updated_on=date,locator=loc,note='没有设备操作或新视觉；作者报告、项目文档及已知缺口见独立摘记',hands_on=False,visual_observed=False,archived_artifact=str(N.relative_to(L)),sha256=sha(N),supersedes=None)
 if key=='reddit':e.update(access_failure_path=r['path'],access_failure_sha256=r['sha256'])
 else:e.update(raw_work_path=r['path'],raw_sha256=r['sha256'])
 assert all(x['evidence_id']!=e['evidence_id'] and x['url']!=e['url'] for x in existing);es.append(e)
(B/'meebook-reading-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in es))
old=json.loads((W/'coverage-before.json').read_text());patch={
'F-M057-02':dict(evidence_ids='E-MEE-03;E-MRCD-01',known_branches='指南的同局域网入口与2024作者Wi-Fi Book Transfer实例分开；私网地址是实例，不是通用固定地址。',gaps='认证、完成/失败、取消、半文件及重复导入未知；作者可传文件不证明书架索引或原生进度同步。'),
'F-M057-04':dict(evidence_ids='E-MEE-04;E-MRCD-02;E-MRCD-03;E-MRCD-04;E-MRCD-05',known_branches='历史系统长按/应用优化入口沿旧日志；2026.07.1个案按键无翻页，经成员给应用内映射后作者反馈有帮助。项目Wiki说明事件映射文件；外部滑动映射是另一作者自配方案。',gaps='系统固件/实际键码、92/93与291/292关系、完整前后配置/重启加载、旋转冲突和全App效果未核；issue关闭不是通用兼容保证。'),
'F-M057-05':dict(evidence_ids='E-MEE-03;E-MEE-04;E-MRCD-01;E-MRCD-05',known_branches='京东记录同步仅旧日志声明；2024作者原生Notes范围、第三方自制摘录工具和另一作者的外部书库/KOReader组合分别记录，不当统一同步。',gaps='安装完整提示、同步触发/完成、原生批注导出、跨App/账户/断网恢复仍未核；其他App可打开书不证明共享进度。'),
'F-M057-06':dict(evidence_ids='E-MEE-03;E-MEE-04;E-MRCD-01',known_branches='清任务与Snow后台入口沿旧指南；2024作者补设置页箭头→Factory reset或Android Settings→System→Reset options两个入口，未展示执行。',gaps='单项控制/撤销、冻结后重开、重置确认/数据删除范围/恢复均未核；不能用找到重置入口代表完成恢复任务。')}
changes=[]
for f in old:
 if f['flow_id'] in patch:
  a=copy.deepcopy(f);a.update(patch[f['flow_id']]);changes.append(dict(flow_id=f['flow_id'],before=f,after=a))
for p in B.glob('*-coverage.csv'):
 rows=list(csv.DictReader(p.open()));modified=False
 for i,f in enumerate(rows):
  for c in changes:
   if f['flow_id']==c['flow_id']:assert f==c['before'];rows[i]=c['after'];modified=True
 if modified:
  with p.open('w',newline='') as h:w=csv.DictWriter(h,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
f=dict(flow_id='F-M057-07',product_id='M-057',task='切换刷新模式并核对应用实际模式',depth='D2',evidence_ids='E-MRCD-01',known_branches='2024作者描述下拉快设选Regal/Normal/Fast/A2及顶栏模式；长按App优化可另设模式，但报告进App未沿所设模式而改用快设。',gaps='固件/文图互证、全局与App优先级、退出恢复和故障原因未核；无帧率/触摸延迟/残影实测，不保证当前所有M7。',visual_status='not_observed',hands_on='false',case_path='cases/M-057-meebook.md',checked_on='2026-10-27')
with (B/'meebook-reading-controls-deepening-coverage.csv').open('w',newline='') as h:w=csv.DictWriter(h,fieldnames=list(f),lineterminator='\n');w.writeheader();w.writerow(f)
tasks=[('下拉刷新模式','01','07'),('顶栏实际模式','01','07'),('按App优化与不一致报告','01','07'),('Hide Store入口','01',''),('首页样式/Read返回','01',''),('原生Notes范围','01','05'),('自制摘录工具排除','01','05'),('Wi-Fi地址实例','01','02'),('Factory reset两入口','01','06'),('语言/设备构建差异','01',''),('按键失效复现步骤','02','04'),('92/93与291/292不等同','02','04'),('反馈与关闭非代码修复','02','04'),('event_map.lua','03','04'),('查实际事件键','03','04'),('Android厂商差异','04','04'),('Home行为范围','04','04'),('外部书库打开KOReader','05','05'),('前台App限制的外部滑动映射','05','04'),('映射保存/复制反向','05','04')]
rs=[dict(item_id=f'MRCD-{i:02d}',product_id='M-057',functional_area=t,evidence_ids='E-MRCD-'+e,flow_ids='F-M057-'+fi if fi else '',review_status='historical_report_or_project_document_text_only',gap='范围与未核结果见来源摘记；非实机/全功能目录',checked_on='2026-10-27') for i,(t,e,fi) in enumerate(tasks,1)]
with (B/'meebook-reading-controls-deepening-inventory.csv').open('w',newline='') as h:w=csv.DictWriter(h,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
C=L/'cases/M-057-meebook.md';assert C.read_bytes()==(W/'case-before.md').read_bytes();s=C.read_text().replace('查阅2026-10-26；2026-10-27补目录与来源归档。','查阅2026-10-26；2026-10-27续补历史体验、项目按键文档和问题反馈。').replace('本轮没有新增F。','前批没有新增F；本次新增F07见末节。')
s+='''\n\n## 刷新与按键的执行范围（2026-10-27续补）\n\n新增E-MRCD-01–05，见[原文定位、历史范围与访问限制](../artifacts/meebook-reading-controls-deepening/source-notes.md)、[20个聚焦条目](../batches/meebook-reading-controls-deepening-inventory.csv)。新增F-M057-07“切换刷新模式并核对应用实际模式”（D2、仅历史文字重建），补F02/04/05/06；F05/06仍D1。\n\nF07区分系统模式、应用优化选项和进入App后的状态；作者报告不一致，原因未知，没有取得配图或实测。F04区分旧系统设置、KOReader2026.07.1个案、项目Wiki和另一个作者的外部映射方案；报告关闭不代表固件或所有App已兼容。两组键码保留原样，不猜其转换关系。\n\n本批没有取得明确M7构建的完整手册，也没有统一笔记/同步/恢复闭环。Helpix图片域名DNS失败，Reddit普通GET403均停止；web此前取得的正文与失败响应分别记录。旧403网站未重试，0新图像、0实操；项目仍只考虑Plus，相关启发不升级为需求。\n''';C.write_text(s)
(W/'coverage-updates-applied.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2)+'\n')
formal=[*A.iterdir(),C,*B.glob('meebook-reading-controls-deepening-*')];(W/'manifest.json').write_text(json.dumps({str(p.relative_to(R)):sha(p) for p in formal},indent=2)+'\n')
print(json.dumps(dict(new_e=5,new_f=1,old_f=4,old_fields=sum(sum(c['before'][k]!=c['after'][k] for k in c['before']) for c in changes),inventory=20,new_visuals=0),ensure_ascii=False))
