from pathlib import Path
import csv, hashlib, json, shutil

root = Path('/home/workspace/reader')
lib = root / 'docs/research/interaction-library'
work = root / 'work/chinese-vocab-deepening'
art = lib / 'artifacts/chinese-vocab-deepening'
art.mkdir(exist_ok=True)

images = [
 ('03','23e8d48f57cc.png','baicizhan-2022-book-selection.png','https://7n.bczcdn.com/editer_uploads/image/202211/23e8d48f57cc.png','2022-11-21 核心词指南，步骤1配图','教学合成图：左侧首页修改，中间更换词书/添加词书，右侧大学考研目录；目标书被圈出，未展示确认后的状态。'),
 ('04','a1acd0595d23.png','baicizhan-2022-wordlist.png','https://7n.bczcdn.com/editer_uploads/image/202211/a1acd0595d23.png','2022-11-21 核心词指南，步骤2配图','教学合成图：由书封至词表；已学/未学/已斩分类可见；右滑的箭头与斩/星形动作可见。没有点击斩后的连续画面或撤销。'),
 ('05','4e4eb0b9f67f.png','baicizhan-2022-context-collection.png','https://7n.bczcdn.com/editer_uploads/image/202211/4e4eb0b9f67f.png','2022-11-21 核心词指南，真题查词举例配图','教学合成图：原文选词及语境义、顶部词条栏；箭头指向本文标记词表及一键收藏；没有执行收藏后的反馈。'),
 ('08','a34b156a15d9.jpeg','baicizhan-2023-spelling-state.jpeg','https://ws.bczcdn.com/editer_uploads/image/202303/a34b156a15d9.jpeg','2023-03-23 提分攻略，背单词步骤3配图','裁切静态题面：新学/复习计数、返回图标、图像与中文提示、句内空格可见；键盘、提交、判错或完成状态不在画面中。'),
 ('09','dc1e38147cad.jpeg','baicizhan-2023-audio-state.jpeg','https://ws.bczcdn.com/editer_uploads/image/202303/dc1e38147cad.jpeg','2023-03-23 提分攻略，背单词步骤4配图','裁切静态界面：待过计数、返回图标、朗读句子/循环播放开关、词与音标词义及音频图标。未听到音频，也未操作任何开关。'),
]
for _,source,target,*_ in images:
 shutil.copyfile(work/source, art/target)

audit = '''# 百词斩官方历史静态图观察

查阅、取得与查看日期：2026-10-26。来源为百词斩官方文章内配图；文章及图中未给具体 App 平台或构建。通过公开 HTTP GET 取得必要配图，再用 `view_image` 实际查看。仅五张与交互有关的配图归档，未下载整篇图集或视频；这些是官方教学合成图/单帧界面，不能证明实际控件响应、连续过程、音频输出或学习效果。`hands_on=false`；静态观察不升 D3。

| E | 文章内定位 | 原始 URL | 本地图 / SHA-256 | 实际看到与未看到 |
| --- | --- | --- | --- | --- |
'''
for n,source,target,url,loc,obs in images:
 digest=hashlib.sha256((art/target).read_bytes()).hexdigest()
 audit += f'| E-BCD-{n} | {loc} | [原图]({url}) | [{target}]({target})；`{digest}` | {obs} |\n'
audit += '\n图片只是各文章的附件，不计为五个独立研究来源。有关操作结果仅在正文明确说明时写入案例，其余保留未知。\n'
(art/'static-observation-log.md').write_text(audit)
audit_hash=hashlib.sha256((art/'static-observation-log.md').read_bytes()).hexdigest()

evidence=[]
def add(eid,pid,title,url,scope,locator,note,source_type='official_help',access='opened_text',build=None,updated=None):
 row=dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type=source_type,access_mode=access,version_scope=scope,app_build=build,firmware_version=None,source_updated_on=updated,locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='not_archived',supersedes=None)
 evidence.append(row)
 return row

add('E-BCD-01','M-016','深度模式下线公告及拼写开关历史路径','https://www.baicizhan.com/little_class/article/915/share_view','2023-02 historical App; platform/build unspecified','发布日期2023-02-21；对我有什么影响/不希望取消深度模式两节','明确当时模式迁移及开启拼写设置；与当前官网同名模式宣传存在未解版本边界，不能证明现版已下线。','official_update')
add('E-BCD-02','M-016','核心词复习指南：历史选书、词表和真题查词','https://www.baicizhan.com/little_class/article/894/share_view','2022-11 historical App; 考研2023核心版; platform/build unspecified','发布日期2022-11-21；具体操作1～3；真题查词段','正文补充历史任务；三张必要静态配图另列E-BCD-03/04/05，不把文章建议的复习次数写成自动队列规则。','official_tutorial')
add('E-BCD-06','M-016','单词本使用教程的App设置计划段','https://www.baicizhan.com/little_class/article/892/share_view','2022-11 companion App; platform/build unspecified','发布日期2022-11-08；首先在百词斩App进行配置段','只取已有单词本设置计划文本；未观看内嵌视频，也不将后续硬件同步套用于所有App。','official_tutorial')
add('E-BCD-07','M-016','提分攻略中的拼写及单词速听片段','https://www.baicizhan.com/little_class/article/925/share_view','2023-03 historical App; platform/build unspecified','发布日期2023-03-23；背单词步骤3/4；页尾标来源为百词斩官方公众号','仅用题型与音频相关段；笼统拼错重现描述无逐题分支，不采用记忆效果承诺或练习次数建议为系统默认。','official_tutorial')
for n,source,target,url,loc,obs in images:
 row=add('E-BCD-'+n,'M-016','百词斩历史静态图：'+target,url,'Historical App illustration; platform/build unspecified',loc,obs,'official_tutorial_image','static_image_observed')
 row.update(visual_observed=True,archived_artifact='artifacts/chinese-vocab-deepening/'+target,sha256=hashlib.sha256((art/target).read_bytes()).hexdigest(),capture_state='selected_static_image',visual_audit_path='artifacts/chinese-vocab-deepening/static-observation-log.md',visual_audit_sha256=audit_hash)

