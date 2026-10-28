import csv,json,hashlib,shutil,re
from pathlib import Path
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/inoreader-controls-deepening';art=lib/'artifacts/inoreader-controls-deepening'
assert not (lib/'batches/inoreader-controls-deepening-evidence.jsonl').exists(),'One-time builder'
metas={p.stem.replace('.meta',''):json.loads(p.read_text()) for p in w.glob('*.meta.json')}
notes={
1:'58分类258主题逐项取得；只读目录与正文已读分开，含Team和账单，不是全部功能完成。',
2:'条件/动作/日志，去重窗口6小时至1个月；批量选择最多50项。当前正文不推定自发布日期从未修改。',
3:'2026-04-07说明内容/去重过滤合并50总额度，不再各自固定配额。',
4:'过滤新条目与API不可见；开头50总额、FAQ50加10与更新说明有冲突。',
5:'功能总览包含收集/阅读/输出及API；仅目录，不推定全部功能已分析。',
6:'Run rule试跑会执行动作，包括邮件和导出；不能当无副作用预览。',
7:'未触发先查启用、触发范围、条件、日志和外部服务连接；未核自动重试。',
8:'web点Edited查看记录和版本，Published回原始版；两张静态示意，不是产品操作。',
9:'账号/源/文件夹和公共索引搜索不同；可按字段/日期/语种/逻辑条件筛选。',
10:'创建监控前检查匹配；修改query保留旧历史，新条件向后生效；告警另配规则。',
11:'文件夹装源，标签装文章；未读时限、标签合并、排序与共享不同操作。',
12:'整篇笔记与段落高亮分开；标注保留个人文本副本，来源修改可另查。',
13:'源历史从首次采集起；取消订阅后保存/标签条目留存，源历史有仍存在读者条件。',
14:'Spotlights只着色，不过滤或改排序；支持大小写条件，Team颜色优先。',
15:'添加widget/选数据源/拖动多列/多看板/设启动页；默认含热门和推荐。',
16:'摘要/问答及多篇报告按选定来源加工；平台/语言/persona及自带key有套餐条件。',
17:'API注册、认证和额度的文档入口；未调用接口，不证明全页面语义覆盖。',
18:'帮助声称两周内批量已读可恢复，单篇不适用；路径却只写时间选项，实际撤销入口有歧义。',
19:'仅Add feed内Import feeds入口，缺导入确认/错误与合并流程。',
20:'网站更新不等于RSS更新；检查feed本身、Properties错误及过滤。',
21:'仅Add feed输入名称/URL再跟随界面，缺最后确认和失败反馈。',
22:'列表布局、排序、分组可配置，未核各平台控件。',
23:'帮助列新到旧/旧到新，Pro Magic另查。',
24:'Magic依据流行度和阅读习惯，具体权重及重置未知。',
25:'文章Summarize入口，生成错误、取消和重试未知。',
26:'Preferences→Intelligence配置prompt/persona/语言/按钮行为。',
27:'摘要存于文章和可搜索账户区，覆盖/多版本未说明。',
28:'选多篇→Create Intelligence report→预设/自定义prompt；未验证引用和校订。',
29:'Automate→Automated reports选源/prompt/schedule，看预览再保存；额度有条件。',
30:'报告侧重多篇分析，digest侧重定时分发；digest也能带摘要。',
31:'监控漏收先核账号/公共范围，再单关键词逐条件恢复；非全网保证。',
32:'空搜索先核范围/语言/日期/标题限制，改独特词后逐项加条件。',
33:'功能页仍将Run rule描述为preview；与明确会执行动作的帮助并列保留。'}
locs={1:'Help center; five groups; all category article links',2:'Rules / Filters / Bulk actions',3:'Usage & availability',4:'Set up a filter / FAQ quota',5:'Feature directory',6:'Answer paragraph',7:'Answer paragraph',8:'How to view / Available now; two official images',9:'Run a search / FAQ',10:'Create / refine query FAQ',11:'Get organized / unread duration / magic',12:'Start annotating / Highlights that stay in place',13:'Find an old article / Keep what matters / FAQ',14:'Create / FAQ',15:'Build your dashboard / FAQ',16:'Start with one article / FAQ',17:'Connect in four steps / limits',18:'Web / mobile / two weeks restriction'}
flowspec=[
(7,'搜索账号内资料并扩大来源',[9,32],'源/文件夹、账号与公共索引范围不同；空结果撤限制并逐项重建。','实际分词、评分、隐藏来源及空结果反馈'),
(8,'将查询保存为持续监控',[10,31],'改查询保留旧历史，仅新到条目按新条件；告警需另建Rule。','历史重筛、漏收、重复、停用删除和错误重试'),
(9,'组织来源文章并管理未读范围',[11,22,23,24],'文件夹装源、标签装文章；合并/固定与未读时限分开；Magic有群体及习惯因素。','未读时限隐藏或改状态、合并撤回、排序权重和多端冲突'),
(10,'批注回访并保留原文关系',[12],'整篇笔记与段落高亮不同；标注文本副本保留，更新另外提示。','批注修改删除、离线同步、外部副本及源版本关联'),
(11,'查看新闻修订与发布版本',[8],'Edited列表、红绿差异、版本选择器和Published；web套餐条件，只有官方静态图。','未采集修订、移动端、更新频率、保留期和AI产物联动'),
(12,'组装个人新闻起始页',[15],'widget数据源、位置、列数、多看板与启动页分别设置；默认含热门推荐。','移动布局、空栏目、限制和跨端冲突'),
(13,'突出关键词并保留原列表',[14],'Spotlights仅着色，不过滤/改排序；大小写与Team颜色优先。','大量重叠词、无色显示、读听与停用恢复'),
(14,'生成单文摘要提问并设偏好',[16,25,26,27],'预设/自写prompt、平台/语言/persona；摘要随文存储和搜索。','生成取消、重试覆盖、源更新、引用和额度失败'),
(15,'生成多篇报告及安排定时报告',[16,28,29,30],'多篇/整源分析与定时分发目标不同；报告作为新文章到达。','空期、部分结果、引用、暂停删除、预览副作用及token不足'),
(16,'批量处理和误标已读恢复范围',[2,18],'最多50项；近两周批量已读恢复文档有范围，所给时间菜单路径却未写明确撤销控件。','单项失败、撤销真实入口、超两周与单篇已读、外发撤回')]
fmap={n:[f'F-M044-{f:02}' for f,_,es,*_ in flowspec if n in es] for n in notes}
fmap[2]+=['F-M044-01','F-M044-02'];fmap[3]=fmap[4]=['F-M044-01'];fmap[6]=fmap[7]=fmap[33]=['F-M044-02'];fmap[13]=['F-M044-03']
records=[];bodymap={}
for n in range(1,34):
 key=next(k for k in metas if k.startswith(f'{n:02}-'));m=metas[key];assert m['status']==200
 raw=root/m['raw_work_path'];assert hashlib.sha256(raw.read_bytes()).hexdigest()==m['raw_sha256']
 bodyfile=w/(key+'.article.txt');title=bodyfile.read_text().splitlines()[0] if bodyfile.exists() else ('Help center' if n==1 else 'Rules')
 r=dict(evidence_id=f'E-INCD-{n:02}',product_id='M-044',title=title,url=m['final_url'],checked_on='2026-10-27',source_type='official_blog' if n in (2,3,8) else 'official_help_index' if n in (1,5) else 'official_help' if '/help/' in m['url'] else 'official_feature_documentation',access_mode='public_http_html_text',version_scope='English public Inoreader documentation; web/mobile and plan conditions distinguished; installed build/account/region untested',app_build=None,firmware_version=None,source_updated_on={2:'2026-01-27',3:'2026-04-07',8:'2026-10-19'}.get(n),locator=locs.get(n,'Answer paragraph'),note=notes[n],hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='durable_notes_temporary_html',raw_work_path=m['raw_work_path'],raw_sha256=m['raw_sha256'],source_notes_path='artifacts/inoreader-controls-deepening/source-notes.md',supersedes=None)
 if n==1:
  r['additional_raw_sources']=[]
  for c in json.loads((w/'help-categories-effective.json').read_text()):
   cm=metas[c['source_key']];assert cm['status']==200;r['additional_raw_sources'].append({k:cm[k] for k in ('url','raw_work_path','raw_sha256')})
 if n==8:
  paths={}
  for i in (1,2):
   src=w/f'edit-history-reference-clean-{i:02}.png';dest=art/f'edit-history-reference-{i:02}.png';shutil.copyfile(src,dest);paths[str(dest.relative_to(lib))]=hashlib.sha256(dest.read_bytes()).hexdigest()
  first=next(iter(paths));r.update(visual_observed=True,visual_kind='static_official_reference',access_mode='public_html_and_browser_viewed_official_images',archived_artifact=first,sha256=paths[first],artifact_hashes=paths,capture_state='two_official_static_frames_observed')
 records.append(r)
 if '/help/' in m['url']:bodymap[m['url']]=(r['evidence_id'],fmap[n])
