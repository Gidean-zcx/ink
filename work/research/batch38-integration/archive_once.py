from pathlib import Path
import csv, hashlib, json, shutil

ROOT=Path('/home/workspace/reader')
LIB=ROOT/'docs/research/interaction-library'
RAW=ROOT/'work/research/batch38-week'
OUT=LIB/'artifacts/week-reader-recovery'
assert not OUT.exists(), 'Archive already exists; do not rerun'
OUT.mkdir(parents=True)
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,v): p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
states=json.loads((RAW/'observed-states.json').read_text())
seen={}
for s in states:
    original=s['screenshot']; source=RAW/original
    assert digest(source)==s['sha256']
    if s['sha256'] not in seen:
        seen[s['sha256']]=original
        shutil.copyfile(source,OUT/original)
    s['raw_screenshot']=original
    s['screenshot']=seen[s['sha256']]
    # Hidden iframe text may contain offscreen articles. Do not publish it as
    # the current article or duplicate editorial text in the formal archive.
    for f in s.get('frames',[]): f.pop('text_prefix',None)
dump(OUT/'observed-states.json',states)
for name in ['trace-actions.json','errors.json','network.json']:
    shutil.copyfile(RAW/name,OUT/name)
env=json.loads((RAW/'environment.json').read_text())
env.pop('fresh_local_profile',None)
env.update(profile_method='New task-specific desktop-profile directory; two subsequent browser launches reuse it. Mobile uses a new empty context.',mobile_emulation={'width':390,'height':844,'is_mobile':True,'has_touch':True,'device_scale_factor':1},audio_observation='UI timer, controls, highlight and scrolling only; no audible listening, quality or output-route test',offline_test=False,screen_capture_timing='Screenshot precedes DOM read; advancing timers may differ by 1–2 seconds.',state_text_scope='Outer controls and iframe controls; getClientRects does not guarantee viewport intersection. Hidden/offscreen article text was excluded.',duplicate_screenshots='Same SHA-256 shares one archived screenshot; raw_screenshot retains the original work filename.')
dump(OUT/'environment.json',env)
dump(OUT/'raw-manifest.json',{str(p.relative_to(ROOT)):digest(p) for p in [RAW/'session.py',RAW/'extend_capture.py',RAW/'observed-states.json',RAW/'trace-actions.json',RAW/'network.json',RAW/'errors.json',RAW/'environment.json',*sorted(RAW.glob('*.html'))]})

