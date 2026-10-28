from pathlib import Path
import csv, json, hashlib, shutil

root=Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
work=root/'work/research/batch43-time-sample'
out=lib/'artifacts/time-sample-reader'
assert not out.exists(), 'One-shot integration; inspect state before rerunning'
assert hashlib.sha256((root/'docs/requirements.md').read_bytes()).hexdigest()=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def put(p,s): p.parent.mkdir(parents=True,exist_ok=True); p.write_text(s.strip()+'\n')
def dump(p,x): put(p,json.dumps(x,ensure_ascii=False,indent=2))
def csvread(p):
    with p.open(newline='') as f: return list(csv.DictReader(f))
def csvwrite(p,rows):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def append(p,s): p.write_text(p.read_text().rstrip()+'\n\n'+s.strip()+'\n')
def rel(p): return str(p.relative_to(lib))
out.mkdir(parents=True); (out/'browser').mkdir()
raw=work/'browser'
selected=['01-k1-sample-initial','05-menu-settled','06-last-page-command','07-save-place-dialog','09-saved-reload','11-mobile-new-tab','13-paused-speed','15-audio-reopened','16-paused-before-reload','17-listen-after-reload','18-contents','19-article-selected','20-narrow-reflow','21-paper-cover-with-saved-page4','22-browser-restart-base-entry']
for name in selected: shutil.copy2(raw/(name+'.png'),out/'browser'/(name+'.png'))
for name in ['observed-states.json','trace-actions.json','network-statuses.json','browser-errors.json','session.json']:
    shutil.copy2(raw/name,out/'browser'/name)
