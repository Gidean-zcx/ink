import csv, hashlib, json, re, shutil
from pathlib import Path

root = Path('/home/workspace/reader')
lib = root / 'docs/research/interaction-library'
batch = lib / 'batches/duolingo-math-interactions'
work = root / 'work/research/batch42-duolingo-math/browser'
art = lib / 'artifacts/duolingo-math-interactions'
out = art / 'browser'
assert not out.exists(), 'Batch42 has already been integrated; do not rerun'
assert hashlib.sha256((root/'docs/requirements.md').read_bytes()).hexdigest() == 'e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
out.mkdir()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p, x): p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def readcsv(p):
    with p.open(newline='') as f: return list(csv.DictReader(f))
def writecsv(p, rows):
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def append(p,s):
    t=p.read_text();assert s not in t;p.write_text(t.rstrip()+'\n\n'+s.strip()+'\n')
def replace(p, a, b):
    t=p.read_text();assert a in t,(p,a);p.write_text(t.replace(a,b,1))

selected = '''13-after-daily-goal
20-calculator-closed
23-wrong-answer-feedback
24-after-first-error
25-correct-choice-pending
27-after-quit-ready
29-reentered-math
30-second-session-error-feedback
36-equation-feedback
38-first-tile-settled
39-selected-tile-removed
40-expression-two-plus-three
41-composed-answer-feedback
43-number-line-selected
44-number-line-feedback
46-typed-eight
47-typed-feedback
57-next-task
59-ticked-line-feedback
64-next-task
65-first-error-review-feedback
65-first-error-review-next
66-review-expression-composed
67-review-expression-feedback
69-result-settled
74-after-refresh-task
77-learning-page-ready'''.splitlines()
assert len(selected)==27
for name in selected: shutil.copy2(work/(name+'.png'),out/(name+'.png'))
for name in ['session.json','observed-states.json','trace-actions.json']:
    shutil.copy2(work/name,out/name)
def sanitize(s):
    s=re.sub(r'(?<=/users/)[^/?]+','<anonymous-user>',s)
    s=re.sub(r'(?<=/sessions/)[^/?]+','<session>',s)
    s=re.sub(r'[a-f0-9]{8}[-_][a-f0-9]{4}[-_][a-f0-9]{4}[-_][a-f0-9]{4}[-_][a-f0-9]{12}','<opaque-id>',s)
    return s
for name in ['network-statuses.json','browser-errors.json']:
    data=json.loads((work/name).read_text())
    for row in data:
        if 'url' in row: row['url']=sanitize(row['url'])
    dump(out/name,data)
dump(out/'raw-capture-manifest.json',dict(
    checked_on='2026-10-27', raw_work_path=str(work.relative_to(root)),
    raw_files={p.name:sha(p) for p in sorted(work.iterdir()) if p.is_file()},
    retained_screens=[n+'.png' for n in selected],
    formal_network_redactions='User/session path identifiers and UUID-like opaque IDs replaced; raw work hashes above refer to originals.',
    browser_process_exit_code=0, states=83, commands=58,
    interpretation='File names are labels, not conclusions; consult the operation audit.'))
source_work=root/'work/research/batch42-duolingo-sources'
captures=json.loads((source_work/'results.json').read_text())
captures['raw_work_base']=str(source_work.relative_to(root))
dump(art/'source-captures.json',captures)
child=lib/'artifacts/child-core-evidence-audit'
shutil.copy2(root/'work/research/batch42-child-inventory/local-media.json',child/'media-locations.json')

ep=[]
def rel(p): return str(p.relative_to(lib))
source_note=art/'source-notes.md'
versions=['历史博文发布2022-10-26，元数据修改2026-10-01；不是当前构建',
          '工程博文发布2024-10-18，元数据修改2026-09-18；不是API或Plus实测',
          '历史音乐介绍发布2023-10-11，元数据修改2025-01-16；核心交互尚缺']
