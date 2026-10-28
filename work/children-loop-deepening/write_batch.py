from pathlib import Path
import csv
import hashlib
import json
import shutil

root = Path('/home/workspace/reader')
lib = root / 'docs/research/interaction-library'
raw = root / 'work/children-loop-deepening'
art = lib / 'artifacts/children-loop-deepening'
art.mkdir(parents=True, exist_ok=True)
def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def row(eid, pid, title, url, kind, scope, locator, note, raw_name=None, **extra):
    r = dict(evidence_id=eid, product_id=pid, title=title, url=url,
        checked_on='2026-10-26', source_type=kind, access_mode='opened_text',
        version_scope=scope, app_build=None, firmware_version=None,
        source_updated_on=None, locator=locator, note=note, hands_on=False,
        visual_observed=False, archived_artifact=None, sha256=None,
        capture_state='bibliography_and_short_note_only', supersedes=None)
    if raw_name:
        r.update(raw_work_path=str((raw/raw_name).relative_to(root)), raw_sha256=digest(raw/raw_name))
    r.update(extra)
    return r
def visuals(r, names, urls, scope):
    paths=[]
    for name in names:
        shutil.copy2(raw/name, art/name)
        paths.append(str((art/name).relative_to(lib)))
    r.update(visual_observed=True, archived_artifact=paths[0], sha256=digest(lib/paths[0]),
        rendered_artifacts=paths, artifact_hashes={p:digest(lib/p) for p in paths},
        image_urls=urls, visual_scope=scope,
        capture_state='necessary_official_static_images_archived_raw_material_temporary')
    return r

items=[]
items.append(row('E-IHD-01','M-029','洪恩识字 Android 小米渠道 4.5.2 开发者说明与更新',
    'https://app.mi.com/details?id=com.hongen.app.word','official_developer_store_listing',
    'China mainland Xiaomi Android phone/tablet listing; not installed; developer 天津洪恩完美未来教育科技有限公司',
    '详细信息; 分阶与生活应用; 新版特性',
    '固定 Android 版本与教材整课测验范围；未推定所有渠道及逐题交互。网站下载维护不是 App 学习失败。',
    'ihuman-mi.html', app_build='4.5.2 (listing only)', source_updated_on='2026-10-23', qualifies='E-IHUM-02'))
gallery=json.loads((raw/'gallery-urls.json').read_text())
r=row('E-IHD-02','M-029','洪恩识字官网图库：绘本入口与复习示意的静态界限',
    'https://www.ihuman.com/shizi/','official_static_promotional_images',
    'Current official public website; image date/platform/build and account state unknown',
    '不止识字，全面提升语文素养; gallery pic_1/pic_6/pic_4',
    '实际查看绘本目标字/开始阅读、复习曲线示意和 AI 合成展示；仅前两图正式保存。不是连续学习任务。',
    'ihuman-site.html', access_mode='public_html_script_asset_discovery_and_view_image', qualifies='E-IHUM-01',
    raw_supporting_paths={'work/children-loop-deepening/ihuman-site.js':digest(raw/'ihuman-site.js'),
        'work/children-loop-deepening/ihuman-gallery-ai.png':digest(raw/'ihuman-gallery-ai.png')},
    access_limitation='Official video URL identified in public HTML only; no playback/download. Initial pic_1_deactive/pic_6_deactive navigation icons were inspected and rejected as app UI evidence.')
items.append(visuals(r,['ihuman-gallery-books.png','ihuman-gallery-review.png'],gallery,
    'Static website promotional composite and schematic only; no app render, no continuous process; AI composite temporarily viewed and not archived.'))
items.append(row('E-IHD-03','M-029','各 APP 反馈入口：洪恩识字',
    'https://www.ihuman.com/agreement/privacy/feedback.html','official_help',
    'China official feedback instruction; date/platform/build unspecified',
    '洪恩识字条目', '只确认官方指引中的首页右上我要反馈入口；未打开 App 或提交问题。',
    'ihuman-feedback.html'))
