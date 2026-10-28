from pathlib import Path
import json, csv, hashlib, shutil

R = Path('/home/workspace/reader')
W = R / 'work/ireader-controls-deepening'
L = R / 'docs/research/interaction-library'
A = L / 'artifacts/ireader-controls-deepening'
B = L / 'batches'
A.mkdir(exist_ok=True)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def save_json(p, value): p.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')

notes = '''# M014 本轮来源短摘记

查阅2026-10-27；匿名文字读取。0新E/F，未看图片、未运行App；下列为旧E追加采集，完整原件留work。菜单文字与正文问题分类不是视觉观察。

## E-IRD-01：同一中国区商店条目的新快照

URL：https://apps.apple.com/cn/app/id463150061 ，普通GET为200并重定向到同ID中文标题路径。原件 `work/ireader-controls-deepening/app-store.html`；SHA256 `c11853a5986a75b5e8fc2f2227f3f07931d67fda841d374a34e20eddb943abef`。

原件版本历史TitledParagraph的primarySubtitle/secondarySubtitle给8.6.2及2026-10-22 UTC日期，兼容性列iOS/iPadOS15.6+。可读派生文本35—38行是阅读/导入/AI声明，103—142行为版本序列。8.5.1与标点停顿、8.5.2与外部音乐中断修复对应；8.5.9仅部分音色升级。开发者修复声明不等于验证成功。检索索引可能仍显8.6.1，未作为新快照版本。排除评论，未观察商店图片。

## E-IRD-03：复读既存华为FAQ

URL：https://consumer.huawei.com/cn/support/content/zh-cn15939571/ 。本轮没有重新GET；复用2026-10-26取得的 `work/research/extension-manuals/huawei-15939571.html`，SHA256 `39e7af2a2b8d1c0593ca18b682cdab7d701c99516afeaa5fd10a14caa540068a`。同名txt69—80行为适用范围，84—157行为4类18问。

适配对象是MatePad Paper/HarmonyOS2.1；预装门槛属于系统，LTE段1.8.4+属于App。F04需区分刷新、书架归属与重复文件；F05需区分排版标记、竖排、PDF重排及显示模式。所谓当前版本无逐笔撤销未绑定构建，不推广后代。完整问题目录只登记主题及缺口，不称为手机手册。原件图片没有打开。

## E-IPAD-01：复读2024政策

URL：https://ah2.ireader.com/fe3/zybook/other/statement/privacyagreement_detail.html 。本轮未联网重抓；旧原件 `work/chinese-app-depth/ireader-privacy.html`，SHA256 `93d47df113271ae28e807eb354f45b3fac14e09a38dd0e7b79156a46c6226d1b`。同名txt6—7行日期，28行游客退出，39—43行阅读记录/分享，62—63行书童，70—94行推荐与权限，169行后台备份清理。

准确层级是第一节内的基本/扩展业务，不是顶层第二节。分享笔记上云、章节记录、本地上传权限分别描述不同对象；不推导所有批注同步或精确续读。后台播放的权限说明、游客消费找回和隐私副本申请都没有补齐手机操作/恢复链。政策2024-06-27不等于8.6.2手册。

## 停止路径与范围

旧主站挑战原件 `work/chinese-apps/ireader-home.html` 只复读，不重新请求。zhangyue.com搜索工具报告robots后停该路线；未使用第三方下载站教程、书城内图书文字、其他App/硬件说明补手机UI。有限搜索的查询和排除结果在work/acquisition-log.json；搜索记录是调查过程，不增E。

本批只修改案例及新目录/摘记，旧F/E精确字段建议另交root；不改主台账、旧批或全局文档。没有旧视觉审计依赖此案例散列。
'''
(A / 'source-notes.md').write_text(notes)
shutil.copyfile(W / 'case-after.md', L / 'cases/M-014-ireader.md')

fields = next(csv.reader((L / 'coverage.csv').open(newline='')))
with (B / 'ireader-controls-deepening-coverage.csv').open('w', newline='') as out:
    csv.writer(out, lineterminator='\n').writerow(fields)
(B / 'ireader-controls-deepening-evidence.jsonl').write_text('')

rows = []
def item(i, kind, domain, loc, ev, flows, scope, status, gap):
    rows.append(dict(inventory_id=i,product_id='M-014',node_type=kind,function_domain=domain,source_locator=loc,evidence_ids=ev,flow_ids=flows,platform_version=scope,body_status=status,remaining_gap=gap,checked_on='2026-10-27'))
