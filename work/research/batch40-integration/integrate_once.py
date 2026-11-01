import csv
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path('/home/workspace/reader')
LIB = ROOT / 'docs/research/interaction-library'
BATCH = LIB / 'batches'
RAW = ROOT / 'work/research/batch40-maimemo'
ART = LIB / 'artifacts/maimemo-input-and-sync'
KART = LIB / 'artifacts/khan-remaining-source-attempt'
DATE = '2026-10-27'
NEW_E = BATCH / 'maimemo-input-and-sync-evidence.jsonl'
assert not NEW_E.exists(), 'One-time integration already applied'
assert (ROOT / 'work/research/batch40-integration/before-manifest.json').exists()
assert hashlib.sha256((ROOT / 'docs/requirements.md').read_bytes()).hexdigest() == 'e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def put(p, s):
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(s.rstrip() + '\n')

def jput(p, obj):
    put(p, json.dumps(obj, ensure_ascii=False, indent=2))

def csvread(p):
    with p.open(newline='') as f:
        return list(csv.DictReader(f))

def csvwrite(p, rows):
    with p.open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)

def edit_e(path, eid, fn):
    rows = [json.loads(s) for s in path.read_text().splitlines()]
    found = [x for x in rows if x['evidence_id'] == eid]
    assert len(found) == 1
    fn(found[0])
    put(path, '\n'.join(json.dumps(x, ensure_ascii=False) for x in rows))

ART.mkdir(parents=True, exist_ok=True)
KART.mkdir(parents=True, exist_ok=True)
captures = json.loads((RAW / 'fetch-results.json').read_text())
images = json.loads((RAW / 'images.json').read_text())
byname = {x['name']: x for x in captures}
source_ids = {
    'controller': 'E-MMSC-01', 'harmony-features': 'E-MMSC-02',
    'reminders': 'E-MMSC-03', 'rating-guide': 'E-MMSC-04',
    'web-study': 'E-MMSC-05', 'web-log': 'E-MMSC-06', 'open-terms': 'E-MMSC-07',
    'android-log': 'E-MMDVC-03',
}

