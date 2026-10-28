import csv,hashlib,io,json
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch54-integration';A=L/'artifacts/quizlet-media-and-sharing'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def js(x):return json.dumps(x,ensure_ascii=False,indent=2)+'\n'
def rows(p):return list(csv.DictReader(p.open()))
def csvtext(rr):
    f=io.StringIO(newline='');w=csv.DictWriter(f,fieldnames=list(rr[0]));w.writeheader();w.writerows(rr);return f.getvalue()
def jlines(rr):return ''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rr)
assert not (L/'batches/quizlet-media-and-sharing-evidence.jsonl').exists()
assert not (L/'batches/quizlet-media-and-sharing-coverage.csv').exists()
src=json.loads((W/'source-map.json').read_text());assert len(src)==23
# locator / task IDs / missing branches, keyed by capture order rather than tuple offsets.
spec={
'01':('L17–45','F-M021-24','图库卡面措辞不一；无结果、取消、移除撤销和保存时点'),
'02':('L17–65','F-M021-24','移动仅释义侧；拒权、替换旧图、缩放质量与同步'),
'03':('L17–47','F-M021-24','录音局部保存与整集提交、重录覆盖/撤销、时长及离线'),
'04':('L17–48','F-M021-25','图替换/点位、模糊撤销、MBP格式原文未验证'),
'05':('L15–21','F-M021-04;F-M021-05','私有/密码/班级登录/未发布分开；无错误画面或同步时点'),
'06':('L17–52','F-M021-27','公开与密码集分享措辞并存；访问结果、撤销和副本后果'),
'07':('L17–76','F-M021-04','Editable by与Visible to联动、撤权与并发修改'),
'08':('L17–65','F-M021-04','查看密码与编辑密码关系、旧会话/副本及生效时点'),
'09':('L15–36','F-M021-26','标签层级、重命名/删除、重名、迁移与权限'),
'10':('L17–76','F-M021-26','高亮与提交、移除/删除后果、空夹/混合内容权限'),
'11':('L15–47','F-M021-30','满员、拒绝/撤回、离组后数据；不合并Study with friends'),
'12':('L15–17','','仅称号标准背景，无完整操作步骤或准确性保证'),
'13':('L15–23','F-M021-31','刻度、取消、重复提交、聚合规则'),
'14':('L15–22','F-M021-31','评分可改不可删；历史和更新时间未知'),
'15':('L15–20','F-M021-31','评分不可关闭；作者不收到通知，身份可见性未知'),
'16':('L15–33','','仅内容模型背景，非完整功能或套餐表'),
'17':('L17–63','F-M021-28','公共搜索筛选不证明准确；排序/空结果和来源版本'),
'18':('L17–52','F-M021-29','申请不等于批准；等待、拒绝/撤回和权限时点'),
'19':('L15–39','F-M021-32','主页问答、推荐控制、短活动玩法与跳回状态缺失，D1'),
'20':('L17–34','F-M021-27','复制HTML不证明嵌入可运行、身份/进度与源权限更新'),
'21':('L17–43','F-M021-29','退出、所有者删除与重新加入分开；历史恢复未知'),
'22':('L17–48','F-M021-24','旧OS/浏览器式排查；重载后的未保存录音未知'),
'23':('L17–69','F-M021-32','13国设置范围、移动Manage subscription路径及推荐生效未知')}
oldes=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];oldem={e['evidence_id']:e for e in oldes}
captures=[]
for d in src:
    eid='E-QZMS-'+d['n'];loc,fids,gaps=spec[d['n']]
    assert eid not in oldem and not any(e['url']==d['url'] for e in oldes)
    p=R/d['raw_work_path'];assert d['title'] in p.read_text() and d['url'] in p.read_text()
    captures.append(dict(evidence_id=eid,title=d['title'],url=d['url'],entry_url=d['entry_url'],body_locator=loc,
        body_read_by_root=True,raw_work_path=d['raw_work_path'],raw_sha256=sha(p),raw_capture_kind='web_tool_response_text_not_source_HTML',flow_ids=fids.split(';') if fids else [],hands_on=False,visual_observed=False))
audit=dict(checked_on='2026-10-27',sources=captures,original_directory_articles_read_this_batch=21,linked_articles_read=2,
    directories=dict(raw_work_path=str((W/'quizlet-directories.txt').relative_to(R)),sha256=sha(W/'quizlet-directories.txt')),
    account_used=False,images_viewed=False,operation='public_help_reading_only',limitations='No actual app or account operation; background pages not independent tasks, personalized home remains D1')