phone='中国区iOS/iPadOS商店8.6.2；2026-10-27快照'
policy='掌阅2024-06-27政策；OS/构建未逐路径绑定'
hw='MatePad Paper/HarmonyOS2.1；预装系统2.1.0.191+；App构建未固定'
mobile=[
('内容/选书','描述：内容类型','E-IRD-01','F-M014-01',phone,'选书入口、打开/返回和章节导航未取得'),
('封面/背景/字体','描述：阅读器','E-IRD-01','F-M014-01',phone,'设置入口、预览与恢复默认未取得'),
('本地导入','描述：epub/txt/pdf','E-IRD-01','F-M014-02',phone,'导入菜单、解析/失败/重复对象未知'),
('多设备同步','描述：阅读器','E-IRD-01','F-M014-02',phone,'原文件、位置、笔记等字段及冲突未知'),
('AI提炼/导图/章节速读','描述：AI','E-IRD-01','F-M014-03',phone,'入口、适用书、生成/引用/回原文未取得'),
('群书速览','描述：AI','E-IRD-01','F-M014-03',phone,'选书范围、来源比较及退出未取得'),
('书童文本问答','第一节扩展业务5','E-IPAD-01','F-M014-03',policy,'未给当前书上下文/提交/纠错/保存步骤'),
('听书音色','描述：音色','E-IRD-01','F-M014-03',phone,'换声、下载、计时、暂停与读听定位未知'),
('语音/后台权限','第一节扩展业务8','E-IPAD-01','F-M014-03',policy,'用途声明非菜单；切后台/断网恢复未知'),
('阅读日志','第一节扩展业务2(1)','E-IPAD-01','F-M014-02',policy,'章节记录不等于精确位置或恢复承诺'),
('分享/笔记服务器存储','第一节扩展业务2(2)','E-IPAD-01','F-M014-02',policy,'不推广全部本地批注；删除/撤销入口未知'),
('推荐开关','第一节扩展业务7','E-IPAD-01','F-M014-06',policy,'历史路径；生效时间/跨端/现版菜单未知'),
('帮助与反馈','开发者描述：联系方式','E-IRD-01','',phone,'取得反馈入口但手机FAQ正文未取得'),
('游客退出/资产求助','第一节基本业务','E-IPAD-01','',policy,'非文件恢复；客服身份校验和完成状态未知'),
('个人资料/账号注销','第五节(一)(二)(五)','E-IPAD-01','',policy,'历史政策路径未形成当前账户操作/恢复任务'),
('删除/个人信息副本','第五节(三)(六)','E-IPAD-01','',policy,'不是书籍/笔记导出或导回，备份清理非即时'),
('签到/活动提醒','第一节扩展业务4及8','E-IPAD-01','',policy,'权限线索不说明学习激励规则/兑付流程')]
for n,(domain,loc,ev,flow,scope,gap) in enumerate(mobile,1):
    item(f'IR-P{n:02}', 'mobile_domain',domain,loc,ev,flow,scope,'body_read_declaration_or_partial_path',gap)

faq=[
('基本操作','卸载后的应用恢复','','只核恢复预装App；用户书/笔记恢复未知'),
('基本操作','同意按钮不可用','','文字给滚动终点；不含权限拒绝后的完整首启'),
('基本操作','LTE网络不可用','','App1.8.4+条件；不推测硬件通信配置/其他版本'),
('基本操作','跨书城检索不到书','','建议改适配App书城检索；资源何时一致未知'),
('基本操作','加入后桌面书架无书','F-M014-04','不同书架归属；不证明桌面联动或重复下载'),
('基本操作','无线传书受网络限制','F-M014-02','仅墨水屏侧异常支路；手机主链不升D2'),
('基本操作','阅读全屏与控制中心','','触发步骤可定位；退出/系统版本变化未另析'),
('帐号登录与云同步','忘记密码','','转软件方客服；没有恢复验证完整链'),
('帐号登录与云同步','阅读进度延迟','F-M014-04','等待建议，不证明跨端位置冲突解决'),
('帐号登录与云同步','同步后书架刷新','F-M014-04','页面刷新与下载/数据恢复分开'),
('帐号登录与云同步','重复书籍文件','F-M014-04','帐号/添加路径差异；合并或去重入口未知'),
('书籍阅读','开书全刷','','正文解释现象；刷新设置/回退未取得'),
('书籍阅读','字体只部分生效','F-M014-01','精品书主字体/简体适用范围；不是手机设置链'),
('书籍阅读','旋转后页面位移','','解释分页和旋转锁定入口；原锚点找回未知'),
('图书笔记','逐笔撤销限制','F-M014-05','FAQ未标构建当前版本无单笔撤销；擦除后恢复未知'),
('图书笔记','笔迹不可见','F-M014-05','父项；四种分支另列，不能重复计任务'),
('图书笔记','分屏动作触发截图','','手写开关改变输入解释；不从文字推断图标形状'),
('图书笔记','分屏返回受限','F-M014-05','焦点退出及菜单返回；保存状态未知')]
for n,(cat,domain,flow,gap) in enumerate(faq,1):
    item(f'IR-H{n:02}', 'faq_question',domain,f'{cat}／第{n}问','E-IRD-03',flow,hw,'body_read',gap)
