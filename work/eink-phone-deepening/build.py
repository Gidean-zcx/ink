from pathlib import Path
import csv
import json
import re

ROOT = Path('/home/workspace/reader')
BASE = ROOT / 'docs/research/interaction-library'
WORK = ROOT / 'work/eink-phone-deepening'
DAY = '2026-10-26'
evidence = []

def add(eid, product, title, url, source_type, access_mode, version, locator,
        note, updated=None, firmware=None):
    evidence.append(dict(evidence_id=eid, product_id=product, title=title,
        url=url, checked_on=DAY, source_type=source_type, access_mode=access_mode,
        version_scope=version, app_build=None, firmware_version=firmware,
        source_updated_on=updated, locator=locator, note=note,
        hands_on=False, visual_observed=False, archived_artifact=None, sha256=None,
        capture_state='bibliography_and_short_note_only', supersedes=None))

bigme_scope = 'Bigme HiBreak Pro B/W 6.13-inch, Android 14; not HiBreak original, Pro Color, Dual or Dual2; manual firmware unknown'
manual_url = 'https://device.report/m/f9c833a5a200f1b7aad371249f065497aa4c9e0caf1023379311412b5579ff50.pdf'
add('E-BGD-01', 'M-011', 'HiBreak pro厂商手册：桌面、分屏、浮球与目录补查', manual_url,
    'manufacturer_manual_mirror', 'opened_pdf_text', bigme_scope,
    '26页，13章；本轮重点PDF p8（桌面）、p13（分屏）、p14（浮球）；p23版本差异声明。',
    '与E-BIGME-02同一文件，非独立交叉验证。截图调用未取得可观察图像；未实操或下载固件。')
add('E-BGD-02', 'M-011', 'HiBreak Pro自定义编辑下滑菜单栏：教程元数据',
    'https://www.bilibili.com/video/BV1vqR2YiEnK/', 'brand_publisher_video_metadata',
    'search_index_text_direct_open_412', bigme_scope,
    '索引题名/作者Bigme智能办公本/2025-04-07；未播放。',
    '发布者使用品牌名称，账号认证未独立核验；直接打开412后停止。仅D1教程主题，不能补出排序/保存步骤。', '2025-04-07')
add('E-BGD-03', 'M-011', 'HiBreak Pro指纹解锁：教程元数据',
    'https://www.bilibili.com/video/BV1GnR1YbE3F/', 'brand_publisher_video_metadata',
    'search_index_text_only', bigme_scope,
    '索引题名/作者Bigme智能办公本/2025-04-07；说明列密码/指纹/人脸解锁。',
    '未播放或观察帧；账号认证未独立核验；题名“详细操作步骤”不等于已取得步骤。', '2025-04-07')
add('E-BGD-04', 'M-011', '用户Joemk888的HiBreak Pro B/W v2.9.6后台设置',
    'https://www.reddit.com/r/Bigme/comments/1qvhbzy/best_tips_settings_for_hibreak_pro_bw_v296/',
    'first_person_user_report', 'opened_text',
    'HiBreak Pro B/W; user-reported v2.9.6; Spotify build unknown; not current-firmware verification',
    'Permissions & background behaviour→Spotify；两处后台策略入口。',
    '个人经验，不是官方修复。未采用全部权限建议、改装或性能承诺；没有受控重测、等待时长或来电恢复证据。',
    firmware='v2.9.6 (self-reported by source author)')

touch_scope = 'Hisense TOUCH original 2021, 4GB+128GB, Android 11 / TOUCH UI; firmware unknown; not TOUCH Lite'
reader_scope = 'Hisense Hi Reader original 2022, 6.7-inch 4GB+64GB; review reports TOUCH OS1 / Android 10; firmware unknown; not Pro or 2026'
a9_scope = 'Hisense A9 original 2022, 6GB+128GB where stated; Android 11; firmware unknown; not A10 or 2023 forum build'
add('E-HSD-01', 'M-055', '海信商城TOUCH黛青4+128参数',
    'https://mall.hisense.com/items/4396', 'official_product_page', 'opened_text', touch_scope,
    '商品标题、包装及参数：Android11/TOUCH UI、AI键与音频接口；下架销售状态。',
    '官方功能/型号D1，不提供文件到播放完整路径；详情图未观察，下架不推导停止维护。')
add('E-HSD-02', 'M-055', '海信商城原版Hi Reader 6.7英寸',
    'https://mall.hisense.com/items/4768', 'official_product_page', 'opened_text', reader_scope,
    '商品标题：6.7英寸玻璃盖板4G+64G金属灰；下架销售状态。',
    '4G+64G是容量写法，不能作为蜂窝网络声明。正文可读，详情图未观察。')
add('E-HSD-03', 'M-055', '海信移动资料下载：旧型号资料获取缺口',
    'https://mobile.hisense.com/yidongziliao/', 'official_support_index', 'opened_text',
    'Current public support portal checked 2026-10-26; not a model firmware guide',
    '服务支持→资料下载；本次返回正文的栏目/导航。',
    '返回正文未出现A9/TOUCH/原版Hi Reader下载条目；不能证明所有渠道没有资料或停止维护。未留言或联系。')
