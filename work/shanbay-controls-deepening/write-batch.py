from pathlib import Path
import json,csv,hashlib
root=Path('/home/workspace/reader');base=root/'docs/research/interaction-library';w=root/'work/shanbay-controls-deepening'
hist='Historical public CMS help; platform as stated per article; exact App build and last update unknown; assign_date is editorial scheduling, not update date'
items=[
(1,'单词学习顺序','bcrmcn','blksqe','我的→学习设置；旧词优先/新旧混合','设置顺序的历史正文，保存反馈和已排队题重算未写。'),
(2,'自定义单词书','onnjg','buxtne','如何使用；如何批量添加；生词本打卡','区分App生词本学习与历史Web批量添加；当前上传能力、校验及额度未知。'),
(3,'同时背诵两本词书','onnjg','puica','切换词书、书库上限、删除后果','历史删除相关词书进度不可恢复；不等于当前所有账户限额，也未解释跨书词状态。'),
(4,'短文计划','btfesx','mjvss','每天需要读多少篇；能否中途退出','历史每日任务和中退失败后果；未写退出按钮与撤销，不能套现行打卡活动。'),
(5,'听力计划','bbumgi','uurvp','素材与打卡；未完文章；中途退出','历史5句计划与未完文章继续安排；不证明进程被杀后恢复精确句子或答案。'),
(11,'新词量和已掌握单词量','bcrmcn','cfucx','怎样才算掌握一个单词','有效学习次数并要求测试通过；与其他旧帮助口径并列，不推为现行算法。'),
(12,'无法发音','blmqnc','gvdcn','桌面浏览器排查；Flash/旧操作系统段','只识别帮助年代与平台边界，未执行所述设置，不把旧桌面修复建议用于当前手机。')]
records=[]
def make(n,title,url,kind,mode,scope,locator,note,path):
 p=root/path
 return dict(evidence_id=f'E-SBCD-{n:02}',product_id='M-019',title=title,url=url,checked_on='2026-10-26',source_type=kind,access_mode=mode,version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None,raw_work_path=path,raw_sha256=hashlib.sha256(p.read_bytes()).hexdigest())
for n,title,col,aid,locator,note in items:
 d=json.loads((w/f'cms-{aid}.json').read_text())
 records.append(make(n,title,f'https://web.shanbay.com/web/help/{col}/{aid}','official_help','http_json_text',hist,locator,note+f' 匿名GET https://apiv3.shanbay.com/cms/articles/{aid}；assign_date={d.get("assign_date")}不是更新时间。',f'work/shanbay-controls-deepening/cms-{aid}.json'))
records.append(make(6,'如何利用好大会员学雅思','https://web.shanbay.com/cms/articles/mijwu','official_tutorial','opened_text_and_http_json','Shanbay listening-speaking App; platform/build unspecified; CMS assign_date=2024-12-16T23:00:00+00:00 is not update date','1.听力：雅思专区；3.口语：语料库、AI口语模考','正文支持入口、作答辅助与评分/语法/润色反馈。第7/8图GET返回200 text/html浏览器校验，停止未观察；未采纳效果或考试规避宣传。','work/shanbay-controls-deepening/cms-mijwu.json'))
apps=[(7,'扇贝单词英语版', '698013609','5.9.75','2026-10-23','开发者功能描述；5.9.75；生词本历史日志','当前词表上传/AI/训练功能是声明；未据评论或隐私字段推导操作。'),(8,'扇贝阅读','598541783','4.62.1','2026-10-23','开发者描述；4.47/4.56.0/4.57.0/4.59.0/4.60.0日志','核日期HTML time datetime；自动/重读打卡、单句播放与旧帮助分开，未推断完成阈值。'),(9,'扇贝听力口语','897432803','3.57.0','2026-10-26','开发者功能描述及版本历史','盲听/选词/拼写与跟读复述目录；不作为完整录音/错误恢复链。'),(10,'扇贝口语独立App','1089026929','2.13.0','2026-10-26','开发者描述；2.0.0/2.1.0/2.2.0/2.6.0及2.13.0','独立App仍有当前商店条目，不能因听力口语命名推断下线或合并；未安装。')]
for n,title,aid,version,date,locator,note in apps:
 r=make(n,title+'：大陆App Store','https://apps.apple.com/cn/app/id'+aid,'developer_app_store_listing','opened_text_and_downloaded_html',f'China App Store iOS/iPadOS listing viewed 2026-10-26; {title} version {version}; release {date}; installed build unknown',locator,note,f'work/shanbay-controls-deepening/apple-{aid}.html')
 r.update(store_version=version,version_released_on=date)
 records.append(r)