dump(out/'browser/raw-capture-manifest.json',{'raw_directory':str(raw.relative_to(root)),'files':{p.name:sha(p) for p in sorted(raw.iterdir()) if p.is_file()},'selected_visual_captures':selected,'note':'Raw HTML/individual states remain in work. Formal cumulative states and trace are complete. Screenshot names reflect capture intent; audit corrects transient or misleading names.'})
for n in [8,9]: shutil.copy2(work/f'teacher-p{n}.png',out/f'teacher-p{n}.png')
sample='https://reader.emagazines.com/issue/time-for-kids-k-1/may-8-2026?ipt=c8869aa3-8f3d-4699-b112-1d4a6270a953'
put(out/'browser-operation-audit.md',f'''# TIME K–1整期样刊：公开网页操作审计

证据 **E-TKSM-01**；任务 **F-M047-06**。2026-10-27，匿名Chromium156.0.8078.4，Linux/headless，初始1280×950；只在状态20改为390×844。不是触屏、儿童使用、原生App或Plus实测，没有实际听音。

## 入口与记录

[TIME教育页](https://www.timeforkids.com/educators/)公开K–1样刊链接指向[May 8, 2026样刊]({sample})。ipt参数直接来自该公开链接，不是猜测、登录凭据或绕过权限。首次正常导航HTTP200；纸刊封面标May 2026/Oceans/Vol.16/No.28，四页。浏览器页面标题、实际URL和状态时间见[23个状态](browser/observed-states.json)；[25条命令](browser/trace-actions.json)包含检查/截图，不是25次用户操作。

正式保留15张已判读画面；其他原始PNG/HTML/逐状态JSON在work，散列见[原件目录](browser/raw-capture-manifest.json)。只执行可见控件点击、下拉选择、正常导航/刷新和浏览器关闭重启；DOM只读检查控件，不注入答案或存储。文中“播放”仅指界面运行状态。

## 动作、反馈与结束

| 状态 | 动作与观察 | 能支持什么及限制 |
| --- | --- | --- |
| 00 | 另查2026-10-16 G3《Packed with Care》文章；正文有朗读/西语/打印，本次未见级别选择器 | 只是这一页的阴性观察，不证明全站不支持分级；不计新增任务 |
| 01 | 公开样刊初次进入封面，1/4 | [封面](browser/01-k1-sample-initial.png)，明确有限一期，不等于账户书库 |
| 02–05 | Next两次并开菜单；02/03短时空白，URL和提取文字一度仍为p2；等待后05显示印刷第3页，菜单位置仍2/4 | [稳定画面](browser/05-menu-settled.png)优先；03文件名last-page4只是预期，实际不是第4页。不能仅凭动作返回或URL报告成功；未量化延迟或确诊原因 |
| 06–08 | 菜单Go to last page到印刷第4页；Save my place给本设备下次返回的成功提示；再点Next仍4/4 | [末页](browser/06-last-page-command.png)与[保存提示](browser/07-save-place-dialog.png)。未见积分或理解记录；点击末页不等于完成阅读。08只记录页码/URL没有变化 |
| 09 | 正常刷新当前纸刊页 | [仍在第4页](browser/09-saved-reload.png)，单次刷新本身不能排除URL的影响 |
| 10–11 | 菜单Mobile view打开新标签；10仍是原纸刊，切换至新标签才得到重排正文 | [新标签](browser/11-mobile-new-tab.png)是Saving Sea Turtles。纸刊#p4在重排为#p3，目录包括封面和两篇文章；不是同一页码单位 |
| 12–13 | Listen展开后时间推进、段落高亮/自动滚动；Pause在0:26；选择1.5x | [暂停画面](browser/13-paused-speed.png)。六档.75/1/1.25/1.5/1.75/2x，实际仅选1.5x；12截图短时0:00/0:00而稍后文字2:02，不能当加载失败；未核声音或对齐精度 |
| 14–15 | 关闭音频工具栏，再Listen | [重开0:28/1.5x、暂停图标](browser/15-audio-reopened.png)，从原位置附近自动继续；没有保留暂停意图。关闭期间是否持续运行没有单独采样，不做推断 |
| 16–17 | 在0:53/1.5x暂停，刷新页面后再Listen | [刷新前](browser/16-paused-before-reload.png)→[刷新后0:01/1x](browser/17-listen-after-reload.png)；本次重建页面没有恢复秒点/倍速；文章身份仍同一篇 |
| 18–20 | 关播放器→CONTENTS→选Meet Sea Turtles；随后改窄视口 | [目录](browser/18-contents.png)→[选文](browser/19-article-selected.png)→[窄版](browser/20-narrow-reflow.png)。按文章跳转并重新排文字/图片，不是把整张纸页缩小；没有模拟触摸或查所有窄屏控件 |
| 21–22 | 回原纸刊标签→Go to first page，实际回封面；关闭整个持久浏览器上下文，再以相同资料目录启动浏览器，访问不含文章路径/页锚的官方样刊入口 | [关闭前封面](browser/21-paper-cover-with-saved-page4.png)→[重启回第4页](browser/22-browser-restart-base-entry.png)。支持本次显式保存页跨浏览器进程恢复；最后浏览封面未覆盖它。不证明账户同步、其他设备、清缓存、重装、刷机或音频续播 |

## 失败与记录边界

命令19把目录标题当link定位而超时；其实际role为heading，随后经可见标题正常点击成功。命令22重复截图名被脚本拒绝，已有21未覆盖。两者属于研究操作错误，保留原日志，不是产品错误。

[网络摘要](browser/network-statuses.json)只记录挂接后的document/xhr/fetch，共75条：32个200、43个204；其中未见401/403/412/429。Mobile view新标签首次加载发生在监听挂接之前，因此不是完整网络审计。旧文章00有一条Load error，见[浏览器错误](browser/browser-errors.json)，未定位来源，不把它归因于样刊音频。后续新标签/重启并非所有事件种类均重挂；无日志不证明无错误。

body.inner_text及部分控件记录可能包含屏幕外/非当前文章的内容；本次以截图和可见标题判断当前文章。没有以DOM中的多篇文字当作同时可见，也没有把URL锚点直接等同印刷页码。音频元素查询返回空，未据此判断播放不存在。

未操作搜索、下载/打印、缩放、分享、订阅/账户、全部年级和其他期刊；未测试离线、故障重试、媒体结束后的自动下一篇、跨成员、跨设备或恢复出厂。没有填写成人年龄门。外部状态仅匿名阅读遥测与该浏览器的保存页状态；未创建账号、发送消息、购买或下载音频。浏览器已正常关闭（退出0）。
''')
put(out/'source-notes.md',f'''# TIME样刊、同刊教师任务与未闭合路线

查阅2026-10-27。实际操作与原文核对分别保存，不将宣传、静态图和操作结果混合。

## E-TFK-01复用：官方公开入口

[教育页](https://www.timeforkids.com/educators/)列K–1/G2/G3–4/G5–6不同样刊与教师材料；本轮只操作其[公开K–1样刊]({sample})。来源仍复用原教育页E，不重复登记。页面说重点文章有多种阅读级别，不能因此认定每篇都有可用选择器；本轮G3文章仅见朗读/西语/打印，保持分级缺口。

## E-TFK-04复用：同一期Turtle Time任务

既有[教师指南PDF](https://www.timeforkids.com/wp-content/themes/tfk/build/assets/pdf/educators/teachers-guide-gk1.pdf)原件SHA-256为849e57011fd382fa8582b4ecf998589d5097877c5bd1fa9f7d0883041ce132a3。本轮从同一本地原件补读并静态查看第8、9页，没有重新下载或以日期相近代替内容核对。第8页封面标题Turtle Time、Vol.16/No.28与样刊一致；旧F-M047-02的Deep Blue Sea是同月另一课。

[第8页](teacher-p8.png)：成人先用线索让孩子猜海龟并联系已有经历，读后按纸刊第2/3/4页提问，涉及出生位置、眼睛、垃圾与照护。最后看部位图、填空、补眼睛细节，再讨论部位作用；可延伸到上一课鲨鱼的比较。[第9页](teacher-p9.png)提供四个填空和一个补充细节任务，图中给提示。

这是“读前调动→读/听→找依据→图示应用→解释”的教师组织流程。没有在线提交、自动评分、积分、个人掌握记录或孩子实际完成证据；图示任务不能自动写成内置小游戏。英文K–1材料不能证明适合本家庭孩子的英语水平。

## 成人新闻路线保留缺口

本轮查Espresso公开入口，未取得独立App的现版实际操作链；economist.com搜索返回robots阻止后停止该路线，没有重试绕过。主Economist应用的离线/队列帮助不能代替独立Espresso。检索出现的2024采访/会议材料只读搜索摘要，未开正文，不登记为新证据或采用其翻译编辑流程主张。原2017流程、商店说明和静态材料仍按旧版本/证据类型使用。

TIME旧目录中还有其他年级、主题和教师入口，仅本地盘点不增加已读或实测数量。本轮新增一个整期样刊任务和一个操作E；同一按钮、同一PDF补读、同一教育页复读不各算新功能。

## 对本项目的候选启发

面向G-001，有限一期可接一个可说明的阅读产出；浏览到末页和理解任务应分开。面向G-002/R-018，同一内容的文章身份、印刷页、重排位置、音频秒点、倍速和暂停原因应分别核查；“保存成功”要限定对象与恢复条件。这些是后续原型研究问题，尚未决定数据结构、同步协议、账户体系或既定交互。
''')
dump(out/'source-captures.json',{'checked_on':'2026-10-27','public_sample':sample,'discovered_from':'https://www.timeforkids.com/educators/','reused_evidence':['E-TFK-01','E-TFK-04'],'teacher_raw_path':'work/news-interactions/tfk-teacher.pdf','teacher_raw_sha256':sha(root/'work/news-interactions/tfk-teacher.pdf'),'teacher_new_static_pages':[8,9],'browser_session':json.loads((raw/'session.json').read_text()),'notes':'Public token copied from official sample link. Desktop and mobile article positions have different units. No logged-in/native/device/child/audio-listening test.'})

