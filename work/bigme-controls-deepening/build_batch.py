from pathlib import Path
import csv,json,hashlib,copy,shutil
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/bigme-controls-deepening';B=L/'batches';A=L/'artifacts/bigme-controls-deepening/source-notes.md'
assert not (W/'build-result.json').exists(),'one-time builder'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
for n in range(1,4):shutil.copyfile(W/f'raw/tts-demo/frame-{n:02}.png',A.parent/f'tts-frame-{n:02}.png')
caps=json.loads((W/'captures.json').read_text());news=json.loads((W/'news-capture.json').read_text());gifs=json.loads((W/'gif-captures.json').read_text());gif=gifs[1]
assert caps[0]['status']==403 and not caps[0]['pdf_header'] and caps[1]['status']==200 and news['status']==200 and gif['status']==200
base=dict(product_id='M-011',checked_on='2026-10-27',app_build=None,firmware_version=None,source_updated_on=None,hands_on=False,archived_artifact=str(A.relative_to(L)),sha256=sha(A),supersedes=None)
e1=dict(base,evidence_id='E-BGCD-01',title='Bigme 80FPS announcement: other-model scope exclusion',url=news['url'],source_type='official_news',access_mode='public_html_get_and_text_read',version_scope='HiBreak Pro Color / HiBreak Dual / B7 Pro series, excluded from B/W case',source_published_on='2026-06-11',date_origin='official blog listing',locator='opening model list and performance claims',note='厂商声明未附测量方法；不是M011黑白版固件证据，也不证明Plus性能',visual_observed=False,capture_state='raw_html_and_scope_note',raw_work_path=news['raw_work_path'],raw_sha256=news['raw_sha256'])
e2=dict(base,evidence_id='E-BGCD-02',title='HiBreak product GIF: music, dictionary and font promotional frames',url=gif['url'],source_type='official_product_image',access_mode='public_gif_get_and_all_three_extracted_frames_viewed',version_scope='product-page promotional composite; displayed app/build unidentified',locator='GIF frames 1/2/3 in file order; not a continuous action sequence',note='文件名含TTS不代表正文为听书；三帧实际看过；仅静态参考，不测刷新率',visual_observed=True,visual_kind='static_promotional_frames',derived_from='E-BIGME-01',capture_state='gif_work_original_and_three_formal_png_frames',raw_work_path=gif['raw_work_path'],raw_sha256=gif['raw_sha256'],artifact_hashes={str(p.relative_to(L)):sha(p) for p in sorted(A.parent.glob('tts-frame-*.png'))})
es=[e1,e2];(B/'bigme-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
old=json.loads((W/'coverage-before.json').read_text());changes={
 'F-M011-01':dict(known_branches='单击/长按各10项映射；底部左/中/右手势各10项；导航、清缓存和全刷语义分开',gaps='PDF p3–7；保存/还原、锁屏响应、映射冲突；不从其他固件补双击'),
 'F-M011-02':dict(known_branches='系统右上控制中心→E Ink Center的4预设/Custom与阅读页Refresh Settings分列；App optimization改字号/文本增强/图标及背景',gaps='PDF p11–12/19；系统与书内设置是否同步、逐App保存、恢复默认、光学残影/延迟；21F/S宣传及其他型号80FPS均非实测'),
 'F-M011-03':dict(known_branches='Books自动扫描；Import→Local从Books/Download或WiFi导入；阅读Settings显示批注/书签导出路径，Contents浏览/导出/删除',gaps='PDF p18–19；传输完成与解析成功分开；重复/坏文件、导出命名与覆盖、删除确认/回收及位置恢复未知'),
 'F-M011-06':dict(known_branches='文本和PDF的Progress都列听书；产品页在线TTS声明；文件名TTS的宣传GIF实际为音乐/词典/字体，不补朗读链',gaps='PDF p20–21；读听启动、暂停/续句、下载/计时/语音、锁屏/来电/蓝牙断开；无TTS控件实观'),
 'F-M011-07':dict(evidence_ids='E-BIGME-01;E-BIGME-02;E-BGCD-02',known_branches='阅读Settings列长按菜单/词典；宣传静态帧可见词典及图标，但选区→保存→再定位链未证实',gaps='PDF p19与宣传帧2；应用/构建、图标执行语义、词典离线、批注定位/取消/导出仍缺',visual_status='static_reference_only')}
props=[]
for x in old:
 if x['flow_id'] in changes:
  y=copy.deepcopy(x);y.update(changes[x['flow_id']],checked_on='2026-10-27');props.append(dict(flow_id=x['flow_id'],before=x,after=y))
dump(W/'existing-coverage-updates.json',props)
f=dict(flow_id='F-M011-12',product_id='M-011',task='分别调整普通文本与PDF的阅读版式',depth='D2',evidence_ids='E-BIGME-02;E-BGCD-02',known_branches='书内底部Format：文本字号/间距；PDF分页/缩放/裁边/旋转/边距；Contrast与Navigation另列。宣传字体帧无确定入口，只作静态参考',gaps='PDF p20–21；保存/撤销、作用于本书还是全局、源文件是否改动、横竖屏与返回位置、Navigation重排具体行为未知',visual_status='static_reference_only',hands_on='false',case_path='cases/M-011-bigme.md',checked_on='2026-10-27')
with (B/'bigme-controls-deepening-coverage.csv').open('w',newline='') as h:w=csv.DictWriter(h,fieldnames=list(old[0]));w.writeheader();w.writerow(f)
ee={x['evidence_id']:x for x in json.loads((W/'evidence-before.json').read_text())};updates=[]
for eid in ['E-BIGME-01','E-BIGME-02','E-BGD-01']:
 cap=dict(checked_on='2026-10-27',url=ee[eid]['url'],archived_artifact=str(A.relative_to(L)),sha256=sha(A),hands_on=False)
 if eid=='E-BIGME-01':
  cap.update(capture_method='public_html_get_and_text_read',raw_work_path=caps[1]['raw_work_path'],raw_sha256=caps[1]['raw_sha256'],http_status=200,note='同页规格冲突保留；引用GIF另记E-BGCD-02，不将宣传图当流程')
 else:
  cap.update(capture_method='public_web_tool_pdf_text_read; direct_download_failed_403_stopped',http_download_status=403,failed_body_work_path=caps[0]['raw_work_path'],failed_body_sha256=caps[0]['raw_sha256'],pdf_original_archived=False,note='同一本26页手册文字复读；13章目录登记、p19–21控件分层；无新PDF静态页或设备操作')
 updates.append(dict(evidence_id=eid,before=ee[eid].get('additional_captures'),after=(ee[eid].get('additional_captures') or [])+[cap]))
dump(W/'additional-captures-updates.json',updates)
rows=[]
def row(title,typ,loc,eid,flows,status='text_read_no_operation',gap='操作反馈、退出与持久状态未验证'):
 rows.append(dict(entry_id=f'I-BGCD-{len(rows)+1:03}',scope='HiBreak Pro B/W; manufacturer manual without build' if eid=='E-BIGME-02' else 'source-specific scope',entry_type=typ,title=title,locator=loc,url=ee[eid]['url'] if eid in ee else next(x['url'] for x in es if x['evidence_id']==eid),evidence_id=eid,related_flow_ids=flows,review_status=status,gap=gap,checked_on='2026-10-27'))
chapters=[('规格与连接','p2',''),('实体键','p3–4','F-M011-01'),('手势与桌面','p5–8','F-M011-01;F-M011-08'),('控制中心','p9–10','F-M011-01'),('E Ink Center','p11–12','F-M011-02'),('分屏','p13','F-M011-09'),('浮球','p14','F-M011-10'),('Bigme GPT','p15',''),('V2T','p16','F-M011-04'),('图库','p17','F-M011-05'),('书库与阅读','p18–21','F-M011-03;F-M011-06;F-M011-07;F-M011-12'),('屏保','p22',''),('版本免责声明','p23','')]
for n,(title,loc,flows) in enumerate(chapters,1):row(title,'manual_chapter',f'§{n}; PDF {loc}','E-BIGME-02',flows,'chapter_text_read_not_complete_task')
groups=[('p3–4','F-M011-01',['单击映射10选项','长按映射10选项']),('p5–7','F-M011-01',['边缘内滑返回','底部左后台/中应用中心/右全刷','左/中/右手势各10选项']),('p8','F-M011-08',['长按图标拖动','Edit Screen与卸载角标','App设置入口']),('p9–10','F-M011-01',['右上控制中心','WiFi/移动数据/蓝牙快捷开关']),('p11–12','F-M011-02',['4种预设与Custom','应用优化字号','文字增强','图标颜色/背景颜色']),('p13','F-M011-09',['分屏入口','分隔线和3种比例']),('p14','F-M011-10',['浮球开关','大小与按钮映射']),('p15','',['知识问答声明','主题写作声明','关键词绘画声明']),('p16','F-M011-04',['加号新建文本/语音','MP3导入大小/时长限制','实时转写语言选择']),('p17','F-M011-05',['单/多图转PDF','OCR复制/导出TXT','PDF与OCR输出路径分开','选图作屏保']),('p18','F-M011-03',['15个列举格式名','Books目录自动扫描','Local Import目录限制','WiFi Import入口']),('p19','F-M011-02',['阅读页Refresh Settings']),('p19','F-M011-03;F-M011-07',['Settings导出路径','长按菜单和词典设置','Contents目录/书签/批注','批注书签导出/删除']),('p20','F-M011-06;F-M011-12',['文本Progress听书/自动翻页/日夜','文本Format字号/间距','文本Contrast']),('p21','F-M011-06;F-M011-12',['PDF Progress听书/自动翻页/快览/日夜','PDF Format分页/缩放/裁边','PDF Format旋转/边距/栏距','PDF Contrast淡化水印/对比','PDF Navigation重排含义未明确']),('p22','',['时钟/自选壁纸屏保'])]
for loc,flows,titles in groups:
 for title in titles:row(title,'manual_control_or_group','PDF '+loc,'E-BIGME-02',flows)
for title,loc,flows in [('音乐播放器图形','frame1','F-M011-06'),('词典及图标','frame2','F-M011-07'),('字号/粗细/字体样例','frame3','F-M011-12')]:row(title,'promotional_static_frame',loc,'E-BGCD-02',flows,'viewed_static_not_action_sequence','不能从文件名/图标外形推演完整链；App/构建未知')
row('其他型号80FPS声明','excluded_model_claim','opening model list','E-BGCD-01','','scope_excluded','不是B/W/Plus性能证据，未给可复现实验')
for title,loc in [('CPU/蓝牙规格冲突','main comparison vs lower specs'),('在线TTS与21F/S声明','reading and refresh promotion')]:row(title,'product_claim_scope',loc,'E-BIGME-01','','claim_only','宣传不是操作/测量；保留内部冲突')
with (B/'bigme-controls-deepening-inventory.csv').open('w',newline='') as h:w=csv.DictWriter(h,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
case=(W/'case-before.md').read_text().replace('查阅2026-10-26。','初查2026-10-26，续查2026-10-27。',1)
case=case.replace('**D1+D2；无实机、无视频播放、无连续视觉记录。**','**D1+D2；无实机、无视频播放、无连续操作记录。** 续查实际查看产品GIF静态帧，范围另列。')
case=case.replace('全部查于2026-10-26；无实操、无新增静态图像。旧证据继续有效，新批不覆盖旧记录。','以下旧批查于2026-10-26，当时无实操或新增静态图像；2026-10-27续查和静态宣传帧另列，旧来源继续保留。')
case+='''

## 2026-10-27：完整手册目录、排版层级与宣传图核对

新增[逐条目录](../batches/bigme-controls-deepening-inventory.csv)，登记13章、正文控件组、已看宣传帧与范围排除；章节和子项重叠，不当完整功能数量。独立[取证摘记](../artifacts/bigme-controls-deepening/source-notes.md)说明PDF文字与下载403的区别、官网规格冲突、全部图帧的实际内容。没有下载成功的PDF原件或新手册截图。

| F编号 | 本轮精化 | 结果与仍缺的状态 |
| --- | --- | --- |
| F-M011-01 | 单击/长按以及三个底部手势分别登记映射组；p3–7 | 保存、还原和锁屏作用域未知，不从别的固件补双击 |
| F-M011-02 | 系统E Ink Center与书内Refresh Settings分列，App optimization另列 | 预设同名不证明设置共享；逐App保存及视觉结果未测 |
| F-M011-03 | Books自动扫描、Local的Books/Download与WiFi入口分列；Settings显示标记导出路径，Contents执行浏览/导出/删除 | 传输/解析/导出成功分别待核；命名、覆盖、确认和位置恢复未知 |
| F-M011-06 | 文本/PDF Progress均有听书声明；宣传TTS文件名不补流程 | 仍D1，没有看到书籍播放控件或暂停续句过程 |
| F-M011-07 | 词典宣传帧仅相关静态参考；长按菜单入口来自手册 | 仍D1，未观察选择、保存或返回定位 |
| F-M011-12 | **D2文档新增**：书内底部Format，普通文本调字号/间距；PDF调分页、缩放、裁边、旋转、边距；Contrast与Navigation另列 | p20–21。未交代保存/撤销、单书/全局作用、源文件改动、旋转返回位置。宣传字体帧仅静态参考，不能补菜单路径 |

F-M011-12依据E-BIGME-02；E-BGCD-02是官网三帧宣传合成图，已逐帧查看：音乐播放器、词典、字体样例，没有连续触摸过程。另一个5帧GIF只是搜索结果滚动宣传图，四个PNG只是装饰图标，均未计为新增任务。图像编码帧率不用于判断电子纸刷新。

新增E-BGCD-01来自[官方80FPS公告](https://store.bigme.vip/blogs/news/bigme-s-biggest-firmware-upgrade-yet-hibreak-pro-color-hibreak-dual-b7-pro-series-push-fast-refresh-e-paper-up-to-80-fps)，适用Pro Color、Dual、B7 Pro系列；不纳入本黑白版的固件功能。未附光学测试条件，不能证明Plus高刷。官网主区/底表CPU、官网/手册蓝牙不一致也保留，不自行择优。

对G-002的分析：AI改刷新模式、改文本排版、裁边PDF和输出批注属于不同对象/作用范围，应分别查实际结果。对R-018，音乐图标和在线TTS声明仍不足以定义续播。以上为研究启发，未升级为本项目实现要求；本案例仍部分覆盖。
'''
(L/'cases/M-011-bigme.md').write_text(case)
dump(W/'build-result.json',dict(newE=len(es),newF=1,oldF=len(props),old_fields=sum(sum(x['before'][k]!=x['after'][k] for k in x['before']) for x in props),oldE_captures=len(updates),inventory_rows=len(rows),formal_static_images=3,excluded_viewed_images=9,hands_on=False))
print((W/'build-result.json').read_text())
