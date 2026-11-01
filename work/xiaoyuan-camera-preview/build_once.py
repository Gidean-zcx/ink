from pathlib import Path
import json,csv,hashlib,shutil,copy
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches';W=R/'work/xiaoyuan-camera-preview';A=L/'artifacts/xiaoyuan-camera-preview';assert not A.exists();A.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
d=json.loads((W/'root-capture-result.json').read_text());assert not d.get('stopped') and len(d['frames'])==8 and d['metadata']['duration']==14.5
selected=[]
for x in d['frames']:
 if x['requested_seconds'] not in [4,6,8,10,12]:continue
 src=R/x['path'];assert sha(src)==x['sha256'];dst=A/src.name;shutil.copyfile(src,dst);selected.append(dict(path=str(dst.relative_to(L)),sha256=sha(dst),actual_seconds=x['actual_seconds'],pixels=x['pixels'],observed_on='2026-10-27',method='paused_video_seek_sample'))
for n in ['source.json','capture-result.json','root-capture-result.json']:shutil.copyfile(W/n,A/n)
N=A/'source-notes.md';N.write_text('''# 小猿官网“智慧拍”：抽样画面与主链缺口

2026-10-27，E-XYCP-01，衍生于E-XYHW-01的已归档官方前端。来源页[S6条目](https://www.xiaoyuanyingjian.com/#/S6)，[媒体原址](https://solar-online.fbcontent.cn/megrez-online/megrez-alkaid/public/20240514/smart_camera_pc_1715653479252_8b72001-136b-1cc-bff8-c24c56f63bb.mp4)。文件名/路径含日期不当发布日期或固件。该前端中的S6范围沿原案例，不移植到后续同名产品或E1。

先前worker已取得206及14.5秒元数据，但在首个seek前因Playwright参数签名错误停止，无截图；失败不是媒体拒绝访问。root按既有配置修正后单次重新取证成功。Chromium156.0.8078.4，匿名静音正常video流，原2260×700；0/2/4/6/8/10/12/14秒逐点暂停截图1280×397，记录实际currentTime，root逐一查看。没有保存MP4、听音、连续完整观看或操作产品。左上投屏图标来自浏览器，不是小猿设备控件。

## 画面定位

| 时间 | 实际可见 | 不能据此推定 |
| --- | --- | --- |
| [4秒](04s.png)、[8秒](08s.png) | 智慧拍下的批改、视频答疑、上传文件、收录错题与作文入口；动画手指指向批改 | 所有入口均已执行、上传完成、错题自动保存 |
| [6秒](06s.png) | 纸面数学作业取景、拍照按钮及动画点击示意 | 真实相机延迟、识别准确率、数学判错或重拍流程 |
| [10秒](10s.png) | 手写作文图像页 | OCR文本是否可编辑、提交条件、拍摄失败处理 |
| [12秒](12s.png) | 作文图像与点评详情分栏；可见评价与建议栏目 | 评分有效、原文识别正确、已持久保存、可改写重评 |

0/14秒为空白过渡，2秒是硬件示意，三张仅留work。数学与作文是两个片段，不串成同一道题；并非真实手指操作的连续录屏。没有取得数学错误→提示→重试→完成或恢复链。

因此0新F；F-M056-01仍D1，只补相关拍照分支的静态观察，手写主链未升级。独立五帧与[成功记录](root-capture-result.json)、[初次脚本失败](capture-result.json)、[原前端定位](source.json)共同保留。得到宣传视频的价值是明确其可见范围及不能支持的结论，不能充当完整产品分析完成证据。
''')
e=dict(evidence_id='E-XYCP-01',product_id='M-056',title='S6官网智慧拍短片：批改入口与作文点评静态样本',url=d['url'],checked_on='2026-10-27',source_type='official_promotional_video_sampled_frames',access_mode='normal_browser_public_video_stream_seek_and_view',version_scope='archived S6 frontend route; exact hardware generation and firmware unknown',app_build=None,source_updated_on=None,locator='14.5 seconds; eight paused samples; formal4/6/8/10/12',note='动画演示分数学与作文片段；未连续看、未听音或操作产品；F01保持D1',hands_on=False,visual_observed=True,visual_kind='sampled_video_frames',archived_artifact=str(N.relative_to(L)),sha256=sha(N),derived_from='E-XYHW-01',supersedes=None,observed_artifacts=selected,artifact_hashes={x['path']:x['sha256'] for x in selected},duration_seconds=14.5,http_status=206)
all_e=[json.loads(s) for p in B.glob('*-evidence.jsonl') for s in p.read_text().splitlines() if s.strip()];assert all(x['url']!=e['url'] and x['evidence_id']!=e['evidence_id'] for x in all_e)
(B/'xiaoyuan-camera-preview-evidence.jsonl').write_text(json.dumps(e,ensure_ascii=False)+'\n')
changes=[]
for p in B.glob('*-coverage.csv'):
 rows=list(csv.DictReader(p.open()));modified=False
 for f in rows:
  if f['flow_id']=='F-M056-01':
   before=copy.deepcopy(f);f.update(evidence_ids=f['evidence_ids']+';E-XYCP-01',known_branches='原S6前端目录；智慧拍短片抽样见拍照批改入口/取景、另段作文点评；两片段不能串成同一题，非真实操作。',gaps='原选题手写链、数学判错/纠正/提示重试/完成及恢复仍缺；官网动画未证准确率/保存，不能套后续S6或E1。',visual_status='static_official_images_only');changes.append(dict(flow_id=f['flow_id'],before=before,after=f));modified=True
 if modified:
  with p.open('w',newline='') as h:w=csv.DictWriter(h,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
assert len(changes)==1
C=L/'cases/M-056-xiaoyuan-hardware.md';(W/'case-before.md').write_bytes(C.read_bytes());C.write_text(C.read_text()+'''\n\n## 官网智慧拍短片补核（2026-10-27）\n\nE-XYCP-01对应既有S6前端的14.5秒官方动画。root实际查看8个暂停时间点，正式保留[5帧、原地址及限制](../artifacts/xiaoyuan-camera-preview/source-notes.md)：拍照批改入口/数学取景，以及独立的作文图像/点评片段。没有连续操作、听音或数学错误重试；0新F，F-M056-01仍D1。此项只补拍照分支的静态范围，不能补齐上方手写任务表。\n\nworker首轮因调用参数错误停下，root修正后单次完成，失败和成功记录均保留；不是换路绕过远端限制。固件及准确代际未知，不套E1或后续同名S6。原资料中“未看视频”保留为历史状态，本次观察范围以独立摘记为准。\n''')
(W/'coverage-update-applied.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2)+'\n')
(W/'manifest.json').write_text(json.dumps({str(p.relative_to(R)):sha(p) for p in [*A.iterdir(),C,B/'xiaoyuan-camera-preview-evidence.jsonl']},indent=2)+'\n')
print(dict(new_e=1,new_f=0,old_f=1,old_fields=4,formal_frames=5,all_samples_viewed=8))