# One new operation evidence and one user task; old guide stays D2.
audit=out/'browser-operation-audit.md'
e=dict(evidence_id='E-TKSM-01',product_id='M-047',title='TIME K–1四页样刊：选文、听读、显式页书签及浏览器重启',url=sample,source_type='actual_public_browser_operation',access_mode='automated_public_browser',checked_on='2026-10-27',source_updated_on=None,version_scope='2026-05-08公开K–1样刊；印刷May2026/Vol16/No28；2026-10-27匿名网页',app_build=None,firmware_version=None,locator='状态01–22；00为G3文章阴性旁查；15张正式画面',note='同页音频重开接续/1.5x；刷新回起始/1x；显式纸页保存经回封面后重启恢复第4页。无实际听音/儿童/实体设备。',hands_on=True,visual_observed=True,archived_artifact=rel(audit),sha256=sha(audit),capture_state='selected_screens_and_full_state_action_logs_archived',supersedes=None,visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=rel(audit),operation_audit_sha256=sha(audit),artifact_hashes={rel(p):sha(p) for p in sorted(out.rglob('*')) if p.is_file() and not p.name.startswith('teacher-')})
put(lib/'batches/time-sample-reader-evidence.jsonl',json.dumps(e,ensure_ascii=False))
f=dict(flow_id='F-M047-06',product_id='M-047',task='K–1整期样刊选文听读与保存位置恢复',depth='D4',evidence_ids='E-TKSM-01',known_branches='四页边界;菜单末页;显式保存;Mobile view新标签;文章目录;暂停/倍速/关闭重开;刷新重置;窄屏重排;同资料目录浏览器重启恢复保存页',gaps='未实际听音;无触屏/儿童/Plus;搜索下载打印/离线/媒体结束/账号同步/刷机恢复未测;翻页瞬态与页码单位需区分',visual_status='observed_browser_sequence',hands_on='true',case_path='cases/M-047-time-for-kids.md',checked_on='2026-10-27')
csvwrite(lib/'batches/time-sample-reader-coverage.csv',[f])
ep=lib/'batches/news-evidence.jsonl'; es=[json.loads(l) for l in ep.read_text().splitlines()]
for old in es:
    if old['evidence_id']=='E-TFK-04':
        old['checked_on']='2026-10-27';old['locator']='PDF pp.1–3 and 8–9; static pp.2,3,8,9 viewed; Lesson4 matches Vol16/No28 sample'
        old['note']='教师组织的纸面任务；本轮补Turtle Time同刊部位填空/应用，仍无自动评分或课堂实测。'
        old['observed_pdf_pages']=[2,3,8,9]
        old['additional_observations']=[{'checked_on':'2026-10-27','pdf_pages':[8,9],'method':'local_original_pdf_text_and_rendered_static_pages','note':'Same raw PDF hash; old observations retained.'}]
        for n in [8,9]:
            p=out/f'teacher-p{n}.png';old['rendered_artifacts'].append(rel(p));old['artifact_hashes'][rel(p)]=sha(p)
        old['artifact_hashes'][rel(out/'source-notes.md')]=sha(out/'source-notes.md')
