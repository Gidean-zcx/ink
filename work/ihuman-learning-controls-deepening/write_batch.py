from pathlib import Path
import csv,json,hashlib,copy
R=Path('docs/research/interaction-library');W=Path('work/ihuman-learning-controls-deepening');A=R/'artifacts/ihuman-learning-controls-deepening';B=R/'batches'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,x):Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvwrite(p,rows,fields):
 with Path(p).open('w',newline='') as f:
  q=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');q.writeheader();q.writerows(rows)
oldf=json.loads((W/'coverage-before.json').read_text());olde=json.loads((W/'evidence-before.json').read_text())
# These are transparent reading records of the already-returned web-tool text, not saved origin HTML.
webrecords=[{'evidence_id':'E-IHLC-01','url':'https://www.bilibili.com/opus/425918819985666271','web_response_reference':'turn972view0','response_kind':'web_open_extracted_article_text','title':'快来识字吧！识字APP测评--第一期','publisher_account':'懂课鸭','article_byline':'小张解课','published_as_shown':'2020-08-21 18:22','location':'web response lines142–221, 洪恩识字/课程体验/其他','short_verbatim_excerpt':'根据语音提示找出“二”字的石头，三次选择正确后','body_read':True,'original_html_archived':False,'direct_get_status':412,'images_viewed':False,'video_viewed':False,'capture_limitation':'网页工具已返回正文；随后普通GET412停止，未取图，不用其他域名/参数重试。此JSON仅记录工具结果的定位和短摘，不是完整原网页。'},
{'evidence_id':'E-IHLC-02','url':'https://post.smzdm.com/p/aoo8one6/','web_response_reference':'turn972view1','response_kind':'web_open_extracted_article_text','title':'孩子识字敏感期，懒家长如何助力？','publisher_account':'晞爸买买买','published_as_shown':'2020-04-25 15:08:34','location':'web response lines98–176, 软件概况/玩认练写/关于软件设置','short_verbatim_excerpt':'点击后分别展示了古代“雨”字的写法，以及笔画顺序、总笔画数','body_read':True,'original_html_archived':False,'direct_get_status':200,'direct_get_body':'TencentCaptcha/WafCaptcha challenge, not article','images_viewed':False,'video_viewed':False,'capture_limitation':'网页工具已返回正文；直接GET只有1697字节验证壳后停止，没有执行验证或换身份。此JSON仅记录工具结果的定位和短摘，不是完整原网页。'}]
for x in webrecords:dump(W/'raw'/(x['evidence_id']+'-web-reading-record.json'),x)
exploration=[
{'candidate':1,'url':'https://www.palmviewpri.moe.edu.sg/files/For%20Parents%20%282023%29/A%20sharing%20on%20the%20learning%20of%20Chinese%20characters%20Combined.pdf','result':'web open 404; stopped','adopted':False},
{'candidate':2,'url':'https://post.smzdm.com/p/arz6epzx/','result':'web open 400 Timeout fetching; stopped','adopted':False},
{'candidate':3,'url':webrecords[0]['url'],'result':'web extracted full article readable; direct GET412; stopped route','adopted':'historical author text only E-IHLC-01'},
{'candidate':4,'url':webrecords[1]['url'],'result':'web extracted full article readable; direct GET200 captcha shell; stopped route','adopted':'historical author text only E-IHLC-02'},
{'candidate':5,'url':'https://h5static1.ihumand.com/website/pc/cn/shizi/assets/media/cn_video.198bceb2.mp4','source_page':'https://www.ihuman.com/shizi/','result':'Playwright browser executable missing before network request; exit1; no retry/download/frames/audio','adopted':False}]
dump(W/'exploration-log.json',{'checked_on':'2026-10-27','candidate_limit':6,'candidates_checked':5,'search_queries':['洪恩识字 使用体验 玩 认 练 读 答错 复习','洪恩识字 教程 学习设置 复习 错字 官方','洪恩识字 产品体验 错误 游戏 2019 2020 玩 认 练 写','site:ihuman.com 识字 家长指南 教程 使用'],'candidates':exploration,'excluded_index_only':'转载2018旧文、下载站、未披露构建/原图的营销比较未升级为新证据；旧403图片未重试'})
notes='''# 洪恩识字：历史作者正文与公开资料目录

查阅2026-10-27。0新流程；未运行App、登录或操作设备；没有新图片/视频观察。两篇新材料是历史作者报道，发布日期不是App构建；平台、客户端版本及设置均未完全披露。当前4.5.2五环节仍按既有开发者说明，未用2020四环节覆盖。

## E-IHLC-01：单字“二”的作者报道

[懂课鸭，署名小张解课，2020-08-21](https://www.bilibili.com/opus/425918819985666271)，定位“洪恩识字→课程体验”，网页工具已返回正文。旧例的练习按语音挑石头，作者报告三次正确后乌鸦喝到水。它提供正常分支的文字依据，未说明错选和重试。其五字一测、星星兑换等只是当年自述，不能作为现行默认。

## E-IHLC-02：单字“雨”及家长配置的作者报道

[晞爸买买买，2020-04-25](https://post.smzdm.com/p/aoo8one6/)，定位“软件概况/玩认练写/关于软件设置”。正文描述练字四选一、写页的古字与问号辅助，并区分作者设置的每日五字/强制任务。没有据此确认当前判分、默认值或开关保存结果。作者建议/效果评价不采用为产品事实。

## 获取差异与原件边界

正文来自web工具open的可读提取结果，不是本地已取到的原站HTML。两个 `raw/E-IHLC-*-web-reading-record.json` 只记录该结果的URL、作者、日期、行段与短摘，**不是完整正文快照**；SHA只证明这些记录文件自身。直接GET：B站412；什么值得买200但仅验证码脚本壳。失败后停止，未请求配图、未执行验证码、未换域/身份/参数。验证码壳保存于raw/smzdm-2020.html，不用它支持正文事实。

另有学校教学PDF404、近期设置文章超时，均排除。官网已知介绍视频在浏览器启动阶段因缺失可执行文件失败，尚未发网络请求，没有视频元数据、画面或声音；不能判断视频是完整教程还是宣传。共核5候选，没有扩散检索。

## 目录如何使用

对应inventory是可获得材料支持的功能域清单，区分当前开发者声明、旧官方静态入口、历史操作文字和未核反馈；不是当前App全部菜单或完整官方帮助目录。历史作者报道不新增一套流程编号，补回F01/F02；深度仍D1/D2原状态。复读旧App Store HTML仅支持功能域与2020官方回复，未重新采集商店版本。

旧E-IHCD-02/03的视觉审计指向独立static-observation-log.md，未引用案例散列，本轮未修改其图或审计。
'''
(A/'source-notes.md').write_text(notes)
new_e=[]
for x in webrecords:
 raw=W/'raw'/(x['evidence_id']+'-web-reading-record.json')
 new_e.append({'evidence_id':x['evidence_id'],'product_id':'M-029','title':x['title']+'（历史作者正文）','url':x['url'],'checked_on':'2026-10-27','source_type':'third_party_firsthand_report_text_only','access_mode':'web_tool_extracted_article_text_direct_http_refused_or_challenged','version_scope':'2020 historical author example; platform/app build unknown; not current4.5.2','app_build':None,'firmware_version':None,'source_published_on':x['published_as_shown'][:10],'source_updated_on':None,'locator':x['location'],'note':'仅历史正文，未看配图或视频；本文正常分支/用户配置不证明现行默认、错误反馈或恢复','hands_on':False,'visual_observed':False,'capture_state':'short_web_response_reading_record_original_article_html_unavailable','archived_artifact':str((A/'source-notes.md').relative_to(R)),'sha256':sha(A/'source-notes.md'),'supersedes':None,'raw_work_path':str(raw),'raw_sha256':sha(raw),'raw_kind':'structured_reading_record_of_web_tool_response_not_origin_html','access_limitation':x['capture_limitation']})
