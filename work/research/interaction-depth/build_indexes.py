import csv
import json
import re
from pathlib import Path

ROOT = Path('/home/workspace/reader')
LIB = ROOT / 'docs/research/interaction-library'
DATE = '2026-10-26'
CASES = {
    'M-002': 'M-002-kobo.md', 'M-015': 'M-015-readwise.md',
    'M-020': 'M-020-anki.md', 'M-030': 'M-030-khan-kids.md',
    'M-035': 'M-035-hiby.md', 'M-037': 'M-037-yoto.md',
    'M-044': 'M-044-inoreader.md',
}

# Metadata indexes authored from the cited research, not automatically inferred product behavior.
sources = [
('E-KOB-01','M-002','Define words with the dictionary','https://help.kobo.com/hc/en-us/articles/360017639893-Define-words-with-the-dictionary-on-your-Kobo-eReader','official_help','opened_text','Kobo eReader; compatible books','Dictionary steps and compatibility note','长按、松手显示释义、轻点关闭；部分内容例外。'),
('E-KOB-02','M-002','Highlight text','https://help.kobo.com/hc/en-us/articles/360017481334-Highlight-text-on-your-Kobo-eReader','official_help','official_index_text','Kobo eReader; colour options model-specific','Highlight steps; Undo Highlight; colour section','选区自动高亮，可撤销；此文本操作不适用 PDF。'),
('E-KOB-03','M-002','Add notes to your eBook','https://help.kobo.com/hc/en-us/articles/360017639993-Add-notes-to-your-eBook','official_help','opened_text','Kobo eReader; purchased vs third-party books','Add note steps; third-party eBooks notice','笔记保存与同步受来源影响；第三方书删除会带走本机笔记。'),
('E-KOB-04','M-002','Export annotations from your books','https://help.kobo.com/hc/en-us/articles/29991333812631-Export-annotations-from-your-books','official_help','opened_text','Kobo.com; eligible Kobo/Kobo Plus books','Before you start; Export your annotations from Kobo.com','同步后网页导出；资格、格式和生成文件保留期分别规定。'),
('E-RDW-01','M-015','Reader Feed FAQ','https://docs.readwise.io/reader/docs/faqs/feed','official_help','opened_text','Reader web/mobile; mode-specific','Adding feeds; seen; partial feed content; refresh','普通滚动和移动卡片视图的 seen 规则不同；提供部分正文处理入口。'),
('E-RDW-02','M-015','Reader Text-to-speech FAQ','https://docs.readwise.io/reader/docs/faqs/text-to-speech','official_help','opened_text','Reader web/mobile; EPUB/PDF distinctions','Pause vs stop; scrolling; offline TTS','暂停与停止语义不同；播放位置与浏览位置可分开；无离线 TTS。'),
('E-RDW-03','M-015','Reader Highlights tags and notes FAQ','https://docs.readwise.io/reader/docs/faqs/highlights-tags-notes','official_help','opened_text','Reader web/mobile; command differences','Keyboard; auto highlighting; missing highlights; tags','自动高亮可调；定位失败的批注仍可从 Notebook 找到。'),
('E-ANK-01','M-020','Anki Studying','https://docs.ankiweb.net/manual/studying','official_manual','opened_text','Anki desktop; no plugin assumptions','Decks; Questions; Answer buttons; More','自评影响调度；暂埋、停用和删除对象不同。'),
('E-ANK-02','M-020','Anki Syncing','https://docs.ankiweb.net/manual/syncing','official_manual','opened_text','Anki desktop and AnkiWeb','First sync; merging; one-way sync; media','首次同步方向、普通合并和媒体处理分别说明。'),
('E-ANK-03','M-020','Anki Backups','https://docs.ankiweb.net/manual/backups','official_manual','opened_text','Anki desktop; features may have minimum versions','Automatic backups; restoring; collection export','自动备份不含媒体；恢复前后有同步状态变化，完整导出另选媒体。'),
('E-KHK-01','M-030','Books in Khan Academy Kids','https://khankids.zendesk.com/hc/en-us/articles/4409036780955-Learn-more-about-the-books-in-the-Khan-Academy-Kids-app','official_help','official_index_text','Khan Kids mobile; Books vs Reading','Read to Me; Read by Myself; Reading tab','同书可自主读听，Reading 入口可能加入互动教学。'),
('E-KHK-02','M-030','Learning level and child progress','https://khankids.zendesk.com/hc/en-us/articles/360041615571-How-does-the-learning-level-adjust-and-how-do-I-view-my-child-s-progress','official_help','opened_text','Khan Kids home mobile; user profile level','Learning Path; parents; checkmarks; missing marks','路线适应表现；进度可见性受档案、级别和同步影响。'),
('E-KHK-03','M-030','Learning Topics in Educational Settings','https://khankids.zendesk.com/hc/en-us/articles/360014856151-Learning-Topics-Using-Khan-Academy-Kids-in-Educational-Settings','official_help','official_index_text','Khan Kids mobile learning path/library','Home learning path; interruption; topics','返回学习路线可能进入下一课，不证明课内断点续接。'),
('E-KHK-04','M-030','Offline content in Khan Academy Kids','https://khankids.zendesk.com/hc/en-us/articles/360029139531-Learn-on-the-go-with-offline-content-in-Khan-Academy-Kids','official_help','opened_text','Khan Kids mobile; initial setup online','Kodi’s Suitcase and Create','离线区有指定活动，首次账户建立仍需联网。'),
('E-HBY-01','M-035','HiBy R3II 2025 User Guide','https://guide.hiby.com/en/docs/products/audio_player/hiby_r3ii_2025/guide/','official_manual','opened_text','HiBy R3II 2025 only','Section 4 gestures/quick operations; Section 5 playback settings','快捷面板短按/长按不同；A–B 入口由设置启用；断电记忆表述不足。'),
('E-YOT-01','M-037','Does my Yoto need to be online','https://support.yotoplay.com/en_gb/does-my-yoto-need-to-be-online-to-play-content-rkmbFFj7fg','official_help','official_index_text','Yoto family; some 4th-generation-only paragraphs','Offline preparation; Audio Download Status; cache','下载状态与书库条目不同；播放中下载描述与 E-YOT-03 待核对。'),
('E-YOT-02','M-037','How to use the sleep timer','https://support.yotoplay.com/en_gb/how-to-use-the-sleep-timer-on-your-yoto-HJ4gYKj7Mg','official_help','official_index_text','Yoto companion app and player; model not pinned','Maxi Player; timer; changing content','换卡/内容不重置计时；重启后行为未核实。'),
('E-YOT-03','M-037','Sound stuttering or playback not working','https://support.yotoplay.com/en_gb/sound-is-stuttering-or-playback-isn%27t-working-on-my-yoto-BJ1yKYsXMg','official_help','official_index_text','Yoto family; model/firmware unpinned','Wi-Fi; download queue; idle download condition','故障文档称新内容空闲下载，与 E-YOT-01 有待解释差异。'),
('E-INO-01','M-044','Inoreader Filters','https://www.inoreader.com/features/filters','official_feature_with_steps','opened_text','Inoreader web setup; server/web/mobile/API; plan-dependent','Set up a filter; duplicates; Filter once for every device','过滤新文章且影响 API 可见性；去重与排序不能混用。'),
('E-INO-02','M-044','Inoreader Rules','https://www.inoreader.com/features/rules','official_feature_with_steps','opened_text','Inoreader web setup; server actions; plan-dependent','Triggers; conditions; actions; chains; test a rule','规则提供条件/动作和预览说明；未实测副作用或重试。'),
]
evidence = []
for eid,pid,title,url,typ,mode,scope,locator,note in sources:
    evidence.append(dict(evidence_id=eid, product_id=pid, title=title, url=url,
        checked_on=DATE, source_type=typ, access_mode=mode, version_scope=scope,
        app_build=None, firmware_version=None, source_updated_on=None,
        locator=locator, note=note, hands_on=False, visual_observed=False,
        archived_artifact=None, sha256=None, capture_state='bibliography_and_short_note_only',
        supersedes=None))