put(ep,'\n'.join(json.dumps(x,ensure_ascii=False) for x in es))
fp=lib/'batches/news-coverage.csv';fs=csvread(fp)
for old in fs:
    if old['flow_id']=='F-M047-02':
        old['known_branches']='Deep Blue Sea读前提问/阅读/模型/讨论；同月Turtle Time线索猜测/按页问题/图示四填空/补眼睛细节/讨论'
        old['gaps']='成人组织纸面任务;非应用自动评分;未观察课堂/孩子完成/长期掌握';old['checked_on']='2026-10-27'
csvwrite(fp,fs)

# Finite inventory, no claims of all features being exhausted.
items=[('entry','官方教育页的K–1样刊链接','document_link_checked','其他学段未进入'),('read','封面/有限四页','observed_browser','未证读完或理解'),('next','连续Next与短时空白','observed_browser','时延与p2/印刷3页关系未定'),('last','菜单到末页/Next边界','observed_browser','其他期刊未核'),('save','Save my place成功提示','observed_browser','清除/覆盖/多书签未核'),('restart','保存4页→回封面→浏览器重启','observed_browser','账号/跨设备/重装未测'),('mobile','Mobile view开新标签','observed_browser','其他浏览器未测'),('contents','目录选择文章','observed_browser','搜索未测'),('audio','Listen/暂停/1.5x/关闭重开','observed_browser','未听音/关闭期间推进未采样'),('reload','0:53暂停后刷新再Listen','observed_browser','断网/来电/睡眠未测'),('narrow','390×844重排文本/图','observed_browser','非真实触屏/Plus'),('search','目录/纸刊搜索入口','visible_control_only','未提交搜索'),('download','Download PDF入口','visible_control_only','未下载/验证离线'),('zoom','放大入口','visible_control_only','未操作'),('guide','同刊Lesson4纸面活动','document_and_static','无孩子实际完成或自动评分')]
inv=[]
for i,(slug,title,status,gap) in enumerate(items,1):
    guide=slug=='guide'
    inv.append(dict(entry_id=f'M047-TKSM-{i:03}',entry_type=slug,title=title,source_url='https://www.timeforkids.com/wp-content/themes/tfk/build/assets/pdf/educators/teachers-guide-gk1.pdf' if guide else sample,evidence_ids='E-TFK-04' if guide else 'E-TKSM-01',flow_ids='F-M047-02' if guide else 'F-M047-06',review_status=status,gap=gap))
