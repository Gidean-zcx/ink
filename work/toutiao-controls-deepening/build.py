from pathlib import Path
import json,csv,hashlib,re
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/toutiao-controls-deepening';A=L/'artifacts/toutiao-controls-deepening';B=L/'batches';A.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
jwrite=lambda p,x:p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
b=json.loads((W/'baseline.json').read_text());cap={c['name']:c for c in json.loads((W/'captures.json').read_text())}
assert not b['prefix_collisions'] and not b['reverse_dependencies']
assert sha(W/'case-before.md')==b['case_sha256']
for f,h in b['old_batch_hashes'].items():assert sha(R/f)==h,f
notes='''# 今日头条：本轮来源与限制

查阅2026-10-27。普通公开GET与网页工具读取；没有浏览器操作、登录、接口调用、提交反馈、截图观察或下载音视频。原件仅在work/toutiao-controls-deepening，正式文件为短摘记。

## E-TTCD-01：个人信息管理指引

[官方静态页](https://sf3-cdn-tos.douyinstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/Personal_information_management.html)标题明确今日头条；正文无更新日期、OS或构建。可核账号、关注/粉丝、历史/收藏等管理入口。其清除兴趣句笼统写删除操作/使用行为，与E-TT-01现有2026政策限定历史兴趣和推测偏好的措辞不同；不据此扩大删除范围。部分钱包/草稿入口也不同，不拼成一套当前菜单。

## E-TTCD-02：基本功能数据处理规则

[官方规则](https://lf9-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/e327f6fd-d5b9-4090-b44e-d65dc891020a.html)，更新2024-05-06、生效2024-05-13；由旧E-TT-01原HTML引出。§1.3.2的推荐路径有省略设置层、兴趣重置含“永久”的历史文字；同段仍说明会重新形成偏好。不能解释为永久停止积累，不能套到18.8.0。

## E-TTCD-03：创作者帮助目录及一篇正文

[内容相关常见问题](https://baike.toutiao.com/detail/195/282/0)，正文更新2025-02-21 13:21:53。HTML同时提供侧栏目录和RENDER_DATA/menuList；本轮仅按JSON解码已有页面内的目录，不执行脚本、不请求账户或内容API。目录仅从ParentId=0的7个根沿Children取73个唯一节点，避免把平铺重复项再次计数；另7个未连入该树的节点不计。页面为创作者审核/推荐量/信用分/上传FAQ，不是读者频道/收藏帮助。其“我的→客服中心”来自创作者语境，与商店“我的→用户反馈”不能擅自统一；未发送。其他目录只有标题，未读对应正文。

## E-TT-01与04补采集

E-TT-01本轮重读旧work/feed-interactions/tt-privacy.html/.txt，不重新GET。重点§6.1.2特征开关、§6.2.2无痕边界、§7.2.1对象与§7.2.2副本申请。推荐和广告控制不同，阅读兴趣/浏览记录/收藏/个人副本也不同；政策步骤不是实测。

E-TT-04同URL新GET中国区商店正文：18.8.0，HTML time datetime=2026-10-22；18.7.0=2026-10-13。近期日志只是一般问题修复/稳定性描述，无频道/长按/恢复细节。未采用用户评论或开发者描述中引述的好评。商店iPhone/iPad版本不为跨系列政策的按钮标版本。

## 取得失败与排除

- https://www.toutiao.com/：HTTP200但只有JavaScript身份初始化壳，不含读者界面/帮助；没有执行ttwid脚本、身份注册或参数重试，停止该路线。
- https://www.toutiao.com/feedback/：网页工具报不可访问，无HTTP状态可据；未另行重试。
- 检索长按/不感兴趣、频道、收藏、历史、官方帮助后，命中大量第三方聚合、个人教程或创作侧内容。未把PHP中文网、百度经验、雨多FAQ、个人头条文章作为官方事实，也未查看其视频。
- 既有本产品原件未定位401/403/429失败记录；没有把其他产品的失败算在本案。本轮5次普通GET均200，但首页是壳。未取得读者帮助全集，缺口不等于不支持。
'''
(A/'source-notes.md').write_text(notes)
es=[]
for num,name,title,scope,loc,date in [
(1,'personal-management.html','今日头条个人信息管理指引','Official undated account/data guide; OS/build unknown; do not map to current iOS store build','标题及清除兴趣、关注/粉丝、我的内容各项',None),
(2,'basic-service.html','今日头条基本功能数据处理规则（2024）','Updated 2024-05-06 / effective 2024-05-13; historical wording and paths, OS/build unspecified','日期；1.3.2推荐控制/兴趣重置；原隐私政策链接','2024-05-06'),
(3,'creator-help.html','头条创作者帮助目录与内容相关FAQ','Creator help; not reader help; HTML sidebar menu and one FAQ body, updated 2025-02-21','侧栏及RENDER_DATA.preloadStore.category.menuList；内容相关常见问题正文','2025-02-21')]:
 c=cap[name];es.append({'evidence_id':f'E-TTCD-{num:02}','product_id':'M-052','title':title,'url':c['url'],'checked_on':'2026-10-27','source_type':'official_creator_help' if num==3 else 'official_policy_guide','access_mode':'public_html_body_read','version_scope':scope,'app_build':None,'firmware_version':None,'source_updated_on':date,'locator':loc,'note':'目录与正文区分；未执行产品操作；版本/范围差异见短摘记。','hands_on':False,'visual_observed':False,'archived_artifact':str((A/'source-notes.md').relative_to(L)),'sha256':sha(A/'source-notes.md'),'capture_state':'short_note_and_raw_capture','raw_work_path':c['raw_work_path'],'raw_sha256':c['raw_sha256'],'supersedes':None})
