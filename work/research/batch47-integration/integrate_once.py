import csv,hashlib,json
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches';W=R/'work/research/batch47-integration'
A=L/'artifacts/shortcuts-data-flow';S=R/'work/research/batch47-shortcuts-data-flow';DATE='2026-10-27'
SCOPE='Official Shortcuts 10.0 / iOS 27 guide selected in captured HTML; installed build, device and region not tested'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rows):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
assert not (B/'shortcuts-data-flow-evidence.jsonl').exists(),'Do not rerun'
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
http=json.loads((S/'http-manifest.json').read_text());assert len(http)==28
invpath=B/'shortcuts-controls-deepening-inventory.csv';inventory=list(csv.DictReader(invpath.open()))
byid={r['upstream_id'].lower():r for r in inventory if r['entry_type']=='topic'}
notes=A/'source-notes.md';evidence=[];audit=[]
for i,r in enumerate(http,1):
 assert int(r['key'][:2])==i and r['status']==200
 assert any(x['text']=='iOS 27' and '/10.0/ios/27' in x['url'] for x in r['selected_versions'])
 for key,hkey in [('raw_work_path','raw_sha256'),('text_work_path','text_sha256')]:assert sha(R/r[key])==r[hkey]
 old=byid[r['body_id']];eid=f'E-SCDF-{i:02d}'
 rec={k:r[k] for k in ['key','url','status','final_url','raw_work_path','raw_sha256','text_work_path','text_sha256','body_id','selected_versions','text_chars','checked_on']}
 rec.update(evidence_id=eid,title=old['title'],body_read=True,visual_observed=False,hands_on=False)
 audit.append(rec)
 evidence.append(dict(evidence_id=eid,product_id='M-043',title=old['title'],url=r['url'],source_type='official_help',access_mode='ordinary_http_200_target_body_read',checked_on=DATE,source_updated_on=None,version_scope=SCOPE,app_build=None,firmware_version=None,locator='正文 '+r['body_id']+'；来源摘记 E后缀'+f'{i:02d}',note='官方27操作/概念正文；具体事实、条件及未知分支见来源摘记。没有快捷指令运行或视觉观察。',hands_on=False,visual_observed=False,archived_artifact=str(notes.relative_to(L)),sha256=sha(notes),capture_state='target_body_and_version_verified_bounded_notes',supersedes=None,raw_work_path=r['raw_work_path'],raw_sha256=r['raw_sha256'],extracted_work_path=r['text_work_path'],extracted_sha256=r['text_sha256'],selected_version=r['selected_versions']))