audittext=js(audit);audithash=hashlib.sha256(audittext.encode()).hexdigest();writes={A/'source-audit.json':audittext}
newes=[]
for d,c in zip(src,captures):
    loc,fids,gaps=spec[d['n']]
    newes.append(dict(evidence_id=c['evidence_id'],product_id='M-021',title=d['title'],url=d['url'],checked_on='2026-10-27',
        source_type='official_help_background' if d['n'] in ['12','16'] else 'official_help',access_mode='opened_text_via_web_tool',
        version_scope='en-US公开帮助，平台/资格按正文；精确构建与修订日未知，录音故障页含旧系统例子',app_build=None,firmware_version=None,source_updated_on=None,
        locator=loc,note=gaps,hands_on=False,visual_observed=False,archived_artifact='artifacts/quizlet-media-and-sharing/source-notes.md',sha256=sha(A/'source-notes.md'),supersedes=None,
        raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],capture_state='saved_web_tool_response_no_source_HTML_no_images',
        artifact_hashes={'artifacts/quizlet-media-and-sharing/source-audit.json':audithash}))
writes[L/'batches/quizlet-media-and-sharing-evidence.jsonl']=jlines(newes)
newdata=[
('24','给词卡添加图片和自定义语音','D2','E-QZMS-01;E-QZMS-02;E-QZMS-03;E-QZMS-22','图库/自有图和平台卡面范围；按住空格录音、松开回放、重录/删除与整集保存；旧式故障排查','取消、替换/重录撤销、局部与整集提交时点、拒权后草稿与离线'),
('25','创建图像位置标注练习','D2','E-QZMS-04','网站一图加点/词及可选释义，至少两点保存；模糊文字、移除点；手机学已有图示','图替换/点位重排、模糊撤销、错误输入与同步；MBP原文格式未验证'),
('26','创建文件夹和标签并归集材料','D2','E-QZMS-09;E-QZMS-10','网站从内容加入、手机筛选高亮后完成；文件夹内建标签、内容Saved选择；可见性和年龄条件','删夹/移除内容、标签重名/嵌套/迁移、空夹和混合对象权限、撤销同步'),
('27','分享学习内容与嵌入学习活动','D2','E-QZMS-06;E-QZMS-20','分享链接/渠道按身份平台；密码另传；网站选活动复制HTML，嵌入可切模式或回原集','公开/密码措辞范围、收件访问与撤销、外部身份/进度、源权限变化'),
('28','搜索和筛选公开学习集','D2','E-QZMS-17','按主题和作者搜索；词数/作者类型/内容筛选，网站另列学校课程；点标题学习，编辑他人集另复制','排序、零结果、筛选反馈、质量校验和来源版本；不等于个人库关键词查找'),
('29','申请加入及退出教师班级','D2','E-QZMS-18;E-QZMS-21','找教师/班级后申请，教师接受才可访问私有集；退出和手机确认；所有者不能退出；误退管理员可加回','等待/拒绝/撤回、重加后的进度/资料恢复、权限时点与所有者删除后果'),
('30','建立Study Group并管理成员关系','D2','E-QZMS-11','公开集建组/命名/分享邀请；登录接受；加集、切组、离组，最多8人；Track Progress看卡分类','满员、拒绝/撤回、离组后记录、实时状态；与Study with friends关系未明'),
('31','提交修改并查看学习集评分','D2','E-QZMS-13;E-QZMS-14;E-QZMS-15','网站留评分/提交、改评分/提交；可改不可删、不能关闭；作者看总体且无通知','刻度、取消/重复提交、历史、聚合更新时间及评分身份'),
('32','移动首页推荐与学习入口','D1','E-QZMS-19;E-QZMS-23','问答后个性化首页；Study Bites、进度卡及推荐；已学习才激活短活动；学校/课程设置有地区条件','首次问答、短活动启动/结束、推荐理由/控制、跳回位置及偏好生效未闭合')]
newfs=[]
for no,task,depth,eids,known,gaps in newdata:
    newfs.append(dict(flow_id='F-M021-'+no,product_id='M-021',task=task,depth=depth,evidence_ids=eids,known_branches=known,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-021-quizlet.md',checked_on='2026-10-27'))
writes[L/'batches/quizlet-media-and-sharing-coverage.csv']=csvtext(newfs)
fc=[]
for p in (L/'batches').glob('*-coverage.csv'):
    rr=rows(p);dirty=False
    for r in rr:
        if r['flow_id'] not in ['F-M021-04','F-M021-05']:continue
        before=r.copy()
        if r['flow_id']=='F-M021-04':
            r['evidence_ids']+=';E-QZMS-05;E-QZMS-07;E-QZMS-08';r['known_branches']+='；Editable by与Visible to入口分开；密码可改、移动确认；访问失败分登录/密码/私有/未发布';r['gaps']+='；编辑与查看权限联动、撤权/旧密码生效及旧副本影响未知'
        else:
            r['evidence_ids']+=';E-QZMS-05';r['known_branches']+='；跨端找不到先核是否发布，不能把未发布草稿当已同步';r['gaps']+='；发布后可见时点与同步失败界面未知'
        r['checked_on']='2026-10-27';fc.append(dict(file=str(p.relative_to(R)),before=before,after=r.copy()));dirty=True
    if dirty:writes[p]=csvtext(rr)
assert len(fc)==2;writes[W/'old-flow-updates.json']=js(fc)
ec=[]
for p in (L/'batches').glob('*-evidence.jsonl'):
    ee=[json.loads(s) for s in p.read_text().splitlines()];dirty=False
    for e in ee:
        if e['evidence_id'] not in ['E-QZCD-01','E-QZCD-02','E-QZCD-04']:continue
        before=json.loads(json.dumps(e));e.setdefault('additional_captures',[]).append(dict(checked_on='2026-10-27',access_mode='opened_text_via_web_tool',raw_work_path=str((W/'quizlet-directories.txt').relative_to(R)),raw_sha256=sha(W/'quizlet-directories.txt'),note='复核原目录并定位剩余21篇正文；追加两篇关联文章不当原章节增加',hands_on=False,visual_observed=False))
        ec.append(dict(file=str(p.relative_to(R)),before=before,after=e));dirty=True
    if dirty:writes[p]=jlines(ee)
assert len(ec)==3;writes[W/'old-evidence-updates.json']=js(ec)
p=L/'batches/quizlet-controls-deepening-inventory.csv';rr=rows(p);bytitle={d['title']:d for d in src[:21]};ic=[]
for r in rr:
    if r['functional_domain'] not in bytitle:continue
    d=bytitle[r['functional_domain']];assert r['coverage']=='仅目录；正文未在本例展开';before=r.copy();loc,fids,gaps=spec[d['n']]
    r.update(article_url=d['url'],evidence_ids=r['evidence_ids']+';E-QZMS-'+d['n'],flow_ids=fids,
        coverage='正文背景；非独立操作任务' if d['n'] in ['12','16'] else ('D1；正文已读但主页交互主链不足' if d['n']=='19' else 'D2部分；正文已读，未操作'),
        missing_controls_or_branches=gaps,checked_on='2026-10-27')
    ic.append(dict(kind='old_directory_body',before=before,after=r.copy()))
assert len(ic)==21 and len(rr)==68
for d,order,parent,origin in [(src[21],'69','11',src[2]),(src[22],'70','28',src[18])]:
    loc,fids,gaps=spec[d['n']]
    r={k:'' for k in rr[0]};r.update(order=order,entry_kind='linked_article',parent_order=parent,functional_domain=d['title'],source_url=origin['url'],source_section=origin['title']+' / related help outside original section snapshot',article_url=d['url'],evidence_ids='E-QZMS-'+origin['n']+';E-QZMS-'+d['n'],flow_ids=fids,
        coverage='正文已读；旧式排查步骤，未操作' if d['n']=='22' else '正文已读；关联推荐任务仍D1',missing_controls_or_branches=gaps,checked_on='2026-10-27')
    rr.append(r);ic.append(dict(kind='linked_article_added',before=None,after=r.copy()))
writes[p]=csvtext(rr);writes[W/'inventory-updates.json']=js(ic)
rpdata=[
('18','录音;保存;回放;编辑','F-M021-24','E-QZMS-03;E-QZMS-22','custom audio steps and troubleshooting','网站词卡编辑，有录音资格','按住录音，松开回放，之后保存卡组','帮助区分单段录音与整集完成','不能由回放推断整集已持久提交','重录覆盖/撤销、重载和中断'),
('19','图片;卡面;平台;儿童','F-M021-24','E-QZMS-01;E-QZMS-02','gallery and own images platform sections','图片输入及平台资格','选图库或上传自有图片','手机自有图限释义面，网站概述范围更广','图片缩放及替换结果未实际核','图库卡面措辞、拒权、同步与质量'),
('20','图示;位置;保存;遮盖答案','F-M021-25','E-QZMS-04','diagram create and tips','一张图及订阅资格','加至少两点及标签后创建，可模糊文字','点提交与整集创建分开','未证明图片替换会保留点位','模糊/点删除撤销、格式MBP与同步'),
('21','文件夹;标签;可见性;保存','F-M021-26','E-QZMS-09;E-QZMS-10','folder/tag create and add','文件夹与选定材料','创建标签、选择内容并完成','手机选中有高亮，另有Done','容器和卡组可见性关系需按文档范围核','空夹/混合内容、删除/迁移和嵌套'),
('22','分享;密码;访问;撤销','F-M021-27;F-M021-04','E-QZMS-05;E-QZMS-06;E-QZMS-08','sharing, access and password','对象可见性及接收者身份','发链接/密码，访问时检查资格','文档分别说明链接与密码访问','没有撤销已复制/下载资料依据','公开/密码分享措辞、失败及失效时点'),
('23','权限;编辑;查看;并发','F-M021-04','E-QZMS-07;E-QZMS-08','Editable by and Visible to','自建集设置','分别选择可编辑者和查看密码','入口字段不同，Just me私有措辞未证明联动','撤权和密码修改的旧会话影响未知','并发修改、旧副本及回退'),
('24','班级;申请;批准;状态','F-M021-29','E-QZMS-18','find class / request','登录并找到教师班级','申请加入，等待教师接受','接受后才可看班级私有集','申请不等于成员资格生效','拒绝/撤回、通知与等待显示'),
('25','班级;退出;恢复;身份','F-M021-29','E-QZMS-21','drop class / recovery','已加入且不是所有者','退出；误退由管理员重新加入','所有者没有退出项','重加不证明进度/内容恢复','删除班级、离线副本和历史'),
('26','小组;邀请;进度;成员','F-M021-30;F-M021-21','E-QZMS-11;E-QZAS-04','Study Groups vs Study with friends','公开集；不同会话入口','建组、接受邀请、加集或离组','Groups需登录、最多8人；朋友会话有匿名试用','两模式关系及离开后数据未说明','满员/拒绝/撤回、同步和历史'),
('27','评分;修改;删除;通知','F-M021-31','E-QZMS-13;E-QZMS-14;E-QZMS-15','rating/change/view','网站学习集','提交或修改评分，作者看总体','可改不可删，不能关闭，作者无通知','聚合更新时间/历史未知','取消、重复提交、身份和评价质量'),
('28','搜索;范围;筛选;质量','F-M021-28;F-M021-23','E-QZMS-17;E-QZAS-08','public search vs library keyword','公开发现或个人库检索','按主题/作者筛选，或限定Recent/Created','返回范围不同；编辑他人集另需复制','结果存在不证明已拥有副本或准确','排序、无结果、来源版本和校验'),
('29','推荐;首页;偏好;年龄','F-M021-32','E-QZMS-19;E-QZMS-23','mobile home and school settings','首页13岁以上，学校设置另有限定','入门后见推荐，可按资格改学校/课程','短活动在学习后出现；主链仍D1','偏好保存不证明推荐已改变或清除历史','问答/短活动、关闭推荐、理由及跳回位置'),
('30','嵌入;模式;身份;进度','F-M021-27','E-QZMS-20','Embed / copy HTML','网站公开学习集','选模式复制HTML放入外部页面','帮助允许切学习模式或回原集','未运行嵌入，身份/学习记录是否延续未知','外部环境失败和源权限变化；不是控制API')]
p=L/'reference-points.csv';rr=rows(p);fields=list(rr[0]);existing={r['reference_id'] for r in rr}
for no,tags,fids,eids,loc,pre,act,result,persist,unknown in rpdata:
    rid='RP-M021-'+no;assert rid not in existing
    r={k:'' for k in fields};r.update(reference_id=rid,product_id='M-021',question_tags=tags,flow_ids=fids,evidence_ids=eids,source_locator=loc,artifact_path='artifacts/quizlet-media-and-sharing/source-notes.md',precondition=pre,action=act,visible_result=result,evidence_basis='official_help_text_reconstruction; no actual operation or image observation',persistence_result=persist,unknown_branches=unknown,version_scope='2026-10-27公开帮助；平台/资格逐条限定，构建未知',checked_on='2026-10-27');rr.append(r)
writes[p]=csvtext(rr)
writes[W/'integration-summary.json']=js(dict(new_evidence=23,new_flows=9,new_D2=8,new_D1=1,old_flows_updated=2,old_directory_captures=3,original_directory_bodies_added=21,linked_articles_added=2,inventory_rows=70,new_reference_points=13,actual_operations=0))
# All shape/identity checks precede mutations. No old integration script is invoked.
assert len(newes)==23 and len(newfs)==9 and len(rr)==165 and len(ic)==23
for p,content in writes.items():p.write_text(content)
print('Applied 23 E, 8 D2 + 1 D1, 2 old F, 3 captures, 21 original article rows + 2 linked rows, 13 RP.')