put(ART / 'source-notes.md', '''# 墨墨：输入阶段、选词对象与跨端同步

查阅：2026-10-27。M-017；8篇官方正文经匿名普通GET取得，7个新来源沿用E-MMSC-01～07，Android日志复用E-MMDVC-03。未安装、登录或操作App、手柄、手表，未调用API。9幅官方静态图片逐幅查看，不能视为同一任务的连续操作。全部原址、HTTP状态、原件及摘记散列见[source-manifest.json](source-manifest.json)，画面边界见[观察记录](visual-observation.json)。

## E-MMSC-01：外设连接与App动作

正文把外设作用范围限定在App复习页。配对先用Home+B保持5秒进入黄灯快闪，在系统蓝牙连接Maimemo Controller，再到App“我的→手柄设置”核对连接状态。蓝牙已连接而App未连接时，文档要求排查是否误入绿/蓝灯游戏模式；两种连接状态不是同一结果。

文档列出三种自评、单词/例句发音、显示答案、上一词及确认/取消；具体默认按键表留在App内，本次未取得。返回上一词不证明撤销评分。App内映射和KeyLinker修改硬件映射不同；后者可能影响识别，文档给重置办法。iOS改蓝牙名称会影响识别，要求恢复默认名称后重启手机。这些是排障说明，没有本轮故障恢复实测。

兼容表写iPhone iOS14+，排障段却写苹果设备iOS/iPadOS16+，冲突未解决；HarmonyOS Next仍写适配中。不能把鸿蒙版的隔空手势理解为这只手柄已经兼容。自动回连和5分钟休眠是厂商声明，唤醒后首键、连发、长按和错误评分恢复未知。

所链[飞书手柄说明书](https://maimemo.feishu.cn/wiki/AaWzwJREaieLFIkbDvIcXRacnWh)：web工具报告无法打开，不提供HTTP状态；随后一次普通GET为200，只有Wiki/PDF查看器外壳与标题“墨墨口袋手柄说明书.pdf”，未取得PDF页或正文。没有把外壳当手册已读，也没有调用猜测的内部下载接口。见[附件访问记录](controller-manual-attempt.json)。

## E-MMSC-02：鸿蒙版入口与状态

拍照取词从“选词”搜索栏右侧A图标进入，正文允许拍照或选已有图片。静态图有识别结果、未选/已选/全部分类和词条详情，没有相机授权、识别失败、手工校正或提交成功。

- [图02](harmony-02.jpg)只显示入口；[图03](harmony-03.jpg)显示16未选、40已选、56全部，数值是示例，不是额度。
- [图04](harmony-04.jpg)同屏顶部为40已选，底部却是“已选0／建议选0”，并有灰色加入记忆入口。根据位置与文案可区分计数范围，但不能把40解释为本次新增，也未实际验证按钮禁用或选中后可提交。
- [图05](harmony-05.jpg)左边复习inheritance，右边内容选择对象是the。图示证明多个对象可以同时可见；不能拼成“点当前词释义→打开同一词”的连续过程。按钮上的预计天数也不是本轮实际排期结果。
- [图09](harmony-09.jpg)设置搜索“例句”，分别列出答案出现前的英文/中文提示，以及显示答案后自动播放第一条例句。图中有续订入口，但不能由此确定所有发音的收费条件；默认值、保存与跨端生效未观察。

隔空手势须在系统启用相应手势且App处于复习页：答案出现前下滑显示答案、上滑返回；出现后下滑自评认识、上滑自评忘记。这个规则来自正文，不是本轮实际手势操作。模糊的对应方式、返回范围、防连发及误评恢复仍缺。

应用接续要求相同华为账号、相同墨墨账号、自动同步，源设备处于复习页；目标设备点dock接续图标后继续。图01仅画图标；未验证答案显隐、待提交反馈、滚动位置、离线冲突或两端并行学习。[图06](harmony-06.png)手表隐藏答案，[图07](harmony-07.png)展示同词答案和认识按钮，[图08](harmony-08.png)展示例句页；这些是静态示意，未见真实点击、三类自评全貌或签到完成。手表范围为文中Watch 5等HarmonyOS设备，不外推普通蓝牙手表。

## E-MMSC-03：学习提醒为什么没有出现

若在设定时间前已完成当日学习和签到，文档说明当天不再提醒。否则依次检查系统对墨墨的通知开关及App“我的→学习提醒→系统提醒”。这个条件有助于区分合理不提醒与通知故障；没有观察时间选择器、权限拒绝、到点通知、跨日和补发。

## E-MMSC-04：好评指南的范围

正文讲应用商店写/查看评论并截图，涉及不同手机商店。它不是学习中的自评或纠错，不建立新的学习任务。未发表任何评价或联系他人。

## E-MMSC-05/06：Web同步与功能范围

Web公测指南要求iOS至少5.5.27、Android至少5.5.25、HarmonyOS至少5.5.03；开启学习数据自动同步后要完成一次同步再进Web。App学完后退出到后台才触发自动同步，等待完成后Web才能获取新进度。进度未更新的排查依次涉及版本、开关、切后台和等待、刷新或重登；账号还可能未获公测资格。没有本轮账号或实际结果，不推断强制结束进程也能触发。

指南称核心复习公测，另一个Web日志已声明选词、内容偏好、搜词历史、学习量等变化。日志日期范围2025-10-16至2026-09-02，帮助正文无发布日期；不能用两者合成与App完全一致的功能表。公测异常是厂商提示，不是本项目遇到数据丢失。

## E-MMDVC-03：复用Android历史日志

此URL已用于M-023排除App日志与设备固件混淆，本次追加M-017相关全文读取，保留原编号与范围。顶部V5.5.00标题为2025-04-28而“发布于”为2025-05-19；不能判断这是当前最高版本。历史项声明过随身听循环/不息屏/倍速、固定学习量和顺序、私有例句/助记、外设滚动/映射、多设备提示等，缺少按钮到保存的全过程。V5.2.40重复条目及其两种日期也不能被静默统一。

随身听的实际播放队列、离线、退出/来电续播仍未补齐；支持某汉王墨水屏的日志不证明支持E-Ink4 Plus。

## E-MMSC-07：接口读到的数据可以怎样使用

协议标明更新/生效于2026-06-26；本轮重点核§3/4/7的应用审核、用户数据处理及内容使用条件，未做完整法律审查。§7的“版权内容”包括通过内容接口取得的释义、助记、例句等。厂商条件限定审核通过应用向授权用户在线展示和必要临时缓存（不超过168小时），另规定来源/作者署名、撤回后的缓存及备份清除。

未经另行书面许可，条款不支持将这些内容永久沉淀成自有语料库、再分发、导出或改写等用途。因此公开API能返回字段，不等于可以拿来构建永久离线AI知识库。这个结论针对文中版权内容范围，不把它扩大为所有个人学习记录都禁止备份；本项目没有选定接入墨墨服务。

## 可复用判断与尚缺证据

对G-002的启发：AI须知道学习阶段、当前动作对象、连接层和真实同步进度；一个手势名、一个“已选”计数或一个开启的同步开关均不足以表示执行完成。这是本项目分析，尚非确定设计。

下一步仍应观察同平台同构建的选词提交、错误识别修正、误评撤销、后台同步失败及恢复。上述静态图与文档不能关闭持久保存、儿童可理解性或学习效果的缺口。
''')