titles=['Math历史课程、工具与换学科说明','数学知识表现形式与动态视觉工程说明','Music历史范围及共享激励说明']
for i,record in enumerate(captures['results']):
    ep.append(dict(evidence_id=f'E-DUMA-0{i+1}',product_id='M-031',title=titles[i],
        url=record['final_url'],source_type='official_blog',access_mode='public_html',
        checked_on='2026-10-27',source_updated_on=record['dates']['dateModified'][0][:10],
        version_scope=versions[i],app_build=None,locator='正文与FAQ；具体定位见来源短摘记',
        note='已读正文；没有查看博文动图/视频或实际操作其示例。范围和未知分支分开。',
        hands_on=False,visual_observed=False,archived_artifact=rel(source_note),sha256=sha(source_note),
        supersedes=None,raw_work_path=str((source_work/record['html_path']).relative_to(root)),
        raw_sha256=record['html_sha256'],artifact_hashes={rel(art/'source-captures.json'):sha(art/'source-captures.json')}))
audit=art/'browser-operation-audit.md'
ep.append(dict(evidence_id='E-DUMA-04',product_id='M-031',title='Math匿名算术课83状态：输入、错题回顾、结算及刷新',
    url='https://www.duolingo.com/math',source_type='actual_public_browser_operation',access_mode='automated_public_browser',
    checked_on='2026-10-27',source_updated_on=None,version_scope='2026-10-27匿名英语Web/Grade 2；Chromium156.0.8078.4；非原生/儿童/Plus实测',app_build=None,
    locator='文件名前缀00–77，共83状态；F-M031-20及F-M031-16/18数学子范围',
    note='两段课含退出重进；第二段15个首次题面+2次课末订正，20XP/88%；学习页刷新字段保留。目标未提交；引导PATCH422影响未知。',
    hands_on=True,visual_observed=True,archived_artifact=rel(audit),sha256=sha(audit),supersedes=None,
    visual_kind='browser_sequence',operation_method='automated_public_browser',operation_audit_path=rel(audit),operation_audit_sha256=sha(audit),
    artifact_hashes={rel(p):sha(p) for p in sorted(art.rglob('*')) if p.is_file()}))
(batch.parent/(batch.name+'-evidence.jsonl')).write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in ep))
newflow=dict(flow_id='F-M031-20',product_id='M-031',task='数学算术课的输入、错误回顾与结算',depth='D4',
    evidence_ids='E-DUMA-01;E-DUMA-02;E-DUMA-04',
    known_branches='Grade 2匿名课：计算器与作答分离，选择/拼块/键入/数轴；错后换题、末尾两题回顾；拼块待提交可移除；20XP/88%，学习页刷新仍见节点/任务/奖励。',
    gaps='Math Coach/跳主题未执行；近似容差、反复错答、输入遗漏原因、评分公式、离线/进程重启/账户同步和儿童效果未知；目标未提交，422保存影响未核。',
    visual_status='observed_browser_sequence',hands_on='true',case_path='cases/M-031-duolingo-adventures.md',checked_on='2026-10-27')
writecsv(batch.parent/(batch.name+'-coverage.csv'),[newflow])
oldpath=lib/'batches/duolingo-controls-deepening-coverage.csv'
oldrows=readcsv(oldpath)
changes=[]
for r in oldrows:
    if r['flow_id'] in ['F-M031-16','F-M031-18']:
        before=dict(r)
        r['evidence_ids']+=';E-DUMA-04'
        if r['flow_id']=='F-M031-16':
            r['known_branches']+='；Math补Student/练习目的/Other/5分钟/Grade 2进入；退出后只重选身份和年级。'
            r['gaps']+='；Math引导PATCH422具体字段未知，不认定偏好已保存；Math Coach与跳级未执行。'
        else:
            r['known_branches']+='；Math选中未提交时X直退引导，重进换题且进度条为空；完成后目标页刷新进入新题，未答X回学习页。'
            r['gaps']+='；Math未提交选择未恢复；正式账户、断网、跨进程与跨设备仍未知，不改其他模式范围。'
        changes.append(dict(flow_id=r['flow_id'],before=before,after=dict(r)))
