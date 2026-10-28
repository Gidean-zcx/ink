import csv
import json
import re
from pathlib import Path

root = Path('/home/workspace/reader')
work = root / 'work/learning-control-deepening'
lib = root / 'docs/research/interaction-library'
logs = {r['name']: r for r in json.loads((work / 'fetch-log.json').read_text())}
date = '2026-10-26'
base = 'https://xxj.xunfei.cn/h5-help/'
specs = [
 (1, 'guide-list', '公开手册目录', '12 entries; not filtered by model', '目录只证明入口存在，不继承 T30 型号范围。'),
 (2, 'guide-19', '连接电脑传输文件', 'Listed legacy X1/X2/Q10/Z1/X3/Q20/T10 models', '先取得家长 USB 许可，再选择文件传输模式；PTP 范围不同。'),
 (3, 'guide-13', '翻译笔生词导入', 'Eight listed legacy learning-device models; same phone binding', '扫描收藏后导出扫码，学习机指定生词入口查看；单向导入，解绑不等于清历史。'),
 (4, 'guide-18', '手写笔 1.0 操作', 'Q20/X3 Pro/X2 Pro/X2/Z1; pen 1.0', '上下键配合擦除、星形收藏和人工批改手势；不属于自动判分。'),
 (5, 'guide-17', '二代 AI 手写笔兼容与查询', 'Three pen variants; listed legacy models only', '兼容型号按笔型分列；白笔有固件查询入口，不推定 T30。'),
 (6, 'guide-15', '投屏指引', 'Generic guide; full compatibility unspecified', 'Miracast 设置与第三方同网/扫码路线；页面夹杂翻译笔说明，不能当投屏型号表。'),
 (7, 'guide-16', '外部打印 App 文件交接', 'Generic learning-device guide; printer app variants', '错题生成文档，再进相印宝/喵喵机文档打印；自动交接方式未说明。'),
 (8, 'guide-90', '账号迁移通用问题', 'X3 Pro/T10/Q20/A10; minimum version conditions in source', '家庭许可、换设备、重置口令与删除孩子是不同操作；历史版本条件不能当现行所有型号规则。'),
 (9, 'guide-91', '历史账号迁移', 'X3 Pro/T10/Q20/A10 legacy family-account transition', '推荐列表非全部账号；验证码缺失可稍后处理。跨家庭删除再迁移建议与不可恢复说明冲突。'),
 (10, 'guide-92', '历史设备迁移', 'X3 Pro/T10/Q20/A10 legacy family-account transition', '设备升级及家庭成员确认影响显示；迁移阶段的管理员归属不可泛化为随时转让。'),
 (11, 'guide-94', '迁移后设备端常见问题', 'X3 Pro/T10/Q20/A10 legacy family-account transition', '守护失效需检查设备端个人中心更新并锁屏重进；迁移清旧锁屏口令，未绑定设备可能无数据。'),
]
records = []
def record(number, title, url, scope, note, source='official_help', mode='opened_text'):
 return dict(evidence_id=f'E-ILD-{number:02}', product_id='M-027', title=title, url=url,
  checked_on=date, source_type=source, access_mode=mode, version_scope=scope,
  app_build=None, firmware_version=None, source_updated_on=None, locator='标题及对应操作/问答段落',
  note=note, hands_on=False, visual_observed=False, archived_artifact=None, sha256=None,
  capture_state='bibliography_and_short_note_only', supersedes=None)
for number, name, title, scope, note in specs:
 log = logs[name]
 url = base if name == 'guide-list' else base + '#/manuals-detail?guideId=' + name.split('-')[1]
 r = record(number, title, url, scope, note, mode='public_help_api_text')
 r.update(retrieval_request={k:log[k] for k in ('url','method','payload','http_status','application_status')},
          raw_work_path=log['raw_work_path'], raw_sha256=log['raw_sha256'])
 records.append(r)
external = [
 (12,'家庭打印助手操作','2a6c7313-c0fc-407f-aaa5-1e11d2002cec/index.html','Companion printing assistant; complete model compatibility unspecified','按内容来源组织练习并配置答案位置、作答空间；提交打印不代表完成练习。'),
 (13,'错题变式与推荐复习范围','c3742620-d654-49cf-9e3d-4c19c361f4c4/gzhqd.html','X1 Pro/X2 Pro/X2/Q10/Z1 legacy guide','变式题按学段、科目、题目来源限制；小学不能套用中学能力。'),
 (14,'纸面指读无响应排查','ba1d635b-f0de-4e48-bb6d-b5a0b72cd060/gzhqd.html','X2 Pro/X2/Q10/T10','先进入桌面指学能力，再查摆放、单指、光线、教材和时间；与屏内点读不同。'),
 (15,'恢复出厂与账号数据','cf95af8a-8a61-4d1d-9e58-ad1e074c10b6/gzhqd.html','Generic learning-device series; exact generation/build unspecified','设置内恢复出厂清本地应用和设置；账号内特定学情可重登恢复，不能扩大为整机完整备份。'),
 (16,'打印入口与来源限制','d476e5e1-b7ef-4173-ae86-917ab0f6fbc7/111.html','Listed legacy device groups, not T30 Pro','阶段测的提交状态影响打印入口；答案解析可用性随题目来源变化。'),
]
for n,title,path,scope,note in external:
 records.append(record(n,title,'https://zskzs.iflytek.com/myfiles/'+path,scope,note))