observations = [
    ('dock接续图标', '仅图标，未见点击后结果'),
    ('选词搜索栏右侧A入口', '未见拍照/相册选择器和授权'),
    ('识别结果16未选/40已选/56全部', '示例计数不是本次新增量或额度'),
    ('识别结果双栏、底部已选0和灰色加入记忆', '未验证控件是否禁用、选择后启用和提交保存'),
    ('左inheritance复习、右the内容选择', '不是同词连续流程；预计间隔未执行'),
    ('手表tone答案隐藏', '未实际点击；10/30含义未确定'),
    ('手表tone答案显示和认识240天后', '未见完整自评按钮组或真实排期'),
    ('手表tone例句页', '导航手势和声音未观察'),
    ('设置搜索例句、答案前提示和答案后自动发音', '默认/保存/跨端/付费范围未知'),
]
for x, (visible, limits) in zip(images, observations):
    src = ROOT / x['path']
    assert digest(src) == x['sha256']
    shutil.copy2(src, ART / src.name)
    x['archived_artifact'] = str((ART / src.name).relative_to(LIB))
    x['visual_observed'] = True
    x['evidence_id'] = 'E-MMSC-02'
    x['observation'] = visible
    x['limits'] = limits
    x['operation_method'] = 'static_official_image_view_only'
    x['hands_on'] = False
jput(ART / 'visual-observation.json', {'checked_on': DATE, 'images': images, 'continuous_sequence': False, 'product_operations': 0})
manual = json.loads((RAW / 'controller-manual-attempt.json').read_text())
manual.update(body_obtained=False, scope='HTTP200 public viewer shell and PDF title only; no PDF pages; no product UI operations', checked_on=DATE)
jput(ART / 'controller-manual-attempt.json', manual)
manifest = []
for x in captures:
    y = {k: v for k, v in x.items() if k not in ('links', 'images')}
    y['evidence_id'] = source_ids[x['name']]
    y['body_status'] = 'selected_sections_3_4_7_read' if x['name'] == 'open-terms' else 'main_text_read'
    y['raw_path'] = str(Path(y['raw_path']).relative_to(ROOT))
    y['text_path'] = str(Path(y['text_path']).relative_to(ROOT))
    manifest.append(y)
jput(ART / 'source-manifest.json', manifest)

