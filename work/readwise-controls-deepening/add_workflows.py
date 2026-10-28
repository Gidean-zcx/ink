from pathlib import Path
import json,csv,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/readwise-controls-deepening';A=L/'artifacts/readwise-controls-deepening';B=L/'batches'
assert not (W/'workflows-applied.json').exists()
def j(p):return json.loads(p.read_text())
def jw(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def cw(p,rows):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
ep=B/'readwise-controls-deepening-evidence.jsonl';es=[json.loads(s) for s in ep.read_text().splitlines()];caps=j(W/'additional-captures.json');ids={x['name']:f'E-RWCD-{i:02}' for i,x in enumerate(caps,14)}
note=A/'source-notes.md';s=note.read_text().replace('15篇官方正文','19篇官方正文').replace('这15篇h2/h3','已读正文h2/h3')
s+='''
## 补充：布局迁移和外部AI能力范围

从E-ink帮助的长文工作流链接进入，另外读取四篇公开正文，均普通GET200，没有执行配置/连接操作，也没有查看其新图片。

- E-RWCD-14，longform：Account/General/Default view可选Currently reading以打开最近文档；不保证精确音频断点。音量键翻页限Android，连续滚动时跳一屏；iOS不提供此选项。长文TTS入口移至More，不是功能消失。
- E-RWCD-15，library-config：Triage、Shortlist、Classic分别组织内容位置；网页Library菜单/Edit Library locations选后Update，移动Account选配置。会移动文档时有确认。Shortlist配置中的位置与Triage中的同名标签视图不同，切换的完整映射/回滚未给。
- E-RWCD-16，recommendations：官方提示示例把时间预算、主题、旧收藏或不同领域筛选后整理清单；全网扩展依赖外部AI的搜索，周期任务依赖外部调度。搜索可能漏项，先按主题列文档是文档建议。其“动作都可逆”不能扩成删除/清空的保证；没有运行示例。
- E-RWCD-17，mcp：只采用Readwise自己公布的数据范围、Reader16项及Readwise6项工具用途和索引更新说明；不是取得工具schema、认证范围或执行回包。客户端安装章节只登记标题，不作为OpenAI等第三方当前用法的验证。新旧服务器的数据范围不同；本文推荐示例提删除，但Reader工具用途表没有明确列文档删除动作，保留覆盖差别。

Action Tags所链readwise.io/mastery正常打开后转到登录页，本轮未登录；这条探索只保存在work/mastery-discovery.json，不能据登录壳补练习、评分或题卡生成结果。没有连接MCP、运行CLI、读取用户书库或设置定时任务。
'''
note.write_text(s);nh=sha(note)
for e in es:
 e['sha256']=nh
 if e.get('visual_audit_path')==str(note.relative_to(L)):e['visual_audit_sha256']=nh
for c in caps:
 assert c['url'] not in {x['url'] for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
 es.append(dict(evidence_id=ids[c['name']],product_id='M-015',title='Readwise公开工作流/能力：'+c['name'],url=c['url'],checked_on='2026-10-27',source_type='official_help',access_mode='ordinary_public_html_read',version_scope='Public workflow guidance; app/client builds not fixed; external AI and native Reader separated',app_build=None,firmware_version=None,source_updated_on=None,locator='article正文；客户端安装仅标题/范围定位',note='正文和能力声明，不是账户、连接或执行结果；见短摘记。',hands_on=False,visual_observed=False,archived_artifact=str(note.relative_to(L)),sha256=nh,capture_state='short_note_archived_raw_temporary',**{k:c[k] for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']}))
ep.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
up=j(W/'additional-captures-updates.json')
for x in up:x['capture']['sha256']=nh
jw(W/'additional-captures-updates.json',up)
cp=B/'readwise-controls-deepening-coverage.csv';fs=list(csv.DictReader(cp.open()));byf={x['flow_id']:x for x in fs}
byf['F-M015-16']['evidence_ids']+=';E-RWCD-14';byf['F-M015-16']['known_branches']+=' 音量键功能限Android；连续滚动模式跳一屏，iOS无此选项。'
new=[dict(flow_id='F-M015-25',product_id='M-015',task='切换内容分流配置并确认位置迁移',depth='D2',evidence_ids='E-RWCD-15',known_branches='网页Library下拉/Edit Library locations选择Triage、Shortlist或Classic→Update；手机Account选配置。若导致位置移动先确认。新保存去向及可用位置随配置变化；Triage的Shortlist标签视图不等于Shortlist配置的位置。',gaps='已有文档逐位置映射、取消/回滚、跨端并发、筛选视图和AI动作参数变化未核；未实际切换。',visual_status='not_observed',hands_on='false',case_path='cases/M-015-readwise.md',checked_on='2026-10-27'),dict(flow_id='F-M015-26',product_id='M-015',task='外部AI推荐与整理阅读清单（声明范围）',depth='D1',evidence_ids='E-RWCD-16;E-RWCD-17',known_branches='官方给时间/主题/来源及跨领域推荐提示和标签/移动用途；新MCP包括Reader文档与Readwise高亮，旧版范围较小；全网搜索和定期运行依赖外部AI。',gaps='只有能力/提示示例；工具schema、授权范围、实际查询/写入反馈、索引时延、部分成功与恢复均未核。推荐页删除/可逆概括不作完整动作保证。',visual_status='not_observed',hands_on='false',case_path='cases/M-015-readwise.md',checked_on='2026-10-27')]
fs.extend(new);cw(cp,fs)
changes=j(W/'existing-coverage-updates.json');byc={x['flow_id']:x for x in changes}
x=byc['F-M015-05']['after'];x['evidence_ids']+=';E-RWCD-14';x['known_branches']+=' 默认视图可选Currently reading以进入最近文档，非精确音频断点证明。'
oldF={x['flow_id']:x for x in csv.DictReader((L/'coverage.csv').open())};before=oldF['F-M015-02'];after={**before,'evidence_ids':before['evidence_ids']+';E-RWCD-14','known_branches':before['known_branches']+' 长文模式顶端改为Aa，More/Listen using text-to-speech进入听读；不是播放能力被移除。','checked_on':'2026-10-27'};changes.append(dict(flow_id=before['flow_id'],before=before,after=after));jw(W/'existing-coverage-updates.json',changes)
ip=B/'readwise-controls-deepening-inventory.csv';rows=list(csv.DictReader(ip.open()))
mapf={'longform':[2,5,15,16],'library-config':[25],'recommendations':[26],'mcp':[26]}
def add(kind,title,c,status):
 rows.append(dict(entry_id=f'M015-RWCD-{len(rows)+1:03}',entry_type=kind,title=title,source_url=c['url'],evidence_ids=ids[c['name']],flow_ids=';'.join(f'F-M015-{n:02}' for n in mapf[c['name']]),review_status=status,gap='文档条目/标题不是全部分支已验证；外部客户端未操作'))
for c in caps:
 add('linked_workflow_page',c['name'],c,'链接已追入；正文范围见短摘记')
 for h in j(W/'raw'/f'{c["name"]}-structure.json')['headings']:
  if h['level']!='h1':add('article_'+h['level'],h['title'],c,'官方标题；客户端安装仅目录，其余本轮读正文' if c['name']=='mcp' else '本轮读正文')
c=next(x for x in caps if x['name']=='mcp')
tools=['Reader: export documents','Reader: get document details/metadata','Reader: get document highlights','Reader: get export status','Reader: list documents','Reader: list tags','Reader: search documents','Reader: add document tags','Reader: add highlight tags','Reader: bulk edit document metadata','Reader: create document','Reader: create highlight','Reader: move documents','Reader: remove document tags','Reader: remove highlight tags','Reader: add highlight note','Readwise: get Daily Review','Readwise: list highlights','Readwise: search highlights','Readwise: create highlight','Readwise: delete highlight','Readwise: update highlight']
for title in tools:add('declared_tool_purpose',title,c,'官方工具用途清单；未取得schema或实际回包')
cw(ip,rows)
case=L/'cases/M-015-readwise.md';s=case.read_text().replace('本批15篇官方正文','本批19篇官方正文');lines=s.splitlines()
for i,line in enumerate(lines):
 for x in changes:
  f=x['after'];fid=f['flow_id']
  if line.startswith('| '+fid+' '):lines[i]='| '+fid+' '+f['task']+' | '+f['known_branches']+' | '+f['gaps']+' | '+f['evidence_ids']+' |'
 for f in fs:
  if line.startswith('| '+f['flow_id']+' '):lines[i]='| '+f['flow_id']+' '+f['task']+' | '+f['known_branches']+' | '+f['gaps']+' | '+f['evidence_ids']+' |'
s='\n'.join(lines)+'\n';needle='新增来源按短摘记同名章节定位'
addrows=''.join('| '+f['flow_id']+' '+f['task']+' | '+f['known_branches']+' | '+f['gaps']+' | '+f['evidence_ids']+' |\n' for f in new)
s=s.replace('\n'+needle,'\n'+addrows+'\n'+needle,1)
s+='\n补充边界：F-M015-26仅D1外部AI用途与提示示例；原生Global的权限流程不能代替外部MCP授权。官方列Reader16项、Readwise6项用途，未获得schema或调用结果。长文工作流补最近文档默认入口及Android音量键条件；实际断点仍未知。Mastery页面转登录后停止，未运行练习。\n';case.write_text(s)
report=j(W/'build-result.json');report.update(newE=len(es),newF=len(fs),newD2=sum(f['depth']=='D2' for f in fs),newD1=sum(f['depth']=='D1' for f in fs),oldF=len(changes),old_fields=sum(sum(x['before'][k]!=v for k,v in x['after'].items()) for x in changes),inventory_rows=len(rows),linked_workflow_rows=4,additional_heading_rows=sum(len(j(W/'raw'/f'{c["name"]}-structure.json')['headings'])-1 for c in caps),declared_tool_rows=len(tools))
jw(W/'workflows-applied.json',report);print(json.dumps(report))