r=record(17,'T30 Pro 功能与问题教程目录复查',base,'AISTUDY_SU30_T30PRO; directory current on check date; video build unknown','34 功能教程及 24 问题教程；只读目录，视频未观看。与原 E-IFLYL-03 同站，非独立验证。','official_help_index','public_help_api_text')
r['retrieval_requests']=[{k:logs[n][k] for k in ('url','method','payload','http_status','application_status','raw_work_path','raw_sha256')} for n in ['t30pro-tutorials','t30pro-problems']]
records.append(r)
(lib/'batches/learning-control-deepening-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in records))

fields=list(next(csv.DictReader((lib/'coverage.csv').open())))
flow_sources={4:[2],5:[3],6:[4,5],7:[6],8:[7],9:[8,9,10,11],10:[8],11:[8,15],12:[8,11],13:[11],14:[12,16],15:[14]}
case=lib/'cases/M-027-iflytek-learning.md'
flows=[]
for line in case.read_text().splitlines():
 m=re.match(r'\| (F-M027-(\d+)) (.*?) \| (.*?) \| (.*?) \|',line)
 if not m or int(m[2]) not in flow_sources: continue
 n=int(m[2]); flows.append(dict(flow_id=m[1],product_id='M-027',task=m[3],depth='D2',
  evidence_ids=';'.join(f'E-ILD-{v:02}' for v in flow_sources[n]),known_branches=m[4],gaps=m[5],
  visual_status='not_observed',hands_on='false',case_path=str(case.relative_to(lib)),checked_on=date))
assert len(flows)==12
def writecsv(path,rows,fields):
 path.parent.mkdir(parents=True,exist_ok=True)
 with path.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
writecsv(lib/'batches/learning-control-deepening-coverage.csv',flows,fields)
old_path=lib/'batches/children-coverage.csv'
old=list(csv.DictReader(old_path.open()))
for row in old:
 if row['flow_id']=='F-M027-03':
  row['evidence_ids']='E-IFLYL-05;E-ILD-08'
  row['known_branches']='旧四型号管理员与普通成员；相同桌面可有不同应用许可；退出家庭与删除孩子不同'
  row['gaps']='T30规则未知；迁移删除说明冲突；离线生效与权限冲突未查'
writecsv(old_path,old,fields)

inventory=[]
for name,key,kind,prefix in [('t30pro-tutorials','videoList','function_video','T'),('t30pro-problems','problemVideoList','problem_video','P')]:
 for item in json.loads((work/(name+'.json')).read_text())['data'][key]:
  inventory.append(dict(entry_id=f'I-M027-{prefix}{item["id"]}',product_id='M-027',scope='T30 Pro directory; video build unknown',entry_type=kind,title=item['videoName'],upstream_id=item['id'],url=item['videoUrl'],evidence_id='E-ILD-17',review_status='directory_only_video_not_watched',related_flow_ids='',checked_on=date))
guide_e={int(name.split('-')[1]):f'E-ILD-{n:02}' for n,name,*_ in specs if name!='guide-list'}
guide_e.update({14:'E-IFLYL-04',93:'E-IFLYL-05'})
guide_f={19:'04',13:'05',18:'06',17:'06',15:'07',16:'08',90:'03;09;10;11;12',91:'09',92:'09',94:'09;12;13',14:'02',93:'03'}
for item in json.loads((work/'guide-list.json').read_text())['data']['guideList']:
 gid=item['guideId']
 inventory.append(dict(entry_id=f'I-M027-G{gid}',product_id='M-027',scope='Unfiltered manual; apply per-source model conditions',entry_type='text_manual',title=item['guideTitle'],upstream_id=gid,url=base+'#/manuals-detail?guideId='+str(gid),evidence_id='E-ILD-01;'+guide_e[gid],review_status='text_read_model_conditioned',related_flow_ids=';'.join('F-M027-'+x for x in guide_f[gid].split(';')),checked_on=date))
assert len(inventory)==70 and len({r['entry_id'] for r in inventory})==70
writecsv(lib/'inventories/M027-help-inventory.csv',inventory,list(inventory[0]))
print(json.dumps(dict(new_evidence=len(records),new_flows=len(flows),help_entries=len(inventory),existing_flow_refined='F-M027-03'),ensure_ascii=False))
