from pathlib import Path
import json,csv,hashlib
b=Path('docs/research/interaction-library/batches');b.mkdir(exist_ok=True)
rows=[]
def e(eid,pid,title,url,loc,mode='opened_text',kind='official_policy',artifact=None,update=None,publish=None,note=''):
 r=dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type=kind,access_mode=mode,version_scope='China mainland main mobile app; OS and installed build unknown',app_build=None,firmware_version=None,source_updated_on=update,locator=loc,note=note,hands_on=False,visual_observed=False,archived_artifact=artifact,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
 if artifact:r['sha256']=hashlib.sha256(Path(artifact).read_bytes()).hexdigest();r['capture_state']='raw_public_response_archived_no_visual_observation'
 if publish:r['source_published_on']=publish
 if mode=='public_readonly_api_text':r['retrieval_request']='GET https://trust.douyin.com/thematic/api/trust/article?id='+url.rsplit('/',1)[1];r['note']+=' Public frontend endpoint; Base64 JSON content decoded locally, no login.'
 rows.append(r)
e('E-DOUY-01','M-051','抖音算法备案公示说明','https://lf3-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/douyin_agreement/70c3d13a-73cf-403a-8ecf-a16f70887c21.html','个性化推荐、检索过滤、排序精选',kind='official_algorithm_disclosure')
e('E-DOUY-02','M-051','抖音隐私政策','https://www.douyin.com/draft/douyin_agreement/douyin_agreement_privacy.html?id=6773901168964798477','3.1.5 自主订阅及个性化内容',update='2026-09-11',note='Effective 2026-09-18. Web tool full body; direct HTTP local copy was an empty page shell and is not archived as evidence.')
e('E-DOUY-03','M-051','抖音推出新版使用管理助手','https://trust.douyin.com/article/15710','切换推荐风格、调节内容偏好、屏蔽',mode='public_readonly_api_text',kind='official_feature_explanation',artifact='work/feed-interactions/douyin-article-15710.json',publish='2025-05-21')
e('E-DOUY-04','M-051','抖音为何突出收藏按钮','https://trust.douyin.com/article/15388','收藏与点赞意图区分',mode='public_readonly_api_text',kind='official_design_explanation',artifact='work/feed-interactions/douyin-article-15388.json',publish='2025-01-20')
e('E-DOUY-05','M-051','未成年人模式使用须知','https://sf3-cdn-tos.douyinstatic.com/obj/ies-hotsoon-draft/douyin_agreement/79f8e7b6-e39d-4329-9cec-da7d38efc43a.html?hide_nav_bar=1','1、2、4',mode='downloaded_public_html_text',artifact='work/feed-interactions/douyin-minors.html',update='2026-06-12',note='Effective 2026-06-19. Web extraction truncated; direct public HTML parsed locally.')
e('E-DOUY-06','M-051','亲子护航、AI笔记功能升级上线','https://trust.douyin.com/article/3111768381106164','亲子护航；AI笔记',mode='public_readonly_api_text',kind='official_feature_explanation',artifact='work/feed-interactions/douyin-article-3111768381106164.json',publish='2026-01-14',note='Images and embedded videos not viewed.')
e('E-TT-01','M-052','今日头条隐私政策','https://lf26-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/5fb89857-7777-40b8-8105-989b87cdfb8f.html','6.1.2、6.2.2、7.2.1、7.5.2',mode='downloaded_public_html_text',artifact='work/feed-interactions/tt-privacy.html',update='2026-10-14',note='Effective 2026-10-21; settings paths may vary by platform and version.')
e('E-TT-02','M-052','今日头条算法备案公示说明','https://lf3-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/fdead1b6-ddb1-4e24-b086-d18e3509c0fd.html','个性化推荐、检索过滤、排序精选',kind='official_algorithm_disclosure')
e('E-TT-03','M-052','今日头条未成年人个人信息处理规则','https://lf3-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/kids-info-protect/kids-info-protect.html?module_name=Android_tt_url&share_token=3d5a06ed-ba6e-4dfe-bf57-404b2753d157&tt_from=copy_link','未成年人模式、信息处理、功能限制',mode='downloaded_public_html_text',artifact='work/feed-interactions/tt-kids.html',note='HTML title/index retain older 儿童个人信息保护指引; actual body uses 未成年人个人信息处理规则. No update date found.')
e('E-TT-04','M-052','今日头条官方中国区 App Store','https://apps.apple.com/cn/app/%E4%BB%8A%E6%97%A5%E5%A4%B4%E6%9D%A1/id529092160','Developer description, platform, age rating, in-app purchases',kind='official_app_store',note='Main app publisher Beijing Douyin Information Service. No user reviews used.')
with (b/'feed-evidence.jsonl').open('w')as f:
 for r in rows:f.write(json.dumps(r,ensure_ascii=False)+'\n')
flows=[]
def flow(pid,num,task,depth,ids,branches,gaps):
 case='M-051-douyin.md' if pid=='M-051' else 'M-052-toutiao.md'
 flows.append(dict(flow_id=f'F-{pid.replace("-","")}-{num:02d}',product_id=pid,task=task,depth=depth,evidence_ids=ids,known_branches=branches,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/'+case,checked_on='2026-10-26'))
flow('M-051',1,'进入观看至退出回访','D1','E-DOUY-01;E-DOUY-04','无连续观察','播放状态与主入口及结束恢复')
flow('M-051',2,'不感兴趣与减少同类','D2','E-DOUY-03','长按或分享入口','撤销及提交反馈')
flow('M-051',3,'助手调节类别与探索','D2','E-DOUY-03','探索风格和三级强度及屏蔽','统计周期及同步恢复')
flow('M-051',4,'关闭推荐或主动搜索','D2','E-DOUY-02','推荐频道隐藏;其他频道保留','搜索结果返回与实际界面')
flow('M-051',5,'亲子绑定与时长管理','D2','E-DOUY-05;E-DOUY-06','双端接受与超时密码夜间限制','拒绝误绑解绑忘记密码')
flow('M-051',6,'AI笔记跳回片段','D2','E-DOUY-06','仅部分知识视频','摘要失败纠错导出与掌握判定')
flow('M-052',1,'一篇资讯读完回访','D1','E-TT-02;E-TT-04','无连续观察','阅读器状态及出处返回续读')
flow('M-052',2,'负反馈与标签屏蔽','D2','E-TT-01','X理由与增删屏蔽','提交撤销及弱网')
flow('M-052',3,'推荐开关与内容丰富度','D2','E-TT-01','隐藏推荐保留其他频道','重启同步及恢复内容')
flow('M-052',4,'兴趣重置与历史查询','D2','E-TT-01','清除后重新积累','确认撤销及列表内容续读')
flow('M-052',5,'搜索历史与无痕设置','D2','E-TT-01','搜索阅读两个无痕入口','实际检索主链及错误恢复')
flow('M-052',6,'未成年人模式阅读与限时','D1','E-TT-03','模式功能有限','开启设龄超时解锁完整流程')
with (b/'feed-coverage.csv').open('w',newline='')as f:
 w=csv.DictWriter(f,fieldnames=list(flows[0]));w.writeheader();w.writerows(flows)
print('wrote',len(rows),'evidence',len(flows),'flows')
