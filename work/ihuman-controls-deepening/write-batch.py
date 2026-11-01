from pathlib import Path
import hashlib,json,csv,shutil
r=Path('docs/research/interaction-library');w=Path('work/ihuman-controls-deepening');case=r/'cases/M-029-ihuman.md'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
b=json.loads((w/'baseline.json').read_text());assert sha(case)==b[str(case)],'concurrent case change'
a=r/'artifacts/ihuman-controls-deepening';a.mkdir(parents=True,exist_ok=True)
for name in ['official-map.png','official-settings.png','2018-proposal-image-1.jpg']:shutil.copyfile(w/name,a/name)
audit='''# M-029 静态图核查

查阅/实际查看日期：2026-10-26。公开 HTML 定位原始图片，HTTP GET 后用 view_image 查看；无登录、安装、连续录像或实机操作。图片保持原始字节；不复制完整文章。静态图最高支撑局部 D2，不升 D3。

| E | 文件与来源定位 | 已观察 | 不可据此推定 | SHA-256 |
| --- | --- | --- | --- | --- |
| E-IHCD-02 | official-map.png；官方隐私入口指引，洪恩识字第一图；https://www.ihuman.com/agreement/privacy/images/images1.png | 地图示例、右上设置；官方已加红框箭头 | 地图点击后的选课、关卡解锁、设置验证动作；图未显示系统版本 | {MAP} |
| E-IHCD-02 | official-settings.png；同条第二图；https://www.ihuman.com/agreement/privacy/images/images2.png | 图内明确当前版本3.0.1；学习设置、离线缓存、学习报告、账户中心、关于我们；音乐/基地开关、检查更新、开始诊断、法律条款与隐私中心进入 | 图为设置页下部，不是全部选项；不证当前4.5.2位置、缓存对象、诊断结果或操作后保存 | {SETTINGS} |
| E-IHCD-03 | 2018-proposal-image-1.jpg；_紫夜1体验文第7节第一幅配图；https://exp-picture.cdn.bcebos.com/e076d77622bc7dc5d6797feb5e460596b91429b6.jpg | 历史练环节、多个字形选项、返回/左右箭头和暂停符号 | 作者在改进建议节用旧界面举例；新增示范动作/休息提醒均是建议。未观察选择、声音、错答或切换后的状态 | {HISTORY} |

历史文章自报：iPhone6s、iOS11.2.6、洪恩识字1.8.1，体验2018-03-18，发布/更新2018-03-19。只记录作者披露的范围，不套目前5–6岁用户或当前构建。

另外四张图片取得失败：2018文章第7节第二图，以及2022退出登录教程的全部三图，均HTTP403。请求完成后停止，未改请求身份、未换镜像或图像处理参数重试；这些图片未看、未归档、未标visual。2022教程仅使用可读正文。
'''.replace('{MAP}',sha(a/'official-map.png')).replace('{SETTINGS}',sha(a/'official-settings.png')).replace('{HISTORY}',sha(a/'2018-proposal-image-1.jpg'))
(a/'static-observation-log.md').write_text(audit)
s=case.read_text()
s=s.replace('中国大陆洪恩识字，iOS 商店查阅时列 4.5.0；新增核查的小米 Android 渠道列 4.5.2，更新时间 2026-10-23，包名', '中国大陆洪恩识字。旧商店索引列 iOS 4.5.0；本轮公开 HTTP 取得的中国 App Store 正文已列 **4.5.2 / 2026-10-23**（HTML `time datetime`），搜索索引仍可能显示 4.5.0，二者不混作同一快照（E-IHCD-01）。小米 Android 渠道原已核 4.5.2，更新时间 2026-10-23，包名')
s=s.replace('| 家长设置 | 时长、每日目标、速度、环节与允许跳过、识字报告和字库打印 | 02；是否能按孩子分别设置未查 |','| 家长设置 | 时长、每日目标、速度、环节与允许跳过、识字报告和字库打印；官方3.0.1图另见设置页与返回箭头 | E-IHUM-02、E-IHCD-01、02；图未显示教学环节开关，不能据此核对当前全部设置；按孩子分别配置未知 |')
s=s.replace('| 数据与账号 | 隐私中心、个人资料与成长报告副本 | 03、04；副本不等于可恢复备份 |','| 数据与账号 | 隐私中心、个人资料与成长报告副本；历史账户中心与退出登录入口 | E-IHUM-03、04、E-IHCD-02、04；副本、退出登录、删除进度和恢复购买是不同任务 |\n| 离线与故障诊断 | 官方3.0.1图有离线缓存页签、开始诊断、检查更新入口 | E-IHCD-02；缓存哪些课、过期/删除、离线保存与网络诊断结果均未核；只发现入口，不新增完整流程 |')
s=s.replace('| F-M029-01 学一个字并进入阅读 | 从地图/教材内容选字；依启用环节经历玩→认→练→写→说；之后可用已学字读子集故事 | 表达从字延伸到词句；复习与报告存在，但各环节是否自动串行、何时计入“学会”未知 | 单字完成页、奖励、进入绘本的解锁条件未知 | 01、02；这是内容循环，未证明一字到一书是自动直连 |','| F-M029-01 学一个字并进入阅读 | 开发者说明从地图/教材选字，依启用环节经历玩→认→练→写→说；再用已学字读子集故事 | 2018年1.8.1旧练图可见字形选项和导航；没有作答结果。当前说明从单字延伸词句，不等于每环节已观察 | 单字完成、复习结算、奖励及绘本解锁仍未知 | D1；E-IHUM-01、02、E-IHD-02、E-IHCD-01、03。旧界面不补当前主链；非一字到一书自动直连 |')
s=s.replace('| F-M029-02 调节游戏与跳过 | 历史官方回复给出右上设置→学习设置→教学环节；可关闭游戏，并把环节跳过改成不能跳过 | 当前商店仍列可调环节/跳过；历史路径与当前位置要分开 | 是否影响正在进行的字、是否立即生效未知 | 02；2020-05-04 开发者回复只作历史操作依据 |','| F-M029-02 调节游戏与跳过 | 2020-05-04官方回复：右上设置→学习设置→教学环节，可关闭游戏并将环节设为不能跳过 | 当前商店仍声明可调环节；3.0.1官方图补到设置页，未显示该开关 | 课内立即生效、只对新任务生效及保存反馈均未知 | D2历史路径；E-IHUM-02、E-IHCD-01、02。2020-07-29关于重置进度的回复只是考虑建议，不证已提供重置 |')
s=s.replace('| F-M029-03 获取孩子报告副本 | 首页右上设置→学习设置→法律条款和隐私中心；进入个人信息管理→下载本人/孩子个人信息 | 登录并授权相册后，档案与成长报告可整合为图片下载 | 得到图片副本；无结构化进度恢复承诺 | 03、04；入口说明注明识字版本不低于 3.0.1 |','| F-M029-03 获取孩子报告副本 | 首页右上设置→学习设置→法律条款和隐私中心；个人信息管理→下载本人/孩子信息 | 既有政策称登录、授权相册后可导出图片；本轮官方图明确“进入”按钮及图内3.0.1 | 图片副本不等于可恢复的练习状态 | D2；E-IHUM-03、04、E-IHCD-02。文字范围>=3.0.1，静态界面只锁定图内3.0.1，平台/日期未知 |')
s=s.replace('| F-M029-04 按教材核查整课汉字 | 选择年级、书册、课文→先学单字→进行整课测验 | 开发者列有整课测验；题型、错字标记与测验结果的数据变化未知 | 未核定通过、未通过后的补练、退出和重进 | E-IHD-01；D1，不能由功能说明补出按钮及结果页 |','| F-M029-04 按教材核查整课汉字 | 年级、书册、课文→单字→整课测验是开发者声明 | 本轮iOS日志将部编版学习地图上线定位到4.4.0/2026-02-05，未给控件/判分 | 通过/未通过补练、退出重进仍未知 | D1；E-IHD-01、E-IHCD-01。版本日志不补成操作链 |\n| F-M029-05 历史退出登录入口 | 2022作者教程自报iPhone12/iOS15.6/3.7.2：主页右上设置→按提示完成家长数字验证→账户中心 | 正文指向退出登录；三张图HTTP403未查看，无可核的退出后画面 | 未核二次确认/取消、退出后身份、离线数据保留与重新登录恢复 | D1；E-IHCD-04。作者当前显示已注销，原作者昵称无法核；不是退出课程、强退App或重置孩子进度 |')
s=s.replace('没有按钮就明确记录不支持。','若未见按钮，应记录本构建与所查路径未见入口，不据此断言全产品不支持。')
s += '''

## 本轮控制补证：边界与剩余任务

本轮没有新增 D2 学习流程。新增 F05 只是历史账号退出入口，仍 D1；F01/F04 主链深度不变，F02/F03 只补旧路径的来源和静态边界。正式保存的三张图及原始URL/SHA见[静态核查记录](../artifacts/ihuman-controls-deepening/static-observation-log.md)。当前iOS版本更新到4.5.2是公开渠道资料修正，不表示本轮运行了该版本。

| 一次识字任务节点 | 可得材料具体支持什么 | 不能补出的反馈/状态 |
| --- | --- | --- |
| 选课 | 官方3.0.1地图静态图、现行教材描述 | 点击哪个已解锁字、下载中/锁定状态、已会字选择和返回原课 |
| 玩、认、练、写、说 | 当前五步是开发者描述；1.8.1作者练图仅显示字形选项、返回/左右箭头与暂停符号 | 没看到点击/拖放、声音或判定，不能指定正确选项、说环节评分、暂停后计时或退出确认 |
| 错答与再试 | 官方只声明AI笔画纠错；旧图无错误画面 | 错笔/错音/错字的提示内容、重试次数、示范触发、求助出口和提示后成绩 |
| 复习与结算 | 现行描述有复习/检测；2018作者报告自述学习后及次日复习 | 当前排程、强制与可取消的边界、奖励、达标阈值及重复完成结果 |
| 家长配置 | 2020官方回复给教学环节控制；3.0.1图确认设置容器与部分设置项 | 验证失败反馈、两个孩子各自配置、保存时点、正在进行的任务是否采用新设置 |
| 退出和恢复 | 3.7.2教程描述退出账号入口；2020开发者对重置进度建议仅表示考虑 | 返回地图、切后台、退出登录、删除档案/重装四种情形的保存范围均不能互相替代；尚无可重建恢复链 |

**历史作者材料的用法**：_紫夜1在2018-03-19发表的原文自报1.8.1、iPhone6s/iOS11.2.6、体验2018-03-18。其“如果我是PM”一节提出游戏首次示范、定制休息提醒等改进，均未当成已实现功能；本轮只把该节第一幅旧练界面图作为局部静态证据。转载站将同文标为2026年，未采用该日期。原文对复习/休息的自述没有故意错答记录，不能推定4.5.2的强制规则，也不作为5–6岁适用或效果证据。E-IHCD-03。

**进度争议的证据口径**：2020-07-29开发者回复承接家长删除/重置学习进度的建议，表示会考虑；本轮只确认当时未作已实现承诺，不把用户评价当故障复现，也不推出当前没有此功能。当前版本日志4.5.2和4.5.1均只写体验优化，不能推断相关问题已修复。E-IHCD-01。

**新增查询与失败**：除现有官方资料，本轮检索官方域名的“教程/帮助/家长指南/错误提示/重试/复习/学习进度恢复/离线缓存”，并检索带平台版本的一手体验文和设置教程。官网公开脚本只检查与help/FAQ/guide相关的文字线索，未取得学习操作帮助；未调用用户接口。官方客服页仍只给联系渠道，没有可读的识字操作FAQ（E-IHCD-05）。App Store的web入口不可访问，普通公开HTTP取得正文200；未使用账户。2022教程三图及2018另一张比较图返回403后停止，没有换镜像、身份或参数重试；未把图片正文描述当视觉观察。官网下载链接未用于安装，介绍视频未下载或观看。

对本家庭的推断：5–6岁且已可独立阅读的孩子，能否直接进入合适故事、从陌生字返回原句，比强制完成全套游戏更需要核定。AI应将“家长允许跳过”“实际跳过”“提示后答对”“独立复习正确”“完成课程”和“账号已退出”分别表示；报告导出与课程缓存也不是草稿备份。这些仍是G-001/G-002下的项目建议，不是洪恩已开放的数据接口或本项目新增需求。

后续最小取证仍需官方同构建完整教程或经授权设备：在4.5.2固定渠道、同一字与同一家庭配置下，从选字记录一次错选/错笔→提示→再试→结算→复习；分开记录返回地图、后台中断和账号退出后的恢复。没有这些连续材料，本轮不宣告主循环完成。

本轮新增证据均查阅2026-10-26：

- E-IHCD-01：[中国App Store公开正文](https://apps.apple.com/cn/app/id1139733348)，当前4.5.2/2026-10-23，4.5.1/10-09，4.4.0/2026-02-05；历史开发者回复2020-05-04与07-29。正文与HTML时间字段，未看商店图片；补E-IHUM-02，不替换历史回复范围。
- E-IHCD-02：[官方隐私入口图](https://www.ihuman.com/agreement/privacy/operator.html)，洪恩识字第一、二图；第二图可见当前版本3.0.1，日期/平台未知。实际查看静态图；补E-IHUM-03，未执行设置/下载。
- E-IHCD-03：[_紫夜1原始体验文](https://jingyan.baidu.com/article/f54ae2fc6c21ec1e92b849c7.html)，2018-03-19；第1节范围、第4节自述、第7节第一图及改进建议边界；仅所述旧版，不引用市场份额或学习效果观点。
- E-IHCD-04：[2022退出登录教程](https://jingyan.baidu.com/article/75ab0bcba4d91997874db22b.html)，2022-08-24，作者现显示“此账号已注销”；正文自报iPhone12/iOS15.6/3.7.2。仅正文，图访问403未观察；不将账号退出写成课程恢复。
- E-IHCD-05：[官方客服支持](https://www.ihuman.com/support/)，本轮正文仍仅联系方式；未联系、未提交资料，不据此推定不存在其他App内帮助。
'''
case.write_text(s)
def evidence(n,title,url,stype,mode,scope,locator,note,build=None,date=None):
 return dict(evidence_id=f'E-IHCD-{n:02d}',product_id='M-029',title=title,url=url,checked_on='2026-10-26',source_type=stype,access_mode=mode,version_scope=scope,app_build=build,firmware_version=None,source_updated_on=date,locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
ev=[
evidence(1,'中国App Store当前构建及历史开发者回复复核','https://apps.apple.com/cn/app/id1139733348','official_app_store_listing_and_developer_reply','opened_public_html','CN iPhone/iPad snapshot; current listing and 2020 replies kept separate','版本历史time datetime；2020-05-04/07-29开发者回复','当前4.5.2/2026-10-23，索引仍可能为4.5.0；4.4.0/2026-02-05列部编版地图。2020关于重置进度仅表示考虑建议，不证现行支持或不支持。','4.5.2 (listing only)','2026-10-23'),
evidence(2,'官方隐私入口图锁定3.0.1设置页','https://www.ihuman.com/agreement/privacy/operator.html','official_help_static_screenshots','opened_text_and_static_images','洪恩识字 path text >=3.0.1; screenshot itself displays3.0.1; platform/date unknown','洪恩识字第一图images1.png；第二图images2.png','已看地图设置入口与设置页；图内3.0.1、离线缓存/报告/账户中心、诊断入口可见，不证缓存对象/设置生效/当前按钮。','3.0.1 (visible screenshot label)'),
evidence(3,'2018版本化作者体验及改版建议边界','https://jingyan.baidu.com/article/f54ae2fc6c21ec1e92b849c7.html','third_party_firsthand_report','opened_text_and_one_static_image','Author _紫夜1; iPhone6s/iOS11.2.6; experienced2018-03-18','第1节版本；第4节自述；第7节第一幅旧练图与改进建议','自报1.8.1；仅看旧练界面选项/导航，没有选择或错答。首次示范与定制休息提醒是作者建议，不是产品事实；另一图403停止。','1.8.1 (author reported)','2018-03-19'),
evidence(4,'历史3.7.2退出账号教程','https://jingyan.baidu.com/article/75ab0bcba4d91997874db22b.html','third_party_self_declared_original_tutorial','opened_text_images_forbidden','iPhone12/iOS15.6/3.7.2; author account now deleted','工具/原料；三步方法；原作者自述原创','正文描述设置→家长数字验证→账户中心退出登录；未展示退出后状态。三图HTTP403后停止，未视觉观察；不能推断课程退出/重置/恢复。','3.7.2 (author reported)','2022-08-24'),
evidence(5,'官方支持页公开帮助范围复核','https://www.ihuman.com/support/','official_support_landing','opened_text','Current public support page snapshot2026-10-26; not app help contents','客服支持正文','只发现联系方式，没有识字逐题操作FAQ；未联系；不能据此断言App内没有帮助。')]
ev[0]['qualifies']='E-IHUM-02';ev[0]['raw_work_path']=str(w/'apple.html');ev[0]['raw_sha256']=sha(w/'apple.html')
ev[1]['qualifies']='E-IHUM-03'
for idx,name,extra in [(1,'official-settings.png','official-map.png'),(2,'2018-proposal-image-1.jpg',None)]:
 ev[idx].update(visual_observed=True,archived_artifact='artifacts/ihuman-controls-deepening/'+name,sha256=sha(a/name),capture_state='archived_static_images_with_observation_log',visual_audit_path='artifacts/ihuman-controls-deepening/static-observation-log.md',visual_audit_sha256=sha(a/'static-observation-log.md'))
 if extra:ev[idx]['additional_artifacts']=[{'path':'artifacts/ihuman-controls-deepening/'+extra,'sha256':sha(a/extra)}]
ev[3]['access_limitation']='All3 article screenshot URLs returned403; stopped; no images observed or archived.'
(r/'batches/ihuman-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in ev))
with (r/'batches/children-coverage.csv').open() as f:
 reader=csv.DictReader(f);fields=reader.fieldnames;old={x['flow_id']:x for x in reader if x['product_id']=='M-029'}
with (r/'batches/children-loop-deepening-coverage.csv').open() as f:
 old.update({x['flow_id']:x for x in csv.DictReader(f) if x['product_id']=='M-029'})
new=dict(zip(fields,['F-M029-05','M-029','历史3.7.2家长验证与退出登录入口','D1','E-IHCD-04','正文描述设置→数字验证→账户中心→退出登录；iPhone12/iOS15.6/3.7.2','截图403；确认/取消与退出后状态；当前构建、草稿/进度保留和重登录恢复','not_observed','false','cases/M-029-ihuman.md','2026-10-26']))
with (r/'batches/ihuman-controls-deepening-coverage.csv').open('w',newline='') as f:
 out=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');out.writeheader();out.writerow(new)
changes={
 'F-M029-01':dict(evidence_ids='E-IHUM-01;E-IHUM-02;E-IHD-02;E-IHCD-01;E-IHCD-03',known_branches='当前五步声明；2018版练界面仅静态选项/导航；作者示范建议不当实现',gaps='现行选课、错答提示/重试、奖励/复习结算、故事解锁及退出恢复',visual_status='static_reference_only'),
 'F-M029-02':dict(evidence_ids='E-IHUM-02;E-IHCD-01;E-IHCD-02',known_branches='2020官方回复关闭游戏与禁止跳过；3.0.1静态设置容器；重置进度回复仅表示考虑',gaps='现行入口/验证失败、教学开关保存与课内生效、按孩子分别配置',visual_status='static_official_images_only'),
 'F-M029-03':dict(evidence_ids='E-IHUM-03;E-IHUM-04;E-IHCD-02',known_branches='登录与相册权限；官方设置图显示3.0.1及隐私中心进入按钮',gaps='平台与图日期、拒绝权限、图片导出失败及结构化进度恢复',visual_status='static_official_images_only'),
 'F-M029-04':dict(evidence_ids='E-IHD-01;E-IHCD-01',known_branches='年级/书册/课文范围；iOS4.4.0/2026-02-05宣布部编版地图；先单字后整课测验',gaps='当前4.5.2具体入口、错答/提示/补练、结算、退出恢复')}
proposed=[];deltas=[]
for fid,d in changes.items():
 row=old[fid].copy();row.update(d);proposed.append(row)
 deltas.append({'flow_id':fid,'field_updates':{k:{'before':old[fid][k],'after':v} for k,v in d.items()},'status':'proposal_only_old_batches_and_master_unchanged'})
with (w/'existing-coverage-updates.csv').open('w',newline='') as f:
 out=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');out.writeheader();out.writerows(proposed)
(w/'existing-coverage-field-changes.json').write_text(json.dumps(deltas,ensure_ascii=False,indent=2)+'\n')
print('wrote M029 case; 5E; 1newF D1; 4old-row proposals; 3static images+audit; no old batches/master writes')