items.append(row('E-DAD-01','M-031','Practice any skill, any time in the Practice tab',
    'https://blog.duolingo.com/guide-to-duolingo-practice-hub/','official_help',
    'iOS/Android Practice tab; Adventures only if offered in current course; builds/child accounts unspecified',
    'Target skills in the Practice tab; Other content',
    '支持杠铃/三点菜单进入并回访课程内容；不证明断点续接、所有旧场景或 Speaking 任务使用相同列表。',
    'duo-practice.html', source_updated_on='2026-04-17',
    publication_date_note='HTML JSON-LD datePublished 2026-02-18T13:00:00.000Z; dateModified 2026-04-17T08:38:04.000Z.'))
r=row('E-DAD-02','M-031','Adventures official 2024 three-screen static illustration',
    'https://blog.duolingo.com/adventures/','official_static_promotional_image',
    '2024 click-based Adventures; sample French course; image build unknown, not current general reward rule',
    'EN_Adventures_UIscreens.png; left/middle/right static screens',
    '实际查看任务入口、开始前奖励标示、地图、选项和关闭图标；不含错答、结算或恢复。',
    'duo-adventures.html', access_mode='opened_text_public_html_and_view_image', source_updated_on='2024-12-02',
    publication_date_note='Article published 2024-09-24; image URL path dated 2024/08.', qualifies='E-DUOA-01',
    access_limitation='Embedded YouTube https://www.youtube.com/watch?v=2IFzfELMSVw returned fetch error in web reader; not watched/downloaded. GIF not continuously observed.')
items.append(visuals(r,['duo-adventure-static.png'],
    {'duo-adventure-static.png':'https://storage.ghost.io/c/7a/33/7a33d0f4-927d-4fe8-a6bf-96131b5e76d4/content/images/2024/08/EN_Adventures_UIscreens.png'},
    'Three juxtaposed promotional static screens, not a continuous observed sequence or interaction.'))
r=row('E-DAD-03','M-031','Duolingo Q1 2026 shareholder letter in official 8-K: speaking UI',
    'https://investors.duolingo.com/static-files/ac220d7c-e313-4049-b309-1095fd24a86f','official_investor_document_static_render',
    'Q1 FY2026; Speaking Adventures screenshot platform/build/course/child-account availability unknown',
    'PDF file page 9 (zero-based 8); printed shareholder-letter page 4; rightmost Speaking adventures screen',
    '实际查看人物话语和发言控件静态图；不把相邻 Flashcards 的不能说话按钮移植；不是一次口语操作。',
    'duo-q1-2026-8k.pdf', access_mode='official_pdf_download_pymupdf_page_render_and_view_image',
    qualifies='E-DUOA-02', publication_date_note='Official 8-K filed 2026-05-04; duplicate shareholder-letter material, not an independent content source.',
    access_limitation='Original gcs-web PDF urllib fetch timed out (exit 1); web screenshot returned no inspectable image. Official investor 8-K copy fetched successfully (exit 0).')
items.append(visuals(r,['duo-q1-2026-8k-p9.png'],
    {'duo-q1-2026-8k-p9.png':'https://investors.duolingo.com/static-files/ac220d7c-e313-4049-b309-1095fd24a86f#page=9'},
    'One official PDF page rendered with existing PyMuPDF at matrix 1.6 and actually viewed; static only.'))

batch=lib/'batches/children-loop-deepening-evidence.jsonl'
batch.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in items))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
flows=[['F-M029-04','M-029','按教材核查整课汉字','D1','E-IHD-01','年级/书册/课文范围；先单字后整课测验','具体入口；错答与提示；补练；结算；退出恢复','not_observed','false','cases/M-029-ihuman.md','2026-10-26'],
    ['F-M031-04','M-031','从 Practice 回访 Adventures','D2','E-DAD-01','底部杠铃；未显示时从三点菜单；课程可用条件','内容列表；未完成任务；重玩结算；中途恢复；Speaking 范围','not_observed','false','cases/M-031-duolingo-adventures.md','2026-10-26']]
with (lib/'batches/children-loop-deepening-coverage.csv').open('w',newline='') as f:
    w=csv.writer(f,lineterminator='\n');w.writerow(fields);w.writerows(flows)
print('wrote',len(items),'evidence records,',len(flows),'new flows; 4 static artifacts')
