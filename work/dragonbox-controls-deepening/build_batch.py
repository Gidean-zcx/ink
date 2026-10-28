"""One-time DragonBox batch; old batch changes are proposals for root review."""
import csv, json, hashlib, shutil, copy
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/dragonbox-controls-deepening';A=L/'artifacts/dragonbox-controls-deepening';B=L/'batches'
assert not (B/'dragonbox-controls-deepening-evidence.jsonl').exists()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def savej(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvwrite(p,rows,fields=None):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields or list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
note='artifacts/dragonbox-controls-deepening/source-notes.md';notehash=sha(L/note)
for n in [4,5]:shutil.copyfile(W/f'guide-p{n}.png',A/f'guide-p{n}.png')
E=[]
def ev(n,title,url,kind,mode,scope,locator,description,raw,updated=None,**more):
 x=dict(evidence_id=f'E-DBCD-{n:02}',product_id='M-050',title=title,url=url,checked_on='2026-10-27',source_type=kind,access_mode=mode,version_scope=scope,app_build=None,firmware_version=None,source_updated_on=updated,locator=locator,note=description,hands_on=False,visual_observed=False,archived_artifact=note,sha256=notehash,capture_state='short_note_archived_raw_temporary',supersedes=None,raw_work_path=str(raw.relative_to(R)),raw_sha256=sha(raw));x.update(more);E.append(x)
pdf=R/'work/children-interactions/dragon-guide.pdf'
ev(1,'Numbers旧教学指南：Sandbox、拼图与数轴静态页','https://dragonbox.com/attachment/14/download','official_pedagogical_guide','existing_pdf_text_and_local_static_pages','Historical nine-page Numbers guide; build/date unknown','PDF pp.4/5/8 visually inspected; all 9 text pages reread','题目气泡属于教材；可见弯箭头不证明撤销；p8教学目标不当达成效果。',pdf,derived_from='E-DRGN-02',visual_observed=True,visual_kind='static_manual_pages',archived_artifact='artifacts/dragonbox-controls-deepening/guide-p4.png',sha256=sha(A/'guide-p4.png'),additional_artifacts=[dict(path='artifacts/dragonbox-controls-deepening/guide-p5.png',sha256=sha(A/'guide-p5.png'))],visual_audit_path=note,visual_audit_sha256=notehash,observed_work_images=[dict(path=str((W/f'guide-p{n}.png').relative_to(R)),sha256=sha(W/f'guide-p{n}.png'),pdf_page=n,archived=n in [4,5]) for n in [4,5,8]])
ev(2,'Numbers Android开发者描述与四模式','https://play.google.com/store/apps/details?hl=en_US&id=com.kahoot.numbers','official_app_store_listing','opened_public_html','Google Play US English com.kahoot.numbers; installed build unknown','About this game / FEATURES / Updated on / Offline tag','Run仅描述数量判断越障，无按钮/错误/结算；Offline标签不证明存档和首次授权；1–20与旧指南部分1–100分开。',W/'googleplay.html','2026-10-01',parsed_text_path=str((W/'googleplay.txt').relative_to(R)))
ev(3,'Kahoot/Kids外部儿童档案编辑和删除入口','https://support.kahoot.com/hc/en-us/articles/15265266752659-How-to-create-edit-and-delete-Kahoot-Kids-profiles','official_help','official_search_index_text','Kahoot and Kids external profile management; not Numbers in-game controls','How to create / In Kahoot Kids / In Kahoot app','Kahoot先激活成人档案；长按头像Edit profile；儿童为当前活动档案不能删除；最终确认与Numbers数据范围未知。',W/'search-profiles.txt','2026-09-17')
ev(4,'Kahoot Kids相关帮助五标题','https://support.kahoot.com/hc/en-us/sections/4412889303699-Kahoot-Kids','official_help_index','official_search_index_text','Related Kahoot Kids help section; not full Numbers feature inventory','Five article titles visible in indexed section','标题发现与正文研究分开；播放列表/问答游戏不并入Numbers。',W/'search-profiles.txt')
ev(5,'DragonBox消费应用与学校产品边界','https://dragonbox.com/privacy-policy','official_privacy_notice','public_html_saved_then_local_parser_recovery','DragonBox consumer apps versus School; no operation evidence','DragonBox Apps / DragonBox School / Published 10.06.2026','消费Apps与学校服务分开；请求落原件后解析报错，HTTP状态未保留，不据其确认存档实现。',W/'privacy.html',source_reported_date='10.06.2026',http_status=None,parsed_text_path=str((W/'privacy.txt').relative_to(R)))
(B/'dragonbox-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))

masterF={r['flow_id']:r for r in csv.DictReader((L/'coverage.csv').open())};masterE={e['evidence_id']:e for e in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
updates=[]
def amend(n,known,gap,extra):
 before=masterF[f'F-M050-{n:02}'];after=copy.deepcopy(before)
 after.update(known_branches=known,gaps=gap,checked_on='2026-10-27')
 after['evidence_ids']=';'.join(dict.fromkeys(before['evidence_ids'].split(';')+extra))
 updates.append(dict(flow_id=before['flow_id'],before=before,after=after,changed_fields={k:dict(before=before[k],after=after[k]) for k in before if before[k]!=after[k]}))
amend(1,'旧p5可见数字6空槽、构件及未完成图；四表示与金币用途沿旧指南。教材Quiz气泡不当内置出题或判分。','错放/提示/撤销、最后落位到结算、金币数值/重玩收益与恢复仍未核；静态白线不当动作轨迹。',['E-DBCD-01'])
amend(2,'旧p8以数轴、70+10+角色和高度说明数量关系；1–100为指南教学范围，非当前App硬上限。','碰撞、生命、重试、成功结算及是否保留奖励；四教学目标不当四关或学习成效。',['E-DBCD-01'])
amend(3,'Numbers外部管理分两App：Kids的Parents→成人年龄→Profiles→头像/铅笔；Kahoot先激活成人，长按孩子头像→Edit profile→Save或底部Delete Profile。','最终删除确认/保留期/撤销、Numbers存档同步和数据删除范围；Kids免费一档案与旧学习App六档案按计划/入口分开。',['E-DBCD-03'])
amend(4,'Profile→Child Activity Dashboard→选孩子；本周/上周、累计主题进度、推荐；学习时间按日更新，免费总时长与套餐详情分开。','Numbers具体准确率/每步事件字段、实时性与精确续关；未看Completed Activities配图，FAQ Settings与步骤Profile层级不同。',[])
amend(6,'旧档案和更新记录仍不证明局内续接；Google Play含Offline标签，但没有Numbers专属离线授权/保存步骤。','退出/重开按钮、未完槽位及金币提交时机、离线存档与跨端冲突；不套Kahoot主App离线机制。',['E-DBCD-02'])
savej(W/'old-coverage-updates.json',updates)
old=masterE['E-DRGN-05'];after=copy.deepcopy(old)
cap=dict(checked_on='2026-10-27',access_mode='official_search_index_text',source_updated_on='2026-10-17',raw_work_path=str((W/'search-profiles.txt').relative_to(R)),raw_sha256=sha(W/'search-profiles.txt'),locator='Child Activity Dashboard indexed result; weekly/topic/FAQ sections',archived_artifact=note,sha256=notehash,hands_on=False,visual_observed=False,access_limitation='Direct HTTP returned403 and stopped; this is independently returned official indexed text, not origin HTML.')
after.setdefault('additional_captures',[]).append(cap);savej(W/'old-evidence-additional-captures.json',[dict(evidence_id='E-DRGN-05',before=old,after=after)])
fields=list(next(iter(masterF.values())))
F=[]
def flow(n,task,depth,ids,known,gaps,visual):
 F.append(dict(flow_id=f'F-M050-{n:02}',product_id='M-050',task=task,depth=depth,evidence_ids=ids,known_branches=known,gaps=gaps,visual_status=visual,hands_on='false',case_path='cases/M-050-dragonbox.md',checked_on='2026-10-27'))
flow(7,'Sandbox自由构数与比较','D2','E-DRGN-02;E-DBCD-01;E-DBCD-02','自选数量问题，组合/分割/堆叠并比较对象高度与数轴；指南示范凑十和33−17，提到构建到100；没有固定胜利终点。','开始菜单/提供对象、精确手势、弯箭头作用、对象丢弃/撤销、退出存档均未核；教材提问不是自动判分。','static_manual_pages_only')
flow(8,'Run快速数量判断与越障','D1','E-DRGN-03;E-DBCD-02','开发者声明用手指/Noom/数字作快速判断并越障；不是旧PDF中的Ladder。','输入手势、节奏/速度调整、错误惩罚、生命/奖励、完成与退出恢复全部未核。','not_observed')
csvwrite(B/'dragonbox-controls-deepening-coverage.csv',F,fields)

I=[]
def inv(kind,title,url,locator,ids,fs,status,gap):
 I.append(dict(entry_id=f'M050-DBCD-{len(I)+1:03}',entry_type=kind,title=title,source_url=url,locator=locator,evidence_ids=ids,flow_ids=fs,review_status=status,gap=gap))
for n,title in enumerate(['封面：Number Sense','Number Sense说明','The powers of the Nooms','The sandbox','The puzzles','Four piece slots and puzzle goals','The Ladder','Ladder pedagogical goals','产品数量声明'],1):
 fs={3:'F-M050-07',4:'F-M050-07',5:'F-M050-01',6:'F-M050-01',7:'F-M050-02',8:'F-M050-02'}.get(n,'')
 inv('historical_guide_page',title,E[0]['url'],f'PDF p{n}','E-DBCD-01' if n in [4,5,8] else 'E-DRGN-02',fs,'本轮文字已读；'+('本轮看静态页' if n in [4,5,8] else '沿用旧观察范围'),'页标题不是完整菜单；未固定构建')
for mode,fs in [('Sandbox','F-M050-07'),('Puzzle','F-M050-01'),('Ladder','F-M050-02'),('Run','F-M050-08')]:inv('store_declared_activity',mode,E[1]['url'],'FEATURES','E-DBCD-02',fs,'开发者正文已读','入口/完整错误和恢复不由声明补齐')
for title,fs in [('Child Activity Dashboard','F-M050-04'),('Intro to Kahoot! Kids app',''),('How to create, edit, and delete Kahoot! Kids profiles','F-M050-03'),('Kahoot! Kids: How to create and share playlists',''),('How to play Kahoot! Kids quiz games','')]:inv('related_kids_help_title',title,E[3]['url'],'5个公开索引标题','E-DBCD-04',fs,'索引标题；相关正文另列E','Kids外部范围，不是Numbers菜单；空映射不当不支持')
for title in ['How to open the Child Activity Dashboard','Weekly overview','Completed Activities','Topic Progress','Boost Learning','Monthly email report']:
 inv('related_dashboard_section',title,old['url'],'官方索引正文小节','E-DRGN-05','F-M050-04','索引正文已读；未看配图','非Numbers专属每步统计；未开通邮件')
for title in ['How to create a child profile','In the Kahoot! Kids app','In the Kahoot! app']:
 inv('external_profile_help_section',title,E[2]['url'],'正文小节','E-DBCD-03','F-M050-03','官方索引正文已读','未执行建档/修改/删除；数据范围未知')
csvwrite(B/'dragonbox-controls-deepening-inventory.csv',I)
savej(W/'build-counts.json',dict(new_E=len(E),new_F=len(F),inventory=len(I),old_F=len(updates),old_fields=sum(len(x['changed_fields']) for x in updates),old_E=1))
print((W/'build-counts.json').read_text())
