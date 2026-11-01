# FINALIZED: do not rerun after root edits or merges; this is a snapshot generator.
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urljoin
import json,csv,hashlib,re
W=Path('work/sony-controls-deepening');L=Path('docs/research/interaction-library');C=L/'cases/M-036-sony.md';B=L/'batches';A=L/'artifacts/sony-controls-deepening'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest=json.loads((W/'source-manifest.json').read_text());old=json.loads((W/'old-coverage-before.json').read_text());oldby={r['flow_id']:r for r in old}
base='https://helpguide.sony.net/dmp/1301/v1/en/'
# Each heading below is a functional directory label, not a claim that its body was analyzed.
class Menu(HTMLParser):
 def __init__(self):super().__init__();self.active=False;self.stack=[];self.capture=None;self.txt=[];self.items=[]
 def handle_starttag(self,t,a):
  d=dict(a)
  if t=='nav' and d.get('id')=='menu':self.active=True
  if not self.active:return
  if t=='li':self.stack.append('')
  if t in ['button','a']:
   self.capture=(t,d.get('href'));self.txt=[]
 def handle_data(self,d):
  if self.capture:self.txt.append(d)
 def handle_endtag(self,t):
  if not self.active:return
  if self.capture and t==self.capture[0]:
   label=' '.join(''.join(self.txt).split());typ,href=self.capture
   if typ=='button':self.stack[-1]=label
   elif href and 'TP100' in href:self.items.append({'title':label,'domain':' / '.join(x for x in self.stack[:-1] if x),'url':urljoin(base,href)})
   self.capture=None
  if t=='li':self.stack.pop()
  if t=='nav':self.active=False
m=Menu();m.feed(Path(manifest[0]['path']).read_text());assert len(m.items)==82
pages=json.loads((W/'extracted/manual-pages.json').read_text());norm=lambda t:re.sub(r'\s+',' ',t).strip().replace('\u200b','')
locs={}
for it in m.items:
 title=norm(it['title']);found=[]
 for i,p in enumerate(pages):
  if i<5 or not norm(p).startswith('Help Guide Digital Media Player NW-A306 '):continue
  head=norm(p).removeprefix('Help Guide Digital Media Player NW-A306 ').replace('USB- DAC','USB-DAC')
  if head.startswith(title):found.append(i+1)
 locs[it['url']]=found[0] if found else ''