add('E-BBD-01','M-018','不背单词中国区iOS开发者商店与历史日志','https://apps.apple.com/cn/app/不背单词-四六级考研等英语单词学习/id698570469?platform=mac','China App Store; iPhone/iPad; 5.11.6 and historical 5.9.4+ logs','产品描述帮助路径；版本历史5.9.4/5.9.5/5.9.8/5.9.11/5.9.15/5.11/5.11.3/5.11.4/5.11.6','正文显示5.11.6，日期为9月18日（该项未显年份）；逐段按描述后的版本标签配对。5.11.4自制书条目限单词机；Mac参数不证明原生Mac流程。','official_app_store',build='5.11.6')
add('E-BBD-02','M-018','魅族官方商店不背单词Android条目','https://app.meizu.com/apps/public/detail?package_name=cn.com.langeasy.LangEasyLexis','China Flyme store Android listing; published build not installed','开发者北京艾斯酷科技有限公司；版本/更新时间；应用简介；版本描述','打开正文为5.11.7及例句显示修复，优先于搜索索引旧5.11.4；仅描述功能，不证明播放或离线体验；未下载APK。','official_app_store',build='5.11.7',updated='2026-10-15')
add('E-BBD-03','M-018','自制词书公开入口重定向复查','https://www.bbdc.cn/lexis_book_index','Public web; unauthenticated; App build not specified','最终URL /lexis/login?redirectUrl=http://www.bbdc.cn/lexis_book_index；登录表单和App账号提示','仅打开页面；未填表、登录或发送验证码。页面可见身份选项不能证明导入字段、校订或账号恢复链。','official_web_entry')
add('E-BBD-04','M-018','官网公告入口：仅取得脚本壳','https://learnywhere.cn/bb/production/notification','Public notification site; version unspecified','官网/index链接；HTTP200，976字节HTML，仅noscript及静态脚本链接','公告正文未取得。其公开app.8a73937b.js脚本一次HTTP请求403后停止，没有通过脚本推断用户流程。','official_help_entry','http_shell_only')
add('E-BBD-05','M-018','官网所链知乎官方主页：访问受限','https://www.zhihu.com/org/bu-bei-dan-ci-app/activities','Official-linked profile; version unspecified','官网/index中的知乎链接；HTTP GET 403','只记录来源定位与本轮访问失败；未读取官方文章，不从第三方教程拼出步骤。','official_social_entry','http_403')

evidence.sort(key=lambda r:(r['product_id'],r['evidence_id']))
batch=lib/'batches'
(batch/'chinese-vocab-deepening-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in evidence))

fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
newrows=[
 ['F-M016-05','M-016','开启额外拼写题型（2023历史局部）','D2','E-BCD-01','深度模式历史迁移；轻快用户不受影响','构建未知；开关保存/即时生效/现版入口未核实','not_observed','false','cases/M-016-baicizhan.md','2026-10-26'],
 ['F-M016-06','M-016','真题查词后收藏（2022历史局部）','D2','E-BCD-02;E-BCD-05','文中义与本文标记词表；批量收藏','App总入口、空表/重复收藏、原句保留、成功反馈与复习启动未知','static_reference_only','false','cases/M-016-baicizhan.md','2026-10-26'],
]
with (batch/'chinese-vocab-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(fields);w.writerows(newrows)

old=list(csv.DictReader((batch/'vocab-coverage.csv').open()))
changes={
 'F-M016-01':dict(evidence_ids='E-BCZ-01;E-BCZ-02;E-BCZ-03;E-BCD-02;E-BCD-03;E-BCD-04;E-BCD-06;E-BCD-07;E-BCD-08;E-BCD-09',known_branches='历史选书/斩词/单词本计划片段；题型与静态题面',gaps='现版完整作答链、误判撤销、离线；历史片段构建与执行结果未知',visual_status='static_reference_only'),
 'F-M018-01':dict(evidence_ids='E-BBDC-01;E-BBDC-02;E-BBD-01;E-BBD-02',known_branches='中国区iOS拼写设置日志；Android例句显示修复',gaps='逐题回忆/揭示/评价/反馈链、原声音频与中断恢复未知'),
 'F-M018-02':dict(evidence_ids='E-BBDC-02;E-BBD-01',known_branches='回上一词/只拼错词/提示页与记错了修复日志',gaps='是否撤销、空错词集合、提示阶梯、重试和队列更新未知'),
 'F-M018-03':dict(evidence_ids='E-BBDC-02;E-BBDC-03;E-BBDC-04;E-BBD-01;E-BBD-03',known_branches='单元范围、已有本学习与PDF日志；公开自制入口重定向',gaps='登录后导入/校订/去重；范围实际状态；删除与进度恢复未知'),
 'F-M018-05':dict(evidence_ids='E-BBDC-03;E-BBD-01;E-BBD-02',known_branches='iOS帮助路径；Android描述路径差异',gaps='具体FAQ正文、反馈受理状态、离线保存与工单恢复未知'),
}
updates=[]
for r in old:
 if r['flow_id'] in changes:
  r.update(changes[r['flow_id']]);updates.append(r)
with (work/'existing-coverage-updates.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(updates)
print(json.dumps({'new_evidence':len(evidence),'new_flows':len(newrows),'existing_rows_for_root':len(updates),'static_images':len(images)},ensure_ascii=False))