(B/'ihuman-learning-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in new_e))
csvwrite(B/'ihuman-learning-controls-deepening-coverage.csv',[],list(oldf[0]['record']))
# bounded functionality inventory; no empty invented menus.
specs=[
('主线地图','当前三阶段内容','E-IHCD-01','开发者说明/分阶与生活应用','F-M029-01','developer_claim_only','解锁、手动选字与返回'),
('教材选择','年级/书册/课文','E-IHD-01;E-IHCD-01','开发者说明/部编版','F-M029-04','developer_claim_only','控件、换教材进度'),
('玩','互动引入','E-IHCD-01','五步学习','F-M029-01','developer_claim_only','同字启动/退出/重试'),
('认','字形字义','E-IHCD-01','五步学习','F-M029-01','developer_claim_only','逐字页面/求助返回'),
('练','辨形/听音找字/看字找音','E-IHCD-01','五步学习','F-M029-01','developer_claim_only','错选提示/再答次数'),
('写','运笔与笔画纠错','E-IHCD-01','五步学习/AI辅助','F-M029-01','developer_claim_only','容错/示范/无法完成出口'),
('说','字到词句表达','E-IHCD-01','五步学习','F-M029-01','developer_claim_only','录音提交/拒绝权限/评分'),
('每日回顾','已学字再次练习','E-IHCD-01','五步学习','F-M029-01','developer_claim_only','触发、必做/跳过与恢复'),
('阶段检测','阶段题组','E-IHCD-01','五步学习','F-M029-01','developer_claim_only','数量、判分、错误回流'),
('趣味挑战','已学字练习','E-IHCD-01','五步学习','F-M029-01','developer_claim_only','入口/结束与奖励'),
('整课测验','单字后核查整课','E-IHD-01;E-IHCD-01','部编版','F-M029-04','developer_claim_only','测验选项/结算/补练'),
('子集绘本','目标字与开始阅读','E-IHD-02','官网原创子集绘本图','F-M029-01','prior_static_entry_only','逐页/音频/解锁/断点'),
('生活中的汉字','生活主题','E-IHCD-01','分阶与生活应用','','developer_claim_only','主题选择/内容边界'),
('偏旁部首','专项内容','E-IHCD-01','分阶与生活应用','','developer_claim_only','入口/练习/完成'),
('笔画知识','专项内容','E-IHCD-01','分阶与生活应用','','developer_claim_only','查询/练习衔接'),
('趣味成语','专项内容','E-IHCD-01','分阶与生活应用','','developer_claim_only','入口/题型/退出'),
('拍照识字','历史版本能力声明','E-IHUM-02','2025-06-25日志','','developer_claim_only','相机权限/识别错字/回源'),
('AI学伴','伴学声明','E-IHUM-01;E-IHCD-01','官网/AI辅助','','developer_claim_only','输入/反馈/退出与数据保存'),
('使用时长','家长节奏','E-IHCD-01','家长管理','F-M029-02','developer_claim_only','到时反馈/延长/离线'),
('每日识字目标','家长节奏','E-IHCD-01','家长管理','F-M029-02','developer_claim_only','可选范围/变更生效'),
('识字速度','家长节奏','E-IHCD-01','家长管理','F-M029-02','developer_claim_only','作用范围/保存'),
('教学环节','关闭游戏历史路径','E-IHCD-01','2020-05-04官方回复','F-M029-02','historical_instruction','现行入口/正在学的字是否改变'),
('环节跳过','是否允许跳过','E-IHCD-01','2020-05-04官方回复/家长管理','F-M029-02','historical_instruction','关闭跳过与退出课程的区别'),
('识字报告','学习情况','E-IHCD-01;E-IHCD-02','家长管理/3.0.1设置图','F-M029-03','claim_and_prior_static_entry','报告计数与真实掌握/导出范围'),
('字库打印','纸面练习','E-IHCD-01','家长管理','','developer_claim_only','选字/格式/打印确认'),
('离线缓存','3.0.1页签','E-IHCD-02','旧设置图','','prior_static_entry_only','缓存对象/下载/过期/离线进度'),
('音乐与基地开关','3.0.1可见控件','E-IHCD-02','旧设置图','','prior_static_entry_only','作用范围/生效与恢复'),
('检查更新','3.0.1入口','E-IHCD-02','旧设置图','','prior_static_entry_only','升级/取消/回退未知未执行'),
('开始诊断','3.0.1入口','E-IHCD-02','旧设置图','','prior_static_entry_only','结果/异常/修复动作'),
('账户中心/退出登录','3.7.2历史路径','E-IHCD-04','2022教程','F-M029-05','historical_text_images_refused','确认/退出后保留/重登恢复'),
('个人资料副本','隐私中心下载','E-IHUM-03;E-IHUM-04;E-IHCD-02','隐私中心/旧设置图','F-M029-03','official_instruction','图片副本不是进度恢复'),
('反馈','首页右上我要反馈','E-IHD-03','官方反馈指引','','official_instruction_entry_only','提交/回执/当前入口未执行'),
('注销账号','内容删除或匿名化政策','E-IHUM-04','政策4.4–4.5','','policy_scope_only','不等同删除单孩子或重置进度'),
('历史二字练习','听音选字三次正确','E-IHLC-01','2020课程体验/二','F-M029-01','historical_author_text_only','错误/退出未描述；非当前通用阈值'),
('历史雨字辅助','古字/笔顺入口','E-IHLC-02','2020写环节/雨','F-M029-01','historical_author_text_only','入口位置和效果未视觉核验'),
('历史强制任务','作者启用的配置','E-IHLC-02','2020复习与软件设置','F-M029-02','historical_author_text_only','开关路径/保存/失败；不作默认')]
inv=[]
for i,(domain,known,es,loc,fs,status,gap) in enumerate(specs,1):
 scope=('2020作者报道；平台/构建未知' if 'E-IHLC-' in es else '2022作者自报iPhone12/iOS15.6/3.7.2' if 'E-IHCD-04' in es else '2020-05-04官方回复；当时构建/平台未知' if status=='historical_instruction' else '3.0.1官方静态图；平台/日期未知' if es=='E-IHCD-02' else 'CN iOS4.5.2开发者说明（2026-10-26旧快照）；与3.0.1静态图分别使用' if 'E-IHCD-01' in es else '官方页面日期/平台/构建未定；按来源限定；非当前实测')
 inv.append({'item_id':f'IHLC-I{i:02d}','product_id':'M-029','functional_domain':domain,'public_material_scope':known,'evidence_ids':es,'source_locator':loc,'flow_ids':fs,'review_status':status,'version_scope':scope,'remaining_gap':gap,'checked_on':'2026-10-27','new_visual_observation':'false'})
csvwrite(B/'ihuman-learning-controls-deepening-inventory.csv',inv,list(inv[0]))
revisions={
'F-M029-01':{'evidence_ids':'E-IHUM-01;E-IHUM-02;E-IHD-02;E-IHCD-01;E-IHCD-03;E-IHLC-01;E-IHLC-02','known_branches':'当前五步声明；2018练界面仅静态；2020作者各述二字听音选择正常分支、雨字练/写辅助；不当现行阈值或连续观察','gaps':'当前选课/错答提示/重试/奖励/复习结算/故事解锁/退出恢复；2020材料平台构建未知且图片未核','checked_on':'2026-10-27'},
'F-M029-02':{'evidence_ids':'E-IHUM-02;E-IHCD-01;E-IHCD-02;E-IHLC-02','known_branches':'2020官方回复关闭游戏与禁止跳过；3.0.1设置容器；重置进度回复仅表示考虑；2020作者每日五字/强制任务是自身配置非默认','gaps':'现行入口/验证失败、教学开关保存与课内生效、按孩子分别配置；作者强制任务的具体开关和错误反馈未核','checked_on':'2026-10-27'}}
updates=[]
for x in oldf:
 r=x['record'];fid=r['flow_id']
 if fid in revisions:
  after={**r,**revisions[fid]};updates.append({'flow_id':fid,'source_batch':x['source_batch'],'changes':{k:{'before':r[k],'after':v} for k,v in revisions[fid].items() if r[k]!=v},'replacement_row':after})
dump(W/'existing-coverage-updates.json',updates)
x=next(x for x in olde if x['record']['evidence_id']=='E-IHCD-01');r=x['record'];raw=Path(r['raw_work_path'])
cap={'checked_on':'2026-10-27','capture_method':'local_reread_of_2026-10-26_public_html_no_network','url':r['url'],'raw_work_path':str(raw),'raw_sha256':sha(raw),'locator':'五步学习/子集绘本/分阶与生活应用/家长管理/AI辅助及2020-05-04回复；只用于新目录，不宣称新版本快照','archived_artifact':str((A/'source-notes.md').relative_to(R)),'sha256':sha(A/'source-notes.md'),'hands_on':False,'visual_observed':False}
before=r.get('additional_captures',[]);dump(W/'existing-evidence-additional-captures.json',[{'evidence_id':r['evidence_id'],'source_batch':x['source_batch'],'field':'additional_captures','before_exists':'additional_captures' in r,'before':before,'after':before+[cap],'append':cap}])
s=(W/'case-before.md').read_text()
s=s.replace('查阅日：2026-10-26。','查阅日：2026-10-26；2026-10-27补历史作者正文和功能目录。',1)
s=s.replace('完整功能清单未核定。','完整功能清单未核定；[公开材料目录](/home/workspace/reader/docs/research/interaction-library/batches/ihuman-learning-controls-deepening-inventory.csv)逐项区分36个已知功能域/历史条目与反馈缺口，不是当前App菜单全量。',1)
s+='''\n\n## 2026-10-27：同字历史材料与可得目录\n\nF01仍D1，F02沿用原历史D2；没有新增任务。两篇可读作者正文补到具体旧例，均未披露可核客户端构建/平台，也未视觉核查其配图，不能与当前4.5.2混合。\n\n- E-IHLC-01：懂课鸭、署名小张解课（2020-08-21）按“二”写玩、认、练、写；练习为按语音选石头，作者称三次正确后乌鸦喝到水。这个正常分支未给错选、重试或退出记录，不当所有字的阈值。\n- E-IHLC-02：晞爸买买买（2020-04-25）以“雨”述练习四选一，以及写页古字/问号辅助；每日五字与强制任务是作者采用的设置，非已核默认。没有据此补出设置生效、错答或恢复。\n\n[独立摘记](/home/workspace/reader/docs/research/interaction-library/artifacts/ihuman-learning-controls-deepening/source-notes.md)列作者、日期和正文定位。web工具返回可读正文，但本地普通GET分别为412和验证码壳；两路停止，未查看图片，也未保存完整原站正文。新目录只收有出处的功能，区分开发者声明、旧官方静态入口、历史操作文字；36行不表示36条已还原流程。\n\n另核一份学校教学PDF返回404、近期设置文章超时，均未采用。官网原已发现的介绍视频在浏览器启动阶段因可执行文件缺失失败，未发网络请求、未取截图/声音、未保存媒体；尚不能判它是完整教程还是宣传。旧403图片没有重试。\n\n有价值的下一观察已具体到同一个字：在辨字候选中故意错选，再比较提示后答对与独立答对是否产生不同状态；退出同字与改教学环节分别记录。历史正常分支不能替代这个错误与恢复链，奖励和地图进度也不等于掌握。\n\n- E-IHLC-01：[2020“二”字课程体验](https://www.bilibili.com/opus/425918819985666271)，洪恩识字/课程体验；2026-10-27读取web正文提取结果，直接原站412停止。\n- E-IHLC-02：[2020“雨”字作者体验](https://post.smzdm.com/p/aoo8one6/)，软件概况及关于软件设置；2026-10-27读取web正文提取结果，直接GET为验证码壳后停止。\n'''
(R/'cases/M-029-ihuman.md').write_text(s)
dump(W/'build-results.json',{'new_E':2,'new_F':0,'old_F_rows':2,'old_F_fields':sum(len(x['changes']) for x in updates),'old_E_additional_capture_rows':1,'inventory_rows':len(inv),'new_visual_observations':0,'candidates_checked':5,'case_depths_unchanged':True,'product_or_account_state_changed':False})
print((W/'build-results.json').read_text())
