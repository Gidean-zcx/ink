from pathlib import Path
import csv,json,hashlib
base=Path('docs/research/interaction-library')
e=[]
def add(eid,p,title,url,loc,note,scope,kind='official_help',access='opened_text',build=None,fw=None,updated=None,visual=False,artifact=None,supersedes=None):
 d={'evidence_id':eid,'product_id':p,'title':title,'url':url,'checked_on':'2026-10-26','source_type':kind,'access_mode':access,'version_scope':scope,'app_build':build,'firmware_version':fw,'source_updated_on':updated,'locator':loc,'note':note,'hands_on':False,'visual_observed':visual,'archived_artifact':artifact,'sha256':hashlib.sha256((base/artifact).read_bytes()).hexdigest() if artifact else None,'capture_state':'official_static_asset_archived' if artifact else 'bibliography_and_short_note_only','supersedes':supersedes};e.append(d)
add('E-BCZ-01','M-016','功能介绍','https://passport.baicizhan.com/privacy/introductions.html','记忆/词库/数据/社群','功能目录，非操作手册。','百词斩 App；构建未固定','official_product')
add('E-BCZ-02','M-016','百词斩官网','https://www.baicizhan.com/','科学记忆/专业拓展/页脚','模式与内容；页脚7.8.10不可代替实测构建。','App；平台未分别核对','official_product','official_index_text')
add('E-BCZ-03','M-016','百词斩 iOS 开发者描述与版本历史','https://apps.apple.com/cn/app/百词斩-背单词-学英语日语韩语法语/id557545298?platform=watch','产品描述；7.10.25、7.10.12','题型及版本变化；未采用用户评论。','中国区 iPhone/iPad；描述可能跨构建','official_app_store','opened_text','7.10.25（商店显示，未实测）')
add('E-BCZ-04','M-016','历史 info 帮助','https://www.baicizhan.com/information','离线、边走边听','翻转反馈；页面写3.8.3，不套用现版。','历史 App 3.8.3','official_help','opened_text','3.8.3（页面上下文）')
add('E-BCZ-05','M-016','2026 单词大赛常见问题','https://www.baicizhan.com/little_class/article/1033/share_view','审核相关、比赛相关','实时匹配、累计积分、晚入场及复审；索引发布日期异常不采用。','2026 大赛；非日常学词','official_help','official_index_text')
add('E-BCZ-06','M-016','百词斩隐私政策','https://passport.baicizhan.com/privacy/privacy.html','查阅/复制个人信息、帮助入口','只作为数据入口证据，不据此保证可恢复备份。','百词斩 App/网站','official_policy','official_index_text')
add('E-BCZ-07','M-016','旧下载页','https://www.baicizhan.com/download','0流量离线使用','历史宣传与3.8.3离线说明范围不同；版权2011–2018。','旧下载页，构建未知','official_product','official_index_text')
mp='https://memodocs.maimemo.com/docs/'
for num,title,slug,loc,note,access in [
(1,'认识/模糊/忘记/熟知','Ok6YwF2X7iJZvvkCunjcmWwLnzb','四种反馈','最长间隔无固定，与旧help30有差异。','http_html_text'),
(2,'每日学习量','PulqwOB1xi5jV0kb8Y1cPE7ZnKc','调整学习量/添加新学','默认复习在前；额外新词立即安排。','opened_text'),
(3,'更换词书','Y0iDwaZYeixhnzkuqJRcOEt6ncc','换书及单词/词书关系','同词跨书共享记忆；熟知、删除、不想背分开。','http_html_text'),
(4,'云词本','QKUlwb6qTiLtzvk49BGc1ZVFnqg','章节/默认/原型；5.5.30前后','结构化解析与入口版本变化。','opened_text'),
(5,'设备间学习数据同步','TJDKwbRRfi1prCktSXPcDQjrnJf','自动/手动同步','最新数据端启用自动；换设备上传后下载。','opened_text'),
(6,'离线学习','Nw46wDLqRiLbpikFsxNcONXCnfc','全文','仅个别功能离线；同步和获取新词需联网。','opened_text'),
(7,'重置学习数据','KE8ZwhQ8UikDpKkeEggcKYgcnsd','批量移出/只背此书/单词删除','删除清历史不可撤销；额度变化另说明。','http_html_text'),
(8,'学习好友','YGRKwwq9Li4VNAk8OeFcng4onie','入口/邀请/数据/删除/奖励','5.5.70+；过期、同步、历史范围和删除；无额外奖励。','opened_text'),
(9,'开放 API 功能介绍','PNvOw2E1AivPtlkRlQgce1W1n2c','核心功能/三种接入方式','词汇与词本、部分学习数据；没有实际API调用。','opened_text'),
(10,'快速上手目录','memo-get-started','功能目录','仅确认子主题存在，未伪称读过全部子功能。','opened_text'),
(11,'iOS版本日志','product-log-memo-ios','5.4.40、5.3.86、5.3.80/96','日志顶部非当前版本保证；注意多设备提示及复习量变化。','opened_text')]:
 add(f'E-MMO-{num:02d}','M-017',title,mp+slug+('/' if num!=11 else ''),loc,note,'墨墨背单词 App；具体功能版本见正文','official_release_notes' if num==11 else ('official_product' if num==9 else 'official_help'),access,supersedes='E-MMO-12' if num==1 else None)