csvwrite(lib/'batches/time-sample-reader-inventory.csv',inv)

case=lib/'cases/M-047-time-for-kids.md'
s=case.read_text().replace('查阅日2026-10-26；2026-10-27补词汇卡、目录与配套测验。','查阅日2026-10-26；2026-10-27补词汇卡、目录、配套测验与K–1整期样刊。')
s=s.replace('**D4限定公开网页三项任务，其余为文档研究：**','**D4限定公开网页四项任务的已操作分支，其余为文档研究：**')
s=s.replace('静态第2/3页已看，年历2026–2027','静态第2/3页已看，后续另补第8/9页；年历2026–2027')
case.write_text(s)
append(case,'''## 2026-10-27：整期样刊、读听状态与同刊活动

新增 **F-M047-06 / E-TKSM-01**：官方教育页所链2026-05-08 K–1公开四页样刊，封面为May2026/Oceans/Vol16/No28。本次公开网页有界操作见[23状态/25命令审计](../artifacts/time-sample-reader/browser-operation-audit.md)，以及[15项入口与缺口](../batches/time-sample-reader-inventory.csv)。没有增加账号、原生App或实体设备测试。

| 前置与任务 | 动作、可见结果 | 恢复与限制 |
| --- | --- | --- |
| F-M047-06：首次进入有限一期 | 封面1/4→Next→菜单末页；第4页Next后页码未变化 | 短时空白、URL与印刷页码存在不一致，按稳定画面核定；没有读完奖励/理解记录的证据 |
| 同任务：保存页位置 | 第4页Save my place给成功提示；回封面后关闭浏览器，沿同资料目录重启并打开基础样刊入口 | 实际回第4页，显式保存页未被最后浏览封面覆盖；不代表云备份/跨设备/刷机恢复 |
| 同任务：阅读形式与位置 | Mobile view开新标签；目录有封面和两文章，选Meet Sea Turtles；390×844实际重排文本/图 | Saving Sea Turtles为纸刊#p4、重排#p3，不能混用页码；未测触摸操作或电子纸表现 |
| 同任务：听读与暂停 | Listen运行并高亮/滚动；0:26暂停、选1.5x，关栏再开从0:28附近自动继续且保留1.5x | 暂停意图未保留；关闭期间是否运行未采样，无实际听音；0:53暂停后刷新再Listen回0:01/1x |
| F-M047-02：同刊纸面学习补证 | 教师指南第8页与样刊同封面/刊号；线索猜测→读/听→按页问题→部位图四填空→补眼睛细节→讨论 | 第8/9页仅文档/静态观察；没有在线作答、自动评分或孩子实际表现；仍D2，复用E-TFK-04 |

入口与同刊核对见[来源摘记](../artifacts/time-sample-reader/source-notes.md)，教师[第8页](../artifacts/time-sample-reader/teacher-p8.png)/[第9页](../artifacts/time-sample-reader/teacher-p9.png)。原F02的Deep Blue Sea是同月另一课，不把它与本期Turtle Time混成一篇。原F04植物网页播放器与本样刊播放器不同，关闭/刷新规则分别引用，不能互相套用。

**设计推断，未采纳：**个人新闻的有限一期可以接“找依据、解释或应用”的结束任务；仅翻到末页不能代表理解。AI续读听应确认内容身份及位置单位、当前运行状态与实际恢复范围。显式页书签、音频暂存、最后浏览位置和服务器备份不是同一结果。全文/DOM可含屏幕外文章，AI仍需确认当前呈现内容。

搜索、下载/打印、缩放、订阅、账户、离线、媒体结束后的下一篇及其他年级仍有缺口。新的G3文章本次仍未发现可核的级别选择器，保持F03分级分支未知；不据此否认官方的部分文章分级能力。全部功能和全部版本没有分析完成。
''')