# New objectives only; source additions to old objectives are separate proposals below.
new=[
(11,'按分类边界设置随机、重复及播放范围','已选中分类、列表或搜索曲目','播放页点随机/重复，或 Options → Settings 设 Shuffle / Repeat；Playback range 选 All range 或 Selected range。图标均暗回普通模式。列表上的随机按钮以当前列表为范围。','All range 仍受入口影响：随机模式从专辑进入时按专辑推进；要全曲随机/循环需从 All songs 进入。歌单/收藏仅播所选项；搜索结果点曲只播该曲，均不会因 All range 扩成全库。','E-SYCD-02','切换范围是否立即改写当前队列、重启保留、空列表反馈未核。'),
(12,'制作并显示随曲歌词','有本地音轨及可编辑文本；电脑传入条件同F01','电脑保存 UTF-8 的同名 .lrc，时间按顺序；歌词与音频同文件夹传入。播放页 Options → Display lyrics。','LRC 上限512 KB；歌词依附音频文件，不是播放器内时间点书签。','E-SYCD-03;E-SYCD-04','错误时间标记/编码的具体提示、点击歌词跳转、歌词滚动与关闭返回状态未说明；不据此宣称可逐句寻位。'),
(13,'把电脑声音送入Walkman的USB DAC并恢复后台无声','电脑已按手册准备兼容播放软件和Windows驱动；此处未安装','USB连接 → Use USB for 选 USB DAC、OK → Music player曲库 USB DAC → 开始提示 YES → 电脑播放。退出用Back或Library top，再确认YES。','输入灯绿=PCM、蓝=DSD、熄灭=未检测信号；显示格式信息。启用期间不能本机播歌或传文件，也不能无线听；本机音效仍应用。其他音视频App令DAC屏退后台后可能无声，返回 WALKMAN Home → Music player 显示DAC屏可能恢复。','E-SYCD-05;E-SYCD-06','恢复仅为官方条件性建议，未实测；切回本地后断点、音频焦点、驱动报错及视频音画同步未验证。'),
(14,'准备、弹出与更换microSD并辨认丢失的状态','先备份重要卡数据；本机格式化将清空卡','金触点朝上插至卡住；Settings → Storage → 下拉选卡 → Options → Format，按提示完成。取卡前同入口先Eject，再按卡弹出；未识别时官方建议取出重插。','移卡时官方把当前播放内容信息、位置、队列曲目/顺序列为deleted，把卡内曲目/专辑/歌单收藏及卡内曲目的歌单登记信息列为disappear。这里不能推定删除了音频文件或重插必然恢复关联。','E-SYCD-07;E-SYCD-08','重新插卡后哪些关联恢复、当前播放来自内存时的影响范围、格式化确认文案、损坏卡错误未知；大量数据建库约10分钟以上只是官方估计。'),
(15,'操作不稳时选择重启，并辨清恢复出厂的另一结果','重启针对不稳；清除前先备份，必要时具备锁屏凭据','屏幕亮时电源长按2秒 → Restart；冻结时电源按住10秒强制重启。若另选出厂清除：Settings → System → Reset options → Erase all data → 确认/所需凭据 → 再确认。','重启不删数据和配对信息。恢复出厂删除内存音乐/照片、机上Google信息及下载App，结束自动重启并恢复预装内容；界面可能随系统版变化。','E-SYCD-09;E-SYCD-10','不把“不删数据”推成播放断点必保留；清除中断、卡数据影响、账号重新绑定及完整恢复流程未核。'),
(16,'将设备曲目传回电脑留存','Windows Music Center for PC 2.5+；USB File Transfer','打开PC软件并连接；未出现USB菜单走Settings → Connected devices → USB。软件传输页选设备或microSD → Manage songs in device → 选曲 → 右键 Import to PC。','保存的是所选音乐内容；传输时不可拔线。清除前可从Detailed song information的File location确认原文件位置。此流程不等于收藏、队列、账号或App整机备份。','E-SYCD-11;E-SYCD-10;E-SONY-09','导出覆盖冲突、半途取消、完整性校验、播放列表/元数据的恢复范围未说明。')
]
updates={
3:dict(evidence_ids='E-SONY-01;E-SONY-02;E-SYCD-12;E-SYCD-17',known_branches='曲库分类图标可长按拖动；Options可选显示项或重置默认分类。播放页标题/作者/专辑或Search related可回相关内容；返回滚动状态未知。',gaps=oldby['F-M036-03']['gaps']),
4:dict(evidence_ids='E-SONY-02;E-SYCD-13',known_branches='插到下一首、追加、替换队列与移出单项分开；播放页右滑到队列，Options的Add to playlist保存整队列，单项菜单只加入该曲。电脑导入歌单不能作为追加目标。',gaps='替换是否确认/可撤销；重复项、当前项被移除、重启保留；整队列保存后的重复处理未核。'),
7:dict(evidence_ids='E-SONY-09;E-SYCD-07',known_branches='收藏可存曲目/专辑/歌单；收藏信息不能导入电脑。移卡时卡上对象的收藏信息会disappear，原文未说明重插恢复；不可据此推定音频被删。',gaps='重插卡/重建数据库后的收藏、误取消恢复、不同成员收藏隔离。'),
8:dict(task='口袋盲操、防误按与寻位',evidence_ids='E-SONY-02;E-SONY-03;E-SONY-04;E-SYCD-17',known_branches='锁实体键、关屏、锁屏认证、暂停四者不同；松开HOLD恢复按键。长按媒体键快退/进须在播放中；官方提示已播时间和时间条可能不准确。',gaps=oldby['F-M036-08']['gaps']),
9:dict(task='蓝牙配对/回连、音质设置与出口相关音效',evidence_ids='E-SONY-11;E-SONY-12;E-SYCD-14;E-SYCD-15;E-SYCD-16;E-SYCD-05',known_branches='首次Pair new device选设备/所需密钥；超时重试，恢复出厂后重配。Wireless playback quality下次连接才生效且受接收端影响。USB音频输出的音效限制不能套USB-DAC输入；后者应用本机音效。High-Res streaming在系统Sound中配置第三方App，兼容性非全覆盖。',gaps='断连后是否暂停、接有线优先级、重新连是否自动出声；第三方App后台/音频焦点/离线授权未核。'),
10:dict(evidence_ids='E-SONY-10;E-SONY-13;E-SYCD-12;E-SYCD-18',known_branches='默认索引范围含内存Music与Download、卡Music及已注册文件夹；可用曲库更新按钮或Options→Settings→Update database。Rebuild只证实菜单存在，未获其作用差异。',gaps='删除确认、文件回收/撤销、Update/Rebuild对收藏/歌单的具体影响未知；不可把两个名称视为已验证的数据行为。')
}
# short source notes, no replicated manuals
notes=['# NW-A306 本轮来源定位与边界','', '查阅2026-10-27；型号NW-A306/YY1301B1，英文Help Guide 5-042-672-11(5)。设备固件、预装应用构建、销售地区未知。18个官方HTML均正常HTTP200；原始全文只在work。复用既有143页PDF，未重新下载。目录82条只代表当前静态HTML菜单条目。','', '两张手册摘页已实际用view_image查看，均非设备操作。p22是数据状态说明；p111是USB-DAC屏标注图。没有连续观察、操作耗时或hands-on。','', '| E | 本页使用范围与限制 | 原件与定位 |','| --- | --- | --- |']
page_range={1:'1–5',2:'90–91',3:'72–73',4:'93',5:'109–110',6:'111–112',7:'21–22',8:'125',9:'122',10:'123–124',11:'69–70',12:'38–41',13:'45–46',14:'102–103',15:'106',16:'119',17:'42–44',18:'120–121'}
limits=[
'仅目录层级及型号；目录不等于任务已分析。',
'播放范围由入口约束；未测状态持久性。',
'外部LRC文件条件；不是内建书签。',
'只证实Display lyrics入口；未证实歌词点击寻位。',
'DAC输入与USB输出不同；后台恢复仅可能有效。',
'输入灯与退出控件；静态布局不证明运行结果。',
'deleted与disappear措辞保留；不推定重插恢复。',
'卡格式化路径与全卡数据清除；确认画面未看。',
'普通及强制重启；不保证播放点持久化。',
'出厂清除范围和确认；固件版本可改变界面。',
'曲目传回PC；不等于整机备份。',
'分类布局和更新入口；未确认重建数据库副作用。',
'整队列与单曲加入歌单不同；目标不可为PC导入歌单。',
'首次配对/超时重试；没有实测断连后的播放行为。',
'音质选项下次连接生效；接收端可限制结果。',
'配置作用于其他App；并非所有服务兼容。',
'寻位显示有误差警告；不可称精确定位。',
'仅设置名；不把Rebuild/Update的字面差异当已查行为。']
for i,r in enumerate(manifest,1):notes.append(f"| E-SYCD-{i:02} | {limits[i-1]} | `{r['path']}`；对应PDF p{page_range[i]} |")
notes+=['','原PDF：`work/sony-controls-deepening/raw/guide-existing.pdf`；SHA256 `'+sha(W/'raw/guide-existing.pdf')+'`。各HTML及摘页散列见证据JSONL和work的source-manifest.json。','', '目录、手册及Settings未取得时间点书签/睡眠计时、逐书续播和通话打断的完整说明；不能据此判定不支持。未访问关联教学视频，未安装驱动或应用。']
(A/'source-notes.md').write_text('\n'.join(notes)+'\n')
# Evidence: repeat-URL captures 17/18 preserve older E rather than supersede them.
e=[]
for i,r in enumerate(manifest,1):
 name=r['name'];ex=W/'extracted'/f'{name}.txt';visual=i in [6,7];art=f'artifacts/sony-controls-deepening/manual-p{111 if i==6 else 22:03}.png' if visual else 'artifacts/sony-controls-deepening/source-notes.md'
 title=next((x['title'] for x in m.items if x['url']==r['url']),'NW-A306完整帮助菜单（82项）')
 row=dict(evidence_id=f'E-SYCD-{i:02}',product_id='M-036',title=title,url=r['url'],checked_on='2026-10-27',source_type='official_help_index' if i==1 else 'official_help',access_mode='official_https_html_read;existing_manual_static_page_view' if visual else 'official_https_html_read',version_scope='NW-A306 / YY1301B1; English Help Guide 5-042-672-11(5); hardware firmware, Music player build and sales region unknown; no A100 generalization',app_build=None,firmware_version=None,source_updated_on=None,locator=f"正文；同版PDF p{page_range[i]}" if i!=1 else 'menu导航：82个主题；PDF目录p1–5',note=limits[i-1],hands_on=False,visual_observed=visual,archived_artifact=art,sha256=sha(L/art),capture_state='http_200_original_html',supersedes=None,raw_work_path=r['path'],raw_sha256=r['sha256'],extracted_work_path=str(ex),extracted_sha256=sha(ex))
 if visual:row.update(visual_scope='single_static_manual_page_not_continuous',derivative_source_work_path=str(W/'raw/guide-existing.pdf'),derivative_source_sha256=sha(W/'raw/guide-existing.pdf'),derivative_page=111 if i==6 else 22)
 e.append(row)
