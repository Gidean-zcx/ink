from pathlib import Path
import copy
import csv
import hashlib
import json
import shutil

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
W = R / 'work/research/batch31-closeout'
assert not (W / 'applied.json').exists(), 'one-time builder already applied'
(W / 'before').mkdir()
def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def rel(p):
    return str(p.relative_to(L))
def js(p, value):
    p.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')

H = R / 'work/hanvon-voice-controls-deepening'
HA = L / 'artifacts/hanvon-voice-controls-deepening'
V = R / 'work/bcz-learning-video-preview'
VA = L / 'artifacts/bcz-learning-video-preview'
image_records = json.loads((H / 'image-captures.json').read_text()) + json.loads((H / 'image-captures-supplement.json').read_text())
by_name = {Path(x['path']).name: x for x in image_records}
hanvon_images = [['sina-18.jpg', 'sina-19.jpg', 'sina-21.jpg', 'sina-26.jpg'], ['pconline-14.png', 'pconline-17.png']]
hanvon_artifacts = []
for group in hanvon_images:
    assets = {}
    for name in group:
        row = by_name[name]
        source = R / row['path']
        assert digest(source) == row['sha256'] and row['status'] == 200
        target = HA / Path(name).with_suffix('.jpg')
        shutil.copyfile(source, target)
        assets[rel(target)] = digest(target)
    hanvon_artifacts.append(assets)
for fn in ['image-captures.json', 'image-captures-supplement.json', 'captures.json']:
    shutil.copyfile(H / fn, HA / fn)
frames = ['000.00', '015.00', '025.00', '030.00', '034.00', '045.00', '060.00', '077.41']
video_assets = {}
for t in frames:
    target = VA / (t + 's.png')
    shutil.copyfile(V / 'frames' / target.name, target)
    video_assets[rel(target)] = digest(target)
for fn in ['root-capture-result.json', 'supplement-failure.json']:
    shutil.copyfile(V / fn, VA / fn)
    video_assets[rel(VA / fn)] = digest(VA / fn)

def evidence(eid, pid, title, url, source_type, access_mode, scope, date, locator, note, artifact, assets, raw=None):
    return dict(evidence_id=eid, product_id=pid, title=title, url=url, checked_on='2026-10-27', source_type=source_type, access_mode=access_mode, version_scope=scope, app_build=None, firmware_version=None, source_updated_on=date, locator=locator, note=note, hands_on=False, visual_observed=True, visual_kind='static_third_party_reference', archived_artifact=rel(artifact), sha256=digest(artifact), artifact_hashes=assets, raw_work_path=str(raw.relative_to(R)) if raw else None, raw_sha256=digest(raw) if raw else None, supersedes=None)

es = [
    evidence('E-HVVC-01','M-008','N10 Pro二代：转写/离线/语稿的作者静态画面','https://zhongce.sina.com.cn/article/view/192508','firsthand_reviewer_report_with_static_photos','public_HTML_GET_and_static_image_view','N10 Pro二代，作者体验2025；与X3 Pro对比图按左右/标注分开，固件未知','2025-11-04','语音转写、会议纪要；原HTML图片18/19/21/26右半','仅静态控件；排除讯飞画面、性能评价及未观看GIF；原F03仍D1。',HA/'source-notes.md',hanvon_artifacts[0],H/'raw/sina-comparison.html'),
    evidence('E-HVVC-02','M-008','N10 Pro二代：等待连接与纪要结果控件','https://best.pconline.com.cn/yuanchuang/31097518.html','firsthand_reviewer_report_with_static_photos','public_HTML_GET_and_static_image_view','N10 Pro二代；作者2025-05-20，设备图示日期非固件，构建未知','2025-05-20','语音转写段；原HTML图片14/17','计时/连接提示与纪要结果不同画面，不拼同一次成功链；重新生成/分享仅入口。',HA/'source-notes.md',hanvon_artifacts[1],H/'raw/pconline-voice.html'),
    evidence('E-BCLV-01','M-022','官方2022教程：计划、设备同步与词卡抽样','https://7n.bczcdn.com/editer_uploads/media/202211/75f293134700.mp4','official_tutorial_video_sampled_frames','anonymous_browser_media_stream_seek_and_view','教程页2022-11-08；设备型号与App/固件未知，不映射当前Mini/Max',None,'首轮0/15/30/45/60/77.41秒；补采目标25/34秒的actualTime未持久记录','静音离散抽样，非连续或产品操作；补采进程exit143保留失败，确认/同步完成回执未抽到。',VA/'source-notes.md',video_assets)
]
es[-1]['visual_kind'] = 'sampled_video_frames'
es[-1]['derived_from'] = 'E-BCDVC-01'
es[-1]['source_published_on'] = '2022-11-08 (embedding tutorial page only)'
existing = [json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()]
assert not {e['evidence_id'] for e in es} & {e['evidence_id'] for e in existing}
assert not {e['url'] for e in es} & {e['url'] for e in existing}
new_batch = L/'batches/voice-and-learning-state-evidence.jsonl'
assert not new_batch.exists()
new_batch.write_text(''.join(json.dumps(e, ensure_ascii=False)+'\n' for e in es))
fields = list(next(csv.DictReader((L/'coverage.csv').open())))
with (L/'batches/voice-and-learning-state-coverage.csv').open('w', newline='') as f:
    csv.DictWriter(f, fieldnames=fields, lineterminator='\n').writeheader()

