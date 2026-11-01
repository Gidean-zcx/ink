import json,csv,pathlib,hashlib
root=pathlib.Path('/home/workspace/reader')
base=root/'docs/research/interaction-library/batches'
E=[]
def add(e,p,t,u,loc,scope,kind='official_help',mode='opened_text',date=None,artifact=None,pages=(),build=None,note=''):
 r=dict(evidence_id=e,product_id=p,title=t,url=u,checked_on='2026-10-26',source_type=kind,access_mode=mode,version_scope=scope,app_build=build,firmware_version=None,source_updated_on=date,locator=loc,note=note,hands_on=False,visual_observed=bool(pages),archived_artifact=artifact,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
 if artifact:
  r['sha256']=hashlib.sha256((root/artifact).read_bytes()).hexdigest();r['capture_state']='local_source_document'
 if pages:
  stem=pathlib.Path(artifact).stem
  r.update(visual_scope='Selected static document pages only; no continuous interaction',observed_on='2026-10-26',rendered_artifacts=[f'work/news-interactions/{stem}-p{n}.png' for n in pages],observed_pdf_pages=list(pages),capture_state='local_pdf_and_selected_page_renders')
 E.append(r)
add('E-ESPR-01','M-045','Espresso subscription','https://subscribenow.economist.com/espresso','Concise analysis; Four longer stories; FAQ','Current official subscription copy; app build unknown','official_product','official_index_text',note='每日/每周问答措辞未统一。')
add('E-ESPR-02','M-045','Espresso Android developer listing','https://play.google.com/store/apps/details?id=com.economist.darwin','About this app; Updated on','Android listing; no installed build','official_app_store','opened_text','2026-04-10',note='独立 Espresso；不转用主应用离线帮助。')
add('E-ESPR-03','M-045','Terms for Specific Promotions','https://www.economistgroup.com/terms-for-specific-promotions','Student Espresso offer','Student eligibility only; 16+','official_terms','opened_text','2026-08-13')
add('E-ESPR-04','M-045','ACG Library: Espresso mobile access guide','https://library.aug.edu/how-to-guides/mobile-apps/espresso-from-the-economist','How can I connect Espresso from the Economist to the library?','Institution-specific login instructions; version unknown','institutional_access_guide','index_text_only',note='直接打开失败；仅读检索索引中的机构步骤。非厂商当前通用界面证明。')
for eid,title,url,loc,note in [
 ('01','Sort and Filter News Sources','6049857','Where to Find; Sorting/Filtering; Resetting; Example','事件筛选与全局筛选分开。'),
 ('02','Save Articles for Later','2044545','Mobile/Web save and access','移动端收藏仍需联网。'),
 ('05','Follow Your Favorite Topics & Interests','950017','On Mobile; On Web','显式关注进入My Feed。'),
 ('06','Hide Read Mobile Stories','3523649','How to Enable','iOS/Android Premium+；已读阈值未知。'),
 ('07','Hide Paywalled Articles','245569','External subscription; Paywall labels; filters','Ground不能解除外部付费墙。'),
 ('08','How to Report an Issue','9683777','Web; Android; iOS; What Happens After','未发送报告；平台路径不同。')]:
 add('E-GRND-'+eid,'M-046',title,'https://help.ground.news/en/articles/'+url,loc,'Web/mobile official help; app builds unknown',date='2026-03-09',note=note)
add('E-GRND-03','M-046','Methodology','https://ground.news/rating-system','Bias Ratings; Factuality; Ownership; Blindspot','Publication-level ratings; current web methodology','official_methodology',note='标签不等于逐篇事实裁决。')
add('E-GRND-04','M-046','Bias Comparison Summary','https://help.ground.news/en/articles/3189505','Where to Find; AI Summaries; I Do Not See','Web/mobile help; app builds unknown',date='2026-08-29',note='独立报道覆盖不足时可缺比较摘要。')
add('E-TFK-01','M-047','For Educators','https://www.timeforkids.com/educators','Subscription Benefits; Accessibility; K-1/2/3-6 editions','US education subscription; selected-article tools only','official_educator_documentation',note='分级/词汇/朗读非全篇统一承诺。')
add('E-TFK-02','M-047','K-1 article index','https://www.timeforkids.com/k1/','Grade/topic/search; per-item Audio/Spanish; pagination','Public web K-1 directory on checked date','official_content','opened_text')
add('E-TFK-03','M-047','Spotlight on Plants','https://www.timeforkids.com/k1/spotlight-on-plants-k1/','Article body; language/print; related stories; HTML audio container','K-1 public article published 2026-10-16; dynamic player not run','official_content','opened_text_and_html_inspection',artifact='work/news-interactions/tfk-plants.html',note='HTML容器只证明配置；未试听或执行播放器。')
E[-1]['source_published_on']='2026-10-16'
add('E-TFK-04','M-047','K-1 Teacher Guide: Oceans','https://www.timeforkids.com/wp-content/themes/tfk/build/assets/pdf/educators/teachers-guide-gk1.pdf','PDF pp.1-3; static pp.2-3 viewed','May 2026 K-1 Oceans; 12-page PDF','official_teacher_guide','downloaded_pdf_text_and_static_pages',artifact='work/news-interactions/tfk-teacher.pdf',pages=(2,3),note='教师组织的纸面任务；不是应用自动批改。')
add('E-TFK-05','M-047','K-1 Editorial Calendar 2026-2027','https://www.timeforkids.com/wp-content/themes/tfk/build/assets/pdf/calendars/k-1-editorial-calendar.pdf','Single page calendar','2026-2027 calendar; illustrative covers have other dates','official_editorial_calendar','downloaded_pdf_text',artifact='work/news-interactions/tfk-calendar.pdf')
add('E-WJ-01','M-048','About us','https://theweekjunior.com/about-us','Ages 8-14; US magazine scope','US current product description','official_product')
add('E-WJ-02','M-048','In the Magazine','https://theweekjunior.com/in-the-magazine','32 pages; reporting; features; sample link','US current product description','official_product')
add('E-WJ-03','M-048','The Week Junior US issue 132 sample','https://static.theweekjunior.com/twjus-issue-132-web.pdf','PDF pp.1,2,5,11,12,14,15; print cover,2-3,8-9,20-23,26-29','2022-10-28 issue132; only cover plus print2-29 in 15 PDF pages','official_sample_issue','downloaded_pdf_static_pages',artifact='work/news-interactions/wj132.pdf',pages=(1,2,5,11,12,14,15),note='图像扫描；不完整32页，不能代表现行应用交互。')
E[-1]['source_published_on']='2022-10-28'
add('E-WJ-04','M-048','The Big Debate','https://kids.theweekjunior.com/polls','This week and last week','Current US kids website; poll not submitted','official_content')
add('E-WJ-05','M-048','The Week Junior US iOS developer listing','https://apps.apple.com/us/app/the-week-junior-us/id1505551245','Description; Version History; Compatibility','US app only; listing version not installed','official_app_store','official_index_text','2025-09-04',build='1.6 (store listing only)',note='内容评级4+不等于产品8-14岁设计目标。')
add('E-WJ-06','M-048','The Week Junior US Android developer listing','https://play.google.com/store/apps/details?id=com.futurenet.theweekjuniorus','About this app; Updated on','US package; no installed build','official_app_store','official_index_text','2025-09-01')
add('E-WJ-07','M-048','Terms and Conditions','https://theweekjunior.com/terms','Digital product subscription access','US subscription access boundary only','official_terms','official_index_text')
add('E-WJ-08','M-048','The Week Junior Subscription','https://subscribe.theweekjunior.com/','Print/Print+Digital; under18 parent/guardian','US offer; no order submitted','official_subscription','official_index_text')
add('E-BRIT-01','M-049','Britannica School: Elementary','https://britannicaeducation.com/solutions/prek-12/britannica-school/elementary/','Grades3-5; Navigator; Dictionary; Explore; In the News','US Elementary; current site; build unknown','official_product',note='与Early Elementary分开；新闻频率与指南不一致。')
add('E-BRIT-02','M-049','Britannica School Guided Tour','https://britannicaeducation.com/wp-content/uploads/2025/10/be-resource-Britannica-School-Guided-Tour_2025-Branding-Interactive.pdf','PDF pp.4-6,13,17-18; static pp.4,6 viewed','20-page current onboarding-linked PDF; body copyright2026; URL2025/10','official_manual','downloaded_pdf_text_and_static_pages',artifact='work/news-interactions/brit-school.pdf',pages=(4,6),note='ASK仅High School；p8-9出现Library措辞，范围保守。')
add('E-BRIT-03','M-049','Early Elementary Guided Tour','https://britannicaeducation.com/wp-content/uploads/2025/10/BSchool-early-elementary-GuidedTour-2025-.pdf','PDF pp.3-5,8; static pp.4,5 viewed','11-page current onboarding-linked PDF; body copyright2026; PreK-2','official_manual','downloaded_pdf_text_and_static_pages',artifact='work/news-interactions/brit-early.pdf',pages=(4,5),note='Fundamentals需订阅启用；原生西语与机器翻译不同。')
add('E-BRIT-04','M-049','Britannica School Onboarding and FAQ','https://britannicaeducation.com/onboarding/britannica-school/','Download a Guided Tour; FAQ Accounts/Research/Technical','Current US web help; builds unknown','official_help',note='个人保存需账号；翻译后部分工具禁用。')
E.sort(key=lambda r:r['evidence_id'])
(base/'news-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in E))
rows=[]
def flow(i,p,task,d,e,b,g,case,v='not_observed'):
 rows.append(dict(flow_id=i,product_id=p,task=task,depth=d,evidence_ids=';'.join(e),known_branches=b,gaps=g,visual_status=v,hands_on='false',case_path='cases/'+case,checked_on='2026-10-26'))
for num,task,ev,gap in [('01','一日简报读完',['01','02'],'精确入口与已读结算返回'),('02','主题偏好与每周深读',['01','02'],'偏好保存反馈与生效时机'),('03','文章听读与翻译',['02'],'播放控制音文同步及中断恢复')]:
 flow('F-M045-'+num,'M-045',task,'D1',['E-ESPR-'+x for x in ev],'内容或权限可用性未实测',gap,'M-045-espresso.md')
flow('F-M045-04','M-045','机构登录及学生资格','D2',['E-ESPR-04','E-ESPR-03'],'机构特定账号和16岁以上资格','通用新用户错误恢复及当前界面','M-045-espresso.md')
for num,task,ev,branch,gap in [('01','同一事件比较并筛选来源',['01','03','04','07'],'缺比较摘要及清除筛选与外部付费墙','原文跳转返回和零结果实际状态'),('02','收藏与回访',['02'],'移动端收藏仍需联网','取消收藏删除和同步冲突'),('03','关注与隐藏已读',['05','06'],'移动端Premium+门槛','已读阈值撤销及账号切换'),('04','报告错误',['08'],'Web Android iOS不同路径','提交成功及修复时延未实测')]:
 flow('F-M046-'+num,'M-046',task,'D2',['E-GRND-'+x for x in ev],branch,gap,'M-046-ground-news.md')
flow('F-M047-01','M-047','K1公开植物文章阅读','D2',['E-TFK-02','E-TFK-03'],'正文西语打印及同主题回访','动态朗读控件与续读状态','M-047-time-for-kids.md')
flow('F-M047-02','M-047','教师组织海洋单元任务','D2',['E-TFK-04'],'读前提问阅读应用与讨论','非应用自动评分;未观察课堂','M-047-time-for-kids.md','static_reference_only')
flow('F-M047-03','M-047','分级辅助与分配','D1',['E-TFK-01','E-TFK-03'],'只有特定文章有辅助工具','选级词义弹窗提交和完成记录','M-047-time-for-kids.md')
flow('F-M048-01','M-048','一期内容阅读路线','D2',['E-WJ-02','E-WJ-03'],'历史公开样张只有封面及2至29页','缺30至32页及完整一期结束','M-048-week-junior.md','static_reference_only')
flow('F-M048-02','M-048','辩论阅读与跨周反馈','D2',['E-WJ-03','E-WJ-04'],'背景双方论据及下周结果','未投票;提交与改票未知','M-048-week-junior.md','static_reference_only')
flow('F-M048-03','M-048','纸面谜题提示与核对','D2',['E-WJ-03'],'第26页题与提示及29页答案','没有数字判错/奖励证据','M-048-week-junior.md','static_reference_only')
flow('F-M048-04','M-048','US数字当期与往期','D1',['E-WJ-05','E-WJ-06','E-WJ-07'],'数字访问随权益期限','阅读器朗读离线恢复和购买恢复','M-048-week-junior.md')
flow('F-M049-01','M-049','Elementary文章研究','D2',['E-BRIT-01','E-BRIT-02','E-BRIT-04'],'章节词典和可用阅读级别','精确搜索返回与播放恢复','M-049-britannica.md','static_reference_only')
flow('F-M049-02','M-049','Early百科读听','D2',['E-BRIT-03'],'原生英西语与逐词朗读','自动续节和退出恢复','M-049-britannica.md','static_reference_only')
flow('F-M049-03','M-049','保存引用与翻译限制','D2',['E-BRIT-02','E-BRIT-04'],'个人账号及翻译禁用工具','成功提示删除重名跨设备','M-049-britannica.md','static_reference_only')
flow('F-M049-04','M-049','Fundamentals活动','D1',['E-BRIT-03'],'订阅必须启用','一道任务的失败提示重试奖励','M-049-britannica.md')
with (base/'news-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
print('wrote',len(E),'evidence;',len(rows),'flows')