desc={
0:'新桌面资料目录打开公开入口，封面1/32。',
1:'点击左上搜索坐标，界面未变。',
2:'点击可见searchIcon2，等1.5秒仍未展开；与00截图同哈希。',
3:'Next进入第2页，页图、页码和URL变化。',
4:'Zoom打开全窗口放大层，底部有缩放控件。',
5:'点击放大层的加号，图像扩大，滑块变化。',
6:'鼠标由(700,650)拖到(400,350)，放大图平移。',
7:'点击右上X返回第2页；未操作全屏按钮。',
8:'在第2页点击搜索图标，侧栏展开。',
9:'可见页码框输入14并Enter，定位印刷第14页。',
10:'搜索Nobel，出现5个页码结果组及命中摘要。',
11:'点击Page 15结果标题并等3秒，选项高亮，主图/页码/URL仍14。',
12:'再点击该结果缩略图并等1.5秒，仍14；与11截图同哈希。',
13:'Clear search后侧栏收起，仍14。',
14:'Save my place出现本设备下次访问回此页的成功提示。',
15:'Go to first page回封面，没有重新保存。',
16:'关闭并重新启动浏览器，同一资料目录打开无页码入口，恢复14。',
17:'耳机按钮打开独立重排正文窗口及音频条，初始0:00/0:00。',
18:'稍后观察到计时推进、段落高亮和正文滚动；间隔不是受控加载测时。',
19:'Pause后画面2:46/7:02，按钮变Play。',
20:'Playback speed选1.5，仍暂停于2:46。',
21:'Play后等待3秒，截图2:50、随后DOM2:51；不据此测真实音速。',
22:'再次Pause，2:52/7:02、1.5x。',
23:'外层Close返回印刷14页；内嵌页面仍存在但隐藏。',
24:'再次点耳机，正文停留处与1.5x保留，并自动续放；截图2:56、DOM2:57。',
25:'暂停到3:08/7:02。',
26:'Close audio player移除音频条，正文保留，顶栏出现Listen。未操作该Listen入口。',
27:'外层Close回印刷14页。',
28:'第二次浏览器重启仍回14，但本次未显示耳机入口。',
29:'定位耳机按钮超时30秒，没有打开音频；截图同28。',
30:'后续已排队的Pause定位也超时30秒；隐藏播放器控件不表示已播放。',
31:'Next动作返回，但捕获时正文空白、页码和URL仍14；不记成功翻到15。',
32:'Previous后重新显示14页，耳机入口仍未见；原因未定。',
33:'新建390×844触摸模拟空上下文，同一期从封面1/32开始。',
34:'触摸点击汉堡菜单，显示搜索/目录/书签/下载及页码框。',
35:'页码框输入14并Enter，主图和页码变14。',
36:'关闭菜单后仍为整页缩放版面，底部有缩放和耳机图标；未在窄屏播放或测试滑动。'
}
rows=['| 状态 | 动作及结果 |','| --- | --- |']
for s in states: rows.append(f"| [{s['index']:02d}]({s['screenshot']}) | {desc[s['index']]} |")
header='''# M048：页码、听读与恢复的实际操作依据

2026-10-27。E-WJCD-02、E-WJCD-03、E-WJCD-04；F-M048-05、F-M048-06、F-M048-07、F-M048-11、F-M048-12。同一来源的续补，不增加来源编号；旧[14张截图和操作审计](../week-junior-controls-deepening/browser-operation-audit.md)保留，以下状态编号仅属于本轮。

根代理用Playwright驱动真实公开网页并查看截图。`hands_on=true`、`operation_method=automated_public_browser`仅指所列网页操作；无手机/电子纸实机、原生App或儿童参与测试。D4是可复查操作记录，包含失败，不表示流程全部成功或产品完成。

来源：[美国版官方样刊入口](https://theweekjunior.com/in-the-magazine) → 官方公开短链 → [2026-10-23第340期阅读器](https://reader.emagazines.com/issue/the-week-junior/october-23-2026?ipt=F0826FC3-4129-46CF-A5F3-FF70A931F1FF)。ipt是公开样刊参数。刊期显示32页；资源路径的20261016不是本次刊期，也不是应用版本。没有逐页读完一期。

环境：Chromium156.0.8078.4、桌面1200×900；新建本任务浏览器资料目录，后两次重启沿用；最后新建390×844触摸模拟空上下文。站点构建未知。详见[环境与采集边界](environment.json)。没有登录、购买、投稿、下载整期或主动导出媒体。

## 可引用结论与边界

- 书签：在14页手动保存、回封面，再重启浏览器后仍回14。第二次重启页码也恢复。支持这一匿名资料目录和样刊的页级恢复；不证明账户、多设备、刷机或音频秒点恢复。
- 听读：耳机入口打开重排正文窗口。UI有播放/暂停、0.75–2倍速选择、音量按钮、关闭播放器；本次实际选择1.5、播放、暂停和关闭。没有听取声音，不判断音质、真实音速或朗读准确性。
- 同页关闭恢复：在2:52、1.5x暂停后关闭外层窗口，再点耳机，会从原位置附近自动推进到2:56（随后DOM2:57）。原iframe并未销毁，因此只支持这一页面内的状态保留，不能称持久化。
- 重启后的音频：第二次重启恢复14页，但耳机入口未出现，定位与后续暂停操作超时。Next捕获正文空白，Previous回14；没有得到音频跨进程接续结果。原因可能涉及页面状态或加载，未定位，不能写成“音频恢复失败已确诊”或“功能不存在”。
- 搜索：封面两次尝试不展开，第2页可展开；Nobel的Page15标题/缩略图都未使主图跳页。仅记本次条件下观察，不能推定所有浏览器均失败。旧空搜索结果仍见旧审计。
- 窄屏：实际触摸模拟菜单并跳14，展示仍为纸刊整页缩放；耳机可见但未操作。模拟视口不代表实体手机手感，更不证明Plus的刷新性能。

## 分开保存的三个位置

外层纸刊是印刷页14，内嵌重排窗口URL是`science-and-technology…#p9`，音频条总时长7:02。根代理只读检查活动slide为page9；它与印刷页号不同。全iframe正文包含其他离屏文章，不能用DOM首段当正在看的文章。本次截图显示科学栏目多个内容块，未验证完整音文映射或逐句同步精度。

对本项目的**推断**：R-018/R-021需要分别认识内容/版本、印刷页、语义段落、音频时间、家庭成员及暂停原因。竞品观察只能说明这些状态可能不同；不证明其有AI接口，也不自动决定本项目的数据模型。

## 实际动作与画面

'''
tail='''

## 失败、原件及复查

保留[35次动作记录](trace-actions.json)、[37个观察状态](observed-states.json)、[网络状态](network.json)和[错误记录](errors.json)。动作返回只表示点击/输入已执行，结果须看图与状态。两次30秒超时保存在动作记录；另一个只读DOM查询对SVG读取innerText时报错，是采集脚本错误，随后修正，不能算产品异常。该轮网络记录没有HTTP400及以上响应；这不证明所有资源都成功完成。

每个状态有截图SHA-256；重复画面共用同一归档文件，原work文件名在raw_screenshot。截图先于DOM读取，播放计时可能相差1–2秒，原值均保留。除了重复哈希图，所用代表图由根代理实际查看。没有连续录像、客观延迟测量或听觉记录。原始HTML、脚本及未裁剪状态留work，见[原件哈希清单](raw-manifest.json)；浏览器资料目录及cookie没有归档到正式库。

复查需使用固定刊期：从第2页打开搜索侧栏，跳14；保存书签后回封面并用同一资料目录重启。听读另测暂停→外层关闭→重开、播放器关闭→重新进入、浏览器重启，记录每一步页码/文章/秒点/倍速/播放状态。若入口缺失，保留现场，不能用强制调用内部函数补出成功。断网、换用户、跨端、清除站点数据、音频队列/拖动进度、音量输出及学习效果仍未知。
'''
(OUT/'browser-operation-audit.md').write_text(header+'\n'.join(rows)+tail)