add('E-HSD-04', 'M-055', '海信移动现行墨水屏目录：排除代际混合',
    'https://mobile.hisense.com/jixiewujinjian/', 'official_product_index', 'opened_text',
    'Current index lists A10 / Hi Reader 2026; excluded from this historical model case',
    '产品中心→墨水屏系列，A10、Hi Reader 2026条目。',
    '仅型号边界证据，不新增研究对象或把现行产品功能带入历史案例。')
add('E-HSD-05', 'M-055', 'IT之家：原版Hi Reader交互一手评测',
    'https://www.ithome.com/0/611/514.htm', 'first_person_editorial_review', 'opened_text', reader_scope,
    '2022-04-06作者马卡；第三部分“交互”：速记、智慧识屏收藏、阅读菜单。',
    '商业编辑部称取得样机；本项目只读文字，未观察图像/视频。只复述局部动作，未采用健康、性能、全app兼容性评价。', '2022-04-06')
add('E-HSD-06', 'M-055', '木木sowhat：原版TOUCH音阅体验',
    'https://post.smzdm.com/p/a3d03kor/', 'first_person_user_review',
    'search_index_text_direct_open_timeout', touch_scope,
    '2021-05-31；Hi-Fi级的音乐体验/简洁、易用的定制系统；作者自述体验。',
    '搜索索引正文可读，直接打开超时；未观察照片。保留D1入口与本地音乐发现，不补MTP/扫描/队列步骤或采纳音质健康结论。', '2021-05-31')
add('E-HSD-07', 'M-055', '王益友A9一手体验的授权转载',
    'https://www.e-paper.org.cn/?p=19129', 'first_person_user_review_authorized_reprint',
    'search_index_text_direct_open_timeout', a9_scope,
    '页面署来源知乎@王益友、2022-06-13、6+128GB；阅读体验§1屏幕显示、§4音量翻页、§5墨智键。',
    '转载页注明授权；原知乎页面未取得，非两份独立验证。仅记录漂白/条件式锁屏阅读路径；未采纳速度、续航、全应用适配或购买建议。', '2022-06-13')
add('E-HSD-08', 'M-055', '天极网A9首发系统范围评测',
    'https://mobile.yesky.com/283/725998783.shtml', 'first_person_editorial_review', 'opened_text',
    'Hisense A9 original 2022; Android 11; exact review unit RAM/firmware not fixed',
    '2022-07-01手机频道；系统/自行安装APP/锁屏与显示模式段。',
    '只补原F-M055-01的D1范围，不据评测形容词承诺性能、护眼或现代应用兼容性。', '2022-07-01')
add('E-HSD-09', 'M-055', 'DealiAxy：原版Hi Reader首次进入系统线索',
    'https://sblt.deali.cn/blog/eink-hisense-hi-reader', 'first_person_user_report', 'opened_text',
    'Hi Reader original unboxing section, 2023; exact firmware unknown; later TOUCH Lite/A9 comparison excluded',
    '2023-11-02；进入系统/进入桌面/设置界面；首启密码和墨水屏控制引导的文字说明。',
    '图片为未加载占位，未读机内引导图。仅记录取证线索；不采用后段其他机型、处理器或团队解散传闻。', '2023-11-02')

coverage_fields = next(csv.reader((BASE / 'coverage.csv').open()))
rows = []
files = []
for product, slug, draft, first_new in [('M-011','bigme','bigme.md',8), ('M-055','hisense','hisense.md',3)]:
    text = (WORK / draft).read_text()
    case_path = BASE / f'cases/{product}-{slug}.md'
    case_path.write_text(text)
    files.append(str(case_path.relative_to(ROOT)))
    for line in text.splitlines():
        if not re.match(r'^\| F-M\d{3}-\d{2} \|', line):
            continue
        fields = [s.strip() for s in line.strip('|').split('|')]
        assert len(fields) == 7, fields
        fid, task, before, action, data, eids, gaps = fields
        if int(fid.rsplit('-',1)[-1]) < first_new:
            continue
        depth = re.search(r'D[12]', task).group()
        rows.append(dict(flow_id=fid, product_id=product, task=task, depth=depth,
            evidence_ids=';'.join(re.findall(r'E-[A-Z]+-\d{2}', eids)),
            known_branches=f'前置：{before}；路径：{action}；状态：{data}', gaps=gaps,
            visual_status='not_observed', hands_on='false',
            case_path=f'cases/{product}-{slug}.md', checked_on=DAY))

e_path = BASE / 'batches/eink-phone-deepening-evidence.jsonl'
e_path.write_text(''.join(json.dumps(e, ensure_ascii=False) + '\n' for e in evidence))
c_path = BASE / 'batches/eink-phone-deepening-coverage.csv'
with c_path.open('w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=coverage_fields)
    writer.writeheader()
    writer.writerows(rows)
files += [str(e_path.relative_to(ROOT)), str(c_path.relative_to(ROOT))]
print(json.dumps({'files_written':files, 'evidence':len(evidence), 'new_flows':len(rows)}, ensure_ascii=False))
