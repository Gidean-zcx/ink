from pathlib import Path
import json,csv,hashlib
b=Path('docs/research/interaction-library');rows=[]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def e(n,title,url,kind,mode,scope,loc,raw=None,artifact=None,visual=False,note='',date=None):
 r=dict(evidence_id=f'E-XYHW-{n:02d}',product_id='M-056',title=title,url=url,checked_on='2026-10-26',source_type=kind,access_mode=mode,version_scope=scope,app_build=None,firmware_version=None,source_updated_on=date,locator=loc,note=note,hands_on=False,visual_observed=visual,archived_artifact=artifact,sha256=sha(b/artifact)if artifact else None,capture_state='official_static_excerpt_archived'if artifact else 'bibliography_short_note_and_temporary_raw'if raw else 'bibliography_and_short_note_only',supersedes=None)
 if raw:r.update(raw_work_path=raw,raw_sha256=sha(raw))
 if visual:r['visual_scope']='Static official images only; no continuous operation or hands-on'
 rows.append(r);return r
r=e(1,'小猿学练机官网：S6/E1 文案、规格和路由','https://www.xiaoyuanyingjian.com/','official_product_frontend','public_frontend_text_inspection','China mainland S6/E1; current website, firmware unknown','Te specs; S6 exercise modules; Rn upload; /S2 and /S6 same component','work/learning-hardware/official-main.js',note='Web extraction returned JS shell. Public referenced JS downloaded and Unicode text read; no browser/device task. AI learning device policy in same bundle excluded from ink-device claims.')
r['retrieval_request']='GET https://www.xiaoyuanyingjian.com/static/js/main.12d356ac.chunk.js';r['route_scope']=['#/S6','#/E1','#/specs?type=E1']
r=e(2,'小猿智能设备 App Store：家长端及四张示意截图','https://apps.apple.com/cn/app/%E5%B0%8F%E7%8C%BF%E6%99%BA%E8%83%BD%E8%AE%BE%E5%A4%87/id6738277050','official_app_store','opened_text_and_static_images','China iPhone listing 1.25.1; installed version and paired hardware unknown','Developer description; duration, answer controls, family list, learning trajectory','work/learning-hardware/parent-store.html','artifacts/learning-hardware/parent-control.webp',True,'Four static developer compositions viewed; only one retained formally. Sample values are not defaults. Store 1.25.1 date displayed Sep 25 without year on row; no user reviews used.')
r['source_images']=json.loads(Path('work/learning-hardware/parent-shots.json').read_text());r['raw_image_hashes']={x['path']:sha(x['path'])for x in r['source_images']}
r=e(3,'E1 官网课程分龄表及功能目录静态图','https://www.xiaoyuanyingjian.com/static/media/system-pc.b3ce827d.png','official_product_diagram','downloaded_static_image_viewed','E1 curriculum levels S1/S2/S3 for ages 3–6; no installed build','Rows Chinese/English/thinking; S3 column 5–6 years','work/learning-hardware/e1-system.png','artifacts/learning-hardware/e1-system.png',True,'S1/S2/S3 here are curriculum levels, not hardware models. Also inspected first-pc directory image; only level table archived.')
r['additional_source_urls']=['https://www.xiaoyuanyingjian.com/static/media/first-pc.1490d1ab.png'];r['additional_raw_files']=[{'path':'work/learning-hardware/e1-age.png','sha256':sha('work/learning-hardware/e1-age.png')}]
e(4,'E1 官网防挫折算法研发反馈示意','https://www.xiaoyuanyingjian.com/static/media/algorithm-lg.7d0a0988.png','official_design_diagram','downloaded_static_image_viewed','E1 website diagram; algorithm version unknown','Teaching goals/content/interaction/incentives; practice count/time/exit/error/next-retention','work/learning-hardware/e1-antifrustration.png','artifacts/learning-hardware/e1-antifrustration.png',True,'Development loop and aggregate metrics, not an observed per-child adaptive task or learning-effect study.')
r=e(5,'小猿智能设备家长端隐私政策','https://megrez.yuanfudao.com/megrez-parent-web/privacys/privacy','official_policy','downloaded_public_html_text','Parent software policy, not S6/E1 student firmware manual','II basic functions; VI account rights','work/learning-hardware/parent-privacy-body.html',date='2025-08-15',note='Linked HTML body declares publication and effective date 2025-08-15. Applies to parent management service; supports files/printing capability but no complete transfer path.')
r['retrieval_request']='GET https://solar-online.fbcontent.cn/megrez-online/megrez-gallery/megrez-resource/1a017eac33f2fb4.html'
e(6,'小猿官方旗舰店系列分类','https://mall.jd.com/view_search-2998983-51133548-99-1-20-1.html','official_storefront','opened_text','China mainland storefront; availability not tested','Series categories S6/S2/E1/S1',note='Model disambiguation only; not workflow evidence or pricing advice.')
with (b/'batches/learning-hardware-evidence.jsonl').open('w')as f:
 for r in rows:f.write(json.dumps(r,ensure_ascii=False)+'\n')
fs=[]
for num,task,depth,ids,branches,gaps,vs in [
 (1,'S6 一题选题手写纠错再练结束','D1','E-XYHW-01','官方能力模块目录','题目实际入口提示重试与恢复','not_observed'),
 (2,'E1 识字与分级阅读','D1','E-XYHW-01;E-XYHW-03;E-XYHW-04','课程分龄S3为5至6岁','选级提示纠错重试结束恢复','static_official_diagram'),
 (3,'家长时间及答案限制','D1','E-XYHW-02;E-XYHW-05','静态管控选项及开关示例','完整入口保存生效超时解锁离线','static_official_screenshot'),
 (4,'轨迹查看与家庭协作','D1','E-XYHW-02;E-XYHW-05','示例时间线及家长列表','数据口径角色权限邀请恢复','static_official_screenshot'),
 (5,'导入文档及发送打印','D1','E-XYHW-01;E-XYHW-05','小程序输入及Word图片PDF','E1适用性文件限制同步打开打印恢复','not_observed'),
 (6,'家长账号注销','D2','E-XYHW-05','确认家庭解绑条件与短信验证码','错误验证码及重新绑定儿童进度','not_observed')]:
 fs.append(dict(flow_id=f'F-M056-{num:02d}',product_id='M-056',task=task,depth=depth,evidence_ids=ids,known_branches=branches,gaps=gaps,visual_status=vs,hands_on='false',case_path='cases/M-056-xiaoyuan-hardware.md',checked_on='2026-10-26'))
with (b/'batches/learning-hardware-coverage.csv').open('w',newline='')as f:
 w=csv.DictWriter(f,fieldnames=list(fs[0]));w.writeheader();w.writerows(fs)
print('wrote',len(rows),'evidence and',len(fs),'flows')
