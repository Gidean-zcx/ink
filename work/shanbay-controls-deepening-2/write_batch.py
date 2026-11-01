from pathlib import Path
import json,csv,hashlib,shutil,re
W=Path('work/shanbay-controls-deepening-2');L=Path('docs/research/interaction-library');B=L/'batches';A=L/'artifacts/shanbay-controls-deepening-2';A.mkdir(parents=True,exist_ok=True)
DATE='2026-10-27';PREFIX='E-SBC2-';HTTP=json.loads((W/'http-results.json').read_text());H={r['name']:r for r in HTTP};oldE=json.loads((W/'old-evidence.json').read_text());OE={r['evidence_id']:r for r in oldE};articles=json.loads((W/'saved-column-index.json').read_text());AR={r['id']:r for r in articles}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dcsv(p,rows,fields):
 with p.open('w',encoding='utf-8',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(rows)
# No repeated identifiers or canonical article URLs.
allE=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()];assert not any(e['evidence_id'].startswith(PREFIX) for e in allE)
notes='''# 扇贝第二次控制补证：范围、原件和边界

查阅2026-10-27。此批没有新增任务；F01/03/04/05/06/08/10补回原编号。四个手机App、旧网页版/历史CMS、浏览器翻译扩展分别处理。商店版本沿用2026-10-26已核记录，本轮未重新抓取，不称当前安装版本。

## 目录与历史材料

E-SBC2-01：复用公开帮助前端JS及八个栏目JSON。前端声明12栏，其中8栏已保存34篇content_html；4栏未取，不能称整个帮助中心全读。栏目内有留声英语小程序、旧炼句、浏览器故障和学习方法文章，不能自动归为当前四App操作。目录CSV逐项标“仅标题”“正文已取未分析”“旧任务已覆盖”或“本轮补证”。本轮没有调用CMS、账户或学习接口。

E-SBC2-02：听力栏目bchgxx，历史产品自述中给三次重听后才可揭晓、刮开原文、没听懂的句子再安排、随后关键词测试及对照回听。它同时包含学习效果宣传；本研究仅取控制条件，构建/平台未固定，不接受学习效果承诺。

E-SBC2-03：打卡栏目zguwz，“怎样打卡”与记录入口。写午夜后当天不能打卡或补卡，内容延后；但旧E-SBY-12明确有已学完上传后遗漏的七日补卡例外。保留范围差异，时区和现行规则未知。

E-SBC2-04：常见问题orqdq，历史单词英语版“我的→单词书→每日新学数量右侧按钮”入口。与E-SBY-05的任务量立即/次日生效规则组合；未读到保存确认和现版队列重算。

## 新公开页面

E-SBC2-05：[扩展帮助](https://web.shanbay.com/bay-translate/support)，正文日期2026-09-14；1.1/1.3/2.1/4.1/4.4。Safari站点许可、悬停词卡及同账号条件属于浏览器扩展。恢复购买属于订阅，不证明恢复学习数据。只补F10外部输入分支。

E-SBC2-06：[扩展隐私政策](https://web.shanbay.com/bay-translate/privacy)，更新/生效2026-09-24；1.2.2明确主动加词时关联词条标识和来源URL，标题为部分场景可选。只作数据声明，未证明手机界面的原句显示、同步完成或API契约。8.5/8.6区分页面对话与保存的测词题，9.3区分本地缓存/配置与账号数据；不据此补手机恢复流程。

E-SBC2-07：[公司单词介绍](https://web.shanbay.com/company/jieshao)，AI助记、个人词表、训练与导出段；页面未给更新日期/构建。AI多次不认识后介入是示例声明，不能当稳定触发阈值；上传词表/草稿实时保存仍D1，不建新F或采纳效果数字。

## 旧E附加保存建议

本批复核同一来源时沿用原E。additional-captures-updates.json只给根代理追加元数据建议；原取得2026-10-26、本轮本地重读2026-10-27明确分开，没有将重读说成重新联网抓取。原件全放本批work；正式仅此短摘记，不复制帮助全文。

## 未取得与未操作

官网帮助未取得当前手机撤销误评、太简单恢复、录音提交/重录、离线队列和冲突恢复完整链。搜索命中的第三方教程/逆向账户工具均未采纳。翻译扩展首页请求重定向后404即停；先前指南图片被校验页阻挡，本轮没有重试。没有图片、视频或实机观察，没有登录、购买、上传和提交。
'''
# append exact primary raw locators, small factual notes only
notes+='\n## 原件散列与定位\n\n'
selected=[('02','bchgxx'),('03','zguwz'),('04','orqdq')]
for n,i in selected:
 a=AR[i];notes+=f'- {PREFIX}{n}：`{a["raw_work_path"]}` objects[id={i}].content_html；SHA-256 `{a["raw_sha256"]}`；assign_date `{a["assign_date"]}` 仅编排字段，不是更新时间。\n'
