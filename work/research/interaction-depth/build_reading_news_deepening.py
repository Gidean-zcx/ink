import csv
import json
from pathlib import Path

root = Path('/home/workspace/reader/docs/research/interaction-library')
sources = [
    ('E-RDD-01','M-015','Basics','https://docs.readwise.io/reader/docs/faqs','Offline documents; reading progress; Trash','缓存、位置和设备回收站；移动端恢复入口两段文字不同。'),
    ('E-RDD-02','M-015','Global Ghostreader','https://docs.readwise.io/reader/guides/ghostreader/global','Start conversation; skills; tools; Manage conversations; mobile history FAQ','跨文档问答、工具许可、技能与会话；全局界面限web/desktop。'),
    ('E-RDD-03','M-015','Quick Lookup','https://docs.readwise.io/reader/guides/ghostreader/quick-lookup','Define / Lookup / Translate; Save, copy or dismiss','联网解释与显式保存分开。'),
    ('E-RDD-04','M-015','Searching','https://docs.readwise.io/reader/docs/faqs/searching','Full-text; Better Search; Feed; Filtered views','联网全库与离线本机范围不同；Feed正文不索引。'),
    ('E-RDD-05','M-015','Exporting','https://docs.readwise.io/reader/docs/faqs/exporting','Notebook export; CSV; OPML; Full Files and Articles','笔记、清单与完整文件包分别导出，未证明可恢复全部状态。'),
    ('E-RDD-06','M-015','Organizing Content','https://docs.readwise.io/reader/docs/organizing-content','Tags; Filtered Views; RSS Folders','命名并保存查询与RSS分组；并非全文搜索。'),
    ('E-IND-01','M-044','Read later','https://www.inoreader.com/features/read-later','Start your list; Two different archives; FAQ','稍后读、归档、删除对象及返回入口。'),
    ('E-IND-02','M-044','Full content','https://www.inoreader.com/features/full-content','Load the full article; FAQ','按需加载；失败留摘要外链；套餐影响保存。'),
    ('E-IND-03','M-044','Offline mode','https://www.inoreader.com/features/offline-mode','Take a folder offline; FAQ','Pro移动端准备与标记，离线动作恢复网络后同步。'),
    ('E-IND-04','M-044','Text-to-speech','https://www.inoreader.com/features/text-to-speech','Start listening; queue; background; limits','服务端语音和共用队列；未证明音频离线。'),
]
evidence=[]
for eid,pid,title,url,locator,note in sources:
    evidence.append(dict(evidence_id=eid, product_id=pid, title=title, url=url,
        checked_on='2026-10-26', source_type='official_help' if pid=='M-015' else 'official_feature_instructions',
        access_mode='opened_text', version_scope='Current unversioned documentation; web/mobile differences retained',
        app_build=None, firmware_version=None, source_updated_on=None, locator=locator, note=note,
        hands_on=False, visual_observed=False, archived_artifact=None, sha256=None,
        capture_state='bibliography_and_short_note_only', supersedes=None))
(root/'batches/reading-news-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evidence))

defs = [
 ('M-015',4,'准备离线内容','E-RDD-01','分区开关、缓存清理、单篇缓存例外','下载中断、容量不足、平台构建'),
 ('M-015',5,'浏览后返回阅读位置','E-RDD-01','最远进度与最后位置；X改进度','判定阈值、跨端冲突、原句位置'),
 ('M-015',6,'删除后恢复','E-RDD-01','原删除设备；正文和批注一起恢复','移动端Search/View入口差异、清空后的处理'),
 ('M-015',7,'搜索范围变化','E-RDD-04','在线全Library、离线本机；Feed不全文索引','索引延迟、迁入后失败、搜索排序'),
 ('M-015',8,'全库问答与动作','E-RDD-02','逐工具权限、确认、Remember','全部工具、执行失败、部分成功、撤回'),
 ('M-015',9,'技能与会话','E-RDD-02','作用范围、侧栏展开、移动端清除与网页历史','模型错误、历史删除传播、移动端构建'),
 ('M-015',10,'临时解释或存入笔记','E-RDD-03','查看、复制、保存、关闭分开','断网错误、连续选词、保存失败'),
 ('M-015',11,'导出不同数据对象','E-RDD-05','笔记、CSV、OPML、ZIP；异步邮件','文件结构、恢复一致性、链接失效重试'),
 ('M-015',12,'保存筛选与分组','E-RDD-06','web/mobile保存查询、RSS分组','无效查询、重命名冲突、删除引用'),
 ('M-044',3,'稍后读、继续与归档','E-IND-01','个人归档和源历史；回稍后读与删除','精确进度语义、误删恢复、跨端冲突'),
 ('M-044',4,'摘要转全文','E-IND-02','按需抓取、Pro存储、失败退回摘要','正文更新和批注定位、失败提示'),
 ('M-044',5,'离线准备和恢复同步','E-IND-03','移动Pro、条目标记、重连同步','半下载、磁盘满、并发修改冲突'),
 ('M-044',6,'文章听读与混合队列','E-IND-04','服务端生成、锁屏、译文朗读、日配额','离线音频、强退重启、跨端断点'),
]
paths={'M-015':'cases/M-015-readwise.md','M-044':'cases/M-044-inoreader.md'}
flows=[dict(flow_id=f'F-M{pid[2:]}-{n:02d}',product_id=pid,task=task,depth='D2',evidence_ids=eid,
    known_branches=branches,gaps=gaps,visual_status='not_observed',hands_on='false',
    case_path=paths[pid],checked_on='2026-10-26') for pid,n,task,eid,branches,gaps in defs]
for f in flows:
    assert f['flow_id'] in (root/f['case_path']).read_text(), f
with (root/'batches/reading-news-deepening-coverage.csv').open('w',newline='') as fp:
    writer=csv.DictWriter(fp,fieldnames=list(flows[0]));writer.writeheader();writer.writerows(flows)
print(json.dumps({'evidence':len(evidence),'flows':len(flows),'depth':'D2','hands_on':False}))