notes_rel = str((ART / 'source-notes.md').relative_to(LIB))
hashes = {str(p.relative_to(LIB)): digest(p) for p in sorted(ART.iterdir()) if p.is_file()}
locators = {
    'controller': '核心功能、Home+B配对、App连接、映射及兼容性冲突',
    'harmony-features': '应用接续、拍照取词、双栏、手表、设置搜索和隔空手势；九幅静态图',
    'reminders': '完成学习及签到后不提醒；系统/App两处开关',
    'rating-guide': '各商店写/查看评价；不属于学习反馈',
    'web-study': '公测条件、最低版本、后台触发自动同步与未更新排查',
    'web-log': '2025-10-16至2026-09-02条目；功能声明非操作链',
    'open-terms': '重点核§3/4/7；版权内容定义、在线展示、临时缓存、撤回',
}
existing = [json.loads(s) for s in (LIB / 'evidence.jsonl').read_text().splitlines()]
assert not {v for k, v in source_ids.items() if k != 'android-log'} & {x['evidence_id'] for x in existing}
newrows = []
for name, eid in source_ids.items():
    if name == 'android-log':
        continue
    x = byname[name]
    assert not any(e['url'].rstrip('/') == x['url'].rstrip('/') for e in existing)
    e = dict(evidence_id=eid, product_id='M-017', title=x['title'], url=x['url'], checked_on=DATE,
             source_type='official_platform_terms' if name == 'open-terms' else 'official_release_notes' if name == 'web-log' else 'official_help_article',
             access_mode='anonymous_http_get_main_text_and_static_images' if name == 'harmony-features' else 'anonymous_http_get_main_text',
             version_scope='墨墨软件；平台按正文分别记录，未固定实际App构建；不是独立单词机固件',
             app_build=None, source_updated_on='2026-06-26' if name == 'open-terms' else None,
             locator=locators[name], note='详见来源摘记；未安装/登录/操作产品。静态图不构成连续任务。',
             hands_on=False, visual_observed=name == 'harmony-features', archived_artifact=notes_rel,
             sha256=hashes[notes_rel], supersedes=None, raw_work_path=str(Path(x['raw_path']).relative_to(ROOT)),
             raw_sha256=x['raw_sha256'], parsed_text_path=str(Path(x['text_path']).relative_to(ROOT)), http_status=x['status'])
    if name == 'harmony-features':
        e.update(visual_kind='official_static_help_images', artifact_hashes=hashes,
                 visual_audit_path=str((ART / 'visual-observation.json').relative_to(LIB)),
                 visual_audit_sha256=digest(ART / 'visual-observation.json'))
    else:
        e['artifact_hashes'] = {k: v for k, v in hashes.items() if k.endswith(('source-notes.md', 'source-manifest.json')) or name == 'controller' and k.endswith('controller-manual-attempt.json')}
    newrows.append(e)
put(NEW_E, '\n'.join(json.dumps(x, ensure_ascii=False) for x in newrows))

def android_append(e):
    x = byname['android-log']
    e['related_product_ids'] = sorted(set(e.get('related_product_ids', []) + ['M-017']))
    e.setdefault('additional_captures', []).append(dict(capture_url=x['url'], checked_on=DATE,
        access_mode='anonymous_http_get_main_text_read', http_status=200, raw_work_path=str(Path(x['raw_path']).relative_to(ROOT)),
        raw_sha256=x['raw_sha256'], archived_artifact=notes_rel, sha256=hashes[notes_rel],
        locator='全文历史条目；V5.5.00标题与发布日不同、V5.2.40重复；App日志非设备固件', hands_on=False, visual_observed=False))
edit_e(BATCH / 'vocab-device-deepening-evidence.jsonl', 'E-MMDVC-03', android_append)