# Normalize the stopped request without retrying the protected website.
tonie=LIB/'artifacts/tonieplay-source-attempt';tonie.mkdir()
traw=ROOT/'work/research/batch38-tonieplay/raw'
attempt={'checked_on':'2026-10-27','inventory_id':'I-M038-139','url':'https://support.tonies.com/hc/en-gb/articles/29036585437714-How-much-help-will-my-child-need-with-playing-a-Tonieplay-game','method':'single_anonymous_curl_GET','http_status':403,'response_marker':'cf-mitigated: challenge','response_date':'Tue, 27 Oct 2026 04:37:18 GMT','body_read':False,'overall_command_exit':1,'local_failure':'Shell read parsing trailing empty redirect field returned nonzero after the request; status TSV remained header-only. Root normalized existing raw metadata without a new request.','other_candidates_requested':False,'raw_files':{str(p.relative_to(ROOT)):digest(p) for p in sorted(traw.glob('29036585437714.*'))}}
dump(tonie/'attempt.json',attempt)
(tonie/'source-notes.md').write_text('''# Tonieplay正文获取未完成

2026-10-27。关联既有目录证据E-TNCD-03、条目I-M038-139。对[成人协助文章](https://support.tonies.com/hc/en-gb/articles/29036585437714-How-much-help-will-my-child-need-with-playing-a-Tonieplay-game)的一次匿名GET返回HTTP403与`cf-mitigated: challenge`，未读正文，随后停止该路线。I-M038-141仅为候选，未请求。不能从URL标题推断成人实际需要做哪些步骤。

执行命令最终exit1还有本地解析问题：shell read读取末尾空重定向字段返回非零，原状态TSV只留下表头。根代理复核现有headers/meta后整理[请求与哈希记录](attempt.json)，没有再次访问或换路线重试。站点访问受限和本地日志处理错误分别保留；没有新增产品行为证据或提升任务深度。
''')
print(json.dumps({'states':len(states),'unique_screenshots':len(seen),'actions':35,'artifacts':str(OUT)},ensure_ascii=False))
