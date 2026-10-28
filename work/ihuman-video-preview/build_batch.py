from pathlib import Path
import json,hashlib,shutil,csv,io,copy
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/ihuman-video-preview';A=L/'artifacts/ihuman-video-preview';B=L/'batches'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert not A.exists(),'one-time build'
data=json.loads((W/'video-inspection.json').read_text());assert not data.get('stopped');assert data['metadata']['duration']==86.122667
A.mkdir();frames=[]
for s in data['samples']:
 if s['seconds'] not in [30,50]:continue
 src=R/s['path'];assert sha(src)==s['sha256'];dst=A/src.name;shutil.copyfile(src,dst)
 raw=dst.read_bytes();frames.append(dict(path=str(dst.relative_to(L)),sha256=sha(dst),position_seconds=s['seconds'],actual_pixels=[int.from_bytes(raw[16:20],'big'),int.from_bytes(raw[20:24],'big')],observed_on='2026-10-27',method='official_promo_video_paused_seek_samples',product_hands_on=False,audio_observed=False))
shutil.copyfile(W/'video-inspection.json',A/'video-inspection.json')
note='''# 洪恩官网视频：能支持什么、不能支持什么

2026-10-27，E-IHVP-01。root在既有配置浏览器中作一次补核，成功读取官网HTML所列公开视频。帮助代理此前因默认浏览器文件缺失在联网前失败，不是该媒体拒绝访问；root没有重试此前412/验证码的两篇作者网页。

来源页[洪恩识字](https://www.ihuman.com/shizi/)，精确[介绍视频](https://h5static1.ihumand.com/website/pc/cn/shizi/assets/media/cn_video.198bceb2.mp4)。Chromium156.0.8078.4，原视频1920×1080、86.122667秒，匿名媒体响应206；以研究页面video元素正常流式读取，没有存MP4或听音。请求10/30/50/70秒，等待seek完成后截1280×720画面，root逐一查看。播放器左上投屏图标不是产品界面。脚本没有保存逐帧精确currentTime，只保存请求秒数和等待容差0.2秒；元数据见[记录](video-inspection.json)。

30秒是练习片段的宣传拼接，可见旧“玩认练写”四标签、返回/暂停、下部选择字及翻页控件；没有操作输入、答错或正确回执。[30秒画面](030.00.png)。50秒展示多个成语故事/互动缩略画面，不能判断它们如何解锁、进度如何保存。[50秒画面](050.00.png)。10秒为字词卡宣传，70秒为荣誉介绍，不支持交互，留work不入正式图集。视频没有App构建或发布日期；荣誉年份也不能当版本。

**F-M029-01仍D1，0新F。** 视频四标签与当前商店五环节说明分开，不能拼成现行完整任务。官网提供视频仍不代表具备选课→错答→提示→重试→结算→复习/退出恢复全链。取得视频只减少了“媒体完全未看”的缺口，没有证明当前教学规则或学习成效。

这是宣传来源的边界记录，供后续研究排除重复尝试；不能作为完成交互深挖的依据。未修改用户需求。
'''
N=A/'source-notes.md';N.write_text(note)
e=dict(evidence_id='E-IHVP-01',product_id='M-029',title='洪恩官网介绍视频：四个抽样位置与交互证据边界',url=data['url'],checked_on='2026-10-27',source_type='official_promotional_video_sampled_frames',access_mode='anonymous_browser_media_stream_seek_and_view',version_scope='发布日期/构建未知；片段四标签不覆盖当前五环节',app_build=None,source_updated_on=None,locator='10/30/50/70 seconds sampled; formal frames30/50',note='营销剪辑，只支持局部静态控件存在；0新流程、F01仍D1；未听音或操作产品',hands_on=False,visual_observed=True,visual_kind='sampled_video_frames',archived_artifact=str(N.relative_to(L)),sha256=sha(N),observed_artifacts=frames,artifact_hashes={x['path']:x['sha256'] for x in frames},derived_from='E-IHUM-01',supersedes=None,duration_seconds=data['metadata']['duration'],http_status=206)
existing=[json.loads(s) for p in B.glob('*-evidence.jsonl') for s in p.read_text().splitlines() if s.strip()];assert all(x['url']!=e['url'] and x['evidence_id']!=e['evidence_id'] for x in existing)
(B/'ihuman-video-preview-evidence.jsonl').write_text(json.dumps(e,ensure_ascii=False)+'\n')
cp=B/'children-coverage.csv';rs=list(csv.DictReader(io.StringIO(cp.read_text())));f=next(x for x in rs if x['flow_id']=='F-M029-01');before=copy.deepcopy(f)
f['evidence_ids']+=';E-IHVP-01';f['known_branches']+='；官网剪辑30s旧四标签与选择/暂停控件可见，未见答题反馈';f['gaps']+='；官网宣传视频不构成现行连续任务'
with cp.open('w',newline='') as h:
 w=csv.DictWriter(h,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
(W/'coverage-update-applied.json').write_text(json.dumps(dict(flow_id=f['flow_id'],before=before,after=f),ensure_ascii=False,indent=2)+'\n')
case=L/'cases/M-029-ihuman.md';(W/'case-before.md').write_bytes(case.read_bytes());case.write_text(case.read_text()+'''\n\n## 官网视频补核：仍未闭合教学链\n\nroot随后成功以既有浏览器配置抽样看了官网86.122667秒介绍视频，未听音或操作App。E-IHVP-01的[正式短摘与两帧](../artifacts/ihuman-video-preview/source-notes.md)记录30s的旧四环节/选择控件和50s成语缩略片段；不同于上节作者文字，且没有当前构建。F01仍D1，0新F；错答、提示、重试、奖励与退出恢复没有因取得宣传视频而补齐。代理原先的启动失败及其他网页的访问拒绝保持历史记录，没有据此重试受限来源。\n''')
print(dict(new_evidence=1,new_flows=0,formal_frames=len(frames),old_flow_additional_fields=3,depth=f['depth']))