existing={json.loads(x)['evidence_id'] for p in (lib/'batches').glob('*-evidence.jsonl') for x in p.read_text().splitlines()};assert not existing.intersection(r['evidence_id'] for r in records)
(lib/'batches/inoreader-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in records))
lines=['# Inoreader来源摘记与观察边界','','查阅2026-10-27。正式库保存短摘记和必要官方静态图；HTML原件及完整请求记录位于临时work，不保证work长期保留。未登录、配置或执行产品；说明中的按钮/结果仅据文档。','','| E | 来源与定位 | 支持与限制 |','| --- | --- | --- |']
for r in records:lines.append(f'| {r["evidence_id"]} | [{r["title"]}]({r["url"]})；{r["locator"]} | {r["note"]} |')
lines+=['','静态观察：Chromium156.0.8078.4、1440×1000、新匿名上下文加载官方博客HTTP200，截取两张内嵌演示图。首轮第二帧被Cookie横条遮挡，居中滚动再截取后清晰；初始两图及清晰第二图均实际查看，清晰第一图散列与已看第一图相同。四份截图元数据保留work。没有产品动作、连续视频、计时或真实新闻状态变化。','','修订：原F02将Run rule概括为预览，本次帮助明确有实际动作；保留功能页冲突。过滤额度50合并与FAQ50+10亦并列。未读恢复帮助给时间菜单而未给明确撤销项，未自行补按钮。']
(art/'source-notes.md').write_text('\n'.join(lines)+'\n')
fields=list(next(csv.DictReader((lib/'coverage.csv').open())))
def writecsv(path,rows,fields):
 with path.open('w',newline='') as f:
  wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(rows)
new=[]
for n,title,es,known,gaps in flowspec:new.append(dict(flow_id=f'F-M044-{n:02}',product_id='M-044',task=title,depth='D2',evidence_ids=';'.join(f'E-INCD-{e:02}' for e in es),known_branches=known,gaps=gaps,visual_status='static_reference_only' if n==11 else 'not_observed',hands_on='false',case_path='cases/M-044-inoreader.md',checked_on='2026-10-27'))
writecsv(lib/'batches/inoreader-controls-deepening-coverage.csv',new,fields)
# Stable IDs derive from original URL, not current order.
def iid(url):return 'INO-'+url.split('/help/',1)[1].replace('/','--')
items=[]
for c in json.loads((w/'help-categories-effective.json').read_text()):
 items.append(dict(entry_id=iid(c['url']),parent_id='',entry_type='category',title=c['title'],url=c['url'],scope=c['url'].split('/help/')[1].split('/')[0],evidence_id='E-INCD-01',related_flow_ids='',review_status='directory_read',gaps='目录存在不等于全文与菜单已核',checked_on='2026-10-27'))
for a in json.loads((w/'help-articles-effective.json').read_text()):
 match=bodymap.get(a['url']);items.append(dict(entry_id=iid(a['url']),parent_id=iid(a['category_url']),entry_type='topic',title=a['title'],url=a['url'],scope=a['url'].split('/help/')[1].split('/')[0],evidence_id='E-INCD-01'+('；'+match[0] if match else ''),related_flow_ids=';'.join(match[1]) if match else '',review_status='body_read_not_operated' if match else 'directory_only',gaps='正文只支持案例所列范围；未实操' if match else '正文与完整任务未核',checked_on='2026-10-27'))
assert len(items)==316 and len({r['entry_id'] for r in items})==316
for r in items:r['evidence_id']=r['evidence_id'].replace('；',';')
writecsv(lib/'batches/inoreader-controls-deepening-inventory.csv',items,list(items[0]))
old={r['flow_id']:r for r in csv.DictReader((lib/'coverage.csv').open())};updates=[]
changes={1:dict(evidence_ids='E-INO-01;E-INCD-02;E-INCD-03;E-INCD-04',known_branches='新增6小时至1个月比较窗口、当天移除量及开关；50合并额度与FAQ50+10有文案冲突。',gaps='旧条目回看/恢复、停用后范围、误去重、配额冲突的账号验证'),2:dict(task='条件规则试跑与故障检查',evidence_ids='E-INO-02;E-INCD-02;E-INCD-06;E-INCD-07;E-INCD-33',known_branches='帮助明确Run rule会执行邮件/导出等动作，修订旧预览概括；查启用/范围/条件/日志/连接。',gaps='功能页preview措辞冲突；级联、重复发送、部分失败、撤回和循环未测'),3:dict(evidence_ids='E-IND-01;E-INCD-13',known_branches='源历史从首次采集起；取消订阅后个人保存/标签条目仍在，普通历史有源仍有读者条件。',gaps='精确进度、个人误删、最后订阅读者、跨端冲突及完整备份')}
for n,changeset in changes.items():
 fid=f'F-M044-{n:02}';changeset['checked_on']='2026-10-27'
 for field,after in changeset.items():updates.append(dict(flow_id=fid,field=field,before=old[fid][field],after=after,reason='根核当前帮助与功能页；保留冲突，不升级实操'))
writecsv(w/'existing-coverage-updates.csv',updates,list(updates[0]))
print(json.dumps(dict(new_evidence=len(records),new_flows=len(new),inventory=len(items),old_tasks=3),ensure_ascii=False))