# Seven reusable references with explicit conditions and persistence scope.
rp=lib/'reference-points.csv'; rows=csvread(rp)
assert len(rows)==49
data=[
('书签;恢复;浏览器重启','F-M047-06','E-TKSM-01','状态07/21/22','browser/22-browser-restart-base-entry.png','第4页显式保存；后来回封面','关闭浏览器→同资料目录重启→基础样刊入口','回第4页，最后浏览封面没有覆盖保存页','audited_public_browser','本设备浏览器进程间恢复','跨设备;清缓存;重装;刷机;多书签'),
('听读;暂停;关闭;倍速','F-M047-06','E-TKSM-01','状态13–15','browser/15-audio-reopened.png','0:26暂停且1.5x','关音频栏→Listen','0:28附近自动继续，仍1.5x','audited_public_browser','仅同页重开；暂停意图未保留','关闭期间是否继续;真实声音;媒体结束'),
('听读;刷新;恢复','F-M047-06','E-TKSM-01','状态16–17','browser/17-listen-after-reload.png','0:53暂停且1.5x','刷新→Listen','同文章回0:01/1x','audited_public_browser','本次未恢复秒点/倍速','跨进程听读;账号;离线'),
('AI;位置单位;读听切换','F-M047-06','E-TKSM-01','状态09–11','browser/11-mobile-new-tab.png','Saving Sea Turtles纸刊第4页','Mobile view→切到新标签','同文章重排视图锚为p3','audited_public_browser','只核同文章身份；位置单位改变','跨视图精确段落映射;其他文章'),
('新闻;目录;内容选择','F-M047-06','E-TKSM-01','状态18–19','browser/18-contents.png','重排正文','CONTENTS→选Meet Sea Turtles','从有限目录进入所选文章','audited_public_browser','本次未测试目录选择的重启恢复','搜索;订阅;其他期刊;阅读完成记录'),
('阅读;窄屏;重排','F-M047-06','E-TKSM-01','状态19–20','browser/20-narrow-reflow.png','已选Meet Sea Turtles','视口1280×950改390×844','文字换行、图文适配宽度','audited_public_browser','布局观察，不涉及数据保存','触屏;Plus刷新;大字设置;长文性能'),
('儿童;读后任务;理解;内容版本','F-M047-02','E-TFK-04','同一PDF第8–9页','teacher-p9.png','K–1 Turtle Time Vol16/No28；成人组织','读前猜测→按页问题→图示填空→补细节→解释','四个填空与一个补充任务有纸面材料','official_guide_static','未观察数字记录或孩子完成','评分;独立完成;中文适配;长期学习效果')]
for i,(tags,flow,eid,loc,art,pre,action,result,basis,persist,gaps) in enumerate(data,1):
    rows.append(dict(reference_id=f'RP-M047-{i:02}',product_id='M-047',question_tags=tags,flow_ids=flow,evidence_ids=eid,source_locator=loc,artifact_path=rel(out/art),precondition=pre,action=action,visible_result=result,evidence_basis=basis,persistence_result=persist,unknown_branches=gaps,version_scope='2026年5月K–1样刊及同月指南；2026-10-27匿名桌面操作/静态文档；非儿童或Plus实测',checked_on='2026-10-27'))
