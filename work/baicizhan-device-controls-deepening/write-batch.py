from pathlib import Path
import json,csv
base=Path('work/baicizhan-device-controls-deepening'); lib=Path('docs/research/interaction-library'); batch=lib/'batches'
raw=json.loads((base/'fetch-results.json').read_text())[1]
e={'evidence_id':'E-BDCD-01','product_id':'M-022','title':'BCZ-Ace-Max 原始拆解的实体键描述（非官方手册）','url':raw['url'],'checked_on':'2026-10-27','source_type':'primary_independent_teardown','access_mode':'opened_text','version_scope':'Wen5，2025-05-23，BCZ-Ace-Max 样机；不外推 Mini、彩屏 Max 或现行全部 Max SKU','app_build':None,'firmware_version':None,'source_published_on':'2025-05-23','source_updated_on':None,'locator':'一、百词斩单词机Ace-Max开箱；方向键、发音/返回键、圆形确认键、顶部斩词键相邻正文；原HTML wp-image-239577/239564/239590/239574附近','note':'仅采用作者对实体键用途的描述；未见操作前后状态，不补学习/复习/同步闭环，沿用 F-M022-02，D1 不变。','hands_on':False,'visual_observed':False,'visual_audit':None,'archived_artifact':None,'sha256':None,'capture_state':'raw_html_in_work; text_only; requested_images_403_not_observed','raw_work_path':raw['path'],'raw_sha256':raw['sha256'],'http_status':raw['status'],'supersedes':None,'version_warning':'作者日期不是固件日期；没有原厂确认、按键状态作用、误斩撤销或同步规则；芯片/麦克风存在不作软件能力证据。','failed_image_requests':[{'url':r['url'],'http_status':r['status'],'locator':r['locator'],'result':r['result']} for r in json.loads((base/'image-fetch-results.json').read_text())]}
(batch/'baicizhan-device-controls-deepening-evidence.jsonl').write_text(json.dumps(e,ensure_ascii=False)+'\n')
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
with (batch/'baicizhan-device-controls-deepening-coverage.csv').open('w',newline='') as f:csv.DictWriter(f,fieldnames=fields,lineterminator='\n').writeheader()
before={r['flow_id']:r for r in json.loads((base/'existing-coverage-before.json').read_text())}; f=before['F-M022-02']
changes={'evidence_ids':f['evidence_ids']+';E-BDCD-01','known_branches':'官方图六入口及分页；2025 BCZ-Ace-Max 独立拆解正文补方向/确认/发音/返回键与单击斩词描述，无动作后状态','gaps':'官方按键规则、长短按及状态作用、进入/答错/误斩撤销/结束、第二页、复习与同步影响未知；固件和 App 构建未知','checked_on':'2026-10-27'}
with (base/'existing-coverage-updates.csv').open('w',newline='') as out:
 w=csv.DictWriter(out,fieldnames=['flow_id','field','before','after','reason'],lineterminator='\n');w.writeheader()
 for field,value in changes.items():w.writerow({'flow_id':f['flow_id'],'field':field,'before':f[field],'after':value,'reason':'同任务补一个非官方原作者正文；D1、hands_on=false、既有静态官方图状态不变；root复核后更新所属旧批次'})
print('Wrote 1 E, 0 new F, 4 field proposals for F-M022-02; master/old batches untouched.')