assert len(changes)==2
writecsv(oldpath,oldrows)
dump(root/'work/research/batch42-integration/old-flow-changes.json',changes)

domains=[
 ('引导身份/年级/日目标','E-DUMA-04','F-M031-16','observed_public_browser','偏好保存422影响、其他年级/语言'),
 ('Toolkit与题目提交分离','E-DUMA-04','F-M031-20','observed_public_browser','Math Coach未操作；解释与其他工具'),
 ('选择题错误后推进','E-DUMA-04','F-M031-20','observed_public_browser','反复错答与其他课程'),
 ('未提交选择退出再进','E-DUMA-04','F-M031-18','observed_public_browser','账号/跨进程/离线'),
 ('拼块不完整输入/移回/判分','E-DUMA-04','F-M031-20','observed_public_browser','遗漏原因、通用撤销/等价式规则'),
 ('数轴估计与刻度范围','E-DUMA-04','F-M031-20','observed_public_browser','容差、吸附、错误和触摸精度'),
 ('键盘输入后判分','E-DUMA-04','F-M031-20','observed_public_browser','空值/非法字符/Enter/粘贴'),
 ('课末错题回顾','E-DUMA-04','F-M031-20','observed_public_browser','重排/调度与反复错答规则'),
 ('完成/目标未设/刷新保留','E-DUMA-04','F-M031-20','observed_public_browser','评分公式、持久身份/备份'),
 ('历史数学内容及动态视觉','E-DUMA-01;E-DUMA-02','F-M031-20','official_document_scope','原生实现/Plus适配未验证'),
 ('Math Coach与主题跳转','E-DUMA-04','','vendor_claim_only','没有执行入口到结果'),
 ('Music琴键/读谱/听辨/歌曲','E-DUMA-03','','range_only','核心错误/重听/节奏容差/退出/结算')]
inventory=[dict(entry_id=f'M-031-MA{i:02d}',product_id='M-031',function_domain=d[0],evidence_ids=d[1],flow_ids=d[2],status=d[3],remaining_gap=d[4]) for i,d in enumerate(domains,1)]
writecsv(batch.parent/(batch.name+'-inventory.csv'),inventory)

case=lib/'cases/M-031-duolingo-adventures.md'
replace(case,'# M-031 Duolingo：主学习应用、Adventures 与 Chess 分开研究','# M-031 Duolingo：主学习应用、Adventures、Chess与Math分开研究')
replace(case,'最新另实际完成Chess入门课，补同题错误/提示/重试、结算和刷新；不能用棋课结算填补法语课缺口。','后续实际完成Chess入门课及Math匿名算术课，分别补同题立即重试、课末错题回顾、结算和刷新；不能用其他学科结算填补法语课缺口。')
append(case,'''## 2026-10-27续补：Math不同输入、课末订正及刷新

新增**F-M031-20 / D4**，E-DUMA-04保存83状态/58命令/27幅必要图。旧**F-M031-16**补数学身份/目的/日目标/年级选择，旧**F-M031-18**补未提交退出、重进与课后刷新；不重复登记每个按钮。三篇官方数学/音乐正文另作范围依据E-DUMA-01～03，具体日期、旧文残留及未操作内容见[来源短摘记](../artifacts/duolingo-math-interactions/source-notes.md)。

| 任务细节 | 本次实际结果 | 不能外推的部分 |
| --- | --- | --- |
| 选择与求助 | 模式题里Toolkit计算器可得出9，关闭后仍需手动选答案；故意错选显示正确数值，Continue换题 | 计算器不是已经验证Math Coach；未测试全部提示和错误成本 |
| 拼块输入与修正 | 未完整进入的5+被判错；待提交块可点回候选区；另题2+3被接受，课末5+0也通过 | 不把漏输入当计算理解错误；提交后撤销、等价式语法规则未知 |
| 数轴和键入 | 同一加法/缺失加数以键入、无中间刻度估计及带刻度轴呈现；范围随题变化 | 像素位置不直接证明内部数值；容差、吸附、触摸及儿童理解未知 |
| 订正与结算 | 第二段15个首次题面含2错，末尾两题显示Previous mistake；通过后20XP/88% | 未推算评分公式或固定题量；这是重试通过和页面成绩，不是独立掌握 |
| 退出与重进 | 第一段第二题选中未提交时X直接回引导；重进是新题，进度条为空 | 不沿用法语课的确认对话框；没有证明账户记录被删除 |
| 完成后的状态 | 目标未提交时刷新课URL进入新题并显示5心；未答退出到路径。路径再刷新仍有首节点勾选、连续1天、505宝石、5心、任务10/10 | 同会话刷新不当长期备份。初始引导课无可见心数，不能推断普通数学课也无心数 |

逐动作、前后画面、三次定位失败、拼块输入遗漏及网络422边界见[操作审计](../artifacts/duolingo-math-interactions/browser-operation-audit.md)。这一课使用英语Grade 2；没有提交孩子身份、实际听音、观察孩子、运行原生App或测试Plus。选择最低可见年级只是研究样本，不代表5–6岁适配结论。

数学的延后错题回顾与Chess的立即同题重试可以横向比较；不同输入方式也提示G-002应区分题目、候选区、待提交值、已判分值和求助记录。G-001还需另测理解与迁移，不能以连对、XP或连续天数取代。以上是研究建议，没有决定本系统的教学策略或数据架构。

Math Coach、跳主题、其他年级/几何/时钟等仍只有部分文档或宣传；Music仅取得历史范围，尚无错误、重听、节奏容差、结束和恢复链。Adventures/口语模式与法语课原缺口保持，不能因本次数学操作而升级。''')