for n,name in [('05','translate-support'),('06','translate-privacy'),('07','company')]:
 h=H[name];notes+=f'- {PREFIX}{n}：`{h["raw_work_path"]}`；HTTP {h["http_status"]}；SHA-256 `{h["raw_sha256"]}`。\n'
notes+=f'- E-SBC2-01：`{W}/shanbayhelp.js` SHA-256 `{sha(W/"shanbayhelp.js")}`；八个栏目原件与哈希逐条见证据additional_raw_sources。\n'
(A/'source-notes.md').write_text(notes)
base=dict(product_id='M-019',checked_on=DATE,app_build=None,firmware_version=None,source_updated_on=None,hands_on=False,visual_observed=False,visual_kind=None,visual_audit=None,archived_artifact='artifacts/shanbay-controls-deepening-2/source-notes.md',sha256=sha(A/'source-notes.md'),capture_state='archived_short_notes_only_no_visual_observation',supersedes=None)
def mk(n,title,url,**kw):return dict(base,evidence_id=PREFIX+n,title=title,url=url,**kw)
cols=[]
for p in sorted(W.glob('sby-col-*.json')):
 i=p.stem.removeprefix('sby-col-');cols.append(dict(url=f'https://apiv3.shanbay.com/cms/v2/columns/{i}/articles?page=1&ipp=50',raw_work_path=str(p),raw_sha256=sha(p),original_acquisition_date='2026-10-26',rechecked_saved_file_on=DATE))
ev=[mk('01','旧官方帮助12栏目与已取得的34篇目录','https://web.shanbay.com/web/help',source_type='official_help_index_and_frontend',access_mode='existing_public_frontend_and_saved_cms_columns',version_scope='2026-10-26保存的公开帮助目录；不是当前手机设置全目录',locator='JS类别数组及cms/v2/columns的objects列表；8栏正文已取、4栏未取',note='只登记目录和覆盖状态；父栏目和文章不是独立完成任务',raw_work_path=str(W/'shanbayhelp.js'),raw_sha256=sha(W/'shanbayhelp.js'),original_acquisition_date='2026-10-26',rechecked_saved_file_on=DATE,additional_raw_sources=cols)]
for n,i,loc,note in [('02','bchgxx','产品训练段：三次后揭晓；重复未懂句；关键词测试；对照回听','历史产品自述中的控制条件；不是现版阈值或效果验证'),('03','zguwz','怎样打卡；app我的-打卡记录','午夜边界与E-SBY-12补卡例外并列；时区和现版未知'),('04','orqdq','全文单段：每日新学量按钮','只补原F03入口；保存及队列结果仍未知')]:
 a=AR[i];ev.append(mk(n,a['title'],f'https://web.shanbay.com/web/help/{a["column_id"]}/{i}',source_type='official_help',access_mode='existing_public_cms_column_content_html',version_scope='历史帮助；App/平台仅以正文为准，构建和最后更新未知',locator=loc,note=note,raw_work_path=a['raw_work_path'],raw_sha256=a['raw_sha256'],capture_url=f'https://apiv3.shanbay.com/cms/v2/columns/{a["column_id"]}/articles?page=1&ipp=50',cms_object_id=i,assign_date=a['assign_date'],assign_date_meaning='editorial scheduling, not last update',original_acquisition_date='2026-10-26',rechecked_saved_file_on=DATE))