(B/'sony-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in e))
# Formal CSV rows only new flows.
fields=list(old[0]);out=[]
for n,task,pre,steps,data,ids,gap in new:
 out.append(dict(flow_id=f'F-M036-{n:02}',product_id='M-036',task=task,depth='D2',evidence_ids=ids,known_branches=data,gaps=gap,visual_status='static_manual_pages_only' if n in [13,14] else 'not_observed',hands_on='false',case_path='cases/M-036-sony.md',checked_on='2026-10-27'))
with (B/'sony-controls-deepening-coverage.csv').open('w',newline='') as f:
 writer=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');writer.writeheader();writer.writerows(out)
proposals=[]
for n,changes in updates.items():
 before=oldby[f'F-M036-{n:02}'];after={**before,**changes,'checked_on':'2026-10-27'}
 proposals.append({'flow_id':before['flow_id'],'before':before,'after':after,'changed_fields':{k:{'before':before[k],'after':after[k]} for k in before if before[k]!=after[k]}})
(W/'old-coverage-updates.json').write_text(json.dumps(proposals,ensure_ascii=False,indent=2)+'\n')
# Inventory links every topic in the actual current menu.
urlflows={
'TP1000733560':[8],'TP1000733525':[14],'TP1000733527':[3],'TP1000733528':[3,10],'TP1000733553':[3,8],'TP1000733551':[4],'TP1000733529':[7],'TP1000733580':[9],'TP1000733594':[1],'TP1000733563':[16],'TP1000733592':[12],'TP1000733545':[3,4,8],'TP1000733513':[11],'TP1000733549':[12],'TP1000735088':[5],'TP1000733548':[6],'TP1000733582':[7],'TP1000733550':[10],'TP1000733596':[9],'TP1000733597':[9],'TP1000733244':[9],'TP1000729844':[13],'TP1000731800':[13],'TP1000752542':[2],'TP1000733581':[9],'TP1000735013':[9],'TP1000733588':[10],'TP1000733532':[15],'TP1000733530':[15],'TP1000733595':[14]}
oldsource={'TP1000733560':4,'TP1000733527':1,'TP1000733529':9,'TP1000733594':5,'TP1000733545':2,'TP1000735088':7,'TP1000733548':8,'TP1000733582':9,'TP1000733550':10,'TP1000733597':11,'TP1000752542':6,'TP1000733581':12}
newurl={x['url']:x['evidence_id'] for x in e};inv=[]
for i,it in enumerate(m.items,1):
 tid=it['url'].split('/')[-1].replace('.html','');flows=urlflows.get(tid,[]);eid=newurl.get(it['url']) or (f'E-SONY-{oldsource[tid]:02}' if tid in oldsource else None)
 inv.append(dict(inventory_id=f'I-M036-{i:03}',domain=it['domain'],item=it['title'],source_url=base+'index.html',locator=f"PDF p{locs[it['url']]}" if locs[it['url']] else 'HTML目录条目；PDF页未自动匹配',article_url=it['url'],coverage_status='partial_documented' if flows else 'directory_only_not_task_complete',evidence_ids='E-SYCD-01'+(';' + eid if eid and eid!='E-SYCD-01' else ''),flow_ids=';'.join(f'F-M036-{n:02}' for n in flows),gap='仅所关联F的局部路径；未完成整章测试/异常覆盖。' if flows else '本轮仅列目录，未建立任务；不能据标题提升D2。'))
with (B/'sony-controls-deepening-inventory.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');wr.writeheader();wr.writerows(inv)
# The case retains the older flows and project inference; insert only scoped research changes.
s=(W/'case-before.md').read_text();s=s.replace('查阅：2026-10-26。','查阅：2026-10-26；本轮深化2026-10-27。',1)
s=s.replace('**D2 文档重建；无实机或连续视觉操作。**另查看了官方播放页的一张静态标注图，仅支持布局观察。已覆盖传入/索引、分类播放、队列、两类歌单、收藏、HOLD、输出/音效与删除。完整系统所有功能未核定；定时停止、时间点书签、来电打断和逐书续播未取得当前机型的完整证据。','**D2 文档重建；无实机或连续视觉操作。**旧播放页静态图之外，本轮实际查看同版手册p22与p111摘页。现有16个任务，新增播放范围、LRC歌词、USB-DAC、卡生命周期、重启/清除分支与传回PC；其余补回原编号。当前Help Guide菜单82条已逐项登记，目录不代表系统所有能力或82项任务完成。定时停止、时间点书签、来电打断和逐书续播仍无完整证据。')
s=s.replace('本次不增加 F，也不把已观察的一张播放页图扩展成整机观察。','这是2026-10-26的基线目录；本轮以2026-10-27取得的82项HTML菜单及下述更新覆盖状态。')
insert='''\n2026-10-27 的逐项台账见 [82项完整目录](../batches/sony-controls-deepening-inventory.csv)，列出每个主题、原链接、PDF定位、F/E与未覆盖状态。[E-SYCD-01] 上表保留前轮缺口记录；本轮实质变化如下，不能继续把这些列为完全未查：\n\n| 原功能域 | 本轮补足 | 仍未闭合 |\n| --- | --- | --- |\n| 分类、队列与播放 | F03分类重排/相关内容；F04整队列保存与单曲保存；F11播放范围；F12LRC；F08收回“精确寻位”措辞 | 时间点书签、逐书断点、歌词点按定位、重启持久化。 |\n| 卡与数据库 | F14格式化/弹出/移卡状态；F10默认目录与更新入口 | 卡重新插入后的关联恢复；Rebuild副作用和取消。 |\n| 输出与后台 | F09首次配对/音质设置生效时机/第三方高解析开关；F13USB-DAC进入退出及后台无声条件性恢复 | 普通播放器音频焦点、蓝牙断连暂停、有线/蓝牙优先级。 |\n| 备份与恢复 | F15正常/强制重启与出厂清除区别；F16曲目传回电脑 | 完整整机备份、故障升级回退、第三方离线授权恢复。 |\n\n'''
s=s.replace('上述是此版 Help Guide 的目录范围',insert+'上述是此版 Help Guide 的目录范围')
lines=s.splitlines()
for idx,line in enumerate(lines):
 if line.startswith('| F-M036-'):
  cols=[c.strip() for c in line.strip('|').split('|')];n=int(cols[0][-2:])
  if n==3:
   cols[3]+=' 曲库分类图标可长按拖动；Options可选择显示分类或重置默认。播放页点标题/作者/专辑或Search related可找相关内容。';cols[5]='E-SONY-01；E-SONY-02；E-SYCD-12；E-SYCD-17'
  if n==4:
   cols[3]='播放页右滑进队列。'+cols[3]+' 队列Options → Add to playlist保存整队列；单项同名菜单只加该曲，不能选电脑导入歌单作为目标。';cols[5]='E-SONY-02；E-SYCD-13';cols[6]=updates[4]['gaps']
  if n==7:
   cols[4]+=' 移卡时卡上对象收藏会disappear；重插恢复未说明（F14）。';cols[5]='E-SONY-09；E-SYCD-07';cols[6]=updates[7]['gaps']
  if n==8:
   cols[1]=updates[8]['task'];cols[3]=cols[3].replace('长按倒退','播放时长按倒退');cols[4]+=' 官方提示已播时间和时间条可能不准确，不能称精确寻位。';cols[5]='E-SONY-02；E-SONY-03；E-SONY-04；E-SYCD-17'
  if n==9:
   cols[1]=updates[9]['task'];cols[2]='蓝牙音频设备可用；首次/已有配对两分支';cols[3]='首次：Settings → Connected devices → Pair new device → 目标/必要密钥；设备名出现表示已连接，超时重试。回连：Settings → Connected devices → Connection preferences → Bluetooth开启后自动连接；未连或换目标则在Connected devices → Previously connected devices选设备，齿轮 → FORGET清配对。Wireless playback quality须下次连接才生效，可能受接收端设置限制。';cols[4]='Sound adjustment与Music player共享数值；USB音频输出限制不等于USB-DAC输入限制，后者可用音效（F13）。系统Settings → Sound → High-Res streaming开关作用于其他App，手册描述开192kHz/32bit升频、关48kHz/16bit降频，并非所有服务兼容。';cols[5]=updates[9]['evidence_ids'].replace(';','；');cols[6]=updates[9]['gaps']
  if n==10:
   cols[3]+=' 曲库也有更新按钮；先核内存Music/Download、卡Music或注册目录，再Update。';cols[4]='删除曲目不同于移出队列/歌单/收藏。Update/Rebuild是两个菜单项，现有资料没有说明二者保留对象或恢复效果的差异。';cols[5]=updates[10]['evidence_ids'].replace(';','；');cols[6]=updates[10]['gaps']
  lines[idx]='| '+' | '.join(cols)+' |'
s='\n'.join(lines)+'\n'
rows='\n'.join('| '+' | '.join([f'F-M036-{n:02}',task,pre,steps,data,ids.replace(';','；'),gap])+' |' for n,task,pre,steps,data,ids,gap in new)
s=s.replace('\n\n## 具体分支与异常','\n'+rows+'\n\n## 具体分支与异常')
s=s.replace('空数据、索引中断、空间不足、账号切换、云端同步冲突、通话打断都尚无本案例证据','本轮补了卡未识别、蓝牙配对超时和USB-DAC退后台的文档分支；普通播放音频中断及自动恢复仍未知。空数据、索引中断、空间不足、账号切换、云端同步冲突、通话打断都尚无本案例证据')
s=s.replace('## 对本项目的分析（推断）','2026-10-27另查看 [手册p22](../artifacts/sony-controls-deepening/manual-p022.png) 与 [p111](../artifacts/sony-controls-deepening/manual-p111.png)。前者确认移卡信息分成deleted/disappear两个列表；后者看到中央输入灯和格式框、顶部Library入口及底部Back。只记录印刷页与示意布局，不代表LED切换、恢复提示或后台行为已观察。[E-SYCD-07、E-SYCD-06]\n\n## 对本项目的分析（推断）')
s+='''\n### 2026-10-27 本轮新增来源\n\n正式短摘记见 [source-notes](../artifacts/sony-controls-deepening/source-notes.md)。原件路径及SHA256保存在新证据JSONL；18个HTML正常HTTP200，PDF复用旧原件，不重复访问/下载视频。E-SYCD-17/18是旧播放页/设置页的本次原文快照补证，不以新E数量代表新增能力。\n\n| E | 官方来源 | 定位 |\n| --- | --- | --- |\n'''
for row in e:s+=f"| {row['evidence_id']} | [{row['title']}]({row['url']}) | {row['locator']} |\n"
s+='\n本轮修改的是研究记录：6个新任务、6个旧任务补证/收窄；全部hands_on=false。没有改变任何设备、账户、音乐库或配对状态。G-001/G-002的进一步跨产品判断留待根代理综合。\n'
C.write_text(s)
print(json.dumps({'new_evidence':len(e),'new_flows':len(new),'old_proposals':len(proposals),'inventory':len(inv),'case_flows':len(re.findall(r'^\| F-M036-',s,re.M))},ensure_ascii=False))
