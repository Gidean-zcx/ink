from pathlib import Path
import csv,json,hashlib,shutil,zipfile
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';work=root/'work/time-controls-deepening';raw=work/'browser';art=lib/'artifacts/time-controls-deepening';browser=art/'browser';browser.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
steps=[
('01-article-initial','直接打开公开K–1植物文章，HTTP200','Read Aloud、西语、打印入口；尚未展开播放器'),
('02-read-aloud-start','点击Listen to audio，等2秒','出现Pause、.9x和0:48；标题/段落高亮，随后页面随进度滚动'),
('03-pause','间隔后点击播放/暂停控件','实际是Pause、0:49；文件名是当时意图，不能称暂停成功'),
('04-pause-later','再次截帧','仍Pause，截图0:21、后取DOM0:20，range=60；位置已到后文'),
('05-speed-menu','点语速入口','显示.5x/.75x/.9x/1x/1.25x；未选新档位'),
('06-paused-confirmed','点击Pause，等300ms','显示Play、0:01、range=100；接近末尾，不能分离暂停与自然结束'),
('07-after-short-play-attempt','点击Play后等2.5秒，再找Pause超时，之后截帧','显示Play、0:49、range=0；没有记录到该次预期暂停，保留失败'),
('08-replay-running','再次点Play，等1.8秒','Pause；截图0:47，后取DOM0:46，range=10；自动回到前文'),
('09-controlled-pause','立即点击Pause','Play、0:46、range=10'),
('10-pause-stability','再等1.5秒','仍Play、0:46、range=10，暂停状态稳定'),
('11-speed-menu-reopened','点击语速并查找button类型1.25x失败后，再点入口','本帧未见选项；只读结构核对，不作视觉结论'),
('12-speed-hover','尝试悬停语速控件','错误button选择器仍未命中；只读结构核对，不作视觉结论'),
('13-speed-menu-current','再次点击语速；读取实际role','菜单显示；选项实际role=radio，不是button'),
('14-speed-selected','点击radio类型1.25x','语速文字/aria改1.25x；仍Play、range=10、0:46'),
('15-seek-forward-paused','聚焦进度条并按Enter','range从10变20，仍Play；时间文字变0:08，不统一解释为剩余时间'),
('16-close','点击关闭播放器','显示Read Aloud；隐藏DOM仍含旧进度与语速，不能当可见或已持久保存'),
('17-reopen','再次点Read Aloud，等1.2秒','展开并播放，回标题；range=0、.9x、0:48，未延续20/1.25x'),
('18-reopen-paused','立即点Pause','Play、range=0、.9x、0:48'),
('19-reload-closed','刷新并等1.3秒','播放器收起；此前range已是0，单独此帧不能证明丢失非零位置'),
('20-reload-play','点Read Aloud，等1秒','Pause、range=0、.9x、0:48'),
('21-reload-pause','点Pause','Play、range=0、.9x、0:48'),
('22-spanish','点En Espanol，等1.5秒','URL带lang=es，标题/正文为西语，播放器收起；有In English'),
('23-spanish-play','在西语页点Read Aloud，等1.6秒','Pause、0:57、.9x，西语标题高亮；没有实际听音，不能判断声音语种或质量'),
('24-spanish-pause','点Pause','Play、0:57、range=0；只核界面状态'),
('25-before-reload-nonzero','点In English，启动后暂停；进度条Enter三次，选择1.25x','英文URL带lang=en；Play、range=30、1.25x、0:16，建立非零对照'),
('26-after-reload-nonzero','刷新，再启动1秒并暂停','Play、range=0、.9x、0:48；非零位置和语速未恢复'),
('27-search-entry','点Search','同网址展开全屏输入层；Enter提交提示；HTML中的原文仍在，不代表可见'),
('28-search-plants','输入plants并Enter，等1.8秒','进入/k1/?text=plants，K–1选中；首条植物、第二条动物等，有Audio/Spanish标识'),
('29-search-empty','首次定位两个同placeholder输入失败；改用searchbox输入zzqvnonexistent并Enter，等1.5秒','URL和搜索框带该字符串，仍有植物/真菌等普通文章；未见明确无结果提示。文件名不代表空结果'),
('30-search-cleared','清空可见searchbox并Enter，等1.2秒','URL text为空，K–1保留；首屏植物/真菌'),
('31-return-article','点击植物文章标题，等1.2秒','回英文正文顶部，播放器收起；未见已读或任务完成记录')]
selected={2,4,7,9,10,13,14,15,16,17,23,25,26,27,28,29,30,31}
states=[]
for i,(name,action,result) in enumerate(steps,1):
 p=raw/(name+'.png');m=json.loads((raw/(name+'.json')).read_text())
 row=dict(capture=name,action=action,observed_result=result,url=m['url'],captured_at_utc=m['captured_at'],raw_image_path=str(p.relative_to(root)),sha256=sha(p),image_reviewed=i not in {11,12},playback_ui=m.get('playback_ui'),raw_metadata_path=str((raw/(name+'.json')).relative_to(root)),raw_metadata_sha256=sha(raw/(name+'.json')),raw_html_sha256=sha(raw/(name+'.html')))
 if i in selected:
  dst=browser/p.name;shutil.copy2(p,dst);row['archived_image']=str(dst.relative_to(lib))
 states.append(row)
