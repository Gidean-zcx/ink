import csv
import hashlib
import json
import re
from pathlib import Path

root=Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
raw=root/'work/anki-deepening'
sources=[
 ('llms.txt','Desktop Manual index','Desktop Manual; Card Templates; Importing','范围目录，不代表全部功能已分析。'),
 ('importing-intro.md','Importing','File / Import','导入入口。'),
 ('importing-text-files.md','Text Files','Format; Mapping; Duplicates and Updating; File Headers','预览、字段匹配与原位更新。'),
 ('importing-packaged-decks.md','Packaged Decks','Scheduling; Updating; Anki 23.10 and Later','共享包更新及学习记录选择。'),
 ('editing.md','Adding/Editing','Adding; Cloze; Image Occlusion','笔记生成卡片与遮挡练习。'),
 ('templates-intro.md','Card Templates','Templates Screen; Deck Override','编辑、预览与生成目的地。'),
 ('templates-generation.md','Card Generation','Generation and Deletion; Conditional Replacement','空卡需显式检查清理。'),
 ('templates-fields.md','Field Replacements','Checking Your Answer','输入比较仍需自行评分。'),
 ('browsing.md','Browsing','Table Modes; Menus and Actions','笔记/卡片动作范围不同。'),
 ('searching.md','Searching','Simple; Limiting; Tags; Card State','查询组合和对象范围。'),
 ('filtered-decks.md','Filtered Decks','Custom Study; Home Decks; Creating Manually; Rescheduling','临时组归属及是否改长期调度。'),
 ('deck-options.md','Deck Options','Presets; Daily Limits; Timers; Auto Advance; FSRS','预设影响范围和重排；CMRR教程与移除条款有历史混用。'),
 ('leeches.md','Leeches','Counter; Editing; Waiting','复习失败计数与停用。'),
 ('stats.md','Statistics','Card Info; Deck/Collection; Today','作答次数和删除后的统计归属。'),
 ('media.md','Media','Checking; Manually Adding Media','检查不覆盖模板；回收位置需进一步核版本。'),
 ('exporting.md','Exporting','Text; Collection; Deck','导出对象与替换/合并区别。'),
 ('profiles.md','Profiles','Switch Profile; Profiles Window','档案与云账户一一对应，插件共享。'),
 ('addons.md','Add-ons','Get Add-ons; Delete; Compatibility','管理入口不保证全部插件兼容。'),
 ('troubleshooting.md','Troubleshooting','Safe Mode; About; Check Database','故障分层排查，非实际执行。'),
 ('preferences.md','Preferences','Distractions; Review; Editing; Syncing','界面、节奏与默认状态。'),
]
logs={Path(r['path']).name:r for r in json.loads((raw/'fetch-log.json').read_text()) if r.get('status')==200}
evidence=[]
for n,(filename,title,locator,note) in enumerate(sources,1):
 log=logs[filename]
 assert hashlib.sha256((raw/filename).read_bytes()).hexdigest()==log['sha256']
 evidence.append(dict(evidence_id=f'E-AND-{n:02}',product_id='M-020',title=title,url=log['url'],
  checked_on='2026-10-26',source_type='official_manual',access_mode='downloaded_official_markdown_read',
  version_scope='Anki desktop current manual; explicit minimum versions retained; installed build unknown',
  app_build=None,firmware_version=None,source_updated_on=None,locator=locator,note=note,
  hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',
  supersedes=None,raw_work_path=str((raw/filename).relative_to(root)),raw_sha256=log['sha256']))
(lib/'batches/anki-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evidence))

gaps={4:'错误弹窗、半导入回退、媒体冲突',5:'类型合并后字段映射、失败恢复',6:'空字段拒绝/取消、重复提示确认',7:'共享模板影响预览、删除恢复',8:'遮挡删除历史、图片替换锚点',9:'非拉丁输入、实际差异布局、强退保存',10:'多选误改撤销范围、同步冲突',11:'所有计数器及历史选项、取消/恢复',12:'取消/重建反馈、预览练习历史口径',13:'父子牌组与共享预设全部组合',14:'固定构建、迁移队列、实验参数效果',15:'停止、重进、语音冲突',16:'停用反馈、修改后调度与历史',17:'全部统计图、跨时区与历史修正',18:'回收位置、期限、格式与损坏恢复',19:'导出中断、旧版恢复、设置和插件',20:'多人同步冲突、退出保留、删除恢复',21:'安装和卸载状态、主版本迁移',22:'设置保存时机、音频和时间盒互动'}
case=lib/'cases/M-020-anki.md'
flows=[]
for line in case.read_text().splitlines():
 m=re.match(r'\| (F-M020-(\d{2})) ([^|]+)\|',line)
 if not m or int(m[2])<4:continue
 cols=[c.strip() for c in line.strip('|').split('|')]
 ids=list(dict.fromkeys(re.findall(r'E-AND-\d{2}',cols[-1])))
 # Source cells use Chinese shorthand for the second source in the same family.
 for short in re.findall(r'、(\d{2})(?:\D|$)',cols[-1]):
  eid='E-AND-'+short
  if eid not in ids:ids.append(eid)
 flows.append(dict(flow_id=m[1],product_id='M-020',task=m[3].strip(),depth='D2',evidence_ids=';'.join(ids),
   known_branches=cols[2],gaps=gaps[int(m[2])],visual_status='not_observed',hands_on='false',
   case_path='cases/M-020-anki.md',checked_on='2026-10-26'))
assert len(flows)==19,len(flows)
assert all(f['evidence_ids'] for f in flows)
with (lib/'batches/anki-deepening-coverage.csv').open('w',newline='') as fp:
 w=csv.DictWriter(fp,fieldnames=list(flows[0]));w.writeheader();w.writerows(flows)
print(json.dumps({'evidence':len(evidence),'new_flows':len(flows),'hands_on':False}))
