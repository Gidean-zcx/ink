from pathlib import Path
import csv
import hashlib
import json
import shutil

root = Path('/home/workspace/reader')
lib = root / 'docs/research/interaction-library'
batch = lib / 'batches'
work = root / 'work/news-app-deepening'
art = lib / 'artifacts/news-app-deepening'
art.mkdir(parents=True, exist_ok=True)
images = []
for number in (2, 3):
    dest = art / f'espresso-store-{number}.jpg'
    shutil.copyfile(work / dest.name, dest)
    images.append(dest.relative_to(lib).as_posix())
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
records = []

def evidence(eid, pid, title, url, locator, note, **extra):
    row = dict(evidence_id=eid, product_id=pid, title=title, url=url,
        checked_on='2026-10-26', source_type='official_help', access_mode='opened_text',
        version_scope='Web/iOS/Android as specified in article; build and region unspecified',
        app_build=None, firmware_version=None, source_updated_on='2026-03-09',
        locator=locator, note=note, hands_on=False, visual_observed=False,
        archived_artifact=None, sha256=None, capture_state='bibliography_and_short_note_only', supersedes=None)
    row.update(extra)
    if row.get('raw_work_path'):
        row['raw_sha256'] = digest(root / row['raw_work_path'])
    records.append(row)

evidence('E-ESD-01','M-045','Espresso iOS listing and official static screenshots',
    'https://apps.apple.com/us/app/espresso-world-news-in-a-shot/id896628003',
    'Version; Age Rating; iPhone6.9_RefreshEspressoApp_2/3/4 images',
    '静态图确认时间标签与文章控件，未操作；图2/3正式归档，图4重复结构仅临时查看。',
    source_type='official_app_store_with_static_images', access_mode='opened_text_public_html_and_view_image',
    version_scope='US iOS listing 3.42.0; image build unknown, sample article dates June 5 2025',
    app_build='3.42.0 (store listing only; screenshot build unknown)', source_updated_on=None,
    raw_work_path='work/news-app-deepening/espresso-appstore.html', visual_observed=True,
    archived_artifact=images[0], sha256=digest(lib/images[0]), rendered_artifacts=images,
    artifact_hashes={p:digest(lib/p) for p in images},
    capture_state='official_static_store_images_archived_raw_html_temporary',
    visual_scope='Static promotional images only, no rendered app or continuous process',
    image_urls=json.loads((work/'espresso-image-urls.json').read_text()))
evidence('E-ESD-02','M-045','ACG Espresso guide — Settings and weekly preferences',
    'https://library.aug.edu/how-to-guides/mobile-apps/espresso-from-the-economist',
    "What are the app's main features?",'机构指南提供设置路径，不证明当前所有平台界面。',
    source_type='institutional_access_guide', access_mode='official_institution_index_text_only',
    version_scope='Institution-specific guide; app build/platform/date unknown', source_updated_on=None,
    qualifies='E-ESPR-04', access_limitation='Direct web open unavailable; indexed institution text readable.')
evidence('E-ESD-03','M-045','Economist Podcasts+ FAQ — Espresso entitlement boundary',
    'https://subscribenow.economist.com/podcasts-plus','FAQ: Espresso subscribers',
    '仅核定播客权益范围，不能推导 Espresso 播放和下载控件。',
    source_type='official_subscription_faq', access_mode='official_index_text_only',
    version_scope='Espresso vs Podcasts+ subscription FAQ; app builds unknown', source_updated_on=None,
    access_limitation='Direct page returned no readable body; official indexed FAQ was readable.')

sources = [
('01','5609857','Search for Articles or Find Coverage','Website/Apps; In Focus; source search','搜索与不同入口；不保证所有 URL 匹配。'),
('02','1001537','Follow Relevant News For Your Region','Edition on Web/Mobile','地区选择与主题按钮；不是实际操作。'),
('03','5428225','Set Up Your Local News Feed','Web/Mobile; customization tips','本地覆盖不足会补较大地区内容。'),
('04','1499905','Hide Unwanted Topics from Your News Feed','Mobile/Web; remove from Show Me Less','无选择取消不保存；可管理撤回。'),
('05','911873','Remove Your Followed Topics & Interests','Mobile; iPad; Web','取消关注不等于屏蔽；趋势栏限制。'),
('06','916353','Remove a News Source on Mobile','Full Coverage; Remove Source; Update','移动屏蔽同步到已登录网页。'),
('07','866177','Custom Web Feeds','Key Features; How to Create','包含/排除语义及确认后刷新；Web Premium/Vantage。'),
('08','7212801','Universal Web Source Filters','Preferences; Overview','全局默认与事件内临时筛选不同。'),
('09','3523713','Universal Mobile Paywall Filter','Enable/Disable','Premium/Vantage；作用于 Full Coverage。'),
('10','898305','Customize App Notifications','Android; iPhone; Daily News Update','每日提醒开关；静默建议不等于应用内定时器。'),
('11','3405953','Translate Foreign Language Articles','Single; automatic; web; language filtering','移动翻译与浏览器翻译分开；不支持语言来源排除。'),
('12','1096641','Reduce Ad Interference','External Browser icon; FAQ','保留官方 External Browser/Reader View 称呼；不保证全部去广告。'),
('13','4663041','Exploring Podcasts and Opinions','What Is; How to Find; FAQ','节目跳原平台；不是原生新闻朗读。'),
('14','225473','Getting Started Tutorial','Subscription access; Note About Internet Access','明确网页和移动均不支持离线浏览。'),
('15','7541441','What Features Are Included With Ground News Vantage?','My News Bias; FAQs','功能和账号保存边界；移动未上线承诺不计入现行。'),
('17','244865','My Subscription Is Not Recognized','Purchase channels; Sign Out; Restore In-App Purchase','恢复购买被官方警告会重新激活已取消订阅；未执行。'),
('18','1858561','Enable Dark Mode','Website; mobile website; mobile app','主题设置平台路径不同。'),
('19','1750529','Our Browser Extension','Download for Free; Key Features','浏览器范围与比较能力；未安装。'),
]
dates = {'03':'2026-03-11','05':'2026-04-15','14':'2026-03-16','17':'2026-08-29'}
for number, article, title, locator, note in sources:
    extra = {'source_updated_on':dates.get(number,'2026-03-09')}
    if number=='14': extra['qualifies']='E-GRND-02'
    evidence('E-GND-'+number,'M-046',title,'https://help.ground.news/en/articles/'+article,locator,note,**extra)
