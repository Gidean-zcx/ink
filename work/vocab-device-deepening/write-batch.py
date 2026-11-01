from pathlib import Path
import csv,json

root=Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
work=root/'work/vocab-device-deepening'
batch=lib/'batches'
ev=[]
def add(eid,pid,title,url,scope,loc,note,typ='official_help',access='opened_text'):
 ev.append(dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type=typ,access_mode=access,version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None))

add('E-BCDVC-01','M-022','单词机单词本历史教程：设备下发与切换','https://www.baicizhan.com/little_class/article/892/share_view','Published 2022-11-08; hardware model, firmware and companion App build unspecified','正文首先/然后两段；设备首页确认同步、我的词书、单词新学','与M016的E-BCD-06为同一文献，本次补设备段。未观看视频；不能映射到Mini/Max。文章日期与当前关于页系列起始年份不能直接用来认定代际。','official_tutorial')
add('E-BCDVC-02','M-022','百词斩硬件关于页：多端同步宣传','https://danciji.baicizhan.com/about','Hardware family marketing; individual Mini/Max firmware unspecified','关于百词斩单词机；生态协同段','只确认多端同步声明D1，不推断双向作答同步、冲突策略或恢复能力；未采用用户评价、效果或排他性宣传。','official_product')
add('E-MMDVC-01','M-023','MM-WM01帮助：资源、按键与恢复补核','https://memodocs.maimemo.com/docs/support_wordmachine/','MM-WM01; binding App5.5.96+; learning sync firmware1.0.36+; actual installed build unknown','Q3/4/8/9/12/16/18/20；快速指南、按键介绍、帮助与反馈','复查E-MMOH-01同一来源。Q12原文Android5.6.06+、iOS5.60.7+，后者不擅自改写。公测/未来功能与现行描述并存。后续定位一次超时未反复重试；第一张配图连接阻塞后终止，未查看图片。')
add('E-MMDVC-02','M-023','墨墨官方iOS日志的时效边界','https://memodocs.maimemo.com/docs/product-log-memo-ios','iOS App changelog; displayed first entry V5.4.40 (2025-03-30); not device firmware','正文首条及全文设备关键词检视（排除导航）','只用于检查是否可佐证设备新版；未找到相应固件记录。顶部日期不能当今天最新版，也不能证明未来设备功能已交付。','official_release_notes','opened_text_and_http_html')
add('E-MMDVC-03','M-023','墨墨官方Android日志的时效边界','https://memodocs.maimemo.com/docs/product-log-memo-android','Android App changelog; displayed first entry V5.5.00 (2025-04-28); not device firmware','正文首条及全文设备关键词检视（排除导航）','与iOS日志互补查阅，但仍未取得设备固件更新条目；没有把App日志扩成硬件发布说明。','official_release_notes','opened_text_and_http_html')
add('E-MMDVC-04','M-023','单词机目录项：没有操作正文','https://memodocs.maimemo.com/docs/OPLXw0KlkiaSUDkN69ScWREfnhc','Public help navigation entry; version unspecified','页面正文标题单词机；无其他操作章节','已打开，仅导航与标题；不计为第二份操作手册，不从空页推断不支持。','official_help_entry','opened_title_only')
(batch/'vocab-device-deepening-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in ev))

fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
new=[
 ['F-M023-06','M-023','无响应排查与转帮助（MM-WM01）','D2','E-MMDVC-01','长按电源、复位、充电再试、App客服','恢复运行不保证数据恢复；重启/复位后的未同步记录和当前位置未知','not_observed','false','cases/M-023-maimemo-device.md','2026-10-26'],
]
with (batch/'vocab-device-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(fields);w.writerows(new)

changes={
 'F-M022-03':dict(evidence_ids='E-BCZH-01;E-BCDVC-01;E-BCDVC-02',known_branches='现Mini/Max离线及多端宣传D1；2022型号未知的计划下发/同步确认/切书局部D2',gaps='历史型号适用性、首次绑定、缓存、取消/断网、同步对象/冲突与恢复未知'),
 'F-M023-01':dict(evidence_ids='E-MMOH-01;E-MMDVC-01',known_branches='单书离线准备；自动同步与资源更新路径；云词本版本条件',gaps='完成提示、配网/绑定失败、主动更新时序、iOS版本号歧义与空词本'),
 'F-M023-02':dict(evidence_ids='E-MMOH-01;E-MMDVC-01',known_branches='短按自评；模式及合并/分开日量方向键；返回/退出',gaps='回退是否撤销、逐题提示/错答反馈、空任务、跨日及调整生效时间'),
 'F-M023-03':dict(evidence_ids='E-MMOH-01;E-MMDVC-01',known_branches='熟知/收藏/不想背及取消；跨书状态保留',gaps='暂停及状态优先级、误长按后的排期、反馈撤销与删除不能混同'),
 'F-M023-04':dict(evidence_ids='E-MMOH-01;E-MMDVC-01',known_branches='回顾不改调度；播放/暂停/重播/音量；有线耳机与未来蓝牙分开',gaps='中断断点、循环/终点、空队列和媒体缺失'),
 'F-M023-05':dict(evidence_ids='E-MMOH-01;E-MMDVC-01;E-MMO-07',known_branches='学习数据同步与资源更新区分；App删除后合并再同步；解绑丢未同步记录',gaps='两端冲突、上限边界、幂等重试、换机/解绑撤回和云端恢复未知'),
}
updates=[]
for r in csv.DictReader((batch/'vocab-coverage.csv').open()):
 if r['flow_id'] in changes:r.update(changes[r['flow_id']]);updates.append(r)
with (work/'existing-coverage-updates.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(updates)
print(json.dumps(dict(new_evidence=len(ev),new_flows=len(new),old_rows_for_root=len(updates),new_visuals=0)))