add('E-MMO-12','M-017','旧墨墨help30','https://www.maimemo.com/help30','反馈与学习量','旧最长一年规则，由新FAQ局部修正；不整体否定旧页。','历史构建未固定')
add('E-BBDC-01','M-018','不背单词官网','https://www.bbdc.cn/index','产品介绍','根URL为跳转脚本；读取/index正文，排除用户评价。','Android/iOS；构建未固定','official_product','http_html_text')
add('E-BBDC-02','M-018','不背单词 iOS描述及版本历史','https://apps.apple.com/au/app/不背单词-学习英语单词的必备软件/id698570469','描述；5.9.4/8/13/17/19；5.11至5.11.6','返回上一词不等同撤销；纠错功能修复记录不等于实测通过。','澳大利亚区 iOS；不套用中国Android','official_app_store','opened_text','5.11.6（商店显示，未实测）')
add('E-BBDC-03','M-018','不背单词隐私协议','https://learnywhere.cn/lexis/privacy','账号体系/学习成就/学习服务/帮助','跨产品只有特定数据同步；记录操作入口，不作法律评估。','不背单词与轻听英语','official_policy','official_index_text',updated='2026-08-28')
add('E-BBDC-04','M-018','自制词书未登录入口','https://www.bbdc.cn/lexis_book_index','重定向登录页','本轮止于登录，不推断登录后导入流程。','网页版；未登录','official_web_ui','opened_text')
add('E-SBY-01','M-019','扇贝四App介绍','https://www.shanbay.com/m/intro/','单词/听力/口语/阅读','旧四App分工；与现听力口语命名区分。','Android/iOS；介绍未定版本','official_product')
sbys=[(2,'单词自评页面','bcrmcn','bbsool','再认与逐步提示'),(3,'单词学习','bcrmcn','mqhnn','自评/提示/详情/任务完成'),(4,'单词详情页','bcrmcn','yjgia','笔记创建收藏和再次显示'),(5,'单词学习量','bcrmcn','brnsyu','修改生效与再来一组'),(6,'阅读页面','btfesx','frhdp','查词/听书/译文/完成阅读'),(7,'单词进度','bcrmcn','evpbg','熟悉度与简单词统计'),(8,'听力App常见问题','bbumgi','bxejg','资源目录/打卡/精品课/字幕'),(9,'文章训练','bbumgi','brszea','反复听与填空'),(10,'听力积分及徽章','bbumgi','iwlnc','提示/比例/积分'),(11,'如何获得听力三星','bbumgi','wldgi','星级口径'),(12,'忘记打卡能补么','svgln','bqahtq','两类遗漏/补签/失败计划'),(13,'打卡分享奖励计划','svgln','kyxac','去重/返回/明细'),(14,'阅读进度与统计','btfesx','bscpci','词数/章节数定义'),(15,'句子训练','bbumgi','lfmwu','短任务与徽章')]
for num,title,col,aid,loc in sbys:
 raw=json.loads(Path('work/vocab-research/sby-'+aid+'.json').read_text())
 note=f'从官方帮助公开JS定位后GET https://apiv3.shanbay.com/cms/articles/{aid} 取得content_html；assign_date={raw.get("assign_date")}不是更新时间。'
 add(f'E-SBY-{num:02d}','M-019',title,f'https://web.shanbay.com/web/help/{col}/{aid}',loc,note,'公开历史帮助；构建与最后更新未固定','official_help','http_json_text')