inventory = csvread(BATCH / 'maimemo-controls-deepening-inventory.csv')
updates = {
    'M017-H07': ('E-MMSC-01', 'F-M017-11', 'main_text_read_partial_task', '默认按键表只在App内；返回上词不证明撤销；连接及回连未实操；所链PDF仅取得外壳'),
    'M017-H09': ('E-MMSC-02', 'F-M017-01;F-M017-04;F-M017-10', 'main_text_and_9_static_images_read', '识词提交/校正、手势误判、同构建学习结果与接续持久状态未知'),
    'M017-H24': ('E-MMSC-03', 'F-M017-12', 'main_text_read_partial_task', '时间选择/权限拒绝/实际通知/跨日补发未知'),
    'M017-H27': ('E-MMSC-04', '', 'read_external_support_not_learning_task', '应用商店评价说明；没有产品学习交互或实际投稿'),
    'M017-H30': ('E-MMSC-05', 'F-M017-04', 'main_text_read_partial_task', '已核后台触发及等待；未登录或观察真实同步/冲突/失败恢复'),
    'M017-S02': ('E-MMDVC-03', 'F-M017-02', 'historical_release_notes_read', '历史声明不补随身听队列和恢复；顶部日期不一，不证明现行最高版本'),
    'M017-S04': ('E-MMSC-06', 'F-M017-01;F-M017-04', 'release_notes_read', '功能声明不等于App全端一致；按钮/完成结果未读到'),
    'M017-S07': ('E-MMSC-07', 'F-M017-07', 'selected_sections_3_4_7_read', '接口可返回不表示允许永久离线语料；非完整法律审查或实际授权'),
}
for r in inventory:
    if r['item_id'] in updates:
        eid, fids, coverage, gap = updates[r['item_id']]
        r.update(evidence_ids=r['evidence_ids'] + ';' + eid, flow_ids=fids, coverage=coverage, gaps=gap, checked_on=DATE)
csvwrite(BATCH / 'maimemo-controls-deepening-inventory.csv', inventory)

flows = csvread(BATCH / 'vocab-coverage.csv')
flowadd = {
    'F-M017-01': ('E-MMSC-02;E-MMSC-06', '鸿蒙隔空手势在答案前后含义不同；九幅静态图含双栏不同词对象、手表显隐和例句提示时机', '手势误评/返回范围、连续拼写及手表完整反馈/完成/保存仍未知', 'static_official_images_only'),
    'F-M017-02': ('E-MMDVC-03', 'Android历史日志亦声明固定学习量/顺序；保留日志日期差异', '实际设置入口、当前任务重排、积压和跨日恢复仍缺', None),
    'F-M017-04': ('E-MMSC-02;E-MMSC-05', 'Web需自动同步完成；App学完切后台触发；鸿蒙接续另需双账号一致及源设备复习页', '答案阶段/待提交反馈能否接续、离线冲突、双端并行和失败恢复未验证', 'static_official_images_only'),
    'F-M017-07': ('E-MMSC-07', '服务协议§7限定版权内容的审核在线展示与最多168小时必要缓存；不能据接口推断永久语料复用', '真实授权/执行/撤销未验证；条款不等于已选集成方案，也不泛化为个人记录一律不能备份', None),
}
for r in flows:
    if r['flow_id'] in flowadd:
        eids, known, gap, visual = flowadd[r['flow_id']]
        r['evidence_ids'] += ';' + eids
        r['known_branches'] += '；' + known
        r['gaps'] += '；' + gap
        r['checked_on'] = DATE
        if visual:
            r['visual_status'] = visual
csvwrite(BATCH / 'vocab-coverage.csv', flows)

newflows = []
for n, task, eid, known, gap, visual in [
    (10, '从图片识词并检查候选', 'E-MMSC-02', '选词页A入口→拍照/选图；静态识别列表/分类/双栏词详情及加入记忆入口；顶部分类与底部当前选取计数不同', '相机授权/识别失败/校正/选中后按钮变化/提交成功及持久化未观察', 'static_official_images_only'),
    (11, '连接外设并在复习页执行快捷动作', 'E-MMSC-01', 'Home+B黄灯配对→系统蓝牙→App连接状态；限定复习页；映射和连接排障、iOS名称问题；兼容版本冲突保留', '默认按键表、真实连接/回连、长按连发/休眠首键与误评恢复未观察；PDF页未取得', 'not_observed'),
    (12, '核对学习提醒的条件与开关', 'E-MMSC-03', '提前学完且签到当天不提醒；系统通知开关→App学习提醒/系统提醒', '时间选择、权限拒绝、到点显示、跨日和补发未观察', 'not_observed'),
]:
    newflows.append(dict(flow_id=f'F-M017-{n:02}', product_id='M-017', task=task, depth='D2', evidence_ids=eid,
        known_branches=known, gaps=gap, visual_status=visual, hands_on='false', case_path='cases/M-017-maimemo.md', checked_on=DATE))