dump(browser/'observed-states.json',states)
with zipfile.ZipFile(raw/'trace.zip') as z:es=[json.loads(x) for x in z.read('trace.trace').decode().splitlines()]
af={x['callId']:x for x in es if x['type']=='after'}
actions=[]
for e in es:
 if e['type']!='before' or e['method'] not in {'goto','click','reload','screenshot','waitForTimeout','press','fill','focus','hover'}:continue
 a=af.get(e['callId'],{});actions.append(dict(method=e['method'],class_name=e['class'],params=e.get('params',{}),start_ms=e['startTime'],end_ms=a.get('endTime'),error=a.get('error')))
dump(browser/'trace-actions.json',dict(origin_trace_sha256=sha(raw/'trace.zip'),context_metadata=es[0],actions=actions))
dump(browser/'raw-trace-manifest.json',dict(path=str((raw/'trace.zip').relative_to(root)),sha256=sha(raw/'trace.zip'),bytes=(raw/'trace.zip').stat().st_size,screenshots_in_trace=False,snapshots_in_trace=False,formal_screenshots=len(selected)))
shutil.copy2(raw/'browser-errors.json',browser/'browser-errors.json')
audit=art/'browser-operation-audit.md'
lines=['# M047：K–1公开文章听读与查找操作核查','','观察日2026-10-26。F-M047-01、F-M047-04；E-TKCD-01、E-TKCD-02、E-TKCD-03。','',
'根代理用Playwright实际操作公开网页。D4/hands_on=true仅限此桌面浏览器任务，不代表原生App、手指触摸、电子纸实机、儿童完成或整个任务通过。未登录、未提交年龄/课堂作业，也未打印或下载音频文件。','',
'环境：Linux、Chromium Headless Shell156.0.8078.4、1280×950、新建匿名上下文；Playwright包1.64.1，trace内标1.64.0-beta-1791568591000。前端构建未知。正常公开[植物文章](https://www.timeforkids.com/k1/spotlight-on-plants-k1/)初始HTTP200；文章2026-10-16。英语/西语是这篇内容的两个入口，不代表每篇具有同样工具。','',
'**未实际听音。** 证据是可见播放控件、计时/进度、高亮、页面滚动和语言正文。DOM未找到audio/video节点，不能以空列表断言没有声音。声源、准确发音、声音语种、语速听感及同步精度未知。','',
'## 顺序记录','',f'共31次捕获、{len(selected)}张必要截图正式归档，29帧作视觉判读；11/12仅核结构。截图与随后DOM读取存在时间差，不把一秒差值当产品问题。','',
'| 捕获 | 动作 | 观察与限制 |','| --- | --- | --- |']
for i,s in enumerate(states,1):
 name=s['capture'];label=f'[{name}](browser/{name}.png)' if i in selected else name+'（原件work）'
 lines.append(f'| {label} | {s["action"]} | {s["observed_result"]} |')
