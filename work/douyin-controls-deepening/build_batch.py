from pathlib import Path
import csv,json,hashlib,shutil,copy
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/douyin-controls-deepening';B=L/'batches';A=L/'artifacts/douyin-controls-deepening'
assert not (W/'build-result.json').exists(),'one-time builder'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def writecsv(p,rows,fields=None):
 with p.open('w',newline='') as h:
  w=csv.DictWriter(h,fieldnames=fields or list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
sources=json.loads((W/'sources.json').read_text());images=json.loads((W/'image-captures.json').read_text());crops=json.loads((W/'guide-crops.json').read_text())
for s in sources:
 assert sha(R/s['raw_work_path'])==s['raw_sha256'] and sha(R/s['decoded_work_path'])==s['decoded_sha256']
for x in images:assert x['status']==200 and sha(R/x['raw_work_path'])==x['raw_sha256']
for x in crops:assert sha(R/x['path'])==x['sha256']
A.mkdir(parents=True,exist_ok=False)
observed=[]
for x in images[1:6]:
 ext='.png' if 'png' in x['content_type'] else '.jpg';p=A/f"article-{x['article_id']}-op{x['op_index']}{ext}";shutil.copyfile(R/x['raw_work_path'],p)
 observed.append(dict(article_id=x['article_id'],locator=f"op{x['op_index']}",artifact=str(p.relative_to(L)),sha256=sha(p),url=x['url'],raw_work_path=x['raw_work_path']))
for x in crops:
 p=A/Path(x['path']).name;shutil.copyfile(R/x['path'],p)
 observed.append(dict(article_id=x['article_id'],locator=f"tile{x['tile']:02}, y={x['y']}, h={x['height']}",artifact=str(p.relative_to(L)),sha256=sha(p),raw_work_path=x['path']))
note=(W/'observations.md').read_text()+'\n## 原文与归档定位\n\n| 官方文章 | 发布日 | 原件获取/复读 |\n| --- | --- | --- |\n'
for s in sources:note+=f"| [{s['title']}]({s['url']}) | {s['source_published_on']} | 2026-10-26原JSON，10-27本地复读；原图10-27新GET |\n"
note+='\n以下20张正式图像均已实际查看。长图裁片有重叠，不能当20个步骤或20次操作。原始8图及15裁片的散列保存在本批采集清单。\n\n| 来源/定位 | 可复查图像 | SHA256 |\n| --- | --- | --- |\n'
for x in observed:note+=f"| {x['article_id']} {x['locator']} | [图像]({Path(x['artifact']).name}) | `{x['sha256']}` |\n"
N=A/'source-notes.md';N.write_text(note)
base=dict(product_id='M-051',checked_on='2026-10-27',app_build=None,firmware_version=None,source_updated_on=None,hands_on=False,visual_observed=True,visual_kind='static_official_guide_images',archived_artifact=str(N.relative_to(L)),sha256=sha(N),supersedes=None,version_scope='中国大陆主抖音；历史官方图示，OS/构建未知，非当前实操')
es=[]
for number,aid,date,parent,locator in [(1,'15710','2025-05-21','E-DOUY-03','op15 four-panel controls'),(2,'3111768381106164','2026-01-14','E-DOUY-06','op7/10/14/23, four static composites'),(3,'9841769585856405','2026-01-28',None,'five sections, nine overlapping crops'),(4,'5411770782085474','2026-02-11',None,'two feedback sections and three tools, six overlapping crops')]:
 s=next(x for x in sources if x['id']==aid);ims=[x for x in observed if x['article_id']==aid];orig=next(x for x in images if x['article_id']==aid and (aid!='15710' or x['op_index']==15))
 e=dict(base,evidence_id=f'E-DYCD-{number:02}',title=s['title']+'：已看静态图',url=orig['url'] if parent else s['url'],source_type='official_instruction_images',source_published_on=date,access_mode='prior_public_article_JSON_reread_and_public_static_image_GET_viewed',locator=locator,capture_state='formal_images_and_independent_notes',raw_work_path=s['raw_work_path'],raw_sha256=s['raw_sha256'],raw_image_work_path=orig['raw_work_path'],raw_image_sha256=orig['raw_sha256'],observed_artifacts=ims,note='只重建图文步骤，未进行App操作；示例数字非默认值，具体冲突见摘记',source_captured_on='2026-10-26',image_captured_on='2026-10-27')
 if parent:e['derived_from']=parent
 es.append(e)
existing=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines() if s]
assert not ({x['evidence_id'] for x in es}&{x['evidence_id'] for x in existing})
assert not ({x['url'] for x in es}&{x['url'] for x in existing})
(B/'douyin-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
old=json.loads((W/'coverage-before.json').read_text());changes={
 'F-M051-01':('E-DOUY-01;E-DOUY-04;E-DYCD-02;E-DYCD-04','侧栏历史/缓存/稍后再看及收藏夹示例仅入口；仍无连续观看至回访','启动、暂停、任务终点、收藏夹中间入口及重启准确恢复未知'),
 'F-M051-02':('E-DOUY-03;E-DYCD-04','长按/分享不感兴趣；图示长按菜单，滑走信号和举报分别记录','当前卡片结果、撤回、弱网提交和后续推荐变化未测'),
 'F-M051-03':('E-DOUY-03;E-DYCD-01;E-DYCD-04','探索更多有取消/确认；三级内容偏好/恢复默认；关键词永久或短期屏蔽','确认后的回执、统计周期、短期时长、清除/撤销/跨端一致未知；已选12仅示例'),
 'F-M051-04':('E-DOUY-02;E-DYCD-04','政策关闭推荐路径保留；搜索结果分类和用户关注静态示意','关闭推荐现版界面、搜索返回位置/零结果/筛选保存和取消关注未操作'),
 'F-M051-05':('E-DOUY-05;E-DOUY-06;E-DYCD-02;E-DYCD-03','孩子三页权限说明后协议/接受；家长近7日统计/时间锁/取消订阅/解除绑定按钮；示图上限3孩子','扫码失败/拒绝/解绑确认和恢复未知；时间锁未开启示图与默认规则适用范围不同，夜间密码例外冲突保留'),
 'F-M051-06':('E-DOUY-06;E-DYCD-02;E-DYCD-03','部分知识视频AI笔记摘要/蓝色章节时间码/X关闭；只静态图','生成失败/纠错、保存导出、实际章节跳转及返回进度未知；不是掌握判定')}
props=[]
for x in old:
 y=copy.deepcopy(x);ev,br,gap=changes[x['flow_id']];y.update(evidence_ids=ev,known_branches=br,gaps=gap,checked_on='2026-10-27',visual_status='static_official_images_only');props.append(dict(flow_id=x['flow_id'],before=x,after=y))
dump(W/'existing-coverage-updates.json',props)
new=[]
for number,task,ev,branch,gaps in [
 (7,'进入和关闭未成年人模式','E-DYCD-03;E-DOUY-05','我侧栏/首页更多/搜索/弹窗进入；首页或我左上关闭，文档有密码校验','年龄实名条件与历史指南不同；X/不再提醒、验证失败、退出后的数据/频道恢复未测'),
 (8,'按上学日和休息日设儿童时长','E-DYCD-03;E-DOUY-05','首页菜单时间管理或我侧栏守护工具时间锁；分上学日/休息日，日超限密码提示','保存/撤销和跨端累计时点未知；夜间密码截图与强制关闭文字冲突，不能认定例外有效'),
 (9,'选择适龄内容并按主题找内容','E-DYCD-03','首页左上适龄内容→年龄→设置完成；发现下拉全部频道，陪伴周刊分类/期刊，订阅频道','1–18岁为图文范围而非能力分级；年龄改后保存/推荐变化、频道进入返回、取消订阅未测'),
 (10,'阻止指定账号继续出现','E-DYCD-04','账号主页右上…→拉黑，区别于某条视频不感兴趣及类别偏好','拉黑确认、成功回执、解除、旧收藏/关注处理及跨端结果未知'),
 (11,'从可疑内容进入核查卡片','E-DYCD-04','视频左下AI抖音求真链接或搜索事件词→求真卡；核查结果与事件脉络示意','无结果/存疑、来源打开/日期与修订、AI误判纠错、返回视频位置未知；不验证示例事实')]:
  new.append(dict(flow_id=f'F-M051-{number:02}',product_id='M-051',task=task,depth='D2',evidence_ids=ev,known_branches=branch,gaps=gaps,visual_status='static_official_images_only',hands_on='false',case_path='cases/M-051-douyin.md',checked_on='2026-10-27'))
writecsv(B/'douyin-controls-deepening-coverage.csv',new)
updates=[]
for eid,aid in [('E-DOUY-03','15710'),('E-DOUY-06','3111768381106164')]:
 e=next(x for x in existing if x['evidence_id']==eid);s=next(x for x in sources if x['id']==aid);before=e.get('additional_captures',[])
 cap=dict(checked_on='2026-10-27',access_mode='prior_public_JSON_local_reread',raw_work_path=s['raw_work_path'],raw_sha256=s['raw_sha256'],archived_artifact=str(N.relative_to(L)),sha256=sha(N),note='旧原件本地复读，文内静态图本轮查看另以E-DYCD-01/02登记；非新文章网络采集')
 updates.append(dict(evidence_id=eid,before=before,after=before+[cap]))
dump(W/'additional-captures-updates.json',updates)
rows=[]
def row(title,loc,ev,flows='',status='static_guide_control',gap='仅图文；结果、取消和恢复待核'):
 rows.append(dict(item_id=f'DYCD-{len(rows)+1:03}',product_id='M-051',item=title,source_locator=loc,evidence_ids=ev,flow_ids=flows,status=status,gap=gap,checked_on='2026-10-27'))
for title in ['进入未成年人模式','健康使用','优质陪伴','亲子护航','退出未成年人模式']:row(title,'child guide section','E-DYCD-03',status='section_parent')
for loc,flows,titles in [
 ('tiles01–02','F-M051-07',['我侧栏入口','首页更多入口','搜索模式名入口','开启弹窗/X/不再提醒']),
 ('tiles02–03','F-M051-08',['时间管理与时间锁入口','上学日时长','休息日时长','日超限密码提示','夜间限制与密码冲突']),
 ('tiles03–04','',['视力保护入口','蓝光弱化开关','护眼提醒开关/了解更多','服务热线支持入口']),
 ('tiles04–05','F-M051-06',['AI笔记入口/摘要/章节']),
 ('tiles04–06','F-M051-09',['创作者订阅/订阅频道','适龄内容1–18岁/完成/X','文本搜索入口','语音搜索声明','搜索建议/换一换','发现全部频道入口','陪伴周刊期刊卡片']),
 ('tiles07–08','F-M051-05',['家长发起扫码绑定','孩子三页权限说明','协议/接受申请','近7日使用统计','未成年人模式开关','家长时间锁','家长订阅管理/取消订阅','解除绑定按钮','最多3个孩子示图']),
 ('tiles08–09','F-M051-07',['首页左上关闭','我左上关闭','校验密码/成年自动退出声明'])]:
 for t in titles:row(t,loc,'E-DYCD-03',flows)
for t in ['推荐','陪伴周刊','院士说','公开课','科普中国','学科知识','代码世界','动物世界','科学探秘','放映厅','美食','国学文化','文学艺术','走进太空','十万好奇','云逛馆','才艺','绘画','体育运动','萌宠','益智玩具','环游世界']:row(t,'child tile06 全部频道','E-DYCD-03','F-M051-09','visible_channel_only','22个可见频道；非现版完整目录，未逐一进入')
for t in ['名师学者','趣味科普','前沿科技','兴趣探索','人文通识','空中课堂','家庭生活','网络素养']:row(t,'child tile06 陪伴周刊','E-DYCD-03','F-M051-09','visible_category_only')
for title in ['正反馈指导','负反馈指导','使用管理助手','AI抖音求真','官方信息验证助手']:row(title,'adult guide section/tool','E-DYCD-04',status='section_parent')
for loc,flows,titles in [
 ('adult tiles01–03','F-M051-01',['看完/点赞等推荐指导非效果证据','评论输入示意','收藏星标','新建收藏夹名称/公开/取消/确认','搜索用户/关注','关键词搜索及结果分类']),
 ('adult tiles03–04','F-M051-02',['滑走负信号','长按不感兴趣','长按举报入口','长按倍速控件','长按清屏入口','长按稍后再看入口']),
 ('adult tile04','F-M051-10',['主页更多/拉黑']),
 ('adult tile05','F-M051-03',['助手入口','探索更多确认/取消','类别减少/默认/增加','关键词填写','永久/短期屏蔽/提交']),
 ('adult tile05','F-M051-11',['视频求真链接','搜索事件词求真卡','核查结果/事件脉络示意']),
 ('adult tiles05–06','',['搜索官方客服/立即咨询/验证助手','短信/电话/网址/邮箱/账号/活动','动态口令/失效记录'])]:
 for t in titles:row(t,loc,'E-DYCD-04',flows)
for t in ['助手内容管理/时间管理标签','近期类别色环','恢复默认按钮','类别选择/X/去调节']:row(t,'15710 op15','E-DYCD-01','F-M051-03')
for t in ['观看历史侧栏入口','离线缓存侧栏入口','稍后再看侧栏入口']:row(t,'3111768381106164 op7','E-DYCD-02','F-M051-01','entry_only')
row('探索风格宣传插画','15710 op4','E-DOUY-03',status='excluded_illustration',gap='已看，非真实界面；不补操作')
writecsv(B/'douyin-controls-deepening-inventory.csv',rows)
case=(W/'case-before.md').read_text().replace('查阅日 2026-10-26。','初查2026-10-26，续查2026-10-27。',1)
case=case.replace('全部 `hands_on=false`、`visual_observed=false`。','全部 `hands_on=false`；初查未看图，续查静态图范围见下。')
case=case.replace('文内图片/视频未观看。','初查未看文内图片/视频；续查只看静态图片，视频仍未观看。')
case=case.replace('另发现图像形式的《未成年人模式使用指南》，尚未观察，不用于补充步骤。','初查发现但未看图的《未成年人模式使用指南》，已在续查逐片查看，见下。')
case=case.replace('查阅日 2026-10-26','初查2026-10-26')
case+='''

## 2026-10-27：逐图补查显式偏好、监护和退出

新增[来源功能目录](../batches/douyin-controls-deepening-inventory.csv)与[逐图摘记/20张归档图](../artifacts/douyin-controls-deepening/source-notes.md)。目录覆盖本次两篇长指南、两篇功能文章中的已读章节/控件/频道；不是整个抖音菜单或全部功能。4篇原JSON于10月26日取得，本轮本地复读；8原图本轮GET，两个长图15张重叠裁片及其余6图全部实际查看。宣传插画排除。静态图按D2材料使用，没有D3连续演示或D4操作。

原表是初查基线，以下补充与coverage.csv为当前细化状态；初查未解决的问题只有被明确补证才改变。

| F | 现在具体知道什么 | 还不能据此设计定案的部分 |
| --- | --- | --- |
| F-M051-01 | 侧栏示例有历史、离线缓存、稍后再看；收藏夹示例有名称/公开开关/取消/确认 | 主观看至退出回访仍D1；中间入口、保存与重启位置没有闭合 |
| F-M051-02 | 长按菜单不感兴趣；滑走、举报是其他信号/操作 | 误触撤回、当前卡片与弱网提交未知 |
| F-M051-03 | 探索更多有确认/取消；类别三级强度、恢复默认；关键词永久/短期屏蔽 | 未知具体期限、回执、撤销和跨端结果；示例已选12个非上限 |
| F-M051-04 | 搜索示意显示分类结果及用户关注入口 | 关闭推荐依然仅政策步骤；返回与筛选持久性未测 |
| F-M051-05 | 孩子三页权限确认后协议/接受；家长页近7日、时间锁、订阅管理、解除绑定、至多3孩子示例 | 未观察拒绝、解绑、误绑和忘记密码恢复；未开启时间锁图不代表默认规则 |
| F-M051-06 | 摘要/蓝色章节时间码/X关闭已见静态图 | 生成/失败/纠错、实际跳转和保存未知 |
| F-M051-07（新D2） | 四种模式入口；首页左上或我左上关闭，图文有监护密码与成年退出说明 | 年龄/实名/历史差异未闭合，不能承诺所有5–6岁账号可关闭 |
| F-M051-08（新D2） | 时间管理分上学日/休息日，日超限密码提示 | 夜间示图与旁文及六月须知冲突；不认定密码例外有效 |
| F-M051-09（新D2） | 适龄内容选年龄并完成；发现下拉频道、陪伴周刊分类、订阅频道 | 1–18岁只是内容条件；改龄后保存、实际推荐及退出恢复未测 |
| F-M051-10（新D2） | 指定账号主页…→拉黑 | 确认/解除、旧关注/收藏及跨端处理未知 |
| F-M051-11（新D2） | 视频求真链接或搜索事件词→求真卡 | 卡片出现条件、来源打开、修订、误判纠错和返回视频未测；不证明示例结论正确 |

### 必须保留的冲突与适用范围

1月28日儿童指南的夜间锁定示图提到监护密码继续，但同图旁文称强制关闭、6月须知也写不能使用。入口截图没有交代年龄、实名与版本，不能合并为“所有儿童夜间可密码解锁”。退出模式同理，实名未满14岁的限制须保留。此处仅比较产品材料，不作实际账号操作承诺。

2月11日成人指南推荐看完、点赞、细评论与收藏，以改善偏好命中；其中观看比例不是学习掌握阈值，也不是本研究验证过的推荐公式。新建收藏夹示例缺中间导航，不因此新增一条完成的收藏任务。AI求真卡与官方身份验证助手分开：前者呈现平台核查，后者核验是否自称官方，不能互相代替。

### 设计复用位置

对G001，可借鉴显式选择探索、按类别调整、以主题刊物进入内容；还应核查质量和视角覆盖，不能仅以看完/点赞/收藏计学习。儿童年龄筛选与理解能力分开；频道丰富不证明适合5–6岁独立消费。对G002，需辨别当前视频、账号、类别、关键词、时长与孩子身份这些不同操作对象，并查清提交结果。以上为助手分析，未升级为需求。

新增证据：E-DYCD-01为2025-05-21助手四栏图（派生E-DOUY-03）；E-DYCD-02为2026-01-14四张亲子/AI示意（派生E-DOUY-06）；E-DYCD-03为2026-01-28完整儿童指南；E-DYCD-04为2026-02-11完整成人指南。原图URL、定位、散列与查阅方式见证据索引，正式图像从摘记进入。没有把这批图的观察扩写为实际点击成功；本案例仍部分覆盖。
'''
(L/'cases/M-051-douyin.md').write_text(case)
dump(W/'observed-artifacts.json',observed)
dump(W/'build-result.json',dict(new_evidence=4,new_flows=5,old_flows=len(props),old_flow_fields=sum(sum(x['before'].get(k)!=v for k,v in x['after'].items()) for x in props),old_evidence_captures=2,inventory_rows=len(rows),formal_images=len(observed),requirements_changed=False))
print((W/'build-result.json').read_text())