records.sort(key=lambda r:r['evidence_id'])
(base/'batches/shanbay-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in records))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
def flow(num,task,eids,branches,gaps):
 return dict(zip(fields,[f'F-M019-{num:02}','M-019',task,'D2',eids,branches,gaps,'not_observed','false','cases/M-019-shanbay.md','2026-10-26']))
new=[flow(10,'从自定义词进入生词本学习','E-SBCD-02','我的→生词本；历史优先安排及5词打卡；Web批量与App分开','当前额度/导入校验/去重；原句保留；退出恢复'),flow(11,'雅思智能口语作答与反馈','E-SBCD-06','雅思专区→智能口语；参考答案/中文辅助；评分/语法/润色反馈','具体构建与权益；开麦/停止/提交；等待/失败；历史报告和返回')]
with (base/'batches/shanbay-controls-deepening-coverage.csv').open('w',newline='') as f:
 cw=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');cw.writeheader();cw.writerows(new)
old=list(csv.DictReader((base/'batches/vocab-coverage.csv').open()));byid={x['flow_id']:x for x in old};updated=[]
changes={
1:dict(evidence_ids='E-SBY-02;E-SBY-03;E-SBCD-11',known_branches='认识/提示/不认识；有效学习与测试通过统计口径分开',gaps='历史帮助文案/掌握口径差异；现版误评撤销、简单词恢复和跳过'),
3:dict(evidence_ids='E-SBY-05;E-SBCD-01;E-SBCD-03',known_branches='即时或次日；再来一组；旧词优先/混合；切换与删除词书后果分开',gaps='词序保存与队列重算；删除确认及共享词状态；当前限额与追加上限'),
4:dict(evidence_ids='E-SBY-06;E-SBCD-08',known_branches='旧查词/加入/手动完成分开；iOS4.47重读打卡及4.56.0自动打卡另标',gaps='新自动完成阈值及是否同一进度对象；重复入库；误点撤销；离线'),
5:dict(evidence_ids='E-SBY-06;E-SBCD-08',known_branches='旧点击句连续播放/付费译文；iOS4.56.0长按单句播放另标',gaps='新长按菜单关系；音频与对照范围；断点/中断/同步'),
6:dict(evidence_ids='E-SBY-08;E-SBY-09;E-SBY-10;E-SBY-11;E-SBCD-05',known_branches='提示后积分/星级不同；历史5句计划打卡与未完文章继续安排',gaps='现版各模式判题；跳过与错误反馈；精确断点/未提交答案恢复'),
7:dict(evidence_ids='E-SBY-08;E-SBY-16;E-SBCD-06;E-SBCD-09',known_branches='字幕/资源目录；雅思专区；5分钟泛听与5句计划分开；当前倍速/循环声明',gaps='当前打卡入口；搜索限制；循环设置及离线/后台中断'),
8:dict(evidence_ids='E-SBY-12;E-SBY-13;E-SBCD-04;E-SBCD-05',known_branches='补签/时光机；分享去重；历史阅读/听力计划中退失败且无完成奖励',gaps='退出确认与取消；失败计划恢复；现行门槛、额度与UI'),
9:dict(evidence_ids='E-SBY-01;E-SBY-16;E-SBCD-06;E-SBCD-10',known_branches='独立口语与听力口语并存；雅思专区口语路径；跟读/复述声明',gaps='两个App各自录音/提交/重录控制；权限/上传失败；评分申诉和恢复')}
for n,fieldsnew in changes.items():
 r=byid[f'F-M019-{n:02}'].copy();r.update(fieldsnew);updated.append(r)
with (w/'existing-coverage-updates.csv').open('w',newline='') as f:
 cw=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');cw.writeheader();cw.writerows(updated)
(w/'coverage-changed-fields.json').write_text(json.dumps({f'F-M019-{n:02}':d for n,d in changes.items()},ensure_ascii=False,indent=2)+'\n')
print('wrote 12E, 2 new F, 8 proposed old rows; no old batch/master writes')