for n,(domain,gap) in enumerate([
    ('版式/字体/方向改变','三点标记展开旧笔迹；未看原图或检验锚点'),
    ('竖排模式','此模式不显示/不支持手写；回横排完整链未给'),
    ('PDF原页与重排','两状态批注独立；合并/导出关系未知'),
    ('PDF阅读/笔记模式','更多中切笔记模式；擦除/跨端恢复未知')],1):
    item(f'IR-H16.{n}', 'faq_subbranch',domain,f'图书笔记／笔迹不可见／分支{n}','E-IRD-03','F-M014-05',hw,'body_read',gap)
for n,(v,date,domain,gap) in enumerate([
    ('8.6.2','2026-10-22','一般听书优化','未给操作变化/修复适用条件'),
    ('8.5.9','2026-08-24','部分听书音色升级','音色枚举、收费/离线与切换未知'),
    ('8.5.4','2026-02-02','搜索结果页优化','不是选书/返回具体流程'),
    ('8.5.3','2026-01-04','书架崩溃修复','未说明崩溃触发条件/数据恢复'),
    ('8.5.2','2025-12-23','冷启动外部音乐中断修复','不证明后台续播/所有系统音频冲突解决'),
    ('8.5.1','2025-10-30','TTS标点停顿修复','未验证效果或给播放恢复步骤')],1):
    item(f'IR-V{n:02}', 'version_note',domain,f'版本历史{v}／{date}','E-IRD-01','F-M014-03' if n in [1,2,5,6] else 'F-M014-01','中国区iOS/iPadOS历史日志','body_read_developer_claim',gap)
with (B / 'ireader-controls-deepening-inventory.csv').open('w',newline='') as out:
    writer=csv.DictWriter(out,fieldnames=list(rows[0]),lineterminator='\n');writer.writeheader();writer.writerows(rows)

oldflows=json.loads((W/'coverage-before.json').read_text())
oldes=json.loads((W/'evidence-before.json').read_text())
sourcef={};sourcee={}
for p in B.glob('*-coverage.csv'):
    for row in csv.DictReader(p.open(newline='')):
        if row.get('product_id')=='M-014':sourcef[row['flow_id']]=str(p.relative_to(R))
for p in B.glob('*-evidence.jsonl'):
    for line in p.read_text().splitlines():
        d=json.loads(line)
        if d.get('product_id')=='M-014':sourcee[d['evidence_id']]=str(p.relative_to(R))
changes={
 'F-M014-01':('中国区商店8.6.2列版式能力；未取得手机选书/选文操作说明','查词/划线创建、撤销、回原文及选择范围未知；不以评论或墨水屏规则补手机链'),
 'F-M014-02':('商店列epub/txt/pdf与同步；2024政策区分本地上传、章节记录和分享笔记服务器存储','导入菜单/解析反馈、重复/损坏/取消、逐对象同步、换机重装及原件恢复未知'),
 'F-M014-03':('书童政策只说明输入文本与回答；8.5.1标点停顿/8.5.2外部音乐中断为历史修复声明，8.5.9仅部分音色升级','AI入口/引用/纠错/保存、听书启停/换声/定时/读听定位、离线与后台恢复均未闭合'),
 'F-M014-04':('仅MatePad Paper适配版：同步后切其他页回书架刷新；进度可延迟；桌面/适配App书架和不同文件须分辨','备份与同步启动入口、原件下载、账号冲突/重复书合并和重装恢复未知；不套手机'),
 'F-M014-05':('仅适配版：三点展开旧笔迹、竖排不显示、PDF原页/重排独立、笔记模式显示；未标构建FAQ称无逐笔撤销','擦除后恢复、导出/跨端原稿、后续版本撤销能力未知；分屏退出不证明保存完成'),
 'F-M014-06':('2024政策路径关个性化栏目，保留精选等；关闭推荐不证明清除阅读记录','历史路径未逐OS绑定；8.6.2菜单、生效时间、跨设备及历史清除仍未知')}
updates=[]
for old in oldflows:
    after=dict(known_branches=changes[old['flow_id']][0],gaps=changes[old['flow_id']][1],checked_on='2026-10-27')
    for key,value in after.items():
        if old[key]!=value:updates.append(dict(flow_id=old['flow_id'],source_batch=sourcef[old['flow_id']],field=key,before=old[key],after=value,reason='旧任务补对象/版本/恢复边界；深度不变；只建议root合并'))