dump(A/'fetch-audit.json',dict(checked_on=DATE,source_count=28,scope=SCOPE,observations='All bodies read; no UI images viewed or app actions executed. Early web navigation varied by region/version; captured selected version and body ID used.',storage_lookup=dict(queries=['site:support.apple.com/guide/shortcuts "Store Content" "27"','site:support.apple.com "Shortcuts Storage" "Get Stored Content"'],result='Only existing release note E-SCCD-24 located; detailed storage workflow remains unknown.'),sources=audit))
for e in evidence:e['artifact_hashes']={str((A/'fetch-audit.json').relative_to(L)):sha(A/'fetch-audit.json')}
(B/'shortcuts-data-flow-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evidence))
def es(*nums):return ';'.join(f'E-SCDF-{n:02d}' for n in nums)
newdata=[
(13,'从候选列表选择并传递具体内容',es(11,26),'List建有序对象；Choose from List单选/多选及默认全选；下标从1开始；字典显示键但传值。','空列表、零选项、取消、越界、选择顺序及写入后的对象身份'),
(14,'按菜单或条件选择执行路径',es(12,13),'菜单仅走所选一支；If按类型/属性比较，可Otherwise If及All/Any；分支最后有输出动作形成If Result。','取消、空分支、缺字段、多条件短路、无输出与不同类型合流'),
(15,'逐项执行并检查汇总输出',es(14,23),'固定次数与Repeat with Each分别处理；当前项/序号；汇总每轮末尾输出，Quick Look或Alert检查。','中途失败已处理对象、重试去重、零项、嵌套顺序、部分结果和副作用恢复'),
(16,'在当前Safari网页运行脚本并接收结果',es(16,17,18),'共享入口限Safari网页；每个JS动作需活动网页输入；completion返回，超时/异常有错误提示；许可按指令/网页。','统一超时时长、空元素、页面更改、拒绝后状态、取消、脚本副作用恢复及页面匹配边界'),
(17,'跨应用调用并区分成功取消和错误返回',es(19,20,21),'名称/编码文本或剪贴板输入；成功附文本result，取消无输出，错误附errorMessage；目标App须提供scheme。','目标缺失/改名、回调未达或重复、断网、业务结果核对、重试幂等和部分产物'),
(18,'请求API并逐项提取结构化内容',es(22,23,24,25,26,27,28),'URL端点与方法/请求体分开；识别顶层列表和嵌套字典；先单项取键再遍历。指南另列鉴权及配额限制。','缺键/null/坏JSON、分页、状态码、认证过期、限流/退避、部分写入及重复导入'),
]
rows=[]
for n,title,eids,known,gap in newdata:
 rows.append(dict(flow_id=f'F-M043-{n:02d}',product_id='M-043',task=title,depth='D2',evidence_ids=eids,known_branches=known,gaps=gap,visual_status='not_observed',hands_on='false',case_path='cases/M-043-shortcuts.md',checked_on=DATE))
csvout(B/'shortcuts-data-flow-coverage.csv',rows)
updates={
1:(es(6,7),'另补27动作连接线与Show Info；无连接可能不取输入而非错误；Quick Look为链中运行预览，不是无副作用试跑。','生成错误、缺动作、额度耗尽、运行副作用与撤回'),
3:(es(1),'27正文核对：输入动作限制类型；缺输入停止回复/询问/剪贴板/继续；Continue保留空输入变量。','各入口空输入实际反馈、权限与取消传播'),
4:(es(2,15),'27 Ask for Input输入Done传值；Ask Each Time到该参数步骤才暂停，Done继续；默认值可改。','输入或参数提问取消、等待/锁屏、已执行步骤与恢复'),
5:(es(3,16),'27单次/总是/拒绝及Privacy撤销；脚本总开关与指令-网页许可分别记录；拒绝不证明此前动作回滚。','网页匹配范围、拒绝/撤销后的部分结果、脚本检查与权限缓存'),
6:(es(4),'27编辑器显示末尾输出；外部运行结束无预览并返回；Stop and Output交回结果，接收上下文决定去向。Alert确认/取消与Notification继续仍分开。','进程终止、停止后对象去留、事务回滚、输出接收及恢复入口'),
8:(es(5,8,9,10),'另补Magic变量来源选择、特殊参数及Set覆盖/Add追加；Clear移除引用不证明删原对象。普通链完成复位，跨运行存储是另一能力。','转换/缺字段、来源被删、参数引用更新、跨运行存储的键/覆盖/删除/恢复'),
}
oldchanges=[]
for p in [B/'ai-coverage.csv',B/'shortcuts-controls-deepening-coverage.csv']:
 rows=list(csv.DictReader(p.open()))
 for r in rows:
  if r['product_id']!='M-043':continue
  n=int(r['flow_id'].split('-')[-1])
  if n not in updates:continue
  before=dict(r);eids,known,gap=updates[n]
  r['evidence_ids']=';'.join(dict.fromkeys((r['evidence_ids']+';'+eids).split(';')));r.update(known_branches=known,gaps=gap,checked_on=DATE)
  oldchanges.append(dict(owner=str(p.relative_to(R)),before=before,after=dict(r)))
 csvout(p,rows)
assert len(oldchanges)==6;dump(W/'old-flow-updates.json',oldchanges)
flowmap={1:[3],2:[4],3:[5],4:[6],5:[8],6:[1],7:[1],8:[8],9:[8],10:[8],11:[13],12:[14],13:[14],14:[15],15:[4],16:[5,16],17:[16],18:[16],19:[17],20:[17],21:[17],22:[18],23:[15,18],24:[18],25:[18],26:[13,18],27:[18],28:[18]}
invchanges=[]
for i,r in enumerate(http,1):
 row=byid[r['body_id']];assert row['review_status']=='directory_only';before=dict(row)
 row.update(evidence_id=f'E-SCDF-{i:02d}',related_flow_ids=';'.join(f'F-M043-{n:02d}' for n in flowmap[i]),review_status='body_read_documented_not_operated',gaps='第47批已读指定27正文；未观察原生界面/实际操作，异常与持久结果见关联任务和来源摘记。',checked_on=DATE)
 invchanges.append(dict(before=before,after=dict(row)))
csvout(invpath,inventory);dump(W/'inventory-updates.json',invchanges)
assert len(inventory)==114 and sum(r['review_status']=='body_read_documented_not_operated' for r in inventory)==49
rpfile=L/'reference-points.csv';rp=list(csv.DictReader(rpfile.open()));assert len(rp)==88 and not any(r['product_id']=='M-043' for r in rp)
rpdata=[
('AI;输入;空值;入口',[3],[1],'01 输入限制','从共享入口或无初始输入入口运行','筛接受类型；设无输入分支','文档区分隐藏入口、询问/取剪贴板、停止和继续传空值','原对象无持久改变证据','各入口差异;取消;拒绝;空值传播'),
('AI;提问;参数;暂停',[4],[2,15],'02/15 输入与参数提问','动作链运行到需补齐信息的步骤','输入或改参数→Done','文档先暂停等待，Done后传值/继续；此前步骤可能已完成','等待/锁屏后的会话保持未知','取消;超时;后台;前序副作用'),
('AI;对象;变量;存储',[8],[5,9,10],'05/09/10 内容流和变量','编辑器已有动作输出','选Magic变量；Set覆盖/Add追加；Clear移除引用','文档描述链内对象传递，普通链完成复位','跨运行存储另有发布声明，具体保存/删除/恢复未知','来源失效;删除对象;跨设备;重启'),
('AI;选择;列表;对象',[13],[11,26],'11/26 选择列表和字典','候选内容列表或字典','单选/多选，可默认全选；按下标取项','文档选字典键后交对应值；标签和后续对象有区别','没有选中后写入或重启保持证据','空列表;零选;取消;越界;顺序'),
('AI;菜单;条件;输出',[14],[12,13],'12/13 菜单与If','有多条动作路径或不同输入类型','选菜单；设Input/Condition及All/Any','只走所选/满足路径；If Result为该路径最后产生的输出','没有分支执行后的业务保存证据','空分支;缺字段;异类型结果;取消'),
('AI;批量;循环;部分失败',[15],[14,23],'14/23 Repeat与逐项处理','明确待处理列表或次数','Repeat with Each；查看End Repeat汇总','文档汇集各轮末尾输出；重复次数不等于不同内容数量','中止后的部分产物与重跑去重未知','零项;中断;重试;副作用补偿'),
('AI;屏幕;网页;超时',[16],[16,17,18],'16–18 网页脚本','活动Safari网页、脚本许可和该页授权','从共享运行；每段传网页对象并completion返回','文档区分输入上下文、完成回调、超时和运行异常','同指令/网页许可可记住；页面改动恢复未知','网页匹配范围;拒绝;空元素;页面跳转;取消'),
('AI;跨App;成功;取消;错误',[17],[19,20,21],'19–21 URL与回调','目标指令存在/目标App提供scheme','传名称和编码输入；接收可选回调','文档成功附result、取消无输出、错误附errorMessage','回调不证明所有业务数据持久成功或自动回滚','改名;缺目标;重复;回调丢失;部分写入'),
('AI;API;导入;结构化数据',[18],[22,23,24,25,27,28],'22–25/27–28 请求与解析','服务端点、访问资格及响应结构明确','选请求方法/请求体，先取单项字段再遍历','文档返回值可为列表/嵌套对象；字段提取与整批业务成功不同','没有执行示例请求、写入或回读验证','缺键;null;坏JSON;分页;限流;重复导入'),
('AI;完成;预览;返回',[1,6],[4,6,7],'04/06/07 完成、连接与预览','明确在编辑器或外部入口运行','查看末尾输出或中间Quick Look，按调用方接收结果','文档外部完成可无预览；无连接线也不一律是错误','预览/结束不证明业务保存或执行撤销','真实结果回读;停止;回滚;输出无人接收'),
]
for i,(tags,ns,ees,loc,pre,act,result,persist,gaps) in enumerate(rpdata,1):
 rp.append(dict(reference_id=f'RP-M043-{i:02d}',product_id='M-043',question_tags=tags,flow_ids=';'.join(f'F-M043-{n:02d}' for n in ns),evidence_ids=es(*ees),source_locator='source-notes.md E-SCDF-'+loc,artifact_path='artifacts/shortcuts-data-flow/source-notes.md',precondition=pre,action=act,visible_result=result,evidence_basis='official_document_text_reconstruction_no_ui_operation',persistence_result=persist,unknown_branches=gaps,version_scope=SCOPE,checked_on=DATE))
csvout(rpfile,rp)
dump(W/'integration-result.json',dict(new_flows=6,updated_flows=6,new_evidence=28,new_reference_points=10,updated_inventory_rows=28,inventory_read_topics=49,inventory_directory_topics=46,requirements_sha256=sha(R/'docs/requirements.md')))
print('Integrated 6 new F, 6 existing F, 28 E, 10 RP; 28 original inventory rows updated.')