rpfile=lib/'reference-points.csv'
rps=readcsv(rpfile)
assert len(rps)==38
rpdata=[
 ('求助;计算器;AI;提交','F-M031-20','16–21','20-calculator-closed','模式题尚未选择答案','Toolkit→输入4+5→等号→关闭','计算器得9；原题仍待选择/Check','没有提交或偏好保存证明','Math Coach;解释;其他工具'),
 ('错答;反馈;推进','F-M031-20','22–24','24-after-first-error','首段模式题选90并Check','错误反馈后Continue','显示正确答案9后切换下一模式题','此段未完成，不推定错题已持久保存','反复错误;错误成本;其他模式'),
 ('退出;未提交;恢复','F-M031-18','25–29','29-reentered-math','第二题选5但未提交','X→重选Student/Grade 2','直退引导；再进新题、空进度条，原待提交值未恢复','仅同临时上下文；未测试长期记录','账号;离线;跨进程;不同退出阶段'),
 ('拼块;输入遗漏;错误原因','F-M031-20','34–37','36-equation-feedback','目标5，快速选择5/+/0','Check提交当时可见的5+','第三槽空，判错并给5+0作为可能答案','没有评分撤销或长期保存验证','0未进入原因;输入节流;儿童误触'),
 ('拼块;修改;撤回','F-M031-20','38–39','39-selected-tile-removed','待提交首槽已有2','点击槽内2','槽清空，2回到候选区','仅未提交输入修正，不是提交后撤销','多槽移除;重排;评分撤销'),
 ('拼块;多解;判分','F-M031-20','40–41、66–67','41-composed-answer-feedback','目标5的不同题/回顾候选池','分别组成2+3和5+0后Check','两次均获得正确反馈','只证本次表达式被接受，评分公式未知','任意等价式;语法;剩余候选;负数'),
 ('数轴;拖动;AI;范围','F-M031-20','42–44、57–63','59-ticked-line-feedback','当前题已给数轴端点/刻度','按当前范围拖游标→Check','0–28的约四分之一及0–42的7刻度均被接受','没有保存拖动路径/精确值的证据','近似容差;吸附;错误反馈;触摸精度'),
 ('键入;提交;编辑状态','F-M031-20','45–47','47-typed-feedback','3+5题的空输入栏','键入8→Check','输入栏变绿且disabled，显示正确/Continue','未测试离开后的输入/成绩保存','Enter;空值;非法字符;已判分编辑'),
 ('错题;延后重试;完成','F-M031-20','64–67','64-next-task','第二段已有两次错误，其余首次题已做','按Previous mistake回顾并答对','两道旧题回来，拼块候选重排；订正后进入结算','同课回顾，不证明后续专项错题库','反复答错;调度规则;提示;退出回顾'),
 ('结算;成绩;奖励;学习效果','F-M031-20','68–71','69-result-settled','课末两道错题均订正通过','Continue并等待结算','Lesson Complete，20XP、88%；后续到连续目标阶段','目标未提交；课程结果不能代表知识掌握','评分公式;固定奖励;重复收益;目标保存'),
 ('刷新;保存;档案;奖励','F-M031-20;F-M031-18','72–77','77-learning-page-ready','完成课、连续目标未提交','刷新课URL→新题不答X→学习页再刷新','路径首节点勾选；连续1天/505宝石/5心/任务10/10仍在','同临时上下文页面刷新；同时提示建档保存','跨进程;清存储;账户恢复;跨设备;备份')]