add('E-SBY-16','M-019','听力口语产品页','https://web.shanbay.com/grammy/landing/listen-landing','泛听/精听/口语/计划','当前产品名称与功能目录；不与旧四App强行合并。','听力口语 App；构建未固定','official_product','official_index_text')
add('E-BCZH-01','M-022','百词斩单词机官网','https://danciji.baicizhan.com/','Mini/Max/系列功能','型号分组与离线等宣传；非操作手册。','Mini、3.52英寸Max；SKU/固件未知','official_product')
for eid,title,name,url,note in [
('E-BCZH-02','Mini区官方产品图','bcz-mini','DCFDFF.7e941f00.png','静态锁定页长按提示；图印BCZ-Ace-1S，与Mini命名对应未知。'),
('E-BCZH-03','Max区官方产品图','bcz-max','94C6C5.cec41304.png','静态首页六入口与1/2页；没有观察选中后状态。'),
('E-BCZH-04','系列图背示意图','bcz-image-study','p1.9f1bf15f.png','静态词卡、计数、考频与图形助记；型号未定。')]:
 add(eid,'M-022',title,'https://cap.bczcdn.com/hardware-homepage/out/_next/static/media/'+url,'完整单张图片',note,'官方产品图；构建未知','official_product_image','downloaded_image_viewed',visual=True,artifact='artifacts/vocab/'+name+'.png')