for n,name,title,stype,scope,loc,note,date in [
('05','translate-support','扇贝翻译扩展帮助与支持','official_help','浏览器扩展Safari/Chrome/Edge；不是手机App；构建未知','1.1/1.3/2.1/4.1/4.4','网页词卡与同账号生词本；同步完成/冲突未说明','2026-09-14'),
('06','translate-privacy','扇贝翻译数据对象与本地/账号边界','official_privacy_policy','浏览器扩展；厂商数据声明，未执行或核源码','1.2.2；8.5/8.6；9.3','词条标识/来源URL；政策不替代手机操作说明','2026-09-24'),
('07','company','扇贝单词官网AI与词表功能介绍','official_product_page','单词英语版产品介绍；平台/构建与发布日期未固定','AI助记；自定义词书；训练模式及导出','声明D1；不将示例多次出错作为可靠阈值；不采纳提分/节省时间效果',None)]:
 h=H[name];ee=mk(n,title,h['url'],source_type=stype,access_mode='anonymous_public_html_get',version_scope=scope,locator=loc,note=note,raw_work_path=h['raw_work_path'],raw_sha256=h['raw_sha256'],http_status=h['http_status']);ee['source_updated_on']=date;ev.append(ee)
# Check same-source collision against full library, not merely this product.
oldurls={x['url'] for x in allE};assert not ({x['url'] for x in ev}&oldurls)
(B/'shanbay-controls-deepening-2-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in ev))
coverage_fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
dcsv(B/'shanbay-controls-deepening-2-coverage.csv',[],coverage_fields)
# Article-level inventory distinguishes raw availability from analyzed behavior.
flow_by_id={'bbsool':'F-M019-01','mqhnn':'F-M019-01','yjgia':'F-M019-01;F-M019-02','brnsyu':'F-M019-03','blksqe':'F-M019-03','puica':'F-M019-03','buxtne':'F-M019-10','frhdp':'F-M019-04;F-M019-05','bscpci':'F-M019-04','bxejg':'F-M019-06;F-M019-07','brszea':'F-M019-06','iwlnc':'F-M019-06','wldgi':'F-M019-06','uurvp':'F-M019-06;F-M019-08','mjvss':'F-M019-08','bqahtq':'F-M019-08','kyxac':'F-M019-08','bchgxx':'F-M019-06','zguwz':'F-M019-08','orqdq':'F-M019-03'}
old_by_id={e['url'].rstrip('/').split('/')[-1]:e['evidence_id'] for e in oldE if '/help/' in e['url']};new_by_id={'bchgxx':PREFIX+'02','zguwz':PREFIX+'03','orqdq':PREFIX+'04'}
inv=[]
for name,cid in json.loads((W/'help-columns.json').read_text()):
 got=any(x['column_id']==cid for x in articles)
 inv.append(dict(item_id='M019-C-'+cid,kind='help_column',parent_id='',title=name,platform_scope='旧公开帮助；不等于手机菜单',source_url='https://web.shanbay.com/web/help',evidence_ids=PREFIX+'01',flow_ids='',capture_status='saved_2026-10-26' if got else 'not_fetched',analysis_status='directory_only',gap='见子文章；未取得栏目不推断内容',checked_on=DATE))
for a in articles:
 i=a['id'];eid=new_by_id.get(i,old_by_id.get(i,''));f=flow_by_id.get(i,'')
 status='this_round_branch_review' if i in new_by_id or i in ['yjgia','brnsyu','frhdp','iwlnc','wldgi','bqahtq','buxtne'] else ('prior_task_or_boundary_review' if eid else 'body_available_not_task_analyzed')
 scope='历史扇贝听力；未定平台/构建' if a['column_id']=='bbumgi' else '历史帮助；按正文指定App，未定构建'
 if i in ['eaqsx','xpwmg']:scope='留声英语小程序/课程；不套听力口语App'
 if i=='jdubu':scope='旧炼句栏目/键盘；不套独立口语或手机'
 if i in ['gvdcn','btnkhg']:scope='旧桌面浏览器帮助；不套当前手机'
 inv.append(dict(item_id='M019-A-'+i,kind='help_article',parent_id='M019-C-'+a['column_id'],title=a['title'],platform_scope=scope,source_url=f'https://web.shanbay.com/web/help/{a["column_id"]}/{i}',evidence_ids=eid or PREFIX+'01',flow_ids=f,capture_status='content_html_saved_2026-10-26',analysis_status=status,gap='正文可得不代表完整任务；现版/异常/恢复未固定，详见case' if f else '仅目录或边界，未重建完整操作',checked_on=DATE))
# Existing off-directory IELTS guide and four separate App IDs retain original date.
for eid in ['E-SBCD-06','E-SBCD-07','E-SBCD-08','E-SBCD-09','E-SBCD-10']:
 e=OE[eid];inv.append(dict(item_id='M019-X-'+eid,kind='existing_off_directory_source',parent_id='',title=e['title'],platform_scope=e['version_scope'],source_url=e['url'],evidence_ids=eid,flow_ids='F-M019-11' if eid=='E-SBCD-06' else '',capture_status='prior_capture_2026-10-26_not_refetched',analysis_status='prior_partial',gap='不证明2026-10-27安装版本；独立口语与听力口语分开',checked_on='2026-10-26'))
sections=[('1.1','Safari启用'),('1.2','Chrome/Edge启用'),('1.3','开始使用'),('2.1','登录'),('2.2','会员与账号'),('2.3','注销账号'),('3.1','App Store订阅'),('3.2','网页购买'),('3.3','两渠道关系'),('4.1','无悬浮球/无反应'),('4.2','积分不足'),('4.3','会员未生效'),('4.4','生词本不同步'),('5','联系')]
for sec,title in sections:
 chosen=sec in ['1.1','1.3','2.1','4.1','4.4']
 inv.append(dict(item_id='M019-EXT-'+sec,kind='extension_help_section',parent_id='',title=sec+' '+title,platform_scope='浏览器扩展；仅作为词汇外部输入边界',source_url=H['translate-support']['url'],evidence_ids=PREFIX+'05',flow_ids='F-M019-10' if chosen else '',capture_status='public_html_200',analysis_status='branch_condition_review' if chosen else 'read_outside_task_scope',gap='未安装/登录；不迁移到四手机App；未展开事项不计完成',checked_on=DATE))
# Three D1 company domains, avoiding restatement of all promotional bullets.
for n,title in enumerate(['AI助记/精简与答疑','个人词表与导出','草稿纸/随身听/句法分析'],1):
 inv.append(dict(item_id=f'M019-P-{n}',kind='product_claim_domain',parent_id='',title=title,platform_scope='单词英语版产品介绍；未定平台/构建',source_url=H['company']['url'],evidence_ids=PREFIX+'07',flow_ids='',capture_status='public_html_200',analysis_status='D1_claim_only',gap='入口、校订、撤回、失败/中断与导出结果未知',checked_on=DATE))
dcsv(B/'shanbay-controls-deepening-2-inventory.csv',inv,list(inv[0]))
# Stable flow changes are suggestions, not mutations of an earlier batch.
old={r['flow_id']:r for r in json.loads((W/'old-coverage.json').read_text())}
chg={
1:dict(evidence_ids=old['F-M019-01']['evidence_ids']+';E-SBY-04',known_branches='分阶段提示；回想不起或测试未过均进详情，最终通过也进详情；三个掌握口径分开',gaps='误评撤销、太简单恢复、跳过计量及现版AI触发未知'),
3:dict(evidence_ids=old['F-M019-03']['evidence_ids']+';E-SBC2-04',known_branches=old['F-M019-03']['known_branches']+'；旧App我的→单词书→每日新学量右侧按钮',gaps='历史入口已补；保存确认、现版队列重算、删书确认和跨书共享词状态仍未知'),
4:dict(known_branches=old['F-M019-04']['known_branches']+'；旧阅读词汇水平标注是大纲匹配，非个人掌握；非文中查词先点空白再右上搜索',gaps=old['F-M019-04']['gaps']+'；级别选择具体入口与标注更新时序未知'),
5:dict(known_branches=old['F-M019-05']['known_branches']+'；旧帮助空白处→底部按钮可改日夜/字体/字号/对齐',gaps=old['F-M019-05']['gaps']+'；排版持久化/撤回与跨端设置未知'),
6:dict(evidence_ids=old['F-M019-06']['evidence_ids']+';E-SBC2-02',known_branches=old['F-M019-06']['known_branches']+'；历史自述三次重听后揭晓/刮开、未懂句再安排；挖空0/30/60/90，提示空不计分',gaps='现版重听门槛、错答/跳过/退出恢复未知；旧星级30/60边界重叠，不能反推精确评分公式'),
8:dict(evidence_ids=old['F-M019-08']['evidence_ids']+';E-SBC2-03',known_branches=old['F-M019-08']['known_branches']+'；旧午夜延后与已上传漏打卡七日补卡分范围；我的→打卡查记录',gaps='午夜时区/当前日界线、补卡例外与计划关系、退出确认和误打撤销未知'),
10:dict(evidence_ids=old['F-M019-10']['evidence_ids']+';E-SBC2-05;E-SBC2-06',known_branches=old['F-M019-10']['known_branches']+'；历史Web批量10/次50/日及总添加2000/日；现代扩展同账号词卡输入，政策说明词条ID/来源URL',gaps='旧额度不套现版；扩展入库完成/重复/冲突、手机语境呈现及恢复未知；不把恢复购买当恢复学习数据')}
updates=[]
for n,fields in chg.items():
 fid=f'F-M019-{n:02}';fields['checked_on']=DATE
 for k,v in fields.items():
  if old[fid][k]!=v:updates.append(dict(flow_id=fid,field=k,before=old[fid][k],after=v,reason='同一任务补分支；历史CMS/现代扩展分开，不新增F或改变深度'))
(W/'existing-coverage-updates.json').write_text(json.dumps(updates,ensure_ascii=False,indent=2)+'\n');dcsv(W/'existing-coverage-updates.csv',updates,list(updates[0]))
# Additional captures of the same pages keep original E IDs. No old evidence file changes.
caps=[]
for eid,src in [('E-SBY-04','work/vocab-research/sby-yjgia.json'),('E-SBY-05','work/vocab-research/sby-brnsyu.json'),('E-SBY-06','work/vocab-research/sby-frhdp.json'),('E-SBY-10','work/vocab-research/sby-iwlnc.json'),('E-SBY-11','work/vocab-research/sby-wldgi.json'),('E-SBY-12','work/vocab-research/sby-bqahtq.json'),('E-SBCD-02','work/shanbay-controls-deepening/cms-buxtne.json')]:
 dest=W/('reused-'+eid+'.json');shutil.copyfile(src,dest);d=json.loads(dest.read_text());before=OE[eid].get('additional_captures');newcap=dict(capture_kind='local_recheck_of_prior_public_cms_response',original_acquisition_date='2026-10-26',checked_on=DATE,network_refetched=False,source_url=OE[eid]['url'],capture_url='https://apiv3.shanbay.com/cms/articles/'+OE[eid]['url'].split('/')[-1],raw_work_path=str(dest),raw_sha256=sha(dest),cms_assign_date=d.get('assign_date'),assign_date_is_update=False,archived_artifact='artifacts/shanbay-controls-deepening-2/source-notes.md',sha256=sha(A/'source-notes.md'),hands_on=False,visual_observed=False)
 caps.append(dict(evidence_id=eid,field='additional_captures',before=before,after=(before or [])+[newcap],reason='同页旧E追加本地原件/散列与本轮复核；非新来源或新网络抓取'))
(W/'additional-captures-updates.json').write_text(json.dumps(caps,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'new_E':len(ev),'new_F':0,'inventory':len(inv),'old_field_updates':len(updates),'old_E_additional_capture_updates':len(caps)},ensure_ascii=False))