csvwrite(rp,rows)
gp=lib/'design-reference-guide.md';s=gp.read_text().replace('当前49条覆盖','当前56条覆盖').replace('及Duolingo数学输入/订正细节','、Duolingo数学输入/订正及TIME整期读听/恢复细节');gp.write_text(s)
append(gp,'''## 同一篇内容，保存和恢复了哪个位置

[TIME整期样刊案例](cases/M-047-time-for-kids.md)增加RP-M047-01～07，可直接比较四种不同结果：

| 问题 | 可复查依据 | 引用限制 |
| --- | --- | --- |
| 最后浏览页是否覆盖显式书签 | [封面后重启回第4页](artifacts/time-sample-reader/browser/22-browser-restart-base-entry.png)，[完整动作](artifacts/time-sample-reader/browser-operation-audit.md) | 同资料目录浏览器恢复，不是账户/服务器备份 |
| 暂停后关闭再打开是否仍暂停 | [同页重开0:28/1.5x](artifacts/time-sample-reader/browser/15-audio-reopened.png) | 自动继续，未保留暂停意图；没有实际听音 |
| 刷新能否继续同一秒点 | [刷新前0:53](artifacts/time-sample-reader/browser/16-paused-before-reload.png)→[刷新后0:01/1x](artifacts/time-sample-reader/browser/17-listen-after-reload.png) | 本次界面重建回起始；不外推其他播放器 |
| 读后怎样得到可解释的产出 | [同刊部位图/填空](artifacts/time-sample-reader/teacher-p9.png)与[教师流程](artifacts/time-sample-reader/teacher-p8.png) | 文档中的成人组织任务，未观察自动评分或儿童效果 |

同篇文章纸刊第4页在重排目录是第3项，说明引用位置时需注明单位。正式设计仍先确定要恢复的内容、成员、状态与条件，再引用相符证据；本轮不据此确定接口、数据模型或账户制度。
''')
dp=lib/'data-dictionary.md';dp.write_text(dp.read_text().replace('当前49条增量示例','当前56条增量示例'))
brief=root/'docs/briefs/043-issue-reading-and-resume.md'
put(brief,'''# 第四十三期：整期阅读、断点与读后产出

2026-10-27。竞品分析仍没有逐产品逐功能完成，65组全部部分覆盖。现有材料以文档重建为主；本轮增加TIME for Kids公开样刊的实际操作，补足“支持阅读/听读/保存”背后的条件和不同结果。

| 设计问题 | 本轮证据 | 不能据此推导 |
| --- | --- | --- |
| 保存的是最后浏览位置吗 | 第4页手动保存后回封面；关闭并重启浏览器，再从样刊入口进入，实际恢复第4页 | 账号同步、服务器备份、换设备或刷机恢复 |
| 关闭播放器后能否继续 | 0:26暂停、1.5x，关栏再开从原位置附近自动继续并保留倍速 | 暂停意图保留；关闭期间是否一直运行未知 |
| 刷新是否也能续播 | 0:53暂停后刷新再开，回0:01和1x | 同页重开有效不等于持久续播；未实际听音 |
| 阅读模式能否共用页码 | 同篇内容纸刊为第4页，重排目录为第3项；Mobile view另开标签，窄屏会重排文字和图 | 同一数字代表同一位置，或已验证触屏/墨水屏体验 |
| 一期读完后接什么 | 同刊教师指南提供读前猜测、按页提问、图示填空和补充解释 | 网站内置闯关、自动评分、儿童实际掌握 |

[操作审计](../research/interaction-library/artifacts/time-sample-reader/browser-operation-audit.md)保留23个状态、25条命令和15张画面，也保留翻页瞬态与两次研究操作错误。只新增一个整期任务；教师指南第8/9页复用旧证据与任务，不按按钮、截图或补读次数累加功能。

这份公开材料是2026年5月英语K–1样刊，本次在10月27日操作。它不能证明本家庭5–6岁孩子能够独立用英文完成，也没有验证原生App、离线、订阅、媒体结束、跨成员或Plus硬件。Espresso仍缺现版独立App的操作链，相关缺口继续保留。

后续设计可从[按问题查依据](../research/interaction-library/design-reference-guide.md)或[可筛选的交互依据表](../research/interaction-library/reference-points.csv)进入；本轮补7条，累计56条增量入口。按“条件→动作→可见结果→保存范围→未知分支”引用，再回到原图/日志核对。全部产品的覆盖状态仍见[案例索引](../research/interaction-library/case-index.md)。

当前仍在资料和原型研究阶段，没有新增已确认功能、确定账户方案或开始全系统设计。
''')
entry='- [第四十三期：整期阅读与恢复](/home/workspace/reader/docs/briefs/043-issue-reading-and-resume.md)：TIME公开样刊实操区分显式页书签、同页续播与刷新重置，配合同刊纸面任务补读后产出；新增7条可检索依据。'
for p in [root/'README.md',lib/'README.md']:
    s=p.read_text();needle='- [第四十二期：';i=s.index(needle);s=s[:i]+entry+'\n'+s[i:];p.write_text(s)