add('E-MMOH-01','M-023','墨墨单词机帮助','https://memodocs.maimemo.com/docs/support_wordmachine/','Q1/2/3/6/8/14/15/17/20；核心/更多功能；按键','正文成功；公测与未来能力描述并存；回上一词不是已证实撤销。','MM-WM01；功能门槛App5.5.96+、固件1.0.36+，未实测','official_help','opened_text')
(base/'batches/vocab-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in e))
rows=[]
def flow(fid,p,task,depth,ids,branches,gaps,case,visual='not_observed'):
 rows.append({'flow_id':fid,'product_id':p,'task':task,'depth':depth,'evidence_ids':ids,'known_branches':branches,'gaps':gaps,'visual_status':visual,'hands_on':'false','case_path':'cases/'+case,'checked_on':'2026-10-26'})
cs={'M-016':'M-016-baicizhan.md','M-017':'M-017-maimemo.md','M-018':'M-018-bbdc.md','M-019':'M-019-shanbay.md','M-022':'M-022-baicizhan-device.md','M-023':'M-023-maimemo-device.md'}
data={
'M-016':[
('日常学词与复习','D1','E-BCZ-01;E-BCZ-02;E-BCZ-03','题型/模式','现版主任务步骤、误评、离线'),
('翻转听词（3.8.3历史）','D2','E-BCZ-04','翻转记不熟后继续','现版存在性、误触、断点'),
('2026单词大赛','D2','E-BCZ-05','审核复审、晚入场','断线、题型、重试、分数公式'),
('查询学习相关数据','D2','E-BCZ-06','页面未显示可申请','导出格式、恢复、API')],
'M-017':[
('选词到当日学习','D2','E-MMO-01;E-MMO-02;E-MMO-03','认识/模糊/忘记、跨书共享','释义显隐、误评撤销、完成页'),
('变更学习负荷','D2','E-MMO-02;E-MMO-03','追加新词即时学','当前小组如何重算'),
('建立云词本','D2','E-MMO-04','章节、词形、版本入口','未命中、重复、保存失败'),
('换设备与重装','D2','E-MMO-05;E-MMO-06','上传/下载、自动同步、离线','冲突、回滚、完成提示'),
('熟知与移出记忆','D2','E-MMO-03;E-MMO-07','熟知/不想背/删除不可撤销','暂停与误评恢复'),
('学习好友','D2','E-MMO-08','过期、权限、延迟、删除','现版视觉与数据延迟'),
('外部资料入库','D1','E-MMO-09','个人凭据/第三方授权','接口实测、失败、限流、幂等')],
'M-018':[
('语境学词后拼写','D1','E-BBDC-01;E-BBDC-02','学习/复习拼写分别设定','完整题目操作、原声位置'),
('返回与错词拼写','D1','E-BBDC-02','返回上一词、只拼错词','是否撤销、空队列、错题提示'),
('词书与学习范围','D1','E-BBDC-02;E-BBDC-03;E-BBDC-04','按单元、登录入口','导入、删除、数据范围'),
('成果及跨产品数据','D2','E-BBDC-03','部分共享数据范围','冲突、离线、重装'),
('寻求帮助','D2','E-BBDC-03','模块反馈','受理状态、工单恢复')],
'M-019':[
('单词回忆与提示','D2','E-SBY-02;E-SBY-03','认识/提示/不认识','帮助文案差异、现版、撤销'),
('学词记录笔记','D2','E-SBY-04','创建/收藏后再次展示','编辑、删除、冲突'),
('调整任务与继续','D2','E-SBY-05','即时或次日、再来一组','队列重算、追加上限'),
('阅读遇词进入复习','D2','E-SBY-06','查词/加入/完成分开','重复、误点、离线'),
('阅读切换听书','D2','E-SBY-06','句子播放、付费译文','断点、中断、同步'),
('听力文章练习','D2','E-SBY-08;E-SBY-09;E-SBY-10;E-SBY-11','提示后积分/星级不同','现版判题、提示阶梯、错误反馈'),
('泛听与素材','D2','E-SBY-08;E-SBY-16','无字幕、打卡、资源目录','当前入口、搜索、离线'),
('打卡遗漏与奖励核对','D2','E-SBY-12;E-SBY-13','补签/时光机、分享去重','现行门槛与额度'),
('跟读与复述','D1','E-SBY-01;E-SBY-16','独立口语/合并产品','录音、评分、重录、失败')],
'M-022':[
('Mini锁定入口','D1','E-BCZH-02','长按提示','型号映射、解锁后状态'),
('Max选择学习任务','D1','E-BCZH-03','六入口、分页','按键映射、完整任务与第二页'),
('离线与再联网','D1','E-BCZH-01','离线宣传','绑定、缓存、学习同步、恢复')],
'M-023':[
('绑定与换书','D2','E-MMOH-01','单书、离线准备','绑定失败、同步完成提示'),
('自评与负荷','D2','E-MMOH-01','模式、按键、返回','是否撤销、完成页、空任务'),
('标记与恢复','D2','E-MMOH-01','熟知/收藏/不想背、取消','暂停优先级、误长按'),
('回顾与随身听','D2','E-MMOH-01','回顾不改调度、播放/暂停','中断断点、缺失媒体'),
('同步与解绑','D2','E-MMOH-01','版本门槛、未同步解绑丢数据','冲突、重试、恢复')]
}
for p,items in data.items():
 for i,(task,dep,ids,br,gap) in enumerate(items,1):flow(f'F-{p.replace("-","")}-{i:02d}',p,task,dep,ids,br,gap,cs[p],'static_official_images_only' if p=='M-022' and i<3 else 'not_observed')
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
with (base/'batches/vocab-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
print('evidence',len(e),'flows',len(rows),'D1',sum(r['depth']=='D1' for r in rows),'D2',sum(r['depth']=='D2' for r in rows))