evidence('E-GND-16','M-046','My News Bias — public demo data',
    'https://ground.news/my-news-bias-vantage','Demo Data; Stories/Articles; analysis fields',
    '仅读示例页文字，不是实际账号记录，未测试筛选或图表。',
    source_type='official_public_demo_text', access_mode='opened_public_demo_text',
    version_scope='Public Web demo; demo dataset date/build unknown', source_updated_on=None)
records.sort(key=lambda r:r['evidence_id'])
(batch/'news-app-deepening-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in records))

flows = []
def flow(pid, num, task, depth, ids, known, gaps, visual='not_observed'):
    case = 'M-045-espresso.md' if pid=='M-045' else 'M-046-ground-news.md'
    flows.append(dict(flow_id=f'F-{pid.replace("-", "")}-{num:02}',product_id=pid,task=task,depth=depth,
        evidence_ids=ids,known_branches=known,gaps=gaps,visual_status=visual,hands_on='false',
        case_path='cases/'+case,checked_on='2026-10-26'))
flow('M-045',5,'设置周度选题偏好','D2','E-ESD-02;E-ESPR-01','机构指南 Settings 到 For you','选项、确认、生效与当前平台')
flow('M-045',6,'按时间提示选择读或听','D1','E-ESD-01','静态阅读和音频入口','播放结果、暂停结束、精确返回恢复','static_official_images_only')
flow('M-045',7,'设置显示与通知','D2','E-ESD-02','机构指南 Settings 选项','保存、通知系统权限、跨重启状态')
flow('M-046',5,'地区和本地信息入口','D2','E-GND-02;E-GND-03','地区主题按钮；本地手动搜索；覆盖不足补充','定位拒绝界面和实际生效')
flow('M-046',6,'搜索事件并追踪 In Focus','D2','E-GND-01;E-GRND-02','标题关键词 URL；覆盖与来源入口','搜索穷尽程度；外链返回位置')
flow('M-046',7,'取消主题关注','D2','E-GND-05','手机 iPad Web 路径；趋势栏不受控','立即重排和同步细节')
flow('M-046',8,'减少主题与屏蔽媒体后回改','D2','E-GND-04;E-GND-06','取消不保存；管理回改；屏蔽跨 Web','撤回按钮与恢复确认')
flow('M-046',9,'建立有条件的专题流','D2','E-GND-07','Web Premium/Vantage；条件 Confirm 刷新','编辑删除、空结果和移动当前版本')
flow('M-046',10,'设置跨事件默认筛选','D2','E-GND-08;E-GND-09','Web 全局默认；移动付费墙开关','与单事件条件优先级')
flow('M-046',11,'翻译与切换阅读视图','D2','E-GND-11;E-GND-12','单篇/自动翻译；External Browser','原文返回、译文保存、加载失败')
flow('M-046',12,'收听或观看事件相关节目','D2','E-GND-13','外部平台与可用时间戳','外站权限、中断、返回与续播')
flow('M-046',13,'开关每日新闻提醒','D2','E-GND-10','Android/iPhone 路径','精确静默、系统权限、成功反馈')
flow('M-046',14,'保存离开后回访与离线边界','D2','E-GND-14;E-GRND-02;E-GRND-06','Saved Stories；不支持离线','已读阈值和撤销、断网恢复、滚动位置')
flow('M-046',15,'检视个人信息范围','D1','E-GND-15;E-GND-16','Vantage Web；公开示例字段','真实个人入口、统计阈值、更新和下钻')
flow('M-046',16,'辨别账号和渠道以恢复权益','D2','E-GND-17','登录方式；商店恢复副作用；错误渠道加载','实际恢复结果和状态反馈')
with (batch/'news-app-deepening-coverage.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(flows[0]));writer.writeheader();writer.writerows(flows)
print(json.dumps({'evidence':len(records),'flows':len(flows),'artifacts':images},ensure_ascii=False))