save_json(W/'existing-coverage-updates.json',updates)

notes_rel=str((A/'source-notes.md').relative_to(L)); notes_sha=sha(A/'source-notes.md')
capture_specs=[
 ('E-IRD-01','work/ireader-controls-deepening/app-store.html','public_app_store_html_read','开发者描述；版本历史primarySubtitle/secondarySubtitle；兼容性','新GET 200；版本8.6.2；用户评论/图片未作为操作依据'),
 ('E-IRD-03','work/research/extension-manuals/huawei-15939571.html','local_reread_of_existing_public_artifact','4类18问；笔迹不可见4子项；派生txt69—157行','未重新GET；同URL既存原件；MatePad Paper适配版而非手机；App构建未知'),
 ('E-IPAD-01','work/chinese-app-depth/ireader-privacy.html','local_reread_of_existing_public_artifact','第一节基本/扩展业务2/5/7/8；第五节删除/副本；派生txt28/39—94/169行','未重新GET；2024-06-27政策不代表8.6.2操作；旧顶层第二节定位需修正')]
caps=[]
for eid,raw,mode,locator,limit in capture_specs:
    old=next(e for e in oldes if e['evidence_id']==eid)
    entry=dict(capture_url=old['url'],checked_on='2026-10-27',access_mode=mode,raw_work_path=raw,raw_sha256=sha(R/raw),locator=locator,limitation=limit,archived_artifact=notes_rel,sha256=notes_sha,hands_on=False,visual_observed=False)
    if eid=='E-IRD-01':entry.update(app_build='8.6.2',source_updated_on='2026-10-22',http_status=200,final_url=json.loads((W/'captures.json').read_text())[0]['final_url'])
    else:entry['original_capture_on']='2026-10-26'
    before=old.get('additional_captures',[])
    caps.append(dict(evidence_id=eid,source_batch=sourcee[eid],field='additional_captures',before=before,after=before+[entry],reason='同URL追加采集/原件复读，不重复编号'))
save_json(W/'existing-evidence-additional-captures.json',caps)
old=next(e for e in oldes if e['evidence_id']=='E-IPAD-01')
save_json(W/'existing-evidence-field-updates.json',[dict(evidence_id='E-IPAD-01',source_batch=sourcee['E-IPAD-01'],field='locator',before=old['locator'],after='第一节内二、扩展业务2阅读记录、5智能对话、7个性化服务；非顶层第二节',reason='对照旧HTML层级修正定位，不改变功能事实')])

queries=[
 'site.ireader.com 掌阅 手机 帮助 笔记 导入 听书','site:ireader.com 掌阅 书童 点击 使用','site:consumer.huawei.com 掌阅 墨水屏 导入 图书笔记',
 'site:ireader.com "帮助" "笔记"','site:zhangyue.com "帮助" "导入"','site:ireader.com "书童" "点击"',
 '掌阅 帮助中心 官方 "导入"','"掌阅" "书童" "长按"','"掌阅" "帮助与反馈" "笔记"','site:ah2.ireader.com 帮助',
 '"掌阅" "笔记" "导出" "官方" "手机"','"掌阅书童" "点击" "掌阅科技"','"掌阅" "常见问题" "听书"','site:mp.weixin.qq.com "掌阅" "本机导入"']
save_json(W/'acquisition-log.json',dict(checked_on='2026-10-27',queries=queries,query_record_type='manual_log_of_tool_queries_and_outcomes_not_response_archive',results='官网域名检索主要返回书城/图书正文；移动端帮助未取得。AppStore与Huawei为旧URL；其他为非官方教程、硬件/精选等不同对象或新闻，不补手机流程。',stopped=[dict(route='www.ireader.com/ previously challenged homepage',action='旧本地原件复读，本轮未重试'),dict(route='zhangyue.com web search',result='Blocked by robots.txt; non-retryable',action='停止该路径，未改用HTTP绕过')],excluded_examples=['https://www.itmop.com/article/52485.html','https://jingyan.baidu.com/article/d45ad148840ba328552b80c3.html','https://apps.apple.com/cn/app/id1447341012','https://www.bilibili.com/video/BV1xt4y1K78M/'],network_gets=1,new_evidence=0,new_flows=0,images_viewed=0))
print(json.dumps(dict(new_e=0,new_f=0,inventory=len(rows),old_f_fields=len(updates),old_e_captures=len(caps),old_e_field_corrections=1,source_notes_sha256=notes_sha),ensure_ascii=False))
