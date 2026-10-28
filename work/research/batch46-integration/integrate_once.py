import csv, hashlib, json, shutil
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches'
W=R/'work/research/batch46-integration';S=R/'work/research/batch46-sony-system-audio';A=L/'artifacts/sony-system-audio'
assert not A.exists(),'Do not rerun';A.mkdir()
DATE='2026-10-27';RAW=R/'work/sony-controls-deepening/raw/guide-existing.pdf'
SCOPE='NW-A306 YY1301B1; English Help Guide 5-042-672-11(5); firmware, Music player build and sales region unknown'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rows):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
assert sha(RAW)=='90b7cb2c48cf9e28b2dfb9fcde541a5e9e450b4ea6c63413fb9af2497181dced'
scope=json.loads((S/'body-review-scope.json').read_text());http=json.loads((S/'http-manifest.json').read_text())
assert len(scope['read_pages'])==59 and len(http)==4 and all(x['status']==200 for x in http)
assert 'E-SYSA-01' not in (L/'evidence.jsonl').read_text()
observations={
27:'WALKMAN主页示例：音乐小组件的封面区域与前曲/播放/后曲按钮分列；底部三键和Dock。未实际点封面或播放，示例曲目不当用户内容。',
29:'Android主页示例含Google搜索条、应用图标、Dock和三键。正文仍称Home去WALKMAN，与33页可设主主页有范围差异；不猜其当前配置。搜索条麦克风图标不证明系统AI。',
32:'纯文字步骤页，没有实际快捷面板截图。点按开关与长按设置是文字说明；音量项为打开对话框，不能笼统当音量开关。',
53:'播放页状态栏静态示例及图标表；列数据库更新、卡已挂载、HOLD、蓝牙/Wi-Fi连接、电量和Battery Care。没有观察图标出现/消失，也不由卡挂载推文件索引完成。',
56:'实体音量键示意及旋钮对话框；示例值68、旋钮和加减控件。68不当默认、分贝或儿童适宜值；未拖动或听音。',
57:'正文及内嵌设置图标，说明其他App音量、蓝牙/USB输出与AVLS例外；没有AVLS开关实际界面或开关结果。',
113:'内部存储目录图示包含Music/Download、m3u/lrc及不识别的根下音乐文件。图根名sdcard处在Internal memory说明下，不能凭名称当物理卡；不是文件管理器实拍。',
}
obs=[]
for n,note in observations.items():
 src=S/f'manual-p{n}.png';dst=A/src.name;shutil.copyfile(src,dst)
 obs.append(dict(evidence_id='E-SYSA-05',pdf_page=n,printed_page=n,observed_on=DATE,actual_view='view_image',hands_on=False,continuous_sequence=False,raw_work_path=str(src.relative_to(R)),archived_artifact=str(dst.relative_to(L)),sha256=sha(dst),note=note))
