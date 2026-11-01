from pathlib import Path
import json,csv,hashlib
root=Path('/home/workspace/reader')
base=root/'docs/research/interaction-library'
work=root/'work/youdao-device-deepening'
case=base/'cases/M-025-youdao-pen.md'
casehash=hashlib.sha256(case.read_bytes()).hexdigest()
imageurls=dict(json.loads((work/'gdufs-image-urls.json').read_text()))
def record(n,title,url,kind,mode,version,locator,note):
 return dict(evidence_id=f'E-YDCD-{n:02d}',product_id='M-025',title=title,url=url,checked_on='2026-10-26',source_type=kind,access_mode=mode,version_scope=version,app_build=None,firmware_version=None,source_updated_on=None,locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
rows=[
 record(1,'有道技术团队：有道词典笔的高效测试探索','https://techblog.youdao.com/?p=1725','official_technical_article','opened_text','Mainland Youdao Dictionary Pen 2.0; article published 2021-03-30 and describes first version; firmware unknown','一、背景：硬件及典型用户场景；二、让硬件动起来：用户移动方向；六、总结','原厂文字补按压、滑扫、识别/翻译/语音合成阶段；未观看所嵌演示，厂内自动化不作用户可用能力。'),
 record(2,'广外图书馆：有道词典笔2.0开放使用','https://lib.gdufs.edu.cn/info/1133/6279.htm','institutional_deployment_notice','opened_text_and_downloaded_html','GDUFS mainland ordinary Youdao Dictionary Pen 2.0 loan stock; 2021-12-02; hardware code/firmware unknown','型号与功能段；文末激活方式/使用引导的原图链接','机构实际借用服务一手资料，非原厂帮助。网页将附图与普通2.0关联；照片没有型号编码和版次，不外推到其他系列。'),
 record(3,'广外2.0借用公告所附纸质激活说明照片',imageurls[4],'institutional_published_instruction_photo','static_image_observed','Instructions accompanying GDUFS 2021-12-02 ordinary 2.0 loan announcement; leaflet edition and firmware unknown','文章内容区第4图，紧邻“激活方式”；四步纸质说明','首次联网激活、失败重配网、成功进入首页。仅静态纸质说明；收藏图标不证明收藏或练习成功。原厂出版来源未核定。'),
 record(4,'广外2.0借用公告所附纸质使用引导照片',imageurls[5],'institutional_published_instruction_photo','static_image_observed','Instructions accompanying GDUFS 2021-12-02 ordinary 2.0 loan announcement; leaflet edition and firmware unknown','文章内容区第5图，紧邻“使用引导”；握笔和扫描姿态图','可辨认匀速直线、笔头贴纸提示；细小不清文字不转录。未显示识别/纠错/收藏结果，非连续操作。'),
 record(5,'北外图书馆：有道词典笔免费借用服务','https://lib.bfsu.edu.cn/info/73649.jspx','institutional_deployment_notice','opened_text','BFSU mainland ordinary Youdao Dictionary Pen 2.0 loan stock; published 2019-12-10; firmware unknown','正文前两段：型号及离线/联网功能边界','历史一手部署公告旁证离线查询、翻译和发音，以及联网语音助手；不是原厂现行功能清单或实测。')
]
for n,imgn in [(3,4),(4,5)]:
 r=rows[n-1]
 p=work/f'gdufs-image-{imgn}.png'
 r.update(visual_observed=True,capture_state='durable_visual_audit_original_temporary',raw_work_path=str(p.relative_to(root)),raw_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),visual_audit_path='cases/M-025-youdao-pen.md',visual_audit_sha256=casehash,observed_on='2026-10-26',derived_from='E-YDCD-02')
(base/'batches/youdao-device-deepening-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
new=dict(zip(fields,['F-M025-05','M-025','历史普通2.0首次联网激活','D2','E-YDCD-02;E-YDCD-03','首次联网；失败重新配网；成功进入首页；不必每次重激活','激活中断/超时/停服；账号依赖；重置后行为；固件未知','static_reference_only','false','cases/M-025-youdao-pen.md','2026-10-26']))
with (base/'batches/youdao-device-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields,lineterminator="\n");w.writeheader();w.writerow(new)
old=list(csv.DictReader((base/'batches/dictionary-coverage.csv').open()))
updated=next(x.copy() for x in old if x['flow_id']=='F-M025-01')
updated.update(depth='D2',evidence_ids='E-YDP-01;E-YDCD-01;E-YDCD-04',known_branches='按压笔头启动采集；向右匀速直线滑扫；识别/翻译显示与语音合成；多行仅声明',gaps='进入扫描前页面；抬笔/取消及返回；错词校订；词典切换；多行追加；收藏练习；固件未知',visual_status='static_reference_only')
with (work/'existing-coverage-updates.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields,lineterminator="\n");w.writeheader();w.writerow(updated)
(work/'commands-and-limits.md').write_text('''# M025本轮命令与边界\n\n日期2026-10-26；只作公开文档研究。\n\n- 读取：`cat AGENTS.md README.md docs/requirements.md docs/research/interaction-library/template.md docs/research/interaction-library/cases/M-025-youdao-pen.md`，exit 0。\n- 原证据和编号：`rg E-YDP-`及`rg ^F-M025`定位dictionary旧批次；`rg E-YDCD docs/research/interaction-library/batches`在写入前exit 1，表示此前无此编号。\n- 公开资料：web.search/open查看有道技术博客和两校借用公告；官方域说明书/收藏/使用引导末轮检索无结果。广外web.open后续一次超时；已有HTML请求200，未再重复访问正文。\n- 原图：`timeout 45s python3 -u -`中的ThreadPoolExecutor(max_workers=3)匿名GET广外正文链接1/4/5，工具进程session 19528最终exit 0；三图均HTTP 200 image/png。原始URL见gdufs-image-urls.json，图片字节与SHA见批次和case审计。\n- 静态观察：view_image检查1/4/5图，另对5图用original；不声称观看视频，不进行图像增强或补写细小文字。\n- 写入：`python3 work/youdao-device-deepening/write-batch.py`。\n- 校验：`python3 work/youdao-device-deepening/check-batch.py`及限定三个正式文件的`git diff --check -- ...`。\n- 未登录、购买、绑定、安装或调用用户接口；没有固件/账户状态改变。图片只暂存work，正式图像审计位于case内。\n\n限制：未取得普通2.0完整原厂手册、固件版本及校订/收藏/复习操作链；机构说明照片不是原厂网站手册，也不是连续操作观察。\n''')
print('wrote 5 evidence, 1 new flow, 1 proposed existing coverage row; masters and old batches not written')
