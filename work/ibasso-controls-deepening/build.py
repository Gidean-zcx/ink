from pathlib import Path
import csv,json,hashlib,shutil
R=Path('.');L=R/'docs/research/interaction-library';W=R/'work/ibasso-controls-deepening';A=L/'artifacts/ibasso-controls-deepening';A.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,o):p.write_text(json.dumps(o,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rows,fields):
 with p.open('w',newline='') as f:
  wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(rows)
oldE={};oldF={};oldsource={}
for p in (L/'batches').glob('*evidence.jsonl'):
 if p.name.startswith('ibasso-controls-deepening'):continue
 for line in p.read_text().splitlines():
  if line.strip():x=json.loads(line);oldE[x['evidence_id']]=x
for p in (L/'batches').glob('*coverage.csv'):
 if p.name.startswith('ibasso-controls-deepening'):continue
 for x in csv.DictReader(p.open()):oldF[x['flow_id']]=x;oldsource[x['flow_id']]=str(p)
assert 'E-IBDC-01' not in oldE
captures=json.loads((W/'fetch-log.json').read_text());by={x['url']:x for x in captures}
manual=json.loads((W/'manual-metadata.json').read_text())
manifest=json.loads((W/'render-manifest.json').read_text())
# These six pages were individually opened with view_image before this build.
for m in manifest:
 m['observed']=True;m['observed_on']='2026-10-27';m['observation_method']='view_image';src=R/m['raw_work_path'];dst=A/src.name;shutil.copyfile(src,dst);m['archived_artifact']=str(dst.relative_to(L));m['sha256']=sha(dst)
dump(W/'render-manifest.json',manifest)
notes='''# DX180：目录、应用与USB用途补证

查阅2026-10-27。固定DX180英文历史63页手册、Android/Mango；PDF元数据生成/修改日均指向2024-06-13但时区不同，**不是官方发布日期或修订号**。本轮未重抓PDF，复用旧原件。新发布资料只补版本能力，不当现版操作验证。

## 旧材料复读（不新建同URL证据）

- E-IBCD-01：[官方PDF](https://www.ibasso.com/uploadfiles/download/DX180userguide.pdf)。印刷1目录，14–26系统，30–42曲库，45–54 EQ；PDF文件页=印刷页+1。本批新任务只依据17–18应用和20对外供电；无安装/供电实测。
- E-IBCD-08：[中文日志](https://ibasso.cn/support/dx180firmware)，正文5版本共37条声明及升级附注；本轮HTTP200与旧HTML SHA相同。v1.0标题与英文V1.01同日，保留原名不悄悄统一。1.04系统关机与Mango行为分开，中文疑误仍保留；1.05“始终有效”没有解释重置条件。
- E-IBA-03：[英文固件目录](https://ibasso.com/dx180-firmware/)，旧?p=3371对应同内容的规范路径。只读目录，未点击包；版本清单不是完整变更链。

## 新E-IBDC-01：日本区域计时解释

[MUSIN一手新闻稿](https://prtimes.jp/main/html/rd/p/000000144.000139013.html)，署名MUSIN、2025-07-22 17:00，段落“iBasso Audio DX180(v1.04)”第1–2条，把系统关机定时器与Mango定时停止播放分开。译意限此日本区域公告；它不自动改写中文疑误，也未给当前1.05设置/取消/重计或后台规则。DX340的Mango OS说明不属于DX180。

品牌[经销名录](https://ibasso.com/where-to-buy/)日本段列Musin Co., LTD及其域名邮箱，作为发布方关系凭据，非交互来源。同一机构[站内新闻](https://musinltd.com/News/1165.html)通过web工具正文可读、日期2025-07-22 13:33；直接HTTP采集断开，故**没有该URL的本地原HTML**。其下游portal/article/index.html?id=812返回444，停止该路。PR TIMES原HTML已得，版本号据其DX180小节，不凭未看的横幅。

## 静态页观察记录

2026-10-27实际逐页view_image；6页来自同一PDF，只做必要页面保存，不能当6个独立来源或连续操作。

| 印刷页（文件页） | 观察与限制 |
| --- | --- |
| 17（18） | 浏览器/下载列表示例，列表为空；正文提供两条安装路径，图未出现安装成功或权限确认。 |
| 18（19） | Settings→Apps→App info的并排示例；Open/Uninstall/Force stop及Permissions/Storage & cache入口。没有卸载确认/结束画面，不推其数据范围。 |
| 20（21） | 纯文字页说明连接后从通知栏USB用途启用对外充电；没有USB选项界面或运行反馈。 |
| 21（22） | 系统与主设置并排菜单；Backup/Reset options只入口，不据此造备份恢复链。 |
| 34（35） | Mango分类/文件导航、Recent Play及底部搜索框；没有搜索结果。 |
| 42（43） | 关键词框、Search history区和删除样式图标、底部播放条；未见键入/结果/清除确认，不能按图推历史清除规则。 |

原件SHA、页面文件/图SHA及取得方式见work/ibasso-controls-deepening下manifest；正式图在本目录。原9静态页仍由旧E引用，不重编号。所有hands_on=false；纯说明页20也只标“看过静态文字页”，不是观察USB界面。

## 台账及未决

正式inventory分手册目录父子项、正文功能、实际看过的系统/导航菜单，以及版本声明四类；不累加成完成任务数。菜单可见不表示进入过，标题不表示全链。标签只证明读取/显示和分类，不证明可编辑ID3。Time排序字段语义、搜索匹配/空结果、扫描失败、卸载数据/权限拒绝、USB退出/电量门槛和现版计时仍未知。Recent Play的50曲清单不是时间断点。
'''
(A/'source-notes.md').write_text(notes);note_rel=str((A/'source-notes.md').relative_to(L));note_sha=sha(A/'source-notes.md')
release=by['https://prtimes.jp/main/html/rd/p/000000144.000139013.html'];dist=by['https://ibasso.com/where-to-buy/']
e={'evidence_id':'E-IBDC-01','product_id':'M-064','title':'MUSIN DX180 v1.04区域计时变更说明','url':release['url'],'checked_on':'2026-10-27','source_type':'official_distributor_press_release','access_mode':'public_press_release_html_body_read','version_scope':'Japan distributor MUSIN; DX180 v1.04 only; not a current UI walkthrough','app_build':None,'firmware_version':'DX180 v1.04 (regional notice)','source_updated_on':'2025-07-22','locator':'MUSIN署名与发布时间；iBasso Audio DX180(v1.04)第1–2条','note':'系统关机定时与Mango定时停止播放的独立区域声明；中文原文疑误仍保留，不升当前版本D2。','hands_on':False,'visual_observed':False,'archived_artifact':note_rel,'sha256':note_sha,'capture_state':'short_note_and_raw_capture','raw_work_path':release['raw_work_path'],'raw_sha256':release['raw_sha256'],'supersedes':None,'provenance_captures':[dict(url=dist['url'],raw_work_path=dist['raw_work_path'],raw_sha256=dist['raw_sha256'],locator='Japan/Distributor Musin Co., LTD；域名邮箱',checked_on='2026-10-27')],'additional_captures':[dict(capture_url='https://musinltd.com/News/1165.html',checked_on='2026-10-27',access_mode='web_tool_body_read_no_local_html',raw_work_path=None,raw_sha256=None,locator='2025-07-22 13:33；DX180段第1–2条',limitation='direct HTTP remote closed connection; downstream portal returned444 and stopped')]}
(L/'batches/ibasso-controls-deepening-evidence.jsonl').write_text(json.dumps(e,ensure_ascii=False)+'\n')
new=[('13','按手册两条路径安装应用','印刷17：预装商店内检索/下载/安装，或浏览器官网取包→返回桌面下载管理→点已下载应用安装','未示安装成功/权限拒绝/取消/失败；现版商店及兼容性未知；未安装'),('14','从系统应用页卸载指定应用','印刷18：桌面Settings→Apps→选应用→Uninstall；文字终点为删除App，静态图只到App info','确认/取消、预装应用限制、卸载后的离线文件/歌单/账号数据与重装恢复未知'),('15','选择USB用途为外接设备供电','印刷20：连接设备→下拉通知栏底部USB项→选择用途并启用对连接设备充电；正文描述开始对外供电','仅文字步骤，无USB选项截图；结束/拔线、低电量、支持设备、电流及文件/DAC同时启用未知')]
rows=[]
for n,task,known,gaps in new:rows.append(dict(flow_id='F-M064-'+n,product_id='M-064',task=task,depth='D2',evidence_ids='E-IBCD-01',known_branches=known,gaps=gaps,visual_status='static_reference_only',hands_on='false',case_path='cases/M-064-ibasso.md',checked_on='2026-10-27'))
csvout(L/'batches/ibasso-controls-deepening-coverage.csv',rows,['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on'])
changes={
'01':dict(evidence_ids='E-IBA-01;E-IBA-02;E-IBCD-04;E-IBCD-01',known_branches='补USB复制/历史Mac MTP条件；Music左向右开分类/文件/底部搜索；All Music按Title/Album/Artist/Time/Folder排序；Position定位当前文件，目录Return逐级返回；关键词范围为曲目/专辑/艺人',gaps='Time排序语义、ID3编辑/标签冲突、空结果/搜索历史清除；授权、完成/取消、重复和路径修复；不由当前50曲清单推断时间断点'),
'06':dict(evidence_ids='E-IBA-02;E-IBCD-08;E-IBDC-01',known_branches='历史手册操作重计/到时关机；CN1.04.237系统定时与Mango变化分列且暂时播放疑误保留；JP MUSIN v1.04明确Mango计时停止播放；CN1.05.273新增始终有效仅声明',gaps='JP区域解释不自动统一CN；当前Mango结束动作/反馈、两计时关系、始终有效和操作重计规则仍缺完整链'),
'09':dict(known_branches='封面切曲、Music/Settings入口；Music子页缩略图回播放、Return逐页返回；Info点上半部收回；Recent Play限最近50曲，ID3年份仅显示',gaps='Info与曲库Delete对象；原筛选/滚动位置、历史清除、跨会话时间点；只读标签不等于编辑ID3'),
'10':dict(known_branches='GEQ5预设+自定义/10段；PEQ每设置最多6滤波器、8类型；Fc33–16000Hz/Gain−20–20dB/Q0.3–20；低通/高通/全通/Band/Notch的Gain不生效；逐项On/Off',gaps='越界输入/校验反馈、保存/重置、模式间持久化、跨app范围及音质未验证')}
updates=[]
for n,fields in changes.items():
 fid='F-M064-'+n;fields['checked_on']='2026-10-27'
 for field,after in fields.items():
  before=oldF[fid][field]
  if before!=after:updates.append(dict(flow_id=fid,source_batch=oldsource[fid],field=field,before=before,after=after,reason='旧任务补机制/版本边界，不改编号/深度；仅建议root合并'))
dump(W/'existing-coverage-updates.json',updates);csvout(W/'existing-coverage-updates.csv',updates,['flow_id','source_batch','field','before','after','reason'])
adds=[]
common=dict(checked_on='2026-10-27',archived_artifact=note_rel,sha256=note_sha,hands_on=False)
c=dict(common,capture_kind='local_reread_and_static_page_observation',network_refetched=False,raw_work_path=manual['raw_work_path'],raw_sha256=manual['raw_sha256'],original_acquisition_date='2026-10-26',locator='印刷1目录；17–21应用/USB/设置；30–42导航搜索；45–54参数；全文页映射',visual_observed=True,visual_kind='official_manual_static_pages_only',visual_audit_path=note_rel,rendered_artifacts=[m['archived_artifact'] for m in manifest],artifact_hashes={m['archived_artifact']:m['sha256'] for m in manifest},limitation='6图同一旧PDF；20是纯文字页，无连续流程')
for eid,cap in [('E-IBCD-01',c),('E-IBCD-08',dict(common,capture_kind='public_http_refetch_same_body_hash',network_refetched=True,raw_work_path=by['https://ibasso.cn/support/dx180firmware']['raw_work_path'],raw_sha256=by['https://ibasso.cn/support/dx180firmware']['raw_sha256'],capture_url='https://ibasso.cn/support/dx180firmware',locator='5版本37项声明及更新提示',visual_observed=False)),('E-IBA-03',dict(common,capture_kind='public_http_canonical_url_read',network_refetched=True,raw_work_path=by['https://ibasso.com/dx180-firmware/']['raw_work_path'],raw_sha256=by['https://ibasso.com/dx180-firmware/']['raw_sha256'],capture_url='https://ibasso.com/dx180-firmware/',locator='V1.01–V1.05/驱动目录；旧URL?p=3371',visual_observed=False))]:
 before=oldE[eid].get('additional_captures');adds.append(dict(evidence_id=eid,field='additional_captures',before=before,after=(before or [])+[cap],reason='沿用旧E，不新编号同PDF/同发布页面'))
dump(W/'additional-captures-updates.json',adds)
print('evidence 1 newFlows',len(rows),'oldFields',len(updates),'oldCaptures',len(adds),'images',len(manifest))