dump(A/'observed-pages.json',obs)
notes='''# Sony NW-A306：系统操作、音量与内容就绪

2026-10-27。固定NW-A306 / YY1301B1英文Help Guide及143页可打印本5-042-672-11(5)，不当已固定设备固件、播放器构建或销售地区。原PDF SHA256：90b7cb2c48cf9e28b2dfb9fcde541a5e9e450b4ea6c63413fb9af2497181dced。正文复读文件/印刷11–20、25–37、53–65、71–86、92、107–108、113–114、126–127，共59页；并非整册已读。82项原目录中36项从仅目录补正文，另3项复读，不将章节数当功能数。

实际查看27、29、32、53、56、57、113共7页静态摘页；32页是文字步骤，57页主要文字和内嵌图标，不能称看过快捷面板或AVLS操作界面。没有实机、听音、点按、安装、登录、付款、传输或更新。四个对应HTML正文另用web打开和普通HTTP200保存，与PDF属于同一帮助体系，不是独立互证。

## 可复用事实与限制

| 来源定位 | 入口、动作与资料所述结果 | 缺口与范围 |
| --- | --- | --- |
| 11–12，初始设置 | 首开选语言、START、阅读许可并Agree，按向导选主主页并OK；Wi-Fi/Google账户可稍后设置。向导中联网后可选择迁移App和数据，时间取决于量。 | 11页说完成后WALKMAN出现，33页又说明主主页可选，保留表述差异。迁移来源、选择范围、取消/失败与完整性未知；未同意协议或执行向导。 |
| 13–20，开关/充电 | 开机长按5秒，亮屏时长按2秒再Power off。USB连接运行中电脑充电，指示灯充电亮/完成灭；低电量可能先不能识别。Battery Care开关限制充电上限，图标即使没充电也显示。Auto power off描述为暂停后无操作计时；另称72小时无操作即使开关Off也关机。 | 不把Battery Care图标当正在充电；时长选项和72小时计时对持续音频的适用未展开。USB hub支持措辞有条件和笼统不充电两种表述；不拼成所有组合保证。没有续航/电量或状态实测。 |
| 15、25–26，按键 | HOLD限制播放和音量键，电源和触屏仍可用；关屏是另一动作。播放/音量加有触觉点；普通重启与强制重启另见原任务。 | 不把HOLD当锁屏认证、暂停或家长限制；未验证熄屏时其他App控制和误触恢复。 |
| 27–37，主页与快捷控制 | WALKMAN小组件点封面打开播放器，前/播放/后按钮直接控制；Android主页为另一页。左右滑动换当前页；长按空白→Home settings→Select Home screen→选择/OK设置Home返回目标。上滑列所有App，下拉通知/快捷，普通图标点开关，部分可长按入设置，音量点开对话框。播放器mini player点封面/标题回播放页，左右划其内容区跳前/后曲。 | 27/29页Home文字都指WALKMAN，33页明确可选主主页，不合成固定默认。未观察返回位置、任务存活、手势取消或配置持久化；Dock可放5快捷是文档，不证明App安装/权限。 |
| 53–58，状态/锁屏/音量 | 状态表区分数据库更新、卡挂载、无线连接及Battery Care。Security/Screen lock选认证方式后按引导；开启后屏灭自动锁。音量键打开面板，点标尺/数值进旋钮；其他App可先调Media volume再调主音量。Sound/AVLS限制上限，但文档明确蓝牙及USB输出不生效；其音量应在连接设备调。 | 忘记解锁方式手册给付费格式化路径，不能当无损找回。AVLS不是所配全部输出的上限；未测数值、回切和恢复。58页音量警告可OK后继续调高，累计20小时再警告并降低；这是本册规则，适用地区/实际阈值未知，不作法律或听力安全保证。 |
| 59–65、71，电脑准备/传输 | Music Center for PC要求2.5+；电脑CD导入可选格式、勾曲、Import/Stop。USB File Transfer/OK后，电脑传输窗选设备或卡、选专辑/曲目再传；ATRAC转AAC在电脑端进行。Mac仅指用适配Android文件传输的软件。 | 电脑预处理不当设备原生转码或读CD。Stop后半成品、重名覆盖、空间不足与索引结果未说明。未安装软件；Mac无具体工具/步骤，不补造当前指南。 |
| 74、113–114，路径与索引 | 内存Music/Download与卡Music为默认目录，也可注册其他目录。根下音乐文件不识别；文件可到目录第8层，第8层内的文件夹不识别。不得改默认目录名。视频内容此播放器只播放其音频。 | 复制到设备、在扫描范围、完成索引是三件事。内部目录图的sdcard名称不当物理介质证明；未测深度边界、坏文件、重扫失败/回滚。 |
| 75–81，网络/账户/App/购买 | Settings/Network & internet/Internet选网输入密码，成功在名称下显示Connected；界面可能随系统变。Google账户可多账户，用于Play安装等；Play搜索并按指引装服务App，服务内购买后在Music player更新数据库。下载到别处可Files移动或注册其目录再更新数据库。 | 联网不等于服务授权；多Google账户不证明家庭数据隔离。购买/订阅步骤由服务决定；没有证明受保护的流媒体缓存可作通用本地音乐。没有卸载/重装恢复、支付取消或下载断点记录。 |
| 82–86、107–108，外部服务与连接条件 | 360 Reality Audio个性化涉及本机、带相机手机、兼容耳机及付费订阅，设置细节交给外部App。网络/蓝牙说明提示环境、干扰、延时和兼容限制。 | 手册旧App名称/最新版要求不当已核当前版本。没看到手机采集、传回/保存、删除和失败过程；不是本机独立完成。规格/示意不能替代连接可靠性测试。 |
| 92，歌词材料 | 需要LRC文本与音频文件配套，可自行整理文本/时间。 | 没有自动联网取词/音频对齐链；不能据“取得歌词”标题推断内置获取服务。 |
| 126–127，版本与更新 | About device可看型号/系统版；更新需电源和Wi-Fi，System/System updates按消息继续。手册要求先备份，并以歌曲详细信息中的File location找原文件。 | 文件留存不等于队列/收藏/账户/App完整备份；无进度、取消、失败回滚和恢复结果。没有下载安装或升级。 |

## 静态摘页

[WALKMAN主页27](manual-p27.png)、[Android主页29](manual-p29.png)、[快捷步骤32](manual-p32.png)、[状态图标53](manual-p53.png)、[音量旋钮56](manual-p56.png)、[AVLS范围57](manual-p57.png)、[目录示意113](manual-p113.png)。细项见[逐页观察](observed-pages.json)。同源图片和纯文字页均不升级实际操作；56页68只是示例，不是默认、分贝或儿童适宜值。

## 对应在线帮助

- E-SYSA-01：[调节音量](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733590.html)，旋钮、其他App、AVLS及输出例外；与56–57页一致。
- E-SYSA-02：[减少耗电](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733557.html)，暂停后自动关机及72小时条款；与18页一致。
- E-SYSA-03：[切换主页](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000735087.html)，当前左右换页与设置Home目标分开；与33页一致。
- E-SYSA-04：[设备上购买音乐](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733547.html)，服务购买、下载路径及更新数据库；与78–79页一致。

四页无独立发布日期/设备构建证据；HTTP Last-Modified即使存在也不当功能发布日期。原HTML、正文及获取结果在work/research/batch46-sony-system-audio/raw及http-manifest.json。任务与项目推断见[案例](../../cases/M-036-sony.md)，不据此确定Plus架构、儿童策略或音量指标。
'''
(A/'source-notes.md').write_text(notes)
new_e=[]
for i,r in enumerate(http,1):
 title={'volume':'音量控制、应用上下文与AVLS范围','battery':'暂停后自动关机及72小时条件','home':'切换当前主页与设置Home目标','downloads':'服务购买、下载路径与曲库索引'}[r['slug']]
 new_e.append(dict(evidence_id=f'E-SYSA-{i:02d}',product_id='M-036',title=title,url=r['url'],source_type='official_help',access_mode='web_open_and_http_200_body_read',checked_on=DATE,source_updated_on=None,version_scope=SCOPE,app_build=None,firmware_version=None,locator={'volume':'对应PDF56–57','battery':'对应PDF18','home':'对应PDF33','downloads':'对应PDF78–79'}[r['slug']],note=title+'；同版PDF复核，非设备操作。',hands_on=False,visual_observed=False,archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md'),capture_state='http_200_original_and_bounded_notes',supersedes=None,raw_work_path=r['raw_path'],raw_sha256=r['raw_sha256'],extracted_work_path=r['text_path'],extracted_sha256=r['text_sha256']))
bundle=dict(evidence_id='E-SYSA-05',product_id='M-036',title='系统主页、音量及目录的七页同源静态摘页',url='https://helpguide.sony.net/dmp/1301/v1/en/print.pdf',source_type='official_manual_static_page_bundle',access_mode='local_pdf_render_actually_viewed',checked_on=DATE,source_updated_on=None,version_scope=SCOPE,app_build=None,firmware_version=None,locator='PDF/printed pp27,29,32,53,56,57,113',note='32页纯文字、57页主要文字；没有观察快捷面板或AVLS开关。其余静态UI/示意也不是操作。',hands_on=False,visual_observed=True,archived_artifact=str((A/'source-notes.md').relative_to(L)),sha256=sha(A/'source-notes.md'),capture_state='seven_same_source_static_pages_archived',supersedes=None,derived_from='E-SONY-15',raw_work_path=str(RAW.relative_to(R)),raw_sha256=sha(RAW),visual_audit_path=str((A/'observed-pages.json').relative_to(L)),artifact_hashes={x['archived_artifact']:x['sha256'] for x in obs})
bundle['artifact_hashes'][bundle['visual_audit_path']]=sha(A/'observed-pages.json');new_e.append(bundle)
(B/'sony-system-audio-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in new_e))
cap=dict(checked_on=DATE,access_mode='existing_local_pdf_body_and_selected_static_pages',raw_work_path=str(RAW.relative_to(R)),raw_sha256=sha(RAW),locator='PDF/printed pp11–20,25–37,53–65,71–86,92,107–108,113–114,126–127',text_pages=scope['read_pages'],text_pages_count=59,visual_pages=list(observations),visual_observed=True,hands_on=False,continuous_sequence=False,archived_artifact=bundle['archived_artifact'],sha256=bundle['sha256'],visual_audit_path=bundle['visual_audit_path'],note='既有PDF正文补读，36个原仅目录项补正文；不是全143页或全部功能完成。')
owners=[]
for p in B.glob('*-evidence.jsonl'):
 rows=[json.loads(x) for x in p.read_text().splitlines()];changed=False
 for e in rows:
  if e['evidence_id']=='E-SONY-15':
   before=json.loads(json.dumps(e));e.setdefault('additional_captures',[]).append(cap);owners.append(dict(owner=str(p.relative_to(R)),before=before,after=e));changed=True
 if changed:p.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in rows))
assert len(owners)==1;dump(W/'old-evidence-update.json',owners)

details=[];flows=[]
def task(n,title,pre,steps,gap,pp,ids,vis=False):
 flows.append(dict(flow_id=f'F-M036-{n:02d}',product_id='M-036',task=title,depth='D2',evidence_ids=ids,known_branches=pre+'；'+steps,gaps=gap,visual_status='static_manual_pages_only' if vis else 'not_observed',hands_on='false',case_path='cases/M-036-sony.md',checked_on=DATE))
 details.append(dict(n=n,title=title,pre=pre,steps=steps,gap=gap,pp=pp,ids=ids))
task(17,'首次设置与选择主主页','首次开机且电量足够','长按电源5秒；选语言/START/许可Agree，按向导选主页OK。Wi-Fi/Google可稍后；向导中联网可选迁移App/数据。','未执行许可/迁移；完成后WALKMAN措辞与可选主主页有差异；中断、迁移对象、失败和恢复未知。','11–12','E-SONY-15')
task(18,'切换主页、启动应用与使用快捷控制','WALKMAN或Android主页','左右滑换当前页；长按空白/Home settings/Select Home screen/选择OK改变Home目标。上滑All Apps选应用；下拉快捷，点开关或音量对话框，部分长按入设置。','27/29页Home固定WALKMAN文案与33页条件并存；未测手势取消、后台、默认配置和持久化。','27–33','E-SONY-15;E-SYSA-03;E-SYSA-05',True)
task(19,'设置锁屏认证并辨明找回代价','Settings → Security → Screen lock','选择认证方式，按该方式引导完成；启用后屏灭自动锁。忘记方式时手册指向付费服务格式化清锁信息。','不是HOLD或家庭账号；无损找回没有依据，具体认证界面、错码/离线解锁、格式化后恢复未知。','12、55','E-SONY-15')
task(20,'联网、配置Google账户并安装服务App','Wi-Fi网络及相应服务条件','Network & internet/Internet选网输入密码CONNECT；文档成功显示Connected。账户菜单可加Google账户，Play搜索/按引导安装。可配置多个Google账户。','系统版可改变菜单；Wi-Fi连接不等于服务授权，多账户不证明隔离。错密/门户/取消、权限、卸载数据和重装恢复未核。','75–77、81','E-SONY-15')
task(21,'取得在线音乐并让本机曲库发现','Wi-Fi、服务App、账户及购买条件','按提供商流程购得内容，Music player更新数据库。若下载不在默认/注册目录，用Files移动或注册目录后再更新；格式/支付/地区依服务。','不保证流媒体受保护缓存可导入；支付取消、下载中断、索引失败、重复和授权失效未知。','78–81','E-SONY-15;E-SYSA-04')
task(22,'按应用与输出方式调音量及上限','Music player或其他App；输出方式明确','音量键开面板，点标尺/数值进旋钮，或快捷音量入口。其他App可能需先调Media volume；Sound/AVLS开上限。蓝牙/USB输出需接收端调节，本机AVLS不生效。','警告OK后仍可调高，与AVLS不同；地区/阈值、切输出音量继承、跨App和重启恢复未测；不作儿童听力保证。','56–58','E-SONY-15;E-SYSA-01;E-SYSA-05',True)
task(23,'辨认充电状态并设置电池保护与自动关机','USB接运行中电脑或Settings → Battery','充电灯充电亮/完成灭；Battery Care限制充电上限且图标非充电时也在。Auto power off为暂停后闲置计时；另有72小时无操作即使Off也关机的文档条款。','72小时对持续播放适用、计时选项/重置和恢复未知；USB hub措辞不统一，电量不是精确测量；未实测续航。','13–20、53','E-SONY-15;E-SYSA-02;E-SYSA-05',True)
task(24,'查看版本并进入系统更新','电源、Wi-Fi及事先数据留存','About device查看型号/系统版；System/System updates确认消息并按指引继续。可用歌曲Detailed song information/File location找原文件用于备份。','没有执行更新；进度/取消/失败回滚与完整恢复未知。歌曲副本不是队列/收藏/账户/App整机备份。','126–127','E-SONY-15')
csvout(B/'sony-system-audio-coverage.csv',flows);dump(W/'new-task-details.json',details)
updates={
'F-M036-01':(' Music Center for PC 2.5+可在电脑选设备/卡和曲目后传输；ATRAC转AAC发生在电脑。CD导入有Import/Stop，Mac仅泛指适用传输软件。',' 电脑导入停止后的半成品、转码/复制完成与设备索引结果未核。','E-SONY-15'),
'F-M036-02':(' 74/113–114页区分目录第8层音乐文件与该层文件夹：后者不识别；根下音乐不识别，默认Music/Download不可改名。',' 图中内存根名sdcard不能当物理卡；深度边界/重扫失败待测。','E-SONY-15;E-SYSA-04;E-SYSA-05'),
'F-M036-03':(' WALKMAN小组件封面打开播放器，媒体按钮直接控制；mini player内容区左右划跳前/后曲，点内容回播放页。',' 未测小组件与App页面同步、误划和返回原列表。','E-SONY-15;E-SYSA-05'),
'F-M036-09':(' 音量与AVLS的蓝牙/USB例外另见F22。360个性化需带相机手机、兼容耳机和服务订阅，具体设置交外部App。',' 360手机采集/传回/保存及失败未知；旧App名称不当已核现行构建。','E-SONY-15;E-SYSA-01'),
}
audit=[]
for p in B.glob('*-coverage.csv'):
 rows=list(csv.DictReader(p.open()));changed=False
 for r in rows:
  if r['flow_id'] not in updates:continue
  before=dict(r);branch,gap,ids=updates[r['flow_id']];r['known_branches']+=branch;r['gaps']+=gap
  r['evidence_ids']=';'.join(dict.fromkeys((r['evidence_ids']+';'+ids).split(';')));r['checked_on']=DATE
  if 'E-SYSA-05' in ids:r['visual_status']='static_manual_pages_only'
  audit.append(dict(owner=str(p.relative_to(R)),before=before,after=dict(r)));changed=True
 if changed:csvout(p,rows)
assert len(audit)==4;dump(W/'old-coverage-updates.json',audit)
invpath=B/'sony-controls-deepening-inventory.csv';inv=list(csv.DictReader(invpath.open()));before=json.loads(json.dumps(inv))
mapflow={4:'17',5:'23',6:'23',7:'08',8:'23',9:'23',10:'23',11:'23',14:'08',15:'18;03',16:'18',17:'18;20',18:'18;22',19:'18',20:'03',26:'18;23',27:'19',28:'22',29:'22',30:'01',31:'01',32:'01',33:'01',36:'01',37:'12',38:'01;02',39:'20',40:'20',41:'21;02',42:'21',43:'20',44:'09',45:'20',46:'20',49:'12',58:'09',61:'02',69:'24',70:'24'}
selected={x['inventory_id']:x for x in scope['selected']};assert len(selected)==39
for r in inv:
 if r['inventory_id'] not in selected:continue
 n=int(r['inventory_id'].rsplit('-',1)[1]);r['coverage_status']='body_read_documented_not_operated'
 r['flow_ids']=';'.join('F-M036-'+x.zfill(2) for x in mapflow[n].split(';'))
 r['evidence_ids']=';'.join(dict.fromkeys((r['evidence_ids']+';E-SONY-15').strip(';').split(';')))
 if n in {28,29}:r['evidence_ids']+=';E-SYSA-01'
 if n==10:r['evidence_ids']+=';E-SYSA-02'
 if n==19:r['evidence_ids']+=';E-SYSA-03'
 if n==41:r['evidence_ids']+=';E-SYSA-04'
 r['gap']='本轮59页范围内正文已读；'+({4:'向导取消/迁移完成与主页差异未核。',19:'当前换页与Home默认目标分开，持久化未知。',27:'忘码手册给格式化；无损找回/恢复未证。',28:'蓝牙/USB与其他App音量不同；无实际听音或切源。',29:'警告可取消，地区/阈值未核，不当所有输出上限。',41:'内容下载、目录范围、更新索引分开；服务缓存/失败未知。',44:'手机相机/耳机/订阅条件；外部App具体链未读。',61:'文件/文件夹层级不同；根名不推介质，边界未测。',70:'未更新；仅文件留存依据，无整机恢复/回滚。'}.get(n,'关联F只表示局部任务交集，实际操作、失败/取消及恢复未验证。'))
csvout(invpath,inv);dump(W/'inventory-update-audit.json',dict(before=before,after=inv,selected_ids=list(selected),previous_directory_items=36))
dump(W/'integration-result.json',dict(new_flows=8,old_flows_changed=4,new_evidence=5,new_distinct_urls=4,new_static_pages=7,text_pages=59,inventory_rows_changed=39,previous_directory_items=36))
print('Integrated 8 new D2, 4 old flows, 5 E / 4 new URLs; 39 inventory rows, including 36 previously directory-only.')