for item in evidence:
    if item['evidence_id']=='E-KHK-02': item['source_updated_on']='2026-02-26'
    if item['evidence_id']=='E-KHK-04': item['source_updated_on']='2026-02-20'
    if item['evidence_id'].startswith('E-YOT-'):
        item['access_limitation']='Direct open returned a dynamic shell; official indexed text used. Not a full archived page.'
    if item['evidence_id']=='E-YOT-01': item['index_reported_date']='2026-09-10'
    if item['evidence_id']=='E-YOT-02': item['index_reported_date']='2026-07-20'
(LIB/'evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in evidence))

tasks = [
('M-002',1,'查词与返回','E-KOB-01','格式/书籍例外','查询历史、精确机型、视觉反馈'),
('M-002',2,'选区、高亮与笔记','E-KOB-02;E-KOB-03','自动高亮、撤销、第三方书删除','取消/撤销全流程、离线、冲突'),
('M-002',3,'同步后导出批注','E-KOB-04','内容资格、格式、文件保留期','失败/等待/重新生成、具体版本'),
('M-015',1,'订阅与处理 Feed','E-RDW-01','不同视图 seen、部分正文','视图切换、解析失败、重复导入'),
('M-015',2,'听读与双向定位','E-RDW-02','暂停/停止、滚动分离、离线','强退/重启/换设备、加载失败'),
('M-015',3,'批注与标签','E-RDW-03','自动高亮、定位失败、标签层级','重定位、内容更新、完整返回流程'),
('M-020',1,'回忆与自评','E-ANK-01','评分、到期队列、当日完成','误评撤销、插件/调度版本'),
('M-020',2,'暂埋、停用与删除','E-ANK-01','恢复期限、对象范围','确认对话、Undo、持久保存粒度'),
('M-020',3,'同步与备份恢复','E-ANK-02;E-ANK-03','单向同步、合并、媒体、恢复','两端测试、失败恢复、版本差异'),
('M-030',1,'自主读听与阅读课','E-KHK-01','Books/Reading、朗读/自主读','中途切换、具体题型与提示'),
('M-030',2,'路线、进度与中断','E-KHK-02;E-KHK-03','级别/档案、奖励、下一课','奖励数值、失败重试、课内保存'),
('M-030',3,'寻找离线活动','E-KHK-04','指定离线范围、初次联网','资源准备、退出重开、失败提示'),
('M-035',1,'全局快捷面板','E-HBY-01','长短按、可配置手势','手势冲突、锁屏、延迟与反馈'),
('M-035',2,'片段循环与睡眠关机','E-HBY-01','入口由设置启用、计时','非法 A/B、切曲、断电记忆粒度'),
('M-037',1,'准备离线故事','E-YOT-01','下载状态、缓存淘汰、网络条件','正文归档、型号、完成/失败状态'),
('M-037',2,'睡眠计时','E-YOT-02','换内容不重置','重启/跨设备/后台状态'),
('M-037',3,'播放卡顿排查','E-YOT-01;E-YOT-03','网络、队列、下载时机冲突','冲突原因、实际错误提示与恢复'),
('M-044',1,'过滤与去重','E-INO-01','范围、匹配、API 可见性','误过滤回看、追溯执行、误去重'),
('M-044',2,'条件规则与预览','E-INO-02','ALL/ANY、级联、翻译触发','预览副作用、循环/重试/限额'),
]
with (LIB/'coverage.csv').open('w',newline='') as f:
    fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
    writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader()
    for pid,num,task,eids,branches,gaps in tasks:
        writer.writerow(dict(flow_id=f'F-{pid.replace("-","")}-{num:02}',product_id=pid,task=task,depth='D2',evidence_ids=eids,known_branches=branches,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/'+CASES[pid],checked_on=DATE))

rows=[]
category=''
for line in (ROOT/'docs/research/market-competitor-matrix.md').read_text().splitlines():
    if re.match(r'## \d+\.',line): category=re.sub(r'^## \d+\. ','',line)
    match=re.match(r'\| (M-\d{3}) (.*?) \|',line)
    if not match: continue
    pid,name=match.groups();cells=line.split('|');description=cells[2].strip()
    urls=re.findall(r'\]\((https?://[^)]+)\)',description)
    seed=re.sub(r'\[([^]]+)\]\([^)]+\)','',description).strip().strip('、')
    rows.append(dict(product_id=pid,name=name,category=category,seed_topics=seed,
        legacy_document='docs/research/market-competitor-matrix.md',source_urls=';'.join(urls)))
assert len(rows)==43, len(rows)
additional=[
('M-044','Inoreader','个人新闻/RSS','订阅、筛选、去重、自动规则','personal-news.md','https://www.inoreader.com/features/filters;https://www.inoreader.com/features/rules'),
('M-045','The Economist Espresso','新闻简报','有限简报、编辑选择、日常阅读','personal-news.md','https://subscribenow.economist.com/espresso'),
('M-046','Ground News','新闻来源比较','事件聚合、来源差异、盲区','personal-news.md','https://ground.news/rating-system'),
('M-047','TIME for Kids','儿童新闻','分龄新闻、背景与词汇','personal-news.md','https://www.timeforkids.com/'),
('M-048','The Week Junior US','儿童新闻','一期内容、解释、知识探索','personal-news.md','https://theweekjunior.com/about-us'),
('M-049','Britannica School Elementary','儿童百科','百科检索、分级、资料探索','personal-news.md','https://britannicaeducation.com/solutions/prek-12/britannica-school/elementary/'),
('M-050','DragonBox Numbers','儿童数学游戏','数量操作、任务与数学玩法','learning-engagement.md','https://dragonbox.com/products/numbers'),
('M-051','抖音','成人信息流机制参考','连续消费、反馈、探索；不作为内容价值范本','learning-engagement.md','https://lf3-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/douyin_agreement/70c3d13a-73cf-403a-8ecf-a16f70887c21.html'),
('M-052','今日头条','成人新闻/推荐机制参考','订阅、推荐、反馈、来源；效果未验证','learning-engagement.md','https://lf26-cdn-tos.draftstatic.com/obj/ies-hotsoon-draft/toutiaoGIP/5fb89857-7777-40b8-8105-989b87cdfb8f.html'),
]
for pid,name,category,seed,legacy,urls in additional:
    rows.append(dict(product_id=pid,name=name,category=category,seed_topics=seed,legacy_document='docs/research/'+legacy,source_urls=urls))
backlog = {}
for line in (LIB/'backlog.md').read_text().splitlines():
    m=re.match(r'\| (M-\d{3}) [^|]+\| (.*?) \|$',line)
    if m: backlog[m.group(1)] = m.group(2)
assert len(backlog)==52, len(backlog)
for row in rows:
    row.update(inventory_status='seed_only',feature_total='unknown',
        detailed_case=('cases/'+CASES[row['product_id']]) if row['product_id'] in CASES else '',
        case_status='partial_D2' if row['product_id'] in CASES else 'no_D2_case',
        next_gap=backlog[row['product_id']],
        indexed_on=DATE)
with (LIB/'catalog.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
print(json.dumps(dict(products=len(rows),detailed_products=len(CASES),task_flows=len(tasks),sources=len(evidence),hands_on=0,visual_flows=0),ensure_ascii=False))