append(lib/'analysis-status.md','第43批补TIME K–1公开四页样刊：23状态、25命令、15图，新增1D4/1E；同刊教师指南第8/9页复用E-TFK-04和F-M047-02，保持D2。RP增7条到56条；明确页书签跨浏览器恢复、同页音频接续及刷新重置的不同范围。全部65组仍部分覆盖，Espresso现版操作入口缺口保留；见[简报43](../../briefs/043-issue-reading-and-resume.md)。')
append(lib/'batches/README.md','第43批 `time-sample-reader-*`：1条新D4整期阅读任务、1条新操作E、15项有状态/缺口的目录；指南补读仍由原 `news-*` 批次持有。实际操作、静态任务和未操作入口分开，见[样刊审计](../artifacts/time-sample-reader/browser-operation-audit.md)。')
append(root/'docs/ideas.md','2026-10-27继续I-021/I-022：TIME公开样刊补选文、读听、显式页书签、刷新与浏览器重启结果；同刊指南补图示应用任务。见[第四十三期简报](/home/workspace/reader/docs/briefs/043-issue-reading-and-resume.md)。按内容身份、位置单位、运行状态和恢复条件比较是后续原型建议；不把文档任务当内置游戏，也未新增需求或开始全系统设计。65组仍全部部分覆盖。')
assert sha(root/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
dump(root/'work/research/batch43-integration/integration-result.json',{'new_flows':1,'new_evidence':1,'updated_flows':['F-M047-02'],'updated_evidence':['E-TFK-04'],'reference_points_total':len(rows),'screens':len(selected),'requirements_unchanged':True})
print('Integrated batch43; run merge and checks next.')
