import json,csv,hashlib,pathlib,xml.etree.ElementTree as ET
ROOT=pathlib.Path('/home/workspace/reader'); W=ROOT/'work/antennapod-controls-deepening'; L=ROOT/'docs/research/interaction-library'; A=L/'artifacts/antennapod-controls-deepening';A.mkdir(parents=True,exist_ok=True)
S=W/'src';SHA='175e5ef8fe3e125a5014d70ef0c57cf85661c71b';URL='https://github.com/AntennaPod/AntennaPod/blob/'+SHA+'/'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def find(name):return next(S.rglob(name))
def rel(p):return str(p.relative_to(ROOT))
manifest=[]
for p in sorted(S.rglob('*')):
 if p.is_file(): manifest.append({'raw_work_path':rel(p),'raw_sha256':digest(p),'source_url':URL+str(p.relative_to(S))})
for p,u in [(W/'release.json','https://api.github.com/repos/AntennaPod/AntennaPod/releases/latest'),(W/'tag-ref.json','https://api.github.com/repos/AntennaPod/AntennaPod/git/ref/tags/3.12.3'),(W/'release-3.12.0.json','https://api.github.com/repos/AntennaPod/AntennaPod/releases/tags/3.12.0'),(W/'documentation-index.json','https://antennapod.org/assets/datasets/documentation_index.json'),(W/'database-error.html','https://antennapod.org/documentation/bugs-first-aid/database-error'),(W/'tree.json','https://api.github.com/repos/AntennaPod/AntennaPod/git/trees/'+SHA+'?recursive=1')]:manifest.append({'raw_work_path':rel(p),'raw_sha256':digest(p),'source_url':u})
(W/'source-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
# Settings manifest is a reproducible source bundle, not a rendered product view.
settings=[r for r in manifest if '/res/xml/preferences' in r['raw_work_path'] or r['raw_work_path'].endswith('/UserPreferences.java') or r['raw_work_path'].endswith('/UserInterfacePreferencesFragment.java') or '/ui/i18n/' in r['raw_work_path']]
(W/'settings-manifest.json').write_text(json.dumps(settings,indent=2)+'\n')
notes='''# AntennaPod 3.12.3：定点静态源码核查记录

查阅日期 2026-10-27。仅文档与代码阅读；未构建、安装、运行、录屏、看产品截图或发送数据。以下“显示/隐藏/暂停”等是代码路径，不是实际观察。正式标签 3.12.3 对应提交 `175e5ef8fe3e125a5014d70ef0c57cf85661c71b`。原件与散列见本案 work 的 source-manifest.json；正式记录保留此分析和功能目录，原始代码不放正式 artifacts。

| E | 文件／定位 | 有限结论与边界 |
| --- | --- | --- |
| E-APCD-01 | release.json、tag-ref.json；app/build.gradle 15–16；release-3.12.0.json | GitHub 正式发布 2026-10-13、非预发布；版本 3.12.3/code 3120395。3.12.0 说明播放服务重写和 Delete podcast 更名 Unsubscribe。未查 APK 与构建产物签名。 |
| E-APCD-02 | 官方 documentation_index.json | 47 条索引（8 分类、39 文章）；只对目录完整性负责，未把整批正文算为已读任务。当前帮助无逐页适用 app 构建。 |
| E-APCD-03 | 11 个 preferences*.xml、英文 strings.xml；UserPreferences 148–152、543；UserInterfacePreferencesFragment 96–102 | 全部 84 个带 key 的设置节点列入 inventory；XML 只证明声明，不保证运行时可见或生效。通知优先级 API26+隐藏、持久控制 API30+隐藏。自动下载上限 XML=25、Java 缺省回退=20，旧网页=50，真实安装默认值未知。About XML 的1.7.2是占位文字，不是本案版本。 |
| E-APCD-04 | common.gradle 15–16；playback/service/AndroidManifest.xml 20–79；PlaybackServiceStarter 44–60 | USE_MEDIA3_PLAYBACK_SERVICE=true；旧服务/旧 QuickSettingsTileService false，新服务 true。旧服务文件仍在树中不代表当前默认路径。依赖 build.gradle media3Version=1.10.0；未审计依赖库全部焦点行为。 |
| E-APCD-05 | CoverFragment 108–111、204–273 | 封面页章按钮：已加载章节非空，或尚未加载但单集标记有章节时显示；前/后章 seek。前章判断使用 10000×speed 的媒体时间窗口；末章隐藏下一章。未验证触摸命中区。 |
| E-APCD-06 | ChaptersFragment 50–87、135–173 | Close、外部章节 Refresh、加载进度、空集关闭并提示；选章可启动暂停媒体并跳起点。异常回调只写日志，不能补造重试提示；无当前媒体路径没有成功事件。 |
| E-APCD-07 | AudioPlayerFragment 347–350、535–536；SleepTimerDialog 147–270、315–419、465–476 | 播放页菜单打开计时底部表单；分钟/集数，设置/停用、延期，当前无播放与0/解析失败提示。关闭连续播放时集数表单不能设置，并给播放设置入口。类型/上次数值/自动时段保存；表单剩余量来自事件。静态标签延期分钟5/10/30、集数1/2/3；未量测实际倒计时。 |
| E-APCD-08 | ClockSleepTimer 34–54、77–142；SleepTimer 7；ShakeListener onSensorChanged；SleepTimerUpdatedEvent 17–35 | 由位置事件触发，扣两次事件到达时System.currentTimeMillis的差；不是position差，间隔>10秒跳过；临近10秒可振动、启用摇动检测，摇动重置初值。不是承诺暂停/休眠/缓冲时的纯墙钟闹钟。硬件无加速度计异常处理未闭合；计时本身只内存字段。 |
| E-APCD-09 | EpisodeSleepTimer 31–65 | 播完才减集数；剩0阻止下一集。最后一集以其剩余媒体时长发渐弱/到期事件，不等于固定墙钟时长。跳集对计数的全部组合未验证。 |
| E-APCD-10 | Media3PlaybackService 339–355、390–428、472–477、504–535、803–879、884–937 | 临时焦点抑制解除可按暂停间隔回退；停播保存位置并排同步动作。位置观察事件驱动计时；到期pause，服务销毁停止计时。连续接播/移动流播确认/无下一项有分支；系统强杀和焦点底层恢复未验证。 |
| E-APCD-11 | ExoPlayerUtils 47–69 | 语音媒体属性、Media3自动焦点处理=true、按偏好处理 noisy。仅能证明配置和委托，不能承诺电话/导航/蓝牙重连结果。 |
| E-APCD-12 | PlaybackPreferencesFragment 38–56 | API31+隐藏耳机和蓝牙重连开关；断开暂停仍有设置。API较低时开关存在不证明新服务有相同旧服务恢复逻辑。 |
| E-APCD-13 | FeedMenuHandler 25–38、62–64；SubscriptionFragment 294–297；RemoveFeedDialog 59–84、119–178、193–206；remove_feed_dialog.xml、strings 207–219 | Unsubscribe/archive 打开表单；Archive保留下载/历史，订阅页Archive入口后可Restore；Unsubscribe先显确认按钮再执行。处理时进度/取消隐藏，成功或错误回调均关闭，错误仅日志；不能据关闭认定成功。 |
| E-APCD-14 | DBWriter 118–167、177–195、212–249、862–884 | 媒体删与订阅删不同；订阅删移出队列、停播、取消下载、删在线源下载和记录，发送待同步移除动作。取消本地文件夹订阅跳过删底层文件；单独删本地媒体则调用DocumentFile.delete。文件删失败可只记日志，不能保证物理文件已消失。归档/恢复改feed state；不等同删除撤销。 |
| E-APCD-15 | ImportExportPreferencesFragment 123–181、189–278、352–389；strings 679 | 3.12.3入口名Backup & restore；DB导入先覆盖警告、可拒绝/取消选择，选择后等待，成功OK重启或错误对话框。OPML/HTML/收藏分别输出；自动DB备份选可写目录持久授权，周期执行未核。 |
| E-APCD-16 | DatabaseExporter 45–105；UserPreferences 148–152 | DB导出只复制SQLite文件；全局偏好在另一个SharedPreferences，不能说全部app设置随DB备份。导入检查数据库schema不能降级，随后删旧DB/移新DB；异常抛回UI，没有据此证明事务回滚。音频不在数据库文件中。 |
| E-APCD-17 | MediaLibrarySessionCallback 113–199 | Session提供快退/快进和可配置速度、下一章、跳集、睡眠按钮，睡眠依活动状态切换设置/停用命令。实际系统可展示数量与布局未观察，不能替代锁屏截图。 |
| E-APCD-18 | 官方 database-error 帮助 Recovery 段 | 文档针对早期库损坏建议备份恢复、修库或重新加入残存媒体文件夹；不是本版正常取消订阅后的回收站。没有执行修复脚本。 |

本批没有新增F；仅补已有任务，并保留原 E-ANTP 证据历史。功能目录声明层 D1 与本轮入口/状态/异常静态重建 D2 区分。原 develop `d3e41c6...` 已不作正式标签音频恢复依据。
'''
(A/'source-notes.md').write_text(notes)
# Complete help index plus keyed global settings declarations.
help_map={'subscribe':('01','E-ANTP-01'),'how-to-use':('02','E-ANTP-02'),'sorting':('03','E-ANTP-03'),'lock':('03','E-ANTP-04'),'automatic-downloads':('04','E-ANTP-05'),'episode-deletion':('05','E-ANTP-06'),'deletion':('05','E-ANTP-07'),'auto-rewind':('06','E-ANTP-08'),'refresh-download-errors':('10','E-ANTP-09'),'cannot-stream-download':('10','E-ANTP-18'),'cannot-resume-playback':('11','E-ANTP-10'),'playback-stops':('11','E-ANTP-11'),'backup':('12','E-ANTP-12'),'synchronization':('12','E-ANTP-13'),'database-error':('12','E-APCD-18')}
fields=['inventory_id','kind','domain','item','source_locator','source_url','version_scope','coverage','flow_ids','evidence_ids','gap']
rows=[]
for i,r in enumerate(json.load(open(W/'documentation-index.json')),1):
 slug=r['url'].rsplit('/',1)[-1];m=help_map.get(slug);category=r['url'].count('/')==2
 rows.append(dict(zip(fields,[f'H-{i:02}','help_category' if category else 'help_article',r['url'].split('/')[2],r['title'],r['url'],'https://antennapod.org'+r['url'],'current public help; app build unspecified','index_only' if not m else 'document_task_partial','F-M040-'+m[0] if m else '',m[1]+';E-APCD-02' if m else 'E-APCD-02','仅目录；未补流程' if not m else '原文档不自动绑定3.12.3；详见case版本边界'])))
strings={x.attrib.get('name'):''.join(x.itertext()) for x in ET.parse(find('strings.xml')).getroot()};ns='{http://schemas.android.com/apk/res/android}'
keyflows={'prefPauseOnHeadsetDisconnect':'09','prefUnpauseOnHeadsetReconnect':'09','prefUnpauseOnBluetoothReconnect':'09','prefEnqueueLocation':'02','prefEnqueueDownloaded':'02','prefFollowQueue':'09','prefAutoDelete':'05','prefAutoDeleteLocal':'05','prefFavoriteKeepsEpisode':'05','prefEpisodeCleanup':'05','prefEnableAutoDl':'04','prefEnableAutoDlQueue':'04','prefEpisodeCacheSize':'04','prefEnableAutoDownloadOnBattery':'04','prefAutoDownloadSettings':'04','prefAutoDeleteScreen':'05','prefDeleteRemovesFromQueue':'05','prefShowDownloadReport':'10','prefPersistNotify':'11','prefExpandNotify':'11','prefFullNotificationButtons':'11','prefDatabaseExport':'12','prefDatabaseImport':'12','prefAutomaticDatabaseExport':'12','prefOpmlExport':'12','prefOpmlImport':'12','prefHtmlExport':'12','prefFavoritesExport':'12','pref_synchronization_sync':'12','pref_synchronization_force_full_sync':'12','pref_synchronization_logout':'12','pref_gpodnet_setlogin_information':'12'}
for p in sorted((S/'ui/preferences/src/main/res/xml').glob('preferences*.xml')):
 for x in ET.parse(p).iter():
  k=x.attrib.get(ns+'key');t=x.attrib.get(ns+'title','');t=strings.get(t[8:],t) if t.startswith('@string/') else t
  if k is None:continue
  f=keyflows.get(k);g='仅设置声明；点击行为、运行可见性和异常未逐项核定'
  if k in ('prefUnpauseOnHeadsetReconnect','prefUnpauseOnBluetoothReconnect'):g='API31+隐藏；旧开关不能证明Media3恢复路径'
  if k=='prefPersistNotify':g='API30+隐藏；未观察系统控制'
  if k=='prefExpandNotify':g='API26+隐藏'
  if k=='prefEpisodeCacheSize':g='XML25/Java回退20/网页50；真实安装默认值未知'
  if k=='about_version':g='XML摘要是占位文字；版本取app/build.gradle'
  if k in ('prefAutomaticDatabaseExport','prefHtmlExport','prefFavoritesExport'):g='本轮只定位入口/输出类型；周期执行、输出回导未闭合'
  rows.append(dict(zip(fields,[f'S-{sum(r["kind"]=="setting_key" for r in rows)+1:02}','setting_key',p.stem,t or k,str(p.relative_to(S))+':'+k,URL+str(p.relative_to(S)),'3.12.3 pinned XML declaration','D1_declaration_with_partial_task_link' if f else 'D1_declaration','F-M040-'+f if f else '','E-APCD-03',g])))
with (A/'functional-inventory.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
# Sources are grouped only when necessary to expose one path across callback/config files.
specs=[
('01','AntennaPod official release 3.12.3 and tag identity','https://github.com/AntennaPod/AntennaPod/releases/tag/3.12.3',W/'release.json','official_release','release metadata; tag-ref; app/build.gradle 15–16','2026-10-13'),
('02','Official documentation index','https://antennapod.org/assets/datasets/documentation_index.json',W/'documentation-index.json','official_help','47 index entries, no per-page build',None),
('03','3.12.3 global settings declaration inventory',URL+'ui/preferences/src/main/res/xml',W/'settings-manifest.json','official_source_code','11 preferences XML; 84 keyed nodes; referenced strings and visibility flags',None),
('04','3.12.3 selected playback service',URL+'common.gradle',S/'common.gradle','official_source_code','common.gradle 15–16; service manifest 20–79; starter 44–60',None),
('05','3.12.3 chapter entry and previous/next controls',URL+str(find('CoverFragment.java').relative_to(S)),find('CoverFragment.java'),'official_source_code','108–111; 204–273',None),
('06','3.12.3 chapter loading and selection',URL+str(find('ChaptersFragment.java').relative_to(S)),find('ChaptersFragment.java'),'official_source_code','50–87;135–173',None),
('07','3.12.3 sleep timer UI and service commands',URL+str(find('SleepTimerDialog.java').relative_to(S)),find('SleepTimerDialog.java'),'official_source_code','147–270;315–419;465–476; AudioPlayerFragment 535–536',None),
('08','3.12.3 clock sleep timer event cadence and reset',URL+str(find('ClockSleepTimer.java').relative_to(S)),find('ClockSleepTimer.java'),'official_source_code','34–54;77–142; SleepTimer/ShakeListener/SleepTimerUpdatedEvent',None),
('09','3.12.3 episode sleep timer',URL+str(find('EpisodeSleepTimer.java').relative_to(S)),find('EpisodeSleepTimer.java'),'official_source_code','31–65',None),
('10','3.12.3 Media3 playback interruption, timer and continuation',URL+str(find('Media3PlaybackService.java').relative_to(S)),find('Media3PlaybackService.java'),'official_source_code','339–355;390–428;472–477;504–535;803–879;884–937',None),
('11','3.12.3 Media3 audio focus and noisy configuration',URL+str(find('ExoPlayerUtils.java').relative_to(S)),find('ExoPlayerUtils.java'),'official_source_code','47–69',None),
('12','3.12.3 playback preferences visibility',URL+str(find('PlaybackPreferencesFragment.java').relative_to(S)),find('PlaybackPreferencesFragment.java'),'official_source_code','38–56; API31 reconnect switches hidden',None),
('13','3.12.3 unsubscribe, archive and restore dialog',URL+str(find('RemoveFeedDialog.java').relative_to(S)),find('RemoveFeedDialog.java'),'official_source_code','59–84;119–178;193–206; FeedMenuHandler and SubscriptionFragment archive entry',None),
('14','3.12.3 media/feed deletion and archive persistence',URL+str(find('DBWriter.java').relative_to(S)),find('DBWriter.java'),'official_source_code','118–167;177–195;212–249;862–884',None),
('15','3.12.3 backup and restore UI, confirmation and errors',URL+str(find('ImportExportPreferencesFragment.java').relative_to(S)),find('ImportExportPreferencesFragment.java'),'official_source_code','123–181;189–278;352–389; strings database_import_warning',None),
('16','3.12.3 database export/import scope and schema gate',URL+str(find('DatabaseExporter.java').relative_to(S)),find('DatabaseExporter.java'),'official_source_code','45–105; UserPreferences 148–152 (separate SharedPreferences)',None),
('17','3.12.3 notification/session control commands',URL+str(find('MediaLibrarySessionCallback.java').relative_to(S)),find('MediaLibrarySessionCallback.java'),'official_source_code','113–199',None),
('18','Official recovery guidance for historical database corruption','https://antennapod.org/documentation/bugs-first-aid/database-error',W/'database-error.html','official_help','Recovery list; pre-2019 damage context',None)]
records=[]
for n,title,url,p,typ,locator,date in specs:
 art=A/('functional-inventory.csv' if n in ('02','03') else 'source-notes.md')
 records.append({'evidence_id':'E-APCD-'+n,'product_id':'M-040','title':title,'url':url,'checked_on':'2026-10-27','source_type':typ,'access_mode':'official_http_text_and_targeted_static_source_read' if typ=='official_source_code' else 'official_http_text_read','version_scope':'AntennaPod Android 3.12.3; commit '+SHA+'; static source only' if typ!='official_help' else 'Current official help; app build unspecified; not evidence of 3.12.3 runtime','app_build':'3.12.3 (versionCode 3120395)' if typ!='official_help' else None,'firmware_version':None,'source_updated_on':date,'locator':locator,'note':'详见归档记录对应E行；相关原件清单在work/antennapod-controls-deepening/source-manifest.json。目录声明不等于流程完成。','hands_on':False,'visual_observed':False,'archived_artifact':str(art.relative_to(L)),'sha256':digest(art),'capture_state':'source_text_and_derived_notes_archived_no_product_visual','supersedes':None,'raw_work_path':rel(p),'raw_sha256':digest(p)})
with (L/'batches/antennapod-controls-deepening-evidence.jsonl').open('w') as f:
 for x in records:f.write(json.dumps(x,ensure_ascii=False)+'\n')
coverage_fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
with (L/'batches/antennapod-controls-deepening-coverage.csv').open('w',newline='') as f:csv.DictWriter(f,fieldnames=coverage_fields).writeheader()
print(json.dumps({'evidence':len(records),'new_flows':0,'inventory_total':len(rows),'help_index':47,'settings_keys':84,'raw_sources':len(manifest)},ensure_ascii=False))