lines+=['','## 失败与解释边界','',
'日志含3次5秒定位超时：近末尾重放后寻找Pause一次；误将语速radio按button查找两次。另有一次两个同placeholder输入导致严格匹配失败。纠正后使用正常控件完成；没有强制点击、改页面业务变量或直接调播放/搜索接口。这些定位错误不是产品故障。','',
'页面错误日志有9次Load error及一次音频请求ERR_ABORTED；没有确定其原因，不能据此宣称完整音频链可靠，也不能把所有错误解释为音频失败。刷新/导航可能取消请求，但本次没有因果定位。失败原始记录保留。','',
'03/04的文件名含pause但实际仍在播放；06位于末尾；真正可比较的暂停是08→09→10。19的刷新前range为0，不用于证明非零断点丢失；有效刷新对照是25→26。29名称含empty但画面非空。','',
'进度条range的值按0–100读取；播放中时间文字下降，但暂停后按Enter时出现0:08或0:16，不把该控件在所有状态统一称为剩余秒数。没有检查其内部计时公式。关闭和刷新各做了一次有效对照；不能推广为所有文章、账户或浏览器的规则。','',
'搜索不命中语义的字符串仍返回普通文章，是本次观察；原因、服务端是否执行搜索、分词/回退规则和更多等待后的变化均未知。没有把结果页URL当检索成功。','',
f'[动作日志](browser/trace-actions.json)含{len(actions)}条所选调用，其中31次截图、22次等待，并非{len(actions)}次点击；[观察状态](browser/observed-states.json)、[原trace散列](browser/raw-trace-manifest.json)及[页面错误](browser/browser-errors.json)一并保存。原trace没有启用连续截图/快照，没有视频；work原件不承诺长期保留。','']
audit.write_text('\n'.join(lines))
e=[]
for i,title,url,loc,note in [
(1,'公开英文文章朗读控制、暂停与关闭重开','https://www.timeforkids.com/k1/spotlight-on-plants-k1/','01–21','观察启动、高亮滚动、稳定暂停、语速选择、进度键控；关闭重开未延续进度及语速；未实际听音。'),
(2,'同篇语言切换与非零位置刷新对照','https://www.timeforkids.com/k1/spotlight-on-plants-k1/?lang=es','22–26','西语正文可见且有播放器运行/暂停状态；返回英文设置range30/1.25x后刷新，重新启动为range0/.9x；声音语种未核。'),
(3,'K–1搜索入口、普通与无意义查询、清空返回','https://www.timeforkids.com/k1/?text=plants','27–31','搜索层→K–1结果；无意义字符串仍见普通文章而非明确空结果；清空后从标题回正文。未核全部检索规则。')]:
 e.append(dict(evidence_id=f'E-TKCD-{i:02}',product_id='M-047',title=title,url=url,checked_on='2026-10-26',source_type='public_web_application',access_mode='automated_public_browser_actions_and_screenshot_review',version_scope='K–1 official public desktop web; anonymous Chromium156.0.8078.4; build unknown; no device or child test',app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=True,visual_observed=True,visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=str(audit.relative_to(lib)),operation_audit_sha256=sha(audit),archived_artifact=str(audit.relative_to(lib)),sha256=sha(audit),capture_state='operation_audit_actions_and_selected_screenshots_archived',supersedes=None,artifact_hashes={str(p.relative_to(lib)):sha(p) for p in browser.iterdir() if p.is_file()}))
(lib/'batches/time-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in e))
updates={
'F-M047-01':dict(depth='D4',evidence_ids='E-TFK-02;E-TFK-03;E-TKCD-02;E-TKCD-03',known_branches='同篇西语/英文切换；Search展开输入层后进入K–1结果；无意义查询仍见普通文章；清空并从标题返回正文。',gaps='仅桌面匿名该篇；打印、所有年级/主题/分页、已读/精准续读、检索失败原因及课堂分配未知。',visual_status='observed_browser_sequence',hands_on='true'),
'F-M047-04':dict(task='公开文章朗读控制、关闭与刷新恢复',depth='D4',evidence_ids='E-TFK-03;E-TKD-01;E-TKCD-01;E-TKCD-02',known_branches='运行/暂停、高亮与自动滚动；5档语速菜单，实际选1.25x；Enter改进度；暂停位置保持，关闭重开与非零刷新对照均回起始/.9x。',gaps='未实际听音；声音语种/质量/同步精度、离线/失败重试/音量/跨设备未知；仅单篇匿名网页，日志错误未定位。',visual_status='observed_browser_sequence',hands_on='true')}
changed=[]
for fname in ['news-coverage.csv','child-information-deepening-coverage.csv']:
 p=lib/'batches'/fname;rows=list(csv.DictReader(p.open()));fields=list(rows[0])
 for row in rows:
  if row['flow_id'] in updates:
   assert row['depth']=='D2' and row['hands_on']=='false';changed.append(dict(batch=fname,before=dict(row),updates=updates[row['flow_id']]));row.update(updates[row['flow_id']])
 with p.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(rows)
assert len(changed)==2;dump(work/'applied-existing-coverage-updates.json',changed)
print(json.dumps(dict(states=31,archived_screenshots=len(selected),calls=len(actions),operation_errors=sum(bool(x['error']) for x in actions),new_evidence=3,updated_flows=2),ensure_ascii=False))