for i,d in enumerate(rpdata,1):
    tags,flows,locator,img,pre,action,result,persist,gaps=d
    rps.append(dict(reference_id=f'RP-M031-{i:02d}',product_id='M-031',question_tags=tags,flow_ids=flows,evidence_ids='E-DUMA-04',
        source_locator='操作审计文件名前缀'+locator,artifact_path='artifacts/duolingo-math-interactions/browser/'+img+'.png',
        precondition=pre,action=action,visible_result=result,evidence_basis='automated_public_browser',persistence_result=persist,
        unknown_branches=gaps,version_scope='2026-10-27匿名英语Web/Grade 2；非原生App/儿童/Plus实测',checked_on='2026-10-27'))
assert len(rps)==49
writecsv(rpfile,rps)
guide=lib/'design-reference-guide.md'
replace(guide,'当前38条覆盖AiPaper历史任务、The Week Junior已有操作样例、墨墨输入/同步及Inoreader/Ground新闻状态细节','当前49条覆盖AiPaper历史任务、The Week Junior已有操作样例、墨墨输入/同步、Inoreader/Ground新闻状态及Duolingo数学输入/订正细节')
marker='| --- | --- | --- | --- |\n'
replace(guide,marker,marker+'''| 儿童答错后何时重试，输入错误和知识错误怎样分开 | [Math F20](cases/M-031-duolingo-adventures.md) | [不完整输入](artifacts/duolingo-math-interactions/browser/36-equation-feedback.png)→[课末回顾](artifacts/duolingo-math-interactions/browser/64-next-task.png)→[操作审计](artifacts/duolingo-math-interactions/browser-operation-audit.md)；RP-M031-04/09 | 第二段两错在课末重做；不套Chess立即重试；漏输入原因未定，不当儿童效果证据 |
| AI得到计算结果，是否已经替用户作答 | [Math F20](cases/M-031-duolingo-adventures.md) | [计算器与待答题](artifacts/duolingo-math-interactions/browser/20-calculator-closed.png)；RP-M031-01 | 求助结果、待提交输入、判分分别核；计算器不是已验证Math Coach |
| 学完后刷新保留结果，能否当作备份 | [Math F18/20](cases/M-031-duolingo-adventures.md) | [刷新学习页](artifacts/duolingo-math-interactions/browser/77-learning-page-ready.png)与[状态72–77](artifacts/duolingo-math-interactions/browser-operation-audit.md)；RP-M031-10/11 | 页面仍提示建档保存；仅同临时上下文，未测试进程重启/换设备/刷机恢复 |
''')
replace(lib/'data-dictionary.md','当前38条增量示例','当前49条增量示例')
append(lib/'backlog.md','''## 第42批：数学操作已补与儿童素材限制

- Math：匿名Grade 2课的选择/拼块/键入/数轴、末尾两题订正、结算和页面刷新已有记录；不要继续标为全无操作。仍缺Math Coach、主题跳转、其他题型/年级、容差、反复错答、离线、进程重启和账户/家庭同步；422字段未知。Music只有历史范围，不把数学链移植过去。
- 学习效果：本次没有儿童或成人被试，没有测独立回忆/迁移和长时动力；英文Grade 2不代表5–6岁适配。题内与课后心数呈现不同，未测试普通数学课扣心。
- Khan：已有三篇本地文本找出13个图片标签，却没有原HTML/实际附件URL/图像，不能据标签补控件；见[素材复查](artifacts/child-core-evidence-audit/source-notes.md)。前轮403路线没有重试。洪恩/DragonBox既有宣传、登录和403/412限制也没有产生新核心链。
- 检索层新增RP-M031-01～11，共49条；仍是增量样例。控件/页面/状态未全量结构化，持续围绕具体设计问题补依据，不把截图数当任务数。''')
replace(lib/'backlog.md','| Duolingo主App / Adventures | 三组17帮助+6文章；普通课57状态、14图，错答/提示/听力暂缓/退出实际观察 | 最后回基础选择未见结算的原因；课末纠错/奖励；当前原生App各模式、听力恢复、完整设置和其他学科；Adventures仍缺独立全回路 |','| Duolingo主App / Adventures / Chess / Math | 普通法语课57状态、Chess82状态、Math83状态分别保存；棋课立即同题重试，数学课末订正与结算/刷新已补 | 法语课未结算原因；Adventures/口语和Music核心链；当前原生App、Math Coach/跳主题、其他年级、长期恢复与实际学习效果仍缺 |')
replace(lib/'backlog.md','RP当前38条仅增量样例','第41批时RP为38条，仅增量样例')
append(lib/'analysis-status.md','''第42批补Math匿名实际课：83状态、58命令、27图，新增1条有界D4，补旧引导/退出2任务；三篇官方数学/音乐正文与一个操作E共4E。Music仅范围发现；Khan13图片标签无实际URL/图像，洪恩/DragonBox缺口仍在。RP增11条到49条，全部65组仍部分覆盖；见[简报42](../../briefs/042-math-feedback-and-research-depth.md)。''')
append(lib/'batches/README.md','''第42批 `duolingo-math-interactions`：4新E、1新D4数学练习任务，旧F-M031-16/18在原批次补数学范围；12个目录/控件节点不当12个功能。27图、83状态和58条命令保存为一个浏览器证据E，含未完成输入、定位失败、422与目标未提交边界。RP新增11条，Khan本地素材复查不新造F/E。见[简报42](../../../briefs/042-math-feedback-and-research-depth.md)。''')
brief='[第四十二期：数学反馈与研究深度](/home/workspace/reader/docs/briefs/042-math-feedback-and-research-depth.md)：实际追踪输入、错答推进、课末订正、结算和刷新；按设计问题增加11条可复查依据，仍明确全部产品部分覆盖。'
replace(root/'README.md','- [第四十一期：新闻阅读与保留]', '- '+brief+'\n- [第四十一期：新闻阅读与保留]')
replace(lib/'README.md','- [第四十一期：新闻阅读与保留]', '- '+brief+'\n- [第四十一期：新闻阅读与保留]')
replace(lib/'README.md','交互依据表现有38条直接检索示例','该批交互依据表累计38条直接检索示例')
append(root/'docs/ideas.md','''2026-10-27继续I-021/I-022：Math实际课补不同输入、未完整拼块、错答后换题与课末订正、结算和页面刷新，区别于Chess立即重试；Khan图片标签无原图/URL的缺口正式保留。新增11条可检索依据，见[第四十二期简报](/home/workspace/reader/docs/briefs/042-math-feedback-and-research-depth.md)。全部产品仍部分覆盖，网页样本不当儿童/Plus实测；没有把机制建议升级为需求，也未开始全系统设计。''')
print(json.dumps(dict(new_evidence=4,new_flows=1,old_flows=2,states=83,screens=27,reference_points=len(rps)),ensure_ascii=False))