csvwrite(BATCH / 'maimemo-input-and-sync-coverage.csv', newflows)

# Retain the Khan directory's prior successful capture and append only the two failed section requests.
kraw = ROOT / 'work/research/batch40-khan'
targets = json.loads((kraw / 'results.json').read_text())
summary = json.loads((kraw / 'summary.json').read_text())
audit = dict(checked_on=DATE, targets=targets, summary=summary, section_attempts=[], target_article_requests=0, product_operations=0)
for name, url in [('home', 'https://khankids.zendesk.com/hc/en-us/sections/360001481891-Home-and-Parent-Resources'), ('class', 'https://khankids.zendesk.com/hc/en-us/sections/360008453772-Class-and-Teacher-Resources')]:
    hp = kraw / 'sections' / (name + '.headers')
    bp = kraw / 'sections' / (name + '.html')
    h, b = hp.read_text(), bp.read_text()
    assert '403' in h and 'Just a moment' in b
    audit['section_attempts'].append(dict(url=url, http_status=403, title='Just a moment...',
        raw_work_path=str(bp.relative_to(ROOT)), raw_sha256=digest(bp), headers_work_path=str(hp.relative_to(ROOT)), headers_sha256=digest(hp),
        body_obtained=False, stopped=True))
jput(KART / 'access-audit.json', audit)
put(KART / 'source-notes.md', '''# Khan Kids剩余目录的访问边界

2026-10-27。关联既有E-KLCD-01；不新增来源或任务。尝试解析家长/教师栏目中的10个未读标题，现有本地索引未提供可直接使用的文章URL。两次普通栏目GET均返回HTTP403、标题Just a moment...验证页，已停止这一来源路线，没有再探测内部接口。

未解析目标文章直链，也没有请求这10篇文章；因此应记录“栏目获取受阻、文章正文仍未读”，不能写“10篇文章均403”。没有观察图片、视频或产品操作。旧成功目录快照继续保留，不用本次失败覆盖它。

涉及KLC-I005/006/018/028/034/041/046/049/050/051，包含账户、网络错误、家校/课堂与学习水平等入口。标题不能补出实际步骤；也不能由未取得资料推断产品不支持。具体请求、原件散列及条目见[访问审计](access-audit.json)。
''')
def khan_append(e):
    for a in audit['section_attempts']:
        e.setdefault('additional_captures', []).append(dict(capture_url=a['url'], checked_on=DATE,
            access_mode='anonymous_get_section_challenge_stopped', http_status=403, body_obtained=False,
            raw_work_path=a['raw_work_path'], raw_sha256=a['raw_sha256'], hands_on=False, visual_observed=False,
            archived_artifact=str((KART / 'source-notes.md').relative_to(LIB)), sha256=digest(KART / 'source-notes.md'),
            locator='栏目验证页；目标文章URL未解析，未请求目标文章'))
    e.setdefault('artifact_hashes', {}).update({str(p.relative_to(LIB)): digest(p) for p in KART.iterdir()})
edit_e(BATCH / 'khan-library-controls-deepening-evidence.jsonl', 'E-KLCD-01', khan_append)
krows = csvread(BATCH / 'khan-library-controls-deepening-inventory.csv')
kids = {'KLC-I' + n for n in ['005','006','018','028','034','041','046','049','050','051']}
for r in krows:
    if r['inventory_id'] in kids:
        r['body_status'] = 'index_only_section_fetch_blocked'
        r['remaining_gap'] += '；2026-10-27栏目GET403验证页后停止；文章URL未解析/未请求，正文仍未读'
csvwrite(BATCH / 'khan-library-controls-deepening-inventory.csv', krows)
print('Integrated source artifacts, 7 new E / 1 reused Android E, 3 new F / 4 old F, 8 M017 and 10 Khan directory updates. Prose and RP follow separately.')