patches = {
    'F-M008-03': dict(evidence_ids='E-HANV-03;E-HVCD-01;E-HVCD-02;E-HVVC-01;E-HVVC-02',known_branches='作者报告入口/离线资源条件；静态图补语言、分角色、离线/转在线、暂停/结束、连接等待及纪要重新生成/分享入口；未执行。',gaps='离线资源下载/取消、说话人纠正、结束/锁屏保存、AI失败重试、重生成覆盖、原音定位及分享导出结果仍缺；不能由图推断纪要离线。',visual_status='static_reference_only'),
    'F-M022-03': dict(evidence_ids='E-BCZH-01;E-BCDVC-01;E-BCDVC-02;E-BDCQ-01;E-BCLV-01',known_branches='Mini/Max仍宣传D1；2022型号未知文档与视频抽样补手机计划、设备锁屏/首页、同步0%、词书类别及末段词卡，未抽到同步确认/完成回执。',gaps='历史型号/构建、绑定、同步对象/冲突、取消/断网及恢复未知；末段词卡不证明传入词本一致或离线资源完整；dcj2仍不适用。',visual_status='static_official_images_only')
}
changes = []
for p in (L/'batches').glob('*-coverage.csv'):
    rows = list(csv.DictReader(p.open()))
    changed = False
    for row in rows:
        if row['flow_id'] in patches:
            before = copy.deepcopy(row)
            row.update(patches[row['flow_id']])
            assert before['depth'] == row['depth'] == 'D1'
            changes.append(dict(flow_id=row['flow_id'],before=before,after=copy.deepcopy(row)))
            changed = True
    if changed:
        shutil.copyfile(p,W/'before'/p.name)
        with p.open('w',newline='') as f:
            writer=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');writer.writeheader();writer.writerows(rows)
assert len(changes)==2
js(W/'coverage-updates-applied.json',changes)
captures=[]
for p in (L/'batches').glob('*-evidence.jsonl'):
    rows=[json.loads(x) for x in p.read_text().splitlines() if x.strip()]
    changed=False
    for e in rows:
        if e['evidence_id']=='E-BCDVC-01':
            before=copy.deepcopy(e.get('additional_captures',[]))
            cap=dict(checked_on='2026-10-27',access_mode='anonymous_public_HTML_GET',http_status=200,raw_work_path='work/bcz-video-locator/article-892.html',raw_sha256=digest(R/'work/bcz-video-locator/article-892.html'),locator='HTML 1078/1082/1088; title/date/video source',note='原页仍未标具体型号。媒体另记E-BCLV-01；最初未观看描述保留为原采集历史。',source_notes_path=rel(VA/'source-notes.md'),source_notes_sha256=digest(VA/'source-notes.md'))
            e['additional_captures']=before+[cap];changed=True;captures.append(dict(evidence_id=e['evidence_id'],before=before,after=copy.deepcopy(e['additional_captures'])))
    if changed:
        shutil.copyfile(p,W/'before'/p.name);p.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in rows))
assert len(captures)==1
js(W/'evidence-updates-applied.json',captures)
js(W/'applied.json',dict(new_evidence=3,new_flows=0,old_flows=2,old_fields=8,old_evidence_captures=1,formal_images=14))
print('APPLIED: 3E, 0F, 2 old F / 8 fields, 1 old E capture, 14 formal images')