(B/'toutiao-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
fields=[k for k in b['flows'][0] if k!='batch']
f={'flow_id':'F-M052-07','product_id':'M-052','task':'申请个人信息副本的政策路径','depth':'D2','evidence_ids':'E-TT-01','known_branches':'我的→设置→隐私设置→个人信息下载；申请需邮箱，文档称发送下载链接；拒绝邮箱可能无法服务','gaps':'身份验证/确认、等待与重试、文件格式/覆盖范围/链接有效期未知；副本不是可导回备份；未申请或接收','visual_status':'not_observed','hands_on':'false','case_path':'cases/M-052-toutiao.md','checked_on':'2026-10-27'}
with (B/'toutiao-controls-deepening-coverage.csv').open('w',newline='') as out:
 w=csv.DictWriter(out,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerow(f)
changes={
'F-M052-01':{'known_branches':'商店18.8.0仍仅一般功能/修复声明；官网普通GET为身份初始化壳，未取得阅读主链','gaps':'文章来源跳转、阅读器结束/回卡位置、收藏回访和重启续读仍未知'},
'F-M052-02':{'evidence_ids':'E-TT-01;E-TTCD-02','known_branches':'政策仅信息栏X→相关理由；内容屏蔽标签可增删，效果措辞为不再或减少；未证长按','gaps':'理由枚举、作者/关键词/单篇对象范围、提交/撤销/弱网与即时移卡未知'},
'F-M052-03':{'evidence_ids':'E-TT-01;E-TTCD-02','known_branches':'关闭推荐隐藏推荐频道；可分别关闭基于位置/搜索/关注的特征；丰富度为尝试多样化，广告开关另列','gaps':'关注内容是否按时间排序、频道编辑按钮、重启/跨端/恢复内容；2024省略设置层不可套现版'},
'F-M052-04':{'evidence_ids':'E-TT-01;E-TTCD-01;E-TTCD-02','known_branches':'2026政策清兴趣/偏好后重新积累；我的内容查删浏览/收藏等记录；旧指引删除行为措辞和2024永久字样单独保留','gaps':'删除确认/撤销/同步、收藏精确续读、旧文行为删除范围与现版关系未知'},
'F-M052-05':{'known_branches':'搜索框删本地历史；无痕搜索不本地记录展示/用于推荐，仍作搜索与安全基本服务；无痕阅读另限点击浏览','gaps':'无痕切换回执、既有记录是否追溯清理、跨端、实际检索错误与返回恢复未知'}
}
u=[]
for old in b['flows']:
 if old['flow_id'] in changes:
  for k,v in (changes[old['flow_id']]|{'checked_on':'2026-10-27'}).items():
   if old[k]!=v:u.append({'flow_id':old['flow_id'],'source_batch':old['batch'],'field':k,'before':old[k],'after':v,'reason':'原任务补对象/数据/历史版本边界；不改旧批，由root合并'})
jwrite(W/'existing-coverage-updates.json',u)
adds=[]
for old in b['evidence']:
 if old['evidence_id']=='E-TT-01':
  p=R/'work/feed-interactions/tt-privacy.html';assert sha(p)==old['raw_sha256']
  c={'capture_url':old['url'],'checked_on':'2026-10-27','access_mode':'local_reread_of_existing_public_artifact','raw_work_path':str(p.relative_to(R)),'raw_sha256':sha(p),'original_capture_on':'2026-10-26','locator':'范围声明；6.1.2/6.2.2/7.2.1/7.2.2；设置指引可能调整','limitation':'未重新GET；政策适用多系列，本文仍聚焦主App且不为每一路径指定OS/build'}
 elif old['evidence_id']=='E-TT-04':
  t=cap['appstore.html'];c={'capture_url':old['url'],'checked_on':'2026-10-27','access_mode':'public_app_store_html_read','raw_work_path':t['raw_work_path'],'raw_sha256':t['raw_sha256'],'app_build':'18.8.0','source_updated_on':'2026-10-22','locator':'版本历史首项HTML datetime及开发者描述','limitation':'中国区iPhone/iPad条目，未安装；无具体长按/恢复变更说明；排除用户好评'}
 else:continue
 c.update(archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md'),hands_on=False,visual_observed=False)
 before=old.get('additional_captures',[]);adds.append({'evidence_id':old['evidence_id'],'source_batch':old['batch'],'field':'additional_captures','before':before,'after':before+[c],'reason':'旧E同源补取得方式/日期与正文定位，不重新编号'})
jwrite(W/'existing-evidence-additional-captures.json',adds)
inv=[]
def I(n,domain,loc,es,fs,scope,status,gap):inv.append({'inventory_id':f'TT-{n:02}','product_id':'M-052','function_domain':domain,'source_locator':loc,'evidence_ids':es,'flow_ids':fs,'platform_version':scope,'body_status':status,'remaining_gap':gap,'checked_on':'2026-10-27'})
scope='2026-10-14政策；主App研究范围；政策本身跨系列/端，OS/build未知'
rows=[
('发现/正文/来源','开发者描述；政策1.2','E-TT-01;E-TT-04','F-M052-01','declaration_only','未取得原文/来源入口、阅读结束与返回'),
('关注列表','7.2.1；管理指引关注/粉丝','E-TT-01;E-TTCD-01','','body_read_partial_path','我的→关注/粉丝可查编辑；关注/取关确认、时间排序与跨端未知'),
('兴趣频道','6.1.2c','E-TT-01','F-M052-03','body_read_declaration','声明增减排序但无具体编辑入口；相邻清兴趣句不能作为频道编辑路径'),
('单条负反馈','6.1.2f；2024规则1.3.2f','E-TT-01;E-TTCD-02','F-M052-02','body_read_partial_path','X和相关理由；长按、作者/关键词/单篇作用范围未知'),
('标签屏蔽','6.1.2c','E-TT-01','F-M052-02','body_read_partial_path','屏蔽管理查看增删标签；减量或不再推荐，不保证绝对消失'),
('总推荐开关','6.1.2a；2024规则1.3.2a','E-TT-01;E-TTCD-02','F-M052-03','body_read_partial_path','推荐隐藏后其他频道保留；返回/重启未知，历史入口不同'),
('推荐特征子开关','6.1.2b','E-TT-01','F-M052-03','body_read_partial_path','地理位置/搜索/关注各特征；不等同取关/清历史/关系统权限'),
('丰富度','6.1.2d','E-TT-01','F-M052-03','body_read_partial_path','提高后尝试供给常看以外内容；档位/滑块/比例未知'),
('个性化广告','6.1.2a独立段','E-TT-01','','body_read_partial_path','另走广告设置；关闭不减广告量；未新增任务或测实际广告'),
('历史兴趣重置','6.1.2e/7.2.1；旧管理指引及2024规则','E-TT-01;E-TTCD-01;E-TTCD-02','F-M052-04','body_read_version_conflict','兴趣/行为/永久措辞不同；确认与可恢复性未知'),
('浏览历史','7.2.1我的内容','E-TT-01;E-TTCD-01','F-M052-04','body_read_partial_path','查删记录存在；排序、分页、批量/单条撤销、读完状态未知'),
('收藏','开发者描述；7.2.1我的内容','E-TT-01;E-TT-04','F-M052-01;F-M052-04','declaration_and_partial_path','创建/取消按钮、分类、失效内容与续读未知'),
('下载记录','7.2.1我的内容','E-TT-01;E-TTCD-01','F-M052-04','body_read_partial_path','记录对象不等于离线文件；失效、重下与删除恢复未知'),
('搜索历史','6.2.2/7.2.1','E-TT-01','F-M052-05','body_read_partial_path','搜索框可查看删除/清空；本地清除不等于全账户删除'),
('无痕搜索','6.2.2','E-TT-01','F-M052-05','body_read_partial_path','停止本地记录展示/推荐利用，仍用于基本服务；不推断完全匿名'),
('无痕阅读','6.1.2b','E-TT-01','F-M052-05','body_read_partial_path','启用期间点击浏览处理边界；不推断追溯删已有历史'),
('个人信息副本','7.2.2','E-TT-01','F-M052-07','body_read_documented_task','邮箱链接条件有说明；格式/内容全量/有效期/导回未知'),
('安全锁定/解除','7.2.1；管理指引','E-TT-01;E-TTCD-01','','body_read_partial_path','安全中心锁定后全设备退出；解除入口/验证链未析，不等于数据恢复'),
('黑名单','7.2.1；管理指引','E-TT-01;E-TTCD-01','','body_read_partial_path','可查编辑但与作者屏蔽/负反馈对象关系未知'),
('未成年人模式','旧未成年人规则','E-TT-03','F-M052-06','previous_body_only','沿用旧证据，本轮未重抓；没有进入/超时/解锁完整链')]
for n,(d,l,e,f,st,g) in enumerate(rows,1):I(n,d,l,e,f,scope,st,g)
I(21,'App帮助入口','开发者描述@联系头条','E-TT-04','','中国区iOS/iPadOS商店18.8.0','declaration_only','我的→用户反馈；未进入且无FAQ正文')
I(22,'创作者帮助','侧栏menuList及内容相关FAQ','E-TTCD-03','','创作者帮助；正文更新2025-02-21','index_plus_one_body','独立creator-help-index.csv逐条登记；其他正文未读，不算读者覆盖')
with (B/'toutiao-controls-deepening-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');w.writeheader();w.writerows(inv)
# Creator directory is kept separately to avoid inflating reader coverage.
data=json.loads((W/'creator-render-data.json').read_text());nodes=[]
def walk(xs,parent=''):
 for x in xs:
  name=(parent+' / ' if parent else '')+x['Name'];nodes.append({'source_id':x['Id'],'parent_id':x['ParentId'],'level':x['Level'],'menu_path':name,'read_scope':'one_faq_body_read' if x['Id']==282 else 'menu_title_only','scope':'creator_help_not_reader_menu','evidence_id':'E-TTCD-03','checked_on':'2026-10-27'});walk(x.get('Children',[]),name)
walk([x for x in data['preloadStore']['category']['menuList'] if x['ParentId']==0])
with (A/'creator-help-index.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(nodes[0]),lineterminator='\n');w.writeheader();w.writerows(nodes)
old=(W/'case-before.md').read_text()
old=old.replace('查阅日 2026-10-26。以中国大陆今日头条主手机客户端为对象，区分抖音、头条极速版、搜索极速版及 Web。未安装、未登录，系统与构建版本未知。','查阅日2026-10-27。研究对象仍为中国大陆今日头条主App；商店快照可固定iPhone/iPad 18.8.0（HTML发布日期2026-10-22），未安装。政策本身覆盖头条多个系列及Web等形态，不据此将每条路径标为该iOS构建；Android构建未知，极速版/抖音/Web不互套。')
old=old.replace('本次覆盖发现、负反馈、偏好重置、历史和搜索边界；','本次覆盖发现、负反馈、偏好重置、历史、搜索及个人信息副本边界；')
old=old.replace('| F-M052-02 | 信息卡 X→选理由，调整推荐；P→内容屏蔽管理可增删标签 | 提交回执、当前卡片移除、误触撤销、零候选如何处理 | D2 |','| F-M052-02 | 文档只有信息栏X→相关理由；P→内容屏蔽管理增删标签，措辞为不再或减少此类推荐 | 未证长按；理由枚举、单篇/作者/关键词范围、即时移卡、提交撤销和弱网未知 | D2 |')
old=old.replace('| F-M052-03 | P→个性化推荐设置关闭：推荐频道隐藏，仍有关注/热门/要闻；其子项可调推荐偏好及内容丰富度 | 入口是否需重启、开关跨端同步、恢复后的首批内容 | D2 |','| F-M052-03 | P→个性化推荐设置关闭隐藏推荐，保留关注/热门/要闻；设置推荐偏好可分别关闭位置/搜索/关注特征；丰富度仅称尝试增加常看以外内容 | 特征关闭不等于取关/删历史；频道编辑按钮、重启/跨端和恢复首批内容未知；广告开关独立 | D2 |')
old=old.replace('| F-M052-04 | P→清除历史阅读兴趣后重新积累偏好；我的→我的内容可查删浏览/收藏/下载记录 | 模型重置与记录删除是不同操作；确认弹窗、撤销及重建时间未知 | D2 |','| F-M052-04 | 2026政策P→清除历史阅读兴趣后重新积累；我的→我的内容查删浏览/收藏/下载等记录 | 旧指引称删行为、2024规则含永久字样，不能与现政策拼接；确认/撤销/同步和收藏续读未知 | D2 |')
old=old.replace('| F-M052-05 | 搜索框内可删本地历史；P 下个人信息收集设置→无痕搜索；个人信息保护设置→无痕阅读 | 零结果、语音识别错误、范围筛选、结果返回位置与无痕切换回执 | D2（管理）；实际检索主链仍欠缺 |','| F-M052-05 | 搜索框删本地历史；P下无痕搜索停止本地记录展示及推荐利用，但仍用于搜索/安全基本服务；无痕阅读另限启用期间点击浏览 | 既有记录是否追溯清理、跨端与切换回执、零结果、语音错误、返回位置未知 | D2（管理）；实际检索主链仍欠缺 |')
needle='| F-M052-06 | 未成年人模式的存在和限制有规则，但没有可靠开关步骤 | 进入→设年龄→阅读→超时→家长解锁→退出全链未完成 | D1 |'
old=old.replace(needle,needle+'\n| F-M052-07 | 我的→设置→隐私设置→个人信息下载；申请需提供邮箱，政策称发送副本下载链接；拒绝邮箱可能无法提供 | 身份验证/确认、等待重试、文件格式/范围、有效期和导回未知；未申请、未收到文件 | D2文档；E-TT-01 §7.2.2 |')
section='''
## 本轮目录、版本差异与未取得材料

[22项功能来源台账](../batches/toutiao-controls-deepening-inventory.csv)区分功能声明、局部路径和完整任务，不是App全功能目录。另存[创作者帮助目录](../artifacts/toutiao-controls-deepening/creator-help-index.csv)：73行含父子项，只有“内容相关常见问题”读到正文，其余只读取目录名称；它不是读者设置目录，不扩大阅读覆盖。

- **来源与频道：**商店声明关注作者/收藏能力；E-TT-01 §7.2.1和E-TTCD-01给出我的→关注/粉丝查询编辑路径，但未提供关注/取关确认或排序机制。政策声明可增减排序兴趣频道，没有具体编辑按钮；其后相邻的清除兴趣路径不能拿来补频道入口。文章来源跳转仍未知。
- **版本差异：**E-TTCD-01没有日期，清兴趣项笼统说删除操作使用行为；E-TT-01的2026版本明确说历史兴趣与推测偏好。E-TTCD-02更新2024-05-06、生效05-13，部分路径省略设置层且有“永久清除”字样，同时说明会再积累兴趣。不将三份文档合成“永久清空所有历史”的承诺。
- **控制对象：**内容推荐总开关、用于推荐的位置/搜索/关注特征、个性化广告、显式关注列表、屏蔽标签、浏览记录与收藏分别记录。关个性化广告的政策结果是仍有广告且数量不变，不能当成关内容推荐；关闭基于关注的特征不等于取消关注。没有证据确认长按负反馈的对象枚举或绝对屏蔽效果。
- **数据出口：**F07是个人信息副本申请，不证明包含原文/全部收藏、能够导回或恢复阅读位置。账号锁定导致已登录设备退出、解除锁定恢复可用，仅见局部政策路径，未新增完整恢复任务。无痕状态、清历史、清兴趣也不等同删除一切服务器信息。
- **实际取证限制：**官网普通GET为要求JavaScript的身份初始化壳，未执行脚本；反馈URL网页工具不可访问并停止。官方创作者帮助可公开读，但其客服入口和创作信用分不移植成读者行为或学习奖励。长按/频道/历史的第三方教程未采用。18.8.0及近期商店日志仅一般问题修复，不能补阅读结束和恢复。

本轮无新图片/连续观察，无产品状态变更；只增加F07，F01仍D1，F02—05补证沿用编号。旧E追加采集与旧F字段建议交root合并，旧批和主索引未修改。详情见[短摘记](../artifacts/toutiao-controls-deepening/source-notes.md)。

'''
old=old.replace('## 本项目启发与限制',section+'## 本项目启发与限制')
old+='''
新增来源（查阅2026-10-27）：E-TTCD-01[个人信息管理指引](https://sf3-cdn-tos.douyinstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/Personal_information_management.html)，无日期；E-TTCD-02[2024基本功能数据处理规则](https://lf9-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/e327f6fd-d5b9-4090-b44e-d65dc891020a.html)，1.3.2；E-TTCD-03[创作者帮助目录与内容FAQ](https://baike.toutiao.com/detail/195/282/0)，正文更新2025-02-21。均为正文/目录读取，不是实操。
'''
(L/'cases/M-052-toutiao.md').write_text(old)
jwrite(W/'build-result.json',{'new_E':3,'new_F':1,'inventory':len(inv),'creator_index_parent_child_rows':len(nodes),'creator_body_read_nodes':sum(n['read_scope']=='one_faq_body_read' for n in nodes),'old_F_changed':len(changes),'old_F_fields':len(u),'old_E_captures':len(adds),'images_observed':0})
print((W/'build-result.json').read_text())
